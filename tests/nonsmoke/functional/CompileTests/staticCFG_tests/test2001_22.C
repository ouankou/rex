// This code tests the unparsing of typedefs of member function pointers

/*
// original code:
typedef void (A::*PointerToMemberFunctionType)();
// unparsed code:
typedef A (A::*PointerToMemberFunctionType);
 */

class A {};
// typedef float MyType;
typedef void (*MyPointerToFunctionType)();
typedef void (A::*MyPointerToMemberFunctionType)();

// PointerToMemberFunctionType X = 0;
