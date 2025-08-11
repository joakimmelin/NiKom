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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "NiKomStr.h"
#include "Nodes.h"
#include "NiKomFuncs.h"
#include "NiKomLib.h"
#include "InfoFiles.h"
#include "Terminal.h"
#include "UserNotificationHooks.h"
#include "Logging.h"
#include "Languages.h"

#include "NiKEditor.h"

extern struct System *Servermem;
extern char inmat[], crashmail;
extern int rad,nu_skrivs,nodnr, inloggad, g_userDataSlot;
extern struct Library *FifoBase;

char yankbuffer[81],edxxcombuf[257];
int antkol=0,kolpos=0,edxxcommand=FALSE,edxxleft=0;

struct MinList edit_list;
struct EditLine *curline,*tmpline;

struct EditLine *allocEditLine(void);
void quote(struct EditContext *context);
int lineedit(struct EditContext *context);
int linegetline(char *str,char *wrap,int linenr);
void linerenumber(void);
void lineloadfile(char *filename);
void linedelline(char *arg);
int lineinsert(int before);
void lineread(void);
void lineflyttatext(char *vart, struct EditContext *context);
void linearende(char *newSubject, struct EditContext *context);
void lineaddera(char *vem, struct EditContext *context);
int linechange(int foorad);
int linedump(void);
void linequote(struct EditContext *context);
void linecrash(void);
int fulledit(struct EditContext *context);
void fullnormalchr(char tecken);
void fullbackspace(void);
void fulldelete(void);
void fulltab(void);
void fullctrla(void);
void fullctrle(void);
void fullctrlk(void);
void fullctrll(void);
void fullctrlx(void);
void fullctrly(void);
void fullleft(void);
void fullright(void);
void fullup(void);
void fulldown(void);
void fullreturn(void);
int doedkmd(struct EditContext *context);
void fulladdera(char *vem, struct EditContext *context);
void fullflyttatext(char *vart, struct EditContext *context);
void fullarende(char *newSubject, struct EditContext *context);
int fulldumpa(void);
int fullnewline(void);
void fullloadtext(char *filename);
void fulldisplaytext(void);
void fullquote(struct EditContext *context);
void fullcrash(void);
void reprintCurrentLine(void);

int edittext(struct EditContext *context) {
  int ret;
  freeeditlist();
  if(CURRENT_USER->flaggor & FULLSCREEN) {
    ret = fulledit(context);
  } else {
    ret = lineedit(context);
  }
  if(ret) {
    freeeditlist();
  }
  return ret;
}

struct EditLine *allocEditLine(void) {
  struct EditLine *el;
  if(!(el = (struct EditLine *)AllocMem(sizeof(struct EditLine),
                                        MEMF_CLEAR | MEMF_PUBLIC))) {
    LogEvent(SYSTEM_LOG, ERROR, "Couldn't allocate %d bytes.",
             sizeof(struct EditLine));
    DisplayInternalError();
  }
  return el;
}

void quote(struct EditContext *context) {
  struct Mote *conf;
  struct FidoText *ft;
  struct Node *line;
  char filename[10],fullpath[100];

  if((conf = getmotpek(context->replyingInConfId)) == NULL) {
    return;
  }

  sprintf(filename, "%ld.msg", context->replyingToText - conf->renumber_offset);
  strcpy(fullpath, conf->dir);
  AddPart(fullpath, filename, 99);
  ft = ReadFidoTextTags(fullpath,
                        RFT_NoKludges,TRUE,
                        RFT_NoSeenBy, TRUE,
                        RFT_Quote, TRUE,
                        RFT_LineLen, 75,
                        TAG_DONE);
  if(!ft) {
    return;
  }
  while((line = RemHead((struct List *)&ft->text))) {
    AddTail((struct List *)&edit_list, line);
  }
  FreeFidoText(ft);
}

