// wordRectSearchMgr.cpp : find largest dense rectangles of words from a dictionary, multi-threaded
// Sprax Lines, July 2010
//

#include "wordTypes.h"
#include "wordPlatform.h"

#ifdef _MBCS	// Microsoft Compiler

#define WIN32_LEAN_AND_MEAN	// Exclude bells and whistles from Windows headers (e.g. Media Center Extensions)
#define VC_EXTRALEAN		// Exclude even more stuff from Windows headers (e.g. COMM, RPC, sound)
#include <windows.h>        // For Windows-specific threading and synchronization functions

static const char *sThreadModel = "Windows";        // TODO: get specific

#else

#if defined(__GNUC__) 
static const char *sThreadModel = "Posix";    // TODO: threading for non-Windows
#elif defined(__APPLE__) 
static const char *sThreadModel = "NSThreads";    // TODO: threading for non-Windows
#else
static const char *sThreadModel = "Unknown";    // TODO: threading for non-Windows
#endif

#include <ctime>
#include <stdio.h>
#include <string.h>

static const int WAIT_TIMEOUT = 0;

#endif

#include <signal.h>
#include <time.h>

// project headers, including template class & method declarations:
#include "WordRectSearchMgr.hpp"
#include "CharFreqMap.hpp"
#include "WordLatticeFinder.hpp"
#include "WordWaffleFinder.hpp"

// project template class & template method definitions:
#include "WordRectFinder.cpp"
#include "WordLatticeFinder.cpp"
#include "WordWaffleFinder.cpp"
#include "WordWaffleFinderSearchA.cpp"

template <typename MapT>
typename WordRectSearchMgr<MapT>::FinderThreadInfo *WordRectSearchMgr<MapT>::sFinders[sHardMaxFinders];

template <typename MapT>   int     WordRectSearchMgr<MapT>::sVerbosity        = 0;
template <typename MapT>   int     WordRectSearchMgr<MapT>::sNumFinders       = 0;
template <typename MapT>   int     WordRectSearchMgr<MapT>::sSoftMaxFinders   = 0;
template <typename MapT>   uint    WordRectSearchMgr<MapT>::sFinderOptions    = 0;
template <typename MapT>   time_t  WordRectSearchMgr<MapT>::sSearchStartTime  = 0;
template <typename MapT>   int     WordRectSearchMgr<MapT>::sFoundArea        = 0;
template <typename MapT>   int	    WordRectSearchMgr<MapT>::sTrumpingArea     = 0;
template <typename MapT>   int	    WordRectSearchMgr<MapT>::sMinimumArea      = 0;
template <typename MapT>   bool    WordRectSearchMgr<MapT>::sAbortIfTrumped   = true;

#ifndef _MBCS
template <typename MapT>
static HANDLE CreateThread(HANDLE handle, int notUsed, typename WordRectSearchMgr<MapT>::finderThreadFnType finder, void *pArgs, int notB, DWORD *threadId)
{
    finder(pArgs);
    return (HANDLE) 1;
}
#endif

