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
#include "mucc_services.h"
#include "Utils.h"
#include "HelperDialog.h"

int MUCCQueryChatGroups(MUCCQUERYRESULT *queryResult) 
{
	ManagerWindow *managerWnd=ManagerWindow::getWindow(queryResult->pszModule);
	if (managerWnd!=NULL) {
		managerWnd->queryResultGroups(queryResult);
	}
	return 1;
}

int MUCCQueryChatRooms(MUCCQUERYRESULT *queryResult) 
{
	ManagerWindow *managerWnd=ManagerWindow::getWindow(queryResult->pszModule);
	if (managerWnd!=NULL) {
		managerWnd->queryResultRooms(queryResult);
	}
	return 1;
}

int MUCCQueryResultContacts(MUCCQUERYRESULT *queryResult) 
{
	ChatWindow *chatWindow=ChatWindow::getWindow(queryResult->pszModule, queryResult->pszParent);
	if (chatWindow!=NULL) {
		chatWindow->queryResultContacts(queryResult);
	}
	return 1;
}

int MUCCQueryUserRooms(MUCCQUERYRESULT *queryResult) 
{
	ManagerWindow *managerWnd=ManagerWindow::getWindow(queryResult->pszModule);
	if (managerWnd!=NULL) {
		managerWnd->queryResultUserRooms(queryResult);
	}
	return 1;
}

int MUCCQueryUserNicks(MUCCQUERYRESULT *queryResult) 
{
	ManagerWindow *managerWnd=ManagerWindow::getWindow(queryResult->pszModule);
	if (managerWnd!=NULL) {
		managerWnd->queryResultUserNick(queryResult);
	}
	return 1;
}

int MUCCQueryUsers(MUCCQUERYRESULT *queryResult) 
{
	ChatWindow *chatWindow=ChatWindow::getWindow(queryResult->pszModule, queryResult->pszParent);
	if (chatWindow!=NULL) {
		chatWindow->queryResultUsers(queryResult);
	}
	return 1;
}

int MUCCQueryResult(WPARAM wParam, LPARAM lParam)
{
	MUCCQUERYRESULT *queryResult=(MUCCQUERYRESULT *)lParam;
	switch (queryResult->iType) {
		case MUCC_EVENT_QUERY_GROUPS:
			MUCCQueryChatGroups(queryResult);
			break;
		case MUCC_EVENT_QUERY_ROOMS:
			MUCCQueryChatRooms(queryResult);
			break;
		case MUCC_EVENT_QUERY_USER_ROOMS:
			MUCCQueryUserRooms(queryResult);
			break;
		case MUCC_EVENT_QUERY_USER_NICKS:
			MUCCQueryUserNicks(queryResult);
			break;
		case MUCC_EVENT_QUERY_USERS:
			MUCCQueryUsers(queryResult);
			break;
		case MUCC_EVENT_QUERY_CONTACTS:
			MUCCQueryResultContacts(queryResult);
			break;
	}
	return 0;
}

int MUCCNewWindow(WPARAM wParam, LPARAM lParam)  
{
	MUCCWINDOW *mucWindow = (MUCCWINDOW *) lParam;
	if (mucWindow->iType == MUCC_WINDOW_CHATROOM) {
		ChatWindow *chat = ChatWindow::getWindow(mucWindow->pszModule, mucWindow->pszID);
		if (chat == NULL) {
			chat = new ChatWindow(mucWindow);
		}
		chat->start();
	} else if (mucWindow->iType == MUCC_WINDOW_CHATLIST) {
		ManagerWindow *manager = ManagerWindow::getWindow(mucWindow->pszModule);
		if (manager == NULL) {
			
		}
		//Utils::log("setting name: %s", mucWindow->pszModuleName);
		manager->setModuleName(mucWindow->pszModuleName);
		manager->start();
	}
	return 0;
}

int MUCCEvent(WPARAM wParam, LPARAM lParam)  
{
	MUCCEVENT* mucEvent = (MUCCEVENT *) lParam;
	ChatWindow * chatWindow;
	switch (mucEvent->iType) {
		case MUCC_EVENT_MESSAGE:
			chatWindow = ChatWindow::getWindow(mucEvent->pszModule, mucEvent->pszID);
			if (chatWindow!=NULL) {
				chatWindow->logEvent(mucEvent);
			}
			break;
		case MUCC_EVENT_TOPIC:
			chatWindow = ChatWindow::getWindow(mucEvent->pszModule, mucEvent->pszID);
			if (chatWindow!=NULL) {
				chatWindow->changeTopic(mucEvent);
			}
			break;
		case MUCC_EVENT_STATUS:
			chatWindow = ChatWindow::getWindow(mucEvent->pszModule, mucEvent->pszID);
			if (chatWindow!=NULL) {
				chatWindow->changePresence(mucEvent);
			}
			break;
		case MUCC_EVENT_INVITATION:
			HelperDialog::acceptDlg(mucEvent);
			break;
		case MUCC_EVENT_ERROR:
			chatWindow = ChatWindow::getWindow(mucEvent->pszModule, mucEvent->pszID);
			if (chatWindow!=NULL) {
				chatWindow->logEvent(mucEvent);
			} else {
				HelperDialog::errorDlg(mucEvent);
			}
			break;
		case MUCC_EVENT_JOIN:
			HelperDialog::joinDlg(mucEvent);
			break;
		case MUCC_EVENT_LEAVE:
			chatWindow = ChatWindow::getWindow(mucEvent->pszModule, mucEvent->pszID);
			if (chatWindow!=NULL) {
				delete chatWindow;
			}
			break;
		case MUCC_EVENT_ROOM_INFO:
			chatWindow = ChatWindow::getWindow(mucEvent->pszModule, mucEvent->pszID);
			if (chatWindow!=NULL) {
				chatWindow->changeRoomInfo(mucEvent);
			}
			break;
	}
	return 0;
}
