// wordRectFinder.hpp : find largest dense rectangles of words from a dictionary, multi-threaded
// Sprax Lines, July 2010

#ifndef WordRectFinder_hpp
#define WordRectFinder_hpp

#include <map>

#include "WordTrie.hpp"
#include "TrixNode.hpp"


struct ltstr
{
    inline bool operator()(const char* s1, const char* s2) const
    {
        return strcmp(s1, s2) < 0;
    }
};
typedef std::map<const char*, int, ltstr> WordMap;

/**
*
*   Examples of Word Rectangle:
*		5x4 Word Rect:		4x4 Word Rect:		4x4 Symmetric Square, or "Word Square":
*		A A H E D			A A H S				P A L S
*		A B O D E			A B E T				A B U T
*		L U N G E			L U B E				L U B E
*		S T E E P			S T E W				S T E W
*/
#define DEFINE_TEMPLATES_IN_CPP 0   // put only declarations in .hpp

template <typename MapT>
class WordRectFinder
{
public:
    typedef int (WordRectFinder::*rowFinderFn)(int);

    static const int sBufSize = 64;

    // Define individual finder states.
    typedef enum { 
        eReady      = 1,
        eSearching  = 2,
        ePaused     = 3,
        eAborted    = 4,
        eFinished   = 5,
    } FinderState;

 
    WordRectFinder(WordTrie<MapT, TrixNode> *wordTries[], const WordMap *maps, int wide, int tall, uint numToFind, uint options);
    WordRectFinder(const WordRectFinder&);             // Prevent pass-by-value by not defining copy constructor.
    WordRectFinder& operator=(const WordRectFinder&);  // Prevent assignment by not defining this operator.
    virtual ~WordRectFinder() 
    {
        if (mRowWordsNow != NULL) {
            free(mRowWordsNow);
            free(mRowWordsOld);
            free(mRowNodesMem);
            free(mColNodesMem);
        }
    }                         

    inline uint			rowCharIndex(uchr letter) const { return mRowTrie.charIndex(letter); }
    inline uint			colCharIndex(uchr letter) const { return mColTrie.charIndex(letter); }
    inline void         setId(int id)           { mId = id; }
    inline time_t       getStartTime()  const   { return mStartTime;  }
    inline int          getWide()       const   { return mWantWide; }  
    inline int          getTall()       const   { return mWantTall; }
    inline void         setNowTall(uint nowTall){ mNowTall = nowTall; }
    inline int          getNowTall()    const   { return mNowTall; }
    inline int          getNumToFind()  const   { return mNumToFind; }
    inline int          getNumFound()   const   { return mNumFound; }
    inline int          incNumFound()           { return ++mNumFound; }
    inline FinderState  getState()      const   { return mState; }

    void printWordRectLastFound(time_t timeNow) const
    {
        // Show complete or in-progress word rectangle. 
        if (mMaxTall < mWantTall || mRowWordsOld == NULL) {
            printWordRectInProgress(timeNow);
            return;
        }

        int wide = getWide();
        int tall = getTall();
        const char *what = mIsLastRectSymSquare ? "Sym Square" : " Word Rect";
        printf("FOUND  %2d * %2d %s, area %3d in %2d seconds in thread %d (SS/WR %d / %d):\n"
            , wide, tall, what, wide*tall, (int)(timeNow - mStartTime), mId
            , mNumSymSquares, mNumFound);
        printWordRows(mRowWordsOld, mMaxTall);
    }

    virtual void printWordRectInProgress(time_t timeNow)    const
    {
        int wantWide = getWide();
        int haveTall = mNowTall;   // snapshot, because mHaveTall is prone to change between printf's
        printf("Trying %2d * %2d & found %2d rows after %2d seconds in thread %d (SS/WR %d / %d):\n", wantWide, getTall()
            , haveTall, (int)(timeNow - mStartTime), mId, mNumSymSquares, mNumFound);
        printWordRows(mRowWordsNow, haveTall);
    }

    /** Print actual word rows of complete or in-progress word rectangle. */
    virtual void printWordRows(const char *wordRows[], int haveTall) const
    {
        for (int k = 0; k < haveTall; k++) {
            for (const char *pc = wordRows[k]; *pc != '\0'; pc++) {
                printf(" %c", *pc);
            }
            printf("\n");
        }
    }

    int doSearch()              // template pattern: non-virtual base method calls virtuals
    {
        initRowsAndCols();
        time(&mStartTime);
        mState = eSearching;
        int foundArea = findWordRows();
        if (mMaxTall == mWantTall) {
            if (mState != eAborted)
                mState  = eFinished;
            return mWantArea;
        } else {
            return foundArea;
        }
    }

protected:
    virtual void initRowsAndCols();

#ifdef         findWordRectRowsUsingGetNextWordNodeAndIndex       // obsolete, but left here as a reference implementation
    static int findWordRectRowsUsingGetNextWordNodeAndIndex(  int wantWide, int wantTall, int haveTall, WordTrie& mRowTrie, WordTrie& mColTrie
        , const char *wordRows[sBufSize], const TrixNode ** colNodes[sBufSize] );
#endif
    int  findWordRectRowsMapUpper(int haveTall, const WordMap& rowMap, char wordCols[][sBufSize]);
    //virtual void printWordRows(const char *wordRows[], int haveTall)    const;

private:
    virtual int  findWordRows();    
    virtual int  findWordRowsUsingTrieLinks(int haveTall); // cpp comment
    //virtual bool isSymmetricSquare()                const;

    //template <typename  T> 
    virtual bool isSymmetricSquare() const
    {
        if (mWantTall != mWantWide)
            return false;                           // not square
        for (int row = mWantTall; --row >= 0; ) {
            for (int col = mWantTall; --col >= 0; ) {
                if (mRowNodes[row][col] != mColNodes[row][col])
                    return false;                   // not symmetric
            }
        }
        return true;                                // square and symmetric
    }

protected:
    const WordTrie<MapT, TrixNode>  &mRowTrie;
    const WordTrie<MapT, TrixNode>  &mColTrie;
    const int           mWantWide;
    const int           mWantTall;
    const int           mWantWideM1;
    const int           mWantArea;
    int                 mMaxTall;
    int                 mNowTall;
    int					mNumSymSquares;			// Number of symmetric squares found
    int                 mId;                    // Any unique id, but in practice, this is the thread index.
    const bool          mUseMaps;
    const bool          mUseAltA;
    bool                mIsLastRectSymSquare;   // Is mLastRect a symmetric word square?
    time_t              mStartTime;             // Negative value means never started.
    FinderState         mState;
    const char        **mRowWordsOld;				// The current array of words, as rows, i.e. the partial word rect being checked.
    const char        **mRowWordsNow;				// The most recently found word rect, if any.
    const TrixNode   ***mRowNodes, **mRowNodesMem;
    const TrixNode   ***mColNodes, **mColNodesMem;
    const WordMap      *mWordMaps;              // used only by findWordRectRowsMapUpper; to add more finders, consider using templates or a factory

private:
    const int           mNumToFind;
    int                 mNumFound;              // Number of wantWide X wantTall word rects found

};

#endif // WordRectFinder_hpp