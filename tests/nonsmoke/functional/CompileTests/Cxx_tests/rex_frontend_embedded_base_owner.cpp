struct {
  int value;
} rex_embedded_global_class;

enum { rex_embedded_global_enumerator } rex_embedded_global_enum;

struct RexEmbeddedNamedGlobal {
  int value;
} rex_embedded_named_global;

struct RexStandaloneType {
  int value;
};

RexStandaloneType rex_standalone_global;

struct RexEmbeddedOwner {
  struct {
    int value;
  } rex_embedded_field_class;

  enum { rex_embedded_field_enumerator } rex_embedded_field_enum;

  struct RexEmbeddedNamedField {
    int value;
  } rex_embedded_named_field;

  RexStandaloneType rex_standalone_field;
};

int rex_use_embedded_base_owners() {
  RexEmbeddedOwner owner{};
  return rex_embedded_global_class.value +
         static_cast<int>(rex_embedded_global_enum) +
         rex_embedded_named_global.value + rex_standalone_global.value +
         owner.rex_embedded_field_class.value +
         static_cast<int>(owner.rex_embedded_field_enum) +
         owner.rex_embedded_named_field.value +
         owner.rex_standalone_field.value;
}
