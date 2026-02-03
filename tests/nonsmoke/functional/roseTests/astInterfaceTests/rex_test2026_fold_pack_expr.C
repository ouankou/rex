#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> folds =
      NodeQuery::querySubTree(project, V_SgFoldExpression);
  ROSE_ASSERT(!folds.empty());

  bool found_sizeof_pack = false;
  Rose_STL_Container<SgNode *> sizeops =
      NodeQuery::querySubTree(project, V_SgSizeOfOp);
  for (SgNode *node : sizeops) {
    if (SgSizeOfOp *sizeof_op = isSgSizeOfOp(node)) {
      if (sizeof_op->get_is_sizeof_pack()) {
        found_sizeof_pack = true;
        ROSE_ASSERT(sizeof_op->get_operand_expr() != nullptr);
        break;
      }
    }
  }
  ROSE_ASSERT(found_sizeof_pack);

  Rose_STL_Container<SgNode *> noexcept_ops =
      NodeQuery::querySubTree(project, V_SgNoexceptOp);
  ROSE_ASSERT(!noexcept_ops.empty());

  return 0;
}
