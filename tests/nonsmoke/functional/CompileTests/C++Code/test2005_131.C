// problem code: (related to test2005_129.C and test2005_130.C)

// segmentation fault!

#include <vector>

int z = 0;

class X
   {
     public:
         void set( std::vector<double> & data )
             {
                  {
                    int a;
                    data[z] = 0.0;
                 // int b;
                  }

                  // int x;
                  // return;
             }
      // int y;
   };

// int abc;
