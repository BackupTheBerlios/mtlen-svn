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
#include "jabber_ssl.h"
#include "jabber_list.h"
#include "jabber_iq.h"
#include "resource.h"
#include "tlen_file.h"
#include "tlen_muc.h"
#include "tlen_voice.h"

//static void __cdecl TlenProcessInvitation(struct ThreadData *info);
static void __cdecl JabberKeepAliveThread(JABBER_SOCKET s);
static void JabberProcessStreamOpening(XmlNode *node, void *userdata);
static void JabberProcessStreamClosing(XmlNode *node, void *userdata);
static void JabberProcessProtocol(XmlNode *node, void *userdata);
static void JabberProcessMessage(XmlNode *node, void *userdata);
static void JabberProcessPresence(XmlNode *node, void *userdata);
static void JabberProcessIq(XmlNode *node, void *userdata);
static void TlenProcessF(XmlNode *node, void *userdata);
static void TlenProcessW(XmlNode *node, void *userdata);
static void TlenProcessM(XmlNode *node, void *userdata);
static void TlenProcessN(XmlNode *node, void *userdata);
static void TlenProcessP(XmlNode *node, void *userdata);
static void TlenProcessV(XmlNode *node, void *userdata);

static VOID CALLBACK JabberDummyApcFunc(DWORD param)
{
	return;
}

static char onlinePassword[128];
static HANDLE hEventPasswdDlg;

static BOOL CALLBACK JabberPasswordDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
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
			GetDlgItemText(hwndDlg, IDC_PASSWORD, onlinePassword, sizeof(onlinePassword));
			JabberLog("Password is %s", onlinePassword);
			//EndDialog(hwndDlg, (int) onlinePassword);
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

static VOID CALLBACK JabberPasswordCreateDialogApcProc(DWORD param)
{
	CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_PASSWORD), NULL, JabberPasswordDlgProc, (LPARAM) param);
}

void __cdecl JabberServerThread(struct ThreadData *info)
{
	DBVARIANT dbv;
	char jidStr[128];
	char *connectHost;
	char *buffer;
	int datalen;
	XmlState xmlState;
	HANDLE hContact;
	int jabberNetworkBufferSize;
	int socket;
	int oldStatus = ID_STATUS_OFFLINE;
	int reconnectMaxTime;
	int numRetry;
	int reconnectTime;
	char *str;
	CLISTMENUITEM clmi;
#ifndef TLEN_PLUGIN
	PVOID ssl;
	BOOL sslMode;
	char *szLogBuffer;
#endif

	JabberLog("Thread started: type=%d", info->type);

	if (info->type == JABBER_SESSION_NORMAL) {

		// Normal server connection, we will fetch all connection parameters
		// e.g. username, password, etc. from the database.

		if (jabberThreadInfo != NULL) {
			// Will not start another connection thread if a thread is already running.
			// Make APC call to the main thread. This will immediately wake the thread up
			// in case it is asleep in the reconnect loop so that it will immediately
			// reconnect.
			QueueUserAPC(JabberDummyApcFunc, jabberThreadInfo->hThread, 0);
			JabberLog("Thread ended, another normal thread is running");
			free(info);
			return;
		}

		jabberThreadInfo = info;
		if (streamId) free(streamId);
		streamId = NULL;

		if (!DBGetContactSetting(NULL, jabberProtoName, "LoginName", &dbv)) {
			strncpy(info->username, dbv.pszVal, sizeof(info->username));
			info->username[sizeof(info->username)-1] = '\0';
			DBFreeVariant(&dbv);
		}
		else {
			free(info);
			jabberThreadInfo = NULL;
			oldStatus = jabberStatus;
			jabberStatus = ID_STATUS_OFFLINE;
			ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
			ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, LOGINERR_BADUSERID);
			JabberLog("Thread ended, login name is not configured");
			return;
		}

		if (!DBGetContactSetting(NULL, jabberProtoName, "LoginServer", &dbv)) {
			strncpy(info->server, dbv.pszVal, sizeof(info->server));
			info->server[sizeof(info->server)-1] = '\0';
			DBFreeVariant(&dbv);
		}
		else {
			free(info);
			jabberThreadInfo = NULL;
			oldStatus = jabberStatus;
			jabberStatus = ID_STATUS_OFFLINE;
			ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
			ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, LOGINERR_NONETWORK);
			JabberLog("Thread ended, login server is not configured");
			return;
		}

		if (DBGetContactSettingByte(NULL, jabberProtoName, "SavePassword", TRUE) == FALSE) {
			_snprintf(jidStr, sizeof(jidStr), "%s@%s", info->username, info->server);
			// Ugly hack: continue logging on only the return value is &(onlinePassword[0])
			// because if WM_QUIT while dialog box is still visible, p is returned with some
			// exit code which may not be NULL.
			// Should be better with modeless.
			onlinePassword[0] = (char) -1;
			hEventPasswdDlg = CreateEvent(NULL, FALSE, FALSE, NULL);
			QueueUserAPC(JabberPasswordCreateDialogApcProc, hMainThread, (DWORD) jidStr);
			WaitForSingleObject(hEventPasswdDlg, INFINITE);
			CloseHandle(hEventPasswdDlg);
			//if ((p=(char *)DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_PASSWORD), NULL, JabberPasswordDlgProc, (LPARAM) jidStr)) != onlinePassword) {
			if (onlinePassword[0] == (char) -1) {
				free(info);
				jabberThreadInfo = NULL;
				oldStatus = jabberStatus;
				jabberStatus = ID_STATUS_OFFLINE;
				ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
				ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, LOGINERR_BADUSERID);
				JabberLog("Thread ended, password request dialog was canceled");
				return;
			}
			strncpy(info->password, onlinePassword, sizeof(info->password));
			info->password[sizeof(info->password)-1] = '\0';
		}
		else {
			if (DBGetContactSetting(NULL, jabberProtoName, "Password", &dbv)) {
				free(info);
				jabberThreadInfo = NULL;
				oldStatus = jabberStatus;
				jabberStatus = ID_STATUS_OFFLINE;
				ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
				ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, LOGINERR_BADUSERID);
				JabberLog("Thread ended, password is not configured");
				return;
			}
			CallService(MS_DB_CRYPT_DECODESTRING, strlen(dbv.pszVal)+1, (LPARAM) dbv.pszVal);
			strncpy(info->password, dbv.pszVal, sizeof(info->password));
			info->password[sizeof(info->password)-1] = '\0';
			DBFreeVariant(&dbv);
		}

		if (DBGetContactSettingByte(NULL, jabberProtoName, "ManualConnect", FALSE) == TRUE) {
			if (!DBGetContactSetting(NULL, jabberProtoName, "ManualHost", &dbv)) {
				strncpy(info->manualHost, dbv.pszVal, sizeof(info->manualHost));
				info->manualHost[sizeof(info->manualHost)-1] = '\0';
				DBFreeVariant(&dbv);
			}
			info->port = DBGetContactSettingWord(NULL, jabberProtoName, "ManualPort", TLEN_DEFAULT_PORT);
		}
		else {
			info->port = DBGetContactSettingWord(NULL, jabberProtoName, "Port", TLEN_DEFAULT_PORT);
		}

		info->useSSL = DBGetContactSettingByte(NULL, jabberProtoName, "UseSSL", FALSE);
	}

	else {
		JabberLog("Thread ended, invalid session type");
		free(info);
		return;
	}

	if (info->manualHost[0])
		connectHost = info->manualHost;
	else
		connectHost = info->server;

	JabberLog("Thread type=%d server='%s' port='%d'", info->type, connectHost, info->port);

	jabberNetworkBufferSize = 2048;
	if ((buffer=(char *) malloc(jabberNetworkBufferSize+1)) == NULL) {	// +1 is for '\0' when debug logging this buffer
		JabberLog("Cannot allocate network buffer, thread ended");
		if (info->type == JABBER_SESSION_NORMAL) {
			oldStatus = jabberStatus;
			jabberStatus = ID_STATUS_OFFLINE;
			ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, LOGINERR_NONETWORK);
			ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
			jabberThreadInfo = NULL;
		}
		else if (info->type == JABBER_SESSION_REGISTER) {
			SendMessage(info->reg_hwndDlg, WM_JABBER_REGDLG_UPDATE, 100, (LPARAM) Translate("Error: Not enough memory"));
		}
		free(info);
		JabberLog("Thread ended, network buffer cannot be allocated");
		return;
	}

	reconnectMaxTime = 10;
	numRetry = 0;

	for (;;) {	// Reconnect loop

		info->s = JabberWsConnect(connectHost, info->port);
		if (info->s == NULL) {
			JabberLog("Connection failed (%d)", WSAGetLastError());
			if (info->type == JABBER_SESSION_NORMAL) {
				if (jabberThreadInfo == info) {
					oldStatus = jabberStatus;
					jabberStatus = ID_STATUS_OFFLINE;
					ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, LOGINERR_NONETWORK);
					ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
					if (DBGetContactSettingByte(NULL, jabberProtoName, "Reconnect", FALSE) == TRUE) {
						reconnectTime = rand() % reconnectMaxTime;
						JabberLog("Sleeping %d seconds before automatic reconnecting...", reconnectTime);
						SleepEx(reconnectTime * 1000, TRUE);
						if (reconnectMaxTime < 10*60)	// Maximum is 10 minutes
							reconnectMaxTime *= 2;
						if (jabberThreadInfo == info) {	// Make sure this is still the active thread for the main Jabber connection
							JabberLog("Reconnecting to the network...");
							if (numRetry < MAX_CONNECT_RETRIES)
								numRetry++;
							oldStatus = jabberStatus;
							jabberStatus = ID_STATUS_CONNECTING + numRetry;
							ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
							continue;
						}
						else {
							JabberLog("Thread ended, connection failed");
							free(buffer);
							free(info);
							return;
						}
					}
					jabberThreadInfo = NULL;
				}
			}
			else if (info->type == JABBER_SESSION_REGISTER) {
				SendMessage(info->reg_hwndDlg, WM_JABBER_REGDLG_UPDATE, 100, (LPARAM) Translate("Error: Cannot connect to the server"));
			}
			JabberLog("Thread ended, connection failed");
			free(buffer);
			free(info);
			return;
		}

		// Determine local IP
		socket = CallService(MS_NETLIB_GETSOCKET, (WPARAM) info->s, 0);
		if (info->type==JABBER_SESSION_NORMAL && socket!=INVALID_SOCKET) {
			struct sockaddr_in saddr;
			int len;

			len = sizeof(saddr);
			getsockname(socket, (struct sockaddr *) &saddr, &len);
			jabberLocalIP = saddr.sin_addr.S_un.S_addr;
			JabberLog("Local IP = %s", inet_ntoa(saddr.sin_addr));
		}

#ifndef TLEN_PLUGIN
		sslMode = FALSE;
		if (info->useSSL) {
			JabberLog("Intializing SSL connection");
			if (hLibSSL!=NULL && socket!=INVALID_SOCKET) {
				JabberLog("SSL using socket = %d", socket);
				if ((ssl=pfn_SSL_new(jabberSslCtx)) != NULL) {
					JabberLog("SSL create context ok");
					if (pfn_SSL_set_fd(ssl, socket) > 0) {
						JabberLog("SSL set fd ok");
						if (pfn_SSL_connect(ssl) > 0) {
							JabberLog("SSL negotiation ok");
							JabberSslAddHandle(info->s, ssl);	// This make all communication on this handle use SSL
							sslMode = TRUE;		// Used in the receive loop below
							JabberLog("SSL enabled for handle = %d", info->s);
						}
						else {
							JabberLog("SSL negotiation failed");
							pfn_SSL_free(ssl);
						}
					}
					else {
						JabberLog("SSL set fd failed");
						pfn_SSL_free(ssl);
					}
				}
			}
			if (!sslMode) {
				if (info->type == JABBER_SESSION_NORMAL) {
					oldStatus = jabberStatus;
					jabberStatus = ID_STATUS_OFFLINE;
					ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
					ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, LOGINERR_NONETWORK);
					if (jabberThreadInfo == info)
						jabberThreadInfo = NULL;
				}
				else if (info->type == JABBER_SESSION_REGISTER) {
					SendMessage(info->reg_hwndDlg, WM_JABBER_REGDLG_UPDATE, 100, (LPARAM) Translate("Error: Cannot connect to the server"));
				}
				free(buffer);
				free(info);
				if (!hLibSSL)
					MessageBox(NULL, Translate("The connection requires an OpenSSL library, which is not installed."), Translate("Jabber Connection Error"), MB_OK|MB_ICONSTOP|MB_SETFOREGROUND);
				JabberLog("Thread ended, SSL connection failed");
				return;
			}
		}
#endif

		// User may change status to OFFLINE while we are connecting above
		if (jabberDesiredStatus!=ID_STATUS_OFFLINE || info->type==JABBER_SESSION_REGISTER) {

			if (info->type == JABBER_SESSION_NORMAL) {
				jabberConnected = TRUE;
				jabberJID = (char *) malloc(strlen(info->username)+strlen(info->server)+2);
				sprintf(jabberJID, "%s@%s", info->username, info->server);
				if (DBGetContactSettingByte(NULL, jabberProtoName, "KeepAlive", 1))
					jabberSendKeepAlive = TRUE;
				else
					jabberSendKeepAlive = FALSE;
				JabberForkThread(JabberKeepAliveThread, 0, info->s);
			}

			JabberXmlInitState(&xmlState);
			JabberXmlSetCallback(&xmlState, 1, ELEM_OPEN, JabberProcessStreamOpening, info);
			JabberXmlSetCallback(&xmlState, 1, ELEM_CLOSE, JabberProcessStreamClosing, info);
			JabberXmlSetCallback(&xmlState, 2, ELEM_CLOSE, JabberProcessProtocol, info);

			JabberSend(info->s, "<s v='3'>");

			JabberLog("Entering main recv loop");
			datalen = 0;

			for (;;) {
				int recvResult, bytesParsed;

#ifndef TLEN_PLUGIN
				if (sslMode) {
					JabberLog("Waiting for SSL data...");
					recvResult = pfn_SSL_read(ssl, buffer+datalen, jabberNetworkBufferSize-datalen);
				}
				else {
					JabberLog("Waiting for data...");
					recvResult = JabberWsRecv(info->s, buffer+datalen, jabberNetworkBufferSize-datalen);
				}
#else
				JabberLog("Waiting for data...");
				recvResult = JabberWsRecv(info->s, buffer+datalen, jabberNetworkBufferSize-datalen);
#endif

				JabberLog("recvResult = %d", recvResult);
				if (recvResult <= 0)
					break;
				datalen += recvResult;

				buffer[datalen] = '\0';
				JabberLog("RECV:%s", buffer);
#ifndef TLEN_PLUGIN
				if (sslMode && DBGetContactSettingByte(NULL, "Netlib", "DumpRecv", TRUE)==TRUE) {
					// Emulate netlib log feature for SSL connection
					if ((szLogBuffer=(char *)malloc(recvResult+128)) != NULL) {
						strcpy(szLogBuffer, "(SSL) Data received\n");
						memcpy(szLogBuffer+strlen(szLogBuffer), buffer+datalen-recvResult, recvResult+1 /* also copy \0 */);
						Netlib_Logf(hNetlibUser, "%s", szLogBuffer);	// %s to protect against when fmt tokens are in szLogBuffer causing crash
						free(szLogBuffer);
					}
				}
#endif

				bytesParsed = JabberXmlParse(&xmlState, buffer, datalen);
				JabberLog("bytesParsed = %d", bytesParsed);
				if (bytesParsed > 0) {
					if (bytesParsed < datalen)
						memmove(buffer, buffer+bytesParsed, datalen-bytesParsed);
					datalen -= bytesParsed;
				}
				else if (datalen == jabberNetworkBufferSize) {
					jabberNetworkBufferSize += 2048;
					JabberLog("Increasing network buffer size to %d", jabberNetworkBufferSize);
					if ((buffer=(char *) realloc(buffer, jabberNetworkBufferSize+1)) == NULL) {
						JabberLog("Cannot reallocate more network buffer, go offline now");
						break;
					}
				}
				else {
					JabberLog("Unknown state: bytesParsed=%d, datalen=%d, jabberNetworkBufferSize=%d", bytesParsed, datalen, jabberNetworkBufferSize);
				}
			}

			JabberXmlDestroyState(&xmlState);

			if (info->type == JABBER_SESSION_NORMAL) {
				jabberOnline = FALSE;
				jabberConnected = FALSE;
				
				memset(&clmi, 0, sizeof(CLISTMENUITEM));
				clmi.cbSize = sizeof(CLISTMENUITEM);
				clmi.flags = CMIM_FLAGS | CMIF_GRAYED;
				CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuMUC, (LPARAM) &clmi);
				CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuChats, (LPARAM) &clmi);
				
				// Set status to offline
				oldStatus = jabberStatus;
				jabberStatus = ID_STATUS_OFFLINE;
				ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);

				// Set all contacts to offline
				hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
				while (hContact != NULL) {
					str = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
					if(str!=NULL && !strcmp(str, jabberProtoName)) {
						if (DBGetContactSettingWord(hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE) != ID_STATUS_OFFLINE) {
							DBWriteContactSettingWord(hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE);
						}
					}
					hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
				}

				free(jabberJID);
				jabberJID = NULL;
				JabberListWipeSpecial();
			}
		}
		else {
			if (info->type == JABBER_SESSION_NORMAL) {
				oldStatus = jabberStatus;
				jabberStatus = ID_STATUS_OFFLINE;
				ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
			}
		}

		Netlib_CloseHandle(info->s);

