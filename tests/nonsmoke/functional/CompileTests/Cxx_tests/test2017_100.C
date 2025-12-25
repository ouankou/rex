// This  is allowed for C++ (passes g++ and legacy frontend), but fails for C
// (gcc and legacy frontend using C98 mode).
void foobar()
   {
     int x;
     switch(x)
       {
          case 0:
             typedef int integer_1;
          case 1:
             x = 42;
             typedef int integer_2;
       }
   }
