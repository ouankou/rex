// This test code demonstrates the required specialization of "template <> int X<int>::foo()"

#define REQUIRED 1

// ****************************
// Member function form of test
// ****************************

// template function containing member function that would 
// be an error to instantiate with a primative type
template <typename T>
class X
   {
     private:
          T t;

     public:
          void foo()
             {
            // This would be an error if T was a primative type
               t.increment();
             }
   };

#if REQUIRED
// specialization for X::foo() when X is instantiated with an "int" 
// (primative, non class, type).  Without this specialized template 
// function the template instatiation directive would generate an error.
template <> void X<int>::foo()
   {
   }
#endif

