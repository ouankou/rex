template <typename First, typename Second>
concept rex_same_type_surface = true;

template <typename T> bool rex_simple_failure() {
  return requires(T value) { value.missing(); };
}

template <typename T> bool rex_type_failure() {
  return requires { typename T::missing; };
}

template <typename T> bool rex_compound_expression_failure() {
  return requires(T value) {
    { value.missing() };
  };
}

template <typename T> bool rex_compound_return_type_failure() {
  return requires(T value) {
    { value } -> rex_same_type_surface<typename T::missing>;
  };
}

bool rex_simple_result = rex_simple_failure<int>();
bool rex_type_result = rex_type_failure<int>();
bool rex_compound_expression_result = rex_compound_expression_failure<int>();
bool rex_compound_return_type_result = rex_compound_return_type_failure<int>();