template <typename MapT> 
int WordRectSearchMgr<MapT>::manageSearch(uint minArea, uint minTall, uint maxTall, uint maxArea, uint maxWordLength, uint numEach, uint numTot)
{
    sMinimumArea = minArea;

    if (maxTall > maxWordLength)
        maxTall = maxWordLength;
    if (maxArea > maxWordLength*maxWordLength)
        maxArea = maxWordLength*maxWordLength;
    mNumEach = numEach;
    mNumTotal = numTot;
    

#ifndef _MBCS
    mSingleThreaded = true;
#endif
    // Init the number of word rect finder threads:
    // TODO: The number is somewhat empirical, and should perhaps depend on hyper-threading,
    // or possibly other architecture characteristics, if available.
    if (mSingleThreaded) {
        sSoftMaxFinders = 1;
        printf("Single-threaded mode: Searching with only one thread (%s).\n", sThreadModel);
    } else {
        SYSTEM_INFO sysinfo;
        GetSystemInfo( &sysinfo );
        int numProcessors = (int)sysinfo.dwNumberOfProcessors;
        int idealNumThreads = 3*numProcessors/2;                
        sSoftMaxFinders = idealNumThreads < sHardMaxFinders ? idealNumThreads : sHardMaxFinders;
        printf("Found %d processors (or cores); will search with up to %d threads (%s).\n"
               , numProcessors, sSoftMaxFinders, sThreadModel);
    }

    WordRectSearchExec::ghMaxThreadsSemaphore = CreateSemaphore(NULL, (LONG)sSoftMaxFinders, (LONG)sSoftMaxFinders, "MaxThreadsSemaphore");
    if (WordRectSearchExec::ghMaxThreadsSemaphore == NULL) 
    {
        printf("CreateSemaphore error: %d\n", GetLastError());
        return -3;
    }
    WordRectSearchExec::ghThreadStartedEvent  = CreateEvent(NULL, true, false, NULL);
    InitializeCriticalSection(&WordRectSearchExec::gcsFinderSection);

    time( &sSearchStartTime );
    char    ctimeBuf[ CTIME_SAFE_BUFSIZE ];
    ctime_safe(ctimeBuf, CTIME_SAFE_BUFSIZE, &sSearchStartTime );
    printf("Starting the search at %s\n", ctimeBuf);
    printf("    Hit Ctrl-C to see the word rectangle(s) in progress...\n\n");
    signal( SIGINT, sigintHandler);

    DWORD result, threadId;
    int  wantWide = -1, wantTall = -1;
    while( nextWantWideTall(wantWide, wantTall, minTall, maxTall, minArea, maxArea, maxWordLength, mAscending) ) {

        if (mOnlyOddDims && (wantWide % 2 == 0 || wantTall % 2 == 0))
            continue;

        int wantArea = wantWide*wantTall;
        if (wantArea <= getTrumpingArea()) {
            printf("Skip   %2d * %2d because %3d <= %3d (area already found)\n", wantWide, wantTall, wantArea, getTrumpingArea());
            continue;
        }

        // Create a new worker thread.  We don't try to control or even observe the 
        // worker threads here in this loop.  Just create one and let it synchronize itself
        // with any that are already running.  If the max number of worker threads are already
        // running, then before this new one actually starts searching, it will have to wait 
        // (on a semaphore) until one of those other threads exits.  The only synchronization 
        // here in this loop is to wait (on an event) for each new thread to start running before 
        // creating another one.  The  threadFunc will be responsible for deleting the WordRectFinder 
        // that we create here and pass in to CreateThread.  (That's easier than making a WRF-factory
        // just for the thread func!)

        assert(mWordTries);
        WordRectFinder<MapT> *pWRF = NULL;
        if (mFindLattices) {
            pWRF = new WordLatticeFinder<MapT>(mWordTries, mWordMaps, wantWide, wantTall, numEach, sFinderOptions);
        } else if (mFindWaffles) {
            pWRF = new WordWaffleFinder<MapT>(mWordTries, mWordMaps, wantWide, wantTall, numEach, sFinderOptions);
        } else {
            pWRF = new WordRectFinder<MapT>(mWordTries, mWordMaps, wantWide, wantTall, mNumEach, sFinderOptions);
        }
#ifdef _MBCS
        HANDLE threadHandle = CreateThread   ( NULL, 0, (LPTHREAD_START_ROUTINE)finderThreadFunc, pWRF, 0, &threadId );
#else
        HANDLE threadHandle = CreateThread<MapT>( NULL, 0, (LPTHREAD_START_ROUTINE)finderThreadFunc, pWRF, 0, &threadId );
#endif
        if (threadHandle == INVALID_HANDLE_VALUE) {
            delete pWRF;
            printf("\n    CreateThread failed for %d x %d finder!  Aborting.\n", wantWide, wantTall);
            return -4;
        }
        pWRF = NULL;    // Forget we ever knew (or newed) this pointer!   ;p

        // We don't strictly need to wait for this thread to start before initializing the next
        // one in the loop, because we are not initializing these threads using pointers to 
        // local memory that might change between the time the thread is created and the time
        // it actually starts processing its function arguments.  (Just calling Sleep(1) would
        // *probably* prevent such slippage.  But it's nice to make sure the threads start in 
        // the order they are created here, and not to create a whole lot of them before we know
        // they are needed, or that will just get aborted anyway.  
        // So we wait for the child thread to set this event, then reset it:
        result = WaitForSingleObject(WordRectSearchExec::ghThreadStartedEvent, INFINITE); 
        if (result == WAIT_FAILED) {
            printf("WaitForSingleObject failed on the start-one-finder-at-a-time event; error: %d  continuing\n", GetLastError());
        }
        (void)ResetEvent(WordRectSearchExec::ghThreadStartedEvent);                         // ignore the return value
    }

    // Wait for all remaining finder threads to end.  If the sigint handler happens to be active,
    // these finder threads must wait for it to yield the critical section before they can die.
    HANDLE finderThreadHandles[sHardMaxFinders];
    EnterCriticalSection(&WordRectSearchExec::gcsFinderSection);
    for (int j = 0; j < sNumFinders; j++) {
        finderThreadHandles[j] = sFinders[j]->getThreadHandle();
    }
    LeaveCriticalSection(&WordRectSearchExec::gcsFinderSection);
    result = WaitForMultipleObjects(sNumFinders, finderThreadHandles, true, INFINITE);

    time_t timeEnd = time( &timeEnd );
    ctime_safe(ctimeBuf, CTIME_SAFE_BUFSIZE, &timeEnd );
    printf("\nFinished the search %s", ctimeBuf);
    printf("Elapsed time: %d seconds\n", (int)(timeEnd - sSearchStartTime) );

    CloseHandle(WordRectSearchExec::ghMaxThreadsSemaphore);
    CloseHandle(WordRectSearchExec::ghThreadStartedEvent);
    DeleteCriticalSection(&WordRectSearchExec::gcsFinderSection);
    return 0;
}


