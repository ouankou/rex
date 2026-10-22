#include "detectMacroOrIncludeFileExpansions.h"
#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project->numberOfFiles() == 1);
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  auto &expansions = file->get_macroExpansionMap();
  ROSE_ASSERT(expansions.size() == 2);
  MacroExpansion *expansion = expansions.begin()->second;
  ROSE_ASSERT(expansion != nullptr);
  const auto statements = expansion->associatedStatementVector;
  ROSE_ASSERT(statements.size() == 2);
  ROSE_ASSERT(expansions.at(statements[0]) == expansion);
  ROSE_ASSERT(expansions.at(statements[1]) == expansion);

  SageInterface::removeStatement(statements[0]);
  ROSE_ASSERT(expansions.size() == 1);
  ROSE_ASSERT(expansions.at(statements[1]) == expansion);
  ROSE_ASSERT(expansion->isTransformed);
  ROSE_ASSERT(expansion->associatedStatementVector.size() == 1);
  ROSE_ASSERT(expansion->associatedStatementVector.front() == statements[1]);

  SageInterface::removeStatement(statements[1]);
  ROSE_ASSERT(expansions.empty());
  for (SgStatement *statement : statements) {
    ROSE_ASSERT(statement->get_parent() == nullptr);
  }
  return 0;
}
