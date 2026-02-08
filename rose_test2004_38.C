
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

class X {
public:
  X();
};
template <class _Alloc> class _List_base {
public:
  typedef _Alloc allocator_type;
  allocator_type get_allocator() const { return allocator_type(); }
};
class _List_base<X> Y;
