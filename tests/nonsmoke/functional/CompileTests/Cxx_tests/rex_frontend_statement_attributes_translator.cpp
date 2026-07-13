#include "nodeQuery.h"
#include "rose.h"

#include <array>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  using Attribute = SgStatementAttribute;
  using Kind = Attribute::statement_attribute_kind_enum;
  std::array<size_t, Attribute::e_last_statement_attribute_kind> counts{};

  const Rose_STL_Container<SgNode *> statements =
      NodeQuery::querySubTree(project, V_SgAttributedStatement);
  ROSE_ASSERT(statements.size() == 10);
  size_t gnuFallthroughCount = 0;
  for (SgNode *node : statements) {
    SgAttributedStatement *statement = isSgAttributedStatement(node);
    ROSE_ASSERT(statement != nullptr);
    statement->validate();
    ROSE_ASSERT(statement->get_statement()->get_parent() == statement);
    ROSE_ASSERT(statement->get_attribute_list()->get_parent() == statement);

    for (Attribute *attribute : statement->get_attributes()) {
      ROSE_ASSERT(attribute != nullptr);
      attribute->validate();
      ROSE_ASSERT(attribute->get_parent() == statement->get_attribute_list());
      const Kind kind = attribute->get_kind();
      ROSE_ASSERT(kind >= Attribute::e_statement_attribute_fallthrough &&
                  kind < Attribute::e_last_statement_attribute_kind);
      ++counts[static_cast<size_t>(kind)];

      if (kind == Attribute::e_statement_attribute_fallthrough &&
          attribute->get_spelling() ==
              Attribute::e_statement_attribute_spelling_gnu) {
        ++gnuFallthroughCount;
      }
      if (kind == Attribute::e_statement_attribute_assume) {
        ROSE_ASSERT(attribute->get_expression_argument() != nullptr);
        ROSE_ASSERT(attribute->get_expression_argument()->get_parent() ==
                    attribute);
      }
    }
  }

  ROSE_ASSERT(counts[Attribute::e_statement_attribute_fallthrough] == 2);
  ROSE_ASSERT(gnuFallthroughCount == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_likely] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_unlikely] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_assume] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_nomerge] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_musttail] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_always_inline] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_loop_hint] == 3);

  SgAttributedStatement *copy = isSgAttributedStatement(
      SageInterface::deepCopy(isSgAttributedStatement(statements.front())));
  ROSE_ASSERT(copy != nullptr);
  copy->validate();
  ROSE_ASSERT(
      copy->get_attribute_list() !=
      isSgAttributedStatement(statements.front())->get_attribute_list());
  ROSE_ASSERT(copy->get_statement() !=
              isSgAttributedStatement(statements.front())->get_statement());

  AstTests::runAllTests(project);
  return backend(project);
}
