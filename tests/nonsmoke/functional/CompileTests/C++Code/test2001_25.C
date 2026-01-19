/* Unparser bug:

     struct MY_STRUCT { int x; } xStruct;
unparses to:
     struct MY_STRUCT xStruct;

     struct { int x; } xStruct;
unparses to:
     struct __T8687204 dans_typedef_of_a_struct;

Note: KCC translates 
         struct A { int x; } a;
      into
         struct A;  struct A { int x; };  struct A a;

Use of typedef does not even get to the unparser phase.
*/

int
main ()
   {
     if (1)
        {
          struct structName { int x; } localXStruct;
          localXStruct.x = 42;
          return localXStruct.x;
        }

  // struct xStruct X;
  // struct structName X;
  // xStruct.x = 7;
  // A X;
  // X.x = 42;
  // return X.x;

     return 0;
}
