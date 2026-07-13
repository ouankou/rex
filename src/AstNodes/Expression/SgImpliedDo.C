#include "sage3basic.h"

void SgImpliedDo::post_construction_initialization() {}

SgType *SgImpliedDo::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[implied-do-type]: implied-do is a control/list "
          "construct and has no standalone expression type\n");
  ROSE_ABORT();
}
