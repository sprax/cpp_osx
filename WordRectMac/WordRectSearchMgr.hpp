// WordRectSearchMgr.hpp : find largest dense rectangles of words from a dictionary, multi-threaded
// Sprax Lines, July 2010

#ifndef WordRectSearchMgr_hpp
#define WordRectSearchMgr_hpp

#include "WordRectFinder.hpp"
#include "WordRectSearchExec.hpp"

#ifndef _MBCS	// Microsoft Compiler

//#include <assert.h>
#include <ctime>
#include <stdio.h>
#include <string.h>

typedef int CRITICAL_SECTION;
typedef int * HANDLE;
typedef int DWORD;
typedef bool BOOL;
typedef long LONG;
typedef unsigned long (*LPTHREAD_START_ROUTINE)(void *pvArgs);
//static  const int INVALID_HANDLE_VALUE = 0;   // not NULL
//static  int WAIT_TIMEOUT = 0;
#endif


template <typename MapT>
class WordRectSearchMgr
{
public:
    typedef unsigned long (*finderThreadFnType)(void *pvArgs);

    WordRectSearchMgr(WordTrie<MapT, TrixNode> **wordTries, WordMap *wordMaps, uint managerFlags, int verbosity)
        : mWordTries(wordTries), mWordMaps(wordMaps), mManagerFlags(managerFlags)
    {
        mAscending		= (managerFlags & WordRectSearchExec::eIncreasingSize)   > 0 ? true : false;
        mFindLattices	= (managerFlags & WordRectSearchExec::eFindLattices)	 > 0 ? true : false;
        mFindWaffles	= (managerFlags & WordRectSearchExec::eFindWaffles)		 > 0 ? true : false;
        mOnlyOddDims	= (managerFlags & WordRectSearchExec::eOnlyOddDims)		 > 0 ? true : false;
        mSingleThreaded = (managerFlags & WordRectSearchExec::eSingleThreaded)	 > 0 ? true : false;
        mUseTracNodes   = (managerFlags & WordRectSearchExec::eTransformIndexes) > 0 ? true : false;
        sAbortIfTrumped = (managerFlags & WordRectSearchExec::eAbortIfTrumped)   > 0 ? true : false;

        sVerbosity      = verbosity;    // expecting default value of 1
    }

    WordRectSearchMgr(const WordRectSearchMgr&);                // don't define
    WordRectSearchMgr& operator=(const WordRectSearchMgr&);     // don't define
    ~WordRectSearchMgr() {
    }

    static const  uint      sMaxWordLength = 128;
    static const  uint      sHardMaxFinders = 16;   // hard max number of worker threads -- used to declare array

    inline static time_t	getSearchStartTime()	{ return sSearchStartTime; }
    inline static int       getVerbosity()          { return sVerbosity; }    
    inline static int       getMinimumArea()        { return sMinimumArea; }

    inline static int		getTrumpingArea()       { return sTrumpingArea; }    

    class FinderThreadInfo 
    {      // private struct type for tracking finder threads
    public:
        FinderThreadInfo(WordRectFinder<MapT> *wrf)
            : mpWRF(wrf), mFinderIdx(-1), mThreadHandle(INVALID_HANDLE_VALUE)
        { };
        ~FinderThreadInfo() { delete mpWRF; }  
        HANDLE	            getThreadHandle()    const   { return mThreadHandle; }
        void	            setThreadHandle(HANDLE hndl) { mThreadHandle = hndl; }
        WordRectFinder<MapT> * getFinder() const { return mpWRF; }

        volatile int        mFinderIdx;
    private:
        WordRectFinder<MapT>  *mpWRF;
        HANDLE              mThreadHandle;
    };

    int  manageSearch(uint minArea, uint minTall, uint maxTall, uint maxArea, uint maxWide, uint numEach, uint numTot);

    static void printWordRectMutex(WordRectFinder<MapT> *pWRF);
    static void printWordWaffleMutex(WordRectFinder<MapT> *pWRF);

private:
    static  unsigned long   finderThreadFunc( void *pvArgs );
    // static unsigned long finderThreadFunc( void *pvArgs );
    // static finderThreadFnType finderThreadFunc;

    static void sigintHandler ( int sig );

    static int nextWantWideTall(int& wide, int& tall, const int& minTall, const int& maxTall
        , const int& minArea, const int& maxArea, const int& maxWide, bool ascending);



private:    // data

    WordTrie<MapT, TrixNode> ** mWordTries;	// pointer to array of pointers to trie, indexed by word length.  Tries owned by the Exec!
    WordMap                   * mWordMaps;	// pointer to array of word maps, indexed by word length
    uint                        mNumEach;
    uint                        mNumTotal;

    // instance options
    uint                mManagerFlags;
    bool                mFindLattices;
    bool                mFindWaffles;
    bool                mOnlyOddDims;
    bool                mAscending;   // search in order (roughly) of ascending area (default value 0, for biggest first)
    bool                mUseMaps;     
    bool                mSingleThreaded; 
    bool                mUseTracNodes;

    // static data
    static FinderThreadInfo   * sFinders[sHardMaxFinders];  // array for tracking workers	
    static int          sVerbosity;           // Greater value means more output.
    static int          sNumFinders;          // current number of worker threads
    static int          sSoftMaxFinders;      // soft maximum, as limited to twice the number of processors
    static uint         sFinderOptions;
    static time_t       sSearchStartTime;     // Static scope is OK because this class is a singleton
    static int			sFoundArea;
    static int			sTrumpingArea;
    static int			sMinimumArea;
    static bool         sAbortIfTrumped;
};

#endif // WordRectSearchMgr_hpp
