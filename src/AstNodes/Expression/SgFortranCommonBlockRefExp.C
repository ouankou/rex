#include "sage3basic.h"

SgType *SgFortranCommonBlockRefExp::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[fortran-common-block-designator-type]: "
          "common-block designator /%s/ is directive syntax, not a value "
          "expression, and has no semantic value type\n",
          get_use_name().str());
  ROSE_ABORT();
}
