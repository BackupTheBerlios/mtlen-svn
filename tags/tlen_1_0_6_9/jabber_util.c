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
#include "jabber_ssl.h"
#include "jabber_list.h"
#include "sha1.h"
#include <ctype.h>

extern CRITICAL_SECTION mutex;
extern UINT jabberCodePage;

static CRITICAL_SECTION serialMutex;
static unsigned int serial;

void JabberSerialInit(void)
{
	InitializeCriticalSection(&serialMutex);
	serial = 0;
}

void JabberSerialUninit(void)
{
	DeleteCriticalSection(&serialMutex);
}

unsigned int JabberSerialNext(void)
{
	unsigned int ret;

	EnterCriticalSection(&serialMutex);
	ret = serial;
	serial++;
	LeaveCriticalSection(&serialMutex);
	return ret;
}

void JabberLog(const char *fmt, ...)
{
#ifdef ENABLE_LOGGING
	char *str;
	va_list vararg;
	int strsize;
	char *text;
	char *p, *q;
	int extra;

	va_start(vararg, fmt);
	str = (char *) malloc(strsize=2048);
	while (_vsnprintf(str, strsize, fmt, vararg) == -1)
		str = (char *) realloc(str, strsize+=2048);
	va_end(vararg);

	extra = 0;
	for (p=str; *p!='\0'; p++)
		if (*p=='\n' || *p=='\r')
			extra++;
	text = (char *) malloc(strlen(jabberProtoName)+2+strlen(str)+2+extra);
	wsprintf(text, "[%s]", jabberProtoName);
	for (p=str,q=text+strlen(text); *p!='\0'; p++,q++) {
		if (*p == '\r') {
			*q = '\\';
			*(q+1) = 'r';
			q++;
		}
		else if (*p == '\n') {
			*q = '\\';
			*(q+1) = 'n';
			q++;
		}
		else
			*q = *p;
	}
	*q = '\n';
	*(q+1) = '\0';
	if (hNetlibUser!=NULL) {
		CallService(MS_NETLIB_LOG, (WPARAM) hNetlibUser, (LPARAM) text);
	}
	//OutputDebugString(text);
	free(text);
	free(str);
#endif
}

// Caution: DO NOT use JabberSend() to send binary (non-string) data
int JabberSend(HANDLE hConn, const char *fmt, ...)
{
	char *str;
	int size;
	va_list vararg;
	int result;
#ifndef TLEN_PLUGIN
	PVOID ssl;
	char *szLogBuffer;
#endif

	EnterCriticalSection(&mutex);

	va_start(vararg,fmt);
	size = 512;
	str = (char *) malloc(size);
	while (_vsnprintf(str, size, fmt, vararg) == -1) {
		size += 512;
		str = (char *) realloc(str, size);
	}
	va_end(vararg);

	JabberLog("SEND:%s", str);
	size = strlen(str);
#ifndef TLEN_PLUGIN
	if ((ssl=JabberSslHandleToSsl(hConn)) != NULL) {
		if (DBGetContactSettingByte(NULL, "Netlib", "DumpSent", TRUE) == TRUE) {
			if ((szLogBuffer=(char *)malloc(size+32)) != NULL) {
				strcpy(szLogBuffer, "(SSL) Data sent\n");
				memcpy(szLogBuffer+strlen(szLogBuffer), str, size+1 /* also copy \0 */);
				Netlib_Logf(hNetlibUser, "%s", szLogBuffer);	// %s to protect against when fmt tokens are in szLogBuffer causing crash
				free(szLogBuffer);
			}
		}
		result = pfn_SSL_write(ssl, str, size);
	}
	else
		result = JabberWsSend(hConn, str, size);
#else
	result = JabberWsSend(hConn, str, size);
#endif
	LeaveCriticalSection(&mutex);

	free(str);
	return result;
}


char *JabberResourceFromJID(const char *jid)
{
	char *p;
	char *nick;

	p=strchr(jid, '/');
	if (p != NULL && p[1]!='\0') {
		p++;
		if ((nick=(char *) malloc(1+strlen(jid)-(p-jid))) != NULL) {
			strncpy(nick, p, strlen(jid)-(p-jid));
			nick[strlen(jid)-(p-jid)] = '\0';
		}
	}
	else {
		nick = _strdup(jid);
	}

	return nick;
}

