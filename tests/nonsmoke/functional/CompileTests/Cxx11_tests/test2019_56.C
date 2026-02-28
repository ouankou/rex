
class A {
public:
  virtual int foobar();
  int i;

public:
  int foo();
  A();
};

class B : public A {
public:
  // C++11 requires a using-declaration.
  using A::foo;
  B();

private:
  int foobar();
};

void myFunction() {
  B o;
  o.foo();
}
