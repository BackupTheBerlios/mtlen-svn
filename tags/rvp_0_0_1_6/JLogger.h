// JLogger.h: interface for the JLogger class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_JLOGGER_H__2EAA6BB4_4D8D_43BD_84B1_908D758E5AB3__INCLUDED_)
#define AFX_JLOGGER_H__2EAA6BB4_4D8D_43BD_84B1_908D758E5AB3__INCLUDED_

/*! \class JLogger
 *  \brief Simple file logging class
 *  \author Piotr Piastucki
 *  \version 1.0
 *  \date    2002
 */

//#define JLOGGER_DISABLE_LOGS
#include <stdio.h>
class JLogger  
{
private:
	FILE*   flog;
	char    filename[400];
public:
	JLogger();
	JLogger(const char *);
	~JLogger();
	void log(const char *);
	void info(const char *fmt, ...);
	void warn(const char *fmt, ...);
	void debug(const char *fmt, ...);
	void error(const char *fmt, ...);
};

#endif // !defined(AFX_JLOGGER_H__2EAA6BB4_4D8D_43BD_84B1_908D758E5AB3__INCLUDED_)
