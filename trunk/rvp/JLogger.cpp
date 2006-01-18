// JLogger.cpp: implementation of the JLogger class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdio.h>
#include <windows.h>
#include "JLogger.h"

JLogger::JLogger() {
	strcpy(filename,"./logs/default.log");
}

JLogger::JLogger(const char *s) {	
	strcpy(filename,s);
}

JLogger::~JLogger() {
}

void JLogger::info(const char *fmt, ...) {
#ifndef JLOGGER_DISABLE_LOGS
	SYSTEMTIME time;
	char *str;
	va_list vararg;
	int strsize;
	flog=fopen(filename,"at");
	if (flog!=NULL) {
		GetLocalTime(&time);
    	va_start(vararg, fmt);
    	str = (char *) malloc(strsize=2048);
    	while (_vsnprintf(str, strsize, fmt, vararg) == -1)
    		str = (char *) realloc(str, strsize+=2048);
    	va_end(vararg);
    	fprintf(flog,"%04d-%02d-%02d %02d:%02d:%02d,%03d [%s]",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds, "INFO");
		fprintf(flog,"  %s\n",str);
    	free(str);
		fclose(flog);
	}
#endif
}

void JLogger::debug(const char *fmt, ...) {
#ifndef JLOGGER_DISABLE_LOGS
	SYSTEMTIME time;
	char *str;
	va_list vararg;
	int strsize;
	flog=fopen(filename,"at");
	if (flog!=NULL) {
		GetLocalTime(&time);
    	va_start(vararg, fmt);
    	str = (char *) malloc(strsize=2048);
    	while (_vsnprintf(str, strsize, fmt, vararg) == -1)
    		str = (char *) realloc(str, strsize+=2048);
    	va_end(vararg);
    	fprintf(flog,"%04d-%02d-%02d %02d:%02d:%02d,%03d [%s]",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds, "DEBUG");
		fprintf(flog,"  %s\n",str);
    	free(str);
		fclose(flog);
	}
#endif
}

void JLogger::warn(const char *fmt, ...) {
#ifndef JLOGGER_DISABLE_LOGS
	SYSTEMTIME time;
	char *str;
	va_list vararg;
	int strsize;
	flog=fopen(filename,"at");
	if (flog!=NULL) {
		GetLocalTime(&time);
    	va_start(vararg, fmt);
    	str = (char *) malloc(strsize=2048);
    	while (_vsnprintf(str, strsize, fmt, vararg) == -1)
    		str = (char *) realloc(str, strsize+=2048);
    	va_end(vararg);
    	fprintf(flog,"%04d-%02d-%02d %02d:%02d:%02d,%03d [%s]",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds, "WARN");
		fprintf(flog,"  %s\n",str);
    	free(str);
		fclose(flog);
	}
#endif
}

void JLogger::error(const char *fmt, ...) {
#ifndef JLOGGER_DISABLE_LOGS
	SYSTEMTIME time;
	char *str;
	va_list vararg;
	int strsize;
	flog=fopen(filename,"at");
	if (flog!=NULL) {
		GetLocalTime(&time);
    	va_start(vararg, fmt);
    	str = (char *) malloc(strsize=2048);
    	while (_vsnprintf(str, strsize, fmt, vararg) == -1)
    		str = (char *) realloc(str, strsize+=2048);
    	va_end(vararg);
    	fprintf(flog,"%04d-%02d-%02d %02d:%02d:%02d,%03d [%s]",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds, "ERROR");
		fprintf(flog,"  %s\n",str);
    	free(str);
		fclose(flog);
	}
#endif
}

void JLogger::log(const char *s) {	
#ifndef JLOGGER_DISABLE_LOGS
	SYSTEMTIME time;
	GetLocalTime(&time);
	flog=fopen(filename,"at");
	if (flog!=NULL) {
		fprintf(flog, "%04d-%02d-%02d %02d:%02d:%02d,%03d %s\n",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,s);
		fclose(flog);
	}
#endif
}


