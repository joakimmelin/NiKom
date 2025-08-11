#ifndef NIKOMLIB_H
#define NIKOMLIB_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef EXEC_LISTS_H
#include <exec/lists.h>
#endif

/* Flagor f�r attribut-f�ltet*/
#define FIDOT_PRIVATE     (1<<0)  /* Texten �r ett NetMail */
#define FIDOT_CRASH       (1<<1)  /* Crashmail */
#define FIDOT_RECIEVED    (1<<2)  /* Tja, mottagen (!?) */
#define FIDOT_SENT        (1<<3)  /* Texten �r utexporterad */
#define FIDOT_FILEATTACH  (1<<4)  /* Texten har en fil */
#define FIDOT_INTRANSIT   (1<<5)  /* ??? */
#define FIDOT_ORPHAN      (1<<6)  /* P� barnhem! :-) */
#define FIDOT_KILLSENT    (1<<7)  /* Texten ska raderas n�r den exporteras (?) */
#define FIDOT_LOCAL       (1<<8)  /* Texten �r skriven p� detta system */
#define FIDOT_HOLD        (1<<9)  /* Texten ska exporteras som Hold */
#define FIDOT_FILEREQ     (1<<11) /* Texten �r en filerequest. (Ehh?) */
#define FIDOT_RETRECREQ   (1<<12) /* Ett kvitto ska skickas n�r texten kommer fram (?) */
#define FIDOT_ISRETREC    (1<<13) /* Denna text �r ett s�dant kvitto (?) */
#define FIDOT_AUDITREQ    (1<<14) /* Que? */
#define FIDOT_FILEUPDREQ  (1<<15) /* Que? */


struct FidoText
{
  UBYTE fromuser[36], touser[36], subject[72],date[20], msgid[50];
  USHORT fromzone,fromnet,fromnode,frompoint,tozone,tonet,tonode,topoint,attribut;
  int charset;
  struct MinList text;
};

/* FidoLine-strukturen �r identisk med EditLine i NiKomStr.h. Detta f�r
   att enkelt kunna spara en text.
   Om en rad b�rjar med Ctrl-A ($01) �r den raden en kludge.*/

struct FidoLine
{
	struct MinNode line_node;
	long number;
	char text[81];
};

/* Tags att skicka med till ReadFidoText() */
#define RFT_Dummy (TAG_USER + 4711)

#define RFT_NoKludges  (RFT_Dummy + 1) /* Inga kludgerader ska tas med */
#define RFT_NoSeenBy   (RFT_Dummy + 2) /* Inga seenby-rader ska tas med */
#define RFT_HeaderOnly (RFT_Dummy + 3) /* Bara FidoText-strukturen */
#define RFT_Quote      (RFT_Dummy + 4) /* Om texten ska anv�ndas som citat */
#define RFT_LineLen    (RFT_Dummy + 5) /* Hur l�nga raderna ska vara */
// If the text is in this charset, don't do any character conversion.
#define RFT_PassthroughCharset (RFT_Dummy + 6)

/* Tags att skicka med till WriteFidoText() */
#define WFT_Dummy (TAG_USER + 1742)

#define WFT_MailDir (WFT_Dummy + 1) /* Vilket directory det ska sparas i */
#define WFT_CharSet (WFT_Dummy + 2) /* Vilken teckenupps�ttning, se nedan */
#define WFT_Domain (WFT_Dummy + 3) /* Vilken domain som ska l�ggas till adresser */
#define WFT_Reply (WFT_Dummy + 4) /* Vilken text som kommenteras, fr�n MSGID */


/* Character sets */
enum nikom_chrs {
  CHRS_PASSTHROUGH = -1,
  CHRS_UNKNOWN =  0,
  CHRS_LATIN1  =  1,
  CHRS_CP437   =  2,
  CHRS_CP850   =  3,
  CHRS_SIS7    =  4,
  CHRS_CP866   =  5,
  CHRS_MAC     =  8,
  CHRS_UTF8    = 32,
};

/* Vilka l�gen en nod kan befinna sig i */
#define NIKSTATE_RELOGIN    1   /* Anv�ndaren ska g�ra en ny inloggning */
#define NIKSTATE_CLOSESER   2   /* Devicet ska st�ngas n�r det inte �r n�gon inloggad */
#define NIKSTATE_NOANSWER   4   /* Ser-noder ska inte svara p� inkommande samtal */
#define NIKSTATE_NOCARRIER  8   /* Carriern �r sl�ppt */
#define NIKSTATE_AUTOLOGOUT     16  /* Anv�ndaren ska loggas ut bums */
#define NIKSTATE_INACTIVITY 32  /* Inaktivitets-tiden har g�tt ut */
#define NIKSTATE_USERLOGOUT 64  // The user has requested to logout
#define NIKSTATE_OUTOFTIME 128  // The user is out of time and should be logged out

/************************************************************************/
/*************************     Grupper     ******************************/
/************************************************************************/

#define UG_Dummy (TAG_USER + 1973)

#define UG_Name       	(UG_Dummy + 1)
#define UG_Flags      	(UG_Dummy + 2)
#define UG_Autostatus 	(UG_Dummy + 3)

#define SECRETGROUP    1

/************************************************************************/
/*************************      Users      ******************************/
/************************************************************************/

#define US_Name		  	(UG_Dummy + 4)
#define US_Street	  	(UG_Dummy + 5)
#define US_Address	  	(UG_Dummy + 6)
#define US_Country	  	(UG_Dummy + 7)
#define US_Phonenumber	(UG_Dummy + 8)
#define US_OtherInfo  	(UG_Dummy + 9)
#define US_Password	  	(UG_Dummy + 10)
#define US_Status	  	(UG_Dummy + 11)
#define US_Charset	  	(UG_Dummy + 12)
#define US_Prompt       (UG_Dummy + 13)
#define US_Rader		(UG_Dummy + 14)
#define US_Protocol		(UG_Dummy + 15)
#define US_Tottid		(UG_Dummy + 16)
#define US_FirstLogin	(UG_Dummy + 17)
#define US_LastLogin	(UG_Dummy + 18)
#define US_Read		    (UG_Dummy + 19)
#define US_Written		(UG_Dummy + 20)
#define US_Flags		(UG_Dummy + 21)
#define US_Defarea		(UG_Dummy + 22)
#define US_Uploads		(UG_Dummy + 23)
#define US_Downloads	(UG_Dummy + 24)
#define US_Loggin		(UG_Dummy + 25)
#define US_Shell		(UG_Dummy + 26)

/************************************************************************/
/*************************     NiKHash     ******************************/
/************************************************************************/

typedef struct NiKHash NiKHash;

#ifndef NIKOM_PROTOS_H
	#include "nikom_protos.h"
#endif


#ifndef NIKOM_PRAGMAS_H
	#include "nikom_pragmas.h"
#endif


#endif /* NIKOMLIB_H */
