// wordRectSearchMgr.cpp : find largest dense rectangles of words from a dictionary, multi-threaded
// Sprax Lines, July 2010
//
// TODO (WordRectMulti Project): 
//      Save memory by not loading dictionary words too long or short for the specified search.
//      If adding more search methods, turn wordRectFinder into an interface and let a factory make the 
//          concrete subclasses.
//      Limit wordRows & colNodes to actual dimensions?    
//          Pros: *slightly* smaller footprint;
//          Cons: don't expect much speed-up (reducing L2 cache misses?); calling new and delete 
//              could introduce bottlenecks
//      Maybe divide searchForLargestWordRect into separate init, search, and quit methods, and
//          allow multiple instances with init and teardown done only once?  No, too much work,
//          no real pay-off.  As of now, the methods here can call exit on any error condition.
//      Send error messages to stderr instead of stdout?
//      Convert printf's to cout or something else?  Right now, the i/o buffering seems to play well 
//          among the threads (maybe because of the critical section).

// project headers, including template class & method declarations:
#include "WordRectSearchExec.hpp"
#include "TrixNode.hpp"

// project template class & template method definitions:
#include "TracNode.cpp"
#include "WordRectSearchMgr.cpp"
#include "WordTrie.cpp"

WordRectSearchExec        * WordRectSearchExec::sInstance = NULL;

// Synchronization objects to be shared by the worker threads, their manager, and the sigint handler object:
// (Declared at file scope to keep windows.h out of WordRectMgr.h.  Make them member vars if converting to DLL.)
HANDLE              WordRectSearchExec::ghMaxThreadsSemaphore = INVALID_HANDLE_VALUE;
HANDLE              WordRectSearchExec::ghThreadStartedEvent;
CRITICAL_SECTION    WordRectSearchExec::gcsFinderSection;
int                 WordRectSearchExec::sTheEvent = 1;
int                 WordRectSearchExec::sTheSemaphore = 1;


void WordRectSearchExec::setOptions(uint managerFlags, uint minCharCount, uint minWordLength, uint maxWordLength, int verbosity)
{
    mManagerFlags   = managerFlags;
    mSingleThreaded = (managerFlags & eSingleThreaded)	 > 0 ? true : false;
    mUseTracNodes   = (managerFlags & eTransformIndexes) > 0 ? true : false;
    mUseMaps        = (managerFlags & WordRectSearchExec::eUseMaps) > 0 ? true : false;
    mMinWordLength  = minWordLength;
    mMinCharCount   = minCharCount;
    mMaxWordLength  = maxWordLength;
    mVerbosity      = verbosity;
};

#ifdef _DEBUG
static void test_freqMap(const CharFreqMap& charFreqMap)
{
#define WANT_LINK_ERROR 0
#if		WANT_LINK_ERROR
    const bool wantRunTimeError = false;
    ////CharMap				charMap(52, 'A');	// error: protected constructor
    CharMap           * pCharMap = NULL;
    if (wantRunTimeError)
        WordTrie<CharMap>	charMapWordTrie(*pCharMap, 10);
    ////uint index =        charMapWordTrie.charIndex('C');	// MSVC error C2039: 'charIndex' : is not a member of 'CharMap'

    ////ArrayCharMap		arrayCharMap(52, 'A');	// error: protected constructor
    ArrayCharMap	  * pArrayCharMap = NULL;
    if (wantRunTimeError)
        WordTrie<CharMap>	arrayWordTrie(*pArrayCharMap, 11);
    ////uint index =        arrayWordTrie.charIndex('C');	// MSVC error C2039: 'charIndex' : is not a member of 'CharMap'

    IdentCharMap		identCharMap(charFreqMap);
    WordTrie<CharMap>	identWordTrie(identCharMap, 12);

    CompactCharMap	    compactCharMap(charFreqMap);
    WordTrie<CharMap>	compactWordTrie(compactCharMap, 13);

    FreqFirstCharMap	freqFirstCharMap(charFreqMap, 50);
    WordTrie<CharMap>	freqFirstWordTrie(freqFirstCharMap, 14);
#endif // WANT_LINK_ERROR
}
#endif

