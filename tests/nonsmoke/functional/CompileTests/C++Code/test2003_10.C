// This test is placed into test2003_08.C with the class defined in test2003_08.h

int x;

typedef struct Ctag
   {
     struct Ctag* next;
   } C;

// The initializer fails to unparse correctly in this case
   C array[1] = {(Ctag *)0};
