namespace rex_function_production {
template <class T> T selected(T value) { return value; }

template <> int selected<int>(int value);
extern template long selected<long>(long value);
template char selected<char>(char value);

#define REX_FUNCTION_PRODUCTION_NAME selected<short>
extern template short REX_FUNCTION_PRODUCTION_NAME(short value);

struct defaults {
  int owned_default(int value = 7) { return value; }
};
} // namespace rex_function_production

int rex_frontend_function_production_contract() {
  rex_function_production::defaults object;
  return object.owned_default();
}

namespace rex_function_production {
struct rex_structural_ctor_list_owner {
  int value;
  rex_structural_ctor_list_owner(int input) : value(input) {}
  int rex_structural_nonconstructor() const { return value; }
};
template <class T = int> struct rex_parameter_box {};
void rex_source_parameter_template(rex_parameter_box<> value);
int rex_cv_parameter(int value);
int rex_cv_parameter(const volatile int value) { return value; }
int rex_cv_parameter_without_prototype(const volatile int value) {
  return value;
}
auto rex_decltype_parameter(int value) -> decltype(value) { return value; }
auto rex_decltype_array_parameter(int (&value)[3]) -> decltype(value) {
  return value;
}
auto rex_decltype_function_parameter(int (&value)(int)) -> decltype(value) {
  return value;
}
using rex_decltype_prototype = auto(int value) -> decltype(value);
using rex_decltype_prototype_dependent_argument = auto(int value,
                                                       decltype(value) other)
    -> decltype(other);
using rex_decltype_prototype_nested_argument = auto(
    auto (*callback)(int value)->decltype(value), int other) -> decltype(other);

template <class T, class U = int> int rex_defaulted_specialization();
template <> int rex_defaulted_specialization<char, int>();
template <> int rex_defaulted_specialization<char>() { return 2; }

template <class T, class U = int> int rex_defaulted_instantiation() {
  return 3;
}
extern template int rex_defaulted_instantiation<char>();

template <int Value> int rex_expression_instantiation() { return Value; }
extern template int rex_expression_instantiation<(1 + 1)>();

struct rex_member_argument_owner {
  template <class T, class U = int> int rex_member_instantiation(T value) {
    return value;
  }
};
extern template int
rex_member_argument_owner::rex_member_instantiation<char>(char);

template <class T> struct rex_argument_box {};
extern template int rex_defaulted_instantiation<rex_argument_box<int>>();
#define REX_PRODUCTION_NESTED_ARGUMENT                                         \
  rex_defaulted_instantiation<rex_argument_box<long>>
extern template int REX_PRODUCTION_NESTED_ARGUMENT();
extern template int rex_defaulted_instantiation<rex_argument_box<short>\
>();
using rex_lookup_argument = char;
} // namespace rex_function_production

using rex_lookup_argument = int;
extern template rex_lookup_argument
rex_function_production::rex_defaulted_instantiation<rex_lookup_argument>();

using rex_point_lookup_argument = long;
namespace rex_function_production {
extern template int rex_defaulted_instantiation<rex_point_lookup_argument>();
using rex_point_lookup_argument = char;
} // namespace rex_function_production
