template <typename T>
class X
   {
public:
  // Note that template and non-template class declarations are not normalized
  // to be forward declarations.
  template <typename S> class A {
  public:
    S x;
    A();
  };

  X() { int x_value; }
   };

// If we treat this as name qualification then it has to be seperated into two parts (to leave space for the return type).
// It might be better to NOT treat this as name qualification and form the qualified name directly with the template 
// function name (and parameter list).  Template names are then treated differenty than non-template function names.
   template <typename T> template <typename S> X<T>::A<S>::A() { int a_value; }

   void foo() {
     X<int> a;
     X<int>::A<int> b;
   }

