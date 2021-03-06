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

#define MIRANDA_VER 0x0700

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
#define _WIN32_WINNT 0x501
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>

#include <newpluginapi.h>
#include <m_system.h>
#include <m_netlib.h>
#include <m_protomod.h>
#include <m_protosvc.h>
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
#include "crypto/aes.h"
#include "crypto/bignum.h"

/*******************************************************************
 * Global constants
 *******************************************************************/
#define TLEN_VERSION PLUGIN_MAKE_VERSION(1,1,0,0)
#define TLEN_VERSION_STRING  "1.1.0.0"
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

#define TLEN_MUC_ASK		0
#define TLEN_MUC_ACCEPT_IR  1
#define TLEN_MUC_ACCEPT_ALL 2
#define TLEN_MUC_IGNORE_NIR 3
#define TLEN_MUC_IGNORE_ALL 4

#define IDC_STATIC (-1)

enum {
	TLEN_IDI_TLEN = 0,
	TLEN_IDI_MAIL,
	TLEN_IDI_MUC,
	TLEN_IDI_CHATS,
	TLEN_IDI_GRANT,
	TLEN_IDI_REQUEST,
	TLEN_IDI_VOICE,
	TLEN_IDI_MICROPHONE,
	TLEN_IDI_SPEAKER,
	TLEN_ICON_TOTAL
};

extern HICON tlenIcons[TLEN_ICON_TOTAL];


/*******************************************************************
 * Macro definitions
 *******************************************************************/
void JabberLog(const char *fmt, ...);
#define JabberSendInvisiblePresence()	JabberSendVisibleInvisiblePresence(TRUE)
#define JabberSendVisiblePresence()		JabberSendVisibleInvisiblePresence(FALSE)

/*******************************************************************
 * Global data structures and data type definitions
 *******************************************************************/
typedef HANDLE JABBER_SOCKET;

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

struct ThreadData {
	HANDLE hThread;

	char username[128];
	char password[128];
	char server[128];
	char manualHost[128];
	char avatarToken[128];
	char avatarHash[64];
	int  avatarFormat;
	WORD port;
	JABBER_SOCKET s;
	BOOL useEncryption;

	aes_context aes_in_context;
	aes_context aes_out_context;
	unsigned char aes_in_iv[16];
	unsigned char aes_out_iv[16];

	BOOL useAES;


	char newPassword[128];

	HWND reg_hwndDlg;
	TlenConfiguration tlenConfig;
};


typedef struct {
	char *szOnline;
	char *szAway;
	char *szNa;
	char *szDnd;
	char *szFreechat;
	char *szInvisible;
} JABBER_MODEMSGS;

typedef enum { FT_CONNECTING, FT_INITIALIZING, FT_RECEIVING, FT_DONE, FT_ERROR, FT_DENIED, FT_SWITCH } JABBER_FILE_STATE;
typedef enum { FT_RECV, FT_SEND} JABBER_FILE_MODE;
typedef struct {
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
	char *name;
} JABBER_FIELD_MAP;

typedef struct {
	int useEncryption;
	int reconnect;
	int rosterSync;
	int offlineAsInvisible;
	int leaveOfflineMessage;
	int offlineMessageOption;
	int ignoreAdvertisements;
	int alertPolicy;
	int groupChatPolicy;
	int voiceChatPolicy;
	int enableAvatars;
	int enableVersion;
	int useNudge;
	int logAlerts;
	int useNewP2P;
} TLEN_OPTIONS;

/*******************************************************************
 * Global variables
 *******************************************************************/
extern HINSTANCE hInst;
extern HANDLE hMainThread;
extern DWORD jabberMainThreadId;
extern char *jabberProtoName;
extern char *jabberModuleName;
extern HANDLE hNetlibUser;
extern HANDLE hFileNetlibUser;

extern struct ThreadData *jabberThreadInfo;
extern char *jabberJID;
extern char *streamId;
extern BOOL jabberConnected;
extern BOOL jabberOnline;
extern int jabberStatus;
extern int jabberDesiredStatus;
//extern char *jabberModeMsg;
extern CRITICAL_SECTION modeMsgMutex;
extern JABBER_MODEMSGS modeMsgs;
extern BOOL jabberSendKeepAlive;
extern TLEN_OPTIONS tlenOptions;