template <typename MapT> 
unsigned long WordRectSearchMgr<MapT>::finderThreadFunc( void *pvArgs )
{
    WordRectFinder<MapT> *pWRF = static_cast<WordRectFinder<MapT> *>(pvArgs);
    int   wantWide  =  pWRF->getWide();
    int   wantTall  =  pWRF->getTall();

    // Use the passed-in pWRF to construct our FinderThreadInfo on the thread-local stack.
    // When this FTI falls out of scope, its destructor will delete the pWRF.
    // That's it for memory managment here.  Unless the synchronization or duplicate handle
    // calls are doing it, no heap memory is allocated by this func or the WRF methods it calls.
    FinderThreadInfo myFTI(pWRF);

    DWORD result  = WaitForSingleObject(WordRectSearchExec::ghMaxThreadsSemaphore, INFINITE);
    if (result != WAIT_OBJECT_0) { 
        printf("Error waiting for MaxThreadsSemaphore: res:%d  err: %d.  Aborting finderThreadFunc..\n", result, GetLastError() );
        return 1;
    }

    // The pseudo handle returned by GetCurrentThread is no good for synchronization by another
    // thread.  To allow our main thread to wait on this thread, we need to give it a handle
    // that has synchronization access rights.
    HANDLE hPseudo = GetCurrentThread();
    HANDLE hProcess = GetCurrentProcess();
    HANDLE hRealThread = 0;
    BOOL success = DuplicateHandle(hProcess, hPseudo, hProcess, &hRealThread, SYNCHRONIZE, false, 0);
    if (!success) {
        printf("DuplicateHandle failed with error: %d.  Aborting finderThreadFunc.\n", GetLastError() );
        return 3;
    }
    myFTI.setThreadHandle(hRealThread);

    // Synchronize checking on the current largest word rect + writing to shared storage (sFinders):
    EnterCriticalSection(&WordRectSearchExec::gcsFinderSection);
    time_t timeNow;
    time( &timeNow );

    // Now that we are past the semaphore and inside the critical section, we check again
    // whether this finder is still needed, or has been scooped by another while we waited to start.  
    const int wantArea     =  wantWide*wantTall;
    const int foundArea    =  getTrumpingArea();
    const bool doRegister  = (wantArea > foundArea);

    if (doRegister) {
        int minimumArea  = getMinimumArea();
        char equalsChar  = '=';         // if we have not yet found a wordRect, we want area >= minimum
        if (minimumArea <= foundArea && sAbortIfTrumped) {
            minimumArea  = foundArea;
            equalsChar   = ' ';         // if we have found a wordRect and are trumping, we want area > found
        }
        if (sVerbosity) {
            time_t startTime = getSearchStartTime();
            printf("TRY    %2d * %2d because %3d >%c %3d (thread %2d began after %2d seconds)\n"
                , wantWide, wantTall, wantArea, equalsChar, minimumArea
                , sNumFinders, (int)(timeNow - startTime));
        }
        // We're about to put the address of our local FTI into shared storage (sFinders), 
        // to be used by the manager's wait functions and sigint handler.   Obviously that 
        // handler had better lock and make sure this thread is still alive before dereferencing
        // the pointer.  In turn, we have to lock and remove it from that storage before deleting it below.
        myFTI.mFinderIdx = sNumFinders;
        myFTI.getFinder()->setId(sNumFinders);
        assert(sNumFinders < sSoftMaxFinders);
        assert(sFinders[sNumFinders] == NULL);
        sFinders[sNumFinders] = &myFTI;    // Add our info to end slot, which should already be empty.
        ++sNumFinders;
    } else {
        if (sVerbosity) {
            printf("CANCEL %2d * %2d because %3d <= %3d (thread %2d dying after 0 seconds)\n"
                , wantWide, wantTall, wantArea, foundArea, sNumFinders);
        }
    }
    // We finished initializing and registering our thread info in the managers' table 
    // (if this finder was still relevant), so now we can leave the CS and set the Event.
    // Other finder threads may be waiting to begin or end, and their doing so could affect 
    // what the manager does next, so we release the CS first.  (In particular, if another 
    // thread just found the biggest wordRect, the manager may find this out by calling 
    // getFoundArea and then "decide" not start any more finder threads.)
    LeaveCriticalSection(&WordRectSearchExec::gcsFinderSection);

    // Only then do we tell the calling loop it can resume and possibly create another finder thread:
    success = SetEvent(WordRectSearchExec::ghThreadStartedEvent);
    if ( ! success) {
        printf("SetEvent failed with error: %d.  Aborting finderThreadFunc.\n", GetLastError() );
        return 5;
    }

    // Before starting this search, which may take milliseconds or hours
    // (or be aborted as soon as it gets going), let's check its relevancy one more time.
    int area = -wantArea;           // Negative area means this search was aborted. 
    if (doRegister && wantArea > getTrumpingArea() ) {
        area = pWRF->doSearch();
    }
    time( &timeNow );   // Immediately record the time this search finished or aborted

    {   //beg CriticalSection block
        EnterCriticalSection(&WordRectSearchExec::gcsFinderSection);

        // Note that if the sigint handler uses this same sync object, so it's in the middle 
        // of printing an in-progress word rect, we'll wait for it here before we check the
        // final area found, possibly print a new found rect, and then delete the pWRF.
        // by falling out of scope.

        // If this finder thread found the new max area, save it.
        if (sFoundArea < area) {
            sFoundArea = area;
            if (sAbortIfTrumped) {
                sTrumpingArea = sFoundArea;
            }
        }
        if (sVerbosity > 0) {
            time_t timeBeg = pWRF->getStartTime();
            int elapsedSeconds = (timeBeg < 0) ? 0 : (int)(timeNow - timeBeg);
            int numFound = pWRF->getNumFound();
#define MSG_LEN 64
            char statusMsg[MSG_LEN];
            if (pWRF->getState() == WordRectFinder<MapT>::eAborted && sVerbosity > 1) {
                sprintf_safe(statusMsg, MSG_LEN, "Aborted, %d rects, because %d <= %d.", numFound, -area, sTrumpingArea);
            } else if (numFound > 0) {
                sprintf_safe(statusMsg, MSG_LEN, "Success: Finished with %d rects.", numFound);
            } else if (sVerbosity > 1) {
                sprintf_safe(statusMsg, MSG_LEN, "Failure: Finished with %d rects.", numFound);
            }
            printf("End    %2d * %2d (%d) at %d seconds; %s\n"
                , wantWide, wantTall, myFTI.mFinderIdx, elapsedSeconds, statusMsg);

            // If at least one was found and not already shown, show it now
            if (area > 0) {
                pWRF->printWordRectLastFound(timeNow);   // show latest complete word rect
            }
        }
        if (doRegister) {   // We did register, so now we must unregister.
            // Remove our FinderThreadInfo from the shared storage:
            int finderIdx = myFTI.mFinderIdx;                // this value may have changed
            assert(sFinders[finderIdx]->getFinder() == pWRF);    // assert that we are in sync
            --sNumFinders;
            assert(sNumFinders >= 0);
            sFinders[finderIdx] = sFinders[sNumFinders];    // Compactify by moving the last entry into our spot (even if it's the same).
            sFinders[finderIdx]->mFinderIdx = finderIdx;    // Tell this entry it where it now is.
            sFinders[sNumFinders] = NULL;                   // Nullify the vacated spot at the end (again, it could be our own at idx 0).
        }
        LeaveCriticalSection(&WordRectSearchExec::gcsFinderSection);
    }   //end CriticalSection block

    // Finally, release the sempaphore (decrement the number of users)
    result = ReleaseSemaphore(WordRectSearchExec::ghMaxThreadsSemaphore, 1, NULL);
    if (result == 0) {
        printf("Error from ReleaseSemaphore: %d.  Aborting finderThreadFunc.\n", GetLastError() );
        return 7;
    }
    return 0;
}

template <typename MapT> 
void WordRectSearchMgr<MapT>::sigintHandler ( int sig )
{
    static time_t   timeLastSigint = 0;
    static int      signalCount = 0;

    // On SIGINT (Ctrl-C), print the word rectangle(s) in progress, or, on double-Ctrl-C, exit.
    if (sig != SIGINT) {
        return;                 // handle only Ctrl-C
    }
    signal(sig,  SIG_IGN);      // ignore all Ctrl-C's until the handler is reset

    time_t timeNow;
    time( &timeNow );                               // time now, in seconds since 1969
    timeLastSigint = timeNow - timeLastSigint;      // seconds since last call
    if (timeLastSigint < 2) {
        printf("sigintHandler: time difference < 2 seconds -- bye bye!\n");
        exit(0);
    }

    time_t elapsedTime = timeNow - getSearchStartTime();
    int seconds = (int)(elapsedTime % 60);
    int minutes = (int)(elapsedTime / 60) % 60;
    int hours   = (int)(elapsedTime / 3600);
    char    ctimeBuf[ CTIME_SAFE_BUFSIZE ];
    ctime_safe(ctimeBuf, CTIME_SAFE_BUFSIZE, &timeNow );
    printf("\n    Ctrl-C: Progress check at (%02d:%02d:%02d) %s\n"
        , hours, minutes, seconds, ctimeBuf);

    {
        EnterCriticalSection(&WordRectSearchExec::gcsFinderSection);
        timeLastSigint = timeNow;                       // remember the time of this call
        for (int j = 0; j < sNumFinders; j++) {
            const FinderThreadInfo *pFTI = (FinderThreadInfo *) sFinders[j];
            if (pFTI) {
                const WordRectFinder<MapT> *pWRF = pFTI->getFinder();
                DWORD status = WaitForSingleObject(pFTI->getThreadHandle(), 0);
                if (status == WAIT_TIMEOUT) {
                    // The finder thread is still alive, so display its progress
                    pWRF->printWordRectInProgress(timeNow);   // show latest (partial) word rect
                } else {
                    // The finder thread was signaled, or the wait failed
                    if (sVerbosity) {
                        printf("Thread info not found for %d x %d finder...\n", pWRF->getWide(), pWRF->getTall());
                    }
                }
            }
        }
        LeaveCriticalSection(&WordRectSearchExec::gcsFinderSection);
    }

    if (++signalCount < 7) {	// advise the user, the first half dozen times or so...
        printf("\n    To exit, hit Ctrl-C twice, quickly.\n\n");
    } else {
        printf("\n");
    }
    signal(SIGINT, &sigintHandler);		            // reinstall this handler
}


