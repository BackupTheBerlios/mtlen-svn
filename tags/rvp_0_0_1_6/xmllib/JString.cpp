/*

RVP Protocol Plugin for Miranda IM
Copyright (C) 2005 Piotr Piastucki

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

#include "JString.h"

JString::JString()
{
	len=0;
	dataSize=0;
	data=NULL;
}

JString::JString(JString *s)
{
	len=0;
	dataSize=0;
	data=NULL;
	add(s);
}

JString::JString(const char *s)
{
	len=0;
	dataSize=0;
	data=NULL;
	add(s);
}

JString::JString(const char *s, int l)
{
	len=0;
	dataSize=0;
	data=NULL;
	add(s, l);
}

JString::~JString()
{
	if (data!=NULL) delete data;
}

void JString::increaseDataSize(int amount)
{
	if (amount+len<dataSize) return;

	char *data2=new char[dataSize+amount];
	if (data!=NULL) {
		memcpy(data2,data,len);
		delete data;
	}
	data=data2;
	dataSize+=amount;
}


void JString::add(char c)
{
	if (dataSize<=len || data==NULL) {
		increaseDataSize(32);
	}
	data[len]=c;
	len++;
}

void JString::add(JString *s)
{
	increaseDataSize(s->length());
	memcpy(&data[len],s->data,s->length());
	len+=s->length();
	return;
}

void JString::add(const char *p)
{	int l;
	l=strlen(p);
	increaseDataSize(l);
	memcpy(&data[len],p,l);
	len+=l;
	return;
}

void JString::add(const char *p, int l)
{
//	char str[1024];
//	sprintf->info(str, "%d ", l);
	increaseDataSize(l);
	memcpy(&data[len],p,l);
	len+=l;
	return;
}

int JString::length()
{
	return len;
}

char JString::getChar(int pos)
{
	return data[pos];
}

bool JString::equals(JString *s)
{
	if (s->length()!=len) return false;
	for (int i=0;i<len;i++) {
		if (s->getChar(i)!=getChar(i)) return false;
	}
	return true;
}

bool JString::equals(const char *s)
{
	if (strlen(s)!=len) return false;
	for (int i=0;i<len;i++) {
		if (s[i]!=getChar(i)) return false;
	}
	return true;
}

char* JString::toString()
{	char *s;
	int i;
	s=new char[len+1];
 	for (i=0;i<len;i++) {
		s[i]=getChar(i);
	}
	s[i]=0;
	return s;
}
