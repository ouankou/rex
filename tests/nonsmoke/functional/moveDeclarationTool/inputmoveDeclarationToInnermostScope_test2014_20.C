// #include <iostream>
// #include <stdio.h>

#include <string>

// namespace std __attribute__ ((__visibility__ ("default"))) {
namespace std {

  template<typename _CharT>
    class numpunct 
    {
  public:
    string grouping() const { return ""; }
    };

}


#define HAVE_VALUE

#ifdef HAVE_VALUE

namespace X {

  void foo()
     {
       int x;
       if (1)
          {
            x = 4;
          }
     }

}  // closing brace for namespace statement

#endif
