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
#include <win2k.h>
#include <commctrl.h>
#include "resource.h"

static BOOL CALLBACK TlenBasicOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK TlenVoiceOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK TlenAdvOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK TlenPopupsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

typedef struct TabDefStruct {
	DLGPROC dlgProc;
	DWORD dlgId;
	TCHAR *tabName;
} TabDef;

static TabDef tabPages[] = {
						 {TlenBasicOptDlgProc, IDD_OPTIONS_BASIC, _T("General")},
						 {TlenVoiceOptDlgProc, IDD_OPTIONS_VOICE, _T("Voice Chats")},
						 {TlenAdvOptDlgProc, IDD_OPTIONS_ADVANCED, _T("Advanced")}
						 };

TLEN_OPTIONS tlenOptions;

void TlenLoadOptions()
{
	tlenOptions.useEncryption = DBGetContactSettingByte(NULL, jabberProtoName, "UseEncryption", TRUE);
	tlenOptions.reconnect = DBGetContactSettingByte(NULL, jabberProtoName, "Reconnect", FALSE);
	tlenOptions.alertPolicy = DBGetContactSettingWord(NULL, jabberProtoName, "AlertPolicy", 0);
	tlenOptions.rosterSync = DBGetContactSettingByte(NULL, jabberProtoName, "RosterSync", FALSE);
	tlenOptions.offlineAsInvisible = DBGetContactSettingByte(NULL, jabberProtoName, "OfflineAsInvisible", FALSE);
	tlenOptions.leaveOfflineMessage = DBGetContactSettingByte(NULL, jabberProtoName, "LeaveOfflineMessage", FALSE);
	tlenOptions.offlineMessageOption = DBGetContactSettingWord(NULL, jabberProtoName, "OfflineMessageOption", 0);
	tlenOptions.ignoreAdvertisements = DBGetContactSettingByte(NULL, jabberProtoName, "IgnoreAdvertisements", TRUE);
	tlenOptions.groupChatPolicy = DBGetContactSettingWord(NULL, jabberProtoName, "GroupChatPolicy", 0);
	tlenOptions.voiceChatPolicy = DBGetContactSettingWord(NULL, jabberProtoName, "VoiceChatPolicy", 0);
	tlenOptions.enableAvatars = DBGetContactSettingByte(NULL, jabberProtoName, "EnableAvatars", FALSE);
	tlenOptions.enableVersion = DBGetContactSettingByte(NULL, jabberProtoName, "EnableVersion", FALSE);
	tlenOptions.useNudge = DBGetContactSettingByte(NULL, jabberProtoName, "UseNudge", FALSE);
	tlenOptions.logAlerts = DBGetContactSettingByte(NULL, jabberProtoName, "LogAlerts", FALSE);
}

static int changed = 0;

static void ApplyChanges(int i) {
	changed &= ~i;
	if (changed == 0) {
		TlenLoadOptions();
	}
}

static void MarkChanges(int i, HWND hWnd) {
	SendMessage(GetParent(hWnd), PSM_CHANGED, 0, 0);
	changed |= i;
}


