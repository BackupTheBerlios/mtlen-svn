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
#include <sys/types.h>
#include <sys/stat.h>
#include "resource.h"
#include "jabber_list.h"
#include "jabber_iq.h"
#include "tlen_file.h"

char *searchJID = NULL;

int JabberGetCaps(WPARAM wParam, LPARAM lParam)
{
	if (wParam == PFLAGNUM_1)
		return PF1_IM|PF1_AUTHREQ|PF1_SERVERCLIST|PF1_MODEMSG|PF1_BASICSEARCH|PF1_SEARCHBYEMAIL|PF1_EXTSEARCH|PF1_EXTSEARCHUI|PF1_SEARCHBYNAME|PF1_FILE|PF1_VISLIST|PF1_INVISLIST;
	if (wParam == PFLAGNUM_2)
		return PF2_ONLINE|PF2_INVISIBLE|PF2_SHORTAWAY|PF2_LONGAWAY|PF2_HEAVYDND|PF2_FREECHAT;
	if (wParam == PFLAGNUM_3)
		return PF2_ONLINE|PF2_INVISIBLE|PF2_SHORTAWAY|PF2_LONGAWAY|PF2_HEAVYDND|PF2_FREECHAT;
	if (wParam == PFLAGNUM_4)
		return PF4_FORCEAUTH|PF4_NOCUSTOMAUTH|PF4_SUPPORTTYPING;
	if (wParam == PFLAG_UNIQUEIDTEXT)
		return (int) Translate("Tlen login");
	if (wParam == PFLAG_UNIQUEIDSETTING)
		return (int) "jid";
	return 0;
}

int JabberGetName(WPARAM wParam, LPARAM lParam)
{
	lstrcpyn((char *) lParam, jabberModuleName, wParam);
	return 0;
}

int JabberLoadIcon(WPARAM wParam, LPARAM lParam)
{
	if ((wParam&0xffff) == PLI_PROTOCOL)
		return (int) LoadImage(hInst, MAKEINTRESOURCE(IDI_TLEN), IMAGE_ICON, GetSystemMetrics(wParam&PLIF_SMALL?SM_CXSMICON:SM_CXICON), GetSystemMetrics(wParam&PLIF_SMALL?SM_CYSMICON:SM_CYICON), 0);
	else
		return (int) (HICON) NULL;
}

int JabberBasicSearch(WPARAM wParam, LPARAM lParam)
{
	char *jid;
	int iqId;

	if (!jabberOnline) return 0;
	if ((char *) lParam == NULL) return 0;

	if ((jid=JabberTextEncode((char *) lParam)) != NULL) {
		iqId = JabberSerialNext();
		searchJID = _strdup((char *)lParam);
		JabberIqAdd(iqId, IQ_PROC_GETSEARCH, JabberIqResultSetSearch);
		JabberSend(jabberThreadInfo->s, "<iq type='get' id='"JABBER_IQID"%d' to='tuba'><query xmlns='jabber:iq:search'><i>%s</i></query></iq>", iqId, jid);
		free(jid);
		return iqId;
	}
	return 0;
}

int JabberSearchByEmail(WPARAM wParam, LPARAM lParam)
{
	char *email;
	int iqId;

	if (!jabberOnline) return 0;
	if ((char *) lParam == NULL) return 0;

	if ((email=JabberTextEncode((char *) lParam)) != NULL) {
		iqId = JabberSerialNext();
		JabberIqAdd(iqId, IQ_PROC_GETSEARCH, JabberIqResultSetSearch);
		JabberSend(jabberThreadInfo->s, "<iq type='get' id='"JABBER_IQID"%d' to='tuba'><query xmlns='jabber:iq:search'><email>%s</email></query></iq>", iqId, email);
		free(email);
		return iqId;
	}

	return 0;
}

int JabberSearchByName(WPARAM wParam, LPARAM lParam)
{
	PROTOSEARCHBYNAME *psbn = (PROTOSEARCHBYNAME *) lParam;
	char *p;
	char text[128];
	unsigned int size;
	int iqId;

	if (!jabberOnline) return 0;

	text[0] = text[sizeof(text)-1] = '\0';
	size = sizeof(text)-1;
	if (psbn->pszNick[0] != '\0') {
		if ((p=JabberTextEncode(psbn->pszNick)) != NULL) {
			if (strlen(p)+13 < size) {
				strcat(text, "<nick>");
				strcat(text, p);
				strcat(text, "</nick>");
				size = sizeof(text)-strlen(text)-1;
			}
			free(p);
		}
	}
	if (psbn->pszFirstName[0] != '\0') {
		if ((p=JabberTextEncode(psbn->pszFirstName)) != NULL) {
			if (strlen(p)+15 < size) {
				strcat(text, "<first>");
				strcat(text, p);
				strcat(text, "</first>");
				size = sizeof(text)-strlen(text)-1;
			}
			free(p);
		}
	}
	if (psbn->pszLastName[0] != '\0') {
		if ((p=JabberTextEncode(psbn->pszLastName)) != NULL) {
			if (strlen(p)+13 < size) {
				strcat(text, "<last>");
				strcat(text, p);
				strcat(text, "</last>");
				size = sizeof(text)-strlen(text)-1;
			}
			free(p);
		}
	}

	iqId = JabberSerialNext();
	JabberIqAdd(iqId, IQ_PROC_GETSEARCH, JabberIqResultSetSearch);
	JabberSend(jabberThreadInfo->s, "<iq type='get' id='"JABBER_IQID"%d' to='tuba'><query xmlns='jabber:iq:search'>%s</query></iq>", iqId, text);

	return iqId;
}

