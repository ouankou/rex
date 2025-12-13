// Declaration chain regression test (Issue #91)
// Expected: No warnings matching "firstNondefiningDeclaration == NULL"

#include <vector>

static void test() {
  std::vector<int> v;
  v.push_back(5);
}

int main() {
  test();
  return 0;
}
