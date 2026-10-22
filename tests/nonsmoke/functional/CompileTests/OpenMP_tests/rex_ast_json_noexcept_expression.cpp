int rex_noexcept_operand(int value) noexcept { return value + 1; }

namespace rex_noexcept_scope {
template <class T> T &forward(T &value) noexcept { return value; }

template <class T>
T &apply(T &value) noexcept(noexcept(rex_noexcept_scope::forward<T>(value))) {
  return value;
}
} // namespace rex_noexcept_scope

template <class T> struct rex_noexcept_receiver {
  T value;
  template <class U>
  void assign(rex_noexcept_receiver<U> &other) noexcept(
      noexcept(value = other.value)) {
    value = other.value;
  }
};

template <class T> struct rex_noexcept_receiver<T *> {
  using pointer = T *;
  static pointer allocate(pointer value);
};

void rex_ast_json_noexcept_expression(int *value) {
  const bool does_not_throw = noexcept(rex_noexcept_operand(*value));
#pragma omp parallel if (does_not_throw)
  {
    *value = rex_noexcept_operand(*value);
  }
}
