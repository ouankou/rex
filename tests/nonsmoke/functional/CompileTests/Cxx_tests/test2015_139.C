
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
  int x;
   };

void foobar()
   {
     C m;
     m.A::x = 7;
   }
