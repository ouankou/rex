/* unparser.C
 * Contains the implementation of the constructors, destructor, formatting
 * functions, and fucntions that unparse directives.
 */

// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"
// #include "propagateHiddenListData.h"
// #include "HiddenList.h"

// include "array_class_interface.h"

#include "nameQualificationSupport.h"

#include "FortranLineWrapSupport.h"
#include "unparser.h"
#include "utility_functions.h"

// DQ (10/21/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.
#include <cstdlib>
#include <filesystem>

#include "rose_config.h"
#include "rose_test_output_path.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <streambuf>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>
// DQ (8/1/2018): This is the suppport for unparsing of header files.
#include "FileHelper.h"

#include "IncludedFilesUnparser.h"

// DQ (9/26/2018): Added so that we can call the display function for
// TokenStreamSequenceToNodeMapping (for debugging).
#include "tokenStreamMapping.h"

namespace si = SageInterface;

namespace {
bool hasExactSemanticStructuralProvenance(SgLocatedNode *node);
int requireStructuralPhysicalFileId(SgLocatedNode *node, const char *contract);

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
          std::count(scopes.begin(), scopes.end(), declarationScope) != 1) {
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
    return true;
  }

  std::unordered_set<SgNode *> visited;
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
          std::count(scopeSuccessors.begin(), scopeSuccessors.end(), child) !=
              1) {
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
            std::count(scopes.begin(), scopes.end(), declarationScope) != 1) {
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
        if (declarationOwner->get_nonreal_decl_scope() == declarationScope) {
          return true;
        }
        SgFunctionDeclaration *functionOwner =
            isSgFunctionDeclaration(declarationOwner);
        if (declarationOwner->get_declarationScope() != declarationScope &&
            declarationOwner->get_source_declarator_scope() !=
                declarationScope &&
            (functionOwner == nullptr ||
             functionOwner->get_function_declarator_scope() !=
                 declarationScope)) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has a direct "
                  "declaration scope without one exact typed owner\n",
                  contract, static_cast<void *>(node),
                  node->class_name().c_str());
          ROSE_ABORT();
        }
        // A declarator scope may mix source-written class/enum declarations
        // with semantic lookup declarations. Preserve that distinction from
        // exact provenance: semantic children are non-lexical, while source
        // children continue through physical-file selection below.
        if (hasExactSemanticStructuralProvenance(isSgLocatedNode(node))) {
          return true;
        }
      }
    }

    if (SgDeclarationScopeList *container = isSgDeclarationScopeList(parent)) {
      SgDeclarationScope *scope = isSgDeclarationScope(child);
      SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
      const SgDeclarationScopePtrList &scopes = container->get_scopes();
      if (scope == nullptr || owner == nullptr ||
          owner->get_auxiliary_declaration_scopes() != container ||
          scope->get_parent() != container ||
          std::count(scopes.begin(), scopes.end(), scope) != 1) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has malformed "
                "auxiliary declaration-scope container ownership\n",
                contract, static_cast<void *>(node),
                node->class_name().c_str());
        ROSE_ABORT();
      }
      return true;
    }

    SgAuxiliaryDeclarationList *container =
        isSgAuxiliaryDeclarationList(parent);
    if (container == nullptr) {
      continue;
    }
    SgDeclarationStatement *declaration = isSgDeclarationStatement(child);
    SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
    const SgDeclarationStatementPtrList &declarations =
        container->get_declarations();
    if (declaration == nullptr || owner == nullptr ||
        owner->get_auxiliary_declarations() != container ||
        std::count(declarations.begin(), declarations.end(), declaration) !=
            1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has malformed "
              "auxiliary declaration ownership\n",
              contract, static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    return true;
  }
  return false;
}

bool isExactlyTemplateInstantiationDirectivePayload(SgNode *node,
                                                    const char *contract) {
  ASSERT_not_null(node);
  ASSERT_not_null(contract);

  std::unordered_set<SgNode *> visited;
  SgNode *child = node;
  for (SgNode *parent = node->get_parent(); parent != nullptr;
       child = parent, parent = parent->get_parent()) {
    if (!visited.insert(parent).second) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: template-instantiation payload=%p/"
              "%s has a parent cycle\n",
              contract, static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }

    SgTemplateInstantiationDirectiveStatement *directive =
        isSgTemplateInstantiationDirectiveStatement(parent);
    if (directive == nullptr) {
      continue;
    }

    SgDeclarationStatement *declaration = isSgDeclarationStatement(child);
    const SgNodePtrList successors =
        directive->get_traversalSuccessorContainer();
    if (declaration == nullptr || directive->get_declaration() != declaration ||
        declaration->get_parent() != directive || successors.size() != 1 ||
        successors.front() != declaration) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: template-instantiation payload=%p/"
              "%s has no exact directive owner edge\n",
              contract, static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    if (!hasExactSemanticStructuralProvenance(declaration)) {
      return false;
    }
    (void)requireStructuralPhysicalFileId(directive, contract);
    return true;
  }
  return false;
}

bool hasExactSemanticStructuralProvenance(SgLocatedNode *node) {
  Sg_File_Info *positions[] = {
      node != nullptr ? node->get_file_info() : nullptr,
      node != nullptr ? node->get_startOfConstruct() : nullptr,
      node != nullptr ? node->get_endOfConstruct() : nullptr};
  for (Sg_File_Info *position : positions) {
    if (node == nullptr || position == nullptr ||
        position->get_parent() != node || position->isShared() ||
        !position->isCompilerGenerated() || !position->isFrontendSpecific() ||
        position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        !position->isOutputInCodeGeneration() ||
        position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        position->get_physical_file_id() !=
            Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
      return false;
    }
  }
  return true;
}

void requireExactSemanticStructuralProvenance(SgLocatedNode *node,
                                              const char *contract) {
  Sg_File_Info *positions[] = {
      node != nullptr ? node->get_file_info() : nullptr,
      node != nullptr ? node->get_startOfConstruct() : nullptr,
      node != nullptr ? node->get_endOfConstruct() : nullptr};
  for (Sg_File_Info *position : positions) {
    if (node == nullptr || position == nullptr ||
        position->get_parent() != node || position->isShared() ||
        !position->isCompilerGenerated() || !position->isFrontendSpecific() ||
        position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        !position->isOutputInCodeGeneration() ||
        position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        position->get_physical_file_id() !=
            Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: semantic structural node=%p/%s "
              "has contradictory source provenance\n",
              contract, static_cast<void *>(node),
              node != nullptr ? node->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }
  }
}

void requireExactTransparentStructuralProvenance(SgLocatedNode *node,
                                                 SgLocatedNode *lexicalOwner,
                                                 const char *contract) {
  Sg_File_Info *ownerPosition =
      lexicalOwner != nullptr ? lexicalOwner->get_file_info() : nullptr;
  if (node == nullptr || lexicalOwner == nullptr || contract == nullptr ||
      ownerPosition == nullptr || ownerPosition->get_parent() != lexicalOwner ||
      ownerPosition->get_physical_file_id() < 0) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: transparent structural node=%p/%s "
            "has no exact lexical source owner\n",
            contract != nullptr ? contract : "transparent-structure",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }

  const int ownerPhysicalFileId = ownerPosition->get_physical_file_id();
  const std::array<Sg_File_Info *, 3> positions = {node->get_file_info(),
                                                   node->get_startOfConstruct(),
                                                   node->get_endOfConstruct()};
  for (Sg_File_Info *position : positions) {
    const bool hasPreassignmentPhysicalIdentity =
        position != nullptr && position->get_physical_file_id() ==
                                   Sg_File_Info::COMPILER_GENERATED_FILE_ID;
    const bool hasAssignedPhysicalIdentity =
        position != nullptr &&
        position->get_physical_file_id() == ownerPhysicalFileId;
    if (position == nullptr || position->get_parent() != node ||
        position->isShared() || !position->isCompilerGenerated() ||
        !position->isFrontendSpecific() || position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        !position->isOutputInCodeGeneration() ||
        position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        hasPreassignmentPhysicalIdentity == hasAssignedPhysicalIdentity) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: transparent structural node=%p/%s "
              "has contradictory source provenance\n",
              contract, static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
  }
}

bool isExactlyRangeForSemanticDeclarationPayload(SgStatement *statement,
                                                 const char *contract) {
  ASSERT_not_null(statement);
  ASSERT_not_null(contract);

  std::unordered_set<SgNode *> visited;
  for (SgNode *cursor = statement; cursor != nullptr;) {
    if (!visited.insert(cursor).second) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: range-for semantic payload=%p/%s "
              "has a parent cycle\n",
              contract, static_cast<void *>(statement),
              statement->class_name().c_str());
      ROSE_ABORT();
    }

    if (SgVariableDeclaration *declaration = isSgVariableDeclaration(cursor)) {
      SgRangeBasedForStatement *owner =
          isSgRangeBasedForStatement(declaration->get_parent());
      if (owner != nullptr) {
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
            std::count(ownerSuccessors.begin(), ownerSuccessors.end(),
                       declaration) != 1) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[%s]: range-for semantic payload=%p/"
                  "%s has no single exact range/begin/end declaration "
                  "owner\n",
                  contract, static_cast<void *>(statement),
                  statement->class_name().c_str());
          ROSE_ABORT();
        }
        requireExactSemanticStructuralProvenance(declaration, contract);
        requireExactSemanticStructuralProvenance(statement, contract);
        const AttachedPreprocessingInfoType *preprocessing =
            declaration->get_attachedPreprocessingInfoPtr();
        if (preprocessing != nullptr && !preprocessing->empty()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[%s]: range-for semantic "
                  "declaration=%p owns preprocessing syntax\n",
                  contract, static_cast<void *>(declaration));
          ROSE_ABORT();
        }
        return true;
      }
    }

    SgNode *parent = cursor->get_parent();
    if (parent == nullptr) {
      break;
    }
    const SgNodePtrList parentSuccessors =
        parent->get_traversalSuccessorContainer();
    if (cursor->get_parent() != parent ||
        std::count(parentSuccessors.begin(), parentSuccessors.end(), cursor) !=
            1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: range-for semantic payload=%p/%s "
              "has no exact structural parent edge\n",
              contract, static_cast<void *>(statement),
              statement->class_name().c_str());
      ROSE_ABORT();
    }
    if (isSgRangeBasedForStatement(parent) != nullptr) {
      return false;
    }
    cursor = parent;
  }
  return false;
}

bool isExactlySourceLessForStructuralNode(SgStatement *statement,
                                          const char *contract) {
  if (SgForInitStatement *wrapper = isSgForInitStatement(statement)) {
    Sg_File_Info *fileInfo = wrapper->get_file_info();
    if (fileInfo == nullptr || fileInfo->get_physical_file_id() >= 0) {
      return false;
    }
    SgForStatement *owner = isSgForStatement(wrapper->get_parent());
    const SgStatementPtrList &initializers = wrapper->get_init_stmt();
    SgNullStatement *payload = initializers.size() == 1
                                   ? isSgNullStatement(initializers.front())
                                   : nullptr;
    const SgNodePtrList ownerSuccessors =
        owner != nullptr ? owner->get_traversalSuccessorContainer()
                         : SgNodePtrList();
    const SgNodePtrList wrapperSuccessors =
        wrapper->get_traversalSuccessorContainer();
    if (owner == nullptr || owner->get_for_init_stmt() != wrapper ||
        std::count(ownerSuccessors.begin(), ownerSuccessors.end(), wrapper) !=
            1 ||
        payload == nullptr || payload->get_parent() != wrapper ||
        wrapperSuccessors.size() != 1 || wrapperSuccessors.front() != payload) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: source-less for-init=%p has no "
              "exact owner and null payload edges\n",
              contract, static_cast<void *>(wrapper));
      ROSE_ABORT();
    }
    requireExactSemanticStructuralProvenance(wrapper, contract);
    requireExactSemanticStructuralProvenance(payload, contract);
    return true;
  }

  SgNullStatement *payload = isSgNullStatement(statement);
  Sg_File_Info *fileInfo =
      payload != nullptr ? payload->get_file_info() : nullptr;
  if (payload == nullptr || fileInfo == nullptr ||
      fileInfo->get_physical_file_id() >= 0) {
    return false;
  }

  bool exactInitializerEdge = false;
  if (SgForInitStatement *wrapper =
          isSgForInitStatement(payload->get_parent())) {
    const SgStatementPtrList &initializers = wrapper->get_init_stmt();
    exactInitializerEdge = initializers.size() == 1 &&
                           initializers.front() == payload &&
                           wrapper->get_file_info() != nullptr &&
                           wrapper->get_file_info()->get_physical_file_id() < 0;
  }
  bool exactConditionEdge = false;
  if (SgForStatement *owner = isSgForStatement(payload->get_parent())) {
    const SgNodePtrList successors = owner->get_traversalSuccessorContainer();
    exactConditionEdge =
        owner->get_test() == payload &&
        std::count(successors.begin(), successors.end(), payload) == 1;
  }
  if (exactInitializerEdge == exactConditionEdge) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: source-less null statement=%p has "
            "no unique typed for-field owner\n",
            contract, static_cast<void *>(payload));
    ROSE_ABORT();
  }
  requireExactSemanticStructuralProvenance(payload, contract);
  return true;
}

bool isExactlyImplicitControlFlowStructuralNode(SgStatement *statement,
                                                const char *contract) {
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
                       : SgNodePtrList();
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
      std::count(blockSuccessors.begin(), blockSuccessors.end(), payload) ==
          1 &&
      (declarationScopes == nullptr ||
       std::count(blockSuccessors.begin(), blockSuccessors.end(),
                  declarationScopes) == 1) &&
      (declarations == nullptr ||
       std::count(blockSuccessors.begin(), blockSuccessors.end(),
                  declarations) == 1);
  if (!exactTypedEdge ||
      std::count(ownerSuccessors.begin(), ownerSuccessors.end(), block) != 1 ||
      payload == nullptr || payload->get_parent() != block ||
      !exactBlockSuccessors || block->get_is_fortran_block_construct()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: implicit control-flow scope=%p has no "
            "exact typed owner and sole statement edge\n",
            contract, static_cast<void *>(block));
    ROSE_ABORT();
  }
  requireExactSemanticStructuralProvenance(block, contract);
  return true;
}

bool isExactlyCatchSequenceStructuralNode(SgStatement *statement,
                                          const char *contract) {
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
      std::count(ownerSuccessors.begin(), ownerSuccessors.end(), sequence) !=
          1 ||
      handlers.empty() || sequenceSuccessors.size() != handlers.size()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: catch sequence=%p has no exact "
            "nonempty structural owner and handler edges\n",
            contract, static_cast<void *>(sequence));
    ROSE_ABORT();
  }
  for (std::size_t index = 0; index < handlers.size(); ++index) {
    if (isSgCatchOptionStmt(handlers[index]) == nullptr ||
        handlers[index]->get_parent() != sequence ||
        sequenceSuccessors[index] != handlers[index]) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: catch sequence=%p has malformed "
              "handler index=%zu\n",
              contract, static_cast<void *>(sequence), index);
      ROSE_ABORT();
    }
  }
  requireExactTransparentStructuralProvenance(sequence, owner, contract);
  return true;
}

int requireStructuralPhysicalFileId(SgLocatedNode *node, const char *contract) {
  if (node == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: cannot resolve physical ownership "
            "for a null node\n",
            contract);
    ROSE_ABORT();
  }

  Sg_File_Info *nodeInfo = node->get_file_info();
  if (nodeInfo == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has no file "
            "information\n",
            contract, static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }
  if (nodeInfo->get_physical_file_id() >= 0) {
    return nodeInfo->get_physical_file_id();
  }
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[%s]: lexical node=%p type=%s has invalid "
          "physical file id=%d output=%d compiler-generated=%d "
          "transformation=%d frontend-specific=%d parent=%p/%s\n",
          contract, static_cast<void *>(node), node->class_name().c_str(),
          nodeInfo->get_physical_file_id(),
          nodeInfo->isOutputInCodeGeneration() ? 1 : 0,
          nodeInfo->isCompilerGenerated() ? 1 : 0,
          (nodeInfo->isTransformation() || node->isTransformation()) ? 1 : 0,
          nodeInfo->isFrontendSpecific() ? 1 : 0,
          static_cast<void *>(node->get_parent()),
          node->get_parent() != nullptr
              ? node->get_parent()->class_name().c_str()
              : "<null>");
  ROSE_ABORT();
}

std::string resolveUnparseOutputToTestDir(const std::string &filename) {
  return Rose::TestOutput::resolvePath(filename);
}

static std::string findOutputDirArg(const std::vector<std::string> &argv) {
  for (size_t i = 0; i + 1 < argv.size(); ++i) {
    if (argv[i] == "-outputdir") {
      return argv[i + 1];
    }
  }
  return std::string();
}

std::string resolveUnparseOutputToTestDir(SgFile *file,
                                          const std::string &filename) {
  if (file == NULL) {
    return resolveUnparseOutputToTestDir(filename);
  }

  const std::string output_dir =
      findOutputDirArg(file->get_originalCommandLineArgumentList());
  return Rose::TestOutput::resolvePath(filename, output_dir);
}

class FileDescriptorOutputBuffer final : public std::streambuf {
public:
  explicit FileDescriptorOutputBuffer(int descriptor)
      : descriptor_(descriptor) {
    setp(buffer_.data(), buffer_.data() + buffer_.size());
  }

  int writeError() const { return writeError_; }

protected:
  int sync() override { return flushBuffer() ? 0 : -1; }

  int_type overflow(int_type character) override {
    if (!flushBuffer()) {
      return traits_type::eof();
    }
    if (!traits_type::eq_int_type(character, traits_type::eof())) {
      *pptr() = traits_type::to_char_type(character);
      pbump(1);
    }
    return traits_type::not_eof(character);
  }

  std::streamsize xsputn(const char *data, std::streamsize count) override {
    if (count < 0 || writeError_ != 0) {
      return 0;
    }

    std::streamsize written = 0;
    while (written < count) {
      if (pptr() == epptr() && !flushBuffer()) {
        break;
      }
      const std::streamsize available = epptr() - pptr();
      const std::streamsize chunk = std::min(available, count - written);
      std::memcpy(pptr(), data + written, static_cast<std::size_t>(chunk));
      pbump(static_cast<int>(chunk));
      written += chunk;
    }
    return written;
  }

private:
  bool flushBuffer() {
    if (writeError_ != 0) {
      return false;
    }

    const std::ptrdiff_t buffered = pptr() - pbase();
    std::ptrdiff_t written = 0;
    while (written < buffered) {
      const ssize_t result =
          write(descriptor_, pbase() + written,
                static_cast<std::size_t>(buffered - written));
      if (result > 0) {
        written += result;
        continue;
      }
      if (result < 0 && errno == EINTR) {
        continue;
      }
      writeError_ = result < 0 ? errno : EIO;
      return false;
    }
    setp(buffer_.data(), buffer_.data() + buffer_.size());
    return true;
  }

  int descriptor_;
  int writeError_ = 0;
  std::array<char, 64 * 1024> buffer_{};
};

class AtomicOutputStagingFile final {
public:
  AtomicOutputStagingFile(std::string filename, int descriptor, dev_t device,
                          ino_t inode)
      : filename_(std::move(filename)), descriptor_(descriptor),
        device_(device), inode_(inode), buffer_(descriptor_),
        output_(&buffer_) {}

  AtomicOutputStagingFile(const AtomicOutputStagingFile &) = delete;
  AtomicOutputStagingFile &operator=(const AtomicOutputStagingFile &) = delete;

  ~AtomicOutputStagingFile() {
    ROSE_ASSERT(descriptor_ == -1);
    ROSE_ASSERT(pathFinalized_);
  }

  const std::string &filename() const { return filename_; }
  int descriptor() const { return descriptor_; }
  std::ostream &output() { return output_; }
  bool writesFinalized() const { return writesFinalized_; }

  void finishWrites(const std::string &outputFilename, const char *contract) {
    if (contract == nullptr || writesFinalized_ || descriptor_ < 0 ||
        pathFinalized_) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[output-staging-state]: output=%s "
              "staging=%s has invalid write-finalization state\n",
              outputFilename.c_str(), filename_.c_str());
      ROSE_ABORT();
    }

    output_.flush();
    if (!output_) {
      const int writeError = buffer_.writeError();
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: output=%s staging=%s write "
              "through exclusive descriptor failed: %s\n",
              contract, outputFilename.c_str(), filename_.c_str(),
              writeError != 0 ? strerror(writeError) : "stream failure");
      ROSE_ABORT();
    }
    writesFinalized_ = true;
    verifyPathIdentity(filename_, outputFilename, "output-staging-identity");
  }

  void verifyStagingPathIdentity(const std::string &outputFilename) const {
    verifyPathIdentity(filename_, outputFilename, "output-staging-identity");
  }

  void verifyCommittedPathIdentity(const std::string &outputFilename) const {
    verifyPathIdentity(outputFilename, outputFilename,
                       "output-commit-identity");
  }

  void markRenamed() {
    if (pathFinalized_) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[output-staging-state]: staging=%s was "
              "finalized more than once\n",
              filename_.c_str());
      ROSE_ABORT();
    }
    pathFinalized_ = true;
  }

  void closeDescriptor(const std::string &outputFilename,
                       const char *contract) {
    if (contract == nullptr || descriptor_ < 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[output-staging-state]: output=%s "
              "staging=%s has no exclusive descriptor to close\n",
              outputFilename.c_str(), filename_.c_str());
      ROSE_ABORT();
    }
    const int descriptor = descriptor_;
    if (close(descriptor) != 0) {
      const int closeError = errno;
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: output=%s staging=%s cannot close "
              "the exclusive descriptor: %s\n",
              contract, outputFilename.c_str(), filename_.c_str(),
              strerror(closeError));
      ROSE_ABORT();
    }
    descriptor_ = -1;
  }

  void discard(const std::string &outputFilename, const char *contract) {
    if (contract == nullptr || !writesFinalized_ || pathFinalized_) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[output-staging-state]: output=%s "
              "staging=%s has invalid discard state\n",
              outputFilename.c_str(), filename_.c_str());
      ROSE_ABORT();
    }
    verifyStagingPathIdentity(outputFilename);
    if (unlink(filename_.c_str()) != 0) {
      const int unlinkError = errno;
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: output=%s staging=%s cannot "
              "remove the staging entry: %s\n",
              contract, outputFilename.c_str(), filename_.c_str(),
              strerror(unlinkError));
      ROSE_ABORT();
    }
    pathFinalized_ = true;
    closeDescriptor(outputFilename, contract);
  }

private:
  void verifyPathIdentity(const std::string &path,
                          const std::string &outputFilename,
                          const char *contract) const {
    if (descriptor_ < 0 || contract == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[output-staging-state]: output=%s "
              "staging=%s has no descriptor identity\n",
              outputFilename.c_str(), filename_.c_str());
      ROSE_ABORT();
    }

    struct stat descriptorStatus{};
    struct stat pathStatus{};
    if (fstat(descriptor_, &descriptorStatus) != 0) {
      const int statusError = errno;
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: output=%s staging=%s cannot "
              "inspect the exclusive descriptor: %s\n",
              contract, outputFilename.c_str(), filename_.c_str(),
              strerror(statusError));
      ROSE_ABORT();
    }
    if (lstat(path.c_str(), &pathStatus) != 0) {
      const int statusError = errno;
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: output=%s staging=%s path=%s "
              "cannot inspect the directory entry: %s\n",
              contract, outputFilename.c_str(), filename_.c_str(), path.c_str(),
              strerror(statusError));
      ROSE_ABORT();
    }
    if (!S_ISREG(descriptorStatus.st_mode) || !S_ISREG(pathStatus.st_mode) ||
        descriptorStatus.st_dev != device_ ||
        descriptorStatus.st_ino != inode_ || pathStatus.st_dev != device_ ||
        pathStatus.st_ino != inode_ || descriptorStatus.st_nlink != 1 ||
        pathStatus.st_nlink != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: output=%s staging=%s path=%s no "
              "longer names the exclusively created regular file\n",
              contract, outputFilename.c_str(), filename_.c_str(),
              path.c_str());
      ROSE_ABORT();
    }
  }

  std::string filename_;
  int descriptor_;
  dev_t device_;
  ino_t inode_;
  FileDescriptorOutputBuffer buffer_;
  std::ostream output_;
  bool writesFinalized_ = false;
  bool pathFinalized_ = false;
};

std::unique_ptr<AtomicOutputStagingFile>
createAtomicOutputStagingFile(const std::string &outputFilename) {
  static std::atomic<unsigned long long> stagingSequence{0};
  const std::filesystem::path outputPath(outputFilename);
  const std::filesystem::path parent =
      outputPath.has_parent_path() ? outputPath.parent_path() : ".";
  std::error_code error;
  std::filesystem::create_directories(parent, error);
  if (error) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-directory]: output=%s error=%s\n",
            outputFilename.c_str(), error.message().c_str());
    ROSE_ABORT();
  }

  int openFlags = O_RDWR | O_CREAT | O_EXCL;
#ifdef O_CLOEXEC
  openFlags |= O_CLOEXEC;
#endif
  std::string stagingFilename;
  int descriptor = -1;
  for (;;) {
    const unsigned long long sequence =
        stagingSequence.fetch_add(1, std::memory_order_relaxed);
    if (sequence == std::numeric_limits<unsigned long long>::max()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[output-staging]: output=%s exhausted the "
              "atomic staging sequence\n",
              outputFilename.c_str());
      ROSE_ABORT();
    }
    stagingFilename =
        (parent / ("." + outputPath.filename().string() + ".rex-unparse-" +
                   std::to_string(static_cast<unsigned long long>(getpid())) +
                   "-" + std::to_string(sequence)))
            .string();
    descriptor =
        open(stagingFilename.c_str(), openFlags,
             S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (descriptor != -1) {
      break;
    }
    const int openError = errno;
    if (openError == EEXIST) {
      continue;
    }
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-staging]: output=%s staging=%s "
            "unable to create an exclusive sibling file: %s\n",
            outputFilename.c_str(), stagingFilename.c_str(),
            strerror(openError));
    ROSE_ABORT();
  }
  struct stat descriptorStatus{};
  const int descriptorStatusResult = fstat(descriptor, &descriptorStatus);
  if (descriptorStatusResult != 0 || !S_ISREG(descriptorStatus.st_mode) ||
      descriptorStatus.st_nlink != 1) {
    const int statusError = descriptorStatusResult != 0 ? errno : EINVAL;
    unlink(stagingFilename.c_str());
    close(descriptor);
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-staging]: output=%s staging=%s "
            "cannot establish the exclusive regular-file identity: %s\n",
            outputFilename.c_str(), stagingFilename.c_str(),
            strerror(statusError));
    ROSE_ABORT();
  }
  return std::make_unique<AtomicOutputStagingFile>(stagingFilename, descriptor,
                                                   descriptorStatus.st_dev,
                                                   descriptorStatus.st_ino);
}

void commitAtomicOutput(AtomicOutputStagingFile &staging,
                        const std::string &outputFilename) {
  if (!staging.writesFinalized()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-staging-state]: output=%s "
            "staging=%s was not write-finalized before commit\n",
            outputFilename.c_str(), staging.filename().c_str());
    ROSE_ABORT();
  }

  struct stat destinationStatus{};
  if (lstat(outputFilename.c_str(), &destinationStatus) == 0) {
    if (!S_ISREG(destinationStatus.st_mode)) {
      staging.discard(outputFilename, "output-permissions");
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[output-permissions]: output=%s is not a "
              "regular file; symlinks and other special files are not "
              "valid output destinations\n",
              outputFilename.c_str());
      ROSE_ABORT();
    }
    if (destinationStatus.st_nlink != 1) {
      staging.discard(outputFilename, "output-destination");
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[output-destination]: output=%s is a "
              "regular file with %llu links; atomic replacement requires "
              "exactly one destination link\n",
              outputFilename.c_str(),
              static_cast<unsigned long long>(destinationStatus.st_nlink));
      ROSE_ABORT();
    }
    const mode_t destinationMode =
        destinationStatus.st_mode &
        (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);
    if (fchmod(staging.descriptor(), destinationMode) != 0) {
      const int chmodError = errno;
      staging.discard(outputFilename, "output-permissions");
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[output-permissions]: output=%s "
              "staging=%s cannot preserve mode=%04o: %s\n",
              outputFilename.c_str(), staging.filename().c_str(),
              static_cast<unsigned int>(destinationMode), strerror(chmodError));
      ROSE_ABORT();
    }
  } else if (errno != ENOENT) {
    const int statusError = errno;
    staging.discard(outputFilename, "output-permissions");
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-permissions]: output=%s cannot "
            "inspect destination permissions: %s\n",
            outputFilename.c_str(), strerror(statusError));
    ROSE_ABORT();
  }

  staging.verifyStagingPathIdentity(outputFilename);
  std::error_code error;
  std::filesystem::rename(staging.filename(), outputFilename, error);
  if (error) {
    staging.discard(outputFilename, "output-commit");
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-commit]: output=%s error=%s\n",
            outputFilename.c_str(), error.message().c_str());
    ROSE_ABORT();
  }
  staging.markRenamed();
  staging.verifyCommittedPathIdentity(outputFilename);
  staging.closeDescriptor(outputFilename, "output-close");
}

bool filesHaveIdenticalContents(const std::filesystem::path &leftPath,
                                const AtomicOutputStagingFile &right) {
  right.verifyStagingPathIdentity(leftPath.string());
  std::error_code leftSizeError;
  const std::uintmax_t leftSize =
      std::filesystem::file_size(leftPath, leftSizeError);
  struct stat rightStatus{};
  if (fstat(right.descriptor(), &rightStatus) != 0) {
    const int statusError = errno;
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-compare]: left=%s right=%s cannot "
            "inspect the exclusive descriptor: %s\n",
            leftPath.string().c_str(), right.filename().c_str(),
            strerror(statusError));
    ROSE_ABORT();
  }
  const std::uintmax_t rightSize =
      static_cast<std::uintmax_t>(rightStatus.st_size);
  if (leftSizeError) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-compare]: left=%s right=%s "
            "unable to read the existing file size: %s\n",
            leftPath.string().c_str(), right.filename().c_str(),
            leftSizeError.message().c_str());
    ROSE_ABORT();
  }
  if (leftSize != rightSize) {
    return false;
  }

  std::ifstream left(leftPath, std::ios::in | std::ios::binary);
  if (!left) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-compare]: left=%s right=%s "
            "unable to open the existing comparison input\n",
            leftPath.string().c_str(), right.filename().c_str());
    ROSE_ABORT();
  }

  std::vector<char> leftBuffer(64 * 1024);
  std::vector<char> rightBuffer(64 * 1024);
  std::uintmax_t compared = 0;
  while (compared < leftSize) {
    const std::size_t requested = static_cast<std::size_t>(
        std::min<std::uintmax_t>(leftBuffer.size(), leftSize - compared));
    left.read(leftBuffer.data(), static_cast<std::streamsize>(requested));
    std::size_t rightRead = 0;
    while (rightRead < requested) {
      const ssize_t result = pread(
          right.descriptor(), rightBuffer.data() + rightRead,
          requested - rightRead, static_cast<off_t>(compared + rightRead));
      if (result > 0) {
        rightRead += static_cast<std::size_t>(result);
        continue;
      }
      if (result < 0 && errno == EINTR) {
        continue;
      }
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[output-compare]: left=%s right=%s "
              "descriptor read failed at byte=%" PRIuMAX ": %s\n",
              leftPath.string().c_str(), right.filename().c_str(), compared,
              result < 0 ? strerror(errno) : "short read");
      ROSE_ABORT();
    }
    if (left.gcount() != static_cast<std::streamsize>(requested)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[output-compare]: left=%s right=%s short "
              "read at byte=%" PRIuMAX "\n",
              leftPath.string().c_str(), right.filename().c_str(), compared);
      ROSE_ABORT();
    }
    if (std::memcmp(leftBuffer.data(), rightBuffer.data(), requested) != 0) {
      return false;
    }
    compared += static_cast<std::uintmax_t>(requested);
  }
  if (left.bad()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-compare]: left=%s right=%s read "
            "failed\n",
            leftPath.string().c_str(), right.filename().c_str());
    ROSE_ABORT();
  }
  right.verifyStagingPathIdentity(leftPath.string());
  return true;
}

void copyOriginalHeaderToOutputLocation(SgProject *project,
                                        const std::string &originalFileName,
                                        const std::string &outputFileName) {
  using OriginalHeaderSnapshots = std::map<std::string, std::string>;
  ASSERT_not_null(project);
  const std::string normalizedOriginalFileName =
      FileHelper::normalizePathIfPossible(originalFileName);
  if (normalizedOriginalFileName.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-snapshot]: header=%s has no "
            "canonical source path\n",
            originalFileName.c_str());
    ROSE_ABORT();
  }
  const OriginalHeaderSnapshots &snapshots =
      project->get_original_header_snapshots();
  const auto snapshot = snapshots.find(normalizedOriginalFileName);
  if (snapshot == snapshots.end()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-snapshot]: header=%s was not "
            "snapshotted by the frontend\n",
            originalFileName.c_str());
    ROSE_ABORT();
  }

  std::unique_ptr<AtomicOutputStagingFile> staging =
      createAtomicOutputStagingFile(outputFileName);
  std::ostream &output = staging->output();
  output.write(snapshot->second.data(), snapshot->second.size());
  staging->finishWrites(outputFileName, "header-copy");
  commitAtomicOutput(*staging, outputFileName);
}

