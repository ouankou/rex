#include <vector>

namespace XXX 
   {
     void foobar(const int def[3]);
   } // namespace XXX

namespace XXX 
   {
     namespace 
        {

          const unsigned size = 3;

          struct AAA
             {
               bool isPeriodic;
             };

       // struct YYY;
          struct YYY {};
          std::vector<YYY*> boundary_data;

      } // unnamed namespace, back to XXX

      void foobar(const int def[size]) {}
   } // namespace XXX
