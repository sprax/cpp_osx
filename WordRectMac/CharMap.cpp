// CharMap.hpp 
// 
// Sprax Lines,  September 2012

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CharFreqMap.hpp"


#ifdef _DEBUG
#if USE_BASE_CHAR_MAP && USE_VIRT_CHAR_IDX	
// If this is 0, then using CharMap as a template argument will lead to slicing:
// default (base class) implementation of pure virtual method
// This is here only for analysis and debugging.
uint CharMap::charToIndex(uchr uc)  const
{
    assert( ! "Base class default impl of pure virt: should never be called!");
    return ((uint) -1) / 2;
}
#endif
#endif


IdentCharMap::IdentCharMap(const CharFreqMap& cfm) 
    : CharMap(cfm.getMinMappedChar(), cfm.getMaxMappedChar(), cfm.getMinMappedChar(), cfm.getMaxMappedChar() )
{ }

CompactCharMap::CompactCharMap(const CharFreqMap& cfm) 
    :  ArrayCharMap(0, cfm.getRangeCount(), cfm.getMinMappedChar(), cfm.getMaxMappedChar()) 
{
    ////mCharToIndexMem = (uint *)  malloc(mDomainSpread*sizeof(uint));
    mCharToIndexMem = new uint[cfm.getDomainSpread()];
    mCharToIndexPtr = mCharToIndexMem - cfm.getMinMappedChar();
    memset(mCharToIndexMem,  (uint)-1, cfm.getDomainSpread()*sizeof(uint));
    uint minCharCount = cfm.getMinCharCount();
    cfm.initTableNaturalOrder(mCharToIndexPtr, minCharCount);
}

FreqFirstCharMap::FreqFirstCharMap(const CharFreqMap& cfm, uint minCount) 
    : ArrayCharMap(0, cfm.getRangeCountOverMin(minCount), cfm.getMinMappedChar(), cfm.getMaxMappedChar())
{
    size_t domainSize = cfm.getDomainSpread();  // FIXME ? = cfm.getDomainSize();
    mCharToIndexMem = new uint[domainSize];
    mCharToIndexPtr = mCharToIndexMem - cfm.getMinFoundChar();
    memset(mCharToIndexMem,  (uint)-1, domainSize*sizeof(uint));
    cfm.initTableFrequencyOrder(mCharToIndexPtr, minCount);
}

FreqFirstWildCharMap::FreqFirstWildCharMap(const CharFreqMap& cfm, uint minCount) 
    : ArrayCharMap(0, cfm.getRangeCountOverMin(minCount), cfm.getMinMappedChar(), cfm.getMaxMappedChar())
{
    size_t domainSize = cfm.getDomainSize();
    mCharToIndexMem = new uint[domainSize];
    mCharToIndexPtr = mCharToIndexMem - cfm.sBegChar;
    memset(mCharToIndexMem, cfm.getRangeCount(), domainSize*sizeof(uint));
    cfm.initTableFrequencyOrder(mCharToIndexPtr, minCount);
}

