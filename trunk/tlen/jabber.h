/*

Jabber Protocol Plugin for Miranda IM
Tlen Protocol Plugin for Miranda IM
Copyright (C) 2002-2004  Santithorn Bunchua, Piotr Piastucki

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

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
//#define ENABLE_LOGGING
/*******************************************************************
 * Global compilation flags
 *******************************************************************/
//#define TLEN_PLUGIN defined/undefined in Project Setting

/*******************************************************************
 * Global header files
 *******************************************************************/
#define _WIN32_WINNT 0x500
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

#include "jabber_xml.h"

/*******************************************************************
 * Global constants
 *******************************************************************/
#define JABBER_DEFAULT_PORT 5222
#define JABBER_IQID "keh_"
#define TLEN_REGISTER   "http://reg.tlen.pl/"
// User-defined message
#define WM_JABBER_REGDLG_UPDATE				WM_USER + 100
#define WM_JABBER_AGENT_REFRESH				WM_USER + 101
#define WM_JABBER_TRANSPORT_REFRESH			WM_USER + 102
#define WM_JABBER_REGINPUT_ACTIVATE			WM_USER + 103
#define WM_JABBER_REFRESH					WM_USER + 104
#define WM_JABBER_CHECK_ONLINE				WM_USER + 105
#define WM_JABBER_CHANGED					WM_USER + 106
#define WM_JABBER_ACTIVATE					WM_USER + 107
#define WM_JABBER_SET_FONT					WM_USER + 108
#define WM_JABBER_FLASHWND					WM_USER + 109
#define WM_JABBER_SHUTDOWN					WM_USER + 112
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
// File transfer setting
#define JABBER_OPTION_FT_DIRECT		0	// Direct connection
#define JABBER_OPTION_FT_PASS		1	// Use PASS server
#define JABBER_OPTION_FT_PROXY		2	// Use proxy with local port forwarding
#define IDC_STATIC (-1)

enum {
	TLEN_IDI_TLEN = 0,
	TLEN_IDI_MUC,
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

typedef enum {
	JABBER_SESSION_NORMAL,
	JABBER_SESSION_REGISTER
} JABBER_SESSION_TYPE;

struct ThreadData {
	HANDLE hThread;
	JABBER_SESSION_TYPE type;

	char username[128];
	char password[128];
	char server[128];
	char manualHost[128];
	WORD port;
	JABBER_SOCKET s;
	BOOL useSSL;

	char newPassword[128];

