
struct A { int global_A_x; };

void foo()
   {
     struct A { int local_A_x; };

  // typedef ::A A_global_type;

     A local_A;
     ::A global_A;

     local_A.local_A_x   = 0;
     global_A.global_A_x = 0;
   }


