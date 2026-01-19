class Test
   {
     public:
          Test & operator+=(const Test & x);
          Test & operator&();
   };

void foobar()
   {
     Test s,t;

     t += s;

     t.operator+=(s);

     t += s.operator&();

     t.operator+=(s.operator&());

     &s;

     t.operator+=(&s);

     // This case requires the operator "*" to be unparsed before the "s".
     t += &s;
   }
