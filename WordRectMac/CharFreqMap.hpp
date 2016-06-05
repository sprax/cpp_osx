// CharFreqMap.hpp 
// 
// Sprax Lines,  September 2012

#ifndef CharFreqMap_hpp
#define CharFreqMap_hpp

#include "CharMap.hpp"

#include <vector>

struct CharFreq {
    CharFreq(uchr c, int n) : mFreq(n), mChar(c) {}
    uint    mFreq;
    uchr    mChar;    // Not const, so sort can be in-place
};

struct CharFreqGreater {
    bool operator()(CharFreq const& a, CharFreq const& b) const {
        return a.mFreq > b.mFreq;
    }
};

struct CharFreqLess {
    bool operator()(CharFreq const& a, CharFreq const& b) const {
        return a.mFreq < b.mFreq;
    }
};

/******************************************************************************
Obtains statistics on the characters in a text file (as in a dictionary file),
such as min and max chars, the counts for each char.  Intended to be used for
making a char-to-index mapping, where the range of output indices may be 
contiguous, or at least more compact than, the input domain.  
Once initialized, it can be used as a factory for create such maps.
******************************************************************************/
class CharFreqMap
{
public:
    static const uint sBufSize    = 128;    // Sufficient for longest word + line ending
    static const uchr sBegChar    = ' ';    // Not  32, which assumes ASCII(?)
    static const uchr sEndChar    = 255;	// Not 256, which would become 0.  This is an invalid char!  Valid chars are < sEndChar.
    static const uint sDomainSize = sEndChar - sBegChar + 1;

    CharFreqMap(const char *fileSpec, uint minWordLength, uint maxWordLength, uint minCharCount = 0);      // Constructor

    inline void setMinCharFraction(double fraction) { mMinCharCount = (uint)(fraction * mNumTotalReadChars); }
    inline uchr getMinMappedChar()			const { return  mMinMappedUchr; }
    inline uchr getMaxMappedChar()			const { return  mMaxMappedUchr; }
    inline uchr getMinFoundChar()			const { return  mMinFoundUchr; }
    inline uchr getMaxFoundChar()			const { return  mMaxFoundUchr; }
    inline uint getMinFoundWordLength()	    const { return  mMinFoundWordLength; }
    inline uint getMaxFoundWordLength()	    const { return  mMaxFoundWordLength; }
    inline uint getDomainSize()             const { return  sDomainSize; }
    inline uint getDomainSpread()			const { return  mDomainSpread; }
    inline uint getRangeCount()				const { return  mRangeCount; }
    inline uint getMinCharCount()			const { return  mMinCharCount; }    // Set only in initRangeFromMinCharCount; otherwise read-only.

    const  CharFreq& getCharFreqPair(uint idx)  const {
        assert(idx < sEndChar);
        return mCharFreqPairs[idx];
    }


    uint getRangeCountOverMin(uint minCount) const;

    int    initFromFile(uint minCharCount, uint minWordLength, uint maxWordLength, int verbosity);
    CharMap::SubType getDefaultCharMapSubType()	const { return  mDefaultSubType; }

    // Factory methods (and helpers) for making a CharMap
    const CharMap			& makeDefaultCharMap();
    const IdentCharMap		& makeIdentityCharMap();
    const CompactCharMap	& makeCompactCharMap();
    const FreqFirstCharMap	& makeFreqFirstCharMap(uint minCharCount=0);

    /** Use the natural character order (ascending bits), but compacted. */
    void initTableNaturalOrder(uint *charToIndexPtr, uint minCharCount)		const;

    /** Order the uchr-to-index table by descending character frequencies. 
    *  The most frequent uchr gets index 0, second-most gets 1, etc.   
    **/
    void initTableFrequencyOrder(uint *freqCharToIndexPtr, uint minCharCount)	const;

    /** Count the chars in the first word in each line of a text file.
    *  Count only chars > mMinChar (default minimum uchr == Space),
    *  and only in words in the specified range of word lengths.
    *  Return the maximum length of any word found, even if greater 
    *  than the maxWordLength argument.
    */	
    static int countRawCharFreqs(const char *fileSpec, uint charCounts[]
    , const uint minWordLength,  const uint maxWordLength
        , uint& rMinFoundWordLength, uint& rMaxFoundWordLength);

protected:

    uint countWordCharsInFile(const char *fname, uint minCharCount, uint minWordLength, uint maxWordLength, int verbosity);
    uint initCharFreqPairs();
    uint initRangeFromMinCharCount(uint minCharCount, int verbosity);
    int  decideDefaultCharMapType();

    /** NULL-terminate the input strong at its first non-word char and return its length. */
    inline static uint extractFirstWord(char *line);

private:

    const char				*mFileSpec;
    const CharMap			*mDefaultCharMap;          // Recommended char map
    const IdentCharMap		*mIdentCharMap;
    const CompactCharMap	*mCompactCharMap;
    const FreqFirstCharMap	*mFreqFirstCharMap;

    CharMap::SubType		 mDefaultSubType;

    uint *mCharCounts;          // Raw counts of characters read
    uchr  mMinFoundUchr;        // Smallest unsigned char actually found in the source text (e.g. dictionary)
    uchr  mMaxFoundUchr;        // Largest unsigned char actually found in the source text
    uchr  mMinMappedUchr;       // Smallest unsigned char mapped into output range
    uchr  mMaxMappedUchr;       // Largest unsigned char mapped into output range
    uint  mMinCharCount;        // Minimum count for char to be mapped uniquely.  Less freq chars may map to a wildcard.  Read-only.
    uint  mDomainSpread;        // Number of chars in the actually occurring domain interval == maxChar - minChar + 1
    uint  mRangeCount;          // Number of chars in the output range; same as the domain spread if there are no holes or aliasing
    uint  mMinWordLength;       // Any word of length less than this will be ignored, its chars not counted.
    uint  mMaxWordLength;       // Any word of length more than this will be ignored, its chars not counted.
    uint  mMinFoundWordLength;  // Maximum length of any words found, even greater than mMaxWordLength (the nominal working limit)
    uint  mMaxFoundWordLength;  // Maximum length of any words found, even greater than mMaxWordLength (the nominal working limit)
    uint  mNumDistinctChars;    // Number of different chars read, not necessarily mapped
    uint  mNumTotalReadChars;   // Total number of characters read
    uint  mCharCountsMem[sDomainSize];   // Raw counts of characters read

    std::vector<CharFreq> mCharFreqPairs;

};

#endif // CharFreqMap_hpp