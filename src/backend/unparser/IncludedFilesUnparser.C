#include "sage3basic.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <sstream>
#include <tuple>

#include "CollectionHelper.h"
#include "astJson/sageAstJson.h"

#include "FileHelper.h"

#include "IncludingPreprocessingInfosCollector.h"

#include "IncludeDirective.h"

#include "IncludedFilesUnparser.h"

#include "rose_test_output_path.h"

#include "unparser.h"

using namespace std;

namespace {
SgSourceFile *requireHeaderPlanningSource(SgProject *project,
                                          const char *contract) {
  ASSERT_not_null(project);
  ASSERT_not_null(contract);
  for (SgFile *projectFile : project->get_fileList()) {
    SgSourceFile *sourceFile = isSgSourceFile(projectFile);
    if (sourceFile != nullptr && !sourceFile->get_skip_unparse() &&
        sourceFile->get_unparseHeaderFiles()) {
      return sourceFile;
    }
  }
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[%s]: project=%p has no enabled header "
          "planning translation unit\n",
          contract, static_cast<void *>(project));
  ROSE_ABORT();
}

bool isExactPrimaryPhysicalFile(const SgProject *project, int physicalFileId,
                                const string &normalizedFileName,
                                const char *contract) {
  ASSERT_not_null(project);
  ASSERT_not_null(contract);
  if (physicalFileId < 0 || normalizedFileName.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: physical=%d file=%s cannot identify "
            "a primary translation unit\n",
            contract, physicalFileId, normalizedFileName.c_str());
    ROSE_ABORT();
  }

  bool found = false;
  for (SgFile *projectFile : project->get_fileList()) {
    SgSourceFile *sourceFile = isSgSourceFile(projectFile);
    if (sourceFile == nullptr || sourceFile->get_skip_unparse() ||
        sourceFile->get_isHeaderFile()) {
      continue;
    }
    Sg_File_Info *sourceInfo = sourceFile->get_file_info();
    if (sourceInfo == nullptr || sourceInfo->isShared() ||
        sourceInfo->get_physical_file_id() < 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: primary source=%p has no exact "
              "physical identity\n",
              contract, static_cast<void *>(sourceFile));
      ROSE_ABORT();
    }
    if (sourceInfo->get_physical_file_id() != physicalFileId) {
      continue;
    }
    const string sourcePhysicalFile = FileHelper::normalizePathIfPossible(
        sourceInfo->get_physical_filename());
    if (sourcePhysicalFile != normalizedFileName) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: physical=%d maps to both %s and "
              "%s\n",
              contract, physicalFileId, normalizedFileName.c_str(),
              sourcePhysicalFile.c_str());
      ROSE_ABORT();
    }
    found = true;
  }
  return found;
}

bool isExactlyAuxiliaryOwned(SgNode *node, const char *contract) {
  ASSERT_not_null(node);
  ASSERT_not_null(contract);

  if (SgAuxiliaryDeclarationList *container =
          isSgAuxiliaryDeclarationList(node)) {
    SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
    if (owner == nullptr || owner->get_auxiliary_declarations() != container) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has malformed "
              "auxiliary declaration-list ownership\n",
              contract, static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    return true;
  }

  if (SgDeclarationScopeList *container = isSgDeclarationScopeList(node)) {
    SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
    if (owner == nullptr ||
        owner->get_auxiliary_declaration_scopes() != container) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has malformed "
              "auxiliary declaration-scope-list ownership\n",
              contract, static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    return true;
  }

  if (SgDeclarationScope *declarationScope = isSgDeclarationScope(node)) {
    SgNode *owner = declarationScope->get_parent();
    if (SgDeclarationScopeList *container = isSgDeclarationScopeList(owner)) {
      SgScopeStatement *scopeOwner =
          isSgScopeStatement(container->get_parent());
      const SgDeclarationScopePtrList &scopes = container->get_scopes();
      if (scopeOwner == nullptr ||
          scopeOwner->get_auxiliary_declaration_scopes() != container ||
          count(scopes.begin(), scopes.end(), declarationScope) != 1) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has malformed "
                "auxiliary declaration-scope ownership\n",
                contract, static_cast<void *>(node),
                node->class_name().c_str());
        ROSE_ABORT();
      }
      return true;
    }

    SgDeclarationStatement *declarationOwner = isSgDeclarationStatement(owner);
    SgFunctionDeclaration *functionOwner =
        isSgFunctionDeclaration(declarationOwner);
    if (declarationOwner == nullptr ||
        (declarationOwner->get_nonreal_decl_scope() != declarationScope &&
         declarationOwner->get_declarationScope() != declarationScope &&
         declarationOwner->get_source_declarator_scope() != declarationScope &&
         (functionOwner == nullptr ||
          functionOwner->get_function_declarator_scope() !=
              declarationScope))) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has no exact typed "
              "declaration owner\n",
              contract, static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    return declarationOwner->get_nonreal_decl_scope() == declarationScope;
  }

  set<SgNode *> visited;
  SgNode *child = node;
  for (SgNode *parent = node->get_parent(); parent != nullptr;
       child = parent, parent = parent->get_parent()) {
    if (!visited.insert(parent).second) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has a parent "
              "cycle\n",
              contract, static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }

    if (SgDeclarationScope *declarationScope = isSgDeclarationScope(parent)) {
      SgNode *scopeParent = declarationScope->get_parent();
      const SgNodePtrList scopeSuccessors =
          declarationScope->get_traversalSuccessorContainer();
      if (child == nullptr || child->get_parent() != declarationScope ||
          count(scopeSuccessors.begin(), scopeSuccessors.end(), child) != 1) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has malformed "
                "declaration-scope child ownership\n",
                contract, static_cast<void *>(node),
                node->class_name().c_str());
        ROSE_ABORT();
      }

      if (SgDeclarationScopeList *container =
              isSgDeclarationScopeList(scopeParent)) {
        SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
        const SgDeclarationScopePtrList &scopes = container->get_scopes();
        if (owner == nullptr ||
            owner->get_auxiliary_declaration_scopes() != container ||
            count(scopes.begin(), scopes.end(), declarationScope) != 1) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has malformed "
                  "auxiliary declaration-scope ownership\n",
                  contract, static_cast<void *>(node),
                  node->class_name().c_str());
          ROSE_ABORT();
        }
        return true;
      }

      if (SgDeclarationStatement *declarationOwner =
              isSgDeclarationStatement(scopeParent)) {
        SgFunctionDeclaration *functionOwner =
            isSgFunctionDeclaration(declarationOwner);
        if (declarationOwner->get_nonreal_decl_scope() != declarationScope &&
            declarationOwner->get_declarationScope() != declarationScope &&
            declarationOwner->get_source_declarator_scope() !=
                declarationScope &&
            (functionOwner == nullptr ||
             functionOwner->get_function_declarator_scope() !=
                 declarationScope)) {
          SgDeclarationStatement *nestedDeclaration =
              isSgDeclarationStatement(node);
          Sg_File_Info *nestedSource = nestedDeclaration != nullptr
                                           ? nestedDeclaration->get_file_info()
                                           : nullptr;
          fprintf(
              stderr,
              "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s name=%s "
              "source=%s:%d has direct scope=%p owned by %p/%s name=%s "
              "declaration-scope=%p source-declarator-scope=%p "
              "nonreal-scope=%p function-declarator-scope=%p without one "
              "exact typed owner\n",
              contract, static_cast<void *>(node), node->class_name().c_str(),
              nestedDeclaration != nullptr
                  ? SageInterface::get_name(nestedDeclaration).c_str()
                  : "<unnamed>",
              nestedSource != nullptr
                  ? nestedSource->get_filenameString().c_str()
                  : "<null>",
              nestedSource != nullptr ? nestedSource->get_line() : -1,
              static_cast<void *>(declarationScope),
              static_cast<void *>(declarationOwner),
              declarationOwner->class_name().c_str(),
              SageInterface::get_name(declarationOwner).c_str(),
              static_cast<void *>(declarationOwner->get_declarationScope()),
              static_cast<void *>(
                  declarationOwner->get_source_declarator_scope()),
              static_cast<void *>(declarationOwner->get_nonreal_decl_scope()),
              static_cast<void *>(
                  functionOwner != nullptr
                      ? functionOwner->get_function_declarator_scope()
                      : nullptr));
          ROSE_ABORT();
        }
        // Every declaration owned below a typed declaration scope is semantic
        // infrastructure.  The source declarator, function declarator, and
        // nonreal roles differ in how lookup is modeled, but their lexical
        // spelling belongs to declarationOwner rather than to descendants such
        // as SgNonrealDecl.  After validating the exact typed edge above, keep
        // the entire semantic subtree out of physical header planning.
        return true;
      }
    }

    if (SgAuxiliaryDeclarationList *container =
            isSgAuxiliaryDeclarationList(parent)) {
      SgDeclarationStatement *declaration = isSgDeclarationStatement(child);
      SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
      const SgDeclarationStatementPtrList &declarations =
          container->get_declarations();
      if (declaration == nullptr || owner == nullptr ||
          owner->get_auxiliary_declarations() != container ||
          count(declarations.begin(), declarations.end(), declaration) != 1) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has malformed "
                "auxiliary declaration ownership\n",
                contract, static_cast<void *>(node),
                node->class_name().c_str());
        ROSE_ABORT();
      }
      return true;
    }

    if (SgDeclarationScopeList *container = isSgDeclarationScopeList(parent)) {
      SgDeclarationScope *declarationScope = isSgDeclarationScope(child);
      SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
      const SgDeclarationScopePtrList &scopes = container->get_scopes();
      if (declarationScope == nullptr || owner == nullptr ||
          owner->get_auxiliary_declaration_scopes() != container ||
          count(scopes.begin(), scopes.end(), declarationScope) != 1) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has malformed "
                "auxiliary declaration-scope ownership\n",
                contract, static_cast<void *>(node),
                node->class_name().c_str());
        ROSE_ABORT();
      }
      return true;
    }
  }
  return false;
}

bool isExactlyRangeForSemanticDeclarationPayload(SgStatement *statement) {
  ASSERT_not_null(statement);

  set<SgNode *> visited;
  for (SgNode *cursor = statement; cursor != nullptr;) {
    if (!visited.insert(cursor).second) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[range-for-semantic-payload]: "
              "statement=%p/%s has a parent cycle\n",
              static_cast<void *>(statement), statement->class_name().c_str());
      ROSE_ABORT();
    }

    if (SgVariableDeclaration *declaration = isSgVariableDeclaration(cursor)) {
      if (SgRangeBasedForStatement *owner =
              isSgRangeBasedForStatement(declaration->get_parent())) {
        const unsigned semanticEdgeCount =
            (owner->get_range_declaration() == declaration ? 1U : 0U) +
            (owner->get_begin_declaration() == declaration ? 1U : 0U) +
            (owner->get_end_declaration() == declaration ? 1U : 0U);
        if (semanticEdgeCount == 0U &&
            owner->get_iterator_declaration() == declaration) {
          return false;
        }
        const SgNodePtrList ownerSuccessors =
            owner->get_traversalSuccessorContainer();
        if (semanticEdgeCount != 1U || declaration->get_scope() != owner ||
            count(ownerSuccessors.begin(), ownerSuccessors.end(),
                  declaration) != 1) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[range-for-semantic-owner]: "
                  "statement=%p/%s has no single exact range/begin/end "
                  "declaration owner\n",
                  static_cast<void *>(statement),
                  statement->class_name().c_str());
          ROSE_ABORT();
        }

        SgLocatedNode *located[] = {declaration, statement};
        for (SgLocatedNode *node : located) {
          const Sg_File_Info *positions[] = {node->get_file_info(),
                                             node->get_startOfConstruct(),
                                             node->get_endOfConstruct()};
          for (const Sg_File_Info *position : positions) {
            if (position == nullptr || position->get_parent() != node ||
                position->isShared() || !position->isCompilerGenerated() ||
                !position->isFrontendSpecific() ||
                position->isTransformation() ||
                position->isSourcePositionUnavailableInFrontend() ||
                !position->isOutputInCodeGeneration() ||
                position->get_file_id() !=
                    Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
                position->get_physical_file_id() !=
                    Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
              fprintf(stderr,
                      "REX_UNPARSE_INVARIANT[range-for-semantic-provenance]: "
                      "node=%p/%s has contradictory source provenance\n",
                      static_cast<void *>(node), node->class_name().c_str());
              ROSE_ABORT();
            }
          }
        }

        const AttachedPreprocessingInfoType *preprocessing =
            declaration->get_attachedPreprocessingInfoPtr();
        if (preprocessing != nullptr && !preprocessing->empty()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[range-for-semantic-preprocessing]: "
                  "declaration=%p owns lexical preprocessing syntax\n",
                  static_cast<void *>(declaration));
          ROSE_ABORT();
        }
        return true;
      }
    }

    // SgGlobal is the typed root of the language AST.  Its SgSourceFile
    // parent is project metadata rather than a statement/scope traversal edge,
    // so reaching it proves that this statement is not nested in a range-for
    // semantic payload.
    if (isSgGlobal(cursor) != nullptr) {
      return false;
    }

    SgNode *parent = cursor->get_parent();
    if (parent == nullptr) {
      break;
    }
    const SgNodePtrList parentSuccessors =
        parent->get_traversalSuccessorContainer();
    if (count(parentSuccessors.begin(), parentSuccessors.end(), cursor) != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[range-for-semantic-edge]: "
              "statement=%p/%s has no exact structural parent edge\n",
              static_cast<void *>(statement), statement->class_name().c_str());
      ROSE_ABORT();
    }
    if (isSgRangeBasedForStatement(parent) != nullptr) {
      return false;
    }
    cursor = parent;
  }
  return false;
}

bool isExactlyCatchSequenceStructuralNode(SgStatement *statement) {
  SgCatchStatementSeq *sequence = isSgCatchStatementSeq(statement);
  if (sequence == nullptr) {
    return false;
  }

  SgTryStmt *owner = isSgTryStmt(sequence->get_parent());
  const SgNodePtrList ownerSuccessors =
      owner != nullptr ? owner->get_traversalSuccessorContainer()
                       : SgNodePtrList{};
  const SgNodePtrList sequenceSuccessors =
      sequence->get_traversalSuccessorContainer();
  const SgStatementPtrList &handlers = sequence->get_catch_statement_seq();
  if (owner == nullptr || owner->get_catch_statement_seq_root() != sequence ||
      count(ownerSuccessors.begin(), ownerSuccessors.end(), sequence) != 1 ||
      handlers.empty() || sequenceSuccessors.size() != handlers.size()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[catch-sequence-owner]: sequence=%p has no "
            "exact nonempty typed owner and handler edges\n",
            static_cast<void *>(sequence));
    ROSE_ABORT();
  }
  for (size_t index = 0; index < handlers.size(); ++index) {
    if (isSgCatchOptionStmt(handlers[index]) == nullptr ||
        handlers[index]->get_parent() != sequence ||
        sequenceSuccessors[index] != handlers[index]) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[catch-sequence-handler]: sequence=%p "
              "has malformed handler index=%zu\n",
              static_cast<void *>(sequence), index);
      ROSE_ABORT();
    }
  }

  const Sg_File_Info *ownerPosition = owner->get_file_info();
  if (ownerPosition == nullptr || ownerPosition->get_parent() != owner ||
      ownerPosition->get_physical_file_id() < 0) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[catch-sequence-source-owner]: sequence=%p "
            "has no exact lexical try-statement owner\n",
            static_cast<void *>(sequence));
    ROSE_ABORT();
  }
  const int ownerPhysicalFileId = ownerPosition->get_physical_file_id();
  const Sg_File_Info *positions[] = {sequence->get_file_info(),
                                     sequence->get_startOfConstruct(),
                                     sequence->get_endOfConstruct()};
  for (const Sg_File_Info *position : positions) {
    const bool hasPreassignmentPhysicalIdentity =
        position != nullptr && position->get_physical_file_id() ==
                                   Sg_File_Info::COMPILER_GENERATED_FILE_ID;
    const bool hasAssignedPhysicalIdentity =
        position != nullptr &&
        position->get_physical_file_id() == ownerPhysicalFileId;
    if (position == nullptr || position->get_parent() != sequence ||
        position->isShared() || !position->isCompilerGenerated() ||
        !position->isFrontendSpecific() || position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        !position->isOutputInCodeGeneration() ||
        position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        hasPreassignmentPhysicalIdentity == hasAssignedPhysicalIdentity) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[catch-sequence-provenance]: sequence=%p "
              "has contradictory source provenance\n",
              static_cast<void *>(sequence));
      ROSE_ABORT();
    }
  }
  return true;
}

bool isExactlyImplicitControlFlowStructuralNode(SgStatement *statement) {
  SgBasicBlock *block = isSgBasicBlock(statement);
  if (block == nullptr || !block->get_is_implicit_control_flow_scope()) {
    return false;
  }

  SgStatement *owner = isSgStatement(block->get_parent());
  const bool exactTypedEdge =
      owner != nullptr &&
      ((isSgIfStmt(owner) != nullptr &&
        (isSgIfStmt(owner)->get_true_body() == block ||
         isSgIfStmt(owner)->get_false_body() == block)) ||
       (isSgForStatement(owner) != nullptr &&
        isSgForStatement(owner)->get_loop_body() == block) ||
       (isSgRangeBasedForStatement(owner) != nullptr &&
        isSgRangeBasedForStatement(owner)->get_loop_body() == block) ||
       (isSgWhileStmt(owner) != nullptr &&
        isSgWhileStmt(owner)->get_body() == block) ||
       (isSgDoWhileStmt(owner) != nullptr &&
        isSgDoWhileStmt(owner)->get_body() == block) ||
       (isSgSwitchStatement(owner) != nullptr &&
        isSgSwitchStatement(owner)->get_body() == block));
  const SgNodePtrList ownerSuccessors =
      owner != nullptr ? owner->get_traversalSuccessorContainer()
                       : SgNodePtrList{};
  const SgNodePtrList blockSuccessors =
      block->get_traversalSuccessorContainer();
  const SgStatementPtrList &statements = block->get_statements();
  SgStatement *payload = statements.size() == 1 ? statements.front() : nullptr;
  SgDeclarationScopeList *declarationScopes =
      block->get_auxiliary_declaration_scopes();
  SgAuxiliaryDeclarationList *declarations =
      block->get_auxiliary_declarations();
  const size_t expectedSuccessorCount = 1 +
                                        (declarationScopes != nullptr ? 1 : 0) +
                                        (declarations != nullptr ? 1 : 0);
  const bool exactBlockSuccessors =
      payload != nullptr && blockSuccessors.size() == expectedSuccessorCount &&
      count(blockSuccessors.begin(), blockSuccessors.end(), payload) == 1 &&
      (declarationScopes == nullptr ||
       count(blockSuccessors.begin(), blockSuccessors.end(),
             declarationScopes) == 1) &&
      (declarations == nullptr ||
       count(blockSuccessors.begin(), blockSuccessors.end(), declarations) ==
           1);
  if (!exactTypedEdge ||
      count(ownerSuccessors.begin(), ownerSuccessors.end(), block) != 1 ||
      payload == nullptr || payload->get_parent() != block ||
      !exactBlockSuccessors || block->get_is_fortran_block_construct()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[implicit-control-flow-header-plan]: "
            "block=%p has no exact typed owner and sole statement edge\n",
            static_cast<void *>(block));
    ROSE_ABORT();
  }

  Sg_File_Info *positions[] = {block->get_file_info(),
                               block->get_startOfConstruct(),
                               block->get_endOfConstruct()};
  for (Sg_File_Info *position : positions) {
    if (position == nullptr || position->get_parent() != block ||
        position->isShared() || !position->isCompilerGenerated() ||
        !position->isFrontendSpecific() || position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        !position->isOutputInCodeGeneration() ||
        position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        position->get_physical_file_id() !=
            Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[implicit-control-flow-header-plan]: "
              "block=%p has contradictory semantic-only provenance\n",
              static_cast<void *>(block));
      ROSE_ABORT();
    }
  }
  return true;
}

bool isTypedNonSurfaceStatement(SgStatement *statement) {
  if (isSgFunctionParameterList(statement) != nullptr ||
      isSgFunctionParameterScope(statement) != nullptr ||
      isSgCtorInitializerList(statement) != nullptr ||
      isSgDeclarationScope(statement) != nullptr) {
    return true;
  }

  if (isExactlyRangeForSemanticDeclarationPayload(statement)) {
    return true;
  }
  if (isExactlyCatchSequenceStructuralNode(statement)) {
    return true;
  }
  if (isExactlyImplicitControlFlowStructuralNode(statement)) {
    return true;
  }

  if (SgForInitStatement *initializer = isSgForInitStatement(statement)) {
    SgForStatement *owner = isSgForStatement(initializer->get_parent());
    if (owner == nullptr || owner->get_for_init_stmt() != initializer ||
        initializer->get_init_stmt().empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[for-init-owner]: wrapper=%p owner=%p/%s "
              "has no exact non-empty for-initializer ownership edge\n",
              static_cast<void *>(initializer),
              static_cast<void *>(initializer->get_parent()),
              initializer->get_parent() != nullptr
                  ? initializer->get_parent()->class_name().c_str()
                  : "<null>");
      ROSE_ABORT();
    }
    for (SgStatement *payload : initializer->get_init_stmt()) {
      if (payload == nullptr || payload->get_parent() != initializer) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[for-init-payload]: wrapper=%p "
                "payload=%p has no exact structural ownership edge\n",
                static_cast<void *>(initializer), static_cast<void *>(payload));
        ROSE_ABORT();
      }
    }
    return true;
  }

  if (SgNullStatement *absentInitializer = isSgNullStatement(statement)) {
    SgForInitStatement *initializer =
        isSgForInitStatement(absentInitializer->get_parent());
    if (initializer == nullptr) {
      SgForStatement *owner = isSgForStatement(absentInitializer->get_parent());
      if (owner == nullptr || owner->get_test() != absentInitializer) {
        return false;
      }
      Sg_File_Info *absentInfo = absentInitializer->get_file_info();
      if (absentInfo == nullptr || !absentInfo->isCompilerGenerated() ||
          !absentInfo->isFrontendSpecific()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[absent-for-condition]: null=%p "
                "owner=%p lacks exact frontend-specific source-less "
                "condition provenance\n",
                static_cast<void *>(absentInitializer),
                static_cast<void *>(owner));
        ROSE_ABORT();
      }
      return true;
    }
    SgForStatement *owner = isSgForStatement(initializer->get_parent());
    Sg_File_Info *initializerInfo = initializer->get_file_info();
    Sg_File_Info *absentInfo = absentInitializer->get_file_info();
    if (owner == nullptr || owner->get_for_init_stmt() != initializer ||
        initializer->get_init_stmt().size() != 1 ||
        initializer->get_init_stmt().front() != absentInitializer ||
        initializerInfo == nullptr || absentInfo == nullptr ||
        !initializerInfo->isCompilerGenerated() ||
        !initializerInfo->isFrontendSpecific() ||
        !absentInfo->isCompilerGenerated() ||
        !absentInfo->isFrontendSpecific()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[absent-for-init]: null=%p "
              "initializer=%p owner=%p has no exact source-less initializer "
              "contract\n",
              static_cast<void *>(absentInitializer),
              static_cast<void *>(initializer), static_cast<void *>(owner));
      ROSE_ABORT();
    }
    return true;
  }

  return false;
}

bool isTypedZeroWidthSourceOwner(SgStatement *statement, const char *contract) {
  SgEmptyDeclaration *empty = isSgEmptyDeclaration(statement);
  if (empty == nullptr) {
    return false;
  }
  ASSERT_not_null(contract);
  empty->validate_lexical_role();
  const auto role = empty->get_lexical_role();
  if (role == SgEmptyDeclaration::e_empty_declaration_source_semicolon) {
    return false;
  }
  if (role != SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor &&
      role != SgEmptyDeclaration::
                  e_empty_declaration_zero_width_source_replacement) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: empty declaration=%p has invalid "
            "typed zero-width role=%d\n",
            contract, static_cast<void *>(empty), static_cast<int>(role));
    ROSE_ABORT();
  }

  Sg_File_Info *primary = empty->get_file_info();
  Sg_File_Info *start = empty->get_startOfConstruct();
  Sg_File_Info *end = empty->get_endOfConstruct();
  if (primary == nullptr || start == nullptr || end == nullptr ||
      primary->isShared() || start->isShared() || end->isShared() ||
      !primary->isOutputInCodeGeneration() ||
      !start->isOutputInCodeGeneration() || !end->isOutputInCodeGeneration() ||
      primary->get_physical_file_id() < 0 ||
      start->get_physical_file_id() != primary->get_physical_file_id() ||
      end->get_physical_file_id() != primary->get_physical_file_id()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: empty declaration=%p role=%d lacks "
            "one exact typed physical output owner\n",
            contract, static_cast<void *>(empty), static_cast<int>(role));
    ROSE_ABORT();
  }

  AttachedPreprocessingInfoType *records =
      empty->getAttachedPreprocessingInfo();
  if (role == SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor &&
      (records == nullptr || records->empty())) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: preprocessing anchor=%p has no exact "
            "record payload\n",
            contract, static_cast<void *>(empty));
    ROSE_ABORT();
  }
  return true;
}

vector<SgLocatedNode *> queryLocatedNodesWithFileInfo(SgScopeStatement *scope,
                                                      const char *inventory) {
  ASSERT_not_null(scope);
  ASSERT_not_null(inventory);

  vector<SgLocatedNode *> locatedNodes;
  const Rose_STL_Container<SgNode *> queryResults =
      NodeQuery::querySubTree(scope, V_SgLocatedNode);
  locatedNodes.reserve(queryResults.size());
  size_t index = 0;
  for (SgNode *node : queryResults) {
    if (node == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-query]: inventory=%s scope=%p "
              "result=%zu is null\n",
              inventory, static_cast<void *>(scope), index);
      ROSE_ABORT();
    }

    SgLocatedNode *located = isSgLocatedNode(node);
    if (located == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-query]: inventory=%s scope=%p "
              "result=%zu node=%p type=%s is not a located node\n",
              inventory, static_cast<void *>(scope), index,
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    if (located->get_file_info() == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-query]: inventory=%s scope=%p "
              "result=%zu node=%p type=%s has no file information\n",
              inventory, static_cast<void *>(scope), index,
              static_cast<void *>(located), located->class_name().c_str());
      ROSE_ABORT();
    }
    locatedNodes.push_back(located);
    ++index;
  }
  return locatedNodes;
}

void insertMaterializedSourceFile(map<string, SgSourceFile *> &sourceFiles,
                                  SgSourceFile *sourceFile,
                                  const char *inventory) {
  ASSERT_not_null(sourceFile);
  const string normalizedFileName =
      FileHelper::normalizePathIfPossible(sourceFile->getFileName());
  if (normalizedFileName.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-source]: inventory=%s source=%p "
            "has no normalized filename\n",
            inventory, static_cast<void *>(sourceFile));
    ROSE_ABORT();
  }
  const auto result = sourceFiles.emplace(normalizedFileName, sourceFile);
  if (!result.second && result.first->second != sourceFile) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-context]: inventory=%s file=%s "
            "maps to distinct source nodes %p and %p\n",
            inventory, normalizedFileName.c_str(),
            static_cast<void *>(result.first->second),
            static_cast<void *>(sourceFile));
    ROSE_ABORT();
  }
}

bool scopeContainsPhysicalFileOutput(SgScopeStatement *scope,
                                     const string &normalizedFileName) {
  ASSERT_not_null(scope);
  if (normalizedFileName.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-scope]: cannot inspect an empty "
            "physical filename\n");
    ROSE_ABORT();
  }

  for (SgLocatedNode *located :
       queryLocatedNodesWithFileInfo(scope, "physical-output")) {
    if (located == scope) {
      continue;
    }
    SgStatement *statement = isSgStatement(located);
    if (statement == nullptr || isTypedNonSurfaceStatement(statement) ||
        isTypedZeroWidthSourceOwner(statement, "header-scope-zero-width")) {
      continue;
    }
    if (isExactlyAuxiliaryOwned(statement, "header-scope")) {
      continue;
    }
    Sg_File_Info *fileInfo = located->get_file_info();
    if (!fileInfo->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-scope]: lexical statement=%p "
              "type=%s uses the legacy non-output suppression bit\n",
              static_cast<void *>(statement), statement->class_name().c_str());
      ROSE_ABORT();
    }
    if (fileInfo->get_physical_file_id() < 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-scope]: lexical statement=%p "
              "type=%s has no exact physical file owner\n",
              static_cast<void *>(statement), statement->class_name().c_str());
      ROSE_ABORT();
    }
    const string nodeFileName = FileHelper::normalizePathIfPossible(
        fileInfo->getFilenameFromID(fileInfo->get_physical_file_id()));
    if (nodeFileName == normalizedFileName) {
      return true;
    }
  }
  return false;
}

bool locatedNodeOwnsPhysicalFilePreprocessing(
    SgLocatedNode *located, const string &normalizedFileName) {
  ASSERT_not_null(located);
  if (normalizedFileName.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-preprocessing]: cannot inspect an "
            "empty physical filename\n");
    ROSE_ABORT();
  }
  if (isExactlyAuxiliaryOwned(located, "header-preprocessing")) {
    return false;
  }

  AttachedPreprocessingInfoType *records =
      located->getAttachedPreprocessingInfo();
  if (records == nullptr) {
    return false;
  }
  for (PreprocessingInfo *record : *records) {
    if (record == nullptr || record->get_file_info() == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-preprocessing]: node=%p type=%s "
              "contains an incomplete preprocessing record\n",
              static_cast<void *>(located), located->class_name().c_str());
      ROSE_ABORT();
    }
    Sg_File_Info *recordInfo = record->get_file_info();
    if (!recordInfo->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-preprocessing]: node=%p type=%s "
              "contains a preprocessing record with the legacy non-output "
              "suppression bit\n",
              static_cast<void *>(located), located->class_name().c_str());
      ROSE_ABORT();
    }
    if (recordInfo->get_physical_file_id() < 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-preprocessing]: node=%p type=%s "
              "contains a preprocessing record without exact physical "
              "ownership\n",
              static_cast<void *>(located), located->class_name().c_str());
      ROSE_ABORT();
    }
    const string recordFileName = FileHelper::normalizePathIfPossible(
        recordInfo->getFilenameFromID(recordInfo->get_physical_file_id()));
    if (recordFileName == normalizedFileName) {
      return true;
    }
  }
  return false;
}

bool scopeContainsPhysicalFilePreprocessing(SgScopeStatement *scope,
                                            const string &normalizedFileName) {
  ASSERT_not_null(scope);
  for (SgLocatedNode *located :
       queryLocatedNodesWithFileInfo(scope, "preprocessing")) {
    if (locatedNodeOwnsPhysicalFilePreprocessing(located, normalizedFileName)) {
      return true;
    }
  }
  return false;
}

struct HeaderOccurrenceContract {
  vector<string> structuralSurfaces;
  vector<string> preprocessingSurfaces;

  bool operator==(const HeaderOccurrenceContract &other) const {
    return structuralSurfaces == other.structuralSurfaces &&
           preprocessingSurfaces == other.preprocessingSurfaces;
  }
};

HeaderOccurrenceContract buildHeaderOccurrenceContract(
    SgScopeStatement *scope, const string &normalizedFileName,
    unsigned int occurrence,
    const map<const PreprocessingInfo *, string> &includeRewrites) {
  ASSERT_not_null(scope);
  if (normalizedFileName.empty() || occurrence == 0) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-context]: file=%s occurrence=%u "
            "cannot build a typed occurrence contract\n",
            normalizedFileName.c_str(), occurrence);
    ROSE_ABORT();
  }

  const auto belongsToOccurrence = [&](SgLocatedNode *located) {
    if (located == nullptr || located->get_file_info() == nullptr) {
      return false;
    }
    Sg_File_Info *info = located->get_file_info();
    if (info->get_physical_file_id() < 0 ||
        info->get_physical_file_occurrence_id() != occurrence) {
      return false;
    }
    return FileHelper::normalizePathIfPossible(info->getFilenameFromID(
               info->get_physical_file_id())) == normalizedFileName;
  };

  HeaderOccurrenceContract contract;
  for (SgLocatedNode *located :
       queryLocatedNodesWithFileInfo(scope, "occurrence-contract")) {
    if (SgStatement *statement = isSgStatement(located)) {
      if (belongsToOccurrence(statement) &&
          !isTypedNonSurfaceStatement(statement) &&
          !isTypedZeroWidthSourceOwner(statement,
                                       "header-contract-zero-width") &&
          !isExactlyAuxiliaryOwned(statement, "header-contract")) {
        bool nestedInOccurrenceStatement = false;
        for (SgNode *ancestor = statement->get_parent();
             ancestor != nullptr && ancestor != scope;
             ancestor = ancestor->get_parent()) {
          SgStatement *ancestorStatement = isSgStatement(ancestor);
          if (ancestorStatement != nullptr &&
              belongsToOccurrence(ancestorStatement) &&
              !isTypedNonSurfaceStatement(ancestorStatement) &&
              !isTypedZeroWidthSourceOwner(
                  ancestorStatement, "header-contract-ancestor-zero-width") &&
              !isExactlyAuxiliaryOwned(ancestorStatement,
                                       "header-contract-ancestor")) {
            nestedInOccurrenceStatement = true;
            break;
          }
        }
        if (!nestedInOccurrenceStatement) {
          Sg_File_Info *start = statement->get_startOfConstruct();
          Sg_File_Info *end = statement->get_endOfConstruct();
          if (start == nullptr || end == nullptr) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[header-context]: file=%s "
                    "occurrence=%u statement=%p/%s has no exact source "
                    "range\n",
                    normalizedFileName.c_str(), occurrence,
                    static_cast<void *>(statement),
                    statement->class_name().c_str());
            ROSE_ABORT();
          }
          ostringstream surface;
          surface << start->get_physical_line() << ':' << start->get_col()
                  << '-' << end->get_physical_line() << ':' << end->get_col()
                  << '|' << statement->class_name() << '|'
                  << Rose::AstJson::canonicalSubtreeSignature(statement);
          contract.structuralSurfaces.push_back(surface.str());
        }
      }
    }

    AttachedPreprocessingInfoType *records =
        located->getAttachedPreprocessingInfo();
    if (records == nullptr) {
      continue;
    }
    for (PreprocessingInfo *record : *records) {
      Sg_File_Info *recordInfo =
          record != nullptr ? record->get_file_info() : nullptr;
      if (recordInfo == nullptr ||
          recordInfo->get_physical_file_occurrence_id() != occurrence ||
          recordInfo->get_physical_file_id() < 0 ||
          FileHelper::normalizePathIfPossible(recordInfo->getFilenameFromID(
              recordInfo->get_physical_file_id())) != normalizedFileName) {
        continue;
      }
      const auto rewrite = includeRewrites.find(record);
      ostringstream surface;
      surface << recordInfo->get_physical_line() << ':' << recordInfo->get_col()
              << '|' << static_cast<int>(record->getTypeOfDirective()) << '|'
              << static_cast<int>(record->getRelativePosition()) << '|'
              << static_cast<int>(record->getOutputPlacement()) << '|'
              << record->getString() << '|'
              << (rewrite != includeRewrites.end() ? rewrite->second : "");
      contract.preprocessingSurfaces.push_back(surface.str());
    }
  }

  sort(contract.structuralSurfaces.begin(), contract.structuralSurfaces.end());
  sort(contract.preprocessingSurfaces.begin(),
       contract.preprocessingSurfaces.end());
  return contract;
}

void collectIncludeTreeFiles(SgIncludeFile *includeTreeRoot,
                             set<string> &allFiles,
                             map<string, SgSourceFile *> &sourceFiles) {
  if (includeTreeRoot == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[include-tree]: missing include-tree root\n");
    ROSE_ABORT();
  }

  vector<SgIncludeFile *> worklist;
  set<SgIncludeFile *> visited;
  worklist.push_back(includeTreeRoot);

  while (!worklist.empty()) {
    SgIncludeFile *includeFile = worklist.back();
    worklist.pop_back();
    if (includeFile == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[include-tree]: null include-file node\n");
      ROSE_ABORT();
    }

    if (!visited.insert(includeFile).second)
      continue;

    const string includeFileName =
        FileHelper::normalizePathIfPossible(includeFile->get_filename());
    if (includeFileName.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[include-tree]: include-file node %p has "
              "no normalized filename\n",
              static_cast<void *>(includeFile));
      ROSE_ABORT();
    }
    allFiles.insert(includeFileName);

    SgSourceFile *includedSourceFile = includeFile->get_source_file();
    if (includedSourceFile != nullptr) {
      const string sourceFileName = FileHelper::normalizePathIfPossible(
          includedSourceFile->getFileName());
      if (sourceFileName.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[include-tree]: source-file node %p "
                "has no normalized filename\n",
                static_cast<void *>(includedSourceFile));
        ROSE_ABORT();
      }
      allFiles.insert(sourceFileName);
      insertMaterializedSourceFile(sourceFiles, includedSourceFile,
                                   "include-tree");
    }

    const SgIncludeFilePtrList &includeFileList =
        includeFile->get_include_file_list();
    for (size_t i = 0; i < includeFileList.size(); ++i)
      worklist.push_back(includeFileList[i]);
  }
}

void buildIncludeTreeParentMap(
    SgIncludeFile *includeTreeRoot,
    map<string, set<string>> &includedToIncludingFiles) {
  if (includeTreeRoot == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[include-tree]: missing include-tree root\n");
    ROSE_ABORT();
  }

  vector<SgIncludeFile *> worklist;
  set<SgIncludeFile *> visited;
  worklist.push_back(includeTreeRoot);

  while (!worklist.empty()) {
    SgIncludeFile *includingFile = worklist.back();
    worklist.pop_back();
    if (includingFile == nullptr) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[include-tree]: null including-file node\n");
      ROSE_ABORT();
    }

    if (!visited.insert(includingFile).second)
      continue;

    const string includingFileName =
        FileHelper::normalizePathIfPossible(includingFile->get_filename());
    if (includingFileName.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[include-tree]: including-file node %p "
              "has no normalized filename\n",
              static_cast<void *>(includingFile));
      ROSE_ABORT();
    }

    const SgIncludeFilePtrList &includeFileList =
        includingFile->get_include_file_list();
    for (size_t i = 0; i < includeFileList.size(); ++i) {
      SgIncludeFile *includedFile = includeFileList[i];
      if (includedFile == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[include-tree]: %s contains a null "
                "child include\n",
                includingFileName.c_str());
        ROSE_ABORT();
      }

      worklist.push_back(includedFile);

      const string includedFileName =
          FileHelper::normalizePathIfPossible(includedFile->get_filename());
      if (includedFileName.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[include-tree]: included-file node %p "
                "has no normalized filename\n",
                static_cast<void *>(includedFile));
        ROSE_ABORT();
      }
      includedToIncludingFiles[includedFileName].insert(includingFileName);
    }
  }
}

bool isParentRelativeToRoot(const std::filesystem::path &relativePath) {
  if (relativePath.empty())
    return false;

  for (const std::filesystem::path &component : relativePath) {
    if (component == "..")
      return false;
  }

  return true;
}

bool isProjectInputFile(SgProject *project, const string &absoluteFileName) {
  ASSERT_not_null(project);
  const string normalizedFileName =
      FileHelper::normalizePathIfPossible(absoluteFileName);
  if (normalizedFileName.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[input-path]: cannot classify an empty "
            "input filename\n");
    ROSE_ABORT();
  }

  const SgFilePtrList &fileList = project->get_fileList();
  SgSourceFile *exactSource = nullptr;
  for (size_t index = 0; index < fileList.size(); ++index) {
    SgFile *file = fileList[index];
    if (file == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-project]: project input %zu is "
              "null\n",
              index);
      ROSE_ABORT();
    }
    const string inputFileName =
        FileHelper::normalizePathIfPossible(file->getFileName());
    if (inputFileName.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-project]: project input %zu has "
              "no normalized filename\n",
              index);
      ROSE_ABORT();
    }
    if (inputFileName == normalizedFileName) {
      SgSourceFile *source = isSgSourceFile(file);
      if (source == nullptr || exactSource != nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-project]: path=%s does not "
                "identify exactly one project source file\n",
                normalizedFileName.c_str());
        ROSE_ABORT();
      }
      if (!source->get_skip_unparse() && source->get_unparseHeaderFiles()) {
        exactSource = source;
      }
    }
  }
  return exactSource != nullptr && exactSource->get_isCommandLineInputSource();
}

string projectInputRelativeOutputPath(SgProject *project,
                                      const string &absoluteFileName) {
  ASSERT_not_null(project);
  const string normalizedInput =
      FileHelper::normalizePathIfPossible(absoluteFileName);
  const string normalizedApplicationRoot = FileHelper::normalizePathIfPossible(
      project->get_applicationRootDirectory());
  if (normalizedInput.empty() || normalizedApplicationRoot.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[input-path]: input=%s application-root=%s "
            "does not define an exact normalized output anchor\n",
            absoluteFileName.c_str(),
            project->get_applicationRootDirectory().c_str());
    ROSE_ABORT();
  }

  const filesystem::path relative =
      filesystem::path(normalizedInput)
          .lexically_relative(filesystem::path(normalizedApplicationRoot))
          .lexically_normal();
  if (relative.empty() || relative == "." || relative.is_absolute() ||
      !isParentRelativeToRoot(relative)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[input-path]: CLI input=%s is outside "
            "application root=%s (relative=%s)\n",
            normalizedInput.c_str(), normalizedApplicationRoot.c_str(),
            Rose::FileSystem::toString(relative).c_str());
    ROSE_ABORT();
  }
  return Rose::FileSystem::toString(relative);
}

struct ExactIncludedHeaderEdge {
  SgSourceFile *sourceFile;
  SgIncludeFile *includeFile;
  SgIncludeFile *parentIncludeFile;
  string includeSpelling;
  string parentFileName;
};

ExactIncludedHeaderEdge
requireExactIncludedHeaderEdge(const string &absoluteFileName,
                               const map<string, SgSourceFile *> &sourceFiles,
                               const char *operation) {
  ASSERT_not_null(operation);
  const string normalizedFileName =
      FileHelper::normalizePathIfPossible(absoluteFileName);
  if (normalizedFileName.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-include-graph]: operation=%s "
            "header=%s has no normalized filename\n",
            operation, absoluteFileName.c_str());
    ROSE_ABORT();
  }

  const auto sourceEntry = sourceFiles.find(normalizedFileName);
  if (sourceEntry == sourceFiles.end() || sourceEntry->second == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-include-graph]: operation=%s "
            "header=%s has no exact materialized source-file owner\n",
            operation, normalizedFileName.c_str());
    ROSE_ABORT();
  }
  SgSourceFile *sourceFile = sourceEntry->second;
  const string sourceFileName =
      FileHelper::normalizePathIfPossible(sourceFile->getFileName());
  if (sourceFileName != normalizedFileName) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-include-graph]: operation=%s "
            "header=%s source=%p owns mismatched filename=%s\n",
            operation, normalizedFileName.c_str(),
            static_cast<void *>(sourceFile), sourceFileName.c_str());
    ROSE_ABORT();
  }

  SgIncludeFile *includeFile = sourceFile->get_associated_include_file();
  if (includeFile == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-include-graph]: operation=%s "
            "header=%s source=%p has no associated include node\n",
            operation, normalizedFileName.c_str(),
            static_cast<void *>(sourceFile));
    ROSE_ABORT();
  }
  const string includeFileName =
      FileHelper::normalizePathIfPossible(includeFile->get_filename());
  if (includeFileName != normalizedFileName ||
      includeFile->get_source_file() != sourceFile) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-include-graph]: operation=%s "
            "header=%s source=%p associated-include=%p owns filename=%s "
            "source=%p\n",
            operation, normalizedFileName.c_str(),
            static_cast<void *>(sourceFile), static_cast<void *>(includeFile),
            includeFileName.c_str(),
            static_cast<void *>(includeFile->get_source_file()));
    ROSE_ABORT();
  }

  SgIncludeFile *parentIncludeFile = includeFile->get_parent_include_file();
  if (parentIncludeFile == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-include-graph]: operation=%s "
            "header=%s include=%p has no parent include node\n",
            operation, normalizedFileName.c_str(),
            static_cast<void *>(includeFile));
    ROSE_ABORT();
  }
  size_t parentEdgeCount = 0;
  for (SgIncludeFile *child : parentIncludeFile->get_include_file_list()) {
    if (child == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-include-graph]: operation=%s "
              "header=%s parent=%p contains a null child include node\n",
              operation, normalizedFileName.c_str(),
              static_cast<void *>(parentIncludeFile));
      ROSE_ABORT();
    }
    if (child == includeFile)
      ++parentEdgeCount;
  }
  if (parentEdgeCount != 1) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-include-graph]: operation=%s "
            "header=%s parent=%p contains associated include=%p exactly "
            "%zu times instead of once\n",
            operation, normalizedFileName.c_str(),
            static_cast<void *>(parentIncludeFile),
            static_cast<void *>(includeFile), parentEdgeCount);
    ROSE_ABORT();
  }

  const string includeSpelling =
      includeFile->get_name_used_in_include_directive().getString();
  if (includeSpelling.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-include-graph]: operation=%s "
            "header=%s include=%p has no exact source spelling\n",
            operation, normalizedFileName.c_str(),
            static_cast<void *>(includeFile));
    ROSE_ABORT();
  }
  if (FileHelper::isAbsolutePath(includeSpelling)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-include-graph]: operation=%s "
            "header=%s include=%p has absolute source spelling=%s\n",
            operation, normalizedFileName.c_str(),
            static_cast<void *>(includeFile), includeSpelling.c_str());
    ROSE_ABORT();
  }
  // Parent components are valid source syntax: a file in a subdirectory can
  // include a header beside that directory with "../header.h". Safety is a
  // property of the fully resolved output path, not of one edge spelling.
  // populateUnparseMap() and getCopiedFileOutputPath() normalize the complete
  // include chain and hard-fail if it escapes the configured output root.

  const string parentFileName =
      FileHelper::normalizePathIfPossible(parentIncludeFile->get_filename());
  if (parentFileName.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-include-graph]: operation=%s "
            "header=%s parent=%p has no normalized filename\n",
            operation, normalizedFileName.c_str(),
            static_cast<void *>(parentIncludeFile));
    ROSE_ABORT();
  }

  return {sourceFile, includeFile, parentIncludeFile, includeSpelling,
          parentFileName};
}

string makeRelativeOutputPath(const string &directory, const string &fileName) {
  if (fileName.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-output]: cannot plan an empty output "
            "filename\n");
    ROSE_ABORT();
  }
  if (directory.empty() || directory == ".")
    return fileName;

  return FileHelper::concatenatePaths(directory, fileName);
}

} // namespace

const string IncludedFilesUnparser::defaultUnparseFolderName =
    "_rose_unparsed_headers_";

// It is needed because otherwise, the default destructor breaks something.

IncludedFilesUnparser::~IncludedFilesUnparser() {
  // do nothing
}

IncludedFilesUnparser::IncludedFilesUnparser(SgProject *projectNode) {
  ASSERT_not_null(projectNode);
  this->projectNode = projectNode;
}

string IncludedFilesUnparser::getUnparseRootPath() { return unparseRootPath; }

map<string, string> IncludedFilesUnparser::getUnparseMap() {
  return unparseMap;
}

map<string, SgScopeStatement *> IncludedFilesUnparser::getUnparseScopesMap() {
  return unparseScopesMap;
}

map<string, unsigned int> IncludedFilesUnparser::getUnparseOccurrenceMap() {
  return unparseOccurrenceMap;
}

map<string, SgSourceFile *> IncludedFilesUnparser::getUnparseSourceFileMap() {
  // DQ (9/7/2018): Added to support retrival of SgSourceFile built in the
  // frontend.
  return unparseSourceFileMap;
}

set<string> IncludedFilesUnparser::getFilesToCopy() {
  // DQ (11/19/2018): Added access function.
  return filesToCopy;
}

const set<string> &
IncludedFilesUnparser::getFilesWithMarkedTransformations() const {
  return filesWithMarkedTransformations;
}

const set<string> &
IncludedFilesUnparser::getFilesWithUpdatedIncludePaths() const {
  return filesWithUpdatedIncludePaths;
}

const map<const PreprocessingInfo *, string> &
IncludedFilesUnparser::getIncludeRewrites() const {
  return includeRewrites;
}

list<string> IncludedFilesUnparser::getIncludeCompilerOptions() {
  list<string> includeCompilerOptions;
  for (list<pair<int, string>>::const_iterator it =
           includeCompilerPaths.begin();
       it != includeCompilerPaths.end(); it++) {
    includeCompilerOptions.push_back("-I" + it->second);
  }
  return includeCompilerOptions;
}

// void IncludedFilesUnparser::unparse()
void IncludedFilesUnparser::figureOutWhichFilesToUnparse() {
  // This function does not unparse any files, but identified which included
  // files will require unparsing (in addition to the original input source
  // file).

  // DQ (4/6/2020): Added assertion.
  ASSERT_not_null(projectNode);
  SgSourceFile *planningSource =
      requireHeaderPlanningSource(projectNode, "header-project");

  // Build invocation-local derived include data. Planning output must not
  // rewrite the project merely because transformations changed an include.
  {
    map<string, set<string>> emptyIncludedFilesMap;
    IncludingPreprocessingInfosCollector collector(projectNode,
                                                   emptyIncludedFilesMap);
    includingPreprocessingInfos = collector.collect();
  }

#define DEBUG_FIGURE_OUT 0

#if DEBUG_FIGURE_OUT
  printf("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \n");
  printf("In IncludedFilesUnparser::figureOutWhichFilesToUnparse(): \n");
  printf(" --- projectNode->usingDeferredTransformations = %s \n",
         projectNode->get_usingDeferredTransformations() ? "true" : "false");
  printf("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \n");
#endif

  workingDirectory =
      FileHelper::normalizePath(planningSource->getWorkingDirectory());
  string userSpecifiedUnparseRootFolder =
      projectNode->get_unparseHeaderFilesRootFolder();
  if (userSpecifiedUnparseRootFolder.empty() == true) {
    // No folder specified, use the default location.
    unparseRootPath =
        Rose::TestOutput::resolvePath(FileHelper::concatenatePaths(
            workingDirectory, defaultUnparseFolderName));
  } else {
    if (FileHelper::isAbsolutePath(userSpecifiedUnparseRootFolder)) {
      unparseRootPath = userSpecifiedUnparseRootFolder;
    } else {
      unparseRootPath = FileHelper::concatenatePaths(
          workingDirectory, userSpecifiedUnparseRootFolder);
    }

    // Check that the specified location does not exist or is empty. This is
    // necessary to avoid data loss since this folder will be erased.
    if (FileHelper::isNotEmptyFolder(unparseRootPath)) {
      // DQ (1/29/2018): This case happens when running ROSE from the command
      // line and maybe we should automate the removal of this directory.
      cout << "Please make sure that the root folder for header files "
              "unparsing does not exist or is empty:"
           << unparseRootPath << endl;
      ROSE_ABORT();
    }
  }

  // Should be erased completely at every run to avoid name collisions with
  // previous runs.
  FileHelper::eraseFolder(unparseRootPath);
  filesWithMarkedTransformations.clear();
  filesWithUpdatedIncludePaths.clear();

  // collect immediately affected files as well as all traversed files

  // DQ (4/6/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("unparseAllHeaderFiles == false: calling traversal to determine "
           "modified header files \n");
  }

  // DQ (11/28/2018): I think the order of the traversal should be postorder
  // instead of preorder, because we sometimes mark the enclosing statement tn
  // as modified.  Note: the travesal sets the allFiles list.
  traverse(projectNode, preorder);
  validateUnparseScopeCandidates();

  for (std::map<SgSourceFile *,
                std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
                    *>::const_iterator tokenMapIt =
           Rose::tokenSubsequenceMapOfMapsBySourceFile.begin();
       tokenMapIt != Rose::tokenSubsequenceMapOfMapsBySourceFile.end();
       ++tokenMapIt) {
    SgSourceFile *sourceFile = tokenMapIt->first;
    if (sourceFile == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-token-map]: token map contains a "
              "null source-file key\n");
      ROSE_ABORT();
    }
    if (sourceFile->get_isHeaderFile() == false) {
      continue;
    }

    string normalizedFileName =
        FileHelper::normalizePathIfPossible(sourceFile->getFileName());
    if (normalizedFileName.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-path]: source-file node %p has "
              "no normalized filename\n",
              static_cast<void *>(sourceFile));
      ROSE_ABORT();
    }

    allFiles.insert(normalizedFileName);
    insertMaterializedSourceFile(unparseSourceFileMap, sourceFile, "token-map");

    SgScopeStatement *globalScope = sourceFile->get_globalScope();
    if (globalScope != NULL && globalScope->get_parent() == sourceFile &&
        (scopeContainsPhysicalFileOutput(globalScope, normalizedFileName) ||
         scopeContainsPhysicalFilePreprocessing(globalScope,
                                                normalizedFileName))) {
      const auto existing = unparseScopesMap.find(normalizedFileName);
      const auto occurrence = unparseOccurrenceMap.find(normalizedFileName);
      if (existing == unparseScopesMap.end() ||
          occurrence == unparseOccurrenceMap.end() ||
          existing->second == nullptr || occurrence->second == 0) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-context]: materialized "
                "header=%s global=%p has no exact lexical emission "
                "occurrence\n",
                normalizedFileName.c_str(), static_cast<void *>(globalScope));
        ROSE_ABORT();
      }
    }
  }

  // DQ (4/6/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("List allFiles list: processing parent include files chain: (size = "
           "%zu): \n",
           allFiles.size());
  }

  // DQ (11/30/2019): Process the header files to include possible header files
  // that only contained another header files (and so are not supported within
  // the traversal).  This addresses at least test11 in the UnparseHeadersTest
  // directory.
  map<string, set<string>> includedToIncludingFiles;
  const SgFilePtrList &fileList = projectNode->get_fileList();
  for (SgFilePtrList::const_iterator file = fileList.begin();
       file != fileList.end(); ++file) {
    SgSourceFile *sourceFile = isSgSourceFile(*file);
    if (sourceFile == nullptr || sourceFile->get_skip_unparse() ||
        !sourceFile->get_unparseHeaderFiles())
      continue;

    SgIncludeFile *includeTreeRoot = sourceFile->get_associated_include_file();
    collectIncludeTreeFiles(includeTreeRoot, allFiles, unparseSourceFileMap);
    buildIncludeTreeParentMap(includeTreeRoot, includedToIncludingFiles);
  }

  const map<string, set<PreprocessingInfo *>> &includingPreprocessingInfosMap =
      includingPreprocessingInfos;

  set<string> workSet = allFiles;
  while (!workSet.empty()) {
    set<string> newParents;
    for (const string &filename : workSet) {
      if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
        printf("   --- allFiles entry = %s \n", filename.c_str());
      }

      const string normalizedFilename =
          FileHelper::normalizePathIfPossible(filename);

      map<string, set<string>>::const_iterator includeTreeEntry =
          includedToIncludingFiles.find(normalizedFilename);
      if (includeTreeEntry == includedToIncludingFiles.end())
        includeTreeEntry = includedToIncludingFiles.find(filename);
      if (includeTreeEntry != includedToIncludingFiles.end()) {
        for (const string &includingFileName : includeTreeEntry->second) {
          if (includingFileName.empty()) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[header-path]: include tree has an "
                    "empty including filename\n");
            ROSE_ABORT();
          }
          if (allFiles.insert(includingFileName).second)
            newParents.insert(includingFileName);
        }
      }

      map<string, set<PreprocessingInfo *>>::const_iterator mapEntry =
          includingPreprocessingInfosMap.find(normalizedFilename);
      if (mapEntry == includingPreprocessingInfosMap.end())
        mapEntry = includingPreprocessingInfosMap.find(filename);
      if (mapEntry != includingPreprocessingInfosMap.end()) {
        const set<PreprocessingInfo *> &includingPreprocessingInfos =
            mapEntry->second;
        for (set<PreprocessingInfo *>::const_iterator
                 includingPreprocessingInfoPtr =
                     includingPreprocessingInfos.begin();
             includingPreprocessingInfoPtr != includingPreprocessingInfos.end();
             includingPreprocessingInfoPtr++) {
          string normalizedIncludingFileName =
              FileHelper::getNormalizedContainingFileName(
                  *includingPreprocessingInfoPtr);
          if (allFiles.insert(normalizedIncludingFileName).second)
            newParents.insert(normalizedIncludingFileName);
        }
      }
    }

    for (map<string, set<PreprocessingInfo *>>::const_iterator mapEntry =
             includingPreprocessingInfosMap.begin();
         mapEntry != includingPreprocessingInfosMap.end(); ++mapEntry) {
      string normalizedIncludedFileName =
          FileHelper::normalizePathIfPossible(mapEntry->first);
      if (normalizedIncludedFileName.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-path]: preprocessing include "
                "map contains an empty normalized path\n");
        ROSE_ABORT();
      }
      if (allFiles.find(normalizedIncludedFileName) != allFiles.end())
        continue;

      const set<PreprocessingInfo *> &includingPreprocessingInfos =
          mapEntry->second;
      for (set<PreprocessingInfo *>::const_iterator
               includingPreprocessingInfoPtr =
                   includingPreprocessingInfos.begin();
           includingPreprocessingInfoPtr != includingPreprocessingInfos.end();
           includingPreprocessingInfoPtr++) {
        IncludeDirective includeDirective(
            (*includingPreprocessingInfoPtr)->getString());
        if (includeDirective.isQuotedInclude() == false &&
            FileHelper::isAbsolutePath(includeDirective.getIncludedPath()) ==
                false)
          continue;

        string normalizedIncludingFileName =
            FileHelper::getNormalizedContainingFileName(
                *includingPreprocessingInfoPtr);
        if (allFiles.find(normalizedIncludingFileName) == allFiles.end())
          continue;

        if (allFiles.insert(normalizedIncludedFileName).second)
          newParents.insert(normalizedIncludedFileName);
        break;
      }
    }
    workSet.swap(newParents);
  }

  // DQ (4/6/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("List allFiles list (size = %zu): \n", allFiles.size());
    for (set<string>::iterator i = allFiles.begin(); i != allFiles.end(); i++) {
      printf("   --- allFiles = %s \n", (*i).c_str());
    }
  }

#if DEBUG_FIGURE_OUT
  // DQ (4/6/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("List modifiedFiles list (size = %zu): \n", modifiedFiles.size());
    set<string>::iterator j = modifiedFiles.begin();
    size_t modified_file_counter = 0;
    while (j != modifiedFiles.end()) {
      printf("   --- modifiedFiles[%zu] = %s \n", modified_file_counter,
             (*j).c_str());

      j++;
      modified_file_counter++;
    }
  }
#endif

  // A transformed physical header requires its frontend-owned SgSourceFile
  // before output-path planning can consult the include graph.  Diagnose that
  // broken producer contract here, where the transformation traversal has
  // established exact file ownership, rather than misclassifying it later as
  // a generic missing include edge.
  for (const string &modifiedFile : filesWithMarkedTransformations) {
    if (isInputFile(modifiedFile)) {
      continue;
    }
    const auto source = unparseSourceFileMap.find(modifiedFile);
    if (source == unparseSourceFileMap.end() || source->second == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[dirty-header-without-ast]: %s was "
              "modified but the frontend did not materialize its "
              "SgSourceFile\n",
              modifiedFile.c_str());
      ROSE_ABORT();
    }
  }

  initializeFilesToUnparse();

  // A more efficient way would be to do it incrementally rather than repeating
  // the whole iteration. But the probability of more than one iteration is
  // extremely low, so an average overhead is very insignificant.
  do {
    prepareForNewIteration();

    // A relocated or regenerated header changes the include search result for
    // every reverse includer. Compute that closure for both structural and
    // token modes, then separately collect unchanged files whose exact bytes
    // must be present in the generated tree.
    collectAdditionalFilesToUnparse();
    collectAdditionalListOfHeaderFilesToCopy();

    // DQ (4/14/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf(
          "In IncludedFilesUnparser::figureOutWhichFilesToUnparse(): calling "
          "applyFunctionToIncludingPreprocessingInfos(filesToUnparse) \n");
    }

    applyFunctionToIncludingPreprocessingInfos(
        filesToUnparse,
        &IncludedFilesUnparser::collectIncludingPathsFromUnaffectedFiles);

    populateUnparseMap();

    collectIncludeCompilerPaths();

    // DQ (4/14/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf("In IncludedFilesUnparser::figureOutWhichFilesToUnparse(): "
             "calling applyFunctionToIncludingPreprocessingInfos(allFiles) \n");
    }

    // DQ (4/15/2020): I don't think this is the cause.
    // DQ (4/14/2020): This causes a problem for the test8 regression test.
    applyFunctionToIncludingPreprocessingInfos(
        allFiles, &IncludedFilesUnparser::collectNotUnparsedPreprocessingInfos);

    collectNotUnparsedFilesThatRequireUnparsingToAvoidFileNameCollisions();

    if (SgProject::get_verbose() > 0) {
      CollectionHelper::printSet(
          newFilesToUnparse,
          "\nAdditional files to unparse due to path conflicts:", "");
      cout << endl << endl;
    }
    // DQ (4/13/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf("At bottom of DO WHILE loop: newFilesToUnparse.size() = %zu \n",
             newFilesToUnparse.size());
    }
  } while (!newFilesToUnparse.empty());

  // Include relocation is part of the output plan for both AST and token
  // emission. Token replay receives the same invocation-owned rewrite map, so
  // it must not preserve a stale include merely because token mode is active.
  applyFunctionToIncludingPreprocessingInfos(
      allFiles, &IncludedFilesUnparser::updatePreprocessingInfoPaths);

  for (list<pair<int, string>>::const_iterator it =
           includeCompilerPaths.begin();
       it != includeCompilerPaths.end(); it++) {
    FileHelper::ensureFolderExists(it->second);
  }

#if DEBUG_FIGURE_OUT
  // DQ (4/13/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() > 0) {
    printf("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \n");
    printf("Leaving IncludedFilesUnparser::figureOutWhichFilesToUnparse(): \n");
    printf("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \n");
  }
#endif

  // DQ (4/5/2020): Exit as part of debugging.
}

void IncludedFilesUnparser::printDiagnosticOutput() {
  if (SgProject::get_verbose() > 0) {
    printf("In IncludedFilesUnparser::printDiagnosticOutput(): Output internal "
           "data \n");
    CollectionHelper::printSet(allFiles, "\nAll files:", "");
    CollectionHelper::printSet(modifiedFiles, "\nModified files:", "");

    // DQ (10/9/2019): Added debugging support, this is not one of the lists we
    // support. CollectionHelper::printSet(modifiedIncludeFiles, "\nModified
    // files:", "");

    CollectionHelper::printSet(filesToUnparse, "\nFiles to unparse:", "");

    CollectionHelper::printSet(filesToCopy, "\nCopy files:", "");

    CollectionHelper::printMapOfSets(
        includingPathsMap,
        "\nIncluding paths map:", "Included file:", "Including path:");

    for (map<string, string>::const_iterator it = unparseMap.begin();
         it != unparseMap.end(); it++) {
      cout << "Unparsed file:" << it->first << "\nDestination:" << it->second
           << endl
           << endl;
    }

    cout << "\nInclude compiler paths:" << endl;
    for (list<pair<int, string>>::const_iterator it =
             includeCompilerPaths.begin();
         it != includeCompilerPaths.end(); it++) {
      cout << it->first << ":" << it->second << endl;
    }

    cout << endl << endl;
  }
}

void IncludedFilesUnparser::prepareForNewIteration() {

  filesToUnparse.insert(newFilesToUnparse.begin(), newFilesToUnparse.end());
  newFilesToUnparse.clear();
  includingPathsMap.clear();
  notUnparsedPreprocessingInfos.clear();
  unparseMap.clear();
  unparsePaths.clear();
  includeCompilerPaths.clear();

  // The unparse root path is always included (though could be redundant if no
  // included files need unparsing).
  addIncludeCompilerPath(0, unparseRootPath);
}

bool IncludedFilesUnparser::isInputFile(const string &absoluteFileName) {
  return isProjectInputFile(projectNode, absoluteFileName);
}

string IncludedFilesUnparser::getCopiedFileOutputPath(
    const string &absoluteFileName) const {
  const string normalizedFileName =
      FileHelper::normalizePathIfPossible(absoluteFileName);
  if (normalizedFileName.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-output]: copied input=%s has no "
            "normalized filename\n",
            absoluteFileName.c_str());
    ROSE_ABORT();
  }
  if (filesToCopy.find(normalizedFileName) == filesToCopy.end())
    return "";

  if (isProjectInputFile(projectNode, normalizedFileName)) {
    return FileHelper::concatenatePaths(
        unparseRootPath,
        projectInputRelativeOutputPath(projectNode, normalizedFileName));
  }

  vector<string> reverseIncludeSpellings;
  set<string> visited;
  string currentFileName = normalizedFileName;
  string resolvedAncestorOutput;
  while (resolvedAncestorOutput.empty()) {
    if (!visited.insert(currentFileName).second) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-output-graph]: copied header=%s "
              "has a cyclic include-parent chain at %s\n",
              normalizedFileName.c_str(), currentFileName.c_str());
      ROSE_ABORT();
    }

    const ExactIncludedHeaderEdge edge = requireExactIncludedHeaderEdge(
        currentFileName, unparseSourceFileMap, "copy-output");
    reverseIncludeSpellings.push_back(edge.includeSpelling);

    const auto parentOutput = unparseMap.find(edge.parentFileName);
    if (parentOutput != unparseMap.end()) {
      if (parentOutput->second.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-output-graph]: copied header=%s "
                "parent=%s has an empty resolved output\n",
                normalizedFileName.c_str(), edge.parentFileName.c_str());
        ROSE_ABORT();
      }
      resolvedAncestorOutput = parentOutput->second;
      break;
    }

    if (isProjectInputFile(projectNode, edge.parentFileName)) {
      resolvedAncestorOutput =
          projectInputRelativeOutputPath(projectNode, edge.parentFileName);
      break;
    }

    if (filesToCopy.find(edge.parentFileName) == filesToCopy.end()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-output-graph]: copied header=%s "
              "parent=%s has no resolved unparse or copy output\n",
              normalizedFileName.c_str(), edge.parentFileName.c_str());
      ROSE_ABORT();
    }
    currentFileName = edge.parentFileName;
  }

  string relativeOutput = resolvedAncestorOutput;
  for (auto spelling = reverseIncludeSpellings.rbegin();
       spelling != reverseIncludeSpellings.rend(); ++spelling) {
    string parentOutputDirectory = Rose::getPathFromFileName(relativeOutput);
    if (parentOutputDirectory == ".")
      parentOutputDirectory.clear();
    const std::filesystem::path outputPath =
        std::filesystem::path(
            makeRelativeOutputPath(parentOutputDirectory, *spelling))
            .lexically_normal();
    if (outputPath.is_absolute() || !isParentRelativeToRoot(outputPath)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-output]: copied header=%s exact "
              "include graph escapes the unparse root at %s\n",
              normalizedFileName.c_str(),
              Rose::FileSystem::toString(outputPath).c_str());
      ROSE_ABORT();
    }
    relativeOutput = Rose::FileSystem::toString(outputPath);
  }

  return FileHelper::concatenatePaths(unparseRootPath, relativeOutput);
}

void IncludedFilesUnparser::
    collectNotUnparsedFilesThatRequireUnparsingToAvoidFileNameCollisions() {
  newFilesToUnparse.clear();
  for (set<PreprocessingInfo *>::const_iterator preprocessingInfoPtr =
           notUnparsedPreprocessingInfos.begin();
       preprocessingInfoPtr != notUnparsedPreprocessingInfos.end();
       preprocessingInfoPtr++) {
    IncludeDirective includeDirective((*preprocessingInfoPtr)->getString());
    const string &includePath = includeDirective.getIncludedPath();
    if (isConflictingIncludePath(includePath)) {
      newFilesToUnparse.insert(
          FileHelper::getNormalizedContainingFileName(*preprocessingInfoPtr));
    }
  }
}

bool IncludedFilesUnparser::isConflictingIncludePath(
    const string &includePath) {
  for (list<pair<int, string>>::const_iterator includeCompilerPathsIterator =
           includeCompilerPaths.begin();
       includeCompilerPathsIterator != includeCompilerPaths.end();
       includeCompilerPathsIterator++) {
    const string &potentialIncludedFilePath = FileHelper::concatenatePaths(
        includeCompilerPathsIterator->second, includePath);
    if (FileHelper::fileExists(potentialIncludedFilePath)) {
      // This is a conflict with an existing file.
      return true;
    }
    for (set<string>::const_iterator unparsePathPtr = unparsePaths.begin();
         unparsePathPtr != unparsePaths.end(); unparsePathPtr++) {
      const string &unparsedIncludedFilePath =
          FileHelper::concatenatePaths(unparseRootPath, *unparsePathPtr);
      if (FileHelper::areEquivalentPaths(potentialIncludedFilePath,
                                         unparsedIncludedFilePath)) {
        // This is a conflict with a file that will be unparsed.
        return true;
      }
    }
  }
  return false;
}

// TODO: Probably this would not handle correctly cases like #include
// <../subdir/../A.h> because the normalized representation would be
//  <../A.h> and thus, "subdir" would not be created and the file would not be
//  found by the preprocessor. Check and fix, if needed.
void IncludedFilesUnparser::collectIncludeCompilerPaths() {
  // DQ (4/13/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("In IncludedFilesUnparser::collectIncludeCompilerPaths(): "
           "includingPathsMap.size() = %zu \n",
           includingPathsMap.size());
  }

  for (map<string, set<string>>::const_iterator mapEntry =
           includingPathsMap.begin();
       mapEntry != includingPathsMap.end(); mapEntry++) {
    string fileToUnparse = mapEntry->first;
    // DQ (4/13/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf(" --- In loop over includingPathsMap: fileToUnparse = %s \n",
             fileToUnparse.c_str());
    }
    map<string, string>::const_iterator unparseMapEntry =
        unparseMap.find(fileToUnparse);
    ROSE_ASSERT(unparseMapEntry != unparseMap.end());
    string commonPath = unparseMapEntry->second;
    const set<string> &includingPaths = mapEntry->second;
    for (set<string>::const_iterator includingPathPtr = includingPaths.begin();
         includingPathPtr != includingPaths.end(); includingPathPtr++) {
      string textualPathPart = FileHelper::getTextualPart(*includingPathPtr);
      size_t startPos = commonPath.rfind(textualPathPart);
      ROSE_ASSERT(startPos != string::npos);
      if (startPos != 0) {
        startPos--; // If did not match the whole commonPath, consider that path
                    // delimiter should also be removed
      }
      string includeCompilerPath = commonPath.substr(0, startPos);
      int upFolderCount = FileHelper::countUpsToParentFolder(*includingPathPtr);
      for (int i = 0; i < upFolderCount; i++) {
        includeCompilerPath = FileHelper::concatenatePaths(
            includeCompilerPath, defaultUnparseFolderName);
      }
      addIncludeCompilerPath(
          upFolderCount,
          FileHelper::concatenatePaths(unparseRootPath, includeCompilerPath));
    }
  }

  // DQ (4/13/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("Leaving IncludedFilesUnparser::collectIncludeCompilerPaths(): "
           "includingPathsMap.size() = %zu \n",
           includingPathsMap.size());
  }
}

void IncludedFilesUnparser::addIncludeCompilerPath(
    int upFolderCount, const string &includeCompilerPath) {
  list<pair<int, string>>::iterator includeCompilerPathsIterator;
  // First, check if this include path is already present
  for (includeCompilerPathsIterator = includeCompilerPaths.begin();
       includeCompilerPathsIterator != includeCompilerPaths.end();
       includeCompilerPathsIterator++) {
    if (includeCompilerPath.compare(includeCompilerPathsIterator->second) ==
        0) {
      if (includeCompilerPathsIterator->first >= upFolderCount) {
        return; // This path is present with an equal or greater priority,
                // nothing to do
      } else {
        // This path is present with a lower priority, so remove it and proceed
        // in a regular way.
        includeCompilerPaths.erase(includeCompilerPathsIterator);
        break;
      }
    }
  }

  // If the path is not already present with a sufficiently high priority,
  // insert it at a position corresponding to its priority.
  pair<int, string> newIncludeCompilerPathsEntry(upFolderCount,
                                                 includeCompilerPath);
  includeCompilerPathsIterator = includeCompilerPaths.begin();
  while (includeCompilerPathsIterator != includeCompilerPaths.end()) {
    if (includeCompilerPathsIterator->first <= upFolderCount) {
      includeCompilerPaths.insert(includeCompilerPathsIterator,
                                  newIncludeCompilerPathsEntry);
      break;
    }
    includeCompilerPathsIterator++;
  }

  if (includeCompilerPathsIterator == includeCompilerPaths.end()) {
    // Iterated till the end, which means that the right place to insert was not
    // found, therefore append to the end.
    includeCompilerPaths.push_back(newIncludeCompilerPathsEntry);
  }
}

void IncludedFilesUnparser::updatePreprocessingInfoPaths(
    const string &includedFile, PreprocessingInfo *includingPreprocessingInfo) {
  ASSERT_not_null(includingPreprocessingInfo);

  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("In updatePreprocessingInfoPaths(): includedFile = %s \n",
           includedFile.c_str());
    printf(" --- includingPreprocessingInfo->getString() = %s \n",
           includingPreprocessingInfo->getString().c_str());
  }

  string normalizedIncludingFileName =
      FileHelper::getNormalizedContainingFileName(includingPreprocessingInfo);

  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("In updatePreprocessingInfoPaths(): normalizedIncludingFileName = "
           "%s \n",
           normalizedIncludingFileName.c_str());

    printf("In updatePreprocessingInfoPaths(): filesToUnparse: \n");
    set<string>::const_iterator fileToUnparsePtr = filesToUnparse.begin();
    while (fileToUnparsePtr != filesToUnparse.end()) {
      printf(" --- *fileToUnparsePtr = %s \n", fileToUnparsePtr->c_str());
      fileToUnparsePtr++;
    }
  }

  if (filesToUnparse.find(normalizedIncludingFileName) !=
      filesToUnparse.end()) {
    // update include paths only in the unparsed files

    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf("In updatePreprocessingInfoPaths(): unparseMap: \n");
      map<string, string>::const_iterator unparseMapPtr = unparseMap.begin();
      while (unparseMapPtr != unparseMap.end()) {
        printf(" --- *unparseMapPtr first = %s second = %s \n",
               unparseMapPtr->first.c_str(), unparseMapPtr->second.c_str());
        unparseMapPtr++;
      }
    }
    map<string, string>::const_iterator includedFileUnparseMapEntry =
        unparseMap.find(includedFile);
    string replacementIncludeString;

    if (includedFileUnparseMapEntry != unparseMap.end()) {
      // Included file is unparsed, make the include directive bracketed and
      // relative to the unparse root.
      replacementIncludeString =
          "<" + includedFileUnparseMapEntry->second + ">";
    } else {
      // Included file is not unparsed, make the include directive quoted and
      // relative to the unparsed including file's containing folder. If the
      // included file is copied into the generated header tree, target that
      // copy so transformed headers in the same include graph remain visible.
      string includingFileUnparseFolder;
      map<string, string>::const_iterator includingFileUnparseMapEntry =
          unparseMap.find(normalizedIncludingFileName);
      if (includingFileUnparseMapEntry != unparseMap.end()) {
        ROSE_ASSERT(includingFileUnparseMapEntry != unparseMap.end());
        includingFileUnparseFolder =
            FileHelper::getParentFolder(FileHelper::concatenatePaths(
                unparseRootPath, includingFileUnparseMapEntry->second));
      } else {
        ROSE_ASSERT(isInputFile(normalizedIncludingFileName));
        includingFileUnparseFolder = workingDirectory;
      }

      const string copiedIncludedFilePath =
          getCopiedFileOutputPath(includedFile);
      const string &includedFilePath = copiedIncludedFilePath.empty()
                                           ? includedFile
                                           : copiedIncludedFilePath;

      replacementIncludeString =
          "\"" +
          FileHelper::getRelativePath(includingFileUnparseFolder,
                                      includedFilePath) +
          "\"";
    }

    string includeString = includingPreprocessingInfo->getString();
    const string originalIncludeString = includeString;
    if (SgProject::get_verbose() >= 1) {
      cout << "Original include string:" << includeString << endl;
    }

    IncludeDirective includeDirective(includeString);
    // Replace the original include directive with the new one, using a relative
    // path and brackets.
    includeString.replace(includeDirective.getStartPos() - 1,
                          includeDirective.getIncludedPath().size() + 2,
                          replacementIncludeString);
    if (includeString != originalIncludeString) {
      includeRewrites[includingPreprocessingInfo] = includeString;
      filesWithUpdatedIncludePaths.insert(normalizedIncludingFileName);
    }
    if (SgProject::get_verbose() >= 1) {
      cout << "Planned include string:" << includeString << endl;
    }
  }

  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("Leaving updatePreprocessingInfoPaths(): includedFile = %s \n",
           includedFile.c_str());
  }
}

void IncludedFilesUnparser::populateUnparseMap() {
  auto recordOutput = [this](const string &inputFileName,
                             const string &relativeOutput) {
    const std::filesystem::path normalizedOutput =
        std::filesystem::path(relativeOutput).lexically_normal();
    if (normalizedOutput.is_absolute() ||
        !isParentRelativeToRoot(normalizedOutput)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-output]: input=%s planned output "
              "escapes the unparse root: %s\n",
              inputFileName.c_str(),
              Rose::FileSystem::toString(normalizedOutput).c_str());
      ROSE_ABORT();
    }
    const string normalizedOutputString =
        Rose::FileSystem::toString(normalizedOutput);
    if (unparsePaths.find(normalizedOutputString) != unparsePaths.end()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-output-collision]: input=%s "
              "collides at exact output path %s\n",
              inputFileName.c_str(), normalizedOutputString.c_str());
      ROSE_ABORT();
    }
    const auto insertion =
        unparseMap.emplace(inputFileName, normalizedOutputString);
    if (!insertion.second) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-output]: input=%s received "
              "multiple output plans\n",
              inputFileName.c_str());
      ROSE_ABORT();
    }
    unparsePaths.insert(normalizedOutputString);
  };

  // A basename has no include-edge provenance, so it is valid only for a file
  // named explicitly on the command line. Every included header is placed from
  // its exact frontend-owned include edge below.
  set<string> pendingIncludeRelativePaths;
  for (const string &fileToUnparse : filesToUnparse) {
    if (isInputFile(fileToUnparse)) {
      recordOutput(fileToUnparse,
                   projectInputRelativeOutputPath(projectNode, fileToUnparse));
    } else {
      pendingIncludeRelativePaths.insert(fileToUnparse);
    }
  }

  bool placedIncludeRelativePath = true;
  while (placedIncludeRelativePath && !pendingIncludeRelativePaths.empty()) {
    placedIncludeRelativePath = false;
    for (auto file = pendingIncludeRelativePaths.begin();
         file != pendingIncludeRelativePaths.end();) {
      const string fileToUnparse = *file;
      const ExactIncludedHeaderEdge edge = requireExactIncludedHeaderEdge(
          fileToUnparse, unparseSourceFileMap, "unparse-output");
      const auto parentOutput = unparseMap.find(edge.parentFileName);
      if (parentOutput == unparseMap.end()) {
        ++file;
        continue;
      }

      if (parentOutput->second.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-output-graph]: header=%s "
                "parent=%s has an empty resolved output\n",
                fileToUnparse.c_str(), edge.parentFileName.c_str());
        ROSE_ABORT();
      }
      string parentOutputDirectory =
          Rose::getPathFromFileName(parentOutput->second);
      if (parentOutputDirectory == ".")
        parentOutputDirectory.clear();

      recordOutput(fileToUnparse, makeRelativeOutputPath(parentOutputDirectory,
                                                         edge.includeSpelling));
      file = pendingIncludeRelativePaths.erase(file);
      placedIncludeRelativePath = true;
    }
  }

  if (!pendingIncludeRelativePaths.empty()) {
    const string &unresolvedHeader = *pendingIncludeRelativePaths.begin();
    const ExactIncludedHeaderEdge edge = requireExactIncludedHeaderEdge(
        unresolvedHeader, unparseSourceFileMap, "unparse-output");
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-output-graph]: header=%s parent=%s "
            "has no resolved output; every included header requires a "
            "complete path to one CLI root\n",
            unresolvedHeader.c_str(), edge.parentFileName.c_str());
    ROSE_ABORT();
  }
}

void IncludedFilesUnparser::collectNotUnparsedPreprocessingInfos(
    const string &includedFile, PreprocessingInfo *includingPreprocessingInfo) {
  string normalizedIncludingFileName =
      FileHelper::getNormalizedContainingFileName(includingPreprocessingInfo);
  if (filesToUnparse.find(includedFile) == filesToUnparse.end() &&
      filesToUnparse.find(normalizedIncludingFileName) ==
          filesToUnparse.end()) {
    // If both the included and the including files are NOT unparsed, collect
    // the including PreprocessingInfo.
    notUnparsedPreprocessingInfos.insert(includingPreprocessingInfo);
  }
}

void IncludedFilesUnparser::collectIncludingPathsFromUnaffectedFiles(
    const string &includedFile, PreprocessingInfo *includingPreprocessingInfo) {
  string normalizedIncludingFileName =
      FileHelper::getNormalizedContainingFileName(includingPreprocessingInfo);
  if (filesToUnparse.find(normalizedIncludingFileName) ==
      filesToUnparse.end()) {
    IncludeDirective includeDirective(includingPreprocessingInfo->getString());
    map<string, set<string>>::iterator mapEntry =
        includingPathsMap.find(includedFile);
    if (mapEntry != includingPathsMap.end()) {
      (mapEntry->second).insert(includeDirective.getIncludedPath());
    } else {
      set<string> includingPaths;
      includingPaths.insert(includeDirective.getIncludedPath());
      includingPathsMap.insert(
          pair<string, set<string>>(includedFile, includingPaths));
    }
  }
}

void IncludedFilesUnparser::initializeFilesToUnparse() {

#define DEBUG_INITIALIZER_FILES_TO_UNPARSE 0

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
  printf("In initializeFilesToUnparse(): filesToUnparse.size() = %zu \n",
         filesToUnparse.size());
  printf(" --- modifiedFiles.size()                           = %zu \n",
         modifiedFiles.size());
#endif

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
  if (modifiedFiles.size() > 0) {
    printf("List of modifiedFiles: \n");
    std::set<std::string>::iterator i = modifiedFiles.begin();
    while (i != modifiedFiles.end()) {
      string filename = *i;
      printf(" --- modifiedFiles: %s \n", filename.c_str());
      i++;
    }
  }
#endif

  // DQ (8/20/2019): Collect the comments and CPP directives of the modified
  // header files so that they can be unparsed. SgSourceFile* file = NULL;
  ASSERT_not_null(projectNode);
  SgSourceFile *file =
      requireHeaderPlanningSource(projectNode, "header-project");

  {
    // DQ (4/24/2021): Debugging header file optimization.
    // file->set_header_file_unparsing_optimization_header_file(true);

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
    printf("Perform collection of comments and CPP directives only on the "
           "header files \n");
    printf("####################################################### \n");
    printf("Processing comments and CPP directives for header files \n");
    printf("####################################################### \n");
#endif

    // Iterate over the modified files and collect comments and CPP directives
    // for any header files.
    std::set<SgIncludeFile *> modifiedIncludeFiles;
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
    printf("modifiedFiles.size() = %zu \n", modifiedFiles.size());
    printf("Initializing modifiedIncludeFiles.size() = %zu \n",
           modifiedIncludeFiles.size());
#endif
    // std::map<std::string, SgSourceFile*> unparseSourceFileMap;
    // std::set<std::string> modifiedFiles;
    std::set<std::string>::iterator i = modifiedFiles.begin();

    while (i != modifiedFiles.end()) {
      string filename = *i;
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
      printf("Iterating over modifiedFiles: Calling function to collect "
             "comments and CPP directives from filename = %s \n",
             filename.c_str());
#endif
      if (unparseScopesMap.find(filename) != unparseScopesMap.end()) {
        SgScopeStatement *scope = unparseScopesMap[filename];
        ASSERT_not_null(scope);
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("Found entry in unparseScopesMap: scope = %p = %s \n", scope,
               scope->class_name().c_str());
#endif
      } else {
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("Entry not found in unparseScopesMap \n");
#endif
      }

      SgSourceFile *sourceFile = NULL;
      if (unparseSourceFileMap.find(filename) != unparseSourceFileMap.end()) {
        // SgSourceFile* sourceFile = unparseSourceFileMap[filename];
        sourceFile = unparseSourceFileMap[filename];
        ASSERT_not_null(sourceFile);
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("Found entry in unparseSourceFileMap: sourceFile = %p = %s \n",
               sourceFile, sourceFile->class_name().c_str());
#endif
      } else {
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("Entry not found in unparseSourceFileMap \n");
#endif
      }

      // ASSERT_not_null(sourceFile);

      if (sourceFile != NULL) {
        ASSERT_not_null(sourceFile);
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("sourceFile->getFileName()      = %s \n",
               sourceFile->getFileName().c_str());
        printf("sourceFile->get_isHeaderFile() = %s \n",
               sourceFile->get_isHeaderFile() ? "true" : "false");

        printf("Check if this is a header file: if so add it to the "
               "modifiedIncludeFiles list \n");
#endif
        if (sourceFile->get_isHeaderFile() == true) {
          const ExactIncludedHeaderEdge edge = requireExactIncludedHeaderEdge(
              filename, unparseSourceFileMap, "unparse-output");
          modifiedIncludeFiles.insert(edge.includeFile);
        }

      } else {
        // DQ (10/14/2019): This should be the case of a non-header file
        // (sometimes this is another generated file source file that was
        // modified).
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("sourceFile == NULL \n");
#endif
      }

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
      printf("modifiedIncludeFiles.size() = %zu \n",
             modifiedIncludeFiles.size());
#endif
      i++;
    }

    // DQ (11/16/2019): We only want to unparse header files if they were
    // modified, and if they were modified then they should have had their CPP
    // directives and comments attached before being transformed. So this is too
    // late in the process.  In some cases (such as the transformations in the
    // regression tests) the transforamtions only change the name of a variable,
    // and so attaching teh comments this late is not an issue.  However, in
    // order to know when we will have to unparse a header file we either alwasy
    // unparse them (which is too slow) or we only unparse the header files that
    // will be transformed and then we have to attached the CPP directives and
    // comments before the transformation.  This is the concept of deferred
    // transformations (an option for the outliner).

    // DQ (10/9/2019): We only want to process the header files identified as
    // having been modified.
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
    printf(
        "######################################################################"
        "############################################################### \n");
    printf("Iterate over the modified header files and process them to attach "
           "comments and CPP directives: modifiedIncludeFiles.size() = %zu \n",
           modifiedIncludeFiles.size());
    printf(
        "######################################################################"
        "############################################################### \n");
#endif
    {
      std::set<SgIncludeFile *>::iterator includeFileIterator =
          modifiedIncludeFiles.begin();

      while (includeFileIterator != modifiedIncludeFiles.end()) {
        SgIncludeFile *includeFile = *includeFileIterator;
        ASSERT_not_null(includeFile);

        string filename = includeFile->get_filename();
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("Iterating over modifiedIncludeFiles: Calling function to "
               "collect comments and CPP directives from filename = %s \n",
               filename.c_str());
#endif
        SgSourceFile *sourceFile =
            isSgSourceFile(includeFile->get_source_file());
        ASSERT_not_null(sourceFile);

        if (!sourceFile->get_header_file_unparsing_optimization_header_file()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[header-not-finalized]: header %s "
                  "was not finalized by the frontend\n",
                  sourceFile->getFileName().c_str());
          ROSE_ABORT();
        }
        if (file->get_unparse_tokens() && !sourceFile->get_unparse_tokens()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[header-token-mode]: header %s has "
                  "no frontend token mapping\n",
                  sourceFile->getFileName().c_str());
          ROSE_ABORT();
        }
        if (!sourceFile->get_processedToIncludeCppDirectivesAndComments()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[header-preprocessing]: header %s "
                  "has incomplete preprocessing ownership\n",
                  sourceFile->getFileName().c_str());
          ROSE_ABORT();
        }

        includeFileIterator++;
      }
    }

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
    printf("In initializeFilesToUnparse(): file = %p = %s name = %s Calling "
           "file->set_header_file_unparsing_optimization_header_file(false) \n",
           file, file->class_name().c_str(), file->getFileName().c_str());
#endif
    // DQ (4/24/2021): Debugging header file optimization.
    // DQ (9/19/2019): Unclear to me why we want to set this to false, or if we
    // are doing so for the correct file.
    // file->set_header_file_unparsing_optimization_header_file(false);
  }

  // All modified frontend inputs and included files have to be planned here.
  // Output-only generated translation units own their independent absolute
  // output path and are emitted by the ordinary project-file unparser; they
  // are not roots of the included-header output tree.
  filesToUnparse = modifiedFiles;
  for (SgFile *projectFile : projectNode->get_fileList()) {
    SgSourceFile *sourceFile = isSgSourceFile(projectFile);
    if (sourceFile == nullptr || sourceFile->get_skip_unparse() ||
        sourceFile->get_isCommandLineInputSource()) {
      continue;
    }
    const string outputSourceInput =
        FileHelper::normalizePathIfPossible(sourceFile->getFileName());
    const string outputSourceDestination = FileHelper::normalizePathIfPossible(
        sourceFile->get_unparse_output_filename());
    if (outputSourceInput.empty() || outputSourceDestination.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[generated-source-output]: source=%p "
              "input=%s output=%s has no exact output-only identity\n",
              static_cast<void *>(sourceFile),
              sourceFile->getFileName().c_str(),
              sourceFile->get_unparse_output_filename().c_str());
      ROSE_ABORT();
    }
    if (sourceFile->get_isGeneratedSource() &&
        outputSourceInput != outputSourceDestination) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[generated-source-output]: direct "
              "generated source=%p input=%s output=%s has split identities\n",
              static_cast<void *>(sourceFile), outputSourceInput.c_str(),
              outputSourceDestination.c_str());
      ROSE_ABORT();
    }
    filesToUnparse.erase(outputSourceInput);
  }

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
  printf("In initializeFilesToUnparse(): initialized with modifiedFiles: "
         "filesToUnparse.size() = %zu \n",
         filesToUnparse.size());
#endif

  // All input files are also unparsed by default.
  SgFilePtrList inputFilesList = projectNode->get_fileList();
  for (SgFile *inputFile : inputFilesList) {
    SgSourceFile *sourceFile = isSgSourceFile(inputFile);
    if (sourceFile == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-project]: project file=%p/%s is "
              "not a source translation unit\n",
              static_cast<void *>(inputFile), inputFile->class_name().c_str());
      ROSE_ABORT();
    }
    if (!sourceFile->get_skip_unparse() &&
        sourceFile->get_unparseHeaderFiles() &&
        sourceFile->get_isCommandLineInputSource()) {
      filesToUnparse.insert(FileHelper::normalizePath(
          sourceFile->getFileName())); // normalize just in case it is not
                                       // normalized by default as expected
    }
  }

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
  printf("Leaving initializeFilesToUnparse(): filesToUnparse.size() = %zu \n",
         filesToUnparse.size());
#endif
}

void IncludedFilesUnparser::collectAdditionalFilesToUnparse() {
  // Recursively add to filesToUnparse set any file that includes using quotes
  // (or an absolute path) at least one of the files that is already in
  // filesToUnparse set.
  set<string> workingSet = filesToUnparse;

  while (!workingSet.empty()) {
    newFilesToUnparse.clear();

    // DQ (4/14/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf("In IncludedFilesUnparser::collectAdditionalFilesToUnparse(): "
             "calling applyFunctionToIncludingPreprocessingInfos(workingSet) "
             "workingSet.size() = %zu \n",
             workingSet.size());
    }

    applyFunctionToIncludingPreprocessingInfos(
        workingSet, &IncludedFilesUnparser::collectNewFilesToUnparse);
    workingSet = newFilesToUnparse;
  }

  // Every included output is positioned relative to its exact canonical
  // parent. Select that complete parent chain now; otherwise populateUnparseMap
  // would receive a well-formed child without the output context required to
  // place it. Preprocessing records can describe additional lexical include
  // occurrences, but they are not a substitute for this structural edge.
  workingSet = filesToUnparse;
  set<string> visited;
  while (!workingSet.empty()) {
    const string fileToUnparse = *workingSet.begin();
    workingSet.erase(workingSet.begin());
    if (!visited.insert(fileToUnparse).second || isInputFile(fileToUnparse))
      continue;

    const ExactIncludedHeaderEdge edge = requireExactIncludedHeaderEdge(
        fileToUnparse, unparseSourceFileMap, "unparse-output");
    if (filesToUnparse.insert(edge.parentFileName).second)
      workingSet.insert(edge.parentFileName);
  }
}

void IncludedFilesUnparser::collectNewFilesToUnparse(
    const string &includedFile, PreprocessingInfo *includingPreprocessingInfo) {

  IncludeDirective includeDirective(includingPreprocessingInfo->getString());
  if (includeDirective.isQuotedInclude() ||
      FileHelper::isAbsolutePath(includeDirective.getIncludedPath())) {
    string normalizedIncludingFileName =
        FileHelper::getNormalizedContainingFileName(includingPreprocessingInfo);
    if (filesToUnparse.find(normalizedIncludingFileName) ==
        filesToUnparse.end()) {
      filesToUnparse.insert(normalizedIncludingFileName);
      newFilesToUnparse.insert(normalizedIncludingFileName);
    }
  }
}

void IncludedFilesUnparser::collectAdditionalListOfHeaderFilesToCopy() {
  // Snapshot-copy selection follows the same exact frontend include graph used
  // by output planning. Directory co-location and a second scan of attached
  // preprocessing strings are not ownership edges and can both omit reachable
  // headers and copy unrelated files.
  filesToCopy.clear();
  vector<SgIncludeFile *> worklist;
  const SgFilePtrList &inputFiles = projectNode->get_fileList();
  for (size_t index = 0; index < inputFiles.size(); ++index) {
    SgSourceFile *inputFile = isSgSourceFile(inputFiles[index]);
    if (inputFile == nullptr) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[header-project]: project input %zu type=%s "
          "cannot own a source include graph\n",
          index,
          inputFiles[index] != nullptr ? inputFiles[index]->class_name().c_str()
                                       : "<null>");
      ROSE_ABORT();
    }
    if (inputFile->get_skip_unparse() || !inputFile->get_unparseHeaderFiles()) {
      continue;
    }
    SgIncludeFile *includeRoot = inputFile->get_associated_include_file();
    if (includeRoot == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-include-graph]: CLI input=%s has "
              "no include-tree root\n",
              inputFile->getFileName().c_str());
      ROSE_ABORT();
    }
    worklist.push_back(includeRoot);
  }

  set<SgIncludeFile *> visited;
  while (!worklist.empty()) {
    SgIncludeFile *includingFile = worklist.back();
    worklist.pop_back();
    if (includingFile == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-include-graph]: copy traversal "
              "contains a null include node\n");
      ROSE_ABORT();
    }
    if (!visited.insert(includingFile).second)
      continue;

    for (SgIncludeFile *includedFile : includingFile->get_include_file_list()) {
      if (includedFile == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-include-graph]: copy parent=%s "
                "contains a null child include node\n",
                includingFile->get_filename().str());
        ROSE_ABORT();
      }
      worklist.push_back(includedFile);

      if (!includedFile->get_isApplicationFile())
        continue;

      const string includedFileName =
          FileHelper::normalizePathIfPossible(includedFile->get_filename());
      if (includedFileName.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-include-graph]: copy child=%p "
                "has no normalized filename\n",
                static_cast<void *>(includedFile));
        ROSE_ABORT();
      }
      if (FileHelper::getFileName(includedFileName) ==
          "rose_required_macros_and_functions.h")
        continue;
      if (filesToUnparse.find(includedFileName) == filesToUnparse.end())
        filesToCopy.insert(includedFileName);
    }
  }
}

void IncludedFilesUnparser::applyFunctionToIncludingPreprocessingInfos(
    const set<string> &includedFiles,
    void (IncludedFilesUnparser::*funPtr)(
        const string &includedFile,
        PreprocessingInfo *includingPreprocessingInfo)) {

  for (set<string>::const_iterator includedFile = includedFiles.begin();
       includedFile != includedFiles.end(); includedFile++) {
    const map<string, set<PreprocessingInfo *>>
        &includingPreprocessingInfosMap = includingPreprocessingInfos;
    map<string, set<PreprocessingInfo *>>::const_iterator mapEntry =
        includingPreprocessingInfosMap.find(*includedFile);
    if (mapEntry != includingPreprocessingInfosMap.end()) {
      // includedFile is really included, so look for all its including
      // preprocessing infos.
      const set<PreprocessingInfo *> &includingPreprocessingInfos =
          mapEntry->second;
      for (set<PreprocessingInfo *>::const_iterator
               includingPreprocessingInfoPtr =
                   includingPreprocessingInfos.begin();
           includingPreprocessingInfoPtr != includingPreprocessingInfos.end();
           includingPreprocessingInfoPtr++) {
        if (*includingPreprocessingInfoPtr == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[include-rewrite]: included=%s has a "
                  "null preprocessing-info owner\n",
                  includedFile->c_str());
          ROSE_ABORT();
        }
        (this->*funPtr)(*includedFile, *includingPreprocessingInfoPtr);
      }
    }
  }
}

void IncludedFilesUnparser::validateUnparseScopeCandidates() {
  set<string> candidateFiles;
  for (const auto &entry : unparseScopeCandidatesMap)
    candidateFiles.insert(entry.first);
  for (const auto &entry : preprocessingUnparseScopeCandidatesMap)
    candidateFiles.insert(entry.first);

  for (const string &fileName : candidateFiles) {
    set<unsigned int> occurrenceIds;
    const auto structuralFile = unparseScopeCandidatesMap.find(fileName);
    if (structuralFile != unparseScopeCandidatesMap.end()) {
      for (const auto &entry : structuralFile->second)
        occurrenceIds.insert(entry.first);
    }
    const auto preprocessingFile =
        preprocessingUnparseScopeCandidatesMap.find(fileName);
    if (preprocessingFile != preprocessingUnparseScopeCandidatesMap.end()) {
      for (const auto &entry : preprocessingFile->second)
        occurrenceIds.insert(entry.first);
    }
    if (occurrenceIds.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-context]: file=%s has no exact "
              "lexical occurrence\n",
              fileName.c_str());
      ROSE_ABORT();
    }

    const auto exactEmissionScope =
        [&](unsigned int occurrence) -> SgScopeStatement * {
      const set<SgScopeStatement *> *candidates = nullptr;
      const map<SgScopeStatement *, SgNode *> *candidateOrigins = nullptr;
      const char *candidateKind = nullptr;
      if (structuralFile != unparseScopeCandidatesMap.end()) {
        const auto structural = structuralFile->second.find(occurrence);
        if (structural != structuralFile->second.end() &&
            !structural->second.empty()) {
          candidates = &structural->second;
          candidateOrigins =
              &unparseScopeCandidateOriginsMap.at(fileName).at(occurrence);
          candidateKind = "structural";
        }
      }
      if (candidates == nullptr &&
          preprocessingFile != preprocessingUnparseScopeCandidatesMap.end()) {
        const auto preprocessing = preprocessingFile->second.find(occurrence);
        if (preprocessing != preprocessingFile->second.end() &&
            !preprocessing->second.empty()) {
          candidates = &preprocessing->second;
          candidateOrigins =
              &preprocessingUnparseScopeCandidateOriginsMap.at(fileName).at(
                  occurrence);
          candidateKind = "preprocessing-only";
        }
      }
      if (occurrence == 0 || candidates == nullptr || candidates->size() != 1) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-context]: file=%s "
                "occurrence=%u has %zu %s emission scopes\n",
                fileName.c_str(), occurrence,
                candidates != nullptr ? candidates->size() : 0,
                candidateKind != nullptr ? candidateKind : "exact");
        if (candidates != nullptr) {
          for (SgScopeStatement *candidate : *candidates) {
            Sg_File_Info *candidateInfo =
                candidate != nullptr ? candidate->get_file_info() : nullptr;
            const auto origin =
                candidateOrigins != nullptr
                    ? candidateOrigins->find(candidate)
                    : map<SgScopeStatement *, SgNode *>::const_iterator{};
            SgNode *originNode =
                candidateOrigins != nullptr && origin != candidateOrigins->end()
                    ? origin->second
                    : nullptr;
            SgLocatedNode *locatedOrigin = isSgLocatedNode(originNode);
            Sg_File_Info *originInfo = locatedOrigin != nullptr
                                           ? locatedOrigin->get_file_info()
                                           : nullptr;
            fprintf(
                stderr,
                "  REX_UNPARSE_CONTEXT scope=%p/%s parent=%p/%s "
                "physical=%d occurrence=%u scope-source=%s:%d:%d "
                "origin=%p/%s name=%s source=%s:%d:%d\n",
                static_cast<void *>(candidate),
                candidate != nullptr ? candidate->class_name().c_str()
                                     : "<null>",
                static_cast<void *>(
                    candidate != nullptr ? candidate->get_parent() : nullptr),
                candidate != nullptr && candidate->get_parent() != nullptr
                    ? candidate->get_parent()->class_name().c_str()
                    : "<null>",
                candidateInfo != nullptr ? candidateInfo->get_physical_file_id()
                                         : -999,
                candidateInfo != nullptr
                    ? candidateInfo->get_physical_file_occurrence_id()
                    : 0,
                candidateInfo != nullptr
                    ? candidateInfo->get_filenameString().c_str()
                    : "<null>",
                candidateInfo != nullptr ? candidateInfo->get_line() : 0,
                candidateInfo != nullptr ? candidateInfo->get_col() : 0,
                static_cast<void *>(originNode),
                originNode != nullptr ? originNode->class_name().c_str()
                                      : "<null>",
                originNode != nullptr
                    ? SageInterface::get_name(originNode).c_str()
                    : "<null>",
                originInfo != nullptr ? originInfo->get_filenameString().c_str()
                                      : "<null>",
                originInfo != nullptr ? originInfo->get_line() : 0,
                originInfo != nullptr ? originInfo->get_col() : 0);
          }
        }
        ROSE_ABORT();
      }
      return *candidates->begin();
    };

    const unsigned int canonicalOccurrence = *occurrenceIds.begin();
    SgScopeStatement *canonicalScope = exactEmissionScope(canonicalOccurrence);
    ASSERT_not_null(canonicalScope);
    if (modifiedFiles.count(fileName) != 0 && occurrenceIds.size() > 1) {
      const HeaderOccurrenceContract canonicalContract =
          buildHeaderOccurrenceContract(canonicalScope, fileName,
                                        canonicalOccurrence, includeRewrites);
      for (unsigned int occurrence : occurrenceIds) {
        if (occurrence == canonicalOccurrence) {
          continue;
        }
        SgScopeStatement *candidateScope = exactEmissionScope(occurrence);
        const HeaderOccurrenceContract candidateContract =
            buildHeaderOccurrenceContract(candidateScope, fileName, occurrence,
                                          includeRewrites);
        if (candidateContract == canonicalContract) {
          continue;
        }
        size_t structuralMismatch = 0;
        const size_t sharedStructural =
            min(canonicalContract.structuralSurfaces.size(),
                candidateContract.structuralSurfaces.size());
        while (structuralMismatch < sharedStructural &&
               canonicalContract.structuralSurfaces[structuralMismatch] ==
                   candidateContract.structuralSurfaces[structuralMismatch]) {
          ++structuralMismatch;
        }
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-context]: modified file=%s has "
                "non-equivalent typed lexical occurrences %u in scope %p (%s) "
                "and %u in scope %p (%s); structural-counts=%zu/%zu "
                "preprocessing-counts=%zu/%zu\n",
                fileName.c_str(), canonicalOccurrence,
                static_cast<void *>(canonicalScope),
                canonicalScope->class_name().c_str(), occurrence,
                static_cast<void *>(candidateScope),
                candidateScope->class_name().c_str(),
                canonicalContract.structuralSurfaces.size(),
                candidateContract.structuralSurfaces.size(),
                canonicalContract.preprocessingSurfaces.size(),
                candidateContract.preprocessingSurfaces.size());
        if (structuralMismatch < sharedStructural) {
          const string &canonical =
              canonicalContract.structuralSurfaces[structuralMismatch];
          const string &candidate =
              candidateContract.structuralSurfaces[structuralMismatch];
          size_t byte = 0;
          while (byte < min(canonical.size(), candidate.size()) &&
                 canonical[byte] == candidate[byte]) {
            ++byte;
          }
          const size_t contextBegin = byte > 100 ? byte - 100 : 0;
          fprintf(stderr,
                  "  REX_UNPARSE_CONTEXT structural-index=%zu byte=%zu "
                  "canonical=%s\n",
                  structuralMismatch, byte,
                  canonical.substr(contextBegin, 240).c_str());
          fprintf(stderr,
                  "  REX_UNPARSE_CONTEXT structural-index=%zu byte=%zu "
                  "candidate=%s\n",
                  structuralMismatch, byte,
                  candidate.substr(contextBegin, 240).c_str());
        }
        ROSE_ABORT();
      }
    }
    unparseScopesMap[fileName] = canonicalScope;
    unparseOccurrenceMap[fileName] = canonicalOccurrence;
  }
}
void IncludedFilesUnparser::addToUnparseScopesMap(const string &fileName,
                                                  SgNode *startNode,
                                                  unsigned int occurrence,
                                                  bool preprocessingOnly) {
  if (fileName.empty() || startNode == NULL || occurrence == 0) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-scope]: file=%s start-node=%p "
            "occurrence=%u cannot determine header emission scope\n",
            fileName.c_str(), static_cast<void *>(startNode), occurrence);
    ROSE_ABORT();
  }
  // We need to find the innermost enclosing scope that is from the including
  // file (i.e. from a file that is different from this node's file) such that
  // we unparse the whole included file, not just the scope containing the
  // modified stuff.
  SgNode *enclosingScope =
      SageInterface::getEnclosingNode<SgScopeStatement>(startNode, false);
  while (enclosingScope != NULL) {
    // An unbraced declaration controlled by if/for/while needs a semantic
    // SgBasicBlock scope, but that typed wrapper owns no source syntax.  Header
    // emission belongs to the first enclosing source-backed scope, just as the
    // statement unparser emits only the wrapper's sole payload.  Validate the
    // exact transparent role before crossing it; an ordinary source block must
    // remain an emission boundary.
    if (isExactlyImplicitControlFlowStructuralNode(
            isSgStatement(enclosingScope))) {
      enclosingScope = SageInterface::getEnclosingNode<SgScopeStatement>(
          enclosingScope, false);
      continue;
    }
    Sg_File_Info *scopeInfo = enclosingScope->get_file_info();
    if (scopeInfo == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-scope]: file=%s scope=%s has no "
              "source information\n",
              fileName.c_str(), enclosingScope->class_name().c_str());
      ROSE_ABORT();
    }
    if (fileName.compare(
            FileHelper::normalizePath(scopeInfo->get_filenameString())) != 0 ||
        scopeInfo->get_physical_file_occurrence_id() != occurrence) {
      break;
    }
    enclosingScope = SageInterface::getEnclosingNode<SgScopeStatement>(
        enclosingScope, false);
  }

  if (enclosingScope == NULL) {
    if (preprocessingOnly && isSgGlobal(startNode) != nullptr) {
      enclosingScope = startNode;
    } else {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-scope]: file=%s node=%s has no "
              "including scope\n",
              fileName.c_str(), startNode->class_name().c_str());
      ROSE_ABORT();
    }
  }

  SgScopeStatement *emissionScope = isSgScopeStatement(enclosingScope);
  ASSERT_not_null(emissionScope);
  auto &candidateMap = preprocessingOnly
                           ? preprocessingUnparseScopeCandidatesMap
                           : unparseScopeCandidatesMap;
  candidateMap[fileName][occurrence].insert(emissionScope);
  auto &candidateOriginsMap = preprocessingOnly
                                  ? preprocessingUnparseScopeCandidateOriginsMap
                                  : unparseScopeCandidateOriginsMap;
  candidateOriginsMap[fileName][occurrence].emplace(emissionScope, startNode);

  if (SgProject::get_verbose() >= 1) {
    cout << "Enclosing node:" << enclosingScope->class_name() << endl;
    cout << "Enclosing node's file:"
         << enclosingScope->get_file_info()->get_filenameString() << endl;
  }
}

void IncludedFilesUnparser::visit(SgNode *node) {

#define DEBUG_INCLUDE_FILE_UNPARSER_VISIT 0

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
  printf("In IncludedFilesUnparser::visit(): node = %p = %s = %s isModified = "
         "%s \n",
         node, node->class_name().c_str(),
         SageInterface::get_name(node).c_str(),
         node->get_isModified() ? "true" : "false");
  SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(node);
  if (functionDeclaration != NULL) {
    printf(" --- functionDeclaration name = %s \n",
           functionDeclaration->get_name().str());
    printf(" --- functionDeclaration->get_definition() = %p \n",
           functionDeclaration->get_definition());
  }
#endif

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
  if (isSgGlobal(node) != NULL) {
    printf("In IncludedFilesUnparser::visit(): (SgGlobal): node = %p = %s = %s "
           "isModified = %s \n",
           node, node->class_name().c_str(),
           SageInterface::get_name(node).c_str(),
           node->get_isModified() ? "true" : "false");
  }
#endif

  SgStatement *statement = isSgStatement(node);
  const bool isTypedZeroWidth =
      statement != nullptr &&
      isTypedZeroWidthSourceOwner(statement, "header-plan-zero-width");
  const bool isStatement = statement != nullptr &&
                           !isTypedNonSurfaceStatement(statement) &&
                           !isTypedZeroWidth;

  SgSourceFile *sourceFile = isSgSourceFile(node);
  if (sourceFile != NULL) {
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
    printf("Building unparseSgSourceFileMap: sourceFile = %p "
           "sourceFile->getFileName() = %s \n",
           sourceFile, sourceFile->getFileName().c_str());
#endif
    insertMaterializedSourceFile(unparseSourceFileMap, sourceFile,
                                 "project-traversal");

    // Save the header file report.
    SgHeaderFileReport *reportData = sourceFile->get_headerFileReport();

    if (reportData != NULL) {
      printf("####################################################### \n");
      printf("####################################################### \n");
      reportData->display("headerFileReport in IncludedFilesUnparser::visit()");
      printf("####################################################### \n");
      printf("####################################################### \n");
    } else {
    }

    // The translation-unit source is emitted by unparseFileList and is not an
    // included-file scope. Header wrappers are indexed above through their
    // SgIncludeFile association and token-map inventory.
    return;
  }

  SgIncludeFile *includeFile = isSgIncludeFile(node);
  if (includeFile != NULL) {
    SgSourceFile *headerFile = isSgSourceFile(includeFile->get_source_file());
    if (headerFile != NULL) {
      insertMaterializedSourceFile(unparseSourceFileMap, headerFile,
                                   "include-node");
    }
  }

  // DQ (9/7/2018): Looking for connections to the SgSourceFile in the
  // SgIncludeDirectiveStatement. We could just build up a map of filenames to
  // SgSourceFile IR nodes and then use it with the modifiedFiles set.
  SgIncludeDirectiveStatement *includeDirectiveStatement =
      isSgIncludeDirectiveStatement(node);
  if (includeDirectiveStatement != NULL) {
    SgHeaderFileBody *headerFileBody =
        includeDirectiveStatement->get_headerFileBody();

    // DQ (5/19/2020): In the new design, allow the headerFileBody to be NULL.
    // Include directives are now added uniformally to the AST, as part of
    // supporting unparsing of arbitrary subsets of header files.
    // ROSE_ASSERT(headerFileBody != NULL);
    if (headerFileBody != NULL) {
      SgSourceFile *headerFile = headerFileBody->get_include_file();

      // DQ (11/22/2018): We only build associated SgSourceFile for application
      // header files. ROSE_ASSERT(headerFile != NULL);
      if (headerFile != NULL) {
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
        printf("Building unparseSgSourceFileMap: headerFile = %p "
               "headerFile->getFileName() = %s \n",
               headerFile, headerFile->getFileName().c_str());
#endif
        insertMaterializedSourceFile(unparseSourceFileMap, headerFile,
                                     "header-body");
      } else {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-body]: include directive %p has "
                "a header body without its source-file owner\n",
                static_cast<void *>(includeDirectiveStatement));
        ROSE_ABORT();
      }
    } else {
      if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
        // Header files may not have a materialized AST body (e.g.,
        // when the frontend doesn't build SgHeaderFileBody instances
        // for a directive).
        printf("NOTE: In IncludedFilesUnparser::visit(): headerFileBody "
               "== NULL: "
               "includeDirectiveStatement->get_directiveString() = %s \n",
               includeDirectiveStatement->get_directiveString().c_str());
      }
    }
  }

  Sg_File_Info *fileInfo = node->get_file_info();

  if (isSgLocatedNode(node) != nullptr && fileInfo == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[source-position]: located node=%s "
            "address=%p has no file information\n",
            node->class_name().c_str(), static_cast<void *>(node));
    ROSE_ABORT();
  }

  if (fileInfo != NULL) {
    // DQ (11/28/2018): Need to use the full filename (perhaps resolved of
    // symbolic links) because filename that match can represent files in
    // different directories.  Though this is more of an issue for system header
    // file. string normalizedFileName = FileHelper::normalizePath(fileInfo ->
    // get_filenameString()); string normalizedFileName =
    // FileHelper::normalizePath(fileInfo->get_physical_filename());
    int physical_file_id = fileInfo->get_physical_file_id();
    SgSourceFile *owningSourceFile =
        SageInterface::getEnclosingSourceFile(node, true);
    Sg_File_Info *owningSourceInfo = owningSourceFile != nullptr
                                         ? owningSourceFile->get_file_info()
                                         : nullptr;
    if (owningSourceFile != nullptr && owningSourceInfo == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-plan-policy]: source=%p has no "
              "exact primary physical identity\n",
              static_cast<void *>(owningSourceFile));
      ROSE_ABORT();
    }
    if (owningSourceFile != nullptr &&
        !owningSourceFile->get_unparseHeaderFiles() &&
        owningSourceInfo != nullptr &&
        physical_file_id != owningSourceInfo->get_physical_file_id()) {
      // This source file explicitly excludes copied header output.  Its
      // primary output remains traversable, while source-backed header nodes
      // are dependencies only and are not candidates for the project's header
      // emission plan.
      return;
    }

    const bool isTransformation = fileInfo->isTransformation();
    const bool isCompilerGenerated = fileInfo->isCompilerGenerated();
    const bool isModified = node->get_isModified();
    const bool isOutputSurface = fileInfo->isOutputInCodeGeneration();
    SgLocatedNode *locatedNode = isSgLocatedNode(node);
    if (locatedNode != nullptr &&
        isExactlyAuxiliaryOwned(locatedNode, "header-plan")) {
      return;
    }
    const bool hasPreprocessing =
        locatedNode != nullptr &&
        locatedNode->getAttachedPreprocessingInfo() != nullptr &&
        !locatedNode->getAttachedPreprocessingInfo()->empty();
    if (hasPreprocessing) {
      for (PreprocessingInfo *record :
           *locatedNode->getAttachedPreprocessingInfo()) {
        if (record == nullptr || record->get_file_info() == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[header-scope]: owner=%p/%s has an "
                  "incomplete preprocessing record\n",
                  static_cast<void *>(locatedNode),
                  locatedNode->class_name().c_str());
          ROSE_ABORT();
        }
        Sg_File_Info *recordInfo = record->get_file_info();
        if (recordInfo->get_physical_file_id() < 0) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[header-scope]: record=%p owner=%p/%s "
                  "has no physical file identity\n",
                  static_cast<void *>(record), static_cast<void *>(locatedNode),
                  locatedNode->class_name().c_str());
          ROSE_ABORT();
        }
        const string recordFileName = FileHelper::normalizePathIfPossible(
            recordInfo->getFilenameFromID(recordInfo->get_physical_file_id()));
        const unsigned int recordOccurrence =
            recordInfo->get_physical_file_occurrence_id();
        const bool recordFromPrimaryInput = isExactPrimaryPhysicalFile(
            projectNode, recordInfo->get_physical_file_id(), recordFileName,
            "header-scope-record-primary");
        const bool attachedGeneratedPrimaryOutput =
            recordFromPrimaryInput && record->isTransformation() &&
            recordInfo->isOutputInCodeGeneration() &&
            recordInfo->get_physical_file_id() == physical_file_id &&
            (record->getOutputPlacement() ==
                 PreprocessingInfo::attached_output_boundary ||
             record->getOutputPlacement() ==
                 PreprocessingInfo::attached_output_trailing_line);
        if (recordFileName.empty() ||
            (recordOccurrence == 0 && !attachedGeneratedPrimaryOutput)) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[header-scope]: record=%p owner=%p/%s "
                  "file=%s occurrence=%u raw=%s:%d:%d type=%d placement=%d "
                  "relative=%d text=%s owner-file=%s/%d "
                  "owner-occurrence=%u has incomplete lexical provenance\n",
                  static_cast<void *>(record), static_cast<void *>(locatedNode),
                  locatedNode->class_name().c_str(), recordFileName.c_str(),
                  recordOccurrence, recordInfo->get_raw_filename().c_str(),
                  recordInfo->get_raw_line(), recordInfo->get_raw_col(),
                  static_cast<int>(record->getTypeOfDirective()),
                  static_cast<int>(record->getOutputPlacement()),
                  static_cast<int>(record->getRelativePosition()),
                  record->getString().c_str(),
                  fileInfo->get_physical_filename().c_str(), physical_file_id,
                  fileInfo->get_physical_file_occurrence_id());
          ROSE_ABORT();
        }
        allFiles.insert(recordFileName);
        if (!recordFromPrimaryInput) {
          addToUnparseScopesMap(recordFileName, locatedNode, recordOccurrence,
                                true);
        }
      }
    }
    const bool requiresPhysicalPlanning =
        isStatement ||
        (!isTypedZeroWidth && (isModified || isTransformation)) ||
        hasPreprocessing;
    if (!requiresPhysicalPlanning) {
      return;
    }
    if (!isOutputSurface &&
        (isStatement ||
         (!isTypedZeroWidth && (isModified || isTransformation)))) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-plan]: lexical node=%s "
              "uses the legacy non-output suppression bit\n",
              node->class_name().c_str());
      ROSE_ABORT();
    }
    if (physical_file_id < 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[physical-file]: node=%s address=%p "
              "name=%s file-id=%d physical-file-id=%d compiler-generated=%d "
              "transformation=%d modified=%d has no explicit physical source "
              "ownership\n",
              node->class_name().c_str(), static_cast<void *>(node),
              SageInterface::get_name(node).c_str(), fileInfo->get_file_id(),
              physical_file_id, isCompilerGenerated ? 1 : 0,
              isTransformation ? 1 : 0, isModified ? 1 : 0);
      for (SgNode *ancestor = node; ancestor != nullptr;
           ancestor = ancestor->get_parent()) {
        SgLocatedNode *locatedAncestor = isSgLocatedNode(ancestor);
        Sg_File_Info *ancestorInfo = locatedAncestor != nullptr
                                         ? locatedAncestor->get_file_info()
                                         : nullptr;
        fprintf(stderr,
                "  REX_UNPARSE_CONTEXT node=%p type=%s source=%s:%d "
                "compiler-generated=%d frontend-specific=%d output=%d\n",
                static_cast<void *>(ancestor), ancestor->class_name().c_str(),
                ancestorInfo != nullptr
                    ? ancestorInfo->get_filenameString().c_str()
                    : "<none>",
                ancestorInfo != nullptr ? ancestorInfo->get_line() : 0,
                ancestorInfo != nullptr && ancestorInfo->isCompilerGenerated(),
                ancestorInfo != nullptr && ancestorInfo->isFrontendSpecific(),
                ancestorInfo != nullptr &&
                    ancestorInfo->isOutputInCodeGeneration());
      }
      ROSE_ABORT();
    }

    // string normalizedFileName =
    // FileHelper::normalizePath(fileInfo->getFilenameFromID(physical_file_id));
    string normalizedFileName = FileHelper::normalizePathIfPossible(
        fileInfo->getFilenameFromID(physical_file_id));

    if (physical_file_id >= 0 &&
        (normalizedFileName.empty() || normalizedFileName == "NULL_FILE")) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[physical-file]: node=%s address=%p "
              "physical-file-id=%d has no registered filename\n",
              node->class_name().c_str(), static_cast<void *>(node),
              physical_file_id);
      ROSE_ABORT();
    }

    // DQ (10/14/2019): Trap cases where the normalizedFileName is not a valid
    // filename.
    if (normalizedFileName == "transformation") {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[physical-file]: node=%s address=%p "
              "uses the synthetic transformation filename instead of an "
              "explicit source owner\n",
              node->class_name().c_str(), static_cast<void *>(node));
      ROSE_ABORT();
    }

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
    bool isShared = fileInfo->isShared();
    if (isStatement == true) {
      printf("In IncludedFilesUnparser::visit(): physical_file_id             "
             "= %d \n",
             physical_file_id);
      printf("In IncludedFilesUnparser::visit(): physical fileName (computed) "
             "= %s \n",
             fileInfo->get_physical_filename().c_str());
      printf("In IncludedFilesUnparser::visit(): physical fileName (raw)      "
             "= %s \n",
             fileInfo->getFilenameFromID(physical_file_id).c_str());
      printf("In IncludedFilesUnparser::visit(): normalizedFileName           "
             "= %s \n",
             normalizedFileName.c_str());
      printf("In IncludedFilesUnparser::visit(): isTransformation             "
             "= %s \n",
             isTransformation ? "true" : "false");
      printf("In IncludedFilesUnparser::visit(): isCompilerGenerated          "
             "= %s \n",
             isCompilerGenerated ? "true" : "false");
      printf("In IncludedFilesUnparser::visit(): isModified                   "
             "= %s \n",
             isModified ? "true" : "false");
      printf("In IncludedFilesUnparser::visit(): isShared                     "
             "= %s \n",
             isShared ? "true" : "false");
    }
#endif

    const bool isPreprocessingSurface =
        locatedNode != nullptr && locatedNodeOwnsPhysicalFilePreprocessing(
                                      locatedNode, normalizedFileName);
    if (isOutputSurface || isPreprocessingSurface) {
      // avoid infos that do not have real file names

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
      if (isStatement == true) {
        printf("fileInfo->get_file_id() = %d \n", fileInfo->get_file_id());
        printf("fileInfo->get_physical_file_id() = %d \n",
               fileInfo->get_physical_file_id());
      }
#endif
      ROSE_ASSERT(physical_file_id >= 0);
      {
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
        if (isStatement == true) {
          printf(
              "In IncludedFilesUnparser::visit(): !isTransformation && "
              "!isCompilerGenerated: node = %p = %s normalizedFileName = %s \n",
              node, node->class_name().c_str(), normalizedFileName.c_str());
        }
#endif
        set<string>::const_iterator setEntry =
            allFiles.find(normalizedFileName);
        ASSERT_not_null(projectNode);
        const bool isPrimaryInputFile = isExactPrimaryPhysicalFile(
            projectNode, physical_file_id, normalizedFileName,
            "header-plan-primary");
        if (setEntry == allFiles.end()) {
          // This is a new file, process it.
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
          printf("In IncludedFilesUnparser::visit(): !isTransformation && "
                 "!isCompilerGenerated: This is a new file, process it: file = "
                 "%s \n",
                 normalizedFileName.c_str());
#endif
          allFiles.insert(normalizedFileName);

          // DQ (9/5/2018): Comment added.
          // We can't just set or use this on the SgSourceFile, since there is
          // only one file object for a translation unit. We need to add a file
          // name to use in the selection of statements to be unparsed, and then
          // the unparsing needs to be directed to that file (and back again,
          // changing whatever is defined to be the current file). This might
          // cause a large number of files to be open at the same time, in the
          // worst case, but that will be fine for now. get_unparse_tokens()

        } else {
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
          printf("This is NOT a new file: normalizedFileName = %s \n",
                 normalizedFileName.c_str());
#endif
        }
        if (!isPrimaryInputFile) {
          const unsigned int occurrence =
              fileInfo->get_physical_file_occurrence_id();
          addToUnparseScopesMap(normalizedFileName, node, occurrence, false);
        }
      }
    }

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
    if (isStatement == true) {
      printf("List allFiles list (size = %zu): \n", allFiles.size());
      set<string>::iterator i = allFiles.begin();
      size_t counter = 0;
      while (i != allFiles.end()) {
        printf("   --- allFiles[%zu] = %s \n", counter, (*i).c_str());

        i++;
        counter++;
      }
    }
#endif

    // DQ (6/8/2019): Ignore SgSourceFile and SgProject IR nodes.
    // DQ (6/6/2019): I think that we want to handle statements that are either
    // marked isModified or isTransformation. NOTE: to support the header file
    // unparsing the associated physical file where the target transformation is
    // considered to live must be specified. if (node -> get_isModified()) if
    // (isModified == true) if (isModified == true || ( (isTransformation ==
    // true) && (normalizedFileName != "transformation") ) )
    if (isModified == true || ((isTransformation == true) &&
                               (normalizedFileName != "transformation"))) {
      // DQ (6/6/2019): Modified statements are important for the token-based
      // unparsing, since they trigger the switch from using the token stream
      // for unparsing to using the AST for the unparsing.  However, the
      // modified flag is not important if we are not using the token based
      // unparsing.  When we are not using the token-based unparsing (or when we
      // are unparsing directly from the AST) the status of the isTransformation
      // flag is all that is important.

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
      printf("In IncludedFilesUnparser::visit(): node -> get_isModified(): "
             "node = %p = %s  \n",
             node, node->class_name().c_str());
#endif
      if (SgProject::get_verbose() > 2) {
        cout << "Found a modified node: " << node->class_name() << endl;
        cout << "   In file: " << normalizedFileName << endl;
        cout << "   Is transformation: " << isTransformation << endl;
        cout << "   Is compiler generated: " << isCompilerGenerated << endl;
      }
      if (isModified || isTransformation) {
        // avoid infos that do not have real file names
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
        // printf ("In IncludedFilesUnparser::visit(): node -> get_isModified():
        // !isTransformation && !isCompilerGenerated: normalizedFileName = %s
        // \n",normalizedFileName.c_str());
        printf("In IncludedFilesUnparser::visit(): node -> get_isModified(): "
               "(isTransformation == true && isCompilerGenerated == false): "
               "normalizedFileName = %s \n",
               normalizedFileName.c_str());
#endif
        modifiedFiles.insert(normalizedFileName);
        filesWithMarkedTransformations.insert(normalizedFileName);

        // DQ (10/14/2019): Trap cases where the normalizedFileName is not a
        // valid filename.
        if (normalizedFileName == "transformation") {
          fprintf(
              stderr,
              "REX_AST_INVARIANT[modified-file-origin]: transformed node "
              "selected for include-file unparsing has no source filename\n");
          ROSE_ABORT();
        }
      }

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
      printf("In IncludedFilesUnparser::visit(): node -> get_isModified(): "
             "output endl \n");
#endif
      // cout << endl << endl;
    }

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
    if (isStatement == true) {
      printf("List modifiedFiles list (size = %zu): \n", modifiedFiles.size());
      set<string>::iterator j = modifiedFiles.begin();
      size_t modified_file_counter = 0;
      while (j != modifiedFiles.end()) {
        printf("   --- modifiedFiles[%zu] = %s \n", modified_file_counter,
               (*j).c_str());

        j++;
        modified_file_counter++;
      }
    }
#endif
  }

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
  if (isStatement == true) {
    // printf ("Leaving IncludedFilesUnparser::visit(): node = %p = %s
    // modifiedFiles.size() = %zu
    // \n",node,SageInterface::get_name(node).c_str(),modifiedFiles.size());
    printf("Leaving IncludedFilesUnparser::visit(): node = %p = %s "
           "modifiedFiles.size() = %zu \n\n",
           node, node->class_name().c_str(), modifiedFiles.size());
  }
#endif
}
