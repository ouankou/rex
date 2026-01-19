class A
   {
  // Note that friend functions are always marked as private in the SgAccessModifier
     public: friend int a();

     int x;

  // This has to be a defining declaration!
     private: friend int b(){};

  // This has to be a forward declaration! (current bug is that "public:" is not output in code generation)
     public: int c();
   };
