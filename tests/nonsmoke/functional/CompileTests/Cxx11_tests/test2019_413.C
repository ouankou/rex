
namespace A
   {
     struct S;
   }

A::S* pointer;

void foobar()
   {
  // This will compile with legacy frontend and GNU 5.1 will compile it with
  // "A::" missing.
  reinterpret_cast<A::S *>(pointer);
   }
