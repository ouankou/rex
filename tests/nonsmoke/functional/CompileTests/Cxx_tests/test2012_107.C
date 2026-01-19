// This should be a single variable declaration, but instead it is two separate
// declarations. this is OK here, but a bug when it is handled this way inside
// of a for loop initialization. See test2012_106.C. class A0 {public: int foo
// (int x) {return x;}} x2;
class A0 {
  int x;
} x2;
