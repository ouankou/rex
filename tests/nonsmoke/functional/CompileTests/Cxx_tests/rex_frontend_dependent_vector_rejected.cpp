template <int N> struct rex_vectors {
  typedef int type __attribute__((vector_size(N)));
};
rex_vectors<16>::type rex_value;
