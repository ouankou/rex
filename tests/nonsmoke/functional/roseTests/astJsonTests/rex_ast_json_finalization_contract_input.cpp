struct RexAstJsonFinalizationOwner {
  int value;

  int read() const { return this->value; }
};

using RexAstJsonSourceForm = int;

template <class T> auto rex_ast_json_dependent_value() -> decltype(T::value) {
  return T::value;
}

template <class T> constexpr T rex_ast_json_variable_template = T{2};

template <class T> struct rex_ast_json_partial_specialization;
template <class T> struct rex_ast_json_partial_specialization<T *> {
  static constexpr int value = 3;
};

template <template <class> class Op, class T>
struct rex_ast_json_template_template_owner {
  using type = Op<T>;
};
template <class T> struct rex_ast_json_template_template_argument {};

int rex_ast_json_contract_function(int value) { return value + 1; }

long rex_ast_json_contract_cast(int value) { return static_cast<long>(value); }

constexpr bool rex_ast_json_noexcept =
    noexcept(rex_ast_json_contract_function(0));

int rex_ast_json_type_owned_array[3];

void rex_ast_json_external_contract(int value);

int main() {
  RexAstJsonFinalizationOwner owner{1};
  return owner.read() + rex_ast_json_contract_function(0) +
         rex_ast_json_contract_cast(0) + rex_ast_json_variable_template<int> +
         rex_ast_json_partial_specialization<int *>::value +
         static_cast<int>(rex_ast_json_noexcept) +
         sizeof(rex_ast_json_template_template_owner<
                rex_ast_json_template_template_argument, int>::type);
}
