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

extern char *jabberProtoName;
extern HANDLE hNetlibUser;
extern struct ThreadData *jabberThreadInfo;
extern DWORD jabberLocalIP;

static void TlenFileReceiveParse(TLEN_FILE_TRANSFER *ft);
static void TlenFileSendParse(TLEN_FILE_TRANSFER *ft);
static void TlenFileSendingConnection(HANDLE hNewConnection, DWORD dwRemoteIP, void * pExtra);
static void TlenFileReceivingConnection(HANDLE hNewConnection, DWORD dwRemoteIP, void * pExtra);


void TlenFileFreeFt(TLEN_FILE_TRANSFER *ft)
{
	int i;

	if (ft->jid) free(ft->jid);
	if (ft->iqId) free(ft->iqId);
	if (ft->httpHostName) free(ft->httpHostName);
	if (ft->httpPath) free(ft->httpPath);
	if (ft->szSavePath) free(ft->szSavePath);
	if (ft->szDescription) free(ft->szDescription);
	if (ft->filesSize) free(ft->filesSize);
	if (ft->files) {
		for (i=0; i<ft->fileCount; i++) {
			if (ft->files[i]) free(ft->files[i]);
		}
		free(ft->files);
	}
	free(ft);
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
//		JabberLog("Entering recv loop for this file connection... (ft->s is hConnection)");
		if (TlenP2PEstablishIncomingConnection(ft->s, LIST_FILE, TRUE)!=NULL) {
			while (ft->state!=FT_DONE && ft->state!=FT_ERROR) {
				if (ft->mode == FT_SEND) {
					TlenFileSendParse(ft);
				} else {
					TlenFileReceiveParse(ft);
				}
			}
		} else ft->state = FT_ERROR;
		if (ft->state==FT_DONE)
			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
		else
			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
	} else {
		if (ft->state!=FT_SWITCH) 
			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
	}
	JabberLog("Closing connection for this file transfer...");
//	Netlib_CloseHandle(ft->s);
	if (ft->hFileEvent != NULL)
		SetEvent(ft->hFileEvent);

}
static JABBER_SOCKET TlenFileBindSocks4(SOCKSBIND * sb, TLEN_FILE_TRANSFER *ft)
{	//rfc1928
	int len;
	BYTE buf[256];
	int status;
	struct in_addr in;
	NETLIBOPENCONNECTION nloc;
	JABBER_SOCKET s;
//	JabberLog("connecting to SOCK4 proxy...%s:%d", sb->szHost, sb->wPort);

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
		JabberLog("Entering recv loop for this file connection... (ft->s is hConnection)");
		if (TlenP2PEstablishIncomingConnection(ft->s, LIST_FILE, TRUE)!=NULL) {
			while (ft->state!=FT_DONE && ft->state!=FT_ERROR) {
				if (ft->mode == FT_SEND) {
					TlenFileSendParse(ft);
				} else {
					TlenFileReceiveParse(ft);
				}
			}
		} else ft->state = FT_ERROR;
		if (ft->state==FT_DONE)
			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
		else
			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
	} else {
		if (ft->state!=FT_SWITCH) 
			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
	}
//	JabberLog("Closing connection for this file transfer...");
//	Netlib_CloseHandle(ft->s);
	if (ft->hFileEvent != NULL)
		SetEvent(ft->hFileEvent);

}

