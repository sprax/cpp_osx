// TracNode.hpp : TrieNode extended to count occurrances of stems and words in dictionaries and texts.
// See TrieNode.hpp and WordTrie.hpp.
// Code ported from my AutoCompleteWord.java
//
// Sprax Lines,  October 2011


#ifndef TracNode_hpp
#define TracNode_hpp

#include "TrieNode.hpp"
#include "WordTrie.hpp"     // Needed for unit test function signature


class TracNode : public TrieNode
{
    template <typename MapT, typename NodeT>
    friend class WordTrie;

protected:

    uint mWordCount;   // number of times this nodes occurs as a word-node in all source text(s), including the dictionary.
    uint mStemCount;   // number of words in the lexicon that include this node in their stems.
    uint mTextCount;   // number of times this nodes occurs as a stem-node in all source text(s). excluding the dictionary.

public:
    TracNode(const CharMap &charMap, int depth, TracNode *parent=NULL)   // constructor
        : TrieNode(charMap, depth)
        , mWordCount(0), mStemCount(0), mTextCount(0)
    { }
    TracNode (const TracNode&);                 // Do not define.
    TracNode& operator=(const TracNode&);       // Do not define.
    ~TracNode() { }

    inline uint         getNumStemWords()	const { return mStemCount; }

    // virtual methods.  An inline hint may be honored by the compiler if the actual class of the calling object is known at compile time.
    inline virtual void readAsDictStem()                        { ++mStemCount; };
    inline virtual void readAsDictWord(TrieNode *prevWordNode)  { ++mStemCount; ++mWordCount; };
    inline virtual void readAsTextStem()                        { ++mTextCount; };
    inline virtual void readAsTextWord()                        { ++mTextCount; ++mWordCount; };

    // unit test function
    template <typename MapT, typename NodeT>
    static int test_TracNode(const WordTrie<MapT, NodeT>& trie, const MapT& charMap);
};

#endif // TracNode_hpp