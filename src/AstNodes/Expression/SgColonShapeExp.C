#include "sage3basic.h"

void SgColonShapeExp::post_construction_initialization() {}

SgType *SgColonShapeExp::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[fortran-shape-syntax-type]: colon shape has no "
          "standalone semantic value type\n");
  ROSE_ABORT();
}
