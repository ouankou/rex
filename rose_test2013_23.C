
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
// This test code demonstrates the requirement for the "mutable" keyword.
typedef int _Atomic_word;

static void __atomic_add_dispatch(_Atomic_word *__mem) {}

class locale {
public:
  class facet;
  friend class facet;
};

class facet {
public:
  friend class locale;

  // Note that "mutable" keyword is required and dropped by ROSE (which is the
  // demonstrated bug in this code).
private:
  _Atomic_word _M_refcount;

private:
  void _M_add_reference() const {
    // __gnu_cxx::__atomic_add_dispatch(&(this) -> _M_refcount);
    ::__atomic_add_dispatch(&(this)->_M_refcount);
  }

  // __gnu_cxx::__atomic_add_dispatch(&_M_refcount, 1);
  // throw()
};