char *JabberNickFromJID(const char *jid)
{
	char *p;
	char *nick;

	if ((p=strchr(jid, '@')) == NULL)
		p = strchr(jid, '/');
	if (p != NULL) {
		if ((nick=(char *) malloc((p-jid)+1)) != NULL) {
			strncpy(nick, jid, p-jid);
			nick[p-jid] = '\0';
		}
	}
	else {
		nick = _strdup(jid);
	}

	return nick;
}

char *JabberLoginFromJID(const char *jid)
{
	char *p;
	char *nick;

	p = strchr(jid, '/');
	if (p != NULL) {
		if ((nick=(char *) malloc((p-jid)+1)) != NULL) {
			strncpy(nick, jid, p-jid);
			nick[p-jid] = '\0';
		}
	}
	else {
		nick = _strdup(jid);
	}
	return nick;
}

char *JabberLocalNickFromJID(const char *jid)
{
	char *p;
	char *localNick;

	p = JabberNickFromJID(jid);
	localNick = JabberTextDecode(p);
	free(p);
	return localNick;
}

char *JabberUtf8Decode(const char *str)
{
	int i, len;
	char *p;
	WCHAR *wszTemp;
	char *szOut;

	if (str == NULL) return NULL;

	len = strlen(str);

	// Convert utf8 to unicode
	if ((wszTemp=(WCHAR *) malloc(sizeof(WCHAR) * (len + 1))) == NULL)
		return NULL;
	p = (char *) str;
	i = 0;
	while (*p) {
		if ((*p & 0x80) == 0)
			wszTemp[i++] = *(p++);
		else if ((*p & 0xe0) == 0xe0) {
			wszTemp[i] = (*(p++) & 0x1f) << 12;
			wszTemp[i] |= (*(p++) & 0x3f) << 6;
			wszTemp[i++] |= (*(p++) & 0x3f);
		}
		else {
			wszTemp[i] = (*(p++) & 0x3f) << 6;
			wszTemp[i++] |= (*(p++) & 0x3f);
		}
	}
	wszTemp[i] = '\0';

	// Convert unicode to local codepage
	if ((len=WideCharToMultiByte(jabberCodePage, 0, wszTemp, -1, NULL, 0, NULL, NULL)) == 0)
		return NULL;
	if ((szOut=(char *) malloc(len)) == NULL)
		return NULL;
	WideCharToMultiByte(jabberCodePage, 0, wszTemp, -1, szOut, len, NULL, NULL);
	free(wszTemp);

	return szOut;
}

char *JabberUtf8Encode(const char *str)
{
	unsigned char *szOut;
	int len, i;
	WCHAR *wszTemp, *w;

	if (str == NULL) return NULL;

	len = strlen(str);

	// Convert local codepage to unicode
	if ((wszTemp=(WCHAR *) malloc(sizeof(WCHAR) * (len + 1))) == NULL) return NULL;
	MultiByteToWideChar(jabberCodePage, 0, str, -1, wszTemp, len + 1);

	// Convert unicode to utf8
	len = 0;
	for (w=wszTemp; *w; w++) {
		if (*w < 0x0080) len++;
		else if (*w < 0x0800) len += 2;
		else len += 3;
	}

	if ((szOut=(unsigned char *) malloc(len + 1)) == NULL)
		return NULL;

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

	free(wszTemp);

	return (char *) szOut;

}

char *JabberSha1(char *str)
{
	SHA1Context sha;
	uint8_t digest[20];
	char *result;
	int i;

	if (str==NULL)
		return NULL;
	if (SHA1Reset(&sha))
		return NULL;
	if (SHA1Input(&sha, str, strlen(str)))
		return NULL;
	if (SHA1Result(&sha, digest))
		return NULL;
	if ((result=(char *)malloc(41)) == NULL)
		return NULL;
	for (i=0; i<20; i++)
		sprintf(result+(i<<1), "%02x", digest[i]);
	return result;
}

char *TlenSha1(char *str, int len)
{
	SHA1Context sha;
	uint8_t digest[20];
	char *result;
	int i;

	if (str==NULL)
		return NULL;
	if (SHA1Reset(&sha))
		return NULL;
	if (SHA1Input(&sha, str, len))
		return NULL;
	if (SHA1Result(&sha, digest))
		return NULL;
	if ((result=(char *)malloc(20)) == NULL)
		return NULL;
	for (i=0; i<20; i++)
		result[i]=digest[4*(i>>2)+(3-(i&0x3))];
	return result;
}

