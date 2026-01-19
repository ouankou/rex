// Test of designators in structs

struct S 
   {
     int a[5];
//   double b;
//   struct T c;
   };

void foo() 
   {
     struct S x = {
        a: { // An obsolete GCC syntax
             [1] = 1,
             [3] = 2,
             3
        },
        };
   }
