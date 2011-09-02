/*

Jabber Protocol Plugin for Miranda IM
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
#include <commctrl.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "jabber_list.h"
#include "resource.h"
#include "tlen_avatar.h"

JABBER_FIELD_MAP tlenFieldGender[] = {
	{ 1, _T("Male") },
	{ 2, _T("Female") },
	{ 0, NULL }
};
JABBER_FIELD_MAP tlenFieldLookfor[] = {
	{ 1, _T("Somebody to talk") },
	{ 2, _T("Friendship") },
	{ 3, _T("Flirt/romance") },
	{ 4, _T("Love") },
	{ 5, _T("Nothing") },
	{ 0, NULL }
};
JABBER_FIELD_MAP tlenFieldStatus[] = {
	{ 1, _T("All") },
	{ 2, _T("Available") },
	{ 3, _T("Free for chat") },
	{ 0, NULL }
};
JABBER_FIELD_MAP tlenFieldOccupation[] = {
	{ 1, _T("Student") },
	{ 2, _T("College student") },
	{ 3, _T("Farmer") },
	{ 4, _T("Manager") },
	{ 5, _T("Specialist") },
	{ 6, _T("Clerk") },
	{ 7, _T("Unemployed") },
	{ 8, _T("Pensioner") },
	{ 9, _T("Housekeeper") },
	{ 10, _T("Teacher") },
	{ 11, _T("Doctor") },
	{ 12, _T("Other") },
	{ 0, NULL }
};
JABBER_FIELD_MAP tlenFieldPlan[] = {
	{ 1, _T("I'd like to go downtown") },
	{ 2, _T("I'd like to go to the cinema") },
	{ 3, _T("I'd like to take a walk") },
	{ 4, _T("I'd like to go to the disco") },
	{ 5, _T("I'd like to go on a blind date") },
	{ 6, _T("Waiting for suggestion") },
	{ 0, NULL }
};

static INT_PTR CALLBACK TlenUserInfoDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

static void InitComboBox(HWND hwndCombo, JABBER_FIELD_MAP *fieldMap)
{
	int i, n;

	n = SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)_T(""));
	SendMessage(hwndCombo, CB_SETITEMDATA, n, 0);
	SendMessage(hwndCombo, CB_SETCURSEL, n, 0);
	for(i=0;;i++) {
		if (fieldMap[i].name == NULL)
			break;
		n = SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM) TranslateTS(fieldMap[i].name));
		SendMessage(hwndCombo, CB_SETITEMDATA, n, fieldMap[i].id);
	}
}

static void FetchField(HWND hwndDlg, UINT idCtrl, char *fieldName, char **str, int *strSize)
{
	char text[512];
	char *localFieldName, *localText;

	if (hwndDlg==NULL || fieldName==NULL || str==NULL || strSize==NULL)
		return;
	GetDlgItemTextA(hwndDlg, idCtrl, text, sizeof(text));
	if (text[0]) {
		if ((localFieldName=JabberTextEncode(fieldName)) != NULL) {
			if ((localText=JabberTextEncode(text)) != NULL) {
				JabberStringAppend(str, strSize, "<%s>%s</%s>", localFieldName, localText, localFieldName);
				mir_free(localText);
			}
			mir_free(localFieldName);
		}
	}
}

static void FetchCombo(HWND hwndDlg, UINT idCtrl, char *fieldName, char **str, int *strSize)
{
	int value;
	char *localFieldName;

	if (hwndDlg==NULL || fieldName==NULL || str==NULL || strSize==NULL)
		return;
	value = (int) SendDlgItemMessage(hwndDlg, idCtrl, CB_GETITEMDATA, SendDlgItemMessage(hwndDlg, idCtrl, CB_GETCURSEL, 0, 0), 0);
	if (value > 0) {
		if ((localFieldName=JabberTextEncode(fieldName)) != NULL) {
			JabberStringAppend(str, strSize, "<%s>%d</%s>", localFieldName, value, localFieldName);
			mir_free(localFieldName);
		}
	}
}


int TlenUserInfoInit(void *ptr, WPARAM wParam, LPARAM lParam)
{
	char *szProto;
	HANDLE hContact;
	OPTIONSDIALOGPAGE odp = {0};
	TlenProtocol *proto = (TlenProtocol *)ptr;

	if (!CallService(MS_PROTO_ISPROTOCOLLOADED, 0, (LPARAM) proto->iface.m_szModuleName))
		return 0;
	hContact = (HANDLE) lParam;
	szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
	if ((szProto!=NULL && !strcmp(szProto, proto->iface.m_szModuleName)) || !lParam) {
		odp.cbSize = sizeof(odp);
		odp.hInstance = hInst;
        odp.flags = ODPF_TCHAR;
		odp.pfnDlgProc = TlenUserInfoDlgProc;
		odp.position = -2000000000;
		odp.pszTemplate = ((HANDLE)lParam!=NULL) ? MAKEINTRESOURCEA(IDD_USER_INFO):MAKEINTRESOURCEA(IDD_USER_VCARD);
        odp.ptszTitle = (hContact != NULL) ? TranslateT("Account") : proto->iface.m_tszUserName;
        odp.dwInitParam = (LPARAM)proto;
		CallService(MS_USERINFO_ADDPAGE, wParam, (LPARAM) &odp);

	}
	if (!lParam && proto->isOnline) {
		CCSDATA ccs = {0};
		JabberGetInfo(ptr, 0, (LPARAM) &ccs);
	}
	return 0;
}

typedef struct {
    TlenProtocol *proto;
    HANDLE hContact;
}TLENUSERINFODLGDATA;


static INT_PTR CALLBACK TlenUserInfoDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TLENUSERINFODLGDATA *data = (TLENUSERINFODLGDATA *) GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	switch (msg) {
	case WM_INITDIALOG:

        data = (TLENUSERINFODLGDATA*)mir_alloc(sizeof(TLENUSERINFODLGDATA));
        data->hContact = (HANDLE) lParam;
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)data);
		// lParam is hContact
		TranslateDialogDefault(hwndDlg);
		InitComboBox(GetDlgItem(hwndDlg, IDC_GENDER), tlenFieldGender);
		InitComboBox(GetDlgItem(hwndDlg, IDC_OCCUPATION), tlenFieldOccupation);
		InitComboBox(GetDlgItem(hwndDlg, IDC_LOOKFOR), tlenFieldLookfor);

		return TRUE;
	case WM_TLEN_REFRESH:
		{
			DBVARIANT dbv;
			char *jid;
			int i;
			JABBER_LIST_ITEM *item;

			SetDlgItemText(hwndDlg, IDC_INFO_JID, _T(""));
			SetDlgItemText(hwndDlg, IDC_SUBSCRIPTION, _T(""));
			SetFocus(GetDlgItem(hwndDlg, IDC_STATIC));

			if (!DBGetContactSettingTString(data->hContact, data->proto->iface.m_szModuleName, "FirstName", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_FIRSTNAME, dbv.ptszVal);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_FIRSTNAME, _T(""));
			if (!DBGetContactSettingTString(data->hContact, data->proto->iface.m_szModuleName, "LastName", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_LASTNAME, dbv.ptszVal);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_LASTNAME, _T(""));
			if (!DBGetContactSettingTString(data->hContact, data->proto->iface.m_szModuleName, "Nick", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_NICKNAME, dbv.ptszVal);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_NICKNAME, _T(""));
			if (!DBGetContactSettingTString(data->hContact, data->proto->iface.m_szModuleName, "e-mail", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_EMAIL, dbv.ptszVal);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_EMAIL, _T(""));
			if (!DBGetContactSetting(data->hContact, data->proto->iface.m_szModuleName, "Age", &dbv)) {
				SetDlgItemInt(hwndDlg, IDC_AGE, dbv.wVal, FALSE);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_AGE, _T(""));
			if (!DBGetContactSettingTString(data->hContact, data->proto->iface.m_szModuleName, "City", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_CITY, dbv.ptszVal);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_CITY, _T(""));
			if (!DBGetContactSettingTString(data->hContact, data->proto->iface.m_szModuleName, "School", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_SCHOOL, dbv.ptszVal);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_SCHOOL, _T(""));
			switch (DBGetContactSettingByte(data->hContact, data->proto->iface.m_szModuleName, "Gender", '?')) {
				case 'M':
					SendDlgItemMessage(hwndDlg, IDC_GENDER, CB_SETCURSEL, 1, 0);
					SetDlgItemText(hwndDlg, IDC_GENDER_TEXT, TranslateTS(tlenFieldGender[0].name));
					break;
				case 'F':
					SendDlgItemMessage(hwndDlg, IDC_GENDER, CB_SETCURSEL, 2, 0);
					SetDlgItemText(hwndDlg, IDC_GENDER_TEXT, TranslateTS(tlenFieldGender[1].name));
					break;
				default:
					SendDlgItemMessage(hwndDlg, IDC_GENDER, CB_SETCURSEL, 0, 0);
					SetDlgItemText(hwndDlg, IDC_GENDER_TEXT, _T(""));
					break;
			}
			i = DBGetContactSettingWord(data->hContact, data->proto->iface.m_szModuleName, "Occupation", 0);
			if (i>0 && i<13) {
				SetDlgItemText(hwndDlg, IDC_OCCUPATION_TEXT, TranslateTS(tlenFieldOccupation[i-1].name));
				SendDlgItemMessage(hwndDlg, IDC_OCCUPATION, CB_SETCURSEL, i, 0);
			} else {
				SetDlgItemText(hwndDlg, IDC_OCCUPATION_TEXT, _T(""));
				SendDlgItemMessage(hwndDlg, IDC_OCCUPATION, CB_SETCURSEL, 0, 0);
			}
			i = DBGetContactSettingWord(data->hContact, data->proto->iface.m_szModuleName, "LookingFor", 0);
			if (i>0 && i<6) {
				SetDlgItemText(hwndDlg, IDC_LOOKFOR_TEXT, TranslateTS(tlenFieldLookfor[i-1].name));
				SendDlgItemMessage(hwndDlg, IDC_LOOKFOR, CB_SETCURSEL, i, 0);
			} else {
				SetDlgItemText(hwndDlg, IDC_LOOKFOR_TEXT, _T(""));
				SendDlgItemMessage(hwndDlg, IDC_LOOKFOR, CB_SETCURSEL, 0, 0);
			}
			i = DBGetContactSettingWord(data->hContact, data->proto->iface.m_szModuleName, "VoiceChat", 0);
			CheckDlgButton(hwndDlg, IDC_VOICECONVERSATIONS, i);
			i = DBGetContactSettingWord(data->hContact, data->proto->iface.m_szModuleName, "PublicStatus", 0);
			CheckDlgButton(hwndDlg, IDC_PUBLICSTATUS, i);
			if (!DBGetContactSetting(data->hContact, data->proto->iface.m_szModuleName, "jid", &dbv)) {
				jid = JabberTextDecode(dbv.pszVal);
				SetDlgItemTextA(hwndDlg, IDC_INFO_JID, jid);
				mir_free(jid);
				jid = dbv.pszVal;
				if (data->proto->isOnline) {
					if ((item=JabberListGetItemPtr(data->proto, LIST_ROSTER, jid)) != NULL) {
						switch (item->subscription) {
						case SUB_BOTH:
							SetDlgItemText(hwndDlg, IDC_SUBSCRIPTION, TranslateT("both"));
							break;
						case SUB_TO:
							SetDlgItemText(hwndDlg, IDC_SUBSCRIPTION, TranslateT("to"));
							break;
						case SUB_FROM:
							SetDlgItemText(hwndDlg, IDC_SUBSCRIPTION, TranslateT("from"));
							break;
						default:
							SetDlgItemText(hwndDlg, IDC_SUBSCRIPTION, TranslateT("none"));
							break;
						}
						SetDlgItemTextA(hwndDlg, IDC_SOFTWARE, item->software);
						SetDlgItemTextA(hwndDlg, IDC_VERSION, item->version);
						SetDlgItemTextA(hwndDlg, IDC_SYSTEM, item->system);
					} else {
						SetDlgItemText(hwndDlg, IDC_SUBSCRIPTION, TranslateT("not on roster"));
					}
				}
				DBFreeVariant(&dbv);
			}
		}
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->idFrom) {
		case 0:
			switch (((LPNMHDR)lParam)->code) {
			case PSN_INFOCHANGED:
				{
					HANDLE hContact = (HANDLE) ((LPPSHNOTIFY) lParam)->lParam;
					SendMessage(hwndDlg, WM_TLEN_REFRESH, 0, (LPARAM) hContact);
				}
				break;
			case PSN_PARAMCHANGED:
				{
					data->proto = ( TlenProtocol* )(( LPPSHNOTIFY )lParam )->lParam;
					SendMessage(hwndDlg, WM_TLEN_REFRESH, 0, 0);
				}
			}
			break;
		}
		break;
	case WM_COMMAND:
		if (LOWORD(wParam)==IDC_SAVE && HIWORD(wParam)==BN_CLICKED) {
			char *str = NULL;
			int strSize;
			JabberStringAppend(&str, &strSize, "<iq type='set' id='"JABBER_IQID"%d' to='tuba'><query xmlns='jabber:iq:register'>", JabberSerialNext(data->proto));
			FetchField(hwndDlg, IDC_FIRSTNAME, "first", &str, &strSize);
			FetchField(hwndDlg, IDC_LASTNAME, "last", &str, &strSize);
			FetchField(hwndDlg, IDC_NICKNAME, "nick", &str, &strSize);
			FetchField(hwndDlg, IDC_EMAIL, "email", &str, &strSize);
			FetchCombo(hwndDlg, IDC_GENDER, "s", &str, &strSize);
			FetchField(hwndDlg, IDC_AGE, "b", &str, &strSize);
			FetchField(hwndDlg, IDC_CITY, "c", &str, &strSize);
			FetchCombo(hwndDlg, IDC_OCCUPATION, "j", &str, &strSize);
			FetchField(hwndDlg, IDC_SCHOOL, "e", &str, &strSize);
			FetchCombo(hwndDlg, IDC_LOOKFOR, "r", &str, &strSize);
			JabberStringAppend(&str, &strSize, "<g>%d</g>", IsDlgButtonChecked(hwndDlg, IDC_VOICECONVERSATIONS) ? 1 : 0);
			JabberStringAppend(&str, &strSize, "<v>%d</v>", IsDlgButtonChecked(hwndDlg, IDC_PUBLICSTATUS) ? 1 : 0);
			JabberStringAppend(&str, &strSize, "</query></iq>");
			JabberSend(data->proto, "%s", str);
			mir_free(str);
			JabberGetInfo((PROTO_INTERFACE *)data->proto, NULL, 0);
		}
		break;
    case WM_DESTROY:
        mir_free(data);
        break;
	}
	return FALSE;
}
