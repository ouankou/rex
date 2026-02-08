
struct __NSConstantString_tag {
  const int *isa;
  int flags;
  const char *str;
  long length;
};

struct __va_list_tag {
  unsigned int gp_offset;
  unsigned int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
};
// This example code fails to link, but it will compile!
// It will compile and link with g++, just not with ROSE.
// Typedefs to define the pointer to member function types.
// Member function pointer
// Set ObjectType pointer.
// Set member function pointer to NULL.
// Call member function using member function pointer.

class Y {

public:
  void foo() {}
};

template <typename ObjectType> class X {
public:
  typedef void (X<ObjectType>::ObjectType::*AccessorFunctionType)();
  ObjectType *object;
  AccessorFunctionType mFieldAccessorMethod;
  void getRemapField();
};
template <typename ObjectType> void X<ObjectType>::getRemapField() {
  object = 0;
  mFieldAccessorMethod = 0;
  (object->*mFieldAccessorMethod)();
}

int main() {
  class X<Y> x;
  x.getRemapField();
  return 0;
}
