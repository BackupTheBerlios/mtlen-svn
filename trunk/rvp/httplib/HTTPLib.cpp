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
#include "HTTPLib.h"
#include "../Utils.h"
#include "MD5.h"

#include "../JLogger.h"
static	JLogger *logger = new JLogger("h:/httplib.log");

static PSecurityFunctionTable pSecurityFunctions=NULL;
static HINSTANCE hInstSecurityDll=LoadLibrary("security.dll");

HTTPCredentials::HTTPCredentials() {
	domain = NULL;
	username = NULL;
	password = NULL;
	flags = 0;
}

HTTPCredentials::HTTPCredentials(const char *domain, const char *username, const char *password, int flags) {
	this->domain = Utils::dupString(domain);
	this->username = Utils::dupString(username);
	this->password = Utils::dupString(password);
	this->flags = flags;
}

HTTPCredentials::~HTTPCredentials() {
	if (domain!=NULL) delete domain;
	if (username!=NULL) delete username;
	if (password!=NULL) delete password;
}

void HTTPCredentials::setDomain(const char *s) {
	if (this->domain!=NULL) delete this->domain;
	this->domain = Utils::dupString(s);
}

const char *HTTPCredentials::getDomain() {
	return domain;
}

void HTTPCredentials::setUsername(const char *s) {
	if (this->username!=NULL) delete this->username;
	this->username = Utils::dupString(s);
}

const char *HTTPCredentials::getUsername() {
	return username;
}

void HTTPCredentials::setPassword(const char *s) {
	if (this->password!=NULL) delete this->password;
	this->password = Utils::dupString(s);
}

const char *HTTPCredentials::getPassword() {
	return password;
}

void HTTPCredentials::setFlags(int f) {
	flags = f;
}

int HTTPCredentials::getFlags() {
	return flags;
}

HTTPHeader::HTTPHeader(const char *name, const char *value) {
	this->name = Utils::dupString(name);
	this->value = Utils::dupString(value);
	this->next = NULL;
}

HTTPHeader::~HTTPHeader() {
	if (name!=NULL) delete name;
	if (value!=NULL) delete value;
	if (next!=NULL) delete next;
}

const char *HTTPHeader::getName() {
	return name;
}

const char *HTTPHeader::getValue() {
	return value;
}

HTTPHeader * HTTPHeader::getAttributes(int offset) {
	HTTPHeader *attributes = NULL;
	int l, j, pos[2];
	char *vc = Utils::dupString(value);
	l = strlen(vc);
	pos[0]=offset;
	j = 1;
	for (;offset<l+1;offset++) {
		if ((vc[offset]==',' || vc[offset]=='\0') && j==2) {
			vc[offset]='\0';
			Utils::trim(vc+pos[0], " \t\"");
			Utils::trim(vc+pos[1], " \t\"");
			HTTPHeader *attribute = new HTTPHeader(vc+pos[0], vc+pos[1]);
			attribute->next = attributes;
			attributes = attribute;
			pos[0]=offset+1;
			j=1;
		}
		if (vc[offset]=='=' && j==1) {
			vc[offset]='\0';
			pos[j++]=offset+1;
		}
	}
	delete vc;
	return attributes;
}

HTTPHeader * HTTPHeader::get(const char *name) {
	for (HTTPHeader *ptr=this;ptr!=NULL;ptr=ptr->next) {
		if (!strcmp(ptr->name, name)) return ptr;
	}
	return NULL;
}


//free() the return value
void HTTPRequest::NtlmDestroy() {
	if (pSecurityFunctions)
	{
		pSecurityFunctions->DeleteSecurityContext(&hNtlmClientContext);
		pSecurityFunctions->FreeCredentialsHandle(&hNtlmClientCredential);
	} //if
	if(ntlmSecurityPackageInfo) pSecurityFunctions->FreeContextBuffer(ntlmSecurityPackageInfo);
	ntlmSecurityPackageInfo=NULL;
}

