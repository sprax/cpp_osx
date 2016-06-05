// WordRectSearchExec.hpp : find largest dense rectangles of words from a dictionary, multi-threaded
// Sprax Lines, July 2010

#ifndef WordRectSearchExec_hpp
#define WordRectSearchExec_hpp

#include "WordRectFinder.hpp"

#ifdef _MBCS	// Microsoft Compiler
#define WIN32_LEAN_AND_MEAN	// Exclude bells and whistles from Windows headers (e.g. Media Center Extensions)
#define VC_EXTRALEAN		// Exclude even more stuff from Windows headers (e.g. COMM, RPC, sound)
#include <windows.h>        // For Windows-specific threading and synchronization functions
#else
#endif

#ifndef _MBCS
#define CreateSemaphore(A,B,C,D)    (&WordRectSearchExec::sTheSemaphore)
#define CreateEvent(A,B,C,D)        (&WordRectSearchExec::sTheEvent)

#endif

class WordRectSearchExec // Singleton. 
{
public:

    // Define manager options as bit flags
    typedef enum { 
        eDefaultZero        =   0,
        eIncreasingSize     =   1,    // Order the search from smaller to larger rects
        eFindLattices       =   2,
        eFindWaffles        =   4,
        eOnlyOddDims        =   8,
        eSingleThreaded     =  16,
        eAbortIfTrumped     =  32,
        eTryTracNodes       =  64,
        eUseMaps            = 128,
        eTransformIndexes   = 256,
        eUseAltAlg          = 512,

    } ManagerOptions;

    static const uint   sMaxWordLength = 128;
    static const uint   sHardMaxFinders = 16;   // hard max number of worker threads -- used to declare array

    static WordRectSearchExec& getInstance() {
        if(!sInstance ) {
            sInstance = new WordRectSearchExec();
        }
        return *sInstance;
    }

    static void deleteInstance() {	
        if (sInstance != NULL)
            delete sInstance; 
    }

    void setOptions(uint managerFlags, uint minCharCount, uint minWordLength, uint maxWordLength, int verbose);

    int  startupSearchManager(const char *dictFileSpec
        , uint minArea, uint minTall
        , uint maxTall, uint maxArea
        , uint numEach, uint numTot);

    int  destroySearchManager();

    typedef unsigned long (*finderThreadFnType)(void *pvArgs);



private:
    WordRectSearchExec()        // private default constructor
#if	USE_BASE_CHAR_MAP
        : mBaseTries(NULL)
#else
        : mCompactTries(NULL), mIdentTries(NULL), mFreqTries(NULL)
#endif
        , mWordMaps(NULL), mNumWords(0), mMaxWordLength(0)
        , mManagerFlags(0), mVerbosity(0)
        , mSingleThreaded(false), mUseMaps(false)
    {}
    WordRectSearchExec(const WordRectSearchExec&);                // don't define
    WordRectSearchExec& operator=(const WordRectSearchExec&);     // don't define
    ~WordRectSearchExec() {
#if	USE_BASE_CHAR_MAP
        deleteTries(mBaseTries);
#else
        deleteTries(mIdentTries);
        deleteTries(mCompactTries);
        deleteTries(mFreqTries);
#endif
#ifndef _MBCS
        if (mWordMaps != NULL)
            delete [] mWordMaps;
#endif
    }

    template <typename MapT, typename NodeT>
    void deleteTries(WordTrie<MapT, NodeT> *tries[])
    {
        if (tries != NULL) {
            for (uint wordLen = 1; wordLen <= mMaxWordLength; wordLen++) {
                delete tries[wordLen];
            }
            delete [] tries;
        }
    }

    template <typename MapT>    // Loads from the file only once; no re-loading after init.  Modifies rMaxWordLength.
    static uint initFromSortedDictionaryFile(const char *fname, const MapT &charMap, WordTrie<MapT, TrixNode> *mTries[]
    , WordMap *maps, uint minWordLength, uint maxWordLength); 

    static int nextWantWideTall(int& wide, int& tall, const int& minTall, const int& maxTall
        , const int& minArea, const int& maxArea, const int& maxWide, bool ascending);

private:    // data

#if	USE_BASE_CHAR_MAP
    WordTrie<CharMap, TrixNode> ** mBaseTries;    // pointer to array of tries, indexed by word length
#else
    WordTrie<IdentCharMap>		** mIdentTries;    // pointer to array of tries, indexed by word length
    WordTrie<CompactCharMap>	** mCompactTries;    // pointer to array of tries, indexed by word length
    WordTrie<FreqFirstCharMap>	** mFreqTries;    // pointer to array of tries, indexed by word length
#endif

    WordMap           * mWordMaps; // pointer to array of word maps, indexed by word length
#ifdef _MBCS
    WordMap             mWordMapsMem[64];   // FIXME TODO remove unless this really is an MSVC bug
#endif
    uint                mNumWords;
    uint                mMinCharCount;	// The min char count used by CharFreqMap to make the mCharMap.  Publicly read-only.
    uint                mMinWordLength;
    uint                mMaxWordLength;
    int                 mVerbosity;   // 0=Report begin and end; 1=Report 1 find; 2=Report all finds & threads (default).

    // instance options
    uint                mManagerFlags;
    bool                mUseMaps;     
    bool                mSingleThreaded; 
    bool                mUseTracNodes;

    static WordRectSearchExec  * sInstance;

public: // FIXME: make public read-only

    // Synchronization objects to be shared by the worker threads, their manager, and the sigint handler object:
    // (Declared at file scope to keep windows.h out of WordRectMgr.h.  Make them member vars if converting to DLL.)
    static HANDLE               ghMaxThreadsSemaphore;
    static HANDLE               ghThreadStartedEvent;
    static CRITICAL_SECTION     gcsFinderSection;
    static int                  sTheEvent;
    static int                  sTheSemaphore;


};

#endif // WordRectSearchExec_hpp