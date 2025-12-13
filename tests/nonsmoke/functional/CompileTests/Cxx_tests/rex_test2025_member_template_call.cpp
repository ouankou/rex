// Explicit template arguments on member DeclRefExpr should be preserved.

struct A {
  template <int N> static void foo() {}

  void bar() {
    foo<5>();
    foo<10>();
  }
};

int main() {
  A a;
  a.bar();
  return 0;
}
