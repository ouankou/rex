#include "sage3basic.h"

#include <algorithm>
#include <array>

void SgCudaKernelExecConfig::post_construction_initialization() {
  const std::array<SgExpression *, 4> operands = {p_grid, p_blocks, p_shared,
                                                  p_stream};
  if (p_grid == nullptr || p_blocks == nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[cuda-exec-config-inputs]: grid and "
                    "blocks are required\n");
    ROSE_ABORT();
  }
  for (auto current = operands.begin(); current != operands.end(); ++current) {
    if (*current != nullptr &&
        ((*current)->get_parent() != nullptr ||
         std::find(operands.begin(), current, *current) != current)) {
      fprintf(stderr, "REX_AST_INVARIANT[cuda-exec-config-ownership]: "
                      "operands must be distinct and unowned\n");
      ROSE_ABORT();
    }
  }
  for (SgExpression *operand : operands)
    if (operand != nullptr)
      operand->set_parent(this);
}
