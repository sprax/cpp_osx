
// wordTypes.h : Common typedefs and macros for my "wordy" projects
// 
// Sprax Lines,  September 2012

#ifndef wordTypes_h
#define wordTypes_h

typedef   signed char schr;	// But avoid using signed char for letters
typedef unsigned char uchr;
typedef   signed int  sint;
typedef unsigned int  uint;

#ifdef _DEBUG
#include <assert.h>
#else
#define NDEBUG          1
#define assert(e_x_p_r)  ((void)0)
#endif

#endif  // wordTypes_h