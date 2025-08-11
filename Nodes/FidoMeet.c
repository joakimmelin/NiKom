#include "NiKomCompat.h"
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#ifdef HAVE_PROTO_ALIB_H
/* For NewList() */
# include <proto/alib.h>
#endif
#include <proto/dos.h>
#include <proto/intuition.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "NiKomStr.h"
#include "Nodes.h"
#include "NiKomFuncs.h"
#include "NiKomLib.h"
#include "NiKEditor.h"
#include "VersionStrings.h"
#include "Logging.h"
#include "Terminal.h"
#include "BasicIO.h"
#include "UserNotificationHooks.h"
#include "Languages.h"
#include "StyleSheets.h"
#include "KOM.h"
#include "StringUtils.h"
#include "FidoUtils.h"
#include "BTree.h"
#include "IntList.h"
#include "Stack.h"

#include "FidoMeet.h"

#define EKO		1

extern struct System *Servermem;
extern char outbuffer[],*argument,inmat[];
extern int nodnr,senast_text_typ,senast_text_nr,senast_text_mote,nu_skrivs,inloggad,
  rad,mote2, g_userDataSlot, senast_text_reply_to;
extern int g_lastKomTextType, g_lastKomTextNr, g_lastKomTextConf;
extern struct Inloggning Statstr;
extern struct MinList edit_list;

void fido_lasa(int tnr,struct Mote *motpek) {
  if(tnr < motpek->lowtext || tnr > motpek->texter) {
    SendString("\r\n\n%s\r\n", CATSTR(MSG_FORUMS_NO_SUCH_TEXT));
    return;
  }
  fido_visatext(tnr, motpek, NULL);
}

void skipAlreadyReadTexts(struct Mote *conf) {
  while(IntListRemoveValue(g_readRepliesList, CUR_USER_UNREAD->lowestPossibleUnreadText[conf->nummer]) != -1) {
    CUR_USER_UNREAD->lowestPossibleUnreadText[conf->nummer]++;
  }
}

int HasUnreadInFidoConf(struct Mote *conf) {
  skipAlreadyReadTexts(conf);
  return CUR_USER_UNREAD->lowestPossibleUnreadText[conf->nummer]
    <= conf->texter;
}

static void displayFidoTextFromKOM(int textId, struct Mote *conf) {
  fido_visatext(textId, conf, g_unreadRepliesStack);
  g_lastKomTextType = TEXT_FIDO;
  g_lastKomTextNr = textId;
  g_lastKomTextConf = conf->nummer;
}

void NextTextInFidoConf(struct Mote *conf) {
  struct UnreadTexts *unreadTexts = CUR_USER_UNREAD;

  skipAlreadyReadTexts(conf);
  if(unreadTexts->lowestPossibleUnreadText[conf->nummer] < conf->lowtext) {
    unreadTexts->lowestPossibleUnreadText[conf->nummer] = conf->lowtext;
  }
  if(unreadTexts->lowestPossibleUnreadText[conf->nummer] > conf->texter) {
    SendString("\n\n\r%s\n\r", CATSTR(MSG_NEXT_TEXT_NO_TEXTS));
    return;
  }
  StackClear(g_unreadRepliesStack);
  displayFidoTextFromKOM(unreadTexts->lowestPossibleUnreadText[conf->nummer], conf);
  unreadTexts->lowestPossibleUnreadText[conf->nummer]++;
}

void NextReplyInFidoConf(struct Mote *conf) {
  int textId;
  textId = StackPop(g_unreadRepliesStack);
  displayFidoTextFromKOM(textId, conf);
  IntListAppend(g_readRepliesList, textId);
}

int countfidomote(struct Mote *motpek) {
  return motpek->texter
    - CUR_USER_UNREAD->lowestPossibleUnreadText[motpek->nummer]
    + 1
    - (motpek->nummer == mote2 ? IntListSize(g_readRepliesList) : 0);
}

int isFidoSystemStr(char *str) {
  return strncmp(str, "--- ", 4) == 0
    || strncmp(str, " * Origin: ", 11) == 0
    || strncmp(str, "SEEN-BY: ", 9) == 0;
}

