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

#include "AdminWindow.h"
#include "Utils.h"


static BOOL CALLBACK UserKickDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{	
	int i;
	const char *banOptions[]={"No ban", "1 minute", "5 minutes", "15 minutes", "30 minutes",
	"1 hour", "6 hours", "1 day", "3 days", "1 week", "2 weeks", "4 weeks"};
	const int banTime[] = {0, 1, 5, 15, 30, 60, 360, 1440, 4320, 10080, 20160, 40320};
	const char *roleOptions[]={"No role", "Member", "Admin", "Moderator"};
	const int roleMask[] = {0, MUCC_EF_USER_MEMBER, MUCC_EF_USER_ADMIN, MUCC_EF_USER_MODERATOR};
	AdminWindow *adminWindow;
	adminWindow = (AdminWindow *) GetWindowLong(hwndDlg, GWL_USERDATA);
	switch (msg) {
	case WM_INITDIALOG:
		adminWindow = (AdminWindow *) lParam;
		TranslateDialogDefault(hwndDlg);
		SetWindowLong(hwndDlg, GWL_USERDATA, (LONG) adminWindow);
		adminWindow->setKickTabHWND(hwndDlg);
		for (i=0;i<12;i++) {
			SendDlgItemMessage(hwndDlg, IDC_KICK_OPTIONS, CB_ADDSTRING, 0, (LPARAM) banOptions[i]);
		}
		SendDlgItemMessage(hwndDlg, IDC_KICK_OPTIONS, CB_SETCURSEL, 0, 0);
		for (i=0;i<3;i++) {
			SendDlgItemMessage(hwndDlg, IDC_ROLE_OPTIONS, CB_ADDSTRING, 0, (LPARAM) roleOptions[i]);
		}
		SendDlgItemMessage(hwndDlg, IDC_ROLE_OPTIONS, CB_SETCURSEL, 0, 0);
		SetDlgItemText(hwndDlg, IDC_NICK, adminWindow->getNick());
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			{
				char nick[256], reason[256];
				GetDlgItemText(hwndDlg, IDC_NICK, nick, sizeof(nick));
				GetDlgItemText(hwndDlg, IDC_REASON, reason, sizeof(reason));
				if (strlen(nick)>0) {
					int iSel = SendDlgItemMessage(hwndDlg, IDC_KICK_OPTIONS, CB_GETCURSEL, 0, 0);
					if (iSel>=0 && iSel<12) {
						adminWindow->getParent()->kickAndBan(nick, banTime[iSel] * 60, reason);
					}
				}
			}
//			EndDialog(hwndDlg, 1);
//			return TRUE;
			break;
		case IDC_SET_ROLE:
			{
				char nick[256];
				GetDlgItemText(hwndDlg, IDC_NICK, nick, sizeof(nick));
				if (strlen(nick)>0) {
					int iSel = SendDlgItemMessage(hwndDlg, IDC_ROLE_OPTIONS, CB_GETCURSEL, 0, 0);
					if (iSel>=0 && iSel<3) {
						adminWindow->getParent()->setRights(nick, roleMask[iSel]);
					}
				}
			}
			break;
//			EndDialog(hwndDlg, 1);
//			return TRUE;
		case IDCANCEL:
//			EndDialog(hwndDlg, 0);
			return TRUE;
		}
		break;
	case WM_CLOSE:
//		EndDialog(hwndDlg, 0);
		break;
	}
	return FALSE;
}

