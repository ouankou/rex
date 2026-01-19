int main(int argc, char *argv[])
   {
     struct S { char c; };
     typedef char S::*volatile PM;
  // typedef PM PM_t;

     return 42;
   }
