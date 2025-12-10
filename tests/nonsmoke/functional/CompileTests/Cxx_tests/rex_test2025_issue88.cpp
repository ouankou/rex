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
