namespace XXX 
   {
     void foobar(const int def[3]);

     namespace Y 
        {

          const unsigned ArraySize = 3;
        }

        void foobar(const int def[Y::ArraySize]) {
          // Y::AAA abc;
          // abc.isPeriodic = false;
        }
   } // namespace XXX
