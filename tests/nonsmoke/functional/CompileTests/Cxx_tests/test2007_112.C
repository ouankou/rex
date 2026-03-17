/*
Verify that a derived-class base initializer can still refer to the global
"x" even though the base class also has a member named "x".
*/

int x;

class A {
protected:
  int x;

  explicit A(int x) : x(x) {}
};

class B : public A {
public:
  B() : A(::x) {}
};
