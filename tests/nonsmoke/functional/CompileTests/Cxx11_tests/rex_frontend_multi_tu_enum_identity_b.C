enum RexMultiTuEnum { rex_multi_tu_enum_b = 2 } rex_multi_tu_enum_object_b;

static_assert(rex_multi_tu_enum_b == 2, "translation unit B enum identity");

namespace {
  enum class RexMultiTuInternalEnum { value = 2 };
  struct RexMultiTuInternalRecord {
    int first;
    int second;
  };
  RexMultiTuInternalEnum rex_internal_enum = RexMultiTuInternalEnum::value;
  RexMultiTuInternalRecord rex_internal_record{1, 2};
} // namespace