static JABBER_SOCKET TlenFileBindSocks5(SOCKSBIND * sb, TLEN_FILE_TRANSFER *ft)
{	//rfc1928
	BYTE buf[512];
	int len, status;
	NETLIBOPENCONNECTION nloc;
	struct in_addr in;
	JABBER_SOCKET s;

//	JabberLog("connecting to SOCK5 proxy...%s:%d", sb->szHost, sb->wPort);

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
		pAuthBuf = (PBYTE)malloc(3+nUserLen+nPassLen);
		pAuthBuf[0] = 1;		//auth version
		pAuthBuf[1] = nUserLen;
		memcpy(pAuthBuf+2, sb->szUser, nUserLen);
		pAuthBuf[2+nUserLen]=nPassLen;
		memcpy(pAuthBuf+3+nUserLen,sb->szPassword,nPassLen);
		status = Netlib_Send(s, pAuthBuf, 3+nUserLen+nPassLen, MSG_NODUMP);
		free(pAuthBuf);
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
		pInit=(PBYTE)malloc(6+nHostLen);
		pInit[0]=5;   //SOCKS5
		pInit[1]=2;   //bind
		pInit[2]=0;   //reserved
		pInit[3]=1;
		*(PDWORD)(pInit+4)=hostIP;
		*(PWORD)(pInit+4+nHostLen)=htons(0);
		status = Netlib_Send(s, pInit, 6+nHostLen, MSG_NODUMP);
		free(pInit);
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

static JABBER_SOCKET TlenFileListen(TLEN_FILE_TRANSFER *ft)
{
	NETLIBBIND nlb = {0};
	JABBER_SOCKET s = NULL;
	int	  useProxy;
	DBVARIANT dbv;
	SOCKSBIND sb;
	struct in_addr in;

	JabberLog("TlenFileListen");

	useProxy=0;
	if (ft->httpHostName != NULL) free (ft->httpHostName);
	ft->httpHostName = NULL;
	ft->httpPort = 0;
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
					s = TlenFileBindSocks4(&sb, ft);
					useProxy = 2;
					break;
				case 2: // socks5
					s = TlenFileBindSocks5(&sb, ft);
					useProxy = 2;
					break;
			}
			ft->httpHostName = _strdup(sb.szHost);
			ft->httpPort = sb.wPort;
		}
	} 
	if (useProxy<2) {
		nlb.cbSize = sizeof(NETLIBBIND);
		if (ft->mode == FT_SEND) {
			nlb.pfnNewConnectionV2 = TlenFileSendingConnection;
		} else {
			nlb.pfnNewConnectionV2 = TlenFileReceivingConnection;
		}
		nlb.wPort = 0;	// User user-specified incoming port ranges, if available
		nlb.pExtra = NULL;
		JabberLog("Calling MS_NETLIB_BINDPORT");
		s = (HANDLE) CallService(MS_NETLIB_BINDPORT, (WPARAM) hNetlibUser, (LPARAM) &nlb);
		JabberLog("listening on %d",s);
	}
	if (useProxy==0) {
		in.S_un.S_addr = jabberLocalIP;
		ft->httpHostName = _strdup(inet_ntoa(in));
		ft->httpPort = nlb.wPort;
	}
	return s;
}

void __cdecl TlenFileReceiveThread(TLEN_FILE_TRANSFER *ft)
{
	NETLIBOPENCONNECTION nloc;
	JABBER_SOCKET s;

	JabberLog("Thread started: type=file_receive server='%s' port='%d'", ft->httpHostName, ft->httpPort);
	ft->mode = FT_RECV;
	nloc.cbSize = NETLIBOPENCONNECTION_V1_SIZE;//sizeof(NETLIBOPENCONNECTION);
	nloc.szHost = ft->httpHostName;
	nloc.wPort = ft->httpPort;
	nloc.flags = 0;
	ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTING, ft, 0);
	s = (HANDLE) CallService(MS_NETLIB_OPENCONNECTION, (WPARAM) hNetlibUser, (LPARAM) &nloc);
	if (s != NULL) {
		ft->s = s;
		JabberLog("Entering file receive loop");
		TlenP2PEstablishOutgoingConnection(ft, TRUE);
		while (ft->state!=FT_DONE && ft->state!=FT_ERROR) {
			TlenFileReceiveParse(ft);
		}
		if (ft->s) {
			Netlib_CloseHandle(s);
		}
		ft->s = NULL;
	} else {
		JabberLog("Connection failed - receiving as server");
		s = TlenFileListen(ft);
		if (s != NULL) {
			HANDLE hEvent;
			char *nick;
			ft->s = s;
			hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
			ft->hFileEvent = hEvent;
			ft->currentFile = 0;
			ft->state = FT_CONNECTING;
			nick = JabberNickFromJID(ft->jid);
			JabberSend(jabberThreadInfo->s, "<f t='%s' i='%s' e='7' a='%s' p='%d'/>", nick, ft->iqId, ft->httpHostName, ft->httpPort);
			free(nick);
			JabberLog("Waiting for the file to be received...");
			WaitForSingleObject(hEvent, INFINITE);
			ft->hFileEvent = NULL;
			CloseHandle(hEvent);
			JabberLog("Finish all files");
			Netlib_CloseHandle(s);
		} else {
			ft->state = FT_ERROR;
		}
	}
	JabberListRemove(LIST_FILE, ft->iqId);
	if (ft->state==FT_DONE)
		ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
	else
		ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);

	JabberLog("Thread ended: type=file_receive server='%s'", ft->httpHostName);

	TlenFileFreeFt(ft);
}

static void TlenFileReceivingConnection(JABBER_SOCKET hConnection, DWORD dwRemoteIP, void * pExtra)
{
	JABBER_SOCKET slisten;
	TLEN_FILE_TRANSFER *ft;

	ft = TlenP2PEstablishIncomingConnection(hConnection, LIST_FILE, TRUE);
	if (ft != NULL) {
		slisten = ft->s;
		ft->s = hConnection;
		JabberLog("Set ft->s to %d (saving %d)", hConnection, slisten);
		JabberLog("Entering send loop for this file connection... (ft->s is hConnection)");
		while (ft->state!=FT_DONE && ft->state!=FT_ERROR) {
			TlenFileReceiveParse(ft);
		}
		if (ft->state==FT_DONE)
			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
		else
			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		JabberLog("Closing connection for this file transfer... (ft->s is now hBind)");
		ft->s = slisten;
		JabberLog("ft->s is restored to %d", ft->s);
	}
	Netlib_CloseHandle(hConnection);
	if (ft!=NULL && ft->hFileEvent != NULL)
		SetEvent(ft->hFileEvent);
}

static void TlenFileReceiveParse(TLEN_FILE_TRANSFER *ft)
{
	int i;
	char *p;
	TLEN_FILE_PACKET *rpacket, *packet;

	rpacket = NULL;
	if (ft->state == FT_CONNECTING) {
		rpacket = TlenP2PPacketReceive(ft->s);
		if (rpacket != NULL) {
			p = rpacket->packet;
			if (rpacket->type == TLEN_FILE_PACKET_FILE_LIST) { // list of files (length & name)
				ft->fileCount = (int)(*((DWORD*)p));
				ft->files = (char **) malloc(sizeof(char *) * ft->fileCount);
				ft->filesSize = (long *) malloc(sizeof(long) * ft->fileCount);
				ft->currentFile = 0;
				ft->allFileTotalSize = 0;
				ft->allFileReceivedBytes = 0;
				p += sizeof(DWORD);
				for (i=0;i<ft->fileCount;i++) {
					ft->filesSize[i] = (long)(*((DWORD*)p));
					ft->allFileTotalSize += ft->filesSize[i];
					p += sizeof(DWORD);
					ft->files[i] = (char *)malloc(256);
					memcpy(ft->files[i], p, 256);
					p += 256;
				}
				if ((packet=TlenP2PPacketCreate(3*sizeof(DWORD))) == NULL) {
					ft->state = FT_ERROR;
				} 
				else {
					TlenP2PPacketSetType(packet, TLEN_FILE_PACKET_FILE_LIST_ACK); 
					TlenP2PPacketSend(ft->s, packet);
					TlenP2PPacketFree(packet);
					ft->state = FT_INITIALIZING;
					JabberLog("Change to FT_INITIALIZING");
				}
			}
			TlenP2PPacketFree(rpacket);
		}
		else {
			ft->state = FT_ERROR;
		}
	}
	else if (ft->state == FT_INITIALIZING) {
		char *fullFileName;
		if ((packet=TlenP2PPacketCreate(3*sizeof(DWORD))) != NULL) {
			TlenP2PPacketSetType(packet, TLEN_FILE_PACKET_FILE_REQUEST); // file request
			TlenP2PPacketPackDword(packet, ft->currentFile);
			TlenP2PPacketPackDword(packet, 0);
			TlenP2PPacketPackDword(packet, 0);
			TlenP2PPacketSend(ft->s, packet);
			TlenP2PPacketFree(packet);

			fullFileName = (char *) malloc(strlen(ft->szSavePath) + strlen(ft->files[ft->currentFile]) + 2);
			strcpy(fullFileName, ft->szSavePath);
			if (fullFileName[strlen(fullFileName)-1] != '\\')
				strcat(fullFileName, "\\");
			strcat(fullFileName, ft->files[ft->currentFile]);
			ft->fileId = _open(fullFileName, _O_BINARY|_O_WRONLY|_O_CREAT|_O_TRUNC, _S_IREAD|_S_IWRITE);
			ft->fileReceivedBytes = 0;
			ft->fileTotalSize = ft->filesSize[ft->currentFile];
			JabberLog("Saving to [%s] [%d]", fullFileName, ft->filesSize[ft->currentFile]);
			free(fullFileName);
			ft->state = FT_RECEIVING;
			JabberLog("Change to FT_RECEIVING");
		}
		else {
			ft->state = FT_ERROR;
		}
	} 
	else if (ft->state == FT_RECEIVING) {
		PROTOFILETRANSFERSTATUS pfts;
		memset(&pfts, 0, sizeof(PROTOFILETRANSFERSTATUS));
		pfts.cbSize = sizeof(PROTOFILETRANSFERSTATUS);
		pfts.hContact = ft->hContact;
		pfts.sending = FALSE;
		pfts.files = ft->files;
		pfts.totalFiles = ft->fileCount;
		pfts.currentFileNumber = ft->currentFile;
		pfts.totalBytes = ft->allFileTotalSize;
		pfts.workingDir = NULL;
		pfts.currentFile = ft->files[ft->currentFile];
		pfts.currentFileSize = ft->filesSize[ft->currentFile];
		pfts.currentFileTime = 0;
		JabberLog("Receiving data...");
		while (ft->state == FT_RECEIVING) {
			rpacket = TlenP2PPacketReceive(ft->s);
			if (rpacket != NULL) {
				p = rpacket->packet;
				if (rpacket->type == TLEN_FILE_PACKET_FILE_DATA) { // file data
					int writeSize;
					writeSize = rpacket->len - 2 * sizeof(DWORD) ; // skip file offset
					if (_write(ft->fileId, p + 2 * sizeof(DWORD), writeSize) != writeSize) {
						ft->state = FT_ERROR;
					}
					else {
						ft->fileReceivedBytes += writeSize;
						ft->allFileReceivedBytes += writeSize;
						pfts.totalProgress = ft->allFileReceivedBytes;
						pfts.currentFileProgress = ft->fileReceivedBytes;
						ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_DATA, ft, (LPARAM) &pfts);
					}
				}
				else if (rpacket->type == TLEN_FILE_PACKET_END_OF_FILE) { // end of file
					_close(ft->fileId);
					JabberLog("Finishing this file...");
					if (ft->currentFile >= ft->fileCount-1) {
						ft->state = FT_DONE;
					}
					else {
						ft->currentFile++;
						ft->state = FT_INITIALIZING;
						JabberLog("File received, advancing to the next file...");
						ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_NEXTFILE, ft, 0);
					}
				}
				TlenP2PPacketFree(rpacket);
			}
			else {
				ft->state = FT_ERROR;
			}
		}
	}
}


