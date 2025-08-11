#include <time.h>
#include <limits.h>
#include <string.h>
#include "NiKomLib.h"
#include "NiKomStr.h"
#include "Nodes.h"
#include "Stack.h"
#include "IntList.h"
#include "Logging.h"
#include "StringUtils.h"
#include "OrgMeet.h"
#include "FidoMeet.h"
#include "Brev.h"
#include "Terminal.h"
#include "UserNotificationHooks.h"
#include "Cmd_KOM.h"
#include "Cmd_Users.h"
#include "Cmd_Conf.h"
#include "Cmd_Misc.h"
#include "InfoFiles.h"
#include "NiKFiles.h"
#include "NiKVersion.h"
#include "BasicIO.h"
#include "Languages.h"
#include "CommandParser.h"
#include "NiKomFuncs.h"
#include "StyleSheets.h"

#include "KOM.h"

#define CMD_NEXTREPLY 211
#define CMD_NEXTTEXT  210
#define CMD_NEXTCONF  221
#define CMD_SEETIME   306
#define CMD_GOMAIL    222
#define CMD_READTEXT  212
#define CMD_DISPNOTIF 331
#define MAILBOX_CONFID -1

struct Stack *g_unreadRepliesStack;
struct IntList *g_readRepliesList;

extern struct System *Servermem;
extern int inloggad, nodnr, nodestate, mote2, g_userDataSlot;
extern long logintime, extratime;
extern char inmat[], *argument;

struct Kommando internalGoMailCommand = {
  { NULL, NULL },
  { { "Go Mailbox", 2 }, { "G� Brevl�dan", 2} },
  CMD_GOMAIL, 0, 0, 0, 0, 0,
  0, 0, NULL, NULL, 0, NULL, NULL, NULL
};

int hasUnreadInConf(int confId) {
  struct Mote *conf;
  if(confId == MAILBOX_CONFID) {
    return HasUnreadMail();
  }
  if(!IsMemberConf(confId, inloggad, CURRENT_USER)) {
    return 0;
  }
  if((conf = getmotpek(confId)) == NULL) {
    LogEvent(SYSTEM_LOG, ERROR,
             "Can't find struct for confId %d in hasUnreadConf()", confId);
    return 0;
  }
  switch(conf->type) {
  case MOTE_ORGINAL:
    return HasUnreadInOrgConf(confId);
  case MOTE_FIDO:
    return HasUnreadInFidoConf(conf);
  default:
    LogEvent(SYSTEM_LOG, ERROR,
             "Undefined type %d for conf %d in hasUnreadConf()", conf->type, confId);
  }
  return 0;
}

/*
 * Returns the id of the next conference with unread texts, starting the search
 * from currentConfId. Returns -2 if there is no conference with unread texts.
 */
int FindNextUnreadConf(int currentConfId) {
  struct Mote *conf;
  int tmpConfId = 0;

  if(IsListEmpty((struct List *)&Servermem->mot_list)) {
    return -2;
  }

  if(currentConfId == MAILBOX_CONFID) {
    conf = (struct Mote *)Servermem->mot_list.mlh_Head;
  } else {
    conf = getmotpek(currentConfId);
    conf = (struct Mote *)conf->mot_node.mln_Succ;
    if(conf->mot_node.mln_Succ == NULL) {
      conf = (struct Mote *)Servermem->mot_list.mlh_Head;
    }
  }
  tmpConfId = conf->nummer;
  while(tmpConfId != currentConfId) {
    if(!(conf->status & SUPERHEMLIGT) && hasUnreadInConf(tmpConfId)) {
      return tmpConfId;
    }
    conf = (struct Mote *)conf->mot_node.mln_Succ;
    if(conf->mot_node.mln_Succ == NULL) {
      if(currentConfId == MAILBOX_CONFID) {
        return -2;
      }
      conf = (struct Mote *)Servermem->mot_list.mlh_Head;
    }
    tmpConfId = conf->nummer;
  }
  return -2;
}

int isUserOutOfTime(void) {
  int limitSeconds, secondsLoggedIn;
  limitSeconds = 60 * Servermem->cfg->maxtid[CURRENT_USER->status]
    + extratime;
  if(Servermem->cfg->maxtid[CURRENT_USER->status] != 0) {
    secondsLoggedIn = time(NULL) - logintime;
    if(secondsLoggedIn > limitSeconds) {
      SendInfoFile("OutOfTime.txt", 0);
      nodestate = NIKSTATE_OUTOFTIME;
      return -1;
    }
    return (limitSeconds - secondsLoggedIn) / 60;
  }
  return INT_MAX;
}

