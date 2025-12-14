// Issue 93: Static Assert Declaration Order
// Verify that class-scope static_assert preserves source order relative to
// member declarations.

struct Foo {
  static_assert(true, "member static_assert");
  int x;
};

int main() {
  static_assert(true, "always passes");
  Foo f;
  (void)f;
  return 0;
}
