struct A { struct B { int global_A_B_x; }; int global_A_x; };

void foo()
   {
     struct A { struct B { int local_A_B_x; }; int local_A_x; };

  // typedef ::A A_global_type;

     A::B local_A_B;
     ::A::B global_A_B;
  // B::A::B namespace_A_B;

     local_A_B.local_A_B_x   = 0;
     global_A_B.global_A_B_x = 0;
  // namespace_A_B.namespace_A_B_x = 0;
   }