struct Kommando *getCommandToExecute(int defaultCmd) {
  int parseRes, cmdId, i;
  struct Alias *alias;
  static unsigned badCommandCnt = 0;
  static char aliasbuf[1081];
  struct Kommando *parseResult[50];

  static char argbuf[1081];

  if(GetString(999, NULL)) {
    return NULL;
  }
  if(inmat[0]=='.' || inmat[0]==' ') {
    return NULL;
  }
  if(inmat[0] && (alias = parsealias(inmat))) {
    strcpy(aliasbuf, alias->blirtill);
    strcat(aliasbuf, " ");
    strncat(aliasbuf, FindNextWord(inmat),980);
    strcpy(inmat,aliasbuf);
  }
  
  parseRes = ParseCommand(inmat, CURRENT_USER->language, CURRENT_USER, parseResult, argbuf);
  switch(parseRes) {
  case -2:
    cmdId = CMD_READTEXT;
    argument = inmat;
    break;

  case -1:
    cmdId = defaultCmd;
    break;

  case 0:
    SendString("\r\n\n%s\r\n", CATSTR(MSG_KOM_INVALID_COMMAND));
    if(++badCommandCnt >= 2 && !(CURRENT_USER->flaggor & INGENHELP)) {
      SendInfoFile("2Errors.txt", 0);
    }
    return NULL;

  case 1:
    cmdId = parseResult[0]->nummer;
    argument = argbuf;
    badCommandCnt = 0;
    if(!HasUserCmdPermission(parseResult[0], CURRENT_USER)) {
      SendString("\r\n\n%s\r\n", CATSTR(MSG_KOM_NO_PERMISSION));
      if(Servermem->cfg->ar.noright) {
        sendautorexx(Servermem->cfg->ar.noright);
      }
      return NULL;
    }

    if(parseResult[0]->losen[0]) {
      SendString("\r\n\n%s: ", CATSTR(MSG_KOM_COMMAND_PASSWORD));
      getstring(CURRENT_USER->flaggor & STAREKOFLAG ? STAREKO : EJEKO, 20, NULL);

      if(strcmp(parseResult[0]->losen, inmat)) {
        SendString("\r\n\n%s\r\n", CATSTR(MSG_KOM_INVALID_PASSWORD));
        return NULL;
      }
    }
    break;

  default:
    SendString("\r\n\n%s\r\n\n", CATSTR(MSG_KOM_AMBIGOUS_COMMAND));
    for(i = 0; i < parseRes; i++) {
      SendString("%s\n\r", ChooseLangCommand(parseResult[i], CURRENT_USER->language)->name);
    }
    return NULL;
  }

  if(cmdId == CMD_GOMAIL) {
    return &internalGoMailCommand;
  }
  return parseRes == 1 ? parseResult[0] : getkmdpek(cmdId);
}

