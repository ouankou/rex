#ifndef REX_FRONTEND_LINKAGE_MACRO_HEADER_HPP
#define REX_FRONTEND_LINKAGE_MACRO_HEADER_HPP

#define REX_BEGIN_DECLS extern "C" {
#define REX_END_DECLS }

REX_BEGIN_DECLS
struct RexMacroInlineTag {
  int value;
} rex_macro_inline;

typedef struct RexMacroTypedefTag {
  int value;
} RexMacroTypedef;

void rex_macro_function();
REX_END_DECLS

#endif