bool fileHasRelevantModifications(SgSourceFile *file) {
  if (file == NULL) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[modification-ownership]: null source "
            "file\n");
    ROSE_ABORT();
  }

  std::set<SgStatement *> transformedStatements =
      SageInterface::collectTransformedStatements(file);
  auto affects_current_file = [&](SgLocatedNode *node) -> bool {
    if (node == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: null modified "
              "node\n");
      ROSE_ABORT();
    }

    Sg_File_Info *node_info = node->get_file_info();
    if (node_info == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: node=%p "
              "type=%s has no file information\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }

    const int physicalFileId = node_info->get_physical_file_id();
    if (physicalFileId < 0 ||
        physicalFileId == Sg_File_Info::TRANSFORMATION_FILE_ID) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: node=%p "
              "type=%s still has transformation-only file ownership\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    const std::string nodeFileName = FileHelper::normalizePathIfPossible(
        node_info->getFilenameFromID(physicalFileId));
    const std::string sourceFileName =
        FileHelper::normalizePathIfPossible(file->getFileName());
    if (nodeFileName.empty() || sourceFileName.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: node=%p "
              "type=%s or source=%p has no normalized physical filename\n",
              static_cast<void *>(node), node->class_name().c_str(),
              static_cast<void *>(file));
      ROSE_ABORT();
    }
    return nodeFileName == sourceFileName;
  };

  for (SgStatement *stmt : transformedStatements) {
    if (stmt == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: transformed "
              "statement set contains null\n");
      ROSE_ABORT();
    }
    if (isExactlyAuxiliaryOwned(stmt, "modification-ownership")) {
      continue;
    }
    Sg_File_Info *stmt_info = stmt->get_file_info();
    if (stmt_info == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: statement=%p "
              "type=%s has no file information\n",
              static_cast<void *>(stmt), stmt->class_name().c_str());
      ROSE_ABORT();
    }
    if (stmt_info->isOutputInCodeGeneration() == false) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: transformed "
              "statement=%p/%s uses the legacy non-output suppression bit\n",
              static_cast<void *>(stmt), stmt->class_name().c_str());
      ROSE_ABORT();
    }
    if (affects_current_file(stmt) == false) {
      continue;
    }
    return true;
  }

  std::set<SgLocatedNode *> modifiedNodes =
      SageInterface::collectModifiedLocatedNodes(file);
  for (SgLocatedNode *node : modifiedNodes) {
    if (node == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: modified node "
              "set contains null\n");
      ROSE_ABORT();
    }
    if (isExactlyAuxiliaryOwned(node, "modification-ownership")) {
      continue;
    }
    Sg_File_Info *node_info = node->get_file_info();
    if (node_info == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: node=%p "
              "type=%s has no file information\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    if (node_info->isOutputInCodeGeneration() == false) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: modified "
              "node=%p/%s uses the legacy non-output suppression bit\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    if (affects_current_file(node) == false) {
      continue;
    }
    return true;
  }

  return false;
}

std::string getAssociatedFileNameForOutput(SgLocatedNode *node) {
  if (node == NULL) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[modification-ownership]: null located "
            "node\n");
    ROSE_ABORT();
  }

  Sg_File_Info *fileInfo = node->get_file_info();
  if (fileInfo == NULL) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[modification-ownership]: node=%p type=%s "
            "has no file information\n",
            static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }

  const int physicalFileId =
      requireStructuralPhysicalFileId(node, "modification-ownership");
  const std::string filename = fileInfo->getFilenameFromID(physicalFileId);
  const std::string normalizedFilename =
      FileHelper::normalizePathIfPossible(filename);
  if (normalizedFilename.empty() || filename == "NULL_FILE" ||
      filename == "transformation") {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[modification-ownership]: node=%p type=%s "
            "physical file id=%d has no filename\n",
            static_cast<void *>(node), node->class_name().c_str(),
            physicalFileId);
    ROSE_ABORT();
  }
  return normalizedFilename;
}

std::set<std::string>
collectFilesWithRelevantModifications(SgProject *project) {
  std::set<std::string> files;
  if (project == NULL) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[modification-ownership]: null project\n");
    ROSE_ABORT();
  }

  std::set<SgStatement *> transformedStatements =
      SageInterface::collectTransformedStatements(project);
  for (SgStatement *statement : transformedStatements) {
    if (statement == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: transformed "
              "statement set contains null\n");
      ROSE_ABORT();
    }
    if (isExactlyAuxiliaryOwned(statement, "modification-ownership")) {
      continue;
    }

    Sg_File_Info *fileInfo = statement->get_file_info();
    if (fileInfo == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: statement=%p "
              "type=%s has no file information\n",
              static_cast<void *>(statement), statement->class_name().c_str());
      ROSE_ABORT();
    }
    if (fileInfo->isOutputInCodeGeneration() == false) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: transformed "
              "statement=%p/%s uses the legacy non-output suppression bit\n",
              static_cast<void *>(statement), statement->class_name().c_str());
      ROSE_ABORT();
    }

    const std::string filename = getAssociatedFileNameForOutput(statement);
    if (!filename.empty()) {
      files.insert(filename);
    }
  }

  std::set<SgLocatedNode *> modifiedNodes =
      SageInterface::collectModifiedLocatedNodes(project);
  for (SgLocatedNode *node : modifiedNodes) {
    if (node == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: modified node "
              "set contains null\n");
      ROSE_ABORT();
    }
    if (isExactlyAuxiliaryOwned(node, "modification-ownership")) {
      continue;
    }

    Sg_File_Info *fileInfo = node->get_file_info();
    if (fileInfo == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: node=%p "
              "type=%s has no file information\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    if (fileInfo->isOutputInCodeGeneration() == false) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[modification-ownership]: modified "
              "node=%p/%s uses the legacy non-output suppression bit\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }

    const std::string filename = getAssociatedFileNameForOutput(node);
    if (!filename.empty()) {
      files.insert(filename);
    }
  }

  return files;
}

bool headerRequiresAstUnparsing(
    const std::set<std::string> &filesWithRelevantModifications,
    const std::set<std::string> &filesWithMarkedTransformations,
    const std::set<std::string> &filesWithUpdatedIncludePaths,
    const std::string &headerFilename) {
  const std::string normalizedHeaderFilename =
      FileHelper::normalizePathIfPossible(headerFilename);
  if (normalizedHeaderFilename.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-plan]: header has no physical "
            "filename\n");
    ROSE_ABORT();
  }

  if (filesWithRelevantModifications.find(normalizedHeaderFilename) !=
      filesWithRelevantModifications.end()) {
    return true;
  }

  if (filesWithMarkedTransformations.find(normalizedHeaderFilename) !=
      filesWithMarkedTransformations.end()) {
    return true;
  }

  return filesWithUpdatedIncludePaths.find(normalizedHeaderFilename) !=
         filesWithUpdatedIncludePaths.end();
}

bool scopeHasRelevantModifications(SgScopeStatement *scope,
                                   const std::string &headerFilename,
                                   SgSourceFile *materializedHeader) {
  const std::string normalizedHeaderFilename =
      FileHelper::normalizePathIfPossible(headerFilename);
  if (scope == NULL || normalizedHeaderFilename.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-scope]: scope=%p header=%s is "
            "incomplete\n",
            static_cast<void *>(scope), headerFilename.c_str());
    ROSE_ABORT();
  }

  class Traversal : public AstSimpleProcessing {
  public:
    Traversal(const std::string &normalizedHeaderFilename,
              SgSourceFile *materializedHeader)
        : normalizedHeaderFilename(normalizedHeaderFilename),
          materializedHeader(materializedHeader), found(false) {}

    void visit(SgNode *node) {
      if (found == true) {
        return;
      }

      SgLocatedNode *locatedNode = isSgLocatedNode(node);
      if (locatedNode == NULL) {
        return;
      }
      if (isExactlyAuxiliaryOwned(locatedNode, "header-scope")) {
        return;
      }
      Sg_File_Info *fileInfo = locatedNode->get_file_info();
      if (fileInfo == NULL) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-scope]: node=%p type=%s has no "
                "file information\n",
                static_cast<void *>(locatedNode),
                locatedNode->class_name().c_str());
        ROSE_ABORT();
      }
      const std::string physicalFileName =
          getAssociatedFileNameForOutput(locatedNode);
      if (physicalFileName != normalizedHeaderFilename) {
        return;
      }
      if (fileInfo->isOutputInCodeGeneration() == false) {
        SgStatement *statement = isSgStatement(locatedNode);
        for (SgNode *owner = locatedNode->get_parent();
             statement == nullptr && owner != nullptr;
             owner = owner->get_parent()) {
          statement = isSgStatement(owner);
        }
        if (materializedHeader == nullptr ||
            FileHelper::normalizePathIfPossible(
                materializedHeader->getFileName()) !=
                normalizedHeaderFilename ||
            statement == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[header-scope]: lexical non-output "
                  "node=%p/%s has no exact materialized header statement "
                  "owner\n",
                  static_cast<void *>(locatedNode),
                  locatedNode->class_name().c_str());
          ROSE_ABORT();
        }
        const auto &tokenMap = materializedHeader->get_tokenSubsequenceMap();
        const auto mapping = tokenMap.find(statement);
        if (mapping == tokenMap.end() || mapping->second == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[header-scope]: lexical non-output "
                  "statement=%p/%s has no exact token-only ownership\n",
                  static_cast<void *>(statement),
                  statement->class_name().c_str());
          ROSE_ABORT();
        }
        if (locatedNode->get_isModified() ||
            (isSgStatement(locatedNode) != nullptr &&
             isSgStatement(locatedNode)->isTransformation())) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[header-scope]: "
                  "modified/transformed node=%p/%s cannot retain token-only "
                  "ownership\n",
                  static_cast<void *>(locatedNode),
                  locatedNode->class_name().c_str());
          ROSE_ABORT();
        }
        return;
      }

      if (locatedNode->get_isModified() == true) {
        found = true;
        return;
      }

      SgStatement *statement = isSgStatement(node);
      if (statement != NULL && statement->isTransformation() == true) {
        found = true;
      }
    }

    const std::string normalizedHeaderFilename;
    SgSourceFile *const materializedHeader;
    bool found;
  };

  Traversal traversal(normalizedHeaderFilename, materializedHeader);
  traversal.traverse(scope, preorder);
  return traversal.found;
}

void insertIncludeFileMapEntry(TokenUnparseFrontierContext &context,
                               const std::string &filename,
                               SgIncludeFile *includeFile) {
  if (includeFile == nullptr || filename.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[include-tree]: filename=%s include=%p "
            "cannot index an incomplete include node\n",
            filename.c_str(), static_cast<void *>(includeFile));
    ROSE_ABORT();
  }

  const std::string normalizedFilename =
      FileHelper::normalizePathIfPossible(filename);
  if (normalizedFilename.empty() || normalizedFilename == "NULL_FILE" ||
      normalizedFilename == "transformation") {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[include-tree]: filename=%s has no "
            "physical include identity\n",
            filename.c_str());
    ROSE_ABORT();
  }
  std::vector<SgIncludeFile *> &occurrences =
      context.includeFileOccurrencesByPath[normalizedFilename];
  if (std::find(occurrences.begin(), occurrences.end(), includeFile) ==
      occurrences.end()) {
    occurrences.push_back(includeFile);
  }

  SgSourceFile *source = includeFile->get_source_file();
  if (source == nullptr) {
    return;
  }
  const std::string sourcePath =
      FileHelper::normalizePathIfPossible(source->getFileName());
  SgIncludeFile *canonical = source->get_associated_include_file();
  const std::string canonicalPath =
      canonical != nullptr
          ? FileHelper::normalizePathIfPossible(canonical->get_filename())
          : std::string();
  if (sourcePath != normalizedFilename || canonical == nullptr ||
      canonical->get_source_file() != source ||
      canonicalPath != normalizedFilename) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-context]: occurrence=%p file=%s "
            "source=%p path=%s has no exact bidirectional canonical include "
            "ownership\n",
            static_cast<void *>(includeFile), normalizedFilename.c_str(),
            static_cast<void *>(source), sourcePath.c_str());
    ROSE_ABORT();
  }

  auto insertResult = context.includeFilesByPath.insert(
      std::make_pair(normalizedFilename, canonical));
  if (!insertResult.second && insertResult.first->second != canonical) {
    SgIncludeFile *existing = insertResult.first->second;
    SgSourceFile *existingSource =
        existing != nullptr ? existing->get_source_file() : nullptr;
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-context]: file=%s has distinct "
            "canonical include owners %p(source=%p) and %p(source=%p)\n",
            normalizedFilename.c_str(), static_cast<void *>(existing),
            static_cast<void *>(existingSource), static_cast<void *>(canonical),
            static_cast<void *>(source));
    ROSE_ABORT();
  }
}

void populateIncludeFileMapForUnparsingFromIncludeTree(
    TokenUnparseFrontierContext &context, SgIncludeFile *includeTreeRoot) {
  if (includeTreeRoot == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[include-tree]: missing include-tree root\n");
    ROSE_ABORT();
  }

  std::vector<SgIncludeFile *> worklist;
  std::set<SgIncludeFile *> visited;
  worklist.push_back(includeTreeRoot);

  while (!worklist.empty()) {
    SgIncludeFile *includeFile = worklist.back();
    worklist.pop_back();
    if (includeFile == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[include-tree]: null include-file node\n");
      ROSE_ABORT();
    }
    if (!visited.insert(includeFile).second) {
      continue;
    }

    const std::string filename = includeFile->get_filename();
    insertIncludeFileMapEntry(context, filename, includeFile);

    SgSourceFile *includedSourceFile = includeFile->get_source_file();
    if (includedSourceFile != nullptr) {
      insertIncludeFileMapEntry(context, includedSourceFile->getFileName(),
                                includeFile);
    }

    const SgIncludeFilePtrList &includeFileList =
        includeFile->get_include_file_list();
    for (size_t i = 0; i < includeFileList.size(); ++i) {
      worklist.push_back(includeFileList[i]);
    }
  }
}

SgIncludeFile *
lookupIncludeFileForUnparsing(const TokenUnparseFrontierContext &context,
                              const std::string &filename) {
  if (filename.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[include-tree]: cannot look up an empty "
            "physical filename\n");
    ROSE_ABORT();
  }

  const std::string normalizedFilename =
      FileHelper::normalizePathIfPossible(filename);
  std::map<std::string, SgIncludeFile *>::const_iterator it =
      context.includeFilesByPath.find(normalizedFilename);
  if (it == context.includeFilesByPath.end()) {
    return nullptr;
  }
  return it->second;
}
} // namespace

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;
using namespace Rose;

// extern ROSEAttributesList *getPreprocessorDirectives( char *fileName); //
// [DT] 3/16/2000

// DQ (6/25/2011): Forward declaration for new name qualification support.
// void generateNameQualificationSupport( SgNode* node, std::set<SgNode*> &
// referencedNameSet );

// DQ (12/6/2014): The call to this function has been moved to the
// sage_support.C file so that it can be called on the AST before
// transformations.  However it is now split into two parts so that the token
// stream can be mapped to the AST before transformations, and then the token
// stream frontier can be computed after transformations have been done in the
// AST. DQ (10/27/2013): Added forward declaration for new token stream support.
// void buildTokenStreamMapping(SgSourceFile* sourceFile);

// DQ (5/9/2021): Activate this code.
void buildTokenStreamFrontier(SgSourceFile *sourceFile,
                              bool traverseHeaderFiles,
                              TokenUnparseFrontierContext &context,
                              SgNode *traversalRoot = nullptr);
void buildFirstAndLastStatementsForIncludeFiles(
    SgProject *project, TokenUnparseFrontierContext &context);
void buildFirstAndLastStatementsForScopes(SgProject *project,
                                          TokenUnparseFrontierContext &context);

namespace {
bool sourceNeedsTokenFrontier(SgSourceFile *sourceFile) {
  ASSERT_not_null(sourceFile);
  return sourceFile->get_unparse_tokens();
}

bool prepareMaterializedHeaderTokenFrontiers(
    SgProject *project, TokenUnparseFrontierContext &context,
    const map<string, SgScopeStatement *> &unparseScopesMap) {
  ASSERT_not_null(project);
  std::vector<SgIncludeFile *> worklist;
  std::set<SgIncludeFile *> visitedIncludes;
  std::set<SgSourceFile *> materializedHeaders;
  bool preparedAny = false;

  for (SgFile *projectFile : project->get_fileList()) {
    SgSourceFile *translationUnit = isSgSourceFile(projectFile);
    if (translationUnit == nullptr ||
        !translationUnit->get_unparseHeaderFiles()) {
      continue;
    }
    SgIncludeFile *root = translationUnit->get_associated_include_file();
    if (root == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[include-tree]: translation-unit=%s "
              "has no include-tree root\n",
              translationUnit->getFileName().c_str());
      ROSE_ABORT();
    }
    worklist.push_back(root);
  }

  while (!worklist.empty()) {
    SgIncludeFile *includeFile = worklist.back();
    worklist.pop_back();
    if (includeFile == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[include-tree]: null include-file node\n");
      ROSE_ABORT();
    }
    if (!visitedIncludes.insert(includeFile).second) {
      continue;
    }

    SgSourceFile *header = includeFile->get_source_file();
    if (header != nullptr && header->get_isHeaderFile() &&
        sourceNeedsTokenFrontier(header)) {
      materializedHeaders.insert(header);
    }
    if (header != nullptr && header->get_isHeaderFile() &&
        sourceNeedsTokenFrontier(header) && !context.hasFile(header) &&
        header->get_associated_include_file() == includeFile) {
      const string normalizedHeaderPath =
          FileHelper::normalizePathIfPossible(header->getFileName());
      const auto scope = unparseScopesMap.find(normalizedHeaderPath);
      if (scope == unparseScopesMap.end() || scope->second == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-context]: header=%s source=%p "
                "has no structural emission scope for frontier analysis\n",
                header->getFileName().c_str(), static_cast<void *>(header));
        ROSE_ABORT();
      }
      buildTokenStreamFrontier(header, false, context, scope->second);
      preparedAny = true;
    }

    for (SgIncludeFile *child : includeFile->get_include_file_list()) {
      if (child == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[include-tree]: include=%s contains a "
                "null child\n",
                includeFile->get_filename().str());
        ROSE_ABORT();
      }
      worklist.push_back(child);
    }
  }

  for (SgSourceFile *header : materializedHeaders) {
    ASSERT_not_null(header);
    if (header->get_associated_include_file() == nullptr ||
        !context.hasFile(header)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-context]: header=%s source=%p has "
              "no canonical include edge with a prepared token frontier\n",
              header->getFileName().c_str(), static_cast<void *>(header));
      ROSE_ABORT();
    }
  }

  return preparedAny;
}
} // namespace

//-----------------------------------------------------------------------------------
//  Unparser::Unparser
//
//  Constructor that takes a SgFile*, iostream*, ROSEAttributesList*,
//  Unparser_Opt, and int. All other fields are set to NULL or 0.
//-----------------------------------------------------------------------------------
// [DT] Old version: Unparser::Unparser(SgFile* nfile, iostream* nos,
//                         ROSEAttributesList* nlist, Unparser_Opt nopt, int
//                         nline) {
// Unparser::Unparser(SgFile* nfile, ostream* nos, char *fname,
//                    ROSEAttributesList* nlist,
//                    ROSEAttributesListContainer* nlistList,
//                    Unparser_Opt nopt, int nline)
// Unparser::Unparser(SgFile* nfile, ostream* nos, char *fname,
//                    Unparser_Opt nopt, int nline)
// Unparser::Unparser( ostream* nos, char *fname, Unparser_Opt nopt, int nline,
//                    UnparseFormatHelp *h, UnparseDelegate* r)

// DQ (8/19/2007): I have removed the "int nline" function parameter becuase it
// was part of an old mechanism to unparse a specific line, now replaced by the
// unparseToString() member function on each IR node. Unparser::Unparser(
// ostream* nos, string fname, Unparser_Opt nopt, int nline, UnparseFormatHelp
// *h, UnparseDelegate* r)
Unparser::Unparser(ostream *nos, string fname, Unparser_Opt nopt,
                   UnparseFormatHelp *h, UnparseDelegate *r,
                   const UnparsePreprocessingInfoRewriteMap *rewrites,
                   NameQualificationContext *nameQualifications,
                   const TokenUnparseFrontierContext *tokenFrontiers)
    : u_type(nullptr), u_name(nullptr), u_debug(nullptr), u_sage(nullptr),
      u_exprStmt(nullptr), u_fortran_type(nullptr),
      u_fortran_locatedNode(nullptr), opt(nopt), cur_index(0),
      prevdir_was_cppDeclaration(false), currentFile(nullptr), cur(nos, h),
      delegate(r), embedColorCodesInGeneratedCode(0),
      generateSourcePositionCodes(0),
      nameQualifications(nameQualifications != nullptr
                             ? nameQualifications
                             : &ownedNameQualifications),
      preprocessingInfoRewrites(rewrites),
      tokenUnparseFrontiers(tokenFrontiers),
      fortranDirectiveKind(FortranDirectiveKind::none) {
  u_type = new Unparse_Type(this);
  u_name = new Unparser_Nameq(this, *this->nameQualifications);
  // u_support  = new Unparse_Support(this);
  u_debug = new Unparse_Debug(this);
  u_sage = new Unparse_MOD_SAGE(this);
  u_exprStmt = new Unparse_ExprStmt(this, fname);

  // DQ (8/14/2007): I have added this here to be consistant, but I question if
  // this is a good design! UnparseFortran_type* u_fortran_type;
  // FortranCodeGeneration_locatedNode* u_fortran_locatedNode;
  u_fortran_type = new UnparseFortran_type(this);
  u_fortran_locatedNode = new FortranCodeGeneration_locatedNode(this, fname);

  // ASSERT_not_null(nfile);
  ASSERT_not_null(nos);

  if (h != nullptr) {
    cur.set_linewrap(cur.get_indentstop());
  }

  // ASSERT_not_null(nlist);
  // file       = nfile;
  //   primary_os = nos; // [DT] 3/9/2000
  //
  // [DT] 3/17/2000 -- Should be careful here.  fname could include a path,
  //      and I think currentOutputFileName should be in the local directory,
  //      or a subdirectory of the local directory, perhaps.
  //
  //   strcpy(primaryOutputFileName, fname);
  // strcpy(currentOutputFileName, fname.c_str());

  // dir_list         = nlist;
  // dir_listList     = nlistList;
  // MK: If overload option is set true, the keyword "operator" occurs in the
  // output. Usually, that's not what you want, but it can be used to avoid a
  // current bug, see file TODO_MK. The default is to set this flag to false,
  // see file preprocessorSupport.C in the src directory
  // opt.set_overload_opt(true);

  // DQ (8/19/2007): Removed this old unpasing mechanism.
  // line_to_unparse  = nline;
  // ltu  = nline;
}

SgAuxiliaryDeclarationList *
Unparser::requireExactAuxiliaryDeclarationOwner(SgNode *node,
                                                const char *contract) {
  ASSERT_not_null(node);
  ASSERT_not_null(contract);

  auto cached = exactAuxiliaryOwners.find(node);
  if (cached != exactAuxiliaryOwners.end()) {
    return cached->second;
  }

  std::vector<const SgNode *> uncached_path;
  std::unordered_set<const SgNode *> visited;
  SgAuxiliaryDeclarationList *owner = nullptr;
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
    uncached_path.push_back(child);
    auto known = exactAuxiliaryOwners.find(parent);
    if (known != exactAuxiliaryOwners.end()) {
      owner = known->second;
      break;
    }

    SgAuxiliaryDeclarationList *container =
        isSgAuxiliaryDeclarationList(parent);
    if (container == nullptr) {
      continue;
    }
    SgDeclarationStatement *declaration = isSgDeclarationStatement(child);
    SgScopeStatement *scope = isSgScopeStatement(container->get_parent());
    const SgDeclarationStatementPtrList &declarations =
        container->get_declarations();
    if (declaration == nullptr || scope == nullptr ||
        scope->get_auxiliary_declarations() != container ||
        std::count(declarations.begin(), declarations.end(), declaration) !=
            1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: node=%p type=%s has malformed "
              "auxiliary declaration ownership\n",
              contract, static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    owner = container;
    uncached_path.push_back(container);
    break;
  }

  uncached_path.push_back(node);
  for (const SgNode *path_node : uncached_path) {
    auto inserted = exactAuxiliaryOwners.emplace(path_node, owner);
    if (!inserted.second && inserted.first->second != owner) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: node=%p has conflicting cached "
              "auxiliary owners=%p/%p\n",
              contract, static_cast<const void *>(path_node),
              static_cast<void *>(inserted.first->second),
              static_cast<void *>(owner));
      ROSE_ABORT();
    }
  }
  return owner;
}

void Unparser::requireExactStatementChild(SgNode *parent,
                                          SgStatement *statement,
                                          const char *contract) {
  ASSERT_not_null(parent);
  ASSERT_not_null(statement);
  ASSERT_not_null(contract);

  auto found = exactChildOwnershipIndices.find(parent);
  if (found == exactChildOwnershipIndices.end()) {
    ExactChildOwnershipIndex index;
    const std::vector<SgNode *> successors =
        parent->get_traversalSuccessorContainer();
    index.occurrences.reserve(successors.size());
    for (SgNode *successor : successors) {
      if (successor != nullptr) {
        ++index.occurrences[successor];
      }
    }
    found = exactChildOwnershipIndices.emplace(parent, std::move(index)).first;
  }

  const auto occurrence = found->second.occurrences.find(statement);
  const std::size_t count =
      occurrence != found->second.occurrences.end() ? occurrence->second : 0;
  if (count != 1) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: lexical statement=%p type=%s is "
            "owned %zu times by parent=%p type=%s; expected exactly one\n",
            contract, static_cast<void *>(statement),
            statement->class_name().c_str(), count, static_cast<void *>(parent),
            parent->class_name().c_str());
    ROSE_ABORT();
  }
}

Unparser::FortranDirectiveKind Unparser::getFortranDirectiveKind() const {
  return fortranDirectiveKind;
}

Unparser::FortranDirectiveContextGuard::FortranDirectiveContextGuard(
    Unparser *unparser, FortranDirectiveKind kind)
    : unparser_(unparser), kind_(kind) {
  ASSERT_not_null(unparser_);
  if (kind_ == FortranDirectiveKind::none) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-directive-context]: cannot enter "
            "an empty directive context\n");
    ROSE_ABORT();
  }
  if (unparser_->currentFile == nullptr ||
      !unparser_->currentFile->get_Fortran_only()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-directive-context]: directive "
            "context requires an exact Fortran source file\n");
    ROSE_ABORT();
  }

  previous_ = unparser_->getFortranDirectiveKind();
  if (previous_ != FortranDirectiveKind::none && previous_ != kind_) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-directive-context]: conflicting "
            "nested directive kinds\n");
    ROSE_ABORT();
  }
  unparser_->setFortranDirectiveKind(kind_);
  active_ = true;
}

Unparser::FortranDirectiveContextGuard::~FortranDirectiveContextGuard() {
  if (!active_) {
    return;
  }
  if (unparser_->getFortranDirectiveKind() != kind_) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-directive-context]: directive "
            "context changed before its emission completed\n");
    ROSE_ABORT();
  }
  unparser_->setFortranDirectiveKind(previous_);
}

void Unparser::setFortranDirectiveKind(FortranDirectiveKind kind) {
  switch (kind) {
  case FortranDirectiveKind::none:
  case FortranDirectiveKind::openmp:
  case FortranDirectiveKind::openacc:
    fortranDirectiveKind = kind;
    return;
  }

  fprintf(stderr, "REX_UNPARSE_INVARIANT[fortran-directive-context]: invalid "
                  "directive kind\n");
  ROSE_ABORT();
}

void Unparser::requireFortranDirectiveKind(FortranDirectiveKind kind) const {
  if (currentFile == nullptr || !currentFile->get_Fortran_only() ||
      kind == FortranDirectiveKind::none || fortranDirectiveKind != kind) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-directive-context]: directive "
            "emission has no matching semantic context\n");
    ROSE_ABORT();
  }
}

const char *
Unparser::fortranDirectiveContinuationPrefix(bool fixedFormat) const {
  switch (fortranDirectiveKind) {
  case FortranDirectiveKind::none:
    return nullptr;
  case FortranDirectiveKind::openmp:
    return fixedFormat ? "!$omp&" : "!$omp& ";
  case FortranDirectiveKind::openacc:
    return fixedFormat ? "!$acc&" : "!$acc& ";
  }

  fprintf(stderr, "REX_UNPARSE_INVARIANT[fortran-directive-context]: invalid "
                  "directive kind\n");
  ROSE_ABORT();
}

Unparser::FortranLineWrapLayout Unparser::fortranLineWrapLayout() const {
  if (currentFile == nullptr || !currentFile->get_Fortran_only()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-line-wrap]: requested layout "
            "outside Fortran file emission\n");
    ROSE_ABORT();
  }
  const std::optional<int> configuredColumns = cur.get_linewrap();
  if (!configuredColumns.has_value()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-line-wrap]: requested layout "
            "while wrapping is disabled\n");
    ROSE_ABORT();
  }

  FortranLineWrapLayout layout;
  switch (currentFile->get_outputFormat()) {
  case SgFile::e_fixed_form_output_format:
    layout.fixedFormat = true;
    layout.physicalColumns =
        std::min(*configuredColumns, MAX_F90_LINE_LEN_FIXED);
    layout.textColumns = layout.physicalColumns;
    break;
  case SgFile::e_free_form_output_format:
    layout.fixedFormat = false;
    layout.physicalColumns =
        std::min(*configuredColumns, MAX_F90_LINE_LEN_FREE);
    layout.textColumns = layout.physicalColumns - 1;
    break;
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-line-wrap]: output format is "
            "neither fixed nor free form\n");
    ROSE_ABORT();
  }

  if (layout.physicalColumns <= 0 || layout.textColumns <= 0) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-line-wrap]: configured width "
            "cannot hold Fortran source text\n");
    ROSE_ABORT();
  }
  return layout;
}

void Unparser::emitFortranRawText(const std::string &text) {
  cur.emit_raw_text(text);
}

void Unparser::emitFortranText(const std::string &text) {
  if (currentFile == nullptr || !currentFile->get_Fortran_only()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-text-context]: emission requires "
            "an active Fortran source file\n");
    ROSE_ABORT();
  }
  if (text.empty()) {
    return;
  }
  if (cur.get_compact_output()) {
    cur << text;
    return;
  }
  if (!cur.get_linewrap().has_value()) {
    emitFortranRawText(text);
    return;
  }

  const size_t newline = text.find('\n');
  if (newline != std::string::npos) {
    std::string line = text.substr(0, newline);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    emitFortranText(line);
    cur.insert_newline(1);
    emitFortranText(text.substr(newline + 1));
    return;
  }
  if (text.find('\r') != std::string::npos) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-line-wrap]: carriage return is "
            "not followed by a newline\n");
    ROSE_ABORT();
  }

  const FortranLineWrapLayout layout = fortranLineWrapLayout();
  const int usedColumns = cur.current_col();
  if (usedColumns < 0 || usedColumns > layout.textColumns) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-line-wrap]: current output column "
            "is outside the configured text width\n");
    ROSE_ABORT();
  }
  const int availableColumns = layout.textColumns - usedColumns;
  if (text.size() <= static_cast<size_t>(availableColumns)) {
    emitFortranRawText(text);
    return;
  }

  const char *directivePrefix =
      fortranDirectiveContinuationPrefix(layout.fixedFormat);
  const size_t prefixSize =
      directivePrefix != nullptr
          ? std::char_traits<char>::length(directivePrefix)
      : layout.fixedFormat ? 6
                           : 1;
  if (prefixSize + text.size() > static_cast<size_t>(layout.textColumns)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-line-wrap]: indivisible %zu-byte "
            "token cannot fit after a %zu-byte continuation prefix in %d "
            "text columns\n",
            text.size(), prefixSize, layout.textColumns);
    ROSE_ABORT();
  }

  if (layout.fixedFormat) {
    if (usedColumns == 0 && text.front() != ' ') {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[fortran-line-wrap]: fixed-format "
                      "column-one text exceeds the configured width\n");
      ROSE_ABORT();
    }
    cur.insert_newline(1);
    emitFortranRawText(directivePrefix != nullptr ? directivePrefix : "     &");
  } else {
    emitFortranRawText("&");
    cur.insert_newline(1);
    emitFortranRawText(directivePrefix != nullptr ? directivePrefix : "&");
  }
  emitFortranRawText(text);
}

void Unparser::emitFortranComment(const std::string &text) {
  if (currentFile == nullptr || !currentFile->get_Fortran_only() ||
      text.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: comment emission "
            "requires nonempty text and an active Fortran file\n");
    ROSE_ABORT();
  }
  if (fortranDirectiveKind != FortranDirectiveKind::none) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: ordinary comment "
            "emission occurred inside a directive context\n");
    ROSE_ABORT();
  }

  bool fixedFormat = false;
  switch (currentFile->get_outputFormat()) {
  case SgFile::e_fixed_form_output_format:
    fixedFormat = true;
    break;
  case SgFile::e_free_form_output_format:
    break;
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: output format is "
            "neither fixed nor free form\n");
    ROSE_ABORT();
  }

  size_t lineStart = 0;
  while (lineStart < text.size()) {
    const size_t newline = text.find('\n', lineStart);
    const bool hasNewline = newline != std::string::npos;
    size_t lineEnd = hasNewline ? newline : text.size();
    if (hasNewline && lineEnd > lineStart && text[lineEnd - 1] == '\r') {
      --lineEnd;
    }
    const std::string line = text.substr(lineStart, lineEnd - lineStart);
    if (line.find('\r') != std::string::npos) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: carriage return "
              "is not followed by a newline\n");
      ROSE_ABORT();
    }

    if (!line.empty()) {
      const bool isComment =
          fixedFormat
              ? Rose::FortranLineWrapSupport::isFixedFormatCommentLine(line)
              : Rose::FortranLineWrapSupport::isCommentLine(line);
      if (!isComment) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: preprocessing "
                "record is not a Fortran comment\n");
        ROSE_ABORT();
      }

      const size_t marker = line.find_first_not_of(' ');
      if (marker != std::string::npos && marker + 1 < line.size() &&
          line[marker] == '!' && line[marker + 1] == '$') {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: directive text "
                "was presented as an ordinary comment\n");
        ROSE_ABORT();
      }

      if (fixedFormat && cur.current_col() != 0) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: fixed-format "
                "comment does not begin in column one\n");
        ROSE_ABORT();
      }

      if (!cur.get_linewrap().has_value()) {
        emitFortranRawText(line);
      } else {
        const FortranLineWrapLayout layout = fortranLineWrapLayout();
        const int usedColumns = cur.current_col();
        const std::vector<std::string> wrappedLines =
            fixedFormat ? Rose::FortranLineWrapSupport::wrapFixedFormatComment(
                              line, usedColumns, layout.textColumns)
                        : Rose::FortranLineWrapSupport::wrapFreeFormatComment(
                              line, usedColumns, layout.textColumns);
        for (size_t i = 0; i < wrappedLines.size(); ++i) {
          if (i != 0) {
            cur.insert_newline(1);
          }
          emitFortranRawText(wrappedLines[i]);
        }
      }
    }

    if (!hasNewline) {
      break;
    }
    cur.insert_newline(1);
    lineStart = newline + 1;
  }
}