int lookupRepliedText(struct FidoText *ft, struct Mote *conf) {
  struct FidoLine *fl;
  struct BTree *msgidTree;
  int replyTextId = -1;
  char replyMsgid[FIDO_MSGID_KEYLEN] = "";
  char treePath[100];
  ITER_EL(fl, ft->text, line_node, struct FidoLine *) {
    if(fl->text[0] != 1) {
      break;
    }
    if(strncmp(&fl->text[1], "REPLY:", 6) == 0) {
      strncpy(replyMsgid, &fl->text[8], FIDO_MSGID_KEYLEN);
      break;
    }
  }
  if(replyMsgid[0] == '\0') {
    return -1;
  }
  sprintf(treePath, "NiKom:FidoComments/%s_id", conf->tagnamn);
  ObtainSemaphore(&((struct ExtMote *)conf)->fidoCommentsSemaphore);
  if((msgidTree = BTreeOpen(treePath)) == NULL) {
    ReleaseSemaphore(&((struct ExtMote *)conf)->fidoCommentsSemaphore);
    return -1;
  }
  BTreeGet(msgidTree, replyMsgid, &replyTextId);
  BTreeClose(msgidTree);
  ReleaseSemaphore(&((struct ExtMote *)conf)->fidoCommentsSemaphore);
  return replyTextId;
}

void displayComments(int text, struct Mote *conf, struct Stack *repliesStack) {
  struct BTree *commentsTree;
  struct FidoText *commentFt;
  int i, comments[FIDO_FORWARD_COMMENTS];

  char path[100];
  sprintf(path, "NiKom:FidoComments/%s_c", conf->tagnamn);
  ObtainSemaphore(&((struct ExtMote *)conf)->fidoCommentsSemaphore);
  if((commentsTree = BTreeOpen(path)) == NULL) {
    ReleaseSemaphore(&((struct ExtMote *)conf)->fidoCommentsSemaphore);
    return;
  }
  if(!BTreeGet(commentsTree, &text, comments)) {
    ReleaseSemaphore(&((struct ExtMote *)conf)->fidoCommentsSemaphore);
    BTreeClose(commentsTree);
    return;
  }
  BTreeClose(commentsTree);
  ReleaseSemaphore(&((struct ExtMote *)conf)->fidoCommentsSemaphore);

  for(i = 0; i < FIDO_FORWARD_COMMENTS && comments[i] != 0; i++) {
    MakeMsgFilePath(conf->dir, comments[i] - conf->renumber_offset, path);
    if((commentFt = ReadFidoTextTags(path, RFT_HeaderOnly, TRUE, TAG_DONE)) != NULL) {
      SendStringCat("  %s\r\n", CATSTR(MSG_ORG_TEXT_COMMENT_IN), comments[i], commentFt->fromuser);
      FreeFidoText(commentFt);
    }
  }
  for(i = FIDO_FORWARD_COMMENTS - 1; i >= 0; i--) {
    if(repliesStack != NULL && comments[i] != 0) {
      StackPush(repliesStack, comments[i]);
    }
  }
}

void fido_visatext(int text,struct Mote *motpek, struct Stack *repliesStack) {
  struct FidoText *ft, *repliedFt;
  struct FidoLine *fl;
  char fullpath[100];
  int repliedTextId, effectiveCharset;
  CURRENT_USER->read++;
  Servermem->info.lasta++;
  Statstr.read++;
  MakeMsgFilePath(motpek->dir, text - motpek->renumber_offset, fullpath);
  effectiveCharset = Servermem->nodeInfo[nodnr].nodeType == NODCON
    ? CHRS_LATIN1 : CURRENT_USER->chrset;
  ft = ReadFidoTextTags(fullpath,
                        RFT_NoSeenBy, !(CURRENT_USER->flaggor & SHOWKLUDGE),
                        RFT_PassthroughCharset, effectiveCharset,
                        TAG_DONE);
  if(!ft) {
    LogEvent(SYSTEM_LOG, ERROR, "Can't read fido text %s.", fullpath);
    DisplayInternalError();
    return;
  }
  repliedTextId = lookupRepliedText(ft, motpek);

  SendString(CURRENT_USER->flaggor & CLEARSCREEN ? "\f" : "\r\n\n");

  SendStringCat("%s\r\n", CATSTR(MSG_FIDO_TEXT_LINE_1), text, motpek->namn, ft->date);
  SendStringCat("%s\r\n", CATSTR(MSG_FIDO_TEXT_LINE_2),
             ft->fromuser,ft->fromzone,ft->fromnet,ft->fromnode,ft->frompoint);
  SendStringCat("%s\r\n", CATSTR(MSG_FIDO_TEXT_TO), ft->touser);
  if(repliedTextId != -1) {
    MakeMsgFilePath(motpek->dir, repliedTextId - motpek->renumber_offset, fullpath);
    repliedFt = ReadFidoTextTags(fullpath, RFT_HeaderOnly, TRUE, TAG_DONE);
    if(repliedFt != NULL) {
      SendStringCat("%s\r\n", CATSTR(MSG_ORG_TEXT_COMMENT_TO), repliedTextId, repliedFt->fromuser);
      FreeFidoText(repliedFt);
    }
  }
  SendStringCat("%s\r\n", CATSTR(MSG_ORG_TEXT_SUBJECT), ft->subject);
  if(CURRENT_USER->flaggor & STRECKRAD) {
    SendRepeatedChr('-', RenderLength(outbuffer));
    SendString("\r\n\n");
  } else {
    SendString("\n");
  }
  SetCharacterSetPassthrough(CURRENT_USER->chrset == ft->charset);
  ITER_EL(fl, ft->text, line_node, struct FidoLine *) {
    if(fl->text[0] == 1) {
      if(CURRENT_USER->flaggor & SHOWKLUDGE) {
        if(SendString("�system�^A%s�reset�\r\n", &fl->text[1])) { break; }
      }
    } else {
      if(IsQuote(fl->text)) {
        if(SendString("�quote�%s�reset�\r\n", fl->text)) break;
      } else if(isFidoSystemStr(fl->text)) {
        if(SendString("�system�%s�reset�\r\n", fl->text)) break;
      } else {
        if(SendString("%s\r\n", fl->text)) break;
      }
    }
  }
  SetCharacterSetPassthrough(FALSE);
  SendStringCat("\n%s\r\n", CATSTR(MSG_ORG_TEXT_END_OF_TEXT), text,ft->fromuser);
  FreeFidoText(ft);
  displayComments(text, motpek, repliesStack);
  senast_text_typ=TEXT;
  senast_text_nr=text;
  senast_text_mote=motpek->nummer;
  senast_text_reply_to = repliedTextId;
}

void makefidodate(char *str) {
	struct DateTime dt;
	char datebuf[14],timebuf[10];
	DateStamp(&dt.dat_Stamp);
	dt.dat_Format = FORMAT_DOS;
	dt.dat_Flags = 0;
	dt.dat_StrDay = NULL;
	dt.dat_StrDate = datebuf;
	dt.dat_StrTime = timebuf;
	DateToStr(&dt);
	dt.dat_StrDate[2] = ' ';
	dt.dat_StrDate[6] = ' ';
	if(dt.dat_StrDate[4] == 'k') dt.dat_StrDate[4] = 'c'; /* Okt -> Oct */
	if(dt.dat_StrDate[5] == 'j') dt.dat_StrDate[5] = 'y'; /* Maj -> May */
	sprintf(str,"%s  %s",dt.dat_StrDate,dt.dat_StrTime);
}

void makefidousername(char *str,int anv) {
	char tmpusername[50];
	int x;
	strcpy(tmpusername,getusername(anv));
	for(x=0;tmpusername[x];x++) if(tmpusername[x] == '#' || tmpusername[x] == '(' || tmpusername[x] == ',' || tmpusername[x] == '/') break;
	tmpusername[x]=0;
	while(tmpusername[--x]==' ') {
		tmpusername[x]=0;
		if(!x) break;
	}
	sprattgok(tmpusername);
	strcpy(str,tmpusername);
}

struct FidoDomain *getfidodomain(int nr,int zone) {
  int i;

  for(i = 0; i < 10; i++) {
    if(!Servermem->cfg->fidoConfig.fd[i].domain[0]) {
      return NULL;
    }
    if(nr > 0) {
      if(nr == Servermem->cfg->fidoConfig.fd[i].nummer) {
        return &Servermem->cfg->fidoConfig.fd[i];
      }
    } else {
      if(IsZoneInStr(zone, Servermem->cfg->fidoConfig.fd[i].zones, FALSE)) {
        return &Servermem->cfg->fidoConfig.fd[i];
      }
    }
  }
  return NULL;
}


