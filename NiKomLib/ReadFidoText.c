/* Hur man letar upp from och to i en Fido-text. Fr�n ANet.
* +------+-----------+-----------+
* !      !  Brev     !   Inl�gg  !
* +------+-----------+-----------+
* !Till  !  DOMAIN   !           !
* !      !  INTL     !           !
* !      !  FTS-1    !           !
* +------+-----------+-----------+
* !Fr�n  !  REPLYTO  !   REPLYTO ! Antar att REPLY ska vara "Till". L�ggs mellan INTL och FTS
* !      !  DOMAIN   !   ORIGIN  !
* !      !  INTL     !           !
* !      !  MSGID    !   MSGID   !
* !      !  FTS-1    !           !
* +------+-----------+-----------+
*/

#include "NiKomCompat.h"
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/datetime.h>
#include <utility/tagitem.h>
#include <proto/dos.h>
#include <proto/exec.h>
#ifdef HAVE_PROTO_ALIB_H
/* For NewList() */
# include <proto/alib.h>
#endif
#include <proto/utility.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "NiKomLib.h"
#include "NiKomBase.h"
#include "Funcs.h"
#include "VersionStrings.h"
#include "Logging.h"

static int getfidoline(char *fidoline, char *buffer, int linelen,
		       enum nikom_chrs chrs, BPTR fh, char *quotepre,
		       struct NiKomBase *NiKomBase) {
  int anttkn,foo,tmpret,hasquoted=FALSE,donotwordwrap=FALSE;
  char fidobuf[8];
  unsigned fidoidx;
  UBYTE tmp,*findquotesign,insquotebuf[81];
  strcpy(fidoline,buffer);
  anttkn=strlen(fidoline);
  buffer[0]=0;
  fidoidx = 0;
  for(;;) {
    tmpret=FGetC(fh);
    if(tmpret==-1) return(FALSE);
    tmp=tmpret;
    if(tmp==0x8d) continue;
    if(tmp==0x0a) continue;
    if(tmp==0x00) continue;
    if(tmp==0x0d) {
      if(quotepre && !hasquoted) {                // Om detta �r sant s� har ett return kommit
        if(fidoline[0]!=1 && fidoline[0]!=0) {   // men raden var kortare �n fem tecken s�
          findquotesign=strchr(fidoline,'>');   // den har inte blivit citerad.
          if(findquotesign) {                   // Det kan ocks� vara s� att str�ngen som
            strcpy(insquotebuf,findquotesign); // kommer i buffervariabeln f�ljs direkt av
            findquotesign[1]=0;                // return.
            strcat(fidoline,insquotebuf);
          } else {
            strcpy(insquotebuf,fidoline);
            strcpy(fidoline,quotepre);
            strcat(fidoline,insquotebuf);
          }
        }
      }
      break;
    }
    fidobuf[fidoidx] = tmp;
    switch(chrs) {
    case CHRS_CP437 :
      if(tmp<128) fidoline[anttkn++]=tmp;
      else fidoline[anttkn++]=NiKomBase->IbmToAmiga[tmp];
      break;
    case CHRS_CP850 :
      if(tmp<128) fidoline[anttkn++]=tmp;
      else fidoline[anttkn++]=NiKomBase->CP850ToAmiga[tmp];
      break;
    case CHRS_CP866 :
      if(tmp<128) fidoline[anttkn++]=tmp;
      else fidoline[anttkn++]=NiKomBase->CP866ToAmiga[tmp];
      break;
    case CHRS_SIS7 :
      fidoline[anttkn++]=NiKomBase->SF7ToAmiga[tmp];
      break;
    case CHRS_MAC :
      if(tmp<128) fidoline[anttkn++]=tmp;
      else fidoline[anttkn++]=NiKomBase->MacToAmiga[tmp];
      break;
    case CHRS_LATIN1 :
    case CHRS_PASSTHROUGH:
      fidoline[anttkn++]=tmp;
      break;
    case CHRS_UTF8 :
      tmpret = convUTF8ToAmiga(fidoline+anttkn, fidobuf, fidoidx+1);
      if (tmpret < 0) {
        /* Need more bytes, continue reading. */
        ++fidoidx;
        continue;
      }
      anttkn += tmpret;
      fidoidx = 0;
      break;
    case CHRS_UNKNOWN :
      fidoline[anttkn++]=convnokludge(tmp);
      break;
    }
    fidoline[anttkn]=0;
    if(quotepre && !hasquoted && anttkn>=5) {    // N�r antal tecken �verstiger fem �r det dags
      hasquoted = TRUE;                         // att kolla om raden redan �r ett citat eller
      if(fidoline[0]!=1) {                      // inte. Om det �r det ska bara ett extra '>'
        findquotesign=strchr(fidoline,'>');    // in. Annars ska hela quotepre in.
        if(findquotesign) {
          if(linelen<79) linelen = 79;
          donotwordwrap = TRUE;
          strcpy(insquotebuf,findquotesign);
          findquotesign[1]=0;
          strcat(fidoline,insquotebuf);
        } else {
          strcpy(insquotebuf,fidoline);
          strcpy(fidoline,quotepre);
          strcat(fidoline,insquotebuf);
        }
        anttkn=strlen(fidoline);
      }
    }
    if(anttkn>=linelen) {
      for(foo=anttkn;fidoline[foo]!=' ' && foo>0;foo--);
      if(foo!=0) {
        if(quotepre && foo <= strlen(quotepre)); // Om hela citatet �r ett ord, wrappa inte
        else if(!donotwordwrap) {
          fidoline[foo]=0;
          strncpy(buffer,&fidoline[foo+1],anttkn-foo-1);
          buffer[anttkn-foo-1]=0;
        }
      }
      break;
    }
  }
  return(TRUE);
}

