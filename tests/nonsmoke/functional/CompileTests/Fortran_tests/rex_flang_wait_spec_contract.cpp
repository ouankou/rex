#include "rose.h"

#include <string>

namespace {

SgVarRefExp *requireVariable(SgExpression *expression, const std::string &name,
                             SgWaitStatement *owner) {
  SgVarRefExp *reference = isSgVarRefExp(expression);
  ROSE_ASSERT(reference != nullptr);
  ROSE_ASSERT(reference->get_parent() == owner);
  ROSE_ASSERT(reference->get_symbol() != nullptr);
  ROSE_ASSERT(reference->get_symbol()->get_name() == name);
  return reference;
}

SgLabelRefExp *requireLabel(SgExpression *expression, int value,
                            SgWaitStatement *owner) {
  SgLabelRefExp *reference = isSgLabelRefExp(expression);
  ROSE_ASSERT(reference != nullptr);
  ROSE_ASSERT(reference->get_parent() == owner);
  ROSE_ASSERT(reference->get_symbol() != nullptr);
  ROSE_ASSERT(reference->get_symbol()->get_numeric_label_value() == value);
  return reference;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  const Rose_STL_Container<SgNode *> waits =
      NodeQuery::querySubTree(project, V_SgWaitStatement);
  ROSE_ASSERT(waits.size() == 1);
  SgWaitStatement *statement = isSgWaitStatement(waits.front());
  ROSE_ASSERT(statement != nullptr);

  requireVariable(statement->get_unit(), "unit_number", statement);
  requireLabel(statement->get_end(), 100, statement);
  requireLabel(statement->get_eor(), 200, statement);
  requireLabel(statement->get_err(), 300, statement);
  requireVariable(statement->get_id(), "request_id", statement);
  requireVariable(statement->get_iomsg(), "message", statement);
  requireVariable(statement->get_iostat(), "status_code", statement);
  ROSE_ASSERT(statement->get_io_stmt_list() == nullptr);

  AstTests::runAllTests(project);
  return 0;
}
