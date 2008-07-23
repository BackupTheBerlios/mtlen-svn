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


struct MM_INTERFACE mmi;
struct SHA1_INTERFACE sha1i;
struct MD5_INTERFACE   md5i;
struct UTF8_INTERFACE utfi;
HINSTANCE hInst;
PLUGINLINK *pluginLink;
HANDLE hMainThread;
HICON tlenIcons[TLEN_ICON_TOTAL];

PLUGININFOEX pluginInfoEx = {
	sizeof(PLUGININFOEX),
	"Tlen Protocol",
	TLEN_VERSION,
	"Tlen protocol plugin for Miranda IM ("TLEN_VERSION_STRING" "__DATE__")",
	"Santithorn Bunchua, Adam Strzelecki, Piotr Piastucki",
	"the_leech@users.berlios.de",
	"(c) 2002-2008 Santithorn Bunchua, Piotr Piastucki",
	"http://mtlen.berlios.de",
	0,
	0,
    #if defined( _UNICODE )
    {0x748f8934, 0x781a, 0x528d, { 0x52, 0x08, 0x00, 0x12, 0x65, 0x40, 0x4a, 0xb3 }}
    #else
    {0x11fc3484, 0x475c, 0x11dc, { 0x83, 0x14, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 }}
    #endif
};

// Main jabber server connection thread global variables

void TlenLoadOptions(TlenProtocol *proto);
int TlenOptionsInit(void *ptr, WPARAM wParam, LPARAM lParam);
int TlenUserInfoInit(void *ptr, WPARAM wParam, LPARAM lParam);
extern void TlenInitServicesVTbl(TlenProtocol *proto);
extern int JabberContactDeleted(void *ptr, WPARAM wParam, LPARAM lParam);
extern int JabberDbSettingChanged(void *ptr, WPARAM wParam, LPARAM lParam);

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpvReserved)
{
#ifdef _DEBUG
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	hInst = hModule;
	return TRUE;
}


__declspec(dllexport) PLUGININFOEX *MirandaPluginInfoEx( DWORD mirandaVersion )
{
	if ( mirandaVersion < PLUGIN_MAKE_VERSION( 0,8,0,10 )) {
		MessageBox( NULL, TranslateT("The Tlen protocol plugin cannot be loaded. It requires Miranda IM 0.7 or later."), TranslateT("Tlen Protocol Plugin"), MB_OK|MB_ICONWARNING|MB_SETFOREGROUND|MB_TOPMOST );
		return NULL;
	}

	return &pluginInfoEx;
}

static const MUUID interfaces[] = {MIID_PROTOCOL, MIID_LAST};
__declspec(dllexport) const MUUID* MirandaPluginInterfaces(void)
{
	return interfaces;
}

static void GetIconName(char *buffer, int bufferSize, int icon) {
	const char *sufixes[]= {
		"PROTO", "MAIL", "MUC", "CHATS", "GRANT", "REQUEST", "VOICE", "MICROPHONE", "SPEAKER"
	};
	mir_snprintf(buffer, bufferSize, "%s_%s", "TLEN", sufixes[icon]);
}

static TCHAR *GetIconDescription(int icon) {
	const TCHAR *keys[]= {
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
	return TranslateTS(keys[icon]);
}

static void TlenReleaseIcons() {
	int i;
	char iconName[256];
	for (i=0; i<TLEN_ICON_TOTAL; i++) {
		GetIconName(iconName, sizeof(iconName), i);
		CallService(MS_SKIN2_RELEASEICON, 0, (LPARAM)iconName);
	}
}

static void TlenLoadIcons() {
	int i;
	char iconName[256];
	for (i=0; i<TLEN_ICON_TOTAL; i++) {
		GetIconName(iconName, sizeof(iconName), i);
		tlenIcons[i] = (HICON) CallService(MS_SKIN2_GETICON, 0, (LPARAM)iconName);
	}
}

static int TlenIconsChanged(void *ptr, WPARAM wParam, LPARAM lParam)
{
	CLISTMENUITEM mi;
    TlenProtocol *proto = (TlenProtocol *)ptr;
	mi.cbSize = sizeof(mi);
	mi.flags = CMIM_ICON;
	TlenReleaseIcons();
	TlenLoadIcons();
	mi.hIcon = tlenIcons[TLEN_IDI_MUC];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuMUC, (LPARAM) &mi);
	mi.hIcon = tlenIcons[TLEN_IDI_CHATS];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuChats, (LPARAM) &mi);
	mi.hIcon = tlenIcons[TLEN_IDI_MAIL];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuInbox, (LPARAM) &mi);
	mi.hIcon = tlenIcons[TLEN_IDI_MUC];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactMUC, (LPARAM) &mi);
	mi.hIcon = tlenIcons[TLEN_IDI_VOICE];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactVoice, (LPARAM) &mi);
	mi.hIcon = tlenIcons[TLEN_IDI_GRANT];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactGrantAuth, (LPARAM) &mi);
	mi.hIcon = tlenIcons[TLEN_IDI_REQUEST];
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactRequestAuth, (LPARAM) &mi);
	return 0;
}

