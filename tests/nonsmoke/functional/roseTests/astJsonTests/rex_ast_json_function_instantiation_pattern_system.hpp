#ifndef REX_AST_JSON_FUNCTION_INSTANTIATION_PATTERN_SYSTEM_HPP
#define REX_AST_JSON_FUNCTION_INSTANTIATION_PATTERN_SYSTEM_HPP

template <typename T> int rexAstJsonSystemFunctionLocalInstantiation() {
  struct Local {
    Local() {}
  };
  Local local;
  return sizeof(local);
}

#endif
