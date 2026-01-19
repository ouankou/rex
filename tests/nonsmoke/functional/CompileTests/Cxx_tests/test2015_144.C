class A
   {
     public:
          int x;
   };

   class B {};

   class C : public A, B {
   public:
     int x;
   };

void foobar()
   {
     C m;
  // m.A::x = 7;
     m.A::x = 7;
   }
