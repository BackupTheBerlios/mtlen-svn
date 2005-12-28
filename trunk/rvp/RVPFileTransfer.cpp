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

/* Receive file thread */

static void __cdecl RVPFileTransferRecv(void *ptr) {
	RVPFileTransfer *ft = (RVPFileTransfer *) ptr;
	ft->doRecvFile();
	delete ft;
}

/* Send file thread */

static void __cdecl RVPFileTransferSend(void *ptr) {
	RVPFileTransfer *ft = (RVPFileTransfer *) ptr;
	ft->doSendFile();
	delete ft;
}

RVPFileTransfer::RVPFileTransfer(RVPFile *file, RVPFileTransferListener *listener):ListItem(file->getContact(), file->getCookie()) {
	this->file = file;
	this->listener = listener;
}

void RVPFileTransfer::recvFile() {
	forkThread(TGROUP1, RVPFileTransferRecv, 0, this);
}

void RVPFileTransfer::sendFile() {
	forkThread(TGROUP1, RVPFileTransferSend, 0, this);
}

void RVPFileTransfer::doRecvFile() {
	Connection * con = new Connection(DEFAULT_CONNECTION_POOL);
	char *line;
	if (con->connect(file->getHost(), file->getPort())) {
		con->send("VER MSNFTP");
		line = con->recvLine();
		if (line != NULL && strstr(line, "MSNFTP") != NULL) {

		}		
	}
	delete con;
}

void RVPFileTransfer::doSendFile() {
	Connection * con = new Connection(DEFAULT_CONNECTION_POOL);
	delete con;
}

void RVPFileTransfer::recvFile(RVPFile *file, RVPFileTransferListener *listener) {
	RVPFileTransfer* ft = new RVPFileTransfer(file, listener);
	ft->recvFile();
}

void RVPFileTransfer::sendFile(RVPFile *file, RVPFileTransferListener *listener) {
	RVPFileTransfer* ft = new RVPFileTransfer(file, listener);
	ft->sendFile();
}

void RVPFileTransfer::cancelFile(RVPFile *file) {

}