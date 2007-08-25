/*

Tlen Protocol Plugin for Miranda IM
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

#include <stdio.h>
#include "jabber.h"
#include "jabber_list.h"
#include "tlen_avatar.h"
#include "crypto/md5.h"
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

extern HWND hAvatarDlg;
extern char *jabberProtoName;
extern HANDLE hNetlibUser;
extern struct ThreadData *jabberThreadInfo;

/* TlenGetAvatarFileName() - gets a file name for the avatar image */

void TlenGetAvatarFileName(JABBER_LIST_ITEM *item, char* pszDest, int cbLen, BOOL isTemp)
{
	int tPathLen;
	int format = PA_FORMAT_PNG;
	char* szFileType;
	if (item != NULL) {
		format = item->avatarFormat;
	} else if (jabberThreadInfo != NULL) {
		format = jabberThreadInfo->avatarFormat;
	} else {
		format = DBGetContactSettingDword(NULL, jabberProtoName, "AvatarFormat", PA_FORMAT_UNKNOWN);
	}
	CallService( MS_DB_GETPROFILEPATH, cbLen, (LPARAM) pszDest );
	tPathLen = strlen( pszDest );
	tPathLen += mir_snprintf( pszDest + tPathLen, cbLen - tPathLen, "\\%s\\", jabberModuleName  );
	CreateDirectoryA( pszDest, NULL );
	szFileType = "png";
	switch(format) {
		case PA_FORMAT_JPEG: szFileType = "jpg";   break;
		case PA_FORMAT_ICON: szFileType = "ico";   break;
		case PA_FORMAT_PNG:  szFileType = "png";   break;
		case PA_FORMAT_GIF:  szFileType = "gif";   break;
		case PA_FORMAT_BMP:  szFileType = "bmp";   break;
	}
	if ( item != NULL ) {
		char* hash;
		hash = JabberSha1(item->jid);
		mir_snprintf( pszDest + tPathLen, MAX_PATH - tPathLen, "%s.%s", hash, szFileType );
		mir_free( hash );
	}
	else {
		if (isTemp) {
			mir_snprintf( pszDest + tPathLen, MAX_PATH - tPathLen, "%s_avatar_temp.%s", jabberProtoName, szFileType );
		} else {
			mir_snprintf( pszDest + tPathLen, MAX_PATH - tPathLen, "%s_avatar.%s", jabberProtoName, szFileType );
		}
	}
}

static void RemoveAvatarNotification(HANDLE hContact) {
	char tFileName[ MAX_PATH ];
	if (hContact != NULL) {
		DBDeleteContactSetting(hContact, "ContactPhoto", "File");
	} else {
		jabberThreadInfo->avatarHash[0] = '\0';
	}
	DBDeleteContactSetting(hContact, jabberProtoName, "AvatarHash");
	DBDeleteContactSetting(hContact, jabberProtoName, "AvatarFormat");
	TlenGetAvatarFileName( NULL, tFileName, sizeof tFileName, FALSE );
	DeleteFileA(tFileName);
	if (hContact == NULL && hAvatarDlg != NULL) {
		PostMessage(hAvatarDlg, WM_TLEN_REFRESH, 0, 0);
	}
}

int TlenProcessAvatarNode(XmlNode *avatarNode, JABBER_LIST_ITEM *item) {
	XmlNode *aNode;
	char *oldHash = NULL;
	char *md5 = NULL, *type = NULL;
	HANDLE hContact;
	hContact = NULL;
	if (item != NULL) {
		if ((hContact=JabberHContactFromJID(item->jid)) == NULL) return 0;
	} 
	if (item == NULL) {
		oldHash = jabberThreadInfo->avatarHash;
	} else {
		oldHash = item->avatarHash;
	}
	if (avatarNode != NULL) {
		aNode = JabberXmlGetChild(avatarNode, "a");
		if (aNode != NULL) {
			type = JabberXmlGetAttrValue(aNode, "type");
			md5 = JabberXmlGetAttrValue(aNode, "md5");
		}
	}
	if (md5 != NULL) {
		/* check contact's avatar hash - md5 */
		if (oldHash == NULL || strcmp(oldHash, md5)) {
			if (item != NULL) {
				item->newAvatarDownloading = TRUE;
			}
			TlenGetAvatar(hContact);
			return 1;
		}
	} else {
		/* remove avatar */
		if (oldHash != NULL) {
			if (item != NULL) {
				item->avatarHash = NULL;
				mir_free(oldHash);
				item->newAvatarDownloading = FALSE;
			}
			RemoveAvatarNotification(hContact);
			ProtoBroadcastAck(jabberProtoName, hContact, ACKTYPE_AVATAR, ACKRESULT_STATUS, NULL, 0);
			return 1;
		}
	}
	return 0;
}

