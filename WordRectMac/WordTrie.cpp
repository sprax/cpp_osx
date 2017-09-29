// WordTrie.cpp : word prefix tree with forward links (optimized for speed at the expense of memory)
// Sprax Lines, July 2010

#include "WordTrie.hpp"
#include <unistd.h>

/**
 * Extracts "words" -- that is, word-boundary delimited strings --
 * from a null-terminated char array (c-string).  These "words"
 * are not checked against any dictionary or rules, other than being
 * delimited by the beginning or end of the array or by non-word-forming
 * characters, such as whitespace or punctuation.
 */

//private:
/** Is an unsigned char that is greater than Space a punctuation character
 *  for purposes of word boundary detection? */
static inline bool isUchrGtSpacePunct(uchr uc) { return ('9' < uc && uc < 'A'); }

//  WordTrie Parse States

typedef enum { 
    eInsideWord         = 0,
    eWhitespace         = 1,
    eUnmappedChar       = 2,
    // eUnmappedPunct   = 3
} ParseState;

template <typename MapT, typename NodeT>
int WordTrie<MapT, NodeT>::addLineWordsToTrie(WordTrie<MapT, NodeT> *trie, char *line, const uint minWordLength, const uint maxWordLength)
{
    int numWordsAdded = 0;
    assert(line);
    const CharMap& charMap = trie->charMap();
    ParseState chrState = eWhitespace;        
    char *pBegWord;
    uint  endMapIdx = charMap.targetEndIdx();
    for (register char *ptrSigned = line; ; ++ptrSigned) {
        
        register uchr valUnsigned = *ptrSigned; // value as unsigned char
        
        switch(chrState) {
                
            case eInsideWord:
                // Inside a sub-string that may be a word
                // Find whitespace, punctuation, or the end of the line to end this word.
                // or find an unmapped char to invalidate this word.
                // Otherwise, stay in this state.
                
                if (valUnsigned <= CharFreqMap::sBegChar) {
                    // Up to here, this sub-string's chars are all mapped, so if it is long 
                    // enough, NULL-terminate it and add it to the trie as a valid word.  
                    uint wordLength = (uint)(ptrSigned - pBegWord);
                    if (minWordLength <= wordLength && wordLength <= maxWordLength) {
                        *ptrSigned = '\0';
                        if (trie->insertWord(pBegWord, NULL)) {
                            numWordsAdded++;
                        }
                    }
                    if (valUnsigned == 0) {
                        return numWordsAdded;   // Word end was also the line end, so don't set state, just return.
                    } else {
                        chrState = eWhitespace;         // Word end was whitespace, so set state to whitespace and continue
                    }
                } else {
                    // Terminate on punctuation; otherwise, reject any string with unmapped char
                    uint idx = charMap.charToIndex(valUnsigned);
                    if (idx >= endMapIdx) {
                        chrState = eUnmappedChar;
                        // If non-whitespace, unmapped char is punctuation, then end the word instead of invalidating it,
                        // but leave the state as invalid, so that actual white space must be found before the next word.
                        // But if any punctuation chars are mapped, they can appear inside words, e.g. "hyphen-mark".
                        if (isUchrGtSpacePunct(valUnsigned)) {
                            uint wordLength = uint(ptrSigned - pBegWord);
                            if (minWordLength <= wordLength && wordLength <= maxWordLength) {
                                *ptrSigned = '\0';
                                if (trie->insertWord(pBegWord, NULL)) {
                                    numWordsAdded++;
                                }
                            }
                        }
                    }
                }
                break;
                
            case eWhitespace:   // If current state is whiteSpace, find a letter to change the state.
                
                if (valUnsigned > CharFreqMap::sBegChar) {
                    uint idx = charMap.charToIndex(valUnsigned);
                    if (idx >= endMapIdx) {
                        if (isUchrGtSpacePunct(valUnsigned)) {
                            // If this unmapped char is punctuation after whitespace; ignore it
                            break;                           // Let the state remain whitespace.
                        } else {
                            // reject strings with any other unmapped char
                            chrState = eUnmappedChar;   // Set the state to invalid word.
                        }
                    } else {
                        pBegWord = ptrSigned;
                        chrState = eInsideWord;
                    }
                } else if (valUnsigned == 0) {      // End of line, not in a word, so return
                    return numWordsAdded;
                }
                break;
                
            case eUnmappedChar:    // Find whitespace or end of line to get out of this state 
                if (valUnsigned <= CharFreqMap::sBegChar) {
                    if (valUnsigned == 0) {         // End of line, not in a word, so just return
                        break;
                    } else {
                        chrState = eWhitespace;             // whitespace
                    }
                }
                break;
                
        }
    }
    return numWordsAdded;
}

