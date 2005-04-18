/*

Tlen Protocol Plugin for Miranda IM
Copyright (C) 2004  Piotr Piastucki

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
#include "jabber_list.h"
#include "jabber_iq.h"
#include "resource.h"
#include "tlen_muc.h"

static int TlenMUCHandleEvent(WPARAM wParam, LPARAM lParam);
static int TlenMUCQueryContacts(const char *roomId);
static int TlenMUCSendInvitation(const char *roomID, const char *user);
static int TlenMUCSendPresence(const char *roomID, const char *nick, int desiredStatus);
static int TlenMUCSendMessage(MUCCEVENT *event);
static int TlenMUCSendTopic(MUCCEVENT *event);
static int TlenMUCSendQuery(int type, const char *parent, int page);

static int isSelf(const char *roomID, const char *nick) 
{
	JABBER_LIST_ITEM *item;
	int result;
	DBVARIANT dbv;
	result=0;
	item = JabberListGetItemPtr(LIST_CHATROOM, roomID);
	if (item!=NULL) {
		if (item->nick==NULL) {
			if (!DBGetContactSetting(NULL, jabberProtoName, "LoginName", &dbv)) {
				if (!strcmp(nick, dbv.pszVal)) result = 1;
				DBFreeVariant(&dbv);
			}
		} else if (nick[0]=='~') {
			if (!strcmp(nick+1, item->nick)) {
				result = 1;
			}
		}
	}
	return result;
}

static int stringToHex(const char *str) 
{
	int i, val;
	val = 0;
	for (i=0;i<2;i++) {
		val <<= 4;
		if (str[i]>='A' && str[i]<='F') {
			val += 10 + str[i]-'A';
		} else if (str[i]>='0' && str[i]<='9') {
			val += str[i]-'0';
		} 
	}
	return val;

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
BOOL TlenMUCInit(void) 
{
	HookEvent(ME_MUCC_EVENT, TlenMUCHandleEvent);
	return 0;
}
static int TlenMUCHandleEvent(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact;
	int id;
	MUCCEVENT *mucce=(MUCCEVENT *) lParam;
	if (!strcmp(mucce->pszModule, jabberProtoName)) {
		JabberLog("ME ! %d", mucce->iType);
		switch (mucce->iType) {
			case MUCC_EVENT_INVITE:
				TlenMUCSendInvitation(mucce->pszID, mucce->pszNick);
				break;
			case MUCC_EVENT_MESSAGE:
				TlenMUCSendMessage(mucce);
				break;
			case MUCC_EVENT_TOPIC:
				TlenMUCSendTopic(mucce);
				break;
			case MUCC_EVENT_LEAVE:
				TlenMUCSendPresence(mucce->pszID, NULL, ID_STATUS_OFFLINE);
				break;
			case MUCC_EVENT_QUERY_GROUPS:
				TlenMUCSendQuery(1, mucce->pszID, 0);
				break;
			case MUCC_EVENT_QUERY_ROOMS:
				TlenMUCSendQuery(2, mucce->pszID, mucce->dwData);
				break;
			case MUCC_EVENT_QUERY_SEARCH:
				TlenMUCSendQuery(3, mucce->pszName, 0);
				break;
			case MUCC_EVENT_QUERY_USERS:
				JabberLog("query3=%d",mucce->dwFlags);
				switch (mucce->dwFlags) {
				case MUCC_EF_USER_OWNER:
					id = 1;
					break;
				case MUCC_EF_USER_ADMIN:
					id = 2;
					break;
				case MUCC_EF_USER_MEMBER:
					id = 3;
					break;
				case MUCC_EF_USER_BANNED:
					id = 4;
					break;
				case MUCC_EF_USER_MODERATOR:
					id = 6;
					break;
				default:
					id = 0;
				}
				TlenMUCSendQuery(4, mucce->pszID, id);
				break;
			case MUCC_EVENT_REGISTER_NICK:
				TlenMUCSendQuery(6, mucce->pszNick, 0);
				break;
			case MUCC_EVENT_REMOVE_NICK:
				TlenMUCSendQuery(6, mucce->pszNick, 1);
				break;
			case MUCC_EVENT_REGISTER_ROOM:
				id = JabberSerialNext();
				if (jabberOnline) {
					if (mucce->pszNick!=NULL) {
						JabberSend(jabberThreadInfo->s, "<p to='c' tp='c' id='"JABBER_IQID"%d' x='%d' n='%s' p='%s' nick='%s'/>", id, mucce->dwFlags | 0x10, mucce->pszName, mucce->pszID);
					} else {
						JabberSend(jabberThreadInfo->s, "<p to='c' tp='c' id='"JABBER_IQID"%d' x='%d' n='%s' p='%s'/>", id, mucce->dwFlags | 0x10, mucce->pszName, mucce->pszID);
					}
				}
				break;
			case MUCC_EVENT_REMOVE_ROOM:
				if (jabberOnline) {
					JabberSend(jabberThreadInfo->s, "<p to='%s' type='d'/>", mucce->pszID);
					JabberListRemove(LIST_CHATROOM, mucce->pszID);
				//	TlenMUCSendPresence(mucce->pszID, NULL, ID_STATUS_OFFLINE);
				}
				break;
			case MUCC_EVENT_KICK_BAN:
				if (jabberOnline) {
					char *nick;
					nick = JabberResourceFromJID(mucce->pszUID);
					if (!isSelf(mucce->pszID, nick)) {
						char *reason = JabberTextEncode(mucce->pszText);
						JabberSend(jabberThreadInfo->s, "<p to='%s'><x><i i='%s' a='4' ex='%d' rs='%s'/></x></p>", mucce->pszID, nick, mucce->dwData, reason);
						free (reason);
					}
					free(nick);
				}
				break;
			case MUCC_EVENT_UNBAN:
				if (jabberOnline) {
					char *nick;
					nick = JabberResourceFromJID(mucce->pszUID);
					if (!isSelf(mucce->pszID, nick)) {
						JabberSend(jabberThreadInfo->s, "<p to='%s'><x><i i='%s' a='0'/></x></p>", mucce->pszID, nick);
					}
					free(nick);
				}
				break;
			case MUCC_EVENT_SET_USER_ROLE:
				if (jabberOnline) {
					char *nick;
					nick = JabberResourceFromJID(mucce->pszUID);
					if (!isSelf(mucce->pszID, nick)) {
						if (mucce->dwFlags == MUCC_EF_USER_ADMIN) {
							id = 2;
						} else if (mucce->dwFlags == MUCC_EF_USER_MEMBER) {
							id = 3;
						} else {
							id = 0;
						}
						JabberSend(jabberThreadInfo->s, "<p to='%s'><x><i i='%s' a='%d' /></x></p>", mucce->pszID, nick, id);
					}
					free(nick);
				}
				break;
			case MUCC_EVENT_QUERY_USER_NICKS:
				TlenMUCSendQuery(7, mucce->pszID, 0);
				break;
			case MUCC_EVENT_QUERY_USER_ROOMS:
				TlenMUCSendQuery(8, mucce->pszID, 0);
				break;
			case MUCC_EVENT_QUERY_CONTACTS:
				TlenMUCQueryContacts(mucce->pszID);
				break;
			case MUCC_EVENT_JOIN:
				if (jabberOnline) {
					if (mucce->pszID==NULL || strlen(mucce->pszID)==0) { 
						if (mucce->pszName==NULL || strlen(mucce->pszName)==0) { // create a new chat room
							id = JabberSerialNext();
							JabberSend(jabberThreadInfo->s, "<p to='c' tp='c' id='"JABBER_IQID"%d'/>", id);
						} else { // find a chat room by name
							TlenMUCSendQuery(3, mucce->pszName, 0);
						}
					} else { // join existing chat room
						if (!TlenMUCCreateWindow(mucce->pszID, mucce->pszName, mucce->dwFlags, mucce->pszNick, NULL)) {
							TlenMUCSendPresence(mucce->pszID, mucce->pszNick, ID_STATUS_ONLINE);
						}
					}
				}
				break;
			case MUCC_EVENT_START_PRIV:
				if (jabberOnline) {
					JABBER_LIST_ITEM *item;
					item = JabberListGetItemPtr(LIST_CHATROOM, mucce->pszID);
					if (item!=NULL) {
						char *nick;
						nick = JabberResourceFromJID(mucce->pszUID);
						if (!isSelf(mucce->pszID, nick)) {
							if (nick[0]=='~' || item->nick!=NULL) {
								char str[256];
								sprintf(str, "%s/%s", mucce->pszID, nick);
								hContact = JabberDBCreateContact(str, nick, TRUE, FALSE); //(char *)mucce->pszUID
								DBWriteContactSettingByte(hContact, jabberProtoName, "bChat", TRUE);
								CallService(MS_MSG_SENDMESSAGE, (WPARAM) hContact, (LPARAM) NULL);
							} else {
								DBVARIANT dbv;
								if (!DBGetContactSetting(NULL, jabberProtoName, "LoginServer", &dbv)) {
									char str[512];
									_snprintf(str, sizeof(str), "%s@%s", nick, dbv.pszVal);
									DBFreeVariant(&dbv);									
									hContact = JabberDBCreateContact(str, nick, TRUE, FALSE);
									CallService(MS_MSG_SENDMESSAGE, (WPARAM) hContact, (LPARAM) NULL);
								}
							}
						}
						free(nick);
					}
				}
				break;
		}
	}
	return 0;
}

int TlenMUCRecvInvitation(const char *roomId, const char *roomName, const char *from, const char *reason)
{
	char *nick;
	MUCCEVENT *mucce;
	if (roomId == NULL) return 1;
	mucce = (MUCCEVENT *) malloc (sizeof(MUCCEVENT));
	mucce->cbSize = sizeof(MUCCEVENT);
	mucce->iType = MUCC_EVENT_INVITATION;
	mucce->pszModule = jabberProtoName;
	mucce->pszID = roomId;
	mucce->pszName = roomName;
	mucce->pszUID = from;
	nick = getDisplayName(from);
	mucce->pszNick = nick;
	mucce->pszText = reason;
	CallService(MS_MUCC_EVENT, 0, (LPARAM) mucce);
	free(nick);
	free(mucce);
	return 0;
}

int TlenMUCRecvPresence(const char *from, int status, int flags, const char *kick)
{
	char str[512];
//	if (JabberListExist(LIST_CHATROOM, from)) {
		char *nick, *roomId, *userId;
		MUCCEVENT *mucce = (MUCCEVENT *) malloc (sizeof(MUCCEVENT));
		roomId = JabberLoginFromJID(from);
		userId = JabberResourceFromJID(from);
		nick = getDisplayName(userId);
		mucce->cbSize = sizeof(MUCCEVENT);
		mucce->pszModule = jabberProtoName;
		mucce->pszID = roomId;
		mucce->iType = MUCC_EVENT_STATUS;
		mucce->pszUID = userId;//from;
		mucce->pszNick = nick;
		mucce->time = time(NULL);
		mucce->bIsMe = isSelf(roomId, userId);
		mucce->dwData = status;
		mucce->dwFlags = 0;
		if (flags & USER_FLAGS_GLOBALOWNER) mucce->dwFlags |= MUCC_EF_USER_GLOBALOWNER;
		if (flags & USER_FLAGS_OWNER) mucce->dwFlags |= MUCC_EF_USER_OWNER;
		if (flags & USER_FLAGS_ADMIN) mucce->dwFlags |= MUCC_EF_USER_ADMIN;
		if (flags & USER_FLAGS_REGISTERED) mucce->dwFlags |= MUCC_EF_USER_REGISTERED;
		if (status == ID_STATUS_OFFLINE && mucce->bIsMe && kick!=NULL) {
			mucce->iType = MUCC_EVENT_ERROR;
			sprintf(str, Translate("You have been kicked. Reason: %s "), kick);
			mucce->pszText = str;
		}
		CallService(MS_MUCC_EVENT, 0, (LPARAM) mucce);
		free(roomId);
		free(userId);
		free(nick);
		free(mucce);
//	}
	return 0;
}

int TlenMUCRecvMessage(const char *from, long timestamp, XmlNode *bodyNode) 
{
//	if (JabberListExist(LIST_CHATROOM, from)) {
		char *localMessage;
		char *nick, *style, *roomId, *userId;
		int	 iStyle;
		MUCCEVENT *mucce = (MUCCEVENT *) malloc (sizeof(MUCCEVENT));
		roomId = JabberLoginFromJID(from);
		userId = JabberResourceFromJID(from);
		nick = getDisplayName(userId);
		localMessage = JabberTextDecode(bodyNode->text);
		mucce->cbSize = sizeof(MUCCEVENT);
		mucce->iType = MUCC_EVENT_MESSAGE;
		mucce->pszID = roomId;
		mucce->pszModule = jabberProtoName;
		mucce->pszText = localMessage;
		mucce->pszUID = userId;//from;
		mucce->pszNick = nick;
		mucce->time = timestamp;
		mucce->bIsMe = isSelf(roomId, userId);
		mucce->dwFlags = 0;
		mucce->iFontSize = 0;
		style = JabberXmlGetAttrValue(bodyNode, "f");
		if (style!=NULL) {
			iStyle = atoi(style);
			if (iStyle & 1) mucce->dwFlags |= MUCC_EF_FONT_BOLD;
			if (iStyle & 2) mucce->dwFlags |= MUCC_EF_FONT_ITALIC;
			if (iStyle & 4) mucce->dwFlags |= MUCC_EF_FONT_UNDERLINE;
		}
		style = JabberXmlGetAttrValue(bodyNode, "c");
		if (style!=NULL && strlen(style)>5) {
			iStyle = (stringToHex(style)<<16) | (stringToHex(style+2)<<8) | stringToHex(style+4);
		} else {
			iStyle = 0xFFFFFFFF;
		}
		mucce->color = (COLORREF) iStyle;
		style = JabberXmlGetAttrValue(bodyNode, "s");
		if (style!=NULL) {
			iStyle = atoi(style);
		} else {
			iStyle = 0;
		}
		mucce->iFontSize = iStyle;
		style = JabberXmlGetAttrValue(bodyNode, "n");
		if (style!=NULL) {
			iStyle = atoi(style)-1;
		} else {
			iStyle = 0;
		}
		mucce->iFont = iStyle;
		CallService(MS_MUCC_EVENT, 0, (LPARAM) mucce);
		free(roomId);
		free(userId);
		free(nick);
		free(localMessage);
		free(mucce);
//	}
	return 0;
}
int TlenMUCRecvTopic(const char *from, const char *subject) 
{
//	if (JabberListExist(LIST_CHATROOM, from)) {
		MUCCEVENT *mucce = (MUCCEVENT *) malloc (sizeof(MUCCEVENT));
		mucce->cbSize = sizeof(MUCCEVENT);
		mucce->iType = MUCC_EVENT_TOPIC;
		mucce->pszID = from;
		mucce->pszModule = jabberProtoName;
		mucce->pszText = subject;
		mucce->time = time(NULL);
		CallService(MS_MUCC_EVENT, 0, (LPARAM) mucce);
		free(mucce);
//	}
	return 0;
}

int TlenMUCRecvError(const char *from, XmlNode *errorNode) 
{
	int errCode;
	char str[512];
	JABBER_LIST_ITEM *item;
	MUCCEVENT *mucce = (MUCCEVENT *) malloc (sizeof(MUCCEVENT));
	mucce->cbSize = sizeof(MUCCEVENT);
	mucce->iType = MUCC_EVENT_ERROR;
	mucce->pszID = from;
	mucce->pszModule = jabberProtoName;
	errCode = atoi(JabberXmlGetAttrValue(errorNode, "code"));
	switch (errCode) {
		case 403:
			sprintf(str, Translate("You cannot join this chat room, because you are banned."));
			break;
		case 404:
			sprintf(str, Translate("Chat room not found."));
			break;
		case 407:
			sprintf(str, Translate("This is a private chat room and you are not one of the members."));
			break;
		case 408:
			sprintf(str, Translate("You cannot send any message unless you join this chat room."));
			break;
		case 410: 
			sprintf(str, Translate("Chat room with already created."));
			break;
		case 411: 
			sprintf(str, Translate("Nickname '%s' is already registered."),
				JabberXmlGetAttrValue(errorNode, "n"));
			break;
		case 412:
			sprintf(str, Translate("Nickname already in use, please try another one. Hint: '%s' is free."),
				JabberXmlGetAttrValue(errorNode, "free"));
			break;
		case 413:
			sprintf(str, Translate("You cannot register more than %s nicknames."),
				JabberXmlGetAttrValue(errorNode, "num"));
			break;
		case 414:
			sprintf(str, Translate("You cannot create more than %s chat rooms."),
				JabberXmlGetAttrValue(errorNode, "num"));
			break;
		case 415:
			sprintf(str, Translate("You cannot join more than %s chat rooms."),
				JabberXmlGetAttrValue(errorNode, "num"));
			break;
		case 601:
			sprintf(str, Translate("Anonymous nicknames are not allowed in this chat room."));
			break;
		default:
			sprintf(str, Translate("Unknown error code : %d"), errCode);
			break;
	}
	mucce->pszText = str;
	CallService(MS_MUCC_EVENT, 0, (LPARAM) mucce);
	if (jabberOnline) {
		switch (errCode) {
			case 412:
				item = JabberListGetItemPtr(LIST_CHATROOM, from);
				if (item!=NULL) {
					mucce->iType = MUCC_EVENT_JOIN;
					mucce->dwFlags = MUCC_EF_ROOM_NICKNAMES;
					mucce->pszModule = jabberProtoName;
					mucce->pszID = from;
					mucce->pszName = item->roomName;
					mucce->pszNick = JabberXmlGetAttrValue(errorNode, "free");
					CallService(MS_MUCC_EVENT, 0, (LPARAM) mucce);
				}
				break;
			case 601:
				item = JabberListGetItemPtr(LIST_CHATROOM, from);
				if (item!=NULL) {
					mucce->iType = MUCC_EVENT_JOIN;
					mucce->dwFlags = 0;
					mucce->pszModule = jabberProtoName;
					mucce->pszID = from;
					mucce->pszName = item->roomName;
					mucce->pszNick = NULL;
					CallService(MS_MUCC_EVENT, 0, (LPARAM) mucce);
				}
				break;
		}
	}
	free(mucce);
	return 1;
}
static int TlenMUCSendInvitation(const char *roomID, const char *user)
{
	if (!jabberOnline) {
		return 1;
	}
	JabberSend(jabberThreadInfo->s, "<m to='%s'><x><inv to='%s'><r></r></inv></x></m>", roomID, user);
	return 0;
}

static int TlenMUCSendPresence(const char *roomID, const char *nick, int desiredStatus)
{
	char str[512];
	char *jid;
	JABBER_LIST_ITEM *item;
	if (!jabberOnline) {
		return 1;
	}
	if (nick!=NULL) {
		_snprintf(str, sizeof(str), "%s/%s", roomID, nick);
	} else {
		_snprintf(str, sizeof(str), "%s", roomID);
	}
	if ((jid = JabberTextEncode(str))!=NULL) {
		switch (desiredStatus) {
			case ID_STATUS_ONLINE:
				JabberSend(jabberThreadInfo->s, "<p to='%s'/>", jid);
				item = JabberListGetItemPtr(LIST_CHATROOM, roomID);
				if (item!=NULL) {
					if (item->nick!=NULL) free(item->nick);
					item->nick = NULL;
					if (nick!=NULL) {
						item->nick = _strdup(nick);
					}
				}
				break;
			default:
				item = JabberListGetItemPtr(LIST_CHATROOM, roomID);
				if (item!=NULL) {
					JabberSend(jabberThreadInfo->s, "<p to='%s'><s>unavailable</s></p>", jid);
					JabberListRemove(LIST_CHATROOM, roomID);
				}
				break;
		}
		free(jid);
	}
	return 0;
}

static int TlenMUCSendMessage(MUCCEVENT *event) 
{
	char *msg, *jid;
	int style;

	if (!jabberOnline) {
		return 1;
	}
	if ((msg = JabberTextEncode(event->pszText))!=NULL) {
		if ((jid = JabberTextEncode(event->pszID))!=NULL) {
			style = 0;
			if (event->dwFlags & MUCC_EF_FONT_BOLD) style |=1;
			if (event->dwFlags & MUCC_EF_FONT_ITALIC) style |=2;
			if (event->dwFlags & MUCC_EF_FONT_UNDERLINE) style |=4;
			JabberSend(jabberThreadInfo->s, "<m to='%s'><b n='%d' s='%d' f='%d' c='%06X'>%s</b></m>", jid, event->iFont+1, event->iFontSize, style, event->color, msg);
			free(jid);
		}
		free(msg);
	}
	return 0;
}

static int TlenMUCSendTopic(MUCCEVENT *event) 
{
	char *msg, *jid;
	if (!jabberOnline) {
		return 1;
	}
	if ((msg = JabberTextEncode(event->pszText))!=NULL) {
		if ((jid = JabberTextEncode(event->pszID))!=NULL) {
			JabberSend(jabberThreadInfo->s, "<m to='%s'><subject>%s</subject></m>", jid, msg);
			free(jid);
		}
		free(msg);
	}
	return 0;
}

static int TlenMUCSendQuery(int type, const char *parent, int page)
{	
	if (!jabberOnline) {
		return 1;
	}
	if (type==3) { // find chat room by name
		char serialId[32];
		JABBER_LIST_ITEM *item;
		sprintf(serialId, JABBER_IQID"%d", JabberSerialNext());
		item = JabberListAdd(LIST_SEARCH, serialId);
		item->roomName = _strdup(parent);
		JabberSend(jabberThreadInfo->s, "<iq to='c' type='3' n='%s' id='%s'/>", parent, serialId);
	} else {
		if (parent==NULL) {
			JabberSend(jabberThreadInfo->s, "<iq to='c' type='%d'/>", type);
		} else { // 1 - groups, 2 - chat rooms, 7 - user nicks, 8 - user rooms
			if (type==1 || (type==2 && page==0) || type==7 || type==8) {
				JabberSend(jabberThreadInfo->s, "<iq to='c' type='%d' p='%s'/>", type, parent);
			} else if (type==2) {
				JabberSend(jabberThreadInfo->s, "<iq to='c' type='%d' p='%s' n='%d'/>", type, parent, page);
			} else if (type==6) {
				if (page) {
					JabberSend(jabberThreadInfo->s, "<iq to='c' type='%d' n='%s' k='u'/>", type, parent);
				} else {
					JabberSend(jabberThreadInfo->s, "<iq to='c' type='%d' n='%s'/>", type, parent);
				}
			} else if (type==4) { // list of users, admins etc.
				JabberSend(jabberThreadInfo->s, "<iq to='%s' type='%d' k='%d'/>", parent, type, page);
			}
		}
	}
	return 0;
}

int TlenMUCCreateWindow(const char *roomID, const char *roomName, int roomFlags, const char *nick, const char *iqId)
{	
	JABBER_LIST_ITEM *item;
	MUCCWINDOW *mucw;
	if (!jabberOnline || roomID==NULL) {
		return 1;
	}
	if (JabberListExist(LIST_CHATROOM, roomID)) {
		return 0;
	}
	item = JabberListAdd(LIST_CHATROOM, roomID);
	if (roomName!=NULL) {
		item->roomName = _strdup(roomName);
	} 
	if (nick!=NULL) {
		item->nick = _strdup(nick);
	} 
	mucw = (MUCCWINDOW *) malloc (sizeof(MUCCWINDOW));
	mucw->cbSize = sizeof(MUCCWINDOW);
	mucw->iType = MUCC_WINDOW_CHATROOM;
	mucw->pszModule = jabberProtoName;
	mucw->pszModuleName = jabberModuleName;
	mucw->pszID = roomID;
	mucw->pszName = roomName;
	mucw->pszNick = nick;
	mucw->dwFlags = roomFlags;
	mucw->pszStatusbarText = "hello";
	CallService(MS_MUCC_NEW_WINDOW, 0, (LPARAM) mucw);
	free (mucw);
	if (iqId != NULL) {
		item = JabberListGetItemPtr(LIST_INVITATIONS, iqId);
		if (item !=NULL) {
			TlenMUCSendInvitation(roomID, item->nick);
		}
		JabberListRemove(LIST_INVITATIONS, iqId);
	}
	return 0;
}

static void TlenMUCFreeQueryResult(MUCCQUERYRESULT *result) 
{	int i;
	for (i=0; i<result->iItemsNum; i++) {
		if (result->pItems[i].pszID != NULL) {
			free ((char *) result->pItems[i].pszID);
		}
		if (result->pItems[i].pszName != NULL) {
			free ((char *) result->pItems[i].pszName);
		}
		if (result->pItems[i].pszNick != NULL) {
			free ((char *) result->pItems[i].pszNick);
		}
		if (result->pItems[i].pszText != NULL) {
			free ((char *) result->pItems[i].pszText);
		}
	}
	free((MUCCQUERYITEM *)result->pItems);
}

void TlenIqResultChatGroups(XmlNode *iqNode, void *userdata)
{
	XmlNode *lNode, *itemNode;
	char *p, *n, *id, *f;
	int i, j;
	MUCCQUERYRESULT queryResult;

	if ((lNode=JabberXmlGetChild(iqNode, "l")) == NULL) return;
	p = JabberXmlGetAttrValue(iqNode, "p");
	if (p==NULL) {
		p="";
	}
	p = JabberTextDecode(p);
	queryResult.cbSize = sizeof (MUCCQUERYRESULT);
	queryResult.iType = MUCC_EVENT_QUERY_GROUPS;
	queryResult.pszModule = jabberProtoName;
	queryResult.pszParent = p;
	queryResult.pItems = malloc(sizeof(MUCCQUERYITEM) * lNode->numChild);
	memset(queryResult.pItems, 0, sizeof(MUCCQUERYITEM) * lNode->numChild);
	for (i=j=0; i<lNode->numChild; i++) {
		itemNode = lNode->child[i];
		if (!strcmp(itemNode->name, "i")) {
			queryResult.pItems[j].iCount = 0;
			if ((f = JabberXmlGetAttrValue(itemNode, "f")) != NULL) {
				queryResult.pItems[j].iCount = !strcmp(f, "3");
			}
			n = JabberXmlGetAttrValue(itemNode, "n");
			id = JabberXmlGetAttrValue(itemNode, "i");
			if (n != NULL && id != NULL) {
				queryResult.pItems[j].pszID =  JabberTextDecode(id);
				queryResult.pItems[j].pszName = JabberTextDecode(n);
				j++;
			}
		}
	}
	queryResult.iItemsNum = j;
	CallService(MS_MUCC_QUERY_RESULT, 0, (LPARAM) &queryResult);
	TlenMUCFreeQueryResult(&queryResult);
	free(p);
}

void TlenIqResultChatRooms(XmlNode *iqNode, void *userdata)
{
	XmlNode *lNode, *itemNode;
	char *id, *c, *n, *x, *p, *px, *pn;
	int i, j;
	MUCCQUERYRESULT queryResult;

	if ((lNode=JabberXmlGetChild(iqNode, "l")) == NULL) return;
	if ((p = JabberXmlGetAttrValue(iqNode, "p")) == NULL) return;
	pn = JabberXmlGetAttrValue(lNode, "n");
	if (pn == NULL) pn = "0";
	px = JabberXmlGetAttrValue(lNode, "x");
	if (px == NULL) px = "0";
	p = JabberTextDecode(p);
	queryResult.cbSize = sizeof (MUCCQUERYRESULT);
	queryResult.iType = MUCC_EVENT_QUERY_ROOMS;
	queryResult.pszModule = jabberProtoName;
	queryResult.pszParent = p;
	queryResult.pItems = malloc(sizeof(MUCCQUERYITEM) * lNode->numChild);
	memset(queryResult.pItems, 0, sizeof(MUCCQUERYITEM) * lNode->numChild);
	queryResult.iPage = atoi(pn);
	queryResult.iLastPage = atoi(px)&2?1:0;
	for (i=j=0; i<lNode->numChild; i++) {
		itemNode = lNode->child[i];
		if (!strcmp(itemNode->name, "i")) {
			n = JabberXmlGetAttrValue(itemNode, "n");
			c = JabberXmlGetAttrValue(itemNode, "c");
			x = JabberXmlGetAttrValue(itemNode, "x");
			if ((id=JabberXmlGetAttrValue(itemNode, "i")) != NULL) {
				queryResult.pItems[j].pszID =  JabberTextDecode(id);
				queryResult.pItems[j].pszName = JabberTextDecode(n);
				queryResult.pItems[j].iCount = atoi(c);
				queryResult.pItems[j].dwFlags = atoi(x);
				j++;
			}
		}
	}
	queryResult.iItemsNum = j;
	CallService(MS_MUCC_QUERY_RESULT, 0, (LPARAM) &queryResult);
	TlenMUCFreeQueryResult(&queryResult);
	free(p);
}
void TlenIqResultUserRooms(XmlNode *iqNode, void *userdata)
{
	XmlNode *lNode, *itemNode;
	char *id, *n;
	int i, j;
	MUCCQUERYRESULT queryResult;
	if ((lNode=JabberXmlGetChild(iqNode, "l")) == NULL) return;
	queryResult.cbSize = sizeof (MUCCQUERYRESULT);
	queryResult.iType = MUCC_EVENT_QUERY_USER_ROOMS;
	queryResult.pszModule = jabberProtoName;
	queryResult.pItems = malloc(sizeof(MUCCQUERYITEM) * lNode->numChild);
	memset(queryResult.pItems, 0, sizeof(MUCCQUERYITEM) * lNode->numChild);
	for (i=j=0; i<lNode->numChild; i++) {
		itemNode = lNode->child[i];
		if (!strcmp(itemNode->name, "i")) {
			n = JabberXmlGetAttrValue(itemNode, "n");
			id = JabberXmlGetAttrValue(itemNode, "i");
			if (n != NULL && id != NULL) {
				queryResult.pItems[j].pszID =  JabberTextDecode(id);
				queryResult.pItems[j].pszName = JabberTextDecode(n);
				j++;
			}
		}
	}
	queryResult.iItemsNum = j;
	CallService(MS_MUCC_QUERY_RESULT, 0, (LPARAM) &queryResult);
	TlenMUCFreeQueryResult(&queryResult);
}
void TlenIqResultUserNicks(XmlNode *iqNode, void *userdata) {
	XmlNode *lNode, *itemNode;
	char *n;
	int i, j;
	MUCCQUERYRESULT queryResult;
	if ((lNode=JabberXmlGetChild(iqNode, "l")) == NULL) return;
	queryResult.cbSize = sizeof (MUCCQUERYRESULT);
	queryResult.iType = MUCC_EVENT_QUERY_USER_NICKS;
	queryResult.pszModule = jabberProtoName;
	queryResult.pItems = malloc(sizeof(MUCCQUERYITEM) * lNode->numChild);
	memset(queryResult.pItems, 0, sizeof(MUCCQUERYITEM) * lNode->numChild);
	for (i=j=0; i<lNode->numChild; i++) {
		itemNode = lNode->child[i];
		if (!strcmp(itemNode->name, "i")) {
			n = JabberXmlGetAttrValue(itemNode, "n");
			queryResult.pItems[j].pszID =  NULL;//JabberTextDecode(n);
			queryResult.pItems[j].pszName = JabberTextDecode(n);
			j++;
		}
	}
	queryResult.iItemsNum = j;
	CallService(MS_MUCC_QUERY_RESULT, 0, (LPARAM) &queryResult);
	TlenMUCFreeQueryResult(&queryResult);
}
void TlenIqResultChatRoomUsers(XmlNode *iqNode, void *userdata)
{
	XmlNode *lNode, *itemNode;
	char *id, *n;
	int i, j;
	MUCCQUERYRESULT queryResult;
	if ((lNode=JabberXmlGetChild(iqNode, "l")) == NULL) return;
	if ((id=JabberXmlGetAttrValue(iqNode, "from")) == NULL) return;
	queryResult.cbSize = sizeof (MUCCQUERYRESULT);
	queryResult.iType = MUCC_EVENT_QUERY_USERS;
	queryResult.pszModule = jabberProtoName;
	queryResult.pszParent = id;
	queryResult.pItems = malloc(sizeof(MUCCQUERYITEM) * lNode->numChild);
	memset(queryResult.pItems, 0, sizeof(MUCCQUERYITEM) * lNode->numChild);
	for (i=j=0; i<lNode->numChild; i++) {
		itemNode = lNode->child[i];
		if (!strcmp(itemNode->name, "i")) {
			id = JabberXmlGetAttrValue(itemNode, "i");
			if (id != NULL) {
				queryResult.pItems[j].pszID =  JabberTextDecode(id);
				n = JabberXmlGetAttrValue(itemNode, "n");
				if (n!=NULL) {
					queryResult.pItems[j].pszName = JabberTextDecode(n);
				}
				n = JabberXmlGetAttrValue(itemNode, "a");
				if (n!=NULL) {
					queryResult.pItems[j].pszNick = JabberTextDecode(n);
				}
				n = JabberXmlGetAttrValue(itemNode, "r");
				if (n!=NULL) {
					queryResult.pItems[j].pszText = JabberTextDecode(n);
				}
				n = JabberXmlGetAttrValue(itemNode, "e");
				if (n!=NULL) {
					queryResult.pItems[j].iCount = atoi(n);
				}
				n = JabberXmlGetAttrValue(itemNode, "s");
				if (n!=NULL) {
					queryResult.pItems[j].dwFlags = atoi(n);
				}
				j++;
			}
		}
	}
	queryResult.iItemsNum = j;
	CallService(MS_MUCC_QUERY_RESULT, 0, (LPARAM) &queryResult);
	TlenMUCFreeQueryResult(&queryResult);
}

void TlenIqResultRoomSearch(XmlNode *iqNode, void *userdata) {
	char *iqId, *id;
	JABBER_LIST_ITEM *item;
	iqId=JabberXmlGetAttrValue(iqNode, "id");
	item=JabberListGetItemPtr(LIST_SEARCH, iqId);
	if ((id=JabberXmlGetAttrValue(iqNode, "i")) != NULL) {
		MUCCEVENT *mucce;
		id = JabberTextDecode(id);
		mucce = (MUCCEVENT *) malloc (sizeof(MUCCEVENT));
		mucce->cbSize = sizeof(MUCCEVENT);
		mucce->iType = MUCC_EVENT_JOIN;
		mucce->pszModule = jabberProtoName;
		mucce->pszID = id;
		mucce->pszName = id;
		if (item!=NULL) {
			mucce->pszName = item->roomName;
		} 
		mucce->pszNick = NULL;
		mucce->dwFlags = MUCC_EF_ROOM_NICKNAMES;
		CallService(MS_MUCC_EVENT, 0, (LPARAM) mucce);
		free(mucce);
		free(id);
	}
	JabberListRemove(LIST_SEARCH, iqId);
}

void TlenIqResultRoomInfo(XmlNode *iqNode, void *userdata) {
	char *id, *name, *group, *flags;
	if ((id=JabberXmlGetAttrValue(iqNode, "from")) != NULL) {
		if ((name=JabberXmlGetAttrValue(iqNode, "n")) != NULL) {
			MUCCEVENT *mucce;
			group = JabberXmlGetAttrValue(iqNode, "cn");
			flags = JabberXmlGetAttrValue(iqNode, "x");
			id = JabberTextDecode(id);
			name = JabberTextDecode(name);
			mucce = (MUCCEVENT *) malloc (sizeof(MUCCEVENT));
			mucce->cbSize = sizeof(MUCCEVENT);
			mucce->iType = MUCC_EVENT_ROOM_INFO;
			mucce->pszModule = jabberProtoName;
			mucce->pszID = id;
			mucce->pszName = name;
			mucce->dwFlags = atoi(flags);
			CallService(MS_MUCC_EVENT, 0, (LPARAM) mucce);
			free(mucce);
			free(id);
			free(name);
		}
	}
}

static void __cdecl TlenMUCCSendQueryResultThread(void *vRoomId)
{
	HANDLE hContact;
	MUCCQUERYRESULT queryResult;
	DBVARIANT dbv;
	char *roomId = (char *)vRoomId;
	queryResult.cbSize = sizeof (MUCCQUERYRESULT);
	queryResult.iType = MUCC_EVENT_QUERY_CONTACTS;
	queryResult.pszModule = jabberProtoName;
	queryResult.pszParent = roomId;
	queryResult.iItemsNum = 0;
	hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
	while (hContact != NULL) {
		char *str = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
		if(str!=NULL && !strcmp(str, jabberProtoName)) {
			if (!DBGetContactSettingByte(hContact, jabberProtoName, "bChat", FALSE)) {
				if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
					if (strcmp(dbv.pszVal, "b73@tlen.pl")) {
						queryResult.iItemsNum++;
					}
					DBFreeVariant(&dbv);
				}
			}
		}
		hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
	}
	queryResult.pItems = malloc(sizeof(MUCCQUERYITEM) * queryResult.iItemsNum);
	memset(queryResult.pItems, 0, sizeof(MUCCQUERYITEM) * queryResult.iItemsNum);
	queryResult.iItemsNum = 0;
	hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
	while (hContact != NULL) {
		char *baseProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
		if(baseProto!=NULL && !strcmp(baseProto, jabberProtoName)) {
			if (!DBGetContactSettingByte(hContact, jabberProtoName, "bChat", FALSE)) {
				if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
					if (strcmp(dbv.pszVal, "b73@tlen.pl")) {
						queryResult.pItems[queryResult.iItemsNum].pszID = _strdup(dbv.pszVal);
						queryResult.pItems[queryResult.iItemsNum].pszName = _strdup((char *) CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM) hContact, 0));
						queryResult.iItemsNum++;
					}
					DBFreeVariant(&dbv);
				}
			}
		}
		hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
	}
	CallService(MS_MUCC_QUERY_RESULT, 0, (LPARAM) &queryResult);
	TlenMUCFreeQueryResult(&queryResult);
	free(roomId);
}


static int TlenMUCQueryContacts(const char *roomId) {
	JabberForkThread(TlenMUCCSendQueryResultThread, 0, (void *)_strdup(roomId));
	return 1;
}


int TlenMUCMenuHandleMUC(WPARAM wParam, LPARAM lParam)
{
	if (!jabberOnline) {
		return 1;
	}
	JabberSend(jabberThreadInfo->s, "<p to='c' tp='c' id='"JABBER_IQID"%d'/>", JabberSerialNext());
	/*
	MUCCEVENT *mucce;
	mucce = (MUCCEVENT *) malloc (sizeof(MUCCEVENT));
	mucce->cbSize = sizeof(MUCCEVENT);
	mucce->iType = MUCC_EVENT_JOIN;
	mucce->dwFlags = 0;
	mucce->pszModule = jabberProtoName;
	mucce->pszID = NULL;
	mucce->pszName = NULL;
	mucce->pszNick = NULL;
	CallService(MS_MUCC_EVENT, 0, (LPARAM) mucce);
	free(mucce);
	*/
	return 0;
}

