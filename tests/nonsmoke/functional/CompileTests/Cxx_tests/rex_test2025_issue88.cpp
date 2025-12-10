// Test Issue 88: Using Directive Crash
// Expected: using namespace std should be translated
// Actual: ASSERTION FAILURE crash (sometimes)

#include <new>

namespace ns {
using namespace std; // This can cause crash

void bar() {
  // vector usage removed
}
} // namespace ns

int main() {
  ns::bar();
  return 0;
}
