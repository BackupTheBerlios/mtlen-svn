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
#include "Utils.h"
#include <windows.h>
#include <process.h>
#include <stdarg.h>

#include <newpluginapi.h>
#include <m_system.h>
#include <m_protomod.h>
#include <m_clist.h>
#include <m_database.h>

extern char *rvpProtoName;
static char b64table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static unsigned char b64rtable[256];

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
   callercode(arg);
   CallService(MS_SYSTEM_THREAD_POP, 0, 0);
   return;
}

unsigned long Utils::forkThread(void (__cdecl *threadcode)(void*), unsigned long stacksize,   void *arg) {

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

void Utils::appendText(char **str, int *sizeAlloced, const char *fmt, ...) {
	va_list vararg;
	char *p;
	int size, len;

	if (str == NULL) return;

	if (*str==NULL || *sizeAlloced<=0) {
		*sizeAlloced = size = 2048;
		*str = (char *) malloc(size);
		len = 0;
	}
	else {
		len = strlen(*str);
		size = *sizeAlloced - strlen(*str);
	}

	p = *str + len;
	va_start(vararg, fmt);
	while (_vsnprintf(p, size, fmt, vararg) == -1) {
		size += 2048;
		(*sizeAlloced) += 2048;
		*str = (char *) realloc(*str, *sizeAlloced);
		p = *str + len;
	}
	va_end(vararg);
}

void Utils::appendText(wchar_t **str, int *sizeAlloced, const wchar_t *fmt, ...) {
	va_list vararg;
	wchar_t *p;
	int size, len;

	if (str == NULL) return;

	if (*str==NULL || *sizeAlloced<=0) {
		*sizeAlloced = size = 2048;
		*str = (wchar_t *) malloc(size);
		len = 0;
	}
	else {
		len = wcslen(*str);
		size = *sizeAlloced - sizeof(wchar_t) * wcslen(*str);
	}

	p = *str + len;
	va_start(vararg, fmt);
	while (_vsnwprintf(p, size / sizeof(wchar_t), fmt, vararg) == -1) {
		size += 2048;
		(*sizeAlloced) += 2048;
		*str = (wchar_t *) realloc(*str, *sizeAlloced);
		p = *str + len;
	}
	va_end(vararg);
}

char *Utils::dupString(const char *a) {
	if (a!=NULL) {
		char *b = new char[strlen(a)+1];
		strcpy(b, a);
		return b;
	}
	return NULL;
}

char *Utils::dupString(const char *a, int l) {
	if (a!=NULL) {
		char *b = new char[l+1];
		strncpy(b, a, l);
		b[l] ='\0';
		return b;
	}
	return NULL;
}

wchar_t *Utils::dupString(const wchar_t *a) {
	if (a!=NULL) {
		wchar_t *b = new wchar_t[wcslen(a)+1];
		wcscpy(b, a);
		return b;
	}
	return NULL;
}

wchar_t *Utils::dupString(const wchar_t *a, int l) {
	if (a!=NULL) {
		wchar_t *b = new wchar_t[l+1];
		wcsncpy(b, a, l);
		b[l] ='\0';
		return b;
	}
	return NULL;
}


wchar_t *Utils::convertToWCS(const char *a) {
	if (a!=NULL) {
		int len;
//		len = strlen(a)+1;
		if ((len=MultiByteToWideChar(CP_ACP, 0, a, -1, NULL, 0)) == 0) return NULL;
		wchar_t *b = new wchar_t[len];
		MultiByteToWideChar(CP_ACP, 0, a, len, b, len);
		return b;
	}
	return NULL;
}

char *Utils::convertToString(const wchar_t *a) {
	if (a!=NULL) {
		int len;
//		len	= wcslen(a)+1;
		if ((len=WideCharToMultiByte(CP_ACP, 0, a, -1, NULL, 0, NULL, FALSE)) == 0) return NULL;
		char *b = new char[len];
		WideCharToMultiByte(CP_ACP, 0, a, len, b, len, NULL, FALSE);
		return b;
	}
	return NULL;
}


void Utils::convertPath(char *path) {
   	for (; *path!='\0'; path++) {
   	    if (*path == '\\') *path = '/';
   	}
}


DWORD Utils::safe_wcslen(wchar_t *msg, DWORD maxLen) {
    DWORD i;
	for (i = 0; i < maxLen; i++) {
		if (msg[i] == (wchar_t)0)
			return i;
	}
	return 0;
}


void Utils::ltrim(char *t, char *p) {
	int i, l;
	l=strlen(t);
	for (i=0;i<l;i++) {
		if (!strchr(p, t[i])) break;
	}
	memcpy(t, t+i, l-i+1);
}


void Utils::rtrim(char *t, char *p) {
	int i, l;
	l=strlen(t);
	for (i=l-1;i>=0;i--) {
		if (!strchr(p, t[i])) break;
	}
	t[i+1] = '\0';
}

void Utils::trim(char *t, char *p) {
	ltrim(t, p);
	rtrim(t, p);
}


char *Utils::base64Encode(const char *buffer, int bufferLen) {
	int n;
	unsigned char igroup[3];
	char *p, *peob;
	char *res, *r;

	if (buffer==NULL || bufferLen<0) return NULL;
	res=new char[(bufferLen+2)/3*4 + 1];

	for (p=(char*)buffer,peob=p+bufferLen,r=res; p<peob;) {
		igroup[0] = igroup[1] = igroup[2] = 0;
		for (n=0; n<3; n++) {
			if (p >= peob) break;
			igroup[n] = (unsigned char) *p;
			p++;
		}
		if (n > 0) {
			r[0] = b64table[ igroup[0]>>2 ];
			r[1] = b64table[ ((igroup[0]&3)<<4) | (igroup[1]>>4) ];
			r[2] = b64table[ ((igroup[1]&0xf)<<2) | (igroup[2]>>6) ];
			r[3] = b64table[ igroup[2]&0x3f ];
			if (n < 3) {
				r[3] = '=';
				if (n < 2)
					r[2] = '=';
			}
			r += 4;
		}
	}
	*r = '\0';
	return res;
}

char *Utils::base64Decode(const char *str, int *resultLen) {
	char *res;
	unsigned char *p, *r, igroup[4], a[4];
	int n, num, count;

	if (str==NULL || resultLen==NULL) return NULL;
	res=new char[(strlen(str)+3)/4*3 + 1];
	for (n=0; n<256; n++)
		b64rtable[n] = (unsigned char) 0x80;
	for (n=0; n<26; n++)
		b64rtable['A'+n] = n;
	for (n=0; n<26; n++)
		b64rtable['a'+n] = n + 26;
	for (n=0; n<10; n++)
		b64rtable['0'+n] = n + 52;
	b64rtable[(int)'+'] = 62;
	b64rtable[(int)'/'] = 63;
	b64rtable[(int)'='] = 0;
	count = 0;
	for (p=(unsigned char *)str,r=(unsigned char *)res; *p!='\0';) {
		for (n=0; n<4; n++) {
			if (*p=='\0' || b64rtable[*p]==0x80) {
				delete res;
				return NULL;
			}
			a[n] = *p;
			igroup[n] = b64rtable[*p];
			p++;
		}
		r[0] = igroup[0]<<2 | igroup[1]>>4;
		r[1] = igroup[1]<<4 | igroup[2]>>2;
		r[2] = igroup[2]<<6 | igroup[3];
		r += 3;
		num = (a[2]=='='?1:(a[3]=='='?2:3));
		count += num;
		if (num < 3) break;
	}
	*resultLen = count;

	return res;
}

char *Utils::utf8Encode(const char *str) {
	unsigned char *szOut;
	int len, i;
	wchar_t *wszTemp, *w;

	if (str == NULL) return NULL;
	len = strlen(str);
	// Convert local codepage to unicode
	wszTemp = new wchar_t[len + 1];
	if (wszTemp == NULL) return NULL;
	MultiByteToWideChar(CP_ACP, 0, str, -1, wszTemp, len + 1);

	// Convert unicode to utf8
	len = 0;
	for (w=wszTemp; *w; w++) {
		if (*w < 0x0080) len++;
		else if (*w < 0x0800) len += 2;
		else len += 3;
	}
	szOut = new unsigned char[len+1];
	if (szOut == NULL) {
		delete wszTemp;
		return NULL;
	}
	i = 0;
	for (w=wszTemp; *w; w++) {
		if (*w < 0x0080)
			szOut[i++] = (unsigned char) *w;
		else if (*w < 0x0800) {
			szOut[i++] = 0xc0 | ((*w) >> 6);
			szOut[i++] = 0x80 | ((*w) & 0x3f);
		}
		else {
			szOut[i++] = 0xe0 | ((*w) >> 12);
			szOut[i++] = 0x80 | (((*w) >> 6) & 0x3f);
			szOut[i++] = 0x80 | ((*w) & 0x3f);
		}
	}
	szOut[i] = '\0';
	delete wszTemp;
	return (char *)szOut;
}

char *Utils::utf8Encode2(const char *str) {
	unsigned char *szOut;
	int len, i;
	wchar_t *wszTemp, *w;

	if (str == NULL) return NULL;
	len = strlen(str);
	// Convert local codepage to unicode
	wszTemp = new wchar_t[len + 1];
	if (wszTemp == NULL) return NULL;
	for (i = 0; i < len; i++) {
		wszTemp[i] = (unsigned char)str[i];
	}
	wszTemp[i] = '\0';
	// Convert unicode to utf8
	len = 0;
	for (w=wszTemp; *w; w++) {
		if (*w < 0x0080) len++;
		else if (*w < 0x0800) len += 2;
		else len += 3;
	}
	szOut = new unsigned char[len+1];
	if (szOut == NULL) {
		delete wszTemp;
		return NULL;
	}
	i = 0;
	for (w=wszTemp; *w; w++) {
		if (*w < 0x0080)
			szOut[i++] = (unsigned char) *w;
		else if (*w < 0x0800) {
			szOut[i++] = 0xc0 | ((*w) >> 6);
			szOut[i++] = 0x80 | ((*w) & 0x3f);
		}
		else {
			szOut[i++] = 0xe0 | ((*w) >> 12);
			szOut[i++] = 0x80 | (((*w) >> 6) & 0x3f);
			szOut[i++] = 0x80 | ((*w) & 0x3f);
		}
	}
	szOut[i] = '\0';
	delete wszTemp;
	return (char *)szOut;
}

char *Utils::utf8Decode(const char *str) {
	int i, len;
	char *p;
	wchar_t *wszTemp;
	char *szOut;

	if (str == NULL) return NULL;
	len = strlen(str);
	// Convert utf8 to unicode
	wszTemp = new wchar_t[len + 1];
	if (wszTemp == NULL) return NULL;
	p = (char *) str;
	i = 0;
	while (*p) {
		if ((*p & 0x80) == 0) {
			wszTemp[i++] = *(p++);
		} else if ((*p & 0xe0) == 0xe0) {
			wszTemp[i] = (*(p++) & 0x1f) << 12;
			wszTemp[i] |= (*(p++) & 0x3f) << 6;
			wszTemp[i++] |= (*(p++) & 0x3f);
		} else {
			wszTemp[i] = (*(p++) & 0x3f) << 6;
			wszTemp[i++] |= (*(p++) & 0x3f);
		}
	}
	wszTemp[i] = '\0';

	// Convert unicode to local codepage
	if ((len=WideCharToMultiByte(CP_ACP, 0, wszTemp, -1, NULL, 0, NULL, NULL)) == 0) return NULL;
	szOut = new char[len];
	if (szOut == NULL) {
		delete wszTemp;
		return NULL;
	}
	WideCharToMultiByte(CP_ACP, 0, wszTemp, -1, szOut, len, NULL, NULL);
	delete wszTemp;
	return szOut;
}

char *Utils::utf8Decode2(const char *str, int len) {
	int i;
	char *p;
	unsigned char *szOut;
	if (str == NULL) return NULL;
	szOut = new unsigned char[2 * (len + 1)];
	p = (char *) str;
	i = 0;
	while (*p) {
		wchar_t wszTemp = 0;
		if ((*p & 0x80) == 0) {
			szOut[i++] = *(p++);
		} else if ((*p & 0xe0) == 0xe0) {
			wszTemp = (*(p++) & 0x1f) << 12;
			wszTemp |= (*(p++) & 0x3f) << 6;
			wszTemp |= (*(p++) & 0x3f);
			szOut[i++] = (unsigned char) (wszTemp & 0xFF);
		} else {
			wszTemp = (*(p++) & 0x3f) << 6;
			wszTemp |= (*(p++) & 0x3f);
			szOut[i++] = (unsigned char) (wszTemp & 0xFF);
		}
	}
	szOut[i] = '\0';
	return (char *)szOut;
}

char *Utils::utf8Encode(const wchar_t *str) {
	unsigned char *szOut;
	int len, i;
	const wchar_t *w;

	if (str == NULL) return NULL;
	len = wcslen(str);
	// Convert local codepage to unicode

	// Convert unicode to utf8
	len = 0;
	for (w=str; *w; w++) {
		if (*w < 0x0080) len++;
		else if (*w < 0x0800) len += 2;
		else len += 3;
	}
	szOut = new unsigned char[len+1];
	if (szOut == NULL) {
		return NULL;
	}
	i = 0;
	for (w=str; *w; w++) {
		if (*w < 0x0080)
			szOut[i++] = (unsigned char) *w;
		else if (*w < 0x0800) {
			szOut[i++] = 0xc0 | ((*w) >> 6);
			szOut[i++] = 0x80 | ((*w) & 0x3f);
		}
		else {
			szOut[i++] = 0xe0 | ((*w) >> 12);
			szOut[i++] = 0x80 | (((*w) >> 6) & 0x3f);
			szOut[i++] = 0x80 | ((*w) & 0x3f);
		}
	}
	szOut[i] = '\0';
	return (char *) szOut;
}

wchar_t *Utils::utf8DecodeW(const char *str) {
	int i, len;
	char *p;
	wchar_t *wszTemp;

	if (str == NULL) return NULL;
	len = strlen(str);
	// Convert utf8 to unicode
	wszTemp = new wchar_t[len + 1];
	if (wszTemp == NULL) return NULL;
	p = (char *) str;
	i = 0;
	while (*p) {
		if ((*p & 0x80) == 0) {
			wszTemp[i++] = *(p++);
		} else if ((*p & 0xe0) == 0xe0) {
			wszTemp[i] = (*(p++) & 0x1f) << 12;
			wszTemp[i] |= (*(p++) & 0x3f) << 6;
			wszTemp[i++] |= (*(p++) & 0x3f);
		} else {
			wszTemp[i] = (*(p++) & 0x3f) << 6;
			wszTemp[i++] |= (*(p++) & 0x3f);
		}
	}
	wszTemp[i] = '\0';
	return wszTemp;
}

char *Utils::cdataEncode(const char *str) {
	const char *p;
	char *out;
	int cdataLen = strlen("<![CDATA[]]>");
	int len = cdataLen;
	if (str == NULL) return NULL;
	for (p=str; *p; p++, len++) {
		if (p[0] == ']') {
			if (p[1] == ']') {
				if (p[2] == '>') {
					len += cdataLen;
				}
			}
		}
	}
	out = new char[len+1];
	memcpy(out, "<![CDATA[", 9);
	len = 9;
	for (p=str; *p; p++, len++) {
		if (p[0] == ']') {
			if (p[1] == ']') {
				if (p[2] == '>') {
					out[len] = *p;
					memcpy(out + len + 1, "]]><![CDATA[", cdataLen);
					len += cdataLen;
					continue;
				}
			}
		}
		out[len] = *p;
	}
	memcpy(out + len, "]]>\0", 4);
	return out;
}

char *Utils::getLine(const char *str, int* len) {
	int i;
	for (i=0; str[i]!='\0'; i++) {
		if (str[i]=='\n') {
			*len = i+1;
			while (i>0 && str[i-1]=='\r') i--;
			char *line = new char[i+1];
			memcpy(line, str, i);
			line[i]='\0';
			return line;
		}
	}
	return NULL;
}

HANDLE Utils::createContact(const char *id,const char *nick, BOOL temporary) {
	HANDLE hContact;
	if (id==NULL || id[0]=='\0') return NULL;
	hContact = getContactFromId(id);
	if (hContact == NULL) {
		hContact = (HANDLE) CallService(MS_DB_CONTACT_ADD, 0, 0);
		CallService(MS_PROTO_ADDTOCONTACT, (WPARAM) hContact, (LPARAM) rvpProtoName);
		DBWriteContactSettingString(hContact, rvpProtoName, "mseid", id);
		if (nick!=NULL && nick[0]!='\0')
			DBWriteContactSettingString(hContact, rvpProtoName, "Nick", nick);
		if (temporary)
			DBWriteContactSettingByte(hContact, "CList", "NotOnList", 1);
	}
	return hContact;
}

char *Utils::getServerFromEmail(const char *email) {
	char *ptr = strchr(email, '@');
	if (ptr == NULL) {
		return NULL;
	}
	return dupString(ptr+1);
}

char *Utils::getUserFromEmail(const char *email) {
	char *ptr = strchr(email, '@');
	if (ptr == NULL) {
		return dupString(email);
	}
	int len = ptr - email;
	char *user = new char[len + 1];
	memcpy(user, email, len);
	user[len] = '\0';
	return user;
}

HANDLE Utils::getContactFromId(const char *id) {
	HANDLE hContact;
	DBVARIANT dbv;
	char *szProto;
	char *p;
	if (id == NULL) return (HANDLE) NULL;
	//char *server = getServerFromEmail(id);
	//char *user = getUserFromEmail(id);
	/*if (server != NULL && user !=NULL) */{
		hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
		while (hContact != NULL) {
			szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
			if (szProto!=NULL && !strcmp(rvpProtoName, szProto)) {
				if (!DBGetContactSetting(hContact, rvpProtoName, "mseid", &dbv)) {
					if ((p=dbv.pszVal) != NULL) {
						if (!stricmp(p, id)) {
							//delete server;
							//delete user;
							DBFreeVariant(&dbv);
							return hContact;
						}/* else {
							char *server2 = getServerFromEmail(p);
							char *user2 = getUserFromEmail(p);
							if (server2 != NULL && user2 !=NULL) {
								if (!stricmp(user, user2)) {
									char *s1 = server;
									char *s2 = server2;
									if (strstr(s1, "im.") == s1) s1 += 3;
									if (strstr(s2, "im.") == s2) s2 += 3;
									if (!stricmp(s1, s2)) {
										delete server;
										delete user;
										delete server2;
										delete user2;
										DBFreeVariant(&dbv);
										return hContact;
									}
								}
							}
							if (server2 != NULL) delete server2;
							if (user2 != NULL) delete user2;
						}*/
					}
					DBFreeVariant(&dbv);
				}
			}
			hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
		}
	}
	//if (server != NULL) delete server;
	//if (user != NULL) delete user;
	return NULL;
}

char *Utils::getLogin(HANDLE hContact) {
	char *contactId = NULL;
	DBVARIANT dbv;
	if (!DBGetContactSetting(hContact, rvpProtoName, "mseid", &dbv)) {
		contactId = Utils::dupString(dbv.pszVal);
		DBFreeVariant(&dbv);
	}
	return contactId;
}