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
#include <mmsystem.h>
#include <math.h>
#include <m_button.h>
//#include <win2k.h>
#include "resource.h"
#include "codec/gsm.h"
#include "jabber_list.h"
#include "tlen_voice.h"

extern char *jabberProtoName;
extern HANDLE hNetlibUser;
extern struct ThreadData *jabberThreadInfo;
extern DWORD jabberLocalIP;

static int modeFrequency[]= {0, 0, 8000, 11025, 22050, 44100};
static int modeFrameSize[]= {0, 0, 5, 5, 10, 25};

typedef struct {
	int			waveMode;
	int			codec;
	int			bDisable;

	short		*recordingData;
	short		*waveData;
	WAVEHDR		*waveHeaders;
	int			waveFrameSize;
	int			waveHeadersPos;
	int			waveHeadersNum;

//	HANDLE		hEvent;
	HANDLE		hThread;
	DWORD		threadID;
	HWAVEOUT	hWaveOut;
	HWAVEIN		hWaveIn;
	int			isRunning;
	int			stopThread;
	gsm_state	*gsmstate;
	JABBER_FILE_TRANSFER *ft;
	int			vuMeter;
	int			bytesSum;
} TLEN_VOICE_CONTROL;

typedef struct {
	unsigned int maxDataLen;
	char *packet;
	DWORD type;
	DWORD len;
} TLEN_FILE_PACKET;

static void TlenVoiceReceiveParse(JABBER_FILE_TRANSFER *ft);
static void TlenVoiceSendParse(JABBER_FILE_TRANSFER *ft);
static void TlenVoiceReceivingConnection(HANDLE hNewConnection, DWORD dwRemoteIP);

static HWND voiceDlgHWND = NULL;
static HWND voiceAcceptDlgHWND = NULL;
static TLEN_VOICE_CONTROL *playbackControl = NULL;
static TLEN_VOICE_CONTROL *recordingControl = NULL;
static int availPlayback = 0;
static int availLimit = 0;
static int availLimitMin = 2;
static int availLimitMax = 4;

static int vuMeterHeight = 64;
static int vuMeterWidth = 20;
static int vuMeterLevels = 16;
static HBITMAP vuMeterBitmaps[100];


static void CALLBACK TlenVoicePlaybackCallback(HWAVEOUT hwo, UINT uMsg, DWORD* dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	if (uMsg == WOM_DONE) {
		//TLEN_VOICE_CONTROL *control = (TLEN_VOICE_CONTROL *) dwInstance;
		waveOutUnprepareHeader(hwo, (WAVEHDR *) dwParam1, sizeof(WAVEHDR));
		if (availPlayback > 0) {
			availPlayback--;
		}
	}
}

static DWORD WINAPI TlenVoiceRecordingThreadProc(TLEN_VOICE_CONTROL *control)
{
	MSG		msg;
	HWAVEIN hWaveIn;
	WAVEHDR *hWaveHdr;
	control->isRunning = 1;
	control->stopThread = 0;
	while (TRUE) {
		GetMessage(&msg,NULL,0,0);
		if (msg.message == MM_WIM_DATA) {
//			JabberLog("recording thread running...%d", msg.message);
			hWaveIn = (HWAVEIN) msg.wParam;
			hWaveHdr = (WAVEHDR *) msg.lParam;
			waveInUnprepareHeader(hWaveIn, hWaveHdr, sizeof(WAVEHDR));
			if (hWaveHdr->dwBytesRecorded>0 && !control->bDisable) {
				control->recordingData = (short *)hWaveHdr->lpData;
				TlenVoiceSendParse(control->ft);
			}
			if (!control->stopThread) {
				waveInPrepareHeader(hWaveIn, &control->waveHeaders[control->waveHeadersPos], sizeof(WAVEHDR));
				waveInAddBuffer(hWaveIn, &control->waveHeaders[control->waveHeadersPos], sizeof(WAVEHDR));
				control->waveHeadersPos = (control->waveHeadersPos +1) % control->waveHeadersNum;
			}
		}
		else if (msg.message == MM_WIM_CLOSE) {
			break;
		}
	}
	control->isRunning = 0;
	JabberLog("recording thread ended...");
	return 0;
}


/*static void CALLBACK TlenVoiceRecordingCallback(HWAVEIN hwi, UINT uMsg, DWORD* dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	if (uMsg == WIM_DATA) {
		TLEN_VOICE_CONTROL *control = (TLEN_VOICE_CONTROL *) dwInstance;
		if (control->waveHeaders[control->waveHeadersPos].dwBytesRecorded>0) {
			control->recordingData = (short *)control->waveHeaders[control->waveHeadersPos].lpData;
			TlenVoiceSendParse(control->ft);
		}
		waveInAddBuffer(control->hWaveIn, &control->waveHeaders[control->waveHeadersPos], sizeof(WAVEHDR));
		control->waveHeadersPos = (control->waveHeadersPos +1) % control->waveHeadersNum;
	}
}*/

static int TlenVoicePlaybackStart(TLEN_VOICE_CONTROL *control)
{
	WAVEFORMATEX wfm;
	MMRESULT mmres;
	int i, j;
	int iNumDevs, iSelDev;
	WAVEOUTCAPS     wic;

    memset(&wfm, 0, sizeof(wfm));
    wfm.cbSize          = sizeof(WAVEFORMATEX);
    wfm.nChannels       = 1;
    wfm.wBitsPerSample  = 16;
    wfm.nSamplesPerSec  = modeFrequency[control->codec];
    wfm.nAvgBytesPerSec = wfm.nSamplesPerSec * wfm.nChannels * wfm.wBitsPerSample/8;
    wfm.nBlockAlign     = 2 * wfm.nChannels;
    wfm.wFormatTag      = WAVE_FORMAT_PCM;

	control->waveMode	= 0;
	control->waveFrameSize = modeFrameSize[control->codec] * 160 * wfm.nChannels;// * wfm.wBitsPerSample / 8;
	control->waveHeadersPos=0;
	control->waveHeadersNum=4;

	j = DBGetContactSettingWord(NULL, jabberProtoName, "VoiceDeviceOut", 0);
	iSelDev = WAVE_MAPPER;
	if (j!=0) {
		iNumDevs = waveOutGetNumDevs();
		for (i = 0; i < iNumDevs; i++) {
			if (!waveOutGetDevCaps(i, &wic, sizeof(WAVEOUTCAPS))) {
				if (wic.dwFormats != 0) {
					j--;
					if (j == 0) {
						iSelDev = i;
						break;
					}
				}
			}
		}
	}
	if (!waveOutGetDevCaps(iSelDev, &wic, sizeof(WAVEOUTCAPS))) {
		JabberLog("Playback device ID #%u: %s\r\n", iSelDev, wic.szPname);
	}

	mmres = waveOutOpen(&control->hWaveOut, iSelDev, &wfm, (DWORD) &TlenVoicePlaybackCallback, (DWORD) control, CALLBACK_FUNCTION);
	if (mmres!=MMSYSERR_NOERROR) {
		JabberLog("TlenVoiceStart FAILED!");
		return 1;
	}
	control->waveData = (short *)malloc(control->waveHeadersNum * control->waveFrameSize * 2);
	memset(control->waveData, 0, control->waveHeadersNum * control->waveFrameSize * 2);
	control->waveHeaders = (WAVEHDR *)malloc(control->waveHeadersNum * sizeof(WAVEHDR));
	JabberLog("TlenVoiceStart OK!");
	return 0;
}


