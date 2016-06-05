// WordWaffleFinder.cpp : find word rectangle of the specified dimensions 
// Sprax Lines, July 2010
////////////////////////////////////////////////////////////////////////////////

#include "WordWaffleFinder.hpp"
#include "wordRectPrinter.hpp"
#include "wordRectSearchMgr.hpp"

template <typename MapT>
int WordWaffleFinder<MapT>::findWordRowsUsingTrieLinksA(int)
{
    return findFirstWordRow();
}

/**
Row 0: The first candidate word is the first wide-letter word in the dictionary.
Verify that each even-column letter begins some tall-letter word,
and that each odd-column letter begins some oddTall-letter word.
*/
template <typename MapT>
int WordWaffleFinder<MapT>::findFirstWordRow()
{
    this->setNowTall(0);
    int prevTall = -1;
    const TrixNode **rowNodes = this->mRowNodes[0], *colBranch;
    const TrixNode  *rowNode = rowNodes[-1]->getFirstChild();
    while (rowNode != NULL) {
        int             col = rowNode->getDepth() - 1;
        const char *rowWord = rowNode->getStem(); // same result as getFirstWord();

        // For each vertical stem (partial column) that would result from adding this word
        // as the next row, is there a whole word of length wantTall that could complete it?
        // If yes (IFF the implied colNode is non-null), continue; else break and get next word.
        for (;;) {
            rowNodes[col] = rowNodes[col-1]->getBranchAtIndex(this->rowCharIndex(rowWord[col])); // Need this later, so always set it.

            // Get column node as colBranch.  We need colBranch later only if it is non-NULL.
            colBranch = this->mColNodes[col][prevTall]->getBranchAtIndex(this->colCharIndex(rowWord[col]));
            if (colBranch == NULL) {
                break;                      // ...if not, break to get the next word.
            }
            this->mColNodes[col][0] = colBranch;  // ...if so, add the branch node and continue.
            if (col == this->mWantWideM1) {
                // At least one wantTall-length word exists to complete each column, including this row,
                this->mRowWordsNow[0] = rowWord;                                 // so try adding another row.
                int area =  (this->*mEvnRowFinder)(1);
                if (area != 0) {	// Either the call above found a wordRect (area > 0),
                    return area;    // or it is aborting (another thread beat this one).
                }                   // So return up through the stack.
                break;              // This word failed, so break to get the next word.
            }
            col++;
        }
        // As a candidate for wordRows[haveTall], this word failed at column k,
        // either locally (no completion for column k < wantWide-1) or deeper down
        // in the wordRect (all wantWide columns were ok, some later word row could not be found).
        // Either way, we try the next possible word, which is the first word whose 
        // k-letter stem is greater than this word's.  If such a word exists, we get 
        // it just by following 2 links:  (failed node)->mNextStemNode->mFirstWordNode.
        // (If we weren't using the rowNodes, we'd need to find the parent of the first
        // disqualified node, as does WordTrie::getNextWordNodeAndIndex, but over all, that is slower.)
        rowNode = rowNodes[col]->getNextStemNode();
    }
    // We've tried all possible words for the first row, so return area==0.
    return 0;
}

/**
Odd Row (any odd row): The first candidate word is the first 
oddWide-letter-word beginning with the letter at stem[row], where stem
is the first word continuation of the prefix given by colNode[0][row-1],
which is completely determined by the first nodes in the first row-1
row word nodes.  If we were keeping camdidate colWords at this point,
this letter would (also) be given by colWord[0][row].
Verify that each letter (even or odd col) is the row-th letter in some 
tall-letter word that starts with the letter in the corresponding column
of the previous row, that is, for each of its oddWide letters L[col], 
for (col = 0, 1, ... oddWide-1)
if (rowNode[row-1][col*2]->branch[rowWord[col]] is null)  break; 
if we get whole row, recursively try for next row.  Otherwise, the next
candidate word depends on where the loop broke.
*/

template <typename MapT>
int WordWaffleFinder<MapT>::findEvnRowWantingOddTotal(int haveTall)
{
    assert(haveTall % 2 == 1);
    this->mNowTall = haveTall;
    if (haveTall == this->mWantTall) {
        // Success: the row just added made words of all columns.
        // Record it right now, right here.
        this->mMaxTall = haveTall;
        for (int row = 0; row < haveTall; ++row)
            this->mRowWordsOld[row] = this->mRowWordsNow[row];
        this->mIsLastRectSymSquare = isSymmetricSquare();
        if (this->mIsLastRectSymSquare)
            ++this->mNumSymSquares;
        if (this->incNumFound() != this->getNumToFind() && WordRectSearchMgr<MapT>::getVerbosity() > 1) {
            WordRectPrinter::printRect(this);
            return 0;       // Return 0 to continue the recursion.
        }
        return this->mWantArea;	// Return the area to stop the recursion.
    }
    return findNextEvnRow(haveTall);
}

