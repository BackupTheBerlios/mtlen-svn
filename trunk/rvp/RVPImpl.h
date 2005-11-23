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
class RVPImpl;
class RVPImplAsyncData;

#ifndef RVPIMPL_INCLUDED
#define RVPIMPL_INCLUDED

#include "RVPUtils.h"
#include "ThreadManager.h"
#include "List.h"

class RVPImplAsyncData {
private:
	char *message;
	wchar_t *messageW;
public:
	RVPImplAsyncData(RVPImpl *);
	~RVPImplAsyncData();
	void setMessage(const char *);
	void setMessage(const wchar_t *);
	const char *getMessage();
	const wchar_t *getMessageW();
	int	id;
	RVPImpl *impl;
	RVPSubscription *subscription;
	HANDLE hContact;
	int	status;
	int	oldStatus;
};

class RVPImpl:public ThreadManager {
private:
	RVPClient* client;
	int oldStatus;
	int msgCounter;
	int typingCoutner;
	int renewCounter;
	int searchCounter;
	List *typingNotifications;
public:
	enum THREADGROUPS {
		TGROUP_SIGNIN = 1,
		TGROUP_NORMAL = 2,
		TGROUP_SHUTDOWN = 4,
		TGROUP_SIGNOUT = 8,
	};
	RVPImpl();
	RVPClient *getClient();
	void setCredentials(HTTPCredentials *c);
	int	signIn(const char *signInName, const char *manualServer, int initialStatus);
	int	signOut();
	int signOutThreadCall();
	int	subscribe(HANDLE hContact);
	int	unsubscribe(HANDLE hContact);
	int	subscribeAll();
	int	unsubscribeAll();
	int setStatus(int status);
	int sendStatus();
	int	sendMessage(CCSDATA *ccs);
	int	sendTyping(HANDLE hContact, bool on);
	int	sendFileAccept(RVPFile *file, const char *path);
	int	sendFileDeny(const char *id);
	int	sendFileInvite(HANDLE hContact, const char * filenames[] , int filenum);
	int	searchContact(const char *login);
	bool isTyping(HANDLE hContact);
	RVPSubscription* getProperty(const char *node, const char *property);

};

#endif
