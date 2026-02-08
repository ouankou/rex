#include <set>

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
template <typename V> class test {
private:
  V v;
};
template <typename V> void foo() {
  std ::set<test<V>> s;
  std ::set<test<V>>::iterator i;
  if (std ::set<test<V>>::iterator i2 = s.begin()) {
  }

  // These are not legal C++ code.
  // if ( (int x = 0) != 2) {}
  // if ( (typename std::set<test<V> >::iterator i3=s.begin() ) != s.end()) {}
}

// "typename" is required here...
template <template <class> class T, class V> void foobar() {
  typename std ::set<T<V>> XXX;
  typename std ::set<T<V>>::iterator i = XXX.begin();
}
