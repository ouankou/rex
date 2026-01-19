

// Things that should work in C++, gcc and C99
// Examples from: http://gcc.gnu.org/onlinedocs/gcc-3.2.2/gcc/Compound-Literals.html#Compound%20Literals

void foobar()
   {
     struct foo {int a; char b[2];} structure;

     int x = 1, y = 2;

  // Here is an example of constructing a struct foo with a compound literal:
  // structure = ((struct foo) {x + y, 'a', 0});
     structure = ((struct foo) {x, 'a', 0});

  // This is equivalent to writing the following:
     {
       struct foo temp = {x + y, 'a', 0};
       structure = temp;
     }
   }