char *JabberUnixToDos(const char *str)
{
	char *p, *q, *res;
	int extra;

	if (str==NULL || str[0]=='\0')
		return NULL;

	extra = 0;
	for (p=(char *) str; *p!='\0'; p++) {
		if (*p == '\n')
			extra++;
	}
	if ((res=(char *)malloc(strlen(str)+extra+1)) != NULL) {
		for (p=(char *) str,q=res; *p!='\0'; p++,q++) {
			if (*p == '\n') {
				*q = '\r';
				q++;
			}
			*q = *p;
		}
		*q = '\0';
	}
	return res;
}

void JabberDosToUnix(char *str)
{
	char *p, *q;

	if (str==NULL || str[0]=='\0')
		return;

	for (p=q=str; *p!='\0'; p++) {
		if (*p != '\r') {
			*q = *p;
			q++;
		}
	}
	*q = '\0';
}

char *JabberHttpUrlEncode(const char *str)
{
	unsigned char *p, *q, *res;

	if (str == NULL) return NULL;
	res = (char *) malloc(3*strlen(str) + 1);
	for (p=(char *)str,q=res; *p!='\0'; p++,q++) {
		if ((*p>='A' && *p<='Z') || (*p>='a' && *p<='z') || (*p>='0' && *p<='9') || strchr("$-_.+!*(),", *p)!=NULL) {
			*q = *p;
		}
		else {
			sprintf(q, "%%%02X", *p);
			q += 2;
		}
	}
	*q = '\0';
	return res;
}

void JabberHttpUrlDecode(char *str)
{
	unsigned char *p, *q;
	unsigned int code;

	if (str == NULL) return;
	for (p=q=str; *p!='\0'; p++,q++) {
		if (*p=='%' && *(p+1)!='\0' && isxdigit(*(p+1)) && *(p+2)!='\0' && isxdigit(*(p+2))) {
			sscanf(p+1, "%2x", &code);
			*q = (unsigned char) code;
			p += 2;
		}
		else {
			*q = *p;
		}
	}
	*q = '\0';
}

struct tagErrorCodeToStr {
	int code;
	char *str;
} JabberErrorCodeToStrMapping[] = {
	{JABBER_ERROR_REDIRECT,					"Redirect"},
	{JABBER_ERROR_BAD_REQUEST,				"Bad request"},
	{JABBER_ERROR_UNAUTHORIZED,				"Unauthorized"},
	{JABBER_ERROR_PAYMENT_REQUIRED,			"Payment required"},
	{JABBER_ERROR_FORBIDDEN,				"Forbidden"},
	{JABBER_ERROR_NOT_FOUND,				"Not found"},
	{JABBER_ERROR_NOT_ALLOWED,				"Not allowed"},
	{JABBER_ERROR_NOT_ACCEPTABLE,			"Not acceptable"},
	{JABBER_ERROR_REGISTRATION_REQUIRED,	"Registration required"},
	{JABBER_ERROR_REQUEST_TIMEOUT,			"Request timeout"},
	{JABBER_ERROR_CONFLICT,					"Conflict"},
	{JABBER_ERROR_INTERNAL_SERVER_ERROR,	"Internal server error"},
	{JABBER_ERROR_NOT_IMPLEMENTED,			"Not implemented"},
	{JABBER_ERROR_REMOTE_SERVER_ERROR,		"Remote server error"},
	{JABBER_ERROR_SERVICE_UNAVAILABLE,		"Service unavailable"},
	{JABBER_ERROR_REMOTE_SERVER_TIMEOUT,	"Remote server timeout"},
	{-1, "Unknown error"}
};

char *JabberErrorStr(int errorCode)
{
	int i;

	for (i=0; JabberErrorCodeToStrMapping[i].code!=-1 && JabberErrorCodeToStrMapping[i].code!=errorCode; i++);
	return JabberErrorCodeToStrMapping[i].str;
}

char *JabberErrorMsg(XmlNode *errorNode)
{
	char *errorStr, *str;
	int errorCode;

	errorStr = (char *) malloc(256);
	if (errorNode == NULL) {
		_snprintf(errorStr, 256, "%s -1: %s", Translate("Error"), Translate("Unknown error message"));
		return errorStr;
	}

	errorCode = -1;
	if ((str=JabberXmlGetAttrValue(errorNode, "code")) != NULL)
		errorCode = atoi(str);
	if ((str=errorNode->text) != NULL)
		_snprintf(errorStr, 256, "%s %d: %s\r\n%s", Translate("Error"), errorCode, Translate(JabberErrorStr(errorCode)), str);
	else
		_snprintf(errorStr, 256, "%s %d: %s", Translate("Error"), errorCode, Translate(JabberErrorStr(errorCode)));
	return errorStr;
}

char *TlenPasswordHash(const char *str)
{
	int magic1 = 0x50305735, magic2 = 0x12345671, sum = 7;
	char *p, *res;

	if (str == NULL) return NULL;
	for (p=(char *)str; *p!='\0'; p++) {
		if (*p!=' ' && *p!='\t') {
			magic1 ^= (((magic1 & 0x3f) + sum) * ((char) *p)) + (magic1 << 8);
			magic2 += (magic2 << 8) ^ magic1;
			sum += ((char) *p);
		}
	}
	magic1 &= 0x7fffffff;
	magic2 &= 0x7fffffff;
	res = (char *) malloc(17);
	sprintf(res, "%08x%08x", magic1, magic2);
	return res;
}

char *TlenUrlEncode(const char *str)
{
	char *p, *q, *res;
	unsigned char c;

	if (str == NULL) return NULL;
	res = (char *) malloc(3*strlen(str) + 1);
	for (p=(char *)str,q=res; *p!='\0'; p++,q++) {
		if (*p == ' ') {
			*q = '+';
		}
		else if (*p<0x20 || *p>=0x7f || strchr("%&+:'<>\"", *p)!=NULL) {
			// Convert first from CP1252 to ISO8859-2
			switch ((unsigned char) *p) {
			case 0xa5: c = (unsigned char) 0xa1; break;
			case 0x8c: c = (unsigned char) 0xa6; break;
			case 0x8f: c = (unsigned char) 0xac; break;
			case 0xb9: c = (unsigned char) 0xb1; break;
			case 0x9c: c = (unsigned char) 0xb6; break;
			case 0x9f: c = (unsigned char) 0xbc; break;
			default: c = (unsigned char) *p; break;
			}
			sprintf(q, "%%%02X", c);
			q += 2;
		}
		else {
			*q = *p;
		}
	}
	*q = '\0';
	return res;
}

void TlenUrlDecode(char *str)
{
	char *p, *q;
	unsigned int code;

	if (str == NULL) return;
	for (p=q=str; *p!='\0'; p++,q++) {
		if (*p == '+') {
			*q = ' ';
		}
		else if (*p=='%' && *(p+1)!='\0' && isxdigit(*(p+1)) && *(p+2)!='\0' && isxdigit(*(p+2))) {
			sscanf(p+1, "%2x", &code);
			*q = (char) code;
			// Convert from ISO8859-2 to CP1252
			switch ((unsigned char) *q) {
			case 0xa1: *q = (char) 0xa5; break;
			case 0xa6: *q = (char) 0x8c; break;
			case 0xac: *q = (char) 0x8f; break;
			case 0xb1: *q = (char) 0xb9; break;
			case 0xb6: *q = (char) 0x9c; break;
			case 0xbc: *q = (char) 0x9f; break;
			}
			p += 2;
		}
		else {
			*q = *p;
		}
	}
	*q = '\0';
}

char * TlenGroupDecode(const char *str)
{
	char *p, *q;
	if (str == NULL) return NULL;
	p = q = JabberTextDecode(str);
	for (; *p!='\0'; p++) {
		if (*p == '/') {
			*p = '\\';
		}
	}
	return q;
}

char * TlenGroupEncode(const char *str)
{
	char *p, *q;
	if (str == NULL) return NULL;
	p = q = strdup(str);
	for (; *p!='\0'; p++) {
		if (*p == '\\') {
			*p = '/';
		}
	}
	p = JabberTextEncode(q);
	free (q);
	return p;
}

char *JabberTextEncode(const char *str)
{
	char *s1;

	if (str == NULL) return NULL;
	if ((s1=TlenUrlEncode(str)) == NULL)
		return NULL;
	return s1;
}

char *JabberTextDecode(const char *str)
{
	char *s1;

	if (str == NULL) return NULL;
	s1 = strdup(str);
	TlenUrlDecode(s1);
	return s1;
}

static char b64table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *JabberBase64Encode(const char *buffer, int bufferLen)
{
	int n;
	unsigned char igroup[3];
	char *p, *peob;
	char *res, *r;

	if (buffer==NULL || bufferLen<=0) return NULL;
	if ((res=(char *) malloc((((bufferLen+2)/3)*4) + 1)) == NULL) return NULL;

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

static unsigned char b64rtable[256];

char *JabberBase64Decode(const char *str, int *resultLen)
{
	char *res;
	unsigned char *p, *r, igroup[4], a[4];
	int n, num, count;

	if (str==NULL || resultLen==NULL) return NULL;
	if ((res=(char *) malloc(((strlen(str)+3)/4)*3)) == NULL) return NULL;

	for (n=0; n<256; n++)
		b64rtable[n] = (unsigned char) 0x80;
	for (n=0; n<26; n++)
		b64rtable['A'+n] = n;
	for (n=0; n<26; n++)
		b64rtable['a'+n] = n + 26;
	for (n=0; n<10; n++)
		b64rtable['0'+n] = n + 52;
	b64rtable['+'] = 62;
	b64rtable['/'] = 63;
	b64rtable['='] = 0;
	count = 0;
	for (p=(unsigned char *)str,r=(unsigned char *)res; *p!='\0';) {
		for (n=0; n<4; n++) {
			if (*p=='\0' || b64rtable[*p]==0x80) {
				free(res);
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
/*
char *JabberGetVersionText()
{
	char filename[MAX_PATH], *fileVersion, *res;
	DWORD unused;
	DWORD verInfoSize, blockSize;
	PVOID pVerInfo;

	GetModuleFileName(hInst, filename, sizeof(filename));
	verInfoSize = GetFileVersionInfoSize(filename, &unused);
	if ((pVerInfo=malloc(verInfoSize)) != NULL) {
		GetFileVersionInfo(filename, 0, verInfoSize, pVerInfo);
		VerQueryValue(pVerInfo, "\\StringFileInfo\\041504e3\\FileVersion", (PVOID *)&fileVersion, (UINT *)&blockSize);
		if (strstr(fileVersion, "cvs")) {
			res = (char *) malloc(strlen(fileVersion) + strlen(__DATE__) + 2);
			sprintf(res, "%s %s", fileVersion, __DATE__);
		}
		else {
			res = _strdup(fileVersion);
		}
		free(pVerInfo);
		return res;
	}
	return NULL;
}
*/
/*
 *	Apply Polish Daylight Saving Time rules to get "DST-unbiased" timestamp
 */

time_t  TlenTimeToUTC(time_t time) {
	struct tm *timestamp;
	timestamp = gmtime(&time);
	if ( (timestamp->tm_mon > 2 && timestamp->tm_mon < 9) ||
		 (timestamp->tm_mon == 2 && timestamp->tm_mday - timestamp->tm_wday >= 25) ||
		 (timestamp->tm_mon == 9 && timestamp->tm_mday - timestamp->tm_wday < 25)) {
	} else {
		time += 3600;
	}
	return time;
}

time_t JabberIsoToUnixTime(char *stamp)
{
	struct tm timestamp;
	char date[9];
	char *p;
	int i, y;
	time_t t;

	if (stamp == NULL) return (time_t) 0;

	p = stamp;

	// Get the date part
	for (i=0; *p!='\0' && i<8 && isdigit(*p); p++,i++)
		date[i] = *p;

	// Parse year
	if (i == 6) {
		// 2-digit year (1970-2069)
		y = (date[0]-'0')*10 + (date[1]-'0');
		if (y < 70) y += 100;
	}
	else if (i == 8) {
		// 4-digit year
		y = (date[0]-'0')*1000 + (date[1]-'0')*100 + (date[2]-'0')*10 + date[3]-'0';
		y -= 1900;
	}
	else
		return (time_t) 0;
	timestamp.tm_year = y;
	// Parse month
	timestamp.tm_mon = (date[i-4]-'0')*10 + date[i-3]-'0' - 1;
	// Parse date
	timestamp.tm_mday = (date[i-2]-'0')*10 + date[i-1]-'0';

	// Skip any date/time delimiter
	for (; *p!='\0' && !isdigit(*p); p++);

	// Parse time
	if (sscanf(p, "%d:%d:%d", &(timestamp.tm_hour), &(timestamp.tm_min), &(timestamp.tm_sec)) != 3)
		return (time_t) 0;

	timestamp.tm_isdst = 0;	// DST is already present in _timezone below
	_tzset();
	t = mktime(&timestamp);
	t -= _timezone;
	t = TlenTimeToUTC(t);
	JabberLog("%s is %s (%d)", stamp, ctime(&t), _timezone);

	if (t >= 0)
		return t;
	else
		return (time_t) 0;
}

void JabberSendVisibleInvisiblePresence(BOOL invisible)
{
	JABBER_LIST_ITEM *item;
	HANDLE hContact;
	WORD apparentMode;
	int i;

	if (!jabberOnline) return;

	i = 0;
	while ((i=JabberListFindNext(LIST_ROSTER, i)) >= 0) {
		if ((item=JabberListGetItemPtrFromIndex(i)) != NULL) {
			if ((hContact=JabberHContactFromJID(item->jid)) != NULL) {
				apparentMode = DBGetContactSettingWord(hContact, jabberProtoName, "ApparentMode", 0);
				if (invisible==TRUE && apparentMode==ID_STATUS_OFFLINE) {
					JabberSend(jabberThreadInfo->s, "<presence to='%s' type='invisible'/>", item->jid);
				}
				else if (invisible==FALSE && apparentMode==ID_STATUS_ONLINE) {
					EnterCriticalSection(&modeMsgMutex);
					if (modeMsgs.szInvisible)
						JabberSend(jabberThreadInfo->s, "<presence to='%s'><show>available</show><status>%s</status></presence>", item->jid, modeMsgs.szInvisible);
					else
						JabberSend(jabberThreadInfo->s, "<presence to='%s'><show>available</show></presence>", item->jid);
					LeaveCriticalSection(&modeMsgMutex);
				}
			}
		}
		i++;
	}
}

void JabberSendPresenceTo(int status, char *to, char *extra)
{
	char *showBody, *statusMsg, *presenceType;
	char *ptr = NULL;
	char priorityStr[32];
	char toStr[512];

	if (!jabberOnline) return;

	// Send <presence/> update for status (we won't handle ID_STATUS_OFFLINE here)
	// Note: jabberModeMsg is already encoded using JabberTextEncode()
	EnterCriticalSection(&modeMsgMutex);

	priorityStr[0] = '\0';

	if (to != NULL)
		_snprintf(toStr, sizeof(toStr), " to='%s'", to);
	else
		toStr[0] = '\0';

	showBody = NULL;
	statusMsg = NULL;
	presenceType = NULL;
	switch (status) {
	case ID_STATUS_ONLINE:
		showBody = "available";
		statusMsg = modeMsgs.szOnline;
		break;
	case ID_STATUS_AWAY:
	case ID_STATUS_ONTHEPHONE:
	case ID_STATUS_OUTTOLUNCH:
		showBody = "away";
		statusMsg = modeMsgs.szAway;
		break;
	case ID_STATUS_NA:
		showBody = "xa";
		statusMsg = modeMsgs.szNa;
		break;
	case ID_STATUS_DND:
	case ID_STATUS_OCCUPIED:
		showBody = "dnd";
		statusMsg = modeMsgs.szDnd;
		break;
	case ID_STATUS_FREECHAT:
		showBody = "chat";
		statusMsg = modeMsgs.szFreechat;
		break;
	case ID_STATUS_INVISIBLE:
		presenceType = "invisible";
		statusMsg = modeMsgs.szInvisible;
		break;
	case ID_STATUS_OFFLINE:
		presenceType = "unavailable";
		if (DBGetContactSettingByte(NULL, jabberProtoName, "LeaveOfflineMessage", FALSE)) {
			int offlineMessageOption = DBGetContactSettingWord(NULL, jabberProtoName, "OfflineMessageOption", 0);
			if (offlineMessageOption == 0) {
				switch (jabberStatus) {
					case ID_STATUS_ONLINE:
						ptr = _strdup(modeMsgs.szOnline);
						break;
					case ID_STATUS_AWAY:
					case ID_STATUS_ONTHEPHONE:
					case ID_STATUS_OUTTOLUNCH:
						ptr = _strdup(modeMsgs.szAway);
						break;
					case ID_STATUS_NA:
						ptr = _strdup(modeMsgs.szNa);
						break;
					case ID_STATUS_DND:
					case ID_STATUS_OCCUPIED:
						ptr = _strdup(modeMsgs.szDnd);
						break;
					case ID_STATUS_FREECHAT:
						ptr = _strdup(modeMsgs.szFreechat);
						break;
					case ID_STATUS_INVISIBLE:
						ptr = _strdup(modeMsgs.szInvisible);
						break;
				}
			} else if (offlineMessageOption == 99) {
				
			} else if (offlineMessageOption < 7) {
				DBVARIANT dbv;
				const char *statusNames[] = {"OnDefault", "AwayDefault", "NaDefault", "DndDefault", "FreeChatDefault", "InvDefault"};
				if (!DBGetContactSetting(NULL, "SRAway", statusNames[offlineMessageOption-1], &dbv)) {
					int i;
					char substituteStr[128];
					ptr = _strdup(dbv.pszVal);
					DBFreeVariant(&dbv);
					for(i=0;ptr[i];i++) {
						if(ptr[i]!='%') continue;
						if(!_strnicmp(ptr+i,"%time%",6))
							GetTimeFormat(LOCALE_USER_DEFAULT,TIME_NOSECONDS,NULL,NULL,substituteStr,sizeof(substituteStr));
						else if(!_strnicmp(ptr+i,"%date%",6))
							GetDateFormat(LOCALE_USER_DEFAULT,DATE_SHORTDATE,NULL,NULL,substituteStr,sizeof(substituteStr));
						else continue;
						if(lstrlen(substituteStr)>6) ptr=(char*)realloc(ptr,lstrlen(ptr)+1+lstrlen(substituteStr)-6);
						MoveMemory(ptr+i+lstrlen(substituteStr),ptr+i+6,lstrlen(ptr)-i-5);
						CopyMemory(ptr+i,substituteStr,lstrlen(substituteStr));
					}
				}
			}
		}
		statusMsg = ptr;
		break;
	default:
		// Should not reach here
		break;
	}
	if (presenceType) {
		extra = NULL;
		if (statusMsg)
			JabberSend(jabberThreadInfo->s, "<presence%s type='%s'><status>%s</status>%s%s</presence>", toStr, presenceType, statusMsg, priorityStr, (extra!=NULL)?extra:"");
		else
			JabberSend(jabberThreadInfo->s, "<presence%s type='%s'>%s%s</presence>", toStr, presenceType, priorityStr, (extra!=NULL)?extra:"");
	} else {
		if (statusMsg)
			JabberSend(jabberThreadInfo->s, "<presence%s><show>%s</show><status>%s</status>%s%s</presence>", toStr, showBody, statusMsg, priorityStr, (extra!=NULL)?extra:"");
		else
			JabberSend(jabberThreadInfo->s, "<presence%s><show>%s</show>%s%s</presence>", toStr, showBody, priorityStr, (extra!=NULL)?extra:"");
	}
	if (ptr) {
		free(ptr);
	}
	LeaveCriticalSection(&modeMsgMutex);
}

void JabberSendPresence(int status)
{
	JabberSendPresenceTo(status, NULL, "<x/>");

//	if (DBGetContactSettingByte(NULL, jabberProtoName, "VisibilitySupport", FALSE)) {
	if (DBGetContactSettingByte(NULL, jabberProtoName, "VisibilitySupport", FALSE)) {
		if (status == ID_STATUS_INVISIBLE)
			JabberSendVisiblePresence();
		else
			JabberSendInvisiblePresence();
	}
}

void JabberStringAppend(char **str, int *sizeAlloced, const char *fmt, ...)
{
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

static char clientJID[3072];

char *JabberGetClientJID(char *jid)
{
	char *p;

	if (jid == NULL) return NULL;
	strncpy(clientJID, jid, sizeof(clientJID));
	clientJID[sizeof(clientJID)-1] = '\0';
	if ((p=strchr(clientJID, '/')) == NULL) {
		p = clientJID + strlen(clientJID);
	}
	return clientJID;
}