int fido_skriv(int komm,int komtill) {
  int length = 0, editret, nummer;
  struct MinNode *first, *last;
  struct FidoText ft, *komft;
  struct FidoDomain *fd;
  struct Mote *motpek;
  struct FidoLine *fl;
  char filnamn[15], fullpath[100], msgid[50];
  struct EditContext editContext;

  Servermem->nodeInfo[nodnr].action = SKRIVER;
  Servermem->nodeInfo[nodnr].currentConf = mote2;
  motpek = getmotpek(mote2);
  memset(&ft, 0, sizeof(struct FidoText));
  if(komm) {
    strcpy(fullpath, motpek->dir);
    sprintf(filnamn, "%ld.msg", komtill - motpek->renumber_offset);
    AddPart(fullpath, filnamn, 99);
    komft = ReadFidoTextTags(fullpath, RFT_HeaderOnly, TRUE, TAG_DONE);
    if(!komft) {
      LogEvent(SYSTEM_LOG, ERROR, "Can't read fido text %s.", fullpath);
      DisplayInternalError();
      return 0;
    }
    strcpy(ft.touser, komft->fromuser);
    strcpy(ft.subject, komft->subject);
    strcpy(msgid, komft->msgid);
    FreeFidoText(komft);
  }
  makefidousername(ft.fromuser, inloggad);
  makefidodate(ft.date);
  fd = getfidodomain(motpek->domain, 0);
  if(!fd) {
    LogEvent(SYSTEM_LOG, ERROR, "Can't find the Fido domain with id %d.", motpek->domain);
    DisplayInternalError();
    return 0;
  }
  ft.fromzone = fd->zone;
  ft.fromnet = fd->net;
  ft.fromnode = fd->node;
  ft.frompoint = fd->point;
  ft.attribut = FIDOT_LOCAL;
  SendStringCat("\r\n\n%s\r\n", CATSTR(MSG_FIDO_WRITE_FORUM_DATE), motpek->namn, ft.date);
  SendStringCat("%s\r\n", CATSTR(MSG_FIDO_WRITE_WRITTEN_BY),
             ft.fromuser, ft.fromzone, ft.fromnet, ft.fromnode, ft.frompoint);
  if(!komm) {
    SendString("%s ", CATSTR(MSG_FIDO_WRITE_TO));
    if(getstring(EKO, 35, NULL)) {
      return 1;
    }
    if(!inmat[0]) {
      strcpy(ft.touser, "All");
    } else {
      strcpy(ft.touser, inmat);
      sprattgok(ft.touser);
    }
    SendString("\n\r%s ", CATSTR(MSG_WRITE_SUBJECT));
    if(getstring(EKO,71,NULL)) {
      return 1;
    }
    strcpy(ft.subject,inmat);
    SendString("\r\n");
  } else {
    SendStringCat("%s\n\r", CATSTR(MSG_FIDO_TEXT_TO), ft.touser);
    SendStringCat("%s\n\r", CATSTR(MSG_ORG_TEXT_SUBJECT), ft.subject);
  }
  if(CURRENT_USER->flaggor & STRECKRAD) {
    length=strlen(ft.subject);
    SendRepeatedChr('-', length);
    SendString("\r\n\n");
  } else {
    SendString("\n");
  }

  memset(&editContext, 0, sizeof(struct EditContext));
  editContext.subject = ft.subject;
  editContext.subjectMaxLen = 71;
  if(komm) {
    editContext.replyingToText = komtill;
    editContext.replyingInConfId = mote2;
  }
  editret = edittext(&editContext);
  if(editret == 1) {
    return 1;
  }
  if(editret ==2 ) {
    return 0;
  }
  CURRENT_USER->skrivit++;
  Servermem->info.skrivna++;
  Statstr.write++;
  NewList((struct List *)&ft.text);
  first =  edit_list.mlh_Head;
  last = edit_list.mlh_TailPred;
  ft.text.mlh_Head = first;
  ft.text.mlh_TailPred = last;
  last->mln_Succ = (struct MinNode *)&ft.text.mlh_Tail;
  first->mln_Pred = (struct MinNode *)&ft.text;
  fl = AllocMem(sizeof(struct FidoLine), MEMF_CLEAR);
  AddTail((struct List *)&ft.text, (struct Node *)fl);
  fl = AllocMem(sizeof(struct FidoLine), MEMF_CLEAR);
  strcpy(fl->text,"--- NiKom " NIKRELEASE);
  AddTail((struct List *)&ft.text, (struct Node *)fl);
  fl = AllocMem(sizeof(struct FidoLine), MEMF_CLEAR);
  sprintf(fl->text, " * Origin: %s (%d:%d/%d.%d)",
          motpek->origin, ft.fromzone, ft.fromnet, ft.fromnode, ft.frompoint);
  AddTail((struct List *)&ft.text, (struct Node *)fl);
  if(!komm) {
    nummer = motpek->renumber_offset + WriteFidoTextTags(&ft,WFT_MailDir, motpek->dir,
                                                         WFT_Domain, fd->domain,
                                                         WFT_CharSet, motpek->charset,
                                                         TAG_DONE);
  } else {
    nummer = motpek->renumber_offset + WriteFidoTextTags(&ft,WFT_MailDir, motpek->dir,
                                                         WFT_Domain, fd->domain,
                                                         WFT_Reply, msgid,
                                                         WFT_CharSet, motpek->charset,
                                                         TAG_DONE);
  }
  if(motpek->texter < nummer) {
    motpek->texter = nummer;
  }
  SendStringCat("%s\r\n\n", CATSTR(MSG_WRITE_TEXT_GOT_NUMBER), nummer);
  if(Servermem->cfg->logmask & LOG_BREV) {
    LogEvent(USAGE_LOG, INFO, "%s skriver text %d i %s",
             getusername(inloggad), nummer, motpek->namn);
  }
  while((first = (struct MinNode *)RemHead((struct List *)&ft.text))) {
    FreeMem(first,sizeof(struct EditLine));
  }
  NewList((struct List *)&edit_list);
  return 0;
}

