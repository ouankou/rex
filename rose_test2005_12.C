
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
template <typename T1> class B {
private:
  typedef int privateType;

public:
  template <typename T2> class BB {};

public:
  void foobar(BB *b) {}
};

int main() {
  class B<int> b
      // This unparses to:
      ;
  // b.foobar(((class B < int > ::BB< B < int > ::privateType  > *)0));
  // but this is an error since "B<int>::privateType" is private!
  b.foobar(0);

  return 0;
}
