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
class ThreadCall;
class ThreadManager;

#ifndef THREADMANAGER_INCLUDED
#define THREADMANAGER_INCLUDED

class ThreadCall {
public:
	ThreadCall(ThreadManager *);
	ThreadManager *manager;
	void (__cdecl *func)(void*);
	void *arg;
	int	 group;
};

class ThreadManager {
private:
	int threadCount[32];
	int	allowedGroups;
	int groupToIndex(int);
public:
	enum THREADGROUPS {
		TGROUP1 = 1,
		TGROUP2 = 2,
		TGROUP3 = 4,
		TGROUP4 = 8,
		TGROUP5 = 16,
		TGROUP6 = 32,
		TGROUP7 = 64,
		TGROUP8 = 128,
	};
	ThreadManager();
	void setAllowedGroups(int);
	int	 getAllowedGroups();
	bool isGroupAllowed(int);
	int	 getThreadCount(int);
	void waitForThreads(int);
	unsigned long forkThread(int group, void (__cdecl *threadcode)(void*), unsigned long stacksize,   void *arg);
	void	endThread(ThreadCall *);
};

#endif /*_THREADMANAGER_INCLUDED */
