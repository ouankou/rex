#include "rose.h"

int main() {
  SgIntVal *expanded = SageBuilder::buildIntVal_nfi(1);
  ROSE_ASSERT(expanded != nullptr);

  SgMacroExpansionExp *macro =
      new SgMacroExpansionExp("REX_CFG_VALUE", expanded);
  ROSE_ASSERT(macro != nullptr);
  expanded->set_parent(macro);

  SgExprStatement *statement = SageBuilder::buildExprStatement(macro);
  ROSE_ASSERT(statement != nullptr);
  SgBasicBlock *block = SageBuilder::buildBasicBlock(statement);
  ROSE_ASSERT(block != nullptr);
  ROSE_ASSERT(statement->get_parent() == block);

  ROSE_ASSERT(macro->cfgIndexForEnd() == 1);
  ROSE_ASSERT(macro->cfgFindChildIndex(expanded) == 0);
  ROSE_ASSERT(macro->cfgOutEdges(0).size() == 1);
  ROSE_ASSERT(macro->cfgOutEdges(1).size() == 1);
  ROSE_ASSERT(macro->cfgInEdges(0).size() == 1);
  ROSE_ASSERT(macro->cfgInEdges(1).size() == 1);
  return 0;
}
