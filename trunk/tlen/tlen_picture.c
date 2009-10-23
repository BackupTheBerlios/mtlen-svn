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

static void TlenPsPostThread(void *ptr) {
    TLENPSREQUESTTHREADDATA *data = (TLENPSREQUESTTHREADDATA *)ptr;
    TlenProtocol *proto = data->proto;
    JABBER_LIST_ITEM *item = data->item;
    JABBER_SOCKET socket = JabberWsConnect(proto, "ps.tlen.pl", 443);
    if (socket != NULL) {
        char header[512];
        DWORD ret;
        item->ft->s = socket;
        item->ft->hFileEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        _snprintf(header, sizeof(header), "<pic auth='%s' t='p' to='%s' size='%d' idt='%s'/>", proto->threadData->username, item->ft->jid, item->ft->fileTotalSize, item->jid);
        JabberWsSend(proto, socket, header, strlen(header));
        ret = WaitForSingleObject(item->ft->hFileEvent, 1000 * 60 * 5);
        if (ret == WAIT_OBJECT_0) {
            FILE *fp = fopen( item->ft->files[0], "rb" );
            if (fp) {
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
                fclose(fp);
                JabberSend(proto, "<message to='%s' idt='%s' rt='%s' pid='1001' type='pic' />", item->ft->jid, item->jid, item->ft->id2);
            } else {
              /* picture not found */
            }
        } else {
            /* 5 minutes passed */
        }
        Netlib_CloseHandle(socket);
        TlenP2PFreeFileTransfer(item->ft);
        JabberListRemove(proto, LIST_PICTURE, item->jid);
    } else {
        /* cannot connect to ps server */
    }
    mir_free(data);
}

static void TlenPsPost(TlenProtocol *proto, JABBER_LIST_ITEM *item) {
    TLENPSREQUESTTHREADDATA *threadData = (TLENPSREQUESTTHREADDATA *)mir_alloc(sizeof(TLENPSREQUESTTHREADDATA));
    threadData->proto = proto;
    threadData->item = item;
    JabberForkThread(TlenPsPostThread, 0, threadData);
}

static void TlenPsGetThread(void *ptr) {
    char path[_MAX_PATH];
    TLENPSREQUESTTHREADDATA *data = (TLENPSREQUESTTHREADDATA *)ptr;
    TlenProtocol *proto = data->proto;
    JABBER_LIST_ITEM *item = data->item;
	FILE *fp;
    _snprintf(path, sizeof(path), "%s.%s", item->ft->files[0], "jpg");
    fp = fopen( path, "wb" );
    if (fp) {
        JABBER_SOCKET socket = JabberWsConnect(proto, "ps.tlen.pl", 443);
        if (socket != NULL) {
            int i;
            XmlState xmlState;
            char header[512];
			char fileBuffer[2048];
			JabberXmlInitState(&xmlState);
            _snprintf(header, sizeof(header), "<pic auth='%s' t='g' to='%s' pid='1001' idt='%s' rt='%s'/>", proto->threadData->username, item->ft->jid, item->jid, item->ft->id2);
            JabberWsSend(proto, socket, header, strlen(header));

			JabberLog(proto, "Reveiving picture data...");
            {
                int totalcount = 0;
                int size = item->ft->filesSize[0];
                BOOL bHeader = TRUE;
                while (TRUE) {
                    int readcount = JabberWsRecv(proto, socket, fileBuffer, 2048 - totalcount);
                    if (readcount == 0) {
                        break;
                    }
                    totalcount += readcount;
                    if (bHeader) {
                        char * tagend = memchr(fileBuffer, '/', totalcount);
                        tagend = memchr(tagend + 1, '>', totalcount - (tagend - fileBuffer) - 1);
                        if (tagend != NULL) {
                            int parsed = JabberXmlParse(&xmlState, fileBuffer, tagend - fileBuffer + 1);
                            if (parsed == 0) {
                                continue;
                            }
                            bHeader = FALSE;
                            totalcount -= parsed;
                            memmove(fileBuffer, fileBuffer+parsed, totalcount);
                        }
                    }
                    if (!bHeader) {
                        if (totalcount > 0) {
                            fwrite(fileBuffer, 1, totalcount, fp);
                            size -= totalcount;
                            totalcount = 0;
                        }
                        if (size == 0) {
                            break;
                        }
                    }
                }
            }
            Netlib_CloseHandle(socket);
            JabberLog(proto, "Picture received...");
        } else {
          /* cannot receive picture */
        }
        fclose(fp);
    } else {
        /* cannot create file */
    }
    TlenP2PFreeFileTransfer(item->ft);
    JabberListRemove(proto, LIST_PICTURE, item->jid);
    mir_free(data);
}

static void TlenPsGet(TlenProtocol *proto, JABBER_LIST_ITEM *item) {
    TLENPSREQUESTTHREADDATA *threadData = (TLENPSREQUESTTHREADDATA *)mir_alloc(sizeof(TLENPSREQUESTTHREADDATA));
    threadData->proto = proto;
    threadData->item = item;
    JabberForkThread(TlenPsGetThread, 0, threadData);
}

void TlenProcessPic(XmlNode *node, TlenProtocol *proto) {
    JABBER_LIST_ITEM *item = NULL;
    char *crc, *crc_c, *idt, *size, *from, *rt;
    from=JabberXmlGetAttrValue(node, "from");
    idt = JabberXmlGetAttrValue(node, "idt");
    size = JabberXmlGetAttrValue(node, "size");
    crc_c = JabberXmlGetAttrValue(node, "crc_c");
    crc = JabberXmlGetAttrValue(node, "crc");
    rt = JabberXmlGetAttrValue(node, "rt");
    if (idt != NULL) {
        item = JabberListGetItemPtr(proto, LIST_PICTURE, idt);
    }
    if (item != NULL) {
        if (!strcmp(from, "ps")) {
            char *st = JabberXmlGetAttrValue(node, "st");
            if (st != NULL) {
                item->ft->iqId = mir_strdup(st);
                item->ft->id2 = mir_strdup(rt);
                if (item->ft->hFileEvent != NULL) {
                    SetEvent(item->ft->hFileEvent);
                    item->ft->hFileEvent = NULL;
                }
            }
        } else if (!strcmp(item->ft->jid, from)) {
            if (crc_c != NULL) {
                if (!strcmp(crc_c, "n")) {
                    /* crc_c = n, picture transfer accepted */
                    TlenPsPost(proto, item);
                } else if (!strcmp(crc_c, "f")) {
                    /* crc_c = f, picture cached, no need to transfer again */
                    JabberListRemove(proto, LIST_PICTURE, idt);
                }
            } else if (rt != NULL) {
                item->ft->id2 = mir_strdup(rt);
                TlenPsGet(proto, item);
            }
        }
    } else if (crc != NULL) {
        /* TODO: apply policy */
        /* TODO: search for existing file */
        char *ext = JabberXmlGetAttrValue(node, "ext");
        item = JabberListAdd(proto, LIST_PICTURE, idt);
        item->ft = TlenFileCreateFT(proto, from);
        item->ft->files = (char **) mir_alloc(sizeof(char *));
        item->ft->filesSize = (long *) mir_alloc(sizeof(long));
        item->ft->files[0] = mir_strdup(crc);
        item->ft->filesSize[0] = atol(size);
        item->ft->fileTotalSize = item->ft->filesSize[0];
        JabberSend(proto, "<message type='pic' to='%s' crc_c='n' idt='%s'/>", from, idt);
    }
}

BOOL SendPicture(TlenProtocol *proto, HANDLE hContact) {
	DBVARIANT dbv;
	if (!DBGetContactSetting(hContact, proto->iface.m_szModuleName, "jid", &dbv)) {
        char *jid = dbv.pszVal;
        char szFilter[512];
        char *szFileName = (char*) mir_alloc(_MAX_PATH);
        OPENFILENAMEA ofn = {0};
        CallService( MS_UTILS_GETBITMAPFILTERSTRINGS, ( WPARAM ) sizeof( szFilter ), ( LPARAM )szFilter );
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
                    mir_sha1_finish( &sha, (mir_sha1_byte_t* )digest );
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
