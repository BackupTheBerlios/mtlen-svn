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

#include "ChatContainer.h"
#include "Utils.h"
#include "Options.h"

#define DM_CREATECHILD		(WM_USER+10)
#define DM_ADDCHILD			(WM_USER+11)
#define DM_ACTIVATECHILD	(WM_USER+12)
#define DM_CHANGECHILDDATA	(WM_USER+13)
#define DM_REMOVECHILD		(WM_USER+14)

ChatContainer *	ChatContainer::list = NULL;
bool ChatContainer::released = false;
CRITICAL_SECTION ChatContainer::mutex;

//BOOL CALLBACK ContainerDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static void __cdecl StartThread(void *vContainer);

void ChatContainer::release() {
	released = true;
	for (ChatContainer *ptr2, *ptr = list; ptr!=NULL; ptr=ptr2) {
//		ptr2 = ptr->getNext();
		ptr2 = ptr->next;
		SendMessage(ptr->getHWND(), WM_CLOSE, 0, 0);
	}
	DeleteCriticalSection(&mutex);
}

void ChatContainer::init() {
	released = false;
	InitializeCriticalSection(&mutex);
}

ChatContainer * ChatContainer::getWindow() {
	ChatContainer *ptr;
	EnterCriticalSection(&mutex);
	if (list == NULL) {
		ptr = new ChatContainer();
	} else {
		ptr = list;
	}
	list = ptr;
	LeaveCriticalSection(&mutex);
	return ptr;
}

ChatContainer::ChatContainer() {
	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hWnd = NULL;
	prev = next =NULL;
	active = NULL;
	childCount = 0;
	Utils::forkThread((void (__cdecl *)(void *))StartThread, 0, (void *) this);
	WaitForSingleObject(hEvent, INFINITE);
}

ChatContainer::~ChatContainer() {
	if (hEvent!=NULL) {
		CloseHandle(hEvent);
	}
	list = NULL;
}

void ChatContainer::setHWND(HWND hWnd) {
	this->hWnd = hWnd;
}

HWND ChatContainer::getHWND() {
	return hWnd;
}

HANDLE ChatContainer::getEvent() {
	return hEvent;
}

void ChatContainer::show(bool bShow) {
	ShowWindow(hWnd, bShow ? SW_SHOW : SW_HIDE);
}

ChatWindow * ChatContainer::getActive() {
	return active;
}

void ChatContainer::activateChild(ChatWindow *window) {
	RECT rcChild;
	getChildWindowRect(&rcChild);
	if (window!=NULL) {
		SetWindowPos(window->getHWND(), HWND_TOP, rcChild.left, rcChild.top, rcChild.right-rcChild.left, rcChild.bottom - rcChild.top, SWP_NOSIZE);
	}
	if(window != active) {
		ChatWindow *prev = active;
		active = window;
		SendMessage(hWnd, WM_SIZE, 0, 0);
		ShowWindow(active->getHWND(), SW_SHOW);
//		SendMessage(active->getHWND(), DM_UPDATETITLE, 0, 0);
		if (prev!=NULL) {
			ShowWindow(prev->getHWND(), SW_HIDE);
		}
		SetWindowText(hWnd, window->getRoomName());
	}
	SendMessage(active->getHWND(), WM_ACTIVATE, WA_ACTIVE, 0);
	SetFocus(active->getHWND());
}


void ChatContainer::addChild(ChatWindow *child) {
	TCITEM tci;
	int tabId;
	HWND hwndTabs = GetDlgItem(hWnd, IDC_TABS);
	childCount++;
	tci.mask = TCIF_TEXT | TCIF_PARAM;
	tci.pszText = (char *)child->getRoomName();
	tci.lParam = (LPARAM) child;
	tabId = TabCtrl_InsertItem(hwndTabs, childCount-1, &tci);
	TabCtrl_SetCurSel(hwndTabs, tabId);
	activateChild(child);
	SendMessage(hWnd, WM_SIZE, 0, 0);
}

void ChatContainer::changeChildData(ChatWindow *child) {
	int tabId;
	HWND hwndTabs = GetDlgItem(hWnd, IDC_TABS);
	tabId = getChildTab(child);
	if (tabId >=0) {
		TCITEM tci;
		tci.mask = TCIF_TEXT;
		tci.pszText = (char *)child->getRoomName();
		TabCtrl_SetItem(hwndTabs, childCount-1, &tci);
	}
	if (child == active) {
		SetWindowText(hWnd, child->getRoomName());
	}
}


void ChatContainer::removeChild(ChatWindow *child) {
	HWND hwndTabs = GetDlgItem(hWnd, IDC_TABS);
	int iSel = getChildTab(child);
	if (iSel >= 0) {
		TabCtrl_DeleteItem(hwndTabs, iSel);
	}
	childCount--;
	if (childCount > 0) {
		TCITEM tci;
		if (iSel == childCount) iSel--;
		TabCtrl_SetCurSel(hwndTabs, iSel);
		tci.mask = TCIF_PARAM;
		if (TabCtrl_GetItem(hwndTabs, iSel, &tci)) {
			child = (ChatWindow *)tci.lParam;
			activateChild(child);
		}
	} else {
		SendMessage(hWnd, WM_CLOSE, 0, 0);
	}
}


void ChatContainer::getChildWindowRect(RECT *rcChild)
{
	RECT rc, rcTabs; //rcStatus, 
	HWND hwndTabs = GetDlgItem(hWnd, IDC_TABS);
	int l = TabCtrl_GetItemCount(hwndTabs);
	GetClientRect(hWnd, &rc);
	GetClientRect(hwndTabs, &rcTabs);
	TabCtrl_AdjustRect(hwndTabs, FALSE, &rcTabs);
//	GetWindowRect(dat->hwndStatus, &rcStatus);
	rcChild->left = 0;
	rcChild->right = rc.right;
	if (l > 1) {
		rcChild->top = rcTabs.top - 1;
	} else {
		rcChild->top = 0;
	}
	rcChild->bottom = rc.bottom - rc.top;// - (rcStatus.bottom - rcStatus.top);
}

ChatWindow * ChatContainer::getChildFromTab(int tabId) {
	TCITEM tci;
	tci.mask = TCIF_PARAM;
	TabCtrl_GetItem(GetDlgItem(hWnd, IDC_TABS), tabId, &tci);
	return (ChatWindow *)tci.lParam;
}

int ChatContainer::getChildTab(ChatWindow *child)  {
	TCITEM tci;
	int l, i;
	HWND hwndTabs = GetDlgItem(hWnd, IDC_TABS);
	l = TabCtrl_GetItemCount(hwndTabs);
	for (i = 0; i < l; i++) {
		tci.mask = TCIF_PARAM;
		TabCtrl_GetItem(hwndTabs, i, &tci);
		if (child == (ChatWindow *) tci.lParam) {
			return i;
		}
	}
	return -1;

}

HWND ChatContainer::remoteCreateChild(DLGPROC proc, ChatWindow *ptr) {
	return (HWND) SendMessage(hWnd, DM_CREATECHILD, (WPARAM)proc, (LPARAM) ptr);
}

void ChatContainer::remoteAddChild(ChatWindow *ptr) {
	SendMessage(hWnd, DM_ADDCHILD, (WPARAM)0, (LPARAM) ptr);
}

void ChatContainer::remoteChangeChildData(ChatWindow *ptr) {
	SendMessage(hWnd, DM_CHANGECHILDDATA, (WPARAM)0, (LPARAM) ptr);
}

void ChatContainer::remoteRemoveChild(ChatWindow *ptr) {
	SendMessage(hWnd, DM_REMOVECHILD, (WPARAM)0, (LPARAM) ptr);
}

BOOL CALLBACK ContainerDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ChatContainer *container;
	container = (ChatContainer *) GetWindowLong(hwndDlg, GWL_USERDATA);
	if (container==NULL && msg!=WM_INITDIALOG) return FALSE;
	switch (msg) {
		case WM_INITDIALOG:
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM) muccIcon[MUCC_IDI_CHAT]);
			container = (ChatContainer *) lParam;
			container->setHWND(hwndDlg);
			SetWindowLong(hwndDlg, GWL_USERDATA, (LONG) container);
			ShowWindow(hwndDlg, SW_SHOW);
			SetEvent(container->getEvent());
			return TRUE;
		case WM_GETMINMAXINFO:
			MINMAXINFO *mmi;
			mmi = (MINMAXINFO *) lParam;
			mmi->ptMinTrackSize.x = 380;
			mmi->ptMinTrackSize.y = 160;
			return FALSE;
		case WM_SIZE:
			if (IsIconic(hwndDlg) || wParam == SIZE_MINIMIZED) break;
			{
				RECT rc, rcChild;
				GetClientRect(hwndDlg, &rc);
				HWND hwndTabs = GetDlgItem(hwndDlg, IDC_TABS);
				MoveWindow(hwndTabs, 0, 0, (rc.right - rc.left), (rc.bottom - rc.top) - 0,	FALSE);
				RedrawWindow(hwndTabs, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ERASE);
				if (container->getActive()!=NULL) {
					container->getChildWindowRect(&rcChild);
					MoveWindow(container->getActive()->getHWND(), rcChild.left, rcChild.top, rcChild.right-rcChild.left, rcChild.bottom - rcChild.top, TRUE);
				}
			}
			return TRUE;
		case DM_CREATECHILD:
			SetWindowLong(hwndDlg, DWL_MSGRESULT, (LONG)CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_GROUPCHAT_LOG), hwndDlg, (DLGPROC) wParam, (LPARAM) lParam));
			return TRUE;
		case DM_ADDCHILD:
			container->addChild((ChatWindow *) lParam);
			return TRUE;
		case DM_REMOVECHILD:
			container->removeChild((ChatWindow *) lParam);
			return TRUE;
		case DM_CHANGECHILDDATA:
			container->removeChild((ChatWindow *) lParam);
			return TRUE;
		case WM_NOTIFY:
			{
				NMHDR* pNMHDR = (NMHDR*) lParam;
				switch (pNMHDR->code) {
				case TCN_SELCHANGE:
					{
						TCITEM tci = {0};
						HWND hwndTabs = GetDlgItem(hwndDlg, IDC_TABS);
						int iSel = TabCtrl_GetCurSel(hwndTabs);
						tci.mask = TCIF_PARAM;
						if (TabCtrl_GetItem(hwndTabs, iSel, &tci)) {
							ChatWindow * chatWindow = (ChatWindow *) tci.lParam;
							container->activateChild(chatWindow);
						}
					}
					break;
				case NM_CLICK:
					{
						FILETIME ft;
						TCHITTESTINFO thinfo;
						int tabId;
						HWND hwndTabs = GetDlgItem(hwndDlg, IDC_TABS);
						GetSystemTimeAsFileTime(&ft);
						GetCursorPos(&thinfo.pt);
						ScreenToClient(hwndTabs, &thinfo.pt);
						tabId = TabCtrl_HitTest(hwndTabs, &thinfo);
						if (tabId != -1 && tabId == container->lastClickTab &&
							(ft.dwLowDateTime - container->lastClickTime) < (GetDoubleClickTime() * 10000)) {
							SendMessage(container->getChildFromTab(tabId)->getHWND(), WM_CLOSE, 0, 0);
							container->lastClickTab = -1;
						} else {
							container->lastClickTab = tabId;
						}
						container->lastClickTime = ft.dwLowDateTime;
					}
					break;
				}

			}
			break;
		case WM_ACTIVATE:
			if (LOWORD(wParam) != WA_ACTIVE)
				break;
		case WM_MOUSEACTIVATE:
			if (container->getActive()!=NULL) {
				SendMessage(container->getActive()->getHWND(), WM_ACTIVATE, WA_ACTIVE, 0);
			}
			break;
		case WM_CLOSE:
			EndDialog(hwndDlg, 0);
//			MessageBox(NULL, "close window", "A", MB_OK);
//			DestroyWindow(hwndDlg);
			return TRUE;
		case WM_DESTROY:
			SetWindowLong(hwndDlg, GWL_USERDATA, 0);
			delete container;
			return FALSE;

	}
	return FALSE;
}


static void __cdecl StartThread(void *vContainer) {
	OleInitialize(NULL);
	DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_GROUPCHAT_CONTAINER), NULL, ContainerDlgProc, (LPARAM) vContainer);
//	MessageBox(NULL, "ChatContainer dies.", "MW", MB_OK);
	OleUninitialize();

}