void fido_endast(struct Mote *motpek,int antal) {
  struct UnreadTexts *unreadTexts = CUR_USER_UNREAD;
  unreadTexts->lowestPossibleUnreadText[motpek->nummer] = motpek->texter - antal + 1;
  if(unreadTexts->lowestPossibleUnreadText[motpek->nummer] < motpek->lowtext) {
    unreadTexts->lowestPossibleUnreadText[motpek->nummer] = motpek->lowtext;
  }
}

void fidolistaarende(struct Mote *motpek,int dir) {
  int from, i, bryt;
  struct FidoText *ft;
  char fullpath[100], filnamn[20];
  
  if(dir > 0) {
    from = motpek->lowtext;
  } else {
    from = motpek->texter;
  }
  SendString("\r\n\n%s", CATSTR(MSG_FIDO_LIST_TEST_HEAD));
  SendString("\r\n-------------------------------------------------------------------------\r\n");
  for(i = from; i >= motpek->lowtext && i <= motpek->texter; i += dir) {
    strcpy(fullpath, motpek->dir);
    sprintf(filnamn, "%ld.msg", i - motpek->renumber_offset);
    AddPart(fullpath, filnamn, 99);
    ft = ReadFidoTextTags(fullpath, RFT_HeaderOnly, TRUE, TAG_DONE);
    if(!ft) {
      continue;
    }
    ft->date[6] = 0;
    ft->subject[27] = 0;
    bryt = SendString("�name�%-34s�number�%5d�reset� %s �subject�%s�reset�\r\n",
                      ft->fromuser, i, ft->date, ft->subject);
    FreeFidoText(ft);
    if(bryt) {
      break;
    }
  }
}

int SkipSubjectInFidoConf(struct Mote *conf, char *args, int lastReadText) {
  struct FidoText *ft;
  int arglen, skipCount = 0, i;
  char path[100], subject[72];

  if(args[0] == '\0') {
    MakeMsgFilePath(conf->dir, lastReadText - conf->renumber_offset, path);
    if((ft = ReadFidoTextTags(path, RFT_HeaderOnly, TRUE, TAG_DONE)) == NULL) {
      LogEvent(SYSTEM_LOG, ERROR,
               "Could not read fido text '%s' as the latest read text for 'Skip Subject'",
               path);
      DisplayInternalError();
      return 0;
    }
    strcpy(subject, ft->subject);
    args = subject;
    FreeFidoText(ft);
  }

  arglen = strlen(args);
  for(i = CUR_USER_UNREAD->lowestPossibleUnreadText[conf->nummer]; i <= conf->texter; i++) {
    if(IntListFind(g_readRepliesList, i) != -1) {
      continue;
    }
    MakeMsgFilePath(conf->dir, i - conf->renumber_offset, path);
    if((ft = ReadFidoTextTags(path, RFT_HeaderOnly, TRUE, TAG_DONE)) == NULL) {
      LogEvent(SYSTEM_LOG, ERROR,
               "Could not read fido text '%s' when searching for texts in 'Skip Subject'",
               path);
      DisplayInternalError();
      return skipCount;
    }
    if(strncmp(ft->subject, args, arglen) == 0) {
      IntListAppend(g_readRepliesList, i);
      skipCount++;
    }
    FreeFidoText(ft);
  }
  return skipCount;
}
