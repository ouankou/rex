// Test code for overloaded member functions of a templated class where at least 
// one is a template function (as opposed to a non-template member function).

// this is a class template
template <typename T>
class X
   {
     public:
       // This is an overloaded non-template member function
       void foobar(T t) {};
       // This is an overloaded template member function
       template <typename S> void foobar(S t) {};
   };

   // For gnu 4.1.2, if we generate the member function template specialization
   // then we have to generate the class template specialization as well.
   // It appears that the only exception is for constructors, I think.

   void foo() {
     X<int> a;

     a.foobar(1);
  // a.foobar(1,2);
     a.foobar(3.14159265);
   }


// Make this compile and link
int main()
   {
     return 0;
   }

