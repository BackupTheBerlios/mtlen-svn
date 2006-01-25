
class RVPFile;
class RVPFileListener;

#ifndef RVPFILE_H_
#define RVPFILE_H_

#include "List.h"
#include "ThreadManager.h"
#include "Connection.h"

class RVPFileListener {
public:
	enum PROGRESS {
		PROGRESS_CONNECTING,
		PROGRESS_CONNECTED,
		PROGRESS_INITIALIZING,
		PROGRESS_PROGRESS,
		PROGRESS_COMPLETED,
		PROGRESS_ERROR,
		PROGRESS_CANCELLED
	};
	virtual void onFileProgress(RVPFile *file, int type, int progress) = 0;
};

class RVPFile:public ThreadManager, public ListItem, public ConnectionListener {
private:
	enum THREADGROUPS {
		TGROUP_TRANSFER = 1
	};
	static List sendList;
	static List recvList;
	int	 mode;
	int size;
	HANDLE hContact;
	char *contact;
	char *login;
	char *file;
	char *path;
	char *host;
	char *cookie;
	char *authCookie;
	int port;
	Connection *connection;
	Connection *listenConnection;
	RVPFileListener *listener;
	void   setAuthCookie();
protected:
	friend void __cdecl RVPFileRecv(void *ptr);
	friend void __cdecl RVPFileSend(void *ptr);
	void	doRecv();
	void	doSend();
	bool	msnftp();
public:
	enum MODES {
		MODE_RECV,
		MODE_SEND
	};
	static RVPFile* find(int mode, const char *contact, const char *id);
	RVPFile(HANDLE hContact, int mode, const char *contact, const char *login, RVPFileListener *listener);
	RVPFile(HANDLE hContact, int mode, const char *contact, const char *cookie, const char *login, RVPFileListener *listener);
	~RVPFile();
	void	onNewConnection(Connection *connection, DWORD dwRemoteIP);
	int		getMode();
//	const char *getContact();
	HANDLE getHContact();
	const char *getLogin();
	const char *getCookie();
	void   setSize(int size);
	int	   getSize();
	void   setAuthCookie(const char *f);
	const char *getAuthCookie();
	void   setFile(const char *f);
	const char *getFile();
	void   setPath(const char *f);
	const char *getPath();
	void	setHost(const char *f);
	const char *getHost();
	void	setPort(int p);
	int		getPort();
	void 	recv();
	void 	send();
	void 	cancel();
};

#endif /*RVPFILETRANSFER_H_*/
