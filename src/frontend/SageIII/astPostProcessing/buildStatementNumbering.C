#include "sage3basic.h"

void SgGlobal::buildStatementNumbering() {
  class NumberStatementsTraversal : public AstSimpleProcessing {
  public:
    void visit(SgNode *node) override {
      if (SgStatement *statement = isSgStatement(node)) {
        statement->set_source_sequence_value(statementCounter++);
      }
    }

  private:
    int statementCounter = 0;
  };

  TimingPerformance timer(
      "Statement sequence numbering (name qualification support):");
  NumberStatementsTraversal traversal;
  traversal.traverse(this, preorder);
}
