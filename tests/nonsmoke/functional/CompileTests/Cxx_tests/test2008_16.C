// This test code demonstrates the use of the asm command to
// mix assembly language with C or C++ code.

// It is also demonstrates a bug since it does not unparse correctly
// even though the result compiles.

// Typical use of asm commnd: asm("assembly instruction here");

// #include <stdio.h>
// #include <stdlib.h>

void
foo()
   {
	  long number = 0;
	  unsigned position = 0;

// DQ (2/20/2010): This is a error for g++ 4.x compilers (at least g++ 4.2).
#if (__GNUC__ >= 3)
#else
  // This will be unparsed as: asm volatile ("bsr %1, %0" : "=r" (position) : "r" (number));
     asm ("bsr %1, %0" : "=r" (position) : "r" (number));
#endif
   }