char *HTTPRequest::NtlmInitialiseAndGetDomainPacket(HINSTANCE hInstSecurityDll) {
	PSecurityFunctionTable (*MyInitSecurityInterface)(VOID);
	SECURITY_STATUS securityStatus;
	SecBufferDesc outputBufferDescriptor;
	SecBuffer outputSecurityToken;
	TimeStamp tokenExpiration;
	ULONG contextAttributes;
	SEC_WINNT_AUTH_IDENTITY authIdentity;

	MyInitSecurityInterface=(PSecurityFunctionTable (*)(VOID))GetProcAddress(hInstSecurityDll,SECURITY_ENTRYPOINT);
	if(MyInitSecurityInterface==NULL) {NtlmDestroy(); return NULL;}
	pSecurityFunctions=MyInitSecurityInterface();
	if(pSecurityFunctions==NULL) {NtlmDestroy(); return NULL;}

	securityStatus=pSecurityFunctions->QuerySecurityPackageInfo("NTLM",&ntlmSecurityPackageInfo);
	if(securityStatus!=SEC_E_OK) {NtlmDestroy(); return NULL;}
	if (getCredentials()->getFlags() & HTTPCredentials::FLAG_MANUAL_NTLM) {
		/* use specified credentials */
		memset((void *)&authIdentity, 0, sizeof(authIdentity));
		authIdentity.Domain = (unsigned char *) getCredentials()->getDomain();
		authIdentity.DomainLength = strlen((char *)authIdentity.Domain);
		authIdentity.User = (unsigned char *) getCredentials()->getUsername();
		authIdentity.UserLength = strlen((char *)authIdentity.User);
		authIdentity.Password = (unsigned char *) getCredentials()->getPassword();
		authIdentity.PasswordLength = strlen((char *)authIdentity.Password);
		authIdentity.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
		securityStatus=pSecurityFunctions->AcquireCredentialsHandle(NULL,"NTLM",SECPKG_CRED_OUTBOUND,NULL,&authIdentity,NULL,NULL,&hNtlmClientCredential,&tokenExpiration);
	} else {
		/* use default credentials */
		securityStatus=pSecurityFunctions->AcquireCredentialsHandle(NULL,"NTLM",SECPKG_CRED_OUTBOUND,NULL,NULL,NULL,NULL,&hNtlmClientCredential,&tokenExpiration);
	}
	if(securityStatus!=SEC_E_OK) {NtlmDestroy(); return NULL;}

	outputBufferDescriptor.cBuffers=1;
	outputBufferDescriptor.pBuffers=&outputSecurityToken;
	outputBufferDescriptor.ulVersion=SECBUFFER_VERSION;
	outputSecurityToken.BufferType=SECBUFFER_TOKEN;
	outputSecurityToken.cbBuffer=ntlmSecurityPackageInfo->cbMaxToken;
	outputSecurityToken.pvBuffer=malloc(outputSecurityToken.cbBuffer);
	if(outputSecurityToken.pvBuffer==NULL) {NtlmDestroy(); SetLastError(ERROR_OUTOFMEMORY); return NULL;}
	securityStatus=pSecurityFunctions->InitializeSecurityContext(&hNtlmClientCredential,NULL,NULL,0,0,SECURITY_NATIVE_DREP,NULL,0,&hNtlmClientContext,&outputBufferDescriptor,&contextAttributes,&tokenExpiration);
	if(securityStatus<0) {free(outputSecurityToken.pvBuffer); NtlmDestroy(); return NULL;}
	if (securityStatus==SEC_I_COMPLETE_NEEDED || securityStatus==SEC_I_COMPLETE_AND_CONTINUE) {
		securityStatus=pSecurityFunctions->CompleteAuthToken(&hNtlmClientContext,&outputBufferDescriptor);
		if(securityStatus<0) {free(outputSecurityToken.pvBuffer); NtlmDestroy(); return NULL;}
	}
	char *base64Encoded = Utils::base64Encode((char *)outputSecurityToken.pvBuffer, outputSecurityToken.cbBuffer);
	if(base64Encoded==NULL) {free(outputSecurityToken.pvBuffer); NtlmDestroy(); return NULL;}
	free(outputSecurityToken.pvBuffer);
	return base64Encoded;
}

