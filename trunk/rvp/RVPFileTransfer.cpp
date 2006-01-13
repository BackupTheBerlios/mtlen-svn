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
#include "RVPFileTransfer.h"
#include "Utils.h"
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

List RVPFileTransfer::list;

/* Receive file thread */

void __cdecl RVPFileTransferRecv(void *ptr) {
	RVPFileTransfer *ft = (RVPFileTransfer *) ptr;
	ft->doRecvFile();
	delete ft;
}

/* Send file thread */

void __cdecl RVPFileTransferSend(void *ptr) {
	RVPFileTransfer *ft = (RVPFileTransfer *) ptr;
	ft->doSendFile();
	delete ft;
}

RVPFileTransfer::RVPFileTransfer(RVPFile *file, RVPFileListener *listener):ListItem(file->getContact(), file->getCookie()) {
	this->file = file;
	this->listener = listener;
	list.add(this);
}

RVPFileTransfer::~RVPFileTransfer() {
	while (getThreadCount(TGROUP_RECV | TGROUP_SEND) > 0) {
	}
	list.remove(this);
	if (connection != NULL) delete connection;
}

void RVPFileTransfer::recvFile() {
	forkThread(TGROUP_RECV, RVPFileTransferRecv, 0, this);
}

void RVPFileTransfer::sendFile() {
	forkThread(TGROUP_SEND, RVPFileTransferSend, 0, this);
}

void RVPFileTransfer::cancelFile() {
	if (connection != NULL) {
		connection->close();
	}
}

bool RVPFileTransfer::msnftp() {
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
						if (file->getMode() == RVPFile::MODE_SEND) {
							connection->send("VER MSNFTP\r\n");
						} else {
							if (msnftp) {
								connection->sendText("USR %s %s\r\n",file->getLogin(),file->getAuthCookie());
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
							if (!strcmp(email, file->getContact()) && !strcmp(cookie, file->getAuthCookie())) {
								connection->sendText("FIL %i\r\n", file->getSize());
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
							file->setSize(atol(filesize));
							connection->send("TFR\r\n");
							/* ack initializing */
							if (listener != NULL) {
								listener->onFileProgress(file, RVPFileListener::PROGRESS_INITIALIZING, 0);
							}
							/* receive data */
							char *fullFileName = new char[strlen(file->getFile()) + strlen(file->getPath()) + 2];
							strcpy(fullFileName, file->getPath());
							if (fullFileName[strlen(fullFileName)-1] != '\\') {
								strcat(fullFileName, "\\");
							}
							strcat(fullFileName, file->getFile());
							int fileId = _open(fullFileName, _O_BINARY|_O_WRONLY|_O_CREAT|_O_TRUNC, _S_IREAD|_S_IWRITE);
							delete fullFileName;
							
							while (true) {
								char buffer[2048];
								int header = 0;
								if (connection->recv((char *)&header, 3)) {
									_write(fileId, (char *)&header, 3);
									int blockSize = header >> 8;
									while (blockSize > 0) {
										int readSize = min(2048, blockSize);
										blockSize -= readSize;
										connection->recv(buffer, readSize);
										if (_write(fileId, buffer, readSize) != readSize) {
											error = true;
											break;
										}
									}										
									//listener->onFileProgress(file, RVPFileListener::PROGRESS_INITIALIZING, 0);
									if (header&0xFF == 0) {
										continue;
									}
								} else {
									error = true;
								}									
								break;
							}
							_close(fileId);
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
						listener->onFileProgress(file, RVPFileListener::PROGRESS_INITIALIZING, 0);
					}
					/* send data */
					completed = true;
					break;
			}
		}
	}
	if (file->getMode() == RVPFile::MODE_RECV) {
		if (error) {
			connection->send("BYE 2164261694\r\n");
		} else {
			connection->send("BYE 16777989\r\n");
		}
	}
	if (listener != NULL) {
		if (error) {
			listener->onFileProgress(file, RVPFileListener::PROGRESS_ERROR, 0);
		} else {
			listener->onFileProgress(file, RVPFileListener::PROGRESS_COMPLETED, 0);
		}
	}
	return !error;
}

void RVPFileTransfer::doRecvFile() {
	connection = new Connection(DEFAULT_CONNECTION_POOL);
	if (connection->connect(file->getHost(), file->getPort())) {
		/* ack connected */
		if (connection->send("VER MSNFTP\r\n")) {
			msnftp();
		}
	}
	delete connection;
	connection = NULL;
}

void RVPFileTransfer::doSendFile() {
	connection = new Connection(DEFAULT_CONNECTION_POOL);
	/* Bind and send info */
	//msnftp(con);
	delete connection;
	connection = NULL;
}

void RVPFileTransfer::recvFile(RVPFile *file, RVPFileListener *listener) {
	RVPFileTransfer* ft = new RVPFileTransfer(file, listener);
	ft->recvFile();
}

void RVPFileTransfer::sendFile(RVPFile *file, RVPFileListener *listener) {
	RVPFileTransfer* ft = new RVPFileTransfer(file, listener);
	ft->sendFile();
}

void RVPFileTransfer::cancelFile(RVPFile *file) {
	RVPFileTransfer* ft = (RVPFileTransfer*) list.find(file->getContact(), file->getCookie());
	if (ft != NULL) {
		ft->cancelFile();
	}
}
