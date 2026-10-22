#include "SlicingInfo.h"
#include "sage3basic.h"

void SlicingInfo::visit(SgNode *node) {
  SgPragmaDeclaration *declaration = isSgPragmaDeclaration(node);
  if (declaration == nullptr) {
    return;
  }
  SgPragma *pragma = declaration->get_pragma();
  ROSE_ASSERT(pragma != nullptr);
  if (pragma->get_pragma() != "SliceTarget") {
    return;
  }

  SgStatement *target = Rose::getNextStatement(declaration);
  if (target == nullptr) {
    fprintf(stderr, "REX_SLICING_INVARIANT[slice-target]: target pragma has no "
                    "following statement\n");
    ROSE_ABORT();
  }

  if (SgForStatement *loop = isSgForStatement(target)) {
    SgStatement *test = loop->get_test();
    if (test != nullptr && !isSgNullStatement(test)) {
      target = test;
    }
  } else if (SgDoWhileStmt *loop = isSgDoWhileStmt(target)) {
    target = loop->get_condition();
  } else if (SgWhileStmt *loop = isSgWhileStmt(target)) {
    target = loop->get_condition();
  } else if (SgIfStmt *if_statement = isSgIfStmt(target)) {
    if (!if_statement->get_is_if_consteval_statement())
      target = if_statement->get_conditional();
  }
  ROSE_ASSERT(target != nullptr);

  targets.push_back(declaration);
  targets.push_back(target);
}