template <typename MapT>
int WordRectSearchMgr<MapT>::nextWantWideTall(int& wide, int& tall, const int& minTall, const int& maxTall
    , const int& minArea, const int& maxArea, const int& maxWide, bool ascending)
{
    if (ascending) {
        if (tall < 0) {                     // special case:
            tall = wide = minTall;          // initialize
        } else if (wide < maxWide) {
            wide++;
        } else {
            tall++;
            wide = tall;
        }
        for ( ; tall <= maxTall ; ++tall, wide = tall ) {
            for ( ; wide <= maxWide ; wide++ ) {
                int area = wide*tall;
                if (minArea <= area && area <= maxArea) {
                    return area;
                }
            }
        }
    } else {
        if (wide < 0 && tall < 0) {     // special case: initialize...
            wide = maxWide;              
            tall = maxTall;             // no special checking: the case maxWide < maxTall falls through to 0
        } else if (wide > tall) {
            wide--;
        } else {
            tall--;
            wide = maxWide;
        }
        for ( ; tall >= minTall ; --tall, wide = maxWide ) {
            for ( ; wide >= tall ; wide-- ) {
                int area = wide*tall;
                if (minArea <= area && area <= maxArea) {
                    return area;
                }
            }
        }
    }
    return 0;     // default: no suitable values for wide & tall remain
}