/**
* Having already found an odd number of rows (haveTall), search for a next,
* even-numbered row.  Don't bother comparing haveTall to wantTall, because
* haveTall is odd and wantTall is even.
*/
template <typename MapT>
int WordWaffleFinder<MapT>::findEvnRowWantingEvnTotal(int haveTall)
{
    assert(haveTall % 2 == 1);
    this->mNowTall = haveTall;
    return findNextEvnRow(haveTall);
}


/** Having an odd number of rows (haveTall), try to add another row,
* which will result in having an even number of rows.
*/
template <typename MapT>
int WordWaffleFinder<MapT>::findNextEvnRow(int haveTall)
{
    if (this->mMaxTall < haveTall) {
        this->mMaxTall = haveTall;
    }

    int prevTall = haveTall - 1;              // prevTall == -1 is ok.
    int lastCol  = this->mOddWide - 1;
    const uchr colLetter = this->mColNodes[0][prevTall]->getLetterAt(haveTall);
    const TrixNode **rowNodes = this->mRowNodes[haveTall], *colBranch;
    const TrixNode  *rowNode  = rowNodes[-1]->getBranchAtIndex(this->rowCharIndex(colLetter));
    while (rowNode != NULL) {
        int             col = rowNode->getDepth() - 1;
        const char *rowWord = rowNode->getStem(); // same result as getFirstWord();
        // For each vertical stem (partial column) that would result from adding this word
        // as the next row, is there a whole word of length wantTall that could complete it?
        // If yes (IFF the implied colNode is non-null), continue; else break and get next word.
        for (;;) {
            rowNodes[col] = rowNodes[col-1]->getBranchAtIndex(this->rowCharIndex(rowWord[col]));
            colBranch = this->mColNodes[col*2][prevTall]->getBranchAtIndex(this->colCharIndex(rowWord[col]));
            if (colBranch == NULL) {
                break;								// No column word; break to get the next row word.
            }
            this->mColNodes[col][haveTall] = colBranch;   // Colums ok so far; add the branch node and continue.
            if (col == lastCol) {
                // For each letter in this row word, we've found that at least 
                // one wantTall-length colun word exists, whose haveTall-length
                // prefix matches the letters in all the rows so far, including 
                // this one.  So try adding another word row.  Since this row
                // is even, the next must be odd.
                this->mRowWordsNow[haveTall] = rowWord;
                int area =  (this->*mOddRowFinder)(haveTall+1);
                if (area != 0) {	// Either the call above found a wordRect (area > 0),
                    return area;    // or it is aborting (another thread beat this one).
                }                   // So return up through the stack.
                break;              // This word failed, so break to get the next word.
            }
            col++;
        }
        // As a candidate for wordRows[haveTall], this word failed at column k,
        // either locally (no completion for column k < wantWide-1) or deeper down
        // in the wordRect (all wantWide columns were ok, some later word row could not be found).
        // Either way, we try the next possible word, which is the first word whose 
        // col-letter stem is greater than this word's.  If such a word exists, we get 
        // it just by following 2 links:  (failed node)->mNextStemNode->mFirstWordNode.
        // (If we weren't using the rowNodes, we'd need to find the parent of the first
        // disqualified node, as does WordTrie::getNextWordNodeAndIndex, but over all, that is slower.)
        rowNode = rowNodes[col]->getNextStemNode();
    }
    return 0;   // Failure: No wantWide x wantTall word rect at the end of this path
}

/**
Even Row (row >= 2): 
Even Col: candidate rowWord[col] must be a branch of colNode[col/2][row-1]
Odd Col:  candidate rowWord[col] must be a branch of colNode[col  ][row-2]
*/
template <typename MapT>
int WordWaffleFinder<MapT>::findOddRowWantingEvnTotal(int haveTall)
{
    assert(haveTall % 2 == 0);
    this->mNowTall = haveTall;
    if (0 < haveTall && this->mWantArea <= WordRectSearchMgr<MapT>::getTrumpingArea()) {
        this->mState = WordRectFinder<MapT>::eAborted;
        return -this->mMaxTall;  // Abort because a wordRect bigger than wantArea has been found
    }
    if (haveTall == this->mWantTall) {
        // Success: the row just added made words of all columns.
        // Record it right now, right here.
        this->mMaxTall = haveTall;
        for (int row = 0; row < haveTall; ++row)
            this->mRowWordsOld[row] = this->mRowWordsNow[row];
        this->mIsLastRectSymSquare = isSymmetricSquare();
        if (this->mIsLastRectSymSquare)
            ++this->mNumSymSquares;
        if (this->incNumFound() != this->getNumToFind() && WordRectSearchMgr<MapT>::getVerbosity() > 1) {
            WordRectPrinter::printRect(this);
            return 0;       // Return 0 to continue the recursion.
        }
        return this->mWantArea;	// Return the area to stop the recursion.
    }
    return findNextOddRow(haveTall);
}

