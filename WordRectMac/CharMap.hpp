// CharMap.hpp 
// 
// Sprax Lines,  September 2012

#ifndef CharMap_hpp
#define CharMap_hpp

#include "wordTypes.h"
class CharFreqMap;		// forward declaration

// Summary: If you use the base type CharMap as a template argument, you'd best declare charIndex
// as a pure virtual method there.  Otherwise, don't declare it at all in the base class, and only 
// use concrete (in fact, "final") sub-classes as template arguments.  But... If you insist on both
// using CharMap as a template argument and overriding a non-virtual charIndex method, then you will
// need to keep track of your actual CharMap sub-types to use them as explicit template arguments, 
// and cast your CharMaps and CharMap-template objects accordingly.  Don't do it!  
#define USE_BASE_CHAR_MAP 1
#define USE_VIRT_CHAR_IDX 1	// If this is 0, then using CharMap as a template argument will lead to slicing:
							// In particular, methods overriding charIndex will be lost in compilation.
/**
*  CharMap
*  Base class for mapping characters to indices.  
*  The constructor is protected so only sub-classes 
*  can be instantiated.
**/
class CharMap 
{
public: 
	// Enumerate known sub-types to be used as template arguments
	typedef enum { 
		eUnknownSubType	=  0,	// Unknown sub-type == declaration as base type
		eIdentityCharMap,		// Uses unsigned chars themselves as indexes
		eCompactCharMap,		// Uses an array to to map unsigned chars to contiguous range of indices
		eCompactFreqFirst,		// Uses an array to to map unsigned chars to contiguous range of indices
		eWildCardFreqFirst,		// Uses an array to to map unsigned chars to contiguous range of indices
		eEnglishRoman52Map,
		eEnglishLower26Map,
		eEnglishUpper26Map,
	} SubType;

    static const char *getSubTypeName(SubType st) {
        switch(st) {
        case eUnknownSubType:       return "Unknown";
        case eIdentityCharMap:      return "Identity";
        case eCompactCharMap:       return "CompactArray";
		case eCompactFreqFirst:		return "CompactMostFrequentFirst";
		case eWildCardFreqFirst:	return "WildCardMostFrequentFirst";
        case eEnglishRoman52Map:    return "EnglishRoman52";
        case eEnglishLower26Map:    return "EnglishLower26";
        case eEnglishUpper26Map:    return "EnglishUpper26";
        default:                    return "ErrorUnknown";
        }
    }

#if USE_BASE_CHAR_MAP
	// Sub-classes must override this virtual method, preferably with an in-line.
    // When called through a concrete subclass that is known at compile time, 
    // the overriding method will be called directly, not via a virtual function
    // table, and thus it may be in-lined and effectively optimized away.
#if	USE_VIRT_CHAR_IDX
    virtual uint charToIndex(uchr uc)  const = 0;
#else
	uint         charToIndex(uchr   )  const { return (uint)-1; };	// Sub-classes can override this non-virtual method, but the override is susceptible to slicing
#endif	// USE_VIRT_CHAR_IDX
#endif	// USE_BASE_CHAR_MAP

	inline uint targetSize()        const { return mTargetEndIdx - mTargetBegIdx;  }
	inline uint targetBegIdx()      const { return mTargetBegIdx;   }
    inline uint targetEndIdx()      const { return mTargetEndIdx;   }
    inline uchr sourceMinChar()     const { return mSourceMinChar;  }
    inline uchr sourceMaxChar()     const { return mSourceMaxChar;  }


protected:
	CharMap(uint begIdx, uint endIdx, uchr minChar, uchr maxChar)	
        : mTargetBegIdx(begIdx), mTargetEndIdx(endIdx)
        , mSourceMinChar(minChar), mSourceMaxChar(maxChar)
    { }
    CharMap(CharMap &);                 // Prevent pass-by-value by not defining copy constructor.
    CharMap& operator=(const CharMap&); // Prevent assignment by not defining this operator.

private:
    // These are all const because they are known at construction time and cannot change.
    // Dependent classes such as WordTrie can fail if these numbers change,
    // and a TrieNode may use the value mTargetOffset in both its constructor and destructor (unless it is known to be 0).
////const uint  mTargetSize;	// Size of the character-indexed output range.
    const uint  mTargetBegIdx;	// Index of first entry == Distance from mBranches to &mBranches[charIndex(mMinChar)]
    const uint  mTargetEndIdx;	// Index of last entry + 1 == Distance from mBranches to &mBranches[charIndex(mMinChar)]
    const uchr  mSourceMinChar;
    const uchr  mSourceMaxChar;
};

