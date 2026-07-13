enum RexEnumInitializerReference {
  rex_enum_base = 1,
  rex_enum_derived = rex_enum_base + 2,
};

int rex_unparser_enum_initializer_reference() { return rex_enum_derived; }
