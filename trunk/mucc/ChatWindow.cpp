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

#include "ChatWindow.h"
#include "HelperDialog.h"
#include "Utils.h"
#include "Options.h"

#define WM_TLEN_SMILEY WM_USER+200

static int logPixelSY;
static BOOL CALLBACK LogDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static void __cdecl StartThread(void *vChat);
COLORREF ChatWindow::colorListBg, ChatWindow::colorListText, ChatWindow::colorListGroupText;
HFONT ChatWindow::hListGroupFont=NULL;
HFONT ChatWindow::hListFont=NULL;


ChatWindow *	ChatWindow::list = NULL;
CRITICAL_SECTION ChatWindow::mutex;


ChatWindow::ChatWindow(MUCCWINDOW *mucw)  {
	prev = next = NULL;
	adminWindow = NULL;
	hWnd = NULL;
	module = roomId = roomName = topic = NULL;
	hSplitterPos = 0;
	vSplitterPos = 0;
	wasFirstMessage = 0;
	isStarted = 0;
	options = Options::getChatWindowOptions();
	bBold = bItalic = bUnderline = 0;
	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hEditFont = NULL;
	userMe = NULL;
	users = NULL;
	for (int i=0;i<5;i++) {
		hUserGroups[i] = NULL;
	}
	setModule(mucw->pszModule);
	setRoomId(mucw->pszID);
	setRoomName(mucw->pszName);
	EnterCriticalSection(&mutex);
	setNext(list);
	if (next!=NULL) {
		next->setPrev(this);
	}
	list = this;
	LeaveCriticalSection(&mutex);
}

ChatWindow::~ChatWindow () {
	EnterCriticalSection(&mutex);
	if (adminWindow!=NULL) {
		delete adminWindow;
	}
	if (getPrev()!=NULL) {
		getPrev()->setNext(next);
	} else {
		list = getNext();
	}
	if (getNext()!=NULL) {
		getNext()->setPrev(prev);
	}
	LeaveCriticalSection(&mutex);
	if (hWnd!=NULL) {
		EndDialog(hWnd, 0);
	}
	if (hEvent!=NULL) {
		CloseHandle(hEvent);
	}
	while (users!=NULL) {
		ChatUser *user = users;
		users = users->getNext();
		delete user;
	}
	if (module!=NULL) {
		delete module;
	}
	if (roomId!=NULL) {
		delete roomId;
	}
	if (roomName!=NULL) {
		delete roomName;
	}
	if (topic!=NULL) {
		delete topic;
	}
};

void ChatWindow::setPrev(ChatWindow *prev) {
	this->prev = prev;
}

ChatWindow * ChatWindow::getPrev() {
	return prev;
}

void ChatWindow::setNext(ChatWindow *next) {
	this->next = next;
}

ChatWindow * ChatWindow::getNext() {
	return next;
}

void ChatWindow::setHWND(HWND hWnd) {
	this->hWnd = hWnd;
}

HWND ChatWindow::getHWND() {
	return hWnd;
}

HANDLE ChatWindow::getEvent() {
	return hEvent;
}

void ChatWindow::setAdminWindow(AdminWindow *aw) {
	this->adminWindow = aw;
}

AdminWindow* ChatWindow::getAdminWindow() {
	return adminWindow;
}

void ChatWindow::setModule(const char *module) {
	Utils::copyString(&this->module, module);
}

const char * ChatWindow::getModule() {
	return module;
}

void ChatWindow::setRoomId(const char *roomId) {
	Utils::copyString(&this->roomId, roomId);
}

const char * ChatWindow::getRoomId() {
	return roomId;
}

void ChatWindow::setRoomName(const char *roomName) {
//	char str[300];
	Utils::copyString(&this->roomName, roomName);
//	sprintf(str, "%s %08X", roomName, roomFlags);
//	SetWindowText(hWnd, str);
	SetWindowText(hWnd, roomName);
}

void ChatWindow::setRoomFlags(int flags) {
//	char str[300];
	roomFlags = flags;
//	sprintf(str, "%s %08X", roomName, roomFlags);
//	SetWindowText(hWnd, str);
}

int ChatWindow::getRoomFlags() {
	return roomFlags;
}

const char * ChatWindow::getRoomName() {
	return roomName;
}

void ChatWindow::setOptions(int options) {
	if (options != this->options) {
		this->options = options;
		rebuildLog();
	}
}

int ChatWindow::getOptions() {
	return options;
}

static void __cdecl StartThread(void *vChat) {
	OleInitialize(NULL);
	DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_GROUPCHAT_LOG), NULL, LogDlgProc, (LPARAM) vChat);
	OleUninitialize();
}

static void __cdecl StartAdminThread(void *vChat) {
	ChatWindow *chat = (ChatWindow *)vChat;
	chat->getAdminWindow()->start();
}

int ChatWindow::start() {
	if (isStarted) return 0;
	isStarted = 1;
	Utils::forkThread((void (__cdecl *)(void *))StartThread, 0, (void *) this);
	WaitForSingleObject(hEvent, INFINITE);
	return 0;
}

void ChatWindow::startAdminDialog(int mode) {
	if (adminWindow==NULL) {
		ChatUser *user = getSelectedUser();
		if (user != NULL) {
			adminWindow = new AdminWindow(this, user->getId(), mode);
		} else {
			adminWindow = new AdminWindow(this, "", mode);
		}
		Utils::forkThread((void (__cdecl *)(void *))StartAdminThread, 0, (void *) this);
	}
}
void ChatWindow::addUser(ChatUser *user) {
	user->setNext(users);
	users = user;
}
void ChatWindow::removeUser(ChatUser *user) {
	ChatUser *user2;
	for (user2=users;user2!=NULL;user2=user2->getNext()) {
		if (user2->getNext()==user) break;
	}
	if (user2!=NULL) {
		user2->setNext(user->getNext());
	} else if (users==user) {
		users = user->getNext();
	}
}

ChatUser * ChatWindow::getMe() {
	return userMe;
}

ChatUser * ChatWindow::findUser(const char *userId) {
	ChatUser *user;
	for (user=users;user!=NULL;user=user->getNext()) {
		if (!(strcmp(user->getId(), userId))) break;
	}
	return user;
}

ChatUser * ChatWindow::findUserByNick(const char *nick) {
	ChatUser *user;
	for (user=users;user!=NULL;user=user->getNext()) {
		if (!(strcmp(user->getNick(), nick))) break;
	}
	return user;
}

ChatUser *ChatWindow::getSelectedUser() {
	HTREEITEM hTreeItem = TreeView_GetSelection(GetDlgItem(hWnd, IDC_TREELIST));
	if (hTreeItem!=NULL) {
		TVITEM tvi;
		tvi.mask = TVIF_PARAM;
		tvi.hItem = hTreeItem;
		TreeView_GetItem(GetDlgItem(hWnd, IDC_TREELIST), &tvi);
		return (ChatUser *) tvi.lParam;
	}
	return NULL;
}

int ChatWindow::startPriv() {
	ChatUser *user = getSelectedUser();
	if (user!=NULL) {
		MUCCEVENT mucce;
		mucce.iType = MUCC_EVENT_START_PRIV;
		mucce.pszModule =  getModule();
		mucce.pszID = getRoomId();
		mucce.pszUID = user->getId();
		NotifyEventHooks(hHookEvent, 0,(WPARAM)&mucce);
	}
	return 0;
}

int ChatWindow::unban(const char *id) {
	if (id!=NULL) {
		MUCCEVENT mucce;
		mucce.iType = MUCC_EVENT_UNBAN;
		mucce.pszModule =  getModule();
		mucce.pszID = getRoomId();
		mucce.pszUID = id;
		NotifyEventHooks(hHookEvent, 0,(WPARAM)&mucce);
	}
	return 0;
}


int ChatWindow::kickAndBan(const char *id, int time, const char *reason) {
	if (id!=NULL) {
		MUCCEVENT mucce;
		mucce.iType = MUCC_EVENT_KICK_BAN;
		mucce.pszModule =  getModule();
		mucce.pszID = getRoomId();
		mucce.pszUID = id;
		mucce.dwData = time;
		mucce.pszText = reason;
		NotifyEventHooks(hHookEvent, 0,(WPARAM)&mucce);
	}
	return 0;
}

int ChatWindow::kickAndBan(int time) {
	ChatUser *user = getSelectedUser();
	if (user!=NULL) {
		kickAndBan(user->getId(), time, "");
	}
	return 0;
}

int ChatWindow::setRights(const char *id, int flags) {
	if (id!=NULL) {
		MUCCEVENT mucce;
		mucce.iType = MUCC_EVENT_SET_USER_ROLE;
		mucce.pszModule =  getModule();
		mucce.pszID = getRoomId();
		mucce.pszUID = id;
		mucce.dwFlags = flags;
		NotifyEventHooks(hHookEvent, 0,(WPARAM)&mucce);
	}
	return 0;
}

int ChatWindow::setRights(int flags) {
	ChatUser *user = getSelectedUser();
	if (user!=NULL) {
		setRights(user->getId(), flags);
	}
	return 0;
}

