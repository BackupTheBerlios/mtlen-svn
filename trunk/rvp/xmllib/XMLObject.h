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

#ifndef XMLOBJECT_INCLUDED
#define XMLOBJECT_INCLUDED

#include "JString.h"

class XMLObject {
private:
	XMLObject*		parent;
	XMLObject*		child;
	XMLObject*		prev;
	XMLObject*		next;
	XMLObject*		attribute;
	XMLObject*		cdata;
	JString*		name;
	JString*   		data;
	int				level;
	JString*		input;
	int				inputpos;
	bool			endExpected;
	bool			endOfData;
	char			getChar();
	char			getNoSpaceChar();
	char			peekChar();
	char			peekChar(int distance);
	void			consumeChars(int counts);
	char			peekNoSpaceChar();
	JString*		peekString(int len);
	int 			parseAtt(JString* s, int pos);
	int 			parseXML(JString* s, int pos);
public:
	enum ERRORS {
		ERROR_UNEXPECTED_EOF=1,
		ERROR_TAG_SYNTAX=2,
		ERROR_ATTRIBUTE_SYNTAX=3,
	};
	XMLObject();
	XMLObject(JString* n);
	XMLObject(JString* n, JString* d);
	~XMLObject();

	JString* treeString();

	XMLObject*  getChild();
	XMLObject*  getChild(JString*);
	XMLObject*	getChild(char *s);
	XMLObject*  getNext();
	XMLObject*  getNext(JString*);
	XMLObject*	getNext(char *s);
	XMLObject*	getCDATA();

	bool		addNext(XMLObject* xml);
	void		addChild(XMLObject* xml);
	void		addCDATA(XMLObject* xml);

	bool		nameEquals(char *s);

	JString*	getName();
	JString*	getData();
	JString*	getAttribute(JString*);
	JString*	getAttribute(char *);
	void		setAttribute(JString* n, JString* s);

	int		getError();

	int 	parseXML(JString* s);
};

#endif
