// CharFreqMap.cpp : trie nodes for word tries, 

#include "CharFreqMap.hpp"
#include "wordPlatform.h"

#include <algorithm>
#include <functional>      // For greater<int>()
#include <iostream>

// Constructor
CharFreqMap::CharFreqMap(const char *fileSpec, uint minWordLength, uint maxWordLength, uint minCharCount)
    : mFileSpec(fileSpec), mMinWordLength(minWordLength), mMaxWordLength(maxWordLength), mMinCharCount(minCharCount)
    , mDefaultCharMap(NULL), mIdentCharMap(NULL), mCompactCharMap(NULL)
    , mFreqFirstCharMap(NULL), mMinFoundUchr(sEndChar), mMaxFoundUchr(0), mMinMappedUchr(sEndChar), mMaxMappedUchr(0)
    , mDomainSpread(0), mRangeCount(0), mMinFoundWordLength(INT_MAX), mMaxFoundWordLength(0)
    , mNumDistinctChars(0), mNumTotalReadChars(0)
{
    memset(mCharCountsMem, 0, sDomainSize*sizeof(int));
    mCharCounts = mCharCountsMem - sBegChar;
}

int CharFreqMap::initFromFile(uint minCharCount, uint minWordLength, uint maxWordLength, int verbosity)
{
    int rangeCount = countWordCharsInFile(mFileSpec, minCharCount, minWordLength, maxWordLength, verbosity);
    if (rangeCount < 1) {
        printf("WARNING: No words or chars read from file <%s>\n", mFileSpec);
        return -1;
    }
    return decideDefaultCharMapType();
}

uint CharFreqMap::countWordCharsInFile(const char *fileSpec, uint minCharCount, uint minWordLength, uint maxWordLength, int verbosity)
{
    uint numWordsCounted = countRawCharFreqs(fileSpec, mCharCounts, minWordLength, maxWordLength, mMinFoundWordLength, mMaxFoundWordLength);
    if (numWordsCounted < 100)  // TODO: arbitrary number
        return (uint) -1;
    if (mMaxFoundWordLength < minWordLength)
        return (uint) -2;
    initCharFreqPairs();
    return initRangeFromMinCharCount(minCharCount, verbosity);
}


/** Count the chars in the first word in each line of a text file.
*  Count only chars > mMinChar (default minimum uchr == Space),
*  and only in words in the specified range of word lengths.
*  Sets the found minimum and maximum lengths of words found, 
*  even if lesser/greater than the min/maxWordLength arguments.
*/	

int CharFreqMap::countRawCharFreqs(const char *fileSpec, uint charCounts[]
, const uint minWordLength,  const uint maxWordLength
    , uint& rMinFoundWordLength, uint& rMaxFoundWordLength)
{
    FILE *fi;
    uint numWordsCharCounted = 0;
    rMinFoundWordLength = INT_MAX;
    rMaxFoundWordLength = 0;
    errno_t err = fopen_safe(&fi, fileSpec, "r");
    if (0 == err) {
        printf("%s opened by %s\n" , fileSpec, __FUNCTION__);
    } else {
        printf("Error opening file: %s\n", fileSpec);
        return -1;
    }
    char line[sBufSize];
    while (fgets(line, sBufSize, fi))  {
        // Got the next line in the file; now get its length, and remove any trailing newline.
        uint length = extractFirstWord(line);
        assert(length < sBufSize - 2);          // buffer was big enough for word + line ending
        if (rMinFoundWordLength > length)
            rMinFoundWordLength = length;
        if (rMaxFoundWordLength < length)
            rMaxFoundWordLength = length;

        if (length < minWordLength || maxWordLength < length)
            continue;

        ++numWordsCharCounted;
        for (uint j = 0; j < length; j++) {
            uchr uch = line[j];
            ++charCounts[uch];
        }
    }
    fclose(fi);
    return numWordsCharCounted;
}

uint CharFreqMap::initCharFreqPairs()
{
    mNumTotalReadChars = 0;
    for (uint j = sBegChar; j <= sEndChar; j++) {
        uint count = mCharCounts[j];
        if (count > 0) {
            mNumDistinctChars  += 1;
            mNumTotalReadChars += count;
            uchr letter = (uchr) j;
            mCharFreqPairs.push_back(CharFreq(letter, count));
            if (mMinFoundUchr > letter) {			// Must check min before max
                mMinFoundUchr = letter;             // because letter only increases
            } else if (mMaxFoundUchr < letter) {	// inside this loop!
                mMaxFoundUchr = letter;
            }
        }
    }
    // Set the domain spread once; this is the first and last place to set it.
    assert(mMaxFoundUchr < sEndChar);                       // TODO: Make this a real warning (or error), not just an assert?  addLineWordsToTrie assumes char 255 is a non-word char  
    mDomainSpread = mMaxFoundUchr - mMinFoundUchr + 1;      // Never changes.
    sort(mCharFreqPairs.begin(), mCharFreqPairs.end(), CharFreqGreater() );
    return mNumDistinctChars;
}


