#include "inputmoveDeclarationToInnermostScope_test2015_13.h"

namespace std {

  template<typename _CharT>
    class numpunct 
    {
  public:
    string grouping() const {
      string s;
      return s;
    }
    };

}

  void foo()
     {
       int x;
       if (1)
          {
            x = 4;
          }
     }
