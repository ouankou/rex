template <class T> struct RexFunctionTemplateTypeIdentity {
  using type = T;
};

template <class TypeIdentity, class NestedType = typename TypeIdentity::type>
constexpr typename TypeIdentity::type
rex_function_template_default_parameter_owner(TypeIdentity) {
  return {};
}

int rex_function_template_default_parameter_owner_result =
    rex_function_template_default_parameter_owner(
        RexFunctionTemplateTypeIdentity<int>{});

struct RexMemberDefaultOwner {
  int value;
};

template <class C, class T, class M = T C::*, class P = int C::*,
          class Q = T RexMemberDefaultOwner::*, class R = T C::**>
constexpr int rex_function_template_member_defaults(C *) {
  return 0;
}

int rex_function_template_member_defaults_result =
    rex_function_template_member_defaults<RexMemberDefaultOwner, int>(nullptr);

template <class T, class U, class F = T (*)(U), class G = T(U, ...),
          class M = T (RexMemberDefaultOwner::*)(U) const,
          class H = T(T (*)(U))>
constexpr int rex_function_template_signature_defaults() {
  return 0;
}

int rex_function_template_signature_defaults_result =
    rex_function_template_signature_defaults<int, double>();

template <class T, class A = T[sizeof(T)], class B = int[sizeof(T)],
          class C = int[alignof(T)]>
constexpr int rex_function_template_array_default() {
  return sizeof(A);
}
int rex_function_template_array_default_result =
    rex_function_template_array_default<int>();

template <unsigned long> struct RexExpressionDefaultWrapper {};
template <class T, class A = RexExpressionDefaultWrapper<sizeof(T)>,
          class B = RexExpressionDefaultWrapper<alignof(T)>,
          unsigned long Count = sizeof(T), unsigned long Next = Count,
          class C = RexExpressionDefaultWrapper<Count>>
constexpr int rex_function_template_expression_default() {
  return sizeof(A) + sizeof(B) + sizeof(C) + Next;
}
int rex_function_template_expression_default_result =
    rex_function_template_expression_default<int>();

template <class T> struct RexDefaultArgumentWrapper {};
template <class T, class U = RexDefaultArgumentWrapper<T>,
          class V = RexDefaultArgumentWrapper<U *>>
struct RexNestedDefaultArgumentOwner {};
RexNestedDefaultArgumentOwner<int> rex_nested_default_argument_owner;

long long rex_non_type_value = 41;
struct RexNonTypeFriend {
  template <class Value, class Other, Value Initial, Value *Address>
  friend constexpr Value rex_function_template_non_type_owner();
};
template <class Value, class Other, Value Initial, Value *Address>
constexpr Value rex_function_template_non_type_owner() {
  return Initial + *Address;
}
long long rex_function_template_non_type_owner_result =
    rex_function_template_non_type_owner<long long, int, 37,
                                         &rex_non_type_value>();

template <class Left, class Right, Right Initial, Right *Address>
void rex_function_template_non_type_different_position();

template <class T> int rex_cv_redeclared_parameter(const volatile int value);
template <class T> int rex_cv_redeclared_parameter(const volatile int value) {
  return value;
}

auto rex_adjusted_array_parameter(int values[3]) -> decltype(values[0]);
