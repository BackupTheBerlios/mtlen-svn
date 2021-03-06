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

#ifndef _JABBER_XML_H_
#define _JABBER_XML_H_

typedef enum { ELEM_OPEN, ELEM_CLOSE, ELEM_OPENCLOSE, ELEM_TEXT } XmlElemType;
typedef enum { NODE_OPEN, NODE_CLOSE } XmlNodeType;

typedef struct tagXmlAttr {
	char *name;
	char *value;
} XmlAttr;

typedef struct tagXmlNode {
	int depth;									// depth of the current node (1=root)
	char *name;									// tag name of the current node
	int numAttr;								// number of attributes
	int maxNumAttr;								// internal use (num of slots currently allocated to attr)
	XmlAttr **attr;								// attribute list
	int numChild;								// number of direct child nodes
	int maxNumChild;							// internal use (num of slots currently allocated to child)
	struct tagXmlNode **child;					// child node list
	char *text;
	XmlNodeType state;							// internal use by parser
} XmlNode;

typedef struct tagXmlState {
	XmlNode root;			// root is the document (depth = 0);
	// callback for depth=n element on opening/closing
	void (*callback1_open)();
	void (*callback1_close)();
	void (*callback2_open)();
	void (*callback2_close)();
	void *userdata1_open;
	void *userdata1_close;
	void *userdata2_open;
	void *userdata2_close;
} XmlState;

void JabberXmlInitState(XmlState *xmlState);
void JabberXmlDestroyState(XmlState *xmlState);
BOOL JabberXmlSetCallback(XmlState *xmlState, int depth, XmlElemType type, void (*callback)(), void *userdata);
int JabberXmlParse(XmlState *xmlState, char *buffer, int datalen);
char *JabberXmlGetAttrValue(XmlNode *node, char *key);
XmlNode *JabberXmlGetChild(XmlNode *node, char *tag);
XmlNode *JabberXmlGetNthChild(XmlNode *node, char *tag, int nth);
XmlNode *JabberXmlGetChildWithGivenAttrValue(XmlNode *node, char *tag, char *attrKey, char *attrValue);
void JabberXmlFreeNode(XmlNode *node);
XmlNode *JabberXmlCopyNode(XmlNode *node);

XmlNode *JabberXmlCreateNode(char *name);
void JabberXmlAddAttr(XmlNode *n, char *name, char *value);
XmlNode *JabberXmlAddChild(XmlNode *n, char *name);
void JabberXmlAddText(XmlNode *n, char *text);

#endif