char *hittaefter(strang)
char *strang;
{
	int test=TRUE;
	while(test) {
		test=FALSE;
		while(*(++strang)!=' ' && *strang!=0);
		if(*(strang+1)==' ') test=TRUE;
	}
	return(*strang==0 ? strang : ++strang);
}

int getzone(char *adr) {
	int x;
	for(x=0;adr[x]!=':' && adr[x]!=' ' && adr[x]!=0;x++);
	if(adr[x]==':') return(atoi(adr));
	else return(0);
}

int getnet(char *adr) {
	int x;
	char *pek;
	for(x=0;adr[x]!=':' && adr[x]!=' ' && adr[x]!=0;x++);
	if(adr[x]==':') pek=&adr[x+1];
	else pek=adr;
	return(atoi(pek));
}

int getnode(char *adr) {
	int x;
	for(x=0;adr[x]!='/' && adr[x]!=' ' && adr[x]!=0;x++);
	return(atoi(&adr[x+1]));
}

int getpoint(char *adr) {
	int x;
	for(x=0;adr[x]!='.' && adr[x]!=' ' && adr[x]!=0;x++);
	if(adr[x]=='.') return(atoi(&adr[x+1]));
	else return(0);
}

// Handle the weird MSGID strings that SynchroNet produces. It produces
// MSGIDs on the format <something>@zone:net/node.point. If we find an
// '@' before a ':', then return the string that starts after the '@'.
// Otherwise return the original string.
// We can not rely on just finding the '@' since some systems (including
// NiKom..) produces MSGIDs like zone:net/node.point@FidoNet.
static UBYTE *findMsgIdStart(UBYTE *str) {
  UBYTE *tmpStr;
  for(tmpStr = str; *tmpStr != '\0'; tmpStr++) {
    if(*tmpStr == '@') return tmpStr+1;
    if(*tmpStr == ':') return str;
  }
  return str;
}

static USHORT getTwoByteField(UBYTE *bytes, char littleEndian) {
  if(littleEndian) {
    return (USHORT) (bytes[1] * 0x100 + bytes[0]);
  } else {
    return (USHORT) (bytes[0] * 0x100 + bytes[1]);
  }
}

