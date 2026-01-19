#include <vector>

void foo() {
  for (int i = 5, j = 7; i != j; i++) {
  }

  std::vector<int> v;
  for (std::vector<int>::iterator nnnn = v.begin(), mmmm = v.end();
       nnnn != mmmm; nnnn++) {
  }
}
