// This is a problem when compiling using the "-m32" option.
// ROSE does not interprete this options to change the sizes of 
// fundamental types such as: int and size_t.

// This problem is demonstrated in the support for Valgrind.

#include<stddef.h>

// The use of size_t requs that it be defined above.
typedef size_t UInt;
typedef size_t SizeT;

typedef 
   struct {
      UInt  state:2;    // Reachedness.
      UInt  pending:1;  // Scan pending.  
      union {
         SizeT indirect_szB : (sizeof(SizeT)*8)-3; // If Unreached, how many bytes
                                                   //   are unreachable from here.
         SizeT  clique :  (sizeof(SizeT)*8)-3;      // if IndirectLeak, clique leader
                                                   // to which it belongs.
      } IorC;
   } 
   LC_Extra;


void foo()
   {
     int a = sizeof(size_t);
     struct X
     {
       int a : sizeof(size_t);
     };
   }
