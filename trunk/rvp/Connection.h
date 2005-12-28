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

class Connection;
class ConnectionListener;

#ifndef CONNECTION_H_INCLUDED
#define CONNECTION_H_INCLUDED

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x500
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>

#include <newpluginapi.h>
#include <m_system.h>
#include <m_netlib.h>
#include "Utils.h"

#define DEFAULT_CONNECTION_POOL "DEFAULT"

class ConnectionListener {
public:
	virtual void onNewConnection(Connection *, DWORD dwRemoteIP) = 0;
};

class Connection {
	friend void IncomingConnectionFunc (HANDLE hNewConnection, DWORD dwRemoteIP, void * pExtra);
private:
	class NetLibUser {
	public:
		NetLibUser *next;
		HANDLE hNetLibUser;
		char *name;
	};
	static NetLibUser *pools;
	ConnectionListener *listener;
	HANDLE hNetLibUser;
	HANDLE sock;
	bool eof;
	char buffer[1024];
	int bufferPos;
	int bufferLen;
	HANDLE getNetLibUser(const char *poolName);
protected:
	void onNewConnection(HANDLE sock, DWORD dwRemoteIP);
	Connection(HANDLE pool, HANDLE sock);
public:
	Connection(const char *poolName);
	~Connection();
	int recv(char *buffer, long len);
	char *recvLine();
	int send(const char *buffer, long len);
	int send(const char *buffer);
	int sendText(const char *fmt, ...);
	int close();
	int connect(const char *host, int port);
	int bind(const char *host, int port, ConnectionListener* listener);
	int socket();
	static void init(const char *poolName, const char *protoName, const char *poolDescription);
};

#endif
