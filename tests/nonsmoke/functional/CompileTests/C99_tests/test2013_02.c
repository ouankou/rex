// Test of designators in structs
struct T 
   {
     int w;
     char x;
   };

struct S 
   {
     int a[5];
     double b;
     struct T c;
   };

void foo() 
   {
  struct S x = {
      .b = 3.14,
      .c = {.x = 7, .w = 8},
  };
   }
