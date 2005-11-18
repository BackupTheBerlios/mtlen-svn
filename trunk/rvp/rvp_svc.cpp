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

#include "rvp.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "resource.h"
#include "Utils.h"
#include "RVPUtils.h"

RVPImpl rvpimpl;

static char ntDomain[256];
static char ntUsername[256];
static char ntPassword[256];
static HANDLE hEventPasswdDlg;

static BOOL CALLBACK RVPPasswordDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	char text[128];
	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		wsprintf(text, "%s %s", Translate("Enter password for"), (char *) lParam);
		SetDlgItemText(hwndDlg, IDC_JID, text);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			GetDlgItemText(hwndDlg, IDC_PASSWORD, ntPassword, sizeof(ntPassword));
			//EndDialog(hwndDlg, (int) ntPassword);
			//return TRUE;
			// Fall through
		case IDCANCEL:
			//EndDialog(hwndDlg, 0);
			SetEvent(hEventPasswdDlg);
			DestroyWindow(hwndDlg);
			return TRUE;
		}
		break;
	}

	return FALSE;
}


static VOID CALLBACK RVPPasswordCreateDialogApcProc(DWORD param)
{
	CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_PASSWORD), NULL, RVPPasswordDlgProc, (LPARAM) param);
}

int startRVPClient() {
	DBVARIANT dbv;
	char jidStr[128];
	//char server[256];
	char username[256];
	int	result;
	/* prepare credentials here */
	if (!DBGetContactSetting(NULL, rvpProtoName, "LoginName", &dbv)) {
		/* set login */
		strncpy(username, dbv.pszVal, sizeof(username));
		username[sizeof(username)-1] = '\0';
		DBFreeVariant(&dbv);
	} else {
		return LOGINERR_BADUSERID;
	}
	DBWriteContactSettingString(NULL, rvpProtoName, "mseid", username);
/*
	if (!DBGetContactSetting(NULL, rvpProtoName, "LoginServer", &dbv)) {
		strncpy(server, dbv.pszVal, sizeof(server));
		server[sizeof(server)-1] = '\0';
		DBFreeVariant(&dbv);
	} else {
		return LOGINERR_NONETWORK;
	}
*/
	int ntFlags = 0;
	ntDomain[0] = ntUsername[0] = ntPassword[0] = '\0';
	if (DBGetContactSettingByte(NULL, rvpProtoName, "ManualConnect", TRUE) == TRUE) {
		if (!DBGetContactSetting(NULL, rvpProtoName, "NTDomain", &dbv)) {
			strncpy(ntDomain, dbv.pszVal, sizeof(ntDomain));
			DBFreeVariant(&dbv);
		}
		if (!DBGetContactSetting(NULL, rvpProtoName, "NTUsername", &dbv)) {
			strncpy(ntUsername, dbv.pszVal, sizeof(ntUsername));
			DBFreeVariant(&dbv);
		}
		if (DBGetContactSettingByte(NULL, rvpProtoName, "SavePassword", TRUE) == FALSE) {
			_snprintf(jidStr, sizeof(jidStr), "%s\\%s", ntDomain, ntUsername);
			// Ugly hack: continue logging on only the return value is &(ntPassword[0])
			// because if WM_QUIT while dialog box is still visible, p is returned with some
			// exit code which may not be NULL.
			// Should be better with modeless.
			ntPassword[0] = (char) -1;
			hEventPasswdDlg = CreateEvent(NULL, FALSE, FALSE, NULL);
			QueueUserAPC(RVPPasswordCreateDialogApcProc, hMainThread, (DWORD) jidStr);
			WaitForSingleObject(hEventPasswdDlg, INFINITE);
			CloseHandle(hEventPasswdDlg);
			//if ((p=(char *)DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_PASSWORD), NULL, JabberPasswordDlgProc, (LPARAM) jidStr)) != ntPassword) {
			if (ntPassword[0] == (char) -1) {
				return LOGINERR_BADUSERID;
			}
		} else {
			if (!DBGetContactSetting(NULL, rvpProtoName, "NTPassword", &dbv)) {
				CallService(MS_DB_CRYPT_DECODESTRING, strlen(dbv.pszVal)+1, (LPARAM) dbv.pszVal);
				strncpy(ntPassword, dbv.pszVal, sizeof(ntPassword));
				DBFreeVariant(&dbv);
			}
		}
		if (ntUsername[0]=='\0') {
			return LOGINERR_BADUSERID;
		}
		ntFlags = HTTPCredentials::FLAG_MANUAL_NTLM;
	}
	// User may change status to OFFLINE while we are connecting above
//	if (jabberDesiredStatus!=ID_STATUS_OFFLINE) {
		rvpimpl.setCredentials(new HTTPCredentials(ntDomain, ntUsername, ntPassword, ntFlags));
		/* Subscribe To RVP Services */
		if (DBGetContactSettingByte(NULL, rvpProtoName, "ManualServer", FALSE) == TRUE) {
			if (!DBGetContactSetting(NULL, rvpProtoName, "LoginServer", &dbv)) {
				result = rvpimpl.signIn(username, dbv.pszVal, jabberDesiredStatus);
				DBFreeVariant(&dbv);
			} else {
				result = rvpimpl.signIn(username, NULL, jabberDesiredStatus);
			}
		} else {
			result = rvpimpl.signIn(username, NULL, jabberDesiredStatus);
		}
		if (result) {
			if (result == -1) {
				return LOGINERR_NONETWORK;
			} else {
				return LOGINERR_BADUSERID;
			}
		}

		/* Subscribe to all contacts and refresh contacts' information */
		rvpimpl.subscribeAll();
//	}
	return 0;
}

