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
#include <commctrl.h>
#include "resource.h"

static BOOL CALLBACK RVPOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

int RVPOptInit(WPARAM wParam, LPARAM lParam) {
	OPTIONSDIALOGPAGE odp;
	ZeroMemory(&odp, sizeof(odp));
	odp.cbSize = sizeof(odp);
	odp.position = 0;
	odp.hInstance = hInst;
	odp.pszGroup = Translate("Network");
	odp.pszTemplate = MAKEINTRESOURCE(IDD_OPTIONS);
	odp.pszTitle = rvpModuleName;
	odp.flags = ODPF_BOLDGROUPS;
	odp.pfnDlgProc = RVPOptDlgProc;
	odp.nIDBottomSimpleControl = IDC_SIMPLE;
	CallService(MS_OPT_ADDPAGE, wParam, (LPARAM) &odp);
	return 0;
}

static BOOL CALLBACK RVPOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	char text[256];
	switch (msg) {
	case WM_INITDIALOG:
		{
			DBVARIANT dbv;

			TranslateDialogDefault(hwndDlg);
			SetDlgItemText(hwndDlg, IDC_RVP, rvpModuleName);
#ifdef TLEN_PLUGIN
			ShowWindow(GetDlgItem(hwndDlg, IDC_USE_SSL), SW_HIDE);
#endif
			if (!DBGetContactSetting(NULL, rvpProtoName, "LoginName", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_EDIT_USERNAME, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			if (!DBGetContactSetting(NULL, rvpProtoName, "LoginServer", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_EDIT_SERVER, dbv.pszVal);
				DBFreeVariant(&dbv);
			} else {
//				SetDlgItemText(hwndDlg, IDC_EDIT_SERVER, "im.companyname.com");
			}
			if (DBGetContactSettingByte(NULL, rvpProtoName, "ManualServer", FALSE) == TRUE) {
				CheckDlgButton(hwndDlg, IDC_MANUAL_SERVER, TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_SERVER), TRUE);
			} else {
				EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_SERVER), FALSE);
			}

			if (DBGetContactSettingByte(NULL, rvpProtoName, "ManualConnect", TRUE) == TRUE) {
				CheckDlgButton(hwndDlg, IDC_MANUAL, TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_NTDOMAIN), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_NTUSERNAME), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_NTPASSWORD), TRUE);
			}
			if (!DBGetContactSetting(NULL, rvpProtoName, "NTDomain", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_EDIT_NTDOMAIN, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			if (!DBGetContactSetting(NULL, rvpProtoName, "NTUsername", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_EDIT_NTUSERNAME, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			if (!DBGetContactSetting(NULL, rvpProtoName, "NTPassword", &dbv)) {
				CallService(MS_DB_CRYPT_DECODESTRING, strlen(dbv.pszVal)+1, (LPARAM) dbv.pszVal);
				SetDlgItemText(hwndDlg, IDC_EDIT_NTPASSWORD, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			CheckDlgButton(hwndDlg, IDC_SAVEPASSWORD, DBGetContactSettingByte(NULL, rvpProtoName, "SavePassword", TRUE));
			return TRUE;
		}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_EDIT_USERNAME:
		case IDC_EDIT_PASSWORD:
		case IDC_EDIT_SERVER:
		case IDC_MANUAL:
		case IDC_EDIT_NTDOMAIN:
		case IDC_EDIT_NTUSERNAME:
		case IDC_EDIT_NTPASSWORD:
			if (LOWORD(wParam) == IDC_MANUAL) {
				if (IsDlgButtonChecked(hwndDlg, IDC_MANUAL)) {
					EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_NTDOMAIN), TRUE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_NTUSERNAME), TRUE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_NTPASSWORD), TRUE);
				} else {
					EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_NTDOMAIN), FALSE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_NTUSERNAME), FALSE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_NTPASSWORD), FALSE);
				}
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			}
			else {
				if ((HWND)lParam==GetFocus() && HIWORD(wParam)==EN_CHANGE)
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			}
			break;
		case IDC_MANUAL_SERVER:
			if (IsDlgButtonChecked(hwndDlg, IDC_MANUAL_SERVER)) {
				EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_SERVER), TRUE);
			} else {
				EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_SERVER), FALSE);
			}
		case IDC_USE_SSL:
		case IDC_SAVEPASSWORD:
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
				if (DBGetContactSetting(NULL, rvpProtoName, "LoginName", &dbv) || strcmp(text, dbv.pszVal))
					reconnectRequired = TRUE;
				if (dbv.pszVal != NULL)	DBFreeVariant(&dbv);
				DBWriteContactSettingString(NULL, rvpProtoName, "LoginName", text);

				GetDlgItemText(hwndDlg, IDC_EDIT_SERVER, text, sizeof(text));
				if (DBGetContactSetting(NULL, rvpProtoName, "LoginServer", &dbv) || strcmp(text, dbv.pszVal))
					reconnectRequired = TRUE;
				if (dbv.pszVal != NULL)	DBFreeVariant(&dbv);
				DBWriteContactSettingString(NULL, rvpProtoName, "LoginServer", text);
				DBWriteContactSettingByte(NULL, rvpProtoName, "ManualServer", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_MANUAL_SERVER));

				DBWriteContactSettingByte(NULL, rvpProtoName, "ManualConnect", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_MANUAL));

				GetDlgItemText(hwndDlg, IDC_EDIT_NTDOMAIN, text, sizeof(text));
//				if (DBGetContactSetting(NULL, rvpProtoName, "NTDomain", &dbv) || strcmp(text, dbv.pszVal))
//				if (dbv.pszVal != NULL)	DBFreeVariant(&dbv);
				DBWriteContactSettingString(NULL, rvpProtoName, "NTDomain", text);

				GetDlgItemText(hwndDlg, IDC_EDIT_NTUSERNAME, text, sizeof(text));
				DBWriteContactSettingString(NULL, rvpProtoName, "NTUsername", text);

				GetDlgItemText(hwndDlg, IDC_EDIT_NTPASSWORD, text, sizeof(text));
				DBWriteContactSettingString(NULL, rvpProtoName, "NTPassword", text);

				if (IsDlgButtonChecked(hwndDlg, IDC_SAVEPASSWORD)) {
					GetDlgItemText(hwndDlg, IDC_EDIT_NTPASSWORD, text, sizeof(text));
					CallService(MS_DB_CRYPT_ENCODESTRING, sizeof(text), (LPARAM) text);
					DBWriteContactSettingString(NULL, rvpProtoName, "NTPassword", text);
				}
				else
					DBDeleteContactSetting(NULL, rvpProtoName, "NTPassword");

				DBWriteContactSettingByte(NULL, rvpProtoName, "SavePassword", (BYTE) IsDlgButtonChecked(hwndDlg, IDC_SAVEPASSWORD));

				if (reconnectRequired && jabberConnected)
					MessageBox(hwndDlg, Translate("These changes will take effect the next time you connect to the Jabber network."), Translate("Jabber Protocol Option"), MB_OK|MB_SETFOREGROUND);

				return TRUE;
			}
		}
		break;
	}

	return FALSE;
}
