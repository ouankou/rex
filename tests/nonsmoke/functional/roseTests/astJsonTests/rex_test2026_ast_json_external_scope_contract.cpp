#include "rose.h"

#include "astJson/sageAstJson.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
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

void requireDistinctSourceParameters(SgFunctionDeclaration *decl) {
  SgFunctionParameterList *semantic = decl->get_parameterList();
  SgFunctionParameterList *syntax = decl->get_parameterList_syntax();
  ROSE_ASSERT(semantic != nullptr && semantic->get_parent() == decl);
  ROSE_ASSERT(syntax != nullptr && syntax != semantic &&
              syntax->get_parent() == decl);
  ROSE_ASSERT(semantic->get_args().size() == 1);
  ROSE_ASSERT(syntax->get_args().size() == 1);
  SgInitializedName *semantic_name = semantic->get_args()[0];
  SgInitializedName *syntax_name = syntax->get_args()[0];
  ROSE_ASSERT(semantic_name != nullptr && syntax_name != nullptr &&
              semantic_name != syntax_name);
  ROSE_ASSERT(semantic_name->get_parent() == semantic &&
              syntax_name->get_parent() == syntax);
  ROSE_ASSERT(semantic_name->get_declptr() == decl &&
              syntax_name->get_declptr() == decl);
  ROSE_ASSERT(semantic_name->get_scope() == decl->get_functionParameterScope());
  ROSE_ASSERT(decl->get_function_declarator_scope() != nullptr);
  ROSE_ASSERT(syntax_name->get_scope() ==
              decl->get_function_declarator_scope());
  ROSE_ASSERT(semantic_name->get_type() != syntax_name->get_type());
  auto *array = isSgArrayType(syntax_name->get_type());
  ROSE_ASSERT(array != nullptr);
  auto *extent = isSgIntVal(array->get_index());
  ROSE_ASSERT(extent != nullptr && extent->get_value() == 3);
  auto *written = isSgIntVal(extent->get_originalExpressionTree());
  ROSE_ASSERT(written != nullptr && written != extent &&
              written->get_value() == 3 && written->get_parent() == extent);
  for (SgInitializedName *name : {semantic_name, syntax_name}) {
    SgAssignInitializer *initializer =
        isSgAssignInitializer(name->get_initptr());
    ROSE_ASSERT(initializer != nullptr && initializer->get_parent() == name);
    ROSE_ASSERT(initializer->get_operand_i() != nullptr &&
                initializer->get_operand_i()->get_parent() == initializer);
    ROSE_ASSERT(initializer->get_type() == name->get_type());
  }
}

void requireSourcePosition(SgNode *node) {
  ROSE_ASSERT(node != nullptr);
  if (node->get_startOfConstruct() == nullptr ||
      node->get_endOfConstruct() == nullptr) {
    SageInterface::setSourcePositionAtRootAndAllChildren(node);
  }
}

void addExternalScopeContracts(SgProject *project, SgSourceFile *file,
                               bool expect_distinct_parameters) {
  SgFunctionDeclaration *target = findFunction(file, "rex_ast_json_target");
  SgFunctionDeclaration *original = findFunction(file, "rex_ast_json_original");
  ROSE_ASSERT(target != nullptr);
  ROSE_ASSERT(original != nullptr);
  if (expect_distinct_parameters)
    requireDistinctSourceParameters(target);

  SgGlobal *global = file->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  SgFunctionSymbol *original_symbol =
      global->lookup_function_symbol("rex_ast_json_original");
  ROSE_ASSERT(original_symbol != nullptr);
  auto *global_alias =
      new SgAliasSymbol(original_symbol, true, "rex_global_alias");
  global_alias->get_causal_nodes().push_back(original);
  global->insert_symbol(global_alias->get_name(), global_alias);
  auto *global_rename =
      new SgRenameSymbol(original, global_alias, "rex_global_rename");
  global->insert_symbol(global_rename->get_name(), global_rename);

  SgFunctionDeclaration *external_peer =
      SageInterface::deepCopy<SgFunctionDeclaration>(target);
  ROSE_ASSERT(external_peer != nullptr);
  if (expect_distinct_parameters)
    requireDistinctSourceParameters(external_peer);
  external_peer->set_parent(project);
  external_peer->set_scope(global);
  external_peer->set_firstNondefiningDeclaration(external_peer);
  external_peer->set_definingDeclaration(nullptr);
  // This peer is deliberately detached from its source file. Preserve its
  // exact external origin explicitly instead of borrowing the current JSON
  // collection boundary as its provenance.
  external_peer->initialize_ast_json_external_provenance(
      SgDeclarationStatement::e_ast_json_external_function,
      file->get_sourceFileNameWithPath());
  requireSourcePosition(external_peer);

  SgFunctionParameterScope *external_scope =
      external_peer->get_functionParameterScope();
  ROSE_ASSERT(external_scope != nullptr);
  ROSE_ASSERT(external_scope->get_parent() == external_peer);
  requireSourcePosition(external_scope);

  SgUseStatement *use_stmt = new SgUseStatement(
      SgName("rex_ast_json_external_mod"), true, "", nullptr);
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

  SgSymbolTable *external_table = external_scope->get_symbol_table();
  ROSE_ASSERT(external_table != nullptr &&
              external_table->get_parent() == external_scope);

  SgAliasSymbol *alias_symbol =
      new SgAliasSymbol(global_rename, true, SgName("alias_original"));
  alias_symbol->get_causal_nodes().push_back(use_stmt);
  external_table->insert(SgName("alias_original"), alias_symbol);

  SgRenameSymbol *rename_symbol =
      new SgRenameSymbol(original, alias_symbol, SgName("renamed_original"));
  external_table->insert(SgName("renamed_original"), rename_symbol);
  auto *outer_alias = new SgAliasSymbol(rename_symbol, true, "outer_original");
  outer_alias->get_causal_nodes().push_back(use_stmt);
  external_table->insert(outer_alias->get_name(), outer_alias);
  auto *reexport =
      new SgRenameSymbol(original, outer_alias, "reexported_original");
  external_table->insert(reexport->get_name(), reexport);

  SgFunctionParameterScope *target_scope = target->get_functionParameterScope();
  ROSE_ASSERT(target_scope != nullptr);
  ROSE_ASSERT(target_scope != external_scope);
  ROSE_ASSERT(target_scope->get_parent() == target);
  for (SgInitializedName *parameter : target->get_parameterList()->get_args()) {
    ROSE_ASSERT(parameter != nullptr);
    SgVariableSymbol *symbol =
        isSgVariableSymbol(parameter->get_symbol_from_symbol_table());
    ROSE_ASSERT(symbol != nullptr);
    target_scope->remove_symbol(symbol);
    external_scope->insert_symbol(symbol->get_name(), symbol);
    parameter->set_scope(external_scope);
    ROSE_ASSERT(parameter->get_symbol_from_symbol_table() == symbol);
  }
  target->set_firstNondefiningDeclaration(external_peer);
  target->set_functionParameterScope(external_scope);
  target_scope->set_parent(nullptr);
  SageInterface::deleteAST(
      target_scope, SageInterface::DeleteAstMode::kSkipExternalReferences);
}

} // namespace

