#include "rose.h"

class Visitor : public AstSimpleProcessing {
public:
  void visit(SgNode *n) override;
  size_t copied_induction_declarations = 0;
};

void Visitor::visit(SgNode *n) {
  SgForStatement *forStatement = isSgForStatement(n);
  if (forStatement == NULL)
    return;

  SgTreeCopy tc;
  SgForStatement *copy = isSgForStatement(n->copy(tc));
  ROSE_ASSERT(copy != NULL);

  const SgStatementPtrList &originalInitializers =
      forStatement->get_for_init_stmt()->get_init_stmt();
  const SgStatementPtrList &copiedInitializers =
      copy->get_for_init_stmt()->get_init_stmt();
  ROSE_ASSERT(originalInitializers.size() == copiedInitializers.size());
  for (size_t index = 0; index < originalInitializers.size(); ++index) {
    SgVariableDeclaration *originalDeclaration =
        isSgVariableDeclaration(originalInitializers[index]);
    if (originalDeclaration == NULL)
      continue;
    SgVariableDeclaration *copiedDeclaration =
        isSgVariableDeclaration(copiedInitializers[index]);
    ROSE_ASSERT(copiedDeclaration != NULL);
    ROSE_ASSERT(originalDeclaration->get_variables().size() ==
                copiedDeclaration->get_variables().size());
    for (size_t variableIndex = 0;
         variableIndex < originalDeclaration->get_variables().size();
         ++variableIndex) {
      SgInitializedName *originalName =
          originalDeclaration->get_variables()[variableIndex];
      SgInitializedName *copiedName =
          copiedDeclaration->get_variables()[variableIndex];
      SgVariableDefinition *originalDefinition = originalName->get_definition();
      SgVariableDefinition *copiedDefinition = copiedName->get_definition();
      ROSE_ASSERT(originalName->get_scope() == forStatement);
      ROSE_ASSERT(copiedName->get_scope() == copy);
      ROSE_ASSERT(copiedName->get_scope() != originalName->get_scope());
      ROSE_ASSERT(originalDefinition != NULL);
      ROSE_ASSERT(copiedDefinition != NULL);
      ROSE_ASSERT(copiedDefinition != originalDefinition);
      ROSE_ASSERT(copiedDefinition->get_parent() == copiedName);
      ROSE_ASSERT(copiedDefinition->get_vardefn() == copiedName);
      ROSE_ASSERT(tc.get_copiedNodeMap().at(originalName) == copiedName);
      ROSE_ASSERT(tc.get_copiedNodeMap().at(originalDefinition) ==
                  copiedDefinition);
      ++copied_induction_declarations;
    }
  }

  // A copy root is deliberately detached.  Giving it the original loop's
  // parent without inserting it into that parent's statement list creates a
  // false ownership edge and masks copy defects.
  ROSE_ASSERT(copy->get_parent() == nullptr);
  ROSE_ASSERT(tc.get_copiedNodeMap().at(forStatement) == copy);
}

int main(int argc, char *argv[]) {
  SgProject *sageProject = frontend(argc, argv);
  AstTests::runAllTests(sageProject);
  Visitor v;
  v.traverse(sageProject, postorder);
  ROSE_ASSERT(v.copied_induction_declarations > 0);

  generateAstGraph(sageProject, 4000);

  return 0;
}
