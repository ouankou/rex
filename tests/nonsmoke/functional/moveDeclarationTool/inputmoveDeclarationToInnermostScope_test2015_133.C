// This test code demonstrate an unparsing error in the matching of #if to #endif when unparsing 
// from the AST and when using -rose:unparse_tokens

int foobar()
   {
     int x;
     int y;

     while (true) {
       x = 42;

       if (true) {
       } else {
         y = 42;
       }
     }

     return 0;
   }
