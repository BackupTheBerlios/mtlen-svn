/*

Jabber Protocol Plugin for Miranda IM
Tlen Protocol Plugin for Miranda IM
Copyright (C) 2002-2004  Santithorn Bunchua, Piotr Piastucki

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

#ifndef _TLEN_VOICE_H_
#define _TLEN_VOICE_H_

#include <windows.h>

extern void __cdecl TlenVoiceSendingThread(JABBER_FILE_TRANSFER *ft);
extern void __cdecl TlenVoiceReceiveThread(JABBER_FILE_TRANSFER *ft);
extern int TlenVoiceIsInUse();
extern int TlenVoiceContactMenuHandleVoice(WPARAM wParam, LPARAM lParam);
extern int TlenVoiceCancelAll();
extern int TlenVoiceStart(JABBER_FILE_TRANSFER *ft, int mode) ;
extern int TlenVoiceAccept(const char *id, const char *from);
extern int TlenVoiceBuildInDeviceList(HWND hWnd);
extern int TlenVoiceBuildOutDeviceList(HWND hWnd);
#endif