/**
* Having already found an even number of rows (haveTall), search for a next,
* odd-numbered row.  Don't bother comparing haveTall to wantTall, because
* haveTall is even and wantTall is odd.  But do check the max area found
* so far, just because haveTall is even and > 1.
*/
template <typename MapT>
int WordWaffleFinder<MapT>::findOddRowWantingOddTotal(int haveTall)
{
    assert(haveTall % 2 == 0);
    this->mNowTall = haveTall;
    if (0 < haveTall && this->mWantArea <= WordRectSearchMgr<MapT>::getTrumpingArea()) {
        this->mState = WordRectFinder<MapT>::eAborted;
        return -this->mMaxTall;  // Abort because a wordRect bigger than wantArea has been found
    }
    return findNextOddRow(haveTall);
}


/** Having an even number of rows (haveTall), try to add another row
* which will result in having an odd number of rows.
*/
template <typename MapT>
int WordWaffleFinder<MapT>::findNextOddRow(int haveTall)
{
    if (this->mMaxTall < haveTall) {
        this->mMaxTall = haveTall;
    }

    int  tallM1  = haveTall - 1;
    int  tallM2  = haveTall - 2;
    int  lastCol = this->mWantWideM1;
    const uchr colLetter = this->mColNodes[0][tallM1]->getLetterAt(haveTall);
    const TrixNode **rowNodes = this->mRowNodes[haveTall], *colBranch;
    const TrixNode  *rowNode  = rowNodes[-1]->getBranchAtIndex(this->rowCharIndex(colLetter));
    while (rowNode != NULL) {
        int             col = rowNode->getDepth() - 1;
        bool        evenCol = (col % 2 == 0) ? true : false;
        const char *rowWord = rowNode->getStem(); // same result as getFirstWord();
        // For each vertical stem (partial column) that would result from adding this word
        // as the next row, is there a whole word of length wantTall that could complete it?
        // If yes (IFF the implied colNode is non-null), continue; else break and get next word.
        for (;;) {
            rowNodes[col] = rowNodes[col-1]->getBranchAtIndex(this->rowCharIndex(rowWord[col])); // Need this later, so always set it.

            // Get column node as colBranch.  We need colBranch later only if it is non-NULL.
            if (evenCol) {
                colBranch = this->mColNodes[col/2][tallM1]->getBranchAtIndex(this->colCharIndex(rowWord[col]));
            } else {
                colBranch = this->mColNodes[col  ][tallM2]->getBranchAtIndex(this->colCharIndex(rowWord[col]));
            }
            if (colBranch == NULL) {
                break;                              // ...if not, break to get the next word.
            }
            this->mColNodes[col][haveTall] = colBranch;      // ...if so, add the branch node and continue.
            if (col == lastCol) {
                // For each letter in this row word, we've found that at least 
                // one wantTall-length colun word exists, whose haveTall-length
                // prefix matches the letters in all the rows so far, including 
                // this one.  So try adding another word row.  Since this row
                // is odd, the next must be even.
                this->mRowWordsNow[haveTall] = rowWord;
                int area =  (this->*mEvnRowFinder)(haveTall+1);
                if (area != 0) {	// Either the call above found a wordRect (area > 0),
                    return area;    // or it is aborting (another thread beat this one).
                }                   // So return up through the stack.
                break;              // This word failed, so break to get the next word.
            }
            col++;
            evenCol = ! evenCol;
        }
        // As a candidate for wordRows[haveTall], this word failed at column k,
        // either locally (no completion for column k < wantWide-1) or deeper down
        // in the wordRect (all wantWide columns were ok, some later word row could not be found).
        // Either way, we try the next possible word, which is the first word whose 
        // k-letter stem is greater than this word's.  If such a word exists, we get 
        // it just by following 2 links:  (failed node)->mNextStemNode->mFirstWordNode.
        // (If we weren't using the rowNodes, we'd need to find the parent of the first
        // disqualified node, as does WordTrie::getNextWordNodeAndIndex, but over all, that is slower.)
        rowNode = rowNodes[col]->getNextStemNode();
    }
    return 0;   // Failure: No wantWide x wantTall word rect at the end of this path
}
