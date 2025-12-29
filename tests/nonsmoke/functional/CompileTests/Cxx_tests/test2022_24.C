// Regression test for base-class using-declaration handling.

// Declaration of template class A
class A
   {
   };

class B
   : public A
   {
  // DQ: fails on this line building a using declaration for a base-class
  // In earlier behavior this was treated as a type and then resolved to be a
  // class. In newer behavior it is treated as a base-class.
  using A::A;
   };
