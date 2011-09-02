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

#ifndef _TLEN_MUC_H_
#define _TLEN_MUC_H_

#include <windows.h>
#include "m_mucc.h"

#define USER_FLAGS_OWNER			0x01
#define USER_FLAGS_ADMIN			0x02
#define USER_FLAGS_REGISTERED		0x04
#define USER_FLAGS_GLOBALOWNER		0x08
#define USER_FLAGS_KICKED			0x80

extern BOOL TlenMUCInit(TlenProtocol *proto);
extern INT_PTR TlenMUCMenuHandleMUC(void *ptr, WPARAM wParam, LPARAM lParam);
extern INT_PTR TlenMUCMenuHandleChats(void *ptr, WPARAM wParam, LPARAM lParam);
extern INT_PTR TlenMUCContactMenuHandleMUC(void *ptr, WPARAM wParam, LPARAM lParam);
extern int TlenMUCCreateWindow(TlenProtocol *proto, const char *roomID, const char *roomName, int roomFlags, const char *nick, const char *iqId);
extern int TlenMUCRecvInvitation(TlenProtocol *proto, const char *roomJid, const char *roomName, const char *from, const char *reason);
extern int TlenMUCRecvPresence(TlenProtocol *proto, const char *from, int status, int flags,  const char *kick);
extern int TlenMUCRecvMessage(TlenProtocol *proto, const char *from, long timestamp, XmlNode *bodyNode);
extern int TlenMUCRecvTopic(TlenProtocol *proto, const char *from, const char *subject);
extern int TlenMUCRecvError(TlenProtocol *proto, const char *from, XmlNode *errorNode);

#endif
