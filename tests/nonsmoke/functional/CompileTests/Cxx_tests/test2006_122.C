// Example showing use of template template arguments

// We don't need the iostream header file if we are not using "typeid()"
// #include <iostream>
// using namespace std;

template<class T, class U> class A { int x; };
template<class U> class A<int, U> { short x; };

// This example defines the template to use the 
// template template argument and is valid C++ code.
template<template<class T, class U> class V> class B
   {
  // Use of template template argument to build instantated template types.
     V<int, char>  i;
     V<char, char> j;
   };

// Declaration using template type taking template template argument
B<A> c;

int main()
   {
  // DQ: This does not work in legacy frontend, I don't know why!
  // cout << typeid(c.i.x).name() << endl;
  // cout << typeid(c.j.x).name() << endl;
   }
