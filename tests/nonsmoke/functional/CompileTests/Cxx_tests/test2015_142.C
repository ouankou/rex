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

class C : public A, public B
   {
public:
  int x;
   };

void foobar()
   {
     C m;
     m.x = 7;
   }