#ifndef TLEN_PLUGIN
		if (sslMode) {
			pfn_SSL_free(ssl);
			JabberSslRemoveHandle(info->s);
		}
#endif

		if (info->type!=JABBER_SESSION_NORMAL || DBGetContactSettingByte(NULL, jabberProtoName, "Reconnect", FALSE)==FALSE)
			break;

		if (jabberThreadInfo != info)	// Make sure this is still the main Jabber connection thread
			break;
		reconnectTime = rand() % 10;
		JabberLog("Sleeping %d seconds before automatic reconnecting...", reconnectTime);
		SleepEx(reconnectTime * 1000, TRUE);
		reconnectMaxTime = 20;
		if (jabberThreadInfo != info)	// Make sure this is still the main Jabber connection thread
			break;
		JabberLog("Reconnecting to the network...");
		jabberDesiredStatus = oldStatus;	// Reconnect to my last status
		oldStatus = jabberStatus;
		jabberStatus = ID_STATUS_CONNECTING;
		numRetry = 1;
		ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
	}

	JabberLog("Thread ended: type=%d server='%s'", info->type, info->server);

	if (info->type==JABBER_SESSION_NORMAL && jabberThreadInfo==info) {
		if (streamId) free(streamId);
		streamId = NULL;
		jabberThreadInfo = NULL;
	}

	free(buffer);
	free(info);
	JabberLog("Exiting ServerThread");
}

static void JabberProcessStreamOpening(XmlNode *node, void *userdata)
{
	struct ThreadData *info = (struct ThreadData *) userdata;
	char *sid;
	int iqId;
	char *p;

	if (node->name==NULL || strcmp(node->name, "s"))
		return;

	if (info->type == JABBER_SESSION_NORMAL) {
		char *str;
		char text[128];

		if ((sid=JabberXmlGetAttrValue(node, "i")) != NULL) {
			if (streamId) free(streamId);
			streamId = _strdup(sid);
		}
		str = TlenPasswordHash(info->password);
		wsprintf(text, "%s%s", streamId, str);
		free(str);
		str = JabberSha1(text);
		if ((p=JabberTextEncode(info->username)) != NULL) {
			iqId = JabberSerialNext();
			JabberIqAdd(iqId, IQ_PROC_NONE, JabberIqResultSetAuth);
			JabberSend(info->s, "<iq type='set' id='"JABBER_IQID"%d'><query xmlns='jabber:iq:auth'><username>%s</username><digest>%s</digest><resource>t</resource></query></iq>", iqId, p /*info->username*/, str);
			free(p);
		}
		free(str);
	}
	else
		JabberSend(info->s, "</s>");

}

static void JabberProcessStreamClosing(XmlNode *node, void *userdata)
{
	struct ThreadData *info = (struct ThreadData *) userdata;

	Netlib_CloseHandle(info->s);
	if (node->name && !strcmp(node->name, "stream:error") && node->text)
		MessageBox(NULL, Translate(node->text), Translate("Jabber Connection Error"), MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
}

static void JabberProcessProtocol(XmlNode *node, void *userdata)
{
	struct ThreadData *info;

	//JabberXmlDumpNode(node);

	info = (struct ThreadData *) userdata;

	if (info->type == JABBER_SESSION_NORMAL) {
		if (!strcmp(node->name, "message"))
			JabberProcessMessage(node, userdata);
		else if (!strcmp(node->name, "presence"))
			JabberProcessPresence(node, userdata);
		else if (!strcmp(node->name, "iq"))
			JabberProcessIq(node, userdata);
		else if (!strcmp(node->name, "f"))
			TlenProcessF(node, userdata);
		else if (!strcmp(node->name, "w"))
			TlenProcessW(node, userdata);
		else if (!strcmp(node->name, "m"))
			TlenProcessM(node, userdata);
		else if (!strcmp(node->name, "n"))
			TlenProcessN(node, userdata);
		else if (!strcmp(node->name, "p"))
			TlenProcessP(node, userdata);
		else if (!strcmp(node->name, "v"))
			TlenProcessV(node, userdata);
		else
			JabberLog("Invalid top-level tag (only <message/> <presence/> <iq/> <f/> <w/> <m/> <n/> and <p/> allowed)");
	}

}

static void JabberProcessMessage(XmlNode *node, void *userdata)
{
	struct ThreadData *info;
	HANDLE hContact;
	CCSDATA ccs;
	PROTORECVEVENT recv;
	XmlNode *bodyNode, *subjectNode, *xNode, *inviteNode, *idNode, *n;
	char *from, *type, *nick, *p, *localMessage, *idStr, *fromResource;
	time_t msgTime;
	BOOL delivered, composing;
	int i, id;
	JABBER_LIST_ITEM *item;
	BOOL isChatRoomJid;
	BOOL isChatRoomInvitation;
	char *inviteRoomJid, *inviteFromJid, *inviteReason, *invitePassword;

	if (!node->name || strcmp(node->name, "message")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;

	if ((type=JabberXmlGetAttrValue(node, "type"))!=NULL && !strcmp(type, "error")) {
	}
	else {
		if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {

			isChatRoomJid = JabberListExist(LIST_CHATROOM, from);

			if (isChatRoomJid && type!=NULL && !strcmp(type, "groupchat"))
				; //JabberGroupchatProcessMessage(node, userdata);

			else {

				// If message is from a stranger (not in roster), item is NULL
				item = JabberListGetItemPtr(LIST_ROSTER, from);

				if ((bodyNode=JabberXmlGetChild(node, "body")) != NULL) {
					if (bodyNode->text != NULL) {
						if ((subjectNode=JabberXmlGetChild(node, "subject"))!=NULL && subjectNode->text!=NULL && subjectNode->text[0]!='\0') {
							p = (char *) malloc(strlen(subjectNode->text)+strlen(bodyNode->text)+12);
							sprintf(p, "Subject: %s\r\n%s", subjectNode->text, bodyNode->text);
							localMessage = JabberTextDecode(p);
							free(p);
						}
						else {
							localMessage = JabberTextDecode(bodyNode->text);
						}

						isChatRoomInvitation = FALSE;
						inviteRoomJid = NULL;
						inviteFromJid = NULL;
						inviteReason = NULL;
						invitePassword = NULL;
						msgTime = 0;
						delivered = composing = FALSE;
						i = 1;
						while ((xNode=JabberXmlGetNthChild(node, "x", i)) != NULL) {
							if ((p=JabberXmlGetAttrValue(xNode, "xmlns")) != NULL) {
								if (!strcmp(p, "jabber:x:delay") && msgTime==0) {
									if ((p=JabberXmlGetAttrValue(xNode, "stamp")) != NULL) {
										msgTime = JabberIsoToUnixTime(p);
									}
								}
								else if (!strcmp(p, "jabber:x:event")) {
									// Check whether any event is requested
									if (!delivered && (n=JabberXmlGetChild(xNode, "delivered"))!=NULL) {
										delivered = TRUE;
										idStr = JabberXmlGetAttrValue(node, "id");
										JabberSend(info->s, "<message to='%s'><x xmlns='jabber:x:event'><delivered/><id>%s</id></x></message>", from, (idStr!=NULL)?idStr:"");
									}
									if (item!=NULL && JabberXmlGetChild(xNode, "composing")!=NULL) {
										composing = TRUE;
										if (item->messageEventIdStr)
											free(item->messageEventIdStr);
										idStr = JabberXmlGetAttrValue(node, "id");
										item->messageEventIdStr = (idStr==NULL)?NULL:_strdup(idStr);
									}
								}
								else if (!strcmp(p, "http://jabber.org/protocol/muc#user")) {
									if ((inviteNode=JabberXmlGetChild(xNode, "invite")) != NULL) {
										inviteFromJid = JabberXmlGetAttrValue(inviteNode, "from");
										if ((n=JabberXmlGetChild(inviteNode, "reason")) != NULL) {
											inviteReason = n->text;
										}
									}
									if ((n=JabberXmlGetChild(xNode, "password")) != NULL) {
										invitePassword = n->text;
									}
								}
								else if (!strcmp(p, "jabber:x:conference")) {
									inviteRoomJid = JabberXmlGetAttrValue(xNode, "jid");
									if (inviteReason == NULL)
										inviteReason = xNode->text;
									isChatRoomInvitation = TRUE;
								}
							}
							i++;
						}

						if (isChatRoomInvitation) {
							if (inviteRoomJid != NULL) {
								; //JabberGroupchatProcessInvite(inviteRoomJid, inviteFromJid, inviteReason, invitePassword);
							}
						}
						else {

							if (item != NULL) {
								item->wantComposingEvent = composing;
								if ((hContact=JabberHContactFromJID(from)) != NULL)
									CallService(MS_PROTO_CONTACTISTYPING, (WPARAM) hContact, PROTOTYPE_CONTACTTYPING_OFF);

								if (item->resourceMode==RSMODE_LASTSEEN && (fromResource=strchr(from, '/'))!=NULL) {
									fromResource++;
									if (*fromResource != '\0') {
										for (i=0; i<item->resourceCount; i++) {
											if (!strcmp(item->resource[i].resourceName, fromResource)) {
												item->defaultResource = i;
												break;
											}
										}
									}
								}
							}

							if ((hContact=JabberHContactFromJID(from)) == NULL) {
								// Create a temporary contact
								if (isChatRoomJid) {
									if ((p=strchr(from, '/'))!=NULL && p[1]!='\0')
										p++;
									else
										p = from;
									nick = JabberTextEncode(p);
									hContact = JabberDBCreateContact(from, nick, TRUE, FALSE);
								}
								else {
									nick = JabberLocalNickFromJID(from);
									hContact = JabberDBCreateContact(from, nick, TRUE, TRUE);
								}	
								free(nick);
							}

							if (msgTime == 0) msgTime = time(NULL);
							recv.flags = 0;
							recv.timestamp = (DWORD) msgTime;
							recv.szMessage = localMessage;
							recv.lParam = 0;
							ccs.hContact = hContact;
							ccs.wParam = 0;
							ccs.szProtoService = PSR_MESSAGE;
							ccs.lParam = (LPARAM) &recv;
							CallService(MS_PROTO_CHAINRECV, 0, (LPARAM) &ccs);
						}

						free(localMessage);
					}
				}
				else {	// bodyNode==NULL - check for message event notification (ack, composing)
					if ((xNode=JabberXmlGetChild(node, "x")) != NULL) {
						if ((p=JabberXmlGetAttrValue(xNode, "xmlns"))!=NULL && !strcmp(p, "jabber:x:event")) {
							idNode = JabberXmlGetChild(xNode, "id");
							if (JabberXmlGetChild(xNode, "delivered")!=NULL ||
								JabberXmlGetChild(xNode, "offline")!=NULL) {

								id = -1;
								if (idNode!=NULL && idNode->text!=NULL) {
									if (!strncmp(idNode->text, JABBER_IQID, strlen(JABBER_IQID)))
										id = atoi((idNode->text)+strlen(JABBER_IQID));
								}
								if (id == item->idMsgAckPending) {
									ProtoBroadcastAck(jabberProtoName, JabberHContactFromJID(from), ACKTYPE_MESSAGE, ACKRESULT_SUCCESS, (HANDLE) 1, 0);
								}
							}
							if (JabberXmlGetChild(xNode, "composing") != NULL) {
								if ((hContact=JabberHContactFromJID(from)) != NULL) {
									CallService(MS_PROTO_CONTACTISTYPING, (WPARAM) hContact, PROTOTYPE_CONTACTTYPING_INFINITE);
								}
							}
							if (xNode->numChild==0 || (xNode->numChild==1 && idNode!=NULL)) {
								// Maybe a cancel to the previous composing
								if ((hContact=JabberHContactFromJID(from)) != NULL) {
									CallService(MS_PROTO_CONTACTISTYPING, (WPARAM) hContact, PROTOTYPE_CONTACTTYPING_OFF);
								}
							}
						}
					}
				}
			}
		}
	}
}

static void JabberProcessPresence(XmlNode *node, void *userdata)
{
	struct ThreadData *info;
	HANDLE hContact;
	XmlNode *showNode, *statusNode;
	JABBER_LIST_ITEM *item;
	JABBER_RESOURCE_STATUS *r;
	char *from, *type, *nick, *show;
	int status, count, i;
	char *p;

	if (!node || !node->name || strcmp(node->name, "presence")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;

	if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {

		if (JabberListExist(LIST_CHATROOM, from))
			; //JabberGroupchatProcessPresence(node, userdata);

		else {
			type = JabberXmlGetAttrValue(node, "type");

			if (type==NULL || (!strcmp(type, "available"))) {
				if ((nick=JabberLocalNickFromJID(from)) != NULL) {
					if ((hContact=JabberHContactFromJID(from)) == NULL)
						hContact = JabberDBCreateContact(from, nick, FALSE, TRUE);
					if (!JabberListExist(LIST_ROSTER, from)) {
						JabberLog("Receive presence online from %s (who is not in my roster)", from);
						JabberListAdd(LIST_ROSTER, from);
					}
					status = ID_STATUS_ONLINE;
					if ((showNode=JabberXmlGetChild(node, "show")) != NULL) {
						if ((show=showNode->text) != NULL) {
							if (!strcmp(show, "away")) status = ID_STATUS_AWAY;
							else if (!strcmp(show, "xa")) status = ID_STATUS_NA;
							else if (!strcmp(show, "dnd")) status = ID_STATUS_DND;
							else if (!strcmp(show, "chat")) status = ID_STATUS_FREECHAT;
							else if (!strcmp(show, "unavailable")) {
								// Always show invisible (on old Tlen client) as invisible (not offline)
								status = ID_STATUS_INVISIBLE;
							}
						}
					}
					if (status == ID_STATUS_OFFLINE)
						JabberListRemoveResource(LIST_ROSTER, from);
					else {
						// Send version query if this is the new resource
						if ((p=strchr(from, '@')) != NULL) {
							if ((p=strchr(p, '/'))!=NULL && p[1]!='\0') {
								p++;
								item = JabberListGetItemPtr(LIST_ROSTER, from);
								r = item->resource;
								for (i=0; i<item->resourceCount && strcmp(r[i].resourceName, p); i++);
								if (i >= item->resourceCount) {
									JabberSend(info->s, "<iq type='get' to='%s'><query xmlns='jabber:iq:version'/></iq>", from);
								}
							}
						}

						statusNode = JabberXmlGetChild(node, "status");
						if (statusNode)
							p = JabberTextDecode(statusNode->text);
						else
							p = NULL;
						JabberListAddResource(LIST_ROSTER, from, status, statusNode?p:NULL);
						if (p) free(p);
					}
					// Determine status to show for the contact
					if ((item=JabberListGetItemPtr(LIST_ROSTER, from)) != NULL) {
						count = item->resourceCount;
						for (i=0; i<count; i++)
							status = JabberCombineStatus(status, item->resource[i].status);
						item->status = status;
					}

					if (strchr(from, '@')!=NULL || DBGetContactSettingByte(NULL, jabberProtoName, "ShowTransport", TRUE)==TRUE) {
						if (DBGetContactSettingWord(hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE) != status)
							DBWriteContactSettingWord(hContact, jabberProtoName, "Status", (WORD) status);
					}
					JabberLog("%s (%s) online, set contact status to %d", nick, from, status);
					free(nick);
				}
			}
			else if (!strcmp(type, "unavailable")) {
				if (!JabberListExist(LIST_ROSTER, from)) {
					JabberLog("Receive presence offline from %s (who is not in my roster)", from);
					JabberListAdd(LIST_ROSTER, from);
				}
				else {
					JabberListRemoveResource(LIST_ROSTER, from);
				}
				status = ID_STATUS_OFFLINE;
				statusNode = JabberXmlGetChild(node, "status");
				if (statusNode) {
					if (DBGetContactSettingByte(NULL, jabberProtoName, "OfflineAsInvisible", FALSE)==TRUE) {
						status = ID_STATUS_INVISIBLE;
					}
					p = JabberTextDecode(statusNode->text);
					JabberListAddResource(LIST_ROSTER, from, status, p);
					if (p) free(p);
				} 
				if ((item=JabberListGetItemPtr(LIST_ROSTER, from)) != NULL) {
					// Determine status to show for the contact based on the remaining resources
					count = item->resourceCount;
					for (i=0; i<count; i++)
						status = JabberCombineStatus(status, item->resource[i].status);
					item->status = status;
				}
				if ((hContact=JabberHContactFromJID(from)) != NULL) {
					if (strchr(from, '@')!=NULL || DBGetContactSettingByte(NULL, jabberProtoName, "ShowTransport", TRUE)==TRUE) {
						if (DBGetContactSettingWord(hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE) != status)
							DBWriteContactSettingWord(hContact, jabberProtoName, "Status", (WORD) status);
					}
					CallService(MS_PROTO_CONTACTISTYPING, (WPARAM) hContact, PROTOTYPE_CONTACTTYPING_OFF);
					JabberLog("%s offline, set contact status to %d", from, status);
				}
			}
			else if (!strcmp(type, "subscribe")) {
				if (strchr(from, '@') == NULL) {
					// automatically send authorization allowed to agent/transport
					JabberSend(info->s, "<presence to='%s' type='subscribed'/>", from);
				}
				else if ((nick=JabberNickFromJID(from)) != NULL) {
					JabberLog("%s (%s) requests authorization", nick, from);
					JabberDBAddAuthRequest(from, nick);
					free(nick);
				}
			}
			else if (!strcmp(type, "subscribed")) {
				if ((item=JabberListGetItemPtr(LIST_ROSTER, from)) != NULL) {
					if (item->subscription == SUB_FROM) item->subscription = SUB_BOTH;
					else if (item->subscription == SUB_NONE) {
						item->subscription = SUB_TO;
					}
				}
			}
		}
	}
}

static void JabberProcessIq(XmlNode *node, void *userdata)
{
	struct ThreadData *info;
	HANDLE hContact;
	XmlNode *queryNode, *n;
	char *type, *jid, *nick;
	char *xmlns;
	char *idStr, *str, *p;
	int id;
	int i;
	JABBER_IQ_PFUNC pfunc;

	if (!node->name || strcmp(node->name, "iq")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;
	if ((type=JabberXmlGetAttrValue(node, "type")) == NULL) return;

	id = -1;
	if ((idStr=JabberXmlGetAttrValue(node, "id")) != NULL) {
		if (!strncmp(idStr, JABBER_IQID, strlen(JABBER_IQID)))
			id = atoi(idStr+strlen(JABBER_IQID));
	}

	queryNode = JabberXmlGetChild(node, "query");
	xmlns = JabberXmlGetAttrValue(queryNode, "xmlns");

	/////////////////////////////////////////////////////////////////////////
	// MATCH BY ID
	/////////////////////////////////////////////////////////////////////////

	if ((pfunc=JabberIqFetchFunc(id)) != NULL) {
		JabberLog("Handling iq request for id=%d", id);
		pfunc(node, userdata);
	}

	/////////////////////////////////////////////////////////////////////////
	// MORE GENERAL ROUTINES, WHEN ID DOES NOT MATCH
	/////////////////////////////////////////////////////////////////////////
/*
	else if ((pfunc=JabberIqFetchXmlnsFunc(xmlns)) != NULL) {
		JabberLog("Handling iq request for xmlns=%s", xmlns);
		pfunc(node, userdata);
	}
*/
	// RECVED: <iq type='set'><query ...
	else if (!strcmp(type, "set") && queryNode!=NULL && (xmlns=JabberXmlGetAttrValue(queryNode, "xmlns"))!=NULL) {

		// RECVED: roster push
		// ACTION: similar to iqIdGetRoster above
		if (!strcmp(xmlns, "jabber:iq:roster")) {
			XmlNode *itemNode, *groupNode;
			JABBER_LIST_ITEM *item;
			char *name;

			JabberLog("<iq/> Got roster push, query has %d children", queryNode->numChild);
			for (i=0; i<queryNode->numChild; i++) {
				itemNode = queryNode->child[i];
				if (!strcmp(itemNode->name, "item")) {
					if ((jid=JabberXmlGetAttrValue(itemNode, "jid")) != NULL) {
						if ((str=JabberXmlGetAttrValue(itemNode, "subscription")) != NULL) {
							// we will not add new account when subscription=remove
							if (!strcmp(str, "to") || !strcmp(str, "both") || !strcmp(str, "from") || !strcmp(str, "none")) {
								if ((name=JabberXmlGetAttrValue(itemNode, "name")) != NULL) {
									nick = JabberTextDecode(name);
								} else {
									nick = JabberLocalNickFromJID(jid);
								}
								if (nick != NULL) {
									if ((item=JabberListAdd(LIST_ROSTER, jid)) != NULL) {
										if (item->nick) free(item->nick);
										item->nick = nick;
										if ((hContact=JabberHContactFromJID(jid)) == NULL) {
											// Received roster has a new JID.
											// Add the jid (with empty resource) to Miranda contact list.
											hContact = JabberDBCreateContact(jid, nick, FALSE, TRUE);
										}
										else
											DBWriteContactSettingString(hContact, jabberProtoName, "jid", jid);
										DBWriteContactSettingString(hContact, "CList", "MyHandle", nick);
										if (item->group) free(item->group);
										if ((groupNode=JabberXmlGetChild(itemNode, "group"))!=NULL && groupNode->text!=NULL) {
											item->group = JabberTextDecode(groupNode->text);
											JabberContactListCreateGroup(item->group);
											DBWriteContactSettingString(hContact, "CList", "Group", item->group);
										}
										else {
											item->group = NULL;
											DBDeleteContactSetting(hContact, "CList", "Group");
										}
										if (!strcmp(str, "none") || (!strcmp(str, "from") && strchr(jid, '@')!=NULL)) {
											if (DBGetContactSettingWord(hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE) != ID_STATUS_OFFLINE)
												DBWriteContactSettingWord(hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE);
										}
									}
									else {
										free(nick);
									}
								}
							}
							if ((item=JabberListGetItemPtr(LIST_ROSTER, jid)) != NULL) {
								if (!strcmp(str, "both")) item->subscription = SUB_BOTH;
								else if (!strcmp(str, "to")) item->subscription = SUB_TO;
								else if (!strcmp(str, "from")) item->subscription = SUB_FROM;
								else item->subscription = SUB_NONE;
								JabberLog("Roster push for jid=%s, set subscription to %s", jid, str);
								// subscription = remove is to remove from roster list
								// but we will just set the contact to offline and not actually
								// remove, so that history will be retained.
								if (!strcmp(str, "remove")) {
									if ((hContact=JabberHContactFromJID(jid)) != NULL) {
										if (DBGetContactSettingWord(hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE) != ID_STATUS_OFFLINE)
											DBWriteContactSettingWord(hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE);
										JabberListRemove(LIST_ROSTER, jid);
									}
								}
							}
						}
					}
				}
			}
		}

	}
	// RECVED: <iq type='get'><query ...
	else if (!strcmp(type, "get") && queryNode!=NULL && (xmlns=JabberXmlGetAttrValue(queryNode, "xmlns"))!=NULL) {

		// RECVED: software version query
		// ACTION: return my software version
		if (!strcmp(xmlns, "jabber:iq:version")) {
			char *from, *resultId, *version, *os;
			OSVERSIONINFO osvi;
			char mversion[64];

			if ((from=JabberXmlGetAttrValue(node, "from"))!=NULL) {
				str = JabberGetVersionText();
				version = JabberTextEncode(str);
				os = NULL;
				memset(&osvi, 0, sizeof(OSVERSIONINFO));
				osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
				if (GetVersionEx(&osvi)) {
					switch (osvi.dwPlatformId) {
					case VER_PLATFORM_WIN32_NT:
						if (osvi.dwMajorVersion == 5) {
							if (osvi.dwMinorVersion == 2) os = JabberTextEncode(Translate("Windows Server 2003"));
							else if (osvi.dwMinorVersion == 1) os = JabberTextEncode(Translate("Windows XP"));
							else if (osvi.dwMinorVersion == 0) os = JabberTextEncode(Translate("Windows 2000"));
						}
						else if (osvi.dwMajorVersion <= 4) {
							os = JabberTextEncode(Translate("Windows NT"));
						}
						break;
					case VER_PLATFORM_WIN32_WINDOWS:
						if (osvi.dwMajorVersion == 4) {
							if (osvi.dwMinorVersion == 0) os = JabberTextEncode(Translate("Windows 95"));
							if (osvi.dwMinorVersion == 10) os = JabberTextEncode(Translate("Windows 98"));
							if (osvi.dwMinorVersion == 90) os = JabberTextEncode(Translate("Windows ME"));
						}
						break;
					}
				}
				if (os == NULL) os = JabberTextEncode(Translate("Windows"));
				CallService(MS_SYSTEM_GETVERSIONTEXT, sizeof(mversion), (LPARAM) mversion);
				if ((resultId=JabberTextEncode(idStr)) != NULL) {
					JabberSend(jabberThreadInfo->s, "<iq type='result' id='%s' to='%s'><query xmlns='jabber:iq:version'><name>Tlen+Protocol+Plugin+(Miranda+IM+%s)</name><version>%s</version><os>%s</os></query></iq>", resultId, from, mversion, version?version:"", os?os:"");
					free(resultId);
				}
				else {
					JabberSend(jabberThreadInfo->s, "<iq type='result' to='%s'><query xmlns='jabber:iq:version'><name>Tlen+Protocol+Plugin+(Miranda+IM+%s)</name><version>%s</version><os>%s</os></query></iq>", from, mversion, version?version:"", os?os:"");
				}
				if (str) free(str);
				if (version) free(version);
				if (os) free(os);
			}
		}
	}
	// RECVED: <iq type='result'><query ...
	else if (!strcmp(type, "result") && queryNode!=NULL && (xmlns=JabberXmlGetAttrValue(queryNode, "xmlns"))!=NULL) {

		// RECVED: software version result
		// ACTION: update version information for the specified jid/resource
		if (!strcmp(xmlns, "jabber:iq:version")) {
			char *from;
			JABBER_LIST_ITEM *item;
			JABBER_RESOURCE_STATUS *r;

			if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
				if ((item=JabberListGetItemPtr(LIST_ROSTER, from))!=NULL && (r=item->resource)!=NULL) {
					if ((p=strchr(from, '/'))!=NULL && p[1]!='\0') {
						p++;
						for (i=0; i<item->resourceCount && strcmp(r[i].resourceName, p); i++);
						if (i < item->resourceCount) {
							if (r[i].software) free(r[i].software);
							if ((n=JabberXmlGetChild(queryNode, "name"))!=NULL && n->text)
								r[i].software = JabberTextDecode(n->text);
							else
								r[i].software = NULL;
							if (r[i].version) free(r[i].version);
							if ((n=JabberXmlGetChild(queryNode, "version"))!=NULL && n->text)
								r[i].version = JabberTextDecode(n->text);
							else
								r[i].version = NULL;
							if (r[i].system) free(r[i].system);
							if ((n=JabberXmlGetChild(queryNode, "os"))!=NULL && n->text)
								r[i].system = JabberTextDecode(n->text);
							else
								r[i].system = NULL;
						}
					}
				}
			}
		}
	}
	// RECVED: <iq type='error'> ...
	else if (!strcmp(type, "error")) {
		JABBER_LIST_ITEM *item;
		// Check for multi-user chat errors
		char *from;
		if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
			if (strstr(from, "@c")!=NULL || !strcmp(from, "c")) {
				TlenMUCRecvError(from, node);
				return;
			}
		}

		// Check for file transfer deny by comparing idStr with ft->iqId
		i = 0;
		while ((i=JabberListFindNext(LIST_FILE, i)) >= 0) {
			item = JabberListGetItemPtrFromIndex(i);
			if (item->ft->state==FT_CONNECTING && !strcmp(idStr, item->ft->iqId)) {
				JabberLog("Denying file sending request");
				item->ft->state = FT_DENIED;
				if (item->ft->hFileEvent != NULL)
					SetEvent(item->ft->hFileEvent);	// Simulate the termination of file server connection
			}
			i++;
		}
	}
	// RECVED: <iq type='1'>...
	else if (!strcmp(type, "1")) { // Chat groups list result
		char *from;
		if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
			if (strcmp(from, "c")==0) {
				TlenIqResultChatGroups(node, userdata);
			}
		}
	}
	else if (!strcmp(type, "2")) { // Chat rooms list result
		char *from;
		if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
			if (strcmp(from, "c")==0) {
				TlenIqResultChatRooms(node, userdata);
			}
		}
	} else if (!strcmp(type, "3")) { // room search result - result to iq type 3 query
		char *from;
		if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
			if (strcmp(from, "c")==0) {
				TlenIqResultRoomSearch(node, userdata);
			}
		}
	} else if (!strcmp(type, "4")) { // chat room users list
		char *from;
		if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
			if (strstr(from, "@c")!=NULL) {
				TlenIqResultChatRoomUsers(node, userdata);
			}
		}
	} else if (!strcmp(type, "5")) { // room name & group & flags info - sent on joining the room
		char *from;
		if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
			if (strstr(from, "@c")!=NULL) {
				TlenIqResultRoomInfo(node, userdata);
			}
		}
	} else if (!strcmp(type, "6")) { // new nick registered
		char *from;
		if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
			if (strcmp(from, "c")==0) {
				TlenIqResultUserNicks(node, userdata);
			}
		}
	} else if (!strcmp(type, "7")) { // user nicknames list
		char *from;
		if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
			if (strcmp(from, "c")==0) {
				TlenIqResultUserNicks(node, userdata);
			}
		}
	} else if (!strcmp(type, "8")) { // user chat rooms list
		char *from;
		if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
			if (strcmp(from, "c")==0) {
				TlenIqResultUserRooms(node, userdata);
			}
		}
	}
}

static void __cdecl JabberKeepAliveThread(JABBER_SOCKET s)
{
	NETLIBSELECT nls = {0};

	nls.cbSize = sizeof(NETLIBSELECT);
	nls.dwTimeout = 60000;	// 60000 millisecond (1 minute)
	nls.hExceptConns[0] = s;
	for (;;) {
		if (CallService(MS_NETLIB_SELECT, 0, (LPARAM) &nls) != 0)
			break;
		if (jabberSendKeepAlive)
			JabberSend(s, " \t ");
	}
	JabberLog("Exiting KeepAliveThread");
}
/*
 * File transfer
 */
static void TlenProcessF(XmlNode *node, void *userdata)
{
	struct ThreadData *info;
	JABBER_FILE_TRANSFER *ft;
	CCSDATA ccs;
	PROTORECVEVENT pre;
	char *szBlob, *from, *p, *e, *nick;
	char jid[128], szFilename[MAX_PATH];
	int numFiles;
	JABBER_LIST_ITEM *item;

	if (!node->name || strcmp(node->name, "f")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;

	if ((from=JabberXmlGetAttrValue(node, "f")) != NULL) {

		if ((e=JabberXmlGetAttrValue(node, "e")) != NULL) {

			if (!strcmp(e, "1")) {
				// FILE_RECV : e='1' : File transfer request
				_snprintf(jid, sizeof(jid), "%s@%s", from, info->server);
				ft = (JABBER_FILE_TRANSFER *) malloc(sizeof(JABBER_FILE_TRANSFER));
				memset(ft, 0, sizeof(JABBER_FILE_TRANSFER));
				ft->jid = _strdup(jid);
				ft->hContact = JabberHContactFromJID(jid);

				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL)
					ft->iqId = _strdup(p);

				szFilename[0] = '\0';
				if ((p=JabberXmlGetAttrValue(node, "c")) != NULL) {
					numFiles = atoi(p);
					if (numFiles == 1) {
						if ((p=JabberXmlGetAttrValue(node, "n")) != NULL)
							p = JabberTextDecode(p);
							strncpy(szFilename, p, sizeof(szFilename));
							free(p);
							
					}
					else if (numFiles > 1) {
						_snprintf(szFilename, sizeof(szFilename), "%d Files", numFiles);
					}
				}

				if (szFilename[0]!='\0' && ft->iqId!=NULL) {
					// blob is DWORD(*ft), ASCIIZ(filenames), ASCIIZ(description)
					szBlob = (char *) malloc(sizeof(DWORD) + strlen(szFilename) + 2);
					*((PDWORD) szBlob) = (DWORD) ft;
					strcpy(szBlob + sizeof(DWORD), szFilename);
					szBlob[sizeof(DWORD) + strlen(szFilename) + 1] = '\0';
					pre.flags = 0;
					pre.timestamp = time(NULL);
					pre.szMessage = szBlob;
					pre.lParam = 0;
					ccs.szProtoService = PSR_FILE;
					ccs.hContact = ft->hContact;
					ccs.wParam = 0;
					ccs.lParam = (LPARAM) &pre;
					JabberLog("sending chainrecv");
					CallService(MS_PROTO_CHAINRECV, 0, (LPARAM) &ccs);
					free(szBlob);
				}
				else {
					// malformed <f/> request, reject
					if (ft->iqId)
						JabberSend(info->s, "<f i='%s' e='4' t='%s'/>", ft->iqId, from);
					else
						JabberSend(info->s, "<f e='4' t='%s'/>", from);
					TlenFileFreeFt(ft);
				}
			}
			else if (!strcmp(e, "3")) {
				// FILE_RECV : e='3' : invalid transfer error
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_FILE, p)) != NULL) {
						if (item->ft != NULL) {
							HANDLE  hEvent = item->ft->hFileEvent;
							item->ft->hFileEvent = NULL;
							item->ft->state = FT_ERROR;
							Netlib_CloseHandle(item->ft->s);
							item->ft->s = NULL;
							if (hEvent != NULL) {
								SetEvent(hEvent);
							}
						} else {
							JabberListRemove(LIST_FILE, p);
						}
					}
				}
			}
			else if (!strcmp(e, "4")) {
				// FILE_SEND : e='4' : File sending request was denied by the remote client
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_FILE, p)) != NULL) {
						nick = JabberNickFromJID(item->ft->jid);
						if (!strcmp(nick, from)) {
							ProtoBroadcastAck(jabberProtoName, item->ft->hContact, ACKTYPE_FILE, ACKRESULT_DENIED, item->ft, 0);
							JabberListRemove(LIST_FILE, p);
						}
						free(nick);
					}
				}
			}
			else if (!strcmp(e, "5")) {
				// FILE_SEND : e='5' : File sending request was accepted
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_FILE, p)) != NULL) {
						nick = JabberNickFromJID(item->ft->jid);
						if (!strcmp(nick, from))
							JabberForkThread((void (__cdecl *)(void*))TlenFileSendingThread, 0, item->ft);
						free(nick);
					}
				}
			}
			else if (!strcmp(e, "6")) {
				// FILE_RECV : e='6' : IP and port information to connect to get file
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_FILE, p)) != NULL) {
						if ((p=JabberXmlGetAttrValue(node, "a")) != NULL) {
							item->ft->httpHostName = _strdup(p);
							if ((p=JabberXmlGetAttrValue(node, "p")) != NULL) {
								item->ft->httpPort = atoi(p);
								JabberForkThread((void (__cdecl *)(void*))TlenFileReceiveThread, 0, item->ft);
							}
						}
					}
				}
			}
			else if (!strcmp(e, "7")) {
				// FILE_RECV : e='7' : IP and port information to connect to send file 
				// in case the conection to the given server was not successful
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_FILE, p)) != NULL) {
						if ((p=JabberXmlGetAttrValue(node, "a")) != NULL) {
							if (item->ft->httpHostName!=NULL) free(item->ft->httpHostName);
							item->ft->httpHostName = _strdup(p);
							if ((p=JabberXmlGetAttrValue(node, "p")) != NULL) {
								item->ft->httpPort = atoi(p);
								item->ft->state = FT_SWITCH;
								SetEvent(item->ft->hFileEvent);
							}
						}
					}
				}
			}
			else if (!strcmp(e, "8")) {
				// FILE_RECV : e='8' : transfer error
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_FILE, p)) != NULL) {
						item->ft->state = FT_ERROR;
						SetEvent(item->ft->hFileEvent);
					}
				}
			}
		}
	}
}
/*
 * Web messages
 */
