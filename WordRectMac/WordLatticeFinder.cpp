// WordLatticeFinder.cpp : find word lattice of the specified dimensions 
// Sprax Lines, July 2010

#include "WordLatticeFinder.hpp"
#include "WordRectPrinter.hpp"
#include "WordRectSearchMgr.hpp"

#if DEFINE_METHODS_IN_CPP
template <typename MapT>
int WordLatticeFinder<MapT>::findWordRows()
{
    return findWordRowsUsingTrieLinks(0);
}
#endif

template <typename MapT>
int WordLatticeFinder<MapT>::findWordRowsUsingTrieLinks(int haveTall)
{
    this->mNowTall = haveTall;
    if (haveTall > 1 && this->mWantArea <= WordRectSearchMgr<MapT>::getTrumpingArea()) {
        this->mState = WordRectFinder<MapT>::eAborted;
        return -this->mMaxTall;  // Abort because a wordRect bigger than wantArea has been found
    }

    int prevTall = haveTall - 1;              // prevTall == -1 is ok.
    const TrixNode *rowWordNode = NULL;
    if (haveTall == 0) {
        rowWordNode = this->mRowTrie.getFirstWordNode();
    } else if (haveTall < this->mWantTall) {
        uchr uc = this->mColNodes[0][prevTall]->getLetterAt(haveTall);
        uint ux = this->colCharIndex(uc);
        rowWordNode = this->mRowTrie.getFirstWordNodeFromIndex(ux);
        if (this->mMaxTall < haveTall)
            this->mMaxTall = haveTall;
    } else {
        // Success: the row just added made words of all columns.
        // Record it right now, right here.
        this->mMaxTall = haveTall;
        for (int row = haveTall - 1, col = 0; col < this->mWantWide; col += 2) {
            this->mColWordsNow[col] = this->mColNodes[col][row]->getStem();  // Same as getFirstWord() in this context
        }
        for (int row = 0; row < haveTall; ++row)
            this->mRowWordsOld[row] = this->mRowWordsNow[row];
        this->mIsLastRectSymSquare = isSymmetricSquare();
        if (this->mIsLastRectSymSquare)
            ++this->mNumSymSquares;
        if (this->incNumFound() != this->getNumToFind() && WordRectSearchMgr<MapT>::getVerbosity() > 1) {
            WordRectPrinter::printRect(this);
        }
        return this->mWantArea;                   // Return the area
    }

    const char *word;
    const TrixNode **rowNodes = this->mRowNodes[haveTall];
    for (int k = 0, kOld = 0, area = 0; rowWordNode != NULL; ) {
        word = rowWordNode->getStem();  // We already know that rowWordNode is a word-node, so its mStem is a word
        for (;;) {
            // For each stem (partial column) that would result from adding this word as the next row,
            // is there a word of length wantTall that could complete it?
            rowNodes[k] = rowNodes[k-1]->getBranchAtIndex(this->rowCharIndex(word[k]));                        // Need this later, so always set it.
            if (k % 2 == 0) {
                const TrixNode *colBranch = this->mColNodes[k][prevTall]->getBranchAtIndex(this->rowCharIndex(word[k]));   // Need this later only if non-NULL.
                if (colBranch == NULL) {
                    break;                              // ...if not, break to get the next word.
                }
                this->mColNodes[k][haveTall] = colBranch;      // ...if so, add the branch node and continue.
            }
            if (k == this->mWantWideM1) {
                // At least one wantTall-length word exists to complete each column, including this row,
                this->mRowWordsNow[haveTall] = word;                                 
                // so try adding another row.  Even rows are solid; odd rows alternate as "waffle ridges"
                //if (haveTall == mWantTall - 1)
                //    area = findWordRowsUsingTrieLinks(mWantTall);
                //else
                area = findWordColsUsingTrieLinks(haveTall, kOld);
                // Return IF this call found a wordRect (area > 0) AND that completes this finder's quota, 
                // OR this call is aborting (area < 0 because another thread found one bigger than this 
                // one's wantArea, and negating the area is the signal for that).  Either way, we keep
                // returning to pop out of the stack of recursive calls.
                if ((area > 0 && this->getNumFound() == this->getNumToFind()) || area < 0) {
                    return area; 
                }
                break;                              // This word failed, so break to get the next word.
            }
            k++;
        }

        // As a candidate for wordRows[haveTall], this word failed at column k,
        // either locally (no completion for column k < wantWide-1) or deeper down
        // in the wordRect (all wantWide columns were ok, some later word row could not be found).
        // Either way, we try the next possible word, which is the first word whose 
        // k-letter stem is greater than this word's.  If such a word exists, we get 
        // it just by following 2 links:  (failed node)->mNextStemNode->mFirstWordNode.
        // (If we weren't using the rowNodes, we'd need to find the parent of the first
        // disqualified node, as does WordTrie::getNextWordNodeAndIndex, but over all, that is slower.)
        const TrixNode * nextStem = rowNodes[k]->getNextStemNode();
        if (nextStem == NULL) {
            return 0;   // We've tried all possible word stems for the partial columns we got, so return. 
        }
        k = nextStem->getDepth() - 1;
        kOld = k;
        if (kOld % 2 == 1)
            kOld++;
        rowWordNode = nextStem->getFirstWordNode();
    }
    return 0;   // Failure: No wantWide x wantTall word rect at the end of this path
}


template <typename MapT>
int WordLatticeFinder<MapT>::findWordColsUsingTrieLinks(int row, int col)
{
    static int zoidDbg = 0;
    if (col >= this->mWantWide) {
        return findWordRowsUsingTrieLinks(row + 2);
    }

    const TrixNode *colNode = this->mColNodes[col][row];
    const TrixNode *child  = colNode->getFirstChild();
    for (int rowP1 = row + 1; child != NULL; child = child->getNextBranch()) {
        this->mColNodes[col][rowP1] = child;
        if (col == 2) {
            zoidDbg++;
        }
        int area =  findWordColsUsingTrieLinks(row, col + 2);
        if (area < 0 || (area > 0 && this->getNumFound() == this->getNumToFind())) { // Either this call found a wordRect (area > 0),
            return area;                      // or it is aborting (because another thread found one bigger
        }                                   // than this one's wantArea); so return up through the stack
    }
    return 0;   // Failure: No wantWide x wantTall word rect at the end of this path
}




template <typename MapT>
int WordLatticeFinder<MapT>::findWordRowsUsingTrieLinks_iterTodo(int haveTall)
{
    static int zoidDbg = 0;

    this->mNowTall = haveTall;
    if (haveTall > 1 && this->mWantArea <= this->getFoundArea()) {
        this->mState = WordRectFinder<MapT>::eAborted;
        return -this->mMaxTall;  // Abort because a wordRect bigger than wantArea has been found
    }

    int prevTall = haveTall - 1;
    const TrixNode *rowWordNode = NULL;
    if (haveTall == 0) {
        rowWordNode = this->mRowTrie.getFirstWordNode();
    } else if (haveTall < this->mWantTall) {
        const TrixNode *firstColNode = this->mColNodes[0][prevTall];
        if (haveTall + 1 < this->mWantTall)
            rowWordNode  = this->mRowTrie.getFirstWordNodeFromIndex(rowCharIndex(this->mColNodes[0][prevTall]->getLetterAt(haveTall+1)));
        else
            rowWordNode = this->mRowTrie.getFirstWordNode();

        const TrixNode *rowWordNodeB = firstColNode->getFirstChild()->getFirstWordNode();
        if (rowWordNode != rowWordNodeB) {
            zoidDbg++;
        }

        if (this->mMaxTall < haveTall)
            this->mMaxTall = haveTall;
    } else {
        // Success: the row just added made words of all columns.
        // Record it right now, right here.
        this->mMaxTall = haveTall;
        for (int row = 0; row < haveTall; ++row)
            this->mRowWordsOld[row] = this->mRowWordsNow[row];
        this->mIsLastRectSymSquare = isSymmetricSquare();
        if (this->mIsLastRectSymSquare)
            ++this->mNumSymSquares;
        if (this->incNumFound() != this->getNumToFind() && WordRectSearchMgr<MapT>::getVerbosity() > 1) {
            PrintWordRectViaSearchMgr(this);
        }
        return this->mWantArea;                   // Return the area
    }

    if (haveTall % 2 == 0) {
        const TrixNode **rowNodes = this->mRowNodes[haveTall];
        for (int k = 0; rowWordNode != NULL; ) {
            const char *word = rowWordNode->getStem();  // We already know that rowWordNode is a word-node, so its mStem is a word
            for (;;) {
                // For each stem (partial column) that would result from adding this word as the next row,
                // is there a word of length wantTall that could complete it?
                rowNodes[k] = rowNodes[k-1]->getBranchAtIndex(this->rowCharIndex(word[k]));                        // Need this later, so always set it.

                if (k % 2 == 0) {
                    const TrixNode *colNodeK = this->mColNodes[k][prevTall];
                    const TrixNode *branch = colNodeK->getFirstChild();
                    const TrixNode *colBranch = NULL;
                    for ( ; branch != NULL; branch = branch->getNextBranch()) {
                        colBranch = branch->getBranchAtIndex(this->colCharIndex(word[k]));   // Need this later only if non-NULL.
                        if (colBranch != NULL) 
                            break;
                    }
                    if (colBranch == NULL) {
                        break;                              // ...if not, break to get the next word.
                    }
                    this->mColNodes[k][haveTall] = colBranch;      // ...if so, add the branch node and continue.
                }
                if (k >= this->mWantWideM1) {
                    // At least one wantTall-length word exists to complete each column, including this row,
                    this->mRowWordsNow[haveTall] = word;                                 // so try adding another row.
                    int area =  findWordRowsUsingTrieLinks(haveTall + 1);
                    if (area < 0 || (area > 0 && this->mNumFound == this->getNumToFind())) { // Either this call found a wordRect (area > 0),
                        return area;                      // or it is aborting (because another thread found one bigger
                    }                                   // than this one's wantArea); so return up through the stack
                    break;                              // This word failed, so break to get the next word.
                }
                k++;
            }
            // As a candidate for wordRows[haveTall], this word failed at column k,
            // either locally (no completion for column k < wantWide-1) or deeper down
            // in the wordRect (all wantWide columns were ok, some later word row could not be found).
            // Either way, we try the next possible word, which is the first word whose 
            // k-letter stem is greater than this word's.  If such a word exists, we get 
            // it just by following 2 links:  (failed node)->mNextStemNode->mFirstWordNode.
            // (If we weren't using the rowNodes, we'd need to find the parent of the first
            // disqualified node, as does WordTrie::getNextWordNodeAndIndex, but over all, that is slower.)
            const TrixNode * nextStem = rowNodes[k]->getNextStemNode();
            if (nextStem == NULL) {
                return 0;   // We've tried all possible word stems for the partial columns we got, so return. 
            }
            k = nextStem->getDepth() - 1;
            rowWordNode = nextStem->getFirstWordNode();
        }
    } else { // haveTall is odd
        for (int k = 0; k < this->mWantWide; k += 2) {
            this->mColNodes[k][haveTall] = this->mColNodes[prevTall][k]->getFirstChild();
        }

    }
    return 0;   // Failure: No wantWide x wantTall word rect at the end of this path
}

template <typename MapT>
bool WordLatticeFinder<MapT>::isSymmetricSquare() const
{
    if (this->mWantTall != this->mWantWide)
        return false;                           // not square
    for (int row = 0; row < this->mWantTall; ++row) {
        for (int col = this->mWantWide; --col >= 0; ) {
            if (this->mRowNodes[row][col] != this->mColNodes[row][col])
                return false;                   // not symmetric
        }
        if (++row == this->mWantTall)
            break;
        for (int col = 0; col < this->mWantWide; col += 2) {
            if (this->mRowNodes[row][col] != this->mColNodes[row][col])
                return false;                   // not symmetric
        }
    }
    return true;                                // square and symmetric
}


/** Print actual word rows of complete or in-progress word rectangle. */
template <typename MapT>
void WordLatticeFinder<MapT>::printWordRows(const char *wordRows[], int haveTall) const
{
    for (int row = haveTall - 1, col = 0; col < this->mWantWide; col += 2) {
        this->mColWordsNow[col] = this->mColNodes[col][row]->getStem(); // same as getFirstWord() in this context
    }

    for (int row = 0; row < haveTall; ++row) {
        for (const char *pc = wordRows[row]; *pc != '\0'; pc++) {
            printf(" %c", *pc);
        }
        printf("\n");
        if (++row == haveTall)
            break;
        for (int col = 0; col < this->mWantWide; col += 2) {
            printf(" %c  ", this->mColWordsNow[col][row]);
        }
        printf("\n");
    }
    /*   printf("\n");
    WordRectFinder::printWordRows(wordRows, haveTall);
    printf("\n");*/
}

/*
a b a c a b a
b   b   b   b
a b a c a b a
c   c   c   c
a b a c a b a
b   b   b   b
a b a c a b a
*/
