#if !defined(REX_MIXED_REPEAT_NAMESPACE) || !defined(REX_MIXED_REPEAT_PREFIX)
#error "repeat-include declaration identity parameters are required"
#endif

#define REX_MIXED_REPEAT_JOIN_IMPL(lhs, rhs) lhs##rhs
#define REX_MIXED_REPEAT_JOIN(lhs, rhs) REX_MIXED_REPEAT_JOIN_IMPL(lhs, rhs)

namespace REX_MIXED_REPEAT_NAMESPACE {
int REX_MIXED_REPEAT_JOIN(REX_MIXED_REPEAT_PREFIX, _object),
    REX_MIXED_REPEAT_JOIN(REX_MIXED_REPEAT_PREFIX, _function)();
}

#undef REX_MIXED_REPEAT_JOIN
#undef REX_MIXED_REPEAT_JOIN_IMPL