static void TlenRegisterIcons()
{
    char iconName[256];
    char path[MAX_PATH];
    SKINICONDESC sid = { 0 };
    GetModuleFileNameA(hInst, path, MAX_PATH);
    sid.cbSize = sizeof(SKINICONDESC);
    sid.cx = sid.cy = 16;
    sid.ptszSection = _T("Tlen");
    sid.pszDefaultFile = path;
    #ifdef UNICODE
    sid.flags = SIDF_UNICODE;
    #endif
    GetIconName(iconName, sizeof(iconName), TLEN_IDI_TLEN);
    sid.pszName = (char *) iconName;
    sid.iDefaultIndex = -IDI_TLEN;
    sid.ptszDescription = GetIconDescription(TLEN_IDI_TLEN);
    CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
    GetIconName(iconName, sizeof(iconName), TLEN_IDI_MAIL);
    sid.pszName = (char *) iconName;
    sid.iDefaultIndex = -IDI_MAIL;
    sid.ptszDescription = GetIconDescription(TLEN_IDI_MAIL);
    CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
    GetIconName(iconName, sizeof(iconName), TLEN_IDI_MUC);
    sid.pszName = (char *) iconName;
    sid.iDefaultIndex = -IDI_MUC;
    sid.ptszDescription = GetIconDescription(TLEN_IDI_MUC);
    CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
    GetIconName(iconName, sizeof(iconName), TLEN_IDI_CHATS);
    sid.pszName = (char *) iconName;
    sid.iDefaultIndex = -IDI_CHATS;
    sid.ptszDescription = GetIconDescription(TLEN_IDI_CHATS);
    CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
    GetIconName(iconName, sizeof(iconName), TLEN_IDI_GRANT);
    sid.pszName = (char *) iconName;
    sid.iDefaultIndex = -IDI_GRANT;
    sid.ptszDescription = GetIconDescription(TLEN_IDI_GRANT);
    CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
    GetIconName(iconName, sizeof(iconName), TLEN_IDI_REQUEST);
    sid.pszName = (char *) iconName;
    sid.iDefaultIndex = -IDI_REQUEST;
    sid.ptszDescription = GetIconDescription(TLEN_IDI_REQUEST);
    CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
    GetIconName(iconName, sizeof(iconName), TLEN_IDI_VOICE);
    sid.pszName = (char *) iconName;
    sid.iDefaultIndex = -IDI_VOICE;
    sid.ptszDescription = GetIconDescription(TLEN_IDI_VOICE);
    CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
    GetIconName(iconName, sizeof(iconName), TLEN_IDI_MICROPHONE);
    sid.pszName = (char *) iconName;
    sid.iDefaultIndex = -IDI_MICROPHONE;
    sid.ptszDescription = GetIconDescription(TLEN_IDI_MICROPHONE);
    CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
    GetIconName(iconName, sizeof(iconName), TLEN_IDI_SPEAKER);
    sid.pszName = (char *) iconName;
    sid.iDefaultIndex = -IDI_SPEAKER;
    sid.ptszDescription = GetIconDescription(TLEN_IDI_SPEAKER);
    CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
	TlenLoadIcons();
}

