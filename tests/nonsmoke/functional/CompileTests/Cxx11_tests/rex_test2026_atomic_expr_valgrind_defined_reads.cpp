template <typename T> struct RexAtomicBox {
  template <typename U> U exchange(U *ptr, U value);
};

template <typename T>
template <typename U>
U RexAtomicBox<T>::exchange(U *ptr, U value) {
  return __atomic_exchange_n(ptr, value, __ATOMIC_SEQ_CST);
}

int rex_test2026_atomic_expr_valgrind_defined_reads(int *ptr) {
  RexAtomicBox<int> box;
  int old_value = box.exchange(ptr, 7);
  int expected = old_value;
  __atomic_compare_exchange_n(ptr, &expected, old_value + 1, false,
                              __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
  return __atomic_fetch_add(ptr, 1, __ATOMIC_SEQ_CST);
}
