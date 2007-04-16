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

#ifndef _JABBER_LIST_H_
#define _JABBER_LIST_H_

typedef enum {
	LIST_ROSTER,	// Roster list
	LIST_CHATROOM,	// Groupchat room currently joined
	LIST_FILE,		// Current file transfer session
	LIST_INVITATIONS,// Invitations to be sent
	LIST_SEARCH,	 // Rooms names being searched
	LIST_VOICE,
	LIST_PICTURE
} JABBER_LIST;

typedef enum {
	SUB_NONE,
	SUB_TO,
	SUB_FROM,
	SUB_BOTH
} JABBER_SUBSCRIPTION;

typedef struct {
	JABBER_LIST list;
	char *jid;
	char *id2;

	// LIST_ROSTER
	// jid = jid of the contact
	char *nick;
	int status;	// Main status, currently useful for transport where no resource information is kept.
				// On normal contact, this is the same status as shown on contact list.
	JABBER_SUBSCRIPTION subscription;
	char *statusMessage;	// Status message when the update is to JID with no resource specified (e.g. transport user)
	char *software;
	char *version;
	char *system;
	char *group;
	int	 avatarFormat;
	char *avatarHash;
	BOOL newAvatarDownloading;
	BOOL versionRequested;
	int idMsgAckPending;
	char *messageEventIdStr;
	BOOL wantComposingEvent;
	BOOL isTyping;

	// LIST_ROOM
	// jid = room JID
	// char *name; // room name
	//char *type;	// room type

	// LIST_CHATROOM
	// jid = room JID
	// char *nick;	// my nick in this chat room (SPECIAL: in UTF8)
	// JABBER_RESOURCE_STATUS *resource;	// participant nicks in this room
	char *roomName;

	// LIST_FILE
	// jid = string representation of port number
	TLEN_FILE_TRANSFER *ft;
	//WORD port;
} JABBER_LIST_ITEM;

void JabberListInit(void);
void JabberListUninit(void);
void JabberListWipe(void);
void JabberListWipeSpecial(void);
int JabberListExist(JABBER_LIST list, const char *jid);
JABBER_LIST_ITEM *JabberListAdd(JABBER_LIST list, const char *jid);
void JabberListRemove(JABBER_LIST list, const char *jid);
void JabberListRemoveList(JABBER_LIST list);
void JabberListRemoveByIndex(int index);
int JabberListFindNext(JABBER_LIST list, int fromOffset);
JABBER_LIST_ITEM *JabberListGetItemPtr(JABBER_LIST list, const char *jid);
JABBER_LIST_ITEM *JabberListGetItemPtrFromIndex(int index);
JABBER_LIST_ITEM *JabberListFindItemPtrById2(JABBER_LIST list, const char *id);

void JabberListAddResource(JABBER_LIST list, const char *jid, int status, const char *statusMessage);
void JabberListRemoveResource(JABBER_LIST list, const char *jid);

void TlenGetAvatarFileName(JABBER_LIST_ITEM *item, char* pszDest, int cbLen );

#endif

