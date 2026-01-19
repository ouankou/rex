void foobar() 
   {
     int count;

     switch (count)
        {
          case 40: 41;
            // #pragma XXX
            __attribute__((__fallthrough__));
            int abc;
          case 42: 43;
        }
   }
