#define DEMONSTRATE_BUG 1

class intArray;

class intArray_Descriptor_Type {
public:
#if DEMONSTRATE_BUG
  friend void transpose(intArray &X);
#endif
  //        void transpose( intArray & X );
};

class intArray {
public:
  intArray_Descriptor_Type Array_Descriptor;
  //       friend void transpose( intArray & X );
};

void transpose(intArray &X)
// void intArray_Descriptor_Type::transpose (intArray & X )
{
  intArray &Result = X; // *(new intArray());
}

// This case works fine.  The code above demonstrates
// the bug (synthesized from a large source code base)
// typedef long unsigned int size_t;

// This approach is independent of 32 or 64 bit systems (at least for GNU)
typedef __SIZE_TYPE__ size_t;
