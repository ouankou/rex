// template<class T> class X { };
// template<class T> void f(X<T>);
// template<> void f(X<int>);

namespace std
   {
   }


namespace std
   {
     template<class T> class X { };
     template<class T> void f(X<T>) {}
     template<> void f(X<int>) {}
   }
