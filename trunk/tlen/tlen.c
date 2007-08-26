/*

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

#include "jabber.h"
#include "tlen_muc.h"
#include "tlen_file.h"
#include "tlen_voice.h"
#include "jabber_list.h"
#include "jabber_iq.h"
#include "resource.h"
#include <m_file.h>
#include <richedit.h>
#include <ctype.h>
#include <m_icolib.h>
#include <m_avatars.h>

HINSTANCE hInst;
PLUGINLINK *pluginLink;

struct MM_INTERFACE memoryManagerInterface;
HANDLE hEventSkin2IconsChanged;

PLUGININFO pluginInfo = {
	sizeof(PLUGININFO),
	"Tlen Protocol",
	TLEN_VERSION,
	"Tlen protocol plugin for Miranda IM ("TLEN_VERSION_STRING" "__DATE__")",
	"Santithorn Bunchua, Adam Strzelecki, Piotr Piastucki",
	"the_leech@users.berlios.de",
	"(c) 2002-2007 Santithorn Bunchua, Piotr Piastucki",
	"http://mtlen.berlios.de",
	0,
	0
};

PLUGININFOEX pluginInfoEx = {
	sizeof(PLUGININFOEX),
	"Tlen Protocol",
	TLEN_VERSION,
	"Tlen protocol plugin for Miranda IM ("TLEN_VERSION_STRING" "__DATE__")",
	"Santithorn Bunchua, Adam Strzelecki, Piotr Piastucki",
	"the_leech@users.berlios.de",
	"(c) 2002-2007 Santithorn Bunchua, Piotr Piastucki",
	"http://mtlen.berlios.de",
	0,
	0,
    {0x11fc3484, 0x475c, 0x11dc, { 0x83, 0x14, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 }} 
};



HANDLE hMainThread;
DWORD jabberMainThreadId;
char *jabberProtoName;	// "TLEN"
char *jabberModuleName;	// "Tlen"
CRITICAL_SECTION mutex;
HANDLE hNetlibUser;
HANDLE hFileNetlibUser;
// Main jabber server connection thread global variables
struct ThreadData *jabberThreadInfo;
BOOL jabberConnected;
BOOL jabberOnline;
int jabberStatus;
int jabberDesiredStatus;
char *jabberJID = NULL;
char *streamId;
UINT jabberCodePage;
JABBER_MODEMSGS modeMsgs;
//char *jabberModeMsg;
CRITICAL_SECTION modeMsgMutex;
char *jabberVcardPhotoFileName;
char *jabberVcardPhotoType;
BOOL jabberSendKeepAlive;

HANDLE hTlenNudge = NULL;

HANDLE hMenuMUC, hMenuChats, hMenuInbox;
HANDLE hMenuContactMUC;
HANDLE hMenuContactVoice;
HANDLE hMenuContactGrantAuth;
HANDLE hMenuContactRequestAuth;
HANDLE hMenuContactFile;

HWND hwndJabberVcard;

HICON tlenIcons[TLEN_ICON_TOTAL];

void TlenLoadOptions();
int TlenOptInit(WPARAM wParam, LPARAM lParam);
int TlenUserInfoInit(WPARAM wParam, LPARAM lParam);
int JabberMsgUserTyping(WPARAM wParam, LPARAM lParam);
extern int JabberSvcInit(void);
extern int JabberSvcUninit();
int TlenContactMenuHandleRequestAuth(WPARAM wParam, LPARAM lParam);
int TlenContactMenuHandleGrantAuth(WPARAM wParam, LPARAM lParam);
int TlenContactMenuHandleSendPicture(WPARAM wParam, LPARAM lParam);

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpvReserved)
{
#ifdef _DEBUG
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	hInst = hModule;
	return TRUE;
}

__declspec(dllexport) PLUGININFO *MirandaPluginInfo(DWORD mirandaVersion)
{
	if (mirandaVersion < PLUGIN_MAKE_VERSION(0,6,0,0)) {
		MessageBoxA(NULL, "The Tlen protocol plugin cannot be loaded. It requires Miranda IM 0.6 or later.", "Tlen Protocol Plugin", MB_OK|MB_ICONWARNING|MB_SETFOREGROUND|MB_TOPMOST);
		return NULL;
	}
	return &pluginInfo;
}


__declspec(dllexport) PLUGININFOEX *MirandaPluginInfoEx( DWORD mirandaVersion )
{
	if ( mirandaVersion < PLUGIN_MAKE_VERSION( 0,7,0,20 )) {
		MessageBoxA( NULL, "The Tlen protocol plugin cannot be loaded. It requires Miranda IM 0.7 or later.", "Jabber Protocol Plugin", MB_OK|MB_ICONWARNING|MB_SETFOREGROUND|MB_TOPMOST );
		return NULL;
	}

	return &pluginInfoEx;
}

static const MUUID interfaces[] = {MIID_PROTOCOL, MIID_LAST};
__declspec(dllexport) const MUUID* MirandaPluginInterfaces(void)
{
	return interfaces;
}

int __declspec(dllexport) Unload(void)
{
#ifdef _DEBUG
	OutputDebugString("Unloading...");
#endif
	JabberLog("Unloading");
	UnhookEvents_Ex();
	if (hTlenNudge)
		DestroyHookableEvent(hTlenNudge);
	JabberSvcUninit();
	JabberWsUninit();
	//JabberSslUninit();
	JabberListUninit();
	JabberIqUninit();
	JabberSerialUninit();
	DeleteCriticalSection(&modeMsgMutex);
	DeleteCriticalSection(&mutex);
	mir_free(modeMsgs.szOnline);
	mir_free(modeMsgs.szAway);
	mir_free(modeMsgs.szNa);
	mir_free(modeMsgs.szDnd);
	mir_free(modeMsgs.szFreechat);
	mir_free(modeMsgs.szInvisible);
	//mir_free(jabberModeMsg);
	mir_free(jabberModuleName);
	mir_free(jabberProtoName);
	if (streamId) mir_free(streamId);

#ifdef _DEBUG
	OutputDebugString("Finishing Unload, returning to Miranda");
	//OutputDebugString("========== Memory leak from TLEN");
	//_CrtDumpMemoryLeaks();
	//OutputDebugString("========== End memory leak from TLEN");
#endif
	return 0;
}

static int PreShutdown(WPARAM wParam, LPARAM lParam)
{
#ifdef _DEBUG
	OutputDebugString("PreShutdown...");
#endif
	JabberLog("Preshutdown");
	TlenVoiceCancelAll();
	TlenFileCancelAll();
	return 0;
}

static int ModulesLoaded(WPARAM wParam, LPARAM lParam)
{
	char str[128];
	JabberWsInit();
	//JabberSslInit();
	TlenMUCInit();
	HookEvent_Ex(ME_USERINFO_INITIALISE, TlenUserInfoInit);
	sprintf(str, "%s", TranslateT("Incoming mail"));
	SkinAddNewSoundEx("TlenMailNotify", jabberModuleName, str);
	sprintf(str, "%s", TranslateT("Alert"));
	SkinAddNewSoundEx("TlenAlertNotify", jabberModuleName, str);
	sprintf(str, "%s", TranslateT("Voice chat"));
	SkinAddNewSoundEx("TlenVoiceNotify", jabberModuleName, str);
	return 0;
}

static void GetIconName(char *buffer, int bufferSize, int icon) {
	const char *sufixes[]= {
		"PROTO", "MAIL", "MUC", "CHATS", "GRANT", "REQUEST", "VOICE", "MICROPHONE", "SPEAKER"
	};
	mir_snprintf(buffer, bufferSize, "%s_%s", jabberProtoName, sufixes[icon]);
}

static TCHAR *GetIconDescription(int icon) {
	const char *keys[]= {
		_T("Protocol icon"),
		_T("Mail"),
		_T("Group chats"),
		_T("Tlen chats"),
		_T("Grant authorization"),
		_T("Request authorization"),
		_T("Voice chat"),
		_T("Microphone"),
		_T("Speaker")
	};
	return TranslateT(keys[icon]);
}

static void TlenReleaseIcons() {
	int i;
	char iconName[256];
	for (i=0; i<TLEN_ICON_TOTAL; i++) {
		if (hEventSkin2IconsChanged) {
			GetIconName(iconName, sizeof(iconName), i);
			CallService(MS_SKIN2_RELEASEICON, 0, (LPARAM)iconName);
		} else {
			DestroyIcon(tlenIcons[i]);
		}
	}
}

static void TlenLoadIcons() {
	int i;
	char iconName[256];
	static int iconList[] = {
		IDI_TLEN,
		IDI_MAIL,
		IDI_MUC,
		IDI_CHATS,
		IDI_GRANT,
		IDI_REQUEST,
		IDI_VOICE,
		IDI_MICROPHONE,
		IDI_SPEAKER,
	};
	for (i=0; i<TLEN_ICON_TOTAL; i++) {
		if (hEventSkin2IconsChanged) {
			GetIconName(iconName, sizeof(iconName), i);
			tlenIcons[i] = (HICON) CallService(MS_SKIN2_GETICON, 0, (LPARAM)iconName);
		} else {
			tlenIcons[i] = (HICON) LoadImage(hInst, MAKEINTRESOURCE(iconList[i]), IMAGE_ICON, 0, 0, 0);
		}
	}
}

static int IcoLibIconsChanged(WPARAM wParam, LPARAM lParam)
{
	CLISTMENUITEM mi;
	mi.cbSize = sizeof(mi);
	mi.flags = CMIM_ICON;
	TlenReleaseIcons();
	TlenLoadIcons();
	mi.hIcon = tlenIcons[TLEN_IDI_MUC];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuMUC, (LPARAM) &mi);
	mi.hIcon = tlenIcons[TLEN_IDI_CHATS];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuChats, (LPARAM) &mi);
	mi.hIcon = tlenIcons[TLEN_IDI_MAIL];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuInbox, (LPARAM) &mi);
	mi.hIcon = tlenIcons[TLEN_IDI_MUC];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactMUC, (LPARAM) &mi);
	mi.hIcon = tlenIcons[TLEN_IDI_VOICE];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactVoice, (LPARAM) &mi);
	mi.hIcon = tlenIcons[TLEN_IDI_GRANT];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactGrantAuth, (LPARAM) &mi);
	mi.hIcon = tlenIcons[TLEN_IDI_REQUEST];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactRequestAuth, (LPARAM) &mi);
	return 0;
}

static void TlenRegisterIcons()
{
	hEventSkin2IconsChanged = HookEvent_Ex(ME_SKIN2_ICONSCHANGED, IcoLibIconsChanged);
	if (hEventSkin2IconsChanged) {
		char iconName[256];
		char path[MAX_PATH];
		SKINICONDESC sid = { 0 };
		GetModuleFileNameA(hInst, path, MAX_PATH);
		sid.cbSize = sizeof(SKINICONDESC);
		sid.cx = sid.cy = 16;
		sid.pszSection = jabberModuleName;
		sid.pszDefaultFile = path;
		GetIconName(iconName, sizeof(iconName), TLEN_IDI_TLEN);
		sid.pszName = (char *) iconName;
		sid.iDefaultIndex = -IDI_TLEN;
		sid.pszDescription = GetIconDescription(TLEN_IDI_TLEN);
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
		GetIconName(iconName, sizeof(iconName), TLEN_IDI_MAIL);
		sid.pszName = (char *) iconName;
		sid.iDefaultIndex = -IDI_MAIL;
		sid.pszDescription = GetIconDescription(TLEN_IDI_MAIL);
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
		GetIconName(iconName, sizeof(iconName), TLEN_IDI_MUC);
		sid.pszName = (char *) iconName;
		sid.iDefaultIndex = -IDI_MUC;
		sid.pszDescription = GetIconDescription(TLEN_IDI_MUC);
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
		GetIconName(iconName, sizeof(iconName), TLEN_IDI_CHATS);
		sid.pszName = (char *) iconName;
		sid.iDefaultIndex = -IDI_CHATS;
		sid.pszDescription = GetIconDescription(TLEN_IDI_CHATS);
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
		GetIconName(iconName, sizeof(iconName), TLEN_IDI_GRANT);
		sid.pszName = (char *) iconName;
		sid.iDefaultIndex = -IDI_GRANT;
		sid.pszDescription = GetIconDescription(TLEN_IDI_GRANT);
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
		GetIconName(iconName, sizeof(iconName), TLEN_IDI_REQUEST);
		sid.pszName = (char *) iconName;
		sid.iDefaultIndex = -IDI_REQUEST;
		sid.pszDescription = GetIconDescription(TLEN_IDI_REQUEST);
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
		GetIconName(iconName, sizeof(iconName), TLEN_IDI_VOICE);
		sid.pszName = (char *) iconName;
		sid.iDefaultIndex = -IDI_VOICE;
		sid.pszDescription = GetIconDescription(TLEN_IDI_VOICE);
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
		GetIconName(iconName, sizeof(iconName), TLEN_IDI_MICROPHONE);
		sid.pszName = (char *) iconName;
		sid.iDefaultIndex = -IDI_MICROPHONE;
		sid.pszDescription = GetIconDescription(TLEN_IDI_MICROPHONE);
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
		GetIconName(iconName, sizeof(iconName), TLEN_IDI_SPEAKER);
		sid.pszName = (char *) iconName;
		sid.iDefaultIndex = -IDI_SPEAKER;
		sid.pszDescription = GetIconDescription(TLEN_IDI_SPEAKER);
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
	}
	TlenLoadIcons();
}

static int TlenPrebuildContactMenu(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact;
	DBVARIANT dbv;
	CLISTMENUITEM clmi = {0};
	JABBER_LIST_ITEM *item;
	clmi.cbSize = sizeof(CLISTMENUITEM);
	if ((hContact=(HANDLE) wParam)!=NULL && jabberOnline) {
		if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
			if ((item=JabberListGetItemPtr(LIST_ROSTER, dbv.pszVal)) != NULL) {
				if (item->subscription==SUB_NONE || item->subscription==SUB_FROM)
					clmi.flags = CMIM_FLAGS;
				else
					clmi.flags = CMIM_FLAGS|CMIF_HIDDEN;
				CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactRequestAuth, (LPARAM) &clmi);

				if (item->subscription==SUB_NONE || item->subscription==SUB_TO)
					clmi.flags = CMIM_FLAGS;
				else
					clmi.flags = CMIM_FLAGS|CMIF_HIDDEN;
				CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactGrantAuth, (LPARAM) &clmi);

				if (item->status!=ID_STATUS_OFFLINE)
					clmi.flags = CMIM_FLAGS;
				else
					clmi.flags = CMIM_FLAGS|CMIF_HIDDEN;
				CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactMUC, (LPARAM) &clmi);

				if (item->status!=ID_STATUS_OFFLINE && !TlenVoiceIsInUse())
					clmi.flags = CMIM_FLAGS;
				else
					clmi.flags = CMIM_FLAGS|CMIF_HIDDEN;
				CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactVoice, (LPARAM) &clmi);

				DBFreeVariant(&dbv);
				return 0;
			}
			DBFreeVariant(&dbv);
		}
	}
	clmi.flags = CMIM_FLAGS|CMIF_HIDDEN;
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactMUC, (LPARAM) &clmi);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactVoice, (LPARAM) &clmi);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactRequestAuth, (LPARAM) &clmi);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactGrantAuth, (LPARAM) &clmi);
	return 0;
}

int TlenMenuHandleInbox(WPARAM wParam, LPARAM lParam)
{
	DBVARIANT dbv;
	char *login = NULL, *password = NULL;
	char szFileName[ MAX_PATH ];
	if (DBGetContactSettingByte(NULL, jabberProtoName, "SavePassword", TRUE) == TRUE) {
		int tPathLen;
		CallService( MS_DB_GETPROFILEPATH, MAX_PATH, (LPARAM) szFileName );
		tPathLen = strlen( szFileName );
		tPathLen += mir_snprintf( szFileName + tPathLen, MAX_PATH - tPathLen, "\\%s\\", jabberModuleName);
		CreateDirectoryA( szFileName, NULL );
		mir_snprintf( szFileName + tPathLen, MAX_PATH - tPathLen, "openinbox.html" );
		if (!DBGetContactSetting(NULL, jabberProtoName, "LoginName", &dbv)) {
			login = mir_strdup(dbv.pszVal);
			DBFreeVariant(&dbv);

		}
		if (!DBGetContactSetting(NULL, jabberProtoName, "Password", &dbv)) {
			CallService(MS_DB_CRYPT_DECODESTRING, strlen(dbv.pszVal)+1, (LPARAM) dbv.pszVal);
			password = mir_strdup(dbv.pszVal);
			DBFreeVariant(&dbv);
		}
	} 
	if (login != NULL && password != NULL) {
		FILE *out;
		out = fopen( szFileName, "wt" );
		if ( out != NULL ) {
			fprintf(out, "<html><head></head><body OnLoad=\"document.forms[0].submit();\">"
						 "<form action=\"http://poczta.o2.pl/index.php\" method=\"post\" name=\"login_form\">"
						 "<input type=\"hidden\" name=\"username\" value=\"%s\">"
						 "<input type=\"hidden\" name=\"password\" value=\"%s\">"
						 "</form></body></html>", login, password);
			fclose( out );
		} else {
			strcat(szFileName, "http://poczta.o2.pl/");
		}
	} else {
		strcat(szFileName, "http://poczta.o2.pl/");
	}
	mir_free(login);
	mir_free(password);
	CallService(MS_UTILS_OPENURL, (WPARAM) 1, (LPARAM) szFileName);
	return 0;
}

int __declspec(dllexport) Load(PLUGINLINK *link)
{
	PROTOCOLDESCRIPTOR pd;
	HANDLE hContact;
	char s[256];
	char text[_MAX_PATH];
	char *p, *q;
	char *szProto;
	CLISTMENUITEM mi, clmi;
	DBVARIANT dbv;

	pluginLink = link;
	memoryManagerInterface.cbSize = sizeof(struct MM_INTERFACE);
	CallService(MS_SYSTEM_GET_MMI,0,(LPARAM)&memoryManagerInterface);

	GetModuleFileName(hInst, text, sizeof(text));
	p = strrchr(text, '\\');
	p++;
	q = strrchr(p, '.');
	*q = '\0';
	jabberProtoName = mir_strdup(p);
	_strupr(jabberProtoName);

	jabberModuleName = mir_strdup(jabberProtoName);
	_strlwr(jabberModuleName);
	jabberModuleName[0] = toupper(jabberModuleName[0]);

	JabberLog("Setting protocol/module name to '%s/%s'", jabberProtoName, jabberModuleName);

	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &hMainThread, THREAD_SET_CONTEXT, FALSE, 0);
	jabberMainThreadId = GetCurrentThreadId();
	//hLibSSL = NULL;
//	hWndListGcLog = (HANDLE) CallService(MS_UTILS_ALLOCWINDOWLIST, 0, 0);

	TlenRegisterIcons();
	TlenLoadOptions();

	HookEvent_Ex(ME_OPT_INITIALISE, TlenOptInit);
	HookEvent_Ex(ME_SYSTEM_MODULESLOADED, ModulesLoaded);
	HookEvent_Ex(ME_SYSTEM_PRESHUTDOWN, PreShutdown);

	sprintf(s, "%s/%s", jabberProtoName, "Nudge");
	hTlenNudge = CreateHookableEvent(s);

	// Register protocol module
	ZeroMemory(&pd, sizeof(PROTOCOLDESCRIPTOR));
	pd.cbSize = sizeof(PROTOCOLDESCRIPTOR);
	pd.szName = jabberProtoName;
	pd.type = PROTOTYPE_PROTOCOL;
	CallService(MS_PROTO_REGISTERMODULE, 0, (LPARAM) &pd);


	memset(&mi, 0, sizeof(CLISTMENUITEM));
	mi.cbSize = sizeof(CLISTMENUITEM);
	memset(&clmi, 0, sizeof(CLISTMENUITEM));
	clmi.cbSize = sizeof(CLISTMENUITEM);
	clmi.flags = CMIM_FLAGS | CMIF_GRAYED;

	mi.pszPopupName = jabberModuleName;
	mi.popupPosition = 500090000;

	wsprintf(text, "%s/MainMenuChats", jabberModuleName);
	CreateServiceFunction_Ex(text, TlenMUCMenuHandleChats);
	mi.pszName = TranslateT("Tlen Chats");
	mi.position = 2000050001;
	mi.hIcon = tlenIcons[TLEN_IDI_CHATS];
	mi.pszService = text;
	hMenuChats = (HANDLE) CallService(MS_CLIST_ADDMAINMENUITEM, 0, (LPARAM) &mi);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuChats, (LPARAM) &clmi);

	// "Multi-User Conference"
	wsprintf(text, "%s/MainMenuMUC", jabberModuleName);
	CreateServiceFunction_Ex(text, TlenMUCMenuHandleMUC);
	mi.pszName = TranslateT("Multi-User Conference");
	mi.position = 2000050002;
	mi.hIcon = tlenIcons[TLEN_IDI_MUC];
	mi.pszService = text;
	hMenuMUC = (HANDLE) CallService(MS_CLIST_ADDMAINMENUITEM, 0, (LPARAM) &mi);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuMUC, (LPARAM) &clmi);

	wsprintf(text, "%s/MainMenuInbox", jabberModuleName);
	CreateServiceFunction_Ex(text, TlenMenuHandleInbox);
	mi.pszName = TranslateT("Tlen Inbox");
	mi.position = 2000050003;
	mi.hIcon = tlenIcons[TLEN_IDI_MAIL];
	mi.pszService = text;
	hMenuInbox = (HANDLE) CallService(MS_CLIST_ADDMAINMENUITEM, 0, (LPARAM) &mi);

	// "Invite to MUC"
	sprintf(text, "%s/ContactMenuMUC", jabberModuleName);
	CreateServiceFunction_Ex(text, TlenMUCContactMenuHandleMUC);
	mi.pszName = TranslateT("Multi-User Conference");
	mi.position = -2000020000;
	mi.hIcon = tlenIcons[TLEN_IDI_MUC];
	mi.pszService = text;
	mi.pszContactOwner = jabberProtoName;
	hMenuContactMUC = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	// "Invite to voice chat"
	sprintf(text, "%s/ContactMenuVoice", jabberModuleName);
	CreateServiceFunction_Ex(text, TlenVoiceContactMenuHandleVoice);
	mi.pszName = TranslateT("Voice Chat");
	mi.position = -2000018000;
	mi.hIcon = tlenIcons[TLEN_IDI_VOICE];
	mi.pszService = text;
	mi.pszContactOwner = jabberProtoName;
	hMenuContactVoice = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	// "Request authorization"
	sprintf(text, "%s/RequestAuth", jabberModuleName);
	CreateServiceFunction_Ex(text, TlenContactMenuHandleRequestAuth);
	mi.pszName = TranslateT("Request authorization");
	mi.position = -2000001001;
	mi.hIcon = tlenIcons[TLEN_IDI_REQUEST];
	mi.pszService = text;
	mi.pszContactOwner = jabberProtoName;
	hMenuContactRequestAuth = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	// "Grant authorization"
	sprintf(text, "%s/GrantAuth", jabberModuleName);
	CreateServiceFunction_Ex(text, TlenContactMenuHandleGrantAuth);
	mi.pszName = TranslateT("Grant authorization");
	mi.position = -2000001000;
	mi.hIcon = tlenIcons[TLEN_IDI_GRANT];
	mi.pszService = text;
	mi.pszContactOwner = jabberProtoName;
	hMenuContactGrantAuth = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	// "Send picture"
	sprintf(text, "%s/SendPicture", jabberModuleName);
	CreateServiceFunction_Ex(text, TlenContactMenuHandleSendPicture);
	mi.pszName = TranslateT("Send picture");
//	clmi.flags = CMIM_FLAGS | CMIF_NOTOFFLINE;// | CMIF_GRAYED;
	mi.position = -2000001002;
	mi.hIcon = tlenIcons[TLEN_IDI_GRANT];
	mi.pszService = text;
	mi.pszContactOwner = jabberProtoName;
	//CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi); //hMenuContactGrantAuth = (HANDLE)

	mi.position = -2000020000;
	mi.flags = CMIF_NOTONLINE;
	mi.hIcon = LoadSkinnedIcon(SKINICON_EVENT_FILE);
	mi.pszName = TranslateT("&File");
	mi.pszService = MS_FILE_SENDFILE;
	mi.pszContactOwner = jabberProtoName;
	hMenuContactFile = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	HookEvent_Ex(ME_CLIST_PREBUILDCONTACTMENU, TlenPrebuildContactMenu);

	if (!DBGetContactSetting(NULL, jabberProtoName, "LoginServer", &dbv)) {
		DBFreeVariant(&dbv);
	} else {
		DBWriteContactSettingString(NULL, jabberProtoName, "LoginServer", "tlen.pl");
	}
	if (!DBGetContactSetting(NULL, jabberProtoName, "ManualHost", &dbv)) {
		DBFreeVariant(&dbv);
	} else {
		DBWriteContactSettingString(NULL, jabberProtoName, "ManualHost", "s1.tlen.pl");
	}

	// Set all contacts to offline
	hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
	while (hContact != NULL) {
		szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
		if(szProto!=NULL && !strcmp(szProto, jabberProtoName)) {
			if (DBGetContactSettingWord(hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE) != ID_STATUS_OFFLINE) {
				DBWriteContactSettingWord(hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE);
			}
		}
		hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
	}

	streamId = NULL;
	jabberThreadInfo = NULL;
	jabberConnected = FALSE;
	jabberOnline = FALSE;
	jabberStatus = ID_STATUS_OFFLINE;
	memset((char *) &modeMsgs, 0, sizeof(JABBER_MODEMSGS));
	//jabberModeMsg = NULL;
	jabberCodePage = CP_ACP;

	InitializeCriticalSection(&mutex);
	InitializeCriticalSection(&modeMsgMutex);

	srand((unsigned) time(NULL));
	JabberSerialInit();
	JabberIqInit();
	JabberListInit();
	JabberSvcInit();

	return 0;
}

int TlenContactMenuHandleRequestAuth(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact;
	DBVARIANT dbv;

	if ((hContact=(HANDLE) wParam)!=NULL && jabberOnline) {
		if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
			JabberSend(jabberThreadInfo->s, "<presence to='%s' type='subscribe'/>", dbv.pszVal);
			DBFreeVariant(&dbv);
		}
	}
	return 0;
}

int TlenContactMenuHandleGrantAuth(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact;
	DBVARIANT dbv;

	if ((hContact=(HANDLE) wParam)!=NULL && jabberOnline) {
		if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
			JabberSend(jabberThreadInfo->s, "<presence to='%s' type='subscribed'/>", dbv.pszVal);
			DBFreeVariant(&dbv);
		}
	}
	return 0;
}

int TlenContactMenuHandleSendPicture(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact;
	DBVARIANT dbv;

	if ((hContact=(HANDLE) wParam)!=NULL && jabberOnline) {
		if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
			JabberSend(jabberThreadInfo->s, "<message type='pic' to='the_leech7@tlen.pl' crc='da4fe23' idt='2174' size='21161'/>");
//			JabberSend(jabberThreadInfo->s, "<message type='pic' to='%s' crc='b4f7bdd' idt='6195' size='5583'/>", dbv.pszVal);
			DBFreeVariant(&dbv);
		}
	}
	return 0;
}
