void foo1();
int foo2();

int foobar()
   {
     int pid;

     if (1) {
       pid = foo2();

       if (pid == -1) {
       }
     }

     return( 0 ) ;
   }

