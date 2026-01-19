class A
   {
     public:
          int x;
   };

class B
   {
     public:
          int x;
   };

class C : public A, B
   {
public:
   };

void foobar()
   {
     C m;
  // m.A::x = 7;
     m.A::x = 7;
   }
