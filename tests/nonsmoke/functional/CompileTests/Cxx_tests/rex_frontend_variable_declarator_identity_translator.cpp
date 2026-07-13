#include "rose.h"

#include "SgNodeHelper.h"
#include "tokenStreamMapping.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace {
SgSourceFile *findMainSourceFile(SgProject *project) {
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *source = isSgSourceFile(file)) {
      if (!source->get_isHeaderFile()) {
        return source;
      }
    }
  }
  return nullptr;
}

template <typename StatementList>
void appendStatements(const StatementList &source,
                      std::vector<SgStatement *> &result) {
  for (SgStatement *statement : source) {
    ROSE_ASSERT(statement != nullptr);
    result.push_back(statement);
  }
}

std::vector<SgStatement *> directStatements(SgNode *parent) {
  std::vector<SgStatement *> result;
  if (SgForInitStatement *forInit = isSgForInitStatement(parent)) {
    appendStatements(forInit->get_init_stmt(), result);
  } else if (SgGlobal *global = isSgGlobal(parent)) {
    appendStatements(global->get_declarations(), result);
  } else if (SgNamespaceDefinitionStatement *namespaceDefinition =
                 isSgNamespaceDefinitionStatement(parent)) {
    appendStatements(namespaceDefinition->get_declarations(), result);
  } else if (SgClassDefinition *classDefinition = isSgClassDefinition(parent)) {
    appendStatements(classDefinition->get_members(), result);
  } else if (SgTemplateClassDefinition *templateDefinition =
                 isSgTemplateClassDefinition(parent)) {
    appendStatements(templateDefinition->get_members(), result);
  } else if (SgTemplateInstantiationDefn *instantiationDefinition =
                 isSgTemplateInstantiationDefn(parent)) {
    appendStatements(instantiationDefinition->get_members(), result);
  } else if (SgBasicBlock *block = isSgBasicBlock(parent)) {
    appendStatements(block->get_statements(), result);
  }
  return result;
}

SgDeclarationGroupStatement *requireGroup(SgVariableDeclaration *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  SgDeclarationGroupStatement *group =
      isSgDeclarationGroupStatement(declaration->get_parent());
  ROSE_ASSERT(group != nullptr);
  group->validate();
  return group;
}

SgDeclarationGroupStatement *checkGroup(
    const std::map<std::string, SgVariableDeclaration *> &byName,
    const std::vector<std::string> &names, bool requireSymbols = true,
    SgDeclarationGroupStatement::source_terminator_enum terminator =
        SgDeclarationGroupStatement::e_source_terminator_file_semicolon) {
  ROSE_ASSERT(names.size() >= 2);
  SgDeclarationGroupStatement *group = nullptr;
  std::vector<SgStatement *> siblings;
  for (size_t index = 0; index < names.size(); ++index) {
    auto found = byName.find(names[index]);
    ROSE_ASSERT(found != byName.end());
    SgVariableDeclaration *declaration = found->second;
    ROSE_ASSERT(declaration->get_variables().size() == 1);
    SgInitializedName *initializedName = declaration->get_variables().front();
    ROSE_ASSERT(initializedName != nullptr);
    ROSE_ASSERT(initializedName->get_parent() == declaration);
    ROSE_ASSERT(initializedName->get_prev_decl_item() == nullptr);
    if (requireSymbols) {
      SgVariableSymbol *variableSymbol =
          isSgVariableSymbol(initializedName->get_symbol_from_symbol_table());
      ROSE_ASSERT(variableSymbol != nullptr);
      ROSE_ASSERT(variableSymbol->get_declaration() == initializedName);
    }

    if (index == 0) {
      group = requireGroup(declaration);
      ROSE_ASSERT(group->get_declarations().size() == names.size());
      ROSE_ASSERT(group->get_parent() != nullptr);
      ROSE_ASSERT(group->get_scope() != nullptr);
      ROSE_ASSERT(group->get_source_terminator() == terminator);
      siblings = directStatements(group->get_parent());
      ROSE_ASSERT(std::count(siblings.begin(), siblings.end(), group) == 1);
    } else {
      ROSE_ASSERT(declaration->get_parent() == group);
    }
    ROSE_ASSERT(group->get_declarations().at(index) == declaration);
    ROSE_ASSERT(std::find(siblings.begin(), siblings.end(), declaration) ==
                siblings.end());
  }
  return group;
}

void checkNoGroup(SgVariableDeclaration *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(declaration->get_variables().size() == 1);
  ROSE_ASSERT(isSgDeclarationGroupStatement(declaration->get_parent()) ==
              nullptr);
}

void checkDeclaratorBoundaryCommentOwner(
    SgSourceFile *source,
    const std::map<std::string, SgVariableDeclaration *> &byName) {
  ROSE_ASSERT(source != nullptr);
  SgVariableDeclaration *expectedOwner = byName.at("rex_group_comment_b");
  SgDeclarationGroupStatement *group = requireGroup(expectedOwner);
  ROSE_ASSERT(group->get_declarations().at(1) == expectedOwner);

  size_t matches = 0;
  for (SgNode *node : NodeQuery::querySubTree(source, V_SgLocatedNode)) {
    SgLocatedNode *located = isSgLocatedNode(node);
    ROSE_ASSERT(located != nullptr);
    AttachedPreprocessingInfoType *attached =
        located->getAttachedPreprocessingInfo();
    if (attached == nullptr) {
      continue;
    }
    for (PreprocessingInfo *info : *attached) {
      ROSE_ASSERT(info != nullptr);
      if (info->getString().find("/* exact declarator boundary */") ==
          std::string::npos) {
        continue;
      }
      ++matches;
      ROSE_ASSERT(located == expectedOwner);
      ROSE_ASSERT(info->getRelativePosition() == PreprocessingInfo::before);
    }
  }
  ROSE_ASSERT(matches == 1);
}

TokenStreamSequenceToNodeMapping *
requireDirectTokenMapping(SgSourceFile *source,
                          SgDeclarationStatement *declaration) {
  ROSE_ASSERT(source != nullptr);
  ROSE_ASSERT(declaration != nullptr);
  auto &tokenMap = source->get_tokenSubsequenceMap();
  auto found = tokenMap.find(declaration);
  ROSE_ASSERT(found != tokenMap.end());
  ROSE_ASSERT(found->second != nullptr);
  ROSE_ASSERT(std::count(found->second->nodeVector.begin(),
                         found->second->nodeVector.end(), declaration) == 1);
  return found->second;
}

void checkTokenGroup(
    SgSourceFile *source,
    const std::map<std::string, SgVariableDeclaration *> &byName,
    const std::vector<std::string> &names) {
  ROSE_ASSERT(names.size() >= 2);
  SgDeclarationGroupStatement *group = requireGroup(byName.at(names.front()));
  ROSE_ASSERT(group->get_source_terminator() ==
              SgDeclarationGroupStatement::e_source_terminator_file_semicolon);
  TokenStreamSequenceToNodeMapping *mapping =
      requireDirectTokenMapping(source, group);
  const SgTokenPtrList &tokens = source->get_token_list();
  const TokenStreamHalfOpenInterval &core =
      mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence);
  ROSE_ASSERT(!core.empty());
  ROSE_ASSERT(static_cast<size_t>(core.end) <= tokens.size());
  ROSE_ASSERT(tokens.at(core.end - 1) != nullptr);
  ROSE_ASSERT(tokens.at(core.end - 1)->get_lexeme_string() == ";");
  for (const std::string &name : names) {
    SgVariableDeclaration *member = byName.at(name);
    ROSE_ASSERT(member->get_parent() == group);
    ROSE_ASSERT(source->get_tokenSubsequenceMap().find(member) ==
                source->get_tokenSubsequenceMap().end());
  }
  ROSE_ASSERT(!mapping->shared || mapping->nodeVector.size() > 1);
}

void checkMacroTokenGroup(
    SgSourceFile *source,
    const std::map<std::string, SgVariableDeclaration *> &byName,
    const std::vector<std::string> &names) {
  ROSE_ASSERT(names.size() >= 2);
  SgDeclarationGroupStatement *group = requireGroup(byName.at(names.front()));
  ROSE_ASSERT(group->get_source_terminator() ==
              SgDeclarationGroupStatement::e_source_terminator_macro_semicolon);
  TokenStreamSequenceToNodeMapping *mapping =
      requireDirectTokenMapping(source, group);
  const TokenStreamHalfOpenInterval &core =
      mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence);
  ROSE_ASSERT(core.end == core.begin + 1);
  const SgTokenPtrList &tokens = source->get_token_list();
  ROSE_ASSERT(core.begin >= 0);
  ROSE_ASSERT(static_cast<size_t>(core.end) <= tokens.size());
  SgToken *invocation = tokens.at(core.begin);
  ROSE_ASSERT(invocation != nullptr);
  ROSE_ASSERT(invocation->get_lexeme_string() == "REX_GROUP_MACRO_DECLARATION");
  for (const std::string &name : names) {
    SgVariableDeclaration *member = byName.at(name);
    ROSE_ASSERT(member->get_parent() == group);
    ROSE_ASSERT(source->get_tokenSubsequenceMap().find(member) ==
                source->get_tokenSubsequenceMap().end());
  }
}

void checkFileTerminatedMacroTokenGroup(
    SgSourceFile *source,
    const std::map<std::string, SgVariableDeclaration *> &byName,
    const std::vector<std::string> &names) {
  ROSE_ASSERT(names.size() >= 2);
  SgDeclarationGroupStatement *group = requireGroup(byName.at(names.front()));
  ROSE_ASSERT(group->get_source_terminator() ==
              SgDeclarationGroupStatement::e_source_terminator_file_semicolon);
  TokenStreamSequenceToNodeMapping *mapping =
      requireDirectTokenMapping(source, group);
  const TokenStreamHalfOpenInterval &core =
      mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence);
  ROSE_ASSERT(core.begin >= 0);
  ROSE_ASSERT(core.end > core.begin + 1);
  const SgTokenPtrList &tokens = source->get_token_list();
  ROSE_ASSERT(static_cast<size_t>(core.end) <= tokens.size());
  SgToken *invocation = tokens.at(core.begin);
  SgToken *terminator = tokens.at(core.end - 1);
  ROSE_ASSERT(invocation != nullptr);
  ROSE_ASSERT(terminator != nullptr);
  ROSE_ASSERT(invocation->get_lexeme_string() ==
              "REX_GROUP_FILE_TERMINATED_DECLARATION");
  ROSE_ASSERT(terminator->get_lexeme_string() == ";");
  for (const std::string &name : names) {
    SgVariableDeclaration *member = byName.at(name);
    ROSE_ASSERT(member->get_parent() == group);
    ROSE_ASSERT(source->get_tokenSubsequenceMap().find(member) ==
                source->get_tokenSubsequenceMap().end());
  }
}

