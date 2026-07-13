__attribute__((visibility("hidden"))) int rex_visibility_hidden();
__attribute__((visibility("protected"))) int rex_visibility_protected();
__attribute__((visibility("default"))) int rex_visibility_default();
__attribute__((visibility("internal"))) int rex_visibility_internal();
__attribute__((visibility("hidden"))) int rex_visibility_hidden_variable = 1;

#define REX_VISIBILITY_KIND "hidden"
#define REX_VISIBILITY_ATTRIBUTE(kind) __attribute__((visibility(#kind)))
#define REX_TYPE_VISIBILITY_ATTRIBUTE(kind)                                    \
  __attribute__((type_visibility(#kind)))

__attribute__((visibility(REX_VISIBILITY_KIND))) int
rex_visibility_macro_hidden();
REX_VISIBILITY_ATTRIBUTE(default) int rex_visibility_macro_default();
REX_VISIBILITY_ATTRIBUTE(internal) int rex_visibility_macro_internal();

struct __attribute__((visibility("hidden"))) RexVisibilityHiddenType {
  int value;
};

struct __attribute__((type_visibility("hidden"))) RexTypeVisibilityHidden {
  int value;
};

struct __attribute__((type_visibility("internal"))) RexTypeVisibilityInternal {
  int value;
};

struct REX_TYPE_VISIBILITY_ATTRIBUTE(protected)
    RexTypeVisibilityMacroProtected {
  int value;
};

int main() {
  RexVisibilityHiddenType value{0};
  RexTypeVisibilityHidden hidden{0};
  RexTypeVisibilityInternal internal{0};
  RexTypeVisibilityMacroProtected macro_protected{0};
  return value.value + hidden.value + internal.value + macro_protected.value +
         rex_visibility_hidden_variable + rex_visibility_internal() +
         rex_visibility_macro_hidden() + rex_visibility_macro_default() +
         rex_visibility_macro_internal();
}
