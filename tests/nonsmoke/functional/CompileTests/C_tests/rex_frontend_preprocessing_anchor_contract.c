#define REX_PREPROCESSING_LEADING 1

#include <stddef.h>

extern struct rex_preprocessing_semantic_tag rex_preprocessing_object;

#define REX_PREPROCESSING_BETWEEN 2

struct rex_preprocessing_semantic_tag;

size_t rex_preprocessing_source_value =
    sizeof(struct rex_preprocessing_semantic_tag *);

int rex_preprocessing_same_line_comment(void) {
  int value = 0;
  value = 1; // REX_PREPROCESSING_SAME_LINE_COMMENT
  return value;
}

int rex_preprocessing_group_first,
    // REX_PREPROCESSING_DECLARATOR_BOUNDARY
    rex_preprocessing_group_second;

int rex_preprocessing_conditional_group_required
#if REX_PREPROCESSING_CONDITIONAL_DECLARATOR_ENABLED // REX_PREPROCESSING_CONDITIONAL_DECLARATOR_OPEN
    ,
    rex_preprocessing_conditional_group_optional
#endif // REX_PREPROCESSING_CONDITIONAL_DECLARATOR_CLOSE
    ;

#define REX_PREPROCESSING_TRAILING 3
