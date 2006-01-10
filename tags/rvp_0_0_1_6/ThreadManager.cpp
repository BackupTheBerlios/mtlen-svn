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
#include "ThreadManager.h"
#include "Utils.h"

ThreadCall::ThreadCall(ThreadManager *m) {
	manager = m;
}

ThreadManager::ThreadManager() {
	allowedGroups = 0xFFFFFFFF;
	for (int i = 0; i < 32; i++) {
		threadCount[i] = 0;
	}
}

int ThreadManager::groupToIndex(int group) {
	int i;
	for (i=31; i>=0; i--) {
		if (group & 0x80000000) break;
		group <<= 1;
	}
	return i;
}

void ThreadManager::setAllowedGroups(int g) {
	allowedGroups = g;
}

int ThreadManager::getAllowedGroups() {
	return allowedGroups;
}

bool ThreadManager::isGroupAllowed(int group) {
	return (getAllowedGroups() & group)!=0;
}

int ThreadManager::getThreadCount(int groups) {
	int count = 0;
	for (int i=0; i<32; i++) {
		if (groups & 1) {
			count += threadCount[i];
		}
		groups >>= 1;
	}
	return count;
}

void ThreadManager::waitForThreads(int groups) {
	while (getThreadCount(groups) > 0) {
		Sleep(100);
	}
}

static void __cdecl ThreadRunner(void *ptr) {
	ThreadCall *tcall = (ThreadCall *)ptr;
	tcall->func(tcall->arg);
	tcall->manager->endThread(tcall);
}

unsigned long ThreadManager::forkThread(int group, void (__cdecl *threadcode)(void*), unsigned long stacksize, void *arg) {
	if (isGroupAllowed(group)) {
		ThreadCall *tcall = new ThreadCall(this);
		tcall->func = threadcode;
		tcall->arg = arg;
		tcall->group = group;
		threadCount[groupToIndex(group)]++;
		Utils::forkThread(ThreadRunner, stacksize, tcall);
		return 1;
	}
	return 0;
}

void ThreadManager::endThread(ThreadCall *tcall) {
	threadCount[groupToIndex(tcall->group)]--;
	delete tcall;
}