static int TlenPrebuildContactMenu(void *ptr, WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact;
	DBVARIANT dbv;
	CLISTMENUITEM clmi = {0};
	JABBER_LIST_ITEM *item;
    TlenProtocol *proto = (TlenProtocol *)ptr;
	clmi.cbSize = sizeof(CLISTMENUITEM);
	if ((hContact=(HANDLE) wParam)!=NULL && proto->isOnline) {
		if (!DBGetContactSetting(hContact, proto->iface.m_szModuleName, "jid", &dbv)) {
			if ((item=JabberListGetItemPtr(proto, LIST_ROSTER, dbv.pszVal)) != NULL) {
				if (item->subscription==SUB_NONE || item->subscription==SUB_FROM)
					clmi.flags = CMIM_FLAGS;
				else
					clmi.flags = CMIM_FLAGS|CMIF_HIDDEN;
				CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactRequestAuth, (LPARAM) &clmi);

				if (item->subscription==SUB_NONE || item->subscription==SUB_TO)
					clmi.flags = CMIM_FLAGS;
				else
					clmi.flags = CMIM_FLAGS|CMIF_HIDDEN;
				CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactGrantAuth, (LPARAM) &clmi);

				if (item->status!=ID_STATUS_OFFLINE)
					clmi.flags = CMIM_FLAGS;
				else
					clmi.flags = CMIM_FLAGS|CMIF_HIDDEN;
				CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactMUC, (LPARAM) &clmi);

				if (item->status!=ID_STATUS_OFFLINE && !TlenVoiceIsInUse(proto))
					clmi.flags = CMIM_FLAGS;
				else
					clmi.flags = CMIM_FLAGS|CMIF_HIDDEN;
				CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactVoice, (LPARAM) &clmi);

				DBFreeVariant(&dbv);
				return 0;
			}
			DBFreeVariant(&dbv);
		}
	}
	clmi.flags = CMIM_FLAGS|CMIF_HIDDEN;
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactMUC, (LPARAM) &clmi);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactVoice, (LPARAM) &clmi);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactRequestAuth, (LPARAM) &clmi);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactGrantAuth, (LPARAM) &clmi);
    if (proto->isOnline) {
    	clmi.flags = CMIM_FLAGS|CMIF_NOTONLINE;
    }
    CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuContactFile, (LPARAM) &clmi);
	return 0;
}

int TlenContactMenuHandleRequestAuth(void *ptr, WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact;
	DBVARIANT dbv;
    TlenProtocol *proto = (TlenProtocol *)ptr;
	if ((hContact=(HANDLE) wParam)!=NULL && proto->isOnline) {
		if (!DBGetContactSetting(hContact, proto->iface.m_szModuleName, "jid", &dbv)) {
			JabberSend(proto, "<presence to='%s' type='subscribe'/>", dbv.pszVal);
			DBFreeVariant(&dbv);
		}
	}
	return 0;
}

int TlenContactMenuHandleGrantAuth(void *ptr, WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact;
	DBVARIANT dbv;
    TlenProtocol *proto = (TlenProtocol *)ptr;
	if ((hContact=(HANDLE) wParam)!=NULL && proto->isOnline) {
		if (!DBGetContactSetting(hContact, proto->iface.m_szModuleName, "jid", &dbv)) {
			JabberSend(proto, "<presence to='%s' type='subscribed'/>", dbv.pszVal);
			DBFreeVariant(&dbv);
		}
	}
	return 0;
}

int TlenContactMenuHandleSendPicture(void *ptr, WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact;
	DBVARIANT dbv;
    TlenProtocol *proto = (TlenProtocol *)ptr;
	if ((hContact=(HANDLE) wParam)!=NULL && proto->isOnline) {
		if (!DBGetContactSetting(hContact, proto->iface.m_szModuleName, "jid", &dbv)) {
			JabberSend(proto, "<message type='pic' to='the_leech7@tlen.pl' crc='da4fe23' idt='2174' size='21161'/>");
//			JabberSend(proto, "<message type='pic' to='%s' crc='b4f7bdd' idt='6195' size='5583'/>", dbv.pszVal);
			DBFreeVariant(&dbv);
		}
	}
	return 0;
}

