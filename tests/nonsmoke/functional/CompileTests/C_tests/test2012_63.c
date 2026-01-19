#include "test2012_63.h"

void foobar()
   {
  // Unparses incorrectly as: "register const struct rlalink { int x; } *rlp;" if the defining declaration is in the same file, and
  // unparses incorrectly as: "register const struct rlalink {} *rlp;" if the defining declaration is in a different (header) file.
     register const struct rlalink *rlp;

     int a;
     a = rlp->x;
   }
