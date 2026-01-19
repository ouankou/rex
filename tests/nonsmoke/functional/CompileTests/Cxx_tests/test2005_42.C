
#include <functional>

#include <algorithm>

#include <vector>

void foo() {
  std::greater<int> X();

  std::bind2nd(std::greater<int>(), 42);
}