void Unparser::emitFortranCharacterLiteral(const std::string &value,
                                           char delimiter) {
  if (currentFile == nullptr || !currentFile->get_Fortran_only()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-string-context]: emission requires "
            "an active Fortran source file\n");
    ROSE_ABORT();
  }
  if (delimiter != '\'' && delimiter != '"') {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-string-delimiter]: character "
            "literal has no exact apostrophe or quotation-mark delimiter\n");
    ROSE_ABORT();
  }

  // SgStringVal stores the decoded character payload.  Fortran represents the
  // active delimiter inside that payload by doubling it; emitting the decoded
  // value directly produces malformed source such as 'isn't'.
  std::string literal;
  literal.reserve(value.size() + 2);
  literal.push_back(delimiter);
  for (char character : value) {
    literal.push_back(character);
    if (character == delimiter) {
      literal.push_back(character);
    }
  }
  literal.push_back(delimiter);

  const std::vector<size_t> lexicalBoundaries =
      Rose::FortranLineWrapSupport::stringLiteralLexicalBoundaries(literal,
                                                                   delimiter);
  auto isSafeBoundary = [&](size_t boundary) {
    return std::binary_search(lexicalBoundaries.begin(),
                              lexicalBoundaries.end(), boundary);
  };

  if (cur.get_compact_output()) {
    cur.emit_literal(literal);
    return;
  }

  if (!cur.get_linewrap().has_value()) {
    emitFortranRawText(literal);
    return;
  }

  const FortranLineWrapLayout layout = fortranLineWrapLayout();
  int usedColumns = cur.current_col();
  if (usedColumns < 0 || usedColumns > layout.textColumns) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-string-wrap]: current output "
            "column is outside the configured text width\n");
    ROSE_ABORT();
  }

  size_t offset = 0;
  while (offset < literal.size()) {
    const size_t capacity =
        static_cast<size_t>(layout.textColumns - usedColumns);
    const size_t furthest = std::min(literal.size(), offset + capacity);
    size_t boundary = furthest;
    while (boundary > offset && !isSafeBoundary(boundary)) {
      --boundary;
    }

    if (boundary == literal.size()) {
      emitFortranRawText(literal.substr(offset));
      return;
    }
    if (boundary > offset) {
      emitFortranRawText(literal.substr(offset, boundary - offset));
      offset = boundary;
    } else if (usedColumns == 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[fortran-string-wrap]: configured width "
              "cannot hold the next string-literal lexical unit\n");
      ROSE_ABORT();
    }

    const char *directivePrefix =
        fortranDirectiveContinuationPrefix(layout.fixedFormat);
    if (layout.fixedFormat) {
      cur.insert_newline(1);
      emitFortranRawText(directivePrefix != nullptr ? directivePrefix
                                                    : "     &");
    } else {
      emitFortranRawText("&");
      cur.insert_newline(1);
      if (directivePrefix != nullptr) {
        emitFortranRawText(directivePrefix);
      }
      emitFortranRawText("&");
    }

    usedColumns = cur.current_col();
    if (usedColumns < 0 || usedColumns >= layout.textColumns) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[fortran-string-wrap]: continuation "
              "prefix leaves no room for string text\n");
      ROSE_ABORT();
    }
    size_t nextBoundary = offset + 1;
    while (nextBoundary <= literal.size() && !isSafeBoundary(nextBoundary)) {
      ++nextBoundary;
    }
    if (nextBoundary > literal.size() ||
        nextBoundary - offset >
            static_cast<size_t>(layout.textColumns - usedColumns)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[fortran-string-wrap]: continuation "
              "line cannot hold the next string-literal lexical unit\n");
      ROSE_ABORT();
    }
  }
}

//-----------------------------------------------------------------------------------
//  Unparser::~Unparser
//
//  Destructor
//-----------------------------------------------------------------------------------
Unparser::~Unparser() {
  if (fortranDirectiveKind != FortranDirectiveKind::none) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-directive-context]: unparser was "
            "destroyed inside a directive context\n");
    ROSE_ABORT();
  }
  delete u_type;
  delete u_name;
  delete u_debug;
  delete u_sage;
  delete u_exprStmt;
  delete u_fortran_type;
  delete u_fortran_locatedNode;
}

UnparseFormat &Unparser::get_output_stream() { return cur; }

NameQualificationContext &Unparser::get_name_qualification_context() {
  ASSERT_not_null(nameQualifications);
  return *nameQualifications;
}

std::string
Unparser::preprocessingInfoText(const PreprocessingInfo *info) const {
  if (info == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-info]: null preprocessing "
            "record\n");
    ROSE_ABORT();
  }
  if (preprocessingInfoRewrites != nullptr) {
    const auto rewrite = preprocessingInfoRewrites->find(info);
    if (rewrite != preprocessingInfoRewrites->end()) {
      return rewrite->second;
    }
  }
  return info->getString();
}

void Unparser::claimPreprocessingInfoReceipt(const PreprocessingInfo *record,
                                             const SgLocatedNode *owner,
                                             int relativePosition) {
  if (record == nullptr || owner == nullptr ||
      record->getAttachedOwner() != owner ||
      static_cast<int>(record->getRelativePosition()) != relativePosition) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-receipt-owner]: record=%p "
            "requested-owner=%p recorded-owner=%p requested-position=%d "
            "recorded-position=%d has no exact emission owner\n",
            static_cast<const void *>(record), static_cast<const void *>(owner),
            record != nullptr
                ? static_cast<const void *>(record->getAttachedOwner())
                : nullptr,
            relativePosition,
            record != nullptr ? static_cast<int>(record->getRelativePosition())
                              : -1);
    ROSE_ABORT();
  }

  const auto inserted = preprocessingInfoReceipts.emplace(
      record, PreprocessingInfoReceipt{owner, relativePosition});
  if (!inserted.second) {
    const PreprocessingInfoReceipt &first = inserted.first->second;
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-receipt-duplicate]: "
            "one record was claimed twice in this unparser invocation\n");
    fprintf(stderr,
            "REX_UNPARSE_DETAIL[preprocessing-receipt-duplicate]: "
            "record=%p text=%s first-owner=%p/%s first-position=%d "
            "duplicate-owner=%p/%s duplicate-position=%d\n",
            static_cast<const void *>(record), record->getString().c_str(),
            static_cast<const void *>(first.owner),
            first.owner != nullptr ? first.owner->class_name().c_str()
                                   : "<null>",
            first.relativePosition, static_cast<const void *>(owner),
            owner->class_name().c_str(), relativePosition);
    ROSE_ABORT();
  }
}

const TokenUnparseFrontierFileContext &
Unparser::tokenUnparseFrontier(SgSourceFile *sourceFile) const {
  ASSERT_not_null(sourceFile);
  if (tokenUnparseFrontiers == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-frontier]: file=%s unparser has no "
            "invocation frontier context\n",
            sourceFile->getFileName().c_str());
    ROSE_ABORT();
  }
  return tokenUnparseFrontiers->file(sourceFile);
}

const TokenUnparseFrontierContext &Unparser::tokenUnparseContext() const {
  if (tokenUnparseFrontiers == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[token-frontier]: unparser has no "
                    "invocation frontier context\n");
    ROSE_ABORT();
  }
  return *tokenUnparseFrontiers;
}

bool Unparser::isPartOfTransformation(SgLocatedNode *n) {
  // return (n->get_file_info()==0 ||
  // n->get_file_info()->get_isPartOfTransformation());

  ASSERT_not_null(n);
  ASSERT_not_null(n->get_file_info());

  // DQ (5/10/2005): For now let's check both, but I think we want to favor
  // isTransfrmation() over get_isPartOfTransformation() in the future.
  return (n->get_file_info()->isTransformation() ||
          n->get_file_info()->get_isPartOfTransformation());
}

bool Unparser::isCompilerGenerated(SgLocatedNode *n) {
  ASSERT_not_null(n);
  ASSERT_not_null(n->get_file_info());

  // DQ (5/22/2005): Support for including any compiler generated code (such as
  // instatiated templates).
  return n->get_file_info()->isCompilerGenerated();
}

void Unparser::computeNameQualification(
    SgSourceFile *file, NameQualificationContext &nameQualifications) {
  // DQ (8/7/2018): Refactored code for name qualification (so that we can call
  // it once before all files are unparsed (where we unparse multiple files
  // because fo the use of header file unparsing)).

  ASSERT_not_null(file);
  if (file->get_skip_unparse()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[skipped-file-qualification]: file=%s was "
            "submitted for output qualification despite its explicit "
            "skip_unparse role\n",
            file->getFileName().c_str());
    ROSE_ABORT();
  }

  // DQ (8/7/2018): Copied this logic from where computeNameQualification() was
  // originally called to limit the number of parameters in the function API.
  bool isCxxFile = false;
  if (((file->get_Cxx_only() == true) &&
       (file->get_outputLanguage() == SgFile::e_default_language)) ||
      (file->get_outputLanguage() == SgFile::e_Cxx_language)) {
    isCxxFile = true;
  }

  // DQ (11/10/2007): Moved computation of hidden list from astPostProcessing.C
  // to unparseFile so that it will be called AFTER any transformations and
  // immediately before code generation where it is really required.  This part
  // of a fix for Liao's outliner, but should be useful for numerous
  // transformations.  This also make simple analysis much cheaper since the
  // hidel list computation is expensive (in this implementation). DQ
  // (8/6/2007): Only compute the hidden lists if working with C++ code! if
  // (isCxxFile == true)
  // Contextual type identities are consumed by both the C and C++ unparsers.
  // In particular, a C function-pointer declarator has typed parameter
  // positions whose (possibly empty) qualification result is keyed to the
  // declaration that emits it.  Skipping this traversal for C left those
  // exact use-site records absent after transformations such as outlining.
  {
    // DQ (5/15/2011): Test clearing the mangled name map.
    // printf ("Calling SgNode::clearGlobalMangledNameMap() \n");

    // DQ (6/25/2011): Test if this is required...it works, I think we don't
    // need to clear the global managled name table...
    // SgNode::clearGlobalMangledNameMap();

    // Build the local set to use to record when declaration that might required
    // qualified references have been seen.
    NameQualificationTraversal::NameQualificationSetType referencedNameSet;

    // DQ (6/11/2015): Added to support debugging the difference between C and
    // C++ support for token-based unparsing.
    std::set<SgLocatedNode *> modifiedLocatedNodesSet_1 =
        SageInterface::collectModifiedLocatedNodes(file);
    size_t numberOfModifiedNodesBeforeNameQualification =
        modifiedLocatedNodesSet_1.size();
    if (SgProject::get_verbose() > 0) {
      printf("Calling name qualification support \n");
    }

    generateNameQualificationSupport(file, referencedNameSet,
                                     nameQualifications);

    if (SgProject::get_verbose() > 0) {
      printf("DONE: Calling name qualification support \n");
    }

    // DQ (6/5/2007): We actually need this now since the hidden lists are not
    // pushed to lower scopes where they are required. DQ (5/22/2007): Added
    // support for passing hidden list information about types, declarations and
    // elaborated types to child scopes.
    if (isCxxFile) {
      propagateHiddenListData(file);
    }

    // DQ (6/11/2015): Added to support debugging the difference between C and
    // C++ support for token-based unparsing.
    std::set<SgLocatedNode *> modifiedLocatedNodesSet_2 =
        SageInterface::collectModifiedLocatedNodes(file);
    size_t numberOfModifiedNodesAfterNameQualification =
        modifiedLocatedNodesSet_2.size();

    // DQ (6/11/2015): Introduce error checking on the AST if we are using the
    // token-based unparsing.
    if ((file->get_unparse_tokens() == true) &&
        (numberOfModifiedNodesAfterNameQualification !=
         numberOfModifiedNodesBeforeNameQualification)) {
      printf("In Unparser::computeNameQualification(): "
             "numberOfModifiedNodesBeforeNameQualification = %zu "
             "numberOfModifiedNodesAfterNameQualification = %zu \n",
             numberOfModifiedNodesBeforeNameQualification,
             numberOfModifiedNodesAfterNameQualification);
      printf("ERROR: namequalification step has introduced modified IR nodes "
             "in the AST (a problem for the token-based unparsing) \n");
      ROSE_ABORT();
    }
  }
}

// DQ (9/2/2008): Separate out the details of unparsing source files from binary
// files.
void Unparser::unparseFile(SgSourceFile *file, SgUnparse_Info &info,
                           SgScopeStatement *unparseScope) {
  // ASSERT_not_null(file);
  // unparseFile (file,info);

#define DEBUG_UNPARSE_FILE 0

  ASSERT_not_null(file);
  // DQ (6/5/2021): Save the previous statement that was just unparsed (at this
  // point we just want to clear the value from any other file).
  info.set_previousStatementUnparsedFromTokenStream(false);

  // DQ (10/29/2018): I now think we need to support this mechanism of
  // specifying the scope to be unparsed separately. This is essential to the
  // support for header files representing nested scopes inside of the global
  // scope. Traversing the global scope does not permit these inner nested
  // scopes to be traversed using the unparser.

  // DQ (8/16/2018): We can remove this as part of a more conventional usage
  // with a single SgSourceFile and SgGlobal for each header file.
  // ROSE_ASSERT(unparseScope == NULL);

  // DQ (10/27/2019): This assertion is false for test2 in our
  // UnparseHeadersTests regression tests. DQ (10/22/2019): I think we can
  // specify this (and later cleanup some of the code below).
  // ROSE_ASSERT(unparseScope == NULL);

#if DEBUG_UNPARSE_FILE || 0
  printf("\n\n");
  printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ \n");
  printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ \n");
  printf("In unparseFile(): file = %p filename = %s unparseScope = %p \n", file,
         file->getFileName().c_str(), unparseScope);
  printf(
      " --- file->get_header_file_unparsing_optimization_source_file() = %s \n",
      file->get_header_file_unparsing_optimization_source_file() ? "true"
                                                                 : "false");
  printf(
      " --- file->get_header_file_unparsing_optimization_header_file() = %s \n",
      file->get_header_file_unparsing_optimization_header_file() ? "true"
                                                                 : "false");
  if (unparseScope != NULL) {
    printf("   --- unparseScope = %p = %s \n", unparseScope,
           unparseScope->class_name().c_str());
  }
  printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ \n");
  printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ \n");
#endif

  SgSourceFile *sourceFile = info.get_current_source_file();
  if (sourceFile == nullptr || sourceFile != file) {
    const std::string requested_name =
        std::filesystem::path(file->getFileName()).filename().string();
    const std::string inherited_name =
        sourceFile != nullptr ? std::filesystem::path(sourceFile->getFileName())
                                    .filename()
                                    .string()
                              : "<null>";
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[current-source-file]: unparse-file=%s "
            "inherited=%s must identify the same source file\n",
            requested_name.c_str(), inherited_name.c_str());
    ROSE_ABORT();
  }

  // DQ (4/24/2021): Sorting out the header file optimization, so that we can
  // correctly handle when both ON or OFF. This data member appears to always be
  // false.
  // ROSE_ASSERT(file->get_header_file_unparsing_optimization_header_file() ==
  // false);
#if DEBUG_UNPARSE_FILE
  if (file->get_header_file_unparsing_optimization_header_file() == true) {
    printf("Found case of "
           "file->get_header_file_unparsing_optimization_header_file() == true "
           "\n");
  }
#endif

  {
    SgGlobal *tmp_globalScope = isSgGlobal(file->get_globalScope());
    if (tmp_globalScope != NULL) {
      // DQ (8/13/2018): Both of these should be true.
      ASSERT_not_null(tmp_globalScope->get_parent());
      // ROSE_ASSERT(globalScope->get_parent() == sourceFile);
    } else {
      printf("Exiting because I think this is an error! \n");
      ROSE_ABORT();
    }
  }

  // Detect reuse of an Unparser object with a different file
  ROSE_ASSERT(currentFile == NULL);

  currentFile = file;
  ASSERT_not_null(currentFile);

  // What kind of language are we unparsing.  These are more robust tests
  // for multiple files of multiple languages than using SageInterface.  This
  // is also how the language-dependent parser is called.
  bool isFortranFile = false;
  if (((file->get_Fortran_only() == true) &&
       (file->get_outputLanguage() == SgFile::e_default_language)) ||
      (file->get_outputLanguage() == SgFile::e_Fortran_language)) {
    isFortranFile = true;
  }
  bool isCfile = false;
  if (((file->get_C_only() == true) &&
       (file->get_outputLanguage() == SgFile::e_default_language)) ||
      (file->get_outputLanguage() == SgFile::e_C_language)) {
    isCfile = true;
  }
  bool isCxxFile = false;
  if (((file->get_Cxx_only() == true) &&
       (file->get_outputLanguage() == SgFile::e_default_language)) ||
      (file->get_outputLanguage() == SgFile::e_Cxx_language)) {
    isCxxFile = true;
  }
  // Can only be parsing to one language!
  ROSE_ASSERT(((int)isFortranFile + (int)isCfile + (int)isCxxFile) <= 1);

  // DQ (6/30/2013): Added support to time the unparsing of the file (name
  // qualification will be nested in this time).
  TimingPerformance timer("Unparse File:");

  // DQ (8/7/2018): use of new data member to explicitly mark SgSourceFile as a
  // header file. bool isHeaderFile = file->get_isHeaderFile();
  // ROSE_ASSERT(file->get_isHeaderFile() == true);

#if DEBUG_UNPARSE_FILE || 0
  printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
  printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
  printf("In Unparser::unparseFile(): file->get_unparse_tokens() = %s \n",
         file->get_unparse_tokens() ? "true" : "false");
  printf("isCfile                    = %s \n", isCfile ? "true" : "false");
  printf("isCxxFile                  = %s \n", isCxxFile ? "true" : "false");
  printf("file->get_unparse_tokens() = %s \n",
         file->get_unparse_tokens() ? "true" : "false");
  printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
  printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
#endif

  // Token mode is a per-file contract established by the frontend.  A
  // transformation narrows the replay frontier; it must not silently change
  // the requested unparse mode for the entire file.
  if ((isCfile || isCxxFile) && file->get_unparse_tokens() == true) {
#define DEBUG_UNPARSE_TOKENS 0

    // This is only currently being tested and evaluated for C language (should
    // also work for C++, but not yet for Fortran).
#if DEBUG_UNPARSE_TOKENS
    printf("In Unparser::unparseFile(): END: this->currentFile = %p "
           "this->currentFile->getFileName() = %s \n",
           this->currentFile, this->currentFile->getFileName().c_str());
#endif
#if DEBUG_UNPARSE_TOKENS
    printf("In Unparser::unparseFile(): Building token stream mapping "
           "frontier: filename = %s \n",
           file->getFileName().c_str());
#endif

    // DQ (8/23/2018): This is not the way to access the token stream.
    ROSE_ASSERT(mapFilenameToAttributes.empty() == true);

    // DQ (8/23/2018): I think this should now be computed when the add the
    // SgHeaderBody. DQ (8/23/2018): I think this is the way to generate the
    // token list (and all comments and CPP directives). ROSEAttributesList*
    // attributeList = AttachPreprocessingInfoTreeTrav::getListOfAttributes (
    // int currentFileNameId ); buildTokenStreamMapping(file);

    // DQ (12/2/2018): This can be empty for an empty file (see test in:
    // roseTests/astTokenStreamTests). DQ (8/23/2018): This should be a
    // non-empty list if we are using the token stream. ROSE_ASSERT
    // (file->get_token_list().empty() == false);

    // This function builds the data base (STL map) for the different
    // subsequences ranges of the token stream. and attaches the toke stream to
    // the SgSourceFile IR node.

    // DQ (8/8/2018): It also marks IR nodes as transformations where they are
    // detected to be a part of any modifications (where the isModified flag
    // detected to be true).

    SgGlobal *nested_globalScope = isSgGlobal(file->get_globalScope());

    // DQ (8/17/2018): Every source file should have a global scope.
    ASSERT_not_null(nested_globalScope);

    if (nested_globalScope != NULL) {
      // DQ (8/13/2018): Both of these should be true.
      ASSERT_not_null(nested_globalScope->get_parent());
      // ROSE_ASSERT(globalScope->get_parent() == sourceFile);
    }

#if DEBUG_UNPARSE_TOKENS || 0
    printf("###################################################################"
           "############################### \n");
    printf(" The token processing is now done in the secondaryFileProcessing "
           "(so we do not need to do it here) \n");
    printf("###################################################################"
           "############################### \n");
#endif
#if DEBUG_UNPARSE_TOKENS
    printf("*******************************************************************"
           " \n");
    printf("In Unparser::unparseFile(): Building token stream mapping "
           "frontier! \n");
    printf("   --- file = %s \n", file->getFileName().c_str());
    printf("*******************************************************************"
           " \n");
#endif

    // *** Next we have to attached the data base ***
    // buildTokenStreamMapping(file);

#if DEBUG_UNPARSE_TOKENS
    printf("file->get_unparseHeaderFiles() = %s \n",
           headerSourceFile->get_unparseHeaderFiles() ? "true" : "false");
#endif

#if DEBUG_UNPARSE_FILE
    printf("In Unparser::unparseFile(): END: this->currentFile = %p "
           "this->currentFile->getFileName() = %s \n",
           this->currentFile, this->currentFile->getFileName().c_str());
#endif
  } else {
    if (file->get_unparse_tokens() == true) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-language]: file=%s token replay "
              "was requested for a non-C/C++ output language\n",
              file->getFileName().c_str());
      ROSE_ABORT();
    }
  }

  if (file->get_markGeneratedFiles() == true) {
    // Output marker to identify code generated by ROSE (causes "#define
    // ROSE_GENERATED_CODE" to be placed at the top of the file). printf
    // ("Output marker to identify code generated by ROSE \n");
    u_exprStmt->markGeneratedFile();
  }

  // SgScopeStatement* globalScope = (SgScopeStatement*) (&(file->root()));
  SgScopeStatement *globalScope = file->get_globalScope();
  ASSERT_not_null(globalScope);

  // Make sure that both the C/C++ and Fortran unparsers are present!
  ASSERT_not_null(u_exprStmt);
  ASSERT_not_null(u_fortran_locatedNode);

  ROSE_ASSERT(file->get_outputLanguage() != SgFile::e_error_language);

  // DQ (8/29/2017): Adding more general handling for language support.

  // Not clear if we want this, translators might not want to have this
  // constraint. But use this for debugging initially.  I would like to see the
  // default setting for the output language set to be the same as the input
  // language and then a translator could change this setting.  If their is a
  // different between the input language and the output language then a
  // collection of tests should be generated that verify compliance of the AST
  // with the output language specified.

  ROSE_ASSERT(file->get_inputLanguage() == file->get_outputLanguage());

  // switch (file->get_outputLanguage())
  // switch (file->get_inputlanguage())
  switch (file->get_outputLanguage()) {
  case SgFile::e_error_language: {
    printf("Error: SgFile::e_error_language detected in unparser \n");
    ROSE_ABORT();
  }

  case SgFile::e_default_language: {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-language]: file=%s has default "
            "output language\n",
            file->getFileName().c_str());
    ROSE_ABORT();
  }

  case SgFile::e_C_language:
  case SgFile::e_Cxx_language: {
    // printf ("Error: SgFile::e_C_language or SgFile::e_Cxx_language detected
    // in unparser (unparser not implemented, unparsing ignored) \n");
#if DEBUG_UNPARSE_FILE
    printf("case SgFile::e_C/Cxx_language: Unparse using C/C++ unparser by "
           "default: unparseScope = %p \n",
           unparseScope);
    printf("In Unparser::unparseFile(): case SgFile::e_C/Cxx_language: "
           "this->currentFile->getFileName() = %s \n",
           this->currentFile->getFileName().c_str());
#endif
    // DQ (10/29/2018): I now think we need to support this mechanism of
    // specifying the scope to be unparsed separately. This is essential to the
    // support for header files representing nested scopes inside of the global
    // scope. Traversing the global scope does not permit these inner nested
    // scopes to be traversed using the unparser.

    // DQ (8/16/2018): With the more conventional usage we have a specific
    // SgSourceFile and SgGlobal for each header file. ROSE_ASSERT(unparseScope
    // == NULL);

    // negara1 (06/29/2011): If unparseScope is provided, unparse it. Otherwise,
    // unparse the global scope (the default behavior).
    if (unparseScope != NULL) {
#if DEBUG_UNPARSE_FILE
      printf("In Unparser::unparseFile(): unparseScope = %p = %s \n",
             unparseScope, unparseScope->class_name().c_str());
#endif
      // DQ (8/6/2018): Use this approach to unparse the statement (a different
      // SgSourceFile using a different filename with the original global
      // scope). unp->opt.get_unparse_includes_opt() == true) SgUnparse_Info
      // ninfo(info); ninfo.set_unparse_includes_opt(true);

      // DQ (8/6/2018): Check the currentFile data member.
      ASSERT_not_null(this->currentFile);

      if (unparseScope == globalScope) {
        // A materialized header shares the translation unit's lexical global
        // scope, but owns a distinct token stream. Enter through the normal
        // global-scope unparser so its file prefix and trailing tokens are
        // emitted from the header's SgSourceFile context.
        u_exprStmt->unparseStatement(globalScope, info);
      } else {
        SgStatementPtrList statements = unparseScope->generateStatementList();
        for (SgStatement *statement : statements) {
#if DEBUG_UNPARSE_FILE
          printf("Unparsing the statements on the unparseScope statement by "
                 "statement (what about comments) statement = %p = %s \n",
                 statement, statement->class_name().c_str());
#endif
          ASSERT_not_null(statement);
          u_exprStmt->unparseStatement(statement, info);
        }
      }

    } else {
      // DQ (5/15/2021): This characterizes this false branch.
      ROSE_ASSERT(unparseScope == NULL);
#if DEBUG_UNPARSE_FILE || 0
      printf("In Unparser::unparseFile(): case C/C++: "
             "unparseStatement(globalScope, info): globalScope = %p \n",
             globalScope);
      printf("globalScope->getDeclarationList().size() = %zu \n",
             globalScope->getDeclarationList().size());
      SgGlobal *temp_globalScope = isSgGlobal(globalScope);
      ROSE_ASSERT(temp_globalScope != NULL);
      printf("Global scope being unparsed: temp_globalScope = %p \n",
             temp_globalScope);
      printf("temp_globalScope->get_declarations().size() = %zu \n",
             temp_globalScope->get_declarations().size());
#endif
      ASSERT_not_null(globalScope->get_parent());
#if DEBUG_UNPARSE_FILE
      // if (this->currentFile != globalScope->get_parent())
      {
        printf("Error: In Unparser::unparseFile(): this->currentFile != "
               "globalScope->get_parent() \n");
        printf(" --- this->currentFile         = %p \n", this->currentFile);
        printf(" --- globalScope->get_parent() = %p \n",
               globalScope->get_parent());
        ROSE_ASSERT(this->currentFile != NULL);
        ROSE_ASSERT(globalScope->get_parent() != NULL);

        SgSourceFile *currentSourceFile = isSgSourceFile(this->currentFile);
        ROSE_ASSERT(currentSourceFile != NULL);

        SgGlobal *globalScope_from_currentFile =
            currentSourceFile->get_globalScope();
        printf("globalScope_from_currentFile = %p \n",
               globalScope_from_currentFile);
        printf("globalScope                  = %p \n", globalScope);

        SgSourceFile *source_file_from_currentFile =
            isSgSourceFile(this->currentFile);
        SgSourceFile *source_file_from_globalScope_parent =
            isSgSourceFile(globalScope->get_parent());
        ROSE_ASSERT(source_file_from_currentFile != NULL);
        ROSE_ASSERT(source_file_from_globalScope_parent != NULL);
        printf("source_file_from_currentFile        = %p filename = %s \n",
               source_file_from_currentFile,
               source_file_from_currentFile->getFileName().c_str());
        printf("source_file_from_globalScope_parent = %p filename = %s \n",
               source_file_from_globalScope_parent,
               source_file_from_globalScope_parent->getFileName().c_str());
      }
#endif
      // ROSE_ASSERT(this->currentFile == globalScope->get_parent());
#if DEBUG_UNPARSE_FILE
      if (this->currentFile == globalScope->get_parent()) {
        // DQ (5/15/2021): Swapped the true and false debug statements (I think
        // they were in reverse order). printf ("Unparsing a header file of an
        // AST from a given source file \n");
        printf("Unparsing a source file (different from all other source "
               "files) \n");
      } else {
        // printf ("Unparsing a source file (different from all other source
        // files) \n");
        printf("Unparsing a header file of an AST from a given source file \n");
      }
      // ROSE_ASSERT(this->currentFile != globalScope->get_parent());
#endif
      SgSourceFile *currentSourceFile = isSgSourceFile(this->currentFile);
      ROSE_ASSERT(currentSourceFile != NULL);

#if DEBUG_UNPARSE_FILE
      // printf ("globalScope_from_currentFile     = %p
      // \n",globalScope_from_currentFile);
      printf("globalScope                      = %p \n", globalScope);
      printf("currentSourceFile->getFileName() = %s \n",
             currentSourceFile->getFileName().c_str());
#endif
      SgGlobal *globalScope_from_currentFile =
          currentSourceFile->get_globalScope();

      // DQ (4/11/2021): Added assertion.
      ASSERT_require(globalScope == globalScope_from_currentFile);

      u_exprStmt->unparseStatement(globalScope, info);
#if DEBUG_UNPARSE_FILE
      printf("DONE: In Unparser::unparseFile(): case C/C++: "
             "unparseStatement(globalScope, info): globalScope = %p \n",
             globalScope);
#endif
    }
    break;
  }

  case SgFile::e_Fortran_language: {
    // printf ("Error: SgFile::e_Fortran_language detected in unparser (unparser
    // not implemented, unparsing ignored) \n");

    // DQ (6/30/2013): Added support to time the unparsing of the file.
    TimingPerformance timer("Source code generation from AST (Fortran):");

    // Unparse using the new Fortran unparser!
    u_fortran_locatedNode->unparseStatement(globalScope, info);
    break;
  }
  case SgFile::e_last_language: {
    printf("Error: SgFile::e_last_language detected in unparser \n");
    ROSE_ABORT();
  }

  default: {
    printf("Error: default reached in unparser (unknown output language "
           "specified) \n");
    ROSE_ABORT();
  }
  }

  // DQ (7/19/2004): Added newline at end of file
  // (some compilers (e.g. g++) complain if no newline is present)
  // This does not work, not sure why
  // cur << "\n/* EOF: can't insert newline at end of file to avoid g++ compiler
  // warning */ \n\n";

  // DQ: This does not compile
  // cur << std::endl;

  // Finalize the source file through the formatter before flushing.  This
  // commits the required final newline while discarding a pending syntactic
  // separator, so the stream cannot end in formatter-generated whitespace.
  cur.insert_newline();
  cur.flush();

  // MH-20140701 removed comment-out
#if DEBUG_UNPARSE_FILE
  printf("Leaving Unparser::unparseFile(): file = %s = %s \n",
         file->get_sourceFileNameWithPath().c_str(),
         file->get_sourceFileNameWithoutPath().c_str());
  printf("Leaving Unparser::unparseFile(): SageInterface::is_Cxx_language()    "
         " = %s \n",
         SageInterface::is_Cxx_language() ? "true" : "false");
#endif
}

// DQ (12/5/2006): Output separate file containing source position information
// for highlighting (useful for debugging).
int Unparser::get_embedColorCodesInGeneratedCode() {
  return embedColorCodesInGeneratedCode;
}

int Unparser::get_generateSourcePositionCodes() {
  return generateSourcePositionCodes;
}

void Unparser::set_embedColorCodesInGeneratedCode(int x) {
  embedColorCodesInGeneratedCode = x;
}

void Unparser::set_generateSourcePositionCodes(int x) {
  generateSourcePositionCodes = x;
}

namespace {
enum class StringUnparseCategory {
  project,
  sourceFile,
  statement,
  expression,
  type,
  templateArgument,
  templateParameter,
  pragma,
  openmpClause,
  templateArgumentList,
  templateParameterList
};

[[noreturn]] void rejectStringUnparseCategory(const SgNode *node,
                                              const char *category) {
  ASSERT_not_null(node);
  ASSERT_not_null(category);
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[unparse-to-string-dispatch]: node=%s "
          "category=%s has no typed standalone emitter\n",
          node->class_name().c_str(), category);
  ROSE_ABORT();
}

