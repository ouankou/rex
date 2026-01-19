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
          void N::f()
             {
               using namespace ::X::N;
               i = 46;
             }
        }
   }


