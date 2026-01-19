class B_
   {
     public:
          int i;
       // int operator + () { return ( i + 11 );}
       // int operator - () const { return ( i + 12 );}
          B_(int j) : i(j) { }
   };

int operator * (B_  s) { return ( s.i + 13 );}

int ivalue(int);
int iequals(int, int);

void foobar()
   {
     B_ a (1);
  // B_ *       p  = &a;
     const B_ * pc = &a;

     int i3 = **pc;
     iequals(ivalue(i3), 14);
   }
