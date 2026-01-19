class A
   {
     private:
          class xxx {};

     public:
         typedef xxx referenced_t;
   };

A::referenced_t* foobar()
   {
  // This is unparsed as: "A::xxx()" which is not visible.
  // return A::referenced_t();
     return 0L;
   }
