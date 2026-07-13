#include "sage3basic.h"

void SgAsteriskShapeExp::post_construction_initialization() {}

SgType *SgAsteriskShapeExp::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[fortran-shape-syntax-type]: asterisk shape has "
          "no standalone semantic value type\n");
  ROSE_ABORT();
}