static int TlenVoiceRecordingStart(TLEN_VOICE_CONTROL *control)
{
	WAVEFORMATEX wfm;
	MMRESULT mmres;
	int i, j;
	int iNumDevs, iSelDev;
	WAVEINCAPS     wic;

    memset(&wfm, 0, sizeof(wfm));
    wfm.cbSize          = sizeof(WAVEFORMATEX);
    wfm.nChannels       = 1;
    wfm.wBitsPerSample  = 16;
    wfm.nSamplesPerSec  = modeFrequency[control->codec];
    wfm.nAvgBytesPerSec = wfm.nSamplesPerSec * wfm.nChannels * wfm.wBitsPerSample/8;
    wfm.nBlockAlign     = 2 * wfm.nChannels;
    wfm.wFormatTag      = WAVE_FORMAT_PCM;


	control->waveMode		= 0;
//	control->isRunning	= 1;
	control->waveFrameSize = modeFrameSize[control->codec] * 160 * wfm.nChannels;// * wfm.wBitsPerSample / 8;
	control->waveHeadersPos = 0;
	control->waveHeadersNum = 2;

	control->hThread = CreateThread( NULL,
                            0,
                            (LPTHREAD_START_ROUTINE)TlenVoiceRecordingThreadProc,
                            control,
                            0,
                            (LPDWORD)&control->threadID);


	SetThreadPriority(control->hThread, THREAD_PRIORITY_ABOVE_NORMAL);

	j = DBGetContactSettingWord(NULL, jabberProtoName, "VoiceDeviceIn", 0);
	iSelDev = WAVE_MAPPER;
	if (j!=0) {
		iNumDevs = waveInGetNumDevs();
		for (i = 0; i < iNumDevs; i++) {
			if (!waveInGetDevCaps(i, &wic, sizeof(WAVEINCAPS))) {
				if (wic.dwFormats != 0) {
					j--;
					if (j == 0) {
						iSelDev = i;
						break;
					}
				}
			}
		}
	}
	if (!waveInGetDevCaps(iSelDev, &wic, sizeof(WAVEINCAPS))) {
		JabberLog("Recording device ID #%u: %s\r\n", iSelDev, wic.szPname);
	}

	mmres = waveInOpen(&control->hWaveIn, iSelDev, &wfm, (DWORD) control->threadID, 0, CALLBACK_THREAD);
//	mmres = waveInOpen(&control->hWaveIn, 3, &wfm, (DWORD) &TlenVoiceRecordingCallback, (DWORD) control, CALLBACK_FUNCTION);
	if (mmres!=MMSYSERR_NOERROR) {
		PostThreadMessage(control->threadID, WIM_CLOSE, 0, 0);
		JabberLog("TlenVoiceStart FAILED %d!", mmres);
		return 1;
	}
	control->waveData = (short *)malloc(control->waveHeadersNum * control->waveFrameSize * 2);
	memset(control->waveData, 0, control->waveHeadersNum * control->waveFrameSize * 2);
	control->waveHeaders = (WAVEHDR *)malloc(control->waveHeadersNum * sizeof(WAVEHDR));
	for (i=0;i<control->waveHeadersNum;i++) {
		control->waveHeaders[i].dwFlags = 0;//WHDR_DONE;
		control->waveHeaders[i].lpData = (char *) (control->waveData + i * control->waveFrameSize);
		control->waveHeaders[i].dwBufferLength = control->waveFrameSize *2;
		mmres = waveInPrepareHeader(control->hWaveIn, &control->waveHeaders[i], sizeof(WAVEHDR));
		if (mmres!=MMSYSERR_NOERROR) {
			waveInClose(control->hWaveIn);
//			PostThreadMessage(control->threadID, WIM_CLOSE, 0, 0);
			JabberLog("TlenVoiceStart FAILED #2!");
			return 1;
	   	}
	}
	for (i=0;i<control->waveHeadersNum;i++) {
		waveInAddBuffer(control->hWaveIn, &control->waveHeaders[i], sizeof(WAVEHDR));
	}
	waveInStart(control->hWaveIn);
	JabberLog("TlenVoiceRStart OK!");
	return 0;
}

static TLEN_FILE_PACKET *TlenVoicePacketCreate(int datalen)
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

static void TlenVoicePacketFree(TLEN_FILE_PACKET *packet)
{
	if (packet != NULL) {
		if (packet->packet != NULL)
			free(packet->packet);
		free(packet);
	}
}

static void TlenVoicePacketSetType(TLEN_FILE_PACKET *packet, DWORD type)
{
	if (packet!=NULL) {
		packet->type = type;
	}
}

static void TlenVoicePacketSetLen(TLEN_FILE_PACKET *packet, DWORD len)
{
	if (packet!=NULL) {
		packet->len = len;
	}
}

static void TlenVoicePacketPackDword(TLEN_FILE_PACKET *packet, DWORD data)
{
	if (packet!=NULL && packet->packet!=NULL) {
		if (packet->len + sizeof(DWORD) <= packet->maxDataLen) {
			(*((DWORD*)((packet->packet)+(packet->len)))) = data;
			packet->len += sizeof(DWORD);
		}
		else {
			JabberLog("TlenVoicePacketPackDword() overflow");
		}
	}
}

static void TlenVoicePacketPackBuffer(TLEN_FILE_PACKET *packet, char *buffer, int len)
{
	if (packet!=NULL && packet->packet!=NULL) {
		if (packet->len + len <= packet->maxDataLen) {
			memcpy((packet->packet)+(packet->len), buffer, len);
			packet->len += len;
		}
		else {
			JabberLog("TlenVoicePacketPackBuffer() overflow");
		}
	}
}

static int TlenVoicePacketSend(JABBER_SOCKET s, TLEN_FILE_PACKET *packet)
{	DWORD sendResult;
	if (packet!=NULL && packet->packet!=NULL) {
		Netlib_Send(s, (char *)&packet->type, 4, MSG_NODUMP);
		Netlib_Send(s, (char *)&packet->len, 4, MSG_NODUMP);
		sendResult=Netlib_Send(s, packet->packet, packet->len, MSG_NODUMP);
		if (sendResult==SOCKET_ERROR || sendResult!=packet->len) return 0;
	}
	return 1;
}

static TLEN_FILE_PACKET* TlenVoicePacketReceive(JABBER_SOCKET s)
{
	TLEN_FILE_PACKET *packet;
	DWORD recvResult;
	DWORD type, len, pos;
	recvResult = Netlib_Recv(s, (char *)&type, 4, MSG_NODUMP);
	if (recvResult==0 || recvResult==SOCKET_ERROR) return NULL;
	recvResult = Netlib_Recv(s, (char *)&len, 4, MSG_NODUMP);
	if (recvResult==0 || recvResult==SOCKET_ERROR) return NULL;
	packet = TlenVoicePacketCreate(len);
	TlenVoicePacketSetType(packet, type);
	TlenVoicePacketSetLen(packet, len);
	pos = 0;
	while (len > 0) {
		recvResult = Netlib_Recv(s, packet->packet+pos, len, MSG_NODUMP);
		if (recvResult==0 || recvResult==SOCKET_ERROR) {
			TlenVoicePacketFree(packet);
			return NULL;
		}
		len -= recvResult;
		pos += recvResult;
	}
	return packet;
}

static void TlenVoiceFreeFt(JABBER_FILE_TRANSFER *ft)
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

static TLEN_VOICE_CONTROL *TlenVoiceCreateVC(int codec)
{
	TLEN_VOICE_CONTROL *vc;
	vc = (TLEN_VOICE_CONTROL *) malloc(sizeof (TLEN_VOICE_CONTROL));
	memset(vc, 0, sizeof(TLEN_VOICE_CONTROL));
	vc->gsmstate = gsm_create();
	vc->codec = codec;
	return vc;
}
static void TlenVoiceFreeVc(TLEN_VOICE_CONTROL *vc)
{
	int i;
	JabberLog("......... free vc .............");
	vc->stopThread = 1;
	if (vc->hWaveIn) {
		for (i=0;i<vc->waveHeadersNum;i++) {
			while(waveInUnprepareHeader(vc->hWaveIn, &vc->waveHeaders[i], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
				JabberLog("vc waiting %d #1",i );
				Sleep(50);
			}
		}
		while(waveInClose(vc->hWaveIn) == WAVERR_STILLPLAYING) {
			JabberLog("vc waiting #2");
			Sleep(50);
		}
	}
	JabberLog("......... free po in .............");
	if (vc->hWaveOut) {
		for (i=0;i<vc->waveHeadersNum;i++) {
			while(waveOutUnprepareHeader(vc->hWaveOut, &vc->waveHeaders[i], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
				JabberLog("vc waiting #3");
				Sleep(50);
			}
		}
		while(waveOutClose(vc->hWaveOut) == WAVERR_STILLPLAYING) {
			JabberLog("vc waiting #4");
			Sleep(50);
		}
	}
	JabberLog("......... free po out .............");
	if (vc->waveData) free(vc->waveData);
	if (vc->waveHeaders) free(vc->waveHeaders);
	if (vc->gsmstate) gsm_release(vc->gsmstate);
	while(vc->isRunning) {
		PostThreadMessage(vc->threadID, MM_WIM_CLOSE, 0, 0);
		Sleep(50);
	}
	if (vc->hThread!=NULL) CloseHandle(vc->hThread);
	JabberLog("......... free po watku .............");
	free(vc);
	JabberLog("......... free vc done .............");
//	Sleep(1500);
}

typedef struct {
	char	szHost[256];
	int		wPort;
	int		useAuth;
	char	szUser[256];
	char	szPassword[256];
}SOCKSBIND;


#define JABBER_NETWORK_BUFFER_SIZE 2048

static void TlenVoiceCrypt(char *buffer, int len)
{
	int i, j, k;
	j = 0x71;
	for (i=0;i<len;i++) {
		k = j;
		j = j << 6;
		j += k;
		j = k + (j << 1);
		//j = j * 131;
		j += 0xBB;/*
		j &= 0x800000FF;
		if (j<0) {
			j--;
			j |= 0xFFFFFF00;
			j++;
		}*/
		buffer[i]^=j;
	}
}

static JABBER_FILE_TRANSFER* TlenVoiceEstablishIncomingConnection(JABBER_SOCKET *s)
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
	packet = TlenVoicePacketReceive(s);
	if (packet == NULL || packet->type!=1 || packet->len<28) {
		if (packet!=NULL) {
			TlenVoicePacketFree(packet);
		}
		JabberLog("Unable to read first packet.");
		return NULL;
	}
	iqId = *((DWORD *)(packet->packet+sizeof(DWORD)));
	i = 0;
	while ((i=JabberListFindNext(LIST_VOICE, i)) >= 0) {
		if ((item=JabberListGetItemPtrFromIndex(i))!=NULL) {
			JabberLog("iter: %s", item->ft->jid);
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
	TlenVoicePacketFree(packet);
	if (i >=0) {
		if ((packet=TlenVoicePacketCreate(sizeof(DWORD))) != NULL) {
			// Send connection establishment acknowledgement
			TlenVoicePacketSetType(packet, 0x2);
			TlenVoicePacketPackDword(packet, (DWORD) atoi(item->ft->iqId));
			TlenVoicePacketSend(s, packet);
			TlenVoicePacketFree(packet);
			item->ft->state = FT_CONNECTING;
//			ProtoBroadcastAck(jabberProtoName, item->ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTED, item->ft, 0);
			return item->ft;
		}
	}
	return NULL;
}

static void TlenVoiceEstablishOutgoingConnection(JABBER_FILE_TRANSFER *ft)
{
	char *hash;
	char str[300];
	char username[128];
	TLEN_FILE_PACKET *packet;
	DBVARIANT dbv;
	JabberLog("Establishing outgoing connection.");
	ft->state = FT_ERROR;
	if ((packet = TlenVoicePacketCreate(2*sizeof(DWORD) + 20))!=NULL) {
		TlenVoicePacketSetType(packet, 1);
		TlenVoicePacketPackDword(packet, 1);
		TlenVoicePacketPackDword(packet, (DWORD) atoi(ft->iqId));
		username[0] = '\0';
		if (!DBGetContactSetting(NULL, jabberProtoName, "LoginName", &dbv)) {
			strncpy(username, dbv.pszVal, sizeof(username));
			username[sizeof(username)-1] = '\0';
			DBFreeVariant(&dbv);
		}
		_snprintf(str, sizeof(str), "%08X%s%d", atoi(ft->iqId), username, atoi(ft->iqId));
		hash = TlenSha1(str, strlen(str));
		TlenVoicePacketPackBuffer(packet, hash, 20);
		free(hash);
		TlenVoicePacketSend(ft->s, packet);
		TlenVoicePacketFree(packet);
		packet = TlenVoicePacketReceive(ft->s);
		if (packet != NULL) {
			if (packet->type == 2) { // acknowledge
				if ((int)(*((DWORD*)packet->packet)) == atoi(ft->iqId)) {
					ft->state = FT_CONNECTING;
//					ProtoBroadcastAck(jabberProtoName, item->ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTED, item->ft, 0);
				}
			}
			TlenVoicePacketFree(packet);
		}
	}
}

static void __cdecl TlenVoiceBindSocks4Thread(JABBER_FILE_TRANSFER* ft)
{
	BYTE buf[8];
	int status;

	JabberLog("Waiting for the file to be sent via SOCKS...");
	status = Netlib_Recv(ft->s, buf, 8, MSG_NODUMP);
	JabberLog("accepted connection !!!");
	if ( status == SOCKET_ERROR || status<8 || buf[1]!=90) {
		status = 1;
	} else {
		status = 0;
	}
	if (!status) {
		JabberLog("Entering recv loop for this file connection... (ft->s is hConnection)");
		if (TlenVoiceEstablishIncomingConnection(ft->s)!=NULL) {
			playbackControl = NULL;
			recordingControl = TlenVoiceCreateVC(3);
			recordingControl->ft = ft;
			TlenVoiceRecordingStart(recordingControl);
			while (ft->state!=FT_DONE && ft->state!=FT_ERROR) {
				TlenVoiceReceiveParse(ft);
			}
			TlenVoiceFreeVc(recordingControl);
			playbackControl = NULL;
			recordingControl = NULL;
		} else ft->state = FT_ERROR;
		if (ft->state==FT_DONE) {
//			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
		}
		else {
//			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		}
	} else {
		if (ft->state!=FT_SWITCH) {
//			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		}
	}
	JabberLog("Closing connection for this file transfer...");
//	Netlib_CloseHandle(ft->s);
	if (ft->hFileEvent != NULL)
		SetEvent(ft->hFileEvent);

}
static JABBER_SOCKET TlenVoiceBindSocks4(SOCKSBIND * sb, JABBER_FILE_TRANSFER *ft)
{	//rfc1928
	int len;
	BYTE buf[256];
	int status;
	struct in_addr in;
	NETLIBOPENCONNECTION nloc;
	JABBER_SOCKET s;
	JabberLog("connecting to SOCK4 proxy...%s:%d", sb->szHost, sb->wPort);

	nloc.cbSize = NETLIBOPENCONNECTION_V1_SIZE;//sizeof(NETLIBOPENCONNECTION);
	nloc.szHost = sb->szHost;
	nloc.wPort = sb->wPort;
	nloc.flags = 0;
	s = (HANDLE) CallService(MS_NETLIB_OPENCONNECTION, (WPARAM) hFileNetlibUser, (LPARAM) &nloc);
	if (s==NULL) {
		JabberLog("Connection failed (%d), thread ended", WSAGetLastError());
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
		JabberLog("Send failed (%d), thread ended", WSAGetLastError());
		Netlib_CloseHandle(s);
		return NULL;
	}
	status = Netlib_Recv(s, buf, 8, MSG_NODUMP);
	if (status==SOCKET_ERROR || status<8 || buf[1]!=90) {
		JabberLog("SOCKS4 negotiation failed");
		Netlib_CloseHandle(s);
		return NULL;
	}
	status = Netlib_Recv(s, buf, sizeof(buf), MSG_NODUMP);
	if ( status == SOCKET_ERROR || status<7 || buf[0]!=5 || buf[1]!=0) {
		JabberLog("SOCKS5 request failed");
		Netlib_CloseHandle(s);
		return NULL;
	}
	in.S_un.S_addr = *(PDWORD)(buf+4);
	strcpy(sb->szHost, inet_ntoa(in));
	sb->wPort = htons(*(PWORD)(buf+2));
	ft->s = s;
	JabberForkThread((void (__cdecl *)(void*))TlenVoiceBindSocks4Thread, 0, ft);
	return s;
}

static void __cdecl TlenVoiceBindSocks5Thread(JABBER_FILE_TRANSFER* ft)
{
	BYTE buf[256];
	int status;

	JabberLog("Waiting for the file to be sent via SOCKS...");
	status = Netlib_Recv(ft->s, buf, sizeof(buf), MSG_NODUMP);
	JabberLog("accepted connection !!!");
	if ( status == SOCKET_ERROR || status<7 || buf[1]!=0) {
		status = 1;
	} else {
		status = 0;
	}
	if (!status) {
		JabberLog("Entering recv loop for this file connection... (ft->s is hConnection)");
		if (TlenVoiceEstablishIncomingConnection(ft->s)!=NULL) {
			playbackControl = NULL;
			recordingControl = TlenVoiceCreateVC(3);
			recordingControl->ft = ft;
			TlenVoiceRecordingStart(recordingControl);
			while (ft->state!=FT_DONE && ft->state!=FT_ERROR) {
				TlenVoiceReceiveParse(ft);
			}
			TlenVoiceFreeVc(recordingControl);
			playbackControl = NULL;
			recordingControl = NULL;
		} else ft->state = FT_ERROR;
		if (ft->state==FT_DONE) {
//			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
		} else {
//			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		}
	} else {
		if (ft->state!=FT_SWITCH) {
//			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		}
	}
	JabberLog("Closing connection for this file transfer...");
//	Netlib_CloseHandle(ft->s);
	if (ft->hFileEvent != NULL)
		SetEvent(ft->hFileEvent);

}

static JABBER_SOCKET TlenVoiceBindSocks5(SOCKSBIND * sb, JABBER_FILE_TRANSFER *ft)
{	//rfc1928
	BYTE buf[512];
	int len, status;
	NETLIBOPENCONNECTION nloc;
	struct in_addr in;
	JABBER_SOCKET s;

	JabberLog("connecting to SOCK5 proxy...%s:%d", sb->szHost, sb->wPort);

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
			JabberLog("Send failed (%d), thread ended", WSAGetLastError());
			Netlib_CloseHandle(s);
			return NULL;
		}
	}
	status = Netlib_Recv(s, buf, sizeof(buf), MSG_NODUMP);
	if ( status == SOCKET_ERROR || status<7 || buf[0]!=5 || buf[1]!=0) {
		JabberLog("SOCKS5 request failed");
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
	JabberForkThread((void (__cdecl *)(void*))TlenVoiceBindSocks5Thread, 0, ft);
	return s;
}

static JABBER_SOCKET TlenVoiceListen(JABBER_FILE_TRANSFER *ft)
{
	NETLIBBIND nlb = {0};
	JABBER_SOCKET s = NULL;
	int	  useProxy;
	DBVARIANT dbv;
	SOCKSBIND sb;
	struct in_addr in;

	JabberLog("TlenVoiceListen");

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
					s = TlenVoiceBindSocks4(&sb, ft);
					useProxy = 2;
					break;
				case 2: // socks5
					s = TlenVoiceBindSocks5(&sb, ft);
					useProxy = 2;
					break;
			}
			ft->httpHostName = _strdup(sb.szHost);
			ft->httpPort = sb.wPort;
		}
	}
	if (useProxy<2) {
		nlb.cbSize = NETLIBBIND_SIZEOF_V1;//sizeof(NETLIBBIND);
		nlb.pfnNewConnection = TlenVoiceReceivingConnection;
		nlb.wPort = 0;	// User user-specified incoming port ranges, if available
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

void __cdecl TlenVoiceReceiveThread(JABBER_FILE_TRANSFER *ft)
{
	NETLIBOPENCONNECTION nloc;
	JABBER_SOCKET s;

	JabberLog("Thread started: type=file_receive server='%s' port='%d'", ft->httpHostName, ft->httpPort);
	nloc.cbSize = NETLIBOPENCONNECTION_V1_SIZE;//sizeof(NETLIBOPENCONNECTION);
	nloc.szHost = ft->httpHostName;
	nloc.wPort = ft->httpPort;
	nloc.flags = 0;
	SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Connecting...");
//	ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTING, ft, 0);
	s = (HANDLE) CallService(MS_NETLIB_OPENCONNECTION, (WPARAM) hNetlibUser, (LPARAM) &nloc);
	if (s != NULL) {
		ft->s = s;
		JabberLog("Entering file receive loop");
		TlenVoiceEstablishOutgoingConnection(ft);
		if (ft->state!=FT_ERROR) {
			playbackControl = NULL;
			recordingControl = TlenVoiceCreateVC(3);
			recordingControl->ft = ft;
			TlenVoiceRecordingStart(recordingControl);
			while (ft->state!=FT_DONE && ft->state!=FT_ERROR) {
				TlenVoiceReceiveParse(ft);
			}
			TlenVoiceFreeVc(recordingControl);
			playbackControl = NULL;
			recordingControl = NULL;
		}
		if (ft->s) {
			Netlib_CloseHandle(s);
		}
		ft->s = NULL;
	} else {
		JabberLog("Connection failed - receiving as server");
		s = TlenVoiceListen(ft);
		if (s != NULL) {
			HANDLE hEvent;
			char *nick;
			SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Waiting for connection...");
			ft->s = s;
			hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
			ft->hFileEvent = hEvent;
			ft->currentFile = 0;
			ft->state = FT_CONNECTING;
			nick = JabberNickFromJID(ft->jid);
			JabberSend(jabberThreadInfo->s, "<v t='%s' i='%s' e='7' a='%s' p='%d'/>", nick, ft->iqId, ft->httpHostName, ft->httpPort);
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
	JabberListRemove(LIST_VOICE, ft->iqId);
	if (ft->state==FT_DONE) {
		SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Finished...");
		//ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
	} else {
		SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Error...");
		//ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
	}
	JabberLog("Thread ended: type=file_receive server='%s'", ft->httpHostName);

	TlenVoiceFreeFt(ft);
}

static void TlenVoiceReceivingConnection(JABBER_SOCKET hConnection, DWORD dwRemoteIP)
{
	JABBER_SOCKET slisten;
	JABBER_FILE_TRANSFER *ft;

	ft = TlenVoiceEstablishIncomingConnection(hConnection);
	if (ft != NULL) {
		slisten = ft->s;
		ft->s = hConnection;
		JabberLog("Set ft->s to %d (saving %d)", hConnection, slisten);
		JabberLog("Entering send loop for this file connection... (ft->s is hConnection)");
		playbackControl = NULL;
		recordingControl = TlenVoiceCreateVC(3);
		recordingControl->ft = ft;
		TlenVoiceRecordingStart(recordingControl);
		while (ft->state!=FT_DONE && ft->state!=FT_ERROR) {
			TlenVoiceReceiveParse(ft);
		}
		TlenVoiceFreeVc(recordingControl);
		playbackControl = NULL;
		recordingControl = NULL;
		if (ft->state==FT_DONE) {
			SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Finished...");
//			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
		} else {
//			ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
			SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Error...");
		}
		JabberLog("Closing connection for this file transfer... (ft->s is now hBind)");
		ft->s = slisten;
		JabberLog("ft->s is restored to %d", ft->s);
	}
	Netlib_CloseHandle(hConnection);
	if (ft!=NULL && ft->hFileEvent != NULL)
		SetEvent(ft->hFileEvent);
}

static void TlenVoiceReceiveParse(JABBER_FILE_TRANSFER *ft)
{
	char *statusTxt;
	int i, j;
	char *p;
	float val;
	TLEN_FILE_PACKET  *packet;
//	while (availPlayback > 2) {
//		SleepEx(5, FALSE);
//	}
	packet = TlenVoicePacketReceive(ft->s);
	if (packet != NULL) {
		statusTxt = " OK ";
		p = packet->packet;
		if (packet->type == 0x96) {
			short *out;
			int codec, chunkNum;
			TlenVoiceCrypt(packet->packet+4, packet->len-4);
			codec = *((int *)packet->packet);
			if (codec<2 || codec>5) {
				statusTxt = " Unknown codec ";
			} else {
				if (playbackControl == NULL) {
					playbackControl = TlenVoiceCreateVC(codec);
					TlenVoicePlaybackStart(playbackControl);
					availPlayback = 0;
					availLimit = availLimitMax;
				} else if (playbackControl->codec != codec) {
					TlenVoiceFreeVc(playbackControl);
					playbackControl = TlenVoiceCreateVC(codec);
					TlenVoicePlaybackStart(playbackControl);
					availPlayback = 0;
					availLimit = availLimitMax;
				}
				if (!playbackControl->bDisable) {
					playbackControl->waveHeaders[playbackControl->waveHeadersPos].dwFlags =  WHDR_DONE;
					playbackControl->waveHeaders[playbackControl->waveHeadersPos].lpData = (char *) (playbackControl->waveData + playbackControl->waveHeadersPos * playbackControl->waveFrameSize);
					playbackControl->waveHeaders[playbackControl->waveHeadersPos].dwBufferLength = playbackControl->waveFrameSize * 2;
					if (availPlayback == 0) {
						statusTxt = "!! Buffer is empty !!";
						availPlayback++;
						waveOutPrepareHeader(playbackControl->hWaveOut, &playbackControl->waveHeaders[playbackControl->waveHeadersPos], sizeof(WAVEHDR));
						waveOutWrite(playbackControl->hWaveOut, &playbackControl->waveHeaders[playbackControl->waveHeadersPos], sizeof(WAVEHDR));
						playbackControl->waveHeadersPos = (playbackControl->waveHeadersPos +1) % playbackControl->waveHeadersNum;
						playbackControl->waveHeaders[playbackControl->waveHeadersPos].dwFlags =  WHDR_DONE;
						playbackControl->waveHeaders[playbackControl->waveHeadersPos].lpData = (char *) (playbackControl->waveData + playbackControl->waveHeadersPos * playbackControl->waveFrameSize);
						playbackControl->waveHeaders[playbackControl->waveHeadersPos].dwBufferLength = playbackControl->waveFrameSize * 2;
					}
					chunkNum = min(modeFrameSize[codec], (int)(packet->len - 4) / 33);
					out = (short *)playbackControl->waveHeaders[playbackControl->waveHeadersPos].lpData;
					//JabberLog("Pos: %d Avail: %d ChunkNum: %d Limit: %d  out:%08X", playbackControl->waveHeadersPos, availPlayback,chunkNum, availLimit, out); //playbackControl->waveHeadersAvail,
					for (i=0; i<chunkNum; i++) {
						for (j=0;j<33;j++) {
							playbackControl->gsmstate->gsmFrame[j] = packet->packet[i*33 +j +4];
						}
						gsm_decode(playbackControl->gsmstate, out);
						out += 160;
					}
					out = (short *)playbackControl->waveHeaders[playbackControl->waveHeadersPos].lpData;
					val = 0;
					for (i=0; i<modeFrameSize[codec] * 160; i+=modeFrameSize[codec]) {
						val += out[i]*out[i];
					}
					j = (int)((log10(val) - 4) * 2.35);
					if (j > vuMeterLevels - 1 ) {
						j = vuMeterLevels - 1;
					} else if (j<0) {
						j = 0;
					}
					playbackControl->vuMeter = j;
					playbackControl->bytesSum  += 8 + packet->len;
					if (availPlayback < availLimit) {
						availPlayback++;
						if (availPlayback == availLimitMax) {
							availLimit = availLimitMin;
						} else {
							availLimit = availLimitMax;
						}
						waveOutPrepareHeader(playbackControl->hWaveOut, &playbackControl->waveHeaders[playbackControl->waveHeadersPos], sizeof(WAVEHDR));
						waveOutWrite(playbackControl->hWaveOut, &playbackControl->waveHeaders[playbackControl->waveHeadersPos], sizeof(WAVEHDR));
						playbackControl->waveHeadersPos = (playbackControl->waveHeadersPos +1) % playbackControl->waveHeadersNum;
					} else {
						statusTxt = "!! Buffer is full !!";
					}
				}
			}
		}
		SetDlgItemText(voiceDlgHWND, IDC_STATUS, statusTxt);
		TlenVoicePacketFree(packet);
	}
	else {
		if (playbackControl!=NULL) {
			TlenVoiceFreeVc(playbackControl);
			playbackControl = NULL;
		}
		ft->state = FT_ERROR;
	}
}


void __cdecl TlenVoiceSendingThread(JABBER_FILE_TRANSFER *ft)
{
	JABBER_SOCKET s = NULL;
	HANDLE hEvent;
	char *nick;

	JabberLog("Thread started: type=voice_send");
	s = TlenVoiceListen(ft);
	if (s != NULL) {
		SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Waiting for connection...");
		//ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTING, ft, 0);
		ft->s = s;
		//JabberLog("ft->s = %d", s);
		//JabberLog("fileCount = %d", ft->fileCount);

		hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		ft->hFileEvent = hEvent;
		ft->currentFile = 0;
		ft->state = FT_CONNECTING;

		nick = JabberNickFromJID(ft->jid);
		JabberSend(jabberThreadInfo->s, "<v t='%s' i='%s' e='6' a='%s' p='%d'/>", nick, ft->iqId, ft->httpHostName, ft->httpPort);
		free(nick);
		JabberLog("Waiting for the voice data to be sent...");
		WaitForSingleObject(hEvent, INFINITE);
		ft->hFileEvent = NULL;
		CloseHandle(hEvent);
		JabberLog("Finish voice");
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
				SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Connecting...");
				//ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTING, ft, 0);
				ft->s = s;
				TlenVoiceEstablishOutgoingConnection(ft);
				if (ft->state!=FT_ERROR) {
					JabberLog("Entering send loop for this file connection...");
					playbackControl = NULL;
					recordingControl = TlenVoiceCreateVC(3);
					recordingControl->ft = ft;
					TlenVoiceRecordingStart(recordingControl);
					while (ft->state!=FT_DONE && ft->state!=FT_ERROR) {
						TlenVoiceReceiveParse(ft);
					}
				}
				if (ft->state==FT_DONE) {
				//	ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
				} else {
				//	ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
				}
				JabberLog("Closing connection for this file transfer... ");
				Netlib_CloseHandle(s);
			} else {
				ft->state = FT_ERROR;
			}
		}
	} else {
		JabberLog("Cannot allocate port to bind for file server thread, thread ended.");
		ft->state = FT_ERROR;
	}
	JabberListRemove(LIST_VOICE, ft->iqId);
	switch (ft->state) {
	case FT_DONE:
		JabberLog("Finish successfully");
		SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Finished...");
		//ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
		break;
	case FT_DENIED:
		SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Denied...");
		//ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_DENIED, ft, 0);
		break;
	default: // FT_ERROR:
		JabberLog("Finish with errors");
		SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Error...");
		//ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		break;
	}
	JabberLog("Thread ended: type=voice_send");
	TlenVoiceFreeFt(ft);
}

static void TlenVoiceSendParse(JABBER_FILE_TRANSFER *ft)
{
	int codec, i;
	TLEN_FILE_PACKET *packet;

	codec = recordingControl->codec;
	if ((packet=TlenVoicePacketCreate(sizeof(DWORD)+modeFrameSize[codec]*33)) != NULL) {
		short *in;
		float val;
		in = recordingControl->recordingData;
		TlenVoicePacketSetType(packet, 0x96);
		packet->packet[0] = codec;
		TlenVoicePacketPackDword(packet, codec);
		val = 0;
		for (i=0; i<modeFrameSize[codec] * 160; i+=modeFrameSize[codec]) {
			val += in[i]*in[i];
		}
		i = (int)((log10(val) - 4) * 2.35);
		if (i > vuMeterLevels - 1 ) {
			i = vuMeterLevels - 1;
		} else if (i<0) {
			i = 0;
		}
		recordingControl->vuMeter = i;
		for (i=0; i<modeFrameSize[codec]; i++) {
			gsm_encode(recordingControl->gsmstate, in + i * 160);
			TlenVoicePacketPackBuffer(packet, recordingControl->gsmstate->gsmFrame, 33);
		}
		TlenVoiceCrypt(packet->packet+4, packet->len-4);
		if (!TlenVoicePacketSend(ft->s, packet)) {
			ft->state = FT_ERROR;
		}
		recordingControl->bytesSum  += 8 + packet->len;
		TlenVoicePacketFree(packet);
	} else {
		ft->state = FT_ERROR;
	}
}

int TlenVoiceCancelAll()
{
	JABBER_LIST_ITEM *item;
	JABBER_FILE_TRANSFER *ft;
	HANDLE hEvent;
	int i;

	JabberLog("Invoking VoiceCancelAll()");
	i = 0;
	while ((i=JabberListFindNext(LIST_VOICE, 0)) >=0 ) {
		if ((item=JabberListGetItemPtrFromIndex(i)) != NULL) {
			ft = item->ft;
			JabberLog("Closing ft->s = %d", ft->s);
			JabberListRemoveByIndex(i);
			if (ft->s) {
				//ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
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
	JabberLog("VoiceCancelAll() done");
	if (voiceDlgHWND !=NULL) {
		EndDialog(voiceDlgHWND, 0);
	}
	return 0;
}

int TlenVoiceContactMenuHandleVoice(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact;
	DBVARIANT dbv;
	JABBER_LIST_ITEM *item;
	JABBER_FILE_TRANSFER *ft;
	if (!jabberOnline) {
		return 1;
	}
	if ((hContact=(HANDLE) wParam)!=NULL && jabberOnline) {
		if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
			char serialId[32];
			sprintf(serialId, "%d", JabberSerialNext());
			if ((item = JabberListAdd(LIST_VOICE, serialId)) != NULL) {
				ft = (JABBER_FILE_TRANSFER *) malloc(sizeof(JABBER_FILE_TRANSFER));
				memset(ft, 0, sizeof(JABBER_FILE_TRANSFER));
				ft->iqId = _strdup(serialId);
				ft->jid = JabberNickFromJID(dbv.pszVal);
				item->ft = ft;
//				JabberSend(jabberThreadInfo->s, "<iq to='%s'><query xmlns='voip'><voip k='1' s='1' v='1' i='51245604'/></query></iq>", ft->jid);
//				Sleep(5000);
				TlenVoiceStart(NULL, 2);
				JabberSend(jabberThreadInfo->s, "<v t='%s' e='1' i='%s' v='1'/>", ft->jid, serialId);
			}
			DBFreeVariant(&dbv);
		}
	}
	return 0;
}

int TlenVoiceIsInUse() {
	if (JabberListFindNext(LIST_VOICE, 0) >= 0 || voiceDlgHWND!=NULL) {
		return 1;
	}
	return 0;
}

int TlenVoicePrebuildContactMenu(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact;
	DBVARIANT dbv;
	CLISTMENUITEM clmi = {0};
	JABBER_LIST_ITEM *item;
	clmi.cbSize = sizeof(CLISTMENUITEM);
	clmi.flags = CMIM_FLAGS|CMIF_HIDDEN;
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactVoice, (LPARAM) &clmi);
	if ((hContact=(HANDLE) wParam)!=NULL && jabberOnline) {
		if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
			if ((item=JabberListGetItemPtr(LIST_ROSTER, dbv.pszVal)) != NULL) {
				if (item->status!=ID_STATUS_OFFLINE && !TlenVoiceIsInUse()) {
					clmi.flags = CMIM_FLAGS;
					CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuContactVoice, (LPARAM) &clmi);
					DBFreeVariant(&dbv);
					return 0;
				}
			}
			DBFreeVariant(&dbv);
		}
	}
	return 0;
}

static HBITMAP TlenVoiceMakeBitmap(int w, int h, int bpp, void *ptr)
{
	BITMAPINFO	   bmih;
	HBITMAP 	   hbm;
	HDC 		   hdc;
	bmih.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmih.bmiHeader.biWidth = w&0xFFFFFFFC;
	bmih.bmiHeader.biHeight = h;//&0xFFFFFFFC;
	bmih.bmiHeader.biPlanes = 1;			   // musi byc 1
	bmih.bmiHeader.biBitCount = bpp;
	bmih.bmiHeader.biCompression = BI_RGB;
	bmih.bmiHeader.biSizeImage = 0;
	bmih.bmiHeader.biXPelsPerMeter = 0;
	bmih.bmiHeader.biYPelsPerMeter = 0;
	bmih.bmiHeader.biClrUsed = 0;
	bmih.bmiHeader.biClrImportant = 0;
	hdc = CreateDC("DISPLAY", NULL, NULL, NULL);
	hbm = CreateDIBitmap(hdc, (PBITMAPINFOHEADER) &bmih, CBM_INIT, ptr, &bmih, DIB_RGB_COLORS);
	ReleaseDC(NULL,hdc);
	return hbm;
}

static void TlenVoiceInitVUMeters()
{
	int i, v, y, x, x0, col, col0;
	unsigned char *pBits;
	int ledWidth, ledHeight;
	ledWidth = 9;
	ledHeight = 6;
	vuMeterHeight = ledHeight;
	vuMeterWidth = (vuMeterLevels-1) * ledWidth;
	vuMeterWidth = (vuMeterWidth + 3) & (~3);
	pBits = (unsigned char *)malloc(3*vuMeterWidth*vuMeterHeight);
	memset(pBits, 0x80, 3*vuMeterWidth*vuMeterHeight);
	for (i=0;i<vuMeterLevels;i++) {
		for (v=0;v<vuMeterLevels-1;v++) {
			if (v>=i) {
				if (v < 10) col0 = 0x104010;
				else if (v<13) col0 = 0x404010;
				else 	col0 = 0x401010;
			} else {
				if (v < 10) col0 = 0x00f000;
				else if (v<13) col0 = 0xf0f000;
				else col0 = 0xf00000;
			}
			x0 = v * ledWidth;
			for (y=1;y<vuMeterHeight-1;y++) {
				col = col0;
				for (x=1;x<ledWidth;x++) {
					pBits[3*(x+x0+y*vuMeterWidth)] = col &0xFF;
					pBits[3*(x+x0+y*vuMeterWidth)+1] = (col>>8) &0xFF;
					pBits[3*(x+x0+y*vuMeterWidth)+2] = (col>>16) &0xFF;
				}
			}
		}
		vuMeterBitmaps[i] = TlenVoiceMakeBitmap(vuMeterWidth, vuMeterHeight, 24, pBits);
	}
	free(pBits);
}

static BOOL CALLBACK TlenVoiceDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HDC hDC, hMemDC;
	int v;
	static int counter;
	switch (msg) {
	case WM_INITDIALOG:
		voiceDlgHWND = hwndDlg;
		TranslateDialogDefault(hwndDlg);
		SendDlgItemMessage(hwndDlg, IDC_VCQUALITY, CB_ADDSTRING, 0, (LPARAM) Translate("8000 Hz / 13.8 kbps"));
		SendDlgItemMessage(hwndDlg, IDC_VCQUALITY, CB_ADDSTRING, 0, (LPARAM) Translate("11025 Hz / 19.1 kbps"));
		SendDlgItemMessage(hwndDlg, IDC_VCQUALITY, CB_ADDSTRING, 0, (LPARAM) Translate("22050 Hz / 36.8 kbps"));
		SendDlgItemMessage(hwndDlg, IDC_VCQUALITY, CB_ADDSTRING, 0, (LPARAM) Translate("44100 Hz / 72 kbps"));
		SendDlgItemMessage(hwndDlg, IDC_VCQUALITY, CB_SETCURSEL, 1, 0);
		SendDlgItemMessage(hwndDlg, IDC_MICROPHONE, BUTTONSETASFLATBTN, 0, 0);
		SendDlgItemMessage(hwndDlg, IDC_SPEAKER, BUTTONSETASFLATBTN, 0, 0);
		SendDlgItemMessage(hwndDlg, IDC_MICROPHONE, BUTTONSETASPUSHBTN, 0, 0);
		SendDlgItemMessage(hwndDlg, IDC_SPEAKER, BUTTONSETASPUSHBTN, 0, 0);
		SendDlgItemMessage(hwndDlg, IDC_MICROPHONE, BM_SETIMAGE, IMAGE_ICON, (LPARAM) tlenIcons[TLEN_IDI_MICROPHONE]);
		SendDlgItemMessage(hwndDlg, IDC_SPEAKER, BM_SETIMAGE, IMAGE_ICON, (LPARAM) tlenIcons[TLEN_IDI_SPEAKER]);
		CheckDlgButton(hwndDlg, IDC_MICROPHONE, TRUE);
		CheckDlgButton(hwndDlg, IDC_SPEAKER, TRUE);
		TlenVoiceInitVUMeters();
		SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...???...");
		counter = 0;
		SetTimer(hwndDlg, 1, 100, NULL);
		return FALSE;
	case WM_TIMER:
		if (recordingControl != NULL && !recordingControl->bDisable) {
			v = recordingControl->vuMeter % vuMeterLevels;
			if (recordingControl->vuMeter >0) {
				recordingControl->vuMeter--;
			}
		} else {
			v = 0;
		}
		hDC = GetDC(GetDlgItem(hwndDlg, IDC_VUMETERIN));
		if (NULL != (hMemDC = CreateCompatibleDC( hDC ))) {
			SelectObject( hMemDC, vuMeterBitmaps[v]) ;
			BitBlt( hDC, 0, 0, vuMeterWidth, vuMeterHeight, hMemDC, 0, 0, SRCCOPY ) ;
			DeleteDC(hMemDC);
		}
		ReleaseDC(GetDlgItem(hwndDlg, IDC_PLAN), hDC);
		if (playbackControl != NULL  && !playbackControl->bDisable) {
			v = playbackControl->vuMeter % vuMeterLevels;
			if (playbackControl->vuMeter >0) {
				playbackControl->vuMeter--;
			}
		} else {
			v = 0;
		}
		hDC = GetDC(GetDlgItem(hwndDlg, IDC_VUMETEROUT));
		if (NULL != (hMemDC = CreateCompatibleDC( hDC ))) {
			SelectObject( hMemDC, vuMeterBitmaps[v]) ;
			BitBlt( hDC, 0, 0, vuMeterWidth, vuMeterHeight, hMemDC, 0, 0, SRCCOPY ) ;
			DeleteDC(hMemDC);
		}
		ReleaseDC(GetDlgItem(hwndDlg, IDC_PLAN), hDC);
		counter ++;
		if (counter %10 == 0) {
			char str[50];
			float fv;
			if (recordingControl != NULL) {
				fv = (float)recordingControl->bytesSum;
				recordingControl->bytesSum = 0;
			} else {
				fv = 0;
			}
			sprintf(str, "%.1f kB/s", fv / 1024);
			SetDlgItemText(hwndDlg, IDC_BYTESOUT, str);
			if (playbackControl != NULL) {
				fv = (float)playbackControl->bytesSum;
				playbackControl->bytesSum = 0;
			} else {
				fv = 0;
			}
			sprintf(str, "%.1f kB/s", fv / 1024);
			SetDlgItemText(hwndDlg, IDC_BYTESIN, str);
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			EndDialog(hwndDlg, 0);
			return TRUE;
		case IDC_VCQUALITY:
			if (HIWORD(wParam)==CBN_SELCHANGE) {
				if (recordingControl!=NULL) {
					int codec;
					codec = SendDlgItemMessage(hwndDlg, IDC_VCQUALITY, CB_GETCURSEL, 0, 0) + 2;
					if (codec!=recordingControl->codec && codec>1 && codec<6) {
						JABBER_FILE_TRANSFER *ft = recordingControl->ft;
						TlenVoiceFreeVc(recordingControl);
						recordingControl = TlenVoiceCreateVC(codec);
						recordingControl->ft = ft;
						TlenVoiceRecordingStart(recordingControl);
					}
				}
			}
		case IDC_MICROPHONE:
			if (recordingControl!=NULL) {
				recordingControl->bDisable = !IsDlgButtonChecked(hwndDlg, IDC_MICROPHONE);
			}
			break;
		case IDC_SPEAKER:
			if (playbackControl!=NULL) {
				playbackControl->bDisable = !IsDlgButtonChecked(hwndDlg, IDC_SPEAKER);
			}
			break;
		}
		break;
	case WM_CLOSE:
		EndDialog(hwndDlg, 0);
		break;
	}
	return FALSE;
}

static void __cdecl TlenVoiceDlgThread(void *ptr)
{
	DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_VOICE), NULL, TlenVoiceDlgProc, (LPARAM) NULL);
	voiceDlgHWND = NULL;
	TlenVoiceCancelAll();
}

int TlenVoiceStart(JABBER_FILE_TRANSFER *ft, int mode)
{

	if (mode==0) {
		JabberForkThread((void (__cdecl *)(void*))TlenVoiceReceiveThread, 0, ft);
	} else if (mode==1) {
		JabberForkThread((void (__cdecl *)(void*))TlenVoiceSendingThread, 0, ft);
	} else {
		JabberForkThread((void (__cdecl *)(void*))TlenVoiceDlgThread, 0, ft);
	}
	return 0;
}

static char *getDisplayName(const char *id)
{
	char jid[256];
	HANDLE hContact;
	DBVARIANT dbv;
	if (!DBGetContactSetting(NULL, jabberProtoName, "LoginServer", &dbv)) {
		_snprintf(jid, sizeof(jid), "%s@%s", id, dbv.pszVal);
		DBFreeVariant(&dbv);
		if ((hContact=JabberHContactFromJID(jid)) != NULL) {
			return _strdup((char *) CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM) hContact, 0));
		}
	}
	return _strdup(id);
}

