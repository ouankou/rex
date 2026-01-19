class X
   {
     public:
          X (int n)
             {
             }

          friend X operator-(const X &Lhs, int i);
          inline friend X operator+(const X &Lhs, int i) { return X(i); }
   };


   
