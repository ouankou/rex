#include "sage3basic.h"

SgType *SgThrowOp::get_type() const {
  SgType *result_type = SgUnaryOp::get_type();
  if (isSgTypeVoid(result_type) == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[throw-result-type]: throw expression result "
            "type must be void\n");
    ROSE_ABORT();
  }
  return result_type;
}