void ExecuteCommandById(int cmdId) {
  switch(cmdId) {
  case 201: Cmd_GoConf(); break; // ga(argument);
  case 210: Cmd_NextText(); break;
  case 211: Cmd_NextReply(); break;
  case 221: Cmd_NextConf(); break;
  case 222: GoConf(MAILBOX_CONFID); break;
  case 301: Cmd_Logout(); break;
  case 101: listmot(argument); break;
  case 102: Cmd_ListUsers(); break;
  case 103: listmed(); break;
  case 104: SendInfoFile("ListCommands.txt", 0); break;
  case 105: listratt(); break;
  case 106: listasenaste(); break;
  case 107: listnyheter(); break;
  case 108: listaarende(); break;
  case 109: listflagg(); break;
  case 111: listarea(); break;
  case 112: listnyckel(); break;
  case 113: listfiler(); break;
  case 114: listagrupper(); break;
  case 115: listgruppmed(); break;
  case 116: listabrev(); break;
  case 117: Cmd_ListActive(); break;
  case 202: skriv(); break;
  case 203: Cmd_Reply(); break;
  case 204: personlig(); break;
  case 205: skickabrev(); break;
  case 206: igen(); break;
  case 208: medlem(argument); break;
  case 209: uttrad(argument); break;
  case 212: Cmd_Read(); break;
  case 213: endast(); break;
  case 214: Cmd_SkipReplies(); break;
  case 215: addratt(); break;
  case 216: subratt(); break;
  case 217: radtext(); break;
  case 218: skapmot(); break;
  case 219: radmot(); break;
  case 220: var(mote2); break;
  case 223: andmot(); break;
  case 224: radbrev(); break;
  case 225: rensatexter(); break;
  case 226: rensabrev(); break;
  case 227: gamlatexter(); break;
  case 228: gamlabrev(); break;
  case 229: dumpatext(); break;
  case 231: movetext(); break;
  case 232: motesstatus(); break;
  case 233: hoppaarende(); break;
  case 234: flyttagren(); break;
  case 235: Cmd_FootNote(); break;
  case 236: Cmd_Search(); break;
  case 237: Cmd_Like(); break;
  case 238: Cmd_Dislike(); break;
  case 302: SendInfoFile("Help.txt", 0); break;
  case 303: Cmd_ChangeUser(); break;
  case 304: slaav(); break;
  case 305: slapa(); break;
  case 306: tiden(); break;
  case 307: ropa(); break;
  case 308: Cmd_Status(); break;
  case 309: Cmd_DeleteUser(); break;
  case 310: vilka(); break;
  case 311: Cmd_ShowInfo(); break;
  case 312: getconfig(); break;
  case 313: writeinfo(); break;
  case 314: Cmd_Say(); break;
  case 315: skrivlapp(); break;
  case 316: radlapp(); break;
  case 317: grab(); break;
  case 318: skapagrupp(); break;
  case 319: andragrupp(); break;
  case 320: raderagrupp(); break;
  case 321: adderagruppmedlem(); break;
  case 322: subtraheragruppmedlem(); break;
  case 323: DisplayVersionInfo(); break;
  case 324: alias(); break;
  case 325: Cmd_ReLogin(); break;
  case 326: bytnodtyp(); break;
  case 327: bytteckenset(); break;
  case 329: Cmd_ChangeLanguage(); break;
  case 330: Cmd_ChangeStyleSheet(); break;
  case 331: Cmd_DisplayNotifications(); break;
  case 401: bytarea(); break;
  case 402: filinfo(); break;
  case 403: upload(); break;
  case 404: download(); break;
  case 405: Cmd_CreateArea(); break;
  case 406: radarea(); break;
  case 407: andraarea(); break;
  case 408: skapafil(); break;
  case 409: radfil(); break;
  case 410: andrafil(); break;
  case 411: lagrafil(); break;
  case 412: flyttafil(); break;
  case 413: sokfil(); break;
  case 414: filstatus(); break;
  case 415: typefil(); break;
  case 416: nyafiler(); break;
  case 417: validerafil(); break;
  default:
    if(cmdId >= 500) {
      sendrexx(cmdId);
    } else {
      LogEvent(SYSTEM_LOG, ERROR,
               "Trying to execute undefined command %d", cmdId);
      DisplayInternalError();
    }
  }
}

void ExecuteCommand(struct Kommando *cmd) {
  int afterRexxScript;
  if(cmd->before) {
    sendautorexx(cmd->before);
  }
  if(cmd->logstr[0]) {
    LogEvent(USAGE_LOG, INFO, "%s %s", getusername(inloggad), cmd->logstr);
  }
  if(cmd->vilkainfo[0]) {
    Servermem->nodeInfo[nodnr].action = GORNGTANNAT;
    Servermem->nodeInfo[nodnr].currentActivity = cmd->vilkainfo;
  }
  // Save 'after' in case the command to execute is to reload the config and
  // cmd is not a valid pointer anymore when ExecuteCommandById() returns.
  afterRexxScript = cmd->after; 
  ExecuteCommandById(cmd->nummer);
  if(afterRexxScript) {
    sendautorexx(afterRexxScript);
  }
}

