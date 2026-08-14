#define REX_OUTLINER_REPEAT_TARGET                                             \
  "rex_outliner_repeated_macro_include_owner_a.hpp"
#include "rex_outliner_repeated_macro_include_owner_dispatch.hpp"
#undef REX_OUTLINER_REPEAT_TARGET

#define REX_OUTLINER_REPEAT_TARGET                                             \
  "rex_outliner_repeated_macro_include_owner_b.hpp"
#include "rex_outliner_repeated_macro_include_owner_dispatch.hpp"
#undef REX_OUTLINER_REPEAT_TARGET

int rex_outliner_repeated_macro_include_owner() {
  int result = 0;
#pragma rose_outline
  {
    result = rex_outliner_repeated_macro_include_owner_a() +
             rex_outliner_repeated_macro_include_owner_b();
  }
  return result;
}
