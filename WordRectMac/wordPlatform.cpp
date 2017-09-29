//  wordPlatform.cpp : platform-specific implementations of common library
//  methods such as (safe versions of) ctime, sprintf, and strdup.
//
//  Sprax Lines, September 2012

#include "wordPlatform.h"
#include <unistd.h>

#ifdef _MBCS	// Microsoft Compiler

#else

//#include <assert.h>
#include <ctime>
#include <stdio.h>
#include <string.h>

int GetLastError() { return -1; }

void GetSystemInfo(SYSTEM_INFO *pSysInfo)
{
    pSysInfo->dwNumberOfProcessors = 2;
}

#define CTIME_BUFSIZE 64
errno_t ctime_safe(char *ctimeBuf, size_t bufSize, time_t *tim) 
{
    assert(bufSize >= CTIME_BUFSIZE);
    char *pc = ctime(tim);				// deprecated as unsafe (MS)
    strncpy(ctimeBuf, pc, bufSize);
    return 0;
}

int fopen_safe(FILE **fh, const char *fname, const char *perms) {
    *fh = fopen(fname, perms);
    if (*fh != NULL)
        return 0;
    return GetLastError();
}

#include <stdarg.h>
int sprintf_safe(char *buffer, size_t bufsize, const char *format, ...)
{
    int num = 0;
    va_list args;
    va_start(args,format);
    num = vsprintf (buffer, format, args );
    va_end(args);
    return num;
}

#endif
