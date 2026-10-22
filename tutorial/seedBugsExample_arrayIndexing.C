// Seed out-of-bounds accesses in direct fixed-size array references.

#include "rose.h"
using namespace SageBuilder;

namespace SeedBugsArrayIndexing {

class BugSeeding : public AstSimpleProcessing {
public:
  void visit(SgNode *astNode) override;
};

void BugSeeding::visit(SgNode *astNode) {
  SgPntrArrRefExp *arrayReference = isSgPntrArrRefExp(astNode);
  if (arrayReference == nullptr)
    return;

  SgVarRefExp *arrayVarRef = isSgVarRefExp(arrayReference->get_lhs_operand());
  ROSE_ASSERT(arrayVarRef != nullptr);
  ROSE_ASSERT(arrayVarRef->get_symbol() != nullptr);
  SgInitializedName *arrayName =
      isSgInitializedName(arrayVarRef->get_symbol()->get_declaration());
  ROSE_ASSERT(arrayName != nullptr);
  SgArrayType *arrayType = isSgArrayType(arrayName->get_type());
  ROSE_ASSERT(arrayType != nullptr);
  SgExpression *arraySize = arrayType->get_index();
  ROSE_ASSERT(arraySize != nullptr);
  SgExpression *indexExpression = arrayReference->get_rhs_operand();
  ROSE_ASSERT(indexExpression != nullptr);

  SgTreeCopy copyHelp;
  SgExpression *arraySizeCopy = isSgExpression(arraySize->copy(copyHelp));
  SgExpression *indexCopy = isSgExpression(indexExpression->copy(copyHelp));
  ROSE_ASSERT(arraySizeCopy != nullptr);
  ROSE_ASSERT(indexCopy != nullptr);

  SgExpression *newIndexExpression =
      buildAddOp(indexCopy, arraySizeCopy, indexExpression->get_type());
  SageInterface::replaceExpression(indexExpression, newIndexExpression);
}
} // namespace SeedBugsArrayIndexing

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  SeedBugsArrayIndexing::BugSeeding treeTraversal;
  treeTraversal.traverseInputFiles(project, preorder);

  AstTests::runAllTests(project);

  // Output the new code seeded with a specific form of bug.
  return backend(project);
}