void TlenProcessPresenceAvatar(XmlNode *node, JABBER_LIST_ITEM *item) {
	HANDLE hContact;
	if ((hContact=JabberHContactFromJID(item->jid)) == NULL) return;
	TlenProcessAvatarNode(JabberXmlGetChild(node, "avatar"), item);
}

static char *replaceTokens(const char *base, const char *uri, const char *login, const char* token, int type, int access) {
	char *result;
	int i, l, size;
	l = strlen(uri);
	size = strlen(base);
	for (i = 0; i < l; ) {
		if (!strncmp(uri + i, "^login^", 7)) {
			size += strlen(login);
			i += 7;
		} else if (!strncmp(uri + i, "^type^", 6)) {
			size++;
			i += 6;
		} else if (!strncmp(uri + i, "^token^", 7)) {
			size += strlen(token);
			i += 7;
		} else if (!strncmp(uri + i, "^access^", 8)) {
			size++;
			i += 8;
		} else {
			size++;
			i++;
		}
	}
	result = (char *)mir_alloc(size +1);
	strcpy(result, base);
	size = strlen(base);
	for (i = 0; i < l; ) {
		if (!strncmp(uri + i, "^login^", 7)) {
			strcpy(result + size, login);
			size += strlen(login);
			i += 7;
		} else if (!strncmp(uri + i, "^type^", 6)) {
			result[size++] = '0' + type;
			i += 6;
		} else if (!strncmp(uri + i, "^token^", 7)) {
			strcpy(result + size, token);
			size += strlen(token);
			i += 7;
		} else if (!strncmp(uri + i, "^access^", 8)) {
			result[size++] = '0' + access;
			i += 8;
		} else {
			result[size++] = uri[i++];
		}
	}
	result[size] = '\0';
	return result;
}

static int getAvatarMutex = 0;

