template <typename T, typename Iter = T *> struct RexTest2026PriorDefault;

template <typename T, typename Iter> struct RexTest2026PriorDefault {
  Iter begin;
  T value;
};

int rex_test2026_template_definition_prior_default() {
  int value = 1;
  RexTest2026PriorDefault<int> item = {&value, value};
  return item.value + *item.begin;
}

template <class T> struct RexMacroInheritedBase {};

#define REX_MACRO_INHERITED_DEFAULTS                                           \
  template <class T = int> struct RexMacroInheritedType;                       \
  template <class T> struct RexMacroInheritedType {                            \
    T value;                                                                   \
  };                                                                           \
  template <int N = 3> struct RexMacroInheritedValue;                          \
  template <int N> struct RexMacroInheritedValue {                             \
    int values[N];                                                             \
  };                                                                           \
  template <template <class> class T = RexMacroInheritedBase>                  \
  struct RexMacroInheritedTemplate;                                            \
  template <template <class> class T> struct RexMacroInheritedTemplate {       \
    T<int> value;                                                              \
  };

REX_MACRO_INHERITED_DEFAULTS

RexMacroInheritedType<> rex_macro_inherited_type;
RexMacroInheritedValue<> rex_macro_inherited_value;
RexMacroInheritedTemplate<> rex_macro_inherited_template;

#define REX_MACRO_VARIABLE_DEFAULT                                             \
  template <class T = int> constexpr int rex_macro_variable_default = sizeof(T);

REX_MACRO_VARIABLE_DEFAULT
static_assert(rex_macro_variable_default<> == sizeof(int));
