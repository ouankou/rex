// template<class T, class U> class A { int x; };
// template<class U> class A<int, U> { short x; };

// This example defines the template to use the 
// template template argument and is valid C++ code.
// template<class T, class U> class V;

template<template<class T, class U> class V> class B
   {
  // Use of template template argument to build instantated template types.
  // V<int, char>  i;
  // V<char, char> j;
   };

// Declaration using template type taking template template argument
// B<A> c;
