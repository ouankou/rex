#include "sage3basic.h"

void SgAssumedRankExp::post_construction_initialization() {}

SgType *SgAssumedRankExp::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[fortran-shape-syntax-type]: assumed-rank shape "
          "has no standalone semantic value type\n");
  ROSE_ABORT();
}
