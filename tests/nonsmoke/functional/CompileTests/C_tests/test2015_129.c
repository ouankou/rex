struct a 
   {
     union b 
        {
          int c;
          int d;
        } e;
     float f;
} g = {.e.c = 3};
