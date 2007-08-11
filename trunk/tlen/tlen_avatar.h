#ifndef _TLEN_AVATAR_H_
#define _TLEN_AVATAR_H_

void TlenProcessPresenceAvatar(XmlNode *node, JABBER_LIST_ITEM *item);
int TlenProcessAvatarNode(XmlNode *avatarNode, JABBER_LIST_ITEM *item);
void TlenGetAvatarFileName(JABBER_LIST_ITEM *item, char* pszDest, int cbLen, BOOL isTemp);
void TlenGetAvatar(HANDLE hContact);
void TlenUploadAvatar();
void TlenRemoveAvatar();

#endif // TLEN_AVATAR_H_INCLUDED