uint CharFreqMap::initRangeFromMinCharCount(uint minCharCount, int verbosity)
{
    // Reset thresholds and counters, in case minCharCount is different from before.
    mMinCharCount  = minCharCount;
    mRangeCount    = 0;
    mMinMappedUchr = sEndChar;
    mMaxMappedUchr = 0;
    for (uint j = sBegChar; j <= sEndChar; j++) {
        uint count = mCharCounts[j];
        if (count >= mMinCharCount) {
            mRangeCount++;
            uchr letter = (uchr) j;
            if (mMinMappedUchr > letter) {			  // Must check min before max
                mMinMappedUchr = letter;              // because letter only increases
            } else if (mMaxMappedUchr < letter) {     // inside this loop!
                mMaxMappedUchr = letter;
            }
        }
    }
    printf("Found %d distinct chars, keeping rangeCount=%d of them.\n", mNumDistinctChars, mRangeCount);
    if (verbosity > 0) {
        bool cutoffPrinted = false;
        printf("index\tchar\tcount:\n");
        uint idx = 0;
        for (std::vector<CharFreq>::const_iterator it = mCharFreqPairs.begin(); it != mCharFreqPairs.end() ; ++it, ++idx) {
            if (! cutoffPrinted && it->mFreq < minCharCount) {
                printf("------- Cut-off for minCharCount == %d -------\n", minCharCount);
                if (verbosity < 2) {
                    break;
                }
                cutoffPrinted = true;
            }
            printf("%3d\t%c\t%d\n", idx, it->mChar, it->mFreq);
        }
    }

    return mRangeCount;
}


/** 
*  Extract the word at the start of the input string by finding the first 
*  non-word character and NULL-terminating it there.
*  Return the length of this null-terminated word string. 
*/
uint CharFreqMap::extractFirstWord(char *line)
{
    assert(line != NULL);
    register char *ptrSigned = line;            // pointer to signed char
    for ( ; ; ++ptrSigned) {
        register uchr valUnsigned = *ptrSigned; // value as unsigned char
        if (valUnsigned <= sBegChar) {
            *ptrSigned = NULL;
            break;
        }
    }
    uint   length = uint(ptrSigned - line);   // pointer arithmetic
    return length;
}

int CharFreqMap::decideDefaultCharMapType()
{
    int numHoles = mDomainSpread - mRangeCount;
    double foundRatio = (double) numHoles / mRangeCount;
    double asciiRatio = (double) 6.0 / 52.0;
    // If the percentage of holes is less than 1 percent worse than ASCII,
    // then index the trie branches using raw char values.  That is, just
    // shift the chars to map mMinChar to index 0.  Otherwise, transform 
    // the char values into contiguous indices, using some form of look-up 
    // table or some simple function.
    if (foundRatio > asciiRatio + 0.01) {   // TODO: magic number
        mDefaultSubType = CharMap::eCompactFreqFirst;
    } else if (numHoles > 2) {              // TODO: magic number
        mDefaultSubType = CharMap::eCompactCharMap;
    } else {
        mDefaultSubType = CharMap::eIdentityCharMap;
    }
    return 0;
}

const CharMap	& CharFreqMap::makeDefaultCharMap()	
{
    switch (mDefaultSubType) {
    case CharMap::eCompactFreqFirst:
        return makeFreqFirstCharMap();
    case CharMap::eCompactCharMap:
        return makeCompactCharMap();
    case CharMap::eIdentityCharMap:
        return makeIdentityCharMap();
    default:
        printf("WARNING: Default subtype of CharMap is not set.  Using IdentCharMap.\n");
        return makeIdentityCharMap();
    }
}

const IdentCharMap& CharFreqMap::makeIdentityCharMap()	
{
    if (mIdentCharMap == NULL)
        mIdentCharMap =  new IdentCharMap(*this);
    return *mIdentCharMap;
}

const CompactCharMap& CharFreqMap::makeCompactCharMap()	
{
    if (mCompactCharMap == NULL) {
        mCompactCharMap =  new CompactCharMap(*this);
    }
    return *mCompactCharMap;
}

const FreqFirstCharMap& CharFreqMap::makeFreqFirstCharMap(uint minCharCount)	
{
    if (mFreqFirstCharMap == NULL) {
        mFreqFirstCharMap =  new FreqFirstCharMap(*this, minCharCount);
    }
    return *mFreqFirstCharMap;
}

uint CharFreqMap::getRangeCountOverMin(uint minCount) const
{
    assert(mRangeCount > 0);	// Counts must already be initialized for const
    if (minCount <= mMinCharCount)
        return mRangeCount;

    int rangeCount = 0;

#if 1	// TODO: use vector's bounds or binary search
    for (std::vector<CharFreq>::const_iterator it = mCharFreqPairs.begin(); it != mCharFreqPairs.end() ; ++it) {
        if (it->mFreq < minCount)
            break;
        ++rangeCount;
    }
#else
    for (uint j = mMinUchr; j < mDomainSpread; j++) {
        uint count = mCharCounts[j];
        if (count > minCount) {
            ++rangeCount;
        }
    }
#endif
    return rangeCount;
}

/** Use the natural character order (ascending bits), but compacted. */
void CharFreqMap::initTableNaturalOrder(uint *charToIndexPtr, uint minCharCount)	const
{
    char ch = 0;
    uchr uc = 0;
    uint idx = 0;
    for (uint j = sBegChar; j <= sEndChar; j++) {
        uint count = mCharCounts[j];
        if (count >= minCharCount) {
            ch = (  signed char) j;
            uc = (unsigned char) j;
            charToIndexPtr[j] = idx;
            printf("%3d  %3d  %c  %c  %d\n", idx, j, ch, uc, mCharCounts[j]);
            ++idx;
        }
    }
}

/** Order the char-to-index table by descending char frequencies. 
*  The most frequent char gets index 0, second-most gets 1, etc.   
**/
void CharFreqMap::initTableFrequencyOrder(uint *freqCharToIndexPtr, uint minCharCount)	const
{
    uint idx = 0;
    for (std::vector<CharFreq>::const_iterator it = mCharFreqPairs.begin(); it != mCharFreqPairs.end() ; ++it) {
        if (it->mFreq < minCharCount)
            break;
        freqCharToIndexPtr[it->mChar] = idx++;
    }
}