void __cdecl TlenFileSendingThread(TLEN_FILE_TRANSFER *ft)
{
	JABBER_SOCKET s = NULL;
	HANDLE hEvent;
	char *nick;

	JabberLog("Thread started: type=tlen_file_send");
	ft->mode = FT_SEND;
	s = TlenFileListen(ft);
	if (s != NULL) {
		ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTING, ft, 0);
		ft->s = s;
		//JabberLog("ft->s = %d", s);
		//JabberLog("fileCount = %d", ft->fileCount);

		hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		ft->hFileEvent = hEvent;
		ft->currentFile = 0;
		ft->state = FT_CONNECTING;

		nick = JabberNickFromJID(ft->jid);
		JabberSend(jabberThreadInfo->s, "<f t='%s' i='%s' e='6' a='%s' p='%d'/>", nick, ft->iqId, ft->httpHostName, ft->httpPort);
		free(nick);
		JabberLog("Waiting for the file to be sent...");
		WaitForSingleObject(hEvent, INFINITE);
		ft->hFileEvent = NULL;
		CloseHandle(hEvent);
		JabberLog("Finish all files");
		Netlib_CloseHandle(s);
		ft->s = NULL;
		JabberLog("ft->s is NULL");

		if (ft->state == FT_SWITCH) {
			NETLIBOPENCONNECTION nloc;
			JABBER_SOCKET s;
			JabberLog("Sending as client...");
			ft->state = FT_CONNECTING;
			nloc.cbSize = NETLIBOPENCONNECTION_V1_SIZE;//sizeof(NETLIBOPENCONNECTION);
			nloc.szHost = ft->httpHostName;
			nloc.wPort = ft->httpPort;
			nloc.flags = 0;
			s = (HANDLE) CallService(MS_NETLIB_OPENCONNECTION, (WPARAM) hNetlibUser, (LPARAM) &nloc);
			if (s != NULL) {
				ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTING, ft, 0);
				ft->s = s;
				TlenP2PEstablishOutgoingConnection(ft, TRUE);
				JabberLog("Entering send loop for this file connection...");
				while (ft->state!=FT_DONE && ft->state!=FT_ERROR) {
					TlenFileSendParse(ft);
				}
				if (ft->state==FT_DONE)
					ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
				else
					ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
				JabberLog("Closing connection for this file transfer... ");
				Netlib_CloseHandle(s);
			} else {
				ft->state = FT_ERROR;
				nick = JabberNickFromJID(ft->jid);
				JabberSend(jabberThreadInfo->s, "<f t='%s' i='%s' e='8'/>", nick, ft->iqId);
				free(nick);
			}
		}
	} else {
		JabberLog("Cannot allocate port to bind for file server thread, thread ended.");
		ft->state = FT_ERROR;
	}
	JabberListRemove(LIST_FILE, ft->iqId);
	switch (ft->state) {
	case FT_DONE:
		JabberLog("Finish successfully");
		ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
		break;
	case FT_DENIED:
		ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_DENIED, ft, 0);
		break;
	default: // FT_ERROR:
		JabberLog("Finish with errors");
		ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		break;
	}
	JabberLog("Thread ended: type=file_send");
	TlenFileFreeFt(ft);
}

