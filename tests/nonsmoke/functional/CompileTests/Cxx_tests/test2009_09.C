// struct b;

struct a
   {
  // Need to get the scope of the non-defining declaration to "b" set properly to be SgGlobal.
     struct b *bp;
   };

struct b;
struct b;
int abcdefg;

struct b {};
