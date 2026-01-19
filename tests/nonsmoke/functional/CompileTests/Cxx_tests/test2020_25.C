namespace X
   {
     int i = 43;
     namespace N {
     void f() {
       using namespace X;
       i = 42;
     }
     } // namespace N
   }


void foobar()
   {
     namespace A = X::N;
  // A::X::f();
  // A::N::f();
     A::f();
   }
