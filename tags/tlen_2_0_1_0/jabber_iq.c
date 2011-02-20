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


void JabberIqInit(TlenProtocol *proto)
{
	InitializeCriticalSection(&proto->csIqList);
    proto->iqList = NULL;
	proto->iqCount = 0;
	proto->iqAlloced = 0;
}

void JabberIqUninit(TlenProtocol *proto)
{
	if (proto->iqList) mir_free(proto->iqList);
	proto->iqList = NULL;
	proto->iqCount = 0;
	proto->iqAlloced = 0;
	DeleteCriticalSection(&proto->csIqList);
}

static void JabberIqRemove(TlenProtocol *proto, int index)
{
	EnterCriticalSection(&proto->csIqList);
	if (index>=0 && index<proto->iqCount) {
		memmove(proto->iqList+index, proto->iqList+index+1, sizeof(JABBER_IQ_FUNC)*(proto->iqCount-index-1));
		proto->iqCount--;
	}
	LeaveCriticalSection(&proto->csIqList);
}

static void JabberIqExpire(TlenProtocol *proto)
{
	int i;
	time_t expire;

	EnterCriticalSection(&proto->csIqList);
	expire = time(NULL) - 120;	// 2 minute
	i = 0;
	while (i < proto->iqCount) {
		if (proto->iqList[i].requestTime < expire)
			JabberIqRemove(proto, i);
		else
			i++;
	}
	LeaveCriticalSection(&proto->csIqList);
}

JABBER_IQ_PFUNC JabberIqFetchFunc(TlenProtocol *proto, int iqId)
{
	int i;
	JABBER_IQ_PFUNC res;

	EnterCriticalSection(&proto->csIqList);
	JabberIqExpire(proto);
	for (i=0; i<proto->iqCount && proto->iqList[i].iqId!=iqId; i++);
	if (i < proto->iqCount) {
		res = proto->iqList[i].func;
		JabberIqRemove(proto, i);
	}
	else {
		res = (JABBER_IQ_PFUNC) NULL;
	}
	LeaveCriticalSection(&proto->csIqList);
	return res;
}

void JabberIqAdd(TlenProtocol *proto, unsigned int iqId, JABBER_IQ_PROCID procId, JABBER_IQ_PFUNC func)
{
	int i;

	EnterCriticalSection(&proto->csIqList);
	if (procId == IQ_PROC_NONE)
		i = proto->iqCount;
	else
		for (i=0; i<proto->iqCount && proto->iqList[i].procId!=procId; i++);

	if (i>=proto->iqCount && proto->iqCount>=proto->iqAlloced) {
		proto->iqAlloced = proto->iqCount + 8;
		proto->iqList = mir_realloc(proto->iqList, sizeof(JABBER_IQ_FUNC)*proto->iqAlloced);
	}

	if (proto->iqList != NULL) {
		proto->iqList[i].iqId = iqId;
		proto->iqList[i].procId = procId;
		proto->iqList[i].func = func;
		proto->iqList[i].requestTime = time(NULL);
		if (i == proto->iqCount) proto->iqCount++;
	}
	LeaveCriticalSection(&proto->csIqList);
}

