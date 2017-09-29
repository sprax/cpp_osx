
// wordPlatform_h : Platform dependencies for my "wordy" projects
// 
// Sprax Lines,  September 2012

#ifndef wordPlatform_h
#define wordPlatform_h

#include "wordTypes.h"

#define CTIME_SAFE_BUFSIZE 64

#ifdef _MBCS	// Microsoft Compiler

#define ctime_safe(A,B,C)	ctime_s((A),(B),(C))
#define fopen_safe(A,B,C)	fopen_s((A),(B),(C))
#define sprintf_safe		sprintf_s
#define strdup_safe(A)		_strdup(A)

#else	// Non-Microsoft build platform

#define _CRT_SECURE_NO_WARNINGS

//#include <assert.h>
#include <ctime>
#include <stdio.h>
#include <string.h>

typedef int errno_t;
typedef int *HANDLE;
typedef int  DWORD;
typedef bool BOOL;
typedef long LONG;
typedef unsigned long (*LPTHREAD_START_ROUTINE)(void *pvArgs);
static  const int INVALID_HANDLE_VALUE = 0;

typedef int CRITICAL_SECTION;
#define InitializeCriticalSection(A) ;
#define EnterCriticalSection(pv) ;
#define LeaveCriticalSection(pv) ;
#define DeleteCriticalSection(A) ;


#define ReleaseSemaphore(A,B,C) (1)
#define WaitForSingleObject(A, B) (1)
#define WaitForMultipleObjects(A,B,C,D) (1);
#define SetEvent(A) (1)
#define DuplicateHandle(A,B,C,D,E,F,G) ((A==0 && B==0) ? 1 : 0)
#define WAIT_OBJECT_0  (1)
#define WAIT_FAILED   (-1)
#define GetCurrentThread()  (NULL)
#define GetCurrentProcess() (NULL)
#define CloseHandle(A) ;
#define ResetEvent(A) (NULL);

errno_t ctime_safe(char *ctimeBuf, size_t bufSize, time_t *tim);
errno_t fopen_safe(FILE **fh, const char *fname, const char *perms);
int     sprintf_safe(char *buffer, size_t bufsize, const char *format, ...);
#define strdup_safe(A)	strdup(A)

int GetLastError();

typedef struct SYSTEM_INFO_tag {      // private struct type for tracking finder threads
	DWORD dwNumberOfProcessors;
}   SYSTEM_INFO;

void    GetSystemInfo(SYSTEM_INFO *pSysInfo);

#endif	// _MBCS




#endif  // wordPlatform_h
