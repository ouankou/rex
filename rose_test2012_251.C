
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
// C++ casting operator definition

class X {};

template <class T> class SwigValueWrapper {
public:
  operator type - parameter - 0 - 0 &() const;
};

void foo() {
  class SwigValueWrapper<X> X_result;
  class X *X_resultptr
      // Force the conversion operator to be called so that the copy constructor
      // for X can be called with new!
      ;
  X_resultptr = (new (X)((class X &)X_result.operator X &()));
}

// Case using non-nested template class (any class type)