extern HANDLE hEventSettingChanged, hEventContactDeleted, hEventTlenUserInfoInit, hEventTlenOptInit, hEventTlenPrebuildContactMenu;

extern HANDLE hMenuMUC;
extern HANDLE hMenuChats;
extern HANDLE hMenuContactMUC;
extern HANDLE hMenuContactVoice;
extern HANDLE hTlenNudge;

/*******************************************************************
 * Function declarations
 *******************************************************************/
void __cdecl JabberServerThread(struct ThreadData *info);
// jabber_ws.c
BOOL JabberWsInit(void);
void JabberWsUninit(void);
JABBER_SOCKET JabberWsConnect(char *host, WORD port);
int JabberWsSend(JABBER_SOCKET s, char *data, int datalen);
int JabberWsRecv(JABBER_SOCKET s, char *data, long datalen);
int JabberWsSendAES(JABBER_SOCKET hConn, char *data, int datalen, aes_context *aes_ctx, unsigned char *aes_iv);
int JabberWsRecvAES(JABBER_SOCKET s, char *data, long datalen, aes_context *aes_ctx, unsigned char *aes_iv);
// jabber_util.c
HANDLE HookEvent_Ex(const char *name, MIRANDAHOOK hook);
HANDLE CreateServiceFunction_Ex(const char *name, MIRANDASERVICE service);
void UnhookEvents_Ex();
void DestroyServices_Ex();
void JabberSerialInit(void);
void JabberSerialUninit(void);
unsigned int JabberSerialNext(void);
int JabberSend(JABBER_SOCKET s, const char *fmt, ...);
HANDLE JabberHContactFromJID(const char *jid);
char *JabberJIDFromHContact(HANDLE hContact);
char *JabberLoginFromJID(const char *jid);
char *JabberResourceFromJID(const char *jid);
char *JabberNickFromJID(const char *jid);
char *JabberLocalNickFromJID(const char *jid);
char *TlenGroupEncode(const char *str);
char *TlenGroupDecode(const char *str);
char *JabberUtf8Decode(const char *str);
char *JabberUtf8Encode(const char *str);
char *JabberSha1(char *str);
char *TlenSha1(char *str, int len);
char *JabberUnixToDos(const char *str);
void JabberDosToUnix(char *str);
void JabberHttpUrlDecode(char *str);
char *JabberHttpUrlEncode(const char *str);
void JabberSendVisibleInvisiblePresence(BOOL invisible);
char *TlenPasswordHash(const char *str);
void TlenUrlDecode(char *str);
char *TlenUrlEncode(const char *str);
char *JabberTextEncode(const char *str);
char *JabberTextDecode(const char *str);
char *JabberBase64Encode(const char *buffer, int bufferLen);
char *JabberBase64Decode(const char *buffer, int *resultLen);
int JabberGetPictureType(const char* buf);
//char *JabberGetVersionText();
time_t JabberIsoToUnixTime(char *stamp);
time_t TlenTimeToUTC(time_t time);
void JabberSendPresenceTo(int status, char *to, char *extra);
void JabberSendPresence(int status);
void JabberStringAppend(char **str, int *sizeAlloced, const char *fmt, ...);
//char *JabberGetClientJID(char *jid);
// jabber_misc.c
void JabberDBAddAuthRequest(char *jid, char *nick);
HANDLE JabberDBCreateContact(char *jid, char *nick, BOOL temporary);
void JabberContactListCreateGroup(char *groupName);
unsigned long JabberForkThread(void (__cdecl *threadcode)(void*), unsigned long stacksize, void *arg);
// jabber_svc.c
int JabberGetInfo(WPARAM wParam, LPARAM lParam);
int JabberRunSearch();
// jabber_advsearch.c
extern JABBER_FIELD_MAP tlenFieldGender[];
extern JABBER_FIELD_MAP tlenFieldLookfor[];
extern JABBER_FIELD_MAP tlenFieldStatus[];
extern JABBER_FIELD_MAP tlenFieldOccupation[];
extern JABBER_FIELD_MAP tlenFieldPlan[];

BOOL CALLBACK TlenAdvSearchDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
char *TlenAdvSearchCreateQuery(HWND hwndDlg, int iqId);


#endif