int lineedit(struct EditContext *context) {
  int currow, getret;
  struct EditLine *el;
  char letmp[81], wrap[81];

  NewList((struct List *)&edit_list);
  if(!(CURRENT_USER->flaggor & INGENHELP)) {
    SendStringNoBrk("%s\n\r", CATSTR(MSG_EDIT_LINE_HELP1));
    SendStringNoBrk("%s\n\n\r", CATSTR(MSG_EDIT_LINE_HELP2));
  }
  if(context != NULL && context->fileName != NULL) {
    lineloadfile(context->fileName);
    linerenumber();
  }
  letmp[0] = 0;
  for(;;) {
    wrap[0] = 0;
    el= (struct EditLine *)edit_list.mlh_TailPred;
    if(el->line_node.mln_Pred) {
      currow = el->number+1;
    } else {
      currow = 1;
    }
    getret = linegetline(letmp, wrap, currow);
    switch(getret) {
    case 0:
    case 3:
      if(letmp[0] == '!' && getret == 0) {
        letmp[0] = 0;
        if(letmp[1] == 'r' || letmp[1] == 'R') linedelline(hittaefter(&letmp[1]));
        else if(letmp[1] == 'i' || letmp[1] == 'I') {
          if(lineinsert(atoi(hittaefter(&letmp[1])))) return 1;
        }
        else if(letmp[1] == 'l' || letmp[1] == 'L') lineread();
        else if(letmp[1] == 'h' || letmp[1] == 'H') lineread();
        else if(letmp[1] == 'b' || letmp[1] == 'B') return 2;
        else if(letmp[1] == 'k' || letmp[1] == 'K') return 2;
        else if(letmp[1] == 's' || letmp[1] == 'S') return 0;
        else if(letmp[1] == 'f' || letmp[1] == 'F') {
          lineflyttatext(hittaefter(&letmp[1]), context);
        } else if(letmp[1] == 'a' || letmp[1] == 'A') {
          lineaddera(hittaefter(&letmp[1]), context);
        } else if(letmp[1] == '�' || letmp[1] == '�') {
          if(letmp[2] == 'n' || letmp[2] == 'N') {
            if(linechange(atoi(hittaefter(&letmp[1])))) return 1;
          } else if(letmp[2]=='r' || letmp[2]=='R') {
            linearende(hittaefter(&letmp[1]), context);
          }
        }
        else if(letmp[1] == 'd' || letmp[1] == 'D') linedump();
        else if(letmp[1] == 'c' || letmp[1] == 'C') {
          if(letmp[2] == 'i' || letmp[2] == 'I') linequote(context);
          else if(letmp[2] == 'r' || letmp[2] == 'R') linecrash();
        }
        else if(letmp[1] == '?') SendInfoFile("EditorHelp.txt", 0);
      } else {
        if(!(el=allocEditLine())) {
          return 0;
        }
        strcpy(el->text,letmp);
        el->number=currow++;
        AddTail((struct List *)&edit_list,(struct Node *)el);
        strcpy(letmp,wrap);
        if(getret == 3) {
          return 0;
        }
      }
      break;
      
    case 1 :
      return 1;
    case 2 :
      if(currow>1) {
        el=(struct EditLine *)RemTail((struct List *)&edit_list);
        strcpy(letmp,el->text);
        FreeMem(el,sizeof(struct EditLine));
        currow--;
      }
      break;
    }
  }
}

int linegetline(char *str, char *wrap, int linenr) {
  int col = strlen(str), cnt, i, ch;
  SendStringNoBrk("\r%2d: %s", linenr, str);
  while(col < 75) {
    ch = GetChar();
    if(ch == GETCHAR_LOGOUT) {
      return 1;
    }
    if((ch > 31 && ch < 128) || (ch > 159 && ch <= 255)) {
      str[col++] = ch;
      str[col] = 0;
      eka(ch);
    } else if(ch == GETCHAR_RETURN) {
      SendStringNoBrk("\n");
      return 0;
    } else if(ch == GETCHAR_BACKSPACE) {
      if(col == 0) {
        return 2;
      } else {
        str[--col] = 0;
        SendStringNoBrk("\b \b");
      }
    } else if(ch == '\t' && col < 66) {
      cnt = 8 - (col % 8);
      for(i = 0; i < cnt; i++) {
        str[col++] = ' ';
        eka(' ');
      }
    } else if(ch == 26) { // Ctrl-Z
      if(col != 0) {
        SendStringNoBrk("\r\n");
      }
      SendStringNoBrk("%s\r\n\n", CATSTR(MSG_EDIT_LINE_SAVING));
      return 3;
    }
  }
  if(!wrap) {
    return 0;
  }
  cnt = col + 1;
  while(str[--cnt] != 32 && cnt > 0);
  if(cnt > 0) {
    str[cnt] = 0;
    strcpy(wrap, &str[cnt + 1]);
    for(i = cnt + 1; i < 75; i++) {
      SendStringNoBrk("\b \b");
    }
  } else {
    wrap[0] = str[74];
    str[74] = 0;
    wrap[1] = 0;
    SendStringNoBrk("\b \b");
  }
  SendStringNoBrk("\n\r");
  return 0;
}

void linerenumber(void) {
  struct EditLine *el;
  int x = 1;
  ITER_EL(el, edit_list, line_node, struct EditLine *) {
    el->number=x++;
  }
}

void lineloadfile(char *filename) {
  FILE *fp;
  struct EditLine *el;
  char loadbuff[81];
  if(!(fp = fopen(filename, "r"))) {
    return;
  }
  while(fgets(loadbuff, 77, fp)) {
    loadbuff[strlen(loadbuff) - 1] = 0;
    if(!(el = allocEditLine())) {
      fclose(fp);
      return;
    }
    strcpy(el->text, loadbuff);
    AddTail((struct List *)&edit_list, (struct Node *)el);
  }
  fclose(fp);
}

