// This file tests that your C++ compiler can compile and link C++ executables
// that use the C++ standard library but don't use any other library that might
// automatically bring in the C++ standard library.  If you get link errors for
// things like std::cout then double check that you're actually using a C++
// compiler and not a C compiler.  Two ways to see which compiler is being used
// are:
//     1. Run "cmake --build <build> --verbose" (or "make VERBOSE=1" for
//     Makefile generators).
//     2. Run "<build>/src/rose-config cxx" where <build> is the top of your
//     build tree (or run "rose-config cxx" if ROSE is installed).

#include <iostream>

int main() {
  std::cout << "3."
               "141592653589793238462643383279502884197169399375105820974944592"
               "307816406286208998628034825342117068\n";
}
