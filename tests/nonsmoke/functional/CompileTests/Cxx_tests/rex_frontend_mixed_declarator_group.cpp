int rex_mixed_global_object, rex_mixed_global_function();
int rex_mixed_global_function_first(), rex_mixed_global_object_second;
int rex_mixed_function_a(), rex_mixed_function_b();
int rex_mixed_function_prefix_first(), rex_mixed_function_prefix_middle(int),
    rex_mixed_function_prefix_last();

namespace rex_mixed_reopened_namespace {
int rex_mixed_reopened_seed;
}
namespace rex_mixed_reopened_namespace {
int rex_mixed_reopened_object, rex_mixed_reopened_function();
}

typedef int rex_mixed_typedef_scalar, rex_mixed_typedef_function();
typedef struct RexMixedEmbeddedRecord {
  int value;
} rex_mixed_embedded_first, rex_mixed_embedded_second;

#include "rex_frontend_suppressed_header_local_group.hpp"

#define REX_MIXED_REPEAT_NAMESPACE rex_mixed_repeat_first_namespace
#define REX_MIXED_REPEAT_PREFIX rex_mixed_repeat_first
#include "rex_frontend_mixed_declarator_group_repeat.hpp"
#undef REX_MIXED_REPEAT_PREFIX
#undef REX_MIXED_REPEAT_NAMESPACE

#define REX_MIXED_REPEAT_NAMESPACE rex_mixed_repeat_second_namespace
#define REX_MIXED_REPEAT_PREFIX rex_mixed_repeat_second
#include "rex_frontend_mixed_declarator_group_repeat.hpp"
#undef REX_MIXED_REPEAT_PREFIX
#undef REX_MIXED_REPEAT_NAMESPACE

struct RexMixedDeclaratorOwner {
  int rex_read_later_grouped_field() const {
    return rex_mixed_later_field_second;
  }
  int rex_call_later_grouped_method() {
    return rex_mixed_later_method_second();
  }
  int rex_mixed_field, rex_mixed_method();
  int rex_mixed_pointer_field, *rex_mixed_pointer_method();
  int rex_mixed_later_field_first, rex_mixed_later_field_second;
  int rex_mixed_later_method_first(), rex_mixed_later_method_second();
};

int rex_mixed_driver() {
  int rex_mixed_local, rex_mixed_block_function();
  extern int rex_mixed_local_extern_object, rex_mixed_local_extern_function();
  for (int rex_mixed_for_object, rex_mixed_for_function(); false;) {
  }
  return 0;
}