int TlenMUCMenuHandleChats(WPARAM wParam, LPARAM lParam)
{
	MUCCWINDOW * mucw;
	if (!jabberOnline) {
		return 1;
	}
	mucw = (MUCCWINDOW *) malloc (sizeof(MUCCWINDOW));
	mucw->cbSize = sizeof(MUCCWINDOW);
	mucw->iType = MUCC_WINDOW_CHATLIST;
	mucw->pszModule = jabberProtoName;
	mucw->pszModuleName = jabberModuleName;
	CallService(MS_MUCC_NEW_WINDOW, 0, (LPARAM) mucw);
	free (mucw);
	return 0;
}

int TlenMUCContactMenuHandleMUC(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact;
	DBVARIANT dbv;
	JABBER_LIST_ITEM *item;
	if (!jabberOnline) {
		return 1;
	}
	if ((hContact=(HANDLE) wParam)!=NULL && jabberOnline) {
		if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
			char serialId[32];
			sprintf(serialId, JABBER_IQID"%d", JabberSerialNext());
			item = JabberListAdd(LIST_INVITATIONS, serialId);
			item->nick = _strdup(dbv.pszVal);
			JabberSend(jabberThreadInfo->s, "<p to='c' tp='c' id='%s'/>", serialId);
			DBFreeVariant(&dbv);
		}
	}
	return 0;
}
