class Internal_Partitioning_Type
   {
public:
  // This causes a failure in the reference to the function in foo().
  static int getReferenceCountBase() { return 64; }
   };

class Partitioning_Type
   {
     public:
          Internal_Partitioning_Type *Internal_Partitioning_Object;

          void foo();
   };

void 
Partitioning_Type::foo()
   {
     Internal_Partitioning_Object->getReferenceCountBase();
   }