char *HTTPRequest::NtlmCreateResponseFromChallenge(char *szChallenge) {
	SECURITY_STATUS securityStatus;
	SecBufferDesc outputBufferDescriptor,inputBufferDescriptor;
	SecBuffer outputSecurityToken,inputSecurityToken;
	TimeStamp tokenExpiration;
	ULONG contextAttributes;
	int base64DecodedLen;
	char *base64Decoded = Utils::base64Decode(szChallenge, &base64DecodedLen);
	if(base64Decoded==NULL) {return NULL;}

	inputBufferDescriptor.cBuffers=1;
	inputBufferDescriptor.pBuffers=&inputSecurityToken;
	inputBufferDescriptor.ulVersion=SECBUFFER_VERSION;
	inputSecurityToken.BufferType=SECBUFFER_TOKEN;
	inputSecurityToken.cbBuffer=base64DecodedLen;
	inputSecurityToken.pvBuffer=base64Decoded;
	outputBufferDescriptor.cBuffers=1;
	outputBufferDescriptor.pBuffers=&outputSecurityToken;
	outputBufferDescriptor.ulVersion=SECBUFFER_VERSION;
	outputSecurityToken.BufferType=SECBUFFER_TOKEN;
	outputSecurityToken.cbBuffer=ntlmSecurityPackageInfo->cbMaxToken;
	outputSecurityToken.pvBuffer=malloc(outputSecurityToken.cbBuffer);
	if(outputSecurityToken.pvBuffer==NULL) {delete base64Decoded; SetLastError(ERROR_OUTOFMEMORY); return NULL;}
	securityStatus=pSecurityFunctions->InitializeSecurityContext(&hNtlmClientCredential,&hNtlmClientContext,NULL,0,0,SECURITY_NATIVE_DREP,&inputBufferDescriptor,0,&hNtlmClientContext,&outputBufferDescriptor,&contextAttributes,&tokenExpiration);
	delete base64Decoded;
	if(securityStatus!=SEC_E_OK) {free(outputSecurityToken.pvBuffer); return NULL;}
	char *base64Encoded = Utils::base64Encode((char *)outputSecurityToken.pvBuffer, outputSecurityToken.cbBuffer);
	if(base64Encoded==NULL) {free(outputSecurityToken.pvBuffer); NtlmDestroy(); return NULL;}
	free(outputSecurityToken.pvBuffer);
	return base64Encoded;
}

HTTPRequest::HTTPRequest() {
	flags = 0;
	url = NULL;
	host = NULL;
	uri = NULL;
	headers = NULL;
	method = NULL;
	headersCount = 0;
	pData = NULL;
	dataLength = 0;
	resultCode = 0;
	credentials = NULL;
	ntlmSecurityPackageInfo = NULL;
}

HTTPRequest::~HTTPRequest() {
	if (headers!=NULL) delete headers;
	if (url!=NULL) delete url;
	if (uri!=NULL) delete uri;
	if (host!=NULL) delete host;
	if (method!=NULL) delete method;
	if (pData!=NULL) delete pData;

}

void HTTPRequest::setMethod(const char *t) {
	if (this->method!=NULL) delete this->method;
	this->method = Utils::dupString(t);
}

const char *HTTPRequest::getMethod() {
	return method;
}

void HTTPRequest::setUrl(const char *url) {
	const char *phost;
	char *puri, *pcolon;

	if (this->url!=NULL) delete this->url;
	this->url = Utils::dupString(url);
	if (this->host!=NULL) delete this->host;
	if (this->uri!=NULL) delete this->uri;

	phost = strstr(url,"://");
	if (phost==NULL) phost = url;
	else phost+=3;
	host = Utils::dupString(phost);
	puri=strchr(host,'/');
	if (puri) {
		uri = Utils::dupString(puri);
		*puri='\0';
	} else {
		uri = Utils::dupString("/");
	}
	pcolon=strrchr(host,':');
	if(pcolon) {
		*pcolon='\0';
		wPort=(WORD)strtol(pcolon+1, NULL, 10);
	}
	else wPort=80;

}
const char *HTTPRequest::getUrl() {
	return url;
}
int HTTPRequest::getPort() {
	return wPort;
}
const char *HTTPRequest::getUri() {
	return uri;
}
const char *HTTPRequest::getHost() {
	return host;
}

void HTTPRequest::setCredentials(HTTPCredentials * c) {
	credentials = c;
}

HTTPCredentials * HTTPRequest::getCredentials() {
	return credentials;
}


void HTTPRequest::setContent(const char *t, int len) {
	if (this->pData!=NULL) delete this->pData;
	this->pData = new char[len];
	memcpy(this->pData, t, len);
	this->dataLength = len;
}

