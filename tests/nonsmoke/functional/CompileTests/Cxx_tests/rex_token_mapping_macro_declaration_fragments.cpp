#define REX_DECLARE_PAIR(prefix)                                               \
  int prefix##_first = 1;                                                      \
  int prefix##_second = 2

REX_DECLARE_PAIR(rex_global);
int rex_regular = 3;

int rex_macro_declaration_fragments() {
  REX_DECLARE_PAIR(rex_local);
  return rex_global_first + rex_global_second + rex_local_first +
         rex_local_second + rex_regular;
}
