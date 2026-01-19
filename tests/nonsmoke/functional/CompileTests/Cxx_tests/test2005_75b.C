// int x;

#include "test2005_75.h"

// int y;

void foobar()
   {
     foo(3.14);

  // Now force instatiation of a template which ROSE will build as a 
  // specialization and which will be defined twice (once in each file).
  // foo(1);
   }
