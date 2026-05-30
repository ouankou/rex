// Regression for qualifying a name from an unnamed namespace nested inside
// another namespace.

namespace sample {
namespace lambda {

namespace {
// These are constants types and need to be initialised
typedef int Integer;
int _1;
} // namespace

} // namespace lambda
} // namespace sample

int _1;

void foo() {
  _1 = 1;
  sample::lambda::_1 = 1;

  int x;
  sample::lambda::Integer y;
}
