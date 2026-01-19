// test2007_51.C

void f()
   {
  // union { int i; } x;      // works
  // { union { int i; } x; }; // works
  // ({ union { int i; } x; });
  // ({ struct { int i; } x; }); // fails
  // { struct { int i; } x; }
     ({ struct { int i; } x; });
   }
