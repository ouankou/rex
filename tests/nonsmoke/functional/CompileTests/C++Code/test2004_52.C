
// Test code extracted from streambuf.h which shows case where 
// the new AST test suggested by Jeremiah fails.  The type of
// the function in the function declaration is not a function type!
// Does this make sense???

int globalTest() {}

class X
   {
     public:
       int test() { return globalTest(); }
   };


float call () 
   {
     X x;
     x.test();
     return 0.0;
   }

