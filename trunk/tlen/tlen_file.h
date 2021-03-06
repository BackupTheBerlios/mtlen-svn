/*

Jabber Protocol Plugin for Miranda IM
Tlen Protocol Plugin for Miranda IM
Copyright (C) 2002-2004  Santithorn Bunchua
Copyright (C) 2004-2007  Piotr Piastucki

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

#ifndef _TLEN_FILE_H_
#define _TLEN_FILE_H_

#include <windows.h>
#include "jabber.h"

extern int TlenFileCancelAll(TlenProtocol *proto);
extern void TlenProcessF(XmlNode *node, ThreadData *userdata);
extern TLEN_FILE_TRANSFER *TlenFileCreateFT(TlenProtocol *proto, const char *jid);
#endif

