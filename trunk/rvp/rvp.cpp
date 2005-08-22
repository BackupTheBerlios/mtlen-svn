/*

RVP Protocol Plugin for Miranda IM
Copyright (C) 2005 Piotr Piastucki

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

#include "rvp.h"
#include "resource.h"
#include <ctype.h>

HINSTANCE hInst;
PLUGINLINK *pluginLink;

PLUGININFO pluginInfo = {
	sizeof(PLUGININFO),
	"MS Exchange (RVP) Protocol",
	PLUGIN_MAKE_VERSION(0,0,1,5),
	"RVP protocol plugin for Miranda IM (0.0.1.5 "__DATE__")",
	"Piotr Piastucki",
	"the_leech@users.berlios.de",
	"(c) 2005 Piotr Piastucki",
	"http://mtlen.berlios.de",
	0,
	0
};

HANDLE hMainThread;
char *rvpProtoName;	// "RVP"
char *rvpModuleName;	// "RVP"

// Main jabber server connection thread global variables
BOOL jabberConnected;
int jabberStatus;
int jabberDesiredStatus;

HANDLE hEventSettingChanged, hEventContactDeleted;

HICON tlenIcons[RVP_ICON_TOTAL];

int RVPOptInit(WPARAM wParam, LPARAM lParam);
int RVPMsgUserTyping(WPARAM wParam, LPARAM lParam);
int RVPSvcInit(void);

extern "C" BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpvReserved)
{
#ifdef _DEBUG
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	hInst = hModule;
	return TRUE;
}

extern "C" __declspec(dllexport) PLUGININFO *MirandaPluginInfo(DWORD mirandaVersion)
{
	if (mirandaVersion < PLUGIN_MAKE_VERSION(0,3,1,0)) {
		MessageBox(NULL, "The RVP protocol plugin cannot be loaded. It requires Miranda IM 0.3.1 or later.", "RVP Protocol Plugin", MB_OK|MB_ICONWARNING|MB_SETFOREGROUND|MB_TOPMOST);
		return NULL;
	}
	return &pluginInfo;
}

extern "C" int __declspec(dllexport) Unload(void)
{
#ifdef _DEBUG
	OutputDebugString("Unloading...");
#endif
	UnhookEvent(hEventSettingChanged);
//	HTTPConnection::release();
	free(rvpModuleName);
	free(rvpProtoName);
#ifdef _DEBUG
	//OutputDebugString("Finishing Unload, returning to Miranda");
	//OutputDebugString("========== Memory leak from TLEN");
	//_CrtDumpMemoryLeaks();
	//OutputDebugString("========== End memory leak from TLEN");
#endif
	return 0;
}

static int PreShutdown(WPARAM wParam, LPARAM lParam)
{
	if (jabberConnected) {
	/* TODO move to signout thread*/
		rvplogin.signOut();
	}

	return 0;
}

static int ModulesLoaded(WPARAM wParam, LPARAM lParam)
{
	HTTPConnection::init(rvpProtoName, rvpModuleName);
	return 0;
}

static void RVPIconInit()
{
	int i;
	static int iconList[] = {
		IDI_RVP,
	};
	for (i=0; i<RVP_ICON_TOTAL; i++)
		tlenIcons[i] = (HICON)LoadImage(hInst, MAKEINTRESOURCE(iconList[i]), IMAGE_ICON, 0, 0, 0);
}

extern "C" int __declspec(dllexport) Load(PLUGINLINK *link)
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
	rvpProtoName = _strdup(p);
	_strupr(rvpProtoName);

	rvpModuleName = _strdup(rvpProtoName);
	//_strlwr(rvpModuleName);
	//rvpModuleName[0] = toupper(rvpModuleName[0]);

	pluginLink = link;
	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &hMainThread, THREAD_SET_CONTEXT, FALSE, 0);

	HookEvent(ME_OPT_INITIALISE, RVPOptInit);
	HookEvent(ME_SYSTEM_MODULESLOADED, ModulesLoaded);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, PreShutdown);

	// Register protocol module
	ZeroMemory(&pd, sizeof(PROTOCOLDESCRIPTOR));
	pd.cbSize = sizeof(PROTOCOLDESCRIPTOR);
	pd.szName = rvpProtoName;
	pd.type = PROTOTYPE_PROTOCOL;
	CallService(MS_PROTO_REGISTERMODULE, 0, (LPARAM) &pd);

	memset(&mi, 0, sizeof(CLISTMENUITEM));
	mi.cbSize = sizeof(CLISTMENUITEM);
	memset(&clmi, 0, sizeof(CLISTMENUITEM));
	clmi.cbSize = sizeof(CLISTMENUITEM);
	clmi.flags = CMIM_FLAGS | CMIF_GRAYED;

	mi.pszPopupName = rvpModuleName;
	mi.popupPosition = 500090000;

	// Set all contacts to offline
	hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
	while (hContact != NULL) {
		szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
		if(szProto!=NULL && !strcmp(szProto, rvpProtoName)) {
			if (DBGetContactSettingWord(hContact, rvpProtoName, "Status", ID_STATUS_OFFLINE) != ID_STATUS_OFFLINE) {
				DBWriteContactSettingWord(hContact, rvpProtoName, "Status", ID_STATUS_OFFLINE);
			}
		}
		hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
	}

	jabberConnected = FALSE;
	jabberStatus = ID_STATUS_OFFLINE;

	RVPIconInit();
	srand((unsigned) time(NULL));
	RVPSvcInit();

	return 0;
}