static BOOL CALLBACK UserBrowserDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{	int i;
	const char *browseOptions[]={"Banned", "Owners", "Administrators", "Members", "Moderators"};
	const int browserFlags[]={MUCC_EF_USER_BANNED, MUCC_EF_USER_OWNER, MUCC_EF_USER_ADMIN, MUCC_EF_USER_MEMBER, MUCC_EF_USER_MODERATOR};
	LVCOLUMN lvCol;
	HWND lv;
	AdminWindow *adminWindow;
	adminWindow = (AdminWindow *) GetWindowLong(hwndDlg, GWL_USERDATA);
	switch (msg) {
	case WM_INITDIALOG:
		adminWindow = (AdminWindow *) lParam;
		TranslateDialogDefault(hwndDlg);
		SetWindowLong(hwndDlg, GWL_USERDATA, (LONG) adminWindow);
		adminWindow->setBrowserTabHWND(hwndDlg);
		lv = GetDlgItem(hwndDlg, IDC_LIST);
		ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT);
		lvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
		lvCol.pszText = Translate("Login");
		lvCol.cx = 80;
		lvCol.iSubItem = 0;
		ListView_InsertColumn(lv, 0, &lvCol);
		lvCol.pszText = Translate("Nick name");
		lvCol.cx = 80;
		lvCol.iSubItem = 1;
		ListView_InsertColumn(lv, 1, &lvCol);
		lvCol.pszText = Translate("When");
		lvCol.cx = 95;
		lvCol.iSubItem = 2;
		ListView_InsertColumn(lv, 2, &lvCol);
		lvCol.pszText = Translate("Admin");
		lvCol.cx = 80;
		lvCol.iSubItem = 2;
		ListView_InsertColumn(lv, 3, &lvCol);
		lvCol.pszText = Translate("Reason");
		lvCol.cx = 70;
		lvCol.iSubItem = 2;
		ListView_InsertColumn(lv, 5, &lvCol);
		lvCol.pszText = Translate("Remaining");
		lvCol.cx = 80;
		lvCol.iSubItem = 2;
		ListView_InsertColumn(lv, 4, &lvCol);
		for (i=0;i<(adminWindow->getParent()->getRoomFlags() & MUCC_EF_ROOM_MEMBERS_ONLY ? 4 : 3);i++) {
			SendDlgItemMessage(hwndDlg, IDC_OPTIONS, CB_ADDSTRING, 0, (LPARAM) browseOptions[i]);
		}
		SendDlgItemMessage(hwndDlg, IDC_OPTIONS, CB_SETCURSEL, 0, 0);
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SHOW:
			int iSel;
			iSel = SendDlgItemMessage(hwndDlg, IDC_OPTIONS, CB_GETCURSEL, 0, 0);
			if (iSel< 5) {
				adminWindow->queryUsers(browserFlags[iSel]);
			}
			break;
//			return TRUE;
		case IDCANCEL:
//			EndDialog(hwndDlg, 0);
			return TRUE;
		}
		break;
	case WM_CLOSE:
