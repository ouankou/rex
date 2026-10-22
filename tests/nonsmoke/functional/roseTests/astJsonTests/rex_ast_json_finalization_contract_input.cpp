struct RexAstJsonFinalizationOwner {
  int value;

  int read() const { return this->value; }
};

using RexAstJsonSourceForm = int;

template <class T> auto rex_ast_json_dependent_value() -> decltype(T::value) {
  return T::value;
}

template <class T> constexpr T rex_ast_json_variable_template = T{2};

template <class T>
void rex_ast_json_signature_variable() noexcept(
    rex_ast_json_variable_template<T> != 0) {}

template <bool Constant> struct rex_ast_json_iterator {
  template <bool Other>
  rex_ast_json_iterator(const rex_ast_json_iterator<Other> &) {}
};

template <class T> struct rex_ast_json_enum_argument {};
template <int Value> struct rex_ast_json_value_argument {};
rex_ast_json_value_argument<7> rex_ast_json_value;

template <class T> struct rex_ast_json_partial_specialization;
template <class T> struct rex_ast_json_partial_specialization<T *> {
  enum class Kind : unsigned { value };
  rex_ast_json_enum_argument<Kind> argument;
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
  rex_ast_json_signature_variable<int>();
  enum class LocalKind { value };
  rex_ast_json_enum_argument<LocalKind> local_argument;
  RexAstJsonFinalizationOwner owner{1};
  return owner.read() + rex_ast_json_contract_function(0) +
         rex_ast_json_contract_cast(0) + rex_ast_json_variable_template<int> +
         rex_ast_json_partial_specialization<int *>::value +
         static_cast<int>(rex_ast_json_noexcept) +
         sizeof(rex_ast_json_template_template_owner<
                rex_ast_json_template_template_argument, int>::type);
}
