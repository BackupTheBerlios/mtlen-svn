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

#ifndef LIST_INCLUDED
#define LIST_INCLUDED
#include <windows.h>

class ListItem {
private:
	char *id;
	ListItem *prev;
	ListItem *next;
public:
	ListItem(const char *id);
	virtual ~ListItem();
	virtual void	setNext(ListItem *);
	virtual void	setPrev(ListItem *);
	virtual const char *getId();
	virtual ListItem *getPrev();
	virtual ListItem *getNext();
};

class List {
private:
	ListItem *items;
	CRITICAL_SECTION mutex;
	void enterCritical();
	void leaveCritical();
public:
	List();
	~List();
	ListItem *find(const char *);
	ListItem *get(int);
	void	add(ListItem *);
	void	remove(ListItem *);
	void	release(ListItem *);
	void	removeAll();
	void	releaseAll();
};
#endif
