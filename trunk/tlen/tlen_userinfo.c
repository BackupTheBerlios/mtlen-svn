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
#include <m_avatars.h>
#include <commctrl.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "jabber_list.h"
#include "resource.h"
#include "tlen_avatar.h"

JABBER_FIELD_MAP tlenFieldGender[] = {
	{ 1, "Male" },
	{ 2, "Female" },
	{ 0, NULL }
};
JABBER_FIELD_MAP tlenFieldLookfor[] = {
	{ 1, "Somebody to talk" },
	{ 2, "Friendship" },
	{ 3, "Flirt/romance" },
	{ 4, "Love" },
	{ 5, "Nothing" },
	{ 0, NULL }
};
JABBER_FIELD_MAP tlenFieldStatus[] = {
	{ 1, "All" },
	{ 2, "Available" },
	{ 3, "Free for chat" },
	{ 0, NULL }
};
JABBER_FIELD_MAP tlenFieldOccupation[] = {
	{ 1, "Student" },
	{ 2, "College student" },
	{ 3, "Farmer" },
	{ 4, "Manager" },
	{ 5, "Specialist" },
	{ 6, "Clerk" },
	{ 7, "Unemployed" },
	{ 8, "Pensioner" },
	{ 9, "Housekeeper" },
	{ 10, "Teacher" },
	{ 11, "Doctor" },
	{ 12, "Other" },
	{ 0, NULL }
};
JABBER_FIELD_MAP tlenFieldPlan[] = {
	{ 1, "I'd like to go downtown" },
	{ 2, "I'd like to go to the cinema" },
	{ 3, "I'd like to take a walk" },
	{ 4, "I'd like to go to the disco" },
	{ 5, "I'd like to go on a blind date" },
	{ 6, "Waiting for suggestion" },
	{ 0, NULL }
};

HWND hAvatarDlg = NULL;

static BOOL CALLBACK TlenUserInfoDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK TlenSetAvatarDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

static void InitComboBox(HWND hwndCombo, JABBER_FIELD_MAP *fieldMap)
{
	int i, n;

	n = SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM) "");
	SendMessage(hwndCombo, CB_SETITEMDATA, n, 0);
	SendMessage(hwndCombo, CB_SETCURSEL, n, 0);
	for(i=0;;i++) {
		if (fieldMap[i].name == NULL)
			break;
		n = SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM) TranslateT(fieldMap[i].name));
		SendMessage(hwndCombo, CB_SETITEMDATA, n, fieldMap[i].id);
	}
}

static void FetchField(HWND hwndDlg, UINT idCtrl, char *fieldName, char **str, int *strSize)
{
	char text[512];
	char *localFieldName, *localText;

	if (hwndDlg==NULL || fieldName==NULL || str==NULL || strSize==NULL)
		return;
	GetDlgItemText(hwndDlg, idCtrl, text, sizeof(text));
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

int TlenUserInfoInit(WPARAM wParam, LPARAM lParam)
{
	char *szProto;
	HANDLE hContact;
	OPTIONSDIALOGPAGE odp = {0};

	if (!CallService(MS_PROTO_ISPROTOCOLLOADED, 0, (LPARAM) jabberProtoName))
		return 0;
	hContact = (HANDLE) lParam;
	szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
	if ((szProto!=NULL && !strcmp(szProto, jabberProtoName)) || !lParam) {
		odp.cbSize = sizeof(odp);
		odp.hIcon = NULL;
		odp.hInstance = hInst;
		odp.pfnDlgProc = TlenUserInfoDlgProc;
		odp.position = -2000000000;
		odp.pszTemplate = ((HANDLE)lParam!=NULL) ? MAKEINTRESOURCE(IDD_USER_INFO):MAKEINTRESOURCE(IDD_USER_VCARD);
		odp.pszTitle = jabberModuleName;
		CallService(MS_USERINFO_ADDPAGE, wParam, (LPARAM) &odp);

	}
	if (!lParam && jabberOnline) {
		CCSDATA ccs = {0};
		JabberGetInfo(0, (LPARAM) &ccs);
	}
	return 0;
}

static BOOL CALLBACK TlenUserInfoDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		// lParam is hContact
		TranslateDialogDefault(hwndDlg);
		SetWindowLong(hwndDlg, GWL_USERDATA, (LONG)(HANDLE) lParam);

		InitComboBox(GetDlgItem(hwndDlg, IDC_GENDER), tlenFieldGender);
		InitComboBox(GetDlgItem(hwndDlg, IDC_OCCUPATION), tlenFieldOccupation);
		InitComboBox(GetDlgItem(hwndDlg, IDC_LOOKFOR), tlenFieldLookfor);

		SendMessage(hwndDlg, WM_TLEN_REFRESH, 0, 0);
		return TRUE;
	case WM_TLEN_REFRESH:
		{
			DBVARIANT dbv;
			HANDLE hContact;
			char *jid;
			int i;
			JABBER_LIST_ITEM *item;

			SetDlgItemText(hwndDlg, IDC_INFO_JID, "");
			SetDlgItemText(hwndDlg, IDC_SUBSCRIPTION, "");
			SetFocus(GetDlgItem(hwndDlg, IDC_STATIC));

			hContact = (HANDLE) GetWindowLong(hwndDlg, GWL_USERDATA);
			if (!DBGetContactSetting(hContact, jabberProtoName, "FirstName", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_FIRSTNAME, dbv.pszVal);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_FIRSTNAME, "");
			if (!DBGetContactSetting(hContact, jabberProtoName, "LastName", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_LASTNAME, dbv.pszVal);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_LASTNAME, "");
			if (!DBGetContactSetting(hContact, jabberProtoName, "Nick", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_NICKNAME, dbv.pszVal);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_NICKNAME, "");
			if (!DBGetContactSetting(hContact, jabberProtoName, "e-mail", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_EMAIL, dbv.pszVal);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_EMAIL, "");
			if (!DBGetContactSetting(hContact, jabberProtoName, "Age", &dbv)) {
				SetDlgItemInt(hwndDlg, IDC_AGE, dbv.wVal, FALSE);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_AGE, "");
			if (!DBGetContactSetting(hContact, jabberProtoName, "City", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_CITY, dbv.pszVal);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_CITY, "");
			if (!DBGetContactSetting(hContact, jabberProtoName, "School", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_SCHOOL, dbv.pszVal);
				DBFreeVariant(&dbv);
			} else SetDlgItemText(hwndDlg, IDC_SCHOOL, "");
			switch (DBGetContactSettingByte(hContact, jabberProtoName, "Gender", '?')) {
				case 'M':
					SendDlgItemMessage(hwndDlg, IDC_GENDER, CB_SETCURSEL, 1, 0);
					SetDlgItemText(hwndDlg, IDC_GENDER_TEXT, TranslateT(tlenFieldGender[0].name));
					break;
				case 'F':
					SendDlgItemMessage(hwndDlg, IDC_GENDER, CB_SETCURSEL, 2, 0);
					SetDlgItemText(hwndDlg, IDC_GENDER_TEXT, TranslateT(tlenFieldGender[1].name));
					break;
				default:
					SendDlgItemMessage(hwndDlg, IDC_GENDER, CB_SETCURSEL, 0, 0);
					SetDlgItemText(hwndDlg, IDC_GENDER_TEXT, "");
					break;
			}
			i = DBGetContactSettingWord(hContact, jabberProtoName, "Occupation", 0);
			if (i>0 && i<13) {
				SetDlgItemText(hwndDlg, IDC_OCCUPATION_TEXT, TranslateT(tlenFieldOccupation[i-1].name));
				SendDlgItemMessage(hwndDlg, IDC_OCCUPATION, CB_SETCURSEL, i, 0);
			} else {
				SetDlgItemText(hwndDlg, IDC_OCCUPATION_TEXT, "");
				SendDlgItemMessage(hwndDlg, IDC_OCCUPATION, CB_SETCURSEL, 0, 0);
			}
			i = DBGetContactSettingWord(hContact, jabberProtoName, "LookingFor", 0);
			if (i>0 && i<6) {
				SetDlgItemText(hwndDlg, IDC_LOOKFOR_TEXT, TranslateT(tlenFieldLookfor[i-1].name));
				SendDlgItemMessage(hwndDlg, IDC_LOOKFOR, CB_SETCURSEL, i, 0);
			} else {
				SetDlgItemText(hwndDlg, IDC_LOOKFOR_TEXT, "");
				SendDlgItemMessage(hwndDlg, IDC_LOOKFOR, CB_SETCURSEL, 0, 0);
			}
			i = DBGetContactSettingWord(hContact, jabberProtoName, "VoiceChat", 0);
			CheckDlgButton(hwndDlg, IDC_VOICECONVERSATIONS, i);
			i = DBGetContactSettingWord(hContact, jabberProtoName, "PublicStatus", 0);
			CheckDlgButton(hwndDlg, IDC_PUBLICSTATUS, i);
			if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
				jid = JabberTextDecode(dbv.pszVal);
				SetDlgItemText(hwndDlg, IDC_INFO_JID, jid);
				mir_free(jid);
				jid = dbv.pszVal;
				if (jabberOnline) {
					if ((item=JabberListGetItemPtr(LIST_ROSTER, jid)) != NULL) {
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
			}
			break;
		}
		break;
	case WM_COMMAND:
		if (LOWORD(wParam)==IDC_SAVE && HIWORD(wParam)==BN_CLICKED) {
			char *str = NULL;
			int strSize;
			CCSDATA ccs = {0};
			JabberStringAppend(&str, &strSize, "<iq type='set' id='"JABBER_IQID"%d' to='tuba'><query xmlns='jabber:iq:register'>", JabberSerialNext());
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
			JabberSend(jabberThreadInfo->s, "%s", str);
			mir_free(str);
			JabberGetInfo(0, (LPARAM) &ccs);
		}
		break;
	}
	return FALSE;
}
