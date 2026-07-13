#include "rose.h"

#include "tokenStreamMapping.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {
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

SgDeclarationGroupStatement *
checkGroup(SgSourceFile *source,
           const std::map<std::string, SgDeclarationStatement *> &declarations,
           const std::vector<std::string> &names,
           bool requireTokenMapping = true) {
  ROSE_ASSERT(source != nullptr);
  ROSE_ASSERT(names.size() >= 2);
  const bool validateOutputClassification =
      !requireTokenMapping || source->get_tokenSubsequenceMap().empty();
  auto requireOutputClassification = [](SgLocatedNode *surface,
                                        bool expectedOutput,
                                        const std::string &role) {
    ROSE_ASSERT(surface != nullptr);
    ROSE_ASSERT(surface->get_file_info() != nullptr);
    ROSE_ASSERT(surface->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(surface->get_endOfConstruct() != nullptr);
    const bool primaryOutput =
        surface->get_file_info()->isOutputInCodeGeneration();
    const bool startOutput =
        surface->get_startOfConstruct()->isOutputInCodeGeneration();
    const bool endOutput =
        surface->get_endOfConstruct()->isOutputInCodeGeneration();
    if (primaryOutput != expectedOutput || startOutput != expectedOutput ||
        endOutput != expectedOutput) {
      fprintf(stderr,
              "REX_TEST_INVARIANT[mixed-declarator-group-output]: role=%s "
              "surface=%p/%s expected=%d primary/start/end=%d/%d/%d\n",
              role.c_str(), static_cast<void *>(surface),
              surface->class_name().c_str(), expectedOutput ? 1 : 0,
              primaryOutput ? 1 : 0, startOutput ? 1 : 0, endOutput ? 1 : 0);
      ROSE_ABORT();
    }
  };
  SgDeclarationGroupStatement *group = nullptr;
  Sg_File_Info *groupStart = nullptr;
  for (size_t index = 0; index < names.size(); ++index) {
    auto found = declarations.find(names[index]);
    if (found == declarations.end()) {
      size_t candidates = 0;
      for (SgNode *node : NodeQuery::querySubTree(source->get_globalScope(),
                                                  V_SgDeclarationStatement)) {
        SgDeclarationStatement *candidate = isSgDeclarationStatement(node);
        ROSE_ASSERT(candidate != nullptr);
        if (declarationName(candidate) != names[index]) {
          continue;
        }
        ++candidates;
        fprintf(stderr,
                "REX_TEST_INVARIANT[mixed-declarator-group]: ungrouped "
                "candidate=%p name=%s type=%s parent=%p/%s scope=%p\n",
                static_cast<void *>(candidate), names[index].c_str(),
                candidate->class_name().c_str(),
                static_cast<void *>(candidate->get_parent()),
                candidate->get_parent() != nullptr
                    ? candidate->get_parent()->class_name().c_str()
                    : "<null>",
                static_cast<void *>(candidate->get_scope()));
      }
      fprintf(stderr,
              "REX_TEST_INVARIANT[mixed-declarator-group]: expected grouped "
              "declaration name=%s is absent; ungrouped candidates=%zu\n",
              names[index].c_str(), candidates);
      ROSE_ABORT();
    }
    SgDeclarationStatement *declaration = found->second;
    ROSE_ASSERT(declaration != nullptr);
    if (index == 0) {
      group = isSgDeclarationGroupStatement(declaration->get_parent());
      if (group == nullptr) {
        fprintf(stderr,
                "REX_TEST_INVARIANT[mixed-declarator-group]: declaration=%p "
                "name=%s type=%s has non-group parent=%p/%s\n",
                static_cast<void *>(declaration), names[index].c_str(),
                declaration->class_name().c_str(),
                static_cast<void *>(declaration->get_parent()),
                declaration->get_parent() != nullptr
                    ? declaration->get_parent()->class_name().c_str()
                    : "<null>");
        ROSE_ABORT();
      }
      group->validate();
      ROSE_ASSERT(group->get_declarations().size() == names.size());
      ROSE_ASSERT(group->get_parent() != nullptr);
      ROSE_ASSERT(group->get_scope() != nullptr);
      groupStart = group->get_startOfConstruct();
      ROSE_ASSERT(groupStart != nullptr);
      if (validateOutputClassification) {
        requireOutputClassification(group, true, names.front() + " group");
      }
    } else {
      ROSE_ASSERT(declaration->get_parent() == group);
      Sg_File_Info *memberStart = declaration->get_startOfConstruct();
      ROSE_ASSERT(memberStart != nullptr);
      ROSE_ASSERT(memberStart->get_physical_file_id() ==
                  groupStart->get_physical_file_id());
      ROSE_ASSERT(memberStart->get_physical_line() ==
                  groupStart->get_physical_line());
      ROSE_ASSERT(memberStart->get_col() == groupStart->get_col());
    }
    ROSE_ASSERT(group->get_declarations().at(index) == declaration);
    SgScopeStatement *lexicalScope = group->get_scope();
    ROSE_ASSERT(lexicalScope != nullptr);
    if (SgVariableDeclaration *variable =
            isSgVariableDeclaration(declaration)) {
      // The declaration group is the exact lexical owner.  A variable's
      // initialized name independently records its semantic lookup scope;
      // these differ, for example, across a reopened namespace definition.
      ROSE_ASSERT(variable->get_scope() == lexicalScope);
      ROSE_ASSERT(variable->get_variables().size() == 1);
      SgInitializedName *name = variable->get_variables().front();
      ROSE_ASSERT(name != nullptr);
      ROSE_ASSERT(name->get_parent() == variable);
      ROSE_ASSERT(name->get_scope() != nullptr);
      SgVariableSymbol *symbol = isSgVariableSymbol(
          name->get_scope()->find_symbol_from_declaration(name));
      ROSE_ASSERT(symbol != nullptr);
      ROSE_ASSERT(symbol->get_declaration() == name);
    } else if (SgFunctionDeclaration *function =
                   isSgFunctionDeclaration(declaration)) {
      // Function declarations store their semantic scope explicitly.  A
      // block- or for-init declarator denotes a namespace-scope function even
      // though the source group remains owned by the local lexical surface.
      SgScopeStatement *semanticScope = function->get_scope();
      ROSE_ASSERT(semanticScope != nullptr);
      const bool localLexicalScope =
          isSgGlobal(lexicalScope) == nullptr &&
          isSgNamespaceDefinitionStatement(lexicalScope) == nullptr &&
          isSgClassDefinition(lexicalScope) == nullptr;
      if (localLexicalScope) {
        ROSE_ASSERT(isSgGlobal(semanticScope) != nullptr);
        ROSE_ASSERT(semanticScope != lexicalScope);
      }
      ROSE_ASSERT(function->get_symbol_from_symbol_table() != nullptr);
      SgFunctionParameterList *parameters = function->get_parameterList();
      ROSE_ASSERT(parameters != nullptr);
      ROSE_ASSERT(parameters->get_parent() == function);
      if (!source->get_tokenSubsequenceMap().empty()) {
        ROSE_ASSERT(source->get_tokenSubsequenceMap().find(parameters) ==
                    source->get_tokenSubsequenceMap().end());
      }

      if (SgMemberFunctionDeclaration *member =
              isSgMemberFunctionDeclaration(function)) {
        SgCtorInitializerList *initializers = member->get_CtorInitializerList();
        ROSE_ASSERT(initializers != nullptr);
        ROSE_ASSERT(initializers->get_parent() == member);
        ROSE_ASSERT(initializers->get_ctors().empty());
        ROSE_ASSERT(initializers->get_firstNondefiningDeclaration() ==
                    initializers);
        ROSE_ASSERT(initializers->get_definingDeclaration() == initializers);
        ROSE_ASSERT(
            !initializers->get_translation_unit_source_order().has_value());
        for (Sg_File_Info *position : {initializers->get_file_info(),
                                       initializers->get_startOfConstruct(),
                                       initializers->get_endOfConstruct()}) {
          ROSE_ASSERT(position != nullptr);
          ROSE_ASSERT(position->get_parent() == initializers);
          ROSE_ASSERT(!position->isShared());
          ROSE_ASSERT(position->isCompilerGenerated());
          ROSE_ASSERT(position->isFrontendSpecific());
          ROSE_ASSERT(!position->isTransformation());
          ROSE_ASSERT(position->isOutputInCodeGeneration());
          ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
          ROSE_ASSERT(position->get_file_id() ==
                      Sg_File_Info::COMPILER_GENERATED_FILE_ID);
          ROSE_ASSERT(position->get_physical_file_id() ==
                      Sg_File_Info::COMPILER_GENERATED_FILE_ID);
        }
        if (!source->get_tokenSubsequenceMap().empty()) {
          ROSE_ASSERT(source->get_tokenSubsequenceMap().find(initializers) ==
                      source->get_tokenSubsequenceMap().end());
        }
      }
    } else {
      SgTypedefDeclaration *typedefDeclaration =
          isSgTypedefDeclaration(declaration);
      ROSE_ASSERT(typedefDeclaration != nullptr);
      ROSE_ASSERT(typedefDeclaration->get_scope() == lexicalScope);
      ROSE_ASSERT(typedefDeclaration->get_symbol_from_symbol_table() !=
                  nullptr);
    }
    if (validateOutputClassification) {
      requireOutputClassification(declaration, true, names[index]);
    }
    if (!source->get_tokenSubsequenceMap().empty()) {
      ROSE_ASSERT(source->get_tokenSubsequenceMap().find(declaration) ==
                  source->get_tokenSubsequenceMap().end());
    }
  }
  ROSE_ASSERT(group->get_source_terminator() ==
              SgDeclarationGroupStatement::e_source_terminator_file_semicolon);
  if (!source->get_tokenSubsequenceMap().empty()) {
    auto direct = source->get_tokenSubsequenceMap().find(group);
    if (!requireTokenMapping) {
      ROSE_ASSERT(direct == source->get_tokenSubsequenceMap().end());
      return group;
    }
    ROSE_ASSERT(direct != source->get_tokenSubsequenceMap().end());
    ROSE_ASSERT(direct->second != nullptr);
    ROSE_ASSERT(std::count(direct->second->nodeVector.begin(),
                           direct->second->nodeVector.end(), group) == 1);
    const SgTokenPtrList &tokens = source->get_token_list();
    const TokenStreamHalfOpenInterval &core = direct->second->halfOpenInterval(
        TokenStreamIntervalKind::token_subsequence);
    ROSE_ASSERT(!core.empty());
    ROSE_ASSERT(static_cast<size_t>(core.end) <= tokens.size());
    ROSE_ASSERT(tokens.at(core.end - 1) != nullptr);
    ROSE_ASSERT(tokens.at(core.end - 1)->get_lexeme_string() == ";");
  }
  return group;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  SgSourceFile *source = nullptr;
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *candidate = isSgSourceFile(file)) {
      if (!candidate->get_isHeaderFile()) {
        source = candidate;
        break;
      }
    }
  }
  ROSE_ASSERT(source != nullptr);

  std::map<std::string, SgDeclarationStatement *> declarations;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgDeclarationStatement)) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    ROSE_ASSERT(declaration != nullptr);
    const std::string name = declarationName(declaration);
    if (name.rfind("rex_mixed_", 0) == 0 &&
        isSgDeclarationGroupStatement(declaration->get_parent()) != nullptr) {
      ROSE_ASSERT(declarations.emplace(name, declaration).second);
    }
  }

  checkGroup(source, declarations,
             {"rex_mixed_global_object", "rex_mixed_global_function"});
  checkGroup(
      source, declarations,
      {"rex_mixed_global_function_first", "rex_mixed_global_object_second"});
  checkGroup(source, declarations,
             {"rex_mixed_function_a", "rex_mixed_function_b"});
  SgDeclarationGroupStatement *functionPrefixGroup = checkGroup(
      source, declarations,
      {"rex_mixed_function_prefix_first", "rex_mixed_function_prefix_middle",
       "rex_mixed_function_prefix_last"});
  ROSE_ASSERT(functionPrefixGroup != nullptr);
  ROSE_ASSERT(isSgGlobal(functionPrefixGroup->get_scope()) != nullptr);
  for (SgDeclarationStatement *member :
       functionPrefixGroup->get_declarations()) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(member);
    ROSE_ASSERT(function != nullptr);
    ROSE_ASSERT(function->get_parent() == functionPrefixGroup);
    ROSE_ASSERT(function->get_scope() == functionPrefixGroup->get_scope());
    ROSE_ASSERT(function->get_definition() == nullptr);
    ROSE_ASSERT(function->get_firstNondefiningDeclaration() == function);
    ROSE_ASSERT(function->get_symbol_from_symbol_table() != nullptr);
  }
  SgDeclarationGroupStatement *reopenedNamespaceGroup =
      checkGroup(source, declarations,
                 {"rex_mixed_reopened_object", "rex_mixed_reopened_function"});
  SgVariableDeclaration *reopenedNamespaceVariable =
      isSgVariableDeclaration(declarations.at("rex_mixed_reopened_object"));
  ROSE_ASSERT(reopenedNamespaceGroup != nullptr);
  ROSE_ASSERT(reopenedNamespaceVariable != nullptr);
  ROSE_ASSERT(reopenedNamespaceVariable->get_variables().size() == 1);
  ROSE_ASSERT(reopenedNamespaceVariable->get_variables().front() != nullptr);
  SgScopeStatement *reopenedSemanticScope =
      reopenedNamespaceVariable->get_variables().front()->get_scope();
  ROSE_ASSERT(reopenedSemanticScope != reopenedNamespaceGroup->get_scope());
  reopenedNamespaceVariable->set_scope(reopenedNamespaceGroup->get_scope());
  ROSE_ASSERT(reopenedNamespaceVariable->get_variables().front()->get_scope() ==
              reopenedSemanticScope);
  checkGroup(source, declarations,
             {"rex_mixed_typedef_scalar", "rex_mixed_typedef_function"});
  SgDeclarationGroupStatement *embeddedTypedefGroup =
      checkGroup(source, declarations,
                 {"rex_mixed_embedded_first", "rex_mixed_embedded_second"});
  ROSE_ASSERT(embeddedTypedefGroup != nullptr);
  SgTypedefDeclaration *embeddedTypedef =
      isSgTypedefDeclaration(declarations.at("rex_mixed_embedded_first"));
  ROSE_ASSERT(embeddedTypedef != nullptr);
  SgClassDeclaration *embeddedRecord =
      isSgClassDeclaration(embeddedTypedef->get_declaration());
  ROSE_ASSERT(embeddedRecord != nullptr);
  if (SgClassDeclaration *defining =
          isSgClassDeclaration(embeddedRecord->get_definingDeclaration())) {
    embeddedRecord = defining;
  }
  ROSE_ASSERT(embeddedRecord->get_definition() != nullptr);
  ROSE_ASSERT(embeddedRecord->get_parent() == embeddedTypedef);
  ROSE_ASSERT(embeddedRecord->get_scope() == embeddedTypedefGroup->get_scope());
  ROSE_ASSERT(!embeddedRecord->get_isAutonomousDeclaration());
  // The tag has no autonomous statement surface, but it is the exact typed
  // source child emitted through the typedef's declaration group.
  ROSE_ASSERT(embeddedRecord->isOutputInCodeGeneration());
  ROSE_ASSERT(embeddedRecord->get_startOfConstruct() != nullptr);
  ROSE_ASSERT(
      embeddedRecord->get_startOfConstruct()->isOutputInCodeGeneration());
  ROSE_ASSERT(embeddedRecord->get_endOfConstruct() != nullptr);
  ROSE_ASSERT(embeddedRecord->get_endOfConstruct()->isOutputInCodeGeneration());
  checkGroup(source, declarations, {"rex_mixed_field", "rex_mixed_method"});
  checkGroup(source, declarations,
             {"rex_mixed_pointer_field", "rex_mixed_pointer_method"});
  checkGroup(source, declarations,
             {"rex_mixed_later_field_first", "rex_mixed_later_field_second"});
  checkGroup(source, declarations,
             {"rex_mixed_later_method_first", "rex_mixed_later_method_second"});
  SgDeclarationGroupStatement *localGroup = checkGroup(
      source, declarations, {"rex_mixed_local", "rex_mixed_block_function"});
  ROSE_ASSERT(localGroup != nullptr);
  SgFunctionDeclaration *blockFunction =
      isSgFunctionDeclaration(declarations.at("rex_mixed_block_function"));
  ROSE_ASSERT(blockFunction != nullptr);
  ROSE_ASSERT(blockFunction->get_declarationModifier()
                  .get_storageModifier()
                  .get_modifier() == SgStorageModifier::e_default);
  SgDeclarationGroupStatement *localExternGroup = checkGroup(
      source, declarations,
      {"rex_mixed_local_extern_object", "rex_mixed_local_extern_function"});
  ROSE_ASSERT(localExternGroup != nullptr);
  for (SgDeclarationStatement *member : localExternGroup->get_declarations()) {
    ROSE_ASSERT(member != nullptr);
    ROSE_ASSERT(member->get_declarationModifier()
                    .get_storageModifier()
                    .get_modifier() == SgStorageModifier::e_extern);
  }
  SgDeclarationGroupStatement *forInitGroup = checkGroup(
      source, declarations, {"rex_mixed_for_object", "rex_mixed_for_function"});
  ROSE_ASSERT(forInitGroup != nullptr);
  SgForInitStatement *forInitWrapper =
      isSgForInitStatement(forInitGroup->get_parent());
  ROSE_ASSERT(forInitWrapper != nullptr);
  SgForStatement *forOwner = isSgForStatement(forInitWrapper->get_parent());
  ROSE_ASSERT(forOwner != nullptr);
  ROSE_ASSERT(forOwner->get_for_init_stmt() == forInitWrapper);
  ROSE_ASSERT(forInitGroup->get_scope() == forOwner);
  ROSE_ASSERT(forInitWrapper->get_init_stmt().size() == 1);
  ROSE_ASSERT(forInitWrapper->get_init_stmt().front() == forInitGroup);
  const SgNodePtrList forOwnerSuccessors =
      forOwner->get_traversalSuccessorContainer();
  const SgNodePtrList forInitSuccessors =
      forInitWrapper->get_traversalSuccessorContainer();
  ROSE_ASSERT(std::count(forOwnerSuccessors.begin(), forOwnerSuccessors.end(),
                         forInitWrapper) == 1);
  ROSE_ASSERT(forInitSuccessors.size() == 1);
  ROSE_ASSERT(forInitSuccessors.front() == forInitGroup);
  for (SgLocatedNode *surface : {static_cast<SgLocatedNode *>(forInitWrapper),
                                 static_cast<SgLocatedNode *>(forInitGroup)}) {
    for (Sg_File_Info *position :
         {surface->get_file_info(), surface->get_startOfConstruct(),
          surface->get_endOfConstruct()}) {
      ROSE_ASSERT(position != nullptr);
      ROSE_ASSERT(position->get_parent() == surface);
      ROSE_ASSERT(!position->isShared());
      ROSE_ASSERT(!position->isCompilerGenerated());
      ROSE_ASSERT(!position->isFrontendSpecific());
      ROSE_ASSERT(!position->isTransformation());
      ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
      ROSE_ASSERT(position->get_physical_file_id() >= 0);
    }
  }
  ROSE_ASSERT(forInitWrapper->get_startOfConstruct()->get_physical_file_id() ==
              forInitGroup->get_startOfConstruct()->get_physical_file_id());
  ROSE_ASSERT(forInitWrapper->get_startOfConstruct()->get_raw_line() ==
              forInitGroup->get_startOfConstruct()->get_raw_line());
  ROSE_ASSERT(forInitWrapper->get_startOfConstruct()->get_raw_col() ==
              forInitGroup->get_startOfConstruct()->get_raw_col());
  ROSE_ASSERT(
      std::make_pair(forInitWrapper->get_endOfConstruct()->get_raw_line(),
                     forInitWrapper->get_endOfConstruct()->get_raw_col()) <=
      std::make_pair(forInitGroup->get_endOfConstruct()->get_raw_line(),
                     forInitGroup->get_endOfConstruct()->get_raw_col()));
  SgDeclarationGroupStatement *firstRepeatGroup = checkGroup(
      source, declarations,
      {"rex_mixed_repeat_first_object", "rex_mixed_repeat_first_function"},
      false);
  SgDeclarationGroupStatement *secondRepeatGroup = checkGroup(
      source, declarations,
      {"rex_mixed_repeat_second_object", "rex_mixed_repeat_second_function"},
      false);
  ROSE_ASSERT(firstRepeatGroup != secondRepeatGroup);
  // The included function body belongs to a suppressed header surface.  Its
  // local declarations must not leak into the primary translation unit as
  // detached semantic nodes or partially published declaration groups.
  ROSE_ASSERT(declarations.count("rex_mixed_suppressed_header_first") == 0);
  ROSE_ASSERT(declarations.count("rex_mixed_suppressed_header_second") == 0);

  if (const char *corruption =
          std::getenv("REX_TEST_SOURCE_GROUP_CORRUPTION")) {
    SgVariableDeclaration *target = isSgVariableDeclaration(
        declarations.at("rex_mixed_global_object_second"));
    ROSE_ASSERT(target != nullptr);
    ROSE_ASSERT(target->get_variables().size() == 1);
    ROSE_ASSERT(target->get_variables().front() != nullptr);
    if (std::string(corruption) == "base-type") {
      target->get_variables().front()->set_type(SageBuilder::buildFloatType());
    } else if (std::string(corruption) == "storage-modifier") {
      target->get_declarationModifier().get_storageModifier().setStatic();
    } else if (std::string(corruption) == "member-scope") {
      SgBasicBlock *foreignScope = SageBuilder::buildBasicBlock();
      ROSE_ASSERT(foreignScope != nullptr);
      SgDeclarationGroupStatement *group =
          isSgDeclarationGroupStatement(target->get_parent());
      ROSE_ASSERT(group != nullptr);
      ROSE_ASSERT(group->get_parent() != nullptr);
      group->set_scope(foreignScope);
      group->validate();
    } else if (std::string(corruption) == "implicit-scope-update") {
      SgBasicBlock *foreignScope = SageBuilder::buildBasicBlock();
      ROSE_ASSERT(foreignScope != nullptr);
      target->set_scope(foreignScope);
    } else {
      ROSE_ABORT();
    }
  }

  return backend(project);
}