StringUnparseCategory classifyStringUnparseNode(const SgNode *node) {
  ASSERT_not_null(node);

  // Symbols and initialized names are semantic identities, not standalone
  // source constructs.  Their spelling requires a typed reference or
  // declaration emission site and must never be fabricated here.
  if (isSgSymbol(node) != nullptr) {
    rejectStringUnparseCategory(node, "symbol");
  }
  if (isSgInitializedName(node) != nullptr) {
    rejectStringUnparseCategory(node, "initialized-name");
  }

  if (isSgProject(node) != nullptr) {
    return StringUnparseCategory::project;
  }
  if (isSgSourceFile(node) != nullptr) {
    return StringUnparseCategory::sourceFile;
  }
  if (isSgStatement(node) != nullptr) {
    return StringUnparseCategory::statement;
  }
  if (isSgExpression(node) != nullptr) {
    return StringUnparseCategory::expression;
  }
  if (isSgType(node) != nullptr) {
    return StringUnparseCategory::type;
  }
  if (isSgTemplateArgument(node) != nullptr) {
    return StringUnparseCategory::templateArgument;
  }
  if (isSgTemplateParameter(node) != nullptr) {
    return StringUnparseCategory::templateParameter;
  }
  if (isSgPragma(node) != nullptr) {
    return StringUnparseCategory::pragma;
  }
  if (isSgOmpClause(node) != nullptr) {
    return StringUnparseCategory::openmpClause;
  }
  if (isSgLocatedNodeSupport(node) != nullptr) {
    rejectStringUnparseCategory(node, "unsupported-located-support");
  }
  if (isSgSupport(node) != nullptr) {
    rejectStringUnparseCategory(node, "unsupported-support");
  }
  rejectStringUnparseCategory(node, "unsupported-node");
}

StringUnparseCategory selectStringUnparseCategory(
    const SgNode *node, const SgTemplateArgumentPtrList *templateArgumentList,
    const SgTemplateParameterPtrList *templateParameterList) {
  const unsigned inputCount = (node != nullptr ? 1U : 0U) +
                              (templateArgumentList != nullptr ? 1U : 0U) +
                              (templateParameterList != nullptr ? 1U : 0U);
  if (inputCount != 1) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[unparse-to-string-dispatch]: exactly one "
            "node or template-list input is required, received count=%u\n",
            inputCount);
    ROSE_ABORT();
  }
  if (node != nullptr) {
    return classifyStringUnparseNode(node);
  }
  return templateArgumentList != nullptr
             ? StringUnparseCategory::templateArgumentList
             : StringUnparseCategory::templateParameterList;
}

bool isContextFreeCFamilyBuiltinType(const SgType *type) {
  if (type == nullptr) {
    return false;
  }

  switch (type->variant()) {
  case T_CHAR:
  case T_SIGNED_CHAR:
  case T_UNSIGNED_CHAR:
  case T_SHORT:
  case T_SIGNED_SHORT:
  case T_UNSIGNED_SHORT:
  case T_INT:
  case T_SIGNED_INT:
  case T_UNSIGNED_INT:
  case T_LONG:
  case T_SIGNED_LONG:
  case T_UNSIGNED_LONG:
  case T_VOID:
  case T_GLOBAL_VOID:
  case T_WCHAR:
  case T_CHAR8:
  case T_CHAR16:
  case T_CHAR32:
  case T_FLOAT:
  case T_DOUBLE:
  case T_FLOAT80:
  case T_FLOAT128:
  case T_FLOAT16:
  case T_FP16:
  case T_BFLOAT16:
  case T_FLOAT32X:
  case T_FLOAT64X:
  case T_FLOAT32:
  case T_FLOAT64:
  case T_LONG_LONG:
  case T_SIGNED_LONG_LONG:
  case T_UNSIGNED_LONG_LONG:
  case T_SIGNED_128BIT_INTEGER:
  case T_UNSIGNED_128BIT_INTEGER:
  case T_LONG_DOUBLE:
  case T_BOOL:
  case T_NULLPTR:
  case T_ELLIPSE:
    return true;
  case T_COMPLEX: {
    const SgTypeComplex *complex_type = isSgTypeComplex(type);
    ASSERT_not_null(complex_type);
    return isContextFreeCFamilyBuiltinType(complex_type->get_base_type());
  }
  default:
    return false;
  }
}

bool requiresExactCFamilyBuiltinLanguage(const SgType *type) {
  if (type == nullptr) {
    return false;
  }
  return type->variant() == T_BOOL || type->variant() == T_NULLPTR;
}

std::vector<SgDeclarationStatement *>
namedTypeDeclarationCandidates(const SgNamedType *namedType) {
  std::vector<SgDeclarationStatement *> candidates;
  if (namedType == nullptr || namedType->get_declaration() == nullptr) {
    return candidates;
  }

  auto appendUnique = [&](SgDeclarationStatement *declaration) {
    if (declaration != nullptr &&
        std::find(candidates.begin(), candidates.end(), declaration) ==
            candidates.end()) {
      candidates.push_back(declaration);
    }
  };
  SgDeclarationStatement *declaration = namedType->get_declaration();
  appendUnique(declaration);
  appendUnique(declaration->get_firstNondefiningDeclaration());
  appendUnique(declaration->get_definingDeclaration());
  return candidates;
}

enum class StringUnparseContextKind { source, canonicalCFamily, project };

struct StringUnparseInvocationContext {
  StringUnparseCategory category;
  StringUnparseContextKind kind;
  SgSourceFile *sourceFile = nullptr;
  SgStatement *emissionStatement = nullptr;
  SgNode *referenceNode = nullptr;
  SgScopeStatement *scope = nullptr;
  SgFile::languageOption_enum language = SgFile::e_error_language;
  std::string fileName;
  bool fullSourceTraversal = false;
};

const char *stringUnparseCategoryName(StringUnparseCategory category) {
  switch (category) {
  case StringUnparseCategory::project:
    return "SgProject";
  case StringUnparseCategory::sourceFile:
    return "SgSourceFile";
  case StringUnparseCategory::statement:
    return "statement";
  case StringUnparseCategory::expression:
    return "expression";
  case StringUnparseCategory::type:
    return "type";
  case StringUnparseCategory::templateArgument:
    return "template-argument";
  case StringUnparseCategory::templateParameter:
    return "template-parameter";
  case StringUnparseCategory::pragma:
    return "pragma";
  case StringUnparseCategory::openmpClause:
    return "OpenMP-clause";
  case StringUnparseCategory::templateArgumentList:
    return "template-argument-list";
  case StringUnparseCategory::templateParameterList:
    return "template-parameter-list";
  }

  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[unparse-to-string-context]: invalid input "
          "category\n");
  ROSE_ABORT();
}

SgSourceFile *sourceFileForContextNode(SgNode *node) {
  if (node == nullptr) {
    return nullptr;
  }
  if (SgSourceFile *sourceFile = isSgSourceFile(node)) {
    return sourceFile;
  }
  return SageInterface::getEnclosingSourceFile(node);
}

SgStatement *structuralEmissionStatement(SgNode *node) {
  for (SgNode *cursor = node; cursor != nullptr;
       cursor = cursor->get_parent()) {
    if (SgStatement *statement = isSgStatement(cursor)) {
      return statement;
    }
  }
  return nullptr;
}

SgScopeStatement *structuralScope(SgNode *node) {
  for (SgNode *cursor = node; cursor != nullptr;
       cursor = cursor->get_parent()) {
    if (SgScopeStatement *scope = isSgScopeStatement(cursor)) {
      return scope;
    }
  }
  return nullptr;
}

bool isSemanticOnlyNamedTypeDeclaration(
    const SgDeclarationStatement *declaration) {
  if (declaration == nullptr) {
    return false;
  }
  if (isSgAuxiliaryDeclarationList(declaration->get_parent()) != nullptr) {
    return true;
  }
  return isSgNonrealDecl(const_cast<SgDeclarationStatement *>(declaration)) !=
             nullptr &&
         isSgDeclarationScope(declaration->get_parent()) != nullptr;
}

bool isContextFreeCFamilyLiteral(const SgExpression *expression) {
  const SgValueExp *value = isSgValueExp(expression);
  if (value != nullptr) {
    // Enum values, template-parameter values, and complex values carry or
    // contain semantic identities.  All other value leaves have a complete
    // canonical/source-spelling representation independent of a lexical use
    // site, provided no contextual original-expression tree is attached.
    if (isSgEnumVal(value) != nullptr ||
        isSgTemplateParameterVal(value) != nullptr ||
        isSgComplexVal(value) != nullptr) {
      return false;
    }
    SgExpression *source = value->get_originalExpressionTree();
    return source == nullptr || isSgOmpSourceExpression(source) != nullptr;
  }

  return isSgMacroExpansionExp(expression) != nullptr ||
         isSgOmpSourceExpression(expression) != nullptr;
}

bool isContextFreeTemplateArgument(const SgTemplateArgument *argument) {
  if (argument == nullptr) {
    return false;
  }
  switch (argument->get_argumentType()) {
  case SgTemplateArgument::type_argument: {
    SgType *type = argument->get_sourceSpelledType();
    if (type == nullptr) {
      type = argument->get_type();
    }
    return isContextFreeCFamilyBuiltinType(type);
  }
  case SgTemplateArgument::nontype_argument:
    return argument->get_initializedName() == nullptr &&
           isContextFreeCFamilyLiteral(argument->get_expression());
  case SgTemplateArgument::template_template_argument:
  case SgTemplateArgument::start_of_pack_expansion_argument:
  case SgTemplateArgument::argument_undefined:
  default:
    return false;
  }
}

bool isContextFreeTemplateParameter(const SgTemplateParameter *parameter) {
  if (parameter == nullptr || parameter->get_typeConstraint() != nullptr) {
    return false;
  }
  switch (parameter->get_parameterType()) {
  case SgTemplateParameter::type_parameter: {
    SgType *type = parameter->get_type();
    const bool hasCanonicalName =
        (isSgTemplateType(type) != nullptr &&
         !isSgTemplateType(type)->get_name().is_null()) ||
        (isSgNonrealType(type) != nullptr &&
         !isSgNonrealType(type)->get_name().is_null());
    return hasCanonicalName && parameter->get_defaultTypeParameter() == nullptr;
  }
  case SgTemplateParameter::nontype_parameter: {
    if (SgExpression *expression = parameter->get_expression()) {
      return isContextFreeCFamilyLiteral(expression);
    }
    SgInitializedName *name = parameter->get_initializedName();
    return name != nullptr &&
           isContextFreeCFamilyBuiltinType(name->get_type()) &&
           parameter->get_defaultExpressionParameter() == nullptr;
  }
  case SgTemplateParameter::template_parameter:
  default:
    return false;
  }
}

bool isContextFreeCanonicalCFamilyInput(StringUnparseCategory category,
                                        const SgNode *node) {
  if (category == StringUnparseCategory::type) {
    return isContextFreeCFamilyBuiltinType(isSgType(node));
  }
  if (category == StringUnparseCategory::expression) {
    return isContextFreeCFamilyLiteral(isSgExpression(node));
  }
  return false;
}

std::string sourceFileNameForDiagnostic(const SgSourceFile *sourceFile) {
  ASSERT_not_null(sourceFile);
  const Sg_File_Info *fileInfo = sourceFile->get_file_info();
  return fileInfo != nullptr && !fileInfo->get_raw_filename().empty()
             ? fileInfo->get_raw_filename()
             : std::string("<missing-file-info>");
}

StringUnparseInvocationContext resolveStringUnparseInvocationContext(
    StringUnparseCategory category, const SgNode *astNode,
    const SgUnparse_Info *inputInfo,
    const SgTemplateArgumentPtrList *templateArgumentList = nullptr,
    const SgTemplateParameterPtrList *templateParameterList = nullptr) {
  StringUnparseInvocationContext context{category,
                                         StringUnparseContextKind::source};
  if (const SgNullExpression *nullExpression = isSgNullExpression(astNode)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[unparse-to-string-null-expression]: "
            "typed null role=%d has no standalone expression spelling\n",
            static_cast<int>(nullExpression->get_role()));
    ROSE_ABORT();
  }
  SgSourceFile *resolvedSourceFile = nullptr;
  const char *resolvedCandidate = nullptr;

  auto addSourceFileCandidate = [&](const char *candidate,
                                    SgSourceFile *sourceFile) {
    ASSERT_not_null(candidate);
    ASSERT_not_null(sourceFile);
    if (resolvedSourceFile != nullptr && resolvedSourceFile != sourceFile) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[unparse-to-string-context]: "
              "candidate=%s file=%s conflicts with candidate=%s file=%s\n",
              candidate, sourceFileNameForDiagnostic(sourceFile).c_str(),
              resolvedCandidate,
              sourceFileNameForDiagnostic(resolvedSourceFile).c_str());
      ROSE_ABORT();
    }
    resolvedSourceFile = sourceFile;
    resolvedCandidate = candidate;
  };

  auto addNodeCandidate = [&](const char *candidate, SgNode *node,
                              bool requireSourceFile) {
    if (node == nullptr) {
      return;
    }
    SgSourceFile *sourceFile = sourceFileForContextNode(node);
    if (sourceFile == nullptr) {
      if (!requireSourceFile) {
        return;
      }
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[unparse-to-string-context]: "
              "candidate=%s node=%s has no enclosing source file\n",
              candidate, node->class_name().c_str());
      ROSE_ABORT();
    }
    addSourceFileCandidate(candidate, sourceFile);
  };

  // Builtin type nodes are globally shared semantic singletons.  Any parent
  // they happen to carry belongs to an unrelated AST use and is not an
  // intrinsic source candidate for a standalone spelling request.  An
  // explicit SgUnparse_Info context below can still select a source language.
  const bool typeInput = category == StringUnparseCategory::type;
  const bool sharedContextFreeBuiltin =
      typeInput && isContextFreeCFamilyBuiltinType(isSgType(astNode));
  const bool callerHasExactTypeUseSite =
      typeInput && inputInfo != nullptr &&
      (inputInfo->get_current_source_file() != nullptr ||
       inputInfo->get_template_argument_qualification_context() != nullptr ||
       inputInfo->get_reference_node_for_qualification() != nullptr ||
       inputInfo->get_current_scope() != nullptr);
  if (typeInput && isSgNamedType(astNode) != nullptr && inputInfo != nullptr &&
      !inputInfo->forceQualifiedNames() &&
      (inputInfo->get_template_argument_qualification_context() == nullptr ||
       inputInfo->get_reference_node_for_qualification() == nullptr)) {
    fprintf(stderr,
            "REX_NAME_QUALIFICATION_INVARIANT[standalone-type-use]: "
            "type=%p/%s lacks an exact emission statement or reference "
            "identity\n",
            static_cast<const void *>(astNode), astNode->class_name().c_str());
    ROSE_ABORT();
  }
  // SgType nodes are shared semantic identities rather than structurally
  // owned AST children.  Their parent pointer can therefore identify an
  // unrelated use in another project or source file.  A caller-provided use
  // site is the only source context for a contextual type spelling; a
  // standalone named type may instead use its canonical declaration family.
  if (category != StringUnparseCategory::project && !typeInput) {
    addNodeCandidate("node", const_cast<SgNode *>(astNode), false);
  } else if (typeInput && !sharedContextFreeBuiltin &&
             !callerHasExactTypeUseSite) {
    // Named types are shared semantic nodes and normally have no structural
    // parent. Their canonical/first/defining declaration chain is the exact
    // intrinsic source identity for a standalone type spelling; every
    // source-backed member of that chain must agree.
    if (const SgNamedType *namedType = isSgNamedType(astNode)) {
      for (SgDeclarationStatement *declaration :
           namedTypeDeclarationCandidates(namedType)) {
        addNodeCandidate("node-declaration", declaration, false);
      }
    }
  }

  bool listElementsAreCanonical = true;
  if (templateArgumentList != nullptr) {
    for (SgTemplateArgument *argument : *templateArgumentList) {
      if (argument == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[unparse-to-string-list-context]: "
                "template argument list contains a null element\n");
        ROSE_ABORT();
      }
      addNodeCandidate("template-argument", argument, false);
      listElementsAreCanonical =
          listElementsAreCanonical && isContextFreeTemplateArgument(argument);
      if (sourceFileForContextNode(argument) == nullptr &&
          !isContextFreeTemplateArgument(argument)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[unparse-to-string-list-context]: "
                "detached template argument is not context-free\n");
        ROSE_ABORT();
      }
    }
  }
  if (templateParameterList != nullptr) {
    for (SgTemplateParameter *parameter : *templateParameterList) {
      if (parameter == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[unparse-to-string-list-context]: "
                "template parameter list contains a null element\n");
        ROSE_ABORT();
      }
      addNodeCandidate("template-parameter", parameter, false);
      listElementsAreCanonical =
          listElementsAreCanonical && isContextFreeTemplateParameter(parameter);
      if (sourceFileForContextNode(parameter) == nullptr &&
          !isContextFreeTemplateParameter(parameter)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[unparse-to-string-list-context]: "
                "detached template parameter is not context-free\n");
        ROSE_ABORT();
      }
    }
  }

  if (inputInfo != nullptr) {
    if (SgSourceFile *sourceFile = inputInfo->get_current_source_file()) {
      addSourceFileCandidate("current-source-file", sourceFile);
    }
    addNodeCandidate("qualification-context",
                     inputInfo->get_template_argument_qualification_context(),
                     true);
    addNodeCandidate("reference-node",
                     inputInfo->get_reference_node_for_qualification(), false);
    addNodeCandidate("current-scope", inputInfo->get_current_scope(), true);
  }

  if (category == StringUnparseCategory::project) {
    if (resolvedSourceFile != nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[unparse-to-string-context]: SgProject "
              "spans source files and cannot use candidate=%s file=%s\n",
              resolvedCandidate,
              sourceFileNameForDiagnostic(resolvedSourceFile).c_str());
      ROSE_ABORT();
    }
    context.kind = StringUnparseContextKind::project;
    context.language = SgFile::e_default_language;
    context.fileName = "<rex-project-unparse>";
    context.fullSourceTraversal = true;
    return context;
  }

  if (resolvedSourceFile == nullptr) {
    const bool canonicalList =
        (category == StringUnparseCategory::templateArgumentList ||
         category == StringUnparseCategory::templateParameterList) &&
        listElementsAreCanonical;
    if (!canonicalList &&
        !isContextFreeCanonicalCFamilyInput(category, astNode)) {
      if (category == StringUnparseCategory::type) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[unparse-to-string-type-context]: "
                "detached type=%s requires an exact source use-site\n",
                astNode->class_name().c_str());
        ROSE_ABORT();
      }
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[unparse-to-string-context]: input=%s "
              "is context-sensitive and has no exact source-file context\n",
              astNode != nullptr ? astNode->class_name().c_str()
                                 : stringUnparseCategoryName(category));
      ROSE_ABORT();
    }
    const SgFile::languageOption_enum explicitLanguage =
        inputInfo != nullptr ? inputInfo->get_language()
                             : SgFile::e_default_language;
    if (explicitLanguage != SgFile::e_default_language &&
        explicitLanguage != SgFile::e_C_language &&
        explicitLanguage != SgFile::e_Cxx_language) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[unparse-to-string-context]: canonical "
              "C/C++ input=%s has conflicting explicit language=%s\n",
              astNode != nullptr ? astNode->class_name().c_str()
                                 : stringUnparseCategoryName(category),
              SgFile::get_outputLanguageOptionName(explicitLanguage).c_str());
      ROSE_ABORT();
    }
    context.kind = StringUnparseContextKind::canonicalCFamily;
    if (explicitLanguage != SgFile::e_default_language) {
      context.language = explicitLanguage;
    } else if (astNode != nullptr &&
               requiresExactCFamilyBuiltinLanguage(isSgType(astNode))) {
      // Bool and nullptr have different valid spellings across the C-family
      // languages, so their language must remain unresolved and fail at the
      // typed emitter instead of silently selecting C++.
      context.language = SgFile::e_default_language;
    } else {
      context.language = SgFile::e_Cxx_language;
    }
    context.fileName = "<rex-canonical-cxx>";
    return context;
  }

  const Sg_File_Info *fileInfo = resolvedSourceFile->get_file_info();
  if (fileInfo == nullptr || fileInfo->get_raw_filename().empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[unparse-to-string-context]: source file "
            "from candidate=%s has no exact filename\n",
            resolvedCandidate);
    ROSE_ABORT();
  }
  const SgFile::languageOption_enum sourceLanguage =
      resolvedSourceFile->get_outputLanguage();
  if (sourceLanguage != SgFile::e_C_language &&
      sourceLanguage != SgFile::e_Cxx_language &&
      sourceLanguage != SgFile::e_Fortran_language) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[unparse-to-string-context]: source "
            "file=%s has non-explicit output language=%s\n",
            fileInfo->get_raw_filename().c_str(),
            SgFile::get_outputLanguageOptionName(sourceLanguage).c_str());
    ROSE_ABORT();
  }
  if (inputInfo != nullptr &&
      inputInfo->get_language() != SgFile::e_default_language &&
      inputInfo->get_language() != sourceLanguage) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[unparse-to-string-context]: source "
            "file=%s language=%s conflicts with explicit language=%s\n",
            fileInfo->get_raw_filename().c_str(),
            SgFile::get_outputLanguageOptionName(sourceLanguage).c_str(),
            SgFile::get_outputLanguageOptionName(inputInfo->get_language())
                .c_str());
    ROSE_ABORT();
  }

  if (category == StringUnparseCategory::type &&
      isSgNonrealType(astNode) != nullptr &&
      (inputInfo == nullptr ||
       inputInfo->get_template_argument_qualification_context() == nullptr ||
       inputInfo->get_reference_node_for_qualification() == nullptr)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[unparse-to-string-type-use-site]: "
            "contextual type=%p/%s requires an exact emitted qualification "
            "context and reference identity\n",
            static_cast<const void *>(astNode), astNode->class_name().c_str());
    ROSE_ABORT();
  }

  context.kind = StringUnparseContextKind::source;
  context.sourceFile = resolvedSourceFile;
  context.language = sourceLanguage;
  context.fileName = fileInfo->get_raw_filename();
  context.fullSourceTraversal = category == StringUnparseCategory::sourceFile;
  SgNode *structuralContextNode = const_cast<SgNode *>(astNode);
  const bool hasExactCallerTypeUseSite =
      category == StringUnparseCategory::type && inputInfo != nullptr &&
      inputInfo->get_template_argument_qualification_context() != nullptr &&
      inputInfo->get_reference_node_for_qualification() != nullptr;
  if (hasExactCallerTypeUseSite) {
    structuralContextNode =
        inputInfo->get_template_argument_qualification_context();
  }
  if (const SgNamedType *namedType = isSgNamedType(astNode)) {
    if (!hasExactCallerTypeUseSite) {
      for (SgDeclarationStatement *declaration :
           namedTypeDeclarationCandidates(namedType)) {
        if (sourceFileForContextNode(declaration) == resolvedSourceFile &&
            !isSemanticOnlyNamedTypeDeclaration(declaration)) {
          structuralContextNode = declaration;
          break;
        }
      }
      if (structuralContextNode == astNode) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[standalone-named-type-context]: "
                "type=%p/%s has a source file but no exact emitted "
                "declaration identity in that file\n",
                static_cast<const void *>(astNode),
                astNode->class_name().c_str());
        ROSE_ABORT();
      }
    } else if (sourceFileForContextNode(structuralContextNode) !=
               resolvedSourceFile) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[standalone-named-type-use-site]: "
              "type=%p/%s has a qualification context outside its exact "
              "source file\n",
              static_cast<const void *>(astNode),
              astNode->class_name().c_str());
      ROSE_ABORT();
    }
  }
  context.emissionStatement =
      inputInfo != nullptr &&
              inputInfo->get_template_argument_qualification_context() !=
                  nullptr
          ? inputInfo->get_template_argument_qualification_context()
          : structuralEmissionStatement(structuralContextNode);
  context.referenceNode =
      inputInfo != nullptr &&
              inputInfo->get_reference_node_for_qualification() != nullptr
          ? inputInfo->get_reference_node_for_qualification()
          : (isSgNamedType(astNode) != nullptr ? structuralContextNode
                                               : nullptr);
  context.scope =
      inputInfo != nullptr && inputInfo->get_current_scope() != nullptr
          ? inputInfo->get_current_scope()
          : structuralScope(structuralContextNode);
  return context;
}
} // namespace

/*! \brief This function is the connection from the SgNode::unparseToString()
   function to the unparser.

    This function connects the SgNode::unparseToString() function
    to the unparser.  It takes an optional SgUnparse_Info object pointer.
    If a SgUnparse_Info object is provided then it is not deleted by this
    function.

    \internal Internally this function allocates a SgUnparse_Info object if one
   is not proviede within the function interface.  If the function allocates a
   SgUnparse_Info object it will delete it.  The user is always responcible for
   the allocation and destruction of objects provided to the interface of
   functions.
 */

// DQ (9/13/2014): Added support for unparsing of STL lists (specifically
// SgTemplateArgumentPtrList and SgTemplateParameterPtrList). This allows us to
// define a simpler API for the name qualification and refactor as much of the
// support as possible. string globalUnparseToString_OpenMPSafe ( const SgNode*
// astNode, SgUnparse_Info* inputUnparseInfoPointer );
string globalUnparseToString_OpenMPSafe(
    const SgNode *astNode,
    const SgTemplateArgumentPtrList *templateArgumentList,
    const SgTemplateParameterPtrList *templateParameterList,
    SgUnparse_Info *inputUnparseInfoPointer,
    NameQualificationContext *nameQualifications,
    TokenUnparseFrontierContext *tokenFrontiers = nullptr,
    const StringUnparseInvocationContext *validatedContext = nullptr);

string globalUnparseToString(const SgNode *astNode,
                             SgUnparse_Info *inputUnparseInfoPointer) {
  // Validate the public node category before name-qualification analysis or
  // any other late unparser setup can hide an unsupported semantic object.
  const StringUnparseCategory category =
      selectStringUnparseCategory(astNode, nullptr, nullptr);
  const StringUnparseInvocationContext invocationContext =
      resolveStringUnparseInvocationContext(category, astNode,
                                            inputUnparseInfoPointer);

  string returnString;
  NameQualificationContext localNameQualifications;
  NameQualificationContext *nameQualifications = nullptr;
  // Name qualification must consume the emission statement selected and
  // cross-validated by the invocation boundary.  Re-reading only the optional
  // caller field here loses the declaration-derived use site of detached named
  // types and makes a valid source context appear projectless.
  SgStatement *exact_emission_statement =
      invocationContext.kind == StringUnparseContextKind::source
          ? invocationContext.emissionStatement
          : nullptr;
  const bool caller_fixed_qualification_use_site =
      inputUnparseInfoPointer != nullptr &&
      inputUnparseInfoPointer->get_template_argument_qualification_context() !=
          nullptr;
  // A structurally parented statement subtree changes emission statements as
  // the traversal descends (for example, a for-statement owns a distinct
  // SgExprStatement in its initializer).  Pin only a caller-specified use site
  // or a non-statement semantic root.  Treating the structurally derived root
  // statement as an explicit fixed context records every descendant under the
  // outer statement while the unparser correctly requests its inner owner.
  SgStatement *qualification_emission_statement =
      caller_fixed_qualification_use_site ||
              isSgStatement(const_cast<SgNode *>(astNode)) == nullptr
          ? exact_emission_statement
          : nullptr;
  SgNode *explicit_reference_node =
      invocationContext.kind == StringUnparseContextKind::source
          ? invocationContext.referenceNode
          : nullptr;

  if (SgSourceFile *sourceFile =
          isSgSourceFile(const_cast<SgNode *>(astNode))) {
    localNameQualifications.clear();
    Unparser::computeNameQualification(sourceFile, localNameQualifications);
    nameQualifications = &localNameQualifications;
  } else if (SgProject *project = isSgProject(const_cast<SgNode *>(astNode))) {
    localNameQualifications.clear();
    ASSERT_not_null(project->get_fileList_ptr());
    for (SgFile *file : project->get_fileList_ptr()->get_listOfFiles()) {
      if (SgSourceFile *sourceFile = isSgSourceFile(file);
          sourceFile != nullptr && !sourceFile->get_skip_unparse()) {
        Unparser::computeNameQualification(sourceFile, localNameQualifications);
      }
    }
    nameQualifications = &localNameQualifications;
  } else if (invocationContext.kind == StringUnparseContextKind::source) {
    ASSERT_not_null(invocationContext.sourceFile);
    if (explicit_reference_node != nullptr ||
        SageInterface::getEnclosingStatement(const_cast<SgNode *>(astNode)) !=
            nullptr ||
        !isContextFreeCFamilyBuiltinType(
            isSgType(const_cast<SgNode *>(astNode)))) {
      if (exact_emission_statement == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[unparse-to-string-name-"
                "qualification-context]: input=%s has no exact emission "
                "statement\n",
                astNode->class_name().c_str());
        ROSE_ABORT();
      }
      localNameQualifications.clear();
      NameQualificationTraversal::NameQualificationSetType referencedNameSet;
      SgNode *qualificationRoot = const_cast<SgNode *>(astNode);
      if (isSgType(qualificationRoot) != nullptr ||
          (qualificationRoot->get_parent() != nullptr &&
           SageInterface::getEnclosingSourceFile(qualificationRoot) ==
               invocationContext.sourceFile)) {
        // Every structurally source-owned subtree can reference declarations
        // and using directives from the lexical prefix between the file root
        // and its exact use site.  This includes expression roots requested by
        // diagnostic clients, not only statements.  SgType identities are
        // shared and therefore always use the already validated source use
        // site rather than a non-owning parent pointer.  Analyze the exact
        // source tree in order; beginning at the requested subtree would
        // falsely treat valid preceding declarations as unpublished.
        qualificationRoot = invocationContext.sourceFile;
      }
      // Function-definition unparsing emits the declaration header before the
      // body, but the definition's generated successor set does not contain
      // its owning declaration.  Start qualification at the exact declaration
      // edge so the analyzed graph matches every node the emitter will visit.
      // The declaration then reaches this same definition and its body through
      // its normal typed successors.
      if (SgFunctionDefinition *definition =
              isSgFunctionDefinition(qualificationRoot)) {
        SgFunctionDeclaration *declaration = definition->get_declaration();
        if (declaration == nullptr ||
            declaration->get_definition() != definition ||
            definition->get_parent() != declaration) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[function-definition-qualification-"
                  "root]: definition=%p has no exact owning declaration\n",
                  static_cast<void *>(definition));
          ROSE_ABORT();
        }
        qualificationRoot = declaration;
      }
      // unparseToString() may request a header or transformed declaration that
      // the input-file traversal would omit. Analyze the requested subtree
      // with its real enclosing scope and statement as the exact emission
      // context. Primitive builtin types are the only context-free exception.
      generateNameQualificationSupport(
          qualificationRoot, referencedNameSet, localNameQualifications,
          /*useInputFileTraversalOptimization=*/
          false, qualification_emission_statement, explicit_reference_node,
          isSgType(const_cast<SgNode *>(astNode)));
      nameQualifications = &localNameQualifications;
    }
  }

  {
    if (inputUnparseInfoPointer != NULL) {
      // DQ (1/13/2014): These should have been setup to be the same.
      ROSE_ASSERT(inputUnparseInfoPointer->SkipClassDefinition() ==
                  inputUnparseInfoPointer->SkipEnumDefinition());
    }

    // DQ (9/13/2014): Call internal funtion modified to be more general.
    // returnString =
    // globalUnparseToString_OpenMPSafe(astNode,inputUnparseInfoPointer);
    returnString = globalUnparseToString_OpenMPSafe(
        astNode, NULL, NULL, inputUnparseInfoPointer, nameQualifications,
        nullptr, &invocationContext);
  }

  return returnString;
}

string globalUnparseToString(const SgNode *astNode,
                             SgUnparse_Info *inputUnparseInfoPointer,
                             NameQualificationContext *nameQualifications) {
  const StringUnparseCategory category =
      selectStringUnparseCategory(astNode, nullptr, nullptr);
  const StringUnparseInvocationContext invocationContext =
      resolveStringUnparseInvocationContext(category, astNode,
                                            inputUnparseInfoPointer);
  return globalUnparseToString_OpenMPSafe(
      astNode, nullptr, nullptr, inputUnparseInfoPointer, nameQualifications,
      nullptr, &invocationContext);
}

string
globalUnparseToString(const SgTemplateArgumentPtrList *templateArgumentList,
                      SgUnparse_Info *inputUnparseInfoPointer) {
  string returnString;

// tps (Jun 24 2008) added because OpenMP crashes all the time at the unparser
#if ROSE_GCC_OMP
#pragma omp critical(unparser)
#endif
  {
    if (inputUnparseInfoPointer != NULL) {
      // DQ (1/13/2014): These should have been setup to be the same.
      ROSE_ASSERT(inputUnparseInfoPointer->SkipClassDefinition() ==
                  inputUnparseInfoPointer->SkipEnumDefinition());
    }

    // DQ (9/13/2014): Call internal funtion modified to be more general.
    // returnString =
    // globalUnparseToString_OpenMPSafe(astNode,inputUnparseInfoPointer);
    const StringUnparseInvocationContext invocationContext =
        resolveStringUnparseInvocationContext(
            StringUnparseCategory::templateArgumentList, nullptr,
            inputUnparseInfoPointer, templateArgumentList, nullptr);
    returnString = globalUnparseToString_OpenMPSafe(
        NULL, templateArgumentList, NULL, inputUnparseInfoPointer, nullptr,
        nullptr, &invocationContext);
  }

  return returnString;
}

string
globalUnparseToString(const SgTemplateArgumentPtrList *templateArgumentList,
                      SgUnparse_Info *inputUnparseInfoPointer,
                      NameQualificationContext *nameQualifications) {
  const StringUnparseInvocationContext invocationContext =
      resolveStringUnparseInvocationContext(
          StringUnparseCategory::templateArgumentList, nullptr,
          inputUnparseInfoPointer, templateArgumentList, nullptr);
  return globalUnparseToString_OpenMPSafe(
      nullptr, templateArgumentList, nullptr, inputUnparseInfoPointer,
      nameQualifications, nullptr, &invocationContext);
}

string
globalUnparseToString(const SgTemplateParameterPtrList *templateParameterList,
                      SgUnparse_Info *inputUnparseInfoPointer) {
  string returnString;

// tps (Jun 24 2008) added because OpenMP crashes all the time at the unparser
#if ROSE_GCC_OMP
#pragma omp critical(unparser)
#endif
  {
    if (inputUnparseInfoPointer != NULL) {
      // DQ (1/13/2014): These should have been setup to be the same.
      ROSE_ASSERT(inputUnparseInfoPointer->SkipClassDefinition() ==
                  inputUnparseInfoPointer->SkipEnumDefinition());
    }

    // DQ (9/13/2014): Call internal funtion modified to be more general.
    // returnString =
    // globalUnparseToString_OpenMPSafe(astNode,inputUnparseInfoPointer);
    const StringUnparseInvocationContext invocationContext =
        resolveStringUnparseInvocationContext(
            StringUnparseCategory::templateParameterList, nullptr,
            inputUnparseInfoPointer, nullptr, templateParameterList);
    returnString = globalUnparseToString_OpenMPSafe(
        NULL, NULL, templateParameterList, inputUnparseInfoPointer, nullptr,
        nullptr, &invocationContext);
  }

  return returnString;
}

string
globalUnparseToString(const SgTemplateParameterPtrList *templateParameterList,
                      SgUnparse_Info *inputUnparseInfoPointer,
                      NameQualificationContext *nameQualifications) {
  const StringUnparseInvocationContext invocationContext =
      resolveStringUnparseInvocationContext(
          StringUnparseCategory::templateParameterList, nullptr,
          inputUnparseInfoPointer, nullptr, templateParameterList);
  return globalUnparseToString_OpenMPSafe(
      nullptr, nullptr, templateParameterList, inputUnparseInfoPointer,
      nameQualifications, nullptr, &invocationContext);
}

// DQ (9/13/2014): Modified to extend the API of this internal function.
// string globalUnparseToString_OpenMPSafe ( const SgNode* astNode,
// SgUnparse_Info* inputUnparseInfoPointer )
string globalUnparseToString_OpenMPSafe(
    const SgNode *astNode,
    const SgTemplateArgumentPtrList *templateArgumentList,
    const SgTemplateParameterPtrList *templateParameterList,
    SgUnparse_Info *inputUnparseInfoPointer,
    NameQualificationContext *nameQualifications,
    TokenUnparseFrontierContext *tokenFrontiers,
    const StringUnparseInvocationContext *validatedContext) {
  const StringUnparseCategory category = selectStringUnparseCategory(
      astNode, templateArgumentList, templateParameterList);
  const StringUnparseInvocationContext invocationContext =
      validatedContext != nullptr
          ? *validatedContext
          : resolveStringUnparseInvocationContext(
                category, astNode, inputUnparseInfoPointer,
                templateArgumentList, templateParameterList);
  if (invocationContext.category != category) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[unparse-to-string-context]: validated "
            "category does not match invocation input\n");
    ROSE_ABORT();
  }

  string returnString;

  TokenUnparseFrontierContext localTokenFrontiers;
  if (tokenFrontiers == nullptr && astNode != nullptr &&
      (isSgProject(astNode) != nullptr || isSgSourceFile(astNode) != nullptr)) {
    SgProject *project = isSgProject(const_cast<SgNode *>(astNode));
    std::vector<SgSourceFile *> sourceFiles;
    if (SgSourceFile *sourceFile =
            isSgSourceFile(const_cast<SgNode *>(astNode))) {
      project = SageInterface::getProject(sourceFile);
      sourceFiles.push_back(sourceFile);
    } else {
      ASSERT_not_null(project);
      for (SgFile *file : project->get_fileList()) {
        if (SgSourceFile *sourceFile = isSgSourceFile(file)) {
          sourceFiles.push_back(sourceFile);
        }
      }
    }

    bool needsTokenContext = false;
    for (SgSourceFile *sourceFile : sourceFiles) {
      const bool needsTokenFrontier = sourceFile->get_unparse_tokens();
      if (!needsTokenFrontier) {
        continue;
      }
      if (project == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[token-frontier]: file=%s string "
                "unparse has no project\n",
                sourceFile->getFileName().c_str());
        ROSE_ABORT();
      }
      needsTokenContext = true;
      buildTokenStreamFrontier(sourceFile, sourceFile->get_unparseHeaderFiles(),
                               localTokenFrontiers);
    }
    if (needsTokenContext) {
      enforceTokenUnparseContract(project);
      buildFirstAndLastStatementsForIncludeFiles(project, localTokenFrontiers);
      buildFirstAndLastStatementsForScopes(project, localTokenFrontiers);
      tokenFrontiers = &localTokenFrontiers;
    }
  }

  // all options are now defined to be false. When these options can be passed
  // in from the prompt, these options will be set accordingly.
  bool _auto = false;
  bool linefile = false;
  bool useOverloadedOperators = false;
  bool num = false;

  // It is an error to have this always turned off (e.g. pointer = this; will
  // not unparse correctly)
  bool _this = true;

  bool caststring = false;
  bool _debug = false;
  bool _class = false;
  bool _forced_transformation_format = false;
  bool _unparse_includes = false;

  Unparser_Opt roseOptions(_auto, linefile, useOverloadedOperators, num, _this,
                           caststring, _debug, _class,
                           _forced_transformation_format, _unparse_includes);

  // DQ (7/19/2007): Remove lineNumber from constructor parameter list.
  // int lineNumber = 0;  // Zero indicates that ALL lines should be unparsed

  // Initialize the Unparser using a special string stream inplace of the usual
  // file stream
  ostringstream outputString;

  // Unparser roseUnparser ( &outputString, fileNameOfStatementsToUnparse,
  // roseOptions, lineNumber );
  Unparser roseUnparser(&outputString, invocationContext.fileName, roseOptions,
                        nullptr, nullptr, nullptr, nameQualifications,
                        tokenFrontiers);

  // Information that is passed down through the tree (inherited attribute)
  // Use the input SgUnparse_Info object if it is available.
  SgUnparse_Info *inheritedAttributeInfoPointer = NULL;

  // DQ (2/18/2013): Keep track of local allocation of the SgUnparse_Info object
  // in this function This is design to fix what appears to be a leak in ROSE
  // (abby-normal growth of the SgUnparse_Info memory pool for compiling large
  // files.
  bool allocatedSgUnparseInfoObjectLocally = false;

  if (inputUnparseInfoPointer != NULL) {
    // Use the user provided SgUnparse_Info object
    inheritedAttributeInfoPointer = inputUnparseInfoPointer;
  } else {
    // DEFINE DEFAULT BEHAVIOUR FOR THE CASE WHEN NO inputUnparseInfoPointer (==
    // NULL) IS PASSED AS ARGUMENT TO THE FUNCTION If no input parameter has
    // been specified then allocate one inheritedAttributeInfoPointer = new
    // SgUnparse_Info (NO_UNPARSE_INFO);
    inheritedAttributeInfoPointer = new SgUnparse_Info();
    ASSERT_not_null(inheritedAttributeInfoPointer);

    // DQ (2/18/2013): Keep track of local allocation of the SgUnparse_Info
    // object in this function
    allocatedSgUnparseInfoObjectLocally = true;

    if (!invocationContext.fullSourceTraversal) {
      // Every standalone spelling uses the dense diagnostic formatter.
      // Typed statement directives have an explicit dense representation;
      // source comments and preprocessing records remain excluded.
      inheritedAttributeInfoPointer->set_SkipComments();
      inheritedAttributeInfoPointer->set_SkipWhitespaces();
      inheritedAttributeInfoPointer->set_SkipCPPDirectives();
      inheritedAttributeInfoPointer->set_forceQualifiedNames();
      inheritedAttributeInfoPointer->unset_CheckAccess();
    }

    // unparseToString() should avoid full class/enum definitions by
    // default; statement-level unparsers can opt back in when needed
    // (e.g., typedefs that include a defining declaration).
    if (!invocationContext.fullSourceTraversal) {
      inheritedAttributeInfoPointer->set_SkipClassDefinition();
      inheritedAttributeInfoPointer->set_SkipEnumDefinition();
    }
  }

  ASSERT_not_null(inheritedAttributeInfoPointer);
  SgUnparse_Info &inheritedAttributeInfo = *inheritedAttributeInfoPointer;

  // Apply the single context resolved and cross-validated at the invocation
  // boundary.  No downstream filename, scope, language, or source-file probe
  // is permitted to repair this state later.
  if (invocationContext.kind == StringUnparseContextKind::source) {
    ASSERT_not_null(invocationContext.sourceFile);
    if (inheritedAttributeInfo.get_current_source_file() == nullptr) {
      inheritedAttributeInfo.set_current_source_file(
          invocationContext.sourceFile);
    }
    if (inheritedAttributeInfo.get_language() == SgFile::e_default_language) {
      inheritedAttributeInfo.set_language(invocationContext.language);
    }
    if (inheritedAttributeInfo.get_template_argument_qualification_context() ==
            nullptr &&
        invocationContext.emissionStatement != nullptr) {
      inheritedAttributeInfo.set_template_argument_qualification_context(
          invocationContext.emissionStatement);
    }
    if (inheritedAttributeInfo.get_current_scope() == nullptr &&
        invocationContext.scope != nullptr) {
      inheritedAttributeInfo.set_current_scope(invocationContext.scope);
    }
    if (inheritedAttributeInfo.get_reference_node_for_qualification() ==
            nullptr &&
        invocationContext.referenceNode != nullptr) {
      inheritedAttributeInfo.set_reference_node_for_qualification(
          invocationContext.referenceNode);
    }
    roseUnparser.currentFile = invocationContext.sourceFile;
  } else if (invocationContext.kind ==
             StringUnparseContextKind::canonicalCFamily) {
    if (inheritedAttributeInfo.get_language() == SgFile::e_default_language &&
        invocationContext.language != SgFile::e_default_language) {
      inheritedAttributeInfo.set_language(invocationContext.language);
    }
    if (category == StringUnparseCategory::templateArgumentList ||
        category == StringUnparseCategory::templateParameterList) {
      inheritedAttributeInfo.set_SkipQualifiedNames();
    }
  }

  if (inheritedAttributeInfo.SkipWhitespaces() &&
      (!inheritedAttributeInfo.SkipComments() ||
       !inheritedAttributeInfo.SkipCPPDirectives())) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[compact-output]: compact diagnostics "
            "require comments and preprocessing directives to be skipped\n");
    ROSE_ABORT();
  }
  if (inheritedAttributeInfo.SkipWhitespaces() && tokenFrontiers != nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[compact-output]: compact diagnostics "
            "cannot use token-stream unparsing\n");
    ROSE_ABORT();
  }
  const bool compactOutput = inheritedAttributeInfo.SkipWhitespaces();
  roseUnparser.get_output_stream().set_compact_output(compactOutput);

  // Detached AST rendering is not part of the file-level token replay or
  // preprocessing-boundary state.
  if (invocationContext.fullSourceTraversal) {
    inheritedAttributeInfoPointer->unset_usedInUparseToStringFunction();
  } else {
    inheritedAttributeInfoPointer->set_usedInUparseToStringFunction();
  }

  // Turn ON the error checking which triggers an error if the default
  // SgUnparse_Info constructor is called

  // DQ (10/19/2004): Cleaned up this code, remove this dead code after we are
  // sure that this worked properly Actually, this code is required to be this
  // way, since after this branch the current function returns and some data
  // must be cleaned up differently!  So put this back and leave it this way,
  // and remove the "Implementation Note".

  // DQ (1/13/2014): These should have been setup to be the same.
  ROSE_ASSERT(inheritedAttributeInfo.SkipClassDefinition() ==
              inheritedAttributeInfo.SkipEnumDefinition());

  switch (category) {
  case StringUnparseCategory::project: {
    const SgProject *project = isSgProject(astNode);
    ASSERT_not_null(project);
    for (int i = 0; i < project->numberOfFiles(); ++i) {
      SgFile *file = project->get_fileList()[i];
      ASSERT_not_null(file);
      const string unparsedFileString = globalUnparseToString_OpenMPSafe(
          file, nullptr, nullptr, inputUnparseInfoPointer, nameQualifications,
          tokenFrontiers);
      const string prefixString =
          string("/* TOP:") + file->getFileName() + string(" */ \n");
      const string suffixString =
          string("\n/* BOTTOM:") + file->getFileName() + string(" */ \n\n");
      returnString += prefixString + unparsedFileString + suffixString;
    }
    break;
  }

  case StringUnparseCategory::sourceFile: {
    const SgSourceFile *file = isSgSourceFile(astNode);
    ASSERT_not_null(file);
    SgGlobal *globalScope = file->get_globalScope();
    ASSERT_not_null(globalScope);
    StringUnparseInvocationContext globalContext = invocationContext;
    globalContext.category = StringUnparseCategory::statement;
    globalContext.emissionStatement = globalScope;
    globalContext.scope = globalScope;
    globalContext.fullSourceTraversal = true;
    returnString = globalUnparseToString_OpenMPSafe(
        globalScope, nullptr, nullptr, inputUnparseInfoPointer,
        nameQualifications, tokenFrontiers, &globalContext);
    break;
  }

  case StringUnparseCategory::statement: {
    const SgStatement *statement = isSgStatement(astNode);
    ASSERT_not_null(statement);
    if (inheritedAttributeInfo.get_language() == SgFile::e_Fortran_language) {
      ASSERT_not_null(roseUnparser.u_fortran_locatedNode);
      roseUnparser.u_fortran_locatedNode->unparseStatement(
          const_cast<SgStatement *>(statement), inheritedAttributeInfo);
    } else if (inheritedAttributeInfo.get_language() == SgFile::e_C_language ||
               inheritedAttributeInfo.get_language() ==
                   SgFile::e_Cxx_language) {
      if (SgGlobal *globalScope =
              isSgGlobal(const_cast<SgStatement *>(statement))) {
        ASSERT_not_null(globalScope->get_parent());
        if (isSgProject(globalScope->get_parent()) != nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[unparse-to-string]: global=%p is "
                  "parented directly by a project and has no source-file "
                  "emission context\n",
                  static_cast<void *>(globalScope));
          ROSE_ABORT();
        }
      }
      ASSERT_not_null(roseUnparser.u_exprStmt);
      roseUnparser.u_exprStmt->unparseStatement(
          const_cast<SgStatement *>(statement), inheritedAttributeInfo);
    } else {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[unparse-to-string-context]: statement "
              "dispatch has invalid language=%s\n",
              SgFile::get_outputLanguageOptionName(
                  inheritedAttributeInfo.get_language())
                  .c_str());
      ROSE_ABORT();
    }
    break;
  }

  case StringUnparseCategory::expression: {
    const SgExpression *expression = isSgExpression(astNode);
    ASSERT_not_null(expression);
    if (inheritedAttributeInfo.get_language() == SgFile::e_Fortran_language) {
      ASSERT_not_null(roseUnparser.u_fortran_locatedNode);
      roseUnparser.u_fortran_locatedNode->unparseExpression(
          const_cast<SgExpression *>(expression), inheritedAttributeInfo);
    } else if (inheritedAttributeInfo.get_language() == SgFile::e_C_language ||
               inheritedAttributeInfo.get_language() ==
                   SgFile::e_Cxx_language) {
      ASSERT_not_null(roseUnparser.u_exprStmt);
      roseUnparser.u_exprStmt->unparseExpression(
          const_cast<SgExpression *>(expression), inheritedAttributeInfo);
    } else {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[unparse-to-string-context]: expression "
              "dispatch has invalid language=%s\n",
              SgFile::get_outputLanguageOptionName(
                  inheritedAttributeInfo.get_language())
                  .c_str());
      ROSE_ABORT();
    }
    break;
  }

  case StringUnparseCategory::type: {
    const SgType *type = isSgType(astNode);
    ASSERT_not_null(type);
    ROSE_ASSERT(inheritedAttributeInfo.SkipClassDefinition() ==
                inheritedAttributeInfo.SkipEnumDefinition());
    if (inheritedAttributeInfo.get_language() == SgFile::e_C_language ||
        inheritedAttributeInfo.get_language() == SgFile::e_Cxx_language) {
      SgNode *reference =
          inheritedAttributeInfo.get_reference_node_for_qualification();
      if (reference != nullptr) {
        SgStatement *useSite =
            inheritedAttributeInfo
                .get_template_argument_qualification_context();
        if (!inheritedAttributeInfo.SkipQualifiedNames() &&
            useSite == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[type-unparse-context]: type=%p/%s "
                  "reference=%p/%s has no exact qualification use site\n",
                  static_cast<const void *>(type), type->class_name().c_str(),
                  static_cast<void *>(reference),
                  reference->class_name().c_str());
          ROSE_ABORT();
        }
        const NameQualificationResult qualification =
            roseUnparser.u_name->lookup_type_qualification_for_output(
                reference, useSite,
                inheritedAttributeInfo.SkipQualifiedNames());
        inheritedAttributeInfo.set_name_qualification_length(
            qualification.length);
        inheritedAttributeInfo.set_global_qualification_required(
            qualification.global);
        inheritedAttributeInfo.set_type_elaboration_required(
            qualification.typeElaboration);
      }
    }
    ASSERT_not_null(roseUnparser.u_type);
    roseUnparser.u_type->unparseType(const_cast<SgType *>(type),
                                     inheritedAttributeInfo);
    break;
  }

  case StringUnparseCategory::templateParameter: {
    const SgTemplateParameter *parameter = isSgTemplateParameter(astNode);
    ASSERT_not_null(parameter);
    ASSERT_not_null(roseUnparser.u_exprStmt);
    roseUnparser.u_exprStmt->unparseTemplateParameter(
        const_cast<SgTemplateParameter *>(parameter), inheritedAttributeInfo);
    break;
  }

  case StringUnparseCategory::templateArgument: {
    const SgTemplateArgument *argument = isSgTemplateArgument(astNode);
    ASSERT_not_null(argument);
    ASSERT_not_null(roseUnparser.u_exprStmt);
    roseUnparser.u_exprStmt->unparseTemplateArgument(
        const_cast<SgTemplateArgument *>(argument), inheritedAttributeInfo);
    break;
  }

  case StringUnparseCategory::pragma: {
    const SgPragma *pragma = isSgPragma(astNode);
    ASSERT_not_null(pragma);
    SgPragmaDeclaration *declaration =
        isSgPragmaDeclaration(pragma->get_parent());
    if (declaration == nullptr || declaration->get_pragma() != pragma) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[unparse-to-string-dispatch]: SgPragma "
              "has no exact owning declaration\n");
      ROSE_ABORT();
    }
    ASSERT_not_null(roseUnparser.u_exprStmt);
    roseUnparser.u_exprStmt->unparseStatement(declaration,
                                              inheritedAttributeInfo);
    break;
  }

  case StringUnparseCategory::openmpClause: {
    SgOmpClause *clause = const_cast<SgOmpClause *>(isSgOmpClause(astNode));
    ASSERT_not_null(clause);
    ASSERT_not_null(roseUnparser.u_exprStmt);
    roseUnparser.u_exprStmt->unparseOmpClause(clause, inheritedAttributeInfo);
    break;
  }

  case StringUnparseCategory::templateArgumentList:
    ASSERT_not_null(templateArgumentList);
    roseUnparser.u_exprStmt->unparseTemplateArgumentList(
        *templateArgumentList, inheritedAttributeInfo,
        TemplateArgumentEmission::complete_typed_identity);
    break;

  case StringUnparseCategory::templateParameterList:
    ASSERT_not_null(templateParameterList);
    roseUnparser.u_exprStmt->unparseTemplateParameterList(
        *templateParameterList, inheritedAttributeInfo);
    break;
  }

  // Turn OFF the error checking which triggers an if the default
  // SgUnparse_Info constructor is called GB (09/27/2007): Removed this error
  // check, see above.

  ROSE_ASSERT(roseUnparser.get_output_stream().get_compact_output() ==
              compactOutput);
  if (compactOutput) {
    roseUnparser.get_output_stream().finalize_compact_output();
  }
  if (isSgProject(astNode) == nullptr && isSgSourceFile(astNode) == nullptr) {
    returnString = outputString.str();
  }

  // delete the allocated SgUnparse_Info object
  if (inputUnparseInfoPointer == NULL) {
    delete inheritedAttributeInfoPointer;
    inheritedAttributeInfoPointer = NULL;
  }

  // DQ (2/18/2013): Keep track of local allocation of the SgUnparse_Info object
  // in this function
  if (allocatedSgUnparseInfoObjectLocally == true) {
    ROSE_ASSERT(inheritedAttributeInfoPointer == NULL);
  }

  return returnString;
}

string get_output_filename(SgFile &file) {
  // DQ (10/15/2005): This can now be made to be a simpler function!
  if (file.get_unparse_output_filename().empty() == true) {
    printf("Error: no output file name specified, use \"-o <output filename>\" "
           "option on commandline (see --help for options) \n");
  }
  ROSE_ASSERT(file.get_unparse_output_filename().empty() == false);

  // return file.get_unparse_output_filename();
  string returnString = file.get_unparse_output_filename();

  return returnString;
}

// DQ (10/11/2007): I think this is redundant with the Unparser::unparseFile()
// member function HOWEVER, this is called by the SgFile::unparse() member
// function, so it has to be here!

// Later we might want to move this to the SgProject or SgFile support class
// (generated by ROSETTA)
void unparseFile(
    SgFile *file, UnparseFormatHelp *unparseHelp,
    UnparseDelegate *unparseDelegate, SgScopeStatement *unparseScope,
    const UnparsePreprocessingInfoRewriteMap *preprocessingInfoRewrites,
    const std::string *outputFilenameOverride,
    NameQualificationContext *nameQualifications,
    TokenUnparseFrontierContext *tokenFrontiers,
    unsigned int physicalFileOccurrence) {
  // DQ (1/24/2010): Refactored code to cal this more directly (part of support
  // for SgDirectory). DQ (7/12/2005): Introduce tracking of performance of
  // ROSE.
  TimingPerformance timer("AST Code Generation (unparsing):");

  // Call the unparser mechanism

  // debugging assertions
  // ROSE_ASSERT ( file.get_verbose() == true );
  // ROSE_ASSERT ( file.get_skip_unparse() == false );
  // file.set_verbose(true);

  ASSERT_not_null(file);
  if (file->get_skip_unparse()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[skipped-file-dispatch]: file=%s was "
            "dispatched directly despite its explicit skip_unparse role\n",
            file->getFileName().c_str());
    ROSE_ABORT();
  }

  NameQualificationContext localNameQualifications;
  if (nameQualifications == nullptr) {
    if (SgSourceFile *sourceFile = isSgSourceFile(file)) {
      localNameQualifications.clear();
      Unparser::computeNameQualification(sourceFile, localNameQualifications);
      nameQualifications = &localNameQualifications;
    }
  }

  TokenUnparseFrontierContext localTokenFrontiers;
  if (SgSourceFile *sourceFile = isSgSourceFile(file)) {
    const bool needsTokenFrontier = sourceFile->get_unparse_tokens();
    if (needsTokenFrontier) {
      if (tokenFrontiers == nullptr) {
        buildTokenStreamFrontier(sourceFile,
                                 sourceFile->get_unparseHeaderFiles(),
                                 localTokenFrontiers);
        SgProject *project = SageInterface::getProject(sourceFile);
        if (project == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[token-boundary]: file=%s has no "
                  "project for token boundary construction\n",
                  sourceFile->getFileName().c_str());
          ROSE_ABORT();
        }
        buildFirstAndLastStatementsForIncludeFiles(project,
                                                   localTokenFrontiers);
        buildFirstAndLastStatementsForScopes(project, localTokenFrontiers);
        tokenFrontiers = &localTokenFrontiers;
      } else if (!tokenFrontiers->hasFile(sourceFile)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[token-frontier]: file=%s was not "
                "prepared by its unparse invocation\n",
                sourceFile->getFileName().c_str());
        ROSE_ABORT();
      }
    }
  }

  // DQ (9/7/2017): This is new code to introduce more general language handling
  // to ROSE.

  // Name of output filename (with prefix).
  const bool hasOutputFilenameOverride = outputFilenameOverride != nullptr;
  string outputFilename =
      hasOutputFilenameOverride ? *outputFilenameOverride : std::string();
  if (hasOutputFilenameOverride && outputFilename.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-name]: file=%s received an empty "
            "output filename override\n",
            file->getFileName().c_str());
    ROSE_ABORT();
  }

  // If we did unparse an intermediate file then we want to compile that file
  // instead of the original source file.
  if (!hasOutputFilenameOverride &&
      file->get_unparse_output_filename().empty() == true) {

    switch (file->get_outputLanguage()) {
    case SgFile::e_error_language: {
      printf("Error: SgFile::e_error_language detected in unparser \n");
      ROSE_ABORT();
    }

    case SgFile::e_default_language: {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[output-language]: file=%s has default "
              "output language\n",
              file->getFileName().c_str());
      ROSE_ABORT();
    }

    case SgFile::e_C_language:
    case SgFile::e_Cxx_language: {
      // printf ("Error: SgFile::e_C_language or SgFile::e_Cxx_language detected
      // in unparser (unparser not implemented, unparsing ignored) \n"); DQ
      // (9/17/2018): When used withouth header file unparsing, this is a valid
      // string.  So likely in needs to be setup else where so that we have
      // consistancy of how it is setup independent of if the header file
      // unparsing is used or not.
      // ROSE_ASSERT(file->get_sourceFileNameWithoutPath().empty() == true);
      ROSE_ASSERT(file->get_sourceFileNameWithoutPath().empty() == false);

      outputFilename = "rose_" + file->get_sourceFileNameWithoutPath();

      // Liao 12/29/2010, generate cuda source files
      if (file->get_Cuda_only() == true) {
        outputFilename =
            StringUtility::stripFileSuffixFromFileName(outputFilename);
        outputFilename += ".cu";
      }

      break;
    }

    case SgFile::e_Fortran_language: {
      // printf ("Error: SgFile::e_Fortran_language detected in unparser
      // (unparser not implemented, unparsing ignored) \n");

      outputFilename = "rose_" + file->get_sourceFileNameWithoutPath();
      break;
    }

    case SgFile::e_last_language: {
      printf("Error: SgFile::e_last_language detected in unparser \n");
      ROSE_ABORT();
    }

    default: {
      printf("Error: default reached in unparser (unknown output language "
             "specified) \n");
      ROSE_ABORT();
    }
    }

    // DQ (9/7/2017): Added support for generated file to be placed into the
    // same directory as the source file.
    SgProject *project = SageInterface::getProject(file);

    if (project != NULL) {
      if (project->get_unparse_in_same_directory_as_input_file() == true) {
        const std::filesystem::path inputPath(
            file->get_sourceFileNameWithPath());
        outputFilename = (inputPath.parent_path() /
                          ("rose_" + file->get_sourceFileNameWithoutPath()))
                             .string();
      }
    } else if (isSgSourceFile(file) != nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[missing-project]: file=%s source file "
              "has no associated project\n",
              file->getFileName().c_str());
      ROSE_ABORT();
    }

    // DQ (9/15/2013): Added assertion.
    ROSE_ASSERT(file->get_unparse_output_filename().empty() == true);

    SgSourceFile *source_file = isSgSourceFile(file);
    if (source_file != NULL) {
      ASSERT_not_null(project);
      if (project->get_unparser__clobber_input_file()) {
        // TOO1 (3/20/2014): Clobber the original input source file X_X
        //
        //            **CAUTION**RED*ALERT**CAUTION**
        //

        outputFilename = source_file->get_sourceFileNameWithPath();
        std::cout << "[WARN] [Unparser] Clobbering the original input file: "
                  << outputFilename << std::endl;
      }
    }

    // string outputFilename = "rose_" + file->get_sourceFileNameWithoutPath();

    outputFilename = resolveUnparseOutputToTestDir(file, outputFilename);

    // Set the output filename in the SgFile IR node.
    file->set_unparse_output_filename(outputFilename);

    ROSE_ASSERT(file->get_unparse_output_filename().empty() == false);
    // assert(file->get_unparse_output_filename().empty() == false);
  }
  if (hasOutputFilenameOverride) {
    outputFilename = resolveUnparseOutputToTestDir(file, outputFilename);
  } else {
    const string resolvedOutputFilename = resolveUnparseOutputToTestDir(
        file, file->get_unparse_output_filename());
    if (resolvedOutputFilename != file->get_unparse_output_filename()) {
      file->set_unparse_output_filename(resolvedOutputFilename);
    }
    outputFilename = file->get_unparse_output_filename();
  }

  SgProject *project = SageInterface::getProject(file);
  const bool explicitClobber =
      project != nullptr && project->get_unparser__clobber_input_file();
  SgSourceFile *sourceFile = isSgSourceFile(file);
  const bool generatedSource =
      sourceFile != nullptr && sourceFile->get_isGeneratedSource();
  if (generatedSource || !explicitClobber) {
    std::error_code inputPathError;
    std::error_code outputPathError;
    const std::filesystem::path inputPath = std::filesystem::weakly_canonical(
        file->get_sourceFileNameWithPath(), inputPathError);
    const std::filesystem::path outputPath =
        std::filesystem::weakly_canonical(outputFilename, outputPathError);
    if (inputPathError || outputPathError) {
      std::cerr << "Error: unable to canonicalize unparser input/output paths: "
                << file->get_sourceFileNameWithPath() << " and "
                << outputFilename << std::endl;
      ROSE_ABORT();
    }
    if (generatedSource) {
      if (inputPath != outputPath) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[generated-output-path]: identity=%s "
                "output=%s do not name the same path\n",
                file->get_sourceFileNameWithPath().c_str(),
                outputFilename.c_str());
        ROSE_ABORT();
      }
    } else {
      bool aliasesInput = inputPath == outputPath;
      std::error_code existsError;
      const bool outputExists =
          std::filesystem::exists(outputPath, existsError);
      if (existsError) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[output-path]: output=%s unable to "
                "inspect destination: %s\n",
                outputFilename.c_str(), existsError.message().c_str());
        ROSE_ABORT();
      }
      if (!aliasesInput && outputExists) {
        std::error_code equivalentError;
        aliasesInput =
            std::filesystem::equivalent(inputPath, outputPath, equivalentError);
        if (equivalentError) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[output-path]: input=%s output=%s "
                  "unable to compare paths: %s\n",
                  file->get_sourceFileNameWithPath().c_str(),
                  outputFilename.c_str(), equivalentError.message().c_str());
          ROSE_ABORT();
        }
      }
      if (aliasesInput) {
        std::cerr << "Error: refusing to overwrite unparser input without "
                     "-rose:unparser:clobber_input_file: "
                  << inputPath << std::endl;
        ROSE_ABORT();
      }
    }
  }

  // DQ (2/23/2021): Added assertion.
  ROSE_ASSERT(outputFilename.empty() == false);

  if (SgProject::get_verbose() > 0) {
    printf("Calling the unparser: outputFilename = %s \n",
           outputFilename.c_str());
  }

  // DQ (3/19/2014): Added support for noclobber option.
  std::filesystem::path output_file = outputFilename;

  bool trigger_file_comparision = false;

  string saved_filename = outputFilename;

  std::error_code outputExistsError;
  const bool outputExists =
      std::filesystem::exists(output_file, outputExistsError);
  if (outputExistsError) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[output-path]: output=%s unable to "
            "inspect destination: %s\n",
            outputFilename.c_str(), outputExistsError.message().c_str());
    ROSE_ABORT();
  }
  if (outputExists) {
    if (SgProject::get_verbose() > 0) {
      printf("In unparseFile(SgFile*): (outputFilename) output file exists = "
             "%s \n",
             output_file.string().c_str());
    }

    SgProject *project = SageInterface::getProject(file);
    ASSERT_not_null(project);

    if (project->get_noclobber_output_file() == true) {
      // If the file exists then it is an error if -rose:noclobber_output_file
      // option was specified.
      printf("\n\n***********************************************************"
             "******************************************* \n");
      printf("Error: the output file already exists, cannot overwrite "
             "(-rose:noclobber_output_file option specified) \n");
      printf("   --- outputFilename = %s \n", outputFilename.c_str());
      printf("***************************************************************"
             "*************************************** \n\n\n");
      // ROSE_ABORT();
      ROSE_ABORT();
    } else {
      // If the file exists then generate an alternative file so that we can
      // compare the new file to the existing file (error if not identical).
      if (project->get_noclobber_if_different_output_file() == true) {
        // If the file exists and the generated file is not identical, then it
        // is an error if -rose:noclobber_if_different_output_file option was
        // specified.
        trigger_file_comparision = true;
      }
      // Pei-Hung (8/6/2014) appending PID as alternative name to avoid
      // collision
      else {
        if (project->get_appendPID() == true) {
          ostringstream os;
          os << getpid();
          unsigned dot = outputFilename.find_last_of(".");
          outputFilename = outputFilename.substr(0, dot) + "_" + os.str() +
                           outputFilename.substr(dot);
          if (SgProject::get_verbose() > 0)
            printf("Generate test output name with PID = %s \n",
                   outputFilename.c_str());
        }
      }
    }

    if (!hasOutputFilenameOverride) {
      file->set_unparse_output_filename(outputFilename);
    }
  }

  {
    std::unique_ptr<AtomicOutputStagingFile> staging =
        createAtomicOutputStagingFile(outputFilename);
    const string &stagingFilename = staging->filename();
    std::ostream &ROSE_OutputFile = staging->output();

    // file.set_unparse_includes(false);
    // ROSE_ASSERT (file.get_unparse_includes() == false);

    // This is the new unparser that Gary Lee is developing
    // The goal of this unparser is to provide formatting
    // similar to that of the original application code

    // all options are now defined to be false. When these options can be
    // passed in from the prompt, these options will be set accordingly.
    bool UseAutoKeyword = false;
    // bool linefile                      = false;
    bool generateLineDirectives = file->get_unparse_line_directives();

    // DQ (6/19/2007): note that test2004_24.C will fail if this is false.
    // If false, this will cause A.operator+(B) to be unparsed as "A+B". This
    // is a confusing point!
    bool useOverloadedOperators = false;
    // bool useOverloadedOperators        = true;

    bool num = false;

    // It is an error to have this always turned off (e.g. pointer = this;
    // will not unparse correctly)
    bool _this = true;

    bool caststring = false;
    bool _debug = false;
    bool _class = false;
    bool _forced_transformation_format = false;

    // control unparsing of include files into the source file (default is
    // false)
    bool _unparse_includes = file->get_unparse_includes();

    Unparser_Opt roseOptions(UseAutoKeyword, generateLineDirectives,
                             useOverloadedOperators, num, _this, caststring,
                             _debug, _class, _forced_transformation_format,
                             _unparse_includes);

    // printf ("Rose::getFileName(file) = %s \n",Rose::getFileName(file));
    // printf ("file->get_file_info()->get_filenameString = %s
    // \n",file->get_file_info()->get_filenameString().c_str());

    // DQ (7/19/2007): Remove lineNumber from constructor parameter list.
    // int lineNumber = 0;  // Zero indicates that ALL lines should be
    // unparsed Unparser roseUnparser ( &file, &ROSE_OutputFile,
    // Rose::getFileName(&file), roseOptions, lineNumber ); Unparser
    // roseUnparser ( &ROSE_OutputFile, Rose::getFileName(&file), roseOptions,
    // lineNumber, NULL, repl ); Unparser roseUnparser ( &ROSE_OutputFile,
    // Rose::getFileName(file), roseOptions, lineNumber, unparseHelp,
    // unparseDelegate ); Unparser roseUnparser ( &ROSE_OutputFile,
    // file->get_file_info()->get_filenameString(), roseOptions, lineNumber,
    // unparseHelp, unparseDelegate );

    Unparser roseUnparser(
        &ROSE_OutputFile, file->get_file_info()->get_filenameString(),
        roseOptions, unparseHelp, unparseDelegate, preprocessingInfoRewrites,
        nameQualifications, tokenFrontiers);

    // Location to turn on unparser specific debugging data that shows up in
    // the output file This prevents the unparsed output file from compiling
    // properly! ROSE_DEBUG = 0;

    // DQ (12/5/2006): Output information that can be used to colorize
    // properties of generated code (useful for debugging).
    roseUnparser.set_embedColorCodesInGeneratedCode(
        file->get_embedColorCodesInGeneratedCode());
    roseUnparser.set_generateSourcePositionCodes(
        file->get_generateSourcePositionCodes());

    // information that is passed down through the tree (inherited attribute)
    // SgUnparse_Info inheritedAttributeInfo (NO_UNPARSE_INFO);
    SgUnparse_Info inheritedAttributeInfo;

    // DQ (9/24/2013): Set the output language to the inpuse language.
    inheritedAttributeInfo.set_language(file->get_outputLanguage());

    // inheritedAttributeInfo.display("Inside of unparseFile(SgFile* file)");
    // Call member function to start the unparsing process
    // roseUnparser.run_unparser();
    // roseUnparser.unparseFile(file,inheritedAttributeInfo);

    // DQ (9/2/2008): This one way to handle the variations in type
    switch (file->variantT()) {
    case V_SgSourceFile: {
      SgSourceFile *sourceFile = isSgSourceFile(file);

      ROSE_ASSERT(inheritedAttributeInfo.get_current_source_file() == NULL);

      // DQ (8/16/2018): Set this here before it is passed into unparseFile().
      inheritedAttributeInfo.set_current_source_file(sourceFile);
      inheritedAttributeInfo.set_current_physical_file_occurrence_id(
          physicalFileOccurrence);

      ASSERT_not_null(inheritedAttributeInfo.get_current_source_file());

      // DQ (10/29/2018): I now think we need to support this mechanism of
      // specifying the scope to be unparsed separately. This is essential to
      // the support for header files representing nested scopes inside of the
      // global scope. Traversing the global scope does not permit these inner
      // nested scopes to be traversed using the unparser.

      // DQ (8/16/2018): the more conventional usage is to us a single
      // SgSourceFile and SgGlobal for each header file.
      // roseUnparser.unparseFile(sourceFile,inheritedAttributeInfo,
      // unparseScope);
      // roseUnparser.unparseFile(sourceFile,inheritedAttributeInfo, NULL);
      roseUnparser.unparseFile(sourceFile, inheritedAttributeInfo,
                               unparseScope);
      break;
    }

    case V_SgUnknownFile: {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[unknown-file]: file=%s cannot unparse "
              "SgUnknownFile\n",
              file->getFileName().c_str());
      ROSE_ABORT();
    }

    default: {
      printf("Error: default reached in unparser: file = %s \n",
             file->class_name().c_str());
      ROSE_ABORT();
    }
    }

    staging->finishWrites(outputFilename, "output-write");
    if (!trigger_file_comparision) {
      commitAtomicOutput(*staging, outputFilename);
    }

    // DQ (3/19/2014): If -rose:noclobber_if_different_output, then test the
    // generated file against the original file.
    if (trigger_file_comparision == true) {
      // Test the generated file against the previously generated file of the
      // same original name. if different, it is an error, if the same it is
      // OK.

      if (SgProject::get_verbose() > 0) {
        printf("Testing saved_filename against stagingFilename (using "
               "std::filesystem::equivalent()): \n");
        printf("   --- saved_filename       = %s \n", saved_filename.c_str());
        printf("   --- stagingFilename      = %s \n", stagingFilename.c_str());
      }

      std::filesystem::path saved_output_file = saved_filename;
      const bool files_are_identical =
          filesHaveIdenticalContents(saved_output_file, *staging);

      if (SgProject::get_verbose() > 0) {
        printf("   --- files_are_identical = %s \n",
               files_are_identical ? "true" : "false");
      }

      if (files_are_identical == false) {
        fprintf(stderr,
                "\n\n*********************************************************"
                "********************************************* \n");
        fprintf(stderr, "Error: files are not equivalent: \n");
        fprintf(stderr, "   --- saved_filename       = %s \n",
                saved_filename.c_str());
        fprintf(stderr, "   --- stagingFilename      = %s \n",
                stagingFilename.c_str());
        fprintf(stderr,
                "*************************************************************"
                "***************************************** \n\n\n");

        // remove the generated file or leave in place to allow users to
        // examine file differences. std::filesystem::remove(unparsed_file);

        staging->discard(outputFilename, "output-compare");
        ROSE_ABORT();
      }
      staging->discard(outputFilename, "output-compare");
    }
  }
}