static BOOL CALLBACK TlenVoiceAcceptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	JABBER_LIST_ITEM * item;
	char *str;
	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		voiceAcceptDlgHWND = hwndDlg;
		item = (JABBER_LIST_ITEM *) lParam;
		str = getDisplayName(item->nick);
		SetDlgItemText(hwndDlg, IDC_FROM, str);
		free(str);
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ACCEPT:
			EndDialog(hwndDlg, 1);
			return TRUE;
		case IDCANCEL:
		case IDCLOSE:
			EndDialog(hwndDlg, 0);
			return TRUE;
		}
		break;
	case WM_CLOSE:
		EndDialog(hwndDlg, 0);
		break;
	}
	return FALSE;
}

static void __cdecl TlenVoiceAcceptDlgThread(void *ptr)
{
	JABBER_LIST_ITEM * item;
	int result = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_ACCEPT_VOICE), NULL, TlenVoiceAcceptDlgProc, (LPARAM) ptr);
	item = (JABBER_LIST_ITEM *)ptr;
	if (result) {
		item->ft = (JABBER_FILE_TRANSFER *) malloc(sizeof(JABBER_FILE_TRANSFER));
		memset(item->ft, 0, sizeof(JABBER_FILE_TRANSFER));
		item->ft->iqId = _strdup(item->jid);
		item->ft->jid = _strdup(item->nick);
		TlenVoiceStart(NULL, 2);
		if (jabberOnline) {
			JabberSend(jabberThreadInfo->s, "<v t='%s' i='%s' e='5' v='1'/>", item->nick, item->jid);
		}
	} else {
		if (jabberOnline) {
			JabberSend(jabberThreadInfo->s, "<v t='%s' i='%s' e='4' />", item->nick, item->jid);
		}
		JabberListRemove(LIST_VOICE, item->jid);
	}
}