/** 
*  This implementation uses the character itself as the index into 
*  an array of trie branches or whatever.  Effectively this is nothing 
*  but a cast from the character type to the index type.
*  Use this for (mostly) contiguous blocks of characters.
*  The cost in a release build should be zero, 
*  optimized out by the compiler.
*/
class IdentCharMap : public CharMap
{
public:
	IdentCharMap(uint begIdx, uint endIdx, uchr minChar, uchr maxChar) 
        : CharMap(begIdx, endIdx, minChar, maxChar) { }
    IdentCharMap(const CharFreqMap& charFreqMap);
    inline uint charToIndex(uchr uc)  const { return uc; };
};


/**
*  Base class that uses an array as an intermediate look-up table to map 
*  characters to indices.  The constructor, destructor, and data members 
*  are all protected.  Only sub-classes can be instantiated to create an
*  actual table and use it for mapping unsigned chars to indices.
**/
class ArrayCharMap: public CharMap
{
public:
	uint	charToIndex(uchr uc)  const { return mCharToIndexPtr[uc]; };

protected:
	ArrayCharMap(uint begIdx, uint endiIdx, uchr minChar, uchr maxChar) 
		:  CharMap(begIdx, endiIdx, minChar, maxChar) 
	{ }
	virtual ~ArrayCharMap()	
	{ delete mCharToIndexMem; }

	uint   *mCharToIndexMem;
	uint   *mCharToIndexPtr;
};

/**
*  This implementation uses an array as an intermediate look-up table to map 
*  characters to indices 1-1 and in their original order, but in the compacted
*  range [0, numDistinctChars).  That is, the output indices preserve the 
*  original or "natural" order of the the input characters, but the range 
*  covers only the number of distinct characters actually found in the 
*  source text (such as a word list).
*
*  Use this when the input range is somewhat larger than the 
*  output range.  Effectively, this compacts a wide range of input chars to 
*  a narrower range of output indices, thus saving space in the target table 
*  which is indexed by this intermediate table's output indices.  The cost 
*  is the space used by this array plus one level of indirection for every
*  look-up.
**/
class CompactCharMap: public ArrayCharMap
{
public:
    CompactCharMap(const CharFreqMap& charFreqMap);
};

/**
*  This implementation uses an array as an intermediate look-up table to 
*  map characters to indices 1-1 in the order of descending frequency. 
*  In particular, the output indices do not preserve the original or 
*  "natural" order of the the input characters as numerical values.
*
*  If a non-zero minimal threshold is applied, than any characters whose counts 
*  fall below that threshold are all be mapped to the greatest index in the 
*  output range, which equals the number of all characters mapped 1-1.
*  That is, each char with count > the threshold is mapped to a unique index
*  in the half-open interval [0, mRangeCount), and and any others are mapped
*  non-uniquely to index == mRangeCount.  Thus index == mRangeCount
*  effectively represents a wild card character that replaces all of the 
*  least frequently occurring characters.  Strings stored using such a
*  non-unique representation may of course have aliases, and are thus
*  not uniquely retrievable.  In practice, it may be easiest to exclude 
*  them from tries, etc.
*
*  Use this when the input range is much larger than the 
*  output range.  Effectively, this compacts a wide range of input chars to 
*  a narrower range of output indices, thus saving space in the target table 
*  which is indexed by this intermediate table's output indices.  The cost 
*  is the space used by this array plus one level of indirection for every
*  look-up.
**/
class FreqFirstCharMap: public ArrayCharMap
{
public:
	FreqFirstCharMap(const CharFreqMap& charFreqMap, uint minCount=0);
	static SubType subType()	{ return eCompactFreqFirst; }
};

class FreqFirstWildCharMap: public ArrayCharMap
{
public:
	FreqFirstWildCharMap(const CharFreqMap& charFreqMap, uint minCount=0);
	static SubType subType()	{ return eWildCardFreqFirst; }
};

#endif // CharMap_hpp
