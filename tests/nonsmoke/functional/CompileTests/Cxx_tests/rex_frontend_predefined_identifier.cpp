const char *rex_predefined_func() {
  const char *first = __func__;
  const char *second = __func__;
  return first == second ? __func__ : second;
}

const char *rex_predefined_function() { return __FUNCTION__; }

const char *rex_predefined_second_func() {
  const char *first = __func__;
  return first == __func__ ? first : nullptr;
}

const char *rex_predefined_pretty_function() { return __PRETTY_FUNCTION__; }

template <typename T> const char *rex_predefined_nested_template(T value) {
  const char *outer = __func__;
  if (value) {
    return __func__;
  }
  return outer;
}
