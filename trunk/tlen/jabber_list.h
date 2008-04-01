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

#ifndef _JABBER_LIST_H_
#define _JABBER_LIST_H_


void JabberListInit(TlenProtocol *proto);
void JabberListUninit(TlenProtocol *proto);
void JabberListWipe(TlenProtocol *proto);
void JabberListWipeSpecial(TlenProtocol *proto);
int JabberListExist(TlenProtocol *proto, JABBER_LIST list, const char *jid);
JABBER_LIST_ITEM *JabberListAdd(TlenProtocol *proto, JABBER_LIST list, const char *jid);
void JabberListRemove(TlenProtocol *proto, JABBER_LIST list, const char *jid);
void JabberListRemoveList(TlenProtocol *proto, JABBER_LIST list);
void JabberListRemoveByIndex(TlenProtocol *proto, int index);
int JabberListFindNext(TlenProtocol *proto, JABBER_LIST list, int fromOffset);
JABBER_LIST_ITEM *JabberListGetItemPtr(TlenProtocol *proto, JABBER_LIST list, const char *jid);
JABBER_LIST_ITEM *JabberListGetItemPtrFromIndex(TlenProtocol *proto, int index);
JABBER_LIST_ITEM *JabberListFindItemPtrById2(TlenProtocol *proto, JABBER_LIST list, const char *id);

void JabberListAddResource(TlenProtocol *proto, JABBER_LIST list, const char *jid, int status, const char *statusMessage);
void JabberListRemoveResource(TlenProtocol *proto, JABBER_LIST list, const char *jid);

#endif

