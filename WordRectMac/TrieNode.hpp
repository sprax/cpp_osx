// TrieNode.hpp : Base class implementing trie nodes for word tries. 
// The allowed chars need not be contiguous as numbers/indices.
// The WordTrie's template argument provides the mapping from char to index.
// See WordTrie and TracNode, a derived class that adds word counting for 
// textual analysis, auto-completion, etc.
// 
// Sprax Lines,  July 2010

// Note: Keeping both mStem and mWord as members can be redundant, if the length of the stem is
//       already stored as part of its string representation, or is known independently,
//       as is the case for fixed-word-length tries.   In those cases, the node is a word-node 
//       IFF node.mDepth == mStem.length, OR node.mDepth = WordTrie.mWordLength, respectively.
//       ALternatively, each TrieNode could store the length of the *first* word that completes
//       its mStem, as in mFirstWordLength.  Then a node is a word-node IFF mDepth == mFirstWordLength.
//       Yet again, if TrieNode were extended to keep a count of its own occurrences as a word 
//       in a lexicon or any other texts, then of course isWord(node) could be defined by
//       node.mCount > 0.  But all of these treatments have downsides.  If "aced" is loaded
//       before "ace" in a variable-word-length trie, the stem for the 'e'-node will have to
//       be changed from "aced" to "ace" so that mStem.length == mDepth and getWord will return
//       "ace", not "aced".  Or, in the case of fixed-word-length tries, the isWord method could
//       not be defined on TrieNode, but would have to be defined on on the fixed-length WordTrie, 
//       which assumes that such an object always available.  The mFirstWordLength approach is again
//       vulnerable to load-order: mStem would have to be changed if "aces" were loaded before "aced",
//       and if "aced" were loaded before "ace", then mFirstWordLength would also have to be changed.
//       Word counts do not depend on the trie or load order, but unless the word count is actually
//       used, maintaining a count field may be useless overhead in time and space.      
//       
//       Current approach: Get rid of mWord, then define:
//       bool isWord()   { return this == mFirstWordNode; }
//       word getWord()  { return this == mFirstWordNode ? mStem : NULL; }
//       When a node is known to be a word-node, just use getStem() instead of getWord().

// TODO: typedef AcWordTrie for Auto-complete.

#ifndef TrieNode_hpp
#define TrieNode_hpp

#include <vector>

#include "CharMap.hpp"

class TrieNode
{
    template <typename MapT, typename NodeT>
    friend class WordTrie;

protected:

    TrieNode       **mBranches;     // all possible continuations...
    const TrieNode  *mFirstBranch;  // First non-NULL branch is the first node in the mBranches as a linked list
    const TrieNode  *mNextBranch;	// Assumes fixed length; otherwise, the next branch depends on the desired word length
    const TrieNode  *mFirstWordNode;	// node of first word that completes this one's stem (this, if this is a word node)

