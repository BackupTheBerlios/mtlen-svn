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
#include "List.h"
#include "Utils.h"

ListItem::ListItem(const char *id) {
	this->id = Utils::dupString(id);
	prev = next = NULL;
}

ListItem::ListItem(const char *id1, const char *id2) {
	char *id = NULL;
	int idSize = 0;
	Utils::appendText(&id, &idSize, "%s%s", id1, id2);
	this->id = Utils::dupString(id);
	free(id);
	prev = next = NULL;
}

ListItem::~ListItem() {
	if (id != NULL) delete id;
}

void ListItem::setNext(ListItem *ptr) {
	next = ptr;
}

void ListItem::setPrev(ListItem *ptr) {
	prev = ptr;
}

ListItem * ListItem::getNext() {
	return next;
}

ListItem * ListItem::getPrev() {
	return prev;
}

const char *ListItem::getId() {
	return id;
}

List::List() {
	items = NULL;
	InitializeCriticalSection(&mutex);
}

List::~List() {
	releaseAll();
	DeleteCriticalSection(&mutex);
}

void List::enterCritical() {
	EnterCriticalSection(&mutex);
}

void List::leaveCritical() {
	LeaveCriticalSection(&mutex);
}

ListItem *List::get(int index) {
	enterCritical();
	ListItem *ptr = items;
	for (int i = 0; ptr != NULL && i < index; i++, ptr=ptr->getNext());
	leaveCritical();
	return ptr;
}

ListItem * List::find(const char *id) {
	ListItem *ptr;
	enterCritical();
	for (ptr=items; ptr!=NULL; ptr=ptr->getNext()) {
		if (!strcmp(ptr->getId(), id)) break;
	}
	leaveCritical();
	return ptr;
}

ListItem * List::find(const char *id1, const char *id2) {
	ListItem *ptr;
	char *id = NULL;
	int idSize = 0;
	Utils::appendText(&id, &idSize, "%s%s", id1, id2);
	enterCritical();
	for (ptr=items; ptr!=NULL; ptr=ptr->getNext()) {
		if (!strcmp(ptr->getId(), id)) break;
	}
	leaveCritical();
	free(id);
	return ptr;
}


void List::add(ListItem *item) {
	ListItem *ptr;
	enterCritical();
	for (ptr=items; ptr!=NULL; ptr=ptr->getNext()) {
		if (!strcmp(ptr->getId(), item->getId())) break;
	}
	if (ptr == NULL) {
		item->setPrev(NULL);
		item->setNext(items);
		if (items != NULL) {
			items->setPrev(item);
		}
		items = item;
	}
	leaveCritical();
}

void List::remove(ListItem* item) {
	ListItem *ptr;
	enterCritical();
	for (ptr=items; ptr!=NULL; ptr=ptr->getNext()) {
		if (ptr == item) break;
	}
	if (ptr!=NULL) {
		if (ptr->getPrev()!=NULL) {
			ptr->getPrev()->setNext(ptr->getNext());
		} else {
			items = ptr->getNext();
		}
		if (ptr->getNext()!=NULL) {
			ptr->getNext()->setPrev(ptr->getPrev());
		}
		ptr->setPrev(NULL);
		ptr->setNext(NULL);
	}
	leaveCritical();
}

void List::release(ListItem *item) {
	remove(item);
	delete item;
}

void List::removeAll() {
	ListItem *ptr, *ptr2;
	enterCritical();
	for (ptr=items; ptr!=NULL; ptr=ptr2) {
		ptr2=ptr->getNext();
		ptr->setPrev(NULL);
		ptr->setNext(NULL);
	}
	items = NULL;
	leaveCritical();
}

void List::releaseAll() {
	ListItem *ptr, *ptr2;
	enterCritical();
	for (ptr=items; ptr!=NULL; ptr=ptr2) {
		ptr2=ptr->getNext();
		ptr->setPrev(NULL);
		ptr->setNext(NULL);
		delete ptr;
	}
	items = NULL;
	leaveCritical();
}


