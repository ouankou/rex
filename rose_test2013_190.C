
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
namespace X {

class A {
public:
  enum FieldType { ALPHA };
  template <typename CType, X::A::FieldType DeclaredType>
  static bool ReadPrimitive(CType *value);
};
} // namespace X
template <X::A::FieldType DeclaredType> void foobar();

void foo() {
  ::foobar<X::A::ALPHA>();
  ReadPrimitive<int, X::A::ALPHA>(0);
}
