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

TLEN_FILE_TRANSFER* TlenP2PEstablishIncomingConnection(JABBER_SOCKET *s, int list, BOOL sendAck) 
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
