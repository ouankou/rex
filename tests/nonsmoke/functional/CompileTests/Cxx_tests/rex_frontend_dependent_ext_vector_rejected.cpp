template <int N> struct rex_vectors {
  typedef int type __attribute__((ext_vector_type(N)));
};
rex_vectors<16>::type rex_value;
