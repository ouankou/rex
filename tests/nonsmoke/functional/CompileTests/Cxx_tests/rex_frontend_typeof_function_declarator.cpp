int rex_typeof_target(int value) { return value + 1; }
int rex_typeof_nothrow_target(int value) throw() { return value + 2; }

extern __typeof(rex_typeof_target) rex_typeof_expression_alias;
extern __typeof(rex_typeof_nothrow_target) rex_typeof_nothrow_alias;
extern __typeof(int(int)) rex_typeof_type_alias;

int rex_typeof_use_aliases() {
  return rex_typeof_expression_alias(1) + rex_typeof_nothrow_alias(2) +
         rex_typeof_type_alias(3);
}