/** 
 * Supplement WordTrie with words from a text file.
 * 
 * @param minWordLen
 * @param maxWordLen
 * @param textFilePath
 * @return number of new words added.  Words already in the Trie, or outside the
 * specified length range don't count.
 */


template <typename MapT, typename NodeT>
int WordTrie<MapT, NodeT>::addAllWordsInTextFile(const char *fileSpec, WordTrie<MapT, NodeT> *trie, uint minWordLength, uint maxWordLength, int verbosity)
{   
    uint numLineWordsAdded, numWordsAdded = 0;
    
    printf("============ Working Directory: %s\n", getcwd(NULL, 0));
    
    FILE *fi;
    if ( ! fopen_safe(&fi, fileSpec, "r") ) {
        printf("%s opened by %s\n" , fileSpec, __FUNCTION__);
    } else {
        printf("Error opening dictionary file: %s\n", fileSpec);
        return 0;
    }
    
    uint  numLinesRead = 0;
    char  line[CharFreqMap::sBufSize];
    while (fgets(line, CharFreqMap::sBufSize, fi))  {        
        numLinesRead++;
        numLineWordsAdded = addLineWordsToTrie(trie, line, minWordLength, maxWordLength);
        numWordsAdded += numLineWordsAdded;
    }
    
    if (verbosity > 1)
        printf("Read %d lines, added %d new words from %s\n", numLinesRead, numWordsAdded, fileSpec);
    fclose(fi);
    return numWordsAdded;  // return the number of words read, not necessarily kept.
}


/*
 //protected:
 template <typename MapT, typename NodeT>
 int WordTrie<MapT, NodeT>::test_addAllWordsInTextFile(WordTrie trie, int minWordLen, int maxWordLen, final String fileName, int verbosity) 
 {
 trie.mNewWordNodes.clear();
 int numNewWords = trie.addAllWordsInTextFile(minWordLen, maxWordLen, fileName, verbosity);
 if (numNewWords > 0 && verbosity > 1) {
 trie.showNewWords(6, trie.mActMaxWordLen);
 }
 if (verbosity > 0) {    
 trie.showStats(fileName);
 
 if (verbosity > 1) {    
 int      maxFreq = 0, maxLen = 0, minLen = 9999;
 WordNode frqNode = null, lnNode = null, shNode = null;
 for (WordNode node : trie.mNewWordNodes.values()) {
 if (maxFreq < node.mTextCount) {
 maxFreq = node.mTextCount;
 frqNode = node;
 }
 if (maxLen < node.mDepth) {
 maxLen = node.mDepth;
 lnNode = node;
 }   
 if (minLen > node.mDepth) {
 minLen = node.mDepth;
 shNode = node;
 }   
 }
 if (frqNode != null) {
 Sx.puts("Most frequent new word: " + frqNode.mStem + "  " + maxFreq);
 Sx.puts("      Longest new word: " + lnNode.mStem + "  " + maxLen);
 Sx.puts("     Shortest new word: " + shNode.mStem + "  " + minLen);
 }
 }
 }
 return numNewWords;
 }
 */



