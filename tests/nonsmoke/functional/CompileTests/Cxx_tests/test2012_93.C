class A
   {
     public:
          int & operator[](int i);
          const int & operator[](int i) const;
   };

class X
   {
     public:
          void set( A & data ) const
             {
               int nz = 1;
               for (int z = 0; z < nz; z++)
                  {
                    data[z] = 42;
                  }
             }
};
