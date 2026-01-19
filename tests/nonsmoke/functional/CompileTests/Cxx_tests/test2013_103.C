class Test
   {
     public:
          Test* operator -> ();
          int getFormat();
   };

void foobar()
   {
     Test ref ;
     // ref.operator->()->getFormat();
     ref.operator->()->getFormat();
     ref->getFormat();
   }

