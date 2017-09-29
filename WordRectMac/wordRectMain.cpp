//  wordRectMain.cpp : find largest dense rectangles of words from a dictionary, multi-threaded
//  Sprax Lines, July 2010
//
//  Word Rectangle (ITA Software hiring puzzle)
//  
//  Write a program to find the largest possible rectangle of letters such 
//  that every row forms a word (reading left to right) and every column 
//  forms a word (reading top to bottom). Words should appear in this 
//  dictionary: WORD.LST (1.66MB). Heuristic solutions that may not always 
//  produce a provably optimal rectangle will be accepted: seek a reasonable 
//  trade-off of efficiency and optimality. 

//  TODO: Put previously static info in base class accessors (read-only)
//  TODO: remove WordRectSearchMgr.hpp from WordTrie.hpp/cpp`

#include "wordPlatform.h"
#include "wordRectMain.hpp"
#include "WordRectSearchExec.hpp"

// default values:
static const char *sProgramName = "wordRectMulti";
static const char *defDictFile  = "/Users/sprax/Text/Lex/Enu/enuWords.txt";
static const uint  defMinTall   =   2;
static const uint  defMaxTall   =  48;
static const uint  defMinArea   =   4;
static const uint  defMaxArea   = defMaxTall*defMaxTall;
static const uint  defMinChrC   = 100;
static const uint  defNumEach   =   1;
static const uint  defNumTotal  =  0;

#define MSG_SIZE           80
#define MAX_ARG_UINT_COUNT  7

static void usage(int argc, const char* argv[], const char *reason=NULL)
{
    printf( "\n  Usage: %s [-ahilmoqswz] [-dDictionary] [-vVerbosity] [minArea minHeight maxHeight maxArea minCharC numEach numTotal]\n", sProgramName);
    printf( "All arguments are optional.  If they conflict (q & v, or m & w), the last one wins.\n"
        "  The defaults are the minimum and maximum values the program accepts:\n"
        "    %d %d %d %d.\n", defMinArea, defMinTall, defMaxTall, defMaxArea);
    printf( "This tells the program to search for word rectangles of the dimensions between\n"
        "%d x %d (or area %d) and %d x %d (or area %d),\n"
        , defMinTall, defMinTall, defMinArea, defMaxTall, defMaxTall, defMaxArea);
    printf( "using linked tries to check column words to find the next candidate for each word row.\n\n"
        "The letter options are:\n");
    printf( "-a   Use alternative search algorithm (might be faster).\n");
    printf( "-d   Use the next argument for the dictionary file name (instead of %s).\n", defDictFile);
    printf( "-e   Try to find at least one rect for each eligible wide-tall pair.\n");
    printf( "-h   Show this help message.\n");
    printf( "-i   Order the search for word rectangles in increasing order (slower but fun).\n"
        "     The default is to order the search by decreasing area, so it can end when the"
        "     first (largest) is found.\n");
    printf( "-l   Find word lattices instead of word rectangles\n");
    printf( "-m   Use std::map::upper_bound instead of tries to find rows (rects only, much slower).\n");
    printf( "-o   Odd dimensions only: search only for rectangles of odd widths and heights.\n");
    printf( "-q   Quiet mode (Verbosity=1).  Omit most of the messages about finder-threads.\n");
    printf( "-s   Single-threaded mode; not using a pool of parallel worker threads\n");
    printf( "-s   Single-threaded mode; not using a pool of parallel worker threads\n");
    printf( "-vN  Verbosity level = N [0 - 9].\n");
    printf( "-z   Find ALL MAXIMAL word rectangles in the specified range, instead of only\n"
        "     one for each possibly maximal total area.  If a larger word rect is found,\n"
        "     any ongoing searches for smaller rects will be aborted as per default -- \n"
        "     that is, unless the option AbortIfTrumped is set off.\n"
        "     Either way, this option may result in long run times.\n");
    printf( "\nExample: %s -seiv5wa -dEnuNamesWords.txt 25 5 6 40 100 2\n\n", sProgramName);
    printf( "Exiting from this command:\n    ");
    for (int j = 0; j < argc; j++) {
        printf("%s ", argv[j]);
    }
    printf("\n");
    if (reason) {
        printf("Reason: %s\n", reason);
    }
    exit(0);
}

//#ifdef _MBCS
int main(int argc, char* argv[])
{   
    return wordRectMain(argc, (const char **)argv);
}
//#endif

