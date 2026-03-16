// This test code is the same as test2020_07.C

#include<string>
using namespace std;

struct A {
  string str;
  int n = 42;
  int m = -1;
};

A a{.m = 21}; // Initializes str with {}, which calls the default constructor
              // then initializes n with = 42
              // then initializes m with = 21
