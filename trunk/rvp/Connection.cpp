#include "Connection.h"

Connection::NetLibUser* Connection::pools = NULL;

void Connection::init(const char * poolName, const char *protoName, const char *poolDescription) {
	NETLIBUSER nlu = {0};
	NETLIBUSERSETTINGS nlus = {0};
//	char name[256];
//	sprintf(name, "%s %s", moduleName, Translate("connection"));
	nlu.cbSize = sizeof(nlu);
	nlu.flags = NUF_OUTGOING | NUF_INCOMING | NUF_HTTPCONNS;	// | NUF_HTTPGATEWAY;
	nlu.szDescriptiveName = (char *)poolDescription;
	nlu.szSettingsModule = (char *)protoName;
	HANDLE hNetLibUser = (HANDLE) CallService(MS_NETLIB_REGISTERUSER, 0, (LPARAM) &nlu);
	if (hNetLibUser != NULL) {
		NetLibUser *pool = new NetLibUser();
		pool->name = Utils::dupString(poolName);
		pool->hNetLibUser = hNetLibUser;
		pool->next = pools;
		pools = pool;
	}
}

Connection::Connection(const char *poolName) {
	this->hNetLibUser = getNetLibUser(poolName);
	eof = false;
	bufferPos = 0;
	bufferLen = 0;
	sock = NULL;
}

Connection::Connection(HANDLE pool, HANDLE sock) {
	this->hNetLibUser = pool;
	this->sock = sock;
	eof = false;
	bufferPos = 0;
	bufferLen = 0;
}

Connection::~Connection() {
	close();
}


HANDLE Connection::getNetLibUser(const char *poolName) {
	NetLibUser *c;
	for (c=pools; c!=NULL; c=c->next) {
		if (!strcmp(c->name, poolName)) {
			return c->hNetLibUser;
		}
	}
	return NULL;
}

int Connection::close() {
	if (sock != NULL) {
		Netlib_CloseHandle(sock);
		sock = NULL;
	}
	return TRUE;
}


int Connection::connect(const char *host, int port) {
	if (hNetLibUser != NULL && sock == NULL) {
		NETLIBOPENCONNECTION nloc;
		nloc.cbSize = NETLIBOPENCONNECTION_V1_SIZE;//sizeof(NETLIBOPENCONNECTION);
		nloc.szHost = host;
		nloc.wPort = port;
		nloc.flags = 0;
		sock = (HANDLE) CallService(MS_NETLIB_OPENCONNECTION, (WPARAM) hNetLibUser, (LPARAM) &nloc);
		if (sock != NULL) {
			return TRUE;
		}
	}
	return FALSE;
}

void IncomingConnectionFunc (HANDLE hNewConnection, DWORD dwRemoteIP, void * pExtra) {
	Connection *c = (Connection *) pExtra;
	c->onNewConnection(hNewConnection, dwRemoteIP);
}

int Connection::bind(const char * host, int port, ConnectionListener* listener) {
	if (hNetLibUser != NULL && sock == NULL) {
		NETLIBBIND nlb = {0};
		nlb.cbSize = sizeof(NETLIBBIND);
		nlb.pfnNewConnectionV2 = IncomingConnectionFunc;
		nlb.wPort = 0;
		nlb.pExtra = this;
		this->listener = listener;
		sock = (HANDLE) CallService(MS_NETLIB_BINDPORT, (WPARAM) hNetLibUser, (LPARAM) &nlb);
		if (sock != NULL) {
			return nlb.wPort;
		}
	}
	return 0;
}

int Connection::socket() {
	return (int) CallService(MS_NETLIB_GETSOCKET, (WPARAM) sock, 0);
}

int Connection::recv(char *data, long datalen) {
	int totalret = 0;
	while (datalen>0) {
		if (bufferLen==0) {
			bufferLen = Netlib_Recv(sock, buffer, 1024, MSG_DUMPASTEXT);
			if(bufferLen == SOCKET_ERROR) {
				eof = true;
				return 0;
			}
			if(bufferLen == 0) {
				eof = true;
				return 0;
			}
			bufferPos = 0;
		}
		int ret = min(bufferLen, datalen);
		memcpy (data, buffer+bufferPos, ret);
		data += ret;
		bufferPos += ret;
		bufferLen -= ret;
		datalen -= ret;
		totalret += ret;
	}
	return totalret;
}

int Connection::send(const char *data, long datalen) {
	int len;
	if ((len=Netlib_Send(sock, data, datalen, MSG_DUMPASTEXT))==SOCKET_ERROR || len!=datalen) { //MSG_NODUMP
		return FALSE;
	}
	return TRUE;
}


char *Connection::recvLine() {
	int i, lineSize = 0;
	char *line = NULL;
	for (i=0;;i++) {
		if (i>=lineSize) {
			line = (char *)realloc(line, lineSize+128);
			lineSize += 128;
		}
		if (recv(line+i, 1) != 0) {
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


int Connection::send(const char *data) {
	return send(data, strlen(data));
}

int Connection::sendText(const char *fmt, ...) {
	va_list vararg;
	int size = 512;
	char *str;
	va_start(vararg, fmt);
	str = (char *) malloc(size);
	while (_vsnprintf(str, size, fmt, vararg) == -1) {
		size += 512;
		str = (char *) realloc(str, size);
	}
	va_end(vararg);
	int result = send(str);
	free(str);
	return result;
}

void Connection::onNewConnection(HANDLE sock, DWORD dwRemoteIP)  {
	Connection *c = new Connection(hNetLibUser, sock);
	if (listener != NULL) {
		listener->onNewConnection(c, dwRemoteIP);
	}
	delete c;
}