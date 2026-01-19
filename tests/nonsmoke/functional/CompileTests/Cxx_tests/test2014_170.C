struct B {};
struct X : B {
  operator B();
};

B foo (X abc)
   {
     return abc.operator B();
   }
