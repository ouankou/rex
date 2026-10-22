struct RexTemplateMemberSemanticOwner {
  template <class T> RexTemplateMemberSemanticOwner &operator<<(T value);
};

template <class T> struct RexNestedSemanticOwner {
  template <class U> static constexpr T rex_nested_value = sizeof(U);
  template <class U> static constexpr bool rex_nested_partial_value = false;
  template <class U> static constexpr bool rex_nested_partial_value<U *> = true;
  template <class U> struct RexNestedSemanticMember {
    T outer;
    U inner;
  };
};

RexNestedSemanticOwner<int>::RexNestedSemanticMember<char> rex_nested_a{1, 'a'};
RexNestedSemanticOwner<long>::RexNestedSemanticMember<char> rex_nested_b{2,
                                                                         'b'};
static_assert(sizeof(rex_nested_a.outer) == sizeof(int));
static_assert(sizeof(rex_nested_b.outer) == sizeof(long));
static_assert(RexNestedSemanticOwner<int>::rex_nested_value<char> == 1);
static_assert(RexNestedSemanticOwner<long>::rex_nested_value<char> == 1);
static_assert(RexNestedSemanticOwner<int>::rex_nested_partial_value<char *>);
static_assert(RexNestedSemanticOwner<long>::rex_nested_partial_value<char *>);

template <class T> extern int rex_partial_redeclaration;
template <class T> int rex_partial_redeclaration<T *> = 1;
template <class T> int rex_partial_redeclaration = 0;
int rex_partial_redeclaration_use = rex_partial_redeclaration<int *>;

struct RexExternalPartialOwner {
  template <class T> static int rex_external_partial;
  template <class T> inline static int value = 0;
};
template <class T> int RexExternalPartialOwner::rex_external_partial<T *> = 1;
template <class T> int RexExternalPartialOwner::value<T *> = 1;
static_assert(sizeof(RexExternalPartialOwner::value<int *>) == sizeof(int));
int rex_external_partial_use =
    RexExternalPartialOwner::rex_external_partial<int *>;

struct RexOutOfLinePartialOwner {
  template <class T> static int rex_outofline_partial;
  template <class T> static int rex_outofline_partial<T *>;
};
template <class T> int RexOutOfLinePartialOwner::rex_outofline_partial = 0;
template <class T> int RexOutOfLinePartialOwner::rex_outofline_partial<T *> = 1;
int rex_outofline_partial_use =
    RexOutOfLinePartialOwner::rex_outofline_partial<int *>;

inline int rex_written_inline_variable = 3;
extern int rex_written_inline_variable;
struct RexImplicitInlineOwner {
  static constexpr int rex_implicit_inline_variable = 7;
};
template <class T> inline int rex_inline_template = 0;
template <class T> inline int rex_inline_template<T *> = 1;
template <> inline int rex_inline_template<char> = 2;
extern template int rex_inline_template<int>;
template int rex_inline_template<long>;

#define REX_CONSTEXPR_CONST constexpr const
#define REX_POINTER_CONST *const
REX_CONSTEXPR_CONST int rex_macro_explicit_const = 5;
constexpr int REX_POINTER_CONST rex_macro_explicit_pointer_const = nullptr;
constexpr int *__attribute__((aligned(1 < 2 ? 8 : 16)))
const rex_attribute_explicit_pointer_const = nullptr;
#undef REX_POINTER_CONST
#undef REX_CONSTEXPR_CONST

int rex_template_member_semantic_identity() {
  RexTemplateMemberSemanticOwner owner;
  owner.operator<<(1);
  owner << 2;
  return 0;
}