int TlenVoiceAccept(const char *id, const char *from)
{
	JABBER_LIST_ITEM * item;
	if (!TlenVoiceIsInUse()) {
		if ((item = JabberListAdd(LIST_VOICE, id)) != NULL) {
			item->nick = _strdup(from);
			JabberForkThread((void (__cdecl *)(void*))TlenVoiceAcceptDlgThread, 0, item);
			return 1;
		}
	}
	return 0;
}

int TlenVoiceBuildInDeviceList(HWND hWnd)
{	int i, j, iNumDevs;
	WAVEINCAPS     wic;
	iNumDevs = waveInGetNumDevs();
	SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)Translate("Default"));
	for (i = j = 0; i < iNumDevs; i++) {
		if (!waveInGetDevCaps(i, &wic, sizeof(WAVEINCAPS))) {
			if (wic.dwFormats != 0) {
				SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)wic.szPname);
				j++;
			}
		}
	}
	i = DBGetContactSettingWord(NULL, jabberProtoName, "VoiceDeviceIn", 0);
	if (i>j) i = 0;
	SendMessage(hWnd, CB_SETCURSEL, i, 0);
	return 0;
}

int TlenVoiceBuildOutDeviceList(HWND hWnd)
{	int i, j, iNumDevs;
	WAVEOUTCAPS  woc;
	iNumDevs = waveInGetNumDevs();
	SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)Translate("Default"));
	for (i = j = 0; i < iNumDevs; i++) {
		if (!waveOutGetDevCaps(i, &woc, sizeof(WAVEOUTCAPS))) {
			if (woc.dwFormats != 0) {
				SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)woc.szPname);
				j++;
			}
		}
	}
	i = DBGetContactSettingWord(NULL, jabberProtoName, "VoiceDeviceOut", 0);
	if (i>j) i = 0;
	SendMessage(hWnd, CB_SETCURSEL, i, 0);
	return 0;
}
