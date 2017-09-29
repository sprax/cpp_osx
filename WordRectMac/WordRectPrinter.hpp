// WordRectPrinter.hpp : display word rects in multi-threaded process
// Sprax Lines, July 2012

#ifndef WordRectPrinter_hpp
#define WordRectPrinter_hpp

#ifdef _MBCS	// Microsoft Compiler

#define WIN32_LEAN_AND_MEAN	// Exclude bells and whistles from Windows headers (e.g. Media Center Extensions)
#define VC_EXTRALEAN		// Exclude even more stuff from Windows headers (e.g. COMM, RPC, sound)
#include <windows.h>        // For Windows-specific threading and synchronization functions

#else



#endif

#include <time.h>
#include "wordPlatform.h"
#include "WordRectFinder.hpp"

class WordRectPrinter
{
public:

    template <typename MapT> 
    static void printRect(WordRectFinder<MapT> *pWRF)
    {
        time_t timeNow ;
        time( &timeNow );
        EnterCriticalSection(&sPrintLock);
        pWRF->printWordRectLastFound(timeNow);
        LeaveCriticalSection(&sPrintLock);
    }

    static void init()	{ InitializeCriticalSection(&sPrintLock); }
    static void stop()	{ DeleteCriticalSection(&sPrintLock); }

private:
    static CRITICAL_SECTION     sPrintLock;

};

#endif // WordRectPrinter_hpp
