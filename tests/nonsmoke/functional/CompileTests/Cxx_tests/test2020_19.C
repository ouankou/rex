namespace X
   {
     int i = 43;
     namespace N
        {
          int i = 44;
          struct X
             {
               static void f();
             };
          struct N
             {
               static void f();
             };
          void X::f()
             {
               using namespace X;
               i = 45;
          }
        }
   }

   void foobar() {}