void HTTPRequest::setContent(const char *t) {
	int len = strlen(t);
	if (this->pData!=NULL) delete this->pData;
	this->pData = new char[len];
	memcpy(this->pData, t, len);
	this->dataLength = len;
}

const char *HTTPRequest::getContent() {
	return pData;
}


void HTTPRequest::addHeader(const char *name, const char *value) {
	HTTPHeader *header = new HTTPHeader(name, value);
	if (headers==NULL) {
		headers = header;
	} else {
		HTTPHeader *ptr;
		for (ptr = headers;ptr->next!=NULL; ptr=ptr->next);
		ptr->next = header;
	}
//	header->next = headers;
	headersCount++;
}

void HTTPRequest::removeHeader(const char *name) {
	for (HTTPHeader *ptr=headers, *lastPtr=NULL;ptr!=NULL;) {
		if (!strcmp(ptr->name, name)) {
			if (lastPtr==NULL) {
				headers=ptr->next;
				ptr->next = NULL;
				delete ptr;
				ptr = headers;
			} else {
				lastPtr->next=ptr->next;
				ptr->next = NULL;
				delete ptr;
				ptr = lastPtr->next;
			}
		} else {
			lastPtr=ptr;
			ptr=ptr->next;
		}
	}
}

HTTPHeader *HTTPRequest::getHeaders() {
	return headers;
}


HTTPHeader *HTTPRequest::getHeader(const char *name) {
	for (HTTPHeader *header = headers; header!=NULL; header=header->next) {
		if (!strcmpi(header->name, name)) return header;
	}
	return NULL;
}

void HTTPRequest::addAutoRequestHeaders() {
	bool isContentLen = false, isHost = false;
	for (HTTPHeader *header = headers; header!=NULL; header=header->next) {
		if (!strcmpi(header->name, "Host")) isHost = true;
		if (!strcmpi(header->name, "Content-Length")) isContentLen = true;
	}
	if (!isHost) {
		addHeader("Host", host);
	}
	if (!isContentLen) {
		char str[64];
		sprintf(str, "%d", dataLength);
		addHeader("Content-Length", str);
	}
}

void HTTPRequest::addAutoResponseHeaders() {
	bool isContentLen = false;
	for (HTTPHeader *header = headers; header!=NULL; header=header->next) {
		if (!strcmpi(header->name, "Content-Length")) isContentLen = true;
	}
	if (!isContentLen) {
		char str[64];
		sprintf(str, "%d", dataLength);
		addHeader("Content-Length", str);
	}
}


char * HTTPRequest::toStringReq() {
	int outputSize = 0;
	char *output = NULL;
	Utils::appendText(&output, &outputSize, "%s %s HTTP/1.1\n", method, uri);
	for (HTTPHeader *header = headers; header!=NULL; header=header->next) {
		Utils::appendText(&output, &outputSize, "%s: %s\n", header->name, header->value);
	}
	Utils::appendText(&output, &outputSize, "\n");
	return output;
}

char * HTTPRequest::toStringRsp() {
	int outputSize = 0;
	char *output = NULL;
	Utils::appendText(&output, &outputSize, "HTTP/1.1 %03d %s\n", resultCode, (resultCode/100==2) ? "Successful" :"Error");
	for (HTTPHeader *header = headers; header!=NULL; header=header->next) {
		Utils::appendText(&output, &outputSize, "%s: %s\n", header->name, header->value);
	}
	Utils::appendText(&output, &outputSize, "\n");
	return output;
}

bool HTTPRequest::authBasic(HTTPHeader *header) {
	if (getCredentials()->getUsername()==NULL || getCredentials()->getPassword()==NULL) {
		return false;
	}
	for (; header!=NULL; header=header->next) {
		if (!strcmpi(header->name, "WWW-Authenticate")) {
			if (!strncmp("Basic", header->value, 5)) {
				int resultSize =0;
				char *result=NULL;
				if (getCredentials()->getDomain()!=NULL) {
					Utils::appendText(&result, &resultSize, "%s/%s:%s",getCredentials()->getDomain(),getCredentials()->getUsername(), getCredentials()->getPassword());
				} else {
					Utils::appendText(&result, &resultSize, "%s:%s",getCredentials()->getUsername(), getCredentials()->getPassword());
				}
				char *base64hash = Utils::base64Encode(result, strlen(result));
				free(result);
				result=NULL;
				Utils::appendText(&result, &resultSize, "Basic %s",base64hash);
				delete base64hash;
				addHeader("Authorization", result);
				free(result);
				return true;
			}
		}
	}
	return false;
}