int TlenOptInit(WPARAM wParam, LPARAM lParam)
{
	int i;
	OPTIONSDIALOGPAGE odp = { 0 };
	odp.cbSize = sizeof(odp);
	odp.position = 0;
	odp.hInstance = hInst;
	odp.pszGroup = TranslateT("Network");
	odp.pszTitle = jabberModuleName;
	odp.flags = ODPF_BOLDGROUPS | ODPF_TCHAR;
	odp.nIDBottomSimpleControl = 0;//IDC_SIMPLE;
	for (i = 0; i < SIZEOF(tabPages); i++) {
		odp.pszTemplate = MAKEINTRESOURCEA(tabPages[i].dlgId);
		odp.pfnDlgProc = tabPages[i].dlgProc;
		odp.ptszTab = tabPages[i].tabName;
		CallService(MS_OPT_ADDPAGE, wParam, (LPARAM) & odp);
	}

	if (ServiceExists(MS_POPUP_ADDPOPUP)) {
		ZeroMemory(&odp,sizeof(odp));
		odp.cbSize = sizeof(odp);
		odp.position = 100000000;
		odp.hInstance = hInst;
		odp.pszGroup = TranslateT("PopUps");
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


static BOOL CALLBACK TlenBasicOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	char text[256];
	WNDPROC oldProc;

	switch (msg) {
	case WM_INITDIALOG:
		{
			DBVARIANT dbv;

			TranslateDialogDefault(hwndDlg);
			SetDlgItemText(hwndDlg, IDC_TLEN, jabberModuleName);
			if (!DBGetContactSetting(NULL, jabberProtoName, "LoginName", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_EDIT_USERNAME, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			if (!DBGetContactSetting(NULL, jabberProtoName, "Password", &dbv)) {
				CallService(MS_DB_CRYPT_DECODESTRING, strlen(dbv.pszVal)+1, (LPARAM) dbv.pszVal);
				SetDlgItemText(hwndDlg, IDC_EDIT_PASSWORD, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			CheckDlgButton(hwndDlg, IDC_SAVEPASSWORD, DBGetContactSettingByte(NULL, jabberProtoName, "SavePassword", TRUE));

			CheckDlgButton(hwndDlg, IDC_RECONNECT, tlenOptions.reconnect);
			CheckDlgButton(hwndDlg, IDC_ROSTER_SYNC, tlenOptions.rosterSync);
			CheckDlgButton(hwndDlg, IDC_SHOW_OFFLINE, tlenOptions.offlineAsInvisible);
			CheckDlgButton(hwndDlg, IDC_OFFLINE_MESSAGE, tlenOptions.leaveOfflineMessage);
			CheckDlgButton(hwndDlg, IDC_IGNORE_ADVERTISEMENTS, tlenOptions.ignoreAdvertisements);
			CheckDlgButton(hwndDlg, IDC_AVATARS, tlenOptions.enableAvatars);
			CheckDlgButton(hwndDlg, IDC_VERSIONINFO, tlenOptions.enableVersion);
			CheckDlgButton(hwndDlg, IDC_NUDGE_SUPPORT, tlenOptions.useNudge);
			CheckDlgButton(hwndDlg, IDC_LOG_ALERTS, tlenOptions.logAlerts);

			SendDlgItemMessage(hwndDlg, IDC_ALERT_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Accept all alerts"));
			SendDlgItemMessage(hwndDlg, IDC_ALERT_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Ignore alerts from unauthorized contacts"));
			SendDlgItemMessage(hwndDlg, IDC_ALERT_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Ignore all alerts"));
			SendDlgItemMessage(hwndDlg, IDC_ALERT_POLICY, CB_SETCURSEL, tlenOptions.alertPolicy, 0);

			SendDlgItemMessage(hwndDlg, IDC_MUC_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Always ask me"));
			SendDlgItemMessage(hwndDlg, IDC_MUC_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Accept invitations from authorized contacts"));
			SendDlgItemMessage(hwndDlg, IDC_MUC_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Accept all invitations"));
			SendDlgItemMessage(hwndDlg, IDC_MUC_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Ignore invitations from unauthorized contacts"));
			SendDlgItemMessage(hwndDlg, IDC_MUC_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Ignore all invitation"));
			SendDlgItemMessage(hwndDlg, IDC_MUC_POLICY, CB_SETCURSEL, tlenOptions.groupChatPolicy, 0);

			SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)TranslateT("<Last message>"));
	        //SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)TranslateT("<Ask me>"));
	        SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)TranslateT("Online"));
	        SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)TranslateT("Away"));
	        SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)TranslateT("NA"));
	        SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)TranslateT("DND"));
	        SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)TranslateT("Free for chat"));
	        SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_ADDSTRING, 0, (LPARAM)TranslateT("Invisible"));
			SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_SETCURSEL, tlenOptions.offlineMessageOption, 0);

			oldProc = (WNDPROC) GetWindowLong(GetDlgItem(hwndDlg, IDC_EDIT_USERNAME), GWL_WNDPROC);
			SetWindowLong(GetDlgItem(hwndDlg, IDC_EDIT_USERNAME), GWL_USERDATA, (LONG) oldProc);
			SetWindowLong(GetDlgItem(hwndDlg, IDC_EDIT_USERNAME), GWL_WNDPROC, (LONG) JabberValidateUsernameWndProc);
			return TRUE;
		}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_EDIT_USERNAME:
		case IDC_EDIT_PASSWORD:
			if ((HWND)lParam==GetFocus() && HIWORD(wParam)==EN_CHANGE)
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			break;
			// Fall through
		case IDC_SAVEPASSWORD:
		case IDC_RECONNECT:
		case IDC_ROSTER_SYNC:
		case IDC_IGNORE_ADVERTISEMENTS:
		case IDC_SHOW_OFFLINE:
		case IDC_OFFLINE_MESSAGE:
			MarkChanges(1, hwndDlg);
			break;
		case IDC_LOG_ALERTS:
			CheckDlgButton(hwndDlg, IDC_NUDGE_SUPPORT, BST_UNCHECKED);
			MarkChanges(1, hwndDlg);
			break;
		case IDC_NUDGE_SUPPORT:
			CheckDlgButton(hwndDlg, IDC_LOG_ALERTS, BST_UNCHECKED);
			MarkChanges(1, hwndDlg);
			break;
		case IDC_REGISTERACCOUNT:
		    CallService(MS_UTILS_OPENURL, (WPARAM) 1, (LPARAM) TLEN_REGISTER);
		    break;
		case IDC_OFFLINE_MESSAGE_OPTION:
		case IDC_ALERT_POLICY:
		case IDC_MUC_POLICY:
			if (HIWORD(wParam) == CBN_SELCHANGE)
				MarkChanges(1, hwndDlg);
			break;
		default:
			MarkChanges(1, hwndDlg);
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
				DBWriteContactSettingString(NULL, jabberProtoName, "LoginName", strlwr(text));

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

				DBWriteContactSettingByte(NULL, jabberProtoName, "SavePassword", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_SAVEPASSWORD));
				DBWriteContactSettingByte(NULL, jabberProtoName, "Reconnect", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_RECONNECT));
				DBWriteContactSettingByte(NULL, jabberProtoName, "RosterSync", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_ROSTER_SYNC));
				DBWriteContactSettingByte(NULL, jabberProtoName, "OfflineAsInvisible", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_SHOW_OFFLINE));
				DBWriteContactSettingByte(NULL, jabberProtoName, "IgnoreAdvertisements", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_IGNORE_ADVERTISEMENTS));
				DBWriteContactSettingByte(NULL, jabberProtoName, "LeaveOfflineMessage", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_OFFLINE_MESSAGE));
				DBWriteContactSettingWord(NULL, jabberProtoName, "OfflineMessageOption", (WORD) SendDlgItemMessage(hwndDlg, IDC_OFFLINE_MESSAGE_OPTION, CB_GETCURSEL, 0, 0));
				DBWriteContactSettingWord(NULL, jabberProtoName, "AlertPolicy", (WORD) SendDlgItemMessage(hwndDlg, IDC_ALERT_POLICY, CB_GETCURSEL, 0, 0));
				DBWriteContactSettingWord(NULL, jabberProtoName, "GroupChatPolicy", (WORD) SendDlgItemMessage(hwndDlg, IDC_MUC_POLICY, CB_GETCURSEL, 0, 0));
				DBWriteContactSettingByte(NULL, jabberProtoName, "EnableAvatars", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_AVATARS));
				DBWriteContactSettingByte(NULL, jabberProtoName, "EnableVersion", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_VERSIONINFO));
				DBWriteContactSettingByte(NULL, jabberProtoName, "UseNudge", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_NUDGE_SUPPORT));
				DBWriteContactSettingByte(NULL, jabberProtoName, "LogAlerts", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_LOG_ALERTS));
				if (reconnectRequired && jabberConnected)
					MessageBox(hwndDlg, TranslateT("These changes will take effect the next time you connect to the Tlen network."), TranslateT("Tlen Protocol Option"), MB_OK|MB_SETFOREGROUND);
				ApplyChanges(1);
				return TRUE;
			}
		}
		break;
	}
	return FALSE;
}

static BOOL CALLBACK TlenVoiceOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		{
			SendDlgItemMessage(hwndDlg, IDC_VOICE_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Always ask me"));
			SendDlgItemMessage(hwndDlg, IDC_VOICE_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Accept invitations from authorized contacts"));
			SendDlgItemMessage(hwndDlg, IDC_VOICE_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Accept all invitations"));
			SendDlgItemMessage(hwndDlg, IDC_VOICE_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Ignore invitations from unauthorized contacts"));
			SendDlgItemMessage(hwndDlg, IDC_VOICE_POLICY, CB_ADDSTRING, 0, (LPARAM)TranslateT("Ignore all invitation"));
			SendDlgItemMessage(hwndDlg, IDC_VOICE_POLICY, CB_SETCURSEL, tlenOptions.voiceChatPolicy, 0);
			TlenVoiceBuildInDeviceList(GetDlgItem(hwndDlg, IDC_VOICE_DEVICE_IN));
			TlenVoiceBuildOutDeviceList(GetDlgItem(hwndDlg, IDC_VOICE_DEVICE_OUT));
			return TRUE;
		}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_VOICE_POLICY:
		case IDC_VOICE_DEVICE_IN:
		case IDC_VOICE_DEVICE_OUT:
			if (HIWORD(wParam) == CBN_SELCHANGE)
				MarkChanges(2, hwndDlg);
			break;
		}
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR) lParam)->code) {
		case PSN_APPLY:
			{
				DBWriteContactSettingWord(NULL, jabberProtoName, "VoiceChatPolicy", (WORD) SendDlgItemMessage(hwndDlg, IDC_VOICE_POLICY, CB_GETCURSEL, 0, 0));
				DBWriteContactSettingWord(NULL, jabberProtoName, "VoiceDeviceIn", (WORD) SendDlgItemMessage(hwndDlg, IDC_VOICE_DEVICE_IN, CB_GETCURSEL, 0, 0));
				DBWriteContactSettingWord(NULL, jabberProtoName, "VoiceDeviceOut", (WORD) SendDlgItemMessage(hwndDlg, IDC_VOICE_DEVICE_OUT, CB_GETCURSEL, 0, 0));
				ApplyChanges(2);
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
			if (!DBGetContactSetting(NULL, jabberProtoName, "LoginServer", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_EDIT_LOGIN_SERVER, dbv.pszVal);
				DBFreeVariant(&dbv);
			} else {
				SetDlgItemText(hwndDlg, IDC_EDIT_LOGIN_SERVER, "tlen.pl");
			}

			EnableWindow(GetDlgItem(hwndDlg, IDC_HOST), TRUE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_HOSTPORT), TRUE);

			if (!DBGetContactSetting(NULL, jabberProtoName, "ManualHost", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_HOST, dbv.pszVal);
				DBFreeVariant(&dbv);
			} else
				SetDlgItemText(hwndDlg, IDC_HOST, "s1.tlen.pl");
			SetDlgItemInt(hwndDlg, IDC_HOSTPORT, DBGetContactSettingWord(NULL, jabberProtoName, "ManualPort", TLEN_DEFAULT_PORT), FALSE);

			CheckDlgButton(hwndDlg, IDC_KEEPALIVE, DBGetContactSettingByte(NULL, jabberProtoName, "KeepAlive", TRUE));

			CheckDlgButton(hwndDlg, IDC_USE_SSL, DBGetContactSettingByte(NULL, jabberProtoName, "UseEncryption", TRUE));
			
			CheckDlgButton(hwndDlg, IDC_VISIBILITY_SUPPORT, DBGetContactSettingByte(NULL, jabberProtoName, "VisibilitySupport", FALSE));
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

			SendDlgItemMessage(hwndDlg, IDC_FILE_PROXY_TYPE, CB_ADDSTRING, 0, (LPARAM)TranslateT("Forwarding"));
            SendDlgItemMessage(hwndDlg, IDC_FILE_PROXY_TYPE, CB_ADDSTRING, 0, (LPARAM)TranslateT("SOCKS4"));
            SendDlgItemMessage(hwndDlg, IDC_FILE_PROXY_TYPE, CB_ADDSTRING, 0, (LPARAM)TranslateT("SOCKS5"));
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
			case IDC_FILE_PROXY_TYPE:
				if (HIWORD(wParam) == CBN_SELCHANGE)
					MarkChanges(4, hwndDlg);
				break;
			case IDC_EDIT_LOGIN_SERVER:
			case IDC_HOST:
			case IDC_HOSTPORT:
			case IDC_FILE_PROXY_HOST:
			case IDC_FILE_PROXY_PORT:
			case IDC_FILE_PROXY_USER:
			case IDC_FILE_PROXY_PASSWORD:
				if ((HWND)lParam==GetFocus() && HIWORD(wParam)==EN_CHANGE)
					MarkChanges(4, hwndDlg);
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
				MarkChanges(4, hwndDlg);
				break;
			case IDC_KEEPALIVE:
			case IDC_VISIBILITY_SUPPORT:
			case IDC_USE_SSL:
				MarkChanges(4, hwndDlg);
				break;
			}
		}
		break;
	case WM_NOTIFY:
		{
			switch (((LPNMHDR) lParam)->code) {
			case PSN_APPLY:
				{
					WORD port;
					BOOL useEncryption;
					BOOL reconnectRequired = FALSE;
					DBVARIANT dbv;
					GetDlgItemText(hwndDlg, IDC_EDIT_LOGIN_SERVER, text, sizeof(text));
					if (DBGetContactSetting(NULL, jabberProtoName, "LoginServer", &dbv) || strcmp(text, dbv.pszVal))
						reconnectRequired = TRUE;
					if (dbv.pszVal != NULL)	DBFreeVariant(&dbv);
					DBWriteContactSettingString(NULL, jabberProtoName, "LoginServer", strlwr(text));

					GetDlgItemText(hwndDlg, IDC_HOST, text, sizeof(text));
					if (DBGetContactSetting(NULL, jabberProtoName, "ManualHost", &dbv) || strcmp(text, dbv.pszVal))
						reconnectRequired = TRUE;
					if (dbv.pszVal != NULL)	DBFreeVariant(&dbv);
					DBWriteContactSettingString(NULL, jabberProtoName, "ManualHost", text);

					port = (WORD) GetDlgItemInt(hwndDlg, IDC_HOSTPORT, NULL, FALSE);
					if (DBGetContactSettingWord(NULL, jabberProtoName, "ManualPort", TLEN_DEFAULT_PORT) != port)
						reconnectRequired = TRUE;
					DBWriteContactSettingWord(NULL, jabberProtoName, "ManualPort", port);

					jabberSendKeepAlive = IsDlgButtonChecked(hwndDlg, IDC_KEEPALIVE);
					DBWriteContactSettingByte(NULL, jabberProtoName, "KeepAlive", (BYTE) jabberSendKeepAlive);

					useEncryption = IsDlgButtonChecked(hwndDlg, IDC_USE_SSL);
					if (DBGetContactSettingWord(NULL, jabberProtoName, "UseEncryption", TRUE) != useEncryption)
						reconnectRequired = TRUE;
					DBWriteContactSettingByte(NULL, jabberProtoName, "UseEncryption", (BYTE) useEncryption);

					DBWriteContactSettingByte(NULL, jabberProtoName, "VisibilitySupport", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_VISIBILITY_SUPPORT));
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
					if (reconnectRequired && jabberConnected)
						MessageBox(hwndDlg, TranslateT("These changes will take effect the next time you connect to the Tlen network."), TranslateT("Tlen Protocol Option"), MB_OK|MB_SETFOREGROUND);
					ApplyChanges(4);
					return TRUE;
				}
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
	ppd.lchIcon = CopyIcon(tlenIcons[TLEN_IDI_MAIL]);
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
				TranslateDialogDefault(hwndDlg);
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
					char title[256];
					if (IsDlgButtonChecked(hwndDlg, IDC_DELAY_POPUP)) {
						delay=0;
					} else if (IsDlgButtonChecked(hwndDlg, IDC_DELAY_PERMANENT)) {
						delay=-1;
					} else {
						delay=GetDlgItemInt(hwndDlg, IDC_DELAY, NULL, FALSE);
					}
					_snprintf(title, sizeof(title), TranslateT("%s mail"), jabberModuleName);
					MailPopupPreview((DWORD) SendDlgItemMessage(hwndDlg,IDC_COLORBKG,CPM_GETCOLOUR,0,0),
									 (DWORD) SendDlgItemMessage(hwndDlg,IDC_COLORTXT,CPM_GETCOLOUR,0,0),
									 title,
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