template <typename MapT> 
void WordRectSearchMgr<MapT>::printWordRectMutex(WordRectFinder<MapT> *pWRF)
{
    if (sVerbosity > 0) {
        time_t timeNow ;
        time( &timeNow );
        EnterCriticalSection(&gcsFinderSection);
        pWRF->printWordRectLastFound(timeNow);
        LeaveCriticalSection(&gcsFinderSection);
    }
}


//template <typename MapT> 
//void WordRectSearchMgr<MapT>::printFinderWordWaffle(WordRectFinder<MapT> *pWRF)
//{
//	time_t timeNow ;
//	time( &timeNow );
//	EnterCriticalSection(&gcsFinderSection);
//	pWRF->printLatestRect(timeNow);
//	LeaveCriticalSection(&gcsFinderSection);
//}

/*
C:\asd\_MSVC\wordRectMulti\Debug\wordRectMulti.exe: using tries,
min & max Area: 44 44, min & max Height: 4 4.
Found 2 processors (or cores); will search with up to 3 threads.
Starting the search at Sat Aug 11 00:13:43 2012

Hit Ctrl-C to see the word rectangle(s) in progress...

TRY    11 *  4 because  44 >=  44 (thread  0 began after  0 seconds)
FOUND  11 *  4  Word Rect, area  44 in 90 seconds in thread 0 (SS/WR 0 / 1):
G A S T R O T R I C H
A G O R A P H O B I A
D E L I N E A T I N G
S E A M I N E S S E S
FOUND  11 *  4  Word Rect, area  44 in 188 seconds in thread 0 (SS/WR 0 / 2):
S C L E R O D E R M A
R E A N I M A T I O N
I N T O L E R A B L E
S T I L L N E S S E S

End    11 *  4 because  44 <=   0 or search exhausted (thread  0 finished in 230
seconds)
LAST FOUND  11 *  4  Word Rect, area  44 in 230 seconds in thread 0 (SS/WR 0 / 2):
S C L E R O D E R M A
R E A N I M A T I O N
I N T O L E R A B L E
S T I L L N E S S E S


Finished the search Sat Aug 11 00:17:33 2012
Elapsed time: 230 seconds
*/