	HWND reg_hwndDlg;
	BOOL reg_done;
};

typedef struct {
	char *szOnline;
	char *szAway;
	char *szNa;
	char *szDnd;
	char *szFreechat;
	char *szInvisible;
} JABBER_MODEMSGS;

typedef struct {
	char username[128];
	char password[128];
	char server[128];
	char manualHost[128];
	WORD port;
	BOOL useSSL;
} JABBER_REG_ACCOUNT;

typedef enum { FT_CONNECTING, FT_INITIALIZING, FT_RECEIVING, FT_DONE, FT_ERROR, FT_DENIED, FT_SWITCH } JABBER_FILE_STATE;
typedef enum { FT_RECV, FT_SEND} JABBER_FILE_MODE;
typedef struct {
	HANDLE hContact;
	JABBER_SOCKET s;
	JABBER_FILE_STATE state;
	char *jid;
	int fileId;
	char *iqId;
	int	mode;

	// Used by file receiving only
	char *httpHostName;
	WORD httpPort;
	char *httpPath;
	char *szSavePath;
	long fileReceivedBytes;
	long fileTotalSize;

	// Used by file sending only
	HANDLE hFileEvent;
	int fileCount;
	char **files;
	long *filesSize;
	//char *httpPath;			// Name of the requested file
	//long fileTotalSize;		// Size of the current file (file being sent)
	long allFileTotalSize;
	long allFileReceivedBytes;
	char *szDescription;
	int currentFile;
} JABBER_FILE_TRANSFER;

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
extern HANDLE hLibSSL;
extern PVOID jabberSslCtx;

extern struct ThreadData *jabberThreadInfo;
extern char *jabberJID;
extern char *streamId;
extern DWORD jabberLocalIP;
extern BOOL jabberConnected;
extern BOOL jabberOnline;
extern int jabberStatus;
extern int jabberDesiredStatus;
//extern char *jabberModeMsg;
extern CRITICAL_SECTION modeMsgMutex;
extern JABBER_MODEMSGS modeMsgs;
extern BOOL jabberChangeStatusMessageOnly;
extern BOOL jabberSendKeepAlive;

extern HANDLE hEventSettingChanged;
extern HANDLE hEventContactDeleted;

extern HANDLE hMenuMUC;
extern HANDLE hMenuChats;
extern HANDLE hMenuContactMUC;
extern HANDLE hMenuContactVoice;
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
// jabber_util.c
void JabberSerialInit(void);
void JabberSerialUninit(void);
unsigned int JabberSerialNext(void);
int JabberSend(JABBER_SOCKET s, const char *fmt, ...);
HANDLE JabberHContactFromJID(const char *jid);
char *JabberLoginFromJID(const char *jid);
char *JabberResourceFromJID(const char *jid);
char *JabberNickFromJID(const char *jid);
char *JabberLocalNickFromJID(const char *jid);
void JabberUrlDecode(char *str);
char *JabberUrlEncode(const char *str);
char *JabberUtf8Decode(const char *str);
char *JabberUtf8Encode(const char *str);
char *JabberSha1(char *str);
char *TlenSha1(char *str, int len);
char *JabberUnixToDos(const char *str);
void JabberDosToUnix(char *str);
void JabberHttpUrlDecode(char *str);
char *JabberHttpUrlEncode(const char *str);
int JabberCombineStatus(int status1, int status2);
char *JabberErrorStr(int errorCode);
char *JabberErrorMsg(XmlNode *errorNode);
void JabberSendVisibleInvisiblePresence(BOOL invisible);
#ifdef TLEN_PLUGIN
char *TlenPasswordHash(const char *str);
void TlenUrlDecode(char *str);
char *TlenUrlEncode(const char *str);
#endif
char *JabberTextEncode(const char *str);
char *JabberTextDecode(const char *str);
char *JabberBase64Encode(const char *buffer, int bufferLen);
char *JabberBase64Decode(const char *buffer, int *resultLen);
char *JabberGetVersionText();
time_t JabberIsoToUnixTime(char *stamp);
time_t TlenTimeToUTC(time_t time);
int JabberCountryNameToId(char *ctry);
void JabberSendPresenceTo(int status, char *to, char *extra);
void JabberSendPresence();
char *JabberRtfEscape(char *str);
void JabberStringAppend(char **str, int *sizeAlloced, const char *fmt, ...);
char *JabberGetClientJID(char *jid);
// jabber_misc.c
void JabberDBAddAuthRequest(char *jid, char *nick);
HANDLE JabberDBCreateContact(char *jid, char *nick, BOOL temporary, BOOL stripResource);
void JabberContactListCreateGroup(char *groupName);
unsigned long JabberForkThread(void (__cdecl *threadcode)(void*), unsigned long stacksize, void *arg);
// jabber_svc.c
int JabberGetInfo(WPARAM wParam, LPARAM lParam);
// jabber_advsearch.c
extern JABBER_FIELD_MAP tlenFieldGender[];
extern JABBER_FIELD_MAP tlenFieldLookfor[];
extern JABBER_FIELD_MAP tlenFieldStatus[];
extern JABBER_FIELD_MAP tlenFieldOccupation[];
extern JABBER_FIELD_MAP tlenFieldPlan[];

BOOL CALLBACK TlenAdvSearchDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
char *TlenAdvSearchCreateQuery(HWND hwndDlg, int iqId);

#endif

