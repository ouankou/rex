#define myAssert \
     if (1) \
          1; \
       else \
          0;


int 
main()
   {
     int x = 42;

     myAssert;
     myAssert;
     myAssert;
     myAssert;

     switch (x)
        {
     case 0: {
       myAssert;
       int y = x;
     } break;
     case 1: {
       myAssert;
       int y = x;
     } break;
     case 2: {
       myAssert;
       int y = x;
     } break;
     case 3: {
       myAssert;
       int y = x;
     } break;
        }

  // printf ("Program Terminated Normally! \n");
     return 0;
   }
