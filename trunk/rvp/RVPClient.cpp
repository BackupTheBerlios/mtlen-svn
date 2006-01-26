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
#include "rvp.h"
#include "RVPClient.h"
#include "time.h"
#include "math.h"
#include "Utils.h"
#include "httplib/HTTPLib.h"
#include "httplib/MD5.h"
#include "xmllib/XMLObject.h"
#include <windns.h>
#include <sys/types.h>
#include <sys/stat.h>

static HINSTANCE hInstDNSDll=LoadLibrary("dnsapi.dll");

List RVPSubscription::list;
List RVPDNSMapping::list;

RVPDNSMapping::RVPDNSMapping(const char *srcHost, const char *destHost, int port):ListItem(srcHost) {
	this->srcHost = Utils::dupString(srcHost);
	this->destHost = Utils::dupString(destHost);
	this->port = port;
}

RVPDNSMapping::~RVPDNSMapping() {
	list.remove(this);
	if (srcHost != NULL) delete srcHost;
	if (destHost != NULL) delete destHost;
}

RVPDNSMapping* RVPDNSMapping::add(const char *srcHost, const char *destHost, int port) {
	RVPDNSMapping *mapping  = new RVPDNSMapping(srcHost, destHost, port);
	list.add(mapping);
	if (strcmpi(srcHost, destHost)) {
		RVPDNSMapping *mappingRev  = new RVPDNSMapping(destHost, srcHost, port);
		list.add(mappingRev);
	}
	return mapping;
}

RVPDNSMapping* RVPDNSMapping::find(const char *host) {
	RVPDNSMapping *mapping = NULL;
	ListItem* item = list.find(host);
	if (item != NULL) {
		mapping = (RVPDNSMapping *)item;
	}
	return mapping;
}

void RVPDNSMapping::releaseAll() {
	list.releaseAll();
}

const char * RVPDNSMapping::getDestHost() {
	return destHost;
}

RVPSubscription* RVPSubscription::add(RVPClient *client, const char *login, const char *sid) {
	RVPSubscription *subscription  = new RVPSubscription(client, login);
	subscription->setSid(sid);
	list.add(subscription);
	return subscription;
}

RVPSubscription* RVPSubscription::find(const char *login) {
	RVPSubscription *subscription = NULL;
	ListItem* item = list.find(login);
	if (item != NULL) {
		subscription = (RVPSubscription *)item;
	}
	return subscription;
}

List* RVPSubscription::getSubscriptions() {
	return &list;
}

RVPSubscription::RVPSubscription(RVPClient *client, const char *login):ListItem(login) {
	this->client = client;
	sid = NULL;
	email = NULL;
	displayName = NULL;
	status = ID_STATUS_OFFLINE;
}

RVPSubscription::~RVPSubscription() {
	list.remove(this);
	if (email != NULL) delete email;
	if (displayName != NULL) delete displayName;
	if (sid != NULL) delete sid;
}

void RVPSubscription::setExpiry(time_t t) {
	expiry = t;
}

void RVPSubscription::setSid(const char *str) {
	if (sid != NULL) delete sid;
	sid = Utils::dupString(str);
}

void RVPSubscription::setDisplayName(const char *str) {
	if (displayName != NULL) delete displayName;
	displayName = Utils::dupString(str);
}

void RVPSubscription::setEmail(const char *str) {
	if (email != NULL) delete email;
	email = Utils::dupString(str);
}

void RVPSubscription::setStatus(int s) {
	status = s;
}

RVPClient *RVPSubscription::getClient() {
	return client;
}


time_t RVPSubscription::getExpiry() {
	return expiry;
}

const char * RVPSubscription::getSid() {
	return sid;
}

const char * RVPSubscription::getDisplayName() {
	return displayName;
}

const char * RVPSubscription::getEmail() {
	return email;
}

int RVPSubscription::getStatus() {
	return status;
}

List RVPSession::list;

RVPSession::RVPSession(const char *id):ListItem(id) {
	sid = NULL;
}

RVPSession::~RVPSession() {
	if (sid != NULL) delete sid;
}

void RVPSession::setSid(const char *str) {
	if (sid != NULL) delete sid;
	sid = Utils::dupString(str);
}

const char * RVPSession::getSid() {
	return sid;
}

void RVPSession::add(const char *login, const char *sid) {
	ListItem* item = list.find(login);
	RVPSession *session = NULL;
	if (item == NULL) {
		session = new RVPSession(login);
		list.add(session);
	} else {
		session = (RVPSession *)item;
	}
	session->setSid(sid);
}

RVPSession* RVPSession::get(const char *login) {
	RVPSession *session = NULL;
	ListItem* item = list.find(login);
	if (item == NULL) {
		char out[128];
		unsigned int output[4];
		time_t tim = time(NULL);
		int rnd = rand();
		MD5 md5;
		md5.init();
		md5.update((unsigned char *)login, strlen(login));
		md5.update((unsigned char *)&time, sizeof(tim));
		md5.update((unsigned char *)&rnd, sizeof(rnd));
		md5.finalize();
		md5.get(output);
		sprintf(out, "{%08X-%04X-%04X-%04X-%04X%08X}", output[0], output[1]>>16, output[1]&0xFFFF, output[2]>>16, output[2]&0xFFFF, output[3]);
		session = new RVPSession(login);
		session->setSid(out);
		list.add(session);
	} else {
		session = (RVPSession *)item;
	}
	return session;
}

void RVPSession::releaseAll() {
	list.releaseAll();
}


void RVPClient::onNewConnection(Connection *connection, DWORD dwRemoteIP) {
	HTTPRequest *response;
	HTTPRequest *request;
	request = HTTPUtils::recvRequest(connection);
	if (request!= NULL && !strcmpi("NOTIFY", request->getMethod())) {
		response = new HTTPRequest();
		response->resultCode = 200;
		response->addHeader("Content-Type", "text/html");
		response->addHeader("RVP-Notifications-Version", "0.2");
		char respString[2048];
		sprintf(respString, "<HTML><HEAD><TITLE>Successful</TITLE></HEAD><BODY><H2>Success 200 (Successful)</H2><HR>NOTIFY on node %s succeeded<HR></BODY></HTML></p>", getCallback());
		response->setContent(respString);
		HTTPUtils::sendResponse(connection, response);
		delete response;
		
		if (request->dataLength > 0) {
			char *utf8Message = Utils::utf8Decode2(request->getContent(), request->dataLength);
			JString *js = new JString(utf8Message, strlen(utf8Message));//request->getContent(), request->dataLength);
			delete utf8Message;
			XMLObject *xml = new XMLObject();
			xml->parseXML(js);
			XMLObject *r_notificationXml = xml->getChild("r:notification");
			if (r_notificationXml!=NULL) {
				for (XMLObject *r_propnotificationXml = r_notificationXml->getChild("r:propnotification");r_propnotificationXml!=NULL;r_propnotificationXml=r_propnotificationXml->getNext("r:propnotification")) {
					XMLObject *r_notificationFromXml = r_propnotificationXml->getChild("r:notification-from");
					XMLObject *r_notificationToXml = r_propnotificationXml->getChild("r:notification-to");
					XMLObject *d_propertyupdateXml = r_propnotificationXml->getChild("d:propertyupdate");
					if (d_propertyupdateXml == NULL) {
						d_propertyupdateXml = r_propnotificationXml->getChild("d:propstat");
					}
					if (d_propertyupdateXml!=NULL && r_notificationFromXml!=NULL) {
						XMLObject *r_contactXml = r_notificationFromXml->getChild("r:contact");
						XMLObject *d_setXml = d_propertyupdateXml->getChild("d:set");
						if (d_setXml!=NULL && r_contactXml!=NULL) {
							XMLObject *d_hrefXml = r_contactXml->getChild("d:href");
							XMLObject *d_propXml = d_setXml->getChild("d:prop");
							if (d_hrefXml!=NULL && d_propXml!=NULL) {
								char *hrefStr = d_hrefXml->getData()->toString();
								char *login = RVPClient::getLoginFromUrl(hrefStr);
								if (login != NULL) {
									XMLObject *r_stateXml = d_propXml->getChild("r:state");
									if (r_stateXml!=NULL) {
										char *statusString = r_stateXml->getChild()->getName()->toString();
										int status = RVPClient::getStatusFromString(statusString);
										delete statusString;
										if (listener != NULL) {
											listener->onStatus(login, status);
										}
									}
									delete login;
								}
								delete hrefStr;
							}
						}
					}
				}
				for (XMLObject *r_messageXml = r_notificationXml->getChild("r:message");r_messageXml!=NULL;r_messageXml=r_messageXml->getNext("r:propnotification")) {
					XMLObject *r_notificationFromXml = r_messageXml->getChild("r:notification-from");
					XMLObject *r_notificationToXml = r_messageXml->getChild("r:notification-to");
					XMLObject *r_msgbodyXml = r_messageXml->getChild("r:msgbody");
					if (r_msgbodyXml!=NULL && r_notificationFromXml!=NULL) {
						XMLObject *r_contactXml = r_notificationFromXml->getChild("r:contact");
						XMLObject *r_mimeDataXml = r_msgbodyXml->getChild("r:mime-data");
						if (r_mimeDataXml!=NULL && r_contactXml!=NULL) {
							XMLObject *d_hrefXml = r_contactXml->getChild("d:href");
							XMLObject *r_descriptionXml = r_contactXml->getChild("r:description");
							if (d_hrefXml!=NULL && r_mimeDataXml->getCDATA()!=NULL) {
								char *hrefStr = d_hrefXml->getData()->toString();
								char *login = RVPClient::getLoginFromUrl(hrefStr);
								if (login != NULL) {
									char *nick = r_descriptionXml->getData()->toString();
									char *message = r_mimeDataXml->getCDATA()->getData()->toString();
									HTTPRequest *request = HTTPUtils::toRequest(message);
									HTTPHeader *typingHdr = request->getHeader("TypingUser");
									HTTPHeader *contentType = request->getHeader("Content-Type");
									HTTPHeader *sessionId = request->getHeader("Session-Id");
									if (sessionId != NULL) {
										RVPSession::add(login, sessionId->getValue());
										if (contentType != NULL) {
											if (strstr(contentType->getValue(), "text/x-msmsgscontrol") == contentType->getValue()) {
												/* typing notification */
												if (typingHdr!=NULL) {
													if (listener != NULL) {
														listener->onTyping(login);
													}
												}
											} else if (strstr(contentType->getValue(), "text/plain") == contentType->getValue()) {
												/* message */
												if (request->getContent()!=NULL && strlen(request->getContent())>0) {
													if (listener != NULL) {
														wchar_t *messageW = Utils::utf8DecodeW(request->getContent());
														listener->onMessage(login, nick, messageW);
														delete messageW;
													}
												}
											} else if (strstr(contentType->getValue(), "text/x-msmsgsinvite") == contentType->getValue()) {
												HTTPRequest *inviteRequest = HTTPUtils::toRequest(request->getContent());
												HTTPHeader *applicationNameHdr = inviteRequest->getHeader("Application-Name");
												HTTPHeader *applicationGUID = inviteRequest->getHeader("Application-GUID");
												HTTPHeader *invitationCommandHdr = inviteRequest->getHeader("Invitation-Command");
												HTTPHeader *invitationCookieHdr = inviteRequest->getHeader("Invitation-Cookie"); /* file transfer id */
												HTTPHeader *applicationFileHdr = inviteRequest->getHeader("Application-File");  /* file name */
												HTTPHeader *applicationFileSizeHdr = inviteRequest->getHeader("Application-FileSize"); /* file size */
												HTTPHeader *ipAddressHdr = inviteRequest->getHeader("IP-Address"); /* host ip */
												HTTPHeader *portHdr = inviteRequest->getHeader("Port"); /* port */
												HTTPHeader *authCookieHdr = inviteRequest->getHeader("AuthCookie"); /* auth cookie */
												if (invitationCommandHdr != NULL && invitationCookieHdr != NULL) {
													if (!strcmpi(invitationCommandHdr->getValue(), "INVITE")) { /* INVITE */
														/* file transfer */
														if (applicationNameHdr != NULL && !strcmpi(applicationNameHdr->getValue(), "File Transfer")) {
															if (applicationFileHdr != NULL && applicationFileSizeHdr != NULL) {
																if (listener != NULL) {
																	HANDLE hContact = Utils::getContactFromId(login);
																	char *node = getRealLoginFromLogin(getSignInName());
																	RVPFile *rvpFile = new RVPFile(hContact, RVPFile::MODE_RECV, login, invitationCookieHdr->getValue(), node, listener);
																	delete node;
																	rvpFile->setFile(applicationFileHdr->getValue());
																	rvpFile->setSize(atol(applicationFileSizeHdr->getValue()));
																	listener->onFileInvite(login, nick, rvpFile);
																}
															}
														}
													} else if (!strcmpi(invitationCommandHdr->getValue(), "ACCEPT")) { /* ACCEPT */
														if (login != NULL) {
															int mode = RVPFile::MODE_SEND;
															if (ipAddressHdr != NULL && portHdr != NULL && authCookieHdr != NULL) {
																mode = RVPFile::MODE_RECV;
															} 
															char *node = RVPClient::getRealLoginFromLogin(login);
															RVPFile *file = RVPFile::find(mode, login, invitationCookieHdr->getValue());
															if (file != NULL) {
																if (file->getMode() == RVPFile::MODE_RECV) {
																	/* Connect to given host/port */
																	if (ipAddressHdr != NULL && portHdr != NULL && authCookieHdr != NULL) {
																		file->setAuthCookie(authCookieHdr->getValue());
																		file->setHost(ipAddressHdr->getValue());
																		file->setPort(atol(portHdr->getValue()));
																		file->recv();
																	} else {
																		file->cancel();
																	}
																} else {
																	/* Start server and send details */
																	char *principalDisplayname = Utils::getDisplayName(NULL);
																	struct sockaddr_in saddr;
																	saddr.sin_addr.S_un.S_addr = localIP;
//																	sprintf(callbackHost, "http://%s:%d", inet_ntoa(saddr.sin_addr), wPort);
																	file->setHost(inet_ntoa(saddr.sin_addr));
																	file->setPort(0);
																	file->send();
																	if (sendFileAcceptResponse(file, login, nick, principalDisplayname)) {
																		file->cancel();
																	}
																	delete principalDisplayname;
																}
															}
															delete node;
														}
													} else if (!strcmpi(invitationCommandHdr->getValue(), "CANCEL")) { /* CANCEL */
														//if (login != NULL) {
														//	RVPFile *file = RVPFile::find(login, invitationCookieHdr->getValue());
														//	if (file != NULL) {
																/* Stop transfer */
														//		file->cancel();
														//	}
														//}
													}
												}
												delete inviteRequest;
											}
										}
									}
									delete request;
									delete message;
									delete nick;
									delete login;
								}
								delete hrefStr;
							}
						}
					}
				}
			}
			delete xml;
		}
	}
	if (request != NULL) {
		delete request;
	}
}