int ChatWindow::getUserGroup(ChatUser *user) {
	int group = 4;
	if (user->getFlags()&MUCC_EF_USER_GLOBALOWNER) {
		group = 0;
	} else if (user->getFlags()&MUCC_EF_USER_OWNER) {
		group = 1;
	} else if (user->getFlags()&MUCC_EF_USER_ADMIN) {
		group = 2;
	} else if (user->getFlags()&MUCC_EF_USER_MODERATOR) {
		group = 3;
	}
	return group;
}

int ChatWindow::changePresence(const MUCCEVENT *event) {
	int i, group, bLogEvent = FALSE;
	const char *groupNames[] = {"Global Owners", "Owners", "Admins", "Moderators", "Users"};
	
	ChatUser *user = findUser(event->pszUID);
	if (event->dwData == ID_STATUS_ONLINE || (user!=NULL && event->dwData!=ID_STATUS_OFFLINE)) {
		Utils::log("new status: %d, %d", event->dwData, user);
		if (user == NULL) {
			user = new ChatUser();
			user->setId(event->pszUID);
			user->setNick(event->pszNick);
			user->setFlags(event->dwFlags);
			user->setMe(event->bIsMe);
			addUser(user);
			if (user->isMe()) {
				userMe = user;
			}
			bLogEvent = TRUE;
		} else {
			group = getUserGroup(user);
			user->setFlags(event->dwFlags);
			TreeView_DeleteItem(GetDlgItem(hWnd, IDC_TREELIST), user->getHTreeItem());
			if (TreeView_GetChild(GetDlgItem(hWnd, IDC_TREELIST), getTreeItem(group))==NULL) {
				TreeView_DeleteItem(GetDlgItem(hWnd, IDC_TREELIST), getTreeItem(group));
				setTreeItem(group, NULL);
			}
		}
		if (user->isMe()) {
			if (user->getFlags() & MUCC_EF_USER_OWNER) {
				EnableWindow(GetDlgItem(hWnd, IDC_TOPIC_BUTTON), TRUE);
			} else {
				EnableWindow(GetDlgItem(hWnd, IDC_TOPIC_BUTTON), FALSE);
			}
		}
		group = getUserGroup(user);
		TVINSERTSTRUCT tvis;
		if (getTreeItem(group)==NULL) {
			for (i=group-1;i>=0;i--) {
				if (getTreeItem(i)!=NULL) break;
			}
			tvis.hParent = NULL;
			if (i>=0) {
				tvis.hInsertAfter = getTreeItem(i);
			} else {
				tvis.hInsertAfter = TVI_FIRST;
			}
			tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN; 
			tvis.item.lParam = (LPARAM) NULL;
			tvis.item.cChildren = 1;
			tvis.item.pszText = (char *)Translate(groupNames[group]);
			tvis.item.state = INDEXTOSTATEIMAGEMASK(1);
			tvis.item.stateMask = TVIS_STATEIMAGEMASK ;
			setTreeItem(group, TreeView_InsertItem(GetDlgItem(hWnd, IDC_TREELIST), &tvis));
		}
		tvis.hParent = getTreeItem(group);
		tvis.hInsertAfter = TVI_SORT;
		tvis.item.mask = TVIF_TEXT  | TVIF_PARAM;
		tvis.item.pszText = (char *)user->getNick();
		tvis.item.lParam = (LPARAM) user;
		user->setHTreeItem(TreeView_InsertItem(GetDlgItem(hWnd, IDC_TREELIST), &tvis));
		TreeView_Expand(GetDlgItem(hWnd, IDC_TREELIST), getTreeItem(group), TVE_EXPAND);
	}
	else {
		if (user != NULL) {
			group = getUserGroup(user);
			TreeView_DeleteItem(GetDlgItem(hWnd, IDC_TREELIST), user->getHTreeItem());
			if (TreeView_GetChild(GetDlgItem(hWnd, IDC_TREELIST), getTreeItem(group))==NULL) {
				TreeView_DeleteItem(GetDlgItem(hWnd, IDC_TREELIST), getTreeItem(group));
				setTreeItem(group, NULL);
			}
			removeUser(user);
			delete user;
			bLogEvent = TRUE;
		}
	}
	if (bLogEvent && wasFirstMessage) {
		logEvent(event);
	}
	return 1;
}
int ChatWindow::changeTopic(const MUCCEVENT *event) {

	SetDlgItemText(hWnd, IDC_TOPIC, event->pszText);
//if (wasFirstMessage) {
		logEvent(event);
//	}
	return 1;
}

int ChatWindow::changeRoomInfo(const MUCCEVENT *event) {

	Utils::log("setting room info !");
	setRoomName(event->pszName);
	setRoomFlags(event->dwFlags);
	return 1;
}

const char * ChatWindow::getFontName(int index) {
	const char *fontNames[] = {"Arial",
							   "Comic Sans MS",
							   "Courier New",
							   "Impact",
							   "Lucida Console",
							   "MS Sans Serif",
							   "Tahoma",
							   "Times New Roman",
							   "Trebuchet MS",
							   "Verdana"};
	if (index>9 || index<0) index = 0;
	return fontNames[index];
}
int	ChatWindow::getFontNameNum() {
	return 10;
}
int ChatWindow::getFontSize(int index) {
	return index+7;
}
int ChatWindow::getFontSizeNum() {
	return 10;
}

void ChatWindow::refreshSettings() {
	SendDlgItemMessage(hWnd, IDC_LOG, EM_SETBKGNDCOLOR , 0, Options::getLogBgColor());
	SendDlgItemMessage(hWnd, IDC_TREELIST, TVM_SETBKCOLOR, 0, Options::getListBgColor());
	eventList.setMaxSize(Options::getLogLimit());
}

void ChatWindow::refreshSettings(int force) {

	if (hListFont!=NULL && !force) {
		return;
	}
	EnterCriticalSection(&mutex);
	if (hListFont!=NULL) {
		DeleteObject(hListFont);
	}
	if (hListGroupFont!=NULL) {
		DeleteObject(hListGroupFont);
	}
	Font * font = Options::getFont(Options::FONT_USERLIST);
	colorListText = font->getColor();
	hListFont = CreateFont (font->getSize(), 0, 0, 0,
							font->getStyle() & Font::BOLD ? FW_BOLD : FW_NORMAL,
							font->getStyle() & Font::ITALIC ? 1 : 0,
							font->getStyle() & Font::UNDERLINE ? 1 : 0, 0,
							font->getCharSet(),
							OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_ROMAN,
                            font->getFace());
	font = Options::getFont(Options::FONT_USERLISTGROUP);
	colorListGroupText = font->getColor();
	hListGroupFont = CreateFont (font->getSize(), 0, 0, 0,
								 font->getStyle() & Font::BOLD ? FW_BOLD : FW_NORMAL,
								 font->getStyle() & Font::ITALIC ? 1 : 0,
								 font->getStyle() & Font::UNDERLINE ? 1 : 0, 0,
								 font->getCharSet(),
								 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_ROMAN,
                                 font->getFace());

	for (ChatWindow *ptr=list;ptr!=NULL;ptr=ptr->getNext()) {
		ptr->refreshSettings();
		InvalidateRect(ptr->getHWND(), NULL, FALSE);
	}
	LeaveCriticalSection(&mutex);
}
HFONT ChatWindow::getListFont() {
	return hListFont;
}

HFONT ChatWindow::getListGroupFont() {
	return hListGroupFont;
}

COLORREF ChatWindow::getListTextColor() {
	return colorListText;
}

COLORREF ChatWindow::getListGroupTextColor() {
	return colorListGroupText;
}

HTREEITEM ChatWindow::getTreeItem(int index) {
	return hUserGroups[index];
}

void ChatWindow::setTreeItem(int index, HTREEITEM hTreeItem) {
	hUserGroups[index]=hTreeItem;
}

int ChatWindow::getDefaultOptions() {
	return FLAG_SHOW_NICKNAMES | FLAG_SHOW_TIMESTAMP | FLAG_FORMAT_ALL | FLAG_LOG_MESSAGES | FLAG_OPT_SENDONENTER;
}

static BOOL CALLBACK EditWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ChatWindow *gcLogInfo;

	switch (msg) {
	case WM_GETDLGCODE:
		return DLGC_WANTALLKEYS; //DLGC_WANTARROWS|DLGC_WANTCHARS|DLGC_HASSETSEL|DLGC_WANTALLKEYS;
	case WM_CHAR:
		if (wParam=='\r' || wParam=='\n') {
			if (((GetKeyState(VK_CONTROL)&0x8000)==0) == ((Options::getChatWindowOptions() & ChatWindow::FLAG_OPT_SENDONENTER) != 0)) {
				PostMessage(GetParent(hwnd), WM_COMMAND, IDOK, 0);
				return FALSE;
			}
		}
		break;
	}
	gcLogInfo = (ChatWindow *) GetWindowLong(GetParent(hwnd), GWL_USERDATA);
	return CallWindowProc(gcLogInfo->oldEditWndProc, hwnd, msg, wParam, lParam);
}

static BOOL CALLBACK LogHSplitterWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ChatWindow *chat;
	chat = (ChatWindow *) GetWindowLong(GetParent(hwnd), GWL_USERDATA);
	switch (msg) {
	case WM_NCHITTEST:
		return HTCLIENT;
	case WM_SETCURSOR:
		SetCursor(LoadCursor(NULL, IDC_SIZENS));//hCursorSizeNS);
		return TRUE;
	case WM_LBUTTONDOWN:
		SetCapture(hwnd);
		return 0;
	case WM_MOUSEMOVE:
		if (GetCapture() == hwnd) {
			HWND hParent;
			RECT rc;
			POINT pt;
			hParent = GetParent(hwnd);
			pt.y = HIWORD(GetMessagePos());
			GetClientRect(hParent, &rc);
			ScreenToClient(hParent, &pt);
			if (pt.y < chat->hSplitterMinTop)
				pt.y = chat->hSplitterMinTop;
			if (rc.bottom-pt.y < chat->hSplitterMinBottom)
				pt.y = rc.bottom-chat->hSplitterMinBottom;
			if (chat->hSplitterPos != rc.bottom-pt.y) {
				chat->hSplitterPos = rc.bottom-pt.y;
				SendMessage(hParent, WM_SIZE, SIZE_RESTORED, (rc.bottom<<16)+rc.right);
			}
		}
		return 0;
	case WM_LBUTTONUP:
		ReleaseCapture();
		return 0;
	}
	return CallWindowProc(chat->oldHSplitterWndProc, hwnd, msg, wParam, lParam);
}

static BOOL CALLBACK LogVSplitterWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ChatWindow *chat;
	chat = (ChatWindow *) GetWindowLong(GetParent(hwnd), GWL_USERDATA);
	switch (msg) {
	case WM_NCHITTEST:
		return HTCLIENT;
	case WM_SETCURSOR:
		SetCursor(LoadCursor(NULL, IDC_SIZEWE));//hCursorSizeNS);
		return TRUE;
	case WM_LBUTTONDOWN:
		SetCapture(hwnd);
		return 0;
	case WM_MOUSEMOVE:
		if (GetCapture() == hwnd) {
			HWND hParent;
			RECT rc;
			POINT pt;
			hParent = GetParent(hwnd);
			pt.x = LOWORD(GetMessagePos());
			GetClientRect(hParent, &rc);
			ScreenToClient(hParent, &pt);
			if (pt.x < chat->vSplitterMinLeft)
				pt.x = chat->vSplitterMinLeft;
			if (rc.right-pt.x < chat->vSplitterMinRight)
				pt.x = rc.right-chat->vSplitterMinRight;
			if (chat->vSplitterPos != rc.right-pt.x) {
				chat->vSplitterPos = rc.right-pt.x;
				SendMessage(hParent, WM_SIZE, SIZE_RESTORED, (rc.bottom<<16)+rc.right);
			}
		}
		return 0;
	case WM_LBUTTONUP:
		ReleaseCapture();
		return 0;
	}
	return CallWindowProc(chat->oldHSplitterWndProc, hwnd, msg, wParam, lParam);
}

static BOOL CALLBACK LogDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int i;
	MUCCEVENT muce;
	ChatWindow *chatWindow;
	chatWindow = (ChatWindow *) GetWindowLong(hwndDlg, GWL_USERDATA);
	switch (msg) {
		case WM_INITDIALOG:
			HDC hdc;
			TranslateDialogDefault(hwndDlg);
			hdc = GetDC(NULL);
		    logPixelSY = GetDeviceCaps(hdc, LOGPIXELSY);
		    ReleaseDC(NULL, hdc);
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM) muccIcon[MUCC_IDI_CHAT]);

			chatWindow = (ChatWindow *) lParam;
			chatWindow->setHWND(hwndDlg);
			ChatWindow::refreshSettings(0);
			chatWindow->oldHSplitterWndProc = (WNDPROC) GetWindowLong(GetDlgItem(hwndDlg, IDC_HSPLIT), GWL_WNDPROC);
			chatWindow->oldVSplitterWndProc = (WNDPROC) GetWindowLong(GetDlgItem(hwndDlg, IDC_VSPLIT), GWL_WNDPROC);
			chatWindow->oldEditWndProc = (WNDPROC) GetWindowLong(GetDlgItem(hwndDlg, IDC_EDIT), GWL_WNDPROC);
			SetWindowLong(GetDlgItem(hwndDlg, IDC_HSPLIT), GWL_WNDPROC, (LONG) LogHSplitterWndProc);
			SetWindowLong(GetDlgItem(hwndDlg, IDC_EDIT), GWL_WNDPROC, (LONG) EditWndProc);
			SetWindowLong(GetDlgItem(hwndDlg, IDC_VSPLIT), GWL_WNDPROC, (LONG) LogVSplitterWndProc);
			ShowWindow(GetDlgItem(hwndDlg, IDC_LIST), SW_HIDE);
			chatWindow->hSplitterMinTop = 90;
			chatWindow->hSplitterMinBottom = 40;
			chatWindow->hSplitterPos=50;
			chatWindow->vSplitterMinLeft = 100;
			chatWindow->vSplitterMinRight = 80;
			chatWindow->vSplitterPos=110;
			SetWindowLong(hwndDlg, GWL_USERDATA, (LONG) chatWindow);
			SetWindowLong(GetDlgItem(hwndDlg, IDC_EDIT), GWL_USERDATA, (LONG) chatWindow);
			chatWindow->refreshSettings();

			SetWindowText(hwndDlg, chatWindow->getRoomName());
			SendDlgItemMessage(hwndDlg, IDC_BOLD, BM_SETIMAGE, IMAGE_ICON, (LPARAM) muccIcon[MUCC_IDI_BOLD]);
			SendDlgItemMessage(hwndDlg, IDC_ITALIC, BM_SETIMAGE, IMAGE_ICON, (LPARAM) muccIcon[MUCC_IDI_ITALIC]);
			SendDlgItemMessage(hwndDlg, IDC_UNDERLINE, BM_SETIMAGE, IMAGE_ICON, (LPARAM) muccIcon[MUCC_IDI_UNDERLINE]);

			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_SETEVENTMASK, 0, ENM_MOUSEEVENTS | ENM_LINK);
			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_SETUNDOLIMIT, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_AUTOURLDETECT, (WPARAM) TRUE, 0);

			/*
			SMADD_GETICON smadd_iconinfo;
            smadd_iconinfo.cbSize = sizeof(smadd_iconinfo);
            smadd_iconinfo.Protocolname = (char *)chatWindow->getModule();
            smadd_iconinfo.SmileySequence = ":)";
            smadd_iconinfo.Smileylength = 0;
            CallService(MS_SMILEYADD_GETSMILEYICON, 0, (LPARAM)&smadd_iconinfo);
			SendDlgItemMessage(hwndDlg, IDC_SMILEYBTN, BM_SETIMAGE, IMAGE_ICON, (LPARAM) smadd_iconinfo.SmileyIcon);
*/
			//	LoadImage(hInst, smadd_iconinfo.SmileyIcon, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
			SendDlgItemMessage(hwndDlg, IDC_OPTIONS, BM_SETIMAGE, IMAGE_ICON, (LPARAM) muccIcon[MUCC_IDI_OPTIONS]);
			SendDlgItemMessage(hwndDlg, IDC_INVITE, BM_SETIMAGE, IMAGE_ICON, (LPARAM) muccIcon[MUCC_IDI_INVITE]);
			SendDlgItemMessage(hwndDlg, IDC_ROOMADMIN, BM_SETIMAGE, IMAGE_ICON, (LPARAM) muccIcon[MUCC_IDI_ADMINISTRATION]);

			SendDlgItemMessage(hwndDlg, IDC_TOPIC_BUTTON, BUTTONSETASFLATBTN, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_ROOMADMIN, BUTTONSETASFLATBTN, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_INVITE, BUTTONSETASFLATBTN, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_BOLD, BUTTONSETASFLATBTN, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_BOLD, BUTTONSETASPUSHBTN, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_ITALIC, BUTTONSETASFLATBTN, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_ITALIC, BUTTONSETASPUSHBTN, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_UNDERLINE, BUTTONSETASFLATBTN, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_UNDERLINE, BUTTONSETASPUSHBTN, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_OPTIONS, BUTTONSETASFLATBTN, 0, 0); // options
			SendDlgItemMessage(hwndDlg, IDC_SMILEYBTN, BUTTONSETASFLATBTN, 0, 0); // options

			SetWindowLong(GetDlgItem(hwndDlg,IDC_TREELIST),GWL_STYLE,GetWindowLong(GetDlgItem(hwndDlg,IDC_TREELIST),GWL_STYLE)|TVS_NOHSCROLL);
			SendDlgItemMessage(hwndDlg,IDC_TREELIST, CCM_SETVERSION,(WPARAM)5,0);
			TreeView_SetImageList(GetDlgItem(hwndDlg, IDC_TREELIST), hImageList, TVSIL_STATE);
			TreeView_SetItemHeight(GetDlgItem(hwndDlg, IDC_TREELIST), 16);
			TreeView_SetIndent(GetDlgItem(hwndDlg, IDC_TREELIST), 16);
			for(i=0;i<chatWindow->getFontSizeNum();i++) {
				char str[10];
				sprintf(str, "%d", chatWindow->getFontSize(i));
				int n = SendDlgItemMessage(hwndDlg, IDC_FONTSIZE, CB_ADDSTRING, 0, (LPARAM) str);
				SendDlgItemMessage(hwndDlg, IDC_FONTSIZE, CB_SETITEMDATA, n, chatWindow->getFontSize(i));
				if (i==5) {
					SendDlgItemMessage(hwndDlg, IDC_FONTSIZE, CB_SETCURSEL, n, 0);
				}
			}
			for(i=0;i<chatWindow->getFontNameNum();i++) {
				int n = SendDlgItemMessage(hwndDlg, IDC_FONT, CB_ADDSTRING, 0, (LPARAM) chatWindow->getFontName(i));
				SendDlgItemMessage(hwndDlg, IDC_FONT, CB_SETITEMDATA, n, i);
				if (i==0) {
					SendDlgItemMessage(hwndDlg, IDC_FONT, CB_SETCURSEL, n, 0);
				}
			}
			chatWindow->setFont(0, 12, 0, 0, 0, 0);
			SetWindowPos(hwndDlg, NULL, 0, 0, 540, 370, SWP_NOMOVE | SWP_NOZORDER);
			SetFocus(GetDlgItem(hwndDlg, IDC_EDIT));
			SetEvent(chatWindow->getEvent());
			return FALSE;
		break;
		case WM_SIZING:
			RECT *rc;
			rc = (RECT *)lParam;
			if (rc->right-rc->left<380) {
				if (wParam == WMSZ_RIGHT || wParam == WMSZ_BOTTOMRIGHT || wParam == WMSZ_TOPRIGHT) {
					rc->right=rc->left+380;
				} else {
					rc->left=rc->right-380;
				}
			}
			if (rc->bottom-rc->top<160) {
				if (wParam == WMSZ_BOTTOM || wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_BOTTOMRIGHT) {
					rc->bottom=rc->top+160;
				} else {
					rc->top=rc->bottom-160;
				}
			}
			return TRUE;
		case WM_SIZE:
			if (wParam==SIZE_RESTORED || wParam==SIZE_MAXIMIZED) {
				int dlgWidth, dlgHeight;
				HDWP hdwp;
				dlgWidth = LOWORD(lParam);
				dlgHeight = HIWORD(lParam);
				if (dlgHeight-chatWindow->hSplitterPos < chatWindow->hSplitterMinTop) {
					chatWindow->hSplitterPos = dlgHeight-chatWindow->hSplitterMinTop;
				}
				if (dlgWidth-chatWindow->vSplitterPos < chatWindow->vSplitterMinLeft) {
					chatWindow->vSplitterPos = dlgWidth-chatWindow->vSplitterMinLeft;
				}

				hdwp = BeginDeferWindowPos(16);
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_TOPIC), 0, 70, 7, dlgWidth-140, 18, SWP_NOZORDER);
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_LOG), 0, 3, 30, dlgWidth-(chatWindow->vSplitterPos)-5, dlgHeight-(chatWindow->hSplitterPos)-30-26-2, SWP_NOZORDER);
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_TREELIST), 0, dlgWidth-(chatWindow->vSplitterPos)+2, 30, (chatWindow->vSplitterPos)-5, dlgHeight-(chatWindow->hSplitterPos)-30-26-2, SWP_NOZORDER);
//				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_LIST), 0, dlgWidth-(chatWindow->vSplitterPos)+2, 30, (chatWindow->vSplitterPos)-5, dlgHeight-(chatWindow->hSplitterPos)-30-26-2, SWP_NOZORDER);
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_EDIT), 0, 3, dlgHeight-(chatWindow->hSplitterPos)+2, dlgWidth-6, (chatWindow->hSplitterPos)-5, SWP_NOZORDER);
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_INVITE), 0, dlgWidth-27, dlgHeight-(chatWindow->hSplitterPos)-26, 24, 24, SWP_NOZORDER);
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_ROOMADMIN), 0, dlgWidth-55, dlgHeight-(chatWindow->hSplitterPos)-26, 24, 24, SWP_NOZORDER);
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_BOLD), 0, 3, dlgHeight-(chatWindow->hSplitterPos)-26, 24, 24, SWP_NOZORDER );
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_ITALIC), 0, 27, dlgHeight-(chatWindow->hSplitterPos)-26, 24, 24, SWP_NOZORDER );
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_UNDERLINE), 0, 51, dlgHeight-(chatWindow->hSplitterPos)-26, 24, 24, SWP_NOZORDER );
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_COLOR), 0, 76, dlgHeight-(chatWindow->hSplitterPos)-25, 22, 22, SWP_NOZORDER );
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_FONT), 0, 100, dlgHeight-(chatWindow->hSplitterPos)-24, 110, 13, SWP_NOZORDER);
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_FONTSIZE), 0, 213, dlgHeight-(chatWindow->hSplitterPos)-24, 38, 13, SWP_NOZORDER);
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_SMILEYBTN), 0, 254, dlgHeight-(chatWindow->hSplitterPos)-26, 24, 24, SWP_NOZORDER);
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_OPTIONS), 0, 285, dlgHeight-(chatWindow->hSplitterPos)-26, 24, 24, SWP_NOZORDER);
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_VSPLIT), 0, dlgWidth-(chatWindow->vSplitterPos)-2, 30, 4, dlgHeight-(chatWindow->hSplitterPos)-30-26-2, SWP_NOZORDER);
				hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_HSPLIT), 0, 4, dlgHeight-(chatWindow->hSplitterPos)-2, dlgWidth-8, 4, SWP_NOZORDER);
				EndDeferWindowPos(hdwp);
			}
		break;
		case WM_SYSCOMMAND:
			if (wParam == SC_CLOSE) {
				SendMessage(hwndDlg, WM_CLOSE, 1, 0);
//				muce.iType = MUCC_EVENT_LEAVE;
//				muce.pszModule =  chatWindow->getModule();
//				muce.pszID = chatWindow->getRoomId();
//				NotifyEventHooks(hHookEvent, 0,(WPARAM)&muce);
//				delete chatWindow;
				return TRUE;
			}
			break;
		case WM_CLOSE:
			if (wParam != 1) { // esc
				return FALSE;
			}
			muce.iType = MUCC_EVENT_LEAVE;
			muce.pszModule =  chatWindow->getModule();
			muce.pszID = chatWindow->getRoomId();
			NotifyEventHooks(hHookEvent, 0,(WPARAM)&muce);
			delete chatWindow;
			break;
		case WM_TLEN_SMILEY:
			if (ServiceExists(MS_SMILEYADD_REPLACESMILEYS)) {
				SMADD_RICHEDIT2 smre;
				smre.cbSize = sizeof(SMADD_RICHEDIT2);
				smre.hwndRichEditControl = GetDlgItem(hwndDlg, IDC_LOG);
				smre.Protocolname = (char *)chatWindow->getModule();
				smre.rangeToReplace = NULL;
				smre.useSounds = FALSE;
				smre.disableRedraw = FALSE;
				CallService(MS_SMILEYADD_REPLACESMILEYS, 0, (LPARAM) &smre);
			}
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_OPTIONS:
					{
						HMENU hMenu;
						RECT rc;
						int iSelection;
						hMenu=GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(IDR_CHATOPTIONS)),0);
						CallService(MS_LANGPACK_TRANSLATEMENU,(WPARAM)hMenu,0);
						GetWindowRect(GetDlgItem(hwndDlg, IDC_OPTIONS), &rc);
						CheckMenuItem(hMenu, ID_OPTIONMENU_SHOWNICKNAMES, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_SHOW_NICKNAMES ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_MSGINNEWLINE, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_MSGINNEWLINE ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_SHOWDATE, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_SHOW_DATE ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_SHOWTIMESTAMP, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_SHOW_TIMESTAMP ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_SHOWSECONDS, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_SHOW_SECONDS ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_USELONGDATE, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_LONG_DATE ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_FORMATFONT, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_FORMAT_FONT ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_FORMATSIZE, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_FORMAT_SIZE  ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_FORMATCOLOR, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_FORMAT_COLOR ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_FORMATSTYLE, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_FORMAT_STYLE ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_LOGMESSAGES, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_LOG_MESSAGES  ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_LOGJOINED, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_LOG_JOINED ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_LOGLEFT, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_LOG_LEFT ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(hMenu, ID_OPTIONMENU_LOGTOPIC, MF_BYCOMMAND | chatWindow->getOptions()&ChatWindow::FLAG_LOG_TOPIC ? MF_CHECKED : MF_UNCHECKED);
						iSelection = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_TOPALIGN | TPM_LEFTALIGN, rc.left, rc.bottom, 0, hwndDlg, NULL);
						DestroyMenu(hMenu);
						switch (iSelection) {
							case ID_OPTIONMENU_SHOWNICKNAMES:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_SHOW_NICKNAMES);
								break;
							case ID_OPTIONMENU_MSGINNEWLINE:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_MSGINNEWLINE);
								break;
							case ID_OPTIONMENU_SHOWDATE:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_SHOW_DATE);
								break;
							case ID_OPTIONMENU_SHOWTIMESTAMP:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_SHOW_TIMESTAMP);
								break;
							case ID_OPTIONMENU_SHOWSECONDS:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_SHOW_SECONDS);
								break;
							case ID_OPTIONMENU_USELONGDATE:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_LONG_DATE);
								break;
							case ID_OPTIONMENU_FORMATFONT:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_FORMAT_FONT);
								break;
							case ID_OPTIONMENU_FORMATSIZE:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_FORMAT_SIZE);
								break;
							case ID_OPTIONMENU_FORMATCOLOR:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_FORMAT_COLOR);
								break;
							case ID_OPTIONMENU_FORMATSTYLE:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_FORMAT_STYLE);
								break;
							case ID_OPTIONMENU_LOGMESSAGES:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_LOG_MESSAGES);
								break;
							case ID_OPTIONMENU_LOGJOINED:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_LOG_JOINED);
								break;
							case ID_OPTIONMENU_LOGLEFT:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_LOG_LEFT);
								break;
							case ID_OPTIONMENU_LOGTOPIC:
								chatWindow->setOptions(chatWindow->getOptions()^ChatWindow::FLAG_LOG_TOPIC);
								break;
							case ID_OPTIONMENU_SAVEDEFAULT:
								Options::setChatWindowOptions(chatWindow->getOptions());
								Options::saveSettings();
								break;
						}
					}
					break;
				case IDC_INVITE:
					{
						MUCCEVENT muce;
						muce.cbSize = sizeof(MUCCEVENT);
						muce.iType = MUCC_EVENT_QUERY_CONTACTS;
						muce.pszModule = chatWindow->getModule();
						muce.pszID = chatWindow->getRoomId();
						NotifyEventHooks(hHookEvent, 0,(WPARAM)&muce);
					}
					break;
				case IDC_ROOMADMIN:
					{
						MUCCEVENT muce;
						HMENU hMenu;
						RECT rc;
						int iSelection;
						GetWindowRect(GetDlgItem(hwndDlg, IDC_ROOMADMIN), &rc);
						hMenu=GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(IDR_CHATOPTIONS)), 2);
						if (chatWindow->getMe()!=NULL) {
							if (chatWindow->getMe()->getFlags() & (MUCC_EF_USER_OWNER | MUCC_EF_USER_ADMIN)) {
								EnableMenuItem(hMenu, ID_ADMINMENU_ADMIN, MF_BYCOMMAND | MF_ENABLED);
								EnableMenuItem(hMenu, ID_ADMINMENU_BROWSE, MF_BYCOMMAND | MF_ENABLED);
							}
							if (chatWindow->getMe()->getFlags() & MUCC_EF_USER_OWNER) {
								EnableMenuItem(hMenu, ID_ADMINMENU_DESTROY, MF_BYCOMMAND | MF_ENABLED);
							}
						}
						iSelection = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_TOPALIGN | TPM_LEFTALIGN, rc.left, rc.bottom, 0, hwndDlg, NULL);
						DestroyMenu(hMenu);
						switch (iSelection) {
						case ID_ADMINMENU_DESTROY:
							muce.cbSize = sizeof(MUCCEVENT);
							muce.iType = MUCC_EVENT_REMOVE_ROOM;
							muce.pszModule = chatWindow->getModule();
							muce.pszID = chatWindow->getRoomId();
							NotifyEventHooks(hHookEvent, 0,(WPARAM)&muce);
							delete chatWindow;
							break;
						case ID_ADMINMENU_ADMIN:
							chatWindow->startAdminDialog(ChatWindow::ADMIN_MODE_KICK);
							break;
						case ID_ADMINMENU_BROWSE:
							chatWindow->startAdminDialog(ChatWindow::ADMIN_MODE_ROLE);
							break;
						}
					}
					break;
				case IDC_TOPIC_BUTTON:
					HelperDialog::topicDlg(chatWindow);
					break;
				case IDCANCEL:
					return TRUE;
				case IDOK:
					{
						char text[2048];
						GetDlgItemText(hwndDlg, IDC_EDIT, text, sizeof(text));
						SetDlgItemText(hwndDlg, IDC_EDIT, "");
						muce.iType = MUCC_EVENT_MESSAGE;
						muce.pszModule =  chatWindow->getModule();
						muce.pszID = chatWindow->getRoomId();
						muce.pszText = text;
						muce.iFont = chatWindow->getFont();
						muce.iFontSize = chatWindow->getFontSize();
						muce.dwFlags = chatWindow->getFontStyle();
						muce.color = chatWindow->getFontColor();
						NotifyEventHooks(hHookEvent, 0,(WPARAM)&muce);
					}
					break;
				case IDC_BOLD:
				case IDC_ITALIC:
				case IDC_UNDERLINE:
					if (HIWORD(wParam)==BN_CLICKED) {
						chatWindow->setFont((int) SendDlgItemMessage(hwndDlg, IDC_FONT, CB_GETITEMDATA, SendDlgItemMessage(hwndDlg, IDC_FONT, CB_GETCURSEL, 0, 0), 0),
												(int) SendDlgItemMessage(hwndDlg, IDC_FONTSIZE, CB_GETITEMDATA, SendDlgItemMessage(hwndDlg, IDC_FONTSIZE, CB_GETCURSEL, 0, 0), 0),
												IsDlgButtonChecked(hwndDlg, IDC_BOLD),
												IsDlgButtonChecked(hwndDlg, IDC_ITALIC),
												IsDlgButtonChecked(hwndDlg, IDC_UNDERLINE),
												(COLORREF) SendDlgItemMessage(hwndDlg, IDC_COLOR, CPM_GETCOLOUR,0,0));
					}
				case IDC_FONT:
				case IDC_FONTSIZE:
					if (HIWORD(wParam)==CBN_SELCHANGE) {
						chatWindow->setFont((int) SendDlgItemMessage(hwndDlg, IDC_FONT, CB_GETITEMDATA, SendDlgItemMessage(hwndDlg, IDC_FONT, CB_GETCURSEL, 0, 0), 0),
												(int) SendDlgItemMessage(hwndDlg, IDC_FONTSIZE, CB_GETITEMDATA, SendDlgItemMessage(hwndDlg, IDC_FONTSIZE, CB_GETCURSEL, 0, 0), 0),
												IsDlgButtonChecked(hwndDlg, IDC_BOLD),
												IsDlgButtonChecked(hwndDlg, IDC_ITALIC),
												IsDlgButtonChecked(hwndDlg, IDC_UNDERLINE),
												(COLORREF) SendDlgItemMessage(hwndDlg, IDC_COLOR, CPM_GETCOLOUR,0,0));
					}
					break;
				case IDC_COLOR:
					if (HIWORD(wParam)==CPN_COLOURCHANGED) {
						InvalidateRect(GetDlgItem(hwndDlg, IDC_EDIT), NULL, FALSE);
						chatWindow->setFont((int) SendDlgItemMessage(hwndDlg, IDC_FONT, CB_GETITEMDATA, SendDlgItemMessage(hwndDlg, IDC_FONT, CB_GETCURSEL, 0, 0), 0),
												(int) SendDlgItemMessage(hwndDlg, IDC_FONTSIZE, CB_GETITEMDATA, SendDlgItemMessage(hwndDlg, IDC_FONTSIZE, CB_GETCURSEL, 0, 0), 0),
												IsDlgButtonChecked(hwndDlg, IDC_BOLD),
												IsDlgButtonChecked(hwndDlg, IDC_ITALIC),
												IsDlgButtonChecked(hwndDlg, IDC_UNDERLINE),
												(COLORREF) SendDlgItemMessage(hwndDlg, IDC_COLOR, CPM_GETCOLOUR,0,0));
					}
					break;

			}
			break;
		case WM_CTLCOLOREDIT:
			if ((HWND) lParam == GetDlgItem(hwndDlg, IDC_EDIT)) {
				SetTextColor((HDC) wParam, (COLORREF) SendDlgItemMessage(hwndDlg, IDC_COLOR, CPM_GETCOLOUR,0,0));
				SetBkColor((HDC) wParam, Options::getInputBgColor());
				//SelectObject((HDC) wParam, Options::getInputBgBrush());
				return (BOOL) Options::getInputBgBrush();
			}

			break;
		case WM_NOTIFY:
			LPNMHDR pNmhdr;
			pNmhdr = (LPNMHDR)lParam;
			if (pNmhdr->idFrom = IDC_TREELIST) {
				switch (pNmhdr->code) {
				case TVN_ITEMEXPANDING:
					{
						LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) lParam;
						if (pnmtv->action==TVE_COLLAPSE) {
							SetWindowLong(hwndDlg,DWL_MSGRESULT, TRUE);
							return TRUE;
						}
					}
				break;
				case NM_RCLICK:
					{
						TVHITTESTINFO hti;
						hti.pt.x=(short)LOWORD(GetMessagePos());
						hti.pt.y=(short)HIWORD(GetMessagePos());
						ScreenToClient(pNmhdr->hwndFrom,&hti.pt);
						if(TreeView_HitTest(pNmhdr->hwndFrom, &hti) && hti.flags&TVHT_ONITEM) {
							TVITEM tvi = {0};
							tvi.mask = TVIF_PARAM|TVIF_HANDLE;
							tvi.hItem = hti.hItem;
							TreeView_GetItem(pNmhdr->hwndFrom, &tvi);
							ChatUser *user = (ChatUser *) tvi.lParam;
							if (user!=NULL) {
								TreeView_Select(pNmhdr->hwndFrom, tvi.hItem, TVGN_CARET);
								if (!user->isMe()) {
									HMENU hMenu;
									int iSelection;
									hMenu = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(IDR_CHATOPTIONS)),1);
									CallService(MS_LANGPACK_TRANSLATEMENU,(WPARAM)hMenu,0);
									if (chatWindow->getMe()!=NULL) {
										if (chatWindow->getMe()->getFlags() & MUCC_EF_USER_OWNER) {
											EnableMenuItem(hMenu, 2, MF_BYPOSITION | MF_ENABLED);
											EnableMenuItem(hMenu, 3, MF_BYPOSITION | MF_ENABLED);
											EnableMenuItem(hMenu, ID_USERMENU_ADMINISTRATION, MF_BYCOMMAND | MF_ENABLED);

										} else if (chatWindow->getMe()->getFlags() & MUCC_EF_USER_ADMIN) {
											if (!(user->getFlags() & (MUCC_EF_USER_OWNER | MUCC_EF_USER_OWNER))) {
												EnableMenuItem(hMenu, 2, MF_BYPOSITION | MF_ENABLED);
												//EnableMenuItem(hMenu, 3, MF_BYPOSITION | MF_ENABLED);
												EnableMenuItem(hMenu, ID_USERMENU_ADMINISTRATION, MF_BYCOMMAND | MF_ENABLED);
											}
										}
									}
									//CheckMenuItem(hMenu, ID_USERMENU_MESSAGE, MF_BYCOMMAND | MF_CHECKED : MF_UNCHECKED);
									//EnableMenuItem(hMenu, ID_USERMENU_MESSAGE, MF_BYCOMMAND | MF_GRAYED);
									iSelection = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_TOPALIGN | TPM_LEFTALIGN, (short)LOWORD(GetMessagePos()), (short)HIWORD(GetMessagePos()), 0, hwndDlg, NULL);
									DestroyMenu(hMenu);
									if (iSelection == ID_USERMENU_MESSAGE) {
										chatWindow->startPriv();
									} else if (iSelection >= ID_USERMENU_KICK_NO_BAN && iSelection <= ID_USERMENU_KICK_BAN_4_W) {
										int banTime[] = {0, 1, 5, 15, 30, 60, 360, 1440, 4320, 10080, 20160, 40320};
										chatWindow->kickAndBan(banTime[iSelection-ID_USERMENU_KICK_NO_BAN] *60);
									} else if (iSelection == ID_USERMENU_RIGHTS_MEMBER) {
										chatWindow->setRights(MUCC_EF_USER_MEMBER);
									} else if (iSelection == ID_USERMENU_RIGHTS_ADMIN) {
										chatWindow->setRights(MUCC_EF_USER_ADMIN);
									} else if (iSelection == ID_USERMENU_RIGHTS_NO) {
										chatWindow->setRights(0);
									} else if (iSelection == ID_USERMENU_ADMINISTRATION) {
										chatWindow->startAdminDialog(ChatWindow::ADMIN_MODE_KICK);
									}
								}
							}
						}

					}
					break;
				case NM_CUSTOMDRAW:
					LPNMTVCUSTOMDRAW pCustomDraw = (LPNMTVCUSTOMDRAW)lParam;
					switch (pCustomDraw->nmcd.dwDrawStage) {
						case CDDS_PREPAINT:
							SetWindowLong(hwndDlg,DWL_MSGRESULT,CDRF_NOTIFYITEMDRAW);
							return TRUE;
						case CDDS_ITEMPREPAINT:
							{
								TVITEM tvi;;
								HICON hIcon;
								char str[200];
								RECT rc = pCustomDraw->nmcd.rc;
								tvi.mask = TVIF_HANDLE | TVIF_STATE | TVIF_TEXT | TVIF_PARAM;
								tvi.pszText = str;
								tvi.cchTextMax = sizeof(str);
								tvi.hItem = (HTREEITEM)pCustomDraw->nmcd.dwItemSpec;
								TreeView_GetItem(pCustomDraw->nmcd.hdr.hwndFrom, &tvi);
								ChatUser * user= (ChatUser *)pCustomDraw->nmcd.lItemlParam;
								hIcon = NULL;
								pCustomDraw->clrTextBk = Options::getListBgColor();
								switch (pCustomDraw->iLevel) {
									case 0:
										pCustomDraw->clrText = ChatWindow::getListGroupTextColor();
										SelectObject(pCustomDraw->nmcd.hdc, ChatWindow::getListGroupFont());
										break;
									case 1:
										pCustomDraw->clrText = ChatWindow::getListTextColor();
										if (pCustomDraw->nmcd.uItemState & CDIS_SELECTED) { // selected (CDIS_FOCUS |
											pCustomDraw->clrTextBk = 0xDAC8C2;
										} else {
											pCustomDraw->clrTextBk = Options::getListBgColor();
										}
										if (user!=NULL) {
											if (user->getFlags()&MUCC_EF_USER_GLOBALOWNER) {
												hIcon = muccIcon[MUCC_IDI_U_GLOBALOWNER];
											} else if (user->getFlags()&MUCC_EF_USER_OWNER) {
												hIcon = muccIcon[MUCC_IDI_U_OWNER];
											} else if (user->getFlags()&MUCC_EF_USER_ADMIN) {
												hIcon = muccIcon[MUCC_IDI_U_ADMIN];
											} else if (user->getFlags()&MUCC_EF_USER_REGISTERED) {
												hIcon = muccIcon[MUCC_IDI_U_REGISTERED];
											} 
										}
										SelectObject(pCustomDraw->nmcd.hdc, ChatWindow::getListFont());
										break;
								}
								if (rc.bottom-rc.top!=0 && rc.right-rc.left!=0) {
									HBRUSH hBr;
									hBr = CreateSolidBrush(pCustomDraw->clrTextBk);
									FillRect(pCustomDraw->nmcd.hdc, &rc, hBr);
									DeleteObject(hBr);
									if (hIcon!=NULL) {
										DrawIconEx(pCustomDraw->nmcd.hdc, rc.left, rc.top, hIcon, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0, NULL, DI_NORMAL);
									}
									SetBkMode(pCustomDraw->nmcd.hdc, TRANSPARENT);
									SetTextColor(pCustomDraw->nmcd.hdc, pCustomDraw->clrText);
									TextOut(pCustomDraw->nmcd.hdc,
											rc.left+pCustomDraw->iLevel*GetSystemMetrics(SM_CXSMICON)+2,
											rc.top, tvi.pszText, strlen(tvi.pszText));
								}
								SetWindowLong(hwndDlg,DWL_MSGRESULT, CDRF_SKIPDEFAULT );
								return TRUE;
							}
						}
					break;
				}
			}
			break;
	}
	return FALSE;
}



char *JabberRtfEscape(char *str) {
	char *escapedStr;
	int size;
	char *p, *q;

	if (str == NULL)
		return NULL;

	for (p=str,size=0; *p!='\0'; p++) {
		if (*p=='\\' || *p=='{' || *p=='}')
			size += 2;
		else if (*p=='\n' || *p=='\t')
			size += 5;
		else
			size++;
	}

	if ((escapedStr=(char *)malloc(size+1)) == NULL)
		return NULL;

	for (p=str,q=escapedStr; *p!='\0'; p++) {
		if (strchr("\\{}", *p) != NULL) {
			*q++ = '\\';
			*q++ = *p;
		}
		else if (*p == '\n') {
			strcpy(q, "\\par ");
			q += 5;
		}
		else if (*p == '\t') {
			strcpy(q, "\\tab ");
			q += 5;
		}
		else {
			*q++ = *p;
		}
	}
	*q = '\0';

	return escapedStr;
}

void JabberStringAppend(char **str, int *sizeAlloced, const char *fmt, ...) {
	va_list vararg;
	char *p;
	int size, len;

	if (str == NULL) return;

	if (*str==NULL || *sizeAlloced<=0) {
		*sizeAlloced = size = 2048;
		*str = (char *) malloc(size);
		len = 0;
	}
	else {
		len = strlen(*str);
		size = *sizeAlloced - strlen(*str);
	}

	p = *str + len;
	va_start(vararg, fmt);
	while (_vsnprintf(p, size, fmt, vararg) == -1) {
		size += 2048;
		(*sizeAlloced) += 2048;
		*str = (char *) realloc(*str, *sizeAlloced);
		p = *str + len;
	}
	va_end(vararg);
}

void ChatWindow::rebuildLog() {
	int	nMin, nMax;
	HWND hwndLog;
	SetDlgItemText(getHWND(), IDC_LOG, "");
	for (ChatEvent* event=eventList.getEvents();event!=NULL;event=event->getNext()) {
		if (event->getEvent()->iType == MUCC_EVENT_MESSAGE) {
			appendMessage(event->getEvent());
		} else {
			appendEvent(event->getEvent());
		}
	}
	if (ServiceExists(MS_SMILEYADD_REPLACESMILEYS)) PostMessage(getHWND(), WM_TLEN_SMILEY, 0, 0);
	hwndLog = GetDlgItem(getHWND(), IDC_LOG);
	GetScrollRange(hwndLog, SB_VERT, &nMin, &nMax);
	SetScrollPos(hwndLog, SB_VERT, nMax, TRUE);
	PostMessage(hwndLog, WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, nMax), (LPARAM) NULL);
}
int ChatWindow::logEvent(const MUCCEVENT *event) {
	int	nMin, nMax;
	HWND hwndLog;
	if (event->iType != MUCC_EVENT_ERROR) {
		if (eventList.addEvent(event)) {
			rebuildLog();
			return 1;
		}
	}
	if (event->iType == MUCC_EVENT_MESSAGE) {
		wasFirstMessage = 1;
		appendMessage(event);
	} else {
		appendEvent(event);
	}
	if (ServiceExists(MS_SMILEYADD_REPLACESMILEYS)) PostMessage(getHWND(), WM_TLEN_SMILEY, 0, 0);
	hwndLog = GetDlgItem(getHWND(), IDC_LOG);
	GetScrollRange(hwndLog, SB_VERT, &nMin, &nMax);
	SetScrollPos(hwndLog, SB_VERT, nMax, TRUE);
	PostMessage(hwndLog, WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, nMax), (LPARAM) NULL);
	return 1;
}
int ChatWindow::appendEvent(const MUCCEVENT *event) {
	char *rtf, *escapedStr;
	char timestampStr[100], str[512];
	Font *fontTimestamp, *fontPrefix, *fontMessage;
	int msgSize, iFontSize, bItalic, bBold, bUnderline;
	HWND hwndLog;
	CHARRANGE sel;
	SETTEXTEX stt;
	DBTIMETOSTRING dbtts;
	DWORD color;

	if (event->iType==MUCC_EVENT_STATUS && event->dwData==ID_STATUS_ONLINE && !(getOptions() & FLAG_LOG_JOINED)) {
		return 0;
	} else if (event->iType==MUCC_EVENT_STATUS && event->dwData==ID_STATUS_OFFLINE && !(getOptions() & FLAG_LOG_LEFT)) {
		return 0;
	} else if (event->iType==MUCC_EVENT_TOPIC && !(getOptions() & FLAG_LOG_TOPIC)) {
		return 0;
	}/* else if (event->iType==MUCC_EVENT_TOPIC && !(getFlags() & FLAG_LOG_TOPIC) {
		return 0;
	} */
	fontTimestamp = FontList::getFont(FontList::FONT_TIMESTAMP);
	fontPrefix = FontList::getFont(FontList::FONT_ERROR);
	rtf = NULL;
	JabberStringAppend(&rtf, &msgSize, "{\\rtf1\\ansi\\deff0{\\fonttbl");
	if (event->iType == MUCC_EVENT_ERROR) {
		fontMessage = FontList::getFont(FontList::FONT_ERROR);
		escapedStr = JabberRtfEscape((char *)event->pszText);
	} else if (event->iType == MUCC_EVENT_STATUS) {
		if (event->dwData == ID_STATUS_ONLINE) {
			fontMessage = FontList::getFont(FontList::FONT_JOINED);
			_snprintf(str, sizeof(str), "%s has joined.", event->pszNick);
		} else {
			fontMessage = FontList::getFont(FontList::FONT_LEFT);
			_snprintf(str, sizeof(str), "%s has left.", event->pszNick);
		}
		escapedStr = JabberRtfEscape(str);
	} else if (event->iType == MUCC_EVENT_TOPIC) {
		fontMessage = FontList::getFont(FontList::FONT_TOPIC);
		_snprintf(str, sizeof(str), "The topic is %s.", event->pszText);
		escapedStr = JabberRtfEscape(str);
	} else {
		return 0;
	}
	JabberStringAppend(&rtf, &msgSize, "{\\f0\\fnil\\fcharset%u %s;}", fontTimestamp->getCharSet(), fontTimestamp->getFace());
	JabberStringAppend(&rtf, &msgSize, "{\\f1\\fnil\\fcharset%u %s;}", fontPrefix->getCharSet(), fontPrefix->getFace());
	JabberStringAppend(&rtf, &msgSize, "{\\f2\\fnil\\fcharset%u %s;}", fontMessage->getCharSet(), fontMessage->getFace());
	JabberStringAppend(&rtf, &msgSize, "}{\\colortbl ");
	color = fontTimestamp->getColor();
	JabberStringAppend(&rtf, &msgSize, "\\red%d\\green%d\\blue%d;", color&0xFF, (color>>8)&0xFF, (color>>16)&0xFF);
	color = fontPrefix->getColor();
	JabberStringAppend(&rtf, &msgSize, "\\red%d\\green%d\\blue%d;", color&0xFF, (color>>8)&0xFF, (color>>16)&0xFF);
	color = fontMessage->getColor();
	JabberStringAppend(&rtf, &msgSize, "\\red%d\\green%d\\blue%d;", color&0xFF, (color>>8)&0xFF, (color>>16)&0xFF);
	JabberStringAppend(&rtf, &msgSize, "}");
	if (event->iType == MUCC_EVENT_ERROR) {
		bBold = fontPrefix->getStyle() & Font::BOLD ? 1 : 0;
		bItalic = fontPrefix->getStyle() & Font::ITALIC ? 1 : 0;
		bUnderline = fontPrefix->getStyle() & Font::UNDERLINE ? 1 : 0;
		iFontSize = fontPrefix->getSize();
		iFontSize = 2 * abs((signed char)iFontSize) * 74 / logPixelSY;
		JabberStringAppend(&rtf, &msgSize, "\\f1\\cf1\\fs%d\\b%d\\i%d%s %s: ",
								iFontSize,
								bBold,
								bItalic,
								bUnderline?"\\ul":"",
								Translate("Error"));
	} else {
		if (getOptions()&FLAG_SHOW_DATE || getOptions()&FLAG_SHOW_TIMESTAMP) {
			bBold = fontTimestamp->getStyle() & Font::BOLD ? 1 : 0;
			bItalic = fontTimestamp->getStyle() & Font::ITALIC ? 1 : 0;
			bUnderline = fontTimestamp->getStyle() & Font::UNDERLINE ? 1 : 0;
			iFontSize = fontTimestamp->getSize();
			iFontSize = 2 * abs((signed char)iFontSize) * 74 / logPixelSY;
			dbtts.cbDest = 90;
			dbtts.szDest = timestampStr;
			timestampStr[0]='\0';
			//time_t time = time
			if (getOptions()&FLAG_SHOW_DATE && getOptions()&FLAG_SHOW_TIMESTAMP) {
				if (getOptions()&FLAG_LONG_DATE) {
					dbtts.szFormat = getOptions()&FLAG_SHOW_SECONDS ? (char *)"D s" : (char *)"D t";
				} else {
					dbtts.szFormat = getOptions()&FLAG_SHOW_SECONDS ? (char *)"d s" : (char *)"d t";
				}
			} else if (getOptions()&FLAG_SHOW_DATE) {
				dbtts.szFormat = getOptions()&FLAG_LONG_DATE ? (char *)"D" : (char *)"d";
			} else if (getOptions()&FLAG_SHOW_TIMESTAMP) {
				dbtts.szFormat = getOptions()&FLAG_SHOW_SECONDS ? (char *)"s" : (char *)"t";
			} else {
				dbtts.szFormat = (char *)"";
			}
			CallService(MS_DB_TIME_TIMESTAMPTOSTRING, (WPARAM)event->time, (LPARAM) & dbtts);
			JabberStringAppend(&rtf, &msgSize, "\\f0\\cf0\\fs%d\\b%d\\i%d%s %s ",
									iFontSize,
									bBold,
									bItalic,
									bUnderline?"\\ul":"",
									timestampStr);
		}
	}
	bBold = fontMessage->getStyle() & Font::BOLD ? 1 : 0;
	bItalic = fontMessage->getStyle() & Font::ITALIC ? 1 : 0;
	bUnderline = fontMessage->getStyle() & Font::UNDERLINE ? 1 : 0;
	iFontSize = fontMessage->getSize();
	iFontSize = 2 * abs((signed char)iFontSize) * 74 / logPixelSY;
	JabberStringAppend(&rtf, &msgSize, "\\f2\\cf2\\fs%d\\b%d\\i%d%s %s",
							iFontSize,
							bBold,
							bItalic,
							bUnderline?"\\ul":"",
							escapedStr);
	JabberStringAppend(&rtf, &msgSize, "\\par}");
	hwndLog = GetDlgItem(getHWND(), IDC_LOG);
	sel.cpMin = sel.cpMax = GetWindowTextLength(hwndLog);
	SendMessage(hwndLog, EM_EXSETSEL, 0, (LPARAM) &sel);
	stt.flags = ST_SELECTION;
	stt.codepage = CP_ACP;
	SendMessage(hwndLog, EM_SETTEXTEX, (WPARAM) &stt, (LPARAM) rtf);
	//Utils::log("%s", rtf);
	free(rtf);
	free(escapedStr);
	return 1;
}

int ChatWindow::appendMessage(const MUCCEVENT *event) {
	char timestampStr[100];
	char *rtf, *escapedStr, *escapedNick;
	Font *fontTimestamp, *fontName, *fontMessage; //*fontColon,
	int msgSize;
	DWORD color;
	int iFontSize, bItalic, bBold, bUnderline;
	HWND hwndLog;
	DBTIMETOSTRING dbtts;
	CHARRANGE sel;
	SETTEXTEX stt;
	tm *ltime;

	if (!(getOptions() & FLAG_LOG_MESSAGES)) return 0;

	escapedStr=JabberRtfEscape((char *)event->pszText);
	escapedNick=JabberRtfEscape((char *)event->pszNick);
	ltime = localtime(&event->time);
	rtf = NULL;
	JabberStringAppend(&rtf, &msgSize, "{\\rtf1\\ansi\\deff0{\\fonttbl");
	fontTimestamp = FontList::getFont(FontList::FONT_TIMESTAMP);
//	fontColon = FontList::getFont(FontList::FONT_COLON);
	if (event->bIsMe) {
		fontName = FontList::getFont(FontList::FONT_MYNAME);
		fontMessage = FontList::getFont(FontList::FONT_OUTMESSAGE);
	} else {
		fontName = FontList::getFont(FontList::FONT_OTHERSNAMES);
		fontMessage = FontList::getFont(FontList::FONT_INMESSAGE);
	}
	JabberStringAppend(&rtf, &msgSize, "{\\f0\\fnil\\fcharset%u %s;}", fontTimestamp->getCharSet(), fontTimestamp->getFace());
	JabberStringAppend(&rtf, &msgSize, "{\\f1\\fnil\\fcharset%u %s;}", fontName->getCharSet(), fontName->getFace());
	if (getOptions()&FLAG_FORMAT_FONT) {
		JabberStringAppend(&rtf, &msgSize, "{\\f2\\fnil\\fcharset%u %s;}", fontMessage->getCharSet(), getFontName(event->iFont));
	} else {
		JabberStringAppend(&rtf, &msgSize, "{\\f2\\fnil\\fcharset%u %s;}", fontMessage->getCharSet(), fontMessage->getFace());
	}
	JabberStringAppend(&rtf, &msgSize, "}{\\colortbl ");
	color = fontTimestamp->getColor();
	JabberStringAppend(&rtf, &msgSize, "\\red%d\\green%d\\blue%d;", color&0xFF, (color>>8)&0xFF, (color>>16)&0xFF);
	color = fontName->getColor();
	JabberStringAppend(&rtf, &msgSize, "\\red%d\\green%d\\blue%d;", color&0xFF, (color>>8)&0xFF, (color>>16)&0xFF);
	if (getOptions()&FLAG_FORMAT_COLOR && event->color!=0xFFFFFFFF) {
		color = event->color;
	} else {
		color = fontMessage->getColor();
	}
	JabberStringAppend(&rtf, &msgSize, "\\red%d\\green%d\\blue%d;", color&0xFF, (color>>8)&0xFF, (color>>16)&0xFF);
	JabberStringAppend(&rtf, &msgSize, "}");
	if (getOptions()&FLAG_SHOW_DATE || getOptions()&FLAG_SHOW_TIMESTAMP) {
		bBold = fontTimestamp->getStyle() & Font::BOLD ? 1 : 0;
		bItalic = fontTimestamp->getStyle() & Font::ITALIC ? 1 : 0;
		bUnderline = fontTimestamp->getStyle() & Font::UNDERLINE ? 1 : 0;
		iFontSize = fontTimestamp->getSize();
		iFontSize = 2 * abs((signed char)iFontSize) * 74 / logPixelSY;
		dbtts.cbDest = 90;
		dbtts.szDest = timestampStr;
		timestampStr[0]='\0';
		//time_t time = time
		if (getOptions()&FLAG_SHOW_DATE && getOptions()&FLAG_SHOW_TIMESTAMP) {
			if (getOptions()&FLAG_LONG_DATE) {
				dbtts.szFormat = getOptions()&FLAG_SHOW_SECONDS ? (char *)"D s" : (char *)"D t";
			} else {
				dbtts.szFormat = getOptions()&FLAG_SHOW_SECONDS ? (char *)"d s" : (char *)"d t";
			}
		} else if (getOptions()&FLAG_SHOW_DATE) {
			dbtts.szFormat = getOptions()&FLAG_LONG_DATE ? (char *)"D" : (char *)"d";
		} else if (getOptions()&FLAG_SHOW_TIMESTAMP) {
			dbtts.szFormat = getOptions()&FLAG_SHOW_SECONDS ? (char *)"s" : (char *)"t";
		} else {
			dbtts.szFormat = (char *)"";
		}
		CallService(MS_DB_TIME_TIMESTAMPTOSTRING, (WPARAM)event->time, (LPARAM) & dbtts);
		JabberStringAppend(&rtf, &msgSize, "\\f0\\cf0\\fs%d\\b%d\\i%d%s %s ",
								iFontSize,
								bBold,
								bItalic,
								bUnderline?"\\ul":"",
								timestampStr);
	}
	bBold = fontName->getStyle() & Font::BOLD ? 1 : 0;
	bItalic = fontName->getStyle() & Font::ITALIC ? 1 : 0;
	bUnderline = fontName->getStyle() & Font::UNDERLINE ? 1 : 0;
	iFontSize = fontName->getSize();
	iFontSize = 2 * abs((signed char)iFontSize) * 74 / logPixelSY;
	if (getOptions()&FLAG_SHOW_NICKNAMES) {
		JabberStringAppend(&rtf, &msgSize, "\\f1\\cf1\\fs%d\\b%d\\i%d%s %s: ",
							iFontSize,
							bBold,
							bItalic,
							bUnderline?"\\ul":"",
							escapedNick);
	}
	bBold = fontMessage->getStyle() & Font::BOLD ? 1 : 0;
	bItalic = fontMessage->getStyle() & Font::ITALIC ? 1 : 0;
	bUnderline = fontMessage->getStyle() & Font::UNDERLINE ? 1 : 0;
	iFontSize = fontMessage->getSize();
	iFontSize = 2 * abs((signed char)iFontSize) * 74 / logPixelSY;
	if (getOptions()&FLAG_FORMAT_STYLE) {
		bBold = bItalic = bUnderline = 0;
		if (event->dwFlags & MUCC_EF_FONT_BOLD) bBold = 1;
		if (event->dwFlags & MUCC_EF_FONT_ITALIC) bItalic = 1;
		if (event->dwFlags & MUCC_EF_FONT_UNDERLINE) bUnderline = 1;
	}
	if (getOptions()&FLAG_FORMAT_SIZE) {
		if (event->iFontSize != 0) iFontSize = 2 * event->iFontSize;
	}
	if (getOptions()&FLAG_MSGINNEWLINE) {
		JabberStringAppend(&rtf, &msgSize, "\\line");
	}
	JabberStringAppend(&rtf, &msgSize, "\\f2\\cf2\\fs%d\\b%d\\i%d%s %s",
							iFontSize,
							bBold,
							bItalic,
							bUnderline?"\\ul":"",
							escapedStr);

	JabberStringAppend(&rtf, &msgSize, "\\par}");
	//MUCCLog("%s", rtf);
	hwndLog = GetDlgItem(getHWND(), IDC_LOG);
	sel.cpMin = sel.cpMax = GetWindowTextLength(hwndLog);
	SendMessage(hwndLog, EM_EXSETSEL, 0, (LPARAM) &sel);
	stt.flags = ST_SELECTION;
	stt.codepage = CP_ACP;
	SendMessage(hwndLog, EM_SETTEXTEX, (WPARAM) &stt, (LPARAM) rtf);

	free(rtf);
	free(escapedStr);
	free(escapedNick);
	return 1;
}

void ChatWindow::setFont(int font, int size, int bBold, int bItalic, int bUnderline, COLORREF color) {
	if (hEditFont!=NULL) {
		DeleteObject(hEditFont);
	}
	hEditFont = CreateFont (MulDiv(-size, logPixelSY, 74), 0, 0, 0, bBold?FW_BOLD:FW_NORMAL,
                                    bItalic, bUnderline, 0, 238, OUT_DEFAULT_PRECIS,
                                    CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                    FIXED_PITCH | FF_ROMAN,
                                    getFontName(font));
	SendDlgItemMessage(hWnd, IDC_EDIT, WM_SETFONT, (WPARAM) hEditFont, TRUE);
	this->font = font;
	this->fontSize = size;
	this->bBold = bBold;
	this->bItalic = bItalic;
	this->bUnderline = bUnderline;
	this->fontColor = color;
}

int ChatWindow::getFontStyle() {
	int style = 0;
	if (bBold) style |= MUCC_EF_FONT_BOLD;
	if (bItalic) style |= MUCC_EF_FONT_ITALIC;
	if (bUnderline) style |= MUCC_EF_FONT_UNDERLINE;
	return style;
}

int ChatWindow::getFont() {
	return font;
}

int ChatWindow::getFontSize() {
	return fontSize;
}

COLORREF ChatWindow::getFontColor() {
	return fontColor;
}

void ChatWindow ::queryResultContacts(MUCCQUERYRESULT *queryResult) {
	HelperDialog::inviteDlg(this, queryResult);
}

void ChatWindow ::queryResultUsers(MUCCQUERYRESULT *queryResult) {
	if (adminWindow!=NULL) {
		Utils::log("ChatWindow::queryResultUsers");
		adminWindow->queryResultUsers(queryResult);
	}
}

ChatWindow * ChatWindow::getWindow(const char *module, const char *roomId) {
	ChatWindow *ptr;
	EnterCriticalSection(&mutex);
	for (ptr=list;ptr!=NULL;ptr=ptr->getNext()) {
		if (strcmp(ptr->getRoomId(), roomId)==0 && strcmp(ptr->getModule(), module)==0) {
			break;
		}
	}
	LeaveCriticalSection(&mutex);
	return ptr;
}

void ChatWindow::release() {
	for (;list!=NULL;) {
		delete list;
	}
	DeleteCriticalSection(&mutex);
}

void ChatWindow::init() {
	InitializeCriticalSection(&mutex);
}

