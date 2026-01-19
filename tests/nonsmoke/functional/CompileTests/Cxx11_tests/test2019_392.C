struct A {};

struct X
   {
     struct Y {};

  // int Y::* *p2;
   };

   void foobar() { int X::Y::*X::Y::**p3; }
