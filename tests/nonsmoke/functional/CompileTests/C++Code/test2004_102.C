
// A typedef with a non-explicit forward declaration (before seeing the definition)
typedef struct Atag A0;

// A typedef with a defining declaration
typedef struct Atag
   {
     Atag* a;
   } A1;

// A typedef with a non-explicit forward declaration (after seeing the definition)
   typedef A1 A2;
