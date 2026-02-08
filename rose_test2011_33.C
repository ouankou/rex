
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
// Test for hiding class name where global qualification is required.

struct A {

  struct B {};
};

void foo() {
  typedef int A;

  // Type elaboration is not required here, but the global qualification is
  // required (but only for GNU, not for legacy frontend).
  :: ::A ::B x;
}
