// WordTrie.hpp : word prefix tree with forward links (optimized for speed at the expense of memory)
// Sprax Lines, July 2010

#ifndef WordTrie_hpp
#define WordTrie_hpp

#include <vector>

#include "CharFreqMap.hpp"
#include "TrixNode.hpp"
#include "wordPlatform.h"

template <typename MapT, typename NodeT>
class WordTrie 
{
public:

    WordTrie(const MapT &charMap, uint wordLen) : mCharMap(charMap), mWordLength(wordLen), mTotalNodes(0)
    {
        mRoot = new NodeT(mCharMap, 0);
    }
    WordTrie(WordTrie &);                               // Prevent pass-by-value by not defining copy constructor.
    WordTrie<MapT, NodeT>& operator=(const WordTrie&);  // Prevent assignment by not defining this operator.
    ~WordTrie() { }                                     // Destroy all the nodes?	TODO: depth-first infanticide...

    inline uint             charIndex(uchr uc)        const { return mCharMap.charToIndex(uc); }
    inline const MapT     & charMap()                 const { return mCharMap; }
    inline const NodeT    * getRoot()                 const { return mRoot; }
    inline       int        getWordLength()           const { return mWordLength; }
    inline const NodeT    * getFirstWordNode()        const { return mRoot->getFirstWordNode(); }
    inline const NodeT    * getFirstWordNodeFromIndex(int ix)   const { return mRoot->getFirstWordNodeFromIndex(ix); }
    inline bool             hasWord(const char *key)  const { return containsWord(key) != NULL; }
    void printWordsAll()                              const { mRoot->printWordsAll(); }

    NodeT * insertWord(const char *word, NodeT *prevWordNode)
    {
        // Traverse down the trie until we reach the end of the word, creating nodes as necessary.
        // When we get to the word's end, create forward links from previously created nodes to the
        // longest stem of this word that is new.  Optimizing the prefix tree with such links greatly
        // speeds up finding word rectangles or what not.  No common ancestor algorithm necessary; 
        // just traverse (i.e. follow the added links).
        assert(word != NULL);
#ifdef _DEBUG
        if (prevWordNode != NULL) {
            assert(prevWordNode->mStem);
            uint prev0 = prevWordNode->getStem()[0];
            uint word0 = word[0];
            assert(prev0 <= word0);
        }
#endif
        NodeT *newNode = NULL;      // This will be returned, NULL or new.
        const char *newWord = NULL;
        NodeT *node = mRoot;
        for (const char *pc = word; ; ) {
            uchr uc = *pc++;
            assert(mCharMap.sourceMinChar() <= uc && uc <= mCharMap.sourceMaxChar());
            uint ix = charIndex(uc);
            assert(ix < mCharMap.targetEndIdx());
            if (node->mBranches[ix] == NULL) {
                // create new branch node as a child the current one.  Some nodes types keep a pointer
                // to their parent, and some may not even look at it, but we supply it in any case.
                newNode = new NodeT(mCharMap, node->mDepth + 1, node);
                mTotalNodes++;

                // Since we had to create a new node, we know this word is new.  Save it by duplicating it up to the NULL.
                // This allocates this word's memory for the life of the program, i.e.,
                // it is free'd only on program exit.  We construe this TrieNode to be
                // its unique owner, but what about the other nodes whose mStem will point to it?
                // Other candidates to be its owner are its actual word node, or the trie, or the *SearchExec,
                // or the program as a whole (as it is now).                
                if (newWord == NULL)
                    newWord = strdup_safe(word);
                // Set mStem as a pointer to the first word off this stem.  
                // If this node is a word-node, this stem is the node's word.
                newNode->mStem =  newWord;
                // Add new node to linked list using iteration (we save memory by not storing a lastBranch)
                if (node->mFirstBranch == NULL) {
                    node->mFirstBranch = newNode;    // This depends on alphabetical ordering!
                } else for (int ib = ix; --ib >= 0; ) {
                    if (node->mBranches[ib] != NULL) {
                        node->mBranches[ib]->mNextBranch = newNode;
                        break;
                    }
                }
                node->mBranches[ix] = newNode;
            }
            node = (NodeT *)node->mBranches[ix];

            if (*pc == '\0') {			  // End of the word.
                assert(mWordLength == 0 || pc - word == mWordLength);
                // We have reached the end of a word, so finish up this node
                // as a word node and set some "shortcut" pointers link it
                // or its antecedents to other nodes.  NB: These shortcuts
                // may depend on the words being added in a canonical order,
                // as in strictly sorted order.
                // First set this word node's word:
                // Don't _strdup or delete this word; its memory is owned 
                // by the caller (wordRectSearchMgr) & shared by others.
                ////node->mWord = word;       
                // For all ancestors not already pointing to an earlier word, set this node as the first completion of their stem:
                node->readAsDictWord(prevWordNode);     // Call-back to increment this word's text count or whatever.  We don't need to know.
                return newNode;             // If this word is new, newNode will point to its new word-node.  If not, newNode will be NULL.
            } else {
                node->readAsDictStem();     // Call-back to increment the number of words stemmed through this node or whatever.  We don't need to know.
            }
        }
        //  return NULL;				// unreachable code: either we return out of the loop or it or goes forever.
    }


