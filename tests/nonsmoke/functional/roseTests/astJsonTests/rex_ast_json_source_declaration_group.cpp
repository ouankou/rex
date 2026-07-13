#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
SgSourceFile *sourceFile(SgProject *project) {
  SgSourceFile *result = nullptr;
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *source = isSgSourceFile(file)) {
      if (!source->get_isHeaderFile()) {
        ROSE_ASSERT(result == nullptr);
        result = source;
      }
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void roundTripWithDeclarationGroupRecordsFirst(SgProject *project) {
  using namespace Rose::AstJson;

  SgSourceFile *source = sourceFile(project);
  constexpr Checkpoint checkpoint = Checkpoint::PreOmpConstruction;
  const std::string json = buildJson(source, checkpoint, source);
  ROSE_ASSERT(json.find("suppress_variable_declaration_normalization") ==
              std::string::npos);
  AstFileRecord ast = parseAstFileJson(json, checkpointName(checkpoint));
  const auto firstNonGroup = std::stable_partition(
      ast.nodes.begin(), ast.nodes.end(), [](const NodeRecord &record) {
        return record.kind == "SgDeclarationGroupStatement";
      });
  ROSE_ASSERT(firstNonGroup != ast.nodes.begin());
  ROSE_ASSERT(firstNonGroup != ast.nodes.end());

  ast.index_by_id.clear();
  for (size_t index = 0; index < ast.nodes.size(); ++index) {
    ROSE_ASSERT(ast.index_by_id.emplace(ast.nodes[index].id, index).second);
  }
  for (auto record = ast.nodes.begin(); record != firstNonGroup; ++record) {
    for (const EdgeRecord &edge : record->edges) {
      if (edge.field == "declarations") {
        ROSE_ASSERT(ast.index_by_id.at(record->id) <
                    ast.index_by_id.at(edge.target));
      }
    }
  }

  SgSourceFile *copy = reconstructSourceFile(ast, source);
  replaceFileInProject(source, copy);
}

std::string declarationName(SgDeclarationStatement *declaration) {
  if (SgVariableDeclaration *variable = isSgVariableDeclaration(declaration)) {
    ROSE_ASSERT(variable->get_variables().size() == 1);
    ROSE_ASSERT(variable->get_variables().front() != nullptr);
    return variable->get_variables().front()->get_name().getString();
  }
  if (SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration)) {
    return function->get_name().getString();
  }
  if (SgTypedefDeclaration *typedefDeclaration =
          isSgTypedefDeclaration(declaration)) {
    return typedefDeclaration->get_name().getString();
  }
  return "";
}

void checkGroup(
    const std::map<std::string, SgDeclarationStatement *> &declarations,
    const std::vector<std::string> &names,
    SgDeclarationGroupStatement::source_terminator_enum terminator =
        SgDeclarationGroupStatement::e_source_terminator_file_semicolon) {
  ROSE_ASSERT(names.size() >= 2);
  SgDeclarationGroupStatement *group = isSgDeclarationGroupStatement(
      declarations.at(names.front())->get_parent());
  ROSE_ASSERT(group != nullptr);
  group->validate();
  ROSE_ASSERT(!group->has_semantic_mangled_name());
  ROSE_ASSERT(group->get_declarations().size() == names.size());
  ROSE_ASSERT(group->get_parent() != nullptr);
  ROSE_ASSERT(group->get_scope() != nullptr);
  ROSE_ASSERT(group->get_source_terminator() == terminator);
  for (size_t index = 0; index < names.size(); ++index) {
    SgDeclarationStatement *declaration = declarations.at(names[index]);
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_parent() == group);
    ROSE_ASSERT(group->get_declarations().at(index) == declaration);
  }
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  roundTripWithDeclarationGroupRecordsFirst(project);

  std::map<std::string, SgDeclarationStatement *> declarations;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgDeclarationStatement)) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    if (declaration == nullptr) {
      continue;
    }
    const std::string name = declarationName(declaration);
    if (name.rfind("rex_json_group_", 0) == 0) {
      ROSE_ASSERT(declarations.emplace(name, declaration).second);
    }
  }

  ROSE_ASSERT(declarations.size() == 11);
  checkGroup(declarations,
             {"rex_json_group_a", "rex_json_group_b", "rex_json_group_c"});
  checkGroup(declarations,
             {"rex_json_group_object", "rex_json_group_function"});
  checkGroup(declarations,
             {"rex_json_group_function_a", "rex_json_group_function_b"});
  checkGroup(declarations, {"rex_json_group_typedef_scalar",
                            "rex_json_group_typedef_function"});
  checkGroup(declarations,
             {"rex_json_group_macro_object", "rex_json_group_macro_function"},
             SgDeclarationGroupStatement::e_source_terminator_macro_semicolon);

  const Rose_STL_Container<SgNode *> groups =
      NodeQuery::querySubTree(project, V_SgDeclarationGroupStatement);
  ROSE_ASSERT(groups.size() == 5);

  project->skipfinalCompileStep(true);
  AstTests::runAllTests(project);
  return backend(project);
}
