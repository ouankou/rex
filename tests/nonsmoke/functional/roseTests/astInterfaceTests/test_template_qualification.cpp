// Template qualification regression: non-type parameter names and redundant
// global qualifiers.

#include <tuple>

#include <vector>

template <int N> void foo() {}
template <typename T, int N> struct Array {
  T data[N];
};

namespace ns {
using namespace std;
void bar() {
  vector<int> v;        // Should unparse as "vector<int>"
  tuple<int, double> t; // Should unparse as "tuple<int, double>"
}
} // namespace ns

int main() {
  foo<5>();           // Should unparse as "foo<5>()"
  Array<int, 10> arr; // Should unparse as "Array<int, 10>"
  return 0;
}
