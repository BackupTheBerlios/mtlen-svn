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
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "Utils.h"
#include "RVPFile.h"

/* Receive file thread */

void __cdecl RVPFileRecv(void *ptr) {
	RVPFile *ft = (RVPFile *) ptr;
	ft->doRecv();
}

/* Send file thread */

void __cdecl RVPFileSend(void *ptr) {
	RVPFile *ft = (RVPFile *) ptr;
	ft->doSend();
}

List RVPFile::list;

RVPFile* RVPFile::find(const char *id1, const char *id2) {
	return (RVPFile*)list.find(id1, id2);
}

RVPFile::RVPFile(int mode, const char *contact, const char *cookie, const char *login, RVPFileListener *listener):ListItem(contact, cookie) {
	file = NULL;
	path = NULL;
	host = NULL;
	authCookie = NULL;
	connection = NULL;
	listenConnection = NULL;
	this->mode = mode;
	this->contact = Utils::dupString(contact);
	this->login = Utils::dupString(login);
	this->cookie = Utils::dupString(cookie);
	this->listener = listener;
	list.add(this);
}

RVPFile::~RVPFile() {
	list.remove(this);
	if (connection != NULL) connection->close();
	if (listenConnection != NULL) listenConnection->close();
	while (getThreadCount(TGROUP_TRANSFER > 0)) {
		Sleep(100);
	}
	if (connection != NULL) delete connection;
	if (file != NULL) delete file;
	if (path != NULL) delete path;
	if (host != NULL) delete host;
	if (contact != NULL) delete contact;
	if (login != NULL) delete login;
	if (cookie != NULL) delete cookie;
	if (authCookie != NULL) delete authCookie;
}

int RVPFile::getMode() {
	return mode;
}

const char *RVPFile::getContact() {
	return contact;
}

const char *RVPFile::getLogin() {
	return login;
}

const char *RVPFile::getCookie() {
	return cookie;
}

const char *RVPFile::getAuthCookie() {
	return authCookie;
}

void RVPFile::setAuthCookie(const char *f) {
	if (authCookie != NULL) delete authCookie;
	authCookie = Utils::dupString(f);
}

void RVPFile::setSize(int s) {
	size = s;
}

int RVPFile::getSize() {
	return size;
}

void RVPFile::setFile(const char *f) {
	if (file != NULL) delete file;
	file = Utils::dupString(f);
}

const char *RVPFile::getFile() {
	return file;
}

void RVPFile::setPath(const char *f) {
	if (path != NULL) delete path;
	path = Utils::dupString(f);
}

const char *RVPFile::getPath() {
	return path;
}

void RVPFile::setHost(const char *f) {
	if (host != NULL) delete host;
	host = Utils::dupString(f);
}

const char *RVPFile::getHost() {
	return host;
}

void RVPFile::setPort(int p) {
	port = p;
}

int RVPFile::getPort() {
	return port;
}

void RVPFile::recv() {
	forkThread(TGROUP_TRANSFER, RVPFileRecv, 0, this);
}

void RVPFile::send() {
	forkThread(TGROUP_TRANSFER, RVPFileSend, 0, this);
}

void RVPFile::cancel() {
	if (listenConnection != NULL) {
		listenConnection->close();
	} 
	if (connection != NULL) {
		connection->close();
	} else {
		if (listener != NULL) {
			listener->onFileProgress(this, RVPFileListener::PROGRESS_CANCELLED, 0);
		}
	}
}

bool RVPFile::msnftp() {
	bool completed = false;
	bool error = false;
	char *params = "";
	char *line;
	while (!completed) {
		line = connection->recvLine();
		if (line != NULL) {
			if (strlen(line) > 3) {
				params = line + 4;
			}
			switch ((((int *)line)[0]&0x00FFFFFF)|0x20000000) {
				case ' REV':
					{
						char protocol[7];
						bool msnftp = false;
						while (params != NULL && sscanf(params, "%6s", protocol) >= 1) {
							if (strcmp(protocol, "MSNFTP") == 0) {
								msnftp = true;
								break;
							}
							params = strstr(params, " ");
							if (params != NULL) params ++;
						}
						if (getMode() == RVPFile::MODE_SEND) {
							connection->send("VER MSNFTP\r\n");
						} else {
							if (msnftp) {
								connection->sendText("USR %s %s\r\n",getLogin(),getAuthCookie());
							} else {
								/* error */
							}
						}
					}
					break;
				case ' RSU':
					{
						char email[130], cookie[14];
						if (sscanf(params,"%129s %13s",email, cookie) >= 2) {
							if (!strcmp(email, getContact()) && !strcmp(cookie, getAuthCookie())) {
								connection->sendText("FIL %i\r\n", getSize());
							} else {
								/* error */
							}
							break;
						}
					}
					break;
				case ' LIF':
					{
						char filesize[ 30 ];
						if ( sscanf( params, "%s", filesize ) >= 1 ) {
							setSize(atol(filesize));
							connection->send("TFR\r\n");
							/* ack initializing */
							if (listener != NULL) {
								listener->onFileProgress(this, RVPFileListener::PROGRESS_INITIALIZING, 0);
							}
							/* receive data */
							char *fullFileName = new char[strlen(getFile()) + strlen(getPath()) + 2];
							strcpy(fullFileName, getPath());
							if (fullFileName[strlen(fullFileName)-1] != '\\') {
								strcat(fullFileName, "\\");
							}
							strcat(fullFileName, getFile());
							int fileId = _open(fullFileName, _O_BINARY|_O_WRONLY|_O_CREAT|_O_TRUNC, _S_IREAD|_S_IWRITE);
							delete fullFileName;
							if (fileId >= 0) {
								int receivedBytes = 0;
								while (receivedBytes < getSize()) {
									char buffer[2048];
									int header = 0;
									if (connection->recv((char *)&header, 3)) {
										_write(fileId, (char *)&header, 3);
										int blockSize = header >> 8;
										while (blockSize > 0) {
											int readSize = min(2048, blockSize);
											blockSize -= readSize;
											receivedBytes += readSize;
											connection->recv(buffer, readSize);
											if (_write(fileId, buffer, readSize) != readSize) {
												error = true;
												break;
											}
										}
										if (listener != NULL) {
											listener->onFileProgress(this, RVPFileListener::PROGRESS_PROGRESS, receivedBytes);
										}
										if (header&0xFF == 0) {
											continue;
										}
									} else {
										error = true;
									}
									break;
								}
								_close(fileId);
							} else {
								error = true;
							}
							completed = true;
						}
					}
					break;
				case ' EYB':
					completed = true;
					break;
				case ' RFT':
					/* ack initializing */
					if (listener != NULL) {
						listener->onFileProgress(this, RVPFileListener::PROGRESS_INITIALIZING, 0);
					}
					/* send data */
					char *fullFileName = new char[strlen(getFile()) + strlen(getPath()) + 2];
					strcpy(fullFileName, getPath());
					if (fullFileName[strlen(fullFileName)-1] != '\\') {
						strcat(fullFileName, "\\");
					}
					strcat(fullFileName, getFile());
					int fileId=_open(fullFileName, _O_BINARY|_O_RDONLY);
					delete fullFileName;
					if (fileId >= 0) {
						int sentBytes = 0;
						while (sentBytes < getSize()) {
							char buffer[2048];
							int numRead=_read(fileId, buffer+3, min(2045, getSize() - sentBytes));
							if (numRead > 0) {
								sentBytes += numRead;
								if (getSize() - sentBytes > 0) {
									buffer[0] = 0;
								} else {
									buffer[0] = 1;
								}
								buffer[1] = numRead & 0xFF;
								buffer[2] = (numRead >> 8) & 0xFF;
								if (connection->send(buffer, 3 + numRead) != 3 + numRead) {
									error = true;
									break;
								}
							} else {
								break;
							}
							if (listener != NULL) {
								listener->onFileProgress(this, RVPFileListener::PROGRESS_PROGRESS, sentBytes);
							}
						}	
						_close(fileId);
					} else {
						error = true;
					}
					completed = true;
					break;
			}
		}
	}
	if (getMode() == RVPFile::MODE_RECV) {
		if (error) {
			connection->send("BYE 2164261694\r\n");
		} else {
			connection->send("BYE 16777989\r\n");
		}
	}
	if (listener != NULL) {
		if (error) {
			listener->onFileProgress(this, RVPFileListener::PROGRESS_ERROR, 0);
		} else {
			listener->onFileProgress(this, RVPFileListener::PROGRESS_COMPLETED, 0);
		}
	}
	return !error;
}

void RVPFile::doRecv() {
	connection = new Connection(DEFAULT_CONNECTION_POOL);
	if (listener != NULL) {
		listener->onFileProgress(this, RVPFileListener::PROGRESS_CONNECTING, 0);
	}
	if (connection->connect(getHost(), getPort())) {
		if (listener != NULL) {
			listener->onFileProgress(this, RVPFileListener::PROGRESS_CONNECTED, 0);
		}
		/* ack connected */
		if (connection->send("VER MSNFTP\r\n")) {
			msnftp();
		}
	}
	delete connection;
	connection = NULL;
}

void RVPFile::doSend() {
	listenConnection = new Connection(DEFAULT_CONNECTION_POOL);
	if (listener != NULL) {
		listener->onFileProgress(this, RVPFileListener::PROGRESS_CONNECTING, 0);
	}
	int port = listenConnection->bind(NULL, 0, this);
	/* Bind and send info */
	
	/* accept and call msnftp */
	delete listenConnection;
	listenConnection = NULL;
}

void RVPFile::onNewConnection(Connection *connection, DWORD dwRemoteIP) {
	this->connection = connection;
	msnftp();
}