int TlenMenuHandleInbox(void *ptr, WPARAM wParam, LPARAM lParam)
{
	DBVARIANT dbv;
	char *login = NULL, *password = NULL;
	char szFileName[ MAX_PATH ];
    TlenProtocol *proto = (TlenProtocol *)ptr;
    if (DBGetContactSettingByte(NULL, proto->iface.m_szModuleName, "SavePassword", TRUE) == TRUE) {
		int tPathLen;
		CallService( MS_DB_GETPROFILEPATH, MAX_PATH, (LPARAM) szFileName );
		tPathLen = strlen( szFileName );
		tPathLen += mir_snprintf( szFileName + tPathLen, MAX_PATH - tPathLen, "\\%s\\", proto->iface.m_szProtoName);
		CreateDirectoryA( szFileName, NULL );
		mir_snprintf( szFileName + tPathLen, MAX_PATH - tPathLen, "openinbox.html" );
		if (!DBGetContactSetting(NULL, proto->iface.m_szModuleName, "LoginName", &dbv)) {
			login = mir_strdup(dbv.pszVal);
			DBFreeVariant(&dbv);

		}
		if (!DBGetContactSetting(NULL, proto->iface.m_szModuleName, "Password", &dbv)) {
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

int TlenOnModulesLoaded(void *ptr, WPARAM wParam, LPARAM lParam) {
    
	char str[128];
    TlenProtocol *proto = (TlenProtocol *)ptr;
    /* Set all contacts to offline */
	HANDLE hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
	while (hContact != NULL) {
		char *szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
		if(szProto!=NULL && !strcmp(szProto, proto->iface.m_szModuleName)) {
			if (DBGetContactSettingWord(hContact, proto->iface.m_szModuleName, "Status", ID_STATUS_OFFLINE) != ID_STATUS_OFFLINE) {
				DBWriteContactSettingWord(hContact, proto->iface.m_szModuleName, "Status", ID_STATUS_OFFLINE);
			}
		}
		hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
	}
	TlenMUCInit(proto);
	sprintf(str, "%s", Translate("Incoming mail"));
	SkinAddNewSoundEx("TlenMailNotify", proto->iface.m_szProtoName, str);
	sprintf(str, "%s", Translate("Alert"));
	SkinAddNewSoundEx("TlenAlertNotify", proto->iface.m_szProtoName, str);
	sprintf(str, "%s", Translate("Voice chat"));
	SkinAddNewSoundEx("TlenVoiceNotify", proto->iface.m_szProtoName, str);

    HookEventObj_Ex(ME_USERINFO_INITIALISE, proto, TlenUserInfoInit);

    return 0;
}

int TlenPreShutdown(void *ptr, WPARAM wParam, LPARAM lParam)
{
    TlenProtocol *proto = (TlenProtocol *)ptr;
	return 0;
}


static void initMenuItems(TlenProtocol *proto) 
{
	char text[_MAX_PATH];
	CLISTMENUITEM mi, clmi;
	memset(&mi, 0, sizeof(CLISTMENUITEM));
	mi.cbSize = sizeof(CLISTMENUITEM);
	memset(&clmi, 0, sizeof(CLISTMENUITEM));
	clmi.cbSize = sizeof(CLISTMENUITEM);
	clmi.flags = CMIM_FLAGS | CMIF_GRAYED;

	mi.pszContactOwner = proto->iface.m_szModuleName;
	mi.popupPosition = 500090000;

	mi.pszService = text;
	mi.ptszName = proto->iface.m_tszUserName;
	mi.position = -1999901009;
	mi.pszPopupName = (char *)-1;
	mi.flags = CMIF_ROOTPOPUP | CMIF_TCHAR;
	mi.hIcon = tlenIcons[TLEN_IDI_TLEN];
	proto->hMenuRoot = (HANDLE)CallService( MS_CLIST_ADDMAINMENUITEM,  (WPARAM)0, (LPARAM)&mi);
    
    mi.flags = CMIF_CHILDPOPUP;

	sprintf(text, "%s/MainMenuChats", proto->iface.m_szModuleName);
	CreateServiceFunction_Ex(text, proto, TlenMUCMenuHandleChats);
	mi.pszName = Translate("Tlen Chats");
	mi.position = 2000050001;
	mi.hIcon = tlenIcons[TLEN_IDI_CHATS];
	mi.pszService = text;
    mi.pszPopupName = (char *)proto->hMenuRoot;
	proto->hMenuChats = (HANDLE) CallService(MS_CLIST_ADDMAINMENUITEM, 0, (LPARAM) &mi);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuChats, (LPARAM) &clmi);

	// "Multi-User Conference"
	sprintf(text, "%s/MainMenuMUC", proto->iface.m_szModuleName);
	CreateServiceFunction_Ex(text, proto, TlenMUCMenuHandleMUC);
	mi.pszName = Translate("Multi-User Conference");
	mi.position = 2000050002;
	mi.hIcon = tlenIcons[TLEN_IDI_MUC];
	mi.pszService = text;
	proto->hMenuMUC = (HANDLE) CallService(MS_CLIST_ADDMAINMENUITEM, 0, (LPARAM) &mi);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto->hMenuMUC, (LPARAM) &clmi);

	sprintf(text, "%s/MainMenuInbox", proto->iface.m_szModuleName);
	CreateServiceFunction_Ex(text, proto, TlenMenuHandleInbox);
	mi.pszName = Translate("Tlen Inbox");
	mi.position = 2000050003;
	mi.hIcon = tlenIcons[TLEN_IDI_MAIL];
	mi.pszService = text;
	proto->hMenuInbox = (HANDLE) CallService(MS_CLIST_ADDMAINMENUITEM, 0, (LPARAM) &mi);

	// "Invite to MUC"
	sprintf(text, "%s/ContactMenuMUC", proto->iface.m_szModuleName);
	CreateServiceFunction_Ex(text, proto, TlenMUCContactMenuHandleMUC);
	mi.flags = 0;
	mi.pszName = Translate("Multi-User Conference");
	mi.position = -2000020000;
	mi.hIcon = tlenIcons[TLEN_IDI_MUC];
	mi.pszService = text;
	proto->hMenuContactMUC = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	// "Invite to voice chat"
	sprintf(text, "%s/ContactMenuVoice", proto->iface.m_szModuleName);
	CreateServiceFunction_Ex(text, proto, TlenVoiceContactMenuHandleVoice);
	mi.pszName = Translate("Voice Chat");
	mi.position = -2000018000;
	mi.hIcon = tlenIcons[TLEN_IDI_VOICE];
	mi.pszService = text;
	proto->hMenuContactVoice = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	// "Request authorization"
	sprintf(text, "%s/RequestAuth", proto->iface.m_szModuleName);
	CreateServiceFunction_Ex(text, proto, TlenContactMenuHandleRequestAuth);
	mi.pszName = Translate("Request authorization");
	mi.position = -2000001001;
	mi.hIcon = tlenIcons[TLEN_IDI_REQUEST];
	mi.pszService = text;
	proto->hMenuContactRequestAuth = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	// "Grant authorization"
	sprintf(text, "%s/GrantAuth", proto->iface.m_szModuleName);
	CreateServiceFunction_Ex(text, proto, TlenContactMenuHandleGrantAuth);
	mi.pszName = Translate("Grant authorization");
	mi.position = -2000001000;
	mi.hIcon = tlenIcons[TLEN_IDI_GRANT];
	mi.pszService = text;
	proto->hMenuContactGrantAuth = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	// "Send picture"
	sprintf(text, "%s/SendPicture", proto->iface.m_szModuleName);
	CreateServiceFunction_Ex(text, proto, TlenContactMenuHandleSendPicture);
	mi.pszName = Translate("Send picture");
//	clmi.flags = CMIM_FLAGS | CMIF_NOTOFFLINE;// | CMIF_GRAYED;
	mi.position = -2000001002;
	mi.hIcon = tlenIcons[TLEN_IDI_GRANT];
	mi.pszService = text;
	//CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi); //hMenuContactGrantAuth = (HANDLE)

	mi.position = -2000020000;
	mi.flags = CMIF_NOTONLINE;
	mi.hIcon = LoadSkinnedIcon(SKINICON_EVENT_FILE);
	mi.pszName = Translate("&File");
	mi.pszService = MS_FILE_SENDFILE;
	proto->hMenuContactFile = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);
    
}

