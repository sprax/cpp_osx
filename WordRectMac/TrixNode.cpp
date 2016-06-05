// TrixNode.cpp : trie nodes for words
// Sprax Lines,  July 2010

#include "TrixNode.hpp"
#include "CharMap.hpp"

TrixNode::TrixNode(const CharMap &charMap, int depth, TrixNode *parent) 
    : TrieNode(charMap, depth)
    , mParent(parent), mNextStemNode(NULL)
{ };


