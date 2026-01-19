// This should be a single variable declaration, but instead it is two seperate
// declarations. this is OK here, but a bug when it is handled this way inside
// of a for loop initialization. See test2012_106.C.
class A0 { int x; };
A0 x2;
// A0 x3;
