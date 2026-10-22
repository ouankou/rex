#include "sage3basic.h"

void SgCudaKernelCallExp::post_construction_initialization() {
  if (p_exec_config == nullptr || p_exec_config->get_parent() != nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[cuda-exec-config-ownership]: execution "
                    "configuration must be nonnull and exclusively owned\n");
    ROSE_ABORT();
  }
  SgFunctionCallExp::post_construction_initialization();
  p_exec_config->set_parent(this);
}

SgType *SgCudaKernelCallExp::get_type() const {
  return SgFunctionCallExp::get_type();
}