static TlenProtocol *tlenProtoInit( const char* pszProtoName, const TCHAR* tszUserName )
{
	DBVARIANT dbv;
    char text[_MAX_PATH];
	TlenProtocol *proto = (TlenProtocol *)mir_alloc(sizeof(TlenProtocol));
    memset(proto, 0, sizeof(TlenProtocol));
    proto->iface.m_tszUserName = mir_tstrdup( tszUserName );
    proto->iface.m_szModuleName = mir_strdup(pszProtoName);
    proto->iface.m_szProtoName = mir_strdup(pszProtoName);
    _strlwr( proto->iface.m_szProtoName );
	proto->iface.m_szProtoName[0] = toupper( proto->iface.m_szProtoName[0] );
    proto->iface.m_iStatus = ID_STATUS_OFFLINE;
    TlenInitServicesVTbl(proto);

	InitializeCriticalSection(&proto->modeMsgMutex);
	InitializeCriticalSection(&proto->csSend);
    
	sprintf(text, "%s/%s", proto->iface.m_szModuleName, "Nudge");
	proto->hTlenNudge = CreateHookableEvent(text);
    
	HookEventObj_Ex(ME_OPT_INITIALISE, proto, TlenOptionsInit);
	HookEventObj_Ex(ME_DB_CONTACT_SETTINGCHANGED, proto, JabberDbSettingChanged);
	HookEventObj_Ex(ME_DB_CONTACT_DELETED, proto, JabberContactDeleted);
	HookEventObj_Ex(ME_CLIST_PREBUILDCONTACTMENU, proto, TlenPrebuildContactMenu);
	HookEventObj_Ex(ME_SKIN2_ICONSCHANGED, proto, TlenIconsChanged);
	HookEventObj_Ex(ME_SYSTEM_PRESHUTDOWN, proto, TlenPreShutdown);

	if (!DBGetContactSetting(NULL, proto->iface.m_szModuleName, "LoginServer", &dbv)) {
		DBFreeVariant(&dbv);
	} else {
		DBWriteContactSettingString(NULL, proto->iface.m_szModuleName, "LoginServer", "tlen.pl");
	}
	if (!DBGetContactSetting(NULL, proto->iface.m_szModuleName, "ManualHost", &dbv)) {
		DBFreeVariant(&dbv);
	} else {
		DBWriteContactSettingString(NULL, proto->iface.m_szModuleName, "ManualHost", "s1.tlen.pl");
	}
    
	TlenLoadOptions(proto);
   
    JabberWsInit(proto);
	JabberSerialInit(proto);
	JabberIqInit(proto);
	JabberListInit(proto);
    initMenuItems(proto);

    return proto;
}

