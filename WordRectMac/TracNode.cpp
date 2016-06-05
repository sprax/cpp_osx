// TracNode.cpp : trie nodes for words
// Sprax Lines,  July 2010

#include "TracNode.hpp"
#include "CharMap.hpp"

template <typename MapT, typename NodeT>
int TracNode::test_TracNode(const WordTrie<MapT, NodeT> &trie, const MapT &charMap)
{
	TracNode *tracNode = new TracNode(charMap, 0);
	delete    tracNode;
	return 0;
}

