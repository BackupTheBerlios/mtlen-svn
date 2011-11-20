/*

Jabber Protocol Plugin for Miranda IM
Tlen Protocol Plugin for Miranda IM
Copyright (C) 2002-2004  Santithorn Bunchua
Copyright (C) 2004-2007  Piotr Piastucki

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef _JABBER_H_
#define _JABBER_H_

#if defined(UNICODE)
	#ifndef _UNICODE
		#define _UNICODE
	#endif
#endif

#define MIRANDA_VER 0x0800

#undef _WIN32_WINNT
#undef _WIN32_IE
#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0501

#define __try
#define __except(x) if (0)
#define __finally

#define _try __try
#define _except __except
#define _finally __finally

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#define ENABLE_LOGGING

/*******************************************************************
 * Global compilation flags
 *******************************************************************/

/*******************************************************************
 * Global header files
 *******************************************************************/
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>
#include <win2k.h>

#include <newpluginapi.h>
#include <m_system.h>
#include <m_netlib.h>
#include <m_protomod.h>
#include <m_protosvc.h>
#include <m_protoint.h>
#include <m_contacts.h>
#include <m_clist.h>
#include <m_clui.h>
#include <m_options.h>
#include <m_userinfo.h>
#include <m_database.h>
#include <m_langpack.h>
#include <m_utils.h>
#include <m_message.h>
#include <m_skin.h>
#include <m_popup.h>
#include <m_avatars.h>

#include "jabber_xml.h"
#include "crypto/polarssl/aes.h"
#include "crypto/polarssl/bignum.h"

/*******************************************************************
 * Global constants
 *******************************************************************/
#define TLEN_DEFAULT_PORT 443
#define JABBER_IQID "mim_"
#define TLEN_REGISTER   "http://reg.tlen.pl/"
#define TLEN_MAX_SEARCH_RESULTS_PER_PAGE 20

// User-defined message
#define WM_TLEN_REFRESH						(WM_USER + 100)
// Error code
#define JABBER_ERROR_REDIRECT				302
#define JABBER_ERROR_BAD_REQUEST			400
#define JABBER_ERROR_UNAUTHORIZED			401
#define JABBER_ERROR_PAYMENT_REQUIRED		402
#define JABBER_ERROR_FORBIDDEN				403
#define JABBER_ERROR_NOT_FOUND				404
#define JABBER_ERROR_NOT_ALLOWED			405
#define JABBER_ERROR_NOT_ACCEPTABLE			406
#define JABBER_ERROR_REGISTRATION_REQUIRED	407
#define JABBER_ERROR_REQUEST_TIMEOUT		408
#define JABBER_ERROR_CONFLICT				409
#define JABBER_ERROR_INTERNAL_SERVER_ERROR	500
#define JABBER_ERROR_NOT_IMPLEMENTED		501
#define JABBER_ERROR_REMOTE_SERVER_ERROR	502
#define JABBER_ERROR_SERVICE_UNAVAILABLE	503
#define JABBER_ERROR_REMOTE_SERVER_TIMEOUT	504

#define TLEN_ALERTS_ACCEPT_ALL 0
#define TLEN_ALERTS_IGNORE_NIR 1
#define TLEN_ALERTS_IGNORE_ALL 2

#define TLEN_IMAGES_ACCEPT_ALL 0
#define TLEN_IMAGES_IGNORE_NIR 1
#define TLEN_IMAGES_IGNORE_ALL 2

#define TLEN_MUC_ASK		0
#define TLEN_MUC_ACCEPT_IR  1
#define TLEN_MUC_ACCEPT_ALL 2
#define TLEN_MUC_IGNORE_NIR 3
#define TLEN_MUC_IGNORE_ALL 4

#define IDC_STATIC (-1)

/*******************************************************************
 * Global data structures and data type definitions
 *******************************************************************/
typedef HANDLE JABBER_SOCKET;

typedef enum {
	LIST_ROSTER,	// Roster list
	LIST_CHATROOM,	// Groupchat room currently joined
	LIST_FILE,		// Current file transfer session
	LIST_INVITATIONS,// Invitations to be sent
	LIST_SEARCH,	 // Rooms names being searched
	LIST_VOICE,
	LIST_PICTURE
} JABBER_LIST;

typedef enum {
	IQ_PROC_NONE,
	IQ_PROC_GETSEARCH
} JABBER_IQ_PROCID;

typedef enum {
	SUB_NONE,
	SUB_TO,
	SUB_FROM,
	SUB_BOTH
} JABBER_SUBSCRIPTION;

typedef struct {
	char *szOnline;
	char *szAway;
	char *szNa;
	char *szDnd;
	char *szFreechat;
	char *szInvisible;
} JABBER_MODEMSGS;