//		EndDialog(hwndDlg, 0);
		break;
	case WM_MEASUREITEM:
		if (wParam == IDC_LIST) {
			MEASUREITEMSTRUCT *lpMis = (MEASUREITEMSTRUCT *) lParam;
			lpMis->itemHeight = 16;//GetSystemMetrics(SM_CYSMICON);
			return TRUE;
		}
		break;
	case WM_DRAWITEM:
		if (wParam == IDC_LIST) {
			RECT rc;
			int x, w;
			char text[256];
			DRAWITEMSTRUCT *lpDis = (DRAWITEMSTRUCT *) lParam;
			switch (lpDis->itemAction) {
				default:
				case ODA_SELECT:
				case ODA_DRAWENTIRE:
					if (lpDis->itemState & ODS_SELECTED) {
						HBRUSH hBrush = CreateSolidBrush(RGB(0xC2, 0xC8, 0xDA));//0xDAC8C2);
						FillRect(lpDis->hDC, &(lpDis->rcItem), hBrush);//(HBRUSH) (COLOR_HIGHLIGHT+1));
						DeleteObject(hBrush);
						SetTextColor(lpDis->hDC, 0);
						SetBkMode(lpDis->hDC, TRANSPARENT);
					}
					else {
						FillRect(lpDis->hDC, &(lpDis->rcItem), (HBRUSH) (COLOR_WINDOW+1));
						SetTextColor(lpDis->hDC, RGB(0, 0, 0));//GetSysColor(COLOR_WINDOWTEXT));
						SetBkMode(lpDis->hDC, TRANSPARENT);
					}
					x = lpDis->rcItem.left + 1;
					for (int col=0;col<6;col++) {
						w = ListView_GetColumnWidth(GetDlgItem(hwndDlg, IDC_LIST), col);
						rc.left = x;
						rc.top = lpDis->rcItem.top;
						rc.bottom = lpDis->rcItem.bottom;
						rc.right = x+w-2;
						ListView_GetItemText(GetDlgItem(hwndDlg, IDC_LIST), lpDis->itemID, col, text, sizeof(text));
						if (strlen(text)==0) {
							strcpy(text, "-");
						}
						DrawText(lpDis->hDC, text, strlen(text), &rc, DT_LEFT|DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER);
						x+=w;
					}
				break;
			}
		}
		break;
	case WM_NOTIFY:
		LPNMHDR pNmhdr;
		pNmhdr = (LPNMHDR)lParam;
		if (pNmhdr->idFrom = IDC_LIST && adminWindow->getBrowserMode() == MUCC_EF_USER_BANNED) {
			switch (pNmhdr->code) {
			case NM_RCLICK:
				{
					LVHITTESTINFO hti;
					int hitItem;
					hti.pt.x=(short)LOWORD(GetMessagePos());
					hti.pt.y=(short)HIWORD(GetMessagePos());
					ScreenToClient(pNmhdr->hwndFrom, &hti.pt);
					if ((hitItem = ListView_HitTest(pNmhdr->hwndFrom,&hti)) !=-1) {
						LVITEM lvi = {0};
						HMENU hMenu;
						int iSelection;
						char nick[256];
						lvi.mask = TVIF_TEXT;
						lvi.iItem = hitItem;
						lvi.iSubItem = 0;
						lvi.pszText = nick;
						lvi.cchTextMax = sizeof(nick);
						ListView_GetItem(pNmhdr->hwndFrom, &lvi);
						hMenu=GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(IDR_CHATOPTIONS)), 3);
						CallService(MS_LANGPACK_TRANSLATEMENU,(WPARAM)hMenu,0);
						iSelection = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_TOPALIGN | TPM_LEFTALIGN, (short)LOWORD(GetMessagePos()), (short)HIWORD(GetMessagePos()), 0, hwndDlg, NULL);
						DestroyMenu(hMenu);
						if (iSelection == ID_USERMENU_UNBAN) {
							adminWindow->getParent()->unban(nick);
						} 
					}

				}
				break;
			}
		}				

	}
	return FALSE;
}

static BOOL CALLBACK AdminDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{	
	HWND hwnd;
	TCITEM tci;
	HWND tc;
	AdminWindow *adminWindow;
	adminWindow = (AdminWindow *) GetWindowLong(hwndDlg, GWL_USERDATA);
	switch (msg) {
	case WM_INITDIALOG:
		adminWindow = (AdminWindow *) lParam;
		TranslateDialogDefault(hwndDlg);
		SetWindowLong(hwndDlg, GWL_USERDATA, (LONG) adminWindow);
		SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM) muccIcon[MUCC_IDI_ADMINISTRATION]);

		hwnd = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_USER_KICK), hwndDlg, UserKickDlgProc, (LPARAM) adminWindow);
		SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		ShowWindow(hwnd, SW_SHOW);
		
		hwnd = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_USER_BROWSER), hwndDlg, UserBrowserDlgProc, (LPARAM) adminWindow);
		SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		adminWindow->setCurrentTab(0);
		adminWindow->setHWND(hwndDlg);

		tc = GetDlgItem(hwndDlg, IDC_TABS);
		tci.mask = TCIF_TEXT;
		tci.pszText = Translate("Administration");
		TabCtrl_InsertItem(tc, 0, &tci);
		tci.pszText = Translate("Browser");
		TabCtrl_InsertItem(tc, 1, &tci);

		return FALSE;
	case WM_NOTIFY:
		switch (wParam) {
		case IDC_TABS:
			switch (((LPNMHDR) lParam)->code) {
			case TCN_SELCHANGE:
				switch (adminWindow->getCurrentTab()) {
					case 0:
						ShowWindow(adminWindow->getKickTabHWND(), SW_HIDE);
						break;
					case 1:
						ShowWindow(adminWindow->getBrowserTabHWND(), SW_HIDE);
						break;
				}
				adminWindow->setCurrentTab(TabCtrl_GetCurSel(GetDlgItem(hwndDlg, IDC_TABS)));
				switch (adminWindow->getCurrentTab()) {
				case 0:
					ShowWindow(adminWindow->getKickTabHWND(), SW_SHOW);
					break;
				case 1:
					ShowWindow(adminWindow->getBrowserTabHWND(), SW_SHOW);
					break;
				}
				break;
			}
			break;
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			delete adminWindow;
		//	EndDialog(hwndDlg, 0);
			return TRUE;
		}
		break;
	case WM_CLOSE:
		delete adminWindow;
	//	EndDialog(hwndDlg, 0);
		break;
	}
	return FALSE;
}



