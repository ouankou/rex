// This is called: Template non-type parameter overload
// This si discussed in: http://stackoverflow.com/questions/17313649/how-can-i-distinguish-overloads-of-templates-with-non-type-parameters

// DQ (8/6/2013): I think that niether of these template functions
// can be called because they are ambigous at compile-time.

// This is the template function for member functions.
template <unsigned int T> void test() {}

// This is the template function for member data.
template <signed long T> void test() {}

void foo()
   {
     const unsigned int x = 1;
     const signed long y  = -1;
   }
