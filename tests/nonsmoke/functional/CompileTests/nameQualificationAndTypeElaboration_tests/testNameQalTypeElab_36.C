// number #36

// Example of unnamed namespace.
// Use of "_1" used to unparse to rex::lambda_test::_1

namespace rex {
namespace lambda_test {

#if 0
namespace X {
  // These are constants types and need to be initialised
  typedef int Integer;
  int _1;
} // unnamed
#endif
   
namespace {
  // These are constants types and need to be initialised
  typedef int Integer;
  int _1;
  } // namespace

  } // namespace lambda_test
  } // namespace rex

int _1;

void foo()
   {
     _1 = 1;
     //   rex::lambda_test::X::_1 = 1;
     rex::lambda_test::_1 = 1;

     int x;
     //   rex::lambda_test::X::Integer y1;
     rex::lambda_test::Integer y2;
   }
