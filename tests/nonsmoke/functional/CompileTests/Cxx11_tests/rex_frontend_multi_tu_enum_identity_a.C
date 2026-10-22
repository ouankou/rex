enum RexMultiTuEnum { rex_multi_tu_enum_a = 1 } rex_multi_tu_enum_object_a;

static_assert(rex_multi_tu_enum_a == 1, "translation unit A enum identity");

namespace {
  enum class RexMultiTuInternalEnum { value = 1 };
  struct RexMultiTuInternalRecord {
    int first;
  };
  RexMultiTuInternalEnum rex_internal_enum = RexMultiTuInternalEnum::value;
  RexMultiTuInternalRecord rex_internal_record{1};
} // namespace