static void TlenGetAvatarThread(HANDLE hContact) {
	JABBER_LIST_ITEM *item = NULL;
	NETLIBHTTPREQUEST req;
    NETLIBHTTPREQUEST *resp;
	char *request;
	char md5[1024];
	char *login = NULL;
	MD5 context;
	BOOL success = FALSE;
	if (hContact != NULL) {
		char *jid = JabberJIDFromHContact(hContact);
		login = JabberNickFromJID(jid);
		item = JabberListGetItemPtr(LIST_ROSTER, jid);
		mir_free(jid);
	} else {
		login = mir_strdup(jabberThreadInfo->username);
	}
	if ((jabberThreadInfo != NULL && hContact == NULL) || item != NULL) {
		PROTO_AVATAR_INFORMATION AI;
		AI.cbSize = sizeof AI;
		AI.hContact = hContact;
		AI.format = PA_FORMAT_UNKNOWN;
		if (item!= NULL) {
			item->newAvatarDownloading = TRUE;
		}
		request = replaceTokens(jabberThreadInfo->tlenConfig.mailBase, jabberThreadInfo->tlenConfig.avatarGet, login, jabberThreadInfo->avatarToken, 0, 0);
		ZeroMemory(&req, sizeof(req));
		req.cbSize = sizeof(req);
		req.requestType = jabberThreadInfo->tlenConfig.avatarGetMthd;
		req.flags = 0;
		req.headersCount = 0;
		req.headers = NULL;
		req.dataLength = 0;
		req.szUrl = request;
		resp = (NETLIBHTTPREQUEST *)CallService(MS_NETLIB_HTTPTRANSACTION, (WPARAM)hNetlibUser, (LPARAM)&req);
		if (resp != NULL) {
			if (resp->resultCode/100==2) {
				if (resp->dataLength > 0) {
					FILE* out;
					int i;
					for (i=0; i<resp->headersCount; i++ ) {
						if (strcmpi(resp->headers[i].szName, "Content-Type")==0) {
							if (strcmpi(resp->headers[i].szValue, "image/png")==0) {
								AI.format = PA_FORMAT_PNG;
							} else if (strcmpi(resp->headers[i].szValue, "image/x-png")==0) {
								AI.format = PA_FORMAT_PNG;
							} else if (strcmpi(resp->headers[i].szValue, "image/jpeg")==0) {
								AI.format = PA_FORMAT_JPEG;
							} else if (strcmpi(resp->headers[i].szValue, "image/jpg")==0) {
								AI.format = PA_FORMAT_JPEG;
							} else if (strcmpi(resp->headers[i].szValue, "image/gif")==0) {
								AI.format = PA_FORMAT_GIF;
							} else if (strcmpi(resp->headers[i].szValue, "image/bmp")==0) {
								AI.format = PA_FORMAT_BMP;
							}
							break;
						}
					}
					if (AI.format == PA_FORMAT_UNKNOWN && resp->dataLength > 4) {
						AI.format = JabberGetPictureType(resp->pData);
					}
					md5_init(&context);
					md5_update(&context, resp->pData, resp->dataLength);
					md5_finalize(&context);
					for (i=0;i<16;i++) {
						char lo, hi;
						unsigned int j=context.state[i>>2];
						j>>=8*(i%4);
						j&=0xFF;
						lo = j & 0x0F;
						hi = j >> 4;
						hi = hi + ((hi > 9) ? 'a' - 10 : '0');
						lo = lo + ((lo > 9) ? 'a' - 10 : '0');
						md5[i*2] = hi;
						md5[i*2+1] = lo;
					}
					md5[i*2] = 0;
					if (item != NULL) {
						char *hash = item->avatarHash;
						item->avatarFormat = AI.format;
						item->avatarHash = mir_strdup(md5);
						mir_free(hash);
					} else {
						jabberThreadInfo->avatarFormat = AI.format;
						strcpy(jabberThreadInfo->avatarHash, md5);
					}
					TlenGetAvatarFileName(item, AI.filename, sizeof AI.filename, FALSE );
					DeleteFileA(AI.filename);
					out = fopen( AI.filename, "wb" );
					if ( out != NULL ) {
						fwrite( resp->pData, resp->dataLength, 1, out );
						fclose( out );
						DBWriteContactSettingString( hContact, "ContactPhoto", "File", AI.filename );
						DBWriteContactSettingString(hContact, jabberProtoName, "AvatarHash",  md5);
						DBWriteContactSettingDword(hContact, jabberProtoName, "AvatarFormat",  AI.format);
						success = TRUE;
					}
					if (hContact == NULL && hAvatarDlg != NULL) {
						PostMessage(hAvatarDlg, WM_TLEN_REFRESH, 0, 0);
					}
				} else {
					RemoveAvatarNotification(hContact);
					success = TRUE;
				}
			}
			CallService(MS_NETLIB_FREEHTTPREQUESTSTRUCT, 0, (LPARAM)resp);
		}
		if (item!= NULL) {
			item->newAvatarDownloading = FALSE;
		}
		if (success) {
			ProtoBroadcastAck( jabberProtoName, hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, (HANDLE) &AI , 0);
		} else {
			ProtoBroadcastAck( jabberProtoName, hContact, ACKTYPE_AVATAR, ACKRESULT_FAILED, (HANDLE) &AI , 0);
		}
		mir_free(request);
		mir_free(login);
	}
	getAvatarMutex = 0;
}

void TlenGetAvatar(HANDLE hContact) {
	if (hContact == NULL && getAvatarMutex != 0) {
		return;
	}
	getAvatarMutex = 1;
	JabberForkThread(TlenGetAvatarThread, 0, hContact);
}

