
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
template <typename T> struct X {
  struct Y;
  struct Y y1;
};

struct Y {}

// This is an error:
;
// template <typename T> struct X::Y {};
struct X<int> a;

void foo() { struct X<int> a; }