RVPClient::RVPClient(RVPClientListener *listener) {
	bOnline = false;
	server = NULL;
	signInName = NULL;
	principalUrl = NULL;
	serverPort = 80;
	credentials = NULL;
	bindConnection = NULL;
	lastStatus = ID_STATUS_OFFLINE;
	this->listener = listener;
	InitializeCriticalSection(&mutex);
}

RVPClient::~RVPClient() {
	if (isOnline()) {
//		signOut();
	}
	DeleteCriticalSection(&mutex);
}

void RVPClient::setCredentials(HTTPCredentials *c) {
	if (credentials!=NULL) delete credentials;
	credentials = c;
}

const char *RVPClient::getStatusString(int status) {
	switch (status) {
		case ID_STATUS_FREECHAT:
		case ID_STATUS_ONLINE:
			return "r:online";
		case ID_STATUS_ONTHEPHONE:
			return "r:on-phone";
		case ID_STATUS_OUTTOLUNCH:
			return "r:at-lunch";
		case ID_STATUS_AWAY:
			return "r:back-soon";
		case ID_STATUS_NA:
			return "r:away";
		case ID_STATUS_INVISIBLE:
			return "r:invisible";
		case ID_STATUS_OFFLINE:
			return "r:offline";
		case ID_STATUS_DND:
		case ID_STATUS_OCCUPIED:
			return "r:busy";
		default:
			break;
	}
	return "online";
}

int RVPClient::getRVPStatus(int status) {
	switch (status) {
		case ID_STATUS_FREECHAT:
		case ID_STATUS_ONLINE:
			return ID_STATUS_ONLINE;
		case ID_STATUS_ONTHEPHONE:
			return ID_STATUS_ONTHEPHONE;
		case ID_STATUS_OUTTOLUNCH:
			return ID_STATUS_OUTTOLUNCH;
		case ID_STATUS_AWAY:
			return ID_STATUS_AWAY;
		case ID_STATUS_NA:
			return ID_STATUS_NA;
		case ID_STATUS_INVISIBLE:
			return ID_STATUS_INVISIBLE;
		case ID_STATUS_OFFLINE:
			return ID_STATUS_OFFLINE;
		case ID_STATUS_DND:
		case ID_STATUS_OCCUPIED:
			return ID_STATUS_OCCUPIED;
		default:
			break;
	}
	return ID_STATUS_ONLINE;
}


int RVPClient::getStatusFromString(const char *statusString) {
	if (!strcmp(statusString, "r:online")) {
		return ID_STATUS_ONLINE;
	} else if (!strcmp(statusString, "r:back-soon")) {
		return ID_STATUS_AWAY;
	} else if (!strcmp(statusString, "r:away")) {
		return ID_STATUS_NA;
	} else if (!strcmp(statusString, "r:offline")) {
		return ID_STATUS_OFFLINE;
	} else if (!strcmp(statusString, "r:busy")) {
		return ID_STATUS_OCCUPIED;
	} else if (!strcmp(statusString, "r:at-lunch")) {
		return ID_STATUS_OUTTOLUNCH;
	} else if (!strcmp(statusString, "r:on-phone")) {
		return ID_STATUS_ONTHEPHONE;
	} else if (!strcmp(statusString, "r:invisible")) {
		return ID_STATUS_INVISIBLE;
	} else if (!strcmp(statusString, "r:idle")) {
		return ID_STATUS_AWAY;
	}
	return ID_STATUS_ONLINE;
}

char *RVPClient::getServerFromLogin(const char *signInName, const char *manualServer) {
	char *ptr = strchr(signInName, '@');
	if (ptr==NULL) {
		return NULL;
	}
	RVPDNSMapping *mapping = RVPDNSMapping::find(ptr + 1);
	if (mapping == NULL) {
#ifndef DNS_TYPE_SRV
# define DNS_TYPE_SRV (33)
#endif
#ifndef DNS_TYPE_AAAA
# define DNS_TYPE_AAAA (28)
#endif
		if (manualServer == NULL) {
			PDNS_RECORD rr, scan;
			char *rvpService = new char[20 + strlen(ptr+1)];
			sprintf (rvpService, "_rvp._tcp.%s", ptr+1);
			if (DnsQuery(rvpService, DNS_TYPE_SRV, DNS_QUERY_STANDARD, NULL, &rr, NULL) == 0) {
				for(scan = rr; scan != NULL; scan = scan->pNext) {
					if(scan->wType != DNS_TYPE_SRV || stricmp(scan->pName, rvpService) != 0)
						continue;
					mapping = RVPDNSMapping::add(ptr+1, scan->Data.SRV.pNameTarget, scan->Data.SRV.wPort);
					break;
				}
				DnsRecordListFree(rr, DnsFreeFlat);
			}
		} else {
			mapping = RVPDNSMapping::add(ptr+1, manualServer, 80);
		}
	}
	if (mapping != NULL) {
		return Utils::dupString(mapping->getDestHost());
	} else {
		return Utils::dupString(ptr + 1);
		/*
		if (strstr(ptr + 1, "im.") == ptr + 1) {
			return Utils::dupString(ptr + 1);
		}
		int len = strlen("im.") + strlen(ptr + 1) + 1;
		char *server = new char[len];
		_snprintf(server, len, "im.%s", ptr + 1);
		return server;
		*/
	}
}

char *RVPClient::getUrlFromLogin(const char *signInName) {
	char *server = getServerFromLogin(signInName, NULL);
	if (server != NULL) {
		char *str = Utils::dupString(signInName);
		char *ptr = strchr(str, '@');
		if (ptr != NULL) {
			*ptr = '\0';
			int len = strlen("http:///instmsg/aliases/") + strlen(server) + strlen(str) + 1;
			char *principalUrl = new char[len];
			_snprintf(principalUrl, len, "http://%s/instmsg/aliases/%s", server, str);
			return principalUrl;
		}
		delete str;
		delete server;
	}
	return NULL;
}
/*
char *RVPClient::getUrlFromLogin(const char *signInName) {
	char *str = Utils::dupString(signInName);
	char *ptr = strchr(str, '@');
	if (ptr != NULL) {
		*ptr = '\0';
		int len = strlen("/instmsg/aliases/") + strlen(str) + 1;
		char *principalUrl = new char[len];
		_snprintf(principalUrl, len, "/instmsg/aliases/%s", str);
		return principalUrl;
	}
	delete str;
	return NULL;
}
*/


char *RVPClient::getLoginFromUrl(const char *url) {
	char *str = Utils::dupString(url);
	char *server = str + 7;
	char *ptr = strchr(server, '/');
	char *ptr2 = strrchr(str, '/');
	char *login = NULL;
	if (ptr != NULL && ptr2 != NULL) {
		*ptr = '\0';
		RVPDNSMapping *mapping = RVPDNSMapping::find(server);
		if (mapping != NULL) {
			server = (char *)mapping->getDestHost();
		}
		int len = strlen("@") + strlen(server) + strlen(ptr2+1) + 1;
		login = new char[len];
		_snprintf(login, len, "%s@%s", ptr2+1, server);
	}
	delete str;
	return login;
}

