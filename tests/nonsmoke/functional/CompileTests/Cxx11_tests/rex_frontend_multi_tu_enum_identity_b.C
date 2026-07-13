enum RexMultiTuEnum { rex_multi_tu_enum_b = 2 } rex_multi_tu_enum_object_b;

static_assert(rex_multi_tu_enum_b == 2, "translation unit B enum identity");
