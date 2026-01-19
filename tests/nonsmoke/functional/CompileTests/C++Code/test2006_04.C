

class X
   {
     public:
          void foo1(){ ::operator   new(1); }
//        void foo2(){ ::new(1); }

   };

// This function shows how the two function calls to new are represented differently in the AST
// The first time as a SgNewExp IR node and the second time as a function call.
void
foobar()
   {
  // This has a call to a SgNewExp
     X* ptr = new X; 
     void* voidPointer = 0L;

  // While this has a call to a SgFunctionCallExp (since it is called with "operator new" syntax)
     ::operator new(1);
     ::operator delete(voidPointer);
}

// See if this also happens for member functions called two different ways!
class Y
   {
     public:
          void* operator    new (unsigned int size) { return 0L; }
//        void foo2(){ ::new(1); }

   };

void
foobar2()
   {
  // This has a call to a SgNewExp (only yhe scope appears to link it the "Y"
     Y* ptr = new Y;

  // While this has a call to a SgFunctionCallExp using SgMemberFunctionRefExp (since it is called with "operator new" syntax)
     Y::operator new(1);
}
