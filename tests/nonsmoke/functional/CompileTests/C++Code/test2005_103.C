#include<string>

class X
   {
     public:
          int A() const;

          std::string getValue() const
             { 
               return valueArray[int(A())];
             }

          static const int arraySize = 16;
          static const std::string valueArray[arraySize+10];
   };