namespace {
void addHeaderUnparseIncludeOptions(
    SgProject *project, const list<string> &includeCompilerOptions) {
  ASSERT_not_null(project);
  SgStringList &extraIncludeOptions =
      project->get_extraIncludeDirectorySpecifierBeforeList();
  for (const std::string &option : includeCompilerOptions) {
    if (option.rfind("-I", 0) != 0 || option.size() <= 2) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-include-path]: project=%p "
              "invalid generated-header include option '%s'\n",
              static_cast<void *>(project), option.c_str());
      ROSE_ABORT();
    }
    if (std::find(extraIncludeOptions.begin(), extraIncludeOptions.end(),
                  option) == extraIncludeOptions.end()) {
      extraIncludeOptions.push_back(option);
    }
  }

  SgStringList &originalSourceIncludeOptions =
      project->get_extraIncludeDirectorySpecifierAfterList();
  for (SgFile *projectFile : project->get_fileList()) {
    SgSourceFile *sourceFile = isSgSourceFile(projectFile);
    if (sourceFile == nullptr || sourceFile->get_isHeaderFile()) {
      continue;
    }
    const std::string sourcePath =
        FileHelper::normalizePathIfPossible(sourceFile->getFileName());
    const std::string sourceDirectory = FileHelper::normalizePathIfPossible(
        Rose::getPathFromFileName(sourcePath));
    if (sourcePath.empty() || sourceDirectory.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-include-path]: source=%p path=%s "
              "has no normalized containing directory\n",
              static_cast<void *>(sourceFile),
              sourceFile->getFileName().c_str());
      ROSE_ABORT();
    }
    const std::string option = "-I" + sourceDirectory;
    if (std::find(originalSourceIncludeOptions.begin(),
                  originalSourceIncludeOptions.end(),
                  option) == originalSourceIncludeOptions.end()) {
      originalSourceIncludeOptions.push_back(option);
    }
  }
}
} // namespace

// DQ (3/14/2021): Output include saved in the SgIncludeFile about first and
// last computed statements in each header file.
void buildFirstAndLastStatementsForIncludeFiles(
    SgProject *project, TokenUnparseFrontierContext &context)
// void buildFirstAndLastStatementsForIncludeFiles ( SgSourceFile* sourceFile )
{
  // This function build the mapping of the first and last statements associated
  // with each SgIncludeFile. It is required to support the token-based
  // unparsing so that we can know when the last statement in the SgIncludeFile
  // has been reached, so that we can output the trailing whitespace tokens
  // properly. The function uses the source sequence limits for the file, and
  // computes which statements in the AST are associated with these limits.  It
  // uses a traversal to evaluate the statements (and the associated source
  // sequence number for each statement) against the limits for each file.

  // DQ (3/10/2021): Add performance analysis support.
  TimingPerformance timer("AST buildFirstAndLastStatementsForIncludeFiles:");

#define DEBUG_FIRST_LAST_STMTS 0

#if DEBUG_FIRST_LAST_STMTS || 0
  printf("###################################################### \n");
  printf("###################################################### \n");
  printf("####  buildFirstAndLastStatementsForIncludeFiles  #### \n");
  printf("###################################################### \n");
  printf("###################################################### \n");
#endif

#if DEBUG_FIRST_LAST_STMTS
  printf("In buildFirstAndLastStatementsForIncludeFiles(): project = %p \n",
         project);
  // printf ("In buildFirstAndLastStatementsForIncludeFiles(): sourceFile = %p
  // \n",sourceFile);
#endif

  class IncludeFileStatementTraversal : public AstSimpleProcessing {
  public:
    explicit IncludeFileStatementTraversal(TokenUnparseFrontierContext &context)
        : context(context) {}

    TokenUnparseFrontierContext &context;

    // DQ (3/13/2021): This needs to be the header file and not the original
    // input file. IncludeFileStatementTraversal(SgSourceFile* sourceFile);

    // We need to recorde the first and last statement that are in the same
    // scope. We want to record the first and last in the same scope.  However,
    // an arbitrary include file could has a last statement in a different
    // scope.  Now clear how to handle this pathological case.
    SgScopeStatement *target_scope = NULL;

    SgSourceFile *sourceFile = NULL;

    void visit(SgNode *node) {
#if DEBUG_FIRST_LAST_STMTS
      // printf ("In IncludeFileStatementTraversal::visit(): node = %p = %s
      // \n",node,node->class_name().c_str());
      printf("In IncludeFileStatementTraversal::visit(): node = %p = %s name = "
             "%s \n",
             node, node->class_name().c_str(),
             SageInterface::get_name(node).c_str());
      printf(" --- filename = %s \n",
             node->get_file_info()->get_filenameString().c_str());
#endif
      SgSourceFile *tmp_sourceFile = isSgSourceFile(node);
      if (tmp_sourceFile != NULL) {
        sourceFile = tmp_sourceFile;
        target_scope = nullptr;
        context.includeFilesByPath.clear();
        context.includeFileOccurrencesByPath.clear();

#if DEBUG_FIRST_LAST_STMTS
        printf("Found the input source file: sourceFile->getFileName() \n",
               sourceFile->getFileName().c_str());
#endif
        SgIncludeFile *includeRoot = sourceFile->get_associated_include_file();
        if (includeRoot != NULL) {
          populateIncludeFileMapForUnparsingFromIncludeTree(context,
                                                            includeRoot);
        } else if (sourceFile->get_unparseHeaderFiles()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[include-tree]: file=%s requested "
                  "header unparsing without an include-tree root\n",
                  sourceFile->getFileName().c_str());
          ROSE_ABORT();
        }
      }
      ROSE_ASSERT(sourceFile != NULL);

      // DQ (4/25/2021): I forget why this is a SgDeclarationStatement instead
      // of a SgStatement. SgStatement*             statement             =
      // isSgStatement(node); SgStatement*             statement             =
      // isSgDeclarationStatement(node); DQ (4/28/2021): I think this is the
      // better solution, since we make sure that the last statement is in the
      // same scope (as I recall) below.
      SgStatement *statement = isSgStatement(node);
      SgGlobal *globalScope = isSgGlobal(statement);
      SgFunctionParameterList *functionParameterList =
          isSgFunctionParameterList(node);
      SgFunctionParameterScope *functionParameterScope =
          isSgFunctionParameterScope(node);
      SgCtorInitializerList *ctorInitializerList =
          isSgCtorInitializerList(node);
      SgDeclarationScope *declarationScope = isSgDeclarationScope(node);

      // IR nodes for which we don't want to identify as the first or last
      // statement of a file (header file).
      bool processStatement =
          globalScope == NULL && functionParameterList == NULL &&
          functionParameterScope == NULL && ctorInitializerList == NULL &&
          declarationScope == NULL;

#if DEBUG_FIRST_LAST_STMTS
      printf("statement        = %p \n", statement);
      printf("processStatement = %s \n", processStatement ? "true" : "false");
#endif
      if (statement != NULL && processStatement == true) {
        if (isExactlyAuxiliaryOwned(statement, "token-frontier") ||
            isExactlyTemplateInstantiationDirectivePayload(statement,
                                                           "token-frontier") ||
            isExactlyRangeForSemanticDeclarationPayload(statement,
                                                        "token-frontier") ||
            isExactlySourceLessForStructuralNode(statement, "token-frontier") ||
            isExactlyImplicitControlFlowStructuralNode(statement,
                                                       "token-frontier") ||
            isExactlyCatchSequenceStructuralNode(statement, "token-frontier")) {
          return;
        }
        Sg_File_Info *file_info = statement->get_file_info();
        ROSE_ASSERT(file_info != NULL);
        const bool has_current_file_token_mapping =
            sourceFile->get_tokenSubsequenceMap().find(statement) !=
            sourceFile->get_tokenSubsequenceMap().end();
        if (!file_info->isOutputInCodeGeneration() &&
            !has_current_file_token_mapping) {
          const int physical_file_id =
              requireStructuralPhysicalFileId(statement, "token-frontier");
          const std::string physical_path =
              file_info->getFilenameFromID(physical_file_id);
          if (physical_path.empty() || physical_path == "NULL_FILE" ||
              physical_path == "transformation" ||
              FileHelper::normalizePathIfPossible(physical_path) ==
                  FileHelper::normalizePathIfPossible(
                      sourceFile->getFileName())) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[token-frontier]: non-output "
                    "lexical statement=%p/%s name=%s source=%s:%d:%d has no "
                    "exact external physical owner and no token mapping\n",
                    static_cast<void *>(statement),
                    statement->class_name().c_str(),
                    SageInterface::get_name(statement).c_str(),
                    physical_path.c_str(), file_info->get_line(),
                    file_info->get_col());
            ROSE_ABORT();
          }
          // A source declaration from a non-emitted header remains a semantic
          // dependency in the project AST, but it is outside this source
          // file's emission frontier by construction.
          return;
        }
#if DEBUG_FIRST_LAST_STMTS
        printf("\nIn IncludeFileStatementTraversal::visit(): statement = %p = "
               "%s \n",
               node, node->class_name().c_str());
        printf(" --- statement = %s \n",
               SageInterface::get_name(statement).c_str());
        printf(" --- statement: line = %d column = %d filename = %s \n",
               file_info->get_line(), file_info->get_col(),
               file_info->get_filenameString().c_str());
        printf(
            " --- statement: (physical) line = %d column = %d filename = %s \n",
            file_info->get_physical_line(), file_info->get_col(),
            file_info->get_physical_filename().c_str());
#endif
        // DQ (3/23/2021): The physical_filename of a transformation needs to be
        // derived directly from the physical file id, instead of computed by
        // the get_physical_filename() function. This could be using the file_id
        // instead of the filename as a string.
        int physical_file_id =
            requireStructuralPhysicalFileId(statement, "token-ownership");
        string filename = file_info->getFilenameFromID(physical_file_id);
        if (filename.empty() || filename == "NULL_FILE" ||
            filename == "transformation") {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[token-ownership]: statement=%p "
                  "type=%s physical file id=%d has no registered source path\n",
                  static_cast<void *>(statement),
                  statement->class_name().c_str(), physical_file_id);
          ROSE_ABORT();
        }
#if DEBUG_FIRST_LAST_STMTS
        printf("before reset filename: physical_file_id = %d filename = %s \n",
               physical_file_id, filename.c_str());
#endif
        const bool isTranslationUnitStatement =
            has_current_file_token_mapping ||
            FileHelper::normalizePathIfPossible(filename) ==
                FileHelper::normalizePathIfPossible(sourceFile->getFileName());
        if (!isTranslationUnitStatement &&
            !sourceFile->get_unparseHeaderFiles()) {
          return;
        }
#if DEBUG_FIRST_LAST_STMTS
        printf("after reset filename: physical_file_id  = %d filename = %s \n",
               physical_file_id, filename.c_str());
#endif
        SgIncludeFile *includeFile =
            has_current_file_token_mapping
                ? nullptr
                : lookupIncludeFileForUnparsing(context, filename);
        if (includeFile == NULL && !has_current_file_token_mapping &&
            sourceFile->get_associated_include_file() != NULL) {
          populateIncludeFileMapForUnparsingFromIncludeTree(
              context, sourceFile->get_associated_include_file());
          includeFile = lookupIncludeFileForUnparsing(context, filename);
        }

        if (includeFile != NULL) {
          auto &includeBounds = context.includeFileStatementBounds[includeFile];
          target_scope =
              includeBounds.first != nullptr
                  ? isSgScopeStatement(includeBounds.first->get_parent())
                  : nullptr;
          SgSourceFile *header_file_asssociated_source_file =
              includeFile->get_source_file();

#if DEBUG_FIRST_LAST_STMTS
          printf("Found an SgIncludeFile: includeFile = %p "
                 "header_file_asssociated_source_file = %p \n",
                 includeFile, header_file_asssociated_source_file);
#endif
          // DQ (3/14/2021): This is null for the ROSE-required macro header
          // (pre-included for all ROSE processed code).
          // ROSE_ASSERT(header_file_asssociated_source_file != NULL);
          if (header_file_asssociated_source_file != NULL) {
#if DEBUG_FIRST_LAST_STMTS
            printf("header_file_asssociated_source_file = %s \n",
                   header_file_asssociated_source_file->getFileName().c_str());
            printf("Rose::tokenSubsequenceMapOfMapsBySourceFile.find(header_"
                   "file_asssociated_source_file) != "
                   "Rose::tokenSubsequenceMapOfMapsBySourceFile.end() = %s \n",
                   Rose::tokenSubsequenceMapOfMapsBySourceFile.find(
                       header_file_asssociated_source_file) !=
                           Rose::tokenSubsequenceMapOfMapsBySourceFile.end()
                       ? "true"
                       : "false");
#endif
            if (Rose::tokenSubsequenceMapOfMapsBySourceFile.find(
                    header_file_asssociated_source_file) !=
                Rose::tokenSubsequenceMapOfMapsBySourceFile.end()) {
              // DQ (3/13/2021): Adding support to filter out collecting
              // references to statements that don't have a corresponding token
              // subsequence.
              std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
                  &tokenStreamSequenceMap = header_file_asssociated_source_file
                                                ->get_tokenSubsequenceMap();
#if DEBUG_FIRST_LAST_STMTS
              printf(" --- tokenStreamSequenceMap.size() = %zu \n",
                     tokenStreamSequenceMap.size());
#endif
              // ROSE_ASSERT(statementBoundsMap.find(includeFile) !=
              // statementBoundsMap.end());

              if (includeBounds.first == NULL) {
#if DEBUG_FIRST_LAST_STMTS
                printf("Previously NULL: first time seeing a statement for "
                       "includeFile->get_filename() = %s \n",
                       includeFile->get_filename().str());
#endif
                // DQ (3/13/2021): We need to make sure that the first and last
                // statements that we select correspond to a collected token
                // subsequence. In codeSegregation test_141_1.h, demonstrates
                // such a case. includeFile->set_firstStatement(statement);
                // target_scope = isSgScopeStatement(statement->get_parent());

                // TokenStreamSequenceToNodeMapping* tokenSubsequence =
                // tokenStreamSequenceMap[stmt];
                if (tokenStreamSequenceMap.find(statement) !=
                    tokenStreamSequenceMap.end()) {
                  ROSE_ASSERT(statement != NULL);

                  includeBounds.first = statement;

                  ROSE_ASSERT(statement->get_parent() != NULL);

                  target_scope = isSgScopeStatement(statement->get_parent());

#if DEBUG_FIRST_LAST_STMTS
                  printf(" --- target_scope = %p = %s \n", target_scope,
                         target_scope->class_name().c_str());
#endif
                } else {
#if DEBUG_FIRST_LAST_STMTS
                  printf("We can't record this as a first statement becuae it "
                         "does not correspond to a token subsequence \n");
#endif
                }
              }

#if DEBUG_FIRST_LAST_STMTS
              printf(" --- (before testing statement parent) statement = %p = "
                     "%s \n",
                     statement, statement->class_name().c_str());
#endif
              ROSE_ASSERT(statement->get_parent() != NULL);
#if DEBUG_FIRST_LAST_STMTS
              if (target_scope != NULL) {
                printf(" --- (before testing statement parent) target_scope    "
                       "        = %p = %s \n",
                       target_scope, target_scope->class_name().c_str());
              }
              printf(" --- (before testing statement parent) "
                     "statement->get_parent() = %p = %s \n",
                     statement->get_parent(),
                     statement->get_parent()->class_name().c_str());
#endif
              // includeFile->set_lastStatement(statement);
              // if (statement->get_parent() == target_scope)
              if (target_scope != NULL &&
                  statement->get_parent() == target_scope) {
                // DQ (3/13/2021): We need to make sure that the first and last
                // statements that we select correspond to a collected token
                // subsequence. In codeSegregation test_141_1.h, demonstrates
                // such a case. includeFile->set_lastStatement(statement);
                if (tokenStreamSequenceMap.find(statement) !=
                    tokenStreamSequenceMap.end()) {
#if DEBUG_FIRST_LAST_STMTS
                  printf("This can be a last statement (it has an associated "
                         "token subsequence) \n");
#endif
                  includeBounds.second = statement;
                } else {
#if DEBUG_FIRST_LAST_STMTS
                  // printf ("We can't record this as a last statement because
                  // it does not correspond to a token subsequence \n");
                  printf("This can be a last statement even if it does not "
                         "have an associated token subsequence (e.g. it may be "
                         "a transforamtion) \n");
#endif
                  // DQ (23/2021): I think we should because it might be that a
                  // transformation is a last statement of a header file and in
                  // which case it is still the last statement independent of if
                  // it is unparsed via the token stream or from the AST.
                  includeBounds.second = statement;
                }

                SgStatement *computedLastStatement =
                    SageInterface::getLastStatement(target_scope);

                // It might be better to make sure that the last "}" is in the
                // current file. if (statement == computedLastStatement &&
                // includeFile->get_firstStatement() == target_scope)
                if (statement == computedLastStatement &&
                    target_scope->get_endOfConstruct()) {
                  // Then make the scope the last statement.
#if DEBUG_FIRST_LAST_STMTS
                  printf("Since this is the last statement of the scope, then "
                         "make the scope the last statement \n");
#endif
                  includeBounds.second = target_scope;
                }
              } else {
#if DEBUG_FIRST_LAST_STMTS
                // printf
                // ("Rose::tokenSubsequenceMapOfMapsBySourceFile.find(header_file_asssociated_source_file)
                // == Rose::tokenSubsequenceMapOfMapsBySourceFile.end() \n");
                printf("This is a different scope: statement->get_parent() == "
                       "target_scope (first and last statements must be in the "
                       "same scope) \n");
#endif
              }
            }
          }
        } else {
          // Not all statements will be in the header files, however this could
          // be the input source file, so we need to support computing the first
          // and last statements in that file as well.
          if (!isTranslationUnitStatement) {
            fprintf(
                stderr,
                "REX_UNPARSE_INVARIANT[token-ownership]: statement=%p "
                "type=%s file=%s compiler-generated=%d frontend-specific=%d "
                "output=%d transformed=%d parent=%s scope=%s is neither the "
                "translation unit nor present in its include tree\n",
                static_cast<void *>(statement), statement->class_name().c_str(),
                filename.c_str(), file_info->isCompilerGenerated() ? 1 : 0,
                file_info->isFrontendSpecific() ? 1 : 0,
                file_info->isOutputInCodeGeneration() ? 1 : 0,
                file_info->isTransformation() ? 1 : 0,
                statement->get_parent() != nullptr
                    ? statement->get_parent()->class_name().c_str()
                    : "<null>",
                statement->get_scope() != nullptr
                    ? statement->get_scope()->class_name().c_str()
                    : "<null>");
            if (SgVariableDeclaration *variable_decl =
                    isSgVariableDeclaration(statement)) {
              fprintf(stderr,
                      "REX_UNPARSE_INVARIANT[token-ownership]: variable "
                      "names=");
              for (SgInitializedName *variable :
                   variable_decl->get_variables()) {
                fprintf(stderr, "%s%s",
                        variable == variable_decl->get_variables().front()
                            ? ""
                            : ",",
                        variable != nullptr
                            ? variable->get_name().getString().c_str()
                            : "<null>");
              }
              fprintf(stderr, "\n");
            }
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[token-ownership]: ancestry=");
            for (SgNode *ancestor = statement; ancestor != nullptr;
                 ancestor = ancestor->get_parent()) {
              fprintf(stderr, "%s%s", ancestor == statement ? "" : " <- ",
                      ancestor->class_name().c_str());
              if (SgClassDefinition *class_definition =
                      isSgClassDefinition(ancestor)) {
                SgClassDeclaration *class_declaration =
                    class_definition->get_declaration();
                fprintf(stderr, "(%s)",
                        class_declaration != nullptr
                            ? class_declaration->get_name().getString().c_str()
                            : "<null-declaration>");
              }
            }
            fprintf(stderr, "\n");
            ROSE_ABORT();
          }
#if DEBUG_FIRST_LAST_STMTS
          printf("filename not found in includeFileMapForUnparsing: filename = "
                 "%s \n",
                 filename.c_str());
#endif
          ROSE_ASSERT(sourceFile != NULL);

          // DQ (3/13/2021): Adding support to filter out collecting references
          // to statements that don't have a corresponding token subsequence.
          std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
              &tokenStreamSequenceMap = sourceFile->get_tokenSubsequenceMap();
          auto &sourceBounds = context.sourceFileStatementBounds[sourceFile];
          target_scope =
              sourceBounds.first != nullptr
                  ? isSgScopeStatement(sourceBounds.first->get_parent())
                  : nullptr;
#if DEBUG_FIRST_LAST_STMTS
          printf(" --- tokenStreamSequenceMap.size() = %zu \n",
                 tokenStreamSequenceMap.size());
#endif

          // DQ (5/20/2021): The firstStatment and lastStatement in the source
          // file are only used for the input source file (the include files
          // used their own data member).
          if (sourceBounds.first == NULL) {
#if DEBUG_FIRST_LAST_STMTS
            printf("Previously NULL: first time seeing a statement for "
                   "sourceFile->get_filename() = %s \n",
                   sourceFile->getFileName().c_str());
#endif
            // DQ (3/13/2021): We need to make sure that the first and last
            // statements that we select correspond to a collected token
            // subsequence. In codeSegregation test_141_1.h, demonstrates such a
            // case. includeFile->set_firstStatement(statement); target_scope =
            // isSgScopeStatement(statement->get_parent());

            // TokenStreamSequenceToNodeMapping* tokenSubsequence =
            // tokenStreamSequenceMap[stmt];
            if (tokenStreamSequenceMap.find(statement) !=
                tokenStreamSequenceMap.end()) {
              sourceBounds.first = statement;
              target_scope = isSgScopeStatement(statement->get_parent());

#if DEBUG_FIRST_LAST_STMTS
              printf(" --- target_scope = %p = %s \n", target_scope,
                     target_scope->class_name().c_str());
#endif
            } else {
#if DEBUG_FIRST_LAST_STMTS
              printf("We can't record this as a first statement becuase it "
                     "does not correspond to a token subsequence \n");
#endif
            }
          }

          ROSE_ASSERT(statement->get_parent() != NULL);
#if DEBUG_FIRST_LAST_STMTS
          printf(" --- (before testing statement parent) target_scope          "
                 "  = %p = %s \n",
                 target_scope, target_scope->class_name().c_str());
          printf(" --- (before testing statement parent) "
                 "statement->get_parent() = %p = %s \n",
                 statement->get_parent(),
                 statement->get_parent()->class_name().c_str());
#endif
          // includeFile->set_lastStatement(statement);
          if (target_scope != NULL && statement->get_parent() == target_scope) {
            // DQ (5/20/2021): We can't have the last statement be the global
            // scope in the input source file. DQ (3/13/2021): We need to make
            // sure that the first and last statements that we select correspond
            // to a collected token subsequence. In codeSegregation
            // test_141_1.h, demonstrates such a case.
            // includeFile->set_lastStatement(statement);
            // if (tokenStreamSequenceMap.find(statement) !=
            // tokenStreamSequenceMap.end())
            if (isSgGlobal(statement) == NULL) {
              if (tokenStreamSequenceMap.find(statement) !=
                  tokenStreamSequenceMap.end()) {
#if DEBUG_FIRST_LAST_STMTS
                printf("This can be a last statement (it has an associated "
                       "token subsequence) \n");
#endif
                sourceBounds.second = statement;
              } else {
#if DEBUG_FIRST_LAST_STMTS
                // printf ("We can't record this as a last statement because it
                // does not correspond to a token subsequence \n");
                printf("This can be a last statement even if it does not have "
                       "an associated token subsequence (e.g. it may be a "
                       "transforamtion) \n");
#endif
                // DQ (23/2021): I think we should because it might be that a
                // transformation is a last statement of a header file and in
                // which case it is still the last statement independent of if
                // it is unparsed via the token stream or from the AST.
                // sourceFile->set_lastStatement(statement);
                if (tokenStreamSequenceMap.find(statement) !=
                    tokenStreamSequenceMap.end()) {
                  sourceBounds.second = statement;
                } else {
#if DEBUG_FIRST_LAST_STMTS
                  printf("We can't record this as a last statement becuase it "
                         "does not correspond to a token subsequence \n");
#endif
                }
              }
            }

            SgStatement *computedLastStatement =
                SageInterface::getLastStatement(target_scope);
#if DEBUG_FIRST_LAST_STMTS
            printf("computedLastStatement = %p = %s \n", computedLastStatement,
                   computedLastStatement->class_name().c_str());
#endif
            // It might be better to make sure that the last "}" is in the
            // current file. if (statement == computedLastStatement &&
            // includeFile->get_firstStatement() == target_scope) if (statement
            // == computedLastStatement && target_scope->get_endOfConstruct() )
            // if (isSgGlobal(statement) == NULL && statement ==
            // computedLastStatement && target_scope->get_endOfConstruct() ) if
            // (statement == computedLastStatement &&
            // target_scope->get_endOfConstruct() )
            if (isSgGlobal(target_scope) == NULL &&
                statement == computedLastStatement &&
                target_scope->get_endOfConstruct()) {
              // Then make the scope the last statement.
#if DEBUG_FIRST_LAST_STMTS
              printf("Since this is the last statement of the scope, then make "
                     "the scope the last statement (except for SgGlobal) \n");
#endif
              sourceBounds.second = target_scope;
            }
          } else {
#if DEBUG_FIRST_LAST_STMTS
            // printf
            // ("Rose::tokenSubsequenceMapOfMapsBySourceFile.find(header_file_asssociated_source_file)
            // == Rose::tokenSubsequenceMapOfMapsBySourceFile.end() \n");
            printf("This is a different scope: statement->get_parent() == "
                   "target_scope (first and last statements must be in the "
                   "same scope) \n");
#endif
          }
        }
      }
    }
  };

  // IncludeFileStatementTraversal traversal(statementBoundsMap);
  IncludeFileStatementTraversal traversal(context);

#if DEBUG_FIRST_LAST_STMTS
  printf("Before call to traversal \n");
#endif

  // DQ (3/11/2021): Need to call this on a specific source file (the one on the
  // original command line, not the dynamic library file, so that we will avoid
  // marking SgIncludeFiles twice (since two files will include the same header
  // files).  An altrnative implementation could define a header file list for
  // each source file, but this would be more complex and error prone, and hard
  // to debug)). traversal.traverse(project,preorder); SgFilePtrList & fileList
  // = project->get_files();
  SgFilePtrList &fileList = project->get_fileList();
  for (size_t i = 0; i < fileList.size(); i++) {
    SgSourceFile *sourceFile = isSgSourceFile(fileList[i]);

    // DQ (8/23/2021): If this is a binary file then sourceFile == NULL (see
    // tests in
    // tests/nonsmoke/functional/CompilerOptionsTests/testGenerateSourceFileNames)
    // ROSE_ASSERT(sourceFile != NULL);
    if (sourceFile != NULL && !sourceFile->get_skip_unparse()) {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
      printf("\nFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
             "FFFFFFFFFFF \n");
      printf("Testing for isDynamicLibrary before calling traversal for "
             "filename = %s \n",
             sourceFile->getFileName().c_str());
#endif
#if DEBUG_FIRST_LAST_STMTS_SCOPES
      printf("sourceFile->get_isDynamicLibrary() = %s \n",
             sourceFile->get_isDynamicLibrary() ? "true" : "false");
#endif
      // DQ (5/24/2021): Since we have to unparse all the files, we need to
      // compute first and last on all of the files. What is less clear is what
      // to do with the information about shared header files. I think that at
      // worst it is redundant information.
#if DEBUG_FIRST_LAST_STMTS_SCOPES
      printf("Calling traversal for filename = %s \n",
             sourceFile->getFileName().c_str());
#endif
      traversal.traverse(sourceFile, preorder);

      // Copy the information of first and last statement per scope for each
      // file to store it in the source file.
      // context.scopeStatementBoundsBySourceFile[sourceFile]
      // = traversal.firstAndLastStatementsToUnparseInScopeMap;

#if DEBUG_FIRST_LAST_STMTS_SCOPES
      printf("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
             "FFFFFFFFF \n\n");
#endif
    }
  }

