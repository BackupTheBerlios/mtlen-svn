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
#include "Connection.h"
//#include "rvp.h"
//#include "RVPClient.h"
#include "Utils.h"

/* Receive file thread */

static void __cdecl RVPFileTransferRecv(void *ptr) {
	RVPFileTransfer *ft = (RVPFileTransfer *) ptr;
//	ft->
}

/* Send file thread */

static void __cdecl RVPFileTransferSend(void *ptr) {
	RVPFileTransfer *ft = (RVPFileTransfer *) ptr;
//	ft->
}

RVPFileTransfer::RVPFileTransfer(RVPFile *file, RVPFileTransferListener *listener) {
	this->file = file;
	this->listener = listener;
}

void RVPFileTransfer::recvFile() {

}

void RVPFileTransfer::sendFile() {

}

void RVPFileTransfer::recvFile(RVPFile *file, RVPFileTransferListener *listener) {

}

void RVPFileTransfer::sendFile(RVPFile *file, RVPFileTransferListener *listener) {
}

void RVPFileTransfer::cancelFile(RVPFile *file) {

}