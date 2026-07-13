struct rex_initializer_include_record {
  const char *name;
  int value;
};

#define REX_INITIALIZER_INCLUDE_ENTRY(NAME, VALUE) {NAME, VALUE},

static const struct rex_initializer_include_record
    rex_initializer_include_records[] = {
#include "rex_frontend_initializer_include_entries.def"
        {"source-tail", 33}};

static const int rex_scalar_initializer_include_value =
#include "rex_frontend_scalar_initializer_value.def"
    ;

static const int rex_complete_initializer_include_value
#include "rex_frontend_complete_initializer_value.def"
    ;

#undef REX_INITIALIZER_INCLUDE_ENTRY

int main(void) {
  return rex_initializer_include_records[0].value != 11 ||
         rex_initializer_include_records[2].value != 33 ||
         rex_scalar_initializer_include_value != 44 ||
         rex_complete_initializer_include_value != 55;
}
