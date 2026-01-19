

void foobar()
  {
 // This generates a constructor initializer using a primative type!
 // This is what should justify the use of primative types within the 
 // SgConstructorInitializer.
    double* x = new double();

 // This is a C style cast using C style notation
    int y1 = (int)1ULL;

 // This is a C style cast using constructor notation (semantically equivalent to the 
 // C style notation, though this is preferred by some in C++). We unparse the C style
 // notation within ROSE.
    int y2 = int(1ULL);

 // This is an assignment initializer using the "default value" for type int (zero is the default value for int so we get zero internally).
    int z = int();
  }