static void TlenProcessW(XmlNode *node, void *userdata)
{
	struct ThreadData *info;
	HANDLE hContact;
	CCSDATA ccs;
	PROTORECVEVENT recv;
	char *f, *e, *s, *body;
	char *str, *localMessage;
	int strSize;

	if (!node->name || strcmp(node->name, "w")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;
	if ((body=node->text) == NULL) return;

	if ((f=JabberXmlGetAttrValue(node, "f")) != NULL) {

		char webContactName[128];
		sprintf(webContactName, Translate("%s Web Messages"), jabberModuleName);
		if ((hContact=JabberHContactFromJID(webContactName)) == NULL) {
			hContact = JabberDBCreateContact(webContactName, webContactName, TRUE, FALSE);
		}

		s = JabberXmlGetAttrValue(node, "s");
		e = JabberXmlGetAttrValue(node, "e");

		str = NULL;
		strSize = 0;
		JabberStringAppend(&str, &strSize, "%s\r\n%s: ", Translate("Web message"), Translate("From"));

		if (f != NULL)
			JabberStringAppend(&str, &strSize, "%s", f);
		JabberStringAppend(&str, &strSize, "\r\n%s: ", Translate("E-mail"));
		if (e != NULL)
			JabberStringAppend(&str, &strSize, "%s", e);
		JabberStringAppend(&str, &strSize, "\r\n\r\n%s", body);

		localMessage = JabberTextDecode(str);

		recv.flags = 0;
		recv.timestamp = (DWORD) time(NULL);
		recv.szMessage = localMessage;
		recv.lParam = 0;
		ccs.hContact = hContact;
		ccs.wParam = 0;
		ccs.szProtoService = PSR_MESSAGE;
		ccs.lParam = (LPARAM) &recv;
		CallService(MS_PROTO_CHAINRECV, 0, (LPARAM) &ccs);

		free(localMessage);
		free(str);
	}
}

/*
 * Typing notification, multi-user conference messages and invitations
 */
static void TlenProcessM(XmlNode *node, void *userdata)
{
	struct ThreadData *info;
	HANDLE hContact;
	CCSDATA ccs;
	PROTORECVEVENT recv;
	char *f;//, *from;//username
	char *tp;//typing start/stop
	char *p, *n, *r, *s, *str, *localMessage;
	int i;
	XmlNode *xNode, *invNode, *bNode, *subjectNode;

	if (!node->name || strcmp(node->name, "m")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;

	if ((f=JabberXmlGetAttrValue(node, "f")) != NULL) {
		if ((hContact=JabberHContactFromJID(f)) != NULL) {
			if ((tp=JabberXmlGetAttrValue(node, "tp")) != NULL) {
				if(!strcmp(tp, "t"))//contact is writing
					CallService(MS_PROTO_CONTACTISTYPING, (WPARAM)hContact, (LPARAM)PROTOTYPE_CONTACTTYPING_INFINITE);
				else if(!strcmp(tp, "u"))//contact stopped writing
					CallService(MS_PROTO_CONTACTISTYPING, (WPARAM)hContact, (LPARAM)PROTOTYPE_CONTACTTYPING_OFF);
				else if(!strcmp(tp, "a"))//alert was received
					SkinPlaySound("TlenAlertNotify");
			}
		}
		if ((p=strchr(f, '@')) != NULL) {
			if ((p=strchr(p, '/'))!=NULL && p[1]!='\0') { // message from user
				time_t timestamp;
				s = JabberXmlGetAttrValue(node, "s");
				if (s != NULL) {
					timestamp = TlenTimeToUTC(atol(s));
				} else {
					timestamp = time(NULL);
				}
				tp=JabberXmlGetAttrValue(node, "tp");
				bNode = JabberXmlGetChild(node, "b");
				f = JabberTextDecode(f);
				if (bNode->text != NULL) {
					if (tp != NULL && !strcmp(tp, "p")) {
						/* MUC private message */
						str = JabberResourceFromJID(f);
						hContact = JabberDBCreateContact(f, str, TRUE, FALSE);
						DBWriteContactSettingByte(hContact, jabberProtoName, "bChat", TRUE);
						free(str);
						localMessage = JabberTextDecode(bNode->text);
						recv.flags = 0;
						recv.timestamp = (DWORD) timestamp;
						recv.szMessage = localMessage;
						recv.lParam = 0;
						ccs.hContact = hContact;
						ccs.wParam = 0;
						ccs.szProtoService = PSR_MESSAGE;
						ccs.lParam = (LPARAM) &recv;
						CallService(MS_PROTO_CHAINRECV, 0, (LPARAM) &ccs);
						free(localMessage);
					} else {
						/* MUC message */ 
						TlenMUCRecvMessage(f, timestamp, bNode);
					}
				}
				free(f);
			} else { // message from chat room (system)
				subjectNode = JabberXmlGetChild(node, "subject");
				if (subjectNode != NULL) {
					f = JabberTextDecode(f);
					localMessage = "";
					if (subjectNode->text!=NULL)  {
						localMessage = subjectNode->text;
					}
					localMessage = JabberTextDecode(localMessage);
					TlenMUCRecvTopic(f, localMessage);
					free(localMessage);
					free(f);
				}
			}
		}
		i=1;
		while ((xNode=JabberXmlGetNthChild(node, "x", i)) != NULL) {
			invNode=JabberXmlGetChild(xNode, "inv");
			if (invNode != NULL) {
				r = JabberTextDecode(f);
				f = JabberXmlGetAttrValue(invNode, "f");
				f = JabberTextDecode(f);
				n = JabberXmlGetAttrValue(invNode, "n");
				if (n!=NULL) {
					n = JabberTextDecode(n);
				} else {
					n = _strdup(Translate("Private conference"));
					//n = JabberNickFromJID(r);
				}
				TlenMUCRecvInvitation(r, n, f, "");
				free(n);
				free(r);
				free(f);
				break;
			}
			i++;
		}
	}
}

static void TlenMailPopup(char *title, char *emailInfo)
{	
	POPUPDATAEX ppd;
	char * lpzContactName;
	char * lpzText;

	if (!DBGetContactSettingByte(NULL, jabberProtoName, "MailPopupEnabled", TRUE)) {
		return;
	}
	lpzContactName = title; 
	lpzText = emailInfo;
	ZeroMemory(&ppd, sizeof(ppd));
	ppd.lchContact = NULL;
	ppd.lchIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAIL));
	lstrcpy(ppd.lpzContactName, lpzContactName);
	lstrcpy(ppd.lpzText, lpzText);
	ppd.colorBack = DBGetContactSettingDword(NULL, jabberProtoName, "MailPopupBack", 0);
	ppd.colorText = DBGetContactSettingDword(NULL, jabberProtoName, "MailPopupText", 0);
	ppd.PluginWindowProc = NULL;
	ppd.PluginData=NULL;
	if ( ServiceExists( MS_POPUP_ADDPOPUPEX )) {
		BYTE delayMode;
		int delay;
		delayMode = DBGetContactSettingByte(NULL, jabberProtoName, "MailPopupDelayMode", 0);
		delay = 0;
		if (delayMode==1) {
			delay = DBGetContactSettingDword(NULL, jabberProtoName, "MailPopupDelay", 4); 
		} else if (delayMode==2) {
			delay = -1;
		}
		ppd.iSeconds = delay;
		CallService(MS_POPUP_ADDPOPUPEX, (WPARAM)&ppd, 0);

	}
	else if ( ServiceExists( MS_POPUP_ADDPOPUP )) {
		CallService(MS_POPUP_ADDPOPUP, (WPARAM)&ppd, 0);
	}
}
/*
 * Incoming e-mail notification
 */
static void TlenProcessN(XmlNode *node, void *userdata)
{	struct ThreadData *info;
	char *f, *s;
	char *str, *popupTitle, *popupText;
	int strSize;

	if (!node->name || strcmp(node->name, "n")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;

	s = JabberXmlGetAttrValue(node, "s");
	f = JabberXmlGetAttrValue(node, "f");
	if (s != NULL && f!=NULL) {
		str = NULL;
		strSize = 0;

		JabberStringAppend(&str, &strSize, "%s", Translate("New mail"));
		popupTitle = JabberTextDecode(str);
		free(str);

		str = NULL;
		strSize = 0;

		JabberStringAppend(&str, &strSize, "%s: %s\n", Translate("From"), f);
		JabberStringAppend(&str, &strSize, "%s: %s", Translate("Subject"), s);
		popupText = JabberTextDecode(str);
		TlenMailPopup(popupTitle, popupText);
		SkinPlaySound("TlenMailNotify");

		free(popupTitle);
		free(popupText);
		free(str);
	}
}

/*
 * Presence is chat rooms
 */
static void TlenProcessP(XmlNode *node, void *userdata)
{	struct ThreadData *info;
	char jid[512];
	char *f, *id, *tp, *a, *n, *k;
	XmlNode *sNode, *xNode, *iNode, *kNode;
	int status, flags;
	DBVARIANT dbv;

	if (!node->name || strcmp(node->name, "p")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;

 // presence from users in chat room
	flags = 0;
	status = ID_STATUS_ONLINE;
	f = JabberXmlGetAttrValue(node, "f");
	xNode = JabberXmlGetChild(node, "x");
	if (xNode != NULL) { // x subtag present (message from chat room) - change user rights only
		char *temp, *iStr;
		iNode = JabberXmlGetChild(xNode, "i");
		if (iNode != NULL) {
			iStr = JabberXmlGetAttrValue(iNode, "i");
			temp = malloc(strlen(f)+strlen(iStr)+2);
			strcpy(temp, f);
			strcat(temp, "/");
			strcat(temp, iStr);
			f = JabberTextDecode(temp);
			free(temp);
			node = iNode;
			status = 0;
		} else {
			f = JabberTextDecode(f);
		}
	} else {
		f = JabberTextDecode(f);
	}
	a = JabberXmlGetAttrValue(node, "z");
	if (a!=NULL) {
		if (atoi(a) &1 ) {
			flags |= USER_FLAGS_REGISTERED;
		}
	}
	a = JabberXmlGetAttrValue(node, "a");
	if (a!=NULL) {
		if (atoi(a) == 2) {
			flags |= USER_FLAGS_ADMIN;
		}
		if (atoi(a) == 1) {
			flags |= USER_FLAGS_OWNER;
		} 
		if (atoi(a) == 3) {
			//flags |= USER_FLAGS_MEMBER;
		} 
		if (atoi(a) == 5) {
			flags |= USER_FLAGS_GLOBALOWNER;
		} 
	}
	sNode = JabberXmlGetChild(node, "s");
	if (sNode != NULL) {
		if (!strcmp(sNode->text, "unavailable")) {
			status = ID_STATUS_OFFLINE;
		}
	}
	kNode = JabberXmlGetChild(node, "kick");
	k = NULL;
	if (kNode != NULL) {
		k = JabberXmlGetAttrValue(kNode, "r");
		if (k==NULL) {
			k = "";
		}
		k = JabberTextDecode(k);
	}
	tp = JabberXmlGetAttrValue(node, "tp");
	if (tp!=NULL && !strcmp(tp, "c")) { // new chat room has just been created
		id = JabberXmlGetAttrValue(node, "id");
		if (id != NULL) {
			n = JabberXmlGetAttrValue(node, "n");
			if (n!=NULL) {
				n = JabberTextDecode(n);
			} else {
				n = _strdup(Translate("Private conference"));// JabberNickFromJID(f);
			}
			if (!DBGetContactSetting(NULL, jabberProtoName, "LoginName", &dbv)) {
				// always real username
				sprintf(jid, "%s/%s", f, dbv.pszVal);
				TlenMUCCreateWindow(f, n, 0, NULL, id);
				TlenMUCRecvPresence(jid, ID_STATUS_ONLINE, flags, k);
				DBFreeVariant(&dbv);
			}
			free(n);
		}
	} else {
		TlenMUCRecvPresence(f, status, flags, k); // user presence
	}
	if (k!=NULL) {
		free(k);
	}
	free(f);
}
/*
 * Voice chat
 */
static void TlenProcessV(XmlNode *node, void *userdata)
{
	JABBER_LIST_ITEM *item;
	struct ThreadData *info;
	char *from, *id, *e, *p, *nick;
	if (!node->name || strcmp(node->name, "v")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;

	if ((from=JabberXmlGetAttrValue(node, "f")) != NULL) {
		if ((e=JabberXmlGetAttrValue(node, "e")) != NULL) {
			if (!strcmp(e, "1")) {
				if ((id=JabberXmlGetAttrValue(node, "i")) != NULL) {
					TlenVoiceAccept(id, from);
				}
			} else if (!strcmp(e, "3")) {
				// FILE_RECV : e='3' : invalid transfer error
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_VOICE, p)) != NULL) {
						if (item->ft != NULL) {
							HANDLE  hEvent = item->ft->hFileEvent;
							item->ft->hFileEvent = NULL;
							item->ft->state = FT_ERROR;
							Netlib_CloseHandle(item->ft->s);
							item->ft->s = NULL;
							if (hEvent != NULL) {
								SetEvent(hEvent);
							}
						} else {
							JabberListRemove(LIST_VOICE, p);
						}
					}
				}
			} else if (!strcmp(e, "4")) {
				// FILE_SEND : e='4' : File sending request was denied by the remote client
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_VOICE, p)) != NULL) {
						nick = JabberNickFromJID(item->ft->jid);
						if (!strcmp(nick, from)) {
							JabberListRemove(LIST_VOICE, p);
						}
						free(nick);
					}
				}
			} else if (!strcmp(e, "5")) {
			// FILE_SEND : e='5' : Voice request was accepted
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_VOICE, p)) != NULL) {
						nick = JabberNickFromJID(item->ft->jid);
						if (!strcmp(nick, from)) {
							TlenVoiceStart(item->ft, 1);
						}
						free(nick);
					}
				}
			} else if (!strcmp(e, "6")) {
				// FILE_RECV : e='6' : IP and port information to connect to get file
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_VOICE, p)) != NULL) {
						if ((p=JabberXmlGetAttrValue(node, "a")) != NULL) {
							item->ft->httpHostName = _strdup(p);
							if ((p=JabberXmlGetAttrValue(node, "p")) != NULL) {
								item->ft->httpPort = atoi(p);
								TlenVoiceStart(item->ft, 0);
								//JabberForkThread((void (__cdecl *)(void*))TlenVoiceReceiveThread, 0, item->ft); 
							}
						}
					}
				}
			}
			else if (!strcmp(e, "7")) {
				// FILE_RECV : e='7' : IP and port information to connect to send file 
				// in case the conection to the given server was not successful
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_VOICE, p)) != NULL) {
						if ((p=JabberXmlGetAttrValue(node, "a")) != NULL) {
							if (item->ft->httpHostName!=NULL) free(item->ft->httpHostName);
							item->ft->httpHostName = _strdup(p);
							if ((p=JabberXmlGetAttrValue(node, "p")) != NULL) {
								item->ft->httpPort = atoi(p);
								item->ft->state = FT_SWITCH;
								SetEvent(item->ft->hFileEvent);
							}
						}
					}
				}
			}
			else if (!strcmp(e, "8")) {
				// FILE_RECV : e='8' : transfer error 
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_VOICE, p)) != NULL) {
						item->ft->state = FT_ERROR;
						SetEvent(item->ft->hFileEvent);
					}
				}
			}

		}
	}
}
