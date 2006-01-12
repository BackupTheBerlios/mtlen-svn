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
#include <sys/types.h>
#include <sys/stat.h>
#include "RVPImpl.h"
#include "rvp.h"
#include "time.h"
#include "math.h"
#include "Utils.h"

static void __cdecl RVPSubscribeAsyncThread(void *ptr) {
	RVPImplAsyncData *data = (RVPImplAsyncData *)ptr;
	char *contactId = Utils::getLogin(data->hContact);
	if (contactId != NULL) {
		RVPSubscription *subscription = data->impl->getClient()->subscribe(contactId);
		if (subscription != NULL) {
			if (DBGetContactSettingWord(data->hContact, rvpProtoName, "Status", ID_STATUS_OFFLINE) != subscription->getStatus()) {
				DBWriteContactSettingWord(data->hContact, rvpProtoName, "Status", (WORD) subscription->getStatus());
			}
			if (subscription->getDisplayName() != NULL) {
				DBWriteContactSettingStringUtf(data->hContact, rvpProtoName, "Nick", subscription->getDisplayName());
				DBWriteContactSettingStringUtf(data->hContact, rvpProtoName, "displayname", subscription->getDisplayName());
			}
			if (subscription->getEmail() != NULL) {
				DBWriteContactSettingStringUtf(data->hContact, rvpProtoName, "e-mail", subscription->getEmail());
			}
		}
		delete contactId;
	}
	delete data;
}

static void __cdecl RVPUnsubscribeAsyncThread(void *ptr) {
	RVPImplAsyncData *data = (RVPImplAsyncData *)ptr;
	data->impl->getClient()->unsubscribe(data->subscription);
	delete data;
}

static void __cdecl RVPSetStatusAsyncThread(void *ptr) {
	RVPImplAsyncData *data = (RVPImplAsyncData *)ptr;
	data->impl->getClient()->sendPresence(data->status);
	ProtoBroadcastAck(rvpProtoName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE) data->oldStatus, data->status);
	delete data;
}

static void __cdecl RVPSendMessageAsyncThread(void *ptr) {
	RVPImplAsyncData *data = (RVPImplAsyncData *)ptr;
	char *contactID;
	char *principalDisplayname;
	char *contactDisplayname;
	contactID = Utils::getLogin(data->hContact);
	if (contactID != NULL) {
		principalDisplayname = Utils::getDisplayName(NULL);
		contactDisplayname = Utils::getDisplayName(data->hContact);
		if (data->getMessageW() != NULL) {
			if (!data->impl->getClient()->sendMessage(data->getMessageW(), contactID, contactDisplayname, principalDisplayname)) {
				ProtoBroadcastAck(rvpProtoName, data->hContact, ACKTYPE_MESSAGE, ACKRESULT_SUCCESS, (HANDLE) data->id, 0);
			} else {
				ProtoBroadcastAck(rvpProtoName, data->hContact, ACKTYPE_MESSAGE, ACKRESULT_FAILED, (HANDLE) data->id, 0);
			}
		} else {
			if (!data->impl->getClient()->sendMessage(data->getMessage(), contactID, contactDisplayname, principalDisplayname)) {
				ProtoBroadcastAck(rvpProtoName, data->hContact, ACKTYPE_MESSAGE, ACKRESULT_SUCCESS, (HANDLE) data->id, 0);
			} else {
				ProtoBroadcastAck(rvpProtoName, data->hContact, ACKTYPE_MESSAGE, ACKRESULT_FAILED, (HANDLE) data->id, 0);
			}
		}
		delete contactID;
		delete contactDisplayname;
		delete principalDisplayname;
	}
	delete data;
}

static void __cdecl RVPSendFileAccept(void *ptr) {
	RVPImplAsyncData *data = (RVPImplAsyncData *)ptr;
	char *contactID;
	char *principalDisplayname;
	char *contactDisplayname;
	contactID = Utils::getLogin(data->hContact);
	if (contactID != NULL) {
		principalDisplayname = Utils::getDisplayName(NULL);
		contactDisplayname = Utils::getDisplayName(data->hContact);
		data->impl->getClient()->sendFileAccept(data->file, contactID, contactDisplayname, principalDisplayname);
		delete contactID;
		delete contactDisplayname;
		delete principalDisplayname;
	}
	delete data;
}

