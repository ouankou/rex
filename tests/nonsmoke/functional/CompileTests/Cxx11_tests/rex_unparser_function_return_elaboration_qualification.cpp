template <typename T> struct rex_return_type {};

template <typename T>
typename ::rex_return_type<T> rex_make_return_type(T value) {
  (void)value;
  return ::rex_return_type<T>();
}

void rex_use_return_type() { (void)rex_make_return_type(1); }
