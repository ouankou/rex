

// Test mixing of declarations within a variable declaration!  This is a good reason
// for the normalization of variable declarations. So that function declarations can't 
// hide inside of a variable declaration!  Note that within ROSE, the unparsed code will
// generate separate declarations for each variable declaration.  Thus the function
// declaration will be seen internally as a function declaration, and not as a part of
// a variable declaration!  This step simplifies analysis and avoid redundent searching
// of all variable declarations for hidden function declaration (and function and member 
// function pointer declaration).

// Test mixing of declarations within a variable declaration
int x, *y, f(int);

// Note that foobar1 and foobar2 can't have the same name,
// since both are function pointers in global scope.
int z, (*foobar1)();

class A
   {
     public:
          void foo();
   };

// Test hidding of member function pointer within variable declaration
int a, (A::*foobar2)();

template <typename T>
class B
   {
     public:
          void foo();
   };

// Test hidding of member function pointer within variable declaration
// int b, (B<int>::*foobar3)();