typedef struct {
	char mailBase[256];
	char mailMsg[256];
	int  mailMsgMthd;
	char mailIndex[256];
	int  mailIndexMthd;
	char mailLogin[256];
	int  mailLoginMthd;
	char mailCompose[256];
	int  mailComposeMthd;
	char avatarGet[256];
	int  avatarGetMthd;
	char avatarUpload[256];
	int  avatarUploadMthd;
	char avatarRemove[256];
	int  avatarRemoveMthd;
} TlenConfiguration;

typedef struct {
	BOOL useEncryption;
	BOOL reconnect;
	BOOL rosterSync;
	BOOL offlineAsInvisible;
	BOOL leaveOfflineMessage;
	int offlineMessageOption;
	BOOL ignoreAdvertisements;
	int alertPolicy;
	int groupChatPolicy;
	int voiceChatPolicy;
	int imagePolicy;
	BOOL enableAvatars;
	BOOL enableVersion;
	BOOL useNudge;
	BOOL logAlerts;
	BOOL useNewP2P;
    BOOL sendKeepAlive;
    BOOL savePassword;
} TlenOptions;

struct JABBER_IQ_FUNC_STRUCT;
struct JABBER_LIST_ITEM_STRUCT;
struct TLEN_VOICE_CONTROL_STRUCT;

typedef struct {
    PROTO_INTERFACE iface;
    HANDLE hNetlibUser;
    HANDLE hFileNetlibUser;

    JABBER_MODEMSGS modeMsgs;

    struct ThreadDataStruct *threadData;
    HANDLE hTlenNudge;
    HANDLE hMenuMUC;
    HANDLE hMenuChats;
    HANDLE hMenuInbox;
    HANDLE hMenuContactMUC;
    HANDLE hMenuContactVoice;
    HANDLE hMenuContactGrantAuth;
    HANDLE hMenuContactRequestAuth;
    HANDLE hMenuPicture;

    HANDLE* hServices;
    unsigned serviceNum;
    HANDLE* hHooks;
    unsigned hookNum;

    int listsCount;
    struct JABBER_LIST_ITEM_STRUCT *lists;
    CRITICAL_SECTION csLists;

    int iqCount;
    int iqAlloced;
    struct JABBER_IQ_FUNC_STRUCT *iqList;
    CRITICAL_SECTION csIqList;

    CRITICAL_SECTION csSerial;
    unsigned int serial;
    BOOL isOnline;
    BOOL isConnected;

    CRITICAL_SECTION modeMsgMutex;

    HANDLE hMenuRoot;

    char *searchJID;
    int searchID;
    int searchIndex;
    char *searchQuery;
    int searchQueryLen;

    CRITICAL_SECTION csSend;

    HWND voiceDlgHWND;
    struct TLEN_VOICE_CONTROL_STRUCT *playbackControl;
    struct TLEN_VOICE_CONTROL_STRUCT *recordingControl;
    int framesAvailableForPlayback;
    int availOverrunValue;

    TlenOptions tlenOptions;
} TlenProtocol;


typedef struct ThreadDataStruct{
	HANDLE hThread;
    char *streamId;
	char username[128];
	char password[128];
	char server[128];
	char manualHost[128];
	char avatarToken[128];
	char avatarHash[64];
	int  avatarFormat;
	WORD port;
	BOOL useEncryption;

    JABBER_SOCKET s;    //HANDLE from CallService(MS_NETLIB_OPENCONNECTION (jabber_ws.c:68)
	aes_context aes_in_context;
	aes_context aes_out_context;
	unsigned char aes_in_iv[16];
	unsigned char aes_out_iv[16];

	BOOL useAES;
	TlenConfiguration tlenConfig;
    TlenProtocol *proto;
} ThreadData;


typedef enum { FT_CONNECTING, FT_INITIALIZING, FT_RECEIVING, FT_DONE, FT_ERROR, FT_DENIED, FT_SWITCH } JABBER_FILE_STATE;
typedef enum { FT_RECV, FT_SEND} JABBER_FILE_MODE;
typedef struct TLEN_FILE_TRANSFER_STRUCT{
	HANDLE hContact;
	JABBER_SOCKET s;
	NETLIBNEWCONNECTIONPROC_V2 pfnNewConnectionV2;
	JABBER_FILE_STATE state;
	char *jid;
	int fileId;
	char *iqId;
	int	mode;

	// Used by file receiving only
	char *hostName;
	WORD wPort;
	char *localName;
	WORD wLocalPort;
	char *szSavePath;
	long fileReceivedBytes;
	long fileTotalSize;

	// Used by file sending only
	HANDLE hFileEvent;
	int fileCount;
	char **files;
	long *filesSize;
	//long fileTotalSize;		// Size of the current file (file being sent)
	long allFileTotalSize;
	long allFileReceivedBytes;
	char *szDescription;
	int currentFile;

	// New p2p
	BOOL newP2P;
	char *id2;
	SOCKET udps;
	aes_context aes_context;
	unsigned char aes_iv[16];
	TlenProtocol *proto;

} TLEN_FILE_TRANSFER;

