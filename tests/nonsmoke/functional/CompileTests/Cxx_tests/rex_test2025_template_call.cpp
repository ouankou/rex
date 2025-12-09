// Template function call should preserve explicit template arguments

template <int N> void foo() {}

int main() {
  foo<5>();
  foo<10>();
  return 0;
}
