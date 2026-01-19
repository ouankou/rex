
class A
   {
     public:
       // This is the current scope.

       // Imagine this wrapped by a class to see the first level of name qualification that 
       // would be required based on how the name qualification is computed in ROSE.
          enum Type
            {
              VALUE
            };
   };

class B : public A
   {
     public:
          B();

       // The name qualification on Type is not required.
          A* getValue(A::Type x);

     private:
       // The enum field value is hidding the enum fiield value in 
       // the base class, so we need additional name qualification.
          enum C
             {
               VALUE
             };
   };

B::B()
   {

  // Use of a derived class enum value requires name qualificaiton to 
  // avoid being mistaken for the derived class enum with the same name.

  // This is the current statement.
     getValue(A::VALUE);
   }
