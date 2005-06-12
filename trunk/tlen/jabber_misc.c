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
#include "jabber_list.h"

#define __try
#define __except(x) if (0) 
#define __finally

#define _try __try
#define _except __except
#define _finally __finally
#include <excpt.h> 

void JabberDBAddAuthRequest(char *jid, char *nick)
{
	char *s, *p, *q;
	DBEVENTINFO dbei = {0};
	PBYTE pCurBlob;
	HANDLE hContact;

	// strip resource if present
	s = _strdup(jid);
	if ((p=strchr(s, '@')) != NULL) {
		if ((q=strchr(p, '/')) != NULL)
			*q = '\0';
	}
	_strlwr(s);

	if ((hContact=JabberHContactFromJID(jid)) == NULL) {
		hContact = (HANDLE) CallService(MS_DB_CONTACT_ADD, 0, 0);
		CallService(MS_PROTO_ADDTOCONTACT, (WPARAM) hContact, (LPARAM) jabberProtoName);
		DBWriteContactSettingString(hContact, jabberProtoName, "jid", s);
	}
	else {
		DBDeleteContactSetting(hContact, jabberProtoName, "Hidden");
	}
	DBWriteContactSettingString(hContact, jabberProtoName, "Nick", nick);

	//blob is: uin(DWORD), hContact(HANDLE), nick(ASCIIZ), first(ASCIIZ), last(ASCIIZ), email(ASCIIZ), reason(ASCIIZ)
	//blob is: 0(DWORD), hContact(HANDLE), nick(ASCIIZ), ""(ASCIIZ), ""(ASCIIZ), email(ASCIIZ), ""(ASCIIZ)
	dbei.cbSize = sizeof(DBEVENTINFO);
	dbei.szModule = jabberProtoName;
	dbei.timestamp = (DWORD) time(NULL);
	dbei.flags = 0;
	dbei.eventType = EVENTTYPE_AUTHREQUEST;
	dbei.cbBlob = sizeof(DWORD) + sizeof(HANDLE) + strlen(nick) + strlen(jid) + 5;
	pCurBlob = dbei.pBlob = (PBYTE) malloc(dbei.cbBlob);
	*((PDWORD) pCurBlob) = 0; pCurBlob += sizeof(DWORD);
	*((PHANDLE) pCurBlob) = hContact; pCurBlob += sizeof(HANDLE);
	strcpy((char *) pCurBlob, nick); pCurBlob += strlen(nick)+1;
	*pCurBlob = '\0'; pCurBlob++;		//firstName
	*pCurBlob = '\0'; pCurBlob++;		//lastName
	strcpy((char *) pCurBlob, jid); pCurBlob += strlen(jid)+1;
	*pCurBlob = '\0';					//reason

	CallService(MS_DB_EVENT_ADD, (WPARAM) (HANDLE) NULL, (LPARAM) &dbei);
	JabberLog("Setup DBAUTHREQUEST with nick='%s' jid='%s'", nick, jid);
}

HANDLE JabberHContactFromJID(const char *jid)
{
	HANDLE hContact, hContactMatched;
	DBVARIANT dbv;
	char *szProto;
	char *p;
	if (jid == NULL) return (HANDLE) NULL;
	hContactMatched = NULL;
	hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
	while (hContact != NULL) {
		szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
		if (szProto!=NULL && !strcmp(jabberProtoName, szProto)) {
			if (!DBGetContactSetting(hContact, jabberProtoName, "jid", &dbv)) {
				if ((p=dbv.pszVal) != NULL) {
					if (!stricmp(p, jid)) {	// exact match (node@domain/resource)
						hContactMatched = hContact;
						DBFreeVariant(&dbv);
						break;
					}
				}
				DBFreeVariant(&dbv);
			}
		}
		hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
	}
	if (hContactMatched != NULL) {
		return hContactMatched;
	}
	return NULL;
}

HANDLE JabberDBCreateContact(char *jid, char *nick, BOOL temporary)
{
	HANDLE hContact;
	if (jid==NULL || jid[0]=='\0')
		return NULL;

	if ((hContact=JabberHContactFromJID(jid)) == NULL) {
		hContact = (HANDLE) CallService(MS_DB_CONTACT_ADD, 0, 0);
		CallService(MS_PROTO_ADDTOCONTACT, (WPARAM) hContact, (LPARAM) jabberProtoName);
		DBWriteContactSettingString(hContact, jabberProtoName, "jid", jid);
		if (nick!=NULL && nick[0]!='\0')
			DBWriteContactSettingString(hContact, jabberProtoName, "Nick", nick);
		if (temporary)
			DBWriteContactSettingByte(hContact, "CList", "NotOnList", 1);
		JabberLog("Create Jabber contact jid=%s, nick=%s", jid, nick);
	}
	return hContact;
}

static void JabberContactListCreateClistGroup(char *groupName)
{
	char str[33], newName[128];
	int i;
	DBVARIANT dbv;
	char *name;

	for (i=0;;i++) {
		itoa(i, str, 10);
		if (DBGetContactSetting(NULL, "CListGroups", str, &dbv))
			break;
		name = dbv.pszVal;
		if (name[0]!='\0' && !strcmp(name+1, groupName)) {
			// Already exist, no need to create
			DBFreeVariant(&dbv);
			return;
		}
		DBFreeVariant(&dbv);
	}

	// Create new group with id = i (str is the text representation of i)
	newName[0] = 1 | GROUPF_EXPANDED;
	strncpy(newName+1, groupName, sizeof(newName)-1);
	newName[sizeof(newName)-1] = '\0';
	DBWriteContactSettingString(NULL, "CListGroups", str, newName);
	CallService(MS_CLUI_GROUPADDED, i+1, 0);
}

void JabberContactListCreateGroup(char *groupName)
{
	char name[128];
	char *p;

	if (groupName==NULL || groupName[0]=='\0' || groupName[0]=='\\') return;

	strncpy(name, groupName, sizeof(name));
	name[sizeof(name)-1] = '\0';
	for (p=name; *p!='\0'; p++) {
		if (*p == '\\') {
			*p = '\0';
			JabberContactListCreateClistGroup(name);
			*p = '\\';
		}
	}
	JabberContactListCreateClistGroup(name);
}

struct FORK_ARG {
	HANDLE hEvent;
	void (__cdecl *threadcode)(void*);
	void *arg;
};

static void __cdecl forkthread_r(struct FORK_ARG *fa)
{	
	void (*callercode)(void*) = fa->threadcode;
	void *arg = fa->arg;
	CallService(MS_SYSTEM_THREAD_PUSH, 0, 0);
	SetEvent(fa->hEvent);
	__try {
		callercode(arg);
	} __finally {
		CallService(MS_SYSTEM_THREAD_POP, 0, 0);
	} 
	return;
}

unsigned long JabberForkThread(
	void (__cdecl *threadcode)(void*),
	unsigned long stacksize,
	void *arg
)
{
	unsigned long rc;
	struct FORK_ARG fa;
	
	fa.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	fa.threadcode = threadcode;
	fa.arg = arg;
	rc = _beginthread((void (__cdecl *)(void*))forkthread_r, stacksize, &fa);
	if ((unsigned long) -1L != rc) {
		WaitForSingleObject(fa.hEvent, INFINITE);
	}
	CloseHandle(fa.hEvent);
	return rc;
}
