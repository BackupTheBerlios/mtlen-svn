/*

Tlen Protocol Plugin for Miranda IM
Copyright (C) 2002-2004  Santithorn Bunchua
Copyright (C) 2004-2009  Piotr Piastucki

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
#include "tlen_avatar.h"

void TlenProcessPresence(XmlNode *node, TlenProtocol *proto)
{
	HANDLE hContact;
	XmlNode *showNode, *statusNode;
	JABBER_LIST_ITEM *item;
	char *from, *type, *nick, *show;
	int status, laststatus = ID_STATUS_OFFLINE;
	char *p;

	if ((from=JabberXmlGetAttrValue(node, "from")) != NULL) {
		if (JabberListExist(proto, LIST_CHATROOM, from)); //JabberGroupchatProcessPresence(node, userdata);

		else {
			type = JabberXmlGetAttrValue(node, "type");
			item = JabberListGetItemPtr(proto, LIST_ROSTER, from);
			if (item != NULL) {
				if (proto->tlenOptions.enableAvatars) {
					TlenProcessPresenceAvatar(proto, node, item);
				}
			}
			if (type==NULL || (!strcmp(type, "available"))) {
				if ((nick=JabberLocalNickFromJID(from)) != NULL) {
					if ((hContact=JabberHContactFromJID(proto, from)) == NULL)
						hContact = JabberDBCreateContact(proto, from, nick, FALSE);
					if (!JabberListExist(proto, LIST_ROSTER, from)) {
						JabberLog(proto, "Receive presence online from %s (who is not in my roster)", from);
						JabberListAdd(proto, LIST_ROSTER, from);
					}
					status = ID_STATUS_ONLINE;
					if ((showNode=JabberXmlGetChild(node, "show")) != NULL) {
						if ((show=showNode->text) != NULL) {
							if (!strcmp(show, "away")) status = ID_STATUS_AWAY;
							else if (!strcmp(show, "xa")) status = ID_STATUS_NA;
							else if (!strcmp(show, "dnd")) status = ID_STATUS_DND;
							else if (!strcmp(show, "chat")) status = ID_STATUS_FREECHAT;
							else if (!strcmp(show, "unavailable")) {
								// Always show invisible (on old Tlen client) as invisible (not offline)
								status = ID_STATUS_OFFLINE;
							}
						}
					}

					statusNode = JabberXmlGetChild(node, "status");
					if (statusNode)
						p = JabberTextDecode(statusNode->text);
					else
						p = NULL;
					JabberListAddResource(proto, LIST_ROSTER, from, status, statusNode?p:NULL);
					if (p) {
						DBWriteContactSettingString(hContact, "CList", "StatusMsg", p);
						mir_free(p);
					} else {
						DBDeleteContactSetting(hContact, "CList", "StatusMsg");
					}
					// Determine status to show for the contact and request version information
					if (item != NULL) {
						laststatus = item->status;
						item->status = status;
					}
					if (strchr(from, '@')!=NULL || DBGetContactSettingByte(NULL, proto->iface.m_szModuleName, "ShowTransport", TRUE)==TRUE) {
						if (DBGetContactSettingWord(hContact, proto->iface.m_szModuleName, "Status", ID_STATUS_OFFLINE) != status)
							DBWriteContactSettingWord(hContact, proto->iface.m_szModuleName, "Status", (WORD) status);
					}
					if (item != NULL) {
						if (!item->infoRequested) {
							int iqId = JabberSerialNext(proto);
							item->infoRequested = TRUE;
							JabberSend( proto, "<iq type='get' id='"JABBER_IQID"%d'><query xmlns='jabber:iq:info' to='%s'></query></iq>", iqId, from);
						}
						if (proto->tlenOptions.enableVersion && !item->versionRequested) {
							item->versionRequested = TRUE;
							if (proto->iface.m_iStatus != ID_STATUS_INVISIBLE) {
								JabberSend( proto, "<message to='%s' type='iq'><iq type='get'><query xmlns='jabber:iq:version'/></iq></message>", from );
							}
						}
					}
					JabberLog(proto, "%s (%s) online, set contact status to %d", nick, from, status);
					mir_free(nick);
				}
			}
			else if (!strcmp(type, "unavailable")) {
				if (!JabberListExist(proto, LIST_ROSTER, from)) {
					JabberLog(proto, "Receive presence offline from %s (who is not in my roster)", from);
					JabberListAdd(proto, LIST_ROSTER, from);
				}
				else {
					JabberListRemoveResource(proto, LIST_ROSTER, from);
				}
				status = ID_STATUS_OFFLINE;
				statusNode = JabberXmlGetChild(node, "status");
				if (statusNode) {
					if (proto->tlenOptions.offlineAsInvisible) {
						status = ID_STATUS_INVISIBLE;
					}
					p = JabberTextDecode(statusNode->text);
					JabberListAddResource(proto, LIST_ROSTER, from, status, p);
					if ((hContact=JabberHContactFromJID(proto, from)) != NULL) {
						if (p) {
							DBWriteContactSettingString(hContact, "CList", "StatusMsg", p);
						} else {
							DBDeleteContactSetting(hContact, "CList", "StatusMsg");
						}
					}
					if (p) mir_free(p);
				}
				if ((item=JabberListGetItemPtr(proto, LIST_ROSTER, from)) != NULL) {
					// Determine status to show for the contact based on the remaining resources
					item->status = status;
					item->versionRequested = FALSE;
					item->infoRequested = FALSE;
				}
				if ((hContact=JabberHContactFromJID(proto, from)) != NULL) {
					if (strchr(from, '@')!=NULL || DBGetContactSettingByte(NULL, proto->iface.m_szModuleName, "ShowTransport", TRUE)==TRUE) {
						if (DBGetContactSettingWord(hContact, proto->iface.m_szModuleName, "Status", ID_STATUS_OFFLINE) != status)
							DBWriteContactSettingWord(hContact, proto->iface.m_szModuleName, "Status", (WORD) status);
					}
					if (item != NULL && item->isTyping) {
						item->isTyping = FALSE;
						CallService(MS_PROTO_CONTACTISTYPING, (WPARAM) hContact, PROTOTYPE_CONTACTTYPING_OFF);
					}
					JabberLog(proto, "%s offline, set contact status to %d", from, status);
				}
			}
			else if (!strcmp(type, "subscribe")) {
				if (strchr(from, '@') == NULL) {
					// automatically send authorization allowed to agent/transport
					JabberSend(proto, "<presence to='%s' type='subscribed'/>", from);
				}
				else if ((nick=JabberNickFromJID(from)) != NULL) {
					JabberLog(proto, "%s (%s) requests authorization", nick, from);
					JabberDBAddAuthRequest(proto, from, nick);
					mir_free(nick);
				}
			}
			else if (!strcmp(type, "subscribed")) {
				if ((item=JabberListGetItemPtr(proto, LIST_ROSTER, from)) != NULL) {
					if (item->subscription == SUB_FROM) item->subscription = SUB_BOTH;
					else if (item->subscription == SUB_NONE) {
						item->subscription = SUB_TO;
					}
				}
			}
		}
	}
}
