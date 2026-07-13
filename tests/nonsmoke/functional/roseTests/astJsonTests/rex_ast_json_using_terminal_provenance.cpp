#include "RoseAst.h"
#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

struct ExpectedUsing {
  std::string owner;
  std::string terminal;
  bool inheriting_constructor = false;
};

const std::vector<ExpectedUsing> expectedUsingDeclarations = {
    {"RexJsonUsingDerived", "rex_call", false},
    {"RexJsonUsingDerived", "operator ()", false},
    {"RexJsonUsingDependent", "T", true},
    {"RexJsonUsingDependent", "rex_call", false},
    {"RexJsonUsingConcrete", "RexJsonUsingTemplateBase", true},
    {"RexJsonUsingConcrete", "rex_call", false},
};

SgSourceFile *sourceFile(SgProject *project) {
  SgSourceFile *result = nullptr;
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source = isSgSourceFile(file);
    if (source != nullptr && !source->get_isHeaderFile()) {
      ROSE_ASSERT(result == nullptr);
      result = source;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

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

void requireUsingDeclarations(SgProject *project) {
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
    if (owner.rfind("RexJsonUsing", 0) != 0) {
      continue;
    }
    const std::string terminal =
        statement->get_source_terminal_name().getString();
    statements.emplace(std::make_pair(owner, terminal), statement);
  }

  ROSE_ASSERT(statements.size() == expectedUsingDeclarations.size());
  for (const ExpectedUsing &expected : expectedUsingDeclarations) {
    const auto range = statements.equal_range(
        std::make_pair(expected.owner, expected.terminal));
    ROSE_ASSERT(std::distance(range.first, range.second) == 1);
    SgUsingDeclarationStatement *statement = range.first->second;
    ROSE_ASSERT(statement->get_is_inheriting_constructor() ==
                expected.inheriting_constructor);
    ROSE_ASSERT(statement->get_source_name_qualification_present());
    ROSE_ASSERT(!statement->get_source_name_qualification_tokens().empty());
    ROSE_ASSERT((statement->get_declaration() != nullptr) !=
                (statement->get_initializedName() != nullptr));
  }
}

void roundTrip(SgProject *project) {
  using namespace Rose::AstJson;
  SgSourceFile *source = sourceFile(project);
  constexpr Checkpoint checkpoint = Checkpoint::PreOmpConstruction;
  AstFileRecord ast = parseAstFileJson(buildJson(source, checkpoint, source),
                                       checkpointName(checkpoint));

  std::multimap<std::pair<std::string, bool>, const NodeRecord *> records;
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgUsingDeclarationStatement") {
      continue;
    }
    const std::string terminal =
        record.properties.requiredString("source_terminal_name");
    const bool inheriting =
        record.properties.requiredBool("is_inheriting_constructor");
    records.emplace(std::make_pair(terminal, inheriting), &record);
  }
  ROSE_ASSERT(records.size() == expectedUsingDeclarations.size());
  for (const ExpectedUsing &expected : expectedUsingDeclarations) {
    const auto range = records.equal_range(
        std::make_pair(expected.terminal, expected.inheriting_constructor));
    const std::size_t expected_count =
        expected.terminal == "rex_call" && !expected.inheriting_constructor ? 3
                                                                            : 1;
    ROSE_ASSERT(static_cast<std::size_t>(std::distance(
                    range.first, range.second)) == expected_count);
  }

  SgSourceFile *copy = reconstructSourceFile(ast, source);
  replaceFileInProject(source, copy);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  requireUsingDeclarations(project);
  roundTrip(project);
  requireUsingDeclarations(project);

  AstTests::runAllTests(project);
  return backend(project);
}