static HANDLE AddToListByJID(const char *newJid, DWORD flags)
{
	HANDLE hContact;
	char *jid, *nick;

	JabberLog("AddToListByJID jid=%s", newJid);

	if ((hContact=JabberHContactFromJID(newJid)) == NULL) {
		// not already there: add
		jid = _strdup(newJid); _strlwr(jid);
		JabberLog("Add new jid to contact jid=%s", jid);
		hContact = (HANDLE) CallService(MS_DB_CONTACT_ADD, 0, 0);
		CallService(MS_PROTO_ADDTOCONTACT, (WPARAM) hContact, (LPARAM) jabberProtoName);
		DBWriteContactSettingString(hContact, jabberProtoName, "jid", jid);
		if ((nick=JabberNickFromJID(newJid)) == NULL)
			nick = _strdup(newJid);
		DBWriteContactSettingString(hContact, "CList", "MyHandle", nick);
		free(nick);
		free(jid);

		// Note that by removing or disable the "NotOnList" will trigger
		// the plugin to add a particular contact to the roster list.
		// See DBSettingChanged hook at the bottom part of this source file.
		// But the add module will delete "NotOnList". So we will not do it here.
		// Also because we need "MyHandle" and "Group" info, which are set after
		// PS_ADDTOLIST is called but before the add dialog issue deletion of
		// "NotOnList".
		// If temporary add, "NotOnList" won't be deleted, and that's expected.
		DBWriteContactSettingByte(hContact, "CList", "NotOnList", 1);
		if (flags & PALF_TEMPORARY)
			DBWriteContactSettingByte(hContact, "CList", "Hidden", 1);
	}
	else {
		// already exist
		// Set up a dummy "NotOnList" when adding permanently only
		if (!(flags&PALF_TEMPORARY))
			DBWriteContactSettingByte(hContact, "CList", "NotOnList", 1);
	}

	return hContact;
}

int JabberAddToList(WPARAM wParam, LPARAM lParam)
{
	JABBER_SEARCH_RESULT *jsr = (JABBER_SEARCH_RESULT *) lParam;
	HANDLE hContact;
	char *jid;

	if (jsr->hdr.cbSize != sizeof(JABBER_SEARCH_RESULT))
		return (int) NULL;
	if ((jid=JabberUtf8Encode(jsr->jid)) == NULL)
		return (int) NULL;
	hContact = AddToListByJID(jid, wParam);	// wParam is flag e.g. PALF_TEMPORARY
	free(jid);
	return (int) hContact;
}

int JabberAddToListByEvent(WPARAM wParam, LPARAM lParam)
{
	DBEVENTINFO dbei;
	HANDLE hContact;
	char *nick, *firstName, *lastName, *jid;

	JabberLog("AddToListByEvent");
	ZeroMemory(&dbei, sizeof(dbei));
	dbei.cbSize = sizeof(dbei);
	if ((dbei.cbBlob=CallService(MS_DB_EVENT_GETBLOBSIZE, lParam, 0)) == (DWORD)(-1))
		return (int)(HANDLE) NULL;
	if ((dbei.pBlob=(PBYTE) malloc(dbei.cbBlob)) == NULL)
		return (int)(HANDLE) NULL;
	if (CallService(MS_DB_EVENT_GET, lParam, (LPARAM) &dbei)) {
		free(dbei.pBlob);
		return (int)(HANDLE) NULL;
	}
	if (strcmp(dbei.szModule, jabberProtoName)) {
		free(dbei.pBlob);
		return (int)(HANDLE) NULL;
	}

/*
	// EVENTTYPE_CONTACTS is when adding from when we receive contact list (not used in Jabber)
	// EVENTTYPE_ADDED is when adding from when we receive "You are added" (also not used in Jabber)
	// Jabber will only handle the case of EVENTTYPE_AUTHREQUEST
	// EVENTTYPE_AUTHREQUEST is when adding from the authorization request dialog
*/

	if (dbei.eventType != EVENTTYPE_AUTHREQUEST) {
		free(dbei.pBlob);
		return (int)(HANDLE) NULL;
	}

	nick = (char *) (dbei.pBlob + sizeof(DWORD) + sizeof(HANDLE));
	firstName = nick + strlen(nick) + 1;
	lastName = firstName + strlen(firstName) + 1;
	jid = lastName + strlen(lastName) + 1;

	hContact = (HANDLE) AddToListByJID(jid, wParam);
	free(dbei.pBlob);

	return (int) hContact;
}

int JabberAuthAllow(WPARAM wParam, LPARAM lParam)
{
	DBEVENTINFO dbei;
	char *nick, *firstName, *lastName, *jid;

	if (!jabberOnline)
		return 1;

	memset(&dbei, sizeof(dbei), 0);
	dbei.cbSize = sizeof(dbei);
	if ((dbei.cbBlob=CallService(MS_DB_EVENT_GETBLOBSIZE, wParam, 0)) == (DWORD)(-1))
		return 1;
	if ((dbei.pBlob=(PBYTE) malloc(dbei.cbBlob)) == NULL)
		return 1;
	if (CallService(MS_DB_EVENT_GET, wParam, (LPARAM) &dbei)) {
		free(dbei.pBlob);
		return 1;
	}
	if (dbei.eventType != EVENTTYPE_AUTHREQUEST) {
		free(dbei.pBlob);
		return 1;
	}
	if (strcmp(dbei.szModule, jabberProtoName)) {
		free(dbei.pBlob);
		return 1;
	}

	nick = (char *) (dbei.pBlob + sizeof(DWORD) + sizeof(HANDLE));
	firstName = nick + strlen(nick) + 1;
	lastName = firstName + strlen(firstName) + 1;
	jid = lastName + strlen(lastName) + 1;

	JabberLog("Send 'authorization allowed' to %s", jid);
	JabberSend(jabberThreadInfo->s, "<presence to='%s' type='subscribed'/>", jid);

	// Automatically add this user to my roster if option is enabled
	if (DBGetContactSettingByte(NULL, jabberProtoName, "AutoAdd", TRUE) == TRUE) {
		HANDLE hContact;
		JABBER_LIST_ITEM *item;

		if ((item=JabberListGetItemPtr(LIST_ROSTER, jid))==NULL || (item->subscription!=SUB_BOTH && item->subscription!=SUB_TO)) {
			JabberLog("Try adding contact automatically jid=%s", jid);
			if ((hContact=AddToListByJID(jid, 0)) != NULL) {
				// Trigger actual add by removing the "NotOnList" added by AddToListByJID()
				// See AddToListByJID() and JabberDbSettingChanged().
				DBDeleteContactSetting(hContact, "CList", "NotOnList");
			}
		}
	}

	free(dbei.pBlob);
	return 0;
}

int JabberAuthDeny(WPARAM wParam, LPARAM lParam)
{
	DBEVENTINFO dbei;
	char *nick, *firstName, *lastName, *jid;

	if (!jabberOnline)
		return 1;

	JabberLog("Entering AuthDeny");
	memset(&dbei, sizeof(dbei), 0);
	dbei.cbSize = sizeof(dbei);
	if ((dbei.cbBlob=CallService(MS_DB_EVENT_GETBLOBSIZE, wParam, 0)) == (DWORD)(-1))
		return 1;
	if ((dbei.pBlob=(PBYTE) malloc(dbei.cbBlob)) == NULL)
		return 1;
	if (CallService(MS_DB_EVENT_GET, wParam, (LPARAM) &dbei)) {
		free(dbei.pBlob);
		return 1;
	}
	if (dbei.eventType != EVENTTYPE_AUTHREQUEST) {
		free(dbei.pBlob);
		return 1;
	}
	if (strcmp(dbei.szModule, jabberProtoName)) {
		free(dbei.pBlob);
		return 1;
	}

	nick = (char *) (dbei.pBlob + sizeof(DWORD) + sizeof(HANDLE));
	firstName = nick + strlen(nick) + 1;
	lastName = firstName + strlen(firstName) + 1;
	jid = lastName + strlen(lastName) + 1;

	JabberLog("Send 'authorization denied' to %s", jid);
	JabberSend(jabberThreadInfo->s, "<presence to='%s' type='unsubscribed'/>", jid);
	JabberSend(jabberThreadInfo->s, "<iq type='set'><query xmlns='jabber:iq:roster'><item jid='%s' subscription='remove'/></query></iq>", jid);

	free(dbei.pBlob);
	return 0;
}

static void JabberConnect(int initialStatus)
{
	if (!jabberConnected) {
		struct ThreadData *thread;
		int oldStatus;

		thread = (struct ThreadData *) malloc(sizeof(struct ThreadData));
		memset(thread, 0, sizeof(struct ThreadData));
		thread->type = JABBER_SESSION_NORMAL;

		jabberDesiredStatus = initialStatus;

		oldStatus = jabberStatus;
		jabberStatus = ID_STATUS_CONNECTING;
		ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
		thread->hThread = (HANDLE) JabberForkThread((void (__cdecl *)(void*))JabberServerThread, 0, thread);
	}
}

int JabberSetStatus(WPARAM wParam, LPARAM lParam)
{
	int oldStatus, desiredStatus;
	HANDLE s;

	JabberLog("PS_SETSTATUS(%d)", wParam);
	desiredStatus = wParam;
	jabberDesiredStatus = desiredStatus;

 	if (desiredStatus == ID_STATUS_OFFLINE) {
		if (jabberThreadInfo) {
			if (jabberConnected) {
				JabberSendPresence(ID_STATUS_OFFLINE);
			}
			s = jabberThreadInfo->s;
			jabberThreadInfo = NULL;
			if (jabberConnected) {
				Sleep(200);
//				JabberSend(s, "</s>");
				// Force closing connection
#ifdef _DEBUG
				OutputDebugString("Force closing connection thread...");
#endif
				jabberConnected = FALSE;
				jabberOnline = FALSE;
				Netlib_CloseHandle(s);
			}
		}
		else {
			if (jabberStatus != ID_STATUS_OFFLINE) {
				oldStatus = jabberStatus;
				jabberStatus = jabberDesiredStatus = ID_STATUS_OFFLINE;
				ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
			}
		}
	}
	else if (desiredStatus != jabberStatus) {
		if (!jabberConnected)
			JabberConnect(desiredStatus);
		else {
			// change status
			oldStatus = jabberStatus;
			switch (desiredStatus) {
			case ID_STATUS_ONLINE:
			case ID_STATUS_NA:
			case ID_STATUS_FREECHAT:
			case ID_STATUS_INVISIBLE:
				jabberStatus = desiredStatus;
				break;
			case ID_STATUS_AWAY:
			case ID_STATUS_ONTHEPHONE:
			case ID_STATUS_OUTTOLUNCH:
				jabberStatus = ID_STATUS_AWAY;
				break;
			case ID_STATUS_DND:
			case ID_STATUS_OCCUPIED:
				jabberStatus = ID_STATUS_DND;
				break;
			default:
				return 1;
			}
			// send presence update
			JabberSendPresence(jabberStatus);
			ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
		}
	}
	return 0;
}

int JabberGetStatus(WPARAM wParam, LPARAM lParam)
{
	return jabberStatus;
}

int JabberSetAwayMsg(WPARAM wParam, LPARAM lParam)
{
	char **szMsg;
	char *newModeMsg;
	int desiredStatus;

	JabberLog("SetAwayMsg called, wParam=%d lParam=%s", wParam, (char *) lParam);

	desiredStatus = wParam;
	newModeMsg = JabberTextEncode((char *) lParam);
	
	EnterCriticalSection(&modeMsgMutex);

	switch (desiredStatus) {
	case ID_STATUS_ONLINE:
		szMsg = &modeMsgs.szOnline;
		break;
	case ID_STATUS_AWAY:
	case ID_STATUS_ONTHEPHONE:
	case ID_STATUS_OUTTOLUNCH:
		szMsg = &modeMsgs.szAway;
		break;
	case ID_STATUS_NA:
		szMsg = &modeMsgs.szNa;
		break;
	case ID_STATUS_DND:
	case ID_STATUS_OCCUPIED:
		szMsg = &modeMsgs.szDnd;
		break;
	case ID_STATUS_FREECHAT:
		szMsg = &modeMsgs.szFreechat;
		break;
	case ID_STATUS_INVISIBLE:
		szMsg = &modeMsgs.szInvisible;
		break;
	default:
		LeaveCriticalSection(&modeMsgMutex);
		return 1;
	}

	if ((*szMsg==NULL && newModeMsg==NULL) ||
		(*szMsg!=NULL && newModeMsg!=NULL && !strcmp(*szMsg, newModeMsg))) {
		// Message is the same, no update needed
		if (newModeMsg != NULL) free(newModeMsg);
	}
	else {
		// Update with the new mode message
		if (*szMsg != NULL) free(*szMsg);
		*szMsg = newModeMsg;
		// Send a presence update if needed
		if (desiredStatus == jabberStatus) {
			JabberSendPresence(jabberStatus);
		}
	}

	LeaveCriticalSection(&modeMsgMutex);
	return 0;
}

int JabberGetInfo(WPARAM wParam, LPARAM lParam)
{
	CCSDATA *ccs = (CCSDATA *) lParam;
	DBVARIANT dbv;
	int iqId;
	char *nick, *pNick;

	if (!jabberOnline) return 1;
	if (ccs->hContact==NULL) {
		iqId = JabberSerialNext();
		JabberIqAdd(iqId, IQ_PROC_NONE, TlenIqResultGetVcard);
		JabberSend(jabberThreadInfo->s, "<iq type='get' id='"JABBER_IQID"%d' to='tuba'><query xmlns='jabber:iq:register'></query></iq>", iqId);
	} else {
		if (DBGetContactSetting(ccs->hContact, jabberProtoName, "jid", &dbv)) return 1;
		if ((nick=JabberNickFromJID(dbv.pszVal)) != NULL) {
			if ((pNick=JabberTextEncode(nick)) != NULL) {
				iqId = JabberSerialNext();
				JabberIqAdd(iqId, IQ_PROC_NONE, TlenIqResultGetVcard);
				JabberSend(jabberThreadInfo->s, "<iq type='get' id='"JABBER_IQID"%d' to='tuba'><query xmlns='jabber:iq:search'><i>%s</i></query></iq>", iqId, pNick);
				free(pNick);
			}
			free(nick);
		}
		DBFreeVariant(&dbv);
	}
	JabberLog("hContact = %d", ccs->hContact);
	return 0;
}

int JabberSetApparentMode(WPARAM wParam, LPARAM lParam)
{
	CCSDATA *ccs = (CCSDATA *) lParam;
	DBVARIANT dbv;
	int oldMode;
	char *jid;

	if (ccs->wParam!=0 && ccs->wParam!=ID_STATUS_ONLINE && ccs->wParam!=ID_STATUS_OFFLINE) return 1;
	oldMode = DBGetContactSettingWord(ccs->hContact, jabberProtoName, "ApparentMode", 0);
	if ((int) ccs->wParam == oldMode) return 1;
	DBWriteContactSettingWord(ccs->hContact, jabberProtoName, "ApparentMode", (WORD) ccs->wParam);

	if (!jabberOnline) return 0;
	if (!DBGetContactSettingByte(NULL, jabberProtoName, "VisibilitySupport", FALSE)) return 0;
	if (!DBGetContactSetting(ccs->hContact, jabberProtoName, "jid", &dbv)) {
		jid = dbv.pszVal;
		switch (ccs->wParam) {
		case ID_STATUS_ONLINE:
			if (jabberStatus==ID_STATUS_INVISIBLE || oldMode==ID_STATUS_OFFLINE)
				JabberSend(jabberThreadInfo->s, "<presence to='%s'><show>available</show></presence>", jid);
			break;
		case ID_STATUS_OFFLINE:
			if (jabberStatus!=ID_STATUS_INVISIBLE || oldMode==ID_STATUS_ONLINE)
				JabberSend(jabberThreadInfo->s, "<presence to='%s' type='invisible'/>", jid);
			break;
		case 0:
			if (oldMode==ID_STATUS_ONLINE && jabberStatus==ID_STATUS_INVISIBLE)
				JabberSend(jabberThreadInfo->s, "<presence to='%s' type='invisible'/>", jid);
			else if (oldMode==ID_STATUS_OFFLINE && jabberStatus!=ID_STATUS_INVISIBLE)
				JabberSend(jabberThreadInfo->s, "<presence to='%s'><show>available</show></presence>", jid);
			break;
		}
		DBFreeVariant(&dbv);
	}

	// TODO: update the zebra list (jabber:iq:privacy)

	return 0;
}

static void __cdecl JabberSendMessageAckThread(HANDLE hContact)
{
	SleepEx(10, TRUE);
	JabberLog("Broadcast ACK");
	ProtoBroadcastAck(jabberProtoName, hContact, ACKTYPE_MESSAGE, ACKRESULT_SUCCESS, (HANDLE) 1, 0);
	JabberLog("Returning from thread");
}
/*
static void __cdecl JabberSendMessageFailedThread(HANDLE hContact)
{
	SleepEx(10, TRUE);
	JabberLog("Broadcast ACK");
	ProtoBroadcastAck(jabberProtoName, hContact, ACKTYPE_MESSAGE, ACKRESULT_FAILED, (HANDLE) 1, 0);
	JabberLog("Returning from thread");
}
*/

int JabberSendMessage(WPARAM wParam, LPARAM lParam)
{
	CCSDATA *ccs = (CCSDATA *) lParam;
	DBVARIANT dbv;
	char *msg;
	JABBER_LIST_ITEM *item;
	int id;
	char msgType[16];

	if (!jabberOnline || DBGetContactSetting(ccs->hContact, jabberProtoName, "jid", &dbv)) {
//		JabberForkThread(JabberSendMessageFailedThread, 0, (void *) ccs->hContact);
		ProtoBroadcastAck(jabberProtoName, ccs->hContact, ACKTYPE_MESSAGE, ACKRESULT_FAILED, (HANDLE) 1, 0);
		return 0;
	}

	if ((msg=JabberTextEncode((char *) ccs->lParam)) != NULL) {
		if (JabberListExist(LIST_CHATROOM, dbv.pszVal) && strchr(dbv.pszVal, '/')==NULL) {
			strcpy(msgType, "groupchat");
		} else if (DBGetContactSettingByte(ccs->hContact, jabberProtoName, "bChat", FALSE)) {
			strcpy(msgType, "privchat");
		} else {
			strcpy(msgType, "chat");
		}
		if (!strcmp(msgType, "groupchat") || DBGetContactSettingByte(NULL, jabberProtoName, "MsgAck", FALSE) == FALSE) {
			if (!strcmp(msgType, "groupchat")) {
				JabberSend(jabberThreadInfo->s, "<message to='%s' type='%s'><body>%s</body></message>", dbv.pszVal, msgType, msg);
			} else if (!strcmp(msgType, "privchat")) {
				JabberSend(jabberThreadInfo->s, "<m to='%s' n='6' s='10' f='0' c='000000'><b>%s</b></m>", dbv.pszVal, msg);
			} else {
				if (strcmp((const char *) ccs->lParam, "<alert>")) {
					id = JabberSerialNext();
					JabberSend(jabberThreadInfo->s, "<message to='%s' type='%s' id='"JABBER_IQID"%d'><body>%s</body><x xmlns='jabber:x:event'><composing/></x></message>", JabberGetClientJID(dbv.pszVal), msgType, id, msg);
				} else {
					JabberSend(jabberThreadInfo->s, "<m tp='a' to='%s'/>", JabberGetClientJID(dbv.pszVal));
				}
			}
			JabberForkThread(JabberSendMessageAckThread, 0, (void *) ccs->hContact);
		}
		else {
			id = JabberSerialNext();
			if ((item=JabberListGetItemPtr(LIST_ROSTER, dbv.pszVal)) != NULL)
				item->idMsgAckPending = id;
			JabberSend(jabberThreadInfo->s, "<message to='%s' type='%s' id='"JABBER_IQID"%d'><body>%s</body><x xmlns='jabber:x:event'><offline/><delivered/><composing/></x></message>", JabberGetClientJID(dbv.pszVal), msgType, id, msg);
		}
		free(msg);
	}
	DBFreeVariant(&dbv);
	return 1;
}

static void __cdecl JabberGetAwayMsgThread(HANDLE hContact)
{
	DBVARIANT dbv;
	JABBER_LIST_ITEM *item;
	JABBER_RESOURCE_STATUS *r;
	char *str;
	int i, len, msgCount;

	if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
		if ((item=JabberListGetItemPtr(LIST_ROSTER, dbv.pszVal)) != NULL) {
			DBFreeVariant(&dbv);
			if (item->resourceCount > 0) {
				JabberLog("resourceCount > 0");
				r = item->resource;
				len = msgCount = 0;
				for (i=0; i<item->resourceCount; i++) {
					if (r[i].statusMessage) {
						msgCount++;
						len += (strlen(r[i].resourceName) + strlen(r[i].statusMessage) + 6);
					}
				}
				if ((str=(char *) malloc(len + 1)) != NULL) {
					str[0] = str[len] = '\0';
					for (i=0; i<item->resourceCount; i++) {
						if (r[i].statusMessage) {
							if (str[0] != '\0') strcat(str, "\r\n");
							if (msgCount > 1) {
								strcat(str, "(");
								strcat(str, r[i].resourceName);
								strcat(str, "): ");
							}
							strcat(str, r[i].statusMessage);
						}
					}
					ProtoBroadcastAck(jabberProtoName, hContact, ACKTYPE_AWAYMSG, ACKRESULT_SUCCESS, (HANDLE) 1, (LPARAM) str);
					free(str);
					return;
				}
			}
			else if (item->statusMessage != NULL) {
				ProtoBroadcastAck(jabberProtoName, hContact, ACKTYPE_AWAYMSG, ACKRESULT_SUCCESS, (HANDLE) 1, (LPARAM) item->statusMessage);
				return;
			}
		}
		else {
			DBFreeVariant(&dbv);
		}
	}
	//ProtoBroadcastAck(jabberProtoName, hContact, ACKTYPE_AWAYMSG, ACKRESULT_FAILED, (HANDLE) 1, (LPARAM) 0);
	ProtoBroadcastAck(jabberProtoName, hContact, ACKTYPE_AWAYMSG, ACKRESULT_SUCCESS, (HANDLE) 1, (LPARAM) "");
}

int JabberGetAwayMsg(WPARAM wParam, LPARAM lParam)
{
	CCSDATA *ccs = (CCSDATA *) lParam;

	JabberLog("GetAwayMsg called, wParam=%d lParam=%d", wParam, lParam);
	JabberForkThread((void (__cdecl *)(void*))JabberGetAwayMsgThread, 0, (void *) ccs->hContact);
	return 1;
}

int JabberFileAllow(WPARAM wParam, LPARAM lParam)
{
	CCSDATA *ccs = (CCSDATA *) lParam;
	JABBER_FILE_TRANSFER *ft;
	JABBER_LIST_ITEM *item;
	char *nick;

	if (!jabberOnline) return 0;

	ft = (JABBER_FILE_TRANSFER *) ccs->wParam;
	ft->szSavePath = _strdup((char *) ccs->lParam);
	if ((item=JabberListAdd(LIST_FILE, ft->iqId)) != NULL) {
		item->ft = ft;
	}
	nick = JabberNickFromJID(ft->jid);
	JabberSend(jabberThreadInfo->s, "<f t='%s' i='%s' e='5' v='1'/>", nick, ft->iqId);
	free(nick);
	return ccs->wParam;
}

int JabberFileDeny(WPARAM wParam, LPARAM lParam)
{
	CCSDATA *ccs = (CCSDATA *) lParam;
	JABBER_FILE_TRANSFER *ft;
	char *nick;

	if (!jabberOnline) return 1;

	ft = (JABBER_FILE_TRANSFER *) ccs->wParam;
	nick = JabberNickFromJID(ft->jid);
	JabberSend(jabberThreadInfo->s, "<f i='%s' e='4' t='%s'/>", ft->iqId, nick);\
	free(nick);
	TlenFileFreeFt(ft);
	return 0;
}

int JabberFileCancel(WPARAM wParam, LPARAM lParam)
{
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
	}
	return 0;
}

int JabberSendFile(WPARAM wParam, LPARAM lParam)
{
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
	if (DBGetContactSettingWord(ccs->hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE) == ID_STATUS_OFFLINE) return 0;
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
}

int JabberRecvMessage(WPARAM wParam, LPARAM lParam)
{
	DBEVENTINFO dbei;
	CCSDATA *ccs = (CCSDATA *) lParam;
	PROTORECVEVENT *pre = (PROTORECVEVENT *) ccs->lParam;

	DBDeleteContactSetting(ccs->hContact, "CList", "Hidden");
	ZeroMemory(&dbei, sizeof(dbei));
	dbei.cbSize = sizeof(dbei);
	dbei.szModule = jabberProtoName;
	dbei.timestamp = pre->timestamp;
	dbei.flags = pre->flags&PREF_CREATEREAD?DBEF_READ:0;
	dbei.eventType = EVENTTYPE_MESSAGE;
	dbei.cbBlob = strlen(pre->szMessage) + 1;
	dbei.pBlob = (PBYTE) pre->szMessage;
	CallService(MS_DB_EVENT_ADD, (WPARAM) ccs->hContact, (LPARAM) &dbei);
	return 0;
}

int JabberRecvFile(WPARAM wParam, LPARAM lParam)
{
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
	return 0;
}

int JabberDbSettingChanged(WPARAM wParam, LPARAM lParam)
{
	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING *) lParam;

	// no action for hContact == NULL or when offline
	if ((HANDLE) wParam == NULL) return 0;
	if (!jabberConnected) return 0;

	if (!strcmp(cws->szModule, "CList")) {
		HANDLE hContact;
		DBVARIANT dbv;
		JABBER_LIST_ITEM *item;
		char *szProto, *nick, *jid, *group;

		hContact = (HANDLE) wParam;
		szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
		if (szProto==NULL || strcmp(szProto, jabberProtoName)) return 0;
//		if (DBGetContactSettingByte(hContact, jabberProtoName, "ChatRoom", 0) != 0) return 0;

		// A contact's group is changed
		if (!strcmp(cws->szSetting, "Group")) {
			if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
				if ((item=JabberListGetItemPtr(LIST_ROSTER, dbv.pszVal)) != NULL) {
					DBFreeVariant(&dbv);
					if (!DBGetContactSetting(hContact, "CList", "MyHandle", &dbv)) {
						nick = JabberTextEncode(dbv.pszVal);
						DBFreeVariant(&dbv);
					}
					else {
						nick = JabberNickFromJID(item->jid);
					}
					if (nick != NULL) {
						// Note: we need to compare with item->group to prevent infinite loop
						if (cws->value.type==DBVT_DELETED && item->group!=NULL) {
							JabberLog("Group set to nothing");
							JabberSend(jabberThreadInfo->s, "<iq type='set'><query xmlns='jabber:iq:roster'><item name='%s' jid='%s'></item></query></iq>", nick, item->jid);
						}
						else if (cws->value.type==DBVT_ASCIIZ && cws->value.pszVal!=NULL && (item->group==NULL || strcmp(cws->value.pszVal, item->group))) {
							JabberLog("Group set to %s", cws->value.pszVal);
							if ((group=JabberTextEncode(cws->value.pszVal)) != NULL) {
								JabberSend(jabberThreadInfo->s, "<iq type='set'><query xmlns='jabber:iq:roster'><item name='%s' jid='%s'><group>%s</group></item></query></iq>", nick, item->jid, group);
								free(group);
							} 
						}
						free(nick);
					}
				}
				else {
					DBFreeVariant(&dbv);
				}
			}
		}
		// A contact is renamed
		else if (!strcmp(cws->szSetting, "MyHandle")) {
			char *newNick;

//			hContact = (HANDLE) wParam;
//			szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
//			if (szProto==NULL || strcmp(szProto, jabberProtoName)) return 0;

			if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
				jid = dbv.pszVal;
				if ((item=JabberListGetItemPtr(LIST_ROSTER, dbv.pszVal)) != NULL) {
					if (cws->value.type == DBVT_DELETED)
						newNick = (char *) CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM) hContact, GCDNF_NOMYHANDLE);
					else if (cws->value.type==DBVT_ASCIIZ && cws->value.pszVal!=NULL)
						newNick = cws->value.pszVal;
					else
						newNick = NULL;
					// Note: we need to compare with item->nick to prevent infinite loop
					if (newNick!=NULL && (item->nick==NULL || (item->nick!=NULL && strcmp(item->nick, newNick)))) {
						if ((nick=JabberTextEncode(newNick)) != NULL) {
							JabberLog("Nick set to %s", newNick);
							if (item->group!=NULL && (group=JabberTextEncode(item->group))!=NULL) {
								JabberSend(jabberThreadInfo->s, "<iq type='set'><query xmlns='jabber:iq:roster'><item name='%s' jid='%s'><group>%s</group></item></query></iq>", nick, jid, group);
								free(group);
							}
							else
								JabberSend(jabberThreadInfo->s, "<iq type='set'><query xmlns='jabber:iq:roster'><item name='%s' jid='%s'></item></query></iq>", nick, jid);
							free(nick);
						}
					}
				}
				DBFreeVariant(&dbv);
			}
		}
		// A temporary contact has been added permanently
		else if (!strcmp(cws->szSetting, "NotOnList")) {
			char *jid, *nick, *pGroup;

			if (cws->value.type==DBVT_DELETED || (cws->value.type==DBVT_BYTE && cws->value.bVal==0)) {
				if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
					jid = _strdup(dbv.pszVal);
					DBFreeVariant(&dbv);
					JabberLog("Add %s permanently to list", jid);
					if (!DBGetContactSetting(hContact, "CList", "MyHandle", &dbv)) {
						nick = JabberTextEncode(dbv.pszVal); //Utf8Encode
						DBFreeVariant(&dbv);
					}
					else {
						nick = JabberNickFromJID(jid);
					}
					if (nick != NULL) {
						JabberLog("jid=%s nick=%s", jid, nick);
						if (!DBGetContactSetting(hContact, "CList", "Group", &dbv)) {
							if ((pGroup=JabberTextEncode(dbv.pszVal)) != NULL) {
								JabberSend(jabberThreadInfo->s, "<iq type='set'><query xmlns='jabber:iq:roster'><item name='%s' jid='%s'><group>%s</group></item></query></iq>", nick, jid, pGroup);
								JabberSend(jabberThreadInfo->s, "<presence to='%s' type='subscribe'/>", jid);
								free(pGroup);
							}
							DBFreeVariant(&dbv);
						}
						else {
							JabberSend(jabberThreadInfo->s, "<iq type='set'><query xmlns='jabber:iq:roster'><item name='%s' jid='%s'/></query></iq>", nick, jid);
							JabberSend(jabberThreadInfo->s, "<presence to='%s' type='subscribe'/>", jid);
						}
						free(nick);
						DBDeleteContactSetting(hContact, "CList", "Hidden");
					}
					free(jid);
				}
			}
		}
	}

	return 0;
}

int JabberContactDeleted(WPARAM wParam, LPARAM lParam)
{
	char *szProto;
	DBVARIANT dbv;

	if(!jabberOnline)	// should never happen
		return 0;
	szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, wParam, 0);
	if (szProto==NULL || strcmp(szProto, jabberProtoName))
		return 0;
	if (!DBGetContactSetting((HANDLE) wParam, jabberProtoName, "jid", &dbv)) {
		char *jid, *p, *q;

		jid = dbv.pszVal;
		if ((p=strchr(jid, '@')) != NULL) {
			if ((q=strchr(p, '/')) != NULL)
				*q = '\0';
		}
		if (JabberListExist(LIST_ROSTER, jid)) {
			// Remove from roster, server also handles the presence unsubscription process.
			JabberSend(jabberThreadInfo->s, "<iq type='set'><query xmlns='jabber:iq:roster'><item jid='%s' subscription='remove'/></query></iq>", jid);
		}

		DBFreeVariant(&dbv);
	}
	return 0;
}

int JabberCreateAdvSearchUI(WPARAM wParam, LPARAM lParam)
{
	return (int) CreateDialog(hInst, MAKEINTRESOURCE(IDD_ADVSEARCH), (HWND) lParam, TlenAdvSearchDlgProc);
}

int JabberSearchByAdvanced(WPARAM wParam, LPARAM lParam)
{
	char *queryStr;
	int iqId;

	if (!jabberOnline) return 0;

	iqId = JabberSerialNext();
	if ((queryStr=TlenAdvSearchCreateQuery((HWND) lParam, iqId)) != NULL) {
		JabberIqAdd(iqId, IQ_PROC_GETSEARCH, JabberIqResultSetSearch);
		JabberSend(jabberThreadInfo->s, "%s", queryStr);
		free(queryStr);
		return iqId;
	}

	return 0;
}

int JabberUserIsTyping(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact = (HANDLE) wParam;
	DBVARIANT dbv;
	JABBER_LIST_ITEM *item;

	if (!jabberOnline) return 0;
	if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
		if ((item=JabberListGetItemPtr(LIST_ROSTER, dbv.pszVal))!=NULL /*&& item->wantComposingEvent==TRUE*/) {
			switch (lParam) {
			case PROTOTYPE_SELFTYPING_OFF:
				JabberSend(jabberThreadInfo->s, "<m tp='u' to='%s'/>", JabberGetClientJID(dbv.pszVal));
				break;
			case PROTOTYPE_SELFTYPING_ON:
				JabberSend(jabberThreadInfo->s, "<m tp='t' to='%s'/>", JabberGetClientJID(dbv.pszVal));
				break;
			}
		}
		DBFreeVariant(&dbv);
	}
	return 0;
}

int JabberSvcInit(void)
{
	char s[128];

	hEventSettingChanged = HookEvent(ME_DB_CONTACT_SETTINGCHANGED, JabberDbSettingChanged);
	hEventContactDeleted = HookEvent(ME_DB_CONTACT_DELETED, JabberContactDeleted);

	sprintf(s, "%s%s", jabberProtoName, PS_GETCAPS);
	CreateServiceFunction(s, JabberGetCaps);

	sprintf(s, "%s%s", jabberProtoName, PS_GETNAME);
	CreateServiceFunction(s, JabberGetName);

	sprintf(s, "%s%s", jabberProtoName, PS_LOADICON);
	CreateServiceFunction(s, JabberLoadIcon);

	sprintf(s, "%s%s", jabberProtoName, PS_BASICSEARCH);
	CreateServiceFunction(s, JabberBasicSearch);

	sprintf(s, "%s%s", jabberProtoName, PS_SEARCHBYEMAIL);
	CreateServiceFunction(s, JabberSearchByEmail);

	sprintf(s, "%s%s", jabberProtoName, PS_SEARCHBYNAME);
	CreateServiceFunction(s, JabberSearchByName);

	sprintf(s, "%s%s", jabberProtoName, PS_CREATEADVSEARCHUI);
	CreateServiceFunction(s, JabberCreateAdvSearchUI);

	sprintf(s, "%s%s", jabberProtoName, PS_SEARCHBYADVANCED);
	CreateServiceFunction(s, JabberSearchByAdvanced);

	sprintf(s, "%s%s", jabberProtoName, PS_ADDTOLIST);
	CreateServiceFunction(s, JabberAddToList);

	sprintf(s, "%s%s", jabberProtoName, PS_ADDTOLISTBYEVENT);
	CreateServiceFunction(s, JabberAddToListByEvent);

	sprintf(s, "%s%s", jabberProtoName, PS_AUTHALLOW);
	CreateServiceFunction(s, JabberAuthAllow);

	sprintf(s, "%s%s", jabberProtoName, PS_AUTHDENY);
	CreateServiceFunction(s, JabberAuthDeny);

	sprintf(s, "%s%s", jabberProtoName, PS_SETSTATUS);
	CreateServiceFunction(s, JabberSetStatus);

	sprintf(s, "%s%s", jabberProtoName, PS_GETSTATUS);
	CreateServiceFunction(s, JabberGetStatus);

	sprintf(s, "%s%s", jabberProtoName, PS_SETAWAYMSG);
	CreateServiceFunction(s, JabberSetAwayMsg);

	sprintf(s, "%s%s", jabberProtoName, PSS_GETINFO);
	CreateServiceFunction(s, JabberGetInfo);

	sprintf(s, "%s%s", jabberProtoName, PSS_SETAPPARENTMODE);
	CreateServiceFunction(s, JabberSetApparentMode);

	sprintf(s, "%s%s", jabberProtoName, PSS_MESSAGE);
	CreateServiceFunction(s, JabberSendMessage);

	sprintf(s, "%s%s", jabberProtoName, PSS_GETAWAYMSG);
	CreateServiceFunction(s, JabberGetAwayMsg);

	sprintf(s, "%s%s", jabberProtoName, PSS_FILEALLOW);
	CreateServiceFunction(s, JabberFileAllow);

	sprintf(s, "%s%s", jabberProtoName, PSS_FILEDENY);
	CreateServiceFunction(s, JabberFileDeny);

	sprintf(s, "%s%s", jabberProtoName, PSS_FILECANCEL);
	CreateServiceFunction(s, JabberFileCancel);

	sprintf(s, "%s%s", jabberProtoName, PSS_FILE);
	CreateServiceFunction(s, JabberSendFile);

	sprintf(s, "%s%s", jabberProtoName, PSR_MESSAGE);
	CreateServiceFunction(s, JabberRecvMessage);

	sprintf(s, "%s%s", jabberProtoName, PSR_FILE);
	CreateServiceFunction(s, JabberRecvFile);

	sprintf(s, "%s%s", jabberProtoName, PSS_USERISTYPING);
	CreateServiceFunction(s, JabberUserIsTyping);

	return 0;
}

