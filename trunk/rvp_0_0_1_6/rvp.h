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

#ifndef RVP_H_INCLUDED
#define RVP_H_INCLUDED

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

/*******************************************************************
 * Global header files
 *******************************************************************/
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x500
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>

#include <newpluginapi.h>
#include <m_system.h>
#include <m_netlib.h>
#include <m_protomod.h>
#include <m_protosvc.h>
#include <m_clist.h>
#include <m_clui.h>
#include <m_options.h>
#include <m_userinfo.h>
#include <m_database.h>
#include <m_langpack.h>
#include <m_utils.h>
#include <m_message.h>
#include <m_skin.h>

#include "RVPImpl.h"

/*******************************************************************
 * Global constants
 *******************************************************************/
#define RVP_DEFAULT_PORT 80
// Error code
#define RVP_ERROR_REDIRECT				302
#define RVP_ERROR_BAD_REQUEST			400
#define RVP_ERROR_UNAUTHORIZED			401
#define RVP_ERROR_PAYMENT_REQUIRED		402
#define RVP_ERROR_FORBIDDEN				403
#define RVP_ERROR_NOT_FOUND				404
#define RVP_ERROR_NOT_ALLOWED			405
#define RVP_ERROR_NOT_ACCEPTABLE		406
#define RVP_ERROR_REGISTRATION_REQUIRED	407
#define RVP_ERROR_REQUEST_TIMEOUT		408
#define RVP_ERROR_CONFLICT				409
#define RVP_ERROR_INTERNAL_SERVER_ERROR	500
#define RVP_ERROR_NOT_IMPLEMENTED		501
#define RVP_ERROR_REMOTE_SERVER_ERROR	502
#define RVP_ERROR_SERVICE_UNAVAILABLE	503
#define RVP_ERROR_REMOTE_SERVER_TIMEOUT	504
#define IDC_STATIC (-1)

enum {
	RVP_IDI_RVP = 0,
	RVP_ICON_TOTAL
};

extern HICON tlenIcons[RVP_ICON_TOTAL];

typedef struct {
	PROTOSEARCHRESULT hdr;
	char jid[256];
} RVP_SEARCH_RESULT;


/*******************************************************************
 * Global variables
 *******************************************************************/
extern HINSTANCE hInst;
extern RVPImpl rvpimpl;

extern HANDLE hMainThread;
extern char *rvpProtoName;
extern char *rvpModuleName;

extern BOOL jabberConnected;
extern int jabberStatus;
extern int jabberDesiredStatus;

extern HANDLE hEventSettingChanged, hEventContactDeleted;

#endif

