/*

Tlen Protocol Plugin for Miranda IM
Copyright (C) 2004 Piotr Piastucki

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
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "jabber_list.h"
#include "tlen_p2p_old.h"


void TlenP2PFreeFileTransfer(TLEN_FILE_TRANSFER *ft)
{
	int i;

	if (ft->jid) mir_free(ft->jid);
	if (ft->iqId) mir_free(ft->iqId);
	if (ft->hostName) mir_free(ft->hostName);
	if (ft->szSavePath) mir_free(ft->szSavePath);
	if (ft->szDescription) mir_free(ft->szDescription);
	if (ft->filesSize) mir_free(ft->filesSize);
	if (ft->files) {
		for (i=0; i<ft->fileCount; i++) {
			if (ft->files[i]) mir_free(ft->files[i]);
		}
		mir_free(ft->files);
	}
	mir_free(ft);
}

TLEN_FILE_PACKET *TlenP2PPacketCreate(int datalen)
{
	TLEN_FILE_PACKET *packet;

	if ((packet=(TLEN_FILE_PACKET *) malloc(sizeof(TLEN_FILE_PACKET))) == NULL)
		return NULL;
	if ((packet->packet=(char *) malloc(datalen)) == NULL) {
		free(packet);
		return NULL;
	}
	packet->maxDataLen = datalen;
	packet->type=0;
	packet->len=0;
	return packet;
}

void TlenP2PPacketFree(TLEN_FILE_PACKET *packet)
{
	if (packet != NULL) {
		if (packet->packet != NULL)
			free(packet->packet);
		free(packet);
	}
}

void TlenP2PPacketSetType(TLEN_FILE_PACKET *packet, DWORD type)
{
	if (packet!=NULL) {
		packet->type = type;
	}
}

void TlenP2PPacketSetLen(TLEN_FILE_PACKET *packet, DWORD len)
{
	if (packet!=NULL) {
		packet->len = len;
	}
}

void TlenP2PPacketPackDword(TLEN_FILE_PACKET *packet, DWORD data)
{
	if (packet!=NULL && packet->packet!=NULL) {
		if (packet->len + sizeof(DWORD) <= packet->maxDataLen) {
			(*((DWORD*)((packet->packet)+(packet->len)))) = data;
			packet->len += sizeof(DWORD);
		}
		else {
			JabberLog("TlenP2PPacketPackDword() overflow");
		}
	}
}

void TlenP2PPacketPackBuffer(TLEN_FILE_PACKET *packet, char *buffer, int len)
{
	if (packet!=NULL && packet->packet!=NULL) {
		if (packet->len + len <= packet->maxDataLen) {
			memcpy((packet->packet)+(packet->len), buffer, len);
			packet->len += len;
		}
		else {
			JabberLog("TlenP2PPacketPackBuffer() overflow");
		}
	}
}

int TlenP2PPacketSend(JABBER_SOCKET s, TLEN_FILE_PACKET *packet)
{
	DWORD sendResult;
	if (packet!=NULL && packet->packet!=NULL) {
		Netlib_Send(s, (char *)&packet->type, 4, MSG_NODUMP);
		Netlib_Send(s, (char *)&packet->len, 4, MSG_NODUMP);
		sendResult=Netlib_Send(s, packet->packet, packet->len, MSG_NODUMP);
		if (sendResult==SOCKET_ERROR || sendResult!=packet->len) return 0;
	}
	return 1;
}

TLEN_FILE_PACKET* TlenP2PPacketReceive(JABBER_SOCKET s)
{
	TLEN_FILE_PACKET *packet;
	DWORD recvResult;
	DWORD type, len, pos;
	recvResult = Netlib_Recv(s, (char *)&type, 4, MSG_NODUMP);
	if (recvResult==0 || recvResult==SOCKET_ERROR) return NULL;
	recvResult = Netlib_Recv(s, (char *)&len, 4, MSG_NODUMP);
	if (recvResult==0 || recvResult==SOCKET_ERROR) return NULL;
	packet = TlenP2PPacketCreate(len);
	TlenP2PPacketSetType(packet, type);
	TlenP2PPacketSetLen(packet, len);
	pos = 0;
	while (len > 0) {
		recvResult = Netlib_Recv(s, packet->packet+pos, len, MSG_NODUMP);
		if (recvResult==0 || recvResult==SOCKET_ERROR) {
			TlenP2PPacketFree(packet);
			return NULL;
		}
		len -= recvResult;
		pos += recvResult;
	}
	return packet;
}

void TlenP2PEstablishOutgoingConnection(TLEN_FILE_TRANSFER *ft, BOOL sendAck)
{
	char *hash;
	char str[300];
	TLEN_FILE_PACKET *packet;
	JabberLog("Establishing outgoing connection.");
	ft->state = FT_ERROR;
	if ((packet = TlenP2PPacketCreate(2*sizeof(DWORD) + 20))!=NULL) {
		TlenP2PPacketSetType(packet, TLEN_FILE_PACKET_CONNECTION_REQUEST);
		TlenP2PPacketPackDword(packet, 1);
		TlenP2PPacketPackDword(packet, (DWORD) atoi(ft->iqId));
		_snprintf(str, sizeof(str), "%08X%s%d", atoi(ft->iqId), jabberThreadInfo->username, atoi(ft->iqId));
		hash = TlenSha1(str, strlen(str));
		TlenP2PPacketPackBuffer(packet, hash, 20);
		free(hash);
		TlenP2PPacketSend(ft->s, packet);
		TlenP2PPacketFree(packet);
		packet = TlenP2PPacketReceive(ft->s);
		if (packet != NULL) {
			if (packet->type == TLEN_FILE_PACKET_CONNECTION_REQUEST_ACK) { // acknowledge
				if ((int)(*((DWORD*)packet->packet)) == atoi(ft->iqId)) {
					ft->state = FT_CONNECTING;
					if (sendAck) {
						ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTED, ft, 0);
					}
				}
			}
			TlenP2PPacketFree(packet);
		}
	}
}

TLEN_FILE_TRANSFER* TlenP2PEstablishIncomingConnection(JABBER_SOCKET s, int list, BOOL sendAck)
{
	JABBER_LIST_ITEM *item = NULL;
	TLEN_FILE_PACKET *packet;
	int i;
	char str[300];
	DWORD iqId;
	JabberLog("Establishing incoming connection.");
	// TYPE: 0x1
	// LEN:
	// (DWORD) 0x1
	// (DWORD) id
	// (BYTE) hash[20]
	packet = TlenP2PPacketReceive(s);
	if (packet == NULL || packet->type != TLEN_FILE_PACKET_CONNECTION_REQUEST || packet->len<28) {
		if (packet!=NULL) {
			TlenP2PPacketFree(packet);
		}
		JabberLog("Unable to read first packet.");
		return NULL;
	}
	iqId = *((DWORD *)(packet->packet+sizeof(DWORD)));
	i = 0;
	while ((i=JabberListFindNext(list, i)) >= 0) {
		if ((item=JabberListGetItemPtrFromIndex(i))!=NULL) {
			_snprintf(str, sizeof(str), "%d", iqId);
			if (!strcmp(item->ft->iqId, str)) {
				char *hash, *nick;
				int j;
				nick = JabberNickFromJID(item->ft->jid);
				_snprintf(str, sizeof(str), "%08X%s%d", iqId, nick, iqId);
				free(nick);
				hash = TlenSha1(str, strlen(str));
				for (j=0;j<20;j++) {
					if (hash[j]!=packet->packet[2*sizeof(DWORD)+j]) break;
				}
				free(hash);
				if (j==20) break;
			}
		}
		i++;
	}
	TlenP2PPacketFree(packet);
	if (i >=0) {
		if ((packet=TlenP2PPacketCreate(sizeof(DWORD))) != NULL) {
			// Send connection establishment acknowledgement
			TlenP2PPacketSetType(packet, TLEN_FILE_PACKET_CONNECTION_REQUEST_ACK);
			TlenP2PPacketPackDword(packet, (DWORD) atoi(item->ft->iqId));
			TlenP2PPacketSend(s, packet);
			TlenP2PPacketFree(packet);
			item->ft->state = FT_CONNECTING;
			if (sendAck) {
				ProtoBroadcastAck(jabberProtoName, item->ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTED, item->ft, 0);
			}
			return item->ft;
		}
	}
	return NULL;
}

static void __cdecl TlenFileBindSocks4Thread(TLEN_FILE_TRANSFER* ft)
{
	BYTE buf[8];
	int status;

//	JabberLog("Waiting for the file to be sent via SOCKS...");
	status = Netlib_Recv(ft->s, buf, 8, MSG_NODUMP);
//	JabberLog("accepted connection !!!");
	if ( status == SOCKET_ERROR || status<8 || buf[1]!=90) {
		status = 1;
	} else {
		status = 0;
	}
	if (!status) {
		ft->pfnNewConnectionV2(ft->s, 0, NULL);
	} else {
		if (ft->state!=FT_SWITCH) {
			ft->state = FT_ERROR;
		}
	}
	JabberLog("Closing connection for this file transfer...");
//	Netlib_CloseHandle(ft->s);
	if (ft->hFileEvent != NULL)
		SetEvent(ft->hFileEvent);

}
static void __cdecl TlenFileBindSocks5Thread(TLEN_FILE_TRANSFER* ft)
{
	BYTE buf[256];
	int status;

//	JabberLog("Waiting for the file to be sent via SOCKS...");
	status = Netlib_Recv(ft->s, buf, sizeof(buf), MSG_NODUMP);
//	JabberLog("accepted connection !!!");
	if ( status == SOCKET_ERROR || status<7 || buf[1]!=0) {
		status = 1;
	} else {
		status = 0;
	}
	if (!status) {
		ft->pfnNewConnectionV2(ft->s, 0, NULL);
	} else {
		if (ft->state!=FT_SWITCH) {
			ft->state = FT_ERROR;
		}
	}
//	JabberLog("Closing connection for this file transfer...");
//	Netlib_CloseHandle(ft->s);
	if (ft->hFileEvent != NULL)
		SetEvent(ft->hFileEvent);

}

static JABBER_SOCKET TlenP2PBindSocks4(SOCKSBIND * sb, TLEN_FILE_TRANSFER *ft)
{	//rfc1928
	int len;
	BYTE buf[256];
	int status;
	struct in_addr in;
	NETLIBOPENCONNECTION nloc;
	JABBER_SOCKET s;

	nloc.cbSize = NETLIBOPENCONNECTION_V1_SIZE;//sizeof(NETLIBOPENCONNECTION);
	nloc.szHost = sb->szHost;
	nloc.wPort = sb->wPort;
	nloc.flags = 0;
	s = (HANDLE) CallService(MS_NETLIB_OPENCONNECTION, (WPARAM) hFileNetlibUser, (LPARAM) &nloc);
	if (s==NULL) {
//		JabberLog("Connection failed (%d), thread ended", WSAGetLastError());
		return NULL;
	}
	buf[0] = 4;  //socks4
	buf[1] = 2;  //2-bind, 1-connect
	*(PWORD)(buf+2) = htons(0); // port
	*(PDWORD)(buf+4) = INADDR_ANY;
	if (sb->useAuth) {
		lstrcpy(buf+8, sb->szUser);
		len = strlen(sb->szUser);
	} else {
		buf[8] = 0;
		len = 0;
	}
	len += 9;
	status = Netlib_Send(s, buf, len, MSG_NODUMP);
	if (status==SOCKET_ERROR || status<len) {
//		JabberLog("Send failed (%d), thread ended", WSAGetLastError());
		Netlib_CloseHandle(s);
		return NULL;
	}
	status = Netlib_Recv(s, buf, 8, MSG_NODUMP);
	if (status==SOCKET_ERROR || status<8 || buf[1]!=90) {
//		JabberLog("SOCKS4 negotiation failed");
		Netlib_CloseHandle(s);
		return NULL;
	}
	status = Netlib_Recv(s, buf, sizeof(buf), MSG_NODUMP);
	if ( status == SOCKET_ERROR || status<7 || buf[0]!=5 || buf[1]!=0) {
//		JabberLog("SOCKS5 request failed");
		Netlib_CloseHandle(s);
		return NULL;
	}
	in.S_un.S_addr = *(PDWORD)(buf+4);
	strcpy(sb->szHost, inet_ntoa(in));
	sb->wPort = htons(*(PWORD)(buf+2));
	ft->s = s;
	JabberForkThread((void (__cdecl *)(void*))TlenFileBindSocks4Thread, 0, ft);
	return s;
}

static JABBER_SOCKET TlenP2PBindSocks5(SOCKSBIND * sb, TLEN_FILE_TRANSFER *ft)
{	//rfc1928
	BYTE buf[512];
	int len, status;
	NETLIBOPENCONNECTION nloc;
	struct in_addr in;
	JABBER_SOCKET s;

	nloc.cbSize = NETLIBOPENCONNECTION_V1_SIZE;//sizeof(NETLIBOPENCONNECTION);
	nloc.szHost = sb->szHost;
	nloc.wPort = sb->wPort;
	nloc.flags = 0;
	s = (HANDLE) CallService(MS_NETLIB_OPENCONNECTION, (WPARAM) hFileNetlibUser, (LPARAM) &nloc);
	if (s==NULL) {
		JabberLog("Connection failed (%d), thread ended", WSAGetLastError());
		return NULL;
	}
	buf[0] = 5;  //yep, socks5
	buf[1] = 1;  //one auth method
	buf[2] = sb->useAuth?2:0; // authorization
	status = Netlib_Send(s, buf, 3, MSG_NODUMP);
	if (status==SOCKET_ERROR || status<3) {
		JabberLog("Send failed (%d), thread ended", WSAGetLastError());
		Netlib_CloseHandle(s);
		return NULL;
	}
	status = Netlib_Recv(s, buf, 2, MSG_NODUMP);
	if (status==SOCKET_ERROR || status<2 || (buf[1]!=0 && buf[1]!=2)) {
		JabberLog("SOCKS5 negotiation failed");
		Netlib_CloseHandle(s);
		return NULL;
	}
	if(buf[1]==2) {		//rfc1929
		int nUserLen, nPassLen;
		PBYTE pAuthBuf;

		nUserLen = lstrlen(sb->szUser);
		nPassLen = lstrlen(sb->szPassword);
		pAuthBuf = (PBYTE)mir_alloc(3+nUserLen+nPassLen);
		pAuthBuf[0] = 1;		//auth version
		pAuthBuf[1] = nUserLen;
		memcpy(pAuthBuf+2, sb->szUser, nUserLen);
		pAuthBuf[2+nUserLen]=nPassLen;
		memcpy(pAuthBuf+3+nUserLen,sb->szPassword,nPassLen);
		status = Netlib_Send(s, pAuthBuf, 3+nUserLen+nPassLen, MSG_NODUMP);
		mir_free(pAuthBuf);
		if (status==SOCKET_ERROR || status<3+nUserLen+nPassLen) {
			JabberLog("Send failed (%d), thread ended", WSAGetLastError());
			Netlib_CloseHandle(s);
			return NULL;
		}
		status = Netlib_Recv(s, buf, sizeof(buf), MSG_NODUMP);
		if (status==SOCKET_ERROR || status<2 || buf[1]!=0) {
			JabberLog("SOCKS5 sub-negotiation failed");
			Netlib_CloseHandle(s);
			return NULL;
		}
	}

	{	PBYTE pInit;
		int nHostLen=4;
		DWORD hostIP=INADDR_ANY;
		pInit=(PBYTE)mir_alloc(6+nHostLen);
		pInit[0]=5;   //SOCKS5
		pInit[1]=2;   //bind
		pInit[2]=0;   //reserved
		pInit[3]=1;
		*(PDWORD)(pInit+4)=hostIP;
		*(PWORD)(pInit+4+nHostLen)=htons(0);
		status = Netlib_Send(s, pInit, 6+nHostLen, MSG_NODUMP);
		mir_free(pInit);
		if (status==SOCKET_ERROR || status<6+nHostLen) {
//			JabberLog("Send failed (%d), thread ended", WSAGetLastError());
			Netlib_CloseHandle(s);
			return NULL;
		}
	}
	status = Netlib_Recv(s, buf, sizeof(buf), MSG_NODUMP);
	if ( status == SOCKET_ERROR || status<7 || buf[0]!=5 || buf[1]!=0) {
//		JabberLog("SOCKS5 request failed");
		Netlib_CloseHandle(s);
		return NULL;
	}
	if (buf[2]==1) { // domain
		len = buf[4];
		memcpy(sb->szHost, buf+5, len);
		sb->szHost[len]=0;
		len += 4;
	} else { // ip address
		in.S_un.S_addr = *(PDWORD)(buf+4);
		strcpy(sb->szHost, inet_ntoa(in));
		len = 8;
	}
	sb->wPort = htons(*(PWORD)(buf+len));
	ft->s = s;

	JabberForkThread((void (__cdecl *)(void*))TlenFileBindSocks5Thread, 0, ft);
	return s;
}


JABBER_SOCKET TlenP2PListen(TLEN_FILE_TRANSFER *ft)
{
	NETLIBBIND nlb = {0};
	JABBER_SOCKET s = NULL;
	int	  useProxy;
	DBVARIANT dbv;
	SOCKSBIND sb;
	struct in_addr in;

	useProxy=0;
	if (ft->hostName != NULL) mir_free(ft->hostName);
	ft->hostName = NULL;
	ft->wPort = 0;
	if (DBGetContactSettingByte(NULL, jabberProtoName, "UseFileProxy", FALSE)) {
		if (!DBGetContactSetting(NULL, jabberProtoName, "FileProxyHost", &dbv)) {
			strcpy(sb.szHost, dbv.pszVal);
			DBFreeVariant(&dbv);
			sb.wPort = DBGetContactSettingWord(NULL, jabberProtoName, "FileProxyPort", 0);
			sb.useAuth = FALSE;
			strcpy(sb.szUser, "");
			strcpy(sb.szPassword, "");
			if (DBGetContactSettingByte(NULL, jabberProtoName, "FileProxyAuth", FALSE)) {
				sb.useAuth = TRUE;
				if (!DBGetContactSetting(NULL, jabberProtoName, "FileProxyUsername", &dbv)) {
					strcpy(sb.szUser, dbv.pszVal);
					DBFreeVariant(&dbv);
				}
				if (!DBGetContactSetting(NULL, jabberProtoName, "FileProxyPassword", &dbv)) {
					CallService(MS_DB_CRYPT_DECODESTRING, strlen(dbv.pszVal)+1, (LPARAM) dbv.pszVal);
					strcpy(sb.szPassword, dbv.pszVal);
					DBFreeVariant(&dbv);
				}
			}
			switch (DBGetContactSettingWord(NULL, jabberProtoName, "FileProxyType", 0)) {
				case 0: // forwarding
					useProxy = 1;
					break;
				case 1: // socks4
					s = TlenP2PBindSocks4(&sb, ft);
					useProxy = 2;
					break;
				case 2: // socks5
					s = TlenP2PBindSocks5(&sb, ft);
					useProxy = 2;
					break;
			}
			ft->hostName = mir_strdup(sb.szHost);
			ft->wPort = sb.wPort;
			ft->wExPort = sb.wPort;
		}
	}
	if (useProxy<2) {
		nlb.cbSize = sizeof(NETLIBBIND);
		nlb.pfnNewConnectionV2 = ft->pfnNewConnectionV2;
		nlb.wPort = 0;	// User user-specified incoming port ranges, if available
		nlb.pExtra = NULL;
		JabberLog("Calling MS_NETLIB_BINDPORT");
		s = (HANDLE) CallService(MS_NETLIB_BINDPORT, (WPARAM) hNetlibUser, (LPARAM) &nlb);
		JabberLog("listening on %d",s);
	}
	if (useProxy==0) {
		in.S_un.S_addr = htonl(nlb.dwExternalIP);
		ft->hostName = mir_strdup(inet_ntoa(in));
		ft->wPort = nlb.wPort;
		ft->wExPort = nlb.wExPort;
	}
	return s;
}
