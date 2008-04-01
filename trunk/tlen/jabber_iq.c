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
#include "jabber_iq.h"


static CRITICAL_SECTION csIqList;
static JABBER_IQ_FUNC *iqList;
static int iqCount;
static int iqAlloced;

void JabberIqInit(TlenProtocol *proto)
{
	InitializeCriticalSection(&csIqList);
	iqList = NULL;
	iqCount = 0;
	iqAlloced = 0;
}

void JabberIqUninit(TlenProtocol *proto)
{
	if (iqList) mir_free(iqList);
	iqList = NULL;
	iqCount = 0;
	iqAlloced = 0;
	DeleteCriticalSection(&csIqList);
}

static void JabberIqRemove(TlenProtocol *proto, int index)
{
	EnterCriticalSection(&csIqList);
	if (index>=0 && index<iqCount) {
		memmove(iqList+index, iqList+index+1, sizeof(JABBER_IQ_FUNC)*(iqCount-index-1));
		iqCount--;
	}
	LeaveCriticalSection(&csIqList);
}

static void JabberIqExpire(TlenProtocol *proto)
{
	int i;
	time_t expire;

	EnterCriticalSection(&csIqList);
	expire = time(NULL) - 120;	// 2 minute
	i = 0;
	while (i < iqCount) {
		if (iqList[i].requestTime < expire)
			JabberIqRemove(proto, i);
		else
			i++;
	}
	LeaveCriticalSection(&csIqList);
}

JABBER_IQ_PFUNC JabberIqFetchFunc(TlenProtocol *proto, int iqId)
{
	int i;
	JABBER_IQ_PFUNC res;

	EnterCriticalSection(&csIqList);
	JabberIqExpire(proto);
	for (i=0; i<iqCount && iqList[i].iqId!=iqId; i++);
	if (i < iqCount) {
		res = iqList[i].func;
		JabberIqRemove(proto, i);
	}
	else {
		res = (JABBER_IQ_PFUNC) NULL;
	}
	LeaveCriticalSection(&csIqList);
	return res;
}

void JabberIqAdd(TlenProtocol *proto, unsigned int iqId, JABBER_IQ_PROCID procId, JABBER_IQ_PFUNC func)
{
	int i;

	EnterCriticalSection(&csIqList);
	if (procId == IQ_PROC_NONE)
		i = iqCount;
	else
		for (i=0; i<iqCount && iqList[i].procId!=procId; i++);

	if (i>=iqCount && iqCount>=iqAlloced) {
		iqAlloced = iqCount + 8;
		iqList = mir_realloc(iqList, sizeof(JABBER_IQ_FUNC)*iqAlloced);
	}

	if (iqList != NULL) {
		iqList[i].iqId = iqId;
		iqList[i].procId = procId;
		iqList[i].func = func;
		iqList[i].requestTime = time(NULL);
		if (i == iqCount) iqCount++;
	}
	LeaveCriticalSection(&csIqList);
}

