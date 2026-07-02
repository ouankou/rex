struct RexTest2026DetachedType;

struct RexTest2026DetachedType {
  int value;
};

int rex_test2026_ast_json_detached_file_type_fixup(
    RexTest2026DetachedType *object) {
  return object->value;
}
