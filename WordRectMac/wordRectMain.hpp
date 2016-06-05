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
//  tradeoff of efficiency and optimality. 

#ifndef wordRectMain_hpp
#define wordRectMain_hpp

#include <time.h>

int     wordRectMain(int argc, const char* argv[]);

#endif // wordRectMain_hpp