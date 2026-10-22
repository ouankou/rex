#include "sage3basic.h"

SgName SgLabelRefExp::get_name() const {
  ROSE_ASSERT(p_symbol != NULL);
  return p_symbol->get_name();
}

int SgLabelRefExp::get_numeric_label_value() const {
  ROSE_ASSERT(p_symbol != NULL);
  return p_symbol->get_numeric_label_value();
}
