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

#if !defined(AFX_JSTRING_H__3B69F435_26CD_4C14_8B28_4E5663393410__INCLUDED_)
#define AFX_JSTRING_H__3B69F435_26CD_4C14_8B28_4E5663393410__INCLUDED_

#include <string.h>
#include <stdio.h>

class JString
{
private:
	int		len;
	void	increaseDataSize(int);
	int    	dataSize;
	char  	*data;
public:
	JString();
	JString(JString *);
	JString(const char *);
	JString(const char *, int);
	~JString();
	int		length();
	void 	add(const char *p);
	void 	add(const char *p, int);
	void 	add(char c);
	void 	add(JString *s);
	char 	getChar(int pos);
	void 	print();
	bool 	equals(JString *s);
	bool 	equals(const char *s);
	char*	toString();

};

#endif // !defined(AFX_JSTRING_H__3B69F435_26CD_4C14_8B28_4E5663393410__INCLUDED_)