void TlenSendHttpRequestThread(void *ptr) {
	NETLIBHTTPREQUEST *resp;
	NETLIBHTTPREQUEST *req = (NETLIBHTTPREQUEST *)ptr;
	resp = (NETLIBHTTPREQUEST *)CallService(MS_NETLIB_HTTPTRANSACTION, (WPARAM)hNetlibUser, (LPARAM)req);
	mir_free(req->szUrl);
	mir_free(req->headers);
	mir_free(req->pData);
	mir_free(req);
	if (resp != NULL) {
		CallService(MS_NETLIB_FREEHTTPREQUESTSTRUCT, (WPARAM)0, (LPARAM)resp);
	}
	
}

void TlenSendHttpRequestThread2(void *ptr) {
	NETLIBHTTPREQUEST *resp;
	NETLIBHTTPREQUEST *req = (NETLIBHTTPREQUEST *)ptr;
	resp = (NETLIBHTTPREQUEST *)CallService(MS_NETLIB_HTTPTRANSACTION, (WPARAM)hNetlibUser, (LPARAM)req);
	mir_free(req->szUrl);
	mir_free(req->headers);
	mir_free(req->pData);
	mir_free(req);
	if (resp != NULL) {
	//	if (resp->resultCode/100==2) {
	//		TlenGetAvatar(NULL);
	//	}
		CallService(MS_NETLIB_FREEHTTPREQUESTSTRUCT, (WPARAM)0, (LPARAM)resp);
	}
}

void TlenRemoveAvatar() {
	NETLIBHTTPREQUEST *req;
	char *request;
	if (jabberThreadInfo != NULL) {
		req = (NETLIBHTTPREQUEST *)mir_alloc(sizeof(NETLIBHTTPREQUEST));
		request = replaceTokens(jabberThreadInfo->tlenConfig.mailBase, jabberThreadInfo->tlenConfig.avatarRemove, "", jabberThreadInfo->avatarToken, 0, 0);
		ZeroMemory(req, sizeof(NETLIBHTTPREQUEST));
		req->cbSize = sizeof(NETLIBHTTPREQUEST);
		req->requestType = jabberThreadInfo->tlenConfig.avatarGetMthd;
		req->szUrl = request;
		JabberForkThread(TlenSendHttpRequestThread, 0, req);
	}
}


void TlenUploadAvatar(unsigned char *data, int dataLen, int access) {
	NETLIBHTTPREQUEST *req;
	NETLIBHTTPHEADER *headers;
	char *request;
	unsigned char *buffer;
	if (jabberThreadInfo != NULL && dataLen > 0 && data != NULL) {
		int size, sizeHead, sizeTail;
		request = replaceTokens(jabberThreadInfo->tlenConfig.mailBase, jabberThreadInfo->tlenConfig.avatarUpload, "", jabberThreadInfo->avatarToken, 0, access);
		req = (NETLIBHTTPREQUEST *)mir_alloc(sizeof(NETLIBHTTPREQUEST));
		headers = (NETLIBHTTPHEADER *)mir_alloc(sizeof(NETLIBHTTPHEADER));
		ZeroMemory(req, sizeof(NETLIBHTTPREQUEST));
		req->cbSize = sizeof(NETLIBHTTPREQUEST);
		req->requestType = jabberThreadInfo->tlenConfig.avatarUploadMthd;
		req->szUrl = request;
		req->flags = 0;
		headers[0].szName = "Content-Type";
		headers[0].szValue = "multipart/form-data; boundary=AaB03x";
		req->headersCount = 1;
		req->headers = headers;
		sizeHead = strlen("--AaB03x\r\nContent-Disposition: form-data; name=\"filename\"; filename=\"plik.png\"\r\nContent-Type: image/png\r\n\r\n");
		sizeTail = strlen("\r\n--AaB03x--\r\n");
		size = dataLen + sizeHead + sizeTail;
		buffer = mir_alloc(size);
		strcpy(buffer, "--AaB03x\r\nContent-Disposition: form-data; name=\"filename\"; filename=\"plik.png\"\r\nContent-Type: image/png\r\n\r\n");
		memcpy(buffer + sizeHead, data, dataLen);
		strcpy(buffer + sizeHead + dataLen, "\r\n--AaB03x--\r\n");
		req->dataLength = size;
		req->pData = buffer;
		JabberForkThread(TlenSendHttpRequestThread2, 0, req);
	}
}

