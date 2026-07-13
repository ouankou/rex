int rex_json_group_a, rex_json_group_b, rex_json_group_c;
int rex_json_group_object, rex_json_group_function();
int rex_json_group_function_a(), rex_json_group_function_b();
typedef int rex_json_group_typedef_scalar, rex_json_group_typedef_function();

#define REX_JSON_GROUP_MACRO                                                   \
  int rex_json_group_macro_object, rex_json_group_macro_function();
REX_JSON_GROUP_MACRO

int main() { return 0; }