    const NodeT * containsWord(const char *key) const
    {
        // Traverse down the trie until we get to the end of the string:
        const NodeT *node = mRoot;
        while (*key != '\0') {
            uchr ucx = *key++;
            uint uix = mCharMap.charIndex(ucx);
            node = node->mBranches[uix];
            if (node == NULL) {
                return  NULL;	// Not found - reached end of branch
            }
        }
        return node->getWord();
    }



    const NodeT* getNextWordNodeAndIndex(const NodeT *keyNode, const char *keyWord, int & fromBeg, int fromEnd) const // fromBeg can change
    {
        // We could memoize lazily using nextWordNode.  That is, either change this 
        // to start with the offending node and just set/get its nextWordNode (if the 
        // disqualified node is passed in!), or else replace the word member with a pointer to basically
        // an array of nextWordNodes for each length, so we just set/get/return nextWordNodes[fromBeg],
        // where fromBeg is rationalized to be in [0, wordLength).  But if we already have the offending
        // node, so we don't have to traverse (as below) to find it (or rather, its parent), then getting 
        // the next different stem is so simple that we don't really need to make a separate method for it.  
        // As it is, we don't memoize lazily, we just do it once and for all in insertWord.

        // Anyway, here is the mon-memoized code.  It was used as a reference implementation before optimizing
        // all needed traversal using links.  I think it's slower than link-following by a factor of 2 or so.

        // Get the parent of the node containing the first 'bad' letter (if we already know it, we don't need to use this method).
        const NodeT *parent = NULL;
        if (fromBeg < fromEnd) {
            // Start at the root and find the starting parent by going deeper through children:
            parent = mRoot;
            for (int j = 0; j < fromBeg; j++) {
                parent = parent->getBranchAtIndex(mCharMap.charIndex(keyWord[j]));
            }
        } else {
            // Start at the word and find the starting parent by going up through parents:
            parent = keyNode;
            while (--fromEnd >= 0) {
                parent = parent->getParent();
            }
        }
        assert(parent != NULL);

        while (fromBeg >= 0) {
            const NodeT *next = parent->getNextBranchFromIndex(mCharMap.charIndex(keyWord[fromBeg]));
            if (next != NULL) {
                return next->getFirstWordNode();
            }
            fromBeg--;
            parent = parent->getParent();
        }
        fromBeg = 0;
        return NULL;
    }


    const NodeT* subTrix(const char *key, int idx) const		// deprecated, doesn't say where the key failed
    {
        //  Follow the mBranches for each letter in the key up the specified index, and:
        //      return the last node, if key is a valid word stem, or 
        //      return NULL, if key is not a stem for any word in the trie.
        const NodeT *node = mRoot;
        do {
            uchr uc = *key++;
            uint ux = charIndex(uc);
            node = (NodeT *)node->mBranches[ux];
        } while (node != NULL && --idx >= 0);
        return node;
    }

    const NodeT* subTrie(const char *nullTerminatedKey) const	// deprecated: depends on null-termination, loses info.
    {
        //  Follow the mBranches for each letter in key, and:
        //      return the last node, if key is a valid word stem, or 
        //      return NULL, if key is not a stem for any word in the trie.
        //  The key must be null-terminated, and we throw away the information of where the key fails.
        const NodeT *node = mRoot;
        for (const char *pc = nullTerminatedKey; *pc != '\0'; pc++) {
            uchr uc = *pc;
            uint ux = mCharMap.charIndex(uc);
            node = node->mBranches[ux];
            if (node == NULL) {
                return  NULL;			// Not found - reached end of branch
            }
        }
        return node;
    }

    bool isSubKeyStem(const char *subKey, int subKeyLength, const NodeT* lastStemNode) const	// deprecated: flexible but tricky to call
    {
        //  subKey may be the address of a letter in the interior of a key; if so, 'this' must correspond to the letter at that address.
        //  Follow the mBranches for each letter in subKey, starting from this, 
        //      return true if key is a valid word stem, or false if it is not, 
        //      and set lastStemNode to be the node of the last letter that is part of a valid stem.

        const NodeT * node = lastStemNode;
        assert(subKey && *subKey == node->mStem[this->mDepth]);		// The letter starting subKey must match letter at this node.
        while (subKeyLength >= 0) {
            uchr uc = *subKey++;
            uint ux = charIndex(uc);
            node = node->mBranches[ux];
            if ( ! node ) {
                return false;
            }
            lastStemNode = node;
            --subKeyLength;
        }
        return true;
    }

