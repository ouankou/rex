extern "C" void rex_nonbraced_function();
extern "C" int rex_nonbraced_variable;
extern "C" struct RexNonbracedForward *rex_nonbraced_forward;
extern "C" struct RexNonbracedInlineTag {
  int value;
} rex_nonbraced_inline;
extern "C" enum RexNonbracedEnum { rex_nonbraced_value } rex_nonbraced_enum;
extern "C" typedef struct RexNonbracedTypedefTag {
  int value;
} RexNonbracedTypedef;

extern "C" {
void rex_braced_function();
int rex_braced_variable;
struct RexBracedForward;
struct RexBracedInlineTag {
  int value;
} rex_braced_inline;
enum RexBracedEnum { rex_braced_value } rex_braced_enum;
typedef struct RexBracedTypedefTag {
  int value;
} RexBracedTypedef;
union RexBracedStandaloneTag {
  int value;
};
typedef union RexBracedStandaloneTag RexBracedStandaloneAlias;
}

#include "rex_frontend_linkage_macro_header.hpp"
