union __anonymous_0xUserUnion {
  int value;
};

enum __anonymous_0xUserEnum { __anonymous_enum_value = 7, rex_named_value = 9 };

#define REX_ANONYMOUS_STRUCT                                                   \
  struct {                                                                     \
    int member;                                                                \
  }
REX_ANONYMOUS_STRUCT rex_first_anonymous;
REX_ANONYMOUS_STRUCT rex_second_anonymous;
#undef REX_ANONYMOUS_STRUCT
static_assert(!__is_same(decltype(rex_first_anonymous),
                         decltype(rex_second_anonymous)),
              "separate macro expansions must retain distinct types");

namespace {
struct RexReopened;
}
namespace {
struct RexReopened {
  int value;
};
RexReopened rex_reopened;
} // namespace

static union {
  int rex_true_anonymous_member;
};

__anonymous_0xUserUnion __anonymous_0xvariable = {rex_named_value};

int rex_anonymous_prefix_identity() {
  rex_true_anonymous_member = __anonymous_enum_value;
  return __anonymous_0xvariable.__anonymous_0xUserUnion::value +
         rex_true_anonymous_member;
}
