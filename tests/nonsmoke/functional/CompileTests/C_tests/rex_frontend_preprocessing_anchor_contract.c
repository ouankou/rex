#define REX_PREPROCESSING_LEADING 1

#include <stddef.h>

extern struct rex_preprocessing_semantic_tag rex_preprocessing_object;

#define REX_PREPROCESSING_BETWEEN 2

struct rex_preprocessing_semantic_tag;

size_t rex_preprocessing_source_value =
    sizeof(struct rex_preprocessing_semantic_tag *);

#define REX_PREPROCESSING_TRAILING 3
