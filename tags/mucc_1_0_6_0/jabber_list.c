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
	int i;

	if (item == NULL)
		return;

	if (item->jid) free(item->jid);
	if (item->nick) free(item->nick);
	for (i=0; i<item->resourceCount; i++) {
		if (item->resource[i].resourceName) free(item->resource[i].resourceName);
		if (item->resource[i].statusMessage) free(item->resource[i].statusMessage);
		if (item->resource[i].software) free(item->resource[i].software);
		if (item->resource[i].version) free(item->resource[i].version);
		if (item->resource[i].system) free(item->resource[i].system);
	}
	if (item->resource) free(item->resource);
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
	item->resourceCount = 0;
	item->resource = NULL;
	item->resourceMode = RSMODE_LASTSEEN;
	item->defaultResource = -1;
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
	int i, j, resourceCount;
	char *p, *q;
	char *resource;
	JABBER_RESOURCE_STATUS *r;

	EnterCriticalSection(&csLists);
	i = JabberListExist(list, jid);
	if (!i) {
		LeaveCriticalSection(&csLists);
		return;
	}
	i--;

	if ((p=strchr(jid, '@')) != NULL) {
		if ((q=strchr(p, '/')) != NULL) {
			resource = q+1;
			if (resource[0]) {
				r = lists[i].resource;
				if (r == NULL) {
					r = (JABBER_RESOURCE_STATUS *) malloc(sizeof(JABBER_RESOURCE_STATUS));
					lists[i].resourceCount = 1;
					r->status = status;
					r->resourceName = _strdup(resource);
					if (statusMessage)
						r->statusMessage = _strdup(statusMessage);
					else
						r->statusMessage = NULL;
					r->software = r->version = r->system = NULL;
				}
				else {
					resourceCount = lists[i].resourceCount;
					r = lists[i].resource;
					for (j=0; j<resourceCount; j++) {
						if (!strcmp(r[j].resourceName, resource)) {
							// Already exist, update status and statusMessage
							r[j].status = status;
							if (r[j].statusMessage) free(r[j].statusMessage);
							if (statusMessage)
								r[j].statusMessage = _strdup(statusMessage);
							else
								r[j].statusMessage = NULL;
							break;
						}
					}
					if (j >= resourceCount) {
						// Not already exist, add new resource
						r = (JABBER_RESOURCE_STATUS *) realloc(r, (resourceCount+1)*sizeof(JABBER_RESOURCE_STATUS));
						r[resourceCount].status = status;
						r[resourceCount].resourceName = _strdup(resource);
						if (statusMessage)
							r[resourceCount].statusMessage = _strdup(statusMessage);
						else
							r[resourceCount].statusMessage = NULL;
						r[resourceCount].software = r[resourceCount].version = r[resourceCount].system = NULL;
						lists[i].resourceCount++;
					}
				}
				lists[i].resource = r;
#ifdef _DEBUG
				PrintResource(i);
#endif
			}
		}
		else {
			// No resource, update the main statusMessage
			if (lists[i].statusMessage != NULL)
				free(lists[i].statusMessage);
			if (statusMessage)
				lists[i].statusMessage = _strdup(statusMessage);
			else
				lists[i].statusMessage = NULL;
		}
	}
	LeaveCriticalSection(&csLists);
}

void JabberListRemoveResource(JABBER_LIST list, const char *jid)
{
	int i, j, resourceCount;
	char *p, *q;
	char *resource;
	JABBER_RESOURCE_STATUS *r;

	EnterCriticalSection(&csLists);
	i = JabberListExist(list, jid);
	if (!i || lists[i-1].resource==NULL) {
		LeaveCriticalSection(&csLists);
		return;
	}
	i--;

	if ((p=strchr(jid, '@')) != NULL) {
		if ((q=strchr(p, '/')) != NULL) {
			resource = q+1;
			if (resource[0]) {
				r = lists[i].resource;
				resourceCount = lists[i].resourceCount;
				for (j=0; j<resourceCount; j++) {
					if (!strcmp(r[j].resourceName, resource))
						break;
				}
				if (j < resourceCount) {
					// Found resource to be removed
					if (lists[i].defaultResource == j)
						lists[i].defaultResource = -1;
					else if (lists[i].defaultResource > j)
						lists[i].defaultResource--;
					if (r[j].resourceName) free(r[j].resourceName);
					if (r[j].statusMessage) free(r[j].statusMessage);
					if (r[j].software) free(r[j].software);
					if (r[j].version) free(r[j].version);
					if (r[j].system) free(r[j].system);
					if (resourceCount == 1) {
						free(r);
						r = NULL;
					}
					else {
						resourceCount--;
						memmove(r+j, r+j+1, (resourceCount-j)*sizeof(JABBER_RESOURCE_STATUS));
						r = realloc(r, resourceCount*sizeof(JABBER_RESOURCE_STATUS));
					}
					lists[i].resourceCount--;
					lists[i].resource = r;
#ifdef _DEBUG
					PrintResource(i);
#endif
				}
			}
		}
	}
	LeaveCriticalSection(&csLists);
}

char *JabberListGetBestResourceNamePtr(const char *jid)
{
	char *res;
	int i, status, s;
	BOOL foundBetter;
	JABBER_RESOURCE_STATUS *r;
	int resourceCount;

	EnterCriticalSection(&csLists);
	i = JabberListExist(LIST_ROSTER, jid);
	if (!i || lists[i-1].resource==NULL) {
		LeaveCriticalSection(&csLists);
		return NULL;
	}
	i--;

	if ((res=JabberListGetBestClientResourceNamePtr(jid)) == NULL) {
		r = lists[i].resource;
		resourceCount = lists[i].resourceCount;
		status = ID_STATUS_OFFLINE;
		res = NULL;
		for (i=0; i<resourceCount; i++) {
			s = r[i].status;
			foundBetter = FALSE;
			switch (s) {
			case ID_STATUS_FREECHAT:
				foundBetter = TRUE;
				break;
			case ID_STATUS_ONLINE:
				if (status != ID_STATUS_FREECHAT)
					foundBetter = TRUE;
				break;
			case ID_STATUS_DND:
				if (status!=ID_STATUS_FREECHAT && status!=ID_STATUS_ONLINE)
					foundBetter = TRUE;
				break;
			case ID_STATUS_AWAY:
				if (status!=ID_STATUS_FREECHAT && status!=ID_STATUS_ONLINE && status!=ID_STATUS_DND)
					foundBetter = TRUE;
				break;
			case ID_STATUS_NA:
				if (status!=ID_STATUS_FREECHAT && status!=ID_STATUS_ONLINE && status!=ID_STATUS_DND && status!=ID_STATUS_AWAY)
					foundBetter = TRUE;
				break;
			}
			if (foundBetter) {
				res = r[i].resourceName;
				status = s;
			}
		}
	}

	LeaveCriticalSection(&csLists);
	return res;
}

char *JabberListGetBestClientResourceNamePtr(const char *jid)
{
	char *res;
	int i;

	EnterCriticalSection(&csLists);
	i = JabberListExist(LIST_ROSTER, jid);
	if (!i || lists[i-1].resource==NULL) {
		LeaveCriticalSection(&csLists);
		return NULL;
	}
	i--;

	if (lists[i].resourceCount == 1) {
		res = lists[i].resource[0].resourceName;
	}
	else {
		res = NULL;
		if (lists[i].resourceMode==RSMODE_MANUAL || lists[i].resourceMode==RSMODE_LASTSEEN) {
			if (lists[i].defaultResource>=0 && lists[i].defaultResource<lists[i].resourceCount)
				res = lists[i].resource[lists[i].defaultResource].resourceName;
		}
	}

	LeaveCriticalSection(&csLists);
	return res;
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

