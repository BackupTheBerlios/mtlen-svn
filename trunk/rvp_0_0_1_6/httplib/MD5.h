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
class MD5;
#ifndef MD5_INCLUDED
#define MD5_INCLUDED
class MD5 {
	unsigned int  	state[4];
	unsigned int	bufferPos;
	unsigned int  	dataLength[2];
	unsigned char 	buffer[64];
public:
	void			init();
	void			transform(unsigned char *input);
	void			update(unsigned char *input, unsigned int inputLen);
	void			finalize();
	void			get(unsigned int output[4]);
	void			getHex(char *output);

};
#endif
