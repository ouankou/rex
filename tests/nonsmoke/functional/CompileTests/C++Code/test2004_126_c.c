int
f1 ( enum {FALSE, TRUE} b )
   {
     return FALSE;
   }

enum {FALSE, TRUE}
f2 ( enum {FALSE, TRUE} b )
   {
     return FALSE;
   }

enum boolean1 {FALSE1, TRUE1}
f3 ( enum {FALSE1, TRUE1} b )
   {
     return FALSE1;
   }

enum boolean2 {FALSE2, TRUE2}
f4 ( enum boolean3 {FALSE3, TRUE3} b )
   {
     return FALSE3;
   }

void foo8 ( struct { int x; } s )
   {
   }
