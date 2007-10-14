/*

Tlen Protocol Plugin for Miranda IM
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
#include "tlen_p2p_old.h"

extern char *jabberProtoName;
extern HANDLE hNetlibUser;
extern struct ThreadData *jabberThreadInfo;

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
	TLEN_FILE_TRANSFER *ft;
	int			vuMeter;
	int			bytesSum;
} TLEN_VOICE_CONTROL;

static void TlenVoiceReceiveParse(TLEN_FILE_TRANSFER *ft);
static void TlenVoiceSendParse(TLEN_FILE_TRANSFER *ft);
static void TlenVoiceReceivingConnection(HANDLE hNewConnection, DWORD dwRemoteIP, void * pExtra);
static HWND voiceDlgHWND = NULL;
static HWND voiceAcceptDlgHWND = NULL;
static TLEN_VOICE_CONTROL *playbackControl = NULL;
static TLEN_VOICE_CONTROL *recordingControl = NULL;
static int availPlayback = 0;
static int availOverrun = 0;
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
//		playbackControl->waveHeaders[playbackControl->waveHeadersPos].dwFlags =  WHDR_DONE;
//		playbackControl->waveHeaders[playbackControl->waveHeadersPos].lpData = (char *) (playbackControl->waveData + playbackControl->waveHeadersPos * playbackControl->waveFrameSize);
//		playbackControl->waveHeaders[playbackControl->waveHeadersPos].dwBufferLength = playbackControl->waveFrameSize * 2;
//		waveOutPrepareHeader(playbackControl->hWaveOut, &playbackControl->waveHeaders[playbackControl->waveHeadersPos], sizeof(WAVEHDR));
//		waveOutWrite(playbackControl->hWaveOut, &playbackControl->waveHeaders[playbackControl->waveHeadersPos], sizeof(WAVEHDR));

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
	control->waveHeadersPos = 0;
	control->waveHeadersNum = availLimitMin + 2;

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
	control->waveData = (short *)mir_alloc(control->waveHeadersNum * control->waveFrameSize * 2);
	memset(control->waveData, 0, control->waveHeadersNum * control->waveFrameSize * 2);
	control->waveHeaders = (WAVEHDR *)mir_alloc(control->waveHeadersNum * sizeof(WAVEHDR));
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
	control->waveData = (short *)mir_alloc(control->waveHeadersNum * control->waveFrameSize * 2);
	memset(control->waveData, 0, control->waveHeadersNum * control->waveFrameSize * 2);
	control->waveHeaders = (WAVEHDR *)mir_alloc(control->waveHeadersNum * sizeof(WAVEHDR));
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


static TLEN_VOICE_CONTROL *TlenVoiceCreateVC(int codec)
{
	TLEN_VOICE_CONTROL *vc;
	vc = (TLEN_VOICE_CONTROL *) mir_alloc(sizeof (TLEN_VOICE_CONTROL));
	memset(vc, 0, sizeof(TLEN_VOICE_CONTROL));
	vc->gsmstate = gsm_create();
	vc->codec = codec;
	return vc;
}
static void TlenVoiceFreeVc(TLEN_VOICE_CONTROL *vc)
{
	int i;
	JabberLog("-> TlenVoiceFreeVc");
	vc->stopThread = 1;
	PostThreadMessage(vc->threadID, MM_WIM_CLOSE, 0, 0);
	while(vc->isRunning) {
		Sleep(50);
	}
	if (vc->hThread!=NULL) CloseHandle(vc->hThread);
	if (vc->hWaveIn) {
		for (i=0;i<vc->waveHeadersNum;i++) {
			while(waveInUnprepareHeader(vc->hWaveIn, &vc->waveHeaders[i], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
				Sleep(50);
			}
		}
		while(waveInClose(vc->hWaveIn) == WAVERR_STILLPLAYING) {
			Sleep(50);
		}
	}
	if (vc->hWaveOut) {
		for (i=0;i<vc->waveHeadersNum;i++) {
			while(waveOutUnprepareHeader(vc->hWaveOut, &vc->waveHeaders[i], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
				Sleep(50);
			}
		}
		while(waveOutClose(vc->hWaveOut) == WAVERR_STILLPLAYING) {
			Sleep(50);
		}
	}
	if (vc->waveData) mir_free(vc->waveData);
	if (vc->waveHeaders) mir_free(vc->waveHeaders);
	if (vc->gsmstate) gsm_release(vc->gsmstate);
	mir_free(vc);
	JabberLog("<- TlenVoiceFreeVc");
}

static void TlenVoiceCrypt(char *buffer, int len)
{
	int i, j, k;
	j = 0x71;
	for (i=0;i<len;i++) {
		k = j;
		j = j << 6;
		j += k;
		j = k + (j << 1);
		j += 0xBB;
		buffer[i]^=j;
	}
}

void __cdecl TlenVoiceReceiveThread(TLEN_FILE_TRANSFER *ft)
{
	NETLIBOPENCONNECTION nloc;
	JABBER_SOCKET s;

	JabberLog("Thread started: type=file_receive server='%s' port='%d'", ft->hostName, ft->wPort);
	nloc.cbSize = NETLIBOPENCONNECTION_V1_SIZE;//sizeof(NETLIBOPENCONNECTION);
	nloc.szHost = ft->hostName;
	nloc.wPort = ft->wPort;
	nloc.flags = 0;
	SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Connecting...");
//	ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTING, ft, 0);
	s = (HANDLE) CallService(MS_NETLIB_OPENCONNECTION, (WPARAM) hNetlibUser, (LPARAM) &nloc);
	if (s != NULL) {
		ft->s = s;
		JabberLog("Entering file receive loop");
		TlenP2PEstablishOutgoingConnection(ft, FALSE);
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
		ft->pfnNewConnectionV2 = TlenVoiceReceivingConnection;
		s = TlenP2PListen(ft);
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
			JabberSend(jabberThreadInfo->s, "<v t='%s' i='%s' e='7' a='%s' p='%d'/>", nick, ft->iqId, ft->localName, ft->wLocalPort);
			mir_free(nick);
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
		char *nick;
		nick = JabberNickFromJID(ft->jid);
		JabberSend(jabberThreadInfo->s, "<f t='%s' i='%s' e='8'/>", nick, ft->iqId);
		mir_free(nick);
		SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Error...");
		//ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
	}
	JabberLog("Thread ended: type=file_receive server='%s'", ft->hostName);

	TlenP2PFreeFileTransfer(ft);
}

static void TlenVoiceReceivingConnection(JABBER_SOCKET hConnection, DWORD dwRemoteIP, void * pExtra)
{
	JABBER_SOCKET slisten;
	TLEN_FILE_TRANSFER *ft;

	ft = TlenP2PEstablishIncomingConnection(hConnection, LIST_VOICE, FALSE);
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

static void TlenVoiceReceiveParse(TLEN_FILE_TRANSFER *ft)
{
	char *statusTxt;
	int i, j;
	char *p;
	float val;
	TLEN_FILE_PACKET  *packet;
	packet = TlenP2PPacketReceive(ft->s);
	if (packet != NULL) {
		statusTxt = " Unknown packet ";
		p = packet->packet;
		if (packet->type == TLEN_VOICE_PACKET) {
			short *out;
			int codec, chunkNum;
			statusTxt = " OK ";
			TlenVoiceCrypt(packet->packet+4, packet->len-4);
			codec = *((int *)packet->packet);
			if (codec<2 || codec>5) {
				statusTxt = " Unknown codec ";
			} else {
				if (playbackControl == NULL) {
					playbackControl = TlenVoiceCreateVC(codec);
					TlenVoicePlaybackStart(playbackControl);
					availPlayback = 0;
					availOverrun = 0;
					availLimit = availLimitMax;
				} else if (playbackControl->codec != codec) {
					TlenVoiceFreeVc(playbackControl);
					playbackControl = TlenVoiceCreateVC(codec);
					TlenVoicePlaybackStart(playbackControl);
					availPlayback = 0;
					availOverrun = 0;
					availLimit = availLimitMax;
				}
				if (!playbackControl->bDisable) {
					playbackControl->waveHeaders[playbackControl->waveHeadersPos].dwFlags =  WHDR_DONE;
					playbackControl->waveHeaders[playbackControl->waveHeadersPos].lpData = (char *) (playbackControl->waveData + playbackControl->waveHeadersPos * playbackControl->waveFrameSize);
					playbackControl->waveHeaders[playbackControl->waveHeadersPos].dwBufferLength = playbackControl->waveFrameSize * 2;
						/*
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
						*/
					chunkNum = min(modeFrameSize[codec], (int)(packet->len - 4) / 33);
					out = (short *)playbackControl->waveHeaders[playbackControl->waveHeadersPos].lpData;
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
					/* Simple logic to avoid huge delays. If a delay is detected a frame is not played */
					j = availOverrun > 0 ? -1 : 0;
					while (availPlayback > availLimitMin) {
						j = 1;
						SleepEx(5, FALSE);
					}
					availOverrun += j;
					/* 40 frames - 800ms/8kHz */
					if (availOverrun < 40) {
						availPlayback++;
						waveOutPrepareHeader(playbackControl->hWaveOut, &playbackControl->waveHeaders[playbackControl->waveHeadersPos], sizeof(WAVEHDR));
						waveOutWrite(playbackControl->hWaveOut, &playbackControl->waveHeaders[playbackControl->waveHeadersPos], sizeof(WAVEHDR));
						playbackControl->waveHeadersPos = (playbackControl->waveHeadersPos +1) % playbackControl->waveHeadersNum;
					} else {
						availOverrun -= 10;
						statusTxt = "!! Skipping frame !!";
					}
				/*
					if (availPlayback < availLimit) {
						availPlayback++;
						if (availPlayback == availLimitMax) {
							availLimit = availLimitMin;
						} else {
							availLimit = availLimitMax;
						}
						waveOutPrepareHeader(playbackControl->hWaveOut, &playbackControl->waveHeaders[playbackControl->waveHeadersPos], sizeof(WAVEHDR));
						waveOutWrite(playbackControl->hWaveOut, &playbackControl->waveHeaders[playbackControl->waveHeadersPos], sizeof(WAVEHDR));
//						playbackAlive--;
						playbackControl->waveHeadersPos = (playbackControl->waveHeadersPos +1) % playbackControl->waveHeadersNum;
					} else {
						statusTxt = "!! Buffer is full !!";
					}
					*/
				}
			}
		}
		{
			char ttt[2048];
			sprintf(ttt, "%s %d %d ", statusTxt, availPlayback, availOverrun);
			SetDlgItemText(voiceDlgHWND, IDC_STATUS, ttt);
		}
		TlenP2PPacketFree(packet);
	}
	else {
		if (playbackControl!=NULL) {
			TlenVoiceFreeVc(playbackControl);
			playbackControl = NULL;
		}
		ft->state = FT_ERROR;
	}
}


