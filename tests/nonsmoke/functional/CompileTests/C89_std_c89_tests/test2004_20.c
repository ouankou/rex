/* The following code is all ANSI C 
(compiles using gcc -std=c99, for example, but does not compile with g++).
The old-style C represented in the function definitions does not currently 
work in ROSE.

   NOTE: In order to work with the gcc C compiler using old-style function definitions
we need to use a file with *.c (and not *.C).
*/

/* int foo1 (int x); */
/* int foo1 (int x, float y, int z); */
int foo1(int x);

int foo1(x)
   int x;
   {
     x = 42;
     return x;
   }
