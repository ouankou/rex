class A {};

class B {
public:
  int x;
};

class C : public A, public B
   {
public:
  int x;
   };

void foobar()
   {
     C m;
  // m.A::x = 7;
     m.B::x = 7;
   }