#if DEBUG_FIRST_LAST_STMTS_SCOPES
  printf("After call to traversal \n");
#endif

#if DEBUG_FIRST_LAST_STMTS_SCOPES || 0
  // DQ (3/14/2021): Output include saved in the SgIncludeFile about first and
  // last computed statements in each header file. void
  // outputFirstAndLastIncludeFileInfo(); outputFirstAndLastIncludeFileInfo();

  // for (size_t i = 0; i < fileList.size(); i++)
  //    {
  //      SgSourceFile* sourceFile = isSgSourceFile(fileList[i]);
  //      ROSE_ASSERT(sourceFile != NULL);

  //      printf ("In buildFirstAndLastStatementsForScopes(): iteration i = %d
  //      \n",i);

  // DQ (5/24/2021): Since we have to unparse all the files, we need to compute
  // first and last on all of the files. What is less clear is what to do with
  // the information about shared header files. I think that at worst it is
  // redundant information.
  // if (sourceFile->get_isDynamicLibrary() == false)
  //    {
  // Only output once... some tools (e.g. codeSegregation tool) will build an
  // additional file, in which case we don't want to output info for that file.
  // outputFirstAndLastIncludeFileInfo(sourceFile);
  // outputFirstAndLastStatementsInScope(sourceFile);

  // ROSE_ASSERT(context.scopeStatementBoundsBySourceFile.find(sourceFile)
  // != context.scopeStatementBoundsBySourceFile.end());

  std::map<SgSourceFile *,
           std::map<SgScopeStatement *,
                    std::pair<SgStatement *, SgStatement *>>>::iterator j =
      context.scopeStatementBoundsBySourceFile.begin();

  printf("Output context.scopeStatementBoundsBySourceFile "
         "(size = %zu): \n",
         context.scopeStatementBoundsBySourceFile.size());
  while (j != context.scopeStatementBoundsBySourceFile.end()) {
    SgSourceFile *tmp_sourceFile = j->first;
    printf("tmp_sourceFile = %p name = %s \n", tmp_sourceFile,
           tmp_sourceFile->getFileName().c_str());

    // std::map<SgScopeStatement*,std::pair<SgStatement*,SgStatement*> >
    // firstAndLastStatements =
    // context.scopeStatementBoundsBySourceFile[sourceFile];
    std::map<SgScopeStatement *, std::pair<SgStatement *, SgStatement *>>
        firstAndLastStatements = j->second;

    printf(" --- firstAndLastStatements (size() = %zu): \n",
           firstAndLastStatements.size());
    std::map<SgScopeStatement *,
             std::pair<SgStatement *, SgStatement *>>::iterator i =
        firstAndLastStatements.begin();
    while (i != firstAndLastStatements.end()) {
      SgScopeStatement *scope = i->first;
      ROSE_ASSERT(scope != NULL);
      printf(" --- --- scope              = %p = %s name = %s \n", scope,
             scope->class_name().c_str(),
             SageInterface::get_name(scope).c_str());

      SgStatement *firstStatement = i->second.first;
      printf(" --- --- --- firstStatement  = %p \n", firstStatement);
      if (firstStatement != NULL) {
        printf(" --- --- --- firstStatement = %p = %s name = %s \n",
               firstStatement, firstStatement->class_name().c_str(),
               SageInterface::get_name(firstStatement).c_str());
      }

      SgStatement *lastStatement = i->second.second;
      printf(" --- --- --- lastStatement  = %p \n", lastStatement);
      if (lastStatement != NULL) {
        printf(" --- --- --- lastStatement  = %p = %s name = %s \n",
               lastStatement, lastStatement->class_name().c_str(),
               SageInterface::get_name(lastStatement).c_str());
      }

      printf("\n");

      i++;
    }

    printf("\n");

    j++;
  }
  // }
  // }
#endif

#if DEBUG_FIRST_LAST_STMTS_SCOPES
  printf("Leaving buildFirstAndLastStatementsForScopes(): project = %p \n",
         project);
#endif
}

void buildFirstAndLastStatementsForScopes(
    SgProject *project, TokenUnparseFrontierContext &context) {
  // DQ (5/27/2021): We need a more comprehensive handling of identifing first
  // and last statements specific to each scope and for each file.  It is
  // similar to the function above that computed the first and last statement
  // for each file (source files and header files that have been transformed).

  // This function build the mapping of the first and last statements associated
  // with each SgScopeStatement. It is required to support the token-based
  // unparsing so that we can know when the last statement in the SgIncludeFile
  // has been reached, so that we can output the trailing whitespace tokens
  // properly. The function uses the source sequence limits for the file, and
  // computes which statements in the AST are associated with these limits.  It
  // uses a traversal to evaluate the statements (and the associated source
  // sequence number for each statement) against the limits for each file.

  // DQ (3/10/2021): Add performance analysis support.
  TimingPerformance timer("AST buildFirstAndLastStatementsForScopes:");

#define DEBUG_FIRST_LAST_STMTS_SCOPES 0

#if DEBUG_FIRST_LAST_STMTS_SCOPES || 0
  printf("################################################ \n");
  printf("################################################ \n");
  printf("####  buildFirstAndLastStatementsForScopes  #### \n");
  printf("################################################ \n");
  printf("################################################ \n");
#endif

#if DEBUG_FIRST_LAST_STMTS_SCOPES
  printf("In buildFirstAndLastStatementsForScopes(): project = %p \n", project);
#endif

  class StatementTraversal : public AstSimpleProcessing {
  public:
    explicit StatementTraversal(TokenUnparseFrontierContext &context)
        : context(context) {}

    TokenUnparseFrontierContext &context;

    // We need to recorde the first and last statement that are in the same
    // scope.

    SgSourceFile *sourceFile = NULL;

    // DQ (6/1/2021): Added to handle.
    int physical_file_id_from_source_file = -1;

    void visit(SgNode *node) {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
      // printf ("In StatementTraversal::visit(): node = %p = %s
      // \n",node,node->class_name().c_str());
      printf("In StatementTraversal::visit(): node = %p = %s name = %s \n",
             node, node->class_name().c_str(),
             SageInterface::get_name(node).c_str());
      printf(" --- filename = %s \n",
             node->get_file_info()->get_filenameString().c_str());
#endif
      SgSourceFile *tmp_sourceFile = isSgSourceFile(node);
      if (tmp_sourceFile != NULL) {
        sourceFile = tmp_sourceFile;
        context.includeFilesByPath.clear();
        context.includeFileOccurrencesByPath.clear();

#if DEBUG_FIRST_LAST_STMTS_SCOPES
        printf("Found the input source file: sourceFile->getFileName() = %s \n",
               sourceFile->getFileName().c_str());
#endif

        SgGlobal *globalScope = sourceFile->get_globalScope();
        physical_file_id_from_source_file =
            globalScope->get_file_info()->get_physical_file_id();

#if DEBUG_FIRST_LAST_STMTS_SCOPES
        printf("setting physical_file_id_from_source_file = %d \n",
               physical_file_id_from_source_file);
#endif
        SgIncludeFile *includeRoot = sourceFile->get_associated_include_file();
        if (includeRoot != NULL) {
          populateIncludeFileMapForUnparsingFromIncludeTree(context,
                                                            includeRoot);
        } else if (sourceFile->get_unparseHeaderFiles()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[include-tree]: file=%s requested "
                  "header unparsing without an include-tree root\n",
                  sourceFile->getFileName().c_str());
          ROSE_ABORT();
        }
      }
      ROSE_ASSERT(sourceFile != NULL);

      // DQ (4/25/2021): I forget why this is a SgDeclarationStatement instead
      // of a SgStatement. SgStatement*             statement             =
      // isSgStatement(node); SgStatement*             statement             =
      // isSgDeclarationStatement(node); DQ (4/28/2021): I think this is the
      // better solution, since we make sure that the last statement is in the
      // same scope (as I recall) below.
      SgStatement *statement = isSgStatement(node);
      SgGlobal *globalScope = isSgGlobal(statement);
      SgFunctionParameterList *functionParameterList =
          isSgFunctionParameterList(node);
      SgFunctionParameterScope *functionParameterScope =
          isSgFunctionParameterScope(node);
      SgCtorInitializerList *ctorInitializerList =
          isSgCtorInitializerList(node);
      SgDeclarationScope *declarationScope = isSgDeclarationScope(node);

      // IR nodes for which we don't want to identify as the first or last
      // statement of a file (header file).
      bool processStatement =
          globalScope == NULL && functionParameterList == NULL &&
          functionParameterScope == NULL && ctorInitializerList == NULL &&
          declarationScope == NULL;

#if DEBUG_FIRST_LAST_STMTS_SCOPES
      printf("statement        = %p \n", statement);
      printf("processStatement = %s \n", processStatement ? "true" : "false");
#endif

      // DQ (5/27/2021): Add an entry for the global scope (even through we
      // don't process the global scope directly (see definition of
      // processStatement), we store information specific to the parent of the
      // statements that we process and so we store information in the entry for
      // the global scope).
      if (globalScope != NULL) {
        // firstAndLastStatementsToUnparseInScopeMap[globalScope] =
        // std::pair<SgStatement*,SgStatement*>(NULL,NULL);
        // firstAndLastStatementsToUnparseInScopeMap->[sourceFile][globalScope]
        // = std::pair<SgStatement*,SgStatement*>(NULL,NULL);
        if (context.scopeStatementBoundsBySourceFile.find(sourceFile) ==
            context.scopeStatementBoundsBySourceFile.end()) {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
          printf("A map for this file already exists \n");
#endif
          context.scopeStatementBoundsBySourceFile[sourceFile] =
              std::map<SgScopeStatement *,
                       std::pair<SgStatement *, SgStatement *>>();
        } else {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
          printf("A map for this file already exists \n");
#endif
        }
      }

      if (statement != NULL && processStatement == true) {
        if (isExactlyAuxiliaryOwned(statement, "token-frontier") ||
            isExactlyTemplateInstantiationDirectivePayload(statement,
                                                           "token-frontier") ||
            isExactlyRangeForSemanticDeclarationPayload(statement,
                                                        "token-frontier") ||
            isExactlySourceLessForStructuralNode(statement, "token-frontier") ||
            isExactlyImplicitControlFlowStructuralNode(statement,
                                                       "token-frontier") ||
            isExactlyCatchSequenceStructuralNode(statement, "token-frontier")) {
          return;
        }
        Sg_File_Info *file_info = statement->get_file_info();
        ROSE_ASSERT(file_info != NULL);
        const bool has_current_file_token_mapping =
            sourceFile->get_tokenSubsequenceMap().find(statement) !=
            sourceFile->get_tokenSubsequenceMap().end();
        if (!file_info->isOutputInCodeGeneration() &&
            !has_current_file_token_mapping) {
          const int physical_file_id =
              requireStructuralPhysicalFileId(statement, "token-frontier");
          const std::string physical_path =
              file_info->getFilenameFromID(physical_file_id);
          if (physical_path.empty() || physical_path == "NULL_FILE" ||
              physical_path == "transformation" ||
              FileHelper::normalizePathIfPossible(physical_path) ==
                  FileHelper::normalizePathIfPossible(
                      sourceFile->getFileName())) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[token-frontier]: non-output "
                    "lexical statement=%p/%s name=%s source=%s:%d:%d has no "
                    "exact external physical owner and no token mapping\n",
                    static_cast<void *>(statement),
                    statement->class_name().c_str(),
                    SageInterface::get_name(statement).c_str(),
                    physical_path.c_str(), file_info->get_line(),
                    file_info->get_col());
            ROSE_ABORT();
          }
          // A source declaration from a non-emitted header remains a semantic
          // dependency in the project AST, but it is outside this source
          // file's emission frontier by construction.
          return;
        }

#if DEBUG_FIRST_LAST_STMTS_SCOPES
        printf("\nIn StatementTraversal::visit(): statement = %p = %s \n", node,
               node->class_name().c_str());
        printf(" --- statement = %s \n",
               SageInterface::get_name(statement).c_str());
        printf(" --- statement: line = %d column = %d filename = %s \n",
               file_info->get_line(), file_info->get_col(),
               file_info->get_filenameString().c_str());
        printf(
            " --- statement: (physical) line = %d column = %d filename = %s \n",
            file_info->get_physical_line(), file_info->get_col(),
            file_info->get_physical_filename().c_str());
#endif
        SgScopeStatement *parentScope =
            isSgScopeStatement(statement->get_parent());

#if DEBUG_FIRST_LAST_STMTS_SCOPES
        printf("parentScope = %p \n", parentScope);
        if (parentScope != NULL) {
          printf("parentScope = %p = %s \n", parentScope,
                 parentScope->class_name().c_str());
        }
#endif
        // DQ (5/27/2021): Filter out some scopes that don't have lists of
        // children.
        bool skipScope = (isSgFunctionDefinition(statement) != NULL);
        (void)skipScope;

        // DQ (5/27/2021): Filter out some scopes that don't have lists of
        // children.
        bool skipParentScope = (isSgFunctionDefinition(parentScope) != NULL);
        if (skipParentScope == true) {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
          printf("setting parentScope = NULL \n");
#endif
          parentScope = NULL;
        }

        // DQ (3/23/2021): The physical_filename of a transformation needs to be
        // derived directly from the physical file id, instead of computed by
        // the get_physical_filename() function. This could be using the file_id
        // instead of the filename as a string.
        int physical_file_id =
            requireStructuralPhysicalFileId(statement, "token-ownership");
        string filename = file_info->getFilenameFromID(physical_file_id);
        if (filename.empty() || filename == "NULL_FILE" ||
            filename == "transformation") {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[token-ownership]: statement=%p "
                  "type=%s physical file id=%d has no registered source path\n",
                  static_cast<void *>(statement),
                  statement->class_name().c_str(), physical_file_id);
          ROSE_ABORT();
        }

#if DEBUG_FIRST_LAST_STMTS_SCOPES
        printf("before reset filename: physical_file_id = %d filename = %s \n",
               physical_file_id, filename.c_str());
#endif
        const bool isTranslationUnitStatement =
            has_current_file_token_mapping ||
            FileHelper::normalizePathIfPossible(filename) ==
                FileHelper::normalizePathIfPossible(sourceFile->getFileName());
        if (!isTranslationUnitStatement &&
            !sourceFile->get_unparseHeaderFiles()) {
          return;
        }

#if DEBUG_FIRST_LAST_STMTS_SCOPES
        printf("after reset filename: physical_file_id  = %d filename = %s \n",
               physical_file_id, filename.c_str());
#endif
        SgIncludeFile *includeFile =
            has_current_file_token_mapping
                ? nullptr
                : lookupIncludeFileForUnparsing(context, filename);
        if (includeFile == NULL && !has_current_file_token_mapping &&
            sourceFile->get_associated_include_file() != NULL) {
          populateIncludeFileMapForUnparsingFromIncludeTree(
              context, sourceFile->get_associated_include_file());
          includeFile = lookupIncludeFileForUnparsing(context, filename);
        }

        if (includeFile != NULL) {
          SgSourceFile *header_file_asssociated_source_file =
              includeFile->get_source_file();

#if DEBUG_FIRST_LAST_STMTS_SCOPES
          printf("Found an SgIncludeFile: includeFile = %p "
                 "header_file_asssociated_source_file = %p \n",
                 includeFile, header_file_asssociated_source_file);
#endif
          // DQ (3/14/2021): This is null for the ROSE-required macro header
          // (pre-included for all ROSE processed code).
          // ROSE_ASSERT(header_file_asssociated_source_file != NULL);
          if (header_file_asssociated_source_file != NULL) {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
            printf("header_file_asssociated_source_file = %s \n",
                   header_file_asssociated_source_file->getFileName().c_str());
            printf("Rose::tokenSubsequenceMapOfMapsBySourceFile.find(header_"
                   "file_asssociated_source_file) != "
                   "Rose::tokenSubsequenceMapOfMapsBySourceFile.end() = %s \n",
                   Rose::tokenSubsequenceMapOfMapsBySourceFile.find(
                       header_file_asssociated_source_file) !=
                           Rose::tokenSubsequenceMapOfMapsBySourceFile.end()
                       ? "true"
                       : "false");
#endif
            if (Rose::tokenSubsequenceMapOfMapsBySourceFile.find(
                    header_file_asssociated_source_file) !=
                Rose::tokenSubsequenceMapOfMapsBySourceFile.end()) {
              // DQ (3/13/2021): Adding support to filter out collecting
              // references to statements that don't have a corresponding token
              // subsequence.
              std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
                  &tokenStreamSequenceMap = header_file_asssociated_source_file
                                                ->get_tokenSubsequenceMap();
#if DEBUG_FIRST_LAST_STMTS_SCOPES
              printf(" --- tokenStreamSequenceMap.size() = %zu \n",
                     tokenStreamSequenceMap.size());
#endif
              // DQ (5/27/2021): Add an entry for the global scope (even through
              // we don't process the global scope directly (see definition of
              // processStatement), we store information specific to the parent
              // of the statements that we process and so we store information
              // in the entry for the global scope).
              {
                if (context.scopeStatementBoundsBySourceFile.find(
                        header_file_asssociated_source_file) ==
                    context.scopeStatementBoundsBySourceFile.end()) {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                  printf(
                      "Add map for this file is it does NOT already exists \n");
#endif
                  context.scopeStatementBoundsBySourceFile
                      [header_file_asssociated_source_file] =
                      std::map<SgScopeStatement *,
                               std::pair<SgStatement *, SgStatement *>>();
                } else {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                  printf("A map for this file ALREADY exists \n");
#endif
                }
              }

              ROSE_ASSERT(context.scopeStatementBoundsBySourceFile.find(
                              header_file_asssociated_source_file) !=
                          context.scopeStatementBoundsBySourceFile.end());

              if (context.scopeStatementBoundsBySourceFile.find(
                      header_file_asssociated_source_file) ==
                  context.scopeStatementBoundsBySourceFile.end()) {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                printf("A map for this file already exists \n");
#endif
                context.scopeStatementBoundsBySourceFile
                    [header_file_asssociated_source_file] =
                    std::map<SgScopeStatement *,
                             std::pair<SgStatement *, SgStatement *>>();
              } else {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                printf("A map for this file already exists \n");
#endif
              }

              // Need to add the global scope to support children of the global
              // scope that are in the inlcude files. SgGlobal* globalScope =
              // isSgGlobal(parentScope);
              if (parentScope != NULL) {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                printf("parentScope != NULL \n");
#endif
                if (context
                        .scopeStatementBoundsBySourceFile
                            [header_file_asssociated_source_file]
                        .find(parentScope) ==
                    context
                        .scopeStatementBoundsBySourceFile
                            [header_file_asssociated_source_file]
                        .end()) {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                  printf("Adding parentScope = %p = %s \n", parentScope,
                         parentScope->class_name().c_str());
#endif
                  context.scopeStatementBoundsBySourceFile
                      [header_file_asssociated_source_file][parentScope] =
                      std::pair<SgStatement *, SgStatement *>(NULL, NULL);
                } else {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                  printf("Rose::"
                         "firstAndLastStatementsToUnparseInScopeMapBySourceFile"
                         "[header_file_asssociated_source_file][parentScope] "
                         "is initialized \n");
#endif
                }
              }

              if (parentScope != NULL) {
                ROSE_ASSERT(context.scopeStatementBoundsBySourceFile.find(
                                header_file_asssociated_source_file) !=
                            context.scopeStatementBoundsBySourceFile.end());
                if (context
                        .scopeStatementBoundsBySourceFile
                            [header_file_asssociated_source_file]
                        .find(parentScope) ==
                    context
                        .scopeStatementBoundsBySourceFile
                            [header_file_asssociated_source_file]
                        .end()) {
                  printf(
                      "Error: "
                      "(Rose::"
                      "firstAndLastStatementsToUnparseInScopeMapBySourceFile["
                      "header_file_asssociated_source_file].find(parentScope) "
                      "== "
                      "Rose::"
                      "firstAndLastStatementsToUnparseInScopeMapBySourceFile["
                      "header_file_asssociated_source_file].end()) == true \n");
                  printf(" --- statement   = %p = %s = %s \n", statement,
                         statement->class_name().c_str(),
                         SageInterface::get_name(statement).c_str());
                  printf(" --- parentScope = %p = %s = %s \n", parentScope,
                         parentScope->class_name().c_str(),
                         SageInterface::get_name(parentScope).c_str());
                }
                ROSE_ASSERT(context
                                .scopeStatementBoundsBySourceFile
                                    [header_file_asssociated_source_file]
                                .find(parentScope) !=
                            context
                                .scopeStatementBoundsBySourceFile
                                    [header_file_asssociated_source_file]
                                .end());

                if (context
                        .scopeStatementBoundsBySourceFile
                            [header_file_asssociated_source_file][parentScope]
                        .first == NULL) {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                  printf("Previously NULL: first time seeing a statement for "
                         "includeFile->get_filename() = %s \n",
                         includeFile->get_filename().str());
#endif
                  if (tokenStreamSequenceMap.find(statement) !=
                      tokenStreamSequenceMap.end()) {
                    ROSE_ASSERT(statement != NULL);

                    context
                        .scopeStatementBoundsBySourceFile
                            [header_file_asssociated_source_file][parentScope]
                        .first = statement;

                    ROSE_ASSERT(statement->get_parent() != NULL);
                  } else {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                    printf("We can't record this as a first statement becuae "
                           "it does not correspond to a token subsequence \n");
#endif
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                    printf("physical_file_id_from_source_file = %d \n",
                           physical_file_id_from_source_file);
                    printf("physical_file_id                  = %d \n",
                           physical_file_id);
#endif
                    if (physical_file_id == physical_file_id_from_source_file) {
                      context
                          .scopeStatementBoundsBySourceFile
                              [header_file_asssociated_source_file][parentScope]
                          .first = statement;
                    }
                  }
                }

#if DEBUG_FIRST_LAST_STMTS_SCOPES
                printf(" --- (before testing statement parent) statement = %p "
                       "= %s \n",
                       statement, statement->class_name().c_str());
#endif
                ROSE_ASSERT(statement->get_parent() != NULL);

                // DQ (3/13/2021): We need to make sure that the first and last
                // statements that we select correspond to a collected token
                // subsequence. In codeSegregation test_141_1.h, demonstrates
                // such a case.
                if (tokenStreamSequenceMap.find(statement) !=
                    tokenStreamSequenceMap.end()) {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                  printf("This can be a last statement (it has an associated "
                         "token subsequence) \n");
#endif
                  context
                      .scopeStatementBoundsBySourceFile
                          [header_file_asssociated_source_file][parentScope]
                      .second = statement;
                } else {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                  printf("We can't record this as a last statement because it "
                         "does not correspond to a token subsequence \n");
#endif
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                  printf("physical_file_id_from_source_file = %d \n",
                         physical_file_id_from_source_file);
                  printf("physical_file_id                  = %d \n",
                         physical_file_id);
#endif
                  if (physical_file_id == physical_file_id_from_source_file) {
                    context
                        .scopeStatementBoundsBySourceFile
                            [header_file_asssociated_source_file][parentScope]
                        .second = statement;
                  }
                }
              }
            }
          }
        } else {
          if (!isTranslationUnitStatement) {
            fprintf(
                stderr,
                "REX_UNPARSE_INVARIANT[token-ownership]: statement=%p "
                "type=%s file=%s compiler-generated=%d frontend-specific=%d "
                "output=%d transformed=%d is neither the translation unit "
                "nor present in its include tree\n",
                static_cast<void *>(statement), statement->class_name().c_str(),
                filename.c_str(), file_info->isCompilerGenerated() ? 1 : 0,
                file_info->isFrontendSpecific() ? 1 : 0,
                file_info->isOutputInCodeGeneration() ? 1 : 0,
                file_info->isTransformation() ? 1 : 0);
            ROSE_ABORT();
          }
#if DEBUG_FIRST_LAST_STMTS_SCOPES
          printf("filename not found in includeFileMapForUnparsing: filename = "
                 "%s \n",
                 filename.c_str());
#endif
          ROSE_ASSERT(sourceFile != NULL);

          // DQ (3/13/2021): Adding support to filter out collecting references
          // to statements that don't have a corresponding token subsequence.
          std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
              &tokenStreamSequenceMap = sourceFile->get_tokenSubsequenceMap();
#if DEBUG_FIRST_LAST_STMTS_SCOPES
          printf(" --- tokenStreamSequenceMap.size() = %zu \n",
                 tokenStreamSequenceMap.size());
#endif

          if (context.scopeStatementBoundsBySourceFile.find(sourceFile) ==
              context.scopeStatementBoundsBySourceFile.end()) {
            context.scopeStatementBoundsBySourceFile[sourceFile] =
                std::map<SgScopeStatement *,
                         std::pair<SgStatement *, SgStatement *>>();
          }

          if (parentScope != NULL) {
            if (context.scopeStatementBoundsBySourceFile[sourceFile].find(
                    parentScope) ==
                context.scopeStatementBoundsBySourceFile[sourceFile].end()) {
              context
                  .scopeStatementBoundsBySourceFile[sourceFile][parentScope] =
                  std::pair<SgStatement *, SgStatement *>(NULL, NULL);
            }

            ROSE_ASSERT(
                context.scopeStatementBoundsBySourceFile[sourceFile].find(
                    parentScope) !=
                context.scopeStatementBoundsBySourceFile[sourceFile].end());

            if (context
                    .scopeStatementBoundsBySourceFile[sourceFile][parentScope]
                    .first == NULL) {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
              printf("Previously NULL: first time seeing a statement for "
                     "sourceFile->get_filename() = %s \n",
                     sourceFile->getFileName().c_str());
#endif
              if (tokenStreamSequenceMap.find(statement) !=
                  tokenStreamSequenceMap.end()) {
                context
                    .scopeStatementBoundsBySourceFile[sourceFile][parentScope]
                    .first = statement;
              } else {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
                printf("We can't record this as a first statement becuae it "
                       "does not correspond to a token subsequence \n");
#endif
                if (physical_file_id == physical_file_id_from_source_file) {
                  context
                      .scopeStatementBoundsBySourceFile[sourceFile][parentScope]
                      .first = statement;
                }
              }
            }

            if (tokenStreamSequenceMap.find(statement) !=
                tokenStreamSequenceMap.end()) {
              context.scopeStatementBoundsBySourceFile[sourceFile][parentScope]
                  .second = statement;
            } else {
#if DEBUG_FIRST_LAST_STMTS_SCOPES
              printf("We can't record this as a last statement because it does "
                     "not correspond to a token subsequence \n");
#endif
              if (physical_file_id == physical_file_id_from_source_file) {
                context
                    .scopeStatementBoundsBySourceFile[sourceFile][parentScope]
                    .second = statement;
              }
            }
          }
        }
      }
    }
  };

  StatementTraversal traversal(context);

#if DEBUG_FIRST_LAST_STMTS_SCOPES
  printf("Before call to traversal \n");
#endif

  SgFilePtrList &fileList = project->get_fileList();
  for (size_t i = 0; i < fileList.size(); i++) {
    SgSourceFile *sourceFile = isSgSourceFile(fileList[i]);
    ROSE_ASSERT(sourceFile != NULL);

#if DEBUG_FIRST_LAST_STMTS_SCOPES
    printf("\nFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
           "FFFFFFFFF \n");
    printf("Testing for isDynamicLibrary before calling traversal for filename "
           "= %s \n",
           sourceFile->getFileName().c_str());
#endif
#if DEBUG_FIRST_LAST_STMTS_SCOPES
    printf("sourceFile->get_isDynamicLibrary() = %s \n",
           sourceFile->get_isDynamicLibrary() ? "true" : "false");
#endif
    // DQ (5/24/2021): Since we have to unparse all the files, we need to
    // compute first and last on all of the files. What is less clear is what to
    // do with the information about shared header files. I think that at worst
    // it is redundant information.
#if DEBUG_FIRST_LAST_STMTS_SCOPES
    printf("Calling traversal for filename = %s \n",
           sourceFile->getFileName().c_str());
#endif
    traversal.traverse(sourceFile, preorder);

#if DEBUG_FIRST_LAST_STMTS_SCOPES
    printf("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
           "FFFFFFF \n\n");
#endif
  }

#if DEBUG_FIRST_LAST_STMTS_SCOPES
  printf("After call to traversal \n");
#endif

#if DEBUG_FIRST_LAST_STMTS_SCOPES
  printf("Leaving buildFirstAndLastStatementsForScopes(): project = %p \n",
         project);
#endif
}

// DQ (11/10/2018): Move this ot a more common location.
void generateGraphOfIncludeFiles(SgProject *project, std::string filename);

void unparseIncludedFiles(
    SgProject *project, UnparseFormatHelp *unparseFormatHelp,
    UnparseDelegate *unparseDelegate,
    UnparsePreprocessingInfoRewriteMap *preprocessingInfoRewrites,
    NameQualificationContext *nameQualifications,
    TokenUnparseFrontierContext *tokenFrontiers) {
  ASSERT_not_null(project);

  UnparsePreprocessingInfoRewriteMap localPreprocessingInfoRewrites;
  UnparsePreprocessingInfoRewriteMap *effectivePreprocessingInfoRewrites =
      preprocessingInfoRewrites != nullptr ? preprocessingInfoRewrites
                                           : &localPreprocessingInfoRewrites;
  if (!effectivePreprocessingInfoRewrites->empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[rewrite-plan]: project=%p include rewrite "
            "plan was not empty at session start\n",
            static_cast<void *>(project));
    ROSE_ABORT();
  }

  TokenUnparseFrontierContext localTokenFrontiers;
  TokenUnparseFrontierContext *effectiveTokenFrontiers =
      tokenFrontiers != nullptr ? tokenFrontiers : &localTokenFrontiers;

#define DEBUG_UNPARSE_INCLUDE_FILES 0

  // DQ (3/10/2021): Add performance analysis support.
  TimingPerformance timer("AST unparseIncludedFiles:");

  // DQ (3/11/2020): The transformation of header files causes them to be output
  // into a separate directory location. The paths associated with each
  // transformed header file must be saved so that then can be output on the
  // compile line for the generated source files.  The current design supports
  // an extra include path list so that we can support the specification of the
  // paths to the transformed header files ahead of the source file's original
  // include path list. However, the extra include paths are strored in the
  // SgSourceFile IR nodes, and there can be more than one source file that is
  // required to support the include path list that includes the paths to the
  // transformed header files. Not clear which is the best way to support this.
  // 1) Currently we build the list in the SgSourceFile for the include file to
  // have a modified extra include path list,
  //    however, this is pointless since the header files are not compiled. And
  //    it is insufficient because the source files that are compiled don't have
  //    the correct extra include path entries.
  // 2) We could store the extra include paths as they are built up in this
  // function, but I don't think this function
  //    is calling the backend compiler.
  // 3) We could build the list of the source files to be compiled, but this
  // might not be a great solution since it would cause every source file to
  // have the same extra include paths. 4) (best idea so far) We need to add a
  // second extra list of include paths to the SgProject object, then when
  // building the include list for each file we can first (or second) include
  // the paths from the SgProject's extra include paths list before adding those
  // specific to the SgSourceFile.

#if DEBUG_UNPARSE_INCLUDE_FILES
  printf("In unparseIncludedFiles(): project = %p \n", project);
#endif

#if DEBUG_UNPARSE_INCLUDE_FILES
  printf("In unparseIncludedFiles(): Output "
         "include_file_graph_from_top_of_unparseIncludedFiles DOT graph \n");
  string dotgraph_filename =
      "include_file_graph_from_top_of_unparseIncludedFiles";
  generateGraphOfIncludeFiles(project, dotgraph_filename);
  printf("DONE: In unparseIncludedFiles(): Output "
         "include_file_graph_from_top_of_unparseIncludedFiles DOT graph \n");