/*
D:\asd\_MSVC\wordRectMulti>Release\wordRectMulti.exe -sz 64 8 8 72
Release\wordRectMulti.exe: using tries,
min & max Area: 64 72, min & max Height: 8 8.
Found 4 processors (or cores); will search with up to 6 threads.
Starting the search at Fri Aug 10 12:02:10 2012

Hit Ctrl-C to see the word rectangle(s) in progress...

TRY     8 *  8 because  64 >=  64 (thread  0 began after  0 seconds)
TRY     9 *  8 because  72 >=  64 (thread  1 began after  0 seconds)
Trying  8 *  8 & found  3 rows after 26 seconds in thread 0 (SS/WR 0 / 0):
A B A L O N E S
L I G U L A T E
P R E M O R A L
Trying  9 *  8 & found  3 rows after 26 seconds in thread 1 (SS/WR 0 / 0):
A B I D A N C E S
M A N I C O T T I
T H I O U R E A S

To exit, hit Ctrl-C twice, quickly.

FOUND   8 *  8 Sym Square, area  64 in 8181 seconds in thread 0 (SS/WR 1 / 1):
C A R B O R A S
A P E R I E N T
R E C A L L E R
B R A S S I C A
O I L S E E D S
R E L I E V O S
A N E C D O T E
S T R A S S E S
End     9 *  8 because none found (thread  1 finished in 8943 seconds)
FOUND   8 *  8 Sym Square, area  64 in 10567 seconds in thread 0 (SS/WR 2 / 2):
C R A B W I S E
R A T L I N E S
A T L A N T E S
B L A S T E M A
W I N T E R L Y
I N T E R T I E
S E E M L I E R
E S S A Y E R S
FOUND   8 *  8 Sym Square, area  64 in 20822 seconds in thread 0 (SS/WR 3 / 3):
N E R E I D E S
E N E R G I S E
R E S O N A T E
E R O T I Z E D
I G N I T E R S
D I A Z E P A M
E S T E R A S E
S E E D S M E N
FOUND   8 *  8 Sym Square, area  64 in 20822 seconds in thread 0 (SS/WR 4 / 4):
N E R E I D E S
E T E R N I S E
R E L O C A T E
E R O T I Z E D
I N C I T E R S
D I A Z E P A M
E S T E R A S E
S E E D S M E N
FOUND   8 *  8 Sym Square, area  64 in 20822 seconds in thread 0 (SS/WR 5 / 5):
N E R E I D E S
E T E R N I S E
R E N O V A T E
E R O T I Z E D
I N V I T E R S
D I A Z E P A M
E S T E R A S E
S E E D S M E N
End     8 *  8 because  64 <=   0 or search exhausted (thread  0 finished in 36223 seconds)
FOUND   8 *  8 Sym Square, area  64 in 36223 seconds in thread 0 (SS/WR 5 / 5):
N E R E I D E S
E T E R N I S E
R E N O V A T E
E R O T I Z E D
I N V I T E R S
D I A Z E P A M
E S T E R A S E
S E E D S M E N

Finished the search at Fri Aug 10 22:05:53 2012
Elapsed time: 36223 seconds
*/
