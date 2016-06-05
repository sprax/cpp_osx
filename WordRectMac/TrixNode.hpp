// TrixNode.hpp : Basic class implementing trie nodes for word tries. 
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
//       ALternatively, each TrixNode could store the length of the *first* word that completes
//       its mStem, as in mFirstWordLength.  Then a node is a word-node IFF mDepth == mFirstWordLength.
//       Yet again, if TrixNode were extended to keep a count of its own occurrences as a word 
//       in a lexicon or any other texts, then of course isWord(node) could be defined by
//       node.mCount > 0.  But all of these treatments have downsides.  If "aced" is loaded
//       before "ace" in a variable-word-length trie, the stem for the 'e'-node will have to
//       be changed from "aced" to "ace" so that mStem.length == mDepth and getWord will return
//       "ace", not "aced".  Or, in the case of fixed-word-length tries, the isWord method could
//       not be defined on TrixNode, but would have to be defined on on the fixed-length WordTrie, 
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

#ifndef TrixNode_hpp
#define TrixNode_hpp

#include <stdio.h>

#include "TrieNode.hpp"

class CharMap;      // forward declaration

class TrixNode : public TrieNode
{
    template <typename MapT, typename NodeT>
    friend class WordTrie;

protected:

    TrixNode        *mParent;       // One level up in the trie tree is the node representing this stem's "prefix" (the string w/o last this one's last letter)
    TrixNode * mNextStemNode;	// node of first stem after this one 
    // TrixNode * nextWordNode;	// node of first word that does not start with this one's stem.  Examples: 
    //     For stem a, first word is aah, next stem is b, and next word is baa (not aba or aal); 
    //     For ac, first word is ace, next stem is ad, and next word is add (not act);
    //     For stem ace, first word is ace (itself), and the next stem & next word are the same: act.
    //     mNextStemNode is more useful than nextWordNode.  Note that if a node's nextWordNode is not null, 
    //     then node.nextWordNode == node.mNextStemNode.mFirstWordNode.  In practice, it may be more useful 
    //     to go through the mNextStemNode rather than skipping directly to a mNextStemNode, because the
    //     mNextStemNode, if it exists, carries additional information, such as its depth.

public:
    TrixNode(const CharMap &charMap, int depth, TrixNode *parent=NULL);     // constructor
    TrixNode (const TrixNode&);                                             // Do not define.
    TrixNode& operator=(const TrixNode&);                                   // Do not define.
    ~TrixNode() { }

////inline const char     * getWord()			const { return mWord; }			// NULL, if this is not a word-node
    inline const char     * getWord()			const { return this == mFirstWordNode ? mStem : NULL; }
    inline const char     * getFirstWord()	    const { assert(mFirstWordNode); return mFirstWordNode->getStem(); } // getWord() should not be needed
    inline const char     * getStem()           const { return mStem; }
    inline       uint       getDepth()			const { return mDepth; }
    inline		 uchr	    getLetter()			const { return mStem[mDepth]; }
    inline		 uchr	    getLetterAt(int ix) const { return mStem[ix]; }
    inline		 TrixNode * getParent()			const { return mParent; }		// NULL, if this is a root node
    inline const TrixNode * getFirstChild()	    const { return (TrixNode *)mFirstBranch; }	
    inline const TrixNode * getNextBranch()	    const { return (TrixNode *)mNextBranch; }	
    inline const TrixNode * getFirstWordNode()	const { return (TrixNode *)mFirstWordNode; }	
    inline const TrixNode * getNextStemNode()   const {	return mNextStemNode; }
    //	inline const TrixNode * getNextWordNode()	const { return nextWordNode; }	// nextWordNode == mNextStemNode->mFirstWordNode, so we don't really need nextWordNode

    // virtual methods.  An inline hint may be honored by the compiler if the actual class of the calling object is known at compile time.
    virtual void readAsDictStem()    { };
    virtual void readAsDictWord(TrixNode *prevWordNode)
    {
        if (prevWordNode != NULL) {

            TrixNode *nodeParent = this, *prevParent = prevWordNode, *nextChild = this;
            // set next word and next stem for shallower nodes that don't already have these pointers set.
            while (nodeParent->mFirstWordNode == NULL) {
                nodeParent->mFirstWordNode =  this;
                nextChild = nodeParent;             // Next child of greatest common ancestor of prevWordNode and this new word node
                nodeParent = nodeParent->getParent();
                //if (nodeParent == NULL) {
                //    break;				
                //}
                prevParent = prevParent->getParent();
            }
            // assert (nextChild == nodeParent->getBranchAtIndex(charIndex(word[nodeParent->mDepth])));
            for (TrixNode *pwn = prevWordNode; pwn != prevParent; pwn = pwn->getParent()) {
                assert(pwn != NULL);
                pwn->mNextStemNode =  nextChild;
            }
        } else {
            for (TrixNode *parent = this; parent != NULL; parent = parent->getParent()) {
                if (parent->mFirstWordNode == NULL) {
                    parent->mFirstWordNode =  this;
                    parent->mStem          =  mStem;
                } else {
                    break;				// all higher (shallower) nodes already point to words that precede this one
                }
            }
        }



    };
    inline virtual void readAsTextStem()    { };
    inline virtual void readAsTextWord()    { };

    inline const TrixNode * getBranchAtIndex(int index) const {
        return (TrixNode *) mBranches[index];    // may return NULL 
    }
    const TrixNode * getNextBranchFromIndex(int index) const {
        assert(mBranches[index]);
        return (TrixNode*)getNextBranch(); 
    }
    inline const TrixNode * getFirstWordNodeFromIndex(int index) const {
        if (mBranches[index] != NULL) {
            return (TrixNode *)mBranches[index]->getFirstWordNode();
        }
        // Still here?  Expect few non-NULL mBranches, so use the links instead of the array indices
        for (const TrieNode *branch = mFirstBranch; branch != NULL; branch = branch->getNextBranch()) {
            if (branch->getStem()[0] >= index) {
                return (TrixNode *)branch->getFirstWordNode();
            }
        }
        return NULL;
    }

};

#endif // TrixNode_hpp