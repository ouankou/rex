#include "rex_frontend_variable_template_explicit_instantiation_semantic_initializer.hpp"

namespace rex_explicit_instantiation_owner {
template <class T>
inline constexpr bool rex_explicit_instantiation_lexical_scope = sizeof(T) > 0;
}

struct rex_explicit_instantiation_global_operand {};

template const bool
    rex_explicit_instantiation_owner::rex_explicit_instantiation_lexical_scope<
        rex_explicit_instantiation_global_operand>;

template <class T> struct rex_explicit_instantiation_member_owner {
  template <class U> static int rex_explicit_instantiation_member_value;
};

template <class T>
template <class U>
int rex_explicit_instantiation_member_owner<
    T>::rex_explicit_instantiation_member_value = sizeof(T) + sizeof(U);

template int rex_explicit_instantiation_member_owner<
    int>::rex_explicit_instantiation_member_value<double>;

extern template const bool
    rex_explicit_instantiation_semantic_initializer<long>;
template const bool rex_explicit_instantiation_semantic_initializer<int>;

static_assert(rex_explicit_instantiation_semantic_initializer<int>);
static_assert(
    rex_explicit_instantiation_owner::rex_explicit_instantiation_lexical_scope<
        rex_explicit_instantiation_global_operand>);
