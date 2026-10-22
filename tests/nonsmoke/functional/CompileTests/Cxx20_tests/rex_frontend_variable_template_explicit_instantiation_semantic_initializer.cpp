#include "rex_frontend_variable_template_explicit_instantiation_semantic_initializer.hpp"

namespace rex_explicit_instantiation_owner {
template <class T>
inline constexpr bool rex_explicit_instantiation_lexical_scope = sizeof(T) > 0;
}

struct rex_explicit_instantiation_global_operand {};

template <class T>
constexpr int rex_template_self_extent = sizeof(rex_template_self_extent<T>);
static_assert(rex_template_self_extent<int> == sizeof(int));

template <class T> inline thread_local int rex_template_thread_storage = 1;
template <class T> inline thread_local int rex_template_thread_storage<T *> = 2;
template <> inline thread_local int rex_template_thread_storage<void> = 3;
template <class T> inline __thread int rex_template_gnu_thread_storage = 1;
template <class T> inline __thread int rex_template_gnu_thread_storage<T *> = 2;
template <> inline __thread int rex_template_gnu_thread_storage<void> = 3;

int rex_template_thread_storage_uses() {
  return rex_template_thread_storage<int> + rex_template_thread_storage<int *> +
         rex_template_thread_storage<void> +
         rex_template_gnu_thread_storage<int> +
         rex_template_gnu_thread_storage<int *> +
         rex_template_gnu_thread_storage<void>;
}

template const bool
    rex_explicit_instantiation_owner::rex_explicit_instantiation_lexical_scope<
        rex_explicit_instantiation_global_operand>;

template <class T> struct rex_explicit_instantiation_member_owner {
  template <class U> static int rex_explicit_instantiation_member_value;
  template <class U> static constexpr int rex_member_partial = 1;
  template <class U> static constexpr int rex_member_partial<U *> = sizeof(T);
};

static_assert(
    rex_explicit_instantiation_member_owner<int>::rex_member_partial<int *> ==
    sizeof(int));

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

template <class T>
inline constexpr bool rex_explicit_instantiation_partial_owner = false;
template <class T>
inline constexpr bool rex_explicit_instantiation_partial_owner<T *> = true;
template const bool rex_explicit_instantiation_partial_owner<int *>;
static_assert(rex_explicit_instantiation_partial_owner<int *>);
static_assert(!rex_explicit_instantiation_partial_owner<int>);

template <class T> inline constexpr int rex_reference_storage_use = 3;
struct rex_reference_storage_owner {
  template <class T> static constexpr int rex_reference_storage_use = 3;
};

int rex_reference_storage_roles(rex_reference_storage_owner &owner) {
  return rex_reference_storage_use<int> + *&rex_reference_storage_use<int> +
         owner.rex_reference_storage_use<int> +
         *&owner.rex_reference_storage_use<int>;
}
