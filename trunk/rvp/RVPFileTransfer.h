
class RVPFileTransferListener;
class RVPFileTransfer;

#ifndef RVPFILETRANSFER_H_
#define RVPFILETRANSFER_H_

#include "List.h"
#include "ThreadManager.h"
#include "Connection.h"

#include "RVPClient.h"

class RVPFileTransfer:public ThreadManager, public ListItem {
private:
	enum THREADGROUPS {
		TGROUP_RECV = 1,
		TGROUP_SEND = 2
	};
	static List list;
	Connection *connection;
	RVPFile *file;
	RVPFileListener *listener;
	void	recvFile();
	void	sendFile();
	void	cancelFile();
	RVPFileTransfer(RVPFile *file, RVPFileListener *listener);
protected:
	friend void __cdecl RVPFileTransferRecv(void *ptr);
	friend void __cdecl RVPFileTransferSend(void *ptr);
	void	doRecvFile();
	void	doSendFile();
	bool	msnftp();
public:
	~RVPFileTransfer();
	static void recvFile(RVPFile *file, RVPFileListener *listener);
	static void sendFile(RVPFile *file, RVPFileListener *listener);
	static void cancelFile(RVPFile *file);
};

#endif /*RVPFILETRANSFER_H_*/