bool HTTPRequest::authDigest(HTTPHeader *header) {
	if (getCredentials()->getUsername()==NULL || getCredentials()->getPassword()==NULL) {
		return false;
	}
	for (; header!=NULL; header=header->next) {
		if (!strcmpi(header->name, "WWW-Authenticate")) {
			 if (!strncmp("Digest", header->value, 6)) {
				HTTPHeader *hAttributes = header->getAttributes(7);
				if (hAttributes!=NULL) {
					HTTPHeader *hRealm = hAttributes->get("realm");
					HTTPHeader *hNonce = hAttributes->get("nonce");
					HTTPHeader *hQop = hAttributes->get("qop");
					char *nc = "00000001";
					char cnonce[33];
					for (int i=0;i<32;i++) {
						int v = (rand()>>2)%0x0F;
						if (v>9) cnonce[i] = 'a' + v - 10;
						else cnonce[i] = '0' + v;
					}
					cnonce[32]='\0';
					if (hRealm == NULL || hNonce == NULL) {
						return false;
					}
					char hashedDigest[33];
					int tSize =0;
					char ha1[33], ha2[33];
					MD5 md5;
					md5.init();
					if (getCredentials()->getDomain()!=NULL) {
						md5.update((unsigned char *)getCredentials()->getDomain(), strlen(getCredentials()->getDomain()));
						md5.update((unsigned char *)"/", 1);
					}
					md5.update((unsigned char *)getCredentials()->getUsername(), strlen(getCredentials()->getUsername()));
					md5.update((unsigned char *)":", 1);
					md5.update((unsigned char *)hRealm->value, strlen(hRealm->value));
					md5.update((unsigned char *)":", 1);
					md5.update((unsigned char *)getCredentials()->getPassword(), strlen(getCredentials()->getPassword()));
					md5.finalize();
					md5.getHex(ha1);

					md5.init();
					md5.update((unsigned char *)getMethod(), strlen(getMethod()));
//									md5.update((unsigned char *)"SUBSCRIBE", strlen("SUBSCRIBE"));
					md5.update((unsigned char *)":", 1);
					md5.update((unsigned char *)getUri(), strlen(getUri()));
					md5.finalize();
					md5.getHex(ha2);

					md5.init();
					md5.update((unsigned char *)ha1, 32);
					md5.update((unsigned char *)":", 1);
					md5.update((unsigned char *)hNonce->value, strlen(hNonce->value));
					if (hQop) {
						md5.update((unsigned char *)":", 1);
						md5.update((unsigned char *)nc, strlen(nc));
						md5.update((unsigned char *)":", 1);
						md5.update((unsigned char *)cnonce, strlen(cnonce));
						md5.update((unsigned char *)":", 1);
						md5.update((unsigned char *)hQop->value, strlen(hQop->value));
					}
					md5.update((unsigned char *)":", 1);
					md5.update((unsigned char *)ha2, strlen(ha2));
					md5.finalize();
					md5.getHex(hashedDigest);

					int resultSize =0;
					char *result=NULL;
					Utils::appendText(&result, &resultSize, "Digest ");
					if (getCredentials()->getDomain()!=NULL) {
						Utils::appendText(&result, &resultSize, "username=\"%s/%s\", ",getCredentials()->getDomain(), getCredentials()->getUsername());
					} else {
						Utils::appendText(&result, &resultSize, "username=\"%s\", ",getCredentials()->getUsername());
					}
					Utils::appendText(&result, &resultSize, "realm=\"%s\", ",hRealm->value);
					Utils::appendText(&result, &resultSize, "algorithm=\"MD5\", ");
					Utils::appendText(&result, &resultSize, "uri=\"%s\", ",getUri());
					Utils::appendText(&result, &resultSize, "nonce=\"%s\", ",hNonce->value);
					if (hQop) {
						Utils::appendText(&result, &resultSize, "qop=\"%s\", ",hQop->value);
						Utils::appendText(&result, &resultSize, "nc=\"%s\", ",nc);
						Utils::appendText(&result, &resultSize, "cnonce=\"%s\", ",cnonce);
					}
					Utils::appendText(&result, &resultSize, "response=\"%s\"",hashedDigest);
					addHeader("Authorization", result);
					free(result);
					return true;
				}
			}
		}
	}
	return false;
}

bool HTTPRequest::authNTLM(HTTPHeader *header) {
	for (; header!=NULL; header=header->next) {
		if (!strcmpi(header->name, "WWW-Authenticate")) {
			if (!strncmp("NTLM", header->value, 4)) {
				authRejectsMax = 2;
				if (strlen(header->value)==4) {
					char *ntlmToken = NtlmInitialiseAndGetDomainPacket(hInstSecurityDll);
					int resultSize =0;
					char *result=NULL;
					Utils::appendText(&result, &resultSize, "NTLM %s", ntlmToken);
					addHeader("Authorization", result);
					free(result);
					delete ntlmToken;
					return true;
				} else {
					char *ntlmToken = NtlmCreateResponseFromChallenge(header->value + 5);
					int resultSize =0;
					char *result=NULL;
					Utils::appendText(&result, &resultSize, "NTLM %s", ntlmToken);
					delete ntlmToken;
					addHeader("Authorization", result);
					free(result);
					return true;
				}
				break;
			}
		}
	}
	return false;
}


char *HTTPUtils::readLine(Connection *con)
{
	int i, lineSize = 0;
	char *line = NULL;
	for (i=0;;i++) {
		if (i>=lineSize) {
			line = (char *)realloc(line, lineSize+128);
			lineSize += 128;
		}
		if (con->recv(line+i, 1) != 0) {
			if (line[i]=='\r') {
				i--;
			}
			if (line[i]=='\n') {
				break;
			}
		} else {
			free(line);
			return NULL;
		}

	}
	line[i]='\0';
	return line;
}


HTTPRequest *HTTPUtils::toRequest(const char *text)  {
	char *line;
	int len;
	HTTPHeader *header = NULL;
	HTTPRequest *request = new HTTPRequest();
	for (len=0;;text+=len) {
		char *pColon;
		line = Utils::getLine(text, &len);
		if (line == NULL) {
			break;
		}
		if (line[0]=='\0') {
			text += len;
			break;
		}
		pColon=strchr(line,':');
		if (pColon == NULL) {
			text += len;
			break;
		}
		*pColon = '\0';
		Utils::trim(line, " \t");
		Utils::trim(pColon+1, " \t");
		request->addHeader(line, pColon+1);
		delete line;
	}
	if (line !=NULL) delete line;
	if (*text != '\0') {
		request->setContent(text, strlen(text)+1);
	}
	return request;
}

HTTPRequest *HTTPUtils::recvHeaders(Connection *con)  {
	int i;
	char *line;
	HTTPHeader *header = NULL;
	HTTPRequest *request = new HTTPRequest();
	request->resultCode = 500;
	for (i=0;;i++) {
		char *pColon;
		line = readLine(con);
		if (line == NULL) {
			break;
		}
		if (i==0) {
			if (strlen(line) > 11 && !strncmp(line, "HTTP/", 5)) {
				line[12]='\0';
				request->resultCode = atoi(line + 9);
			} else if (!strncmp(line, "GET", 3)) {
				request->setMethod("GET");
				request->setUrl(line + 3);
			} else if (!strncmp(line, "POST", 4)) {
				request->setMethod("POST");
				request->setUrl(line + 4);
			} else if (!strncmp(line, "SUBSCRIBE", 9)) {
				request->setMethod("SUBSCRIBE");
				request->setUrl(line + 9);
			} else if (!strncmp(line, "UNSUBSCRIBE", 11)) {
				request->setMethod("UNSUBSCRIBE");
				request->setUrl(line + 9);
			} else if (!strncmp(line, "NOTIFY", 6)) {
				request->setMethod("NOTIFY");
				request->setUrl(line + 6);
			} else {
				free(line);
				break;
			}
		} else {
			if (line[0]=='\0') {
				free(line);
				break;
			}
			pColon=strchr(line,':');
			if (pColon != NULL) {
				*pColon = '\0';
				Utils::trim(line, " \t");
				Utils::trim(pColon+1, " \t");
				request->addHeader(line, pColon+1);
			} else {
				break;
			}
		}
		free(line);
	}
	return request;
}

/*
 * Add auto headers, send request and receive response.
 */

HTTPRequest *HTTPUtils::performRequest(Connection *con, HTTPRequest *request)  {
	HTTPRequest *response = NULL;
	if (con!=NULL) {
		request->addAutoRequestHeaders();
		char *str = request->toStringReq();
		logger->info("REQUEST:\n%s\n", str);
		if (con->send(str)) {
			if (con->send(request->getContent(), request->dataLength)) {
				response = recvHeaders(con);
				for (HTTPHeader *header=response->getHeaders(); header!=NULL; header=header->next) {
					if (!strcmpi(header->name, "Content-Length")) {
						int len = atol(header->value);
						char *data = new char[len+1];
						for (int l=0;l<len;) {
							int j = con->recv(data+l, len-l);
							if (j<=0) {
								break;
							}
							l+=j;
						}
						data[len]='\0';
						response->setContent(data, len);
						logger->info("RESPONSE:\n%s\n", data);
						delete data;
					}
				}
			}
		}
		free(str);
	}
	return response;
}


HTTPRequest *HTTPUtils::recvRequest(Connection *con)  {
	HTTPRequest *response = NULL;
	if (con!=NULL) {
		response = recvHeaders(con);
		for (HTTPHeader *header=response->getHeaders(); header!=NULL; header=header->next) {
			if (!strcmpi(header->name, "Content-Length")) {
				int len = atol(header->value);
				char *data = new char[len+1];
				for (int l=0;l<len;) {
					int j = con->recv(data+l, len-l);
					if (j<=0) {
						break;
					}
					l+=j;
				}
				data[len]='\0';
				response->setContent(data, len);
				delete data;
			}
		}
	}
	return response;
}


int HTTPUtils::sendResponse(Connection *con, HTTPRequest *response)  {
	int result = 500;
	if (con!=NULL) {
		response->addAutoResponseHeaders();
		char *str = response->toStringRsp();
		if (con->send(str)) {
			if (con->send(response->getContent(), response->dataLength)) {
				result = 0;
			}
		}
		free(str);
	}
	return result;
}



HTTPRequest *HTTPUtils::performTransaction(HTTPRequest *request)  {
	char *str;
	HTTPRequest *response = NULL;
	int authRejects = 0;
	Connection *con = NULL;
	request->keepAlive = false;
	request->authRejectsMax = 1;
	request->authRejects = 0;

	str = request->toStringReq();
	free(str);
	while (1) {
		if (con == NULL) {
			con = new Connection(DEFAULT_CONNECTION_POOL);
			con->connect(request->getHost(), request->getPort());
		}
		response = HTTPUtils::performRequest(con, request);
		if (response != NULL) {
			/* Look for important headers: Connection, */
			request->keepAlive = true;
			for (HTTPHeader *header=response->getHeaders(); header!=NULL; header=header->next) {
				if (!strcmpi(header->name, "Connection")) {
					if (!strcmpi(header->value, "close")) {
						request->keepAlive = false;
					}
				}
			}
			if (response->resultCode == 302) {
				HTTPHeader *header;
				for (header=response->getHeaders(); header!=NULL; header=header->next) {
					if (!strcmpi(header->name, "Location")) {
						request->removeHeader("Host");
						request->setUrl(header->value);
						request->keepAlive = false;
						break;
					}
				}
				if (header==NULL) {
					// no destination location
					break;
				}
			} else if (response->resultCode == 401) {
				request->removeHeader("Authorization");
				if (request->getCredentials()==NULL) break;
				if (request->authRejects > request->authRejectsMax) break;
				request->authRejects++;
				if (!request->authNTLM(response->getHeaders())) {
					if (!request->authDigest(response->getHeaders())) {
						if (!request->authBasic(response->getHeaders())) {
							break;
						}
					}
				}
			} else if (response->resultCode/100 == 2) {
				break;
			} else {
				break;
			}
			delete response;
			if (!request->keepAlive) {
				delete con;
				con = NULL;
			}
		} else {
			break;
		}
	}
	if (con != NULL) {
		delete con;
	}
	return response;

}