void linedelline(char *arg) {
  struct EditLine *el;
  int line = atoi(arg);
  ITER_EL(el, edit_list, line_node, struct EditLine *) {
    if(el->number == line) {
      Remove((struct Node *)el);
      FreeMem(el, sizeof(struct EditLine));
      linerenumber();
      break;
    }
  }
}

int lineinsert(int before) {
  struct EditLine *el, *ela;
  int getret, highline;
  char instmp[81] = "";

  el=(struct EditLine *)edit_list.mlh_TailPred;
  if(el->line_node.mln_Pred) {
    highline=el->number+1;
  } else {
    SendStringNoBrk("\n\rFinns ingen rad att infoga framf�r.\n\r");
    return 0;
  }
  if(before < 1 || before > highline) {
    SendStringNoBrk("\n\rFinns ingen s�dan rad!\n\r");
    return 0;
  }
  getret = linegetline(instmp, NULL, before);
  if(getret == 1) {
    return 1;
  }
  ITER_EL(el, edit_list, line_node, struct EditLine *) {
    if(el->number==before) {
      break;
    }
  }
  el = (struct EditLine *)el->line_node.mln_Pred;
  if(!(ela = allocEditLine())) {
    return 0;
  }
  strcpy(ela->text, instmp);
  Insert((struct List *)&edit_list, (struct Node *)ela, (struct Node *)el);
  linerenumber();
  return 0;
}

void lineread(void) {
  struct EditLine *el;
  int x=1;
  SendStringNoBrk("\n\n\r");
  ITER_EL(el, edit_list, line_node, struct EditLine *) {
    SendStringNoBrk("%2d: %s\n\r", x++, el->text);
  }
}

void lineflyttatext(char *vart, struct EditContext *context) {
  int confId;
  if(nu_skrivs!=TEXT) {
    SendStringNoBrk("\r\nDu kan bara flytta texter, inte brev!\r\n");
    return;
  }
  if((confId=parsemot(vart))==-1) {
    SendStringNoBrk("\r\nFinns inget s�dant m�te!\r\n");
    return;
  }
  if(confId==-3) {
    SendStringNoBrk("\r\nSkriv : !flytta <m�tesnamn>\r\n");
    return;
  }
  if(!MayWriteConf(confId, inloggad, CURRENT_USER)
     || !MayReplyConf(confId, inloggad, CURRENT_USER)) {
    SendStringNoBrk("\n\rDu har ingen r�tt att flytta texten till det m�tet.\n\r");
    return;
  }
  *context->confId = confId;
  SendStringNoBrk("\r\nFlyttar texten till m�tet %s.\r\n", getmotnamn(confId));
}

void linearende(char *newSubject, struct EditContext *context) {
  if(context == NULL || context->subject == NULL) {
    SendStringNoBrk("\n\rKan inte �ndra �rendet!\n\r");
    return;
  }
  if(!newSubject[0]) {
    SendStringNoBrk("\r\nSkriv: !�rende <nytt �rende>\r\n");
    return;
  }
  strncpy(context->subject, newSubject, context->subjectMaxLen);
}

int ismotthere(int mot,char *motstr) {
  while(motstr[0]) {
    if(atoi(motstr) == mot) {
      return 1;
    }
    motstr=hittaefter(motstr);
  }
  return 0;
}


void lineaddera(char *vem, struct EditContext *context) {
  int userId;
  char nrbuf[10];
  if(nu_skrivs != BREV) {
    SendStringNoBrk("\n\rDu kan bara addera personer till brev!\n\r");
    return;
  }
  if((userId = parsenamn(vem)) == -1) {
    SendStringNoBrk("\n\rFinns ingen s�dan anv�ndare!\n\r");
    return;
  }
  if(userId == -3) {
    SendStringNoBrk("\n\rSkriv: !Addera <anv�ndare>\n\r");
    return;
  }
  if(ismotthere(userId, context->mailRecipients)) {
    SendStringNoBrk("\n\rAnv�ndaren �r redan mottagare av brevet!\n\r");
    return;
  }
  sprintf(nrbuf, " %d", userId);
  if(strlen(nrbuf) + strlen(context->mailRecipients) > 98) {
    SendStringNoBrk("\n\rKan inte addera flera anv�ndare!\n\r");
    return;
  }
  strcat(context->mailRecipients, nrbuf);
  SendStringNoBrk("\n\rAdderar %s\n\r", getusername(userId));
}

