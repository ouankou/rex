class A {
public:
  template <typename T> friend void foo(T t) {
    // Body
    int x = 0;
  }
};

void bar() {
  A a;
  foo(10);
}
