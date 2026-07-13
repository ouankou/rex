int rex_group_global_a = 1, *rex_group_global_b = nullptr,
    rex_group_global_c[2] = {};

#define REX_GROUP_FILE_TERMINATED_DECLARATION                                  \
  int rex_group_file_macro_a = 1, rex_group_file_macro_b = 2
REX_GROUP_FILE_TERMINATED_DECLARATION;

#define REX_GROUP_MACRO_DECLARATION                                            \
  int rex_group_macro_a = 1, rex_group_macro_b = 2;
REX_GROUP_MACRO_DECLARATION

enum {
  rex_group_enum_value = 3
} rex_group_enum_a = rex_group_enum_value,
  rex_group_enum_b = rex_group_enum_value;

namespace rex_group_namespace {
long rex_group_namespace_a = 1, *rex_group_namespace_b = nullptr;
}

struct rex_group_members {
  int rex_group_field_a = 1, *rex_group_field_b = nullptr,
      rex_group_field_c[2] = {};
  static int rex_group_static_a, rex_group_static_b;
};

extern "C" {
int rex_group_linkage_a = 1, rex_group_linkage_b = 2;
}

int rex_frontend_variable_declarator_identity() {
  int rex_group_local_a = 1, *rex_group_local_b = nullptr,
      rex_group_local_c[2] = {};
  int rex_group_independent_a;
  int rex_group_independent_b;

  {
    int rex_group_copy_a = 1, rex_group_copy_b = 2;
    rex_group_independent_a = rex_group_copy_a + rex_group_copy_b;
  }

  for (int rex_group_for_a = 0, rex_group_for_b = 1; rex_group_for_a < 1;
       ++rex_group_for_a) {
    rex_group_independent_b += rex_group_for_b;
  }

  static int rex_group_attribute_a __attribute__((unused)) = 1,
                                   rex_group_attribute_b __attribute__((used)) =
                                       2;
  int rex_group_comment_a = 1, /* exact declarator boundary */
      rex_group_comment_b = 2;

  return rex_group_global_a + *rex_group_global_b + rex_group_global_c[0] +
         rex_group_file_macro_a + rex_group_file_macro_b + rex_group_macro_a +
         rex_group_macro_b + rex_group_enum_a + rex_group_enum_b +
         static_cast<int>(rex_group_namespace::rex_group_namespace_a) +
         rex_group_local_a + *rex_group_local_b + rex_group_local_c[0] +
         rex_group_independent_a + rex_group_independent_b +
         rex_group_attribute_a + rex_group_attribute_b + rex_group_comment_a +
         rex_group_comment_b;
}