    const char      *mStem;  // Some word that completes this stem; only the first N chars matter, where N == depth.  But if the trie
        // is loaded in alphabetic order, then mStem == mFirstWordNode.mWord.  If other words are inserted later, it may be necessary
        // (and worthwhile in terms of performance) to maintain this identity by fixing up both mStem and/or mFirstWordNode, as needed.
    ////const char *mWord;  // The word, if this is a word node; NULL, if it is not a word node.  Used as a boolean and for printing.
    const uint       mDepth;  // level in the tree.  Note that stem[depth] is this node's letter.  This const int prevents generation of assignment operator (ok).
    const CharMap& mCharMap;

public:
    TrieNode(const CharMap &charMap, int depth, TrieNode *parent=NULL); // constructor
    TrieNode (const TrieNode&);                                         // Do not define.
    TrieNode& operator=(const TrieNode&);                               // Do not define.
    virtual ~TrieNode();                                                // virtual destructor

////inline const char     * getWord()			        const { return mWord; }			// NULL, if this is not a word-node
    inline const char     * getWord()			        const { return this == mFirstWordNode ? mStem : NULL; }
    inline const char     * getFirstWord()	            const { assert(mFirstWordNode); return mFirstWordNode->getStem(); } // getWord() should not be needed
    inline const char     * getStem()                   const { return mStem; }
    inline       uint       getDepth()			        const { return mDepth; }
    inline		 uchr	    getLetter()			        const { return mStem[mDepth]; }
    inline		 uchr	    getLetterAt(uint ix)        const { return mStem[ix]; }
    inline       uint       getIndexFromChar(uchr uc)   const { return mCharMap.charToIndex(uc); }
    inline const TrieNode * getFirstChild()	            const { return mFirstBranch; }	
    inline const TrieNode * getNextBranch()	            const { return mNextBranch; }	
    inline const TrieNode * getFirstWordNode()	        const { return mFirstWordNode; }	
    inline const TrieNode * getBranchFromChar(uchr uc)	const { return mBranches[getIndexFromChar(uc)]; }
    inline const TrieNode * getBranchAtIndex(uint  ux)  const { return mBranches[ux]; }  // May return NULL 
////inline const TrieNode * getNextStemNode()       const {	return mNextStemNode; }
    //	inline const TrieNode * getNextWordNode()	const { return nextWordNode; }	// nextWordNode == mNextStemNode->mFirstWordNode, so we don't really need nextWordNode

    // virtual methods.  An inline hint may be honored by the compiler if the actual class of the calling object is known at compile time.
    inline virtual void readAsDictStem()    { };
    inline virtual void readAsDictWord(TrieNode *prevWordNode)    { };
    inline virtual void readAsTextStem()    { };
    inline virtual void readAsTextWord()    { };

    const TrieNode * getNextBranchFromIndex(int index) const {
        assert(mBranches[index]);
        return mBranches[index]->mNextBranch; 
    }
    inline const TrieNode * getFirstWordNodeFromIndex(int index) const {
        if (mBranches[index] != NULL) {
            return mBranches[index]->mFirstWordNode;
        }
        // Still here?  Expect few non-NULL mBranches, so use the links instead of the array indices
        for (const TrieNode *branch = mFirstBranch; branch != NULL; branch = branch->mNextBranch) {
            if (branch->mStem[0] >= index) {
                return branch->mFirstWordNode;
            }
        }
        return NULL;
    }

    void printWordsAll() const;

    void countBranchesPerChar(const CharMap& charMap, uint myIdx, uint& totCount, uint maxBranchCnt[], uint charCounts[], uint branchCounts[])
    {
        uint count = 0;
        for (uint j = charMap.targetBegIdx(), endIdx = charMap.targetEndIdx(); j < endIdx; j++) {
            if (mBranches[j] != NULL) {
                mBranches[j]->countBranchesPerChar(charMap, j, totCount, maxBranchCnt, charCounts, branchCounts);
                ++count;
            }
        }
        ++totCount;
        if (count > 0) {
            if (maxBranchCnt[myIdx] < count)
                maxBranchCnt[myIdx] = count;
            ++charCounts[myIdx];
            branchCounts[myIdx] += count;
        }
    }

    void countBranchesPerCharPos(const CharMap& charMap, uint myIdx, uint& totCount, std::vector<uint> maxBranchCnt[], std::vector<uint> charPosCounts[], std::vector<uint> branchPosCounts[], uint maxDepth)
    {
        if (mDepth >= maxDepth)
            return;

        uint count = 0;
        for (uint j = charMap.targetBegIdx(), endIdx = charMap.targetEndIdx(); j < endIdx; j++) {
            if (mBranches[j] != NULL) {
                mBranches[j]->countBranchesPerCharPos(charMap, j, totCount, maxBranchCnt, charPosCounts, branchPosCounts, maxDepth);
                ++count;
            }
        }
        ++totCount;
        if (count > 0) {
            if (maxBranchCnt[myIdx][mDepth] < count)
                maxBranchCnt[myIdx][mDepth] = count;
            ++charPosCounts[myIdx][mDepth];
            branchPosCounts[myIdx][mDepth] += count;
        }
    }

};

#endif // TrieNode_hpp