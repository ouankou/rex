struct a 
   {
     struct b 
        {
          int c;
          int d;
        } e;
     float f;
}
// Normalized form as an alternative.
g = {.e = {.c = {3}}};
