#include "rex_frontend_variable_template_semantic_cross_file_constraint.hpp"

template <class T>
inline constexpr bool rex_cross_file_constraint = requires {
  requires(!requires(T value) { value.missing(); } &&
           rex_external_constraint<T> && (sizeof(T) < 32));
};

static_assert(rex_cross_file_constraint<int>);
