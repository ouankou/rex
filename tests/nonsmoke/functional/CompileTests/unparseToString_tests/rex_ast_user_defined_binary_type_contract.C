#include "rose.h"

int main() {
  SgUserDefinedBinaryOp *expression = new SgUserDefinedBinaryOp(
      SageBuilder::buildIntVal(1), SageBuilder::buildIntVal(2), nullptr,
      SgName(".missing."), nullptr);
  ROSE_ASSERT(expression != nullptr);
  (void)expression->get_type();
  fprintf(stderr, "A user-defined binary operator accepted no result type.\n");
  ROSE_ABORT();
}
