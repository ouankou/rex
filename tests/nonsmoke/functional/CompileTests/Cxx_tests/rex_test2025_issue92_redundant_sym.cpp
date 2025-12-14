// Issue 92: Redundant Symbol Removal Warnings
// Verify that using common STL templates does not emit
// "Redundant symbol removed...from symbol table" diagnostics.

#include <vector>

static void test() {
  std::vector<int> v;
  v.push_back(5);
}

int main() {
  test();
  return 0;
}