static void TlenFileSendingConnection(JABBER_SOCKET hConnection, DWORD dwRemoteIP, void * pExtra)
{
	JABBER_SOCKET slisten;
	TLEN_FILE_TRANSFER *ft;

	ft = TlenP2PEstablishIncomingConnection(hConnection, LIST_FILE, TRUE);
	if (ft != NULL) {
		slisten = ft->s;
		ft->s = hConnection;
		JabberLog("Set ft->s to %d (saving %d)", hConnection, slisten);

		JabberLog("Entering send loop for this file connection... (ft->s is hConnection)");
		while (ft->state!=FT_DONE && ft->state!=FT_ERROR) {
			TlenFileSendParse(ft);
		}
		if (ft->state==FT_DONE)
			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
		else
			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		JabberLog("Closing connection for this file transfer... (ft->s is now hBind)");
		ft->s = slisten;
		JabberLog("ft->s is restored to %d", ft->s);
	}
	Netlib_CloseHandle(hConnection);
	if (ft!=NULL && ft->hFileEvent != NULL)
		SetEvent(ft->hFileEvent);
}

static void TlenFileSendParse(TLEN_FILE_TRANSFER *ft)
{
	int i;
	char *p, *t;
	int currentFile, numRead;
	char *fileBuffer;
	TLEN_FILE_PACKET *rpacket, *packet;


	if (ft->state == FT_CONNECTING) {
		char filename[256];	// Must be 256 (0x100)
		if ((packet=TlenP2PPacketCreate(sizeof(DWORD)+(ft->fileCount*(sizeof(filename)+sizeof(DWORD))))) != NULL) {
			// Must pause a bit, sending these two packets back to back
			// will break the session because the receiver cannot take it :)
			SleepEx(1000, TRUE);
			TlenP2PPacketSetLen(packet, 0); // Reuse packet
			TlenP2PPacketSetType(packet, TLEN_FILE_PACKET_FILE_LIST);
			TlenP2PPacketPackDword(packet, (DWORD) ft->fileCount);
			for (i=0; i<ft->fileCount; i++) {
//					struct _stat statbuf;
//					_stat(ft->files[i], &statbuf);
//					TlenP2PPacketPackDword(packet, statbuf.st_size);
				TlenP2PPacketPackDword(packet, ft->filesSize[i]);
				memset(filename, 0, sizeof(filename));
				if ((t=strrchr(ft->files[i], '\\')) != NULL)
					t++;
				else
					t = ft->files[i];
				_snprintf(filename, sizeof(filename)-1, t);
				TlenP2PPacketPackBuffer(packet, filename, sizeof(filename));
			}
			TlenP2PPacketSend(ft->s, packet);
			TlenP2PPacketFree(packet);

			ft->allFileReceivedBytes = 0;
			ft->state = FT_INITIALIZING;
			JabberLog("Change to FT_INITIALIZING");
		}
		else {
			ft->state = FT_ERROR;
		}
	}
	else if (ft->state == FT_INITIALIZING) {	// FT_INITIALIZING
		rpacket = TlenP2PPacketReceive(ft->s);
		if (rpacket == NULL) {
			ft->state = FT_ERROR;
			return;
		}
		p = rpacket->packet;
		// TYPE: TLEN_FILE_PACKET_FILE_LIST_ACK	will be ignored
		// LEN: 0
		if (rpacket->type == TLEN_FILE_PACKET_FILE_LIST_ACK) {

		}
		// Then the receiver will request each file
		// TYPE: TLEN_FILE_PACKET_REQUEST
		// LEN:
		// (DWORD) file number
		// (DWORD) 0
		// (DWORD) 0
		else if (rpacket->type == TLEN_FILE_PACKET_FILE_REQUEST) {
			PROTOFILETRANSFERSTATUS pfts;
			//struct _stat statbuf;

			currentFile = *((DWORD*)p);
			if (currentFile != ft->currentFile) {
				JabberLog("Requested file (#%d) is invalid (must be %d)", currentFile, ft->currentFile);
				ft->state = FT_ERROR;
			}
			else {
			//	_stat(ft->files[currentFile], &statbuf);	// file size in statbuf.st_size
				JabberLog("Sending [%s] [%d]", ft->files[currentFile], ft->filesSize[currentFile]);
				if ((ft->fileId=_open(ft->files[currentFile], _O_BINARY|_O_RDONLY)) < 0) {
					JabberLog("File cannot be opened");
					ft->state = FT_ERROR;
				} 
				else  {
					memset(&pfts, 0, sizeof(PROTOFILETRANSFERSTATUS));
					pfts.cbSize = sizeof(PROTOFILETRANSFERSTATUS);
					pfts.hContact = ft->hContact;
					pfts.sending = TRUE;
					pfts.files = ft->files;
					pfts.totalFiles = ft->fileCount;
					pfts.currentFileNumber = ft->currentFile;
					pfts.totalBytes = ft->allFileTotalSize;
					pfts.workingDir = NULL;
					pfts.currentFile = ft->files[ft->currentFile];
					pfts.currentFileSize = ft->filesSize[ft->currentFile]; //statbuf.st_size;
					pfts.currentFileTime = 0;
					ft->fileReceivedBytes = 0;
					if ((packet = TlenP2PPacketCreate(2*sizeof(DWORD)+2048)) == NULL) {
						ft->state = FT_ERROR;
					}
					else {
						TlenP2PPacketSetType(packet, TLEN_FILE_PACKET_FILE_DATA);
						fileBuffer = (char *) malloc(2048);
						JabberLog("Sending file data...");
						while ((numRead=_read(ft->fileId, fileBuffer, 2048)) > 0) {
							TlenP2PPacketSetLen(packet, 0); // Reuse packet
							TlenP2PPacketPackDword(packet, (DWORD) ft->fileReceivedBytes);
							TlenP2PPacketPackDword(packet, 0);
							TlenP2PPacketPackBuffer(packet, fileBuffer, numRead);
							if (TlenP2PPacketSend(ft->s, packet)) {
								ft->fileReceivedBytes += numRead;
								ft->allFileReceivedBytes += numRead;
								pfts.totalProgress = ft->allFileReceivedBytes;
								pfts.currentFileProgress = ft->fileReceivedBytes;
								ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_DATA, ft, (LPARAM) &pfts);
							}
							else {
								ft->state = FT_ERROR;
								break;
							}
						}
						free(fileBuffer);
						_close(ft->fileId);
						if (ft->state != FT_ERROR) {
							if (ft->currentFile >= ft->fileCount-1)
								ft->state = FT_DONE;
							else {
								ft->currentFile++;
								ft->state = FT_INITIALIZING;
								JabberLog("File sent, advancing to the next file...");
								ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_NEXTFILE, ft, 0);
							}
						}
						JabberLog("Finishing this file...");
						TlenP2PPacketSetLen(packet, 0); // Reuse packet
						TlenP2PPacketSetType(packet, TLEN_FILE_PACKET_END_OF_FILE);
						TlenP2PPacketPackDword(packet, currentFile);
						TlenP2PPacketSend(ft->s, packet);
						TlenP2PPacketFree(packet);
					}
				}
			}
			TlenP2PPacketFree(rpacket);
		}
		else {
			ft->state = FT_ERROR;
		}
	}
}

int TlenFileCancelAll()
{
	JABBER_LIST_ITEM *item;
	TLEN_FILE_TRANSFER *ft;
	HANDLE hEvent;
	int i;

	JabberLog("Invoking FileCancelAll()");
	i = 0;
	while ((i=JabberListFindNext(LIST_FILE, 0)) >=0 ) {
		if ((item=JabberListGetItemPtrFromIndex(i)) != NULL) {
			ft = item->ft;
			JabberListRemoveByIndex(i);
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
		}
	}
	return 0;
}