int linechange(int line) {
  struct EditLine *el, *chel = NULL;
  int buffercnt = 0, col = 0, ch;
  char chgbuffer[80];

  ITER_EL(el, edit_list, line_node, struct EditLine *) {
    if(el->number == line) {
      chel = el;
      break;
    }
  }
  if(chel == NULL) {
    SendStringNoBrk("\n\rRaden finns inte!\n\r");
    return 0;
  }
  strcpy(chgbuffer, chel->text);
  SendStringNoBrk("\r\n%2d: ",line);
  for(;;) {
    ch = GetChar();
    if(ch == GETCHAR_LOGOUT) {
      return 1;
    }
    if((ch > 31 && ch < 128) || (ch > 159 && ch <= 255)) {
      if(col < 75) {
        chel->text[col++] = ch;
        chel->text[col] = 0;
        eka(ch);
      } else {
        eka('\a');
      }
    } else if(ch == GETCHAR_RETURN) {
      break;
    } else if(ch == GETCHAR_BACKSPACE) {
      if(col == 0) {
        eka('\a');
      } else {
        chel->text[--col] = 0;
        SendStringNoBrk("\b \b");
      }
    } else if(ch == 6) { // Ctrl-F
      if(col < 75 && chgbuffer[buffercnt]) {
        eka(chgbuffer[buffercnt]);
        chel->text[col++] = chgbuffer[buffercnt++];
        chel->text[col] = 0;
      } else {
        eka('\a');
      }
    } else if(ch == 18) { // Ctrl-R
      while(col<75 && chgbuffer[buffercnt]) {
        eka(chgbuffer[buffercnt]);
        chel->text[col++] = chgbuffer[buffercnt++];
      }
      chel->text[col] = 0;
    }
  }
  SendStringNoBrk("\n");
  return 0;
}

int linedump(void) {
  int ch, col=0;
  struct EditLine *el;
  if(!(el = allocEditLine())) {
    return 0;
  }
  AddTail((struct List *)&edit_list, (struct Node *)el);
  while((ch = GetChar()) != 3) {
    if(ch == GETCHAR_LOGOUT) {
      return 1;
    }
    if(ch > 0 && IsPrintableCharacter(ch)) {
      el->text[col++] = ch;
      if(col > 75) {
        col = 0;
        if(!(el = allocEditLine())) {
          return 0;
        }
        AddTail((struct List *)&edit_list, (struct Node *)el);
      }
    } else if(ch == GETCHAR_RETURN) {
      col = 0;
      if(!(el = allocEditLine())) {
        return 0;
      }
      AddTail((struct List *)&edit_list, (struct Node *)el);
    }
  }
  linerenumber();
  return 0;
}

void linequote(struct EditContext *context) {
  if(context->replyingToText == 0) {
    SendStringNoBrk("\n\rCan't quote.\n\r");
    return;
  }
  quote(context);
  linerenumber();
}

void linecrash(void) {
  if(nu_skrivs != BREV_FIDO) {
    SendStringNoBrk("\n\rEndast Fido-brev kan skickas som crashmail.\n\r");
    return;
  }
  if(CURRENT_USER->status < Servermem->cfg->fidoConfig.crashstatus) {
    SendStringNoBrk("\n\rDu har inte r�tt att skicka crashmail.\n\r");
    return;
  }
  crashmail = TRUE;
  SendStringNoBrk("\n\rBrevet skickas som crashmail.\n\r");
}

/*********** Fullsk�rmseditorn **************/

int fulledit(struct EditContext *context) {
  int ch;
  NewList((struct List *)&edit_list);
  yankbuffer[0] = 0;
  if(!(CURRENT_USER->flaggor & INGENHELP)) {
    SendStringNoBrk("\r\n%s\n\r", CATSTR(MSG_EDIT_FULL_HELP1));
    SendStringNoBrk("%s\n\n\r", CATSTR(MSG_EDIT_FULL_HELP2));
  }
  if(!(curline = allocEditLine())) {
    return 2;
  }
  AddTail((struct List *)&edit_list, (struct Node *)curline);
  antkol = kolpos = 0;
  if(context != NULL && context->fileName != NULL) {
    fullloadtext(context->fileName);
    fulldisplaytext();
  }
  for(;;) {
    ch = GetChar();
    if(ch == GETCHAR_LOGOUT) {
      return 1;
    }
    if(ch > 0 && IsPrintableCharacter(ch)) {
      fullnormalchr(ch);
    }
    else if(ch == GETCHAR_BACKSPACE) fullbackspace();
    else if(ch == GETCHAR_DELETE) fulldelete();
    else if(ch == '\t' && antkol < 66) fulltab();
    else if(ch == GETCHAR_RETURN) fullreturn();
    else if(ch == 17) { if(doedkmd(context)) return 1; }
    else if(ch == GETCHAR_SOL) fullctrla();
    else if(ch == GETCHAR_EOL) fullctrle();
    else if(ch == GETCHAR_LEFT) fullleft();
    else if(ch == GETCHAR_RIGHT) fullright();
    else if(ch == GETCHAR_UP) fullup();
    else if(ch == GETCHAR_DOWN) fulldown();
    else if(ch == 11) fullctrlk();
    else if(ch == 12) fullctrll();
    else if(ch == 18) fullquote(context);
    else if(ch == GETCHAR_DELETELINE) fullctrlx();
    else if(ch == 25) fullctrly();
    else if(ch == 26) {
      SendStringNoBrk("\r\n\n%s\r\n", CATSTR(MSG_EDIT_FULL_SAVING));
      return 0;
    } else if(ch == 3) {
      SendStringNoBrk("\r\n\n%s\r\n", CATSTR(MSG_EDIT_FULL_THROWING));
      return 2;
    }
  }
}

