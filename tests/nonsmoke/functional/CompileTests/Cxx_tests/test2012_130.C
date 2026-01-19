class X
   {
public:
  int getValue() const { return arraySize; }
  static int arraySize;
   };

int X::arraySize = 16;
