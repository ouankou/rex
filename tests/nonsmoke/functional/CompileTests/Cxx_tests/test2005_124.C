
class A {
public:
  A();
  A(int x);
  A(float x, float y);
};

class B {};

A objectA6 = A(1.0, 6.0);
A objectA7 = A(1);

A objectA8(1.0, 6.0);

// These are not paired
