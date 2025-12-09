// tests/nonsmoke/functional/CompileTests/Cxx_tests/test2025_issue84_friend_template_in_class.C
template <typename T> void foo(T);

struct S {
  template <typename T> friend void foo(T) {}
};

void bar() {
  foo(1); // Should link to definition in S
}
