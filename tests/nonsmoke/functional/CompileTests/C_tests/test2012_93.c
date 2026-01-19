

void foobar()
   {
      for (;;)
         if (((({ union {
              long __in;
              int __i;}__u;
              __u.__in = 42;
              __u.__i;
                  })) & 0xff) == 0x7f)
         {
            break;
         }
      
   }


