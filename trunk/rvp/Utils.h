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
class Utils;

#ifndef UTILS_INCLUDED
#define UTILS_INCLUDED

#include <stdio.h>
#include <windows.h>

class Utils {
public:
	static void appendText(char **str, int *sizeAlloced, const char *fmt, ...);
	static void appendText(wchar_t **str, int *sizeAlloced, const wchar_t *fmt, ...);
	static void convertPath(char *path);
	static char *dupString(const char *a);
	static char *dupString(const char *a, int l);
	static wchar_t *dupString(const wchar_t *a);
	static wchar_t *dupString(const wchar_t *a, int l);
	static wchar_t *convertToWCS(const char *a);
	static char *convertToString(const wchar_t *a);
	static DWORD safe_wcslen(wchar_t *msg, DWORD maxLen);
	static void ltrim(char *t, char *p);
	static void rtrim(char *t, char *p);
	static void trim(char *t, char *p);
	static char *base64Encode(const char *buffer, int bufferLen);
	static char *base64Decode(const char *str, int *resultLen);
	static char *utf8Encode(const char *str);
	static char *utf8Encode2(const char *str);
	static char *utf8Decode(const char *str);
	static char *utf8Decode2(const char *str, int len);
	static char *utf8Encode(const wchar_t *str);
	static char *cdataEncode(const char *str);
	static wchar_t *utf8DecodeW(const char *str);
	static char *getLine(const char *str, int* len);
	static HANDLE createContact(const char *id, const char *nick, BOOL temporary);
	static HANDLE contactFromID(const char *id);
	static const char *getErrorStr(int errorCode);
	static char *getServerFromEmail(const char *email);
	static char *getUserFromEmail(const char *email);
	static unsigned long forkThread(void (__cdecl *threadcode)(void*), unsigned long stacksize,   void *arg);
};

#endif // UTILS_INCLUDED

