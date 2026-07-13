#ifndef REX_UNPARSE_IMPLICIT_CONTROL_FLOW_HEADER_PLAN_HPP
#define REX_UNPARSE_IMPLICIT_CONTROL_FLOW_HEADER_PLAN_HPP

template <typename T>
constexpr T *rex_unparse_implicit_control_flow_header_plan(T *pointer) {
  if constexpr (sizeof(T) == 0)
    static_assert(sizeof(T) != 0, "object type must have nonzero size");
  return pointer;
}

#endif
