// This is the template function for member functions.
template <int T> void test() {}

void foo()
   {
     const int    x = 1;
     const double y = -1.0;

     test<x>();

  // This is not allowed for GNU g++ version 6.x
  // #if __cplusplus < 201103L
#if (__GNUC__ < 6)
     test<y>();
#endif
   }

