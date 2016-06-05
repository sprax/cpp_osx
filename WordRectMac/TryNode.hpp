// TryNode.hpp : A base class implementing trie nodes for word tries. 
// The allowed chars need not be contiguous as numbers/indices.
// The TryNode itself contains the mapping from chars to child indices,
// and it may be different for distinct nodes.  The goal is to make
// the maps small but adaptive.
//
// TODO: Consider maps A, B, C, D, and E, and try to use only B, C, D.
// Regarding L as a letter or a "virtual" node, then:
// L.mapA can handle all possible continuations from L in any letter position
// L.mapB handles continuations from L only when L *begins* a word.
// L.mapC handles continuations from L only when L *continues* a word in the middle
// L.mapD handles continuations from L only when L is the 2nd to last letter in a word.
// L.mapE goes backwards.  If L is the last letter, mapE defines all antecedents as back-branches.
// 
// Sprax Lines,  October 2012


#ifndef TryNode_hpp
#define TryNode_hpp

#include <stdio.h>

#include "wordTypes.h"
#include "CharMap.hpp"

class TryNode
{
    template <typename MapT, typename NodeT>
    friend class WordTrie;

protected:

    TryNode       **mBranches;     // all possible continuations...
    const TryNode  *mFirstBranch;  // First non-NULL branch is the first node in the mBranches as a linked list
    const TryNode  *mNextBranch;	// Assumes fixed length; otherwise, the next branch depends on the desired word length
    TryNode        *mParent;       // One level up in the trie tree is the node representing this stem's "prefix" (the string w/o last this one's last letter)

////const char *mWord;  // The word, if this is a word node; NULL, if it is not a word node.  Used as a boolean and for printing.
    const char *mStem;  // Some word that completes this stem; only the first N chars matter, where N == depth.  But if the trie
        // is loaded in alphabetic order, then mStem == mFirstWordNode.mWord.  If other words are inserted later, it may be necessary
        // (and worthwhile in terms of performance) to maintain this identity by fixing up both mStem and/or mFirstWordNode, as needed.
    const int  mDepth;  // level in the tree.  Note that stem[depth] is this node's letter.  This const int prevents generation of assignment operator (ok).
    TryNode * mFirstWordNode;	// node of first word that completes this one's stem (this, if this is a word node)
    TryNode * mNextStemNode;	// node of first stem after this one 
    // TryNode * nextWordNode;	// node of first word that does not start with this one's stem.  Examples: 
    //     For stem a, first word is aah, next stem is b, and next word is baa (not aba or aal); 
    //     For ac, first word is ace, next stem is ad, and next word is add (not act);
    //     For stem ace, first word is ace (itself), and the next stem & next word are the same: act.
    //     mNextStemNode is more useful than nextWordNode.  Note that if a node's nextWordNode is not null, 
    //     then node.nextWordNode == node.mNextStemNode.mFirstWordNode.  In practice, it may be more useful 
    //     to go through the mNextStemNode rather than skipping directly to a mNextStemNode, because the
    //     mNextStemNode, if it exists, carries additional information, such as its depth.

public:

    TryNode(const CharMap &charMap, int depth, TryNode *parent)              // constructor
        : mFirstBranch(NULL), mNextBranch(NULL), mParent(parent)
        ////, mWord(NULL)
        , mStem(NULL)
        , mDepth(depth), mFirstWordNode(NULL), mNextStemNode(NULL)
        //, nextWordNode(NULL)
    {
        // mBranches = (TryNode**)calloc(charMap.targetSize(), sizeof(TryNode*)) - charMap.targetOffset();
    };



    TryNode (const TryNode&);                                                 // Do not define.
    TryNode& operator=(const TryNode&);                                       // Do not define.
    ~TryNode() { delete mBranches; }

////inline const char     * getWord()			const { return mWord; }			// NULL, if this is not a word-node
    inline const char     * getWord()			const { return this == mFirstWordNode ? mStem : NULL; }
    inline const char     * getFirstWord()	    const { assert(mFirstWordNode); return mFirstWordNode->getStem(); } // getWord() should not be needed
    inline const char     * getStem()           const { return mStem; }
    inline       uint       getDepth()			const { return mDepth; }
    inline		 uchr	    getLetter()			const { return mStem[mDepth]; }
    inline		 uchr	    getLetterAt(int ix) const { return mStem[ix]; }
    inline		 TryNode * getParent()			const { return mParent; }		// NULL, if this is a root node
    inline const TryNode * getFirstChild()	    const { return mFirstBranch; }	
    inline const TryNode * getNextBranch()	    const { return mNextBranch; }	
    inline const TryNode * getFirstWordNode()	const { return mFirstWordNode; }	
    inline const TryNode * getNextStemNode()   const {	return mNextStemNode; }
    //	inline const TryNode * getNextWordNode()	const { return nextWordNode; }	// nextWordNode == mNextStemNode->mFirstWordNode, so we don't really need nextWordNode

    // virtual methods.  An inline hint may be honored by the compiler if the actual class of the calling object is known at compile time.
    inline virtual void readAsDictStem()    { };
    inline virtual void readAsDictWord()    { };
    inline virtual void readAsTextStem()    { };
    inline virtual void readAsTextWord()    { };

    inline const TryNode * getBranchAtIndex(int index) const {
        return mBranches[index];    // may return NULL 
    }
    const TryNode * getNextBranchFromIndex(int index) const {
        assert(mBranches[index]);
        return mBranches[index]->mNextBranch; 
    }
    inline const TryNode * getFirstWordNodeFromIndex(int index) const {
        if (mBranches[index] != NULL) {
            return mBranches[index]->mFirstWordNode;
        }
        // Still here?  Expect few non-NULL mBranches, so use the links instead of the array indices
        for (const TryNode *branch = mFirstBranch; branch != NULL; branch = branch->mNextBranch) {
            if (branch->mStem[0] >= index) {
                return branch->mFirstWordNode;
            }
        }
        return NULL;
    }

    void TryNode::printWordsAll() const
    {
        ////if (mWord != NULL) {
        if (this == mFirstWordNode) {
            printf("%s\n", mStem);
        }
        for (const TryNode *node = mFirstBranch; node != NULL; node = node->mNextBranch) {
            node->printWordsAll();
        }
    }

};

#endif // TryNode_hpp