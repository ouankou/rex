#include "RoseAst.h"
#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <stdexcept>
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

  AstFileRecord missingTarget = ast;
  bool corrupted = false;
  for (NodeRecord &record : missingTarget.nodes) {
    if (record.kind != "SgUsingDeclarationStatement")
      continue;
    record.properties.object.at("declaration") = parseJson("0");
    record.properties.object.at("initialized_name") = parseJson("0");
    for (EdgeRecord &edge : record.edges) {
      if (edge.field == "declaration" || edge.field == "initializedName")
        edge.target = 0;
    }
    corrupted = true;
    break;
  }
  ROSE_ASSERT(corrupted);
  const size_t usingCount = SgUsingDeclarationStatement::numberOfNodes();
  bool rejected = false;
  try {
    (void)reconstructSourceFile(missingTarget, source);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ROSE_ASSERT(rejected);
  ROSE_ASSERT(SgUsingDeclarationStatement::numberOfNodes() == usingCount);

  AstFileRecord cyclicTarget = ast;
  bool cycled = false;
  for (NodeRecord &record : cyclicTarget.nodes) {
    if (record.kind != "SgUsingDeclarationStatement" ||
        record.properties.requiredInt("declaration") == 0)
      continue;
    record.properties.object.at("declaration") =
        parseJson(std::to_string(record.id));
    for (EdgeRecord &edge : record.edges) {
      if (edge.field == "declaration") {
        edge.target = record.id;
        cycled = true;
      }
    }
    break;
  }
  ROSE_ASSERT(cycled);
  bool rejectedCycle = false;
  try {
    (void)reconstructSourceFile(cyclicTarget, source);
  } catch (const std::runtime_error &error) {
    rejectedCycle = std::string(error.what()) ==
                    "AST JSON using declaration has a cyclic typed target";
  }
  ROSE_ASSERT(rejectedCycle);
  ROSE_ASSERT(SgUsingDeclarationStatement::numberOfNodes() == usingCount);

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
