// number #33

typedef float numberType;

class Y
   {
     public:
          typedef int numberType;
   };

class Z : public Y
   {
     public:
       // Example of where "::" is significant, without it numberType 
       // will be an "int" instead of a "myNumberType"
          typedef ::numberType numberType;
   };
