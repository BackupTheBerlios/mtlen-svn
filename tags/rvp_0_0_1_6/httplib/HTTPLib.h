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
class HTTPCredentials;
class HTTPHeader;
class HTTPRequest;

#include "../rvp.h"

#ifndef HTTPUTILS_INCLUDED
#define HTTPUTILS_INCLUDED

#define SECURITY_WIN32
#include <security.h>
#ifndef __MINGW32__
	#ifndef FreeCredentialsHandle
	#define FreeCredentialsHandle FreeCredentialHandle
	#endif
#else
	#define SECURITY_ENTRYPOINT_ANSIW "InitSecurityInterfaceW"
	#define SECURITY_ENTRYPOINT_ANSIA "InitSecurityInterfaceW"
	#define SECURITY_ENTRYPOINTW "InitSecurityInterfaceW"
	#define SECURITY_ENTRYPOINTA "InitSecurityInterfaceA"
	#define SECURITY_ENTRYPOINT16 "INITSECURITYINTERFACEA"

	#ifdef SECURITY_WIN32
	#  ifdef UNICODE
	#    define SECURITY_ENTRYPOINT SECURITY_ENTRYPOINTW
	#    define SECURITY_ENTRYPOINT_ANSI SECURITY_ENTRYPOINT_ANSIW
	#  else // UNICODE
	#    define SECURITY_ENTRYPOINT SECURITY_ENTRYPOINTA
	#    define SECURITY_ENTRYPOINT_ANSI SECURITY_ENTRYPOINT_ANSIA
	#  endif // UNICODE
	#else // SECURITY_WIN32
	#  define SECURITY_ENTRYPOINT SECURITY_ENTRYPOINT16
	#  define SECURITY_ENTRYPOINT_ANSI SECURITY_ENTRYPOINT16
	#endif // SECURITY_WIN32
#endif

class HTTPConnection {
private:
	static HANDLE hNetlibUser;
	bool eof;
	int	 bufferLen;
	int	 bufferPos;
	char buffer[1024];
	HANDLE socket;
public:
	static bool init(const char * protoName, const char *moduleName);
	static void release();
	static HANDLE connect(const char *host, int port);
	static HANDLE bind(NETLIBBIND *nlb);
	HTTPConnection();
	HTTPConnection(HANDLE socket);
	HTTPConnection(const char *host, int port);
	~HTTPConnection();
	int recv(char *data, long datalen);
	int send(const char *data, long datalen);
};

class HTTPCredentials {
private:
	char *domain;
	char *username;
	char *password;
	int	 flags;
public:
	enum FLAGS {
		FLAG_MANUAL_NTLM	  = 1,
		FLAG_PREFER_DIGEST    = 2,
	};
	HTTPCredentials(const char *domain, const char *username, const char *password, int flags);
	HTTPCredentials();
	~HTTPCredentials();
	const char *getDomain();
	void		setDomain(const char *);
	const char *getUsername();
	void		setUsername(const char *);
	const char *getPassword();
	void		setPassword(const char *);
	void		setFlags(int);
	int			getFlags();
};

class HTTPHeader {
public:
	char *name;
	char *value;
	HTTPHeader *next;
	HTTPHeader(const char *, const char *);
	~HTTPHeader();
	HTTPHeader* getAttributes(int offest);
	HTTPHeader* get(const char *);
	const char *getValue();
	const char *getName();

};

class HTTPRequest {
protected:
	char *pData;
	char *method;
	char *url, *host, *uri;
	HTTPHeader *headers;
	int headersCount;
	WORD wPort;
	HTTPCredentials *credentials;

	PSecPkgInfo ntlmSecurityPackageInfo;
	CtxtHandle hNtlmClientContext;
	CredHandle hNtlmClientCredential;
	char *NtlmInitialiseAndGetDomainPacket(HINSTANCE hInstSecurityDll);
	char *NtlmCreateResponseFromChallenge(char *szChallenge);
	void NtlmDestroy();
public:
	DWORD flags;
	int dataLength;
	int resultCode;
	int	authRejects, authRejectsMax;

	bool keepAlive;
	HTTPRequest();
	~HTTPRequest();

	void			setMethod(const char *t);
	const char *	getMethod();
	void			setUrl(const char *url);
	const char *	getUrl();
	int				getPort();
	const char *	getUri();
	const char *	getHost();
	void			setContent(const char *data, int len);
	void			setContent(const char *data);
	const char *	getContent();
	void			setCredentials(HTTPCredentials *);
	HTTPCredentials * getCredentials();

	char *			toStringReq();
	char *			toStringRsp();

	void			addAutoRequestHeaders();
	void			addAutoResponseHeaders();
	bool			authNTLM(HTTPHeader *header);
	bool			authBasic(HTTPHeader *header);
	bool			authDigest(HTTPHeader *header);
	void			addHeader(const char *name, const char *value);
	void			removeHeader(const char *);
	HTTPHeader *	getHeaders();
	HTTPHeader *	getHeader(const char *name);
};


class HTTPUtils {
protected:
	static char *			readLine(HTTPConnection *con);
	static HTTPRequest *	performRequest(HTTPConnection *con, HTTPRequest *request);
public:
	static HTTPRequest *	toRequest(const char *text);
	static HTTPRequest *	recvHeaders(HTTPConnection *con);
	static HTTPRequest *	recvRequest(HTTPConnection *con);
	static int				sendResponse(HTTPConnection *con, HTTPRequest *response);
	static HTTPRequest *	performTransaction(HTTPRequest *request);
};


#endif
