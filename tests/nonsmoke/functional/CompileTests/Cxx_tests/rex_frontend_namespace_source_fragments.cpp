#include "rex_frontend_namespace_source_fragments.hpp"

} // namespace rex_nested_split_fragment
} // namespace rex_split_fragment

int rex_after_split_namespace = 1;

#define REX_OPEN_NAMESPACE(NAME) namespace NAME {
#define REX_CLOSE_NAMESPACE() }

REX_OPEN_NAMESPACE(rex_macro_fragment)
int macro_value;
REX_CLOSE_NAMESPACE()

#include <vector>

namespace rex_inline_owner {
inline namespace rex_inline_fragment {
int inline_value;
}
} // namespace rex_inline_owner

namespace rex_reopened_fragment {
int first_value;
}
namespace rex_reopened_fragment {
int second_value;
}

namespace rex_reopened_function_fragment {
int rex_cross_fragment(int value);
}
namespace rex_reopened_function_fragment {
int rex_cross_fragment(int value);
}
namespace rex_reopened_function_fragment {
int rex_cross_fragment(int value) { return value; }
}

// Every token in these two reopenings maps to the same physical macro
// invocation coordinate.  Their immutable expanded-token orders are the only
// exact identities that can select the proper namespace fragment.
#define REX_TWO_NAMESPACE_REOPENINGS()                                         \
  namespace rex_macro_reopened_fragment {                                      \
  int macro_first_value;                                                       \
  }                                                                            \
  namespace rex_macro_reopened_fragment {                                      \
  int macro_second_value;                                                      \
  }
REX_TWO_NAMESPACE_REOPENINGS()

template <bool Value> struct rex_semantic_boolean_argument {};
rex_semantic_boolean_argument<true> rex_semantic_boolean_instance;

class rex_friend_parameter_owner {
  friend void
  rex_friend_parameter_function(class rex_friend_parameter_tag &value);
};
class rex_friend_parameter_tag {};

int main() {
  std::vector<int> values = {1, 2, 3};
  return rex_split_fragment::rex_nested_split_fragment::split_value +
         rex_macro_fragment::macro_value + rex_inline_owner::inline_value +
         rex_reopened_fragment::first_value +
         rex_reopened_fragment::second_value + rex_after_split_namespace +
         rex_reopened_function_fragment::rex_cross_fragment(4) +
         rex_macro_reopened_fragment::macro_first_value +
         rex_macro_reopened_fragment::macro_second_value + values.front();
}

#include "rex_frontend_namespace_split_introducer.hpp"
rex_split_introducer { int rex_split_introducer_value; }