void __cdecl TlenVoiceSendingThread(TLEN_FILE_TRANSFER *ft)
{
	JABBER_SOCKET s = NULL;
	HANDLE hEvent;
	char *nick;

	JabberLog("Thread started: type=voice_send");
	ft->pfnNewConnectionV2 = TlenVoiceReceivingConnection;
	s = TlenP2PListen(ft);
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
		JabberSend(jabberThreadInfo->s, "<v t='%s' i='%s' e='6' a='%s' p='%d'/>", nick, ft->iqId, ft->localName, ft->wLocalPort);
		mir_free(nick);
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
			nloc.szHost = ft->hostName;
			nloc.wPort = ft->wPort;
			nloc.flags = 0;
			s = (HANDLE) CallService(MS_NETLIB_OPENCONNECTION, (WPARAM) hNetlibUser, (LPARAM) &nloc);
			if (s != NULL) {
				SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Connecting...");
				//ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTING, ft, 0);
				ft->s = s;
				TlenP2PEstablishOutgoingConnection(ft, FALSE);
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
		nick = JabberNickFromJID(ft->jid);
		JabberSend(jabberThreadInfo->s, "<v t='%s' i='%s' e='8'/>", nick, ft->iqId);
		mir_free(nick);
		JabberLog("Finish with errors");
		SetDlgItemText(voiceDlgHWND, IDC_STATUS, "...Error...");
		//ProtoBroadcastAck(jabberProtoName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		break;
	}
	JabberLog("Thread ended: type=voice_send");
	TlenP2PFreeFileTransfer(ft);
}