static int tlenProtoUninit( TlenProtocol *proto )
{
    /* TODO: remove menu items */
	TlenVoiceCancelAll(proto);
	TlenFileCancelAll(proto);
	if (proto->hTlenNudge)
		DestroyHookableEvent(proto->hTlenNudge);
    UnhookEvents_Ex(proto);
    //DestroyServices_Ex(proto);
	JabberWsUninit(proto);
	JabberListUninit(proto);
	JabberIqUninit(proto);
	JabberSerialUninit(proto);
	DeleteCriticalSection(&proto->modeMsgMutex);
    DeleteCriticalSection(&proto->csSend);
	mir_free(proto->modeMsgs.szOnline);
	mir_free(proto->modeMsgs.szAway);
	mir_free(proto->modeMsgs.szNa);
	mir_free(proto->modeMsgs.szDnd);
	mir_free(proto->modeMsgs.szFreechat);
	mir_free(proto->modeMsgs.szInvisible);
    mir_free(proto);
 	return 0;
}

int __declspec(dllexport) Load(PLUGINLINK *link)
{
	PROTOCOLDESCRIPTOR pd;
    
	pluginLink = link;
	mir_getMMI( &mmi );
    mir_getMD5I( &md5i );
    mir_getSHA1I( &sha1i );
	mir_getUTFI( &utfi );

	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &hMainThread, THREAD_SET_CONTEXT, FALSE, 0);

    srand((unsigned) time(NULL));

	TlenRegisterIcons();
    
	// Register protocol module
	ZeroMemory(&pd, sizeof(PROTOCOLDESCRIPTOR));
	pd.cbSize = sizeof(PROTOCOLDESCRIPTOR);
	pd.szName = "TLEN";
    pd.fnInit = ( pfnInitProto )tlenProtoInit;
	pd.fnUninit = ( pfnUninitProto )tlenProtoUninit;
	pd.type = PROTOTYPE_PROTOCOL;
	CallService(MS_PROTO_REGISTERMODULE, 0, (LPARAM) &pd);
  
	return 0;
}

int __declspec(dllexport) Unload(void)
{
    TlenReleaseIcons();
	return 0;
}

