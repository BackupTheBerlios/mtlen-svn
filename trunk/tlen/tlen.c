/*

Tlen Protocol Plugin for Miranda IM
Copyright (C) 2002-2004  Santithorn Bunchua

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
//#include "jabber_ssl.h"
#include "jabber_list.h"
#include "jabber_iq.h"
#include "resource.h"
#include <richedit.h>
#include <ctype.h>

HINSTANCE hInst;
PLUGINLINK *pluginLink;

PLUGININFO pluginInfo = {
	sizeof(PLUGININFO),
	"Tlen Protocol",
	PLUGIN_MAKE_VERSION(1,0,6,2),
	"Tlen protocol plugin for Miranda IM (1.0.6.2 "__DATE__")",
	"Santithorn Bunchua, Adam Strzelecki, Piotr Piastucki",
	"the_leech@users.berlios.de",
	"(c) 2002-2005 Santithorn Bunchua, Piotr Piastucki",
	"http://mtlen.berlios.de",
	0,
	0
};

HANDLE hMainThread;
DWORD jabberMainThreadId;
char *jabberProtoName;	// "JABBER"
char *jabberModuleName;	// "Jabber"
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
DWORD jabberLocalIP;
UINT jabberCodePage;
JABBER_MODEMSGS modeMsgs;
//char *jabberModeMsg;
CRITICAL_SECTION modeMsgMutex;
char *jabberVcardPhotoFileName;
char *jabberVcardPhotoType;
BOOL jabberSendKeepAlive;

HANDLE hEventSettingChanged;
HANDLE hEventContactDeleted;
#ifndef TLEN_PLUGIN
// SSL-related global variable
HANDLE hLibSSL;
PVOID jabberSslCtx;
#endif

HANDLE hMenuMUC;
HANDLE hMenuChats;
HANDLE hMenuContactMUC;
HANDLE hMenuContactVoice;
HANDLE hMenuContactGrantAuth;
HANDLE hMenuContactRequestAuth;

HWND hwndJabberVcard;
#ifndef TLEN_PLUGIN
HWND hwndJabberChangePassword;
#endif

HICON tlenIcons[TLEN_ICON_TOTAL];


int TlenOptInit(WPARAM wParam, LPARAM lParam);
int TlenUserInfoInit(WPARAM wParam, LPARAM lParam);
int JabberMsgUserTyping(WPARAM wParam, LPARAM lParam);
int JabberSvcInit(void);
int TlenContactMenuHandleRequestAuth(WPARAM wParam, LPARAM lParam);
int TlenContactMenuHandleGrantAuth(WPARAM wParam, LPARAM lParam);

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
	if (mirandaVersion < PLUGIN_MAKE_VERSION(0,3,1,0)) {
		MessageBox(NULL, "The Tlen protocol plugin cannot be loaded. It requires Miranda IM 0.3.1 or later.", "Tlen Protocol Plugin", MB_OK|MB_ICONWARNING|MB_SETFOREGROUND|MB_TOPMOST);
		return NULL;
	}
	return &pluginInfo;
}

int __declspec(dllexport) Unload(void)
{
#ifdef _DEBUG
	OutputDebugString("Unloading...");
#endif
	JabberLog("Unloading");
	UnhookEvent(hEventSettingChanged);
	UnhookEvent(hEventContactDeleted);
	JabberWsUninit();
	//JabberSslUninit();
	JabberListUninit();
	JabberIqUninit();
	JabberSerialUninit();
	DeleteCriticalSection(&modeMsgMutex);
	DeleteCriticalSection(&mutex);
	free(modeMsgs.szOnline);
	free(modeMsgs.szAway);
	free(modeMsgs.szNa);
	free(modeMsgs.szDnd);
	free(modeMsgs.szFreechat);
	free(modeMsgs.szInvisible);
	//free(jabberModeMsg);
	free(jabberModuleName);
	free(jabberProtoName);
	if (jabberVcardPhotoFileName) {
		DeleteFile(jabberVcardPhotoFileName);
		free(jabberVcardPhotoFileName);
	}
	if (jabberVcardPhotoType) free(jabberVcardPhotoType);
	if (streamId) free(streamId);

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
	HookEvent(ME_USERINFO_INITIALISE, TlenUserInfoInit);
	sprintf(str, "%s: %s", jabberModuleName, Translate("Incoming mail"));
	SkinAddNewSound("TlenMailNotify", str, "tlennotify.wav");
	sprintf(str, "%s: %s", jabberModuleName, Translate("Alert"));
	SkinAddNewSound("TlenAlertNotify", str, "tlennotify.wav");
	return 0;
}

static void TlenIconInit()
{
	int i;
	static int iconList[] = {
		IDI_TLEN,
		IDI_MUC,
		IDI_MICROPHONE,
		IDI_SPEAKER
	};
	for (i=0; i<TLEN_ICON_TOTAL; i++)
		tlenIcons[i] = LoadImage(hInst, MAKEINTRESOURCE(iconList[i]), IMAGE_ICON, 0, 0, 0);
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
//	TlenMUCPrebuildContactMenu(wParam, lParam);
//	TlenVoicePrebuildContactMenu(wParam, lParam);
	return 0;
}


int __declspec(dllexport) Load(PLUGINLINK *link)
{
	PROTOCOLDESCRIPTOR pd;
	HANDLE hContact;
	char text[_MAX_PATH];
	char *p, *q;
	char *szProto;
	CLISTMENUITEM mi, clmi;

	GetModuleFileName(hInst, text, sizeof(text));
	p = strrchr(text, '\\');
	p++;
	q = strrchr(p, '.');
	*q = '\0';
	jabberProtoName = _strdup(p);
	_strupr(jabberProtoName);

	jabberModuleName = _strdup(jabberProtoName);
	_strlwr(jabberModuleName);
	jabberModuleName[0] = toupper(jabberModuleName[0]);

	JabberLog("Setting protocol/module name to '%s/%s'", jabberProtoName, jabberModuleName);

	pluginLink = link;
	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &hMainThread, THREAD_SET_CONTEXT, FALSE, 0);
	jabberMainThreadId = GetCurrentThreadId();
	//hLibSSL = NULL;
//	hWndListGcLog = (HANDLE) CallService(MS_UTILS_ALLOCWINDOWLIST, 0, 0);

	HookEvent(ME_OPT_INITIALISE, TlenOptInit);
	HookEvent(ME_SYSTEM_MODULESLOADED, ModulesLoaded);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, PreShutdown);

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

	// "Multi-User Conference"
	wsprintf(text, "%s/MainMenuMUC", jabberModuleName);
	CreateServiceFunction(text, TlenMUCMenuHandleMUC);
	mi.pszName = Translate("Multi-User Conference");
	mi.position = 2000050001;
	mi.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_TLEN));
	mi.pszService = text;
	hMenuMUC = (HANDLE) CallService(MS_CLIST_ADDMAINMENUITEM, 0, (LPARAM) &mi);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuMUC, (LPARAM) &clmi);

	wsprintf(text, "%s/MainMenuChats", jabberModuleName);
	CreateServiceFunction(text, TlenMUCMenuHandleChats);
	mi.pszName = Translate("Tlen Chats...");
	mi.position = 2000050002;
	mi.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_TLEN));
	mi.pszService = text;
	hMenuChats = (HANDLE) CallService(MS_CLIST_ADDMAINMENUITEM, 0, (LPARAM) &mi);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuChats, (LPARAM) &clmi);

	// "Invite to MUC"
	sprintf(text, "%s/ContactMenuMUC", jabberModuleName);
	CreateServiceFunction(text, TlenMUCContactMenuHandleMUC);
	mi.pszName = Translate("Multi-User Conference");
	mi.position = -2000020000;
	mi.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MUC));
	mi.pszService = text;
	mi.pszContactOwner = jabberProtoName;
	hMenuContactMUC = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	// "Invite to voice chat"
	sprintf(text, "%s/ContactMenuVoice", jabberModuleName);
	CreateServiceFunction(text, TlenVoiceContactMenuHandleVoice);
	mi.pszName = Translate("Voice Chat");
	mi.position = -2000018000;
	mi.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MICROPHONE));
	mi.pszService = text;
	mi.pszContactOwner = jabberProtoName;
	hMenuContactVoice = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);


	// "Request authorization"
	sprintf(text, "%s/RequestAuth", jabberModuleName);
	CreateServiceFunction(text, TlenContactMenuHandleRequestAuth);
	mi.pszName = Translate("Request authorization");
	mi.position = -2000001001;
	mi.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_REQUEST));
	mi.pszService = text;
	mi.pszContactOwner = jabberProtoName;
	hMenuContactRequestAuth = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	// "Grant authorization"
	sprintf(text, "%s/GrantAuth", jabberModuleName);
	CreateServiceFunction(text, TlenContactMenuHandleGrantAuth);
	mi.pszName = Translate("Grant authorization");
	mi.position = -2000001000;
	mi.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_GRANT));
	mi.pszService = text;
	mi.pszContactOwner = jabberProtoName;
	hMenuContactGrantAuth = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);


	HookEvent(ME_CLIST_PREBUILDCONTACTMENU, TlenPrebuildContactMenu);

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
	jabberVcardPhotoFileName = NULL;
	jabberVcardPhotoType = NULL;
	memset((char *) &modeMsgs, 0, sizeof(JABBER_MODEMSGS));
	//jabberModeMsg = NULL;
	jabberCodePage = CP_ACP;

	InitializeCriticalSection(&mutex);
	InitializeCriticalSection(&modeMsgMutex);

	TlenIconInit();
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
