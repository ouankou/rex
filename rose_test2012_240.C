
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
// cc.in51
// accessing object fields with qualified names

class A {
public:
  void f();
};

class B {
public:
  void f();
};

class C : public A, public B {};

int main() {
  class C c
      // ERROR(1):*/ c.f();      // ambiguous
      ;
  c.A::f();
  c.B::f();
  return 0;
}
