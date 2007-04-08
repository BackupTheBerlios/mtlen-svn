/*

Jabber Protocol Plugin for Miranda IM
Tlen Protocol Plugin for Miranda IM
Copyright (C) 2002-2004  Santithorn Bunchua

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

#ifndef _JABBER_IQ_H_
#define _JABBER_IQ_H_

#include "jabber_xml.h"

typedef enum {
	IQ_PROC_NONE,
	IQ_PROC_GETSEARCH
} JABBER_IQ_PROCID;

typedef void (*JABBER_IQ_PFUNC)(XmlNode *iqNode, void *usedata);

void JabberIqInit();
void JabberIqUninit();
JABBER_IQ_PFUNC JabberIqFetchFunc(int iqId);
void JabberIqAdd(unsigned int iqId, JABBER_IQ_PROCID procId, JABBER_IQ_PFUNC func);

void JabberIqResultSetAuth(XmlNode *iqNode, void *userdata);
void JabberIqResultGetRoster(XmlNode *iqNode, void *userdata);
void TlenIqResultGetVcard(XmlNode *iqNode, void *userdata);
void JabberIqResultSetSearch(XmlNode *iqNode, void *userdata);
void TlenIqResultChatGroups(XmlNode *iqNode, void *userdata);
void TlenIqResultChatRooms(XmlNode *iqNode, void *userdata);
void TlenIqResultUserRooms(XmlNode *iqNode, void *userdata);
void TlenIqResultUserNicks(XmlNode *iqNode, void *userdata);
void TlenIqResultRoomSearch(XmlNode *iqNode, void *userdata);
void TlenIqResultRoomInfo(XmlNode *iqNode, void *userdata);
void TlenIqResultChatRoomUsers(XmlNode *iqNode, void *userdata);
void TlenIqResultTcfg(XmlNode *iqNode, void *userdata);
//void JabberIqResultSetPassword(XmlNode *iqNode, void *userdata);

#endif

