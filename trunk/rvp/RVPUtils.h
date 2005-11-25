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
class RVPSubscription;
class RVPSession;
class RVPClient;

#ifndef RVPUTILS_INCLUDED
#define RVPUTILS_INCLUDED

#include <windows.h>
#include "httplib/HTTPLib.h"
#include "List.h"

class RVPDNSMapping:public ListItem {
private:
	static List list;
	char *srcHost;
	char *destHost;
	int	  port;
	RVPDNSMapping(const char *srcHost, const char *destHost, int port);
public:
	~RVPDNSMapping();
	const char * getSrcHost();
	const char * getDestHost();
	int		getPort();
	void	setDestHost(const char *);
	void	setPort(int);
	static	RVPDNSMapping* add(const char *srcHost, const char *destHost, int port);
	static 	RVPDNSMapping* find(const char *host);
	static  void releaseAll();
};

class RVPSubscription:public ListItem {
private:
	static List list;
	time_t expiry;
	int	 status;
	char *sid;
	char *displayName;
	char *email;
	RVPClient *client;
	RVPSubscription(RVPClient *client, const char *id);
public:
	~RVPSubscription();
	int	getStatus();
	const char * getSid();
	const char * getDisplayName();
	const char * getEmail();
	void	setStatus(int);
	void	setSid(const char *);
	void	setDisplayName(const char *);
	void	setEmail(const char *);
	void	setExpiry(time_t time);
	time_t	getExpiry();
	RVPClient *getClient();
	static RVPSubscription *add(RVPClient *client, const char *login, const char *sid);
	static RVPSubscription* find(const char *login);
	static List* getSubscriptions();
};

class RVPSession:public ListItem  {
private:
	static List list;
	char *sid;
	RVPSession(const char *id);
public:
	~RVPSession();
	void	setSid(const char *);
	const char * getSid();
	static RVPSession *get(const char *node);
	static void add(const char *node, const char *sid);
	static void releaseAll();
};

class RVPListener {
public:
	virtual void recvTyping(const char *login) = 0;
	virtual void recvMessage(const char *login, const char *message) = 0;
	virtual void recvMessage(const char *login, const wchar_t *message) = 0;
	virtual void recvStatus(const char *login, int status) = 0;
	virtual void recvFileInvite(const char *login, const char *filename, int filesize) = 0;
};

class RVPFile:public ListItem {
private:
	static List list;
	HANDLE hContact;
public:
	RVPFile(HANDLE hContact, const char *id);
	~RVPFile();
	static RVPFile* find(const char *id);
	int	   size;
	HANDLE getContact();
	void   setSize(int size);
	int	   getSize();
};

class RVPContact:public ListItem {
private:
	static List list;
public:
	RVPContact();
	static RVPContact* get(HANDLE);
};

class RVPClient {
private:
	static List dnsList;
	static const char *getStatusString(int status);
	static int 	getRVPStatus(int status);
	CRITICAL_SECTION mutex;
	bool bStarted;
	bool bOnline;
	HTTPCredentials *credentials;
	char *signInName;
	char *server;
	int	 serverPort;
	char *principalUrl;
	HANDLE lSocket;
	DWORD localIP;
	char  callbackHost[256];
	int	 lastStatus;
public:
	RVPClient();
	~RVPClient();
	void	setCredentials(HTTPCredentials *c);
	bool	isOnline();
	int		signIn(const char *principal, const char *manualServer); //synchronized
	int		signOut(); //synchronized
	int		stopListening();
	RVPSubscription*	subscribe(const char *login);
	int 	subscribeMain(const char *callbackHost);
	int		renew(RVPSubscription *subscription);
	int		unsubscribe(RVPSubscription*);
	int		unsubscribe(const char *login);
	//int		unsubscribeAll();
	int 	sendPresence(int status);
	int 	sendPresence();
	int 	sendMessage(const char *message, const char *contactID, const char *contactDisplayname, const char *principalDisplayname);
	int 	sendMessage(const wchar_t *message, const char *contactID, const char *contactDisplayname, const char *principalDisplayname);
	int		sendTyping(const char *contactID, const char *contactDisplayname, const char *principalDisplayname);
	int		sendFileAccept(RVPFile *);
	int		getSubscribers();
	int		getACL();
	const char *getCallback();
	RVPSubscription*	getProperty(const char *login, const char *property);
	List*	getSubscriptions();
	RVPSubscription* getSubscription(const char *login);
	static int	getStatusFromString(const char *statusString);
	static char *getServerFromLogin(const char *signInName, const char *manualServer);
	static char *getUrlFromLogin(const char *signInName);
//	static char *getFullUrlFromLogin(const char *signInName);
	static char *getLoginFromUrl(const char *url);
};

#endif
