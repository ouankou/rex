#include <stddef.h>

#include "rex_frontend_preprocessing_only_header_owner.h"

#define REX_UNPARSER_SELF REX_UNPARSER_SELF
#define REX_UNPARSER_INCREMENT(value) ((value) + 1)
#define REX_UNPARSER_CONDITIONAL_IF 0

#if defined(REX_UNPARSER_SELF)
int rex_unparser_preprocessing_value(void) {
  return REX_UNPARSER_INCREMENT((int)sizeof(size_t)) +
         REX_FRONTEND_PREPROCESSING_ONLY_VALUE;
}
#endif

#if 0
#elif defined(REX_UNPARSER_SELF)
#else
#endif

int rex_unparser_conditional_if_boundary(int value) {
#if REX_UNPARSER_CONDITIONAL_IF
  if (1)
#else
  if (value != 0 && 0)
#endif
  {
    return 1;
  }

  return 0;
}
