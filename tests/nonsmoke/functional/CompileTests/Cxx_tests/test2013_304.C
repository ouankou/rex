// This is the template function for member functions.
template <int T> void test() {}

void foo() {
  constexpr int x = 1;

  test<x>();
}
