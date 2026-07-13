template <class Element, class Allocator> struct RexPartial;

template <class Allocator> struct RexPartial<bool, Allocator> {
  Allocator value;
};

int rex_unparser_partial_specialization_identity() {
  RexPartial<bool, long> partial{9};
  return static_cast<int>(partial.value);
}
