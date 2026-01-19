#include <vector>
using namespace std;

class xxx {};

class A {
private:
  class xxx {};

public:
  // typedef xxx referenced_t;
  typedef std::vector<class xxx *> referenced_t;
};

// extern "C" {

void foo();

A::referenced_t *foobar() {
  A::referenced_t X;
  A::referenced_t *Y;

  // This is unparsed as: "A::xxx()" which is not visible.
  // return A::referenced_t();
  return 0L;
}
// }
