
// #include<string>
namespace A
   {
     class string {};
   }

class X
   {
     public:
          int A() const;

          A::string getValue() const
             { 
               return valueArray[int(A())];
             }

       // Note that this SgInitializedName does not have a valid parent pointer.
       // It was built when the "return valueArray[int(A())];" was built.
          static const int arraySize = 16;

          static const A::string valueArray[arraySize+10];
   };
