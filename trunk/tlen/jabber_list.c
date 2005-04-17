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

static int count;
static JABBER_LIST_ITEM *lists;
static CRITICAL_SECTION csLists;

static void JabberListFreeItemInternal(JABBER_LIST_ITEM *item);

#ifdef _DEBUG
static void PrintResource(int index)
{
	char *p;
	int i, resourceCount;
	JABBER_RESOURCE_STATUS *resource;
	char *str;

	if (index<0 || index>=count) {
		JabberLog("PrintResource(): index out-of-bound");
	}
	else {
		resourceCount = lists[index].resourceCount;
		if (resourceCount <= 0)
			JabberLog("Resource count is zero");
		else {
			if ((str=malloc(resourceCount * 48)) != NULL) {
				p = str;
				for (i=0; i<resourceCount; i++) {
					resource = &(lists[index].resource[i]);
					sprintf(p, "(%d)", resource->status);
					for (; *p!='\0'; p++);
					if (resource->resourceName)
						sprintf(p, "%s-", resource->resourceName);
					else
						strcpy(p, "(NULL)");
					for (; *p!='\0'; p++);
				}
				JabberLog("Resource is now '%s'", str);
				free(str);
			}
		}
	}
}
#endif

void JabberListInit(void)
{
	lists = NULL;
	count = 0;
	InitializeCriticalSection(&csLists);
}

void JabberListUninit(void)
{
	JabberListWipe();
	DeleteCriticalSection(&csLists);
}

void JabberListWipe(void)
{
	int i;

	EnterCriticalSection(&csLists);
	for(i=0; i<count; i++)
		JabberListFreeItemInternal(&(lists[i]));
	if (lists != NULL) {
		free(lists);
		lists = NULL;
	}
	count=0;
	LeaveCriticalSection(&csLists);
}

void JabberListWipeSpecial(void)
{
	int i;
	EnterCriticalSection(&csLists);
	for(i=0; i<count; i++) {
		if (lists[i].list != LIST_FILE && lists[i].list != LIST_VOICE) {
			JabberListFreeItemInternal(&(lists[i]));
			count--;
			memmove(lists+i, lists+i+1, sizeof(JABBER_LIST_ITEM)*(count-i));
			i--;
		}
	}
	lists = (JABBER_LIST_ITEM *) realloc(lists, sizeof(JABBER_LIST_ITEM)*count);
	LeaveCriticalSection(&csLists);
}

static void JabberListFreeItemInternal(JABBER_LIST_ITEM *item)
{
	if (item == NULL)
		return;

	if (item->jid) free(item->jid);
	if (item->nick) free(item->nick);
	if (item->statusMessage) free(item->statusMessage);
	if (item->group) free(item->group);
	if (item->photoFileName) {
		DeleteFile(item->photoFileName);
		free(item->photoFileName);
	}
	if (item->messageEventIdStr) free(item->messageEventIdStr);
	if (item->type) free(item->type);
	//if (item->ft) JabberFileFreeFt(item->ft); // No need to free (it is always free when exit from JabberFileServerThread())
	if (item->roomName) free(item->roomName);
}

int JabberListExist(JABBER_LIST list, const char *jid)
{
	int i, len;
	char *s, *p, *q;

	s = _strdup(jid); _strlwr(s);
	// strip resouce name if any
	if ((p=strchr(s, '@')) != NULL) {
		if ((q=strchr(p, '/')) != NULL)
			*q = '\0';
	}
	len = strlen(s);

	EnterCriticalSection(&csLists);
	for(i=0; i<count; i++)
		if (lists[i].list==list) {
			p = lists[i].jid;
			if (p && (int)strlen(p)>=len && (p[len]=='\0'||p[len]=='/') && !strncmp(p, s, len)) {
			  	LeaveCriticalSection(&csLists);
				free(s);
				return i+1;
			}
		}
	LeaveCriticalSection(&csLists);
	free(s);
	return 0;
}

JABBER_LIST_ITEM *JabberListAdd(JABBER_LIST list, const char *jid)
{
	char *s, *p, *q;
	JABBER_LIST_ITEM *item;

	EnterCriticalSection(&csLists);
	if ((item=JabberListGetItemPtr(list, jid)) != NULL) {
		LeaveCriticalSection(&csLists);
		return item;
	}

	s = _strdup(jid); _strlwr(s);
	// strip resource name if any
	if ((p=strchr(s, '@')) != NULL) {
		if ((q=strchr(p, '/')) != NULL)
			*q = '\0';
	}

	lists = (JABBER_LIST_ITEM *) realloc(lists, sizeof(JABBER_LIST_ITEM)*(count+1));
	item = &(lists[count]);
	item->list = list;
	item->jid = s;
	item->nick = NULL;
	item->status = ID_STATUS_OFFLINE;
	item->statusMessage = NULL;
	item->group = NULL;
	item->photoFileName = NULL;
	item->messageEventIdStr = NULL;
	item->wantComposingEvent = FALSE;
	item->type = NULL;
	item->ft = NULL;
	item->roomName = NULL;
	count++;
	LeaveCriticalSection(&csLists);

	return item;
}

void JabberListRemove(JABBER_LIST list, const char *jid)
{
	int i;

	EnterCriticalSection(&csLists);
	i = JabberListExist(list, jid);
	if (!i) {
		LeaveCriticalSection(&csLists);
		return;
	}
	i--;
	JabberListFreeItemInternal(&(lists[i]));
	count--;
	memmove(lists+i, lists+i+1, sizeof(JABBER_LIST_ITEM)*(count-i));
	lists = (JABBER_LIST_ITEM *) realloc(lists, sizeof(JABBER_LIST_ITEM)*count);
	LeaveCriticalSection(&csLists);
}

void JabberListRemoveList(JABBER_LIST list)
{
	int i;

	i = 0;
	while ((i=JabberListFindNext(list, i)) >= 0) {
		JabberListRemoveByIndex(i);
	}
}

void JabberListRemoveByIndex(int index)
{
	EnterCriticalSection(&csLists);
	if (index>=0 && index<count) {
		JabberListFreeItemInternal(&(lists[index]));
		count--;
		memmove(lists+index, lists+index+1, sizeof(JABBER_LIST_ITEM)*(count-index));
		lists = (JABBER_LIST_ITEM *) realloc(lists, sizeof(JABBER_LIST_ITEM)*count);
	}
	LeaveCriticalSection(&csLists);
}

void JabberListAddResource(JABBER_LIST list, const char *jid, int status, const char *statusMessage)
{
	int i;

	EnterCriticalSection(&csLists);
	i = JabberListExist(list, jid);
	if (!i) {
		LeaveCriticalSection(&csLists);
		return;
	}
	i--;

	if (lists[i].statusMessage != NULL)
		free(lists[i].statusMessage);
	if (statusMessage)
		lists[i].statusMessage = _strdup(statusMessage);
	else
		lists[i].statusMessage = NULL;
	LeaveCriticalSection(&csLists);
}

void JabberListRemoveResource(JABBER_LIST list, const char *jid)
{
	int i;
	EnterCriticalSection(&csLists);
	i = JabberListExist(list, jid);
	if (!i) {
		LeaveCriticalSection(&csLists);
		return;
	}
	i--;
	LeaveCriticalSection(&csLists);
}

int JabberListFindNext(JABBER_LIST list, int fromOffset)
{
	int i;

	EnterCriticalSection(&csLists);
	i = (fromOffset>=0) ? fromOffset : 0;
	for(; i<count; i++)
		if (lists[i].list == list) {
		  	LeaveCriticalSection(&csLists);
			return i;
		}
	LeaveCriticalSection(&csLists);
	return -1;
}

JABBER_LIST_ITEM *JabberListGetItemPtr(JABBER_LIST list, const char *jid)
{
	int i;

	EnterCriticalSection(&csLists);
	i = JabberListExist(list, jid);
	if (!i) {
		LeaveCriticalSection(&csLists);
		return NULL;
	}
	i--;
	LeaveCriticalSection(&csLists);
	return &(lists[i]);
}

JABBER_LIST_ITEM *JabberListGetItemPtrFromIndex(int index)
{
	EnterCriticalSection(&csLists);
	if (index>=0 && index<count) {
		LeaveCriticalSection(&csLists);
		return &(lists[index]);
	}
	LeaveCriticalSection(&csLists);
	return NULL;
}

