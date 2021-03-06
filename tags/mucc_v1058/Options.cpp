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
#include "Options.h"
#include "ChatWindow.h"
#include "Utils.h"
#include "resource.h"
#include <commctrl.h>

static BOOL CALLBACK MUCCOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK MUCCLogOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK MUCCPopupsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);


COLORREF Options::colorListBg, Options::colorInputBg, Options::colorLogBg;
HBRUSH   Options::brushListBg=NULL, Options::brushInputBg=NULL, Options::brushLogBg=NULL;
int		 Options::logLimit, Options::chatWindowOptions;

void Options::setListBgColor(COLORREF c) {
	colorListBg = c;
	if (brushListBg != NULL) {
		DeleteObject(brushListBg);
	}
	brushListBg = CreateSolidBrush(colorListBg);
}

void Options::setInputBgColor(COLORREF c) {
	colorInputBg = c;
	if (brushInputBg!= NULL) {
		DeleteObject(brushInputBg);
	}
	brushInputBg = CreateSolidBrush(colorInputBg);
}

void Options::setLogBgColor(COLORREF c) {
	colorLogBg = c;
	if (brushLogBg!= NULL) {
		DeleteObject(brushLogBg);
	}
	brushLogBg = CreateSolidBrush(colorLogBg);
}

COLORREF Options::getListBgColor() {
	return colorListBg;
}

COLORREF Options::getInputBgColor() {
	return colorInputBg;
}

COLORREF Options::getLogBgColor() {
	return colorLogBg;
}

HBRUSH Options::getListBgBrush() {
	return brushListBg;
}

HBRUSH Options::getInputBgBrush() {
	return brushInputBg;
}

HBRUSH Options::getLogBgBrush() {
	return brushLogBg;
}

void Options::setLogLimit(int l) {
	logLimit = l;
}

int Options::getLogLimit() {
	return logLimit;
}

void Options::setChatWindowOptions(int o) {
	chatWindowOptions = o;
}

int Options::getChatWindowOptions() {
	return chatWindowOptions;
}

void Options::setExpandFlags(int o) {
	DBWriteContactSettingDword(NULL, muccModuleName, "ExpandFlags", o);
}

int Options::getExpandFlags() {
	return DBGetContactSettingDword(NULL, muccModuleName, "ExpandFlags", 0xFFFFFFFF);
}

void Options::loadSettings() {
	FontList::loadSettings();
	setLogBgColor((COLORREF)DBGetContactSettingDword(NULL, muccModuleName, "BackgroundLog", 0xFFFFFF));
	setListBgColor((COLORREF)DBGetContactSettingDword(NULL, muccModuleName, "BackgroundList", 0xFFFFFF));
	setInputBgColor((COLORREF)DBGetContactSettingDword(NULL, muccModuleName, "BackgroundTyping", 0xFFFFFF));
	setLogLimit(DBGetContactSettingDword(NULL, muccModuleName, "LogSizeLimit", 100));
	setChatWindowOptions(DBGetContactSettingDword(NULL, muccModuleName, "ChatWindowOptions", ChatWindow::getDefaultOptions()));
}

void Options::saveSettings() {
	FontList::saveSettings();
	DBWriteContactSettingDword(NULL, muccModuleName, "BackgroundLog", colorLogBg);
	DBWriteContactSettingDword(NULL, muccModuleName, "BackgroundList", colorListBg);
	DBWriteContactSettingDword(NULL, muccModuleName, "BackgroundTyping", colorInputBg);
	DBWriteContactSettingDword(NULL, muccModuleName, "LogSizeLimit", logLimit);
	DBWriteContactSettingDword(NULL, muccModuleName, "ChatWindowOptions", chatWindowOptions);
}

void Options::init()
{
	FontList::init();
	loadSettings();
}

int MUCCOptInit(WPARAM wParam, LPARAM lParam)
{
	OPTIONSDIALOGPAGE odp;

	ZeroMemory(&odp, sizeof(odp));
	odp.cbSize = sizeof(odp);
	odp.position = 0;
	odp.hInstance = hInst;
	odp.pszGroup = Translate("Events");
	odp.pszTemplate = MAKEINTRESOURCE(IDD_OPTIONS);
	odp.pszTitle = muccModuleName;
	odp.flags = ODPF_BOLDGROUPS;
	odp.pfnDlgProc = MUCCOptDlgProc;
	odp.nIDBottomSimpleControl = 0;
	CallService(MS_OPT_ADDPAGE, wParam, (LPARAM) &odp);

	/*
	odp.pszTemplate = MAKEINTRESOURCE(IDD_OPTIONS_LOG);
	_snprintf(str, sizeof(str), "%s %s", muccModuleName, Translate("Log"));
	str[sizeof(str)-1] = '\0';
	odp.pszTitle = str;
	odp.pfnDlgProc = MUCCLLogOptDlgProc;
	odp.flags = ODPF_BOLDGROUPS|ODPF_EXPERTONLY;
	odp.nIDBottomSimpleControl = 0;
	CallService(MS_OPT_ADDPAGE, wParam, (LPARAM) &odp);
	if (ServiceExists(MS_POPUP_ADDPOPUP)) {
		ZeroMemory(&odp,sizeof(odp));
		odp.cbSize = sizeof(odp);
		odp.position = 100000000;
		odp.hInstance = hInst;
		odp.pszGroup = Translate("PopUps");
		odp.pszTemplate = MAKEINTRESOURCE(IDD_OPTIONS_POPUPS);
		odp.pszTitle = jabberModuleName;
		odp.flags=ODPF_BOLDGROUPS;
		odp.pfnDlgProc = TlenPopupsDlgProc;
		odp.nIDBottomSimpleControl = 0;
		CallService(MS_OPT_ADDPAGE,wParam,(LPARAM)&odp);
	}

*/
	return 0;
}


static int getOptions(HWND hTreeControl, HTREEITEM hItem, int flags) {
	TVITEM tvi = {0};
	tvi.mask = TVIF_HANDLE | TVIF_STATE | TVIF_PARAM;
	tvi.stateMask = TVIS_STATEIMAGEMASK;
	tvi.hItem = hItem;
	TreeView_GetItem(hTreeControl, &tvi);
	if ((tvi.state>>12) == 2) {
		flags |= tvi.lParam;
	} else {
		flags &= ~tvi.lParam;
	}
	tvi.hItem = TreeView_GetChild(hTreeControl, hItem);
	if (tvi.hItem != NULL) {
		flags = getOptions(hTreeControl, tvi.hItem, flags);
	}
	tvi.hItem=TreeView_GetNextSibling(hTreeControl, hItem);
	if (tvi.hItem != NULL) {
		flags = getOptions(hTreeControl, tvi.hItem, flags);
	}
	return flags;

}

static int getExpandFlags(HWND hTreeControl) {
	TVITEM tvi = {0};
	int flags =0, mask = 1;
	tvi.mask = TVIF_HANDLE | TVIF_STATE;
	tvi.stateMask = TVIS_EXPANDED;
	tvi.hItem = TreeView_GetRoot(hTreeControl);
	while (tvi.hItem != NULL) {
		TreeView_GetItem(hTreeControl, &tvi);
		if (tvi.state & TVIS_EXPANDED) {
			flags |= mask;
		}
		mask <<= 1;
		tvi.hItem=TreeView_GetNextSibling(hTreeControl, tvi.hItem);
	}
	return flags;
}


static int addOption(HWND hTreeControl, HTREEITEM hParent, const char *text, int flag) {
	TVINSERTSTRUCT tvis;
	tvis.hInsertAfter = TVI_LAST;
	tvis.hParent = hParent;
	tvis.item.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
	tvis.item.pszText = Translate(text);
	tvis.item.lParam = flag;
	tvis.item.stateMask = TVIS_STATEIMAGEMASK;
	tvis.item.state = Options::getChatWindowOptions() & flag ? INDEXTOSTATEIMAGEMASK(2) : INDEXTOSTATEIMAGEMASK(1);
	TreeView_InsertItem(hTreeControl, &tvis);
	return (Options::getChatWindowOptions() & flag) != 0;

}

static BOOL CALLBACK MUCCOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int i;
    char *pszText;
	switch (msg) {
	case WM_INITDIALOG:
		{
			TVINSERTSTRUCT tvis;
			TVITEM tvi;
			HWND hTreeControl;
			HTREEITEM	   hItem;
			int bCheckParent, expandFlags;

			TranslateDialogDefault(hwndDlg);
			Options::loadSettings();
			for (i=0;i<Options::getFontNum();i++) {
				SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_ADDSTRING, 0, i + 1);
			}
		
			SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_SETSEL, TRUE, 0);
			SendDlgItemMessage(hwndDlg, IDC_COLORTXT, CPM_SETCOLOUR, 0, Options::getFontSettings(0)->getColor());
			SendDlgItemMessage(hwndDlg, IDC_COLORBKG, CPM_SETCOLOUR, 0, Options::getLogBgColor());
			SendDlgItemMessage(hwndDlg, IDC_COLORLIST, CPM_SETCOLOUR, 0, Options::getListBgColor());
			SendDlgItemMessage(hwndDlg, IDC_COLORINPUT, CPM_SETCOLOUR, 0, Options::getInputBgColor());
			SendDlgItemMessage(hwndDlg,	IDC_LOGLIMITSPIN, UDM_SETRANGE, 0, MAKELONG(1000,20));
			SendDlgItemMessage(hwndDlg,	IDC_LOGLIMITSPIN, UDM_SETPOS, 0, MAKELONG(Options::getLogLimit(), 0));

			SetWindowLong(GetDlgItem(hwndDlg,IDC_TREELIST),GWL_STYLE,GetWindowLong(GetDlgItem(hwndDlg,IDC_TREELIST),GWL_STYLE)|TVS_NOHSCROLL|TVS_CHECKBOXES);


			hTreeControl = GetDlgItem(hwndDlg, IDC_TREELIST);
			expandFlags = Options::getExpandFlags();
			tvis.hInsertAfter = TVI_LAST;
			tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
			tvis.hParent = NULL;
			tvis.item.lParam = 0;

			tvi.mask = TVIF_HANDLE | TVIF_STATE;
			tvi.stateMask = TVIS_STATEIMAGEMASK;

			tvis.item.pszText = Translate("Default Log Appearence");
			hItem = TreeView_InsertItem(hTreeControl, &tvis);
			bCheckParent = 1;
			bCheckParent &= addOption(hTreeControl, hItem, "Show nicknames", ChatWindow::FLAG_SHOW_NICKNAMES);
			bCheckParent &= addOption(hTreeControl, hItem, "Show messages in new line", ChatWindow::FLAG_MSGINNEWLINE);
			tvi.state = (bCheckParent ? INDEXTOSTATEIMAGEMASK(2) : INDEXTOSTATEIMAGEMASK(1));
			tvi.hItem = hItem;
			TreeView_SetItem(hTreeControl, &tvi);
			if (expandFlags & 1) {
				TreeView_Expand(hTreeControl, hItem, TVE_EXPAND);
			}
		
			tvis.item.pszText = Translate("Default Timestamp Settings");
			hItem = TreeView_InsertItem(hTreeControl, &tvis);
			bCheckParent = 1;
			bCheckParent &= addOption(hTreeControl, hItem, "Show date", ChatWindow::FLAG_SHOW_DATE);
			bCheckParent &= addOption(hTreeControl, hItem, "Show timestamp", ChatWindow::FLAG_SHOW_TIMESTAMP);
			bCheckParent &= addOption(hTreeControl, hItem, "Show seconds", ChatWindow::FLAG_SHOW_SECONDS);
			bCheckParent &= addOption(hTreeControl, hItem, "Use long date format", ChatWindow::FLAG_LONG_DATE);
			tvi.state = (bCheckParent ? INDEXTOSTATEIMAGEMASK(2) : INDEXTOSTATEIMAGEMASK(1));
			tvi.hItem = hItem;
			TreeView_SetItem(hTreeControl, &tvi);
			if (expandFlags & 2) {
				TreeView_Expand(hTreeControl, hItem, TVE_EXPAND);
			}

			tvis.item.pszText = Translate("Default Text Formatting Filter");
			hItem = TreeView_InsertItem(hTreeControl, &tvis);
			bCheckParent = 1;
			bCheckParent &= addOption(hTreeControl, hItem, "Font name", ChatWindow::FLAG_FORMAT_FONT);
			bCheckParent &= addOption(hTreeControl, hItem, "Font size", ChatWindow::FLAG_FORMAT_SIZE);
			bCheckParent &= addOption(hTreeControl, hItem, "Font style", ChatWindow::FLAG_FORMAT_STYLE);
			bCheckParent &= addOption(hTreeControl, hItem, "Font color", ChatWindow::FLAG_FORMAT_COLOR);
			tvi.state = (bCheckParent ? INDEXTOSTATEIMAGEMASK(2) : INDEXTOSTATEIMAGEMASK(1));
			tvi.hItem = hItem;
			TreeView_SetItem(hTreeControl, &tvi);
			if (expandFlags & 4) {
				TreeView_Expand(hTreeControl, hItem, TVE_EXPAND);
			}

			tvis.item.pszText = Translate("Default Event Filter");
			hItem = TreeView_InsertItem(hTreeControl, &tvis);
			bCheckParent = 1;
			bCheckParent &= addOption(hTreeControl, hItem, "Messages", ChatWindow::FLAG_LOG_MESSAGES);
			bCheckParent &= addOption(hTreeControl, hItem, "User has joined", ChatWindow::FLAG_LOG_JOINED);
			bCheckParent &= addOption(hTreeControl, hItem, "User has left", ChatWindow::FLAG_LOG_LEFT);
			bCheckParent &= addOption(hTreeControl, hItem, "Topic changes", ChatWindow::FLAG_LOG_TOPIC);
			tvi.state = (bCheckParent ? INDEXTOSTATEIMAGEMASK(2) : INDEXTOSTATEIMAGEMASK(1));
			tvi.hItem = hItem;
			TreeView_SetItem(hTreeControl, &tvi);
			if (expandFlags & 8) {
				TreeView_Expand(hTreeControl, hItem, TVE_EXPAND);
			}
			
			CheckDlgButton(hwndDlg, IDC_SENDONENTER, Options::getChatWindowOptions() & ChatWindow::FLAG_OPT_SENDONENTER);
		}
	 case WM_MEASUREITEM:
		if (wParam == IDC_FONTLIST) {
			MEASUREITEMSTRUCT *lpMis = (MEASUREITEMSTRUCT *) lParam;
            HFONT hFont, hoFont;
			HDC hDC;
            SIZE fontSize;
            int iItem = lpMis->itemData - 1;
			Font *font = Options::getFontSettings(iItem);
			if (font != NULL) {
				hFont = CreateFont(font->getSize(), 0, 0, 0,
								   font->getStyle() & Font::BOLD ? FW_BOLD : FW_NORMAL,
								   font->getStyle() & Font::ITALIC ? 1 : 0, 0, 0,
								   font->getCharSet(),
								   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, font->getFace());
				hDC = GetDC(GetDlgItem(hwndDlg, lpMis->CtlID));
				hoFont = (HFONT) SelectObject(hDC, hFont);
				pszText = Translate(Options::getFontSettings(iItem)->getName());//fontOptionsList[iItem].szDescr);
				GetTextExtentPoint32(hDC, pszText, lstrlen(pszText), &fontSize);
				SelectObject(hDC, hoFont);
				ReleaseDC(GetDlgItem(hwndDlg, lpMis->CtlID), hDC);
				DeleteObject(hFont);
				lpMis->itemWidth = fontSize.cx;
				lpMis->itemHeight = fontSize.cy;
				return TRUE;
			}
			break;
		}
	 case WM_DRAWITEM:
		if (wParam == IDC_FONTLIST) {
			DRAWITEMSTRUCT *lpDis = (DRAWITEMSTRUCT *) lParam;
			HFONT hFont;
			HBRUSH hBrush;
			int iItem = lpDis->itemData - 1;
			Font *font = Options::getFontSettings(iItem);
			if (font != NULL) {
				COLORREF color = (COLORREF) SendDlgItemMessage(hwndDlg, IDC_COLORBKG, CPM_GETCOLOUR, 0, 0);
				if (iItem == Options::FONT_USERLIST || iItem == Options::FONT_USERLISTGROUP) {
					color = (COLORREF) SendDlgItemMessage(hwndDlg, IDC_COLORLIST, CPM_GETCOLOUR, 0, 0);
				} else if (iItem == Options::FONT_TYPING) {
					color = (COLORREF) SendDlgItemMessage(hwndDlg, IDC_COLORINPUT, CPM_GETCOLOUR, 0, 0);
				}
				hBrush = CreateSolidBrush(color);
				hFont = CreateFont(font->getSize(), 0, 0, 0,
								   font->getStyle() & Font::BOLD ? FW_BOLD : FW_NORMAL,
								   font->getStyle() & Font::ITALIC ? 1 : 0, 
								   font->getStyle() & Font::UNDERLINE ? 1 : 0, 0,
								   font->getCharSet(),
								   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
								   font->getFace());
				hFont = (HFONT) SelectObject(lpDis->hDC, hFont);
				SetBkMode(lpDis->hDC, TRANSPARENT);
				FillRect(lpDis->hDC, &lpDis->rcItem, hBrush);
				if (lpDis->itemState & ODS_SELECTED) {
					FrameRect(lpDis->hDC, &lpDis->rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
				}
				SetTextColor(lpDis->hDC, font->getColor());
				pszText = Translate(font->getName());
				TextOut(lpDis->hDC, lpDis->rcItem.left, lpDis->rcItem.top, pszText, lstrlen(pszText));
				hFont = (HFONT) SelectObject(lpDis->hDC, hFont);
				DeleteObject(hBrush);
				DeleteObject(hFont);
			}
			return TRUE;
        }
	case WM_COMMAND:
			switch (LOWORD(wParam)) {
			case IDC_COLORTXT:
				{
					int totalCount = SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_GETCOUNT, 0, 0);
					int *selItems= new int[totalCount];
					int selCount =	SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_GETSELITEMS, totalCount, (LPARAM) selItems);
					COLORREF color = (COLORREF) SendDlgItemMessage(hwndDlg, IDC_COLORTXT, CPM_GETCOLOUR, 0, 0);
					for (int i=0; i<selCount; i++) {
						int iItem = SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_GETITEMDATA, selItems[i], 0) - 1;
						Font *font = Options::getFontSettings(iItem);
						if (font != NULL) {
							font->setColor(color);
						}
					}
					delete selItems;
					InvalidateRect(GetDlgItem(hwndDlg, IDC_FONTLIST), NULL, FALSE);
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
					break;
				}
				break;
			case IDC_LOGLIMIT:
				if (HIWORD(wParam) == EN_CHANGE  && (HWND)lParam==GetFocus()) {
					i = GetDlgItemInt(hwndDlg, IDC_LOGLIMIT, NULL, FALSE);
				//	if (i>1000) {
				//		i = 1000;
					//	SetDlgItemInt(hwndDlg, IDC_LOGLIMIT, i, FALSE);
				//	}
					if (i!=Options::getLogLimit()) {
						SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
					}
				}
				break;
			case IDC_COLORBKG:
			case IDC_COLORLIST:
			case IDC_COLORINPUT:
				InvalidateRect(GetDlgItem(hwndDlg, IDC_FONTLIST), NULL, FALSE);
			case IDC_SENDONENTER:
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;
			case IDC_FONTLIST:
				if (HIWORD(wParam) == LBN_SELCHANGE) {
					if (SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_GETSELCOUNT, 0, 0) > 1) {
						SendDlgItemMessage(hwndDlg, IDC_COLORTXT, CPM_SETCOLOUR, 0, GetSysColor(COLOR_3DFACE));
						SendDlgItemMessage(hwndDlg, IDC_COLORTXT, CPM_SETDEFAULTCOLOUR, 0, GetSysColor(COLOR_WINDOWTEXT));
					}
					else {
						int iItem = SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_GETITEMDATA,
									  SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_GETCURSEL, 0, 0), 0) - 1;
						Font *font = Options::getFontSettings(iItem);
						if (font != NULL) {
							SendDlgItemMessage(hwndDlg, IDC_COLORTXT, CPM_SETCOLOUR, 0, font->getColor());
							SendDlgItemMessage(hwndDlg, IDC_COLORTXT, CPM_SETDEFAULTCOLOUR, 0, font->getColor());
						}
					}
				}
				if (HIWORD(wParam) != LBN_DBLCLK) {
					return TRUE;
				}
			case IDC_CHOOSEFONT:
                {
                    CHOOSEFONT cf;
                    LOGFONT lf;
                    int iItem = SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_GETITEMDATA, SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_GETCURSEL, 0, 0), 0) - 1;
                    ZeroMemory(&cf, sizeof(cf));
                    ZeroMemory(&lf, sizeof(lf));
					Font *font = Options::getFontSettings(iItem);
					if (font != NULL) {
						lf.lfHeight = font->getSize();
						lf.lfWeight = font->getStyle() & Font::BOLD ? FW_BOLD : FW_NORMAL;
						lf.lfItalic = font->getStyle() & Font::ITALIC ? 1 : 0;
						lf.lfCharSet = DEFAULT_CHARSET;
						lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
						lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
						lf.lfQuality = DEFAULT_QUALITY;
						lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
						lstrcpy(lf.lfFaceName, font->getFace());
						cf.lStructSize = sizeof(cf);
						cf.hwndOwner = hwndDlg;
						cf.lpLogFont = &lf;
						cf.Flags = CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
						if (ChooseFont(&cf)) {
							int totalCount = SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_GETCOUNT, 0, 0);
							int *selItems= new int[totalCount];
						    int selCount =  SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_GETSELITEMS, totalCount, (LPARAM) selItems);
		                    for (int i=0; i<selCount; i++) {
				                int iItem = SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_GETITEMDATA, selItems[i], 0) - 1;
								Font *font2 = Options::getFontSettings(iItem);
								if (font2 != NULL) {
									font2->setStyle((lf.lfWeight >= FW_BOLD ? Font::BOLD : 0) | (lf.lfItalic ? Font::ITALIC : 0));
									font2->setCharSet(lf.lfCharSet);
									font2->setSize(lf.lfHeight);
									font2->setFace(lf.lfFaceName);
									MEASUREITEMSTRUCT mis;
									ZeroMemory(&mis, sizeof(mis));
									mis.CtlID = IDC_FONTLIST;
									mis.itemData = iItem + 1;
									SendMessage(hwndDlg, WM_MEASUREITEM, (WPARAM)IDC_FONTLIST, (LPARAM) & mis);
									SendDlgItemMessage(hwndDlg, IDC_FONTLIST, LB_SETITEMHEIGHT, selItems[i], mis.itemHeight);
								}
							}
							delete selItems;
							InvalidateRect(GetDlgItem(hwndDlg, IDC_FONTLIST), NULL, TRUE);
							SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
							break;
						}
					}
                    return TRUE;
                }
				break;
			}
			break;
	case WM_DESTROY:
		Options::setExpandFlags(getExpandFlags(GetDlgItem(hwndDlg, IDC_TREELIST)));
		break;
	case WM_NOTIFY:
		{
		switch (((LPNMHDR) lParam)->code) {
			case PSN_APPLY:
				COLORREF color;
				color = (COLORREF) SendDlgItemMessage(hwndDlg, IDC_COLORLIST, CPM_GETCOLOUR, 0, 0);
				Options::setListBgColor(color);
				color = (COLORREF) SendDlgItemMessage(hwndDlg, IDC_COLORINPUT, CPM_GETCOLOUR, 0, 0);
				Options::setInputBgColor(color);
				color = (COLORREF) SendDlgItemMessage(hwndDlg, IDC_COLORBKG, CPM_GETCOLOUR, 0, 0);
				Options::setLogBgColor(color);
				Options::setLogLimit(SendDlgItemMessage(hwndDlg, IDC_LOGLIMITSPIN, UDM_GETPOS, 0, 0));
				Options::setChatWindowOptions(getOptions(GetDlgItem(hwndDlg, IDC_TREELIST), TreeView_GetRoot(GetDlgItem(hwndDlg, IDC_TREELIST)), 0));
				Options::setChatWindowOptions(Options::getChatWindowOptions() | (IsDlgButtonChecked(hwndDlg, IDC_SENDONENTER) ? ChatWindow::FLAG_OPT_SENDONENTER : 0));
				Options::saveSettings();
				ChatWindow::refreshSettings(1);
				break;
			case NM_CLICK:
				/* Handle checking/unchecking options here - update children and parent appropriately */
				if(((LPNMHDR)lParam)->idFrom == IDC_TREELIST)  {
					TVHITTESTINFO hti;
					hti.pt.x = (short)LOWORD(GetMessagePos());
					hti.pt.y = (short)HIWORD(GetMessagePos());
					ScreenToClient(((LPNMHDR)lParam)->hwndFrom, &hti.pt);
					if(TreeView_HitTest(((LPNMHDR)lParam)->hwndFrom, &hti)) {
						if(hti.flags&TVHT_ONITEMSTATEICON) {
							TVITEM tvi = {0};
							tvi.mask = TVIF_HANDLE | TVIF_STATE;
							tvi.stateMask = TVIS_STATEIMAGEMASK;
							tvi.hItem = hti.hItem;
							TreeView_GetItem(((LPNMHDR)lParam)->hwndFrom, &tvi);
							tvi.state ^= INDEXTOSTATEIMAGEMASK(3);
							tvi.hItem = TreeView_GetChild(((LPNMHDR)lParam)->hwndFrom, hti.hItem);
							if (tvi.hItem!=NULL) {
								while(tvi.hItem != NULL) {
									TreeView_SetItem(((LPNMHDR)lParam)->hwndFrom, &tvi);
									tvi.hItem=TreeView_GetNextSibling(((LPNMHDR)lParam)->hwndFrom, tvi.hItem);
								}
							} else {
								HTREEITEM hParent = TreeView_GetParent(((LPNMHDR)lParam)->hwndFrom, hti.hItem);
								if (hParent != NULL) {
									if ((tvi.state>>12) == 2) {
										tvi.hItem = TreeView_GetNextSibling(((LPNMHDR)lParam)->hwndFrom, hti.hItem);
										while(tvi.hItem != NULL) {
											TreeView_GetItem(((LPNMHDR)lParam)->hwndFrom, &tvi);
											if ((tvi.state>>12) != 2) {
												break;
											}
											tvi.hItem = TreeView_GetNextSibling(((LPNMHDR)lParam)->hwndFrom, tvi.hItem);
										}
										if ((tvi.state>>12) == 2) {
											tvi.hItem = TreeView_GetPrevSibling(((LPNMHDR)lParam)->hwndFrom, hti.hItem);
											while(tvi.hItem != NULL) {
												TreeView_GetItem(((LPNMHDR)lParam)->hwndFrom, &tvi);
												if ((tvi.state>>12) != 2) {
													break;
												}
												tvi.hItem = TreeView_GetPrevSibling(((LPNMHDR)lParam)->hwndFrom, tvi.hItem);
											}
										}
									}
									tvi.hItem = hParent;
									TreeView_SetItem(((LPNMHDR)lParam)->hwndFrom, &tvi);
								}
							}
							SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
						}
					}
				}
				break;
			}
		}
		break;
	}	
	return FALSE;
}

/*	char text[256];
static BOOL CALLBACK MUCCLogOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	BOOL bChecked;

	switch (msg) {
	case WM_INITDIALOG:
		{
			DBVARIANT dbv;
			TranslateDialogDefault(hwndDlg);
			// File transfer options
			bChecked = FALSE;
			if (DBGetContactSettingByte(NULL, jabberProtoName, "UseFileProxy", FALSE) == TRUE) {
			    bChecked = TRUE;
				CheckDlgButton(hwndDlg, IDC_FILE_USE_PROXY, TRUE);
			}
			EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_TYPE_LABEL), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_TYPE), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_HOST_LABEL), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_HOST), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_PORT_LABEL), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_PORT), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_USE_AUTH), bChecked);
			if (DBGetContactSettingByte(NULL, jabberProtoName, "FileProxyAuth", FALSE) == TRUE) {
				CheckDlgButton(hwndDlg, IDC_FILE_PROXY_USE_AUTH, TRUE);
			} else {
			    bChecked = FALSE;
			}    
			EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_USER_LABEL), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_USER), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_PASSWORD_LABEL), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_PASSWORD), bChecked);
			
			SendDlgItemMessage(hwndDlg, IDC_FILE_PROXY_TYPE, CB_ADDSTRING, 0, (LPARAM)Translate("Forwarding"));
            SendDlgItemMessage(hwndDlg, IDC_FILE_PROXY_TYPE, CB_ADDSTRING, 0, (LPARAM)Translate("SOCKS4"));    
            SendDlgItemMessage(hwndDlg, IDC_FILE_PROXY_TYPE, CB_ADDSTRING, 0, (LPARAM)Translate("SOCKS5"));    
			SendDlgItemMessage(hwndDlg, IDC_FILE_PROXY_TYPE, CB_SETCURSEL, DBGetContactSettingWord(NULL, jabberProtoName, "FileProxyType", 0), 0);
			if (!DBGetContactSetting(NULL, jabberProtoName, "FileProxyHost", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_FILE_PROXY_HOST, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			SetDlgItemInt(hwndDlg, IDC_FILE_PROXY_PORT, DBGetContactSettingWord(NULL, jabberProtoName, "FileProxyPort", 0), FALSE);
			if (!DBGetContactSetting(NULL, jabberProtoName, "FileProxyUsername", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_FILE_PROXY_USER, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			if (!DBGetContactSetting(NULL, jabberProtoName, "FileProxyPassword", &dbv)) {
				CallService(MS_DB_CRYPT_DECODESTRING, strlen(dbv.pszVal)+1, (LPARAM) dbv.pszVal);
				SetDlgItemText(hwndDlg, IDC_FILE_PROXY_PASSWORD, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (LOWORD(wParam)) {
			case IDC_FILE_PROXY_HOST:
			case IDC_FILE_PROXY_PORT:
				if ((HWND)lParam==GetFocus() && HIWORD(wParam)==EN_CHANGE)
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;
			case IDC_FILE_USE_PROXY:
				bChecked = IsDlgButtonChecked(hwndDlg, IDC_FILE_USE_PROXY);
				EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_TYPE_LABEL), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_TYPE), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_HOST_LABEL), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_HOST), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_PORT_LABEL), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_PORT), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_USE_AUTH), bChecked);
			case IDC_FILE_PROXY_USE_AUTH:
				bChecked = IsDlgButtonChecked(hwndDlg, IDC_FILE_PROXY_USE_AUTH) & IsDlgButtonChecked(hwndDlg, IDC_FILE_USE_PROXY);
				EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_USER_LABEL), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_USER), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_PASSWORD_LABEL), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_FILE_PROXY_PASSWORD), bChecked);
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;
			default:
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;
			}
		}
		break;
	case WM_NOTIFY:
		{
			switch (((LPNMHDR) lParam)->code) {
			case PSN_APPLY:
				// File transfer options
				DBWriteContactSettingByte(NULL, jabberProtoName, "UseFileProxy", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_FILE_USE_PROXY));
				DBWriteContactSettingWord(NULL, jabberProtoName, "FileProxyType", (WORD) SendDlgItemMessage(hwndDlg, IDC_FILE_PROXY_TYPE, CB_GETCURSEL, 0, 0));
				GetDlgItemText(hwndDlg, IDC_FILE_PROXY_HOST, text, sizeof(text));
				DBWriteContactSettingString(NULL, jabberProtoName, "FileProxyHost", text);
				DBWriteContactSettingWord(NULL, jabberProtoName, "FileProxyPort", (WORD) GetDlgItemInt(hwndDlg, IDC_FILE_PROXY_PORT, NULL, FALSE));
				DBWriteContactSettingByte(NULL, jabberProtoName, "FileProxyAuth", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_FILE_PROXY_USE_AUTH));
				GetDlgItemText(hwndDlg, IDC_FILE_PROXY_USER, text, sizeof(text));
				DBWriteContactSettingString(NULL, jabberProtoName, "FileProxyUsername", text);
				GetDlgItemText(hwndDlg, IDC_FILE_PROXY_PASSWORD, text, sizeof(text));
				CallService(MS_DB_CRYPT_ENCODESTRING, sizeof(text), (LPARAM) text);
				DBWriteContactSettingString(NULL, jabberProtoName, "FileProxyPassword", text);
				return TRUE;
			}
		}
		break;
	case WM_DESTROY:
		break;
	}
	return FALSE;
}
*/

/*
#define POPUP_DEFAULT_COLORBKG 0xDCBDA5
#define POPUP_DEFAULT_COLORTXT 0x000000
static void MailPopupPreview(DWORD colorBack, DWORD colorText, char *title, char *emailInfo, int delay)
{
	POPUPDATAEX ppd;
	char * lpzContactName;
	char * lpzText;

	lpzContactName = title; 
	lpzText = emailInfo;
	ZeroMemory(&ppd, sizeof(ppd));
	ppd.lchContact = NULL;
	ppd.lchIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAIL));
	lstrcpy(ppd.lpzContactName, lpzContactName);
	lstrcpy(ppd.lpzText, lpzText);
	ppd.colorBack = colorBack;
	ppd.colorText = colorText;
	ppd.PluginWindowProc = NULL;
	ppd.PluginData=NULL;
	if ( ServiceExists( MS_POPUP_ADDPOPUPEX )) {
		ppd.iSeconds = delay;
		CallService(MS_POPUP_ADDPOPUPEX, (WPARAM)&ppd, 0);

	}
	else if ( ServiceExists( MS_POPUP_ADDPOPUP )) {
		CallService(MS_POPUP_ADDPOPUP, (WPARAM)&ppd, 0);
	}
}
*/
static BOOL CALLBACK MUCCPopupsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	/*
	switch (msg) {
		case WM_INITDIALOG:
			{
				BYTE delayMode;
				CheckDlgButton(hwndDlg, IDC_ENABLEPOPUP, DBGetContactSettingByte(NULL, jabberProtoName, "MailPopupEnabled", TRUE));
				SendDlgItemMessage(hwndDlg, IDC_COLORBKG, CPM_SETCOLOUR, 0, DBGetContactSettingDword(NULL, jabberProtoName, "MailPopupBack", POPUP_DEFAULT_COLORBKG));
				SendDlgItemMessage(hwndDlg, IDC_COLORTXT, CPM_SETCOLOUR, 0, DBGetContactSettingDword(NULL, jabberProtoName, "MailPopupText", POPUP_DEFAULT_COLORTXT));
				SetDlgItemInt(hwndDlg, IDC_DELAY, DBGetContactSettingDword(NULL, jabberProtoName, "MailPopupDelay", 4), FALSE);
				delayMode = DBGetContactSettingByte(NULL, jabberProtoName, "MailPopupDelayMode", 0);
				if (delayMode==1) {
					CheckDlgButton(hwndDlg, IDC_DELAY_CUSTOM, TRUE);
				} else if (delayMode==2) {
					CheckDlgButton(hwndDlg, IDC_DELAY_PERMANENT, TRUE);
				} else {
					CheckDlgButton(hwndDlg, IDC_DELAY_POPUP, TRUE);
				}
				return TRUE;
			}
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
			case IDC_COLORTXT:
			case IDC_COLORBKG:
			case IDC_ENABLEPOPUP:
			case IDC_DELAY:
			case IDC_DELAY_POPUP:
			case IDC_DELAY_CUSTOM:
			case IDC_DELAY_PERMANENT:
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;
			case IDC_PREVIEW:
				{
					int delay;
					if (IsDlgButtonChecked(hwndDlg, IDC_DELAY_POPUP)) {
						delay=0;
					} else if (IsDlgButtonChecked(hwndDlg, IDC_DELAY_PERMANENT)) {
						delay=-1;
					} else {
						delay=GetDlgItemInt(hwndDlg, IDC_DELAY, NULL, FALSE);
					}
					MailPopupPreview((DWORD) SendDlgItemMessage(hwndDlg,IDC_COLORBKG,CPM_GETCOLOUR,0,0), 
									 (DWORD) SendDlgItemMessage(hwndDlg,IDC_COLORTXT,CPM_GETCOLOUR,0,0), 
									 "New mail", 
									 "From: test@test.test\nSubject: test", 
									 delay);
				}

			}
			break;


		case WM_NOTIFY:
			switch (((LPNMHDR) lParam)->code) {
				case PSN_APPLY: 
				{
					BYTE delayMode;
					DBWriteContactSettingByte(NULL, jabberProtoName, "MailPopupEnabled", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_ENABLEPOPUP));
					DBWriteContactSettingDword(NULL, jabberProtoName, "MailPopupBack", (DWORD) SendDlgItemMessage(hwndDlg,IDC_COLORBKG,CPM_GETCOLOUR,0,0));
					DBWriteContactSettingDword(NULL, jabberProtoName, "MailPopupText", (DWORD) SendDlgItemMessage(hwndDlg,IDC_COLORTXT,CPM_GETCOLOUR,0,0));
					DBWriteContactSettingDword(NULL, jabberProtoName, "MailPopupDelay", (DWORD) GetDlgItemInt(hwndDlg,IDC_DELAY, NULL, FALSE));
					delayMode=0;
					if (IsDlgButtonChecked(hwndDlg, IDC_DELAY_CUSTOM)) {
						delayMode=1;
					} else if (IsDlgButtonChecked(hwndDlg, IDC_DELAY_PERMANENT)) {
						delayMode=2;

					}
					DBWriteContactSettingByte(NULL, jabberProtoName, "MailPopupDelayMode", delayMode);
					return TRUE;
				}
			}
			break;

	}*/
	return FALSE;
}

