struct RexExistingTag {
  int value;
};

int rex_expression_tag_ownership(void *raw) {
  enum {
    rex_standalone_anonymous = 7,
  };
  unsigned long size = sizeof(struct RexSizeTag { int value; });
  unsigned long alignment = _Alignof(enum RexAlignTag{
      rex_align_zero,
      rex_align_one,
  });
  void *inline_cast = (struct RexCastTag { int value; } *)raw;
  void *anonymous_cast = (struct {
    char byte;
    unsigned int value;
  } *)raw;
  void *compound_literal = &(struct RexCompoundTag { int value; }){1};

  unsigned long existing_size = sizeof(struct RexExistingTag);
  unsigned long existing_alignment = _Alignof(struct RexExistingTag);
  void *existing_cast = (struct RexExistingTag *)raw;
  return rex_standalone_anonymous == 0 || size == 0 || alignment == 0 ||
         inline_cast == existing_cast || anonymous_cast == existing_cast ||
         compound_literal == raw || existing_size == 0 ||
         existing_alignment == 0;
}