void fullnormalchr(char ch) {
  int cnt;
  if(antkol < MAXFULLEDITTKN - 1) {
    if(kolpos != antkol) {
      SendStringNoBrk("\x1b\x5b\x31\x40");
    }
    eka(ch);
    if(kolpos != antkol) {
      memmove(&curline->text[kolpos + 1], &curline->text[kolpos], antkol - kolpos);
    }
    curline->text[kolpos++] = ch;
    antkol++;
  } else  {
    if(antkol != kolpos) {
      return;
    }
    if(!(tmpline = allocEditLine())) {
      return;
    }
    memmove(&curline->text[kolpos + 1], &curline->text[kolpos], antkol - kolpos);
    curline->text[kolpos++] = ch;
    antkol++;
    curline->text[kolpos] = 0;
    cnt = antkol + 1;
    while(curline->text[--cnt] != 32 && cnt > 0);
    if(cnt > 0) {
      SendStringNoBrk("\r\x1b\x5b%d\x43\x1b\x5b\x4b\n",cnt);
      strcpy(tmpline->text, &curline->text[cnt + 1]);
      memset(&curline->text[cnt], 0, 80 - cnt);
      Insert((struct List *)&edit_list,(struct Node *)tmpline,(struct Node *)curline);
    } else {
      SendStringNoBrk("\n");
      Insert((struct List *)&edit_list,(struct Node *)tmpline,(struct Node *)curline);
      tmpline->text[0] = curline->text[MAXFULLEDITTKN - 1];
      curline->text[MAXFULLEDITTKN - 1] = 0;
    }
    curline = tmpline;
    SendStringNoBrk("\x1b\x5b\x4c\r%s", curline->text);
    antkol = kolpos = strlen(curline->text);
  }
}

void fullbackspace(void) {
  struct EditLine *prevline = (struct EditLine *)curline->line_node.mln_Pred;
  if(kolpos) {
    SendStringNoBrk("\x1b\x5b\x44\x1b\x5b\x50");
    memmove(&curline->text[kolpos - 1], &curline->text[kolpos], antkol - kolpos + 1);
    kolpos--;
    antkol--;
  } else if(prevline->line_node.mln_Pred != NULL) {
    if(strlen(curline->text) + strlen(prevline->text) < MAXFULLEDITTKN) {
      kolpos = strlen(prevline->text);
      strcat(prevline->text, curline->text);
      SendStringNoBrk("\x1b\x5b\x4d");
      Remove((struct Node *)curline);
      FreeMem(curline, sizeof(struct EditLine));
      SendStringNoBrk("\x1b\x5b\x41\r%s", prevline->text);
      curline = prevline;
      antkol = strlen(curline->text);
      if(kolpos) {
        SendStringNoBrk("\r\x1b\x5b%d\x43", kolpos);
      } else {
        eka('\r');
      }
    }
  }
}

void fulldelete(void) {
  struct EditLine *succline = (struct EditLine *)curline->line_node.mln_Succ;
  if(kolpos != antkol) {
    SendStringNoBrk("\x1b\x5b\x50");
    memmove(&curline->text[kolpos], &curline->text[kolpos + 1], antkol - kolpos);
    antkol--;
  } else if(succline->line_node.mln_Succ != NULL) {
    if(strlen(curline->text) + strlen(succline->text) < MAXFULLEDITTKN - 1) {
      strcat(curline->text, succline->text);
      SendStringNoBrk("\n");
      Remove((struct Node *)succline);
      FreeMem(succline, sizeof(struct EditLine));
      SendStringNoBrk("\x1b\x5b\x4d");
      SendStringNoBrk("\x1b\x5b\x41\r%s", curline->text);
      antkol = strlen(curline->text);
      if(kolpos) {
        SendStringNoBrk("\r\x1b\x5b%d\x43", kolpos);
      } else {
        eka('\r');
      }
    }
  }
}

void fulltab(void) {
  int cnt = 8- (kolpos % 8), i;
  SendStringNoBrk("\x1b\x5b%d\x40", cnt);
  memmove(&curline->text[kolpos+cnt], &curline->text[kolpos], antkol - kolpos);
  for(i = 0; i < cnt; i++) {
    curline->text[kolpos + i] = ' ';
    eka(' ');
  }
  kolpos += cnt;
  antkol += cnt;
}

void fullctrla(void) {
  kolpos=0;
  eka('\r');
}

void fullctrle(void) {
  int oldpos = kolpos, move;
  kolpos = strlen(curline->text);
  move = kolpos - oldpos;
  if(move) {
    SendStringNoBrk("\x1b\x5b%d\x43", move);
  }
}

void fullctrlk(void) {
  struct EditLine *succline = (struct EditLine *)curline->line_node.mln_Succ;
  if(antkol != kolpos) {
    SendStringNoBrk("\x1b\x5b\x4b");
    strcpy(yankbuffer, &curline->text[kolpos]);
    memset(&curline->text[kolpos], 0, 80 - kolpos);
    antkol = kolpos;
  } else if(succline->line_node.mln_Succ != NULL) {
    if(strlen(curline->text) + strlen(succline->text) < MAXFULLEDITTKN - 1) {
      strcat(curline->text, succline->text);
      SendStringNoBrk("\n");
      Remove((struct Node *)succline);
      FreeMem(succline,sizeof(struct EditLine));
      SendStringNoBrk("\x1b\x5b\x4d");
      SendStringNoBrk("\x1b\x5b\x41\r%s", curline->text);
      antkol=strlen(curline->text);
      if(kolpos) {
        SendStringNoBrk("\r\x1b\x5b%d\x43", kolpos);
      } else {
        eka('\r');
      }
    }
  }
}

void fullctrll(void) {
  struct EditLine *prevline = (struct EditLine *)curline->line_node.mln_Pred,
    *succline = (struct EditLine *)curline->line_node.mln_Succ;
  eka('\f');
  SendStringNoBrk("%s\n\r", prevline->text);
  SendStringNoBrk("%s\n\r", curline->text);
  SendStringNoBrk("%s\x1b\x5b\x41\r\x1b\x5b%d\x43", succline->text,kolpos);
}

void fullctrlx(void) {
  struct EditLine *succline = (struct EditLine *)curline->line_node.mln_Succ;
  if(succline->line_node.mln_Succ != NULL) {
    Remove((struct Node *)curline);
    strcpy(yankbuffer, curline->text);
    FreeMem(curline, sizeof(struct EditLine));
    SendStringNoBrk("\x1b\x5b\x4d");
    curline = succline;
    antkol = strlen(curline->text);
    kolpos = 0;
    eka('\r');
  } else {
    strcpy(yankbuffer, curline->text);
    memset(curline->text, 0, 80);
    antkol = kolpos = 0;
    SendStringNoBrk("\r\x1b\x5b\x4b");
  }
}

void fullctrly(void) {
  if(strlen(curline->text) + strlen(yankbuffer) < MAXFULLEDITTKN - 1) {
    memmove(&curline->text[kolpos+strlen(yankbuffer)], &curline->text[kolpos],
            antkol - kolpos);
    memmove(&curline->text[kolpos], yankbuffer, strlen(yankbuffer));
    SendStringNoBrk("\r%s", curline->text);
    antkol = strlen(curline->text);
    if(kolpos) {
      SendStringNoBrk("\r\x1b\x5b%d\x43", kolpos);
    } else {
      eka('\r');
    }
  }
}

void fullleft(void) {
  struct EditLine *prevline = (struct EditLine *)curline->line_node.mln_Pred;
  
  if(kolpos) {
    SendStringNoBrk("\x1b\x5b\x44");
    kolpos--;
  } else if(prevline->line_node.mln_Pred) {
    SendStringNoBrk("\x1b\x5b\x41");
    curline = prevline;
    SendStringNoBrk("\r%s", curline->text);
    kolpos = strlen(curline->text);
    if(kolpos) {
      SendStringNoBrk("\r\x1b\x5b%d\x43", kolpos);
    } else {
      eka('\r');
    }
    antkol = strlen(curline->text);
  }
}

void fullright(void) {
  struct EditLine *succline=(struct EditLine *)curline->line_node.mln_Succ;

  if(kolpos < antkol) {
    SendStringNoBrk("\x1b\x5b\x43");
    kolpos++;
  } else if(succline->line_node.mln_Succ) {
    curline = succline;
    SendStringNoBrk("\n\r%s", curline->text);
    kolpos = 0;
    eka('\r');
    antkol = strlen(curline->text);
  }
}  

void fullup(void) {
  struct EditLine *prevline=(struct EditLine *)curline->line_node.mln_Pred;

  if(prevline->line_node.mln_Pred == NULL) {
    return;
  }

  SendStringNoBrk("\x1b\x5b\x41");
  curline = prevline;
  SendStringNoBrk("\r%s",curline->text);
  if(kolpos>strlen(curline->text)) {
    kolpos = strlen(curline->text);
  }
  antkol = strlen(curline->text);
  if(kolpos) {
    SendStringNoBrk("\r\x1b\x5b%d\x43", kolpos);
  } else {
    eka('\r');
  }
}

void fulldown(void) {
  struct EditLine *succline=(struct EditLine *)curline->line_node.mln_Succ;

  if(succline->line_node.mln_Succ == NULL) {
    return;
  }

  curline = succline;
  SendStringNoBrk("\n\r%s", curline->text);
  if(kolpos>strlen(curline->text)) {
    kolpos=strlen(curline->text);
  }
  antkol = strlen(curline->text);
  if(kolpos) {
    SendStringNoBrk("\r\x1b\x5b%d\x43", kolpos);
  } else {
    eka('\r');
  }
}

void fullreturn(void) {
  if(!(tmpline = allocEditLine())) {
    return;
  }
  if(kolpos == antkol) {
    SendStringNoBrk("\n");
    Insert((struct List *)&edit_list, (struct Node *)tmpline, (struct Node *)curline);
    curline = tmpline;
    SendStringNoBrk("\x1b\x5b\x4c\r");
    antkol = kolpos = 0;
  } else {
    SendStringNoBrk("\x1b\x5b\x4b\n");
    Insert((struct List *)&edit_list, (struct Node *)tmpline, (struct Node *)curline);
    strcpy(tmpline->text, &curline->text[kolpos]);
    memset(&curline->text[kolpos], 0, 80 - kolpos);
    kolpos = 0;
    curline = tmpline;
    antkol = strlen(curline->text);
    SendStringNoBrk("\x1b\x5b\x4c\r%s\r", curline->text);
  }
}

int doedkmd(struct EditContext *context) {
  SendStringNoBrk("\r\x1b\x5b\x4b\x1b\x5b\x31\x6d Kommando: ");
  if(GetString(45, NULL)) return 1;
  if(inmat[0]=='f' || inmat[0]=='F') fullflyttatext(hittaefter(inmat), context);
  else if(inmat[0]=='a' || inmat[0]=='A') fulladdera(hittaefter(inmat), context);
  else if(inmat[0]=='d' || inmat[0]=='D') { if(fulldumpa()) return 1; }
  else if(inmat[0]=='�' || inmat[0]=='�') fullarende(hittaefter(inmat), context);
  else if(inmat[0]=='c' || inmat[0]=='C') fullcrash();
  else if(inmat[0]=='?' || inmat[0]=='h' || inmat[0]=='H') {
    SendStringNoBrk("\rCtrl-Z=Spara, Ctrl-C=Avbryt, 'Info Full' f�r mer hj�lp. <RETURN>");
    GetChar();
  } else {
    SendStringNoBrk("\rFelaktigt kommando! <RETURN>");
    GetChar();
  }
  reprintCurrentLine();
  return 0;
}

void reprintCurrentLine(void) {
  SendStringNoBrk("\r\x1b\x5b\x4b\x1b\x5b\x30\x6d");
  SendStringNoBrk("\r%s", curline->text);
  if(kolpos) {
    SendStringNoBrk("\r\x1b\x5b%d\x43", kolpos);
  } else {
    eka('\r');
  }
}

void fulladdera(char *userName, struct EditContext *context) {
  int userId;
  char nrbuf[10];
  if(nu_skrivs != BREV) {
    SendStringNoBrk("\rDu kan bara addera personer till brev! <RETURN>");
    GetChar();
    return;
  }
  if((userId = parsenamn(userName)) == -1) {
    SendStringNoBrk("\rFinns ingen s�dan anv�ndare! <RETURN>");
    GetChar();
    return;
  }
  if(userId == -3) {
    SendStringNoBrk("\rSkriv: Addera <anv�ndare> <RETURN>");
    GetChar();
    return;
  }
  if(ismotthere(userId, context->mailRecipients)) {
    SendStringNoBrk("\rAnv�ndaren �r redan mottagare av brevet! <RETURN>");
    GetChar();
    return;
  }
  sprintf(nrbuf, " %d", userId);
  if(strlen(nrbuf) + strlen(context->mailRecipients) > 98) {
    SendStringNoBrk("\rKan inte addera flera anv�ndare! <RETURN>");
    GetChar();
    return;
  }
  strcat(context->mailRecipients, nrbuf);
  SendStringNoBrk("\rAdderar %s <RETURN>", getusername(userId));
  GetChar();
}

void fullflyttatext(char *confName, struct EditContext *context) {
  int confId;
  if(nu_skrivs != TEXT) {
    SendStringNoBrk("\rDu kan bara flytta texter i m�ten! <RETURN>");
    GetChar();
    return;
  }
  if((confId=parsemot(confName))==-1) {
    SendStringNoBrk("\rFinns inget s�dant m�te! <RETURN>");
    GetChar();
    return;
  }
  if(confId==-3) {
    SendStringNoBrk("\rSkriv : flytta <m�tesnamn> <RETURN>");
    GetChar();
    return;
  }
  if(!MayWriteConf(confId, inloggad, CURRENT_USER)
     || !MayReplyConf(confId, inloggad, CURRENT_USER)) {
    SendStringNoBrk("\rDu har ingen r�tt att flytta texten till det m�tet. <RETURN>");
    return;
  }
  *context->confId = confId;
  SendStringNoBrk("\rFlyttar texten till m�tet %s. <RETURN>", getmotnamn(confId));
  GetChar();
}

void fullarende(char *newSubject, struct EditContext *context) {
  if(context == NULL || context->subject == NULL) {
    SendStringNoBrk("\rKan inte �ndra �rende! <RETURN>");
    GetChar();
    return;
  }
  if(!newSubject[0]) {
    SendStringNoBrk("\rSkriv: �rende <nytt �rende>  <RETURN>");
    GetChar();
    return;
  }
  strncpy(context->subject, newSubject, context->subjectMaxLen);
}

int fulldumpa(void) {
  int ch, col=0;
  if(antkol && fullnewline()) {
    return 0;
  }
  SendStringNoBrk("\r\n\x1b\x5b\x4b\x1b\x5b\x31\x6d Ok, tar emot text utan eko. "
                  "Tryck Ctrl-C f�r att �terg�.\r\n");
  while((ch = GetChar()) != 3) {
    if(ch == GETCHAR_LOGOUT) {
      return 1;
    }
    if(ch > 0 && IsPrintableCharacter(ch)) {
      curline->text[col++] = ch;
      if(col > MAXFULLEDITTKN - 1) {
        col=0;
        if(fullnewline()) {
          return 0;
        }
      }
    } else if(ch == GETCHAR_RETURN) {
      col=0;
      if(fullnewline()) {
        return 0;
      }
    }
  }
  SendStringNoBrk("\r\x1b\x5b\x4b\x1b\x5b\x30\x6d");
  if(fullnewline()) {
    return 0;
  }
  antkol = kolpos = 0;
  return 0;
}

int fullnewline(void) {
  if(!(tmpline = allocEditLine())) {
    return 1;
  }
  Insert((struct List *)&edit_list, (struct Node *)tmpline, (struct Node *)curline);
  curline = tmpline;
  return 0;
}

void fullloadtext(char *filename) {
  FILE *fp;
  if(!(fp = fopen(filename, "r"))) {
    return;
  }
  while(fgets(curline->text, MAXFULLEDITTKN + 1, fp)) {
    if(strlen(curline->text) > MAXFULLEDITTKN) {
      curline->text[strlen(curline->text)-1]=0;
    } else {
      if(curline->text[strlen(curline->text)-1] == '\n')
        curline->text[strlen(curline->text)-1] = 0;
    }
    if(fullnewline()) {
      break;
    }
  }
  fclose(fp);
}

void fulldisplaytext(void) {
  struct EditLine *el;

  ITER_EL(el, edit_list, line_node, struct EditLine *) {
    SendStringNoBrk("\r\n%s", el->text);
  }
  eka('\r');
  curline = (struct EditLine *)edit_list.mlh_TailPred;
  antkol = strlen(curline->text);
  kolpos = 0;
}

void fullquote(struct EditContext *context) {
  if(context->replyingToText == 0) {
    SendStringNoBrk("\rCan't quote.  <RETURN>");
    GetChar();
    reprintCurrentLine();
    return;
  }
  SendStringNoBrk("\r\x1b\x5b\x4a");
  quote(context);
  fulldisplaytext();
}

void fullcrash(void) {
  if(nu_skrivs != BREV_FIDO) {
    SendStringNoBrk("\rEndast Fido-brev kan skickas som crashmail. <RETURN>");
    GetChar();
    return;
  }
  if(CURRENT_USER->status < Servermem->cfg->fidoConfig.crashstatus) {
    SendStringNoBrk("\rDu har inte r�tt att skicka crashmail. <RETURN>");
    GetChar();
    return;
  }
  crashmail = TRUE;
  SendStringNoBrk("\rBrevet skickas som crashmail. <RETURN>");
  GetChar();
}
