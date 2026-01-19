
class A {};

class A;

class B { class C; };

class B::C {};

class D
   {
     template <typename T> class E;
   };

template <typename T> class D::E{};

namespace F
   {
     class G;
     namespace H
        {
          class I ;
        }
   }

class F::G {};
class F::H::I {};

class J
   {
     void foo();
   };

void J::foo() {};

namespace K
   {
     void foo();
   };

void K::foo() {};

namespace K
   {
     void foo();
     };

void foobar( int x )
   {
     class A;
     class A {};
     class A;
   }

   // Looking at the scope of the unnamed ellipsis parameter (scope is NULL
   // since there is no legacy frontend scope)
   void foobar(...);

   // Scope of ellipsis parameter is that of the function (SgFunctionDefinition)
   void foobar(...) {};
