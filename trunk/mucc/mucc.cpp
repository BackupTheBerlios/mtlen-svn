/*

MUCC Group Chat GUI Plugin for Miranda IM
Copyright (C) 2004  Piotr Piastucki

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

#include "mucc.h"
#include "mucc_services.h"
#include "HelperDialog.h"
#include "Options.h"

char *muccModuleName;
HINSTANCE hInst;
PLUGINLINK *pluginLink;
HANDLE hHookEvent;
HIMAGELIST hImageList;
HICON muccIcon[MUCC_ICON_TOTAL];
static int ModulesLoaded(WPARAM wParam, LPARAM lParam);
static int PreShutdown(WPARAM wParam, LPARAM lParam);

PLUGININFO pluginInfo = {
	sizeof(PLUGININFO),
	"MUCC Plugin",
	PLUGIN_MAKE_VERSION(1,0,5,10),
	"Group chats GUI plugin for Miranda IM (1.0.5.10 "__DATE__")",
	"Piotr Piastucki",
	"the_leech@users.berlios.de",
	"(c) 2004-2005 Piotr Piastucki",
	"http://mtlen.berlios.de",
	0,
	0
};

extern "C" BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpvReserved)
{
	hInst = hModule;
	return TRUE;
}

extern "C" __declspec(dllexport) PLUGININFO *MirandaPluginInfo(DWORD mirandaVersion)
{
	if (mirandaVersion < PLUGIN_MAKE_VERSION(0,3,1,0)) {
		MessageBox(NULL, "The MUCC plugin cannot be loaded. It requires Miranda IM 0.3.1 or later.", "Tlen Protocol Plugin", MB_OK|MB_ICONWARNING|MB_SETFOREGROUND|MB_TOPMOST);
		return NULL;
	}
	return &pluginInfo;
}


extern "C" int __declspec(dllexport) Load(PLUGINLINK *link)
{
	char text[_MAX_PATH];
	char *p, *q;
	int i;
	static int iconList[] = {
		IDI_CHAT,
		IDI_GLOBALOWNER,
		IDI_OWNER,
		IDI_ADMIN,
		IDI_REGISTERED,
		IDI_R_MODERATED,
		IDI_R_MEMBERS,
		IDI_R_ANONYMOUS,
		IDI_PREV,
		IDI_NEXT,
		IDI_SEARCH,
		IDI_BOLD,
		IDI_ITALIC,
		IDI_UNDERLINE,
		IDI_OPTIONS,
		IDI_INVITE,
		IDI_ADMINISTRATION,
		IDI_SMILEY
	};

	GetModuleFileName(hInst, text, sizeof(text));
	p = strrchr(text, '\\');
	p++;
	q = strrchr(p, '.');
	*q = '\0';
	muccModuleName = _strdup(p);
	_strupr(muccModuleName);

	pluginLink = link;
	/*
	**	HookEvent(ME_OPT_INITIALISE, TlenOptInit);
	*/
	HookEvent(ME_OPT_INITIALISE, MUCCOptInit);
	HookEvent(ME_SYSTEM_MODULESLOADED, ModulesLoaded);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, PreShutdown);

	CreateServiceFunction(MS_MUCC_QUERY_RESULT, MUCCQueryResult);
	CreateServiceFunction(MS_MUCC_NEW_WINDOW, MUCCNewWindow);
	CreateServiceFunction(MS_MUCC_EVENT, MUCCEvent);
	hHookEvent = CreateHookableEvent(ME_MUCC_EVENT);

	for (i=0; i<MUCC_ICON_TOTAL; i++) {
		muccIcon[i] = (HICON) LoadImage(hInst, MAKEINTRESOURCE(iconList[i]), IMAGE_ICON, 0, 0, 0);
	}
	hImageList = ImageList_Create(GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),ILC_COLOR32|ILC_MASK,0,3);
	ImageList_AddIcon(hImageList,(HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_BLANK),IMAGE_ICON,0,0,0));
	ImageList_AddIcon(hImageList,(HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_BLANK),IMAGE_ICON,0,0,0));
	return 0;
}

static int ModulesLoaded(WPARAM wParam, LPARAM lParam)
{
	Options::init();
	HelperDialog::init();
	ManagerWindow::init();
	ChatWindow::init();
	ChatContainer::init();

	/*
	**	HookEvent(ME_USERINFO_INITIALISE, TlenUserInfoInit);
	**	SkinAddNewSound("TlenMailNotify", Translate("Tlen: Incoming mail"), "");
	*/
	return 0;
}

static int PreShutdown(WPARAM wParam, LPARAM lParam)
{
	ChatContainer::release();
	ChatWindow::release();
	ManagerWindow::release();
	HelperDialog::release();
	return 0;
}

extern "C" int __declspec(dllexport) Unload(void)
{
	return 0;
}

