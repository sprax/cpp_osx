// TrieNode.cpp : trie nodes for words
// Sprax Lines,  July 2010
//

#include "TrieNode.hpp"
#include "CharMap.hpp"

#include <stdio.h>
#include <stdlib.h>

TrieNode::TrieNode(const CharMap &charMap, int depth, TrieNode *parent) 
    : mFirstBranch(NULL), mNextBranch(NULL), mFirstWordNode(NULL)
    , mStem(NULL), mDepth(depth), mCharMap(charMap)
{
    mBranches = (TrieNode**)calloc(charMap.targetSize(), sizeof(TrieNode*)) - charMap.targetBegIdx();
};

TrieNode::~TrieNode() 
{ 
    delete (mBranches + mCharMap.targetBegIdx()); 
}


void TrieNode::printWordsAll() const
{
    ////if (mWord != NULL) {
    if (this == mFirstWordNode) {
        printf("%s\n", mStem);
    }
    for (const TrieNode *node = mFirstBranch; node != NULL; node = node->mNextBranch) {
        node->printWordsAll();
    }
}