char *RVPClient::getRealLoginFromLogin(const char *login) {
	char *reallogin = NULL;
	char *str = Utils::dupString(login);
	char *ptr = strchr(str, '@');
	if (ptr != NULL) {
		char *server = ptr + 1;
		*ptr = '\0';
		RVPDNSMapping *mapping = RVPDNSMapping::find(server);
		if (mapping != NULL) {
			server = (char *)mapping->getDestHost();
		}
		int len = strlen("@") + strlen(server) + strlen(str) + 1;
		reallogin = new char[len];
		_snprintf(reallogin, len, "%s@%s", str, server);
	}
	if (reallogin == NULL) {
		reallogin = Utils::dupString(login);
	}
	delete str;
	return reallogin;
}

int RVPClient::signIn(const char *signInName, const char *manualServer) {
	int result = 1;
	EnterCriticalSection(&mutex);
	if (bOnline) {
		LeaveCriticalSection(&mutex);
		return result;
	}
	if (this->signInName != NULL) delete this->signInName;
	if (principalUrl != NULL) delete this->principalUrl;
	if (server != NULL) delete this->server;
	this->signInName = Utils::dupString(signInName);
	RVPDNSMapping::releaseAll();
	server = getServerFromLogin(signInName, manualServer);
	principalUrl = getUrlFromLogin(signInName);
	if (server==NULL || principalUrl==NULL) {
		LeaveCriticalSection(&mutex);
		return result;
	}
	/* Try to connect to the server and determine local IP */
	Connection *con = new Connection(DEFAULT_CONNECTION_POOL);
	if (!con->connect(server, serverPort)) {
		// connection to server failed
		delete con;
		LeaveCriticalSection(&mutex);
		return result;
	} else {
		int socket = con->socket();
		if (socket!=INVALID_SOCKET) {
			struct sockaddr_in saddr;
			int len;
			len = sizeof(saddr);
			getsockname(socket, (struct sockaddr *) &saddr, &len);
			localIP = saddr.sin_addr.S_un.S_addr;
		} else {
			LeaveCriticalSection(&mutex);
			return result;
		}
	}
	delete con;
	bindConnection = new Connection(DEFAULT_CONNECTION_POOL);
	int wPort = bindConnection->bind(NULL, 0, this);
	if (wPort == 0) {
		delete bindConnection;
		bindConnection = NULL;
		LeaveCriticalSection(&mutex);
		return result;
	}
	struct sockaddr_in saddr;
	saddr.sin_addr.S_un.S_addr = localIP;
	sprintf(callbackHost, "http://%s:%d", inet_ntoa(saddr.sin_addr), wPort);
	/* Sign In */
	result = subscribeMain(callbackHost);
	if (result) {
		delete bindConnection;
		bindConnection = NULL;
	} else {
		bOnline = true;
	}
	LeaveCriticalSection(&mutex);
	return result;
}

int RVPClient::stopListening() {
	EnterCriticalSection(&mutex);
	if (bOnline) {
		if (bindConnection!=NULL) delete bindConnection;
	}
	LeaveCriticalSection(&mutex);
	return 0;
}

int RVPClient::signOut() {
	EnterCriticalSection(&mutex);
	if (bOnline) {
//		if (lSocket!=NULL) Netlib_CloseHandle(lSocket);
		RVPSession::releaseAll();
		bOnline = false;
	}
	LeaveCriticalSection(&mutex);
	return 0;
}

int RVPClient::sendPresence(int status) {
	int result = 1;
	if (bOnline) {
		int bufferSize = 0;
		char *buffer = NULL;
		char *utf8Message;
		HTTPRequest *request, *response;
		request = new HTTPRequest();
		request->setMethod("PROPPATCH");
		request->setUrl(principalUrl);
		request->addHeader("RVP-Notifications-Version", "0.2");
		request->addHeader("RVP-From-Principal", principalUrl);
		request->addHeader("Content-Type", "text/xml");
		request->setCredentials(credentials);
		Utils::appendText(&buffer, &bufferSize, "<?xml version=\"1.0\"?><d:propertyupdate xmlns:d='DAV:' xmlns:r='http://schemas.microsoft.com/rvp/' xmlns:a='http://schemas.microsoft.com/rvp/acl/'><d:set><d:prop><r:state><r:leased-value><r:value><%s/></r:value><r:default-value><r:offline/></r:default-value><d:timeout>1200</d:timeout></r:leased-value></r:state></d:prop></d:set></d:propertyupdate>", getStatusString(status));
		utf8Message = Utils::utf8Encode2(buffer);
		free(buffer);
		request->setContent(utf8Message, strlen(utf8Message));
		delete utf8Message;
		response = HTTPUtils::performTransaction(request);
		delete request;
		if (response != NULL) {
			result = response->resultCode;
			delete response;
			lastStatus = status;
		}
	}
	return (result/100 == 2) ? 0 : result;
}

int RVPClient::sendPresence() {
	return sendPresence(lastStatus);
}

/*
<?xml version="1.0"?>
<r:notification xmlns:d='DAV:' xmlns:r='http://schemas.microsoft.com/rvp/' xmlns:a='http://schemas.microsoft.com/rvp/acl/'><r:message><r:notification-from><r:contact><d:href>http://im.sabre.com/instmsg/aliases/piotr.piastucki</d:href><r:description>Piastucki, Piotr</r:description></r:contact></r:notification-from><r:notification-to><r:contact><d:href>http://im.sabre.com/instmsg/aliases/grzegorz.mucha</d:href><r:description>Mucha, Grzegorz</r:description></r:contact></r:notification-to><r:msgbody><r:mime-data><![CDATA[MIME-Version: 1.0
Content-Type: text/plain; charset=UTF-8
X-MMS-IM-Format: FN=Tahoma; EF=; CO=0; CS=ee; PF=22
Session-Id: {FE85D9C6-8995-4524-B8F2-D29E25D4555C}
a sniffuje co messenger wysyla - zrobilem juz kawalek plugina do Mirandy, dziala logowanie przez NTLM i zmiany statusow, teraz patrze jak wyglada wysylanie wiadomosci]]></r:mime-data></r:msgbody></r:message></r:notification>
*/

int RVPClient::sendMessage(const char *message, const char *contactID, const char *contactDisplayname, const char *principalDisplayname) {
	int result = 1;
	if (bOnline) {
		int bufferSize, cdataBufferSize;
		char *buffer, *cdataBuffer, *cdata;
		char *node = getUrlFromLogin(contactID);
		if (node != NULL) {
			char *utf8Message = Utils::utf8Encode(message);
			RVPSession *session = RVPSession::get(contactID);
			HTTPRequest *request, *response;
			request = new HTTPRequest();
			request->setMethod("NOTIFY");
			request->setUrl(node);
			request->addHeader("RVP-Ack-Type", "DeepOr");
			request->addHeader("RVP-Hop-Count", "1");
			request->addHeader("RVP-Notifications-Version", "0.2");
			request->addHeader("RVP-From-Principal", principalUrl);
			request->addHeader("Content-Type", "text/xml");
			request->setCredentials(credentials);
			buffer = NULL;
			cdataBuffer = NULL;
		/* TODO: get url from login here*/
			Utils::appendText(&cdataBuffer, &cdataBufferSize,
			"MIME-Version: 1.0\r\n"
			"Content-Type: text/plain; charset=UTF-8\r\n"
			"X-MMS-IM-Format: FN=Tahoma; EF=; CO=0; CS=ee; PF=22\r\n"
			"Session-Id: %s\r\n\r\n%s", session->getSid(), utf8Message);
			delete utf8Message;
			cdata = Utils::cdataEncode(cdataBuffer);
			free(cdataBuffer);
			Utils::appendText(&buffer, &bufferSize,
			"<?xml version=\"1.0\"?>"
			"<r:notification xmlns:d='DAV:' xmlns:r='http://schemas.microsoft.com/rvp/' xmlns:a='http://schemas.microsoft.com/rvp/acl/'>"
			"<r:message><r:notification-from><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-from>"
			"<r:notification-to><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-to>"
			"<r:msgbody><r:mime-data>%s</r:mime-data></r:msgbody></r:message></r:notification>",
			principalUrl, principalDisplayname, node, contactDisplayname, cdata);
			utf8Message = Utils::utf8Encode2(buffer);
			free(buffer);
			request->setContent(utf8Message, strlen(utf8Message));
			delete utf8Message;
			response = HTTPUtils::performTransaction(request);
			delete request;
			delete cdata;
			delete node;
			if (response != NULL) {
				result = response->resultCode;
				delete response;
			}
		}
	}
	return (result/100 == 2) ? 0 : result;
}

