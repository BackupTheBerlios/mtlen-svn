#ifndef _TLEN_AVATAR_H_
#define _TLEN_AVATAR_H_

void TlenProcessPresenceAvatar(XmlNode *node, JABBER_LIST_ITEM *item);
void TlenGetAvatarFileName(JABBER_LIST_ITEM *item, char* pszDest, int cbLen);
void TlenGetAvatar(HANDLE hContact);
int TlenUploadAvatar();
int TlenRemoveAvatar();

#endif // TLEN_AVATAR_H_INCLUDED
