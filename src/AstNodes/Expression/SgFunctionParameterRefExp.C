#include "sage3basic.h"

SgType *SgFunctionParameterRefExp::get_type() const {
  SgType *result_type = p_parameter_type;
  if (result_type == nullptr || isSgTypeUnknown(result_type) != nullptr ||
      isSgTypeDefault(result_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[function-parameter-reference-type]: "
            "SgFunctionParameterRefExp has no exact semantic result type\n");
    ROSE_ABORT();
  }
  return result_type;
}
