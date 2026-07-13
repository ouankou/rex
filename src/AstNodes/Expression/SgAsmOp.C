#include "sage3basic.h"

SgType *SgAsmOp::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[syntax-expression-type]: node=SgAsmOp has no "
          "standalone semantic value type\n");
  ROSE_ABORT();
}
