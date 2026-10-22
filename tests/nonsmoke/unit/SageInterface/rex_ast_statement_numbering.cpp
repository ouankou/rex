#include "rose.h"

namespace {
class CheckStatementNumbers : public AstSimpleProcessing {
public:
  void visit(SgNode *node) override {
    if (SgStatement *statement = isSgStatement(node)) {
      ROSE_ASSERT(statement->get_source_sequence_value() == next++);
    }
  }

  int next = 0;
};
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  SgGlobal *global = file->get_globalScope();
  ROSE_ASSERT(global != nullptr);

  global->buildStatementNumbering();
  CheckStatementNumbers first;
  first.traverse(global, preorder);
  ROSE_ASSERT(first.next > 5);

  // Repeated numbering restarts at the global scope and preserves parentage.
  global->buildStatementNumbering();
  CheckStatementNumbers second;
  second.traverse(global, preorder);
  ROSE_ASSERT(first.next == second.next);
  ROSE_ASSERT(global->get_parent() == file);
  return 0;
}