AdminWindow::AdminWindow(ChatWindow *parent, const char *nick, int mode) {
	this->parent = parent;
	hWnd = NULL;
	browserMode = 0;
	this->nick = NULL;
	Utils::copyString(&this->nick, nick);
}

AdminWindow::~AdminWindow() {

	if (hWnd != NULL) {
		EndDialog(hWnd, 0);
	}
	if (parent != NULL) {
		parent->setAdminWindow(NULL);
	}
	if (nick != NULL) {
		delete nick;
	}
}

void AdminWindow::start() {
	DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_USER_ADMIN), NULL, AdminDlgProc, (LPARAM) this);
}

void AdminWindow::setHWND(HWND hWnd) {
	this->hWnd = hWnd;
}

void AdminWindow::setCurrentTab(int t) {
	currentTab = t;
}

int AdminWindow::getCurrentTab() {
	return currentTab;
}


void AdminWindow::setKickTabHWND(HWND hWnd) {
	hKickTabWnd = hWnd;
}

void AdminWindow::setBrowserTabHWND(HWND hWnd) {
	hBrowserTabWnd = hWnd;
}

HWND AdminWindow::getKickTabHWND() {
	return hKickTabWnd;
}

HWND AdminWindow::getBrowserTabHWND() {
	return hBrowserTabWnd;
}

void AdminWindow::queryUsers(int queryType) {
	MUCCEVENT muce;
	EnableWindow(GetDlgItem(getBrowserTabHWND(), IDC_SHOW), FALSE);
	muce.cbSize = sizeof(MUCCEVENT);
	muce.iType = MUCC_EVENT_QUERY_USERS;
	muce.pszModule = parent->getModule();
	muce.pszID = parent->getRoomId();
	muce.dwFlags = queryType;
	browserMode = queryType;
	NotifyEventHooks(hHookEvent, 0,(WPARAM)&muce);
}

ChatWindow * AdminWindow::getParent() {
	return parent;
}

