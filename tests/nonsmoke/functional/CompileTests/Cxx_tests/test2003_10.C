// This test is placed into test2003_08.C with the class defined in test2003_08.h

int x;

typedef struct Ctag
   {
     struct Ctag* next;
   } C;

// The initializer fails to unparse correctly in this case
// DQ (8/15/2006): C language rules require the "struct" while C++ does not.
#if __cplusplus
   C array[1] = { (Ctag*) 0 };
#else
   C array[1] = { (struct Ctag*) 0 };
#endif
