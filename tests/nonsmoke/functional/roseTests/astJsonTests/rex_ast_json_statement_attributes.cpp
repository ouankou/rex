#include "nodeQuery.h"
#include "rose.h"

#include <array>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  using Attribute = SgStatementAttribute;
  std::array<size_t, Attribute::e_last_statement_attribute_kind> counts{};
  const Rose_STL_Container<SgNode *> statements =
      NodeQuery::querySubTree(project, V_SgAttributedStatement);
  ROSE_ASSERT(statements.size() == 4);
  for (SgNode *node : statements) {
    SgAttributedStatement *statement = isSgAttributedStatement(node);
    ROSE_ASSERT(statement != nullptr);
    statement->validate();
    for (Attribute *attribute : statement->get_attributes()) {
      ROSE_ASSERT(attribute != nullptr);
      attribute->validate();
      ++counts[static_cast<size_t>(attribute->get_kind())];
    }
  }
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_assume] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_likely] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_nomerge] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_loop_hint] == 2);

  AstTests::runAllTests(project);
  return backend(project);
}