void AdminWindow::queryResultUsers(MUCCQUERYRESULT *queryResult) {
	ListView_DeleteAllItems(GetDlgItem(getBrowserTabHWND(), IDC_LIST));
	for (int i=0;i<queryResult->iItemsNum;i++) {
		char timestampStr[100];
		DBTIMETOSTRING dbtts;
		LVITEM lvItem;
		lvItem.mask = LVIF_TEXT;// | LVIF_PARAM;
		lvItem.iSubItem = 0;
		lvItem.iItem = ListView_GetItemCount(GetDlgItem(getBrowserTabHWND(), IDC_LIST));
		lvItem.pszText = (char *) queryResult->pItems[i].pszID;
		if (lvItem.pszText == NULL) lvItem.pszText = "";
//		lvItem.lParam = (LPARAM) room;
		ListView_InsertItem(GetDlgItem(getBrowserTabHWND(), IDC_LIST), &lvItem);
		lvItem.iSubItem = 1;
		lvItem.pszText = (char *) queryResult->pItems[i].pszName;
		if (lvItem.pszText == NULL) lvItem.pszText = "";
		ListView_InsertItem(GetDlgItem(getBrowserTabHWND(), IDC_LIST), &lvItem);
		ListView_SetItemText(GetDlgItem(getBrowserTabHWND(), IDC_LIST), lvItem.iItem, lvItem.iSubItem, lvItem.pszText);
		lvItem.iSubItem = 2;
		dbtts.cbDest = 90;
		dbtts.szDest = timestampStr;
		dbtts.szFormat = (char *)"d t";
		timestampStr[0]='\0';
		if (queryResult->pItems[i].dwFlags) {
			CallService(MS_DB_TIME_TIMESTAMPTOSTRING, (WPARAM)queryResult->pItems[i].dwFlags, (LPARAM) & dbtts);
		}
		lvItem.pszText = timestampStr;
		if (lvItem.pszText == NULL) lvItem.pszText = "";
		ListView_InsertItem(GetDlgItem(getBrowserTabHWND(), IDC_LIST), &lvItem);
		ListView_SetItemText(GetDlgItem(getBrowserTabHWND(), IDC_LIST), lvItem.iItem, lvItem.iSubItem, lvItem.pszText);
		lvItem.iSubItem = 3;
		lvItem.pszText = (char *) queryResult->pItems[i].pszNick;
		if (lvItem.pszText == NULL) lvItem.pszText = "";
		ListView_InsertItem(GetDlgItem(getBrowserTabHWND(), IDC_LIST), &lvItem);
		ListView_SetItemText(GetDlgItem(getBrowserTabHWND(), IDC_LIST), lvItem.iItem, lvItem.iSubItem, lvItem.pszText);
		lvItem.iSubItem = 4;
		timestampStr[0] = '\0';
		if (queryResult->pItems[i].iCount > 0) {
			int days = queryResult->pItems[i].iCount / (60*60*24);
			int hours = (queryResult->pItems[i].iCount % (60*60*24)) / (60*60);
			int minutes = (queryResult->pItems[i].iCount % (60*60)) / 60;
			int seconds = queryResult->pItems[i].iCount % 60;
			if (days != 0) {
				sprintf(timestampStr, "%dd%dh%dm%ds", days, hours, minutes, seconds);
			} else if (hours != 0) {
				sprintf(timestampStr, "%dh%dm%ds", hours, minutes, seconds);
			} else if (minutes != 0) {
				sprintf(timestampStr, "%dm%ds", minutes, seconds);
			} else {
				sprintf(timestampStr, "%ds", seconds);
			}
		}
		lvItem.pszText = timestampStr;
		if (lvItem.pszText == NULL) lvItem.pszText = "";
		ListView_InsertItem(GetDlgItem(getBrowserTabHWND(), IDC_LIST), &lvItem);
		ListView_SetItemText(GetDlgItem(getBrowserTabHWND(), IDC_LIST), lvItem.iItem, lvItem.iSubItem, lvItem.pszText);
		lvItem.iSubItem = 5;
		lvItem.pszText = (char *) queryResult->pItems[i].pszText;
		if (lvItem.pszText == NULL) lvItem.pszText = "";
		ListView_InsertItem(GetDlgItem(getBrowserTabHWND(), IDC_LIST), &lvItem);
		ListView_SetItemText(GetDlgItem(getBrowserTabHWND(), IDC_LIST), lvItem.iItem, lvItem.iSubItem, lvItem.pszText);

/*
		ptr = new HelperContact(queryResult->pItems[i].pszID, queryResult->pItems[i].pszName);
		if (lastptr !=NULL) {
			lastptr->setNext(ptr);
		} else {
			contactList=ptr;
		}
		lastptr=ptr;

	*/
	}
	EnableWindow(GetDlgItem(getBrowserTabHWND(), IDC_SHOW), TRUE);
}

const char *AdminWindow::getNick() {
	return nick;
}

int AdminWindow::getBrowserMode() {
	return browserMode;
}
