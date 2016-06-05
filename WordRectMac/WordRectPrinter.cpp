// WordRectPrinter.hpp : display word rects in multi-threaded process
// Sprax Lines, July 2012

#include "WordRectPrinter.hpp"

CRITICAL_SECTION	WordRectPrinter::sPrintLock;

#ifdef _DEBUG
// Make sure template methods actually compile for base class
static void test_WordRectPrinter()
{
    WordRectFinder<CharMap> *pWRF = NULL;
    WordRectPrinter::printRect(pWRF);
}
#endif