static void __cdecl RVPSendFileReject(void *ptr) {
	RVPImplAsyncData *data = (RVPImplAsyncData *)ptr;
	char *contactID;
	char *principalDisplayname;
	char *contactDisplayname;
	contactID = Utils::getLogin(data->hContact);
	if (contactID != NULL) {
		principalDisplayname = Utils::getDisplayName(NULL);
		contactDisplayname = Utils::getDisplayName(data->hContact);
		data->impl->getClient()->sendFileReject(data->file, contactID, contactDisplayname, principalDisplayname);
		delete contactID;
		delete contactDisplayname;
		delete principalDisplayname;
		delete data->file;
	}
	delete data;
}

static void __cdecl RVPSendTypingAsyncThread(void *ptr) {
	int counter = 0;
	RVPImplAsyncData *data = (RVPImplAsyncData *)ptr;
	char *contactID;
	char *principalDisplayname;
	char *contactDisplayname;
	contactID = Utils::getLogin(data->hContact);
	if (contactID != NULL) {
		principalDisplayname = Utils::getDisplayName(NULL);
		contactDisplayname = Utils::getDisplayName(data->hContact);
		while (data->impl->isGroupAllowed(RVPImpl::TGROUP_NORMAL) && data->impl->isTyping(data->hContact)) {
			if (counter == 0) {
				data->impl->getClient()->sendTyping(contactID, contactDisplayname, principalDisplayname);
			}
			Sleep(1000);
			counter = (counter+1)%5; // check timeout every 5 sec
		}
		delete contactID;
		delete contactDisplayname;
		delete principalDisplayname;
	}
	delete data;
}

/*
 * Send current status every 15 minutes
 */
static void __cdecl RVPSendPresenceThread(void *ptr) {
	int counter = 0;
	time_t lastTime = time(NULL);
	RVPImpl *impl = (RVPImpl *) ptr;
	while (impl->isGroupAllowed(RVPImpl::TGROUP_NORMAL)) {
		if (counter == 0) {
			time_t t = time(NULL);
			if (t - lastTime > 900) {
				lastTime = t;
				impl->sendStatus();
			}
		}
		counter = (counter+1) % 30; // check timeout every 30 sec
		Sleep(1000);
	}
}

/*
 * Renew subscriptions
 */
static void __cdecl RVPRenewSubscriptionsThread(void *ptr) {
	int counter = 0;
	RVPImpl *impl = (RVPImpl *) ptr;
	while (impl->isGroupAllowed(RVPImpl::TGROUP_NORMAL)) {
		time_t t = time(NULL);
		if (counter == 0) {
			RVPSubscription *subscription = (RVPSubscription *)impl->getClient()->getSubscriptions()->get(0);
			for (;subscription != NULL; subscription = (RVPSubscription *)subscription->getNext()) {
				if (subscription->getExpiry() - t < 1800) { // subscription expires in less than 1/2 h
					impl->getClient()->renew(subscription);
					counter = 29;
					break;
				}
			}
		}
		Sleep(1000);
		counter = (counter+1) % 30; // check timeout every 30 sec
	}
}

static void __cdecl RVPSearchAsyncThread(void *ptr) {
	RVPImplAsyncData *data = (RVPImplAsyncData *)ptr;
	RVPSubscription *subscription = data->impl->getClient()->subscribe(data->getMessage());
	if (subscription != NULL) {
		RVP_SEARCH_RESULT jsr;
		jsr.hdr.cbSize = sizeof(RVP_SEARCH_RESULT);
		strncpy(jsr.jid, data->getMessage(), sizeof(jsr.jid));
		jsr.jid[sizeof(jsr.jid)-1] = '\0';
		if (subscription->getDisplayName() != NULL) {
			jsr.hdr.nick = (char *)subscription->getDisplayName();
		} else {
			jsr.hdr.nick = "";
		}
		if (subscription->getEmail() != NULL) {
			jsr.hdr.email = (char *)subscription->getEmail();
		} else {
			jsr.hdr.email = "";
		}
		jsr.hdr.firstName = "";
		jsr.hdr.lastName = "";
		data->impl->getClient()->unsubscribe(data->subscription);
		ProtoBroadcastAck(rvpProtoName, NULL, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE) data->id, (LPARAM) &jsr);
	}
	ProtoBroadcastAck(rvpProtoName, NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE) data->id, 0);
	delete data;
}

static void __cdecl RVPSignOutThread(void *ptr) {
	RVPImpl *impl = (RVPImpl *) ptr;
	impl->signOutThreadCall();
}

RVPImplAsyncData::RVPImplAsyncData(RVPImpl *c) {
	impl = c;
	subscription = NULL;
	hContact = NULL;
	message = NULL;
	messageW = NULL;
	status = oldStatus = ID_STATUS_OFFLINE;
}

RVPImplAsyncData::~RVPImplAsyncData() {
	if (message != NULL) delete message;
	if (messageW != NULL) delete messageW;
}

void RVPImplAsyncData::setMessage(const char *str) {
	message = Utils::dupString(str);
}

void RVPImplAsyncData::setMessage(const wchar_t *str) {
	messageW = Utils::dupString(str);
}

const wchar_t *RVPImplAsyncData::getMessageW() {
	return messageW;
}

const char *RVPImplAsyncData::getMessage() {
	return message;
}

RVPImpl::RVPImpl() {
	client = new RVPClient(this);
	oldStatus = ID_STATUS_OFFLINE;
	typingNotifications = new List();
	msgCounter = typingCoutner = renewCounter = searchCounter = 0;
}

RVPClient *RVPImpl::getClient() {
	return client;
}

int RVPImpl::signIn(const char *login, const char *manualServer, int initialStatus) {
	/* TODO: increase thread counter here*/
	waitForThreads(TGROUP_SHUTDOWN);
	setAllowedGroups(TGROUP_SIGNIN | TGROUP_NORMAL | TGROUP_SHUTDOWN | TGROUP_SIGNOUT);
	int result = client->signIn(login, manualServer);
	if (result == 0) {
		forkThread(TGROUP_NORMAL, RVPSendPresenceThread, 0, this);
		forkThread(TGROUP_NORMAL, RVPRenewSubscriptionsThread, 0, this);
	}
	/* Check who is watching me */
	client->getSubscriptions();
	/* Check who can watching me */
	client->getACL();
	/* Set my status */
	oldStatus = initialStatus;
	client->sendPresence(initialStatus);
	/* Get Display Name */
	RVPSubscription *subscription = getProperty(NULL, "d:displayname");
	if (subscription != NULL) {
		delete subscription;
	}
	return result;
}

int RVPImpl::signOutThreadCall() {
	getClient()->stopListening();
	setStatus(ID_STATUS_OFFLINE);
	setAllowedGroups(RVPImpl::TGROUP_SHUTDOWN);
	waitForThreads(RVPImpl::TGROUP_SIGNIN | RVPImpl::TGROUP_NORMAL);
	unsubscribeAll();
	waitForThreads(RVPImpl::TGROUP_SHUTDOWN);
	getClient()->signOut();
	setAllowedGroups(RVPImpl::TGROUP_SIGNIN);
	return 0;
}

int RVPImpl::signOut() {
	if (!forkThread(TGROUP_SIGNOUT, RVPSignOutThread, 0, this)) {
		return 1;
	}
	return 0;
}

void RVPImpl::setCredentials(HTTPCredentials *c) {
	client->setCredentials(c);
}

int RVPImpl::setStatus(int status) {
	RVPImplAsyncData *data = new RVPImplAsyncData(this);
	data->status = status;
	data->oldStatus = oldStatus;
	oldStatus = status;
	if (!forkThread(TGROUP_NORMAL, RVPSetStatusAsyncThread, 0, data)) {
		delete data;
	}
	return 0;
}

int RVPImpl::sendMessage(CCSDATA *ccs) {
	RVPImplAsyncData *data = new RVPImplAsyncData(this);
	data->hContact = ccs->hContact;
	char *msg = (char *)ccs->lParam;
	if ( ccs->wParam & PREF_UNICODE ) {
		data->setMessage((wchar_t *)&msg[strlen(msg) + 1]);
	} else {
		data->setMessage(msg);
	}
	data->id = ++msgCounter;
	if (!forkThread(TGROUP_NORMAL, RVPSendMessageAsyncThread, 0, data)) {
		ProtoBroadcastAck(rvpProtoName, data->hContact, ACKTYPE_MESSAGE, ACKRESULT_FAILED, (HANDLE) data->id, 0);
		delete data;
	}
	return msgCounter;
}

int RVPImpl::sendTyping(HANDLE hContact, bool on) {
	char cid[128];
	sprintf(cid, "%08X", (int)hContact);
	if (on) {
		RVPImplAsyncData *data = new RVPImplAsyncData(this);
		data->hContact = hContact;
		typingNotifications->add(new ListItem(cid));
		if (!forkThread(TGROUP_NORMAL, RVPSendTypingAsyncThread, 0, data)) {
			delete data;
		}
	} else {
		ListItem *item = typingNotifications->find(cid);
		if (item != NULL) {
			typingNotifications->release(item);
		}
	}
	return 0;
}

int	RVPImpl::sendFileInvite(HANDLE hContact, const char * filenames[] , int filenum) {
	struct _stat statbuf;
	char *contactID = Utils::getLogin(hContact);
	if (_stat(filenames[0], &statbuf)) {
      //   JabberLog("'%s' is an invalid filename", files[i]);
	} else {
		RVPFile *rvpFile = new RVPFile(RVPFile::MODE_SEND, contactID, "12345678", client->getSignInName());
		rvpFile->setFile(filenames[0]);
		rvpFile->setSize(statbuf.st_size);
	}	
	delete contactID;
}

int RVPImpl::sendFileAccept(RVPFile *file) {
	RVPImplAsyncData *data = new RVPImplAsyncData(this);
	data->hContact = Utils::getContactFromId(file->getContact());
	data->file = file;
	if (!forkThread(TGROUP_NORMAL, RVPSendFileAccept, 0, data)) {
		delete data;
	}
	return 0;
}

int RVPImpl::sendFileReject(RVPFile *file) {
	RVPImplAsyncData *data = new RVPImplAsyncData(this);
	data->hContact = Utils::getContactFromId(file->getContact());
	data->file = file;
	if (!forkThread(TGROUP_NORMAL, RVPSendFileReject, 0, data)) {
		delete data;
		delete file;
	}
	return 0;
}

int	RVPImpl::searchContact(const char *login) {
	RVPImplAsyncData *data = new RVPImplAsyncData(this);
	data->id = ++searchCounter;
	char *url = RVPClient::getUrlFromLogin(login);
	if (url != NULL) {
		char *newlogin = RVPClient::getLoginFromUrl(url);
		if (newlogin != NULL) {
			data->setMessage(newlogin);
			if (!forkThread(TGROUP_NORMAL, RVPSearchAsyncThread, 0, data)) {
				ProtoBroadcastAck(rvpProtoName, NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE) data->id, 0);
				delete data;
			}
			delete newlogin;
		}
		delete url;
	}
	return searchCounter;
}

RVPSubscription* RVPImpl::getProperty(const char *node, const char *property) {
	return client->getProperty(node, property);
}

int RVPImpl::subscribeAll() {
	RVPImplAsyncData *data;
	HANDLE hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
	while (hContact != NULL) {
		char *szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
		if (szProto!=NULL && !strcmp(rvpProtoName, szProto)) {
			/* TODO: wait until number of threads < 10*/
			data = new RVPImplAsyncData(this);
			data->hContact = hContact;
			if (!forkThread(TGROUP_NORMAL, RVPSubscribeAsyncThread, 0, data)) {
				delete data;
			}
		}
		hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
	}
	/* Subscribe to any changes made to my account (callback) */
	data = new RVPImplAsyncData(this);
	data->hContact = NULL;
	if (!forkThread(TGROUP_NORMAL, RVPSubscribeAsyncThread, 0, data)) {
		delete data;
	}
	return 0;
}

int RVPImpl::subscribe(HANDLE hContact) {
	RVPImplAsyncData *data;
	data = new RVPImplAsyncData(this);
	data->hContact = hContact;
	if (!forkThread(TGROUP_NORMAL, RVPSubscribeAsyncThread, 0, data)) {
		delete data;
	}
	return 0;
}

int RVPImpl::unsubscribe(HANDLE hContact) {
	char *contactId = Utils::getLogin(hContact);
	if (contactId != NULL) {
		RVPSubscription *subscription = client->getSubscription(contactId);
		if (subscription != NULL) {
			RVPImplAsyncData *data = new RVPImplAsyncData(this);
			data->subscription = subscription;
			if (!forkThread(TGROUP_SHUTDOWN, RVPUnsubscribeAsyncThread, 0, data)) {
				delete data;
			}
		}
		delete contactId;
	}
	return 0;
}

int RVPImpl::unsubscribeAll() {
	RVPSubscription *subscriptionNext;
	RVPSubscription *subscription = (RVPSubscription *)client->getSubscriptions()->get(0);
	for (;subscription != NULL; subscription = subscriptionNext) {
		subscriptionNext = (RVPSubscription *)subscription->getNext();
		RVPImplAsyncData *data = new RVPImplAsyncData(this);
		data->subscription = subscription;
		if (!forkThread(TGROUP_SHUTDOWN, RVPUnsubscribeAsyncThread, 0, data)) {
			delete data;
		}
	}
	return 0;
}

int RVPImpl::sendStatus() {
	return client->sendPresence(oldStatus);
}

bool RVPImpl::isTyping(HANDLE hContact) {
	char cid[128];
	sprintf(cid, "%08X", (int)hContact);
	return (typingNotifications->find(cid) != NULL);
}


void RVPImpl::onStatus(const char *login, int status) {
	HANDLE hContact = Utils::getContactFromId(login);
	if (hContact!=NULL) {
		if (DBGetContactSettingWord(hContact, rvpProtoName, "Status", ID_STATUS_OFFLINE) != status)
			DBWriteContactSettingWord(hContact, rvpProtoName, "Status", (WORD) status);
	}
}

void RVPImpl::onTyping(const char *login) {
	HANDLE hContact = Utils::getContactFromId(login);
	if (hContact!=NULL) {
		CallService(MS_PROTO_CONTACTISTYPING, (WPARAM)hContact, (LPARAM) 10);//PROTOTYPE_CONTACTTYPING_INFINITE);
	}
}

void RVPImpl::onMessage(const char *login, const char *nick, const wchar_t *message) {
	HANDLE hContact = Utils::getContactFromId(login);
	if (hContact==NULL) {
		hContact = Utils::createContact(login, nick, FALSE);
	}
	if (hContact!=NULL) {
		CCSDATA ccs;
		PROTORECVEVENT recv;
		char *messageA = Utils::convertToString(message); // Utils::utf8Decode(request->getContent());
		int len = strlen(messageA)+1;
		int wlen = wcslen(message)+1;
		char *blob = new char[(len + sizeof(wchar_t)) * wlen];
		memcpy(blob, messageA, len);
		memcpy(blob+len, message, wlen * sizeof(wchar_t));
		delete messageA;
		recv.flags = PREF_UNICODE;
		recv.timestamp = (DWORD) time(NULL);
		recv.szMessage = blob;
		recv.lParam = 0;
		ccs.hContact = hContact;
		ccs.wParam = 0;
		ccs.szProtoService = PSR_MESSAGE;
		ccs.lParam = (LPARAM) &recv;
		CallService(MS_PROTO_CONTACTISTYPING, (WPARAM)hContact, (LPARAM)PROTOTYPE_CONTACTTYPING_OFF);
		CallService(MS_PROTO_CHAINRECV, 0, (LPARAM) &ccs);
		delete blob;
	}
}

void RVPImpl::onFileInvite(const char *login, const char *nick, RVPFile *rvpFile) {
	PROTORECVEVENT pre;
	CCSDATA ccs;
	HANDLE hContact = Utils::getContactFromId(login);
	if (hContact==NULL) {
		hContact = Utils::createContact(login, nick, FALSE);
	}
	// blob is DWORD(*ft), ASCIIZ(filenames), ASCIIZ(description)
	char *szBlob = (char *) malloc(sizeof(DWORD) + strlen(rvpFile->getFile()) + 2);
	*((PDWORD) szBlob) = (DWORD) rvpFile;
	strcpy(szBlob + sizeof(DWORD), rvpFile->getFile());
	szBlob[sizeof(DWORD) + strlen(rvpFile->getFile()) + 1] = '\0';
	pre.flags = 0;
	pre.timestamp = time(NULL);
	pre.szMessage = szBlob;
	pre.lParam = 0;
	ccs.szProtoService = PSR_FILE;
	ccs.hContact = hContact;
	ccs.wParam = 0;
	ccs.lParam = (LPARAM) &pre;
	CallService(MS_PROTO_CHAINRECV, 0, (LPARAM) &ccs);
	free(szBlob);
}

void RVPImpl::onFileProgress(RVPFile *file, int type, int progress) {
	HANDLE hContact = Utils::getContactFromId(file->getContact());
	if (hContact != NULL) {
		if (type == RVPFileListener::PROGRESS_CONNECTING) {
			ProtoBroadcastAck(rvpProtoName, hContact, ACKTYPE_FILE, ACKRESULT_CONNECTED, file, 0);
		} else if (type == RVPFileListener::PROGRESS_CONNECTED) {
			ProtoBroadcastAck(rvpProtoName, hContact, ACKTYPE_FILE, ACKRESULT_CONNECTED, file, 0);
		} else if (type == RVPFileListener::PROGRESS_INITIALIZING) {
			ProtoBroadcastAck(rvpProtoName, hContact, ACKTYPE_FILE, ACKRESULT_INITIALISING, file, 0);
		} else if (type == RVPFileListener::PROGRESS_PROGRESS) {
			PROTOFILETRANSFERSTATUS pfts;
			if (file != NULL) {
				char *files[1];
				files[0] = (char *)file->getFile();
				memset(&pfts, 0, sizeof(PROTOFILETRANSFERSTATUS));
				pfts.cbSize = sizeof(PROTOFILETRANSFERSTATUS);
				pfts.hContact = hContact;
				pfts.sending = file->getMode() == RVPFile::MODE_SEND ? TRUE : FALSE;
				pfts.files = files;
				pfts.totalFiles = 1;
				pfts.currentFileNumber = 0;
				pfts.totalBytes = file->getSize();
				pfts.workingDir = NULL;
				pfts.currentFile = (char *)file->getFile();
				pfts.currentFileSize = file->getSize();
				pfts.currentFileTime = 0;
				pfts.totalProgress = progress;
				pfts.currentFileProgress = progress;
				ProtoBroadcastAck(rvpProtoName, hContact, ACKTYPE_FILE, ACKRESULT_DATA, file, (LPARAM) &pfts);
			}
		}
	}
}

/*
void RVPImpl::onFileReject(const char *login, const char *filename, int filesize) {
}

void RVPImpl::onFileAccept(const char *login, const char *filename, int filesize) {
}

void RVPImpl::onFileProgress(const char *login, const char *filename, int filesize) {
}

void RVPImpl::onFileComplete(const char *login, const char *filename, int filesize) {
}

void RVPImpl::onFileError(const char *login, const char *filename, int filesize) {
}
*/
