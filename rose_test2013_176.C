
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
// This test code tries to reproduce the problem with the map container
// Templated class
template <typename T, typename U = int> class Xt {
public:
  Xt();

  // template <typename V> Xt ( V v, U u );
};

template <typename T, typename U = int> class Xt;
// Example of code that should be generated for the specialization of Xt
// template using default parameter
template <> class Xt<int, int>;

template <> class Xt<int, int> {
  // typedef int U;
  // The bug is that this is not output in the generated code.
public:
  Xt();

  // Xt (int x);
  // template <typename V> Xt ( V v, U u );
  template <typename V> Xt(V v);

}

// Added typedef to allow for unconverted default parameters appearing in
// template declarations!
;

void foo() { class Xt<int, int> x; }

// Xt<int,int> y;