void displayPrompt(int defaultCmd) {
  int minutesLeft;
  char *cmdStr, minutesLeftStr[10], notificationStr[40];
  struct Kommando *cmd;

  displaysay();
  if((minutesLeft = isUserOutOfTime()) == -1) {
    return;
  }

  Servermem->nodeInfo[nodnr].lastActiveTime = time(NULL);
  Servermem->nodeInfo[nodnr].action = LASER;
  Servermem->nodeInfo[nodnr].currentConf = mote2;
  switch(defaultCmd) {
  case CMD_NEXTCONF:
    if(Servermem->cfg->ar.nextmeet) {
      sendautorexx(Servermem->cfg->ar.nextmeet);
    }
    cmdStr = CATSTR(MSG_PROMPT_GO_TO_NEXT_FORUM);
    break;
  case CMD_NEXTTEXT:
    if(mote2 == MAILBOX_CONFID) {
      if(Servermem->cfg->ar.nextletter) {
        sendautorexx(Servermem->cfg->ar.nextletter);
      }
      cmdStr = CATSTR(MSG_PROMPT_READ_NEXT_MAIL);
    } else {
      if(Servermem->cfg->ar.nexttext) {
        sendautorexx(Servermem->cfg->ar.nexttext);
      }
      cmdStr = CATSTR(MSG_PROMPT_READ_NEXT_TEXT);
    }
    break;
  case CMD_NEXTREPLY:
    if(Servermem->cfg->ar.nextkom) {
      sendautorexx(Servermem->cfg->ar.nextkom);
    }
    cmdStr = CATSTR(MSG_PROMPT_READ_NEXT_COMMENT);
    break;
  case CMD_SEETIME:
    Servermem->nodeInfo[nodnr].action = INGET;
    if(Servermem->cfg->ar.setid) {
      sendautorexx(Servermem->cfg->ar.setid);
    }
    cmdStr = CATSTR(MSG_PROMPT_SEE_TIME);
    break;
  case CMD_GOMAIL:
    if(Servermem->cfg->ar.nextmeet) {
      sendautorexx(Servermem->cfg->ar.nextmeet);
    }
    cmdStr = CATSTR(MSG_PROMPT_GO_TO_MAILBOX);
    break;
  case CMD_DISPNOTIF:
    cmdStr = CATSTR(MSG_PROMPT_DISPLAY_NOTIF);
    break;
  default:
    cmdStr = "*** Undefined default command ***";
  }

  if(minutesLeft <= 4) {
    sprintf(minutesLeftStr, "(%d) ", minutesLeft);
  } else {
    minutesLeftStr[0] = '\0';
  }
  if(Servermem->waitingNotifications[g_userDataSlot] > 0) {
    sprintf(notificationStr, "�promptnotif�[%d] ", Servermem->waitingNotifications[g_userDataSlot]);
  } else {
    notificationStr[0] = '\0';
  }
  SendString("\r\n�prompttext�%s %s%s�promptarrow�%s�reset� ",
             cmdStr, minutesLeftStr, notificationStr, CURRENT_USER->prompt);

  if((cmd = getCommandToExecute(defaultCmd)) == NULL) {
    return;
  }
  ExecuteCommand(cmd);
}

int shouldLogout(void) {
  return (nodestate & (NIKSTATE_USERLOGOUT | NIKSTATE_OUTOFTIME))
    || ImmediateLogout();
}

void KomLoop(void) {
  int defaultCmd;
  g_unreadRepliesStack = CreateStack();
  g_readRepliesList = CreateIntList(100);

  for(;;) {
    if(StackSize(g_unreadRepliesStack) > 0) {
      defaultCmd = CMD_NEXTREPLY;
    } else if(hasUnreadInConf(mote2)) {
      defaultCmd = CMD_NEXTTEXT;
    } else if(HasUnreadMail()) {
      defaultCmd = CMD_GOMAIL;
    } else if(FindNextUnreadConf(mote2) >= 0) {
      defaultCmd = CMD_NEXTCONF;
    } else if(Servermem->waitingNotifications[g_userDataSlot] > 0) {
      defaultCmd = CMD_DISPNOTIF;
    } else {
      defaultCmd = CMD_SEETIME;
    }

    displayPrompt(defaultCmd);
    if(shouldLogout()) {
      break;
    }
  }

  DeleteStack(g_unreadRepliesStack);
  DeleteIntList(g_readRepliesList);
}

void GoConf(int confId) {
  mote2 = confId;
  StackClear(g_unreadRepliesStack);
  IntListClear(g_readRepliesList);
  var(mote2);
}