static void TlenVoiceSendParse(TLEN_FILE_TRANSFER *ft)
{
	int codec, i;
	TLEN_FILE_PACKET *packet;

	codec = recordingControl->codec;
	if ((packet=TlenP2PPacketCreate(sizeof(DWORD)+modeFrameSize[codec]*33)) != NULL) {
		short *in;
		float val;
		in = recordingControl->recordingData;
		TlenP2PPacketSetType(packet, 0x96);
		packet->packet[0] = codec;
		TlenP2PPacketPackDword(packet, codec);
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
			TlenP2PPacketPackBuffer(packet, recordingControl->gsmstate->gsmFrame, 33);
		}
		TlenVoiceCrypt(packet->packet+4, packet->len-4);
		if (!TlenP2PPacketSend(ft->s, packet)) {
			ft->state = FT_ERROR;
		}
		recordingControl->bytesSum  += 8 + packet->len;
		TlenP2PPacketFree(packet);
	} else {
		ft->state = FT_ERROR;
	}
}

int TlenVoiceCancelAll()
{
	JABBER_LIST_ITEM *item;
	TLEN_FILE_TRANSFER *ft;
	HANDLE hEvent;
	int i;

	JabberLog("Invoking VoiceCancelAll()");
	i = 0;
	while ((i=JabberListFindNext(LIST_VOICE, 0)) >=0 ) {
		if ((item=JabberListGetItemPtrFromIndex(i)) != NULL) {
			ft = item->ft;
			JabberListRemoveByIndex(i);
			if (ft != NULL) {
				if (ft->s) {
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
				} else {
					JabberLog("freeing (V) ft struct");
					TlenP2PFreeFileTransfer(ft);
				}
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
	TLEN_FILE_TRANSFER *ft;
	if (!jabberOnline) {
		return 1;
	}
	if ((hContact=(HANDLE) wParam)!=NULL && jabberOnline) {
		if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
			char serialId[32];
			sprintf(serialId, "%d", JabberSerialNext());
			if ((item = JabberListAdd(LIST_VOICE, serialId)) != NULL) {
				ft = (TLEN_FILE_TRANSFER *) mir_alloc(sizeof(TLEN_FILE_TRANSFER));
				memset(ft, 0, sizeof(TLEN_FILE_TRANSFER));
				ft->iqId = mir_strdup(serialId);
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
	pBits = (unsigned char *)mir_alloc(3*vuMeterWidth*vuMeterHeight);
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
	mir_free(pBits);
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
		SendDlgItemMessage(hwndDlg, IDC_VCQUALITY, CB_ADDSTRING, 0, (LPARAM) TranslateT("8000 Hz / 13.8 kbps"));
		SendDlgItemMessage(hwndDlg, IDC_VCQUALITY, CB_ADDSTRING, 0, (LPARAM) TranslateT("11025 Hz / 19.1 kbps"));
		SendDlgItemMessage(hwndDlg, IDC_VCQUALITY, CB_ADDSTRING, 0, (LPARAM) TranslateT("22050 Hz / 36.8 kbps"));
		SendDlgItemMessage(hwndDlg, IDC_VCQUALITY, CB_ADDSTRING, 0, (LPARAM) TranslateT("44100 Hz / 72 kbps"));
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
						TLEN_FILE_TRANSFER *ft = recordingControl->ft;
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

int TlenVoiceStart(TLEN_FILE_TRANSFER *ft, int mode)
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
			return mir_strdup((char *) CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM) hContact, 0));
		}
	}
	return mir_strdup(id);
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
		mir_free(str);
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
	if (result && jabberOnline) {
		item->ft = (TLEN_FILE_TRANSFER *) mir_alloc(sizeof(TLEN_FILE_TRANSFER));
		memset(item->ft, 0, sizeof(TLEN_FILE_TRANSFER));
		item->ft->iqId = mir_strdup(item->jid);
		item->ft->jid = mir_strdup(item->nick);
		TlenVoiceStart(NULL, 2);
		JabberSend(jabberThreadInfo->s, "<v t='%s' i='%s' e='5' v='1'/>", item->nick, item->jid);
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
			int ask, ignore, voiceChatPolicy;
			ask = TRUE;
			ignore = FALSE;
			voiceChatPolicy = DBGetContactSettingWord(NULL, jabberProtoName, "VoiceChatPolicy", 0);
			if (voiceChatPolicy == TLEN_MUC_ASK) {
				ignore = FALSE;
				ask = TRUE;
			} else if (voiceChatPolicy == TLEN_MUC_IGNORE_ALL) {
				ignore = TRUE;
			} else if (voiceChatPolicy == TLEN_MUC_IGNORE_NIR) {
				char jid[256];
				JABBER_LIST_ITEM *item;
				DBVARIANT dbv;
				if (!DBGetContactSetting(NULL, jabberProtoName, "LoginServer", &dbv)) {
					_snprintf(jid, sizeof(jid), "%s@%s", from, dbv.pszVal);
					DBFreeVariant(&dbv);
				} else {
					strcpy(jid, from);
				}
				item = JabberListGetItemPtr(LIST_ROSTER, jid);
				ignore = FALSE;
				if (item == NULL) ignore = TRUE;
				else if (item->subscription==SUB_NONE || item->subscription==SUB_TO) ignore = TRUE;
				ask = TRUE;
			} else if (voiceChatPolicy == TLEN_MUC_ACCEPT_IR) {
				char jid[256];
				JABBER_LIST_ITEM *item;
				DBVARIANT dbv;
				if (!DBGetContactSetting(NULL, jabberProtoName, "LoginServer", &dbv)) {
					_snprintf(jid, sizeof(jid), "%s@%s", from, dbv.pszVal);
					DBFreeVariant(&dbv);
				} else {
					strcpy(jid, from);
				}
				item = JabberListGetItemPtr(LIST_ROSTER, jid);
				ask = FALSE;
				if (item == NULL) ask = TRUE;
				else if (item->subscription==SUB_NONE || item->subscription==SUB_TO) ask = TRUE;
				ignore = FALSE;
			} else if (voiceChatPolicy == TLEN_MUC_ACCEPT_ALL) {
				ask = FALSE;
				ignore = FALSE;
			}
			if (ignore) {
				if (jabberOnline) {
					JabberSend(jabberThreadInfo->s, "<v t='%s' i='%s' e='4' />", from, id);
				}
				JabberListRemove(LIST_VOICE, id);
			} else {
				item->nick = mir_strdup(from);
				if (ask) {
					JabberForkThread((void (__cdecl *)(void*))TlenVoiceAcceptDlgThread, 0, item);
				} else if (jabberOnline) {
					item->ft = (TLEN_FILE_TRANSFER *) mir_alloc(sizeof(TLEN_FILE_TRANSFER));
					memset(item->ft, 0, sizeof(TLEN_FILE_TRANSFER));
					item->ft->iqId = mir_strdup(id);
					item->ft->jid = mir_strdup(from);
					TlenVoiceStart(NULL, 2);
					JabberSend(jabberThreadInfo->s, "<v t='%s' i='%s' e='5' v='1'/>", item->nick, item->jid);
				}
			}
			return 1;
		}
	}
	return 0;
}

int TlenVoiceBuildInDeviceList(HWND hWnd)
{	int i, j, iNumDevs;
	WAVEINCAPS     wic;
	iNumDevs = waveInGetNumDevs();
	SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)TranslateT("Default"));
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
	SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)TranslateT("Default"));
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
