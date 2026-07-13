#include "sage3basic.h"

SgType *SgCAFImageSelectorExp::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[fortran-image-selector-type]: coarray image "
          "selector has no standalone semantic value type\n");
  ROSE_ABORT();
}
