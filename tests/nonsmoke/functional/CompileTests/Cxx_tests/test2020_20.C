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
       // This is required for this bug.
          struct N
             {
            // static void f();
          };
          void X::f()
             {
               using namespace X;
               i = 45;
             }
        }
   }

