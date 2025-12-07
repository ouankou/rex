// test2025_issue83_collisions.cpp
// Tests that multiple explicit specializations of the same template
// do not collide in the symbol table.

template <typename T> struct A {
  T val;
};

// First specialization
template <> struct A<int> {
  int x;
};

// Second specialization - should have a unique symbol
template <> struct A<double> {
  double y;
};

void foo() {
  A<int> a;
  a.x = 1;

  A<double> b;
  b.y = 2.0;
}
