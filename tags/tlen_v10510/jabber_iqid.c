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

#include "jabber.h"
#include "resource.h"
#include "jabber_list.h"
#include "jabber_iq.h"
#include "tlen_muc.h"

extern char *streamId;
extern char *searchJID;

void JabberIqResultSetAuth(XmlNode *iqNode, void *userdata)
{
	struct ThreadData *info = (struct ThreadData *) userdata;
	char *type;
	int iqId;

	// RECVED: authentication result
	// ACTION: if successfully logged in, continue by requesting roster list and set my initial status
	JabberLog("<iq/> iqIdSetAuth");
	if ((type=JabberXmlGetAttrValue(iqNode, "type")) == NULL) return;

	if (!strcmp(type, "result")) {
		DBVARIANT dbv;

		if (DBGetContactSetting(NULL, jabberProtoName, "Nick", &dbv))
			DBWriteContactSettingString(NULL, jabberProtoName, "Nick", info->username);
		else
			DBFreeVariant(&dbv);
		iqId = JabberSerialNext();
		JabberIqAdd(iqId, IQ_PROC_NONE, JabberIqResultGetRoster);
		JabberSend(info->s, "<iq type='get' id='"JABBER_IQID"%d'><query xmlns='jabber:iq:roster'/></iq>", iqId);
	}
	// What to do if password error? etc...
	else if (!strcmp(type, "error")) {
		char text[128];

		JabberSend(info->s, "</s>");
		_snprintf(text, sizeof(text), "%s %s@%s.", Translate("Authentication failed for"), info->username, info->server);
		MessageBox(NULL, text, Translate("Tlen Authentication"), MB_OK|MB_ICONSTOP|MB_SETFOREGROUND);
		ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, LOGINERR_WRONGPASSWORD);
		jabberThreadInfo = NULL;	// To disallow auto reconnect
	}
}

