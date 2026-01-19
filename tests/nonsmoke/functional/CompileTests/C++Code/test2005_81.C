#include "test2005_81.h"

// GNU g++ does not allow the class to be specified with a qualified name but
// only with a namespace of the same name using namespace std; // a using
// directive does not help! ALSO, g++ version 3.4.x does not allow the class
// name to be qualified! while g++ version 3.3.x does allow it to be qualified
// (likely an error).
template <> struct std::X<int> {
  float x;
};

// See if specialization of templated functions are just as much of a problem
template <> void std::foobar(float t) { float x; };

// using namespace X;
void foo() {
  std::X<int> x;
  std::foobar(2.0);
}