int WordRectSearchExec::startupSearchManager(const char *dictFile
    , uint minArea, uint minTall, uint maxTall, uint maxArea, uint numEach, uint numTot)
{   
    CharFreqMap charFreqMap(dictFile, mMinWordLength, mMaxWordLength, mMinCharCount); 
    int err = charFreqMap.initFromFile(mMinCharCount, mMinWordLength, mMaxWordLength, mVerbosity);
    if (err < 0)
        return err;
    const CharMap::SubType defaultType = charFreqMap.getDefaultCharMapSubType(); 
    const uint minFoundWordLength = charFreqMap.getMinFoundWordLength();
    const uint maxFoundWordLength = charFreqMap.getMaxFoundWordLength();
    if (minFoundWordLength > mMaxWordLength)
        return -11;
    if (maxFoundWordLength < mMinWordLength)
        return -12;
    if (mMinWordLength < minFoundWordLength)
        mMinWordLength = minFoundWordLength;
    if (mMaxWordLength > maxFoundWordLength)
        mMaxWordLength = maxFoundWordLength;

    if (mUseMaps) {
#ifdef _MBCS
        mWordMaps = mWordMapsMem;   // TODO: why not new?
#else
        mWordMaps = new WordMap[mMaxWordLength+1];  // index wordMaps by word length (not length-1)
#endif
    }

    if (mVerbosity > 0)
        printf("Default CharMap sub-type is: %s\n", CharMap::getSubTypeName(defaultType));

    WordRectPrinter::init();


#if USE_BASE_CHAR_MAP

    const CharMap &charMap = charFreqMap.makeDefaultCharMap();

    // index array of tries by word length (not by strlen(word) - 1); mTries[0] remains NULL. 
    //mBaseTries = (WordTrie<CharMap> **)calloc(maxLineLength+1, sizeof(WordTrie<CharMap> *));
    mBaseTries = new WordTrie<CharMap, TrixNode>*[mMaxWordLength+1]; // index wordMaps by word length

#if USE_VIRT_CHAR_IDX
    // create (and later delete) the tries in a loop; we don't want a default constructor
    for (uint wordLen = 1; wordLen <= mMaxWordLength; wordLen++)
        mBaseTries[wordLen] = new WordTrie<CharMap, TrixNode>(charMap, wordLen);
    mNumWords += initFromSortedDictionaryFile(dictFile, charMap, mBaseTries, mWordMaps, mMinWordLength, mMaxWordLength);
    if (mNumWords == 0) {
        printf("SearchExec Found no words in dictionary %s.  Aborting.)\n", dictFile);
        return -2;
    }

    WordRectSearchMgr<CharMap> *searchMgr = new WordRectSearchMgr<CharMap>(mBaseTries, mWordMaps
        , mManagerFlags, mVerbosity);
#else
    FreqFirstCharMap *pFFCM = (FreqFirstCharMap *) &charMap;
    CharMap::SubType charMapType = pFFCM->subType();
    if (charMapType != defaultType) {
        printf("ERROR: defaultType is %d but explicit cast type is %d; crash is likely...\n", defaultType, charMapType);
        printf("ABORT: explicit cast will mis-reference array.\n");
        return -1;
    }
    for (uint wordLen = 1; wordLen <= maxLineLength; wordLen++)
        mBaseTries[wordLen] =  (WordTrie<CharMap> *) new WordTrie<FreqFirstCharMap>(*pFFCM, wordLen);
    mNumWords += initFromSortedDictionaryFile(dictFile, *pFFCM, (WordTrie<FreqFirstCharMap>**)mBaseTries, mWordMaps, mMaxWordLength);
    if (mNumWords == 0) {
        printf("SearchExec Found no words in dictionary %s.  Aborting.)\n", dictFile);
        return -2;
    }
    WordRectSearchMgr<FreqFirstCharMap> *searchMgr = new WordRectSearchMgr<FreqFirstCharMap>((WordTrie<FreqFirstCharMap>**)mBaseTries, mWordMaps
        , mManagerFlags, mFinderOptions, mVerbosity);
#endif

#ifdef _DEBUG
    test_freqMap(charFreqMap);
    if (mManagerFlags & eTryTracNodes)
        TracNode::test_TracNode(*mBaseTries[7], charMap);

    FreqFirstWildCharMap *pFFWCM = new FreqFirstWildCharMap(charFreqMap, 1000); 
    WordTrie<FreqFirstWildCharMap, TracNode> wtf(*pFFWCM, 0);
    int numAdded = 0;
    numAdded = WordTrie<FreqFirstWildCharMap, TracNode>::addAllWordsInTextFile(dictFile, &wtf, mMinWordLength, mMaxWordLength, mVerbosity);
    assert(numAdded > 0);
////numAdded = WordTrie<FreqFirstWildCharMap, TracNode>::addAllWordsInTextFile(dictFile, &wtf, mMinWordLength, mMaxWordLength, mVerbosity);
////assert(numAdded == 0);

    if (mVerbosity > 5) {
        uint totalNodes = wtf.countBranchesPerChar(charFreqMap);
        printf("mTotalNodes vs. counted nodes:  %d  %d\n", wtf.mTotalNodes, totalNodes);

        uint reTotNodes = wtf.countBranchesPerCharPos(charFreqMap, 10);
        printf("mTotalNodes vs. counted nodes:  %d  %d\n", wtf.mTotalNodes, reTotNodes);
        exit(0); // FIXME
    }

#ifndef __APPLE__
    numAdded = WordTrie<FreqFirstWildCharMap, TracNode>::addAllWordsInTextFile("../MarkTwain/TomSawyer_En.txt", &wtf, mMinWordLength, mMaxWordLength, mVerbosity);
    assert(numAdded > 0);
#endif

#endif

    searchMgr->manageSearch(minArea, minTall, maxTall, maxArea, mMaxWordLength, numEach, numTot);

#else

    // create (and later delete) the tries in a loop; we don't want a default constructor
#ifdef _DEBUG
    const CharMap &defaultCharMap = charFreqMap.makeDefaultCharMap();
#endif

    switch(defaultType) {
    case CharMap::eIdentityCharMap:
        {
            const IdentCharMap	 &identCharMap = charFreqMap.makeIdentityCharMap();

            mIdentTries = new WordTrie<IdentCharMap>*[maxLineLength+1];
            for (int wordLen = 1; wordLen <= maxLineLength; wordLen++)
                mIdentTries[wordLen] = new WordTrie<IdentCharMap>(identCharMap, wordLen);
            mNumWords += initFromSortedDictionaryFile(dictFile, identCharMap, mIdentTries, mWordMaps, mMaxWordLength);
            WordRectSearchMgr<IdentCharMap> *searchMgr = new WordRectSearchMgr<IdentCharMap>(mIdentTries, mWordMaps
                , mManagerFlags, mFinderOptions, mVerbosity);
            searchMgr->manageSearch(minArea, minTall, maxTall, maxArea, mMaxWordLength);
        }
        break;
    case CharMap::eCompactCharMap:
        {
            const CompactCharMap &compactCharMap = charFreqMap.makeCompactCharMap();
            assert(&defaultCharMap == &compactCharMap);
            mCompactTries = new WordTrie<CompactCharMap>*[maxLineLength+1];
            for (int wordLen = 1; wordLen <= maxLineLength; wordLen++)
                mCompactTries[wordLen] = new WordTrie<CompactCharMap>(compactCharMap, wordLen);
            mNumWords += initFromSortedDictionaryFile(dictFile, compactCharMap, mCompactTries, mWordMaps, mMaxWordLength);
            WordRectSearchMgr<CompactCharMap> *searchMgr = new WordRectSearchMgr<CompactCharMap>(mCompactTries, mWordMaps
                , mManagerFlags, mFinderOptions, mVerbosity);
            searchMgr->manageSearch(minArea, minTall, maxTall, maxArea, mMaxWordLength);
        }
        break;
    case CharMap::eFreqFirstCharMap:
        {
            const FreqFirstCharMap &freqFirstCharMap = charFreqMap.makeFreqFirstCharMap();
            assert(&defaultCharMap == &freqFirstCharMap);

            mFreqTries = new WordTrie<FreqFirstCharMap>*[maxLineLength+1];
            for (int wordLen = 1; wordLen <= maxLineLength; wordLen++)
                mFreqTries[wordLen] = new WordTrie<FreqFirstCharMap>(freqFirstCharMap, wordLen);
            mNumWords += initFromSortedDictionaryFile(dictFile, freqFirstCharMap, mFreqTries, mWordMaps, mMaxWordLength);
            WordRectSearchMgr<FreqFirstCharMap> *searchMgr = new WordRectSearchMgr<FreqFirstCharMap>(mFreqTries, mWordMaps
                , mManagerFlags, mFinderOptions, mVerbosity);
            searchMgr->manageSearch(minArea, minTall, maxTall, maxArea, mMaxWordLength);
        }
        break;	default:
        assert( ! "unhandled CharMap sub-type");
    }
#endif
    return 0;
}


