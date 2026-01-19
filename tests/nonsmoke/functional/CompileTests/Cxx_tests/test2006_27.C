

template <char sep>
class ReadContainer2
   {
     public:
          static bool read()
             {
               return 0;
             }
   };

class ParameterDeclaration
   {
     public:
          bool read()
             {
               return ReadContainer2<','>::read();
             }
   };