void __cdecl SignInThread(void *vPtr)
{	int result;
	int oldStatus = ID_STATUS_OFFLINE;
	result = startRVPClient();
	if (result != 0) {
		char str [1024];
		sprintf (str, "RVP Error: %d", result);
		MessageBoxA(NULL, str, "RVP Error", MB_OK);
		oldStatus = jabberStatus;
		jabberStatus = ID_STATUS_OFFLINE;
		ProtoBroadcastAck(rvpProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
		ProtoBroadcastAck(rvpProtoName, NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, result);
	} else {
		jabberConnected = TRUE;
		jabberStatus = jabberDesiredStatus;
		ProtoBroadcastAck(rvpProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
	}
}

int RVPGetCaps(WPARAM wParam, LPARAM lParam)
{
	if (wParam == PFLAGNUM_1)
		return PF1_IM|PF1_BASICSEARCH|PF1_FILE;
	if (wParam == PFLAGNUM_2)
		return PF2_ONLINE|PF2_INVISIBLE|PF2_SHORTAWAY|PF2_LONGAWAY|PF2_LIGHTDND|PF2_OUTTOLUNCH|PF2_ONTHEPHONE;
	if (wParam == PFLAGNUM_3)
		return PF2_ONLINE|PF2_INVISIBLE|PF2_SHORTAWAY|PF2_LONGAWAY|PF2_LIGHTDND|PF2_OUTTOLUNCH|PF2_ONTHEPHONE;
	if (wParam == PFLAGNUM_4)
		return PF4_SUPPORTTYPING;
	if (wParam == PFLAG_UNIQUEIDTEXT)
		return (int) Translate("MS Exchange Login");
	if (wParam == PFLAG_UNIQUEIDSETTING)
		return (int) "mseid";
	return 0;
}

int RVPGetName(WPARAM wParam, LPARAM lParam)
{
	lstrcpyn((char *) lParam, rvpModuleName, wParam);
	return 0;
}

int RVPLoadIcon(WPARAM wParam, LPARAM lParam)
{
	if ((wParam&0xffff) == PLI_PROTOCOL)
		return (int) LoadImage(hInst, MAKEINTRESOURCE(IDI_RVP), IMAGE_ICON, GetSystemMetrics(wParam&PLIF_SMALL?SM_CXSMICON:SM_CXICON), GetSystemMetrics(wParam&PLIF_SMALL?SM_CYSMICON:SM_CYICON), 0);
	else
		return (int) (HICON) NULL;
}

int RVPBasicSearch(WPARAM wParam, LPARAM lParam)
{
	if ((char *) lParam == NULL) return 0;
	return rvpimpl.searchContact((char *) lParam);
}

int RVPSearchByEmail(WPARAM wParam, LPARAM lParam)
{
//	char *email;
//	if ((char *) lParam == NULL) return 0;

//	if ((email=JabberTextEncode((char *) lParam)) != NULL) {
//		iqId = JabberSerialNext();
//		free(email);
//		return iqId;
//	}

	return 0;
}

int RVPAddToList(WPARAM wParam, LPARAM lParam) {
	HANDLE hContact;
	RVP_SEARCH_RESULT *jsr = (RVP_SEARCH_RESULT *) lParam;
	if (jsr->hdr.cbSize != sizeof(RVP_SEARCH_RESULT)) return 0;
	if ((hContact=Utils::contactFromID(jsr->jid)) == NULL) {
		// not already there: add
		hContact = (HANDLE) CallService(MS_DB_CONTACT_ADD, 0, 0);
		CallService(MS_PROTO_ADDTOCONTACT, (WPARAM) hContact, (LPARAM) rvpProtoName);
		DBWriteContactSettingString(hContact, rvpProtoName, "mseid", jsr->jid);
		DBWriteContactSettingString(hContact, rvpProtoName, "Nick", jsr->hdr.nick);
		// Note that by removing or disable the "NotOnList" will trigger
		// the plugin to add a particular contact to the roster list.
		// See DBSettingChanged hook at the bottom part of this source file.
		// But the add module will delete "NotOnList". So we will not do it here.
		// Also because we need "MyHandle" and "Group" info, which are set after
		// PS_ADDTOLIST is called but before the add dialog issue deletion of
		// "NotOnList".
		// If temporary add, "NotOnList" won't be deleted, and that's expected.
		DBWriteContactSettingByte(hContact, "CList", "NotOnList", 1);
		if (wParam & PALF_TEMPORARY)
			DBWriteContactSettingByte(hContact, "CList", "Hidden", 1);
	} else {
		// already exist
		// Set up a dummy "NotOnList" when adding permanently only
		if (!(wParam&PALF_TEMPORARY))
			DBWriteContactSettingByte(hContact, "CList", "NotOnList", 1);
	}
	rvpimpl.subscribe(hContact);
	return (int) hContact;
}

int RVPSetStatus(WPARAM wParam, LPARAM lParam)
{
	int oldStatus, desiredStatus;
	desiredStatus = wParam;
	jabberDesiredStatus = desiredStatus;

 	if (desiredStatus == ID_STATUS_OFFLINE) {
		if (jabberConnected) {
			/* TODO move to signout thread*/
			rvpimpl.signOut();
			HANDLE hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
            while (hContact != NULL) {
               char *str = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
               if(str!=NULL && !strcmp(str, rvpProtoName)) {
                  if (DBGetContactSettingWord(hContact, rvpProtoName, "Status", ID_STATUS_OFFLINE) != ID_STATUS_OFFLINE) {
                     DBWriteContactSettingWord(hContact, rvpProtoName, "Status", ID_STATUS_OFFLINE);
                  }
               }
               hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
            }
			jabberConnected = FALSE;
		}
		if (jabberStatus != ID_STATUS_OFFLINE) {
			oldStatus = jabberStatus;
			jabberStatus = jabberDesiredStatus = ID_STATUS_OFFLINE;
			ProtoBroadcastAck(rvpProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
		}
	} else if (desiredStatus != jabberStatus) {
		if (!jabberConnected) {
			int oldStatus;
			jabberDesiredStatus = desiredStatus;
			oldStatus = jabberStatus;
			jabberStatus = ID_STATUS_CONNECTING;
			ProtoBroadcastAck(rvpProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
			Utils::forkThread((void (__cdecl *)(void*))SignInThread, 0, NULL);
		} else {
			// change status
			oldStatus = jabberStatus;
			jabberStatus = desiredStatus;
			// send presence update
			/* TODO move to send presence thread*/
			rvpimpl.setStatus(jabberStatus);
		}
	}
	return 0;
}

int RVPGetStatus(WPARAM wParam, LPARAM lParam)
{
	return jabberStatus;
}

int RVPGetInfo(WPARAM wParam, LPARAM lParam)
{
	/*
	TODO: subscribe here
	*/
	/*
	CCSDATA *ccs = (CCSDATA *) lParam;
	DBVARIANT dbv;
	int iqId;
	char *nick, *pNick;

	if (ccs->hContact==NULL) {
		iqId = JabberSerialNext();
	} else {
		if (DBGetContactSetting(ccs->hContact, rvpProtoName, "jid", &dbv)) return 1;
		if ((nick=JabberNickFromJID(dbv.pszVal)) != NULL) {
			if ((pNick=JabberTextEncode(nick)) != NULL) {
				iqId = JabberSerialNext();
				free(pNick);
			}
			free(nick);
		}
		DBFreeVariant(&dbv);
	}
	*/
	return 0;
}

int RVPSendMessage(WPARAM wParam, LPARAM lParam) {
	return rvpimpl.sendMessage((CCSDATA *) lParam);
}

int RVPRecvMessage(WPARAM wParam, LPARAM lParam)
{
	DBEVENTINFO dbei;
	CCSDATA *ccs = (CCSDATA *) lParam;
	PROTORECVEVENT *pre = (PROTORECVEVENT *) ccs->lParam;

	DBDeleteContactSetting(ccs->hContact, "CList", "Hidden");
	ZeroMemory(&dbei, sizeof(dbei));
	dbei.cbSize = sizeof(dbei);
	dbei.szModule = rvpProtoName;
	dbei.timestamp = pre->timestamp;
	dbei.flags = pre->flags&PREF_CREATEREAD?DBEF_READ:0;
	dbei.eventType = EVENTTYPE_MESSAGE;
	dbei.cbBlob = strlen(pre->szMessage) + 1;
	if ( pre->flags & PREF_UNICODE )
		dbei.cbBlob *= ( sizeof( wchar_t )+1 );
	dbei.pBlob = (PBYTE) pre->szMessage;
	CallService(MS_DB_EVENT_ADD, (WPARAM) ccs->hContact, (LPARAM) &dbei);
	return 0;
}

int RVPDbSettingChanged(WPARAM wParam, LPARAM lParam)
{
/*
	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING *) lParam;

	// no action for hContact == NULL or when offline
	if ((HANDLE) wParam == NULL) return 0;
	if (!jabberConnected) return 0;

	if (!strcmp(cws->szModule, "CList")) {
		HANDLE hContact;
		DBVARIANT dbv;
		char *szProto, *nick, *jid;
		hContact = (HANDLE) wParam;
		szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
		if (szProto==NULL || strcmp(szProto, rvpProtoName)) return 0;
	}
	*/
	return 0;
}


int RVPContactDeleted(WPARAM wParam, LPARAM lParam)
{
	char *szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, wParam, 0);
	if (szProto==NULL || strcmp(szProto, rvpProtoName)) return 0;
	rvpimpl.unsubscribe((HANDLE) wParam);
	return 0;
}

int RVPUserIsTyping(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact = (HANDLE) wParam;
	switch (lParam) {
	case PROTOTYPE_SELFTYPING_OFF:
		rvpimpl.sendTyping(hContact, false);
		break;
	case PROTOTYPE_SELFTYPING_ON:
		rvpimpl.sendTyping(hContact, true);
		break;
	}
	return 0;
}

int RVPFileAllow(WPARAM wParam, LPARAM lParam)
{
	CCSDATA *ccs = (CCSDATA *) lParam;
//	ft = (JABBER_FILE_TRANSFER *) ccs->wParam;
	rvpimpl.sendFileAccept("", (const char *) ccs->lParam);
	return ccs->wParam;
}

int RVPFileDeny(WPARAM wParam, LPARAM lParam)
{
	/*
	CCSDATA *ccs = (CCSDATA *) lParam;
	JABBER_FILE_TRANSFER *ft;
	char *nick;

	if (!jabberOnline) return 1;

	ft = (JABBER_FILE_TRANSFER *) ccs->wParam;
	nick = JabberNickFromJID(ft->jid);
	JabberSend(jabberThreadInfo->s, "<f i='%s' e='4' t='%s'/>", ft->iqId, nick);\
	free(nick);
	TlenFileFreeFt(ft);
	*/
	return 0;
}

int RVPFileCancel(WPARAM wParam, LPARAM lParam)
{/*
	CCSDATA *ccs = (CCSDATA *) lParam;
	JABBER_FILE_TRANSFER *ft = (JABBER_FILE_TRANSFER *) ccs->wParam;
	HANDLE hEvent;

	JabberLog("Invoking FileCancel()");
	if (ft->s) {
		JabberLog("FT canceled");
		//ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		JabberLog("Closing ft->s = %d", ft->s);
		ft->state = FT_ERROR;
		Netlib_CloseHandle(ft->s);
		ft->s = NULL;
		if (ft->hFileEvent != NULL) {
			hEvent = ft->hFileEvent;
			ft->hFileEvent = NULL;
			SetEvent(hEvent);
		}
		JabberLog("ft->s is now NULL, ft->state is now FT_ERROR");
	}*/
	return 0;
}

int RVPSendFile(WPARAM wParam, LPARAM lParam)
{
	/*
	CCSDATA *ccs = (CCSDATA *) lParam;
	char **files = (char **) ccs->lParam;
	JABBER_FILE_TRANSFER *ft;
	int i, j;
	struct _stat statbuf;
	DBVARIANT dbv;
	char *nick, *p, idStr[10];
	JABBER_LIST_ITEM *item;
	int id;

	if (!jabberOnline) return 0;
//	if (DBGetContactSettingWord(ccs->hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE) == ID_STATUS_OFFLINE) return 0;
	if (DBGetContactSetting(ccs->hContact, jabberProtoName, "jid", &dbv)) return 0;
	ft = (JABBER_FILE_TRANSFER *) malloc(sizeof(JABBER_FILE_TRANSFER));
	memset(ft, 0, sizeof(JABBER_FILE_TRANSFER));
	for(ft->fileCount=0; files[ft->fileCount]; ft->fileCount++);
	ft->files = (char **) malloc(sizeof(char *) * ft->fileCount);
	ft->filesSize = (long *) malloc(sizeof(long) * ft->fileCount);
	ft->allFileTotalSize = 0;
	for(i=j=0; i<ft->fileCount; i++) {
		if (_stat(files[i], &statbuf))
			JabberLog("'%s' is an invalid filename", files[i]);
		else {
			ft->filesSize[j] = statbuf.st_size;
			ft->files[j++] = _strdup(files[i]);
			ft->allFileTotalSize += statbuf.st_size;
		}
	}
	ft->fileCount = j;
	ft->szDescription = _strdup((char *) ccs->wParam);
	ft->hContact = ccs->hContact;
	ft->currentFile = 0;
	ft->jid = _strdup(dbv.pszVal);
	DBFreeVariant(&dbv);

	id = JabberSerialNext();
	_snprintf(idStr, sizeof(idStr), "%d", id);
	if ((item=JabberListAdd(LIST_FILE, idStr)) != NULL) {
		ft->iqId = _strdup(idStr);
		nick = JabberNickFromJID(ft->jid);
		item->ft = ft;
		if (ft->fileCount == 1) {
			if ((p=strrchr(files[0], '\\')) != NULL)
				p++;
			else
				p = files[0];
			p = JabberTextEncode(p);
			JabberSend(jabberThreadInfo->s, "<f t='%s' n='%s' e='1' i='%s' c='1' s='%d' v='1'/>", nick, p, idStr, ft->allFileTotalSize);
			free(p);
		}
		else
			JabberSend(jabberThreadInfo->s, "<f t='%s' e='1' i='%s' c='%d' s='%d' v='1'/>", nick, idStr, ft->fileCount, ft->allFileTotalSize);
		free(nick);
	}

	return (int)(HANDLE) ft;
	*/
	return 0;
}

int RVPRecvFile(WPARAM wParam, LPARAM lParam)
{
	/*
	DBEVENTINFO dbei;
	CCSDATA *ccs = (CCSDATA *) lParam;
	PROTORECVEVENT *pre = (PROTORECVEVENT *) ccs->lParam;
	char *szDesc, *szFile;

	DBDeleteContactSetting(ccs->hContact, "CList", "Hidden");
	szFile = pre->szMessage + sizeof(DWORD);
	szDesc = szFile + strlen(szFile) + 1;
	JabberLog("Description = %s", szDesc);
	ZeroMemory(&dbei, sizeof(dbei));
	dbei.cbSize = sizeof(dbei);
	dbei.szModule = jabberProtoName;
	dbei.timestamp = pre->timestamp;
	dbei.flags = pre->flags & (PREF_CREATEREAD ? DBEF_READ : 0);
	dbei.eventType = EVENTTYPE_FILE;
	dbei.cbBlob = sizeof(DWORD) + strlen(szFile) + strlen(szDesc) + 2;
	dbei.pBlob = (PBYTE) pre->szMessage;
	CallService(MS_DB_EVENT_ADD, (WPARAM) ccs->hContact, (LPARAM) &dbei);
	*/
	return 0;
}


int RVPSvcInit(void)
{
	char s[128];

	hEventSettingChanged = HookEvent(ME_DB_CONTACT_SETTINGCHANGED, RVPDbSettingChanged);
	hEventContactDeleted = HookEvent(ME_DB_CONTACT_DELETED, RVPContactDeleted);

	sprintf(s, "%s%s", rvpProtoName, PS_GETCAPS);
	CreateServiceFunction(s, RVPGetCaps);

	sprintf(s, "%s%s", rvpProtoName, PS_GETNAME);
	CreateServiceFunction(s, RVPGetName);

	sprintf(s, "%s%s", rvpProtoName, PS_LOADICON);
	CreateServiceFunction(s, RVPLoadIcon);

	sprintf(s, "%s%s", rvpProtoName, PS_BASICSEARCH);
	CreateServiceFunction(s, RVPBasicSearch);

	sprintf(s, "%s%s", rvpProtoName, PS_ADDTOLIST);
	CreateServiceFunction(s, RVPAddToList);

	sprintf(s, "%s%s", rvpProtoName, PS_SETSTATUS);
	CreateServiceFunction(s, RVPSetStatus);

	sprintf(s, "%s%s", rvpProtoName, PS_GETSTATUS);
	CreateServiceFunction(s, RVPGetStatus);

	sprintf(s, "%s%s", rvpProtoName, PSS_GETINFO);
	CreateServiceFunction(s, RVPGetInfo);

	sprintf(s, "%s%s", rvpProtoName, PSS_MESSAGE);
	CreateServiceFunction(s, RVPSendMessage);

	sprintf(s, "%s%s", rvpProtoName, PSR_MESSAGE);
	CreateServiceFunction(s, RVPRecvMessage);
	
	sprintf(s, "%s%s", rvpProtoName, PSS_FILEALLOW);
	CreateServiceFunction(s, RVPFileAllow);

	sprintf(s, "%s%s", rvpProtoName, PSS_FILEDENY);
	CreateServiceFunction(s, RVPFileDeny);

	sprintf(s, "%s%s", rvpProtoName, PSS_FILECANCEL);
	CreateServiceFunction(s, RVPFileCancel);

	sprintf(s, "%s%s", rvpProtoName, PSS_FILE);
	CreateServiceFunction(s, RVPSendFile);

	sprintf(s, "%s%s", rvpProtoName, PSR_FILE);
	CreateServiceFunction(s, RVPRecvFile);

	sprintf(s, "%s%s", rvpProtoName, PSS_USERISTYPING);
	CreateServiceFunction(s, RVPUserIsTyping);

	return 0;
}