    /**
     * Extracts "words" -- that is, word-boundary delimited strings --
     * from a null-terminated char array (c-string).  These "words"
     * are not checked against any dictionary or rules, other than being
     * delimited by the beginning or end of the array or by non-word-forming
     * characters, such as whitespace or punctuation.
     * Returns the number of "new" words, that is, words added to the WordTrie.
     */
    static int addLineWordsToTrie(WordTrie<MapT, NodeT> *trie, char *line, const uint minWordLength, const uint maxWordLength);
    
    /** 
    * Supplement WordTrie with words from a text file.
    * 
    * Returns number of new words added.  Words already in the Trie, or outside the
    * specified length range, do not count.
    */
    static int addAllWordsInTextFile(const char *fileSpec, WordTrie<MapT, NodeT> *trie, uint minWordLength, uint maxWordLength, int verbosity);


    uint countBranchesPerChar(const CharFreqMap& freqMap)
    {
        uint totalCount = 0;    // return value
        uint maxBranches = 0, sumParentCounts = 0, sumBranchCounts = 0;
        uint targetSize = mCharMap.targetSize();
        uint countSize = 1 + targetSize;
        std::vector<uint> parentCounts(countSize, 0);
        std::vector<uint> branchCounts(countSize, 0);
        std::vector<uint> maxBranchCnt(countSize, 0);

        mRoot->countBranchesPerChar(mCharMap, targetSize, totalCount, &maxBranchCnt[0], &parentCounts[0], &branchCounts[0]);

        printf("idx let letCnt branCnt   aveBPC   maxBPC\n");
        for (uint jC = 0; jC < targetSize; jC++) {
            uint denom = parentCounts[jC];
            double ave = denom > 0 ? (double) branchCounts[jC] / denom : 0.0;
            sumParentCounts += parentCounts[jC];
            sumBranchCounts += branchCounts[jC];
            printf("%3d  %c %7d %7d  %7.4f    %3d\n", jC, freqMap.getCharFreqPair(jC).mChar, parentCounts[jC], branchCounts[jC], ave, maxBranchCnt[jC]);
        }
        printf("sumParentCounts & sumBranchCounts:  %d  %d\n", sumParentCounts, sumBranchCounts);
        double aveNumBranches = (double) sumBranchCounts / sumParentCounts;
        printf("aveCount & maxCount:  %f  %d\n", aveNumBranches, maxBranches);

        return totalCount;
    }

    uint countBranchesPerCharPos(const CharFreqMap& freqMap, uint maxDepth)
    {
        using namespace std;
        uint totalCount = 0;    // return value
        uint targetSize = mCharMap.targetSize();
        uint countSize = 1 + targetSize;
        vector<vector<uint> > letterPosCounts(countSize);
        vector<vector<uint> > branchPosCounts(countSize);
        vector<vector<uint> > maxBranchPosCnt(countSize);
        for (uint j = 0; j < countSize; j++) {
            letterPosCounts[j].resize(maxDepth);
            branchPosCounts[j].resize(maxDepth);
            maxBranchPosCnt[j].resize(maxDepth);
        }
        assert(branchPosCounts[countSize-1][maxDepth-1] == 0);

        mRoot->countBranchesPerCharPos(mCharMap, targetSize, totalCount, &maxBranchPosCnt[0], &letterPosCounts[0], &branchPosCounts[0], maxDepth);

        printf("idx let letCnt brnCnt max \n");
        for (uint jC = 0; jC < targetSize; jC++) {
            printf("\n%3d  %c   nodeC branchC   aveBPN   maxBPN\n", jC, freqMap.getCharFreqPair(jC).mChar);
            for(uint depth = 1, endDepth = letterPosCounts[jC].size(); depth < endDepth; depth++) {
                uint numer = branchPosCounts[jC][depth];
                uint denom = letterPosCounts[jC][depth];
                double ave = denom > 0 ? (double) numer / denom : 0.0;
                printf("        %6d  %6d  %7.4f       %2d\n"
                    , letterPosCounts[jC][depth], branchPosCounts[jC][depth], ave, maxBranchPosCnt[jC][depth]);
            }
        }
 
        return totalCount;
    }


private:
    const MapT  & mCharMap;
    const uint    mWordLength;	// If this is > 0, all words must be exactly this long.

    NodeT       * mRoot;        // The root node has no parent, and its depth == 0.

public:
    uint          mTotalNodes;
};


#endif // WordTrie_hpp