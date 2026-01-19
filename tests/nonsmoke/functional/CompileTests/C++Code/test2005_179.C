

class foo
   {
     public:
          void prod ();
   };

void prod (int x);

void foo::prod ()
   {
  // Error: global scope name qualification is dropped in generated code!
     ::prod (0);
   }
