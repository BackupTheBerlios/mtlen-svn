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
#include "jabber_list.h"
#include "jabber_iq.h"
#include "resource.h"
#include "tlen_p2p_old.h"
#include "tlen_file.h"
#include "tlen_muc.h"
#include "tlen_voice.h"
#include "tlen_avatar.h"
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>

extern void TlenProcessP2P(XmlNode *node, void *userdata);

//static void __cdecl TlenProcessInvitation(struct ThreadData *info);
static void __cdecl JabberKeepAliveThread(JABBER_SOCKET s);
static void JabberProcessStreamOpening(XmlNode *node, void *userdata);
static void JabberProcessStreamClosing(XmlNode *node, void *userdata);
static void JabberProcessProtocol(XmlNode *node, void *userdata);
static void JabberProcessMessage(XmlNode *node, void *userdata);
static void JabberProcessPresence(XmlNode *node, void *userdata);
static void JabberProcessIq(XmlNode *node, void *userdata);
static void TlenProcessW(XmlNode *node, void *userdata);
static void TlenProcessM(XmlNode *node, void *userdata);
static void TlenProcessN(XmlNode *node, void *userdata);
static void TlenProcessP(XmlNode *node, void *userdata);
static void TlenProcessV(XmlNode *node, void *userdata);
static void TlenProcessAvatar(XmlNode* node, void *userdata);
static void TlenProcessCipher(XmlNode *node, void *userdata);

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
		wsprintf(text, "%s %s", TranslateT("Enter password for"), (char *) lParam);
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
	int oldStatus = ID_STATUS_OFFLINE;
	int reconnectMaxTime;
	int numRetry;
	int reconnectTime;
	char *str;
	CLISTMENUITEM clmi;

	JabberLog("Thread started");


	// Normal server connection, we will fetch all connection parameters
	// e.g. username, password, etc. from the database.

	if (jabberThreadInfo != NULL) {
		// Will not start another connection thread if a thread is already running.
		// Make APC call to the main thread. This will immediately wake the thread up
		// in case it is asleep in the reconnect loop so that it will immediately
		// reconnect.
		QueueUserAPC(JabberDummyApcFunc, jabberThreadInfo->hThread, 0);
		JabberLog("Thread ended, another normal thread is running");
		mir_free(info);
		return;
	}

	jabberThreadInfo = info;
	if (streamId) mir_free(streamId);
	streamId = NULL;

	if (!DBGetContactSetting(NULL, jabberProtoName, "LoginName", &dbv)) {
		strncpy(info->username, dbv.pszVal, sizeof(info->username));
		info->username[sizeof(info->username)-1] = '\0';
		_strlwr(info->username);
		DBWriteContactSettingString(NULL, jabberProtoName, "LoginName", info->username);
		DBFreeVariant(&dbv);
	}
	else {
		mir_free(info);
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
		_strlwr(info->server);
		DBWriteContactSettingString(NULL, jabberProtoName, "LoginServer", info->server);
		DBFreeVariant(&dbv);
	}
	else {
		mir_free(info);
		jabberThreadInfo = NULL;
		oldStatus = jabberStatus;
		jabberStatus = ID_STATUS_OFFLINE;
		ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
		ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, LOGINERR_NONETWORK);
		JabberLog("Thread ended, login server is not configured");
		return;
	}
	if (!DBGetContactSetting(NULL, jabberProtoName, "AvatarHash", &dbv)) {
		strcpy(jabberThreadInfo->avatarHash, dbv.pszVal);
		DBFreeVariant(&dbv);
	}
	info->avatarFormat = DBGetContactSettingDword(NULL, jabberProtoName, "AvatarFormat", PA_FORMAT_UNKNOWN);

	_snprintf(jidStr, sizeof(jidStr), "%s@%s", info->username, info->server);
	DBWriteContactSettingString(NULL, jabberProtoName, "jid", jidStr);
	if (DBGetContactSettingByte(NULL, jabberProtoName, "SavePassword", TRUE) == FALSE) {
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
			mir_free(info);
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
	} else {
		if (DBGetContactSetting(NULL, jabberProtoName, "Password", &dbv)) {
			mir_free(info);
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

	if (!DBGetContactSetting(NULL, jabberProtoName, "ManualHost", &dbv)) {
		strncpy(info->manualHost, dbv.pszVal, sizeof(info->manualHost));
		info->manualHost[sizeof(info->manualHost)-1] = '\0';
		DBFreeVariant(&dbv);
	}
	info->port = DBGetContactSettingWord(NULL, jabberProtoName, "ManualPort", TLEN_DEFAULT_PORT);
	info->useEncryption = tlenOptions.useEncryption;

	if (info->manualHost[0])
		connectHost = info->manualHost;
	else
		connectHost = info->server;

	JabberLog("Thread server='%s' port='%d'", connectHost, info->port);

	jabberNetworkBufferSize = 2048;
	if ((buffer=(char *) mir_alloc(jabberNetworkBufferSize+1)) == NULL) {	// +1 is for '\0' when debug logging this buffer
		JabberLog("Cannot allocate network buffer, thread ended");
		oldStatus = jabberStatus;
		jabberStatus = ID_STATUS_OFFLINE;
		ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, LOGINERR_NONETWORK);
		ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
		jabberThreadInfo = NULL;
		mir_free(info);
		JabberLog("Thread ended, network buffer cannot be allocated");
		return;
	}

	reconnectMaxTime = 10;
	numRetry = 0;

	for (;;) {	// Reconnect loop

		info->s = JabberWsConnect(connectHost, info->port);
		if (info->s == NULL) {
			JabberLog("Connection failed (%d)", WSAGetLastError());
			if (jabberThreadInfo == info) {
				oldStatus = jabberStatus;
				jabberStatus = ID_STATUS_OFFLINE;
				ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, LOGINERR_NONETWORK);
				ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
				if (tlenOptions.reconnect == TRUE) {
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
						mir_free(buffer);
						mir_free(info);
						return;
					}
				}
				jabberThreadInfo = NULL;
			}
			JabberLog("Thread ended, connection failed");
			mir_free(buffer);
			mir_free(info);
			return;
		}

		// Determine local IP
		/*
		socket = CallService(MS_NETLIB_GETSOCKET, (WPARAM) info->s, 0);
		struct sockaddr_in saddr;
		int len;

		len = sizeof(saddr);
		getsockname(socket, (struct sockaddr *) &saddr, &len);
		jabberLocalIP = saddr.sin_addr.S_un.S_addr;
		JabberLog("Local IP = %s", inet_ntoa(saddr.sin_addr));
		*/

		// User may change status to OFFLINE while we are connecting above
		if (jabberDesiredStatus!=ID_STATUS_OFFLINE) {

			jabberConnected = TRUE;
			jabberJID = (char *) mir_alloc(strlen(info->username)+strlen(info->server)+2);
			sprintf(jabberJID, "%s@%s", info->username, info->server);
			if (DBGetContactSettingByte(NULL, jabberProtoName, "KeepAlive", 1))
				jabberSendKeepAlive = TRUE;
			else
				jabberSendKeepAlive = FALSE;
			JabberForkThread(JabberKeepAliveThread, 0, info->s);

			JabberXmlInitState(&xmlState);
			JabberXmlSetCallback(&xmlState, 1, ELEM_OPEN, JabberProcessStreamOpening, info);
			JabberXmlSetCallback(&xmlState, 1, ELEM_CLOSE, JabberProcessStreamClosing, info);
			JabberXmlSetCallback(&xmlState, 2, ELEM_CLOSE, JabberProcessProtocol, info);

			info->useAES = FALSE;

			if (info->useEncryption) {
				JabberSend(info->s, "<s s='1' v='9' t='06000106'>");
				
			} else {
				JabberSend(info->s, "<s v='3'>");
			}

			JabberLog("Entering main recv loop");
			datalen = 0;

			for (;;) {
				int recvResult, bytesParsed;

				JabberLog("Waiting for data...");
				if (info->useAES) {
					recvResult = JabberWsRecvAES(info->s, buffer+datalen, jabberNetworkBufferSize-datalen, &info->aes_in_context, info->aes_in_iv);
				} else {
					recvResult = JabberWsRecv(info->s, buffer+datalen, jabberNetworkBufferSize-datalen);
				}

				JabberLog("recvResult = %d", recvResult);
				if (recvResult <= 0)
					break;
				datalen += recvResult;

				buffer[datalen] = '\0';
				JabberLog("RECV:%s", buffer);

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
					if ((buffer=(char *) mir_realloc(buffer, jabberNetworkBufferSize+1)) == NULL) {
						JabberLog("Cannot reallocate more network buffer, go offline now");
						break;
					}
				}
				else {
					JabberLog("Unknown state: bytesParsed=%d, datalen=%d, jabberNetworkBufferSize=%d", bytesParsed, datalen, jabberNetworkBufferSize);
				}
			}

			JabberXmlDestroyState(&xmlState);

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

			mir_free(jabberJID);
			jabberJID = NULL;
			JabberListWipeSpecial();
		}
		else {
			oldStatus = jabberStatus;
			jabberStatus = ID_STATUS_OFFLINE;
			ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
		}

		Netlib_CloseHandle(info->s);

		if (tlenOptions.reconnect==FALSE)
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

	JabberLog("Thread ended: server='%s'", info->server);

	if (jabberThreadInfo==info) {
		if (streamId) mir_free(streamId);
		streamId = NULL;
		jabberThreadInfo = NULL;
	}

	mir_free(buffer);
	mir_free(info);
	JabberLog("Exiting ServerThread");
}

static void TlenSendAuth(void *userdata) {
	int iqId;
	char *p;
	char *str;
	char text[128];
	struct ThreadData *info = (struct ThreadData *) userdata;
	str = TlenPasswordHash(info->password);
	wsprintf(text, "%s%s", streamId, str);
	mir_free(str);
	str = JabberSha1(text);
	if ((p=JabberTextEncode(info->username)) != NULL) {
		iqId = JabberSerialNext();
		JabberIqAdd(iqId, IQ_PROC_NONE, JabberIqResultAuth);
		JabberSend(info->s, "<iq type='set' id='"JABBER_IQID"%d'><query xmlns='jabber:iq:auth'><username>%s</username><digest>%s</digest><resource>t</resource></query></iq>", iqId, p /*info->username*/, str);
		mir_free(p);
	}
	mir_free(str);
}

static void JabberProcessStreamOpening(XmlNode *node, void *userdata)
{
	struct ThreadData *info = (struct ThreadData *) userdata;
	char *sid, *s;

	if (node->name==NULL || strcmp(node->name, "s")) 
		return;

	if ((sid=JabberXmlGetAttrValue(node, "i")) != NULL) {
		if (streamId) mir_free(streamId);
		streamId = mir_strdup(sid);
	}
	if ((s=JabberXmlGetAttrValue(node, "s")) != NULL && !strcmp(s, "1")) {
		int i;
		char *k1, *k2, *k3;
		unsigned char aes_key[32];
		char aes_key_str[140], aes_iv_str[40];
		mpi k1_mpi, k2_mpi, aes_mpi;
		int slen;

		k1=JabberXmlGetAttrValue(node, "k1");
		k2=JabberXmlGetAttrValue(node, "k2");
		k3=JabberXmlGetAttrValue(node, "k3");

		memset(&info->aes_in_context, 0, sizeof (aes_context));
		memset(&info->aes_out_context, 0, sizeof (aes_context));
		memset(&aes_mpi, 0, sizeof (mpi));
		mpi_read_string(&aes_mpi, 16, k3);
		slen = 16;
		mpi_write_binary(&aes_mpi, info->aes_in_iv, &slen);
		for (i = 0; i < 16; i++) {
			info->aes_out_iv[i] = rand();
			aes_key[i] = rand();
		}
		memset(&aes_mpi, 0, sizeof (mpi));
		mpi_read_binary(&aes_mpi, info->aes_out_iv, 16);
		slen = 40;
		mpi_write_string(&aes_mpi, 16, aes_iv_str, &slen);
		aes_set_key(&info->aes_in_context, aes_key, 128);
		aes_set_key(&info->aes_out_context, aes_key, 128);
		memset(&aes_mpi, 0, sizeof (mpi));
		mpi_read_binary(&aes_mpi, aes_key, 16);
		memset(&k1_mpi, 0, sizeof (mpi));
		mpi_read_string( &k1_mpi, 16, k1 );
		memset(&k2_mpi, 0, sizeof (mpi));
	    mpi_read_string( &k2_mpi, 16, k2 );
		memset(&aes_mpi, 0, sizeof (mpi));
		mpi_read_binary(&aes_mpi, (unsigned char *)aes_key, 16);
		mpi_exp_mod( &aes_mpi, &aes_mpi, &k1_mpi, &k2_mpi, NULL );
		slen = 140;
		mpi_write_string(&aes_mpi, 16, aes_key_str, &slen);
		JabberSend(info->s, "<cipher k1='%s' k2='%s'/>", aes_key_str, aes_iv_str);
	} else {
		TlenSendAuth(userdata);
	}
}

static void JabberProcessStreamClosing(XmlNode *node, void *userdata)
{
	struct ThreadData *info = (struct ThreadData *) userdata;

	Netlib_CloseHandle(info->s);
	if (node->name && !strcmp(node->name, "stream:error") && node->text)
		MessageBox(NULL, TranslateT(node->text), TranslateT("Jabber Connection Error"), MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
}

static void JabberProcessProtocol(XmlNode *node, void *userdata)
{
	struct ThreadData *info;

	//JabberXmlDumpNode(node);

	info = (struct ThreadData *) userdata;

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
	else if (!strcmp(node->name, "avatar"))
		TlenProcessAvatar(node, userdata);
	else if (!strcmp(node->name, "cipher"))
		TlenProcessCipher(node, userdata);
	else
		JabberLog("Invalid top-level tag (only <message/> <presence/> <iq/> <f/> <w/> <m/> <n/> <p/> <v/> <cipher/> and <avatar/> allowed)");

}

static void TlenProcessCipher(XmlNode *node, void *userdata)
{
	char *type;
	struct ThreadData *info = (struct ThreadData *) userdata;
	type=JabberXmlGetAttrValue(node, "type");
	JabberLog("Cipher OK");
	info->useAES = TRUE;
	JabberSend(info->s, "<cipher type='ok'/>");
	TlenSendAuth(userdata);
}

static void TlenProcessIqGetVersion(XmlNode* node)
{
	OSVERSIONINFO osvi = { 0 };
	char mversion[256];
	char* from, *version, *mver;
	char* os = NULL;
	JABBER_LIST_ITEM *item;

	if (jabberStatus == ID_STATUS_INVISIBLE) return;
	if (!tlenOptions.enableVersion) return;
	if (( from=JabberXmlGetAttrValue( node, "from" )) == NULL ) return;
	if (( item=JabberListGetItemPtr( LIST_ROSTER, from )) ==NULL) return;
	version = JabberTextEncode( TLEN_VERSION_STRING );
	osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
	if ( GetVersionEx( &osvi )) {
		switch ( osvi.dwPlatformId ) {
		case VER_PLATFORM_WIN32_NT:
			if ( osvi.dwMajorVersion == 5 ) {
				if ( osvi.dwMinorVersion == 2 ) os = JabberTextEncode( TranslateT( "Windows Server 2003" ));
				else if ( osvi.dwMinorVersion == 1 ) os = JabberTextEncode( TranslateT( "Windows XP" ));
				else if ( osvi.dwMinorVersion == 0 ) os = JabberTextEncode( TranslateT( "Windows 2000" ));
			}
			else if ( osvi.dwMajorVersion <= 4 ) {
				os = JabberTextEncode( TranslateT( "Windows NT" ));
			}
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			if ( osvi.dwMajorVersion == 4 ) {
				if ( osvi.dwMinorVersion == 0 ) os = JabberTextEncode( TranslateT( "Windows 95" ));
				if ( osvi.dwMinorVersion == 10 ) os = JabberTextEncode( TranslateT( "Windows 98" ));
				if ( osvi.dwMinorVersion == 90 ) os = JabberTextEncode( TranslateT( "Windows ME" ));
			}
			break;
	}	}

	if ( os == NULL ) os = JabberTextEncode( TranslateT( "Windows" ));

	strcpy(mversion, "Miranda IM ");
	CallService( MS_SYSTEM_GETVERSIONTEXT, sizeof( mversion ) - 11, ( LPARAM )mversion + 11 );
	strcat(mversion, " (Tlen v.");
	strcat(mversion, TLEN_VERSION_STRING);
	strcat(mversion, ")");
	mver = JabberTextEncode( mversion );
	JabberSend( jabberThreadInfo->s, "<message to='%s' type='iq'><iq type='result'><query xmlns='jabber:iq:version'><name>%s</name><version>%s</version><os>%s</os></query></iq></message>", from, mver?mver:"", version?version:"", os?os:"" );
	if (!item->versionRequested) {
		item->versionRequested = TRUE;
		JabberSend(jabberThreadInfo->s, "<message to='%s' type='iq'><iq type='get'><query xmlns='jabber:iq:version'/></iq></message>", from);
	}

	if ( mver ) mir_free( mver );
	if ( version ) mir_free( version );
	if ( os ) mir_free( os );
}

// Support for Tlen avatars - avatar token used to access web interface
static void TlenProcessAvatar(XmlNode* node, void *userdata)
{
	struct ThreadData *info;
	XmlNode *tokenNode, *aNode;
	if ((info=(struct ThreadData *) userdata) == NULL) return;
	tokenNode = JabberXmlGetChild(node, "token");
	aNode = JabberXmlGetChild(node, "a");
	if (tokenNode != NULL) {
		char *token = tokenNode->text;
		strcpy(info->avatarToken, token);
	}
	if (aNode != NULL) {
		if (TlenProcessAvatarNode(node, NULL)) {
		}
	}
}

static void JabberProcessMessage(XmlNode *node, void *userdata)
{
	struct ThreadData *info;
	HANDLE hContact;
	CCSDATA ccs;
	PROTORECVEVENT recv;
	XmlNode *bodyNode, *subjectNode, *xNode, *idNode, *n;
	char *from, *type, *nick, *p, *localMessage, *idStr;
	DWORD msgTime;
	BOOL delivered, composing;
	int i, id;
	JABBER_LIST_ITEM *item;
	BOOL isChatRoomJid;

	if (!node->name || strcmp(node->name, "message")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;

	if ((type=JabberXmlGetAttrValue(node, "type"))!=NULL && !strcmp(type, "error")) {
	}
	else {
		if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
			if (DBGetContactSettingByte(NULL, jabberProtoName, "IgnoreAdvertisements", TRUE) && strstr(from, "b73@tlen.pl") == from) {
				return;
			}
			// If message is from a stranger (not in roster), item is NULL
			item = JabberListGetItemPtr(LIST_ROSTER, from);
			isChatRoomJid = JabberListExist(LIST_CHATROOM, from);

			if (isChatRoomJid && type!=NULL && !strcmp(type, "groupchat")) {
				//JabberGroupchatProcessMessage(node, userdata);
			} else if (type!=NULL && !strcmp(type, "pic")) {
				//1.  <message type='pic' to='piastucki@tlen.pl' crc='3baccc1' idt='7709' size='1434'/>
				//2.  <message to='the_leech7@tlen.pl' from='ps' type='pic' pid='1014' st='CtIO' rt='nWGa' idt='7709'/>
				char *crc, *crc_c, *idt, *size, *rt, *pid, *st;
				idt = JabberXmlGetAttrValue(node, "idt");
				size = JabberXmlGetAttrValue(node, "size");
				pid = JabberXmlGetAttrValue(node, "pid");
				crc_c = JabberXmlGetAttrValue(node, "crc_c");
				crc = JabberXmlGetAttrValue(node, "crc");
				rt = JabberXmlGetAttrValue(node, "rt");
				st = JabberXmlGetAttrValue(node, "st");
				if (strcmp(from, "ps")) {
					if (pid == NULL && crc != NULL) {
						JabberSend(info->s, "<message type='pic' to='%s' crc_c='n' idt='%s'/>", from, idt);
					} else if (crc_c != NULL) {
						/* crc_c = f, if the picure has been already received */
						/* crc_c = n, if the picture should be transferred */
						if (!strcmp(crc, "n")) {

						}
					} else if (rt != NULL) {
						// retrieve the picture here
					}
				} else {
					//from ps
				}
			} else if (type!=NULL && !strcmp(type, "iq")) {
				XmlNode *iqNode;
				// Jabber-compatible iq
				if ((iqNode=JabberXmlGetChild(node, "iq")) != NULL) {
					JabberXmlAddAttr(iqNode, "from", from);
					JabberProcessIq(iqNode, userdata);
				}
			} else {
				if ((bodyNode=JabberXmlGetChild(node, "body")) != NULL) {
					if (bodyNode->text != NULL) {
						if ((subjectNode=JabberXmlGetChild(node, "subject"))!=NULL && subjectNode->text!=NULL && subjectNode->text[0]!='\0') {
							p = (char *) mir_alloc(strlen(subjectNode->text)+strlen(bodyNode->text)+5);
							sprintf(p, "%s\r\n%s", subjectNode->text, bodyNode->text);
							localMessage = JabberTextDecode(p);
							mir_free(p);
						} else {
							localMessage = JabberTextDecode(bodyNode->text);
						}

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
											mir_free(item->messageEventIdStr);
										idStr = JabberXmlGetAttrValue(node, "id");
										item->messageEventIdStr = (idStr==NULL)?NULL:mir_strdup(idStr);
									}
								}
							}
							i++;
						}

						if (item != NULL) {
							item->wantComposingEvent = composing;
							if (item->isTyping) {
								item->isTyping = FALSE;
								if ((hContact=JabberHContactFromJID(from)) != NULL)
									CallService(MS_PROTO_CONTACTISTYPING, (WPARAM) hContact, PROTOTYPE_CONTACTTYPING_OFF);
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
								hContact = JabberDBCreateContact(from, nick, TRUE);
							}
							else {
								nick = JabberLocalNickFromJID(from);
								hContact = JabberDBCreateContact(from, nick, TRUE);
							}
							mir_free(nick);
						}

						if (msgTime == 0) {
							msgTime = time(NULL);
						} else {
							HANDLE hDbEvent = (HANDLE) CallService(MS_DB_EVENT_FINDLAST, (WPARAM) hContact, 0);
							if (hDbEvent != NULL) {
								DBEVENTINFO dbei = { 0 };
								dbei.cbSize = sizeof(dbei);
								CallService(MS_DB_EVENT_GET, (WPARAM) hDbEvent, (LPARAM) &dbei);
								if (msgTime < dbei.timestamp) {
									msgTime = dbei.timestamp + 1;
								}
							}
							if (msgTime > (DWORD)time(NULL)) {
								msgTime = time(NULL);
							}
						}
						recv.flags = 0;
						recv.timestamp = (DWORD) msgTime;
						recv.szMessage = localMessage;
						recv.lParam = 0;
						ccs.hContact = hContact;
						ccs.wParam = 0;
						ccs.szProtoService = PSR_MESSAGE;
						ccs.lParam = (LPARAM) &recv;
						CallService(MS_PROTO_CHAINRECV, 0, (LPARAM) &ccs);

						mir_free(localMessage);
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
									if (item != NULL) {
										item->isTyping = TRUE;
										CallService(MS_PROTO_CONTACTISTYPING, (WPARAM) hContact, PROTOTYPE_CONTACTTYPING_INFINITE);
									}
								}
							}
							if (xNode->numChild==0 || (xNode->numChild==1 && idNode!=NULL)) {
								// Maybe a cancel to the previous composing
								if ((hContact=JabberHContactFromJID(from)) != NULL) {
									if (item != NULL && item->isTyping) {
										item->isTyping = FALSE;
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
}

static void JabberProcessPresence(XmlNode *node, void *userdata)
{
	struct ThreadData *info;
	HANDLE hContact;
	XmlNode *showNode, *statusNode;
	JABBER_LIST_ITEM *item;
	char *from, *type, *nick, *show;
	int status, laststatus = ID_STATUS_OFFLINE;
	char *p;

	if (!node || !node->name || strcmp(node->name, "presence")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;

	if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
		if (JabberListExist(LIST_CHATROOM, from)); //JabberGroupchatProcessPresence(node, userdata);

		else {
			type = JabberXmlGetAttrValue(node, "type");
			item = JabberListGetItemPtr(LIST_ROSTER, from);
			if (item != NULL) {
				if (tlenOptions.enableAvatars) {
					TlenProcessPresenceAvatar(node, item);
				}
			}
			if (type==NULL || (!strcmp(type, "available"))) {
				if ((nick=JabberLocalNickFromJID(from)) != NULL) {
					if ((hContact=JabberHContactFromJID(from)) == NULL)
						hContact = JabberDBCreateContact(from, nick, FALSE);
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
								status = ID_STATUS_OFFLINE;
							}
						}
					}

					statusNode = JabberXmlGetChild(node, "status");
					if (statusNode)
						p = JabberTextDecode(statusNode->text);
					else
						p = NULL;
					JabberListAddResource(LIST_ROSTER, from, status, statusNode?p:NULL);
					if (p) {
						DBWriteContactSettingString(hContact, "CList", "StatusMsg", p);
						mir_free(p);
					} else {
						DBDeleteContactSetting(hContact, "CList", "StatusMsg");
					}
					// Determine status to show for the contact and request version information
					if (item != NULL) {
						laststatus = item->status;
						item->status = status;
					}
					if (strchr(from, '@')!=NULL || DBGetContactSettingByte(NULL, jabberProtoName, "ShowTransport", TRUE)==TRUE) {
						if (DBGetContactSettingWord(hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE) != status)
							DBWriteContactSettingWord(hContact, jabberProtoName, "Status", (WORD) status);
					}
					if (item != NULL) {
						if (!item->infoRequested) {
							int iqId = JabberSerialNext();
							item->infoRequested = TRUE;
							JabberSend( info->s, "<iq type='get' id='"JABBER_IQID"%d'><query xmlns='jabber:iq:info' to='%s'></query></iq>", iqId, from);
						}
						if (tlenOptions.enableVersion && !item->versionRequested) {
							int iqId = JabberSerialNext();
							item->versionRequested = TRUE;
							if (jabberStatus != ID_STATUS_INVISIBLE) {
								JabberSend( info->s, "<message to='%s' type='iq'><iq type='get'><query xmlns='jabber:iq:version'/></iq></message>", from );
							}
						}
					}
					JabberLog("%s (%s) online, set contact status to %d", nick, from, status);
					mir_free(nick);
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
					if ((hContact=JabberHContactFromJID(from)) != NULL) {
						if (p) {
							DBWriteContactSettingString(hContact, "CList", "StatusMsg", p);
						} else {
							DBDeleteContactSetting(hContact, "CList", "StatusMsg");
						}
					}
					if (p) mir_free(p);
				}
				if ((item=JabberListGetItemPtr(LIST_ROSTER, from)) != NULL) {
					// Determine status to show for the contact based on the remaining resources
					item->status = status;
					item->versionRequested = FALSE;
					item->infoRequested = FALSE;
				}
				if ((hContact=JabberHContactFromJID(from)) != NULL) {
					if (strchr(from, '@')!=NULL || DBGetContactSettingByte(NULL, jabberProtoName, "ShowTransport", TRUE)==TRUE) {
						if (DBGetContactSettingWord(hContact, jabberProtoName, "Status", ID_STATUS_OFFLINE) != status)
							DBWriteContactSettingWord(hContact, jabberProtoName, "Status", (WORD) status);
					}
					if (item != NULL && item->isTyping) {
						item->isTyping = FALSE;
						CallService(MS_PROTO_CONTACTISTYPING, (WPARAM) hContact, PROTOTYPE_CONTACTTYPING_OFF);
					}
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
					mir_free(nick);
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
	XmlNode *queryNode = NULL;
	char *type, *jid, *nick;
	char *xmlns = NULL;
	char *idStr, *str;
	int id;
	int i;
	JABBER_IQ_PFUNC pfunc;

	if (!node->name || strcmp(node->name, "iq")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;
	type=JabberXmlGetAttrValue(node, "type");
//	if ((type=JabberXmlGetAttrValue(node, "type")) == NULL) return;

	id = -1;
	if ((idStr=JabberXmlGetAttrValue(node, "id")) != NULL) {
		if (!strncmp(idStr, JABBER_IQID, strlen(JABBER_IQID)))
			id = atoi(idStr+strlen(JABBER_IQID));
	}

	queryNode = JabberXmlGetChild(node, "query");
	if (queryNode != NULL) {
		xmlns = JabberXmlGetAttrValue(queryNode, "xmlns");
	}


	/////////////////////////////////////////////////////////////////////////
	// MATCH BY ID
	/////////////////////////////////////////////////////////////////////////
	if ((pfunc=JabberIqFetchFunc(id)) != NULL) {
		JabberLog("Handling iq request for id=%d", id);
		pfunc(node, userdata);
	/////////////////////////////////////////////////////////////////////////
	// MORE GENERAL ROUTINES, WHEN ID DOES NOT MATCH
	/////////////////////////////////////////////////////////////////////////
	// new p2p connections
	} else if (xmlns != NULL && !strcmp(xmlns, "p2p")) { 
		if (tlenOptions.useNewP2P) {
			TlenProcessP2P(node, userdata);
		}
	}
	// RECVED: <iq type='set'><query ...
	else if (!strcmp(type, "set") && queryNode!=NULL && xmlns!=NULL) {

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
										if (item->nick) mir_free(item->nick);
										item->nick = nick;
										if ((hContact=JabberHContactFromJID(jid)) == NULL) {
											// Received roster has a new JID.
											// Add the jid (with empty resource) to Miranda contact list.
											hContact = JabberDBCreateContact(jid, nick, FALSE);
										}
										else
											DBWriteContactSettingString(hContact, jabberProtoName, "jid", jid);
										DBWriteContactSettingString(hContact, "CList", "MyHandle", nick);
										if (item->group) mir_free(item->group);
										if ((groupNode=JabberXmlGetChild(itemNode, "group"))!=NULL && groupNode->text!=NULL) {
											item->group = TlenGroupDecode(groupNode->text);
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
										mir_free(nick);
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
	else if ( !strcmp( type, "get" ) && queryNode!=NULL && xmlns!=NULL ) {
		// RECVED: software version query
		// ACTION: return my software version
		if ( !strcmp( xmlns, "jabber:iq:version" )) TlenProcessIqGetVersion(node);
	}
	// RECVED: <iq type='result'><query ...
	else if ( !strcmp( type, "result") && queryNode!=NULL) {
		if (xmlns!=NULL ) {
			if ( !strcmp(xmlns, "jabber:iq:roster" )) {
				JabberIqResultRoster(node, userdata);
			} else if ( !strcmp( xmlns, "jabber:iq:version" )) {
				TlenIqResultVersion(node, userdata);
			} else if ( !strcmp( xmlns, "jabber:iq:info" )) {
				TlenIqResultInfo(node, userdata);
			}
		} else {
			char *from;
			if (( from=JabberXmlGetAttrValue( node, "from" )) != NULL ) {
				if ( !strcmp(from, "tcfg" )) {
					TlenIqResultTcfg(node, info);
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
		sprintf(webContactName, TranslateT("%s Web Messages"), jabberModuleName);
		if ((hContact=JabberHContactFromJID(webContactName)) == NULL) {
			hContact = JabberDBCreateContact(webContactName, webContactName, TRUE);
		}

		s = JabberXmlGetAttrValue(node, "s");
		e = JabberXmlGetAttrValue(node, "e");

		str = NULL;
		strSize = 0;
		JabberStringAppend(&str, &strSize, "%s\r\n%s: ", TranslateT("Web message"), TranslateT("From"));

		if (f != NULL)
			JabberStringAppend(&str, &strSize, "%s", f);
		JabberStringAppend(&str, &strSize, "\r\n%s: ", TranslateT("E-mail"));
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

		mir_free(localMessage);
		mir_free(str);
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
				JABBER_LIST_ITEM *item = JabberListGetItemPtr(LIST_ROSTER, f);
				if(!strcmp(tp, "t")) { //contact is writing
					if (item!=NULL ) {
						item->isTyping = TRUE;
						CallService(MS_PROTO_CONTACTISTYPING, (WPARAM)hContact, (LPARAM)PROTOTYPE_CONTACTTYPING_INFINITE);
					}
				}
				else if(!strcmp(tp, "u")) {//contact stopped writing
					if (item!=NULL) {
						item->isTyping = FALSE;
						CallService(MS_PROTO_CONTACTISTYPING, (WPARAM)hContact, (LPARAM)PROTOTYPE_CONTACTTYPING_OFF);
					}
				}
				else if(!strcmp(tp, "a")) {//alert was received
					int bAlert = TRUE;
					if (tlenOptions.alertPolicy == TLEN_ALERTS_IGNORE_ALL) {
						bAlert = FALSE;
					} else if (tlenOptions.alertPolicy == TLEN_ALERTS_IGNORE_NIR) {
						if (item == NULL) bAlert = FALSE;
						else if (item->subscription==SUB_NONE || item->subscription==SUB_TO) bAlert = FALSE;
					}
					if (bAlert) {
						if (tlenOptions.useNudge) {
							NotifyEventHooks(hTlenNudge,(WPARAM) hContact,0);
						} else {
							if (tlenOptions.logAlerts) {
								CCSDATA ccs;
								PROTORECVEVENT recv;
								char *localMessage = mir_strdup(TranslateT("An alert has been received."));
								recv.flags = 0;
								recv.timestamp = (DWORD) time(NULL);
								recv.szMessage = localMessage;
								recv.lParam = 0;
								ccs.hContact = hContact;
								ccs.wParam = 0;
								ccs.szProtoService = PSR_MESSAGE;
								ccs.lParam = (LPARAM) &recv;
								CallService(MS_PROTO_CHAINRECV, 0, (LPARAM) &ccs);
								mir_free(localMessage);
							}
							SkinPlaySound("TlenAlertNotify");
						}
					}
				}
			}
		}
		if ((p=strchr(f, '@')) != NULL) {
			if ((p=strchr(p, '/'))!=NULL && p[1]!='\0') { // message from user
				time_t timestamp;
				s = JabberXmlGetAttrValue(node, "s");
				if (s != NULL) {
					timestamp = TlenTimeToUTC(atol(s));
					if (timestamp > time(NULL)) {
						timestamp = time(NULL);
					}
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
						hContact = JabberDBCreateContact(f, str, TRUE);
						DBWriteContactSettingByte(hContact, jabberProtoName, "bChat", TRUE);
						mir_free(str);
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
						mir_free(localMessage);
					} else {
						/* MUC message */
						TlenMUCRecvMessage(f, timestamp, bNode);
					}
				}
				mir_free(f);
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
					mir_free(localMessage);
					mir_free(f);
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
				if (n!=NULL && strstr(r, n)!=r) {
					n = JabberTextDecode(n);
				} else {
					n = mir_strdup(TranslateT("Private conference"));
					//n = JabberNickFromJID(r);
				}
				TlenMUCRecvInvitation(r, n, f, "");
				mir_free(n);
				mir_free(r);
				mir_free(f);
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

		JabberStringAppend(&str, &strSize, TranslateT("%s mail"), jabberModuleName);
		popupTitle = JabberTextDecode(str);
		mir_free(str);

		str = NULL;
		strSize = 0;

		JabberStringAppend(&str, &strSize, "%s: %s\n", TranslateT("From"), f);
		JabberStringAppend(&str, &strSize, "%s: %s", TranslateT("Subject"), s);
		popupText = JabberTextDecode(str);
		TlenMailPopup(popupTitle, popupText);
		SkinPlaySound("TlenMailNotify");

		mir_free(popupTitle);
		mir_free(popupText);
		mir_free(str);
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
			temp = mir_alloc(strlen(f)+strlen(iStr)+2);
			strcpy(temp, f);
			strcat(temp, "/");
			strcat(temp, iStr);
			f = JabberTextDecode(temp);
			mir_free(temp);
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
				n = mir_strdup(TranslateT("Private conference"));// JabberNickFromJID(f);
			}
			sprintf(jid, "%s/%s", f, info->username);
//			if (!DBGetContactSetting(NULL, jabberProtoName, "LoginName", &dbv)) {
				// always real username
//				sprintf(jid, "%s/%s", f, dbv.pszVal);
			TlenMUCCreateWindow(f, n, 0, NULL, id);
			TlenMUCRecvPresence(jid, ID_STATUS_ONLINE, flags, k);
//				DBFreeVariant(&dbv);
//			}
			mir_free(n);
		}
	} else {
		TlenMUCRecvPresence(f, status, flags, k); // user presence
	}
	if (k!=NULL) {
		mir_free(k);
	}
	mir_free(f);
}
/*
 * Voice chat
 */
static void TlenProcessV(XmlNode *node, void *userdata)
{
	JABBER_LIST_ITEM *item;
	struct ThreadData *info;
	char *from, *id, *e, *p, *nick;
//	if (!node->name || strcmp(node->name, "v")) return;
	if ((info=(struct ThreadData *) userdata) == NULL) return;

	if ((from=JabberXmlGetAttrValue(node, "f")) != NULL) {
		if ((e=JabberXmlGetAttrValue(node, "e")) != NULL) {
			if (!strcmp(e, "1")) {
				if ((id=JabberXmlGetAttrValue(node, "i")) != NULL) {
					SkinPlaySound("TlenVoiceNotify");
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
							if (item->ft->s != NULL) {
								Netlib_CloseHandle(item->ft->s);
								item->ft->s = NULL;
								if (hEvent != NULL) {
									SetEvent(hEvent);
								}
							} else {
								TlenP2PFreeFileTransfer(item->ft);
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
							TlenVoiceCancelAll();
							//JabberListRemove(LIST_VOICE, p);
						}
						mir_free(nick);
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
						mir_free(nick);
					}
				}
			} else if (!strcmp(e, "6")) {
				// FILE_RECV : e='6' : IP and port information to connect to get file
				if ((p=JabberXmlGetAttrValue(node, "i")) != NULL) {
					if ((item=JabberListGetItemPtr(LIST_VOICE, p)) != NULL) {
						if ((p=JabberXmlGetAttrValue(node, "a")) != NULL) {
							item->ft->hostName = mir_strdup(p);
							if ((p=JabberXmlGetAttrValue(node, "p")) != NULL) {
								item->ft->wPort = atoi(p);
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
							if (item->ft->hostName!=NULL) mir_free(item->ft->hostName);
							item->ft->hostName = mir_strdup(p);
							if ((p=JabberXmlGetAttrValue(node, "p")) != NULL) {
								item->ft->wPort = atoi(p);
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

