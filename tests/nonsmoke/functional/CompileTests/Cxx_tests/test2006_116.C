
#include <stdio.h>
extern void foo(double y[]);
extern void foobar(double* y);
double mm_lowerBound;
double mm[10];
int main(void)
{
//double mm[10]; // no warning and no changes at all if it is local scope, ??
 foo(mm-1); // looks a very strange argument passing, but actually used in the benchmark.
 foobar(mm+1);
 foo((mm+1)-1);
 foo((mm+1)-2);
 foo((mm-1)-2);
 mm_lowerBound = (mm-1)[0];
 (mm-1)[0] = 0.0;
 ((mm+1)-2)[0] = 0.0;
 return 0;
}
