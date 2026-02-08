
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
#define DEFINE_IN_CLASS 1

class X {
#if DEFINE_IN_CLASS
public:
  template <typename T> void foo(int i, int j, int k) {}

#endif

public:
  void foobar() { foo<int>(1, 2, 3); }
#else
};

#if !DEFINE_IN_CLASS
#endif
// Template instantiation Directive for a member function
