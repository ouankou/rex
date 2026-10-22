#include "sage3basic.h"

SgType *SgFunctionParameterRefExp::get_type() const {
  if (p_parameter_number < 0 || p_parameter_levels_up < 0) {
    fprintf(stderr,
            "REX_AST_INVARIANT[function-parameter-reference-coordinate]: "
            "SgFunctionParameterRefExp requires nonnegative coordinates\n");
    ROSE_ABORT();
  }
  SgType *result_type = p_parameter_type;
  if (result_type == nullptr ||
      SageInterface::containsUnknownType(result_type)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[function-parameter-reference-type]: "
            "SgFunctionParameterRefExp has no exact semantic result type\n");
    ROSE_ABORT();
  }
  return result_type;
}
