// WordWaffleFinder.cpp : find word rectangle of the specified dimensions 
// Sprax Lines, July 2010
////////////////////////////////////////////////////////////////////////////////

#include "WordWaffleFinder.hpp"
#include "WordRectPrinter.hpp"
#include "WordRectSearchMgr.hpp"

template <typename MapT>
void WordWaffleFinder<MapT>::initRowsAndCols()
{
    if (this->mRowWordsNow != NULL)
        return;

    this->mRowWordsNow = (const char **)      new char*[this->mWantTall];
    this->mRowWordsOld = (const char **)      new char*[this->mWantTall];
    this->mRowNodes    = (const TrixNode ***) new TrixNode**[this->mWantTall];
    this->mColNodes    = (const TrixNode ***) new TrixNode**[this->mWantWide];

    // For each row, make rowNodes[row] point to the start of the word row,
    // which is one pointer address *after* the row's raw memory start, 
    // and make rowNodes[row][-1] point to the mRowTrie root.  This memory
    // offset allows word-node indexing to agree with word-character indexing.
    int wideP1 = this->mWantWide + 1;
    int tallP1 = this->mWantTall + 1;
    this->mRowNodesMem = (const TrixNode  **) new TrixNode*[wideP1 * this->mWantTall];
    // Init the even row node arrays.  
    for (int row = 0; row < this->mWantTall; row += 2) {
        this->mRowNodes[row]     = &this->mRowNodesMem[row * wideP1 + 1];
        this->mRowNodes[row][-1] =  this->mRowTrie.getRoot();
    }
    // Init the odd row node arrays.  
    for (int row = 1; row < this->mWantTall; row += 2) {
        this->mRowNodes[row]     = &this->mRowNodesMem[row * wideP1 + 1];
        this->mRowNodes[row][-1] =  this->mOddRowTrie.getRoot();
    }

    // For each column, make colNodes[col] point to the start of the column,
    // which is one pointer address *after* the column's raw memory start, 
    // and make colNodes[col][-1] point to the mColTrie root.
    this->mColNodesMem = (const TrixNode  **) new TrixNode*[this->mWantWide * tallP1];
    // Re-init the even col node arrays.
    for (int col = 0; col < this->mWantWide; col += 2) {
        this->mColNodes[col]     = &this->mColNodesMem[col * tallP1 + 1];
        this->mColNodes[col][-1] = this->mColTrie.getRoot();
    }
    // Re-init the odd col node arrays.
    for (int col = 1; col < this->mWantWide; col += 2) {
        this->mColNodes[col]     = &this->mColNodesMem[col * tallP1 + 1];
        this->mColNodes[col][-1] = this->mOddColTrie.getRoot();
    }
}


template <typename MapT>
int WordWaffleFinder<MapT>::findWordRows()
{

    if (this->mUseAltA)
        return findWordRowsUsingTrieLinksA(0);
    return     findWordRowsUsingTrieLinks(0);
}


template <typename MapT>
int WordWaffleFinder<MapT>::findWordRowsUsingTrieLinks(int haveTall)
{
    this->mNowTall = haveTall;
    if (haveTall > 1 && this->mWantArea <= WordRectSearchMgr<MapT>::getTrumpingArea()) {
        this->mState = WordRectFinder<MapT>::eAborted;
        return -this->mMaxTall;  // Abort because a wordRect bigger than wantArea has been found
    }

    bool evenRow = true;
    bool evenCol = true;
    int prevTall = haveTall - 1;              // prevTall == -1 is ok.
    int lastCol  = this->mWantWideM1;
    const TrixNode *rowWordNode = NULL;
    if (haveTall == 0) {
        rowWordNode = this->mRowTrie.getFirstWordNode();
    } else if (haveTall < this->mWantTall) {
        if (haveTall % 2 == 0) {
            rowWordNode = this->mEvnRowTrie.getFirstWordNodeFromIndex(this->rowCharIndex(this->mColNodes[0][prevTall]->getLetterAt(haveTall)));
        } else {
            rowWordNode = this->mOddRowTrie.getFirstWordNodeFromIndex(this->rowCharIndex(this->mColNodes[0][prevTall]->getLetterAt(haveTall)));
            evenRow = false;
            lastCol = mOddWide - 1;
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
            WordRectPrinter::printRect(this);
        }
        return this->mWantArea;                   // Return the area
    }

    /**
    How to construct and verify rows, i.e. get the next candidate row word and
    either reject it or complete its translation into row and column nodes.
    Row 0: The first candidate word is the first wide-letter word in the dictionary.
    Verify that each even-column letter begins some tall-letter word,
    and that each odd-column letter begins some oddTall-letter word.
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
    Even Row (row >= 2): 
    Even Col: candidate rowWord[col] must be a branch of colNode[col/2][row-1]
    Odd Col:  candidate rowWord[col] must be a branch of colNode[col][row-2]
    */
    const char *rowWord;
    const TrixNode **rowNodes = this->mRowNodes[haveTall], *colBranch, *nextStem;
    for (int col = 0; rowWordNode != NULL; ) {
        rowWord = rowWordNode->getStem();  // Not getWord(), because we already know that rowWordNode is a word-node
        for (;;) {
            // For each stem (partial column) that would result from adding this word as the next row,
            // is there a word of length wantTall that could complete it?
            // We'll need this later, so always set it.
            rowNodes[col] = rowNodes[col-1]->getBranchAtIndex(this->rowCharIndex(rowWord[col]));

            //  We'll need colBranch later only if it is non-NULL.
            if (evenRow) {
                if (haveTall == 0) {
                    colBranch = this->mColNodes[col][-1]->getBranchAtIndex(this->colCharIndex(rowWord[col]));
                } else if (evenCol) {
                    colBranch = this->mColNodes[col/2][prevTall]->getBranchAtIndex(this->colCharIndex(rowWord[col]));
                } else {
                    colBranch = this->mColNodes[col][prevTall-1]->getBranchAtIndex(this->colCharIndex(rowWord[col]));
                }
            } else {                
                colBranch = this->mColNodes[col*2][prevTall]->getBranchAtIndex(this->colCharIndex(rowWord[col]));
            }
            if (colBranch == NULL) {
                break;                              // ...if not, break to get the next word.
            }
            this->mColNodes[col][haveTall] = colBranch;      // ...if so, add the branch node and continue.
            if (col == lastCol) {
                // At least one wantTall-length word exists to complete each column, including this row,
                this->mRowWordsNow[haveTall] = rowWord;                                 // so try adding another row.
                int area =  findWordRowsUsingTrieLinks(haveTall+1);
                if (area < 0 || (area > 0 && this->getNumFound() == this->getNumToFind())) { // Either this call found a wordRect (area > 0),
                    return area;                      // or it is aborting (because another thread found one bigger
                }                                   // than this one's wantArea); so return up through the stack
                break;                              // This word failed, so break to get the next word.
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
        assert(rowNodes[col] != NULL);
        nextStem = rowNodes[col]->getNextStemNode();
        if (nextStem == NULL) {
            return 0;   // We've tried all possible word stems for the partial columns we got, so return. 
        }
        col = nextStem->getDepth() - 1;
        evenCol = (col % 2 == 0) ? true : false;
        rowWordNode = nextStem->getFirstWordNode();
    }
    return 0;   // Failure: No wantWide x wantTall word rect at the end of this path
}



template <typename MapT>
bool WordWaffleFinder<MapT>::isSymmetricSquare() const
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
void WordWaffleFinder<MapT>::printWordRows(const char *wordRows[], int haveTall) const
{
    for (int row = 0; row < haveTall; ++row) {
        for (const char *pc = wordRows[row]; *pc != '\0'; pc++) {
            printf(" %c", *pc);
        }
        printf("\n");
        if (++row == haveTall)
            break;
        for (const char *pc = wordRows[row]; *pc != '\0'; pc++) {
            printf(" %c  ", *pc);
        }
        printf("\n");
    }
    /*   printf("\n");
    WordRectFinder::printWordRows(wordRows, haveTall);
    printf("\n");*/
}

/*
FOUND  19 *  3  Word Rect, area  57 in  3 seconds in thread 0 (SS/WR 0 / 1):
A C C O M M O D A T I V E N E S S E S
A   A   A   A   C   C   G   L   I   I
H Y P O P A R A T H Y R O I D I S M S
TRY    25 *  3 because  75 >   57 (thread  2 began after  3 seconds)
End    23 *  3 (0) at 29480 seconds; Success: Finished with 1 rects.
FOUND  23 *  3  Word Rect, area  69 in 29480 seconds in thread 2 (SS/WR 0 / 1):
C A R B O X Y M E T H Y L C E L L U L O S E S
O   E   B   E   E   A   A   R   E   E   E   E
O V E R I N T E L L E C T U A L I Z A T I O N
End    21 *  3 (1) at 29480 seconds; Aborted, 0 rects, because -2 <= 69.
TRY    27 *  3 because  81 >   69 (thread  1 began after 29480 seconds)
Skip    5 *  5 because  25 <=  69 (area already found)
Skip    7 *  5 because  35 <=  69 (area already found)
Skip    9 *  5 because  45 <=  69 (area already found)
Skip   11 *  5 because  55 <=  69 (area already found)
Skip   13 *  5 because  65 <=  69 (area already found)
TRY    15 *  5 because  75 >   69 (thread  2 began after 29480 seconds)


FOUND   9 *  7  Word Rect, area  63 in 1279 seconds in thread 5 (SS/WR 0 / 1):
A B U N D A N C E
S   P   E   A   N
S U B A R C T I C
A   U   N   T   Y
G R I M I N E S S
A   L   E   R   T
I N T E R E S T S

*/
