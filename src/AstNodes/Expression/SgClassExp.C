#include "sage3basic.h"

SgType *SgClassExp::get_type() const {
  ROSE_ASSERT(get_class_symbol() != NULL);
  ROSE_ASSERT(get_class_symbol()->get_type() != NULL);

  return get_class_symbol()->get_type();
}
