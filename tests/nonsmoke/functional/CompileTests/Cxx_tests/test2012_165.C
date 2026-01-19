// struct B {};

struct A
   {
  // Note that legacy frontend automatically inserts a forward declaration for
  // struct B (but this is wrong, since it declares struct  B to be in struct
  // A). 1) We don't want legacy frontend to output this extra declaration.
  // struct B;

  // This is a declaration of a pointer to the global struct B (which is not declared in global scope).
  // 2) This reference to B should not be qualified since it's type has not been seen as a declaration.
     struct B *bp;

  // The declaration of "struct B;", after the declaration, changes the scope of the B used in the declaration 
  // of "struct B *bp;" so that B is in the namespace outside of the class (global scope in this case).
  // Now there is a vailid struct B in A, so A::B*y2 is vaild.
     struct B;

//   struct B {};
   };

   // struct B {};
