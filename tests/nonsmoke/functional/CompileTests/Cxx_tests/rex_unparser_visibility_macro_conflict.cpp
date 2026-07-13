#define REX_CONFLICTING_VISIBILITY(kind) __attribute__((visibility("hidden")))

REX_CONFLICTING_VISIBILITY(internal)
int rex_visibility_macro_conflict();