int main(int argc, char **argv) {
  const std::string contract_dir = takeContractDir(argc, argv);
  ROSE_ASSERT(!contract_dir.empty());
  const bool expect_distinct_parameters =
      std::getenv("REX_AST_JSON_EXPECT_DISTINCT_PARAMETERS") != nullptr;

  std::filesystem::remove_all(contract_dir);
  std::filesystem::create_directories(contract_dir);

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  SgSourceFile *file = firstSourceFile(project);
  addExternalScopeContracts(project, file, expect_distinct_parameters);

  file = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction);
  ROSE_ASSERT(file != nullptr);
  SgFunctionDeclaration *target = findFunction(file, "rex_ast_json_target");
  SgFunctionDeclaration *peer =
      isSgFunctionDeclaration(target->get_firstNondefiningDeclaration());
  ROSE_ASSERT(peer != nullptr && peer != target && SgNode::isLiveNode(peer));
  ROSE_ASSERT(peer->get_firstNondefiningDeclaration() == peer);
  ROSE_ASSERT(peer->get_parent() == project);
  ROSE_ASSERT(peer->get_scope() == file->get_globalScope());
  if (expect_distinct_parameters) {
    requireDistinctSourceParameters(peer);
    unsigned friends = 0;
    for (auto *node :
         NodeQuery::querySubTree(file, V_SgTemplateInstantiationFunctionDecl)) {
      auto *function = isSgTemplateInstantiationFunctionDecl(node);
      if (function->get_templateName() != "rex_ast_json_friend_parameter" ||
          !function->get_declarationModifier().isFriend())
        continue;
      auto *parameters = function->get_parameterList();
      auto *written = function->get_parameterList_syntax();
      ROSE_ASSERT(parameters != nullptr && parameters->get_args().size() == 1);
      ROSE_ASSERT(isSgPointerType(parameters->get_args().front()->get_type()) !=
                  nullptr);
      ROSE_ASSERT(written != nullptr && written != parameters &&
                  written->get_parent() == function &&
                  written->get_args().size() == 1);
      auto *array = isSgArrayType(written->get_args().front()->get_type());
      auto *bound = array != nullptr ? isSgIntVal(array->get_index()) : nullptr;
      ROSE_ASSERT(bound != nullptr && bound->get_value() == 3);
      auto *signature = function->get_type_syntax();
      ROSE_ASSERT(function->get_type_syntax_is_available() &&
                  signature != nullptr && signature->get_parent() == function &&
                  signature->get_arguments().size() == 1 &&
                  signature->get_arguments().front() == array);
      ++friends;
    }
    ROSE_ASSERT(friends == 1);
  }
  ROSE_ASSERT(target->get_functionParameterScope() ==
              peer->get_functionParameterScope());
  ROSE_ASSERT(peer->get_functionParameterScope()->get_parent() == peer);

  SgFunctionParameterScope *scope = peer->get_functionParameterScope();
  SgSymbolTable *table = scope->get_symbol_table();
  ROSE_ASSERT(table != nullptr && table->get_parent() == scope);
  auto exact_symbol = [&](const char *name) {
    auto entries = table->get_table()->equal_range(SgName(name));
    ROSE_ASSERT(std::distance(entries.first, entries.second) == 1);
    return entries.first->second;
  };
  auto *alias = isSgAliasSymbol(exact_symbol("alias_original"));
  auto *rename = isSgRenameSymbol(exact_symbol("renamed_original"));
  auto *outer_alias = isSgAliasSymbol(exact_symbol("outer_original"));
  auto *reexport = isSgRenameSymbol(exact_symbol("reexported_original"));
  SgFunctionSymbol *original =
      file->get_globalScope()->lookup_function_symbol("rex_ast_json_original");
  ROSE_ASSERT(original != nullptr && alias != nullptr && rename != nullptr);
  auto *global = file->get_globalScope();
  auto *global_alias =
      global->lookup_alias_symbol("rex_global_alias", original);
  ROSE_ASSERT(global_alias != nullptr && global_alias->get_alias() == original);
  auto range = global->get_symbol_table()->get_table()->equal_range(
      SgName("rex_global_rename"));
  ROSE_ASSERT(std::distance(range.first, range.second) == 1);
  auto *global_rename = isSgRenameSymbol(range.first->second);
  ROSE_ASSERT(global_rename != nullptr &&
              global_rename->get_original_symbol() == global_alias);
  ROSE_ASSERT(alias->get_alias() == global_rename);
  ROSE_ASSERT(rename->get_original_symbol() == alias);
  ROSE_ASSERT(outer_alias != nullptr && outer_alias->get_alias() == rename);
  ROSE_ASSERT(reexport != nullptr &&
              reexport->get_original_symbol() == outer_alias);
  ROSE_ASSERT(reexport->get_symbol_basis() == original->get_symbol_basis());
  ROSE_ASSERT(alias->get_new_name() == "alias_original");
  ROSE_ASSERT(rename->get_new_name() == "renamed_original");
  ROSE_ASSERT(alias->get_causal_nodes().size() == 1);
  auto *use = isSgUseStatement(alias->get_causal_nodes().front());
  ROSE_ASSERT(use != nullptr && use->get_parent() == scope &&
              use->get_scope() == scope);
  ROSE_ASSERT(use->get_rename_list().size() == 1);
  SgRenamePair *pair = use->get_rename_list().front();
  ROSE_ASSERT(pair != nullptr && pair->get_parent() == use);
  ROSE_ASSERT(pair->get_local_name() == "local_name");
  ROSE_ASSERT(pair->get_use_name() == "use_name");
  for (SgInitializedName *parameter : target->get_parameterList()->get_args()) {
    ROSE_ASSERT(parameter->get_declptr() == target);
    ROSE_ASSERT(parameter->get_scope() == scope);
    SgSymbol *symbol = scope->find_symbol_from_declaration(parameter);
    ROSE_ASSERT(symbol != nullptr && symbol->get_symbol_basis() == parameter);
  }
  return 0;
}
