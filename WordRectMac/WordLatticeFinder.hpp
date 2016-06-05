// wordRectFinder.hpp : find largest dense rectangles of words from a dictionary, multi-threaded
// Sprax Lines, July 2010

#ifndef WordLatticeFinder_hpp
#define WordLatticeFinder_hpp

#include "WordRectFinder.hpp"

/**
*
*  Examples of Word Wafflea:
*    A)  Where the minor rows and cols
*        are ot words:
*      A B A S H E D
*      B   R   Y   E
*      A R C A D E S
*      S   A   R   S
*      H Y D R I D E
*      E   E   D   R
*      D E S S E R T
*/
template <typename MapT>
class WordLatticeFinder : public WordRectFinder<MapT>
{
public:

    WordLatticeFinder(WordTrie<MapT, TrixNode> *wordTries[], const WordMap *maps, int wide, int tall, int numToFind, uint options) 
        : WordRectFinder<MapT>(wordTries, maps, wide, tall, numToFind, options)
    { 
        mColWordsNow = (const char**) new char*[wide];
    }

    WordLatticeFinder(const WordLatticeFinder&);			// Prevent pass-by-value by not defining copy constructor.
    WordLatticeFinder& operator=(const WordLatticeFinder&);	// Prevent assignment by not defining this operator.
    virtual ~WordLatticeFinder() 
    {
        free(mColWordsNow);
    }


private:
#if DEFINE_TEMPLATES_IN_CPP
    virtual int  findWordRows();
#else
    virtual int  findWordRows()  { return findWordRowsUsingTrieLinks(0); }
#endif

    virtual int  findWordRowsUsingTrieLinks(int haveTall);
    virtual int  findWordColsUsingTrieLinks(int row, int col);

    int  findWordRowsUsingTrieLinks_iterTodo(int haveTall); // TODO or ToRemove

    virtual bool isSymmetricSquare() const;
    virtual void printWordRows(const char *wordRows[], int haveTall) const;

protected:

    const char       **mColWordsNow;
};

#endif // WordLatticeFinder_hpp