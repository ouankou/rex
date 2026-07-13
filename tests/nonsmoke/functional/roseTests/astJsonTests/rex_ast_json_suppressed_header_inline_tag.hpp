#ifndef REX_AST_JSON_SUPPRESSED_HEADER_INLINE_TAG_HPP
#define REX_AST_JSON_SUPPRESSED_HEADER_INLINE_TAG_HPP

typedef struct RexSuppressedHeaderState {
  int mode;
  union {
    unsigned int wide;
    unsigned char bytes[4];
  } rex_suppressed_nested_value;
} RexSuppressedHeaderState;

template <typename T> struct RexSuppressedHeaderTemplateState {
  enum { rex_suppressed_template_enum_value = sizeof(T) };
};

using RexSuppressedHeaderTemplateInt =
    RexSuppressedHeaderTemplateState<unsigned int>;

#endif