void injectTokenMappingCorruption(
    SgSourceFile *source,
    const std::map<std::string, SgVariableDeclaration *> &byName,
    const char *corruption) {
  ROSE_ASSERT(source != nullptr);
  ROSE_ASSERT(corruption != nullptr);
  auto &tokenMap = source->get_tokenSubsequenceMap();
  if (std::string(corruption) == "borrow-independent") {
    SgVariableDeclaration *first = byName.at("rex_group_independent_a");
    SgVariableDeclaration *second = byName.at("rex_group_independent_b");
    TokenStreamSequenceToNodeMapping *firstMapping =
        requireDirectTokenMapping(source, first);
    TokenStreamSequenceToNodeMapping *secondMapping =
        requireDirectTokenMapping(source, second);
    auto secondNode = std::find(secondMapping->nodeVector.begin(),
                                secondMapping->nodeVector.end(), second);
    ROSE_ASSERT(secondNode != secondMapping->nodeVector.end());
    secondMapping->nodeVector.erase(secondNode);
    ROSE_ASSERT(std::find(firstMapping->nodeVector.begin(),
                          firstMapping->nodeVector.end(),
                          second) == firstMapping->nodeVector.end());
    firstMapping->nodeVector.push_back(second);
    firstMapping->shared = true;
    tokenMap.at(second) = firstMapping;
    return;
  }
  if (std::string(corruption) == "macro-as-file-terminator") {
    SgDeclarationGroupStatement *group =
        requireGroup(byName.at("rex_group_macro_a"));
    ROSE_ASSERT(
        group->get_source_terminator() ==
        SgDeclarationGroupStatement::e_source_terminator_macro_semicolon);
    group->set_source_terminator(
        SgDeclarationGroupStatement::e_source_terminator_file_semicolon);
    return;
  }

  auto target = byName.find("rex_group_local_b");
  if (target == byName.end()) {
    target = byName.find("rex_group_range_init_b");
  }
  ROSE_ASSERT(target != byName.end());

  SgDeclarationGroupStatement *group = requireGroup(target->second);
  TokenStreamSequenceToNodeMapping *mapping =
      requireDirectTokenMapping(source, group);
  if (std::string(corruption) == "erase-direct") {
    ROSE_ASSERT(tokenMap.erase(group) == 1);
  } else if (std::string(corruption) == "drop-group-member") {
    auto associated = std::find(mapping->nodeVector.begin(),
                                mapping->nodeVector.end(), group);
    ROSE_ASSERT(associated != mapping->nodeVector.end());
    mapping->nodeVector.erase(associated);
  } else if (std::string(corruption) == "direct-group-member") {
    ROSE_ASSERT(tokenMap.emplace(target->second, mapping).second);
  } else if (std::string(corruption) == "borrow-group-member") {
    ROSE_ASSERT(std::find(mapping->nodeVector.begin(),
                          mapping->nodeVector.end(),
                          target->second) == mapping->nodeVector.end());
    mapping->nodeVector.push_back(target->second);
    mapping->shared = true;
  } else if (std::string(corruption) == "truncate-group-semicolon") {
    const SgTokenPtrList &tokens = source->get_token_list();
    const TokenStreamHalfOpenInterval &published_core =
        mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence);
    ROSE_ASSERT(published_core.end > published_core.begin + 1);
    ROSE_ASSERT(static_cast<size_t>(published_core.end) <= tokens.size());
    ROSE_ASSERT(tokens.at(published_core.end - 1) != nullptr);
    ROSE_ASSERT(tokens.at(published_core.end - 1)->get_lexeme_string() == ";");
    TokenStreamHalfOpenInterval &corrupted_core =
        const_cast<TokenStreamHalfOpenInterval &>(published_core);
    --corrupted_core.end;
  } else if (std::string(corruption) == "file-as-macro-terminator") {
    ROSE_ASSERT(
        group->get_source_terminator() ==
        SgDeclarationGroupStatement::e_source_terminator_file_semicolon);
    group->set_source_terminator(
        SgDeclarationGroupStatement::e_source_terminator_macro_semicolon);
  } else {
    fprintf(stderr, "Unknown token-mapping corruption mode: %s\n", corruption);
    ROSE_ABORT();
  }
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  SgSourceFile *source = findMainSourceFile(project);
  ROSE_ASSERT(source != nullptr);

  std::map<std::string, SgVariableDeclaration *> byName;
  std::map<std::string, SgVariableDeclaration *> conditionDeclarations;
  for (SgNode *node :
       NodeQuery::querySubTree(source, V_SgVariableDeclaration)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    if (declaration == nullptr || declaration->get_variables().size() != 1) {
      continue;
    }
    SgInitializedName *initializedName = declaration->get_variables().front();
    ROSE_ASSERT(initializedName != nullptr);
    const std::string name = initializedName->get_name().getString();
    if (name.rfind("rex_group_", 0) == 0) {
      ROSE_ASSERT(byName.emplace(name, declaration).second);
    } else if (name.rfind("rex_condition_", 0) == 0 &&
               name != "rex_condition_source") {
      ROSE_ASSERT(conditionDeclarations.emplace(name, declaration).second);
    }
  }

  if (const char *corruption =
          std::getenv("REX_TEST_TOKEN_MAPPING_CORRUPTION")) {
    ROSE_ASSERT(!source->get_tokenSubsequenceMap().empty());
    injectTokenMappingCorruption(source, byName, corruption);
    // The normal token-unparse frontier is the production validation boundary.
    // A malformed mapping must fail there without a test-only validation API.
    return backend(project);
  }

  if (!conditionDeclarations.empty()) {
    ROSE_ASSERT(conditionDeclarations.size() == 5);
    auto checkConditionOwner = [&](const char *name, VariantT expectedParent) {
      SgVariableDeclaration *declaration = conditionDeclarations.at(name);
      checkNoGroup(declaration);
      SgScopeStatement *parent = isSgScopeStatement(declaration->get_parent());
      ROSE_ASSERT(parent != nullptr);
      ROSE_ASSERT(parent->variantT() == expectedParent);

      // Condition-owning scopes do not expose a generic statement list.  The
      // producer contract is the exact typed child edge for each construct.
      SgStatement *ownedCondition = nullptr;
      if (SgIfStmt *ifStatement = isSgIfStmt(parent)) {
        ownedCondition = ifStatement->get_conditional();
      } else if (SgWhileStmt *whileStatement = isSgWhileStmt(parent)) {
        ownedCondition = whileStatement->get_condition();
      } else if (SgSwitchStatement *switchStatement =
                     isSgSwitchStatement(parent)) {
        ownedCondition = switchStatement->get_item_selector();
      } else if (SgForStatement *forStatement = isSgForStatement(parent)) {
        ownedCondition = forStatement->get_test();
      } else if (SgCatchOptionStmt *catchStatement =
                     isSgCatchOptionStmt(parent)) {
        ownedCondition = catchStatement->get_condition();
      }
      ROSE_ASSERT(ownedCondition == declaration);
      if (!source->get_tokenSubsequenceMap().empty()) {
        requireDirectTokenMapping(source, declaration);
      }
    };
    checkConditionOwner("rex_condition_if", V_SgIfStmt);
    checkConditionOwner("rex_condition_while", V_SgWhileStmt);
    checkConditionOwner("rex_condition_switch", V_SgSwitchStatement);
    checkConditionOwner("rex_condition_for", V_SgForStatement);
    checkConditionOwner("rex_condition_catch", V_SgCatchOptionStmt);

    // A catch variable reference must bind directly to the initialized name
    // structurally owned by the catch condition declaration.  This exercises
    // the production SgNodeHelper path that formerly repaired a malformed
    // catch-variable edge after frontend construction.
    SgVariableDeclaration *catchDeclaration =
        conditionDeclarations.at("rex_condition_catch");
    SgInitializedName *catchName = catchDeclaration->get_variables().front();
    SgCatchOptionStmt *catchStatement =
        isSgCatchOptionStmt(catchDeclaration->get_parent());
    ROSE_ASSERT(catchName != nullptr);
    ROSE_ASSERT(catchStatement != nullptr);
    ROSE_ASSERT(catchName->get_parent() == catchDeclaration);
    ROSE_ASSERT(catchStatement->get_condition() == catchDeclaration);
    ROSE_ASSERT(catchDeclaration->get_childIndex(catchName) == 0);

    size_t catchReferenceCount = 0;
    for (SgNode *node : NodeQuery::querySubTree(source, V_SgVarRefExp)) {
      SgVarRefExp *reference = isSgVarRefExp(node);
      ROSE_ASSERT(reference != nullptr);
      if (reference->get_symbol() == nullptr ||
          reference->get_symbol()->get_name() != catchName->get_name()) {
        continue;
      }
      ++catchReferenceCount;
      SgSymbol *symbol = SgNodeHelper::getSymbolOfVariable(reference);
      ROSE_ASSERT(symbol == reference->get_symbol());
      SgVariableSymbol *variableSymbol = isSgVariableSymbol(symbol);
      ROSE_ASSERT(variableSymbol != nullptr);
      ROSE_ASSERT(variableSymbol->get_declaration() == catchName);
    }
    ROSE_ASSERT(catchReferenceCount == 1);
    return backend(project);
  }

  if (byName.count("rex_group_range_init_a") != 0) {
    checkGroup(byName, {"rex_group_range_init_a", "rex_group_range_init_b",
                        "rex_group_range_init_c"});
    checkNoGroup(byName.at("rex_group_range_values"));
    if (!source->get_tokenSubsequenceMap().empty()) {
      checkTokenGroup(source, byName,
                      {"rex_group_range_init_a", "rex_group_range_init_b",
                       "rex_group_range_init_c"});
      requireDirectTokenMapping(source, byName.at("rex_group_range_values"));
    }
    return backend(project);
  }

  checkGroup(byName, {"rex_group_global_a", "rex_group_global_b",
                      "rex_group_global_c"});
  checkGroup(byName, {"rex_group_file_macro_a", "rex_group_file_macro_b"});
  checkGroup(byName, {"rex_group_macro_a", "rex_group_macro_b"}, true,
             SgDeclarationGroupStatement::e_source_terminator_macro_semicolon);
  checkGroup(byName, {"rex_group_enum_a", "rex_group_enum_b"});
  checkGroup(byName, {"rex_group_namespace_a", "rex_group_namespace_b"});
  checkGroup(byName,
             {"rex_group_field_a", "rex_group_field_b", "rex_group_field_c"});
  checkGroup(byName, {"rex_group_static_a", "rex_group_static_b"});
  checkGroup(byName, {"rex_group_linkage_a", "rex_group_linkage_b"});
  checkGroup(byName,
             {"rex_group_local_a", "rex_group_local_b", "rex_group_local_c"});
  checkGroup(byName, {"rex_group_copy_a", "rex_group_copy_b"});
  checkGroup(byName, {"rex_group_for_a", "rex_group_for_b"});
  checkGroup(byName, {"rex_group_attribute_a", "rex_group_attribute_b"});
  SgInitializedName *attributeA =
      byName.at("rex_group_attribute_a")->get_variables().front();
  SgInitializedName *attributeB =
      byName.at("rex_group_attribute_b")->get_variables().front();
  ROSE_ASSERT(attributeA != nullptr);
  ROSE_ASSERT(attributeB != nullptr);
  ROSE_ASSERT(attributeA->isGnuAttributeUnused());
  ROSE_ASSERT(!attributeA->isGnuAttributeUsed());
  ROSE_ASSERT(!attributeB->isGnuAttributeUnused());
  ROSE_ASSERT(attributeB->isGnuAttributeUsed());
  checkGroup(byName, {"rex_group_comment_a", "rex_group_comment_b"});
  checkDeclaratorBoundaryCommentOwner(source, byName);
  checkNoGroup(byName.at("rex_group_independent_a"));
  checkNoGroup(byName.at("rex_group_independent_b"));

  if (!source->get_tokenSubsequenceMap().empty()) {
    checkTokenGroup(
        source, byName,
        {"rex_group_global_a", "rex_group_global_b", "rex_group_global_c"});
    checkFileTerminatedMacroTokenGroup(
        source, byName, {"rex_group_file_macro_a", "rex_group_file_macro_b"});
    checkMacroTokenGroup(source, byName,
                         {"rex_group_macro_a", "rex_group_macro_b"});
    checkTokenGroup(source, byName, {"rex_group_enum_a", "rex_group_enum_b"});
    checkTokenGroup(source, byName,
                    {"rex_group_namespace_a", "rex_group_namespace_b"});
    checkTokenGroup(
        source, byName,
        {"rex_group_field_a", "rex_group_field_b", "rex_group_field_c"});
    checkTokenGroup(source, byName,
                    {"rex_group_static_a", "rex_group_static_b"});
    checkTokenGroup(source, byName,
                    {"rex_group_linkage_a", "rex_group_linkage_b"});
    checkTokenGroup(
        source, byName,
        {"rex_group_local_a", "rex_group_local_b", "rex_group_local_c"});
    checkTokenGroup(source, byName, {"rex_group_copy_a", "rex_group_copy_b"});
    checkTokenGroup(source, byName, {"rex_group_for_a", "rex_group_for_b"});
    checkTokenGroup(source, byName,
                    {"rex_group_attribute_a", "rex_group_attribute_b"});
    checkTokenGroup(source, byName,
                    {"rex_group_comment_a", "rex_group_comment_b"});
    ROSE_ASSERT(requireDirectTokenMapping(
                    source, byName.at("rex_group_independent_a")) !=
                requireDirectTokenMapping(
                    source, byName.at("rex_group_independent_b")));
  }

  SgDeclarationGroupStatement *copySourceGroup =
      requireGroup(byName.at("rex_group_copy_a"));
  SgBasicBlock *copySource = isSgBasicBlock(copySourceGroup->get_parent());
  ROSE_ASSERT(copySource != nullptr);
  SgBasicBlock *copy = isSgBasicBlock(SageInterface::deepCopy(copySource));
  ROSE_ASSERT(copy != nullptr);
  std::map<std::string, SgVariableDeclaration *> copiedByName;
  for (SgNode *node : NodeQuery::querySubTree(copy, V_SgVariableDeclaration)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_variables().size() == 1);
    const std::string name =
        declaration->get_variables().front()->get_name().getString();
    if (name == "rex_group_copy_a" || name == "rex_group_copy_b") {
      ROSE_ASSERT(copiedByName.emplace(name, declaration).second);
    }
  }
  ROSE_ASSERT(copiedByName.size() == 2);
  checkGroup(copiedByName, {"rex_group_copy_a", "rex_group_copy_b"}, false);

  SgSymbolTable *copySymbolTable = copy->get_symbol_table();
  ROSE_ASSERT(copySymbolTable != nullptr);
  ROSE_ASSERT(copySymbolTable->get_table() != nullptr);
  copySymbolTable->get_table()->clear();
  copySymbolTable->set_symbolSet(SgNodeSet());
  copySymbolTable->clear_functionSymbolExactIndex();
  ROSE_ASSERT(copySymbolTable->size() == 0);
  SageInterface::rebuildSymbolTable(copy);
  checkGroup(copiedByName, {"rex_group_copy_a", "rex_group_copy_b"});

  return backend(project);
}