void JabberIqResultGetRoster(XmlNode *iqNode, void *userdata)
{
	//struct ThreadData *info = (struct ThreadData *) userdata;
	XmlNode *queryNode;
	char *type;
	char *str;

	// RECVED: roster information
	// ACTION: populate LIST_ROSTER and create contact for any new rosters
	JabberLog("<iq/> iqIdGetRoster");
	if ((type=JabberXmlGetAttrValue(iqNode, "type")) == NULL) return;
	if ((queryNode=JabberXmlGetChild(iqNode, "query")) == NULL) return;

	if (!strcmp(type, "result")) {
		str = JabberXmlGetAttrValue(queryNode, "xmlns");
		if (str!=NULL && !strcmp(str, "jabber:iq:roster")) {
			DBVARIANT dbv;
			XmlNode *itemNode, *groupNode;
			JABBER_SUBSCRIPTION sub;
			JABBER_LIST_ITEM *item;
			HANDLE hContact;
			char *jid, *name, *nick;
			int i, oldStatus;

			for (i=0; i<queryNode->numChild; i++) {
				itemNode = queryNode->child[i];
				if (!strcmp(itemNode->name, "item")) {
					str = JabberXmlGetAttrValue(itemNode, "subscription");
					if (str==NULL) sub = SUB_NONE;
					else if (!strcmp(str, "both")) sub = SUB_BOTH;
					else if (!strcmp(str, "to")) sub = SUB_TO;
					else if (!strcmp(str, "from")) sub = SUB_FROM;
					else sub = SUB_NONE;
					//if (str!=NULL && (!strcmp(str, "to") || !strcmp(str, "both"))) {
					if ((jid=JabberXmlGetAttrValue(itemNode, "jid")) != NULL) {
						if ((name=JabberXmlGetAttrValue(itemNode, "name")) != NULL) {
							nick = JabberTextDecode(name);
						} else {
							nick = JabberLocalNickFromJID(jid);
						}
						if (nick != NULL) {
							item = JabberListAdd(LIST_ROSTER, jid);
							if (item->nick) free(item->nick);
							item->nick = nick;
							item->subscription = sub;
							if ((hContact=JabberHContactFromJID(jid)) == NULL) {
								// Received roster has a new JID.
								// Add the jid (with empty resource) to Miranda contact list.
								hContact = JabberDBCreateContact(jid, nick, FALSE, TRUE);
							}
							DBWriteContactSettingString(hContact, "CList", "MyHandle", nick);
							if (item->group) free(item->group);
							if ((groupNode=JabberXmlGetChild(itemNode, "group"))!=NULL && groupNode->text!=NULL) {
								item->group = JabberTextDecode(groupNode->text);
								JabberContactListCreateGroup(item->group);
								// Don't set group again if already correct, or Miranda may show wrong group count in some case
								if (!DBGetContactSetting(hContact, "CList", "Group", &dbv)) {
									if (strcmp(dbv.pszVal, item->group))
										DBWriteContactSettingString(hContact, "CList", "Group", item->group);
									DBFreeVariant(&dbv);
								}
								else
									DBWriteContactSettingString(hContact, "CList", "Group", item->group);
							}
							else {
								item->group = NULL;
								DBDeleteContactSetting(hContact, "CList", "Group");
							}
						}
					}
				}
			}
			// Delete orphaned contacts (if roster sync is enabled)
			if (DBGetContactSettingByte(NULL, jabberProtoName, "RosterSync", FALSE) == TRUE) {
				HANDLE *list;
				int listSize, listAllocSize;

				listSize = listAllocSize = 0;
				list = NULL;
				hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
				while (hContact != NULL) {
					str = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
					if(str!=NULL && !strcmp(str, jabberProtoName)) {
						if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
							if (!JabberListExist(LIST_ROSTER, dbv.pszVal)) {
								JabberLog("Syncing roster: preparing to delete %s (hContact=0x%x)", dbv.pszVal, hContact);
								if (listSize >= listAllocSize) {
									listAllocSize = listSize + 100;
									if ((list=(HANDLE *) realloc(list, listAllocSize)) == NULL) {
										listSize = 0;
										break;
									}
								}
								list[listSize++] = hContact;
							}
							DBFreeVariant(&dbv);
						}
					}
					hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
				}
				for (i=0; i<listSize; i++) {
					JabberLog("Syncing roster: deleting 0x%x", list[i]);
					CallService(MS_DB_CONTACT_DELETE, (WPARAM) list[i], 0);
				}
				if (list != NULL)
					free(list);
			}
			///////////////////////////////////////
			{
				CLISTMENUITEM clmi;
				memset(&clmi, 0, sizeof(CLISTMENUITEM));
				clmi.cbSize = sizeof(CLISTMENUITEM);
				clmi.flags = CMIM_FLAGS;
				CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuMUC, (LPARAM) &clmi);
				CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenuChats, (LPARAM) &clmi);
			}

			jabberOnline = TRUE;
			JabberLog("Status changed via THREADSTART");
			modeMsgStatusChangePending = FALSE;
			oldStatus = jabberStatus;
			switch (jabberDesiredStatus) {
			case ID_STATUS_ONLINE:
			case ID_STATUS_NA:
			case ID_STATUS_FREECHAT:
			case ID_STATUS_INVISIBLE:
				jabberStatus = jabberDesiredStatus;
				break;
			case ID_STATUS_AWAY:
			case ID_STATUS_ONTHEPHONE:
			case ID_STATUS_OUTTOLUNCH:
				jabberStatus = ID_STATUS_AWAY;
				break;
			case ID_STATUS_DND:
			case ID_STATUS_OCCUPIED:
				jabberStatus = ID_STATUS_DND;
				break;
			}
			JabberSendPresence(jabberStatus);
			ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) oldStatus, jabberStatus);
			//////////////////////////////////
		}
	}
}

// Tlen actually use jabber:iq:search for other users vCard or jabber:iq:register for own vCard
void TlenIqResultGetVcard(XmlNode *iqNode, void *userdata)
{
	XmlNode *queryNode, *itemNode, *n;
	char *type, *jid;
	char text[128];
	HANDLE hContact;
	char *nText;

	JabberLog("<iq/> iqIdGetVcard (tlen)");
	if ((type=JabberXmlGetAttrValue(iqNode, "type")) == NULL) return;

	if (!strcmp(type, "result")) {
		BOOL hasFirst, hasLast, hasNick, hasEmail, hasCity, hasAge, hasGender, hasSchool, hasLookFor, hasOccupation;
		DBVARIANT dbv;
		int i;

		if ((queryNode=JabberXmlGetChild(iqNode, "query")) == NULL) return;
		if ((itemNode=JabberXmlGetChild(queryNode, "item")) == NULL) return;
		if ((jid=JabberXmlGetAttrValue(itemNode, "jid")) != NULL) {
			if (DBGetContactSetting(NULL, jabberProtoName, "LoginServer", &dbv)) return;
			sprintf(text, "%s@%s", jid, dbv.pszVal);	// Add @tlen.pl
			DBFreeVariant(&dbv);
			if ((hContact=JabberHContactFromJID(text)) == NULL) {
				if (DBGetContactSetting(NULL, jabberProtoName, "LoginName", &dbv)) return;
				if (strcmp(dbv.pszVal, jid)) {
					DBFreeVariant(&dbv);
					return;
				}
				DBFreeVariant(&dbv);
			}
		} else {
			hContact = NULL;
		}
		hasFirst = hasLast = hasNick = hasEmail = hasCity = hasAge = hasGender = hasOccupation = hasLookFor = hasSchool = FALSE;
		for (i=0; i<itemNode->numChild; i++) {
			n = itemNode->child[i];
			if (n==NULL || n->name==NULL) continue;
			if (!strcmp(n->name, "first")) {
				if (n->text != NULL) {
					hasFirst = TRUE;
					nText = JabberTextDecode(n->text);
					DBWriteContactSettingString(hContact, jabberProtoName, "FirstName", nText);
					free(nText);
				}
			}
			else if (!strcmp(n->name, "last")) {
				if (n->text != NULL) {
					hasLast = TRUE;
					nText = JabberTextDecode(n->text);
					DBWriteContactSettingString(hContact, jabberProtoName, "LastName", nText);
					free(nText);
				}
			}
			else if (!strcmp(n->name, "nick")) {
				if (n->text != NULL) {
					hasNick = TRUE;
					nText = JabberTextDecode(n->text);
					DBWriteContactSettingString(hContact, jabberProtoName, "Nick", nText);
					free(nText);
				}
			}
			else if (!strcmp(n->name, "email")) {
				if (n->text != NULL) {
					hasEmail = TRUE;
					nText = JabberTextDecode(n->text);
					DBWriteContactSettingString(hContact, jabberProtoName, "e-mail", nText);
					free(nText);
				}
			}
			else if (!strcmp(n->name, "c")) {
				if (n->text != NULL) {
					hasCity = TRUE;
					nText = JabberTextDecode(n->text);
					DBWriteContactSettingString(hContact, jabberProtoName, "City", nText);
					free(nText);
				}
			}
			else if (!strcmp(n->name, "b")) {
				if (n->text != NULL) {
					WORD nAge;
					hasAge = TRUE;
					nAge = atoi(n->text);
					DBWriteContactSettingWord(hContact, jabberProtoName, "Age", nAge);
				}
			}
			else if (!strcmp(n->name, "s")) {
				if (n->text!=NULL && n->text[1]=='\0' && (n->text[0]=='1' || n->text[0]=='2')) {
					hasGender = TRUE;
					DBWriteContactSettingByte(hContact, jabberProtoName, "Gender", (BYTE) (n->text[0]=='1'?'M':'F'));
				}
			}
			else if (!strcmp(n->name, "e")) {
				if (n->text != NULL) {
					hasSchool = TRUE;
					nText = JabberTextDecode(n->text);
					DBWriteContactSettingString(hContact, jabberProtoName, "School", nText);
					free(nText);
				}
			}
			else if (!strcmp(n->name, "j")) {
				if (n->text != NULL) {
					WORD nOccupation;
					hasOccupation = TRUE;
					nOccupation = atoi(n->text);
					DBWriteContactSettingWord(hContact, jabberProtoName, "Occupation", nOccupation);
				}
			}
			else if (!strcmp(n->name, "r")) {
				if (n->text != NULL) {
					WORD nLookFor;
					hasLookFor = TRUE;
					nLookFor = atoi(n->text);
					DBWriteContactSettingWord(hContact, jabberProtoName, "LookingFor", nLookFor);
				}
			}
			else if (!strcmp(n->name, "g")) {
				if (n->text != NULL) {
					BYTE bVoice;
					bVoice = atoi(n->text);
					DBWriteContactSettingWord(hContact, jabberProtoName, "VoiceChat", bVoice);
				}
			}
		}
		if (!hasFirst)
			DBDeleteContactSetting(hContact, jabberProtoName, "FirstName");
		if (!hasLast)
			DBDeleteContactSetting(hContact, jabberProtoName, "LastName");
		// We are not removing "Nick"
//		if (!hasNick)
//			DBDeleteContactSetting(hContact, jabberProtoName, "Nick");
		if (!hasEmail)
			DBDeleteContactSetting(hContact, jabberProtoName, "e-mail");
		if (!hasCity)
			DBDeleteContactSetting(hContact, jabberProtoName, "City");
		if (!hasAge)
			DBDeleteContactSetting(hContact, jabberProtoName, "Age");
		if (!hasGender)
			DBDeleteContactSetting(hContact, jabberProtoName, "Gender");
		if (!hasSchool)
			DBDeleteContactSetting(hContact, jabberProtoName, "School");
		if (!hasOccupation)
			DBDeleteContactSetting(hContact, jabberProtoName, "Occupation");
		if (!hasLookFor)
			DBDeleteContactSetting(hContact, jabberProtoName, "LookingFor");
		ProtoBroadcastAck(jabberProtoName, hContact, ACKTYPE_GETINFO, ACKRESULT_SUCCESS, (HANDLE) 1, 0);
	}
}

