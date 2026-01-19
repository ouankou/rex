// This test code has several instances of the same bug in the legacy
// frontend/SAGE connection. The problem is fixed (1/14/2004), the current
// source seq poitner is no longer conditionally incremented (which allows it to
// be NULL in some cases).

// this works fine
struct foo_A
   {
     int x;
   } varA1;

   typedef struct foo_A varA2;

   // this works fine
   struct foo_B {
     int x;
   };
   typedef struct foo_B varB;

   int function_A() {
     typedef int myint;

     return 0;
   }

// Causes a assertion failure in legacy frontend/SAGE connection
struct foo_C
   {
     int x;
   } varC1;

int function_B (int argc, char* argv[])
   {
     typedef struct foo_C varC2;
     return 0;
}