int WordRectSearchExec::destroySearchManager()
{ 
    WordRectPrinter::stop();
    return 0; 
}


// Loads from the file only once; no re-loading after init.  Modifies rMaxWordLength.
template <typename MapT>
uint WordRectSearchExec::initFromSortedDictionaryFile(const char *fileSpec, const MapT &charMap
    , WordTrie<MapT, TrixNode> *wordTries[], WordMap *wordMaps, uint minWordLength, uint maxWordLength)
{   
    // This method allocates memory for holding all the words, and that memory is retained 
    // until the process terminates.  So it should succeed only once.
    static bool alreadyLoaded = false;
    if (alreadyLoaded) {
        return 0;
    }
    alreadyLoaded = true;

    FILE *fi;
    if ( ! fopen_safe(&fi, fileSpec, "r") ) {
        printf("%s opened by %s\n" , fileSpec, __FUNCTION__);
    } else {
        printf("Error opening dictionary file: %s\n", fileSpec);
        return 0;
    }

    TrixNode *prevNode[sMaxWordLength] = { NULL, };
    uint  length, numWords = 0;
    uint  endMapIdx = charMap.targetEndIdx();
    char  line[CharFreqMap::sBufSize];
    while (fgets(line, CharFreqMap::sBufSize, fi))  {
        // Got next NULL-terminated line from the dictionary text file.
        // Extract the first word as the sequence of all the chars before
        // the NULL or any other char <= sBegChar (default: the SPACE char).
        // Thus the first whitespace or control-char, including LF (line feed,
        // new line, \n), TAB, VT, FF, CR (carriage return), ESC, FS, and 
        // of course Space, will end the word and be replaced with NULL
        // if a copy of this string is to be saved as a word.
        // If the raw string contains any other non-mapped character
        // before the first word-terminator, it must be rejected, 
        // since the wordTrie cannot store it.  If we do extract a word,
        // we must also compute its length before saving it.

        register char *ptrSigned = line;            // pointer to signed char
        for ( ; ; ++ptrSigned) {
            register uchr valUnsigned = *ptrSigned; // value as unsigned char
            if (valUnsigned > CharFreqMap::sBegChar) {
                // reject any string with unmapped char
                uint idx = charMap.charToIndex(valUnsigned);
                if (idx >= endMapIdx) {
                    goto GET_NEXT_LINE;
                }
            } else {
                // The string up to here is a valid word.  
                // NULL-terminate it, then add it to the trie.
                *ptrSigned = NULL;
                break;
            }
        }

        // filter by word length
        length = uint(ptrSigned - line);   // pointer arithmetic
        if (length < minWordLength || maxWordLength < length)
            continue;

        numWords++;
        prevNode[length] = wordTries[length]->insertWord(line, prevNode[length]);
        if (wordMaps != NULL) {
            assert(length > 0);
            // Duplicate the input line up to the NULL.
            // This allocates this word's memory for the life of the program, i.e.,
            // it is free'd only on program exit, though some TrieNode could be
            // construed as its unique owner, and everyone else only a borrower -- but, which TrieNode??
            const char *saveWord = strdup_safe(line);		
            wordMaps[length].insert(WordMap::value_type(saveWord, numWords));   // strictly increasing value preserves the order
        }
GET_NEXT_LINE: continue;
    }
    fclose(fi);
    return numWords;
}





/*
C:\asd\_MSVC\wordRectMulti\Debug\wordRectMulti.exe: using tries,
min & max Area: 44 44, min & max Height: 4 4.
Opened WORD.LST.txt
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
