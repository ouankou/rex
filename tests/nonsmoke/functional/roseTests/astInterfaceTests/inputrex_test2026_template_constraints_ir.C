#include "rex_template_system_header_constraints.hpp"
#include <type_traits>

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
int sfinae_pick(T) {
  return 1;
}

template <typename T>
concept Integral = std::is_integral_v<T>;

template <Integral T> int concept_pick(T) { return 2; }

template <int N>
  requires(N > 0)
int rex_constraint_literal_identity();
template <int N>
  requires(N > 0L)
int rex_constraint_literal_identity();

template <class T, class U>
  requires std::is_pointer_v<T>
struct rex_partial_constraint_class {
  static constexpr bool selected = false;
};

template <class T>
  requires std::is_integral_v<T>
struct rex_partial_constraint_class<T *, T> {
  static constexpr bool selected = true;
};

template <class T, class U>
  requires std::is_pointer_v<T>
inline constexpr bool rex_partial_constraint_variable = false;

template <class T>
  requires std::is_integral_v<T>
inline constexpr bool rex_partial_constraint_variable<T *, T> = true;

static_assert(rex_partial_constraint_class<int *, int>::selected);
static_assert(!rex_partial_constraint_class<float *, float>::selected);
static_assert(rex_partial_constraint_variable<int *, int>);
static_assert(!rex_partial_constraint_variable<float *, float>);

template <class Outer> struct rex_constraint_owner {
  template <class T>
    requires std::is_same_v<T, Outer>
  struct rex_nested_constraint_class {};

  template <class T>
    requires std::is_same_v<T, Outer>
  static constexpr bool rex_nested_constraint_variable = true;
};

rex_constraint_owner<int>::rex_nested_constraint_class<int>
    rex_nested_constraint_object;
static_assert(rex_constraint_owner<int>::rex_nested_constraint_variable<int>);

int main() {
  return sfinae_pick(1) + concept_pick(2) + rex_header_type(3) +
         rex_header_value<4>();
}
