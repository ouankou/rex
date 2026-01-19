struct A
   {
  // Note that legacy frontend automatically inserts a forward declaration for
  // struct B (but this is wrong since it declares struct  B to be in struct A).
  // However, if we do it, then it means that "struct B;" is declared in "struct
  // A" and so ROSE will generate the correct code.
  struct B;

  // This is a declaration of a pointer to the global struct B (which is not declared in global scope).
     struct B *bp;

  // Now there is a vailid struct B in A, so A::B*y2 is vaild.
  // struct B;

//   struct B {};
   };

// struct B {};
