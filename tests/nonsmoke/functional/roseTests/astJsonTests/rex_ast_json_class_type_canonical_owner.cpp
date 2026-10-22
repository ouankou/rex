#include "astJson/sageAstJson.h"
#include "astJson/sageAstJsonPrivate.h"
#include "rose.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string takeJsonDir(int &argc, char **argv) {
  constexpr char prefix[] = "--rex-ast-json-dir=";
  std::string result;
  std::vector<char *> filtered;
  filtered.reserve(argc);
  filtered.push_back(argv[0]);

  for (int index = 1; index < argc; ++index) {
    if (std::strncmp(argv[index], prefix, sizeof(prefix) - 1) == 0) {
      result = argv[index] + sizeof(prefix) - 1;
      continue;
    }
    filtered.push_back(argv[index]);
  }

  argc = static_cast<int>(filtered.size());
  for (int index = 0; index < argc; ++index) {
    argv[index] = filtered[index];
  }
  argv[argc] = nullptr;
  return result;
}

SgSourceFile *firstSourceFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  return file;
}

void verifyCanonicalClassType(SgNode *root, VariantT variant,
                              const char *name) {
  SgClassDeclaration *canonical = nullptr;
  SgClassDeclaration *defining = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, variant)) {
    SgClassDeclaration *declaration = isSgClassDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_name() != name) {
      continue;
    }

    SgClassDeclaration *first =
        isSgClassDeclaration(declaration->get_firstNondefiningDeclaration());
    ROSE_ASSERT(first != nullptr);
    if (canonical == nullptr) {
      canonical = first;
    }
    ROSE_ASSERT(first == canonical);
    if (declaration->get_definition() != nullptr) {
      ROSE_ASSERT(defining == nullptr || defining == declaration);
      defining = declaration;
    }
  }

  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
  ROSE_ASSERT(canonical->get_scope() != nullptr);

  SgClassType *type = canonical->get_type();
  ROSE_ASSERT(type != nullptr);
  ROSE_ASSERT(type->get_declaration() == canonical);
  ROSE_ASSERT(SgClassType::createType(canonical) == type);

  if (defining != nullptr) {
    ROSE_ASSERT(canonical->get_definingDeclaration() == defining);
    ROSE_ASSERT(defining->get_firstNondefiningDeclaration() == canonical);
    ROSE_ASSERT(defining->get_definingDeclaration() == defining);
    ROSE_ASSERT(defining->get_scope() == canonical->get_scope());
    ROSE_ASSERT(defining->get_type() == type);
  }
}

void verifyCanonicalClassTypes(SgNode *root) {
  verifyCanonicalClassType(root, V_SgClassDeclaration,
                           "RexAstJsonCanonicalOwner");
  verifyCanonicalClassType(root, V_SgTemplateClassDeclaration,
                           "RexAstJsonCanonicalTemplate");
  unsigned patterns = 0, holding_variables = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgInitializedName)) {
    auto *name = isSgInitializedName(node);
    auto *pattern = name->get_structured_binding_pattern();
    if (pattern == nullptr)
      continue;
    ROSE_ASSERT(pattern->get_parent() == name &&
                pattern->get_expressions().size() == 1);
    auto *element = isSgVarRefExp(pattern->get_expressions().front());
    ROSE_ASSERT(element != nullptr && element->get_parent() == pattern);
    auto *scope = name->get_structured_binding_scope();
    ROSE_ASSERT(scope != nullptr && scope->get_parent() == name &&
                scope->get_scope() == name->get_scope() &&
                element->get_symbol()->get_declaration()->get_scope() == scope);
    auto *alias = element->get_symbol()->get_declaration();
    alias->validate_structured_binding();
    ROSE_ASSERT(alias->get_initializer() == nullptr &&
                alias->get_structured_binding_owner() == name &&
                alias->get_structured_binding_value() != nullptr);
    SgNode *values = alias->get_structured_binding_value();
    if (auto *holding = alias->get_structured_binding_holding_variable()) {
      ++holding_variables;
      holding->validate_structured_binding();
      ROSE_ASSERT(holding->get_structured_binding_alias() == alias);
      values = holding->get_initializer();
      bool saw_temporary_owner = false;
      for (auto *child : NodeQuery::querySubTree(values, V_SgExpression))
        if (auto *temporary = isSgExpression(child);
            temporary->has_semantic_wrapper(
                SgExpression::e_materialized_temporary)) {
          ROSE_ASSERT(temporary->get_materialized_lifetime_owner() == holding);
          saw_temporary_owner = true;
        }
      ROSE_ASSERT(saw_temporary_owner);
    }
    const auto references =
        NodeQuery::querySubTree(values, V_SgCxxDecompositionRefExp);
    ROSE_ASSERT(
        references.size() == 1 &&
        isSgCxxDecompositionRefExp(references.front())->get_decomposition() ==
            name);
    ++patterns;
  }
  ROSE_ASSERT(patterns == 2 && holding_variables == 1);
}

