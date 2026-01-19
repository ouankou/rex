// Example of where multiple SgInitializedName objects are built for the "x" in "int x;"

// Example #1
namespace X
   {
  // In this variable declaration "x" is a SgInitializedName object
     extern int x;
   }

// This is a different SgInitializedName object (but it is a reference to the same variable above)
// DQ (2/7/2006): This variable is being placed into both the global smybol table and the symbol table for namespace X.
int X::x = 0;


// Example #2
class Y
   {
     public:
       // In this variable declaration "x" is a SgInitializedName object
          int x;

       // This is a different SgInitializedName object (but it is a reference to the same variable above)
          Y():x(1) {}      
   };
