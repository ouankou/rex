#include "sage3basic.h"

SgType *SgSuperExp::get_type() const {
  ROSE_ASSERT(get_class_symbol() != NULL);
  ROSE_ASSERT(get_class_symbol()->get_type() != NULL);

  return SgPointerType::createType(get_class_symbol()->get_type());
}
