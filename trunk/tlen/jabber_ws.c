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

BOOL JabberWsInit(void)
{
	NETLIBUSER nlu = {0};
	NETLIBUSERSETTINGS nlus = {0};
	char name[128];

	sprintf(name, "%s %s", jabberModuleName, TranslateT("connection"));

	nlu.cbSize = sizeof(nlu);
	nlu.flags = NUF_OUTGOING | NUF_INCOMING | NUF_HTTPCONNS;	// | NUF_HTTPGATEWAY;
	nlu.szDescriptiveName = name;
	nlu.szSettingsModule = jabberProtoName;
	//nlu.szHttpGatewayHello = "http://http.proxy.icq.com/hello";
	//nlu.szHttpGatewayUserAgent = "Mozilla/4.08 [en] (WinNT; U ;Nav)";
	//nlu.pfnHttpGatewayInit = JabberHttpGatewayInit;
	//nlu.pfnHttpGatewayBegin = JabberHttpGatewayBegin;
	//nlu.pfnHttpGatewayWrapSend = JabberHttpGatewayWrapSend;
	//nlu.pfnHttpGatewayUnwrapRecv = JabberHttpGatewayUnwrapRecv;
	hNetlibUser = (HANDLE) CallService(MS_NETLIB_REGISTERUSER, 0, (LPARAM) &nlu);

	nlu.cbSize = sizeof(nlu);
	nlu.flags = NUF_OUTGOING | NUF_INCOMING | NUF_NOOPTIONS;
	sprintf(name, "%sSOCKS", jabberProtoName);
	nlu.szSettingsModule = name;
	hFileNetlibUser = (HANDLE) CallService(MS_NETLIB_REGISTERUSER, 0, (LPARAM) &nlu);
	nlus.cbSize = sizeof(nlus);
	nlus.useProxy = 0;
	CallService(MS_NETLIB_SETUSERSETTINGS, (WPARAM) hFileNetlibUser, (LPARAM) &nlus);

	return (hNetlibUser!=NULL)?TRUE:FALSE;
}

void JabberWsUninit(void)
{
	if (hNetlibUser!=NULL) Netlib_CloseHandle(hNetlibUser);
	if (hFileNetlibUser!=NULL) Netlib_CloseHandle(hFileNetlibUser);
}

JABBER_SOCKET JabberWsConnect(char *host, WORD port)
{
	NETLIBOPENCONNECTION nloc;

	nloc.cbSize = NETLIBOPENCONNECTION_V1_SIZE;//sizeof(NETLIBOPENCONNECTION);
	nloc.szHost = host;
	nloc.wPort = port;
	nloc.flags = 0;
	return (HANDLE) CallService(MS_NETLIB_OPENCONNECTION, (WPARAM) hNetlibUser, (LPARAM) &nloc);
}

int JabberWsSend(JABBER_SOCKET hConn, char *data, int datalen)
{
	int len;

	if ((len=Netlib_Send(hConn, data, datalen, /*MSG_NODUMP|*/MSG_DUMPASTEXT))==SOCKET_ERROR || len!=datalen) {
		JabberLog("Netlib_Send() failed, error=%d", WSAGetLastError());
		return FALSE;
	}
	return TRUE;
}

int JabberWsRecv(JABBER_SOCKET hConn, char *data, long datalen)
{
	int ret;

	ret = Netlib_Recv(hConn, data, datalen, /*MSG_NODUMP|*/MSG_DUMPASTEXT);
	if(ret == SOCKET_ERROR) {
		JabberLog("Netlib_Recv() failed, error=%d", WSAGetLastError());
		return 0;
	}
	if(ret == 0) {
		JabberLog("Connection closed gracefully");
		return 0;
	}
	return ret;
}


int JabberWsSendAES(JABBER_SOCKET hConn, char *data, int datalen, aes_context *aes_ctx, unsigned char *aes_iv)
{
	int len, sendlen;
	unsigned char aes_input[16];
	unsigned char aes_output[256];
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
			JabberLog("Sending %d bytes", len);
			if ((sendlen=Netlib_Send(hConn, aes_output, len, MSG_NODUMP))==SOCKET_ERROR || len!=sendlen) {
				JabberLog("Netlib_Send() failed, error=%d", WSAGetLastError());
				return FALSE;
			}
		}
	}
	return TRUE;
}

int JabberWsRecvAES(JABBER_SOCKET hConn, char *data, long datalen, aes_context *aes_ctx, unsigned char *aes_iv)
{
	int ret, len = 0, maxlen = datalen;
	unsigned char aes_input[16];
	unsigned char *aes_output = (unsigned char *)data;

	for (maxlen = maxlen & ~0xF; maxlen != 0; maxlen = maxlen & 0xF) {
		ret = Netlib_Recv(hConn, data, maxlen, MSG_NODUMP);
		if(ret == SOCKET_ERROR) {
			JabberLog("Netlib_Recv() failed, error=%d", WSAGetLastError());
			return 0;
		}
		if(ret == 0) {
			JabberLog("Connection closed gracefully");
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

