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
	bool error = false;
	char *params = "";
	char *line;
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
					if (msnftp) {
						if (file->getMode() == RVPFile::MODE_SEND) {
							connection->send("VER MSNFTP\r\n");
						} else {
							connection->sendText("USR %s %s\r\n",file->getLogin(),file->getCookie());
						}
					} 
				}
				break;
			case ' RSU':
				{
					char email[130], cookie[14];
					if (sscanf(params,"%129s %13s",email, cookie) >= 2) {
						if (!strcmp(email, file->getContact()) && !strcmp(cookie, file->getCookie())) {
							connection->sendText("FIL %i\r\n", file->getSize());
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
					}
				}
				break;
			case ' EYB':
				if (listener != NULL) {
					listener->onFileProgress(file, RVPFileListener::PROGRESS_COMPLETED, 0);
				}
				return !error;
				break;
			case ' RFT':
				/* ack initializing */
				if (listener != NULL) {
					listener->onFileProgress(file, RVPFileListener::PROGRESS_INITIALIZING, 0);
				}
				/* send data */ 
				break;
		}
	}
	if (listener != NULL) {
		listener->onFileProgress(file, RVPFileListener::PROGRESS_ERROR, 0);
	}
	return !error;
}

void RVPFileTransfer::doRecvFile() {
	bool completed = false;
	connection = new Connection(DEFAULT_CONNECTION_POOL);
	MessageBoxA(NULL, file->getHost(), "Host 2", MB_OK);
	MessageBoxA(NULL,  file->getAuthCookie(), file->getHost(), MB_OK);
	if (connection->connect(file->getHost(), file->getPort())) {
		/* ack connected */
		if (connection->send("VER MSNFTP\r\n")) {
			msnftp();
		}
	}
	delete connection;
	connection = NULL;
	if (completed) {
		
	} else {

	}
}

void RVPFileTransfer::doSendFile() {
	bool completed = false;
	connection = new Connection(DEFAULT_CONNECTION_POOL);
	/* Bind and send info */
	//msnftp(con);
	delete connection;
	connection = NULL;
	if (completed) {
		
	} else {

	}
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