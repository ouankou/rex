#include "RoseAst.h"
#include "rose.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace {

struct ExpectedUsing {
  std::string owner;
  std::string terminal;
  std::vector<std::string> qualifier_tokens;
  bool inheriting_constructor = false;
};

std::string owningClassName(SgUsingDeclarationStatement *statement) {
  ROSE_ASSERT(statement != nullptr);
  SgClassDefinition *definition = isSgClassDefinition(statement->get_parent());
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(statement->get_scope() == definition);
  ROSE_ASSERT(definition->get_declaration() != nullptr);
  ROSE_ASSERT(std::count(definition->get_members().begin(),
                         definition->get_members().end(), statement) == 1);
  return definition->get_declaration()->get_name().getString();
}

void requireUsing(SgUsingDeclarationStatement *statement,
                  const ExpectedUsing &expected) {
  ROSE_ASSERT(statement != nullptr);
  ROSE_ASSERT(owningClassName(statement) == expected.owner);
  ROSE_ASSERT(statement->get_source_terminal_name().getString() ==
              expected.terminal);
  ROSE_ASSERT(statement->get_source_name_qualification_present());
  ROSE_ASSERT(!statement->get_source_name_global_qualification());
  ROSE_ASSERT(statement->get_source_name_qualification_tokens() ==
              expected.qualifier_tokens);
  ROSE_ASSERT(statement->get_is_inheriting_constructor() ==
              expected.inheriting_constructor);
  ROSE_ASSERT((statement->get_declaration() != nullptr) !=
              (statement->get_initializedName() != nullptr));

  Sg_File_Info *source = statement->get_startOfConstruct();
  ROSE_ASSERT(source != nullptr);
  ROSE_ASSERT(source->get_line() > 0);
  ROSE_ASSERT(!source->isCompilerGenerated());
  ROSE_ASSERT(!source->isTransformation());
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  const std::vector<ExpectedUsing> expected = {
      {"RexUsingDerived", "rex_call", {"RexUsingOverloads::"}, false},
      {"RexUsingDerived", "operator()", {"RexUsingOverloads::"}, false},
      {"RexUsingDependent", "T", {"T::"}, true},
      {"RexUsingDependent", "rex_call", {"T::"}, false},
      {"RexUsingConcrete",
       "RexUsingTemplateBase",
       {"RexUsingTemplateBase<long>::"},
       true},
      {"RexUsingConcrete", "rex_call", {"RexUsingTemplateBase<long>::"}, false},
  };

  std::multimap<std::pair<std::string, std::string>,
                SgUsingDeclarationStatement *>
      statements;
  for (SgNode *node : RoseAst(project)) {
    SgUsingDeclarationStatement *statement =
        isSgUsingDeclarationStatement(node);
    if (statement == nullptr) {
      continue;
    }
    const std::string owner = owningClassName(statement);
    if (owner.rfind("RexUsing", 0) != 0) {
      continue;
    }
    const std::string terminal =
        statement->get_source_terminal_name().getString();
    statements.emplace(std::make_pair(owner, terminal), statement);
  }

  ROSE_ASSERT(statements.size() == expected.size());
  for (const ExpectedUsing &entry : expected) {
    const auto range =
        statements.equal_range(std::make_pair(entry.owner, entry.terminal));
    ROSE_ASSERT(std::distance(range.first, range.second) == 1);
    requireUsing(range.first->second, entry);
  }

  AstTests::runAllTests(project);
  return backend(project);
}