int RVPClient::sendMessage(const wchar_t *message, const char *contactID, const char *contactDisplayname, const char *principalDisplayname) {
	int result = 1;
	if (bOnline) {
		int bufferSize, cdataBufferSize;
		char *buffer, *cdataBuffer, *cdata;
		char *node = getUrlFromLogin(contactID);
		if (node != NULL) {
			char *utf8Message = Utils::utf8Encode(message);
			RVPSession *session = RVPSession::get(contactID);
			HTTPRequest *request, *response;
			request = new HTTPRequest();
			request->setMethod("NOTIFY");
			request->setUrl(node);
			request->addHeader("RVP-Ack-Type", "DeepOr");
			request->addHeader("RVP-Hop-Count", "1");
			request->addHeader("RVP-Notifications-Version", "0.2");
			request->addHeader("RVP-From-Principal", principalUrl);
			request->addHeader("Content-Type", "text/xml");
			request->setCredentials(credentials);
			buffer = NULL;
			cdataBuffer = NULL;
			Utils::appendText(&cdataBuffer, &cdataBufferSize,
			"MIME-Version: 1.0\r\n"
			"Content-Type: text/plain; charset=UTF-8\r\n"
			"X-MMS-IM-Format: FN=Tahoma; EF=; CO=0; CS=ee; PF=22\r\n"
			"Session-Id: %s\r\n\r\n%s", session->getSid(), utf8Message);
			delete utf8Message;
			cdata = Utils::cdataEncode(cdataBuffer);
			free(cdataBuffer);
			Utils::appendText(&buffer, &bufferSize,
			"<?xml version=\"1.0\"?>"
			"<r:notification xmlns:d='DAV:' xmlns:r='http://schemas.microsoft.com/rvp/' xmlns:a='http://schemas.microsoft.com/rvp/acl/'>"
			"<r:message><r:notification-from><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-from>"
			"<r:notification-to><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-to>"
			"<r:msgbody><r:mime-data>%s</r:mime-data></r:msgbody></r:message></r:notification>",
			principalUrl, principalDisplayname, node, contactDisplayname, cdata);
			utf8Message = Utils::utf8Encode2(buffer);
			free(buffer);
			request->setContent(utf8Message, strlen(utf8Message));
			delete utf8Message;
			response = HTTPUtils::performTransaction(request);
			delete request;
			delete cdata;
			delete node;
			if (response != NULL) {
				result = response->resultCode;
				delete response;
			}
		}
	}
	return (result/100 == 2) ? 0 : result;
}

int RVPClient::sendTyping(const char *contactID, const char *contactDisplayname, const char *principalDisplayname) {
	int result = 1;
	if (bOnline) {
		int bufferSize;
		char *buffer;
		char *node = getUrlFromLogin(contactID);
		if (node != NULL) {
			char *utf8Message;
			RVPSession *session = RVPSession::get(contactID);
			HTTPRequest *request, *response;
			request = new HTTPRequest();
			request->setMethod("NOTIFY");
			request->setUrl(node);
			request->addHeader("RVP-Ack-Type", "DeepOr");
			request->addHeader("RVP-Hop-Count", "1");
			request->addHeader("RVP-Notifications-Version", "0.2");
			request->addHeader("RVP-From-Principal", principalUrl);
			request->addHeader("Content-Type", "text/xml");
			request->setCredentials(credentials);
			buffer=NULL;
			/* TODO: get url from login here*/
			Utils::appendText(&buffer, &bufferSize,
			"<?xml version=\"1.0\"?>"
			"<r:notification xmlns:d='DAV:' xmlns:r='http://schemas.microsoft.com/rvp/' xmlns:a='http://schemas.microsoft.com/rvp/acl/'>"
			"<r:message><r:notification-from><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-from>"
			"<r:notification-to><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-to>"
			"<r:msgbody><r:mime-data><![CDATA[MIME-Version: 1.0\r\n"
			"Content-Type: text/x-msmsgscontrol\r\n"
			"TypingUser: %s\r\n"
			"Session-Id: %s\r\n\r\n]]></r:mime-data></r:msgbody></r:message></r:notification>",
			principalUrl, principalDisplayname, node, contactDisplayname, signInName, session->getSid());
			utf8Message = Utils::utf8Encode2(buffer);
			free(buffer);
			request->setContent(utf8Message, strlen(utf8Message));
			delete utf8Message;
			response = HTTPUtils::performTransaction(request);
			delete request;
			delete node;
			if (response != NULL) {
				result = response->resultCode;
				delete response;
			}
		}
	}
	return (result/100 == 2) ? 0 : result;
}

RVPSubscription *RVPClient::subscribe(const char *login) {
	RVPSubscription *subscription = NULL;
	if (bOnline && login != NULL) {
		subscription = RVPSubscription::find(login);
		if (subscription != NULL) {
			return subscription;
		}
		char *node = getUrlFromLogin(login);
		if (node != NULL) {
			HTTPRequest *request, *response;
			request = new HTTPRequest();
			request->setMethod("SUBSCRIBE");
			request->setUrl(node);
			request->addHeader("Subscription-Lifetime", "14400");
			request->addHeader("Notification-Type", "update/propchange");
			request->addHeader("Call-Back", principalUrl);
			request->addHeader("RVP-Notifications-Version", "0.2");
			request->addHeader("RVP-From-Principal", principalUrl);
			request->setCredentials(credentials);
			response = HTTPUtils::performTransaction(request);
			delete request;
			delete node;
			if (response != NULL) {
				if (response->resultCode/100 == 2) {
					time_t expiry = time(NULL);
					HTTPHeader *header = response->getHeader("Subscription-Id");
					HTTPHeader *lifetime = response->getHeader("Subscription-Lifetime");
					if (lifetime != NULL) {
						expiry += atol(lifetime->getValue());
					} else {
						expiry += 14400;
					}
					if (header!=NULL) {
						subscription = RVPSubscription::add(this, login, header->getValue());
						subscription->setExpiry(expiry);
						JString *js = new JString(response->getContent(), response->dataLength);
						XMLObject *xml = new XMLObject();
						xml->parseXML(js);
						delete js;
						XMLObject *d_multistatusXml, *d_responseXml, *d_propstatXml, *d_propXml, *xml2;
						if ((d_multistatusXml = xml->getChild("d:multistatus")) != NULL) {
							if ((d_responseXml = d_multistatusXml->getChild("d:response")) != NULL) {
								if ((d_propstatXml = d_responseXml->getChild("d:propstat")) != NULL) {
									if ((d_propXml = d_propstatXml->getChild("d:prop")) != NULL) {
										for (xml2=d_propXml->getChild(); xml2!=NULL; xml2=xml2->getNext()) {
											if (xml2->getName()->equals("r:state")) {
												char *statusString = xml2->getChild()->getName()->toString();
												subscription->setStatus(RVPClient::getStatusFromString(statusString));
												delete statusString;
											} else if (xml2->getName()->equals("d:displayname")) {
												char *nick = xml2->getData()->toString();
												subscription->setDisplayName(nick);
												delete nick;
											} else if (xml2->getName()->equals("r:email")) {
												char *email = xml2->getData()->toString();
												subscription->setEmail(email);
												delete email;
											}
										}
									}
								}
							}
						}
						delete xml;
					}
				}
				delete response;
			}
		}
	}
	return subscription;
}

int RVPClient::subscribeMain(const char *callbackHost) {
	int result = 1;
	RVPSubscription *subscription = NULL;
	if (callbackHost != NULL) {
		HTTPRequest *request, *response;
		request = new HTTPRequest();
		request->setMethod("SUBSCRIBE");
		request->setUrl(principalUrl);
		request->addHeader("Subscription-Lifetime", "14400");
		request->addHeader("Notification-Type", "pragma/notify");
		request->addHeader("User-Agent", "msmsgs/5.1.0.639");
		request->addHeader("Call-Back", callbackHost);
		request->addHeader("RVP-Notifications-Version", "0.2");
		request->addHeader("RVP-From-Principal", principalUrl);
		request->setCredentials(credentials);
		response = HTTPUtils::performTransaction(request);
		delete request;
		if (response != NULL) {
			result = response->resultCode;
			if (result / 100 == 2) {
				time_t expiry = time(NULL);
				HTTPHeader *header = response->getHeader("Subscription-Id");
				HTTPHeader *lifetime = response->getHeader("Subscription-Lifetime");
				if (lifetime != NULL) {
					expiry += atol(lifetime->getValue());
				} else {
					expiry += 14400;
				}
				if (header!=NULL) {
					subscription = RVPSubscription::add(this, "__main__", header->getValue());
					subscription->setExpiry(expiry);
				}
			}
			delete response;
		}
	}
	return (result/100 == 2) ? 0 : result;
}

int RVPClient::renew(RVPSubscription *subscription) {
	int result = 1;
	if (bOnline) {
		char *node;
		if (!strcmp(subscription->getId(), "__main__")) {
			node = Utils::dupString(principalUrl);
		} else {
			node = getUrlFromLogin(subscription->getId());
		}
		if (node != NULL) {
			HTTPRequest *request, *response;
			request = new HTTPRequest();
			request->setMethod("SUBSCRIBE");
			request->setUrl(node);
			request->addHeader("Subscription-Lifetime", "14400");
			request->addHeader("Subscription-Id", subscription->getSid());
			request->addHeader("RVP-Notifications-Version", "0.2");
			request->addHeader("RVP-From-Principal", principalUrl);
			request->setCredentials(credentials);
			response = HTTPUtils::performTransaction(request);
			delete request;
			if (response != NULL) {
				result = response->resultCode;
				if (result/100 == 2) {
					time_t expiry = time(NULL);
					HTTPHeader *header = response->getHeader("Subscription-Id");
					HTTPHeader *lifetime = response->getHeader("Subscription-Lifetime");
					if (lifetime != NULL) {
						expiry += atol(lifetime->getValue());
					} else {
						expiry += 14400;
					}
					if (header!=NULL) {
						subscription->setExpiry(expiry);
						/*
						JString *js = new JString(response->getContent(), response->dataLength);
						XMLObject *xml = new XMLObject();
						xml->parseXML(js);
						delete js;
						XMLObject *d_multistatusXml, *d_responseXml, *d_propstatXml, *d_propXml, *xml2;
						if ((d_multistatusXml = xml->getChild("d:multistatus")) != NULL) {
							if ((d_responseXml = d_multistatusXml->getChild("d:response")) != NULL) {
								if ((d_propstatXml = d_responseXml->getChild("d:propstat")) != NULL) {
									if ((d_propXml = d_propstatXml->getChild("d:prop")) != NULL) {
										for (xml2=d_propXml->getChild(); xml2!=NULL; xml2=xml2->getNext()) {
											if (xml2->getName()->equals("r:state")) {
												char *statusString = xml2->getChild()->getName()->toString();
												subscription->setStatus(RVPClient::getStatusFromString(statusString));
												delete statusString;
											} else if (xml2->getName()->equals("d:displayname")) {
												char *nick = xml2->getData()->toString();
												subscription->setDisplayName(nick);
												delete nick;
											} else if (xml2->getName()->equals("r:email")) {
												char *email = xml2->getData()->toString();
												subscription->setEmail(email);
												delete email;
											}
										}
									}
								}
							}
						}
						delete xml;
						*/
					}
				}
				delete response;
			}
			delete node;
		}
	}
	return (result/100 == 2) ? 0 : result;
}

int RVPClient::unsubscribe(RVPSubscription *subscription) {
	int result = 1;
	if (bOnline) {
		if (subscription != NULL) {
			char *node = getUrlFromLogin(subscription->getId());
			if (node != NULL) {
				HTTPRequest *request, *response;
				request = new HTTPRequest();
				request->setMethod("UNSUBSCRIBE");
				request->setUrl(node);
				request->addHeader("Subscription-Id", subscription->getSid());
				request->addHeader("RVP-Notifications-Version", "0.2");
				request->addHeader("RVP-From-Principal", principalUrl);
				request->setCredentials(credentials);
				response = HTTPUtils::performTransaction(request);
				delete request;
				delete node;
				if (response != NULL) {
					result = response->resultCode;
					delete response;
				}
			}
			if (result/100 == 2) {
				delete subscription;
			}
		}
	}
	return (result/100 == 2) ? 0 : result;
}


int RVPClient::unsubscribe(const char *login) {
	int result = 1;
	if (bOnline && login != NULL) {
		RVPSubscription *subscription = RVPSubscription::find(login);
		if (subscription != NULL) {
			char *node = getUrlFromLogin(login);
			if (node != NULL) {
				HTTPRequest *request, *response;
				request = new HTTPRequest();
				request->setMethod("UNSUBSCRIBE");
				request->setUrl(node);
				request->addHeader("Subscription-Id", subscription->getSid());
				request->addHeader("RVP-Notifications-Version", "0.2");
				request->addHeader("RVP-From-Principal", principalUrl);
				request->setCredentials(credentials);
				response = HTTPUtils::performTransaction(request);
				delete request;
				delete node;
				if (response != NULL) {
					result = response->resultCode;
					delete response;
				}
			}
			if (result/100 == 2) {
				delete subscription;
			}
		}
	}
	return (result/100 == 2) ? 0 : result;
}

RVPSubscription * RVPClient::getProperty(const char *login, const char *property) {
	RVPSubscription *subscription = NULL;
	if (bOnline && login!=NULL) {
		char *node = getUrlFromLogin(login);
		if (node != NULL) {
			int bufferSize;
			char *buffer;
			char *utf8Message;
			HTTPRequest *request, *response;
			request = new HTTPRequest();
			request->setMethod("PROPFIND");
			request->setUrl(node);
			request->addHeader("Depth", "0");
			request->addHeader("RVP-Notifications-Version", "0.2");
			request->addHeader("RVP-From-Principal", principalUrl);
			request->addHeader("Content-Type", "text/xml");
			request->setCredentials(credentials);
			buffer=NULL;
			Utils::appendText(&buffer, &bufferSize, "<?xml version=\"1.0\"?><d:propfind xmlns:d='DAV:' xmlns:r='http://schemas.microsoft.com/rvp/'><d:prop><%s/></d:prop></d:propfind>", property);
			utf8Message = Utils::utf8Encode2(buffer);
			free(buffer);
			request->setContent(utf8Message, strlen(utf8Message));
			delete utf8Message;
			response = HTTPUtils::performTransaction(request);
			delete request;
			if (response != NULL) {
				if (response->resultCode/100 == 2) {
				//subscription = new RVPSubscription();
				//subscription->setDisplayName();
				}
				delete response;
			}
		}
	}
	return subscription;
}


/* Check who is watching us */
int RVPClient::getSubscribers() {
	int result = 1;
	if (bOnline) {
		HTTPRequest *request, *response;
		request = new HTTPRequest();
		request->setMethod("SUBSCRIPTIONS");
		request->setUrl(principalUrl);
		request->addHeader("Notification-Type", "update/propchange");
		request->addHeader("RVP-Notifications-Version", "0.2");
		request->addHeader("RVP-From-Principal", principalUrl);
		request->setCredentials(credentials);
		response = HTTPUtils::performTransaction(request);
		delete request;
		if (response != NULL) {
			result = response->resultCode;
			if (result/100 == 2) {
				JString *js = new JString(response->getContent(), response->dataLength);
				XMLObject *xml = new XMLObject();
				xml->parseXML(js);
				delete js;
				XMLObject *subscriptionsXml = xml->getChild("r:subscriptions");
				if (subscriptionsXml!=NULL) {
					for (XMLObject *subscriptionXml = subscriptionsXml->getChild("r:subscription");subscriptionXml!=NULL;subscriptionXml=subscriptionXml->getNext("r:subscription")) {
						XMLObject *subscriptionIdXml = subscriptionXml->getChild("r:subscription-id");
						XMLObject *timeoutXml = subscriptionXml->getChild("d:timeout");
						XMLObject *principalXml = subscriptionXml->getChild("a:principal");
						if (principalXml!=NULL) {
							XMLObject *rvpPrincipalXml = principalXml->getChild("a:rvp-principal");
							if (rvpPrincipalXml!=NULL) {
				//				char *jid = rvpPrincipalXml->getData()->toString();
				//							char *nick = strrchr(jid, '/')+1;
								//if ((hContact=JabberHContactFromJID(jid)) == NULL) {
									// Received roster has a new JID.
									// Add the jid (with empty resource) to Miranda contact list.
								//	hContact = JabberDBCreateContact(jid, nick, FALSE, TRUE);
								//}
				//				delete jid;
							}
						}
					}
				}
				delete xml;
			} else {
				delete response;
				return result;
			}
			delete response;
		}
	}
	return 0;
}

/* Get access control list */
int RVPClient::getACL() {
	int result = 1;
	if (bOnline) {
		HTTPRequest *request, *response;
		request = new HTTPRequest();
		request->setMethod("ACL");
		request->setUrl(principalUrl);
		request->addHeader("RVP-Notifications-Version", "0.2");
		request->addHeader("RVP-From-Principal", principalUrl);
		request->setCredentials(credentials);
		response = HTTPUtils::performTransaction(request);
		delete request;
		if (response != NULL) {
			result = response->resultCode;
			if (result/100 == 2) {
				JString *js = new JString(response->getContent(), response->dataLength);
				XMLObject *xml = new XMLObject();
				xml->parseXML(js);
				delete js;
				XMLObject *rvpaclXml, *aclXml, *aceXml, *principalXml, *rvpprincipalXml;
				rvpaclXml = xml->getChild();
				rvpaclXml = xml->getChild("a:rvpacl");
				if (rvpaclXml!=NULL) {
					aclXml = rvpaclXml->getChild();
					aclXml = rvpaclXml->getChild("a:acl");
					if (aclXml!=NULL) {
						for (aceXml=aclXml->getChild("a:ace");aceXml!=NULL;aceXml=aceXml->getNext("a:ace")) {
							principalXml = aceXml->getChild("a:principal");
							if (principalXml!=NULL) {
								rvpprincipalXml = principalXml->getChild("a:rvp-principal");
								if (rvpprincipalXml!=NULL) {
		//							char *jid = rvpprincipalXml->getData()->toString();
		//							char *nick = strrchr(jid, '/')+1;
		//							if ((hContact=Utils::contactFromID(jid)) == NULL) {
										// Received roster has a new JID.
										// Add the jid (with empty resource) to Miranda contact list.
								//		hContact = JabberDBCreateContact(jid, nick, FALSE, TRUE);
		//							}
		//							delete jid;
								}
							}
						}
					}
				}
				delete xml;
			}
			delete response;
		}
	}
	return 0;
}


int RVPClient::sendFile(RVPFile *file, const char *contactID, const char *contactDisplayname, const char *principalDisplayname) {
	int result = 1;
	if (bOnline) {
		char *node = getUrlFromLogin(contactID);
		if (node != NULL) {
			int bufferSize = 0;
			char *buffer = NULL;
			char *utf8Message;
			HTTPRequest *request, *response;
			RVPSession *session = RVPSession::get(contactID);
			request = new HTTPRequest();
			request->setMethod("NOTIFY");
			request->setUrl(node);
			request->addHeader("RVP-Ack-Type", "DeepOr");
			request->addHeader("RVP-Hop-Count", "1");
			request->addHeader("RVP-Notifications-Version", "0.2");
			request->addHeader("RVP-From-Principal", principalUrl);
			request->addHeader("Content-Type", "text/xml");
			request->setCredentials(credentials);
			/* TODO: get url from login here*/
			Utils::appendText(&buffer, &bufferSize,
			"<?xml version=\"1.0\"?>"
			"<r:notification xmlns:d='DAV:' xmlns:r='http://schemas.microsoft.com/rvp/' xmlns:a='http://schemas.microsoft.com/rvp/acl/'>"
			"<r:message><r:notification-from><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-from>"
			"<r:notification-to><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-to>"
			"<r:msgbody><r:mime-data><![CDATA[MIME-Version: 1.0\r\n"
			"Content-Type: text/x-msmsgsinvite; charset=UTF-8\r\n"
			"Session-Id: %s\r\n\r\n"
			"Application-Name: File Transfer\r\n"
			"Application-GUID: {5D3E02AB-6190-11d3-BBBB-00C04F795683}\r\n"
			"Invitation-Command: INVITE\r\n"
			"Invitation-Cookie: %s\r\n"
			"Application-File: %s\r\n"
			"Application-FileSize: %d\r\n\r\n]]></r:mime-data></r:msgbody></r:message></r:notification>",
			principalUrl, principalDisplayname, node, contactDisplayname, session->getSid(), file->getCookie(), file->getFile(), file->getSize());
			utf8Message = Utils::utf8Encode2(buffer);
			free(buffer);
			request->setContent(utf8Message, strlen(utf8Message));
			delete utf8Message;
			response = HTTPUtils::performTransaction(request);
			delete request;
			delete node;
			if (response != NULL) {
				result = response->resultCode;
				delete response;
			}
		}
	}
	return (result/100 == 2) ? 0 : result;
}

int RVPClient::sendFileAcceptResponse(RVPFile *file, const char *contactID, const char *contactDisplayname, const char *principalDisplayname) {
	int result = 1;
	if (bOnline) {
		char *node = getUrlFromLogin(contactID);
		if (node != NULL) {
			int bufferSize = 0;
			char *buffer = NULL;
			char *utf8Message;
			HTTPRequest *request, *response;
			RVPSession *session = RVPSession::get(contactID);
			request = new HTTPRequest();
			request->setMethod("NOTIFY");
			request->setUrl(node);
			request->addHeader("RVP-Ack-Type", "DeepOr");
			request->addHeader("RVP-Hop-Count", "1");
			request->addHeader("RVP-Notifications-Version", "0.2");
			request->addHeader("RVP-From-Principal", principalUrl);
			request->addHeader("Content-Type", "text/xml");
			request->setCredentials(credentials);
			/* TODO: get url from login here*/
			Utils::appendText(&buffer, &bufferSize,
			"<?xml version=\"1.0\"?>"
			"<r:notification xmlns:d='DAV:' xmlns:r='http://schemas.microsoft.com/rvp/' xmlns:a='http://schemas.microsoft.com/rvp/acl/'>"
			"<r:message><r:notification-from><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-from>"
			"<r:notification-to><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-to>"
			"<r:msgbody><r:mime-data><![CDATA[MIME-Version: 1.0\r\n"
			"Content-Type: text/x-msmsgsinvite; charset=UTF-8\r\n"
			"Session-Id: %s\r\n\r\n"
			"Invitation-Command: ACCEPT\r\n"
			"Invitation-Cookie: %s\r\n"
			"IP-Address: %s\r\n"
			"Port: %d\r\n"
			"AuthCookie: %s\r\n"
			"Launch-Application: FALSE\r\n"
			"Request-Data: IP-Address:\r\n\r\n]]></r:mime-data></r:msgbody></r:message></r:notification>",
			principalUrl, principalDisplayname, node, contactDisplayname, session->getSid(), file->getCookie(),
			file->getHost(), file->getPort(), file->getAuthCookie());
			utf8Message = Utils::utf8Encode2(buffer);
			free(buffer);
			request->setContent(utf8Message, strlen(utf8Message));
			delete utf8Message;
			response = HTTPUtils::performTransaction(request);
			delete request;
			delete node;
			if (response != NULL) {
				result = response->resultCode;
				delete response;
			}
		}
	}
	return (result/100 == 2) ? 0 : result;
}


int RVPClient::sendFileAccept(RVPFile *file, const char *contactID, const char *contactDisplayname, const char *principalDisplayname) {
	int result = 1;
	if (bOnline) {
		char *node = getUrlFromLogin(contactID);
		if (node != NULL) {
			int bufferSize = 0;
			char *buffer = NULL;
			char *utf8Message;
			HTTPRequest *request, *response;
			RVPSession *session = RVPSession::get(contactID);
			request = new HTTPRequest();
			request->setMethod("NOTIFY");
			request->setUrl(node);
			request->addHeader("RVP-Ack-Type", "DeepOr");
			request->addHeader("RVP-Hop-Count", "1");
			request->addHeader("RVP-Notifications-Version", "0.2");
			request->addHeader("RVP-From-Principal", principalUrl);
			request->addHeader("Content-Type", "text/xml");
			request->setCredentials(credentials);
			/* TODO: get url from login here*/
			Utils::appendText(&buffer, &bufferSize,
			"<?xml version=\"1.0\"?>"
			"<r:notification xmlns:d='DAV:' xmlns:r='http://schemas.microsoft.com/rvp/' xmlns:a='http://schemas.microsoft.com/rvp/acl/'>"
			"<r:message><r:notification-from><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-from>"
			"<r:notification-to><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-to>"
			"<r:msgbody><r:mime-data><![CDATA[MIME-Version: 1.0\r\n"
			"Content-Type: text/x-msmsgsinvite; charset=UTF-8\r\n"
			"Session-Id: %s\r\n\r\n"
			"Invitation-Command: ACCEPT\r\n"
			"Invitation-Cookie: %s\r\n"
			"Launch-Application: FALSE\r\n"
			"Request-Data: IP-Address:\r\n\r\n]]></r:mime-data></r:msgbody></r:message></r:notification>",
			principalUrl, principalDisplayname, node, contactDisplayname, session->getSid(), file->getCookie());
			utf8Message = Utils::utf8Encode2(buffer);
			free(buffer);
			request->setContent(utf8Message, strlen(utf8Message));
			delete utf8Message;
			response = HTTPUtils::performTransaction(request);
			delete request;
			delete node;
			if (response != NULL) {
				result = response->resultCode;
				delete response;
			}
		}
	}
	return (result/100 == 2) ? 0 : result;
}

int RVPClient::sendFileReject(RVPFile *file, const char *contactID, const char *contactDisplayname, const char *principalDisplayname) {
	int result = 1;
	if (bOnline) {
		char *node = getUrlFromLogin(contactID);
		if (node != NULL) {
			int bufferSize = 0;
			char *buffer = NULL;
			char *utf8Message;
			HTTPRequest *request, *response;
			RVPSession *session = RVPSession::get(contactID);
			request = new HTTPRequest();
			request->setMethod("NOTIFY");
			request->setUrl(node);
			request->addHeader("RVP-Ack-Type", "DeepOr");
			request->addHeader("RVP-Hop-Count", "1");
			request->addHeader("RVP-Notifications-Version", "0.2");
			request->addHeader("RVP-From-Principal", principalUrl);
			request->addHeader("Content-Type", "text/xml");
			request->setCredentials(credentials);
			/* TODO: get url from login here*/
			Utils::appendText(&buffer, &bufferSize,
			"<?xml version=\"1.0\"?>"
			"<r:notification xmlns:d='DAV:' xmlns:r='http://schemas.microsoft.com/rvp/' xmlns:a='http://schemas.microsoft.com/rvp/acl/'>"
			"<r:message><r:notification-from><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-from>"
			"<r:notification-to><r:contact><d:href>%s</d:href>"
			"<r:description>%s</r:description></r:contact></r:notification-to>"
			"<r:msgbody><r:mime-data><![CDATA[MIME-Version: 1.0\r\n"
			"Content-Type: text/x-msmsgsinvite; charset=UTF-8\r\n"
			"Session-Id: %s\r\n\r\n"
			"Invitation-Command: CANCEL\r\n"
			"Invitation-Cookie: %s\r\n"
			"Cancel-Code: REJECT\r\n\r\n]]></r:mime-data></r:msgbody></r:message></r:notification>",
			principalUrl, principalDisplayname, node, contactDisplayname, session->getSid(), file->getCookie());
			utf8Message = Utils::utf8Encode2(buffer);
			free(buffer);
			request->setContent(utf8Message, strlen(utf8Message));
			delete utf8Message;
			response = HTTPUtils::performTransaction(request);
			delete request;
			delete node;
			if (response != NULL) {
				result = response->resultCode;
				delete response;
			}
		}
	}
	return (result/100 == 2) ? 0 : result;
}


const char *RVPClient::getCallback() {
	return callbackHost;
}


const char *RVPClient::getSignInName() {
	return signInName;
}


bool RVPClient::isOnline() {
	return bOnline;
}


List* RVPClient::getSubscriptions() {
	return RVPSubscription::getSubscriptions();
}

RVPSubscription* RVPClient::getSubscription(const char *login) {
	return RVPSubscription::find(login);
}
