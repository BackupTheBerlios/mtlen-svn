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

BOOL JabberWsInit(TlenProtocol *proto)
{
	NETLIBUSER nlu = {0};
	NETLIBUSERSETTINGS nlus = {0};
	char name[128];

	sprintf(name, "%s %s", proto->iface.m_szModuleName, TranslateT("connection"));

	nlu.cbSize = sizeof(nlu);
	nlu.flags = NUF_OUTGOING | NUF_INCOMING | NUF_HTTPCONNS;	// | NUF_HTTPGATEWAY;
	nlu.szDescriptiveName = name;
	nlu.szSettingsModule = proto->iface.m_szModuleName;
	proto->hNetlibUser = (HANDLE) CallService(MS_NETLIB_REGISTERUSER, 0, (LPARAM) &nlu);

	nlu.flags = NUF_OUTGOING | NUF_INCOMING | NUF_NOOPTIONS;
	sprintf(name, "%sSOCKS", proto->iface.m_szModuleName);
	nlu.szDescriptiveName = name;
	proto->hFileNetlibUser = (HANDLE) CallService(MS_NETLIB_REGISTERUSER, 0, (LPARAM) &nlu);
	nlus.cbSize = sizeof(nlus);
	nlus.useProxy = 0;
	CallService(MS_NETLIB_SETUSERSETTINGS, (WPARAM) proto->hFileNetlibUser, (LPARAM) &nlus);

	return (proto->hNetlibUser!=NULL)?TRUE:FALSE;
}

void JabberWsUninit(TlenProtocol *proto)
{
	if (proto->hNetlibUser!=NULL) Netlib_CloseHandle(proto->hNetlibUser);
	if (proto->hFileNetlibUser!=NULL) Netlib_CloseHandle(proto->hFileNetlibUser);
    proto->hNetlibUser = NULL;
    proto->hFileNetlibUser = NULL;
}

JABBER_SOCKET JabberWsConnect(TlenProtocol *proto, char *host, WORD port)
{
	NETLIBOPENCONNECTION nloc;

	nloc.cbSize = NETLIBOPENCONNECTION_V1_SIZE;//sizeof(NETLIBOPENCONNECTION);
	nloc.szHost = host;
	nloc.wPort = port;
	nloc.flags = 0;
	return (HANDLE) CallService(MS_NETLIB_OPENCONNECTION, (WPARAM) proto->hNetlibUser, (LPARAM) &nloc);
}

int JabberWsSend(TlenProtocol *proto, char *data, int datalen)
{
	int len;
    if (proto->threadData == NULL) {
        return FALSE;
    }
	if ((len=Netlib_Send(proto->threadData->s, data, datalen, /*MSG_NODUMP|*/MSG_DUMPASTEXT))==SOCKET_ERROR || len!=datalen) {
		JabberLog(proto, "Netlib_Send() failed, error=%d", WSAGetLastError());
		return FALSE;
	}
	return TRUE;
}

int JabberWsRecv(TlenProtocol *proto, char *data, long datalen)
{
	int ret;
    if (proto->threadData == NULL) {
        return 0;
    }
	ret = Netlib_Recv(proto->threadData->s, data, datalen, /*MSG_NODUMP|*/MSG_DUMPASTEXT);
	if(ret == SOCKET_ERROR) {
		JabberLog(proto, "Netlib_Recv() failed, error=%d", WSAGetLastError());
		return 0;
	}
	if(ret == 0) {
		JabberLog(proto, "Connection closed gracefully");
		return 0;
	}
	return ret;
}


int JabberWsSendAES(TlenProtocol *proto, char *data, int datalen, aes_context *aes_ctx, unsigned char *aes_iv)
{
	int len, sendlen;
	unsigned char aes_input[16];
	unsigned char aes_output[256];
    if (proto->threadData == NULL) {
        return FALSE;
    }
	while (datalen > 0) {
		len = 0;
		while (datalen > 0 && len < 256) {
			int pad = datalen < 16 ? 16 - datalen : 0;
			memcpy(aes_input, data, datalen < 16 ? datalen : 16);
			memset(aes_input + 16 - pad, ' ', pad);
			aes_cbc_encrypt(aes_ctx, aes_iv, aes_input, aes_output + len, 16);
			datalen -= 16;
			data += 16;
			len += 16;
		}
		if (len > 0) {
			JabberLog(proto, "Sending %d bytes", len);
			if ((sendlen=Netlib_Send(proto->threadData->s, aes_output, len, MSG_NODUMP))==SOCKET_ERROR || len!=sendlen) {
				JabberLog(proto, "Netlib_Send() failed, error=%d", WSAGetLastError());
				return FALSE;
			}
		}
	}
	return TRUE;
}

int JabberWsRecvAES(TlenProtocol *proto, char *data, long datalen, aes_context *aes_ctx, unsigned char *aes_iv)
{
	int ret, len = 0, maxlen = datalen;
	unsigned char aes_input[16];
	unsigned char *aes_output = (unsigned char *)data;
    if (proto->threadData == NULL) {
        return 0;
    }
	for (maxlen = maxlen & ~0xF; maxlen != 0; maxlen = maxlen & 0xF) {
		ret = Netlib_Recv(proto->threadData->s, data, maxlen, MSG_NODUMP);
		if(ret == SOCKET_ERROR) {
			JabberLog(proto, "Netlib_Recv() failed, error=%d", WSAGetLastError());
			return 0;
		}
		if(ret == 0) {
			JabberLog(proto, "Connection closed gracefully");
			return 0;
		}
		data += ret;
		len += ret;
		maxlen -= ret;
	}

	ret = len;
	while (len > 15) {
		memcpy(aes_input, aes_output, 16);
		aes_cbc_decrypt(aes_ctx, aes_iv, aes_input, aes_output, 16);
		aes_output += 16;
		len -= 16;
	}
	return ret;
}

