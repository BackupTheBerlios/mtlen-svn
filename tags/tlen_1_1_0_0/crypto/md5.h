/*

Jabber Protocol Plugin for Miranda IM
Tlen Protocol Plugin for Miranda IM
Copyright (C) 2002-2007  Santithorn Bunchua
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


#ifndef _MD5_H_
#define _MD5_H_


typedef struct MD5_tag {
	unsigned int  	state[4];
	unsigned int	bufferPos;
	unsigned int  	dataLength[2];
	unsigned char 	buffer[64];
} MD5;

void			md5_init(MD5* context);
void			md5_update(MD5* context, unsigned char *input, unsigned int inputLen);
void			md5_finalize(MD5* context);

#endif
