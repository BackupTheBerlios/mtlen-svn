/*

Jabber Protocol Plugin for Miranda IM
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
#include "jabber_list.h"
#include "tlen_voice.h"
#include <commctrl.h>
#include "resource.h"

extern BOOL jabberSendKeepAlive;
extern UINT jabberCodePage;

static BOOL CALLBACK TlenOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK TlenAdvOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK TlenPopupsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

int TlenOptInit(WPARAM wParam, LPARAM lParam)
{
	OPTIONSDIALOGPAGE odp;
	char str[50];

	ZeroMemory(&odp, sizeof(odp));
	odp.cbSize = sizeof(odp);
	odp.position = 0;
	odp.hInstance = hInst;
	odp.pszGroup = Translate("Network");
	odp.pszTemplate = MAKEINTRESOURCE(IDD_OPTIONS);
	odp.pszTitle = jabberModuleName;
	odp.flags = ODPF_BOLDGROUPS;
	odp.pfnDlgProc = TlenOptDlgProc;
	odp.nIDBottomSimpleControl = IDC_SIMPLE;
	CallService(MS_OPT_ADDPAGE, wParam, (LPARAM) &odp);

	odp.pszTemplate = MAKEINTRESOURCE(IDD_OPTIONS_EXPERT);
	_snprintf(str, sizeof(str), "%s %s", jabberModuleName, Translate("Advanced"));
	str[sizeof(str)-1] = '\0';
	odp.pszTitle = str;
	odp.pfnDlgProc = TlenAdvOptDlgProc;
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

	return 0;
}

static LRESULT CALLBACK JabberValidateUsernameWndProc(HWND hwndEdit, UINT msg, WPARAM wParam, LPARAM lParam)
{
	WNDPROC oldProc = (WNDPROC) GetWindowLong(hwndEdit, GWL_USERDATA);

	switch (msg) {
	case WM_CHAR:
		if (strchr("\"&'/:<>@", wParam&0xff) != NULL)
			return 0;
		break;
	}
	return CallWindowProc(oldProc, hwndEdit, msg, wParam, lParam);
}

static BOOL CALLBACK TlenOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	char text[256];
	WORD port;
	WNDPROC oldProc;

	switch (msg) {
	case WM_INITDIALOG:
		{
			DBVARIANT dbv;
			BOOL enableRegister = TRUE;

			TranslateDialogDefault(hwndDlg);
			SetDlgItemText(hwndDlg, IDC_TLEN, jabberModuleName);
#ifdef TLEN_PLUGIN
			ShowWindow(GetDlgItem(hwndDlg, IDC_USE_SSL), SW_HIDE);
#endif
			if (!DBGetContactSetting(NULL, jabberProtoName, "LoginName", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_EDIT_USERNAME, dbv.pszVal);
				if (!dbv.pszVal[0]) enableRegister = FALSE;
				DBFreeVariant(&dbv);
			}
			if (!DBGetContactSetting(NULL, jabberProtoName, "Password", &dbv)) {
				CallService(MS_DB_CRYPT_DECODESTRING, strlen(dbv.pszVal)+1, (LPARAM) dbv.pszVal);
				SetDlgItemText(hwndDlg, IDC_EDIT_PASSWORD, dbv.pszVal);
				if (!dbv.pszVal[0]) enableRegister = FALSE;
				DBFreeVariant(&dbv);
			}
			if (!DBGetContactSetting(NULL, jabberProtoName, "Resource", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_EDIT_RESOURCE, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			else {
				SetDlgItemText(hwndDlg, IDC_EDIT_RESOURCE, "t");
			}
			CheckDlgButton(hwndDlg, IDC_SAVEPASSWORD, DBGetContactSettingByte(NULL, jabberProtoName, "SavePassword", TRUE));
			if (!DBGetContactSetting(NULL, jabberProtoName, "LoginServer", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_EDIT_LOGIN_SERVER, dbv.pszVal);
				if (!dbv.pszVal[0]) enableRegister = FALSE;
				DBFreeVariant(&dbv);
			}
			else {
				SetDlgItemText(hwndDlg, IDC_EDIT_LOGIN_SERVER, "tlen.pl");
			}
			port = (WORD) DBGetContactSettingWord(NULL, jabberProtoName, "Port", 443);
			SetDlgItemInt(hwndDlg, IDC_PORT, port, FALSE);
			if (port <= 0) enableRegister = FALSE;

			CheckDlgButton(hwndDlg, IDC_USE_SSL, DBGetContactSettingByte(NULL, jabberProtoName, "UseSSL", FALSE));

			if (DBGetContactSettingByte(NULL, jabberProtoName, "ManualConnect", TRUE) == TRUE) {
				CheckDlgButton(hwndDlg, IDC_MANUAL, TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_HOST), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_HOSTPORT), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_PORT), FALSE);
			}
			if (!DBGetContactSetting(NULL, jabberProtoName, "ManualHost", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_HOST, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			else
				SetDlgItemText(hwndDlg, IDC_HOST, "s1.tlen.pl");
			SetDlgItemInt(hwndDlg, IDC_HOSTPORT, DBGetContactSettingWord(NULL, jabberProtoName, "ManualPort", 443), FALSE);

			CheckDlgButton(hwndDlg, IDC_KEEPALIVE, DBGetContactSettingByte(NULL, jabberProtoName, "KeepAlive", TRUE));
			CheckDlgButton(hwndDlg, IDC_RECONNECT, DBGetContactSettingByte(NULL, jabberProtoName, "Reconnect", FALSE));
			CheckDlgButton(hwndDlg, IDC_ROSTER_SYNC, DBGetContactSettingByte(NULL, jabberProtoName, "RosterSync", FALSE));
			CheckDlgButton(hwndDlg, IDC_SHOW_OFFLINE, DBGetContactSettingByte(NULL, jabberProtoName, "OfflineAsInvisible", FALSE));
			CheckDlgButton(hwndDlg, IDC_OFFLINE_MESSAGE, DBGetContactSettingByte(NULL, jabberProtoName, "LeaveOfflineMessage", FALSE));
			CheckDlgButton(hwndDlg, IDC_VISIBILITY_SUPPORT, DBGetContactSettingByte(NULL, jabberProtoName, "VisibilitySupport", FALSE));

			SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)Translate("<Last message>"));
	        //SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)Translate("<Ask me>"));
	        SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)Translate("Online"));
	        SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)Translate("Away"));
	        SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)Translate("NA"));
	        SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)Translate("DND"));
	        SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)Translate("Free for chat"));
	        SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)Translate("Invisible"));
			SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_SETCURSEL, DBGetContactSettingWord(NULL, jabberProtoName, "OfflineMessageOption", 0), 0);

			oldProc = (WNDPROC) GetWindowLong(GetDlgItem(hwndDlg, IDC_EDIT_USERNAME), GWL_WNDPROC);
			SetWindowLong(GetDlgItem(hwndDlg, IDC_EDIT_USERNAME), GWL_USERDATA, (LONG) oldProc);
			SetWindowLong(GetDlgItem(hwndDlg, IDC_EDIT_USERNAME), GWL_WNDPROC, (LONG) JabberValidateUsernameWndProc);

			return TRUE;
		}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_EDIT_USERNAME:
		case IDC_EDIT_PASSWORD:
		case IDC_EDIT_RESOURCE:
		case IDC_EDIT_LOGIN_SERVER:
		case IDC_PORT:
		case IDC_MANUAL:
		case IDC_HOST:
		case IDC_HOSTPORT:
			if (LOWORD(wParam) == IDC_MANUAL) {
				if (IsDlgButtonChecked(hwndDlg, IDC_MANUAL)) {
					EnableWindow(GetDlgItem(hwndDlg, IDC_HOST), TRUE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_HOSTPORT), TRUE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_PORT), FALSE);
				}
				else {
					EnableWindow(GetDlgItem(hwndDlg, IDC_HOST), FALSE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_HOSTPORT), FALSE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_PORT), TRUE);
				}
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			}
			else {
				if ((HWND)lParam==GetFocus() && HIWORD(wParam)==EN_CHANGE)
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			}
			break;
		case IDC_USE_SSL:
			// Fall through
		case IDC_SAVEPASSWORD:
		case IDC_KEEPALIVE:
		case IDC_RECONNECT:
		case IDC_ROSTER_SYNC:
		case IDC_SHOW_OFFLINE:
		case IDC_OFFLINE_MESSAGE:
		case IDC_VISIBILITY_SUPPORT:
			SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			break;
		case IDC_REGISTERACCOUNT:
		    CallService(MS_UTILS_OPENURL, (WPARAM) 1, (LPARAM) TLEN_REGISTER);
		    break;
		default:
			SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			break;
		}
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR) lParam)->code) {
		case PSN_APPLY:
			{
				BOOL reconnectRequired = FALSE;
				DBVARIANT dbv;

				GetDlgItemText(hwndDlg, IDC_EDIT_USERNAME, text, sizeof(text));
				if (DBGetContactSetting(NULL, jabberProtoName, "LoginName", &dbv) || strcmp(text, dbv.pszVal))
					reconnectRequired = TRUE;
				if (dbv.pszVal != NULL)	DBFreeVariant(&dbv);
				DBWriteContactSettingString(NULL, jabberProtoName, "LoginName", text);

				if (IsDlgButtonChecked(hwndDlg, IDC_SAVEPASSWORD)) {
					GetDlgItemText(hwndDlg, IDC_EDIT_PASSWORD, text, sizeof(text));
					CallService(MS_DB_CRYPT_ENCODESTRING, sizeof(text), (LPARAM) text);
					if (DBGetContactSetting(NULL, jabberProtoName, "Password", &dbv) || strcmp(text, dbv.pszVal))
						reconnectRequired = TRUE;
					if (dbv.pszVal != NULL)	DBFreeVariant(&dbv);
					DBWriteContactSettingString(NULL, jabberProtoName, "Password", text);
				}
				else
					DBDeleteContactSetting(NULL, jabberProtoName, "Password");

				GetDlgItemText(hwndDlg, IDC_EDIT_RESOURCE, text, sizeof(text));
				if (DBGetContactSetting(NULL, jabberProtoName, "Resource", &dbv) || strcmp(text, dbv.pszVal))
					reconnectRequired = TRUE;
				if (dbv.pszVal != NULL)	DBFreeVariant(&dbv);
				DBWriteContactSettingString(NULL, jabberProtoName, "Resource", text);

				DBWriteContactSettingByte(NULL, jabberProtoName, "SavePassword", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_SAVEPASSWORD));

				GetDlgItemText(hwndDlg, IDC_EDIT_LOGIN_SERVER, text, sizeof(text));
				if (DBGetContactSetting(NULL, jabberProtoName, "LoginServer", &dbv) || strcmp(text, dbv.pszVal))
					reconnectRequired = TRUE;
				if (dbv.pszVal != NULL)	DBFreeVariant(&dbv);
				DBWriteContactSettingString(NULL, jabberProtoName, "LoginServer", text);

				port = (WORD) GetDlgItemInt(hwndDlg, IDC_PORT, NULL, FALSE);
				if (DBGetContactSettingWord(NULL, jabberProtoName, "Port", JABBER_DEFAULT_PORT) != port)
					reconnectRequired = TRUE;
				DBWriteContactSettingWord(NULL, jabberProtoName, "Port", port);

				DBWriteContactSettingByte(NULL, jabberProtoName, "UseSSL", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_USE_SSL));

				DBWriteContactSettingByte(NULL, jabberProtoName, "ManualConnect", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_MANUAL));

				GetDlgItemText(hwndDlg, IDC_HOST, text, sizeof(text));
				if (DBGetContactSetting(NULL, jabberProtoName, "ManualHost", &dbv) || strcmp(text, dbv.pszVal))
					reconnectRequired = TRUE;
				if (dbv.pszVal != NULL)	DBFreeVariant(&dbv);
				DBWriteContactSettingString(NULL, jabberProtoName, "ManualHost", text);

				port = (WORD) GetDlgItemInt(hwndDlg, IDC_HOSTPORT, NULL, FALSE);
				if (DBGetContactSettingWord(NULL, jabberProtoName, "ManualPort", JABBER_DEFAULT_PORT) != port)
					reconnectRequired = TRUE;
				DBWriteContactSettingWord(NULL, jabberProtoName, "ManualPort", port);

				DBWriteContactSettingByte(NULL, jabberProtoName, "KeepAlive", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_KEEPALIVE));
				jabberSendKeepAlive = IsDlgButtonChecked(hwndDlg, IDC_KEEPALIVE);

				DBWriteContactSettingByte(NULL, jabberProtoName, "Reconnect", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_RECONNECT));
				DBWriteContactSettingByte(NULL, jabberProtoName, "RosterSync", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_ROSTER_SYNC));
				DBWriteContactSettingByte(NULL, jabberProtoName, "OfflineAsInvisible", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_SHOW_OFFLINE));
				DBWriteContactSettingByte(NULL, jabberProtoName, "LeaveOfflineMessage", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_OFFLINE_MESSAGE));
				DBWriteContactSettingWord(NULL, jabberProtoName, "OfflineMessageOption", (WORD) SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_GETCURSEL, 0, 0));
				DBWriteContactSettingByte(NULL, jabberProtoName, "VisibilitySupport", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_VISIBILITY_SUPPORT));

				if (reconnectRequired && jabberConnected)
					MessageBox(hwndDlg, Translate("These changes will take effect the next time you connect to the Tlen network."), Translate("Tlen Protocol Option"), MB_OK|MB_SETFOREGROUND);

				return TRUE;
			}
		}
		break;
	}

	return FALSE;
}

static BOOL CALLBACK TlenAdvOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	char text[256];
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
			TlenVoiceBuildInDeviceList(GetDlgItem(hwndDlg, IDC_VOICE_DEVICE_IN));
			TlenVoiceBuildOutDeviceList(GetDlgItem(hwndDlg, IDC_VOICE_DEVICE_OUT));
			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (LOWORD(wParam)) {
			case IDC_VOICE_DEVICE_IN:
			case IDC_VOICE_DEVICE_OUT:
				if (HIWORD(wParam) == CBN_SELCHANGE)
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;
			case IDC_FILE_PROXY_HOST:
			case IDC_FILE_PROXY_PORT:
			case IDC_FILE_PROXY_USER:
			case IDC_FILE_PROXY_PASSWORD:
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
//				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
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
				DBWriteContactSettingWord(NULL, jabberProtoName, "VoiceDeviceIn", (WORD) SendDlgItemMessage(hwndDlg, IDC_VOICE_DEVICE_IN, CB_GETCURSEL, 0, 0));
				DBWriteContactSettingWord(NULL, jabberProtoName, "VoiceDeviceOut", (WORD) SendDlgItemMessage(hwndDlg, IDC_VOICE_DEVICE_OUT, CB_GETCURSEL, 0, 0));
				return TRUE;
			}
		}
		break;
	case WM_DESTROY:
		break;
	}

	return FALSE;
}

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

static BOOL CALLBACK TlenPopupsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
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

	}
	return FALSE;
}

