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
#include "stdio.h"
#include "XMLObject.h"

static int	errorCode;

	XMLObject::~XMLObject() {
		if (next!=NULL) delete next;
		if (child!=NULL) delete child;
		if (name!=NULL) delete name;
		if (cdata!=NULL) delete cdata;
		if (attribute!=NULL) delete attribute;
		if (data!=NULL) delete data;
	}

	XMLObject::XMLObject() {
		parent=NULL;
		child=NULL;
		prev=NULL;
		next=NULL;
		data=NULL;
		name=new JString();
		attribute=NULL;
		cdata=NULL;
		input=NULL;
		inputpos=0;
		level=0;
		errorCode=0;
	}
	XMLObject::XMLObject(JString* n) {
		parent=NULL;
		child=NULL;
		prev=NULL;
		next=NULL;
		data=NULL;
		input=NULL;
		attribute=NULL;
		cdata=NULL;
		inputpos=0;
		level=0;
		errorCode=0;
		if (n!=NULL) name=new JString(n);
		else name=new JString();
	}
	XMLObject::XMLObject(JString* n, JString* d) {
		parent=NULL;
		child=NULL;
		prev=NULL;
		next=NULL;
		data=NULL;
		input=NULL;
		attribute=NULL;
		cdata=NULL;
		inputpos=0;
		level=0;
		errorCode=0;
		if (n!=NULL) name=new JString(n);
		else name=new JString();
		if (d!=NULL) data=new JString(d);
	}

	static bool isWhitespace(char c)
	{
		if (c==' ' || c==9 || c==10 || c==13) return true;
		return false;
	}

	char XMLObject::peekChar()
	{
		if (input!=NULL && inputpos<input->length()) {
			endOfData=false;
			return input->getChar(inputpos);
		}
		else {
			endOfData=true;
			if (!endExpected) {
				errorCode=ERROR_UNEXPECTED_EOF;
				return '\0';
			}
			return '\0';
		}
	}

	char XMLObject::peekChar(int distance)
	{
		if (input!=NULL && (inputpos+distance)<input->length()) {
//			endOfData=false;
			return input->getChar(inputpos+distance);
		}
		else {
//			endOfData=true;
//			if (!endExpected) {
//				errorCode=ERROR_UNEXPECTED_EOF;
//				return '\0';
//			}
			return '\0';
		}
	}

	JString * XMLObject::peekString(int len) {
		JString *out = new JString();
		for (int i=0;i<len;i++) {
			char c = peekChar(i);
			if (c=='\0') {
				return out;
			}
			out->add(c);
		}
		return out;
	}

	void XMLObject::consumeChars(int count) {
		inputpos += count;
	}

	char XMLObject::getChar()
	{	char ch;
		if (ch=peekChar()) inputpos++;
		return ch;
	}

	char XMLObject::getNoSpaceChar()
	{	char ch;
		while (isWhitespace(ch=getChar()) && (!endOfData));
		return ch;
	}

	char XMLObject::peekNoSpaceChar()
	{	char ch;
		while (isWhitespace(ch=peekChar()) && (!endOfData)) ch=getChar();
		return ch;
	}

	int XMLObject::parseAtt(JString* s, int pos)
	{	char ch;
		JString* s1,*s2;
		input=s;
		inputpos=pos;

		ch=getNoSpaceChar();
		if (endOfData) errorCode=ERROR_TAG_SYNTAX;
		while (!endOfData && ch!='>' && ch!='/') {
			s1=new JString();
			s2=new JString();
			while (!(isWhitespace(ch) || ch=='>' || ch=='/' || ch=='=' || endOfData)) {
				s1->add(ch);
				ch=getChar();
			}
			if (isWhitespace(ch)) {
				ch=getNoSpaceChar();
			}
			if (ch=='=') {
				ch=getNoSpaceChar();
				if (ch=='"' || ch=='\'') {
					ch=getChar();
					while (ch!='"' && ch!='\'' && !endOfData) {
						s2->add(ch);
						ch=getChar();
					}
					if (endOfData) errorCode=ERROR_ATTRIBUTE_SYNTAX;
					else ch=getNoSpaceChar();
				}
				else {
					errorCode=ERROR_ATTRIBUTE_SYNTAX;
				}
			}
			if (errorCode==0) setAttribute(s1,s2);
			delete s1;
			delete s2;
			if (errorCode!=0) break;
		}
		if (endOfData) errorCode=ERROR_TAG_SYNTAX;
		return inputpos-1;
	}

	int XMLObject::parseXML(JString* s, int pos)
	{	XMLObject* newxml;
		JString* st;
		char ch;

		input=s;
		inputpos=pos;

		data=new JString();

		if (pos==0) endExpected=true;
		else endExpected=false;
		ch=getNoSpaceChar();
		while (errorCode==0 && !endOfData) {
			st=new JString();
			if (ch=='<') { // tag
				ch=peekChar();
				if (ch=='!') { //special tags
					getChar();
					JString *str = peekString(7);
					if (str->equals("[CDATA[")) {
						consumeChars(7);
						ch=getChar();
						while (!endOfData) {
							if (ch==']') {
								ch=peekChar(0);
								if (ch==']') {
									ch=peekChar(1);
									if (ch=='>') {
										ch=getChar();
										ch=getChar();
										ch=getChar();
										break;
									}
								}
							}
							st->add(ch);
							ch=getChar();
    					}
						newxml = new XMLObject(NULL, st);
						addCDATA(newxml);
					} else {
    					while (!endOfData) {
    						if (getNoSpaceChar()=='>') break;
    					}
					}
					if (endOfData) errorCode=ERROR_TAG_SYNTAX;
					delete str;
				}
				else if (ch=='?') { // xml preprocessor commands - skip them
       				getChar();
					while (!endOfData) {
						if (getNoSpaceChar()==ch) {
							if (getNoSpaceChar()=='>') break;
						}
					}
					if (endOfData) errorCode=ERROR_TAG_SYNTAX;
				}
				else if (ch!='/') {	// open tag
					ch=getNoSpaceChar();
					while (ch!='>' && ch!='/' && !endOfData) {
						if (isWhitespace(ch)) break;
						st->add(ch);
						ch=getChar();
					}
					if (endOfData || (st->length()==0)) errorCode=ERROR_TAG_SYNTAX;
					else {
						newxml=new XMLObject(st);
						addChild(newxml);
						if (isWhitespace(ch)) ch=getNoSpaceChar();
						if (ch!='>' && ch!='/') {
							inputpos=newxml->parseAtt(s,inputpos-1);
							ch=getNoSpaceChar();
						}
           				if (errorCode!=0) return inputpos;
           				if (ch=='/') {
               				ch=getChar();
           					if (ch!='>') errorCode=ERROR_TAG_SYNTAX;
						}
						else if (ch=='>') inputpos=newxml->parseXML(s,inputpos);
						else errorCode=ERROR_TAG_SYNTAX;
					}
				}
				else { // close tag
					ch=getChar();
					while ((ch=getNoSpaceChar())!='>' && !endOfData) st->add(ch);
					if (ch!='>') errorCode=ERROR_UNEXPECTED_EOF;
					else if (st->length()>0) {
						if (!(st->equals(name))) {
							errorCode=ERROR_TAG_SYNTAX;
						}
					}
					return inputpos;
				}
			}
			else {
				data->add(ch);
				ch=getChar();
			}
			delete st;
		}
		if (endOfData && !endExpected) {
			errorCode=ERROR_UNEXPECTED_EOF;
		}
		return inputpos;
	}

	bool XMLObject::addNext(XMLObject* xml)
	{	XMLObject* xml1,*xml2;
		for (xml1=xml2=this;xml2!=NULL;xml2=xml2->next) {
			xml1=xml2;
		}
		xml1->next=xml;
		xml->prev=xml1;
		return false;
	}

	void XMLObject::addChild(XMLObject* xml) {
		xml->parent=this;
		xml->level=level+1;
		if (child==NULL) child=xml;
		else {
			if (child->addNext(xml)) child=xml;
		}
	}

	void XMLObject::setAttribute(JString* n, JString* s)
	{	XMLObject* xml;
		if (n==NULL) return;
		xml=new XMLObject();
		xml->name=new JString(n);
		if (s!=NULL) xml->data=new JString(s);
		else xml->data=new JString();
		if (attribute==NULL) attribute=xml;
		else {
			if (attribute->addNext(xml)) attribute=xml;
		}
	}

JString* XMLObject::getName()
{
	return name;
}
JString* XMLObject::getData()
{
	if (data==NULL) return NULL;
	return data;
}
JString* XMLObject::getAttribute(JString* s)
{	XMLObject* xml;
	for (xml=attribute;xml!=NULL;xml=xml->getNext()) {
		if (xml->name->equals(s)) {
			if (xml->data==NULL) return NULL;
			return xml->data;
		}
	}
	return NULL;
}
JString* XMLObject::getAttribute(char* s)
{	XMLObject* xml;
	for (xml=attribute;xml!=NULL;xml=xml->getNext()) {
		if (xml->name->equals(s)) {
			if (xml->data==NULL) return NULL;
			return xml->data;
		}
	}
	return NULL;
}
XMLObject* XMLObject::getChild() {
	return child;
}
XMLObject* XMLObject::getChild(JString *s)
{	XMLObject* xml;
	if (s==NULL) return child;
	for (xml=child;xml!=NULL;xml=xml->getNext()) {
		if (xml->name->equals(s)) break;
	}
	return xml;
}
XMLObject* XMLObject::getChild(char *s)
{	XMLObject* xml;
	if (s==NULL) return child;
	for (xml=child;xml!=NULL;xml=xml->getNext()) {
		if (xml->name->equals(s)) break;
	}
	return xml;
}
XMLObject* XMLObject::getNext()
{
	return next;
}
XMLObject* XMLObject::getNext(JString *s)
{	XMLObject* xml;
	if (s==NULL) return next;
	for (xml=next;xml!=NULL;xml=xml->getNext()) {
		if (xml->name->equals(s)) break;
	}
	return xml;
}
XMLObject* XMLObject::getNext(char *s)
{	XMLObject* xml;
	if (s==NULL) return next;
	for (xml=next;xml!=NULL;xml=xml->getNext()) {
		if (xml->name->equals(s)) break;
	}
	return xml;
}
XMLObject* XMLObject::getCDATA() {
	return cdata;
}

void XMLObject::addCDATA(XMLObject* xml) {
	xml->parent=this;
	xml->level=level+1;
	if (cdata==NULL) {
		cdata=xml;
	} else {
		cdata->getData()->add(xml->getData());
		delete xml;
	}
}

bool XMLObject::nameEquals(char *s) {
	if (name->equals(s)) return 1;
	return 0;
}
int XMLObject::getError()
{
	return errorCode;
}


JString* XMLObject::treeString()
{	JString *s;
	JString *s2;
	if (level==0) {
		if (child!=NULL) s=child->treeString();
		else s=NULL;
		return s;
	}
	s=new JString();
	for (int i=0;i<level;i++) {
		s->add("|   ");
	}
	s->add("<");
	s->add(name);
	s->add(">    ");
//	if (data!=NULL && data->length()>0) s->add(data);
	s->add("\n");
	if (child!=NULL) {
		s2=child->treeString();
		s->add(s2);
		delete s2;
	}
	if (next!=NULL)  {
		s2=next->treeString();
		s->add(s2);
		delete s2;
	}
	return s;
}

int XMLObject::parseXML(JString *s)
{	int pos;
	pos=0;
	parseXML(s, pos);
	return getError();
}