static void writeTwoByteField(USHORT value, UBYTE *bytes, char littleEndian) {
  if(littleEndian) {
    bytes[1] = value / 0x100;
    bytes[0] = value % 0x100;
  } else {
    bytes[0] = value / 0x100;
    bytes[1] = value % 0x100;
  }
}

struct FidoText * __saveds AASM LIBReadFidoText(register __a0 char *filename AREG(a0),
                                                register __a1 struct TagItem *taglist AREG(a1),
                                                register __a6 struct NiKomBase *NiKomBase AREG(a6)) {
  int nokludge, noseenby, chrset=0, x, tearlinefound=FALSE, headeronly, quote,
    linelen, passthroughCharset;
  struct FidoText *fidotext;
  struct FidoLine *fltmp;
  BPTR fh;
  UBYTE intl[30], replyto[20], origin[20], topt[5], fmpt[5], ftshead[190],
    fidoline[81],flbuffer[81],*foo,prequote[10], *msgId;
  int bytes;
  
  if(!NiKomBase->Servermem) return(NULL);
  
  intl[0]=replyto[0]=origin[0]=topt[0]=fmpt[0]=0;
  nokludge=GetTagData(RFT_NoKludges,0,taglist);
  noseenby=GetTagData(RFT_NoSeenBy,0,taglist);
  headeronly=GetTagData(RFT_HeaderOnly,0,taglist);
  quote = GetTagData(RFT_Quote,0,taglist);
  linelen = GetTagData(RFT_LineLen,79,taglist);
  passthroughCharset = GetTagData(RFT_PassthroughCharset, CHRS_PASSTHROUGH, taglist);
  
  if(!(fidotext = (struct FidoText *)AllocMem(sizeof(struct FidoText),MEMF_CLEAR))) return(NULL);
  NewList((struct List *)&fidotext->text);
  if(!(fh=Open(filename,MODE_OLDFILE))) {
    FreeMem(fidotext,sizeof(struct FidoText));
    return(NULL);
  }
  Read(fh,ftshead,190);
  /* Re-extracted after we know charset. */
  strcpy(fidotext->fromuser,&ftshead[0]);
  /* touser and subject extracted after we know charset. */
  strcpy(fidotext->date,&ftshead[144]);
  fidotext->tonode = getTwoByteField(&ftshead[166], NIK_LITTLE_ENDIAN);
  fidotext->fromnode = getTwoByteField(&ftshead[168], NIK_LITTLE_ENDIAN);
  fidotext->fromnet = getTwoByteField(&ftshead[172], NIK_LITTLE_ENDIAN);
  fidotext->tonet = getTwoByteField(&ftshead[174], NIK_LITTLE_ENDIAN);
  fidotext->attribut = getTwoByteField(&ftshead[186], NIK_LITTLE_ENDIAN);
  Flush(fh);
  if(quote) {
    prequote[0]=' ';
    prequote[1]=0;
    x=1;
    foo = flbuffer; // Temporarily borrowing flbuffer...
    bytes = LIBConvMBChrsToAmiga(flbuffer, fidotext->fromuser, 0, 0,
                                 NiKomBase);
    flbuffer[bytes] = '\0';
    while(foo[0]) {
      prequote[x]=foo[0];
      prequote[x+1]=0;
      x++;
      foo = hittaefter(foo);
    }
    prequote[x]='>';
    prequote[x+1]=' ';
    prequote[x+2]=0;
  }
  flbuffer[0]=0;
  while(getfidoline(fidoline,
                    flbuffer,
                    linelen,
                    chrset == passthroughCharset ? CHRS_PASSTHROUGH : chrset,
                    fh,
                    quote ? prequote : NULL,
                    NiKomBase)) {
    if(fidoline[0]==1) {
      foo=hittaefter(&fidoline[1]);
      if(!strncmp(&fidoline[1],"INTL",4)) strcpy(intl,foo);
      else if(!strncmp(&fidoline[1],"TOPT",4)) strcpy(topt,foo);
      else if(!strncmp(&fidoline[1],"FMPT",4)) strcpy(fmpt,foo);
      else if(!strncmp(&fidoline[1],"MSGID",5)) {
        strncpy(fidotext->msgid,foo,49);
        fidotext->msgid[49]=0;
      } else if(!strncmp(&fidoline[1],"REPLY",5)) {
        strncpy(replyto,foo,19);
        replyto[19]=0;
      } else if(!strncmp(&fidoline[1],"CHRS",4)) {
        if(!strncmp(foo, "LATIN-1 2", 9)) {
          chrset = CHRS_LATIN1;
        } else if(!strncmp(foo, "CP437 2", 7) ||
                  !strncmp(foo, "IBMPC 2", 7)) {
          chrset = CHRS_CP437;
        } else if(!strncmp(foo, "CP850 2", 7)) {
          chrset = CHRS_CP850;
        } else if(!strncmp(foo, "CP866 2", 7)) {
          chrset = CHRS_CP866;
        } else if(!strncmp(foo, "SWEDISH 1", 9)) {
          chrset = CHRS_SIS7;
        } else if(!strncmp(foo, "CP10000 2", 9) ||
                  !strncmp(foo, "MAC 2", 5)) {
          chrset = CHRS_MAC;
        } else if(!strncmp(foo, "UTF-8 4", 7) ||
                  !strncmp(foo, "UTF-8 2", 7)) {
          /* The latter variant is incorrect, but quite common in
             practice. */
          chrset = CHRS_UTF8;
        }
      }
      if(nokludge) continue;
    }
    if(quote) foo=hittaefter(&fidoline[1]);
    else foo=fidoline;
    if(tearlinefound && noseenby &&  !strncmp(foo,"SEEN-BY:",8)) continue;
    if(tearlinefound && !strncmp(foo," * Origin:",10)) {
      for(x=strlen(fidoline)-1;x>0 && fidoline[x]!='('; x--);
      if(x>0) {
        strncpy(origin,&fidoline[x+1],19);
        origin[19]=0;
      }
    }
    if(!strncmp(foo,"---",3) && foo[3]!='-') tearlinefound=TRUE;
    if(tearlinefound && quote) {
      continue;
    }
    if(headeronly) continue;
    if(!(fltmp = (struct FidoLine *)AllocMem(sizeof(struct FidoLine),MEMF_CLEAR))) {
      Close(fh);
      while((fltmp=(struct FidoLine *)RemHead((struct List *)&fidotext->text))) FreeMem(fltmp,sizeof(struct FidoLine));
      FreeMem(fidotext,sizeof(struct FidoText));
      return(NULL);
    }
    strcpy(fltmp->text,fidoline);
    AddTail((struct List *)&fidotext->text,(struct Node *)fltmp);
  }
  Close(fh);
  fidotext->charset = chrset;
  if(fidotext->attribut & FIDOT_PRIVATE) {
    if(fidotext->msgid[0]) {
      msgId = findMsgIdStart(fidotext->msgid);
      if((x=getzone(msgId))) fidotext->fromzone=x;
      if((x=getnet(msgId))) fidotext->fromnet=x;
      if((x=getnode(msgId))) fidotext->fromnode=x;
      if((x=getpoint(msgId))) fidotext->frompoint=x;
    }
    if(replyto[0]) {
      if((x=getzone(replyto))) fidotext->tozone=x;
      if((x=getnet(replyto))) fidotext->tonet=x;
      if((x=getnode(replyto))) fidotext->tonode=x;
      if((x=getpoint(replyto))) fidotext->topoint=x;
    }
    if(intl[0]) {
      if((x=getzone(intl))) fidotext->tozone=x;
      if((x=getnet(intl))) fidotext->tonet=x;
      if((x=getnode(intl))) fidotext->tonode=x;
      foo=hittaefter(intl);
      if((x=getzone(foo))) fidotext->fromzone=x;
      if((x=getnet(foo))) fidotext->fromnet=x;
      if((x=getnode(foo))) fidotext->fromnode=x;
    }
    if(topt[0]) fidotext->topoint = atoi(topt);
    if(fmpt[0]) fidotext->frompoint = atoi(fmpt);
  } else {
    if(fidotext->msgid[0]) {
      msgId = findMsgIdStart(fidotext->msgid);
      if((x=getzone(msgId))) fidotext->fromzone=x;
      if((x=getnet(msgId))) fidotext->fromnet=x;
      if((x=getnode(msgId))) fidotext->fromnode=x;
      if((x=getpoint(msgId))) fidotext->frompoint=x;
    }
    if(replyto[0]) {
      if((x=getzone(replyto))) fidotext->tozone=x;
      if((x=getnet(replyto))) fidotext->tonet=x;
      if((x=getnode(replyto))) fidotext->tonode=x;
      if((x=getpoint(replyto))) fidotext->topoint=x;
    }
    if(origin[0]) {
      if((x=getzone(origin))) fidotext->fromzone=x;
      if((x=getnet(origin))) fidotext->fromnet=x;
      if((x=getnode(origin))) fidotext->fromnode=x;
      if((x=getpoint(origin))) fidotext->frompoint=x;
    }
  }
  if(fidotext->tozone==0) fidotext->tozone=fidotext->fromzone;
  
  bytes = LIBConvMBChrsToAmiga(fidotext->fromuser, &ftshead[0], 0,
                               chrset, NiKomBase);
  fidotext->fromuser[bytes] = '\0';
  bytes = LIBConvMBChrsToAmiga(fidotext->touser, &ftshead[36], 0,
                               chrset, NiKomBase);
  fidotext->touser[bytes] = '\0';
  bytes = LIBConvMBChrsToAmiga(fidotext->subject, &ftshead[72], 0,
                               chrset, NiKomBase);
  fidotext->subject[bytes] = '\0';
  return(fidotext);
}

void __saveds AASM LIBFreeFidoText(register __a0 struct FidoText *fidotext AREG(a0)) {
	struct FidoLine *fltmp;
	while((fltmp=(struct FidoLine *)RemHead((struct List *)&fidotext->text))) FreeMem(fltmp,sizeof(struct FidoLine));
	FreeMem(fidotext,sizeof(struct FidoText));
}

int gethwm(char *dir, char littleEndian) {
	BPTR fh;
	int nummer;
	char fullpath[100], buf[2];
	strcpy(fullpath,dir);
	AddPart(fullpath,"1.msg",99);
	if(!(fh=Open(fullpath,MODE_OLDFILE))) return(0);
	Seek(fh,184,OFFSET_BEGINNING);
        buf[0] = FGetC(fh);
        buf[1] = FGetC(fh);
	nummer = getTwoByteField(buf, littleEndian);
	Close(fh);
	return(nummer);
}

int sethwm(char *dir, int nummer, char littleEndian) {
	BPTR fh;
	char fullpath[100], buf[2];
	strcpy(fullpath,dir);
	AddPart(fullpath,"1.msg",99);
	if(!(fh=Open(fullpath,MODE_OLDFILE))) return(0);
	Seek(fh,184,OFFSET_BEGINNING);
        writeTwoByteField(nummer, buf, littleEndian);
	FPutC(fh, buf[0]);
	FPutC(fh, buf[1]);
	Close(fh);
	return(1);
}

int __saveds AASM LIBWriteFidoText(register __a0 struct FidoText *fidotext AREG(a0), register __a1 struct TagItem *taglist AREG(a1),register __a6 struct NiKomBase *NiKomBase AREG(a6)) {
	BPTR lock,fh;
	struct FidoLine *fl,*next;
	int nummer, going=TRUE, charset, bytes, hwmToLog = 0;
	UBYTE *dir,filename[20],fullpath[100],ftshead[190],*domain,*reply,flowchar,datebuf[14],timebuf[10];
	struct DateTime dt;

	if(!NiKomBase->Servermem) return(FALSE);

	dir=(UBYTE *)GetTagData(WFT_MailDir,0,taglist);
	charset=GetTagData(WFT_CharSet,CHRS_LATIN1,taglist);
	domain=(UBYTE *)GetTagData(WFT_Domain,0,taglist);
	reply=(UBYTE *)GetTagData(WFT_Reply,0,taglist);
	if(!dir) return(0);
	memset(ftshead,0,190);
	strncpy(&ftshead[0],fidotext->fromuser,35);
	bytes = LIBConvMBChrsFromAmiga(&ftshead[36], fidotext->touser, 0,
				       charset, 35, NiKomBase);
	ftshead[36+bytes] = '\0';
	bytes = LIBConvMBChrsFromAmiga(&ftshead[72], fidotext->subject, 0,
				       charset,71, NiKomBase);
	ftshead[72+bytes] = '\0';
	strncpy(&ftshead[144],fidotext->date,19);
        writeTwoByteField(fidotext->tonode, &ftshead[166], NIK_LITTLE_ENDIAN);
        writeTwoByteField(fidotext->fromnode, &ftshead[168], NIK_LITTLE_ENDIAN);
        writeTwoByteField(fidotext->fromnet, &ftshead[172], NIK_LITTLE_ENDIAN);
        writeTwoByteField(fidotext->tonet, &ftshead[174], NIK_LITTLE_ENDIAN);
        writeTwoByteField(fidotext->tozone, &ftshead[176], NIK_LITTLE_ENDIAN);
        writeTwoByteField(fidotext->fromzone, &ftshead[178], NIK_LITTLE_ENDIAN);
        writeTwoByteField(fidotext->topoint, &ftshead[180], NIK_LITTLE_ENDIAN);
        writeTwoByteField(fidotext->frompoint, &ftshead[182], NIK_LITTLE_ENDIAN);
        writeTwoByteField(fidotext->attribut, &ftshead[186], NIK_LITTLE_ENDIAN);
	if(nummer = gethwm(dir, NIK_LITTLE_ENDIAN)) {
          hwmToLog = nummer;
        } else {
          nummer = 2;
        }
	while(going) {
		sprintf(filename,"%d.msg",nummer);
		strcpy(fullpath,dir);
		AddPart(fullpath,filename,99);
		lock = Lock(fullpath, ACCESS_READ);
		if(lock) {
			UnLock(lock);
			nummer++;
		} else going=FALSE;
	}
        LogEvent(NiKomBase->Servermem, FIDO_LOG, INFO,
                 "Saving new message %s, first available number counting up from %d (%s)",
                 fullpath,
                 hwmToLog != 0 ? hwmToLog : 2,
                 hwmToLog != 0 ? "which is HWM in 1.msg" : "which is default HWM since no 1.msg was found");
	if(!(fh=Open(fullpath,MODE_NEWFILE))) {
          LogEvent(NiKomBase->Servermem, FIDO_LOG, ERROR,
                   "Error creating %s.", fullpath);
          return FALSE;
        }
	if(Write(fh,ftshead,190)==-1) {
		Close(fh);
		return(FALSE);
	}
	Flush(fh);
	if(domain) sprintf(ftshead,"\001MSGID: %d:%d/%d.%d@%s %lx\r",fidotext->fromzone,fidotext->fromnet,
		fidotext->fromnode, fidotext->frompoint, domain, time(NULL));
	else sprintf(ftshead,"\001MSGID: %d:%d/%d.%d %ld\r",fidotext->fromzone,fidotext->fromnet,
		fidotext->fromnode, fidotext->frompoint, time(NULL));
	FPuts(fh,ftshead);
	if(reply) {
		sprintf(ftshead,"\001REPLY: %s\r",reply);
		FPuts(fh,ftshead);
	}
	switch(charset) {
		case CHRS_CP437 :
			strcpy(ftshead,"\001CHRS: CP437 2\r");
			break;
		case CHRS_CP850 :
			strcpy(ftshead,"\001CHRS: CP850 2\r");
			break;
		case CHRS_CP866 :
			strcpy(ftshead,"\001CHRS: CP866 2\r");
			break;
		case CHRS_SIS7 :
			strcpy(ftshead,"\001CHRS: SWEDISH 1\r");
			break;
		case CHRS_MAC :
			strcpy(ftshead,"\001CHRS: CP10000 2\r");
			break;
		case CHRS_LATIN1 :
		default :
			strcpy(ftshead,"\001CHRS: LATIN-1 2\r");
			break;
	}
	FPuts(fh,ftshead);
	if(fidotext->attribut & FIDOT_PRIVATE) {
		sprintf(ftshead,"\001INTL %d:%d/%d %d:%d/%d\r",fidotext->tozone,fidotext->tonet,fidotext->tonode,
			fidotext->fromzone,fidotext->fromnet,fidotext->fromnode);
		FPuts(fh,ftshead);
		if(fidotext->frompoint) {
			sprintf(ftshead,"\001FMPT %d\r",fidotext->frompoint);
			FPuts(fh,ftshead);
		}
		if(fidotext->topoint) {
			sprintf(ftshead,"\001TOPT %d\r",fidotext->topoint);
			FPuts(fh,ftshead);
		}
	}
	FPuts(fh, "\001PID: NiKom " NIKRELEASE "\r");
	for(fl=(struct FidoLine *)fidotext->text.mlh_Head;fl->line_node.mln_Succ;fl=(struct FidoLine *)fl->line_node.mln_Succ) {
		next=(struct FidoLine *)fl->line_node.mln_Succ;
		bytes = LIBConvMBChrsFromAmiga(ftshead, fl->text, 0, charset,
					       sizeof(ftshead) - 1, NiKomBase);
		ftshead[bytes] = '\0';
		if(!next->line_node.mln_Succ) next=NULL;
		flowchar='\r';
		if(next) {
			if(next->text[0] == ' ' || next->text[0]=='\t' || next->text[0]==0) flowchar = '\r';
			else flowchar=' ';
		}
		if(strlen(ftshead)<50) flowchar='\r';
		FPuts(fh,ftshead);
		FPutC(fh,flowchar);
	}
	if(fidotext->attribut & FIDOT_PRIVATE) {
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
		if(domain) sprintf(ftshead,"\001Via NiKom %d:%d/%d.%d@%s %s %s\r",fidotext->fromzone,fidotext->fromnet,
			fidotext->fromnode,fidotext->frompoint,domain,datebuf,timebuf);
		else sprintf(ftshead,"\001Via NiKom %d:%d/%d.%d %s %s\r",fidotext->fromzone,fidotext->fromnet,
			fidotext->fromnode,fidotext->frompoint,datebuf,timebuf);
		FPuts(fh,ftshead);
	} else {
		if(fidotext->frompoint) sprintf(ftshead,"SEEN-BY: %d/%d.%d\r",fidotext->fromnet,fidotext->fromnode,fidotext->frompoint);
		else sprintf(ftshead,"SEEN-BY: %d/%d\r",fidotext->fromnet,fidotext->fromnode);
		FPuts(fh,ftshead);
		if(fidotext->frompoint) sprintf(ftshead,"\001PATH: %d/%d.%d\r",fidotext->fromnet,fidotext->fromnode,fidotext->frompoint);
		else sprintf(ftshead,"\001PATH: %d/%d\r",fidotext->fromnet,fidotext->fromnode);
		FPuts(fh,ftshead);
	}
	Close(fh);
	return(nummer);
}
