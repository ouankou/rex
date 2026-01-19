
class X
   {
public:
  int getValue() const { return valueArray[0]; }
  //        static const int arraySize = 16;
  static const int arraySize = 16;
  static const int valueArray[arraySize + 10];
   };

// const int X::arraySize = 16;
const int X::arraySize;
