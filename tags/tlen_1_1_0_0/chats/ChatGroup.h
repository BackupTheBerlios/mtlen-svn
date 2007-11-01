/*

MUCC Group Chat GUI Plugin for Miranda IM
Copyright (C) 2004  Piotr Piastucki

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
#ifndef CHATGROUP_INCLUDED
#define CHATGROUP_INCLUDED
#include "mucc.h"

class ChatGroup {
private:
	HTREEITEM hItem;
	ChatGroup *		parent;
	ChatGroup *		child;
	ChatGroup *		prev;
	ChatGroup *		next;
	ChatGroup *		listNext;
	char *			name;
	char *			id;
public:
	ChatGroup();
	~ChatGroup();
	ChatGroup *		getPrev();
	ChatGroup *		getNext();
	ChatGroup *		getListNext();
	ChatGroup *		getChild();
	ChatGroup *		getParent();
	HTREEITEM		getTreeItem();
	const char *	getName();
	const char *	getId();
	void			setPrev(ChatGroup *);
	void			setNext(ChatGroup *);
	void			setListNext(ChatGroup *);
	void			addChild(ChatGroup *);
	void			setParent(ChatGroup *);
	void			setName(const char *);
	void			setTreeItem(HTREEITEM );
	void			setId(const char *);
};
#endif