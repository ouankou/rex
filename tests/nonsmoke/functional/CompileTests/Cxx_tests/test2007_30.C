void f();

struct A {
  static void m() { f(); }
  friend void f();
};
