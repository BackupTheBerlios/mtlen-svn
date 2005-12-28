
class RVPFileTransferListener;
class RVPFileTransfer;

#ifndef RVPFILETRANSFER_H_
#define RVPFILETRANSFER_H_

#include "List.h"
#include "ThreadManager.h"
#include "Connection.h"

class RVPFileTransferListener {
public:
	virtual void onNewConnection(Connection *, DWORD dwRemoteIP) = 0;
};

#include "RVPClient.h"

class RVPFileTransfer:public ThreadManager, public ListItem {
private:
	enum THREADGROUPS {
		TGROUP_RECV = 1,
		TGROUP_SEND = 2
	};
	static List list;
	RVPFile *file;
	RVPFileTransferListener *listener;
	void	recvFile();
	void	sendFile();
	void	cancelFile();
protected:
	friend void __cdecl RVPFileTransferRecv(void *ptr);
	friend void __cdecl RVPFileTransferSend(void *ptr);
	void	doRecvFile();
	void	doSendFile();
public:
	RVPFileTransfer(RVPFile *file, RVPFileTransferListener *listener);
	~RVPFileTransfer();
	static void recvFile(RVPFile *file, RVPFileTransferListener *listener);
	static void sendFile(RVPFile *file, RVPFileTransferListener *listener);
	static void cancelFile(RVPFile *file);
};

#endif /*RVPFILETRANSFER_H_*/
