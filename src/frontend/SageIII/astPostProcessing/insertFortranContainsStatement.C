// tps (01/14/2010) : Switching from rose.h to sage3.
#include "insertFortranContainsStatement.h"
#include "sage3basic.h"
void insertFortranContainsStatement(SgNode *node) {
  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("Fixup Fortran references:");

  // DQ (10/3/2008): This bug in OFP is now fixed so no fixup is required.
  printf("Error: fixup of contains statement no longer required. \n");
  ROSE_ABORT();

  InsertFortranContainsStatement astFixupTraversal;

  // I think the default should be preorder so that the interfaces would be more
  // uniform
  astFixupTraversal.traverse(node, preorder);
}

void InsertFortranContainsStatement::visit(SgNode *node) {
  // DQ (10/3/2008): This bug in OFP is now fixed so no fixup is required.
  printf("Error: fixup of contains statement no longer required. \n");
  ROSE_ABORT();

  // DQ (11/24/2007): Output the current IR node for debugging the traversal of
  // the Fortran AST.
  ROSE_ASSERT(node != NULL);

  SgFunctionDefinition *functionDefinition = isSgFunctionDefinition(node);

  // This is for handling where CONTAINS is required in a function
  if (functionDefinition != NULL) {
    SgBasicBlock *block = functionDefinition->get_body();
    SgStatementPtrList &statementList = block->get_statements();
    SgStatementPtrList::iterator i = statementList.begin();

    bool firstFunctionDeclaration = false;
    bool functionDeclarationSeen = false;

    while (i != statementList.end() && firstFunctionDeclaration == false) {
      SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(*i);

      // DQ (1/20/2008): Note that entry statements should not cause
      // introduction of a contains statement!
      if (isSgEntryStatement(functionDeclaration) != NULL)
        functionDeclaration = NULL;

      if (functionDeclaration != NULL) {
        firstFunctionDeclaration = functionDeclarationSeen == false;
        functionDeclarationSeen = true;

        if (firstFunctionDeclaration == true) {
          // Insert a CONTAINS statement.
          // printf ("Building a contains statement (in function) \n");
          SgContainsStatement *containsStatement = new SgContainsStatement();
          SageInterface::setSourcePosition(containsStatement);
          containsStatement->set_definingDeclaration(containsStatement);

          block->get_statements().insert(i, containsStatement);
          containsStatement->set_parent(block);
          ROSE_ASSERT(containsStatement->get_parent() != NULL);
        }
      }

      i++;
    }
  }
}
