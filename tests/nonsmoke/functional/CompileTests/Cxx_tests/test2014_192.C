template <typename T>
class X
   {
public:
  // Note that template and non-template class declarations are not normalized
  // to be forward declarations.
  template <typename S> class A {
  public:
    S x;
    A(int x) { int a_value; }
  };

  X() { int x_value; }
   };

void foo()
   {
      X<int> a;
      X<int>::A<int> b(4);
   }

