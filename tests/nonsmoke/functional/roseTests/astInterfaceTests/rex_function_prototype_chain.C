#include "rose.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

SgFunctionDeclaration *findDefinition(SgNode *root, const std::string &name) {
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(node);
    if (function != nullptr && function->get_name().getString() == name &&
        function->get_definition() != nullptr &&
        function->get_definingDeclaration() == function) {
      return function;
    }
  }
  return nullptr;
}

void verifyDefinitionFamily(SgFunctionDeclaration *definition,
                            bool expectAuxiliaryCanonical) {
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(definition->get_definition() != nullptr);
  ROSE_ASSERT(definition->get_definition()->get_declaration() == definition);
  ROSE_ASSERT(definition->get_definingDeclaration() == definition);

  SgFunctionDeclaration *first =
      isSgFunctionDeclaration(definition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(first != nullptr);
  ROSE_ASSERT(first != definition);
  ROSE_ASSERT(first->get_firstNondefiningDeclaration() == first);
  ROSE_ASSERT(first->get_definingDeclaration() == definition);
  ROSE_ASSERT(first->variantT() == definition->variantT());
  ROSE_ASSERT(first->get_scope() == definition->get_scope());

  SgSymbol *symbol = first->get_symbol_from_symbol_table();
  ROSE_ASSERT(symbol != nullptr);
  if (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
    symbol = alias->get_alias();
  }
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(symbol->get_symbol_basis() == first);
  ROSE_ASSERT(first->get_scope()->find_symbol_from_declaration(first) ==
              symbol);

  if (expectAuxiliaryCanonical) {
    SgAuxiliaryDeclarationList *auxiliary =
        isSgAuxiliaryDeclarationList(first->get_parent());
    ROSE_ASSERT(auxiliary != nullptr);
    ROSE_ASSERT(auxiliary->get_parent() == first->get_scope());
    ROSE_ASSERT(first->get_file_info() != nullptr);
    ROSE_ASSERT(first->get_file_info()->isCompilerGenerated());
    ROSE_ASSERT(first->get_file_info()->isFrontendSpecific());
    ROSE_ASSERT(std::count(auxiliary->get_declarations().begin(),
                           auxiliary->get_declarations().end(), first) == 1);
  } else {
    SgScopeStatement *lexicalOwner = isSgScopeStatement(first->get_parent());
    ROSE_ASSERT(lexicalOwner != nullptr);
    ROSE_ASSERT(lexicalOwner == first->get_scope());
    ROSE_ASSERT(lexicalOwner->statementExistsInScope(first));
    ROSE_ASSERT(first->get_file_info() != nullptr);
    ROSE_ASSERT(!first->get_file_info()->isCompilerGenerated());
    ROSE_ASSERT(!first->get_file_info()->isFrontendSpecific());
  }
}

void verifyCompleteDefinitionFamily(SgNode *root,
                                    SgFunctionDeclaration *definition,
                                    size_t expectedFamilySize) {
  ROSE_ASSERT(root != nullptr);
  ROSE_ASSERT(definition != nullptr);
  SgFunctionDeclaration *first =
      isSgFunctionDeclaration(definition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(first != nullptr);

  std::vector<SgFunctionDeclaration *> family;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *candidate = isSgFunctionDeclaration(node);
    if (candidate == nullptr ||
        candidate->get_firstNondefiningDeclaration() != first) {
      continue;
    }

    ROSE_ASSERT(candidate->get_firstNondefiningDeclaration() == first);
    ROSE_ASSERT(candidate->get_definingDeclaration() == definition);
    ROSE_ASSERT(candidate->variantT() == definition->variantT());
    ROSE_ASSERT(candidate->get_scope() == definition->get_scope());
    ROSE_ASSERT(std::find(family.begin(), family.end(), candidate) ==
                family.end());
    family.push_back(candidate);
  }

  ROSE_ASSERT(family.size() == expectedFamilySize);
  ROSE_ASSERT(std::find(family.begin(), family.end(), first) != family.end());
  ROSE_ASSERT(std::find(family.begin(), family.end(), definition) !=
              family.end());
}

void verifySemanticFunctionPhysicalOwner(SgFunctionDeclaration *function,
                                         int expectedPhysicalFileId) {
  ROSE_ASSERT(function != nullptr);
  ROSE_ASSERT(expectedPhysicalFileId >= 0);
  ROSE_ASSERT(function->get_file_info() == function->get_startOfConstruct());
  for (Sg_File_Info *position :
       {function->get_file_info(), function->get_startOfConstruct(),
        function->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->get_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(position->get_physical_file_id() == expectedPhysicalFileId);
    ROSE_ASSERT(SageInterface::hasExactSemanticFrontendSourcePosition(
        function, position));
  }
}

void verifyStandaloneClassCopyAuxiliaryOwnership(
    SgFunctionDeclaration *inlineMemberDefinition) {
  ROSE_ASSERT(inlineMemberDefinition != nullptr);
  SgClassDefinition *originalDefinition =
      isSgClassDefinition(inlineMemberDefinition->get_scope());
  ROSE_ASSERT(originalDefinition != nullptr);
  SgClassDeclaration *originalDeclaration =
      originalDefinition->get_declaration();
  ROSE_ASSERT(originalDeclaration != nullptr);
  ROSE_ASSERT(originalDeclaration->get_definition() == originalDefinition);

  // Outlining and dependency reconstruction copy a class declaration as a
  // detached root before attaching it to the generated translation unit.  Its
  // semantic-only declarations must already name the copied class definition;
  // symbol rebuilding is not allowed to repair pointers back to the original.
  SgClassDeclaration *copiedDeclaration =
      SageInterface::deepCopy(originalDeclaration);
  ROSE_ASSERT(copiedDeclaration != nullptr);
  ROSE_ASSERT(copiedDeclaration != originalDeclaration);
  SgClassDefinition *copiedDefinition = copiedDeclaration->get_definition();
  ROSE_ASSERT(copiedDefinition != nullptr);
  ROSE_ASSERT(copiedDefinition != originalDefinition);
  ROSE_ASSERT(copiedDefinition->get_declaration() == copiedDeclaration);

  SgAuxiliaryDeclarationList *copiedAuxiliary =
      copiedDefinition->get_auxiliary_declarations();
  ROSE_ASSERT(copiedAuxiliary != nullptr);
  ROSE_ASSERT(copiedAuxiliary->get_parent() == copiedDefinition);
  ROSE_ASSERT(!copiedAuxiliary->get_declarations().empty());
  for (SgDeclarationStatement *declaration :
       copiedAuxiliary->get_declarations()) {
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_parent() == copiedAuxiliary);
    ROSE_ASSERT(declaration->get_scope() == copiedDefinition);
  }

  // deepCopy() publishes the copied declarations and rebuilds each copied
  // scope's symbol table before returning.  Rebuilding a nonempty table is a
  // caller error, not a repair operation.  Verify that the copied canonical
  // declaration and symbol are exact instead of invoking that invalid second
  // rebuild.
  SgSymbolTable *copiedSymbolTable = copiedDefinition->get_symbol_table();
  ROSE_ASSERT(copiedSymbolTable != nullptr);
  ROSE_ASSERT(copiedSymbolTable->get_parent() == copiedDefinition);
  SgSymbolTable *originalSymbolTable = originalDefinition->get_symbol_table();
  ROSE_ASSERT(originalSymbolTable != nullptr);
  ROSE_ASSERT(originalSymbolTable->get_parent() == originalDefinition);
  // The copied lexical definitions and their auxiliary canonical declarations
  // are two structural nodes in each family, but define exactly one callable
  // symbol.  Comparing cardinality exposes duplicate family publication
  // directly instead of relying on a later reference-fixup mismatch.
  ROSE_ASSERT(copiedSymbolTable->size() == originalSymbolTable->size());

  SgFunctionDeclaration *copiedInlineDefinition =
      findDefinition(copiedDefinition, "rex_inline_member_definition");
  ROSE_ASSERT(copiedInlineDefinition != nullptr);
  ROSE_ASSERT(copiedInlineDefinition != inlineMemberDefinition);
  verifyDefinitionFamily(copiedInlineDefinition, true);
  verifyCompleteDefinitionFamily(copiedDeclaration, copiedInlineDefinition, 2);

  SgFunctionDeclaration *originalFirst = isSgFunctionDeclaration(
      inlineMemberDefinition->get_firstNondefiningDeclaration());
  SgFunctionDeclaration *copiedFirst = isSgFunctionDeclaration(
      copiedInlineDefinition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(originalFirst != nullptr);
  ROSE_ASSERT(copiedFirst != nullptr);
  ROSE_ASSERT(copiedFirst != originalFirst);
  ROSE_ASSERT(copiedFirst->get_scope() == copiedDefinition);
  ROSE_ASSERT(copiedFirst->get_symbol_from_symbol_table() !=
              originalFirst->get_symbol_from_symbol_table());

  SageInterface::deepDelete(copiedDeclaration);
}

void verifyReplacement(SgFunctionDeclaration *definition) {
  ROSE_ASSERT(definition != nullptr);
  SgScopeStatement *semanticScope = definition->get_scope();
  SgScopeStatement *structuralOwner =
      isSgScopeStatement(definition->get_parent());
  ROSE_ASSERT(semanticScope != nullptr);
  ROSE_ASSERT(structuralOwner != nullptr);
  SgFunctionDeclaration *originalFirst =
      isSgFunctionDeclaration(definition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(originalFirst != nullptr);
  ROSE_ASSERT(originalFirst != definition);
  const bool isOutOfClassMember =
      isSgMemberFunctionDeclaration(definition) != nullptr &&
      structuralOwner != semanticScope;
  const int physicalFileId =
      definition->get_file_info()->get_physical_file_id();
  ROSE_ASSERT(physicalFileId >= 0);

  std::vector<SgFunctionDeclaration *> originalFamily;
  SgProject *project = SageInterface::getProject(definition);
  ROSE_ASSERT(project != nullptr);
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *candidate = isSgFunctionDeclaration(node);
    if (candidate != nullptr &&
        candidate->get_firstNondefiningDeclaration() == originalFirst) {
      ROSE_ASSERT(candidate->get_definingDeclaration() == definition);
      originalFamily.push_back(candidate);
    }
  }
  ROSE_ASSERT(originalFamily.size() >= 2);

  PreprocessingInfo *ownedComment = SageInterface::attachComment(
      definition, "REX function definition replacement ownership",
      PreprocessingInfo::C_StyleComment);
  ROSE_ASSERT(ownedComment != nullptr);
  std::vector<PreprocessingInfo *> originalPreprocessing;
  if (AttachedPreprocessingInfoType *infos =
          definition->getAttachedPreprocessingInfo()) {
    originalPreprocessing.assign(infos->begin(), infos->end());
  }
  ROSE_ASSERT(std::find(originalPreprocessing.begin(),
                        originalPreprocessing.end(),
                        ownedComment) != originalPreprocessing.end());

  SgDeclarationStatement *sourceReplacement =
      SageInterface::replaceFunctionDefinitionWithDeclaration(definition);
  ROSE_ASSERT(sourceReplacement != nullptr);
  ROSE_ASSERT(sourceReplacement->get_parent() == structuralOwner);
  ROSE_ASSERT(structuralOwner->statementExistsInScope(sourceReplacement));
  ROSE_ASSERT(sourceReplacement->get_file_info() != nullptr);
  ROSE_ASSERT(sourceReplacement->get_startOfConstruct() != nullptr);
  ROSE_ASSERT(sourceReplacement->get_endOfConstruct() != nullptr);
  ROSE_ASSERT(sourceReplacement->get_file_info()->get_physical_file_id() ==
              physicalFileId);
  ROSE_ASSERT(
      sourceReplacement->get_startOfConstruct()->get_physical_file_id() ==
      physicalFileId);
  ROSE_ASSERT(sourceReplacement->get_endOfConstruct()->get_physical_file_id() ==
              physicalFileId);
  for (PreprocessingInfo *info : originalPreprocessing) {
    ROSE_ASSERT(info != nullptr);
    AttachedPreprocessingInfoType *replacementInfos =
        sourceReplacement->getAttachedPreprocessingInfo();
    ROSE_ASSERT(replacementInfos != nullptr);
    ROSE_ASSERT(std::find(replacementInfos->begin(), replacementInfos->end(),
                          info) != replacementInfos->end());
  }

  ROSE_ASSERT(originalFirst->get_firstNondefiningDeclaration() ==
              originalFirst);
  ROSE_ASSERT(originalFirst->get_definingDeclaration() == definition);
  ROSE_ASSERT(definition->get_firstNondefiningDeclaration() == originalFirst);
  ROSE_ASSERT(definition->get_definingDeclaration() == definition);
  for (SgFunctionDeclaration *declaration : originalFamily) {
    ROSE_ASSERT(declaration->get_firstNondefiningDeclaration() ==
                originalFirst);
    ROSE_ASSERT(declaration->get_definingDeclaration() == definition);
  }

  SgAuxiliaryDeclarationList *auxiliary =
      isSgAuxiliaryDeclarationList(definition->get_parent());
  ROSE_ASSERT(auxiliary != nullptr);
  ROSE_ASSERT(auxiliary->get_parent() == semanticScope);
  ROSE_ASSERT(std::find(auxiliary->get_declarations().begin(),
                        auxiliary->get_declarations().end(),
                        definition) != auxiliary->get_declarations().end());

  if (isOutOfClassMember) {
    SgEmptyDeclaration *empty = isSgEmptyDeclaration(sourceReplacement);
    ROSE_ASSERT(empty != nullptr);
    empty->validate_lexical_role();
    ROSE_ASSERT(
        empty->get_lexical_role() ==
        SgEmptyDeclaration::e_empty_declaration_zero_width_source_replacement);
    ROSE_ASSERT(empty->get_scope() == structuralOwner);
    ROSE_ASSERT(empty->get_firstNondefiningDeclaration() == empty);
    ROSE_ASSERT(empty->get_definingDeclaration() == empty);
    for (Sg_File_Info *position :
         {empty->get_file_info(), empty->get_startOfConstruct(),
          empty->get_endOfConstruct()}) {
      ROSE_ASSERT(position != nullptr);
      ROSE_ASSERT(position->get_parent() == empty);
      ROSE_ASSERT(!position->isShared());
      ROSE_ASSERT(position->isTransformation());
      ROSE_ASSERT(!position->isCompilerGenerated());
      ROSE_ASSERT(!position->isFrontendSpecific());
      ROSE_ASSERT(position->isOutputInCodeGeneration());
      ROSE_ASSERT(position->get_physical_file_id() == physicalFileId);
    }
    ROSE_ASSERT(originalFirst->get_parent() == semanticScope);
  } else {
    SgFunctionDeclaration *prototype =
        isSgFunctionDeclaration(sourceReplacement);
    ROSE_ASSERT(prototype != nullptr);
    ROSE_ASSERT(prototype->get_scope() == semanticScope);
    ROSE_ASSERT(prototype->get_forward());
    ROSE_ASSERT(prototype->get_firstNondefiningDeclaration() == originalFirst);
    ROSE_ASSERT(prototype->get_definingDeclaration() == definition);
  }

  SgFunctionSymbol *symbol =
      isSgFunctionSymbol(originalFirst->get_symbol_from_symbol_table());
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(symbol->get_declaration() == originalFirst);
  ROSE_ASSERT(semanticScope->find_symbol_from_declaration(originalFirst) ==
              symbol);
}

} // namespace

int main(int argc, char **argv) {
  bool rejectOutOfClassPrototype = false;
  bool rejectNonemptySymbolRebuild = false;
  bool requireCopiedSemanticFunctionIdentity = false;
  enum class FunctionChainContractMode {
    none,
    requireComplete,
    rejectMissingScope,
    rejectMissingOwner,
    rejectWrongScope,
    rejectWrongOwner,
    rejectBrokenReciprocal
  };
  FunctionChainContractMode functionChainContractMode =
      FunctionChainContractMode::none;
  int writeIndex = 1;
  for (int readIndex = 1; readIndex < argc; ++readIndex) {
    const std::string argument = argv[readIndex];
    if (argument == "--reject-out-of-class-member-prototype") {
      rejectOutOfClassPrototype = true;
    } else if (argument == "--reject-nonempty-symbol-rebuild") {
      rejectNonemptySymbolRebuild = true;
    } else if (argument == "--require-copied-semantic-function-identity") {
      requireCopiedSemanticFunctionIdentity = true;
    } else if (argument == "--require-complete-function-chain") {
      functionChainContractMode = FunctionChainContractMode::requireComplete;
    } else if (argument == "--reject-function-chain-missing-scope") {
      functionChainContractMode = FunctionChainContractMode::rejectMissingScope;
    } else if (argument == "--reject-function-chain-missing-owner") {
      functionChainContractMode = FunctionChainContractMode::rejectMissingOwner;
    } else if (argument == "--reject-function-chain-wrong-scope") {
      functionChainContractMode = FunctionChainContractMode::rejectWrongScope;
    } else if (argument == "--reject-function-chain-wrong-owner") {
      functionChainContractMode = FunctionChainContractMode::rejectWrongOwner;
    } else if (argument == "--reject-function-chain-broken-reciprocal") {
      functionChainContractMode =
          FunctionChainContractMode::rejectBrokenReciprocal;
    } else {
      argv[writeIndex++] = argv[readIndex];
    }
  }
  argc = writeIndex;
  argv[argc] = nullptr;
  const int selectedContractModes =
      (rejectOutOfClassPrototype ? 1 : 0) +
      (rejectNonemptySymbolRebuild ? 1 : 0) +
      (requireCopiedSemanticFunctionIdentity ? 1 : 0) +
      (functionChainContractMode != FunctionChainContractMode::none ? 1 : 0);
  ROSE_ASSERT(selectedContractModes <= 1);

  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);

  SgFunctionDeclaration *freeDefinition =
      findDefinition(project, "rex_free_definition");
  SgFunctionDeclaration *inlineMemberDefinition =
      findDefinition(project, "rex_inline_member_definition");
  SgFunctionDeclaration *outOfLineMemberDefinition =
      findDefinition(project, "rex_out_of_line_member_definition");
  ROSE_ASSERT(freeDefinition != nullptr);
  ROSE_ASSERT(inlineMemberDefinition != nullptr);
  ROSE_ASSERT(outOfLineMemberDefinition != nullptr);

  if (requireCopiedSemanticFunctionIdentity) {
    SgFunctionDeclaration *canonical = isSgFunctionDeclaration(
        inlineMemberDefinition->get_firstNondefiningDeclaration());
    SgSourceFile *source =
        SageInterface::getEnclosingSourceFile(inlineMemberDefinition, true);
    ROSE_ASSERT(canonical != nullptr);
    ROSE_ASSERT(source != nullptr);
    ROSE_ASSERT(isSgAuxiliaryDeclarationList(canonical->get_parent()) !=
                nullptr);

    const int originalPhysicalFileId =
        source->get_file_info()->get_physical_file_id();
    verifySemanticFunctionPhysicalOwner(canonical, originalPhysicalFileId);

    SageBuilder::rebindCopiedSourceFilePhysicalIdentity(
        source, "rex_copied_semantic_function_identity_first.C");
    const int firstOutputPhysicalFileId =
        source->get_file_info()->get_physical_file_id();
    ROSE_ASSERT(firstOutputPhysicalFileId != originalPhysicalFileId);
    verifySemanticFunctionPhysicalOwner(canonical, firstOutputPhysicalFileId);

    SageBuilder::rebindCopiedSourceFilePhysicalIdentity(
        source, "rex_copied_semantic_function_identity_second.C");
    const int secondOutputPhysicalFileId =
        source->get_file_info()->get_physical_file_id();
    ROSE_ASSERT(secondOutputPhysicalFileId != originalPhysicalFileId);
    ROSE_ASSERT(secondOutputPhysicalFileId != firstOutputPhysicalFileId);
    verifySemanticFunctionPhysicalOwner(canonical, secondOutputPhysicalFileId);
    return 0;
  }

  if (functionChainContractMode != FunctionChainContractMode::none) {
    SgFunctionDeclaration *first = isSgFunctionDeclaration(
        freeDefinition->get_firstNondefiningDeclaration());
    ROSE_ASSERT(first != nullptr);
    ROSE_ASSERT(first != freeDefinition);
    SgScopeStatement *semanticScope = first->get_scope();
    SgNode *structuralOwner = first->get_parent();
    ROSE_ASSERT(semanticScope != nullptr);
    ROSE_ASSERT(structuralOwner != nullptr);

    if (functionChainContractMode ==
        FunctionChainContractMode::rejectMissingScope) {
      first->set_scope(nullptr);
      first->set_parent(nullptr);
    } else if (functionChainContractMode ==
               FunctionChainContractMode::rejectMissingOwner) {
      first->set_parent(nullptr);
    } else if (functionChainContractMode ==
               FunctionChainContractMode::rejectWrongScope) {
      ROSE_ASSERT(inlineMemberDefinition->get_scope() != semanticScope);
      first->set_scope(inlineMemberDefinition->get_scope());
    } else if (functionChainContractMode ==
               FunctionChainContractMode::rejectWrongOwner) {
      ROSE_ASSERT(freeDefinition->get_parameterList() != nullptr);
      first->set_parent(freeDefinition->get_parameterList());
    } else if (functionChainContractMode ==
               FunctionChainContractMode::rejectBrokenReciprocal) {
      first->set_definingDeclaration(first);
    }

    SageInterface::fixFunctionDeclaration(freeDefinition,
                                          freeDefinition->get_scope());
    if (functionChainContractMode ==
        FunctionChainContractMode::requireComplete) {
      SageInterface::fixFunctionDeclaration(first, first->get_scope());
      ROSE_ASSERT(first->get_scope() == semanticScope);
      ROSE_ASSERT(first->get_parent() == structuralOwner);
      ROSE_ASSERT(freeDefinition->get_firstNondefiningDeclaration() == first);
      ROSE_ASSERT(first->get_definingDeclaration() == freeDefinition);
      return 0;
    }

    ROSE_ABORT();
  }

  if (rejectOutOfClassPrototype) {
    // This API is deliberately narrower than the source-surface replacement
    // API.  A qualified member prototype at namespace scope is invalid C++ and
    // must be rejected at construction time.
    SageInterface::buildFunctionPrototype(outOfLineMemberDefinition);
    ROSE_ABORT();
  }

  if (rejectNonemptySymbolRebuild) {
    SgClassDefinition *originalDefinition =
        isSgClassDefinition(inlineMemberDefinition->get_scope());
    ROSE_ASSERT(originalDefinition != nullptr);
    SgClassDeclaration *copiedDeclaration =
        SageInterface::deepCopy(originalDefinition->get_declaration());
    ROSE_ASSERT(copiedDeclaration != nullptr);
    SgClassDefinition *copiedDefinition = copiedDeclaration->get_definition();
    ROSE_ASSERT(copiedDefinition != nullptr);
    ROSE_ASSERT(copiedDefinition->get_symbol_table() != nullptr);
    ROSE_ASSERT(copiedDefinition->get_symbol_table()->size() > 0);

    // rebuildSymbolTable() is construction-only: callers must clear an
    // existing table explicitly before requesting a rebuild.  A deep copy has
    // already completed that construction phase and must be rejected here.
    SageInterface::rebuildSymbolTable(copiedDefinition);
    ROSE_ABORT();
  }

  // The first free-function prototype is source-written and must remain on the
  // global source surface.  Only an implicit canonical declaration belongs in
  // the auxiliary container.
  verifyDefinitionFamily(freeDefinition, false);
  verifyDefinitionFamily(inlineMemberDefinition, true);
  verifyDefinitionFamily(outOfLineMemberDefinition, false);
  verifyCompleteDefinitionFamily(project, freeDefinition, 3);
  verifyCompleteDefinitionFamily(project, inlineMemberDefinition, 2);
  verifyCompleteDefinitionFamily(project, outOfLineMemberDefinition, 2);
  verifyStandaloneClassCopyAuxiliaryOwnership(inlineMemberDefinition);

  // Copy the complete project so every copied global remains structurally
  // owned by a project while its declaration symbols are rebuilt.  A detached
  // SgGlobal is malformed: it has no project-level ownership and must not be
  // used as a positive symbol-table fixture.
  SgProject *generatedProject = SageInterface::deepCopy(project);
  ROSE_ASSERT(generatedProject != nullptr);
  ROSE_ASSERT(generatedProject != project);
  SgGlobal *generatedGlobal =
      SageInterface::getFirstGlobalScope(generatedProject);
  ROSE_ASSERT(generatedGlobal != nullptr);
  SgFunctionDeclaration *copiedFreeDefinition =
      findDefinition(generatedGlobal, "rex_free_definition");
  SgFunctionDeclaration *copiedInlineMemberDefinition =
      findDefinition(generatedGlobal, "rex_inline_member_definition");
  SgFunctionDeclaration *copiedOutOfLineMemberDefinition =
      findDefinition(generatedGlobal, "rex_out_of_line_member_definition");
  verifyDefinitionFamily(copiedFreeDefinition, false);
  verifyDefinitionFamily(copiedInlineMemberDefinition, true);
  verifyDefinitionFamily(copiedOutOfLineMemberDefinition, false);
  verifyCompleteDefinitionFamily(generatedProject, copiedFreeDefinition, 3);
  verifyCompleteDefinitionFamily(generatedProject, copiedInlineMemberDefinition,
                                 2);
  verifyCompleteDefinitionFamily(generatedProject,
                                 copiedOutOfLineMemberDefinition, 2);

  SgFunctionDeclaration *originalReopenedDefinition =
      findDefinition(project, "rex_copy_reopened_definition");
  SgFunctionDeclaration *copiedReopenedDefinition =
      findDefinition(generatedProject, "rex_copy_reopened_definition");
  ROSE_ASSERT(originalReopenedDefinition != nullptr);
  ROSE_ASSERT(copiedReopenedDefinition != nullptr);
  ROSE_ASSERT(copiedReopenedDefinition != originalReopenedDefinition);
  SgFunctionDeclaration *originalReopenedFirst = isSgFunctionDeclaration(
      originalReopenedDefinition->get_firstNondefiningDeclaration());
  SgFunctionDeclaration *copiedReopenedFirst = isSgFunctionDeclaration(
      copiedReopenedDefinition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(originalReopenedFirst != nullptr);
  ROSE_ASSERT(copiedReopenedFirst != nullptr);
  ROSE_ASSERT(copiedReopenedFirst != originalReopenedFirst);
  SgNamespaceDefinitionStatement *originalFirstNamespace =
      isSgNamespaceDefinitionStatement(originalReopenedFirst->get_scope());
  SgNamespaceDefinitionStatement *originalDefiningNamespace =
      isSgNamespaceDefinitionStatement(originalReopenedDefinition->get_scope());
  SgNamespaceDefinitionStatement *copiedFirstNamespace =
      isSgNamespaceDefinitionStatement(copiedReopenedFirst->get_scope());
  SgNamespaceDefinitionStatement *copiedDefiningNamespace =
      isSgNamespaceDefinitionStatement(copiedReopenedDefinition->get_scope());
  ROSE_ASSERT(originalFirstNamespace != nullptr);
  ROSE_ASSERT(originalDefiningNamespace != nullptr);
  ROSE_ASSERT(copiedFirstNamespace != nullptr);
  ROSE_ASSERT(copiedDefiningNamespace != nullptr);
  ROSE_ASSERT(
      originalFirstNamespace->isSameNamespace(originalDefiningNamespace));
  ROSE_ASSERT(copiedFirstNamespace->isSameNamespace(copiedDefiningNamespace));

  SgVariableDeclaration *copiedLoopLocal = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(copiedReopenedDefinition,
                                              V_SgVariableDeclaration)) {
    SgVariableDeclaration *variable = isSgVariableDeclaration(node);
    if (variable != nullptr && variable->get_variables().size() == 1 &&
        variable->get_variables().front()->get_name() ==
            "rex_copy_loop_local") {
      ROSE_ASSERT(copiedLoopLocal == nullptr);
      copiedLoopLocal = variable;
    }
  }
  ROSE_ASSERT(copiedLoopLocal != nullptr);
  ROSE_ASSERT(copiedLoopLocal->get_definingDeclaration() == nullptr);
  ROSE_ASSERT(copiedLoopLocal->get_firstNondefiningDeclaration() ==
              copiedLoopLocal);
  ROSE_ASSERT(isSgNamespaceDefinitionStatement(copiedLoopLocal->get_scope()) ==
              nullptr);
  SageInterface::deepDelete(generatedProject);

  verifyReplacement(freeDefinition);
  verifyReplacement(inlineMemberDefinition);

  // Empty source replacements must not inherit an unrelated active builder
  // scope.  Keep the class scope active while replacing the
  // namespace-owned out-of-class definition; the replacement transaction must
  // still receive only its exact structural source owner.
  SgScopeStatement *const priorBuilderScope = SageBuilder::topScopeStack();
  SgScopeStatement *const unrelatedActiveScope =
      inlineMemberDefinition->get_scope();
  ROSE_ASSERT(unrelatedActiveScope != nullptr);
  ROSE_ASSERT(unrelatedActiveScope !=
              isSgScopeStatement(outOfLineMemberDefinition->get_parent()));
  SageBuilder::pushScopeStack(unrelatedActiveScope);
  ROSE_ASSERT(SageBuilder::topScopeStack() == unrelatedActiveScope);
  verifyReplacement(outOfLineMemberDefinition);
  ROSE_ASSERT(SageBuilder::topScopeStack() == unrelatedActiveScope);
  SageBuilder::popScopeStack();
  ROSE_ASSERT(SageBuilder::topScopeStack() == priorBuilderScope);

  AstTests::runAllTests(project);
  return backend(project);
}