typedef struct {
	PROTOSEARCHRESULT hdr;
	char jid[256];
} JABBER_SEARCH_RESULT;

typedef struct {
	char *iqId;
	PROTOSEARCHRESULT hdr;
	char jid[256];
} TLEN_CONFERENCE;


typedef struct {
	int id;
	TCHAR *name;
} JABBER_FIELD_MAP;


/*******************************************************************
 * Global variables
 *******************************************************************/
extern HINSTANCE hInst;
extern HANDLE hMainThread;

/*******************************************************************
 * Function declarations
 *******************************************************************/
HICON GetIcolibIcon(int iconId);
void ReleaseIcolibIcon(HICON hIcon);

void JabberLog(TlenProtocol *proto, const char *fmt, ...);
void __cdecl JabberServerThread(ThreadData *info);
// jabber_ws.c
BOOL JabberWsInit(TlenProtocol *proto);
void JabberWsUninit(TlenProtocol *proto);
JABBER_SOCKET JabberWsConnect(TlenProtocol *proto, char *host, WORD port);
int JabberWsSend(TlenProtocol *proto, JABBER_SOCKET s, char *data, int datalen);
int JabberWsRecv(TlenProtocol *proto, JABBER_SOCKET s, char *data, long datalen);
int JabberWsSendAES(TlenProtocol *proto, char *data, int datalen, aes_context *aes_ctx, unsigned char *aes_iv);
int JabberWsRecvAES(TlenProtocol *proto, char *data, long datalen, aes_context *aes_ctx, unsigned char *aes_iv);
// jabber_util.c
HANDLE HookEventObj_Ex(const char *name, TlenProtocol *proto, MIRANDAHOOKOBJ hook);
HANDLE CreateServiceFunction_Ex(const char *name, TlenProtocol *proto, MIRANDASERVICEOBJ service);
void UnhookEvents_Ex(TlenProtocol *proto);
void DestroyServices_Ex(TlenProtocol *proto);
void JabberSerialInit(TlenProtocol *proto);
void JabberSerialUninit(TlenProtocol *proto);
unsigned int JabberSerialNext(TlenProtocol *proto);
int JabberSend(TlenProtocol *proto, const char *fmt, ...);
HANDLE JabberHContactFromJID(TlenProtocol *proto, const char *jid);
char *JabberJIDFromHContact(TlenProtocol *proto, HANDLE hContact);
char *JabberLoginFromJID(const char *jid);
char *JabberResourceFromJID(const char *jid);
char *JabberNickFromJID(const char *jid);
char *JabberLocalNickFromJID(const char *jid);
char *TlenGroupEncode(const char *str);
char *TlenGroupDecode(const char *str);
char *JabberSha1(char *str);
char *TlenSha1(char *str, int len);
char *TlenPasswordHash(const char *str);
void TlenUrlDecode(char *str);
char *TlenUrlEncode(const char *str);
char *JabberTextEncode(const char *str);
char *JabberTextDecode(const char *str);
char *JabberBase64Encode(const char *buffer, int bufferLen);
char *JabberBase64Decode(const char *buffer, int *resultLen);
int JabberGetPictureType(const char* buf);
void TlenLogMessage(TlenProtocol *proto, HANDLE hContact, DWORD flags, const char *message);
BOOL IsAuthorized(TlenProtocol *proto, const char *jid);
//char *JabberGetVersionText();
time_t JabberIsoToUnixTime(char *stamp);
time_t TlenTimeToUTC(time_t time);
void JabberSendPresence(TlenProtocol *proto,int status);
void JabberStringAppend(char **str, int *sizeAlloced, const char *fmt, ...);
//char *JabberGetClientJID(char *jid);
// jabber_misc.c
void JabberDBAddEvent(TlenProtocol *proto, HANDLE hContact, int eventType, DWORD flags, PBYTE pBlob, DWORD cbBlob);
void JabberDBAddAuthRequest(TlenProtocol *proto, char *jid, char *nick);
HANDLE JabberDBCreateContact(TlenProtocol *proto, char *jid, char *nick, BOOL temporary);
void JabberContactListCreateGroup(char *groupName);
unsigned long JabberForkThread(void (__cdecl *threadcode)(void*), unsigned long stacksize, void *arg);
// jabber_svc.c
int JabberGetInfo(PROTO_INTERFACE *ptr, HANDLE hContact, int infoType);
int TlenRunSearch(TlenProtocol *proto);
// jabber_advsearch.c
extern JABBER_FIELD_MAP tlenFieldGender[];
extern JABBER_FIELD_MAP tlenFieldLookfor[];
extern JABBER_FIELD_MAP tlenFieldStatus[];
extern JABBER_FIELD_MAP tlenFieldOccupation[];
extern JABBER_FIELD_MAP tlenFieldPlan[];

INT_PTR CALLBACK TlenAdvSearchDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
char *TlenAdvSearchCreateQuery(HWND hwndDlg, int iqId);


#endif

