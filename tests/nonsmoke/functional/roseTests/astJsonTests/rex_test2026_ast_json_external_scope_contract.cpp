#include "rose.h"

#include "astJson/sageAstJson.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string takeContractDir(int &argc, char **argv) {
  const char prefix[] = "--rex-ast-json-contract-dir=";
  std::string result;
  std::vector<char *> filtered;
  filtered.reserve(argc);
  filtered.push_back(argv[0]);

  for (int i = 1; i < argc; ++i) {
    if (std::strncmp(argv[i], prefix, sizeof(prefix) - 1) == 0) {
      result = argv[i] + sizeof(prefix) - 1;
      continue;
    }
    filtered.push_back(argv[i]);
  }

  argc = static_cast<int>(filtered.size());
  for (int i = 0; i < argc; ++i) {
    argv[i] = filtered[i];
  }
  argv[argc] = nullptr;
  return result;
}

SgSourceFile *firstSourceFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->numberOfFiles() > 0);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  return file;
}

SgFunctionDeclaration *findFunction(SgNode *root, const std::string &name) {
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *decl = isSgFunctionDeclaration(node);
    if (decl != nullptr && decl->get_name().getString() == name) {
      return decl;
    }
  }
  return nullptr;
}

void requireSourcePosition(SgNode *node) {
  ROSE_ASSERT(node != nullptr);
  if (node->get_startOfConstruct() == nullptr ||
      node->get_endOfConstruct() == nullptr) {
    SageInterface::setSourcePositionAtRootAndAllChildren(node);
  }
}

SgSymbolTable *ensureSymbolTable(SgScopeStatement *scope) {
  ROSE_ASSERT(scope != nullptr);
  if (scope->get_symbol_table() == nullptr) {
    SgSymbolTable *table = new SgSymbolTable(17);
    table->set_parent(scope);
    scope->set_symbol_table(table);
  }
  ROSE_ASSERT(scope->get_symbol_table() != nullptr);
  return scope->get_symbol_table();
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("failed to open " + path.string());
  }
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

bool anyJsonContainsAll(const std::filesystem::path &dir,
                        const std::vector<std::string> &needles) {
  if (!std::filesystem::exists(dir)) {
    return false;
  }
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::recursive_directory_iterator(dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".json") {
      continue;
    }
    const std::string text = readFile(entry.path());
    bool all_present = true;
    for (const std::string &needle : needles) {
      if (text.find(needle) == std::string::npos) {
        all_present = false;
        break;
      }
    }
    if (all_present) {
      return true;
    }
  }
  return false;
}

void addExternalScopeContracts(SgProject *project, SgSourceFile *file) {
  SgFunctionDeclaration *target = findFunction(file, "rex_ast_json_target");
  SgFunctionDeclaration *original = findFunction(file, "rex_ast_json_original");
  ROSE_ASSERT(target != nullptr);
  ROSE_ASSERT(original != nullptr);

  SgGlobal *global = file->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  SgFunctionSymbol *original_symbol =
      global->lookup_function_symbol("rex_ast_json_original");
  ROSE_ASSERT(original_symbol != nullptr);

  SgFunctionDeclaration *external_peer =
      SageInterface::deepCopy<SgFunctionDeclaration>(target);
  ROSE_ASSERT(external_peer != nullptr);
  external_peer->set_parent(project);
  external_peer->set_scope(global);
  external_peer->set_firstNondefiningDeclaration(external_peer);
  external_peer->set_definingDeclaration(nullptr);
  requireSourcePosition(external_peer);

  SgFunctionParameterScope *external_scope =
      external_peer->get_functionParameterScope();
  if (external_scope == nullptr) {
    external_scope = new SgFunctionParameterScope();
    external_peer->set_functionParameterScope(external_scope);
  }
  external_scope->set_parent(external_peer);
  requireSourcePosition(external_scope);

  SgUseStatement *use_stmt =
      new SgUseStatement(SgName("rex_ast_json_external_mod"), true, "");
  use_stmt->set_parent(external_scope);
  use_stmt->set_scope(external_scope);
  use_stmt->set_firstNondefiningDeclaration(use_stmt);
  use_stmt->set_definingDeclaration(use_stmt);
  SgRenamePair *rename_pair =
      new SgRenamePair(SgName("local_name"), SgName("use_name"));
  rename_pair->set_parent(use_stmt);
  use_stmt->get_rename_list().push_back(rename_pair);
  requireSourcePosition(use_stmt);
  requireSourcePosition(rename_pair);
  external_scope->append_declaration(use_stmt);

  SgSymbolTable *external_table = ensureSymbolTable(external_scope);

  SgAliasSymbol *alias_symbol =
      new SgAliasSymbol(original_symbol, true, SgName("alias_original"));
  alias_symbol->get_causal_nodes().push_back(use_stmt);
  external_table->insert(SgName("alias_original"), alias_symbol);

  SgRenameSymbol *rename_symbol =
      new SgRenameSymbol(target, original_symbol, SgName("renamed_original"));
  external_table->insert(SgName("renamed_original"), rename_symbol);

  target->set_firstNondefiningDeclaration(external_peer);
  target->set_functionParameterScope(external_scope);
}

void verifyContractJson(const std::filesystem::path &dir) {
  const std::vector<std::string> required = {
      "\"external_function_parameter_scope\"",
      "\"external_function_parameter_scope_source\": "
      "\"firstNondefiningDeclaration\"",
      "\"kind\": \"SgUseStatement\"",
      "\"kind\": \"SgRenamePair\"",
      "\"symbol_kind\": \"SgAliasSymbol\"",
      "\"symbol_kind\": \"SgRenameSymbol\"",
  };
  if (!anyJsonContainsAll(dir, required)) {
    throw std::runtime_error("AST JSON contract markers were not written to " +
                             dir.string());
  }
}

} // namespace

int main(int argc, char **argv) {
  const std::string contract_dir = takeContractDir(argc, argv);
  ROSE_ASSERT(!contract_dir.empty());

  std::filesystem::remove_all(contract_dir);
  std::filesystem::create_directories(contract_dir);

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  SgSourceFile *file = firstSourceFile(project);
  addExternalScopeContracts(project, file);

  file = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction);
  ROSE_ASSERT(file != nullptr);

  verifyContractJson(contract_dir);
  return 0;
}