int wordRectMain(int argc, const char* argv[])
{   
    // set/get default args, check actual args and either show usage or call wordRect
    uint minWordLength = 2, maxWordLength = 36;  // command line options can replace these.
    uint minArea, minTall, maxTall, maxArea, minChrC, numEach, numTot;
    sint tmpSint = 0;
    uint argUintC = 0, argUintV[MAX_ARG_UINT_COUNT] = { 0, };
    uint verbosity = 2;
    uint managerFlags = WordRectSearchExec::eDefaultZero | WordRectSearchExec::eAbortIfTrumped;

    if (argv[0])
        sProgramName  = argv[0];
    const char *dictFileName = defDictFile;
    char reason[MSG_SIZE];
    for (int j = 1; j < argc; j++) {   // [-letters] options?
        const char *pc = argv[j];
        if (*pc == '-') {
            for ( ++pc ; ; ++pc ) {
                switch (*pc) {
                    case '\0':
                        goto NEXT_ARG;
                    case 'a' :
                        managerFlags |= WordRectSearchExec::eUseAltAlg;
                        break;
                    case 'c' :
                        // TODO: use compact array map
                        break;
                    case 'd' :
                        if ( *(++pc) == '\0') {
                            sprintf_safe(reason, MSG_SIZE, "-d option not followed immediately by dictionary file-spec");
                            usage(argc, argv, reason);
                        }
                        dictFileName = pc;
                        goto NEXT_ARG;
                        break;
                    case 'e' :
                        managerFlags &= ~WordRectSearchExec::eAbortIfTrumped;
                        break;
                    case 'h' : 
                        usage(argc, argv, "help was requested (-h)");
                        break;
                    case 'i' :
                        managerFlags |= WordRectSearchExec::eIncreasingSize;
                        break;
                    case 'l' :
                        managerFlags |= WordRectSearchExec::eFindLattices;
                        break;
                    case 'm' :
                        managerFlags |= WordRectSearchExec::eUseMaps;
                        break;
                    case 'o' :
                        managerFlags |= WordRectSearchExec::eOnlyOddDims;
                        break;
                    case 'q' :
                        verbosity = 1;
                        break;
                    case 's' :
                        managerFlags |= WordRectSearchExec::eSingleThreaded;
                        break;
                    case 't' :
                        managerFlags |= WordRectSearchExec::eTryTracNodes;
                        break;
                    case 'v' :
                        tmpSint = atoi(++pc);
                        if (1 <= tmpSint && tmpSint <= 9) {
                            verbosity = tmpSint;
                        } else {
                            sprintf_safe(reason, MSG_SIZE, "-v option without a number [1-9]");
                            usage(argc, argv, reason);
                        }
                        break;
                    case 'w' :
                        managerFlags |= WordRectSearchExec::eFindWaffles;
                        break;
                    case 'x' :
                        tmpSint = atoi(++pc);
                        if ((sint)minWordLength <= tmpSint && tmpSint < 256) {
                            maxWordLength = tmpSint;
                        } else {
                            sprintf_safe(reason, MSG_SIZE, "-x option not followed immediately by max word length < 256");
                            usage(argc, argv, reason);
                        }
                        goto NEXT_ARG;
                        break;
                    case 'z' :
                        numEach = 0;
                        break;
                    default:
                        sprintf_safe(reason, MSG_SIZE, "got unknown option: %c", *pc);
                        usage(argc, argv, reason);
                        break;
                }
            }
        } else {
            tmpSint = atoi(pc);
            if (tmpSint < 2) {
                sprintf_safe(reason, MSG_SIZE, "Numeric args must be at least %d, but %c => %d", defMinTall, pc, tmpSint);
                usage(argc, argv, reason);
                break;
            }
            if (argUintC < MAX_ARG_UINT_COUNT)
                argUintV[argUintC++] = tmpSint;
            else {
                sprintf_safe(reason, MSG_SIZE, "Too many arguments at <%s>", pc);
                usage(argc, argv, reason);
            }
        }
    NEXT_ARG:	continue;
    }
    // Word lattices must have odd dims
    if (managerFlags &  WordRectSearchExec::eFindLattices)
        managerFlags |= WordRectSearchExec::eOnlyOddDims;

    minArea = argUintC > 0 ? argUintV[0] : defMinArea;
    minTall = argUintC > 1 ? argUintV[1] : defMinTall;
    maxTall = argUintC > 2 ? argUintV[2] : defMaxTall;
    maxArea = argUintC > 3 ? argUintV[3] : defMaxArea;
    minChrC = argUintC > 4 ? argUintV[4] : defMinChrC;
    numEach = argUintC > 5 ? argUintV[5] : defNumEach;
    numTot  = argUintC > 6 ? argUintV[6] : defNumTotal;

    const char *mapsOrTries = managerFlags & WordRectSearchExec::eUseMaps ? "maps" : "tries";
    printf("%s: using %s,\n", sProgramName, mapsOrTries);
    printf("   min & max Area: %d %d, min & max Height: %d %d.\n", minArea, maxArea, minTall, maxTall);
    if (minArea > maxArea || minTall > maxTall) {
        usage(argc, argv, "minArea > maxArea or minHeight > maxHeight");
    }

#ifdef _DEBUG
    void test_Mult();
    test_Mult();
    void test_Mem();
    test_Mem();
#endif
    
    WordRectSearchExec& searchExec = WordRectSearchExec::getInstance();
    searchExec.setOptions(managerFlags, minChrC, minWordLength, maxWordLength, verbosity);
    searchExec.startupSearchManager(dictFileName, minArea, minTall, maxTall, maxArea, numEach, numTot);
    searchExec.destroySearchManager();
    WordRectSearchExec::deleteInstance();
    return 0;
}