void requireStandaloneBindingCopies(SgSourceFile *file) {
  unsigned copies = 0;
  for (auto *node : NodeQuery::querySubTree(file, V_SgInitializedName)) {
    auto *owner = isSgInitializedName(node);
    auto *pattern = owner->get_structured_binding_pattern();
    if (pattern == nullptr)
      continue;
    owner->set_name(owner->get_name());
    auto *replacement_pattern = SageInterface::deepCopySemantic(pattern);
    owner->set_structured_binding_pattern(replacement_pattern);
    ROSE_ASSERT(pattern->get_parent() == nullptr &&
                replacement_pattern->get_parent() == owner);
    SageInterface::deleteAST(pattern,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    pattern = replacement_pattern;
    auto *declaration = isSgVariableDeclaration(owner->get_parent());
    ROSE_ASSERT(declaration != nullptr &&
                declaration->get_variables().size() == 1);
    auto *alias = isSgVarRefExp(pattern->get_expressions().front())
                      ->get_symbol()
                      ->get_declaration();
    alias->set_name(alias->get_name());
    if (auto *holding = alias->get_structured_binding_holding_variable()) {
      auto *previous = holding->get_initializer();
      auto *replacement = SageInterface::deepCopySemantic(previous);
      holding->set_initializer(replacement);
      ROSE_ASSERT(previous->get_parent() == nullptr &&
                  replacement->get_parent() == holding);
      SageInterface::deleteAST(previous,
                               SageInterface::DeleteAstMode::kRequireIsolated);
      holding->validate_structured_binding();
    }
    SgCopyHelp::copiedNodeMapType identities;
    auto *copy = isSgVariableDeclaration(
        SageInterface::deepCopySemanticSubtree(declaration, identities));
    ROSE_ASSERT(copy != nullptr && copy != declaration &&
                copy->get_variables().size() == 1);
    auto *copied_owner = copy->get_variables().front();
    auto *copied_pattern = copied_owner->get_structured_binding_pattern();
    ROSE_ASSERT(copied_pattern != nullptr && copied_pattern != pattern &&
                copied_pattern->get_expressions().size() == 1);
    auto *copied_alias =
        isSgVarRefExp(copied_pattern->get_expressions().front())
            ->get_symbol()
            ->get_declaration();
    ROSE_ASSERT(copied_alias != alias && identities.count(alias) == 1 &&
                identities.at(alias) == copied_alias &&
                copied_alias->get_structured_binding_owner() == copied_owner);
    ROSE_ASSERT(copied_owner->get_structured_binding_scope() !=
                    owner->get_structured_binding_scope() &&
                copied_owner->get_structured_binding_scope()->get_parent() ==
                    copied_owner &&
                copied_alias->get_scope() ==
                    copied_owner->get_structured_binding_scope());
    copied_alias->validate_structured_binding();
    if (auto *holding = alias->get_structured_binding_holding_variable()) {
      auto *copied_holding =
          copied_alias->get_structured_binding_holding_variable();
      ROSE_ASSERT(copied_holding != nullptr && copied_holding != holding &&
                  identities.at(holding) == copied_holding &&
                  copied_holding->get_structured_binding_owner() ==
                      copied_owner);
      copied_holding->validate_structured_binding();
    }
    SageInterface::deleteAST(copy,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    auto *source = SageBuilder::buildBasicBlock();
    auto *destination = SageBuilder::buildBasicBlock();
    SageInterface::appendStatement(source, owner->get_scope());
    SageInterface::appendStatement(destination, owner->get_scope());
    auto *published =
        isSgVariableDeclaration(SageInterface::deepCopyNode(declaration));
    SageInterface::appendStatement(published, source);
    auto *published_owner = published->get_variables().front();
    auto *published_symbol =
        isSgVarRefExp(published_owner->get_structured_binding_pattern()
                          ->get_expressions()
                          .front())
            ->get_symbol();
    ROSE_ASSERT(source->lookup_variable_symbol(alias->get_name()) ==
                    published_symbol &&
                source->lookup_variable_symbol(SgName()) == nullptr);
    SageInterface::moveStatementsBetweenBlocks(source, destination);
    ROSE_ASSERT(source->get_statements().empty() &&
                source->lookup_variable_symbol(alias->get_name()) == nullptr &&
                destination->lookup_variable_symbol(alias->get_name()) ==
                    published_symbol &&
                destination->lookup_variable_symbol(SgName()) == nullptr &&
                published_owner->get_scope() == destination);
    SageInterface::moveVariableDeclaration(published, source);
    ROSE_ASSERT(
        destination->get_statements().empty() &&
        destination->lookup_variable_symbol(alias->get_name()) == nullptr &&
        source->lookup_variable_symbol(alias->get_name()) == published_symbol &&
        published_owner->get_scope() == source);
    published_symbol->get_declaration()->validate_structured_binding();
    auto *ordinary = SageBuilder::buildVariableDeclaration(
        "rex_move_ordinary", SageBuilder::buildIntType(), nullptr, source);
    SageInterface::appendStatement(ordinary, source);
    auto *ordinary_symbol = source->lookup_variable_symbol("rex_move_ordinary");
    SageInterface::moveVariableDeclaration(ordinary, destination);
    ROSE_ASSERT(source->lookup_variable_symbol("rex_move_ordinary") ==
                    nullptr &&
                destination->lookup_variable_symbol("rex_move_ordinary") ==
                    ordinary_symbol &&
                ordinary->get_variables().front()->get_scope() == destination);
    SageInterface::setExtern(ordinary);
    auto *redeclaration = SageBuilder::buildUnpublishedVariableDeclaration(
        "rex_move_ordinary", SageBuilder::buildIntType(), nullptr, destination);
    SageInterface::setExtern(redeclaration);
    auto *redeclared_name = redeclaration->get_variables().front();
    redeclared_name->set_prev_decl_item(ordinary->get_variables().front());
    SageInterface::appendStatement(redeclaration, destination);
    SageInterface::moveStatementsBetweenBlocks(destination, source);
    ROSE_ASSERT(
        destination->get_statements().empty() &&
        source->lookup_variable_symbol("rex_move_ordinary") ==
            ordinary_symbol &&
        source->find_symbol_from_declaration(redeclared_name) == nullptr &&
        redeclared_name->search_for_symbol_from_symbol_table() ==
            ordinary_symbol &&
        redeclared_name->get_scope() == source &&
        ordinary->get_variables().front()->get_scope() == source &&
        source->lookup_variable_symbol(alias->get_name()) == published_symbol);
    for (auto *block : {source, destination}) {
      SageInterface::removeStatement(block, false);
      SageInterface::deleteAST(block,
                               SageInterface::DeleteAstMode::kRequireIsolated);
    }
    alias->validate_structured_binding();
    ++copies;
  }
  ROSE_ASSERT(copies == 2);
}

void requireTagMoves(SgSourceFile *file) {
  for (const char *name :
       {"rex_scope_enum_embedded_move", "rex_scope_enum_typedef_move",
        "rex_scope_enum_forward_move", "rex_scope_class_embedded_move",
        "rex_scope_class_typedef_move", "rex_scope_class_forward_move"}) {
    auto *function =
        SageInterface::findFunctionDeclaration(file, name, nullptr, true);
    ROSE_ASSERT(function != nullptr && function->get_definition() != nullptr);
    const auto &statements =
        function->get_definition()->get_body()->get_statements();
    ROSE_ASSERT(statements.size() == 2);
    auto *source = isSgBasicBlock(statements.front());
    auto *target = isSgBasicBlock(statements.back());
    ROSE_ASSERT(source != nullptr && target != nullptr &&
                target->get_statements().empty());
    std::vector<std::pair<SgDeclarationStatement *, SgNode *>> parents;
    std::vector<std::pair<SgInitializedName *, SgSymbol *>> fields;
    std::vector<SgSymbol *> symbols;
    std::vector<SgDeclarationStatement *> auxiliary;
    for (auto *node :
         NodeQuery::querySubTree(source, V_SgDeclarationStatement)) {
      auto *declaration = isSgDeclarationStatement(node);
      if (isSgEnumDeclaration(declaration) == nullptr &&
          isSgClassDeclaration(declaration) == nullptr)
        continue;
      ROSE_ASSERT(declaration->get_scope() == source);
      parents.emplace_back(declaration, declaration->get_parent());
      symbols.push_back(declaration->get_symbol_from_symbol_table());
      ROSE_ASSERT(symbols.back() != nullptr);
      auto *canonical = declaration->get_firstNondefiningDeclaration();
      ASSERT_not_null(canonical);
      if (auto *owner = isSgAuxiliaryDeclarationList(canonical->get_parent())) {
        ROSE_ASSERT(owner->get_parent() == source);
        auxiliary.push_back(canonical);
      }
      if (auto *enumeration = isSgEnumDeclaration(declaration))
        for (auto *field : enumeration->get_enumerators()) {
          ROSE_ASSERT(field->get_scope() == source);
          fields.emplace_back(field, field->get_symbol_from_symbol_table());
          ROSE_ASSERT(fields.back().second != nullptr);
        }
    }
    ROSE_ASSERT(!parents.empty());
    SageInterface::moveStatementsBetweenBlocks(source, target);
    ROSE_ASSERT(source->get_statements().empty());
    for (size_t index = 0; index < parents.size(); ++index) {
      auto *declaration = parents[index].first;
      ROSE_ASSERT(declaration->get_parent() == (parents[index].second == source
                                                    ? target
                                                    : parents[index].second) &&
                  declaration->get_scope() == target &&
                  declaration->get_firstNondefiningDeclaration()->get_scope() ==
                      target &&
                  declaration->get_symbol_from_symbol_table() ==
                      symbols[index]);
      if (auto *enumeration = isSgEnumDeclaration(declaration))
        enumeration->validate_enumerator_source_ownership();
    }
    for (const auto &field : fields)
      ROSE_ASSERT(field.first->get_scope() == target &&
                  field.first->get_symbol_from_symbol_table() == field.second);
    for (auto *canonical : auxiliary) {
      auto *owner = target->get_auxiliary_declarations();
      ROSE_ASSERT(owner != nullptr && canonical->get_parent() == owner &&
                  owner->get_parent() == target &&
                  std::count(owner->get_declarations().begin(),
                             owner->get_declarations().end(), canonical) == 1);
    }
  }
}

void requireFunctionMoves(SgSourceFile *file) {
  auto *source_symbol = file->get_globalScope()->lookup_namespace_symbol(
      "RexScopeFunctionSource");
  auto *target_symbol = file->get_globalScope()->lookup_namespace_symbol(
      "RexScopeFunctionTarget");
  ROSE_ASSERT(source_symbol != nullptr && target_symbol != nullptr);
  auto *source = source_symbol->get_declaration()->get_definition();
  auto *target = target_symbol->get_declaration()->get_definition();
  ROSE_ASSERT(source != nullptr && target != nullptr &&
              source->get_declarations().size() == 5 &&
              target->get_declarations().empty());
  struct FunctionMove {
    SgFunctionDeclaration *declaration;
    SgNode *parent;
    SgSymbol *symbol;
    bool auxiliary;
  };
  std::vector<FunctionMove> functions;
  for (auto *node : NodeQuery::querySubTree(source, V_SgFunctionDeclaration)) {
    auto *declaration = isSgFunctionDeclaration(node);
    ROSE_ASSERT(declaration != nullptr && declaration->get_scope() == source);
    auto *symbol = declaration->get_symbol_from_symbol_table();
    ASSERT_not_null(symbol);
    auto *parent = declaration->get_parent();
    auto *auxiliary = isSgAuxiliaryDeclarationList(parent);
    if (auxiliary != nullptr)
      ROSE_ASSERT(auxiliary == source->get_auxiliary_declarations());
    functions.push_back({declaration, parent, symbol, auxiliary != nullptr});
  }
  ROSE_ASSERT(functions.size() == 7);
  SageInterface::moveStatementsBetweenBlocks(source, target);
  ROSE_ASSERT(source->get_declarations().empty() &&
              target->get_declarations().size() == 5);
  for (const auto &entry : functions) {
    auto *declaration = entry.declaration;
    SgNode *expected_parent =
        entry.auxiliary ? target->get_auxiliary_declarations()
                        : (entry.parent == source ? target : entry.parent);
    ROSE_ASSERT(expected_parent != nullptr &&
                declaration->get_parent() == expected_parent &&
                declaration->get_scope() == target &&
                declaration->get_firstNondefiningDeclaration()->get_scope() ==
                    target &&
                declaration->get_symbol_from_symbol_table() == entry.symbol &&
                entry.symbol->get_parent() == target->get_symbol_table());
    if (auto *group = isSgDeclarationGroupStatement(expected_parent))
      ROSE_ASSERT(group->get_parent() == target &&
                  group->get_scope() == target);
  }
}

SgMemberFunctionType *canonicalMemberType(SgSourceFile *file) {
  for (SgNode *node :
       NodeQuery::querySubTree(file, V_SgMemberFunctionDeclaration)) {
    auto *declaration = isSgMemberFunctionDeclaration(node);
    if (declaration->get_name() != "rex_canonical_member")
      continue;
    auto *type = isSgMemberFunctionType(declaration->get_type());
    ROSE_ASSERT(type != nullptr);
    ROSE_ASSERT(
        SgNode::get_globalFunctionTypeTable()->lookup_exact_function_type(
            type->get_return_type(), type->get_argument_list(),
            type->get_class_type(), type->get_mfunc_specifier()) == type);
    return type;
  }
  ROSE_ABORT();
}

void requireBindingPatternPreflight(SgSourceFile *file) {
  using namespace Rose::AstJson;
  const auto checkpoint = Checkpoint::PreOmpConstruction;
  const auto original = parseAstFileJson(
      buildJson(file, checkpoint, file, isSgFileList(file->get_parent())),
      checkpointName(checkpoint));
  const auto owner = std::find_if(
      original.nodes.begin(), original.nodes.end(),
      [](const NodeRecord &record) {
        return record.kind == "SgInitializedName" &&
               singleEdgeTarget(record, "structured_binding_pattern") != 0;
      });
  ROSE_ASSERT(owner != original.nodes.end());
  const auto pattern_id =
      requiredSingleEdgeTarget(*owner, "structured_binding_pattern");
  const auto binding_scope_id =
      requiredSingleEdgeTarget(*owner, "structured_binding_scope");
  const auto element_id =
      requiredSingleEdgeTarget(original.node(pattern_id), "expressions");
  const auto scope_id = requiredSingleEdgeTarget(*owner, "scope");
  const auto alias_id = original.node(element_id)
                            .properties.at("symbol")
                            .requiredNonnegativeNodeId("symbol_declaration");
  const auto value_id = requiredSingleEdgeTarget(original.node(alias_id),
                                                 "structured_binding_value");
  const auto names = SgInitializedName::numberOfNodes();
  const auto lists = SgExprListExp::numberOfNodes();
  const auto references = SgVarRefExp::numberOfNodes();
  const auto positions = Sg_File_Info::numberOfNodes();
  const auto revision = SgNode::get_globalAstModificationSequence();
  for (int mutation = 0; mutation != 17; ++mutation) {
    auto ast = original;
    auto &name = ast.nodes.at(ast.index_by_id.at(owner->id));
    auto &pattern = ast.nodes.at(ast.index_by_id.at(pattern_id));
    auto &element = ast.nodes.at(ast.index_by_id.at(element_id));
    auto &alias = ast.nodes.at(ast.index_by_id.at(alias_id));
    if (mutation == 0)
      name.properties.object.erase("structured_binding_pattern");
    else if (mutation == 1)
      name.properties.object.at("structured_binding_pattern") =
          JsonValue::number("0");
    else if (mutation == 2)
      name.edges.erase(std::remove_if(name.edges.begin(), name.edges.end(),
                                      [](const EdgeRecord &edge) {
                                        return edge.field ==
                                               "structured_binding_pattern";
                                      }),
                       name.edges.end());
    else if (mutation == 3) {
      name.properties.object.at("structured_binding_pattern") =
          JsonValue::number(std::to_string(scope_id));
      for (auto &edge : name.edges)
        if (edge.field == "structured_binding_pattern")
          edge.target = scope_id;
    } else if (mutation == 4 || mutation == 5 || mutation == 6) {
      auto &record = mutation == 6 ? element : pattern;
      for (auto &edge : record.edges)
        if (edge.field == (mutation == 5 ? "expressions" : "parent"))
          edge.target = mutation == 5 ? scope_id : 0;
    } else if (mutation == 7) {
      element.properties.object.at("symbol").object.at("symbol_declaration") =
          JsonValue::number(std::to_string(scope_id));
    } else if (mutation == 8) {
      alias.properties.object.erase("structured_binding_kind");
    } else if (mutation == 9) {
      alias.properties.object.at("structured_binding_kind") =
          JsonValue::number("99");
    } else if (mutation == 10) {
      alias.properties.object.at("structured_binding_value") =
          JsonValue::number("0");
    } else if (mutation == 11) {
      auto &value = ast.nodes.at(ast.index_by_id.at(value_id));
      for (auto &edge : value.edges)
        if (edge.field == "parent")
          edge.target = 0;
    } else if (mutation == 12) {
      name.properties.object.erase("structured_binding_scope");
    } else if (mutation == 13) {
      name.properties.object.at("structured_binding_scope") =
          JsonValue::number("0");
    } else if (mutation == 14) {
      auto &scope = ast.nodes.at(ast.index_by_id.at(binding_scope_id));
      for (auto &edge : scope.edges)
        if (edge.field == "parent")
          edge.target = 0;
    } else if (mutation == 15) {
      for (auto &edge : alias.edges)
        if (edge.field == "scope")
          edge.target = scope_id;
    } else {
      name.edges.erase(std::remove_if(name.edges.begin(), name.edges.end(),
                                      [](const EdgeRecord &edge) {
                                        return edge.field == "initptr";
                                      }),
                       name.edges.end());
    }
    for (bool constructor_reader : {false, true}) {
      NodeMap nodes;
      std::string failure;
      try {
        if (constructor_reader)
          rebuildConstructorOnlyNodes(ast, nodes);
        else
          (void)reconstructSourceFile(ast, file);
      } catch (const std::runtime_error &error) {
        failure = error.what();
      }
      ROSE_ASSERT(failure.find("structured-binding") != std::string::npos ||
                  failure.find("structured_binding_pattern") !=
                      std::string::npos ||
                  (mutation == 12 && failure.find("structured_binding_scope") !=
                                         std::string::npos) ||
                  (mutation == 8 && failure.find("structured_binding_kind") !=
                                        std::string::npos));
      ROSE_ASSERT(nodes.empty() &&
                  SgInitializedName::numberOfNodes() == names &&
                  SgExprListExp::numberOfNodes() == lists &&
                  SgVarRefExp::numberOfNodes() == references &&
                  Sg_File_Info::numberOfNodes() == positions &&
                  SgNode::get_globalAstModificationSequence() == revision);
    }
  }
}

} // namespace

int main(int argc, char **argv) {
  const std::string json_dir = takeJsonDir(argc, argv);
  ROSE_ASSERT(!json_dir.empty());
  std::filesystem::remove_all(json_dir);
  std::filesystem::create_directories(json_dir);

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  SgSourceFile *file = firstSourceFile(project);
  verifyCanonicalClassTypes(file);
  requireTagMoves(file);
  requireFunctionMoves(file);
  requireStandaloneBindingCopies(file);
  requireBindingPatternPreflight(file);
  auto *originalFile = file;
  auto *originalMemberType = canonicalMemberType(file);
  const auto originalFunctions =
      SgNode::get_globalFunctionTypeTable()->get_types();
  auto *copiedProject = SageInterface::deepCopy(project);
  ROSE_ASSERT(copiedProject != nullptr && copiedProject != project);
  auto *checkpointInput = firstSourceFile(copiedProject);
  ROSE_ASSERT(checkpointInput != originalFile);
  verifyCanonicalClassTypes(checkpointInput);
  file = Rose::AstJson::roundTripSourceFile(
      checkpointInput, Rose::AstJson::Checkpoint::PreOmpConstruction);
  ROSE_ASSERT(file != nullptr && file != originalFile);
  ROSE_ASSERT(file != checkpointInput && !SgNode::isLiveNode(checkpointInput));
  ROSE_ASSERT(SgNode::isLiveNode(originalFile));
  verifyCanonicalClassTypes(file);
  verifyCanonicalClassTypes(originalFile);
  auto *copiedMemberType = canonicalMemberType(file);
  ROSE_ASSERT(copiedMemberType != originalMemberType);
  ROSE_ASSERT(copiedMemberType->get_class_type() !=
              originalMemberType->get_class_type());
  ROSE_ASSERT(canonicalMemberType(originalFile) == originalMemberType);
  const auto &functions = SgNode::get_globalFunctionTypeTable()->get_types();
  for (const auto &entry : originalFunctions) {
    const auto found = functions.find(entry.first);
    ROSE_ASSERT(found != functions.end() && found->second == entry.second);
  }
  SageInterface::deleteAST(copiedProject,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(copiedProject));
  ROSE_ASSERT(SgNode::isLiveNode(originalFile));
  verifyCanonicalClassTypes(originalFile);
  ROSE_ASSERT(canonicalMemberType(originalFile) == originalMemberType);
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(project));
  return 0;
}
