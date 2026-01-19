namespace XXX 
   {
     void foobar(const int def[3]); // (const int def[3]);
   } // namespace XXX

namespace XXX 
   {
  // namespace {
     namespace Y 
        {

     const unsigned size = 3;
        } // unnamed namespace, back to XXX

        void foobar(const int def[Y::size]) {
          // Y::AAA abc;
          // abc.isPeriodic = false;
        }
   } // namespace XXX