#endif

  // DQ (3/8/2021): Build a map of the first and last statement in each include
  // file. buildFirstAndLastStatementsForIncludeFiles(project);

  // DQ (5/2/2021): Get the file so that we can get the data member for
  // unparseHeaderFiles. NOTE: An improvement would be to make the data member
  // for unparseHeaderFiles a static data member.
  SgSourceFile *headerSourceFile = nullptr;
  bool needsAnyTokenFrontier = false;
  for (SgFile *projectFile : project->get_fileList()) {
    SgSourceFile *candidate = isSgSourceFile(projectFile);
    if (candidate == nullptr || candidate->get_skip_unparse()) {
      continue;
    }
    needsAnyTokenFrontier =
        needsAnyTokenFrontier || candidate->get_unparse_tokens();
    if (candidate->get_unparseHeaderFiles() && headerSourceFile == nullptr) {
      headerSourceFile = candidate;
    }
  }
  if (tokenFrontiers == nullptr) {
    for (SgFile *projectFile : project->get_fileList()) {
      SgSourceFile *candidate = isSgSourceFile(projectFile);
      if (candidate == nullptr || candidate->get_skip_unparse()) {
        continue;
      }
      const bool needsTokenFrontier = candidate->get_unparse_tokens();
      if (needsTokenFrontier) {
        buildTokenStreamFrontier(candidate, candidate->get_unparseHeaderFiles(),
                                 *effectiveTokenFrontiers);
      }
    }
  }

  // Proceed only if there are input files and they require header files
  // unparsing. if (!project -> get_fileList().empty() && (*(project ->
  // get_fileList()).begin()) -> get_unparseHeaderFiles())
  if (headerSourceFile != nullptr) {
    if (SgProject::get_verbose() >= 1) {
      cout << endl << "***HEADER FILES UNPARSING***" << endl << endl;
    }

    IncludedFilesUnparser includedFilesUnparser(project);

#if DEBUG_UNPARSE_INCLUDE_FILES
    printf("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
           "BBBBBBBBBBBB \n");
    printf("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
           "BBBBBBBBBBBB \n");
    printf("In unparseIncludedFiles(): calling "
           "buildFirstAndLastStatementsForIncludeFiles() \n");
    printf("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
           "BBBBBBBBBBBB \n");
    printf("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
           "BBBBBBBBBBBB \n");
#endif

    // DQ (3/14/2021): Moved to be after includedFilesUnparser(). Build a map of
    // the first and last statement in each include file.
    // buildFirstAndLastStatementsForIncludeFiles(project);
    SgSourceFile *sourceFile = headerSourceFile;
    // DQ (9/20/2018): Choosing a better name for this function.
    // includedFilesUnparser.unparse();
    includedFilesUnparser.figureOutWhichFilesToUnparse();
    *effectivePreprocessingInfoRewrites =
        includedFilesUnparser.getIncludeRewrites();

    const string &unparseRootPath = includedFilesUnparser.getUnparseRootPath();
    const map<string, string> &unparseMap =
        includedFilesUnparser.getUnparseMap();
    const map<string, SgScopeStatement *> &unparseScopesMap =
        includedFilesUnparser.getUnparseScopesMap();
    const map<string, unsigned int> &unparseOccurrenceMap =
        includedFilesUnparser.getUnparseOccurrenceMap();
    const std::set<std::string> filesWithRelevantModifications =
        collectFilesWithRelevantModifications(project);
    const std::set<std::string> &filesWithMarkedTransformations =
        includedFilesUnparser.getFilesWithMarkedTransformations();
    const std::set<std::string> &filesWithUpdatedIncludePaths =
        includedFilesUnparser.getFilesWithUpdatedIncludePaths();
    const map<string, SgSourceFile *> unparseSourceFileMap =
        includedFilesUnparser.getUnparseSourceFileMap();

    if (needsAnyTokenFrontier) {
      prepareMaterializedHeaderTokenFrontiers(project, *effectiveTokenFrontiers,
                                              unparseScopesMap);
      buildFirstAndLastStatementsForIncludeFiles(project,
                                                 *effectiveTokenFrontiers);
      buildFirstAndLastStatementsForScopes(project, *effectiveTokenFrontiers);
    }

    // Validate the complete header plan before creating any output file. A
    // dirty header without a frontend AST is never eligible for snapshot copy.
    for (const auto &entry : unparseMap) {
      const std::string originalFileName =
          FileHelper::normalizePathIfPossible(entry.first);
      if (originalFileName.empty() || entry.second.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-plan]: input=%s output=%s has "
                "an incomplete planned path\n",
                entry.first.c_str(), entry.second.c_str());
        ROSE_ABORT();
      }

      bool requiresAstUnparsing = headerRequiresAstUnparsing(
          filesWithRelevantModifications, filesWithMarkedTransformations,
          filesWithUpdatedIncludePaths, originalFileName);
      const auto materializedSource =
          unparseSourceFileMap.find(originalFileName);
      SgSourceFile *materializedHeader =
          materializedSource != unparseSourceFileMap.end()
              ? materializedSource->second
              : nullptr;
      const auto scope = unparseScopesMap.find(originalFileName);
      if (scope != unparseScopesMap.end() &&
          scopeHasRelevantModifications(scope->second, originalFileName,
                                        materializedHeader)) {
        requiresAstUnparsing = true;
      }
      if (!requiresAstUnparsing) {
        continue;
      }

      auto source = unparseSourceFileMap.find(originalFileName);
      if (source == unparseSourceFileMap.end() || source->second == NULL) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[dirty-header-without-ast]: %s was "
                "modified but the frontend did not materialize its "
                "SgSourceFile\n",
                originalFileName.c_str());
        ROSE_ABORT();
      }
      if (FileHelper::normalizePathIfPossible(source->second->getFileName()) !=
          originalFileName) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-filename]: expected=%s "
                "frontend-source=%s\n",
                originalFileName.c_str(),
                source->second->getFileName().c_str());
        ROSE_ABORT();
      }
    }

    addHeaderUnparseIncludeOptions(
        project, includedFilesUnparser.getIncludeCompilerOptions());

    // #if DEBUG_UNPARSE_INCLUDE_FILES
    // DQ (4/4/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 2) {
      const map<string, SgSourceFile *> &temp_unparseSourceFileMap =
          includedFilesUnparser.getUnparseSourceFileMap();
      printf("Output the temp_unparseSourceFileMap: \n");
      for (map<string, SgSourceFile *>::const_iterator sourceFile =
               temp_unparseSourceFileMap.begin();
           sourceFile != temp_unparseSourceFileMap.end(); sourceFile++) {
        printf("   --- sourceFile->first = %s sourceFile->second = %p = %s \n",
               sourceFile->first.c_str(), sourceFile->second,
               sourceFile->second->class_name().c_str());
      }
    }
    // #endif

    // DQ (11/19/2018): Copy the files that are specified in the filesToCopy
    // list.
    const set<string> copySet = includedFilesUnparser.getFilesToCopy();
    for (const string &originalFileName : copySet) {
      const string copiedOutputFileName =
          includedFilesUnparser.getCopiedFileOutputPath(originalFileName);
      if (copiedOutputFileName.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-plan]: copied header=%s has no "
                "planned output path\n",
                originalFileName.c_str());
        ROSE_ABORT();
      }
      if (headerRequiresAstUnparsing(
              filesWithRelevantModifications, filesWithMarkedTransformations,
              filesWithUpdatedIncludePaths, originalFileName)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-plan]: modified header=%s was "
                "incorrectly classified for snapshot copy\n",
                originalFileName.c_str());
        ROSE_ABORT();
      }
      copyOriginalHeaderToOutputLocation(project, originalFileName,
                                         copiedOutputFileName);
    }

#if DEBUG_UNPARSE_INCLUDE_FILES
    printf("file->get_unparseHeaderFiles() = %s \n",
           file->get_unparseHeaderFiles() ? "true" : "false");
#endif

    // DQ (5/2/2021): We can assert this because of the predicate for this true
    // case (above).
    ROSE_ASSERT(headerSourceFile->get_unparseHeaderFiles() == true);

#if DEBUG_UNPARSE_INCLUDE_FILES
    printf("In Unparser::unparseFile(): calling buildTokenStreamFrontier(): "
           "filename = %s \n",
           headerSourceFile->getFileName().c_str());
#endif
    // DQ (5/2/2021): This is the version from 5/1/2021.
    // buildTokenStreamFrontier(file);

#if DEBUG_UNPARSE_INCLUDE_FILES
    printf("DONE: In Unparser::unparseFile(): Building token stream mapping "
           "frontier! \n");
#endif

    for (map<string, string>::const_iterator unparseMapEntry =
             unparseMap.begin();
         unparseMapEntry != unparseMap.end(); unparseMapEntry++) {
      // const string & originalFileName = unparseMapEntry -> first;
      string originalFileName = unparseMapEntry->first;
      bool requiresAstUnparsing = headerRequiresAstUnparsing(
          filesWithRelevantModifications,
          includedFilesUnparser.getFilesWithMarkedTransformations(),
          includedFilesUnparser.getFilesWithUpdatedIncludePaths(),
          originalFileName);

      string originalFileNameWithoutPath =
          Rose::utility_stripPathFromFileName(originalFileName);

      // DQ (3/7/2020): Save the path so that we can include it in the list of
      // include paths that we need to add.
      string originalFileNamePath = Rose::getPathFromFileName(originalFileName);

#if DEBUG_UNPARSE_INCLUDE_FILES
      printf("Use this path when compiling generated code: "
             "originalFileNamePath = %s \n",
             originalFileNamePath.c_str());
#endif
      // #if DEBUG_UNPARSE_INCLUDE_FILES
      // DQ (4/4/2020): Added header file unparsing feature specific debug
      // level.
      if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
        printf("TOP of loop over unparseMap \n");
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
      }
      // #endif

      // #if DEBUG_UNPARSE_INCLUDE_FILES
      // DQ (4/4/2020): Added header file unparsing feature specific debug
      // level.
      if (SgProject::get_unparseHeaderFilesDebug() >= 7) {
        printf("In unparseIncludedFiles(): Processing unparseMapEntries: "
               "originalFileName            = %s \n",
               originalFileName.c_str());
        printf("In unparseIncludedFiles(): Processing unparseMapEntries: "
               "originalFileNameWithoutPath = %s \n",
               originalFileNameWithoutPath.c_str());
      }
      // #endif
      const auto unparseScopeIter = unparseScopesMap.find(originalFileName);
      SgSourceFile *unparsedFile = NULL;
      const auto sourceFileIter = unparseSourceFileMap.find(
          FileHelper::normalizePathIfPossible(originalFileName));
      if (sourceFileIter != unparseSourceFileMap.end()) {
        unparsedFile = sourceFileIter->second;
      }
      if (unparseScopeIter != unparseScopesMap.end() &&
          scopeHasRelevantModifications(unparseScopeIter->second,
                                        originalFileName, unparsedFile)) {
        requiresAstUnparsing = true;
      }

      if (unparsedFile != NULL && fileHasRelevantModifications(unparsedFile)) {
        requiresAstUnparsing = true;
      }

      if (unparsedFile == NULL) {
        if (requiresAstUnparsing) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[dirty-header-without-ast]: %s was "
                  "modified but the frontend did not materialize its "
                  "SgSourceFile\n",
                  originalFileName.c_str());
          ROSE_ABORT();
        }
        const string outputFileName = FileHelper::concatenatePaths(
            unparseRootPath, unparseMapEntry->second);
        copyOriginalHeaderToOutputLocation(project, originalFileName,
                                           outputFileName);
        continue;
      }

      // #if DEBUG_UNPARSE_INCLUDE_FILES
      // DQ (4/4/2020): Added header file unparsing feature specific debug
      // level.
      if (SgProject::get_unparseHeaderFilesDebug() >= 7) {
        printf(
            "Unparse file from unparseMap: unparsedFile = %p filename = %s \n",
            unparsedFile, unparsedFile->getFileName().c_str());
        printf("                              size of global scope = %zu \n",
               unparsedFile->get_globalScope()->get_declarations().size());
      }
      // #endif

      // DQ (11/12/2018): This is the newer approach to getting any associated
      // SgIncludeFile information.
      SgIncludeFile *associated_include_file =
          unparsedFile->get_associated_include_file();
      if (associated_include_file == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-include-node]: header=%s has "
                "no associated SgIncludeFile\n",
                originalFileName.c_str());
        ROSE_ABORT();
      }
      if (associated_include_file != NULL) {
        if (requiresAstUnparsing == true &&
            unparsedFile->get_unparse_tokens() == true &&
            associated_include_file
                    ->get_can_be_supported_using_token_based_unparsing() ==
                false) {
          const string outputFileName = FileHelper::concatenatePaths(
              unparseRootPath, unparseMapEntry->second);
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[header-token-support]: header=%s "
                  "output=%s was modified but its token mapping is marked "
                  "unsafe\n",
                  originalFileName.c_str(), outputFileName.c_str());
          ROSE_ABORT();
        }
      }

      // DQ (10/2/2019): This will be checked below (test it here), but it is
      // not reasonable for a header file when using the header file unparsing
      // optimization. DQ (11/7/2018): Make sure that this is available.
      // ASSERT_not_null(unparsedFile->get_project());
      const string expectedHeaderPath =
          FileHelper::normalizePathIfPossible(originalFileName);
      const string actualEmissionPath =
          FileHelper::normalizePathIfPossible(unparsedFile->getFileName());
      const string actualParsePath = FileHelper::normalizePathIfPossible(
          unparsedFile->get_sourceFileNameWithPath());
      const bool parseIdentityIsInternallyConsistent =
          !actualParsePath.empty() &&
          unparsedFile->get_sourceFileNameWithoutPath() ==
              FileHelper::getFileName(
                  unparsedFile->get_sourceFileNameWithPath());
      const bool headerParseIdentityMatches =
          unparsedFile->get_isHeaderFile() == false ||
          actualParsePath == expectedHeaderPath;
      if (expectedHeaderPath.empty() ||
          actualEmissionPath != expectedHeaderPath ||
          !parseIdentityIsInternallyConsistent || !headerParseIdentityMatches) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[header-filename]: expected=%s "
                "emission=%s parse-input=%s parse-basename=%s header=%d\n",
                originalFileName.c_str(), unparsedFile->getFileName().c_str(),
                unparsedFile->get_sourceFileNameWithPath().c_str(),
                unparsedFile->get_sourceFileNameWithoutPath().c_str(),
                unparsedFile->get_isHeaderFile() ? 1 : 0);
        ROSE_ABORT();
      }

      ASSERT_not_null(unparsedFile->get_parent());
      // #if DEBUG_UNPARSE_INCLUDE_FILES
      // DQ (4/4/2020): Added header file unparsing feature specific debug
      // level.
      if (SgProject::get_unparseHeaderFilesDebug() >= 7) {
        printf("Processing unparseMapEntries: unparseMapEntry->second = %s \n",
               unparseMapEntry->second.c_str());
        printf("Processing unparseMapEntries: unparsedFile->get_parent() = %p "
               "= %s \n",
               unparsedFile->get_parent(),
               unparsedFile->get_parent()->class_name().c_str());
      }
      // #endif
      // The planning phase owns the complete relative output path. Recomputing
      // it here from AST parent shape or the application root can place the
      // header outside the include-search path and silently select the original
      // unmodified header.
      const string outputFileName = FileHelper::concatenatePaths(
          unparseRootPath, unparseMapEntry->second);
      FileHelper::ensureParentFolderExists(outputFileName);

      // DQ (10/2/2019): The project is an input parameter to this function,
      // plus the get_project() function can requrn NULL for a header file
      // (within header file optimzation). DQ (11/7/2018): Make sure that this
      // is available. ASSERT_not_null(unparsedFile->get_project()); DQ
      // (9/11/2018): Check that this is a header file (and not the original
      // source file).
      if (unparsedFile->get_isHeaderFile() == true) {
        if (requiresAstUnparsing == false) {
          copyOriginalHeaderToOutputLocation(project, originalFileName,
                                             outputFileName);
          continue;
        }

        // Unparse only included files (the original source file will be
        // unparsed as usual).

        // DQ (5/19/2020): Need to insert comments and CPP directives for this
        // header file. It is also required to add the source position
        // information to the include directives. DQ (10/29/2018): We can't just
        // unparse the file using the translation unit's global scope since we
        // would not visit statements from header files that are nested. So we
        // need to either put the statements from the associated scope were the
        // header file's statements are located into the global scope, or
        // reference the associated inner scope directly so that it will be
        // unparsed (directly).  The previous solution was to call the unparser
        // for each statement in a loop over the statements in the associated
        // scope.  Not clear if that is enough for the token-based unparsing.

        // string filename = unparseScopesMap[
        // SgIncludeFile* include_file =
        // Rose::includeFileMapForUnparsing[filename];
        const string filename = unparsedFile->getFileName();
        ROSE_ASSERT(unparseScopesMap.find(filename) != unparseScopesMap.end());

        map<string, SgScopeStatement *>::const_iterator unparseScopesMapEntry =
            unparseScopesMap.find(originalFileName);
        ROSE_ASSERT(unparseScopesMapEntry != unparseScopesMap.end());

        // const SgScopeStatement* header_file_associated_scope =
        // unparseScopesMap[filename]; const SgScopeStatement*
        // header_file_associated_scope = unparseScopesMapEntry->second;
        SgScopeStatement *header_file_associated_scope =
            unparseScopesMapEntry->second;
        ASSERT_not_null(header_file_associated_scope);
        const auto occurrenceEntry =
            unparseOccurrenceMap.find(originalFileName);
        if (occurrenceEntry == unparseOccurrenceMap.end() ||
            occurrenceEntry->second == 0) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[header-context]: header=%s has no "
                  "exact selected lexical occurrence\n",
                  originalFileName.c_str());
          ROSE_ABORT();
        }
        // unparseFile(unparsedFile, unparseFormatHelp, unparseDelegate, NULL);
        // unparseStatement(header_file_associated_scope);
        // u_exprStmt->unparseStatement(header_file_associated_scope, ninfo);

        // DQ (10/29/2018): Maybe this is the best way to handle this (4th
        // parameter is non-NULL for header file unparsing. This might be a
        // better solution.
        {
          // Headers included inside function bodies can map to a basic block.
          // Those scopes still need AST-based unparsing so transformed
          // statements in the header are emitted instead of silently copying
          // the original file back out.
#if DEBUG_UNPARSE_INCLUDE_FILES
          printf("calling unparseFile(): using header_file_associated_scope %p "
                 "= %s : calling unparseFile() \n",
                 header_file_associated_scope,
                 header_file_associated_scope->class_name().c_str());
#endif
#if DEBUG_UNPARSE_INCLUDE_FILES
          printf("header_file_associated_scope->get_containsTransformation() = "
                 "%s \n",
                 header_file_associated_scope->get_containsTransformation()
                     ? "true"
                     : "false");
#endif
          bool marking_the_header_file_associated_scope = false;
          if (header_file_associated_scope->get_containsTransformation() ==
              false) {
#if DEBUG_UNPARSE_INCLUDE_FILES
            printf("Resetting the scope as containing a transformation, since "
                   "it should contain one somewhere inside \n");
#endif
            marking_the_header_file_associated_scope = true;
            // header_file_associated_scope->set_containsTransformation(true);
          }

          unparseFile(unparsedFile, unparseFormatHelp, unparseDelegate,
                      header_file_associated_scope,
                      effectivePreprocessingInfoRewrites, &outputFileName,
                      nameQualifications, effectiveTokenFrontiers,
                      occurrenceEntry->second);

#if DEBUG_UNPARSE_INCLUDE_FILES
          printf(
              "DONE: calling unparseFile(): using header_file_associated_scope "
              "%p = %s : calling unparseFile() \n",
              header_file_associated_scope,
              header_file_associated_scope->class_name().c_str());
#endif
          if (marking_the_header_file_associated_scope == true) {
#if DEBUG_UNPARSE_INCLUDE_FILES
            printf("Resetting the scope as containing a transformation to "
                   "FALSE, so we can leave it the way it was \n");
#endif
            // header_file_associated_scope->set_containsTransformation(false);
          }
        }

      } else {
      }

      // #if DEBUG_UNPARSE_INCLUDE_FILES
      // DQ (4/4/2020): Added header file unparsing feature specific debug
      // level.
      if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
        printf("BOTTOM of loop over unparseMap \n");
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
      }
      // #endif
    }

  } else {
    printf("This may be where we need to compute the first and last statements "
           "for each scope \n");

    // DQ (5/22/2021): This should be called only when we are using the token
    // based unparsing. DQ (5/20/2021): Need to support this here where we have
    // only a source file with no header files.
    // buildFirstAndLastStatementsForIncludeFiles(project);
    SgSourceFile *sourceFile = headerSourceFile;

    // if (sourceFile->get_unparse_tokens() == true)
    if (sourceFile != NULL && needsAnyTokenFrontier &&
        tokenFrontiers == nullptr) {
      buildFirstAndLastStatementsForIncludeFiles(project,
                                                 *effectiveTokenFrontiers);

      // DQ (5/27/2021): We need to debug the collection of first and last
      // statements associated with each scope (actually we just need the last
      // statement for each scope). This only important for the token-based
      // unarpsing.
      buildFirstAndLastStatementsForScopes(project, *effectiveTokenFrontiers);
    }
  }

  // DQ (9/14/2018): At this point we have unparsed the include files, and
  // skipped the unparsing of the source file (since they will be unparsed using
  // the usual memchanism which ignores include files).

  // #if DEBUG_UNPARSE_INCLUDE_FILES || 0
  // DQ (4/4/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("Leaving unparseIncludedFiles() project = %p \n", project);
  }
  // #endif
}

// DQ (10/11/2007): I think this is redundant with the
// Unparser::unparseProject() member function But it is allowed to call it
// directly from the user's translator if compilation using the backend is not
// required!  So we have to allow it to be here.

void unparseProject(SgProject *project, UnparseFormatHelp *unparseFormatHelp,
                    UnparseDelegate *unparseDelegate) {
  ASSERT_not_null(project);
  enforceTokenUnparseContract(project);
  UnparsePreprocessingInfoRewriteMap preprocessingInfoRewrites;
  NameQualificationContext nameQualifications;
  nameQualifications.clear();
  TokenUnparseFrontierContext tokenFrontiers;

#define DEBUG_UNPARSE_PROJECT 0

  // #if 1
  // DQ (4/4/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("In unparseProject(): project = %p \n", project);
  }
  // #endif

  // Put the call to support name qualification here!
#if DEBUG_UNPARSE_PROJECT || 0
  printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
         "!!!! \n");
  printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
         "!!!! \n");
  printf("In unparseProject(): calling computeNameQualification() for the "
         "whole AST \n");
  printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
         "!!!! \n");
  printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
         "!!!! \n");
#endif

#if DEBUG_UNPARSE_PROJECT
  printf("In unparseProject(): Calling buildFirstAndLastStatementsForScopes(): "
         "Testing for robustness ... \n");
#endif

#if DEBUG_UNPARSE_PROJECT
  printf(
      "DONE: In unparseProject(): Calling "
      "buildFirstAndLastStatementsForScopes(): Testing for robustness ... \n");
#endif

  // DQ (8/7/2018): Added assertion.
  ASSERT_not_null(project->get_fileList_ptr());

  SgSourceFile *sourceFile = NULL;
  bool hasHeaderUnparseSource = false;
  bool needsTokenBoundaries = false;
  // DQ (8/7/2018): Call the name qualification support on each file in the
  // project.
  for (size_t i = 0; i < project->get_fileList_ptr()->get_listOfFiles().size();
       ++i) {
    // These are actually separate translation units.
    SgFile *file = project->get_fileList_ptr()->get_listOfFiles()[i];
    ASSERT_not_null(file);
    // SgSourceFile* sourceFile = isSgSourceFile(file);
    sourceFile = isSgSourceFile(file);
    // ASSERT_not_null(sourceFile);
    // DQ (8/7/2018): We might want to allow mixed collections of binaries and
    // source files.

    if (sourceFile != NULL && !sourceFile->get_skip_unparse()) {
      hasHeaderUnparseSource =
          hasHeaderUnparseSource || sourceFile->get_unparseHeaderFiles();
      // #if 1
      // DQ (4/4/2020): Added header file unparsing feature specific debug
      // level.
      if (SgProject::get_unparseHeaderFilesDebug() >= 2) {
        printf("In unparseProject(): loop over all files: calling "
               "computeNameQualification() for sourceFile = %p = %s \n",
               sourceFile, sourceFile->getFileName().c_str());
      }
      // #endif

      Unparser::computeNameQualification(sourceFile, nameQualifications);

      // DQ (4/4/2020): Added header file unparsing feature specific debug
      // level.
      if (SgProject::get_unparseHeaderFilesDebug() >= 2) {
        printf("DONE: In unparseProject(): loop over all files: calling "
               "computeNameQualification() for sourceFile = %p = %s \n",
               sourceFile, sourceFile->getFileName().c_str());
      }
      // DQ (5/10/2021): We only need to support the case of
      // file->get_unparseHeaderFiles() == true.
      bool traverseHeaderFiles = false;
      if (sourceFile->get_unparseHeaderFiles() == true) {
        traverseHeaderFiles = true;
      }

      // DQ (5/22/2021): We only want to compute the token frontier if the
      // unparser will consult preserved token mappings. Partial replay for
      // transformation tracking uses the same frontier even when full-file
      // `-rose:unparse_tokens` mode is disabled.
      const bool needs_token_unparse_frontier =
          sourceFile->get_unparse_tokens();
      // buildTokenStreamFrontier(sourceFile,traverseHeaderFiles);
      if (needs_token_unparse_frontier) {
        needsTokenBoundaries = true;
        buildTokenStreamFrontier(sourceFile, traverseHeaderFiles,
                                 tokenFrontiers);
      }
    } else {
    }
  }

  if (needsTokenBoundaries && !hasHeaderUnparseSource) {
    buildFirstAndLastStatementsForIncludeFiles(project, tokenFrontiers);
    buildFirstAndLastStatementsForScopes(project, tokenFrontiers);
  }

  // DQ (5/10/2021): We only need to support the case of
  // file->get_unparseHeaderFiles() == true. unparseIncludedFiles(project,
  // unparseFormatHelp, unparseDelegate); if
  // (sourceFile->get_unparseHeaderFiles() == true)
  if (hasHeaderUnparseSource) {
    // negara1 (06/27/2011)
    unparseIncludedFiles(project, unparseFormatHelp, unparseDelegate,
                         &preprocessingInfoRewrites, &nameQualifications,
                         &tokenFrontiers);
  }

#if ROSE_USING_OLD_PROJECT_FILE_LIST_SUPPORT
#error                                                                         \
    "This implementation of the support for the older interface has been refactored"
  for (int i = 0; i < project->numberOfFiles(); ++i) {
    SgFile &file = project->get_file(i);
    unparseFile(&file, unparseFormatHelp, unparseDelegate);
  }
#else
  if (SgProject::get_verbose() >= 1) {
    printf("In unparseProject(): Unparse the file list first, then the "
           "directory list \n");
  }

  // DQ (4/13/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("In unparseProject(): Unparse the file list first, then the "
           "directory list \n");
  }

  // DQ (1/23/2010): refactored the SgFileList
  unparseFileList(project->get_fileList_ptr(), unparseFormatHelp,
                  unparseDelegate, &preprocessingInfoRewrites,
                  &nameQualifications, nullptr, &tokenFrontiers);

  if (SgProject::get_verbose() >= 1) {
    printf("In unparseProject(): Unparse the directory list... \n");
  }

  // DQ (4/13/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("In unparseProject(): Unparse the directory list... \n");
  }

  for (int i = 0; i < project->numberOfDirectories(); ++i) {
    if (SgProject::get_verbose() > 0)
      printf("Unparse each directory (i = %d) \n", i);

    ASSERT_not_null(project->get_directoryList());
    SgDirectory *directory =
        project->get_directoryList()->get_listOfDirectories()[i];
    unparseDirectory(directory, unparseFormatHelp, unparseDelegate,
                     &preprocessingInfoRewrites, &nameQualifications,
                     &tokenFrontiers);
  }
#endif

  // DQ (4/13/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("Leaving unparseProject(): project = %p \n", project);
  }
}

namespace {
bool containsParentDirectoryComponent(const std::filesystem::path &path) {
  return std::find(path.begin(), path.end(), std::filesystem::path("..")) !=
         path.end();
}

void unparseDirectoryAt(
    SgDirectory *directory, const std::filesystem::path &outputDirectory,
    UnparseFormatHelp *unparseFormatHelp, UnparseDelegate *unparseDelegate,
    const UnparsePreprocessingInfoRewriteMap *preprocessingInfoRewrites,
    NameQualificationContext *nameQualifications,
    TokenUnparseFrontierContext *tokenFrontiers) {
  ASSERT_not_null(directory);
  if (outputDirectory.empty() ||
      containsParentDirectoryComponent(outputDirectory)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[directory-output]: directory=%s resolved "
            "to unsafe output path %s\n",
            directory->get_name().c_str(), outputDirectory.string().c_str());
    ROSE_ABORT();
  }

  std::error_code error;
  std::filesystem::create_directories(outputDirectory, error);
  if (error) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[directory-output]: output=%s error=%s\n",
            outputDirectory.string().c_str(), error.message().c_str());
    ROSE_ABORT();
  }

  const std::string outputDirectoryString = outputDirectory.string();
  unparseFileList(directory->get_fileList(), unparseFormatHelp, unparseDelegate,
                  preprocessingInfoRewrites, nameQualifications,
                  &outputDirectoryString, tokenFrontiers);

  if (directory->get_directoryList() == NULL) {
    if (directory->numberOfDirectories() != 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[directory-output]: directory=%s reports "
              "children without a directory list\n",
              directory->get_name().c_str());
      ROSE_ABORT();
    }
    return;
  }
  for (SgDirectory *subdirectory :
       directory->get_directoryList()->get_listOfDirectories()) {
    ASSERT_not_null(subdirectory);
    const std::filesystem::path childName(subdirectory->get_name());
    if (childName.empty() || childName.is_absolute() ||
        containsParentDirectoryComponent(childName)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[directory-output]: parent=%s has unsafe "
              "child name %s\n",
              outputDirectory.string().c_str(),
              subdirectory->get_name().c_str());
      ROSE_ABORT();
    }
    unparseDirectoryAt(
        subdirectory, (outputDirectory / childName).lexically_normal(),
        unparseFormatHelp, unparseDelegate, preprocessingInfoRewrites,
        nameQualifications, tokenFrontiers);
  }
}
} // namespace

void unparseDirectory(
    SgDirectory *directory, UnparseFormatHelp *unparseFormatHelp,
    UnparseDelegate *unparseDelegate,
    const UnparsePreprocessingInfoRewriteMap *preprocessingInfoRewrites,
    NameQualificationContext *nameQualifications,
    TokenUnparseFrontierContext *tokenFrontiers) {
  ASSERT_not_null(directory);
  const std::filesystem::path directoryName(directory->get_name());
  if (directoryName.empty() ||
      containsParentDirectoryComponent(directoryName)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[directory-output]: unsafe root directory "
            "name %s\n",
            directory->get_name().c_str());
    ROSE_ABORT();
  }
  const std::filesystem::path outputDirectory =
      directoryName.is_absolute()
          ? directoryName.lexically_normal()
          : (std::filesystem::current_path() / directoryName)
                .lexically_normal();
  unparseDirectoryAt(directory, outputDirectory, unparseFormatHelp,
                     unparseDelegate, preprocessingInfoRewrites,
                     nameQualifications, tokenFrontiers);
}

// DQ (1/19/2010): Added support for refactored handling directories of files.

/* Disable address sanitizer for this function */
// __attribute__((no_sanitize("address")))
void unparseFileList(
    SgFileList *fileList, UnparseFormatHelp *unparseFormatHelp,
    UnparseDelegate *unparseDelegate,
    const UnparsePreprocessingInfoRewriteMap *preprocessingInfoRewrites,
    NameQualificationContext *nameQualifications,
    const std::string *outputDirectoryOverride,
    TokenUnparseFrontierContext *tokenFrontiers) {
  ASSERT_not_null(fileList);

  // #if 0
  // DQ (4/9/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 3) {
    printf("In unparseFileList(): fileList->get_listOfFiles().size() = %zu \n",
           fileList->get_listOfFiles().size());
  }
  // #endif

  // DQ (9/17/2020): Testing using address sanitizer.
  ROSE_ASSERT(fileList != NULL);
  //~ ROSE_ASSERT(fileList->get_listOfFiles().size() >= 0);

  // for (size_t i=0; i < fileList->get_listOfFiles().size(); ++i)
  // size_t i;
  // for (i=0; i < fileList->get_listOfFiles().size(); ++i)
  // for (i=0; i < fileList->get_listOfFiles().size(); i++)
  auto &listOfFiles = fileList->get_listOfFiles();
  for (size_t i = 0; i < listOfFiles.size(); ++i) {
    SgFile *file = listOfFiles[i];
    // DQ (4/9/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf("\n**************************************************** \n");
      printf("**************************************************** \n");
      printf("**************************************************** \n");
      printf("**************************************************** \n");
      printf("In unparseFileList(): unparse file = %p filename = %s \n", file,
             file->getFileName().c_str());
    }

    // {
    ASSERT_not_null(file);

    // skip_unparse is a file-dispatch role, used both by an explicit command
    // line request and by semantic module files loaded for symbol resolution.
    // It is consumed only here; reaching the direct file emitter with the bit
    // set is a caller-contract violation.
    if (file->get_skip_unparse()) {
      continue;
    }

    if (SgProject::get_verbose() > 1) {
      printf("Unparsing file = %p = %s \n", file, file->class_name().c_str());
    }

    if (SgSourceFile *sourceFile = isSgSourceFile(file)) {
      if (sourceFile->get_frontendErrorCode() != 0) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[frontend-error]: file=%s code=%d "
                "refusing to unparse an invalid frontend result\n",
                file->getFileName().c_str(),
                sourceFile->get_frontendErrorCode());
        ROSE_ABORT();
      }
    }

    if (SgProject::get_unparseHeaderFilesDebug() >= 3) {
      printf("In unparseFileList(): calling unparseFile(): filename = %s \n",
             file->getFileName().c_str());
    }
    std::string outputFilename;
    const std::string *outputFilenameOverride = nullptr;
    if (outputDirectoryOverride != nullptr) {
      if (outputDirectoryOverride->empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[directory-output]: file=%s received "
                "an empty output directory\n",
                file->getFileName().c_str());
        ROSE_ABORT();
      }
      SgProject *project = SageInterface::getProject(file);
      if (project != nullptr &&
          project->get_unparse_in_same_directory_as_input_file()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[directory-output]: file=%s cannot use "
                "both SgDirectory output and same-directory output\n",
                file->getFileName().c_str());
        ROSE_ABORT();
      }
      std::string basename =
          file->get_unparse_output_filename().empty()
              ? "rose_" + file->get_sourceFileNameWithoutPath()
              : std::filesystem::path(file->get_unparse_output_filename())
                    .filename()
                    .string();
      if (file->get_Cuda_only()) {
        basename = StringUtility::stripFileSuffixFromFileName(basename) + ".cu";
      }
      if (basename.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[directory-output]: file=%s has no "
                "output basename\n",
                file->getFileName().c_str());
        ROSE_ABORT();
      }
      outputFilename =
          (std::filesystem::path(*outputDirectoryOverride) / basename).string();
      outputFilenameOverride = &outputFilename;
    }
    unparseFile(file, unparseFormatHelp, unparseDelegate, nullptr,
                preprocessingInfoRewrites, outputFilenameOverride,
                nameQualifications, tokenFrontiers);
    if (outputFilenameOverride != nullptr) {
      file->set_unparse_output_filename(outputFilename);
    }
    // }//file

    // DQ (4/9/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf("In unparseFileList(): base of loop \n");
      printf("**************************************************** \n");
      printf("**************************************************** \n");
      printf("**************************************************** \n");
      printf("**************************************************** \n");
    }

  } // for each
}
