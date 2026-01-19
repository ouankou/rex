struct A
   {
     int i;
     A(int);
   };

struct X_
   {
     struct Y_
        {
          int i, j;
          int A::* pm;
          int A::*const cpm;

       // The bug is an extra ")"
       // Unparsed as: inline Y_(int a,int b,int A::*q)) : i(a), j(b), pm(q), cpm(q)
       // Should be:          Y_(int a, int b, int A::* q) : i(a), j(b), pm(q), cpm(q) { }
          Y_(int a, int b, int A::* q) : i(a), j(b), pm(q), cpm(q) { }
        };
   };
