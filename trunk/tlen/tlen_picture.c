/*

Tlen Protocol Plugin for Miranda IM
Copyright (C) 2002-2004  Santithorn Bunchua
Copyright (C) 2004-2009  Piotr Piastucki

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

#include "jabber.h"
#include "jabber_list.h"
#include "tlen_file.h"
#include "tlen_p2p_old.h"

typedef struct {
    TlenProtocol *proto;
    JABBER_LIST_ITEM *item;
} TLENPSREQUESTTHREADDATA;

static void TlenPsAuthThread(void *ptr) {
    TLENPSREQUESTTHREADDATA *data = (TLENPSREQUESTTHREADDATA *)ptr;
    TlenProtocol *proto = data->proto;
    JABBER_LIST_ITEM *item = data->item;
    JABBER_SOCKET socket = JabberWsConnect(proto, "ps.tlen.pl", 443);
    if (socket != NULL) {
        char header[512];
        _snprintf(header, sizeof(header), "<pic auth='%s' t='p' to='%s' size='%d' idt='%s'/>", proto->threadData->username, item->ft->jid, item->ft->fileTotalSize, item->jid);
        JabberWsSend(proto, socket, header, strlen(header));
        Netlib_CloseHandle(socket);
    }
    mir_free(data);
}

static void TlenPsAuth(TlenProtocol *proto, JABBER_LIST_ITEM *item) {
    TLENPSREQUESTTHREADDATA *threadData = (TLENPSREQUESTTHREADDATA *)mir_alloc(sizeof(TLENPSREQUESTTHREADDATA));
    threadData->proto = proto;
    threadData->item = item;
    JabberForkThread(TlenPsAuthThread, 0, threadData);
}

static void TlenPsPostThread(void *ptr) {
    TLENPSREQUESTTHREADDATA *data = (TLENPSREQUESTTHREADDATA *)ptr;
    TlenProtocol *proto = data->proto;
    JABBER_LIST_ITEM *item = data->item;
    FILE *fp = fopen( item->ft->files[0], "rb" );
    if (fp) {
        JABBER_SOCKET socket = JabberWsConnect(proto, "ps.tlen.pl", 443);
        if (socket != NULL) {
            int i;
            char header[512];
			char fileBuffer[2048];
            _snprintf(header, sizeof(header), "<pic st='%s' idt='%s'/>", item->ft->iqId, item->jid);
            JabberWsSend(proto, socket, header, strlen(header));
			JabberLog(proto, "Sending picture data...");
            for (i = item->ft->filesSize[0]; i > 0; ) {
                int toread = min(2048, i);
                int readcount = fread(fileBuffer, 1, toread, fp);
                i -= readcount;
                if (readcount > 0) {
                    JabberWsSend(proto, socket, fileBuffer, readcount);
                }
                if (toread != readcount) {
                    break;
                }
            }
            SleepEx(500, TRUE);
            Netlib_CloseHandle(socket);
            SleepEx(500, TRUE);
       		JabberSend(proto, "<message to='%s' type='pic' pid='1001' rt='%s' idt='%s'></message>", item->ft->jid, item->ft->id2, item->jid);
        } else {
          /* cannot send picture */
        }
        TlenP2PFreeFileTransfer(item->ft);
        JabberListRemove(proto, LIST_PICTURE, item->jid);
        fclose(fp);
    }
    mir_free(data);
}

static void TlenPsPost(TlenProtocol *proto, JABBER_LIST_ITEM *item) {
    TLENPSREQUESTTHREADDATA *threadData = (TLENPSREQUESTTHREADDATA *)mir_alloc(sizeof(TLENPSREQUESTTHREADDATA));
    threadData->proto = proto;
    threadData->item = item;
    JabberForkThread(TlenPsPostThread, 0, threadData);
}

void TlenProcessPic(XmlNode *node, TlenProtocol *proto) {
    JABBER_LIST_ITEM *item = NULL;
    char *crc, *crc_c, *idt, *size, *from;
    from=JabberXmlGetAttrValue(node, "from");
    idt = JabberXmlGetAttrValue(node, "idt");
    size = JabberXmlGetAttrValue(node, "size");
    crc_c = JabberXmlGetAttrValue(node, "crc_c");
    crc = JabberXmlGetAttrValue(node, "crc");
    if (idt != NULL) {
        item = JabberListGetItemPtr(proto, LIST_PICTURE, idt);
    }
    if (item != NULL) {
        if (!strcmp(from, "ps")) {
            char *rt = JabberXmlGetAttrValue(node, "rt");
            char *st = JabberXmlGetAttrValue(node, "st");
            item->ft->iqId = mir_strdup(st);
            item->ft->id2 = mir_strdup(rt);
            TlenPsPost(proto, item);
        } else if (!strcmp(item->ft->jid, from)) {
            if (crc_c != NULL) {
                if (!strcmp(crc_c, "n")) {
                    /* crc_c = n, picture transfer accepted */
                    TlenPsAuth(proto, item);
                } else if (!strcmp(crc_c, "f")) {
                    /* crc_c = f, picture cached, no need to transfer again */
                    JabberListRemove(proto, LIST_PICTURE, idt);
                }
            }
        }
    } else if (crc != NULL) {
        JabberSend(proto, "<message type='pic' to='%s' crc_c='n' idt='%s'/>", from, idt);
    }
}

BOOL SendPicture(TlenProtocol *proto, HANDLE hContact) {
	DBVARIANT dbv;
	if (!DBGetContactSetting(hContact, proto->iface.m_szModuleName, "jid", &dbv)) {
        char *jid = dbv.pszVal;
        char szFilter[512];
        char *szFileName = (char*) mir_alloc(_MAX_PATH);
        CallService( MS_UTILS_GETBITMAPFILTERSTRINGS, ( WPARAM ) sizeof( szFilter ), ( LPARAM )szFilter );
        OPENFILENAMEA ofn = {0};
        ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = szFilter;
        ofn.lpstrCustomFilter = NULL;
        ofn.lpstrFile = szFileName;
        ofn.nMaxFile = _MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST;
        szFileName[0] = '\0';
        if ( GetOpenFileNameA( &ofn )) {
            long size;
            FILE* fp = fopen( szFileName, "rb" );
            if (fp) {
                fseek(fp, 0, SEEK_END);
                size = ftell(fp);
                if (size > 0 && size < 256*1024) {
                    JABBER_LIST_ITEM *item;
                    mir_sha1_ctx sha;
                    DWORD digest[5];
                    int i;
                    char idStr[10];
                    char fileBuffer[2048];
                    int id = JabberSerialNext(proto);
                    _snprintf(idStr, sizeof(idStr), "%d", id);
                    item = JabberListAdd(proto, LIST_PICTURE, idStr);
                    item->ft = TlenFileCreateFT(proto, jid);
                    item->ft->files = (char **) mir_alloc(sizeof(char *));
                    item->ft->filesSize = (long *) mir_alloc(sizeof(long));
                    item->ft->files[0] = szFileName;
                    item->ft->filesSize[0] = size;
                    item->ft->fileTotalSize = size;
                    fseek(fp, 0, SEEK_SET);
                    mir_sha1_init( &sha );
                    for (i = item->ft->filesSize[0]; i > 0; ) {
                        int toread = min(2048, i);
                        int readcount = fread(fileBuffer, 1, toread, fp);
                        i -= readcount;
                        if (readcount > 0) {
                            mir_sha1_append( &sha, (mir_sha1_byte_t* )fileBuffer, readcount);
                        }
                        if (toread != readcount) {
                            break;
                        }
                    }
                    mir_sha1_finish( &sha, digest );
            		JabberSend(proto, "<message type='pic' to='%s' crc='%08x%08x%08x%08x%08x' idt='%s' size='%d' ext='%s'/>", jid,
                        (int)htonl(digest[0]), (int)htonl(digest[1]), (int)htonl(digest[2]), (int)htonl(digest[3]), (int)htonl(digest[4]), idStr, item->ft->filesSize[0], "jpg");
                } else {
                    /* file too big */
                }
                fclose(fp);
            }
        }
        DBFreeVariant(&dbv);
    }
    return FALSE;
}
