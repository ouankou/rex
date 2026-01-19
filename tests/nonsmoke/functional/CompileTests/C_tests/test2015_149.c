// This is a simplified version of test2015_77.c

#include "test2015_149.h"

void foobar(struct vcpu *v)
   {
     struct xsave_struct *xsave_area;
     typeof(xsave_area->fpu_sse) * abc;
     abc->fsw = 42;

   }