void JabberIqResultSetSearch(XmlNode *iqNode, void *userdata)
{
	XmlNode *queryNode, *itemNode, *n;
	char *type, *jid, *str;
	int id, i, found;
	JABBER_SEARCH_RESULT jsr;
	DBVARIANT dbv;

	found = 0;
	JabberLog("<iq/> iqIdGetSearch");
	if ((type=JabberXmlGetAttrValue(iqNode, "type")) == NULL) return;
	if ((str=JabberXmlGetAttrValue(iqNode, "id")) == NULL) return;
	id = atoi(str+strlen(JABBER_IQID));

	if (!strcmp(type, "result")) {
		if ((queryNode=JabberXmlGetChild(iqNode, "query")) == NULL) return;
		if (!DBGetContactSetting(NULL, jabberProtoName, "LoginServer", &dbv)) {
			jsr.hdr.cbSize = sizeof(JABBER_SEARCH_RESULT);
			for (i=0; i<queryNode->numChild; i++) {
				itemNode = queryNode->child[i];
				if (!strcmp(itemNode->name, "item")) {
					if ((jid=JabberXmlGetAttrValue(itemNode, "jid")) != NULL) {
						_snprintf(jsr.jid, sizeof(jsr.jid), "%s@%s", jid, dbv.pszVal);
						jsr.jid[sizeof(jsr.jid)-1] = '\0';
						if ((n=JabberXmlGetChild(itemNode, "nick"))!=NULL && n->text!=NULL)
							jsr.hdr.nick = JabberTextDecode(n->text);
						else
							jsr.hdr.nick = _strdup("");
						if ((n=JabberXmlGetChild(itemNode, "first"))!=NULL && n->text!=NULL)
							jsr.hdr.firstName = JabberTextDecode(n->text);
						else
							jsr.hdr.firstName = _strdup("");
						if ((n=JabberXmlGetChild(itemNode, "last"))!=NULL && n->text!=NULL)
							jsr.hdr.lastName = JabberTextDecode(n->text);
						else
							jsr.hdr.lastName = _strdup("");
						if ((n=JabberXmlGetChild(itemNode, "email"))!=NULL && n->text!=NULL)
							jsr.hdr.email = JabberTextDecode(n->text);
						else
							jsr.hdr.email = _strdup("");
						ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE) id, (LPARAM) &jsr);
						found = 1;
						free(jsr.hdr.nick);
						free(jsr.hdr.firstName);
						free(jsr.hdr.lastName);
						free(jsr.hdr.email);
					}
				}
			}
			if (searchJID!=NULL) {
				if (!found) {
					_snprintf(jsr.jid, sizeof(jsr.jid), "%s@%s", searchJID, dbv.pszVal);
					jsr.jid[sizeof(jsr.jid)-1] = '\0';
					jsr.hdr.nick = _strdup("");
					jsr.hdr.firstName = _strdup("");
					jsr.hdr.lastName = _strdup("");
					jsr.hdr.email = _strdup("");
					ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE) id, (LPARAM) &jsr);
					free(jsr.hdr.nick);
					free(jsr.hdr.firstName);
					free(jsr.hdr.lastName);
					free(jsr.hdr.email);
				}
				free(searchJID);
				searchJID = NULL;
			}
			DBFreeVariant(&dbv);
		}
		ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE) id, 0);
	}
	else if (!strcmp(type, "error")) {
		// ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_SEARCH, ACKRESULT_FAILED, (HANDLE) id, 0);
		// There is no ACKRESULT_FAILED for ACKTYPE_SEARCH :) look at findadd.c
		// So we will just send a SUCCESS
		ProtoBroadcastAck(jabberProtoName, NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE) id, 0);
	}
}

