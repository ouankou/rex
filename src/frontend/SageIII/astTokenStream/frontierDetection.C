#include "sage3basic.h"

#include "tokenStreamMapping.h"

using namespace std;
using namespace Rose;

FrontierDetectionForTokenStreamMapping_InheritedAttribute::
    FrontierDetectionForTokenStreamMapping_InheritedAttribute() {
  sourceFile = nullptr;
  processChildNodes = false;
  isFrontier = false;

  unparseUsingTokenStream = false;
  unparseFromTheAST = false;

  isInCurrentFile = true;
  node = nullptr;

  isPartOfTemplateInstantiation = false;
  isPartOfAuxiliaryDeclarationSubtree = false;
  isFunctionDeclarationStructuralWrapper = false;
  isSourceDeclaratorStructuralWrapper = false;
  isRangeForSemanticDeclarationWrapper = false;
  isPartOfRangeForSemanticDeclarationSubtree = false;
  isImplicitControlFlowStructuralWrapper = false;
  isForInitDeclarationGroupWrapper = false;
  isSourceLessForStructuralPayload = false;
}

FrontierDetectionForTokenStreamMapping_InheritedAttribute::
    FrontierDetectionForTokenStreamMapping_InheritedAttribute(
        SgSourceFile *file, SgNode *n) {
  sourceFile = file;
  ASSERT_not_null(sourceFile);
  processChildNodes = false;
  isFrontier = false;

  unparseUsingTokenStream = false;
  unparseFromTheAST = false;

  isInCurrentFile = true;
  node = n;

  isPartOfTemplateInstantiation = false;
  isPartOfAuxiliaryDeclarationSubtree = false;
  isFunctionDeclarationStructuralWrapper = false;
  isSourceDeclaratorStructuralWrapper = false;
  isRangeForSemanticDeclarationWrapper = false;
  isPartOfRangeForSemanticDeclarationSubtree = false;
  isImplicitControlFlowStructuralWrapper = false;
  isForInitDeclarationGroupWrapper = false;
  isSourceLessForStructuralPayload = false;
}

FrontierDetectionForTokenStreamMapping_InheritedAttribute::
    FrontierDetectionForTokenStreamMapping_InheritedAttribute(
        SgSourceFile *file) {
  sourceFile = file;
  ASSERT_not_null(sourceFile);

  processChildNodes = false;
  isFrontier = false;
  unparseUsingTokenStream = false;
  unparseFromTheAST = false;

  isInCurrentFile = true;
  node = file;
  isPartOfTemplateInstantiation = false;
  isPartOfAuxiliaryDeclarationSubtree = false;
  isFunctionDeclarationStructuralWrapper = false;
  isSourceDeclaratorStructuralWrapper = false;
  isRangeForSemanticDeclarationWrapper = false;
  isPartOfRangeForSemanticDeclarationSubtree = false;
  isImplicitControlFlowStructuralWrapper = false;
  isForInitDeclarationGroupWrapper = false;
  isSourceLessForStructuralPayload = false;
}

FrontierDetectionForTokenStreamMapping_InheritedAttribute::
    FrontierDetectionForTokenStreamMapping_InheritedAttribute(
        const FrontierDetectionForTokenStreamMapping_InheritedAttribute &X) {
  sourceFile = X.sourceFile;
  processChildNodes = X.processChildNodes;

  isFrontier = X.isFrontier;

  unparseUsingTokenStream = X.unparseUsingTokenStream;
  unparseFromTheAST = X.unparseFromTheAST;

  isInCurrentFile = X.isInCurrentFile;
  node = X.node;

  isPartOfTemplateInstantiation = X.isPartOfTemplateInstantiation;
  isPartOfAuxiliaryDeclarationSubtree = X.isPartOfAuxiliaryDeclarationSubtree;
  isFunctionDeclarationStructuralWrapper =
      X.isFunctionDeclarationStructuralWrapper;
  isSourceDeclaratorStructuralWrapper = X.isSourceDeclaratorStructuralWrapper;
  isRangeForSemanticDeclarationWrapper = X.isRangeForSemanticDeclarationWrapper;
  isPartOfRangeForSemanticDeclarationSubtree =
      X.isPartOfRangeForSemanticDeclarationSubtree;
  isImplicitControlFlowStructuralWrapper =
      X.isImplicitControlFlowStructuralWrapper;
  isForInitDeclarationGroupWrapper = X.isForInitDeclarationGroupWrapper;
  isSourceLessForStructuralPayload = X.isSourceLessForStructuralPayload;
}

FrontierDetectionForTokenStreamMapping_InheritedAttribute
FrontierDetectionForTokenStreamMapping_InheritedAttribute::operator=(
    const FrontierDetectionForTokenStreamMapping_InheritedAttribute &X) {
  sourceFile = X.sourceFile;
  processChildNodes = X.processChildNodes;

  isFrontier = X.isFrontier;

  unparseUsingTokenStream = X.unparseUsingTokenStream;
  unparseFromTheAST = X.unparseFromTheAST;

  isInCurrentFile = X.isInCurrentFile;
  node = X.node;

  isPartOfTemplateInstantiation = X.isPartOfTemplateInstantiation;
  isPartOfAuxiliaryDeclarationSubtree = X.isPartOfAuxiliaryDeclarationSubtree;
  isFunctionDeclarationStructuralWrapper =
      X.isFunctionDeclarationStructuralWrapper;
  isSourceDeclaratorStructuralWrapper = X.isSourceDeclaratorStructuralWrapper;
  isRangeForSemanticDeclarationWrapper = X.isRangeForSemanticDeclarationWrapper;
  isPartOfRangeForSemanticDeclarationSubtree =
      X.isPartOfRangeForSemanticDeclarationSubtree;
  isImplicitControlFlowStructuralWrapper =
      X.isImplicitControlFlowStructuralWrapper;
  isForInitDeclarationGroupWrapper = X.isForInitDeclarationGroupWrapper;
  isSourceLessForStructuralPayload = X.isSourceLessForStructuralPayload;

  return *this;
}

FrontierDetectionForTokenStreamMapping_SynthesizedAttribute::
    FrontierDetectionForTokenStreamMapping_SynthesizedAttribute() {
  node = nullptr;
  isFrontier = false;
  sourceFile = nullptr;

  unparseUsingTokenStream = false;
  unparseFromTheAST = false;
  containsNodesToBeUnparsedFromTheAST = false;

  containsNodesToBeUnparsedFromTheTokenStream = false;
}

FrontierDetectionForTokenStreamMapping_SynthesizedAttribute::
    FrontierDetectionForTokenStreamMapping_SynthesizedAttribute(
        SgNode *n, SgSourceFile *file) {
  ASSERT_not_null(n);
  ASSERT_not_null(file);

  node = isSgStatement(n);
  isFrontier = false;
  sourceFile = file;

  unparseUsingTokenStream = false;
  unparseFromTheAST = false;
  containsNodesToBeUnparsedFromTheAST = false;

  containsNodesToBeUnparsedFromTheTokenStream = false;
}

FrontierDetectionForTokenStreamMapping_SynthesizedAttribute::
    FrontierDetectionForTokenStreamMapping_SynthesizedAttribute(
        const FrontierDetectionForTokenStreamMapping_SynthesizedAttribute &X) {
  node = X.node;
  isFrontier = X.isFrontier;
  sourceFile = X.sourceFile;

  unparseUsingTokenStream = X.unparseUsingTokenStream;
  unparseFromTheAST = X.unparseFromTheAST;
  containsNodesToBeUnparsedFromTheAST = X.containsNodesToBeUnparsedFromTheAST;

  containsNodesToBeUnparsedFromTheTokenStream =
      X.containsNodesToBeUnparsedFromTheTokenStream;
}

FrontierDetectionForTokenStreamMapping_SynthesizedAttribute
FrontierDetectionForTokenStreamMapping_SynthesizedAttribute::operator=(
    const FrontierDetectionForTokenStreamMapping_SynthesizedAttribute &X) {
  node = X.node;
  isFrontier = X.isFrontier;
  sourceFile = X.sourceFile;

  unparseUsingTokenStream = X.unparseUsingTokenStream;
  unparseFromTheAST = X.unparseFromTheAST;
  containsNodesToBeUnparsedFromTheAST = X.containsNodesToBeUnparsedFromTheAST;

  containsNodesToBeUnparsedFromTheTokenStream =
      X.containsNodesToBeUnparsedFromTheTokenStream;

  return *this;
}

FrontierDetectionForTokenStreamMapping::FrontierDetectionForTokenStreamMapping(
    SgSourceFile *sourceFile,
    const TokenUnparseFrontierFileContext &frontierContext)
    : sourceFile(sourceFile), frontierContext(frontierContext) {
  ASSERT_not_null(sourceFile);
  if (!frontierContext.transformationAnalysisComplete) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-frontier]: file=%s frontier traversal "
            "started before transformation analysis completed\n",
            sourceFile->getFileName().c_str());
    ROSE_ABORT();
  }
}

FrontierDetectionForTokenStreamMapping::
    ~FrontierDetectionForTokenStreamMapping() {
  for (auto &entry : frontierNodes) {
    std::map<SgStatement *, FrontierNode *> *frontierMap = entry.second;
    if (frontierMap == nullptr) {
      continue;
    }
    delete frontierMap;
  }
  frontierNodes.clear();
}

[[noreturn]] static void rejectAuxiliaryFrontierOwnership(
    SgNode *node, SgAuxiliaryDeclarationList *container,
    SgScopeStatement *owner, SgDeclarationStatement *declaration,
    const char *reason) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[frontier-auxiliary-owner]: node=%p/%s "
          "container=%p owner=%p declaration=%p reason=%s\n",
          static_cast<void *>(node),
          node != nullptr ? node->class_name().c_str() : "<null>",
          static_cast<void *>(container), static_cast<void *>(owner),
          static_cast<void *>(declaration), reason);
  ROSE_ABORT();
}

static SgScopeStatement *requireExactAuxiliaryFrontierOwner(
    SgNode *node, SgAuxiliaryDeclarationList *container,
    SgDeclarationStatement *requiredDeclaration = nullptr) {
  if (node == nullptr || container == nullptr) {
    rejectAuxiliaryFrontierOwnership(node, container, nullptr,
                                     requiredDeclaration,
                                     "missing typed auxiliary container");
  }

  SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
  if (owner == nullptr || owner->get_auxiliary_declarations() != container) {
    rejectAuxiliaryFrontierOwnership(
        node, container, owner, requiredDeclaration,
        "container has no exact lexical-scope owner");
  }

  const SgNodePtrList ownerSuccessors =
      owner->get_traversalSuccessorContainer();
  if (std::count(ownerSuccessors.begin(), ownerSuccessors.end(), container) !=
      1) {
    rejectAuxiliaryFrontierOwnership(
        node, container, owner, requiredDeclaration,
        "scope does not traverse the container exactly once");
  }

  container->validate_semantic_non_output_role();
  const SgDeclarationStatementPtrList &declarations =
      container->get_declarations();
  if (declarations.empty()) {
    rejectAuxiliaryFrontierOwnership(node, container, owner,
                                     requiredDeclaration,
                                     "semantic container has no declarations");
  }

  const SgNodePtrList containerSuccessors =
      container->get_traversalSuccessorContainer();
  for (SgDeclarationStatement *declaration : declarations) {
    if (declaration == nullptr || declaration->get_parent() != container ||
        std::count(declarations.begin(), declarations.end(), declaration) !=
            1 ||
        std::count(containerSuccessors.begin(), containerSuccessors.end(),
                   declaration) != 1 ||
        declaration->get_scope() != owner) {
      rejectAuxiliaryFrontierOwnership(
          node, container, owner, declaration,
          "declaration has no exact semantic-container ownership edge");
    }
  }

  if (requiredDeclaration != nullptr &&
      (requiredDeclaration->get_parent() != container ||
       std::count(declarations.begin(), declarations.end(),
                  requiredDeclaration) != 1)) {
    rejectAuxiliaryFrontierOwnership(
        node, container, owner, requiredDeclaration,
        "traversed declaration is not owned exactly once by the container");
  }
  return owner;
}

[[noreturn]] static void rejectAuxiliaryScopeFrontierOwnership(
    SgNode *node, SgDeclarationScopeList *container, SgScopeStatement *owner,
    SgDeclarationScope *scope, const char *reason) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[frontier-auxiliary-scope-owner]: "
          "node=%p/%s container=%p owner=%p scope=%p reason=%s\n",
          static_cast<void *>(node),
          node != nullptr ? node->class_name().c_str() : "<null>",
          static_cast<void *>(container), static_cast<void *>(owner),
          static_cast<void *>(scope), reason);
  ROSE_ABORT();
}

template <class Reject>
static void requireExactSemanticDeclarationScopeSubtree(SgNode *root,
                                                        Reject reject) {
  std::unordered_set<SgNode *> visited;
  std::vector<SgNode *> pending{root};
  while (!pending.empty()) {
    SgNode *current = pending.back();
    pending.pop_back();
    if (current == nullptr || !visited.insert(current).second) {
      continue;
    }

    if (SgLocatedNode *located = isSgLocatedNode(current)) {
      Sg_File_Info *positions[] = {located->get_file_info(),
                                   located->get_startOfConstruct(),
                                   located->get_endOfConstruct()};
      for (Sg_File_Info *position : positions) {
        if (position == nullptr || position->get_parent() != current ||
            position->isShared() || !position->isCompilerGenerated() ||
            !position->isFrontendSpecific() || position->isTransformation() ||
            position->isSourcePositionUnavailableInFrontend() ||
            position->get_file_id() !=
                Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
            position->get_physical_file_id() !=
                Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
          reject(current,
                 "declaration-scope subtree contains a non-semantic located "
                 "node");
        }
      }
      const AttachedPreprocessingInfoType *preprocessing =
          located->get_attachedPreprocessingInfoPtr();
      if (preprocessing != nullptr && !preprocessing->empty()) {
        reject(current, "declaration-scope subtree owns preprocessing syntax");
      }
    }

    for (SgNode *child : current->get_traversalSuccessorContainer()) {
      if (child == nullptr || child->get_parent() != current) {
        reject(child, "declaration-scope subtree has an inexact child edge");
      }
      pending.push_back(child);
    }
  }
}

static bool nodeHasExactSemanticDeclarationScopeProvenance(SgNode *node) {
  SgLocatedNode *located = isSgLocatedNode(node);
  if (located == nullptr) {
    return false;
  }
  Sg_File_Info *positions[] = {located->get_file_info(),
                               located->get_startOfConstruct(),
                               located->get_endOfConstruct()};
  for (Sg_File_Info *position : positions) {
    if (position == nullptr || position->get_parent() != node ||
        position->isShared() || !position->isCompilerGenerated() ||
        !position->isFrontendSpecific() || position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        position->get_physical_file_id() !=
            Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
      return false;
    }
  }
  return true;
}

static void requireExactSemanticAuxiliaryScopeSubtree(
    SgNode *root, SgDeclarationScopeList *container, SgScopeStatement *owner) {
  requireExactSemanticDeclarationScopeSubtree(root, [&](SgNode *offender,
                                                        const char *reason) {
    rejectAuxiliaryScopeFrontierOwnership(offender, container, owner,
                                          isSgDeclarationScope(root), reason);
  });
}

[[noreturn]] static void rejectDirectNonrealScopeFrontierOwnership(
    SgNode *node, SgDeclarationStatement *owner, SgDeclarationScope *scope,
    const char *reason) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[frontier-direct-nonreal-scope-owner]: "
          "node=%p/%s owner=%p/%s scope=%p reason=%s\n",
          static_cast<void *>(node),
          node != nullptr ? node->class_name().c_str() : "<null>",
          static_cast<void *>(owner),
          owner != nullptr ? owner->class_name().c_str() : "<null>",
          static_cast<void *>(scope), reason);
  ROSE_ABORT();
}

static SgDeclarationStatement *
requireExactDirectNonrealScopeFrontierOwner(SgNode *node,
                                            SgDeclarationScope *scope) {
  SgDeclarationStatement *owner = isSgDeclarationStatement(
      scope != nullptr ? scope->get_parent() : nullptr);
  const SgNodePtrList ownerSuccessors =
      owner != nullptr ? owner->get_traversalSuccessorContainer()
                       : SgNodePtrList();
  if (node == nullptr || scope == nullptr || owner == nullptr ||
      node != scope || owner->get_nonreal_decl_scope() != scope ||
      owner->get_declarationScope() == scope ||
      SageBuilder::getDeclarationScopeOwner(scope) != owner ||
      std::count(ownerSuccessors.begin(), ownerSuccessors.end(), scope) != 1) {
    rejectDirectNonrealScopeFrontierOwnership(
        node, owner, scope,
        "scope has no exact direct nonreal-declaration ownership edge");
  }

  requireExactSemanticDeclarationScopeSubtree(scope, [&](SgNode *offender,
                                                         const char *reason) {
    rejectDirectNonrealScopeFrontierOwnership(offender, owner, scope, reason);
  });
  return owner;
}

static SgScopeStatement *requireExactAuxiliaryScopeFrontierOwner(
    SgNode *node, SgDeclarationScopeList *container,
    SgDeclarationScope *requiredScope = nullptr) {
  if (node == nullptr || container == nullptr) {
    rejectAuxiliaryScopeFrontierOwnership(
        node, container, nullptr, requiredScope,
        "missing typed auxiliary declaration-scope container");
  }

  SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
  if (owner == nullptr ||
      owner->get_auxiliary_declaration_scopes() != container) {
    rejectAuxiliaryScopeFrontierOwnership(
        node, container, owner, requiredScope,
        "container has no exact lexical-scope owner");
  }
  const SgNodePtrList ownerSuccessors =
      owner->get_traversalSuccessorContainer();
  if (std::count(ownerSuccessors.begin(), ownerSuccessors.end(), container) !=
      1) {
    rejectAuxiliaryScopeFrontierOwnership(
        node, container, owner, requiredScope,
        "scope does not traverse the container exactly once");
  }

  const SgDeclarationScopePtrList &scopes = container->get_scopes();
  const SgNodePtrList containerSuccessors =
      container->get_traversalSuccessorContainer();
  if (scopes.empty()) {
    rejectAuxiliaryScopeFrontierOwnership(
        node, container, owner, requiredScope,
        "semantic declaration-scope container has no scopes");
  }
  for (SgDeclarationScope *scope : scopes) {
    if (scope == nullptr || scope->get_parent() != container ||
        SageBuilder::getDeclarationScopeOwner(scope) != owner ||
        std::count(scopes.begin(), scopes.end(), scope) != 1 ||
        std::count(containerSuccessors.begin(), containerSuccessors.end(),
                   scope) != 1) {
      rejectAuxiliaryScopeFrontierOwnership(
          node, container, owner, scope,
          "scope has no exact semantic-container ownership edge");
    }
  }
  if (requiredScope != nullptr &&
      (requiredScope->get_parent() != container ||
       std::count(scopes.begin(), scopes.end(), requiredScope) != 1)) {
    rejectAuxiliaryScopeFrontierOwnership(
        node, container, owner, requiredScope,
        "traversed scope is not owned exactly once by the container");
  }
  requireExactSemanticAuxiliaryScopeSubtree(container, container, owner);
  return owner;
}

static bool requireAuxiliaryFrontierTraversalRole(
    SgNode *node,
    const FrontierDetectionForTokenStreamMapping_InheritedAttribute
        &inheritedAttribute) {
  if (node == nullptr) {
    rejectAuxiliaryFrontierOwnership(nullptr, nullptr, nullptr, nullptr,
                                     "traversal reached a null node");
  }

  if (inheritedAttribute.isPartOfAuxiliaryDeclarationSubtree) {
    SgNode *parent = node->get_parent();
    const SgNodePtrList parentSuccessors =
        parent != nullptr ? parent->get_traversalSuccessorContainer()
                          : SgNodePtrList();
    if (parent == nullptr || parent != inheritedAttribute.node ||
        std::count(parentSuccessors.begin(), parentSuccessors.end(), node) !=
            1) {
      rejectAuxiliaryFrontierOwnership(
          node, isSgAuxiliaryDeclarationList(parent),
          isSgScopeStatement(parent), isSgDeclarationStatement(node),
          "semantic subtree child has no exact traversal owner");
    }
    return true;
  }

  // A declarator scope can contain both real source-written tag surfaces and
  // semantic-only lookup declarations.  Classify only the latter as an
  // auxiliary subtree, using their complete provenance rather than their node
  // kind, so a real nested tag remains visible to transformation analysis.
  if (SgDeclarationScope *declaratorScope =
          isSgDeclarationScope(node->get_parent())) {
    SgDeclarationStatement *declaratorOwner =
        isSgDeclarationStatement(declaratorScope->get_parent());
    SgFunctionDeclaration *functionOwner =
        isSgFunctionDeclaration(declaratorOwner);
    const bool exactSourceDeclaratorScope =
        declaratorOwner != nullptr &&
        declaratorOwner->get_source_declarator_scope() == declaratorScope;
    const bool exactFunctionDeclaratorScope =
        functionOwner != nullptr &&
        functionOwner->get_function_declarator_scope() == declaratorScope;
    if ((exactSourceDeclaratorScope || exactFunctionDeclaratorScope) &&
        nodeHasExactSemanticDeclarationScopeProvenance(node)) {
      const SgNodePtrList scopeSuccessors =
          declaratorScope->get_traversalSuccessorContainer();
      if (std::count(scopeSuccessors.begin(), scopeSuccessors.end(), node) !=
          1) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[frontier-declarator-structural-owner]: "
                "node=%p/%s scope=%p owner=%p/%s has no exact semantic child "
                "edge\n",
                static_cast<void *>(node), node->class_name().c_str(),
                static_cast<void *>(declaratorScope),
                static_cast<void *>(declaratorOwner),
                declaratorOwner->class_name().c_str());
        ROSE_ABORT();
      }
      requireExactSemanticDeclarationScopeSubtree(
          node, [&](SgNode *offender, const char *reason) {
            fprintf(
                stderr,
                "REX_UNPARSE_INVARIANT[frontier-declarator-structural-owner]: "
                "node=%p/%s scope=%p owner=%p/%s offender=%p/%s reason=%s\n",
                static_cast<void *>(node), node->class_name().c_str(),
                static_cast<void *>(declaratorScope),
                static_cast<void *>(declaratorOwner),
                declaratorOwner->class_name().c_str(),
                static_cast<void *>(offender),
                offender != nullptr ? offender->class_name().c_str() : "<null>",
                reason);
            ROSE_ABORT();
          });
      return true;
    }
  }

  if (SgDeclarationScope *scope = isSgDeclarationScope(node)) {
    SgDeclarationStatement *owner =
        isSgDeclarationStatement(scope->get_parent());
    if (owner != nullptr && owner->get_nonreal_decl_scope() == scope) {
      (void)requireExactDirectNonrealScopeFrontierOwner(node, scope);
      return true;
    }
  }

  if (SgAuxiliaryDeclarationList *container =
          isSgAuxiliaryDeclarationList(node)) {
    requireExactAuxiliaryFrontierOwner(node, container);
    return true;
  }

  if (SgDeclarationScopeList *container = isSgDeclarationScopeList(node)) {
    requireExactAuxiliaryScopeFrontierOwner(node, container);
    return true;
  }

  if (SgDeclarationScopeList *container =
          isSgDeclarationScopeList(node->get_parent())) {
    SgDeclarationScope *scope = isSgDeclarationScope(node);
    if (scope == nullptr) {
      rejectAuxiliaryScopeFrontierOwnership(
          node, container, isSgScopeStatement(container->get_parent()), nullptr,
          "auxiliary declaration-scope container traverses a non-scope "
          "child");
    }
    requireExactAuxiliaryScopeFrontierOwner(node, container, scope);
    return true;
  }

  if (SgAuxiliaryDeclarationList *container =
          isSgAuxiliaryDeclarationList(node->get_parent())) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    if (declaration == nullptr) {
      rejectAuxiliaryFrontierOwnership(
          node, container, isSgScopeStatement(container->get_parent()), nullptr,
          "auxiliary container traverses a non-declaration child");
    }
    requireExactAuxiliaryFrontierOwner(node, container, declaration);
    return true;
  }

  return false;
}

[[noreturn]] static void rejectFunctionStructuralFrontierOwnership(
    SgNode *node, SgFunctionDeclaration *owner, const char *reason) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[frontier-function-structural-owner]: "
          "node=%p/%s owner=%p/%s reason=%s\n",
          static_cast<void *>(node),
          node != nullptr ? node->class_name().c_str() : "<null>",
          static_cast<void *>(owner),
          owner != nullptr ? owner->class_name().c_str() : "<null>", reason);
  ROSE_ABORT();
}

static SgFunctionDeclaration *
requireExactFunctionStructuralFrontierOwner(SgNode *node) {
  SgFunctionParameterList *parameters = isSgFunctionParameterList(node);
  SgFunctionParameterScope *parameterScope = isSgFunctionParameterScope(node);
  SgCtorInitializerList *initializers = isSgCtorInitializerList(node);
  SgDeclarationScope *declaratorScope = isSgDeclarationScope(node);
  const unsigned structuralRoleCount = (parameters != nullptr ? 1U : 0U) +
                                       (parameterScope != nullptr ? 1U : 0U) +
                                       (initializers != nullptr ? 1U : 0U) +
                                       (declaratorScope != nullptr ? 1U : 0U);
  if (structuralRoleCount != 1) {
    rejectFunctionStructuralFrontierOwnership(
        node, nullptr, "node is not one typed function structural wrapper");
  }

  SgFunctionDeclaration *owner =
      isSgFunctionDeclaration(node != nullptr ? node->get_parent() : nullptr);
  const bool exactParameterEdge = parameters != nullptr && owner != nullptr &&
                                  owner->get_parameterList() == parameters;
  const bool exactParameterScopeEdge =
      parameterScope != nullptr && owner != nullptr &&
      owner->get_functionParameterScope() == parameterScope;
  const unsigned exactFunctionScopeEdgeCount =
      declaratorScope != nullptr && owner != nullptr
          ? (owner->get_function_declarator_scope() == declaratorScope ? 1U
                                                                       : 0U)
          : 0U;
  SgMemberFunctionDeclaration *member =
      initializers != nullptr ? isSgMemberFunctionDeclaration(owner) : nullptr;
  const bool exactInitializerEdge =
      initializers != nullptr && member != nullptr &&
      member->get_CtorInitializerList() == initializers;
  if (!exactParameterEdge && !exactParameterScopeEdge &&
      !exactInitializerEdge && exactFunctionScopeEdgeCount != 1U) {
    rejectFunctionStructuralFrontierOwnership(
        node, owner, "wrapper is not owned by its exact function edge");
  }

  const SgNodePtrList ownerSuccessors =
      owner->get_traversalSuccessorContainer();
  if (std::count(ownerSuccessors.begin(), ownerSuccessors.end(), node) != 1) {
    rejectFunctionStructuralFrontierOwnership(
        node, owner, "function does not traverse the wrapper exactly once");
  }

  if (declaratorScope == nullptr && parameterScope == nullptr) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    if (declaration == nullptr ||
        declaration->get_firstNondefiningDeclaration() != declaration ||
        (initializers != nullptr &&
         declaration->get_definingDeclaration() != declaration) ||
        (parameters != nullptr &&
         declaration->get_definingDeclaration() != nullptr)) {
      rejectFunctionStructuralFrontierOwnership(
          node, owner,
          "wrapper has a malformed structural declaration identity");
    }
  }

  SgLocatedNode *located = isSgLocatedNode(node);
  Sg_File_Info *positions[] = {
      located != nullptr ? located->get_file_info() : nullptr,
      located != nullptr ? located->get_startOfConstruct() : nullptr,
      located != nullptr ? located->get_endOfConstruct() : nullptr};
  bool exactPhysicalOutput = true;
  bool exactSemantic = true;
  int sourcePhysicalFileId = Sg_File_Info::BAD_FILE_ID;
  std::optional<bool> sourceIsTransformation;
  for (Sg_File_Info *position : positions) {
    if (position == nullptr || position->get_parent() != node ||
        position->isShared()) {
      rejectFunctionStructuralFrontierOwnership(
          node, owner, "wrapper does not exclusively own complete file info");
    }
    const int physicalFileId = position->get_physical_file_id();
    if (physicalFileId < 0 || position->isCompilerGenerated() ||
        position->isFrontendSpecific() ||
        position->isSourcePositionUnavailableInFrontend() ||
        !position->isOutputInCodeGeneration()) {
      exactPhysicalOutput = false;
    } else if (sourcePhysicalFileId == Sg_File_Info::BAD_FILE_ID) {
      sourcePhysicalFileId = physicalFileId;
    } else if (sourcePhysicalFileId != physicalFileId) {
      exactPhysicalOutput = false;
    }
    if (!sourceIsTransformation.has_value()) {
      sourceIsTransformation = position->isTransformation();
    } else if (*sourceIsTransformation != position->isTransformation()) {
      exactPhysicalOutput = false;
    }

    if (position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        physicalFileId != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        !position->isCompilerGenerated() || !position->isFrontendSpecific() ||
        position->isTransformation() || !position->isOutputInCodeGeneration() ||
        position->isSourcePositionUnavailableInFrontend()) {
      exactSemantic = false;
    }
  }
  if (exactPhysicalOutput) {
    Sg_File_Info *ownerPositions[] = {owner->get_file_info(),
                                      owner->get_startOfConstruct(),
                                      owner->get_endOfConstruct()};
    for (Sg_File_Info *position : ownerPositions) {
      if (position == nullptr || position->get_parent() != owner ||
          position->isShared() || position->isCompilerGenerated() ||
          position->isFrontendSpecific() ||
          position->isSourcePositionUnavailableInFrontend() ||
          !position->isOutputInCodeGeneration() ||
          position->get_physical_file_id() != sourcePhysicalFileId ||
          !sourceIsTransformation.has_value() ||
          position->isTransformation() != *sourceIsTransformation) {
        exactPhysicalOutput = false;
        break;
      }
    }
  }
  if (exactPhysicalOutput == exactSemantic) {
    for (std::size_t index = 0; index < std::size(positions); ++index) {
      Sg_File_Info *position = positions[index];
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[frontier-function-structural-"
              "provenance]: node=%p/%s position[%zu]=%p parent=%p "
              "file/physical=%d/%d flags=[shared:%d,output:%d,compiler:%d,"
              "frontend:%d,transformation:%d,unavailable:%d]\n",
              static_cast<void *>(node), node->class_name().c_str(), index,
              static_cast<void *>(position),
              static_cast<void *>(position != nullptr ? position->get_parent()
                                                      : nullptr),
              position != nullptr ? position->get_file_id()
                                  : Sg_File_Info::BAD_FILE_ID,
              position != nullptr ? position->get_physical_file_id()
                                  : Sg_File_Info::BAD_FILE_ID,
              position != nullptr && position->isShared() ? 1 : 0,
              position != nullptr && position->isOutputInCodeGeneration() ? 1
                                                                          : 0,
              position != nullptr && position->isCompilerGenerated() ? 1 : 0,
              position != nullptr && position->isFrontendSpecific() ? 1 : 0,
              position != nullptr && position->isTransformation() ? 1 : 0,
              position != nullptr &&
                      position->isSourcePositionUnavailableInFrontend()
                  ? 1
                  : 0);
    }
    rejectFunctionStructuralFrontierOwnership(
        node, owner,
        "wrapper is neither one exact owner-matched physical output range nor "
        "one exact semantic role");
  }
  if ((declaratorScope != nullptr || parameterScope != nullptr) &&
      !exactSemantic) {
    rejectFunctionStructuralFrontierOwnership(
        node, owner,
        "function-owned semantic scope owns physical source provenance");
  }
  if (exactSemantic && initializers != nullptr) {
    for (SgInitializedName *entry : initializers->get_ctors()) {
      if (entry == nullptr || entry->get_parent() != initializers ||
          entry->get_ctor_initializer_source_order().has_value())
        rejectFunctionStructuralFrontierOwnership(
            node, owner,
            "semantic initializer list has a source or misowned entry");
      for (Sg_File_Info *position :
           {entry->get_file_info(), entry->get_startOfConstruct(),
            entry->get_endOfConstruct()}) {
        if (!SageInterface::hasExactSemanticFrontendSourcePosition(entry,
                                                                   position))
          rejectFunctionStructuralFrontierOwnership(
              node, owner, "semantic initializer entry has source provenance");
      }
    }
  }
  return owner;
}

static bool requireFunctionStructuralFrontierTraversalRole(
    SgNode *node,
    const FrontierDetectionForTokenStreamMapping_InheritedAttribute
        &inheritedAttribute) {
  if (inheritedAttribute.isFunctionDeclarationStructuralWrapper) {
    SgNode *wrapper = inheritedAttribute.node;
    SgFunctionDeclaration *owner =
        requireExactFunctionStructuralFrontierOwner(wrapper);
    const SgNodePtrList wrapperSuccessors =
        wrapper->get_traversalSuccessorContainer();
    if (node == nullptr || node->get_parent() != wrapper ||
        std::count(wrapperSuccessors.begin(), wrapperSuccessors.end(), node) !=
            1) {
      rejectFunctionStructuralFrontierOwnership(
          node, owner, "wrapper payload has no exact structural owner");
    }
    // Only the declaration wrapper is non-lexical. Its payload resumes normal
    // traversal so a real source child cannot be hidden by this boundary.
    return false;
  }

  SgDeclarationScope *functionOwnedScope = isSgDeclarationScope(node);
  SgFunctionDeclaration *functionScopeOwner =
      functionOwnedScope != nullptr
          ? isSgFunctionDeclaration(functionOwnedScope->get_parent())
          : nullptr;
  const bool functionOwnedDeclarationScope =
      functionScopeOwner != nullptr &&
      functionScopeOwner->get_function_declarator_scope() == functionOwnedScope;
  if (isSgFunctionParameterList(node) != nullptr ||
      isSgFunctionParameterScope(node) != nullptr ||
      isSgCtorInitializerList(node) != nullptr ||
      functionOwnedDeclarationScope) {
    requireExactFunctionStructuralFrontierOwner(node);
    return true;
  }
  return false;
}

[[noreturn]] static void rejectSourceDeclaratorStructuralFrontierOwnership(
    SgNode *node, SgDeclarationStatement *owner, const char *reason) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[frontier-source-declarator-owner]: "
          "node=%p/%s owner=%p/%s reason=%s\n",
          static_cast<void *>(node),
          node != nullptr ? node->class_name().c_str() : "<null>",
          static_cast<void *>(owner),
          owner != nullptr ? owner->class_name().c_str() : "<null>", reason);
  ROSE_ABORT();
}

static SgDeclarationStatement *
requireExactSourceDeclaratorStructuralFrontierOwner(SgNode *node) {
  if (auto *definition = isSgVariableDefinition(node)) {
    SgInitializedName *name = definition->get_vardefn();
    SgVariableDeclaration *owner =
        isSgVariableDeclaration(name != nullptr ? name->get_parent() : nullptr);
    if (name == nullptr || owner == nullptr ||
        definition->get_parent() != name ||
        name->get_variable_definition() != definition ||
        name->get_declptr() != owner ||
        name->get_scope() != owner->get_scope() ||
        std::count(owner->get_variables().begin(), owner->get_variables().end(),
                   name) != 1) {
      rejectSourceDeclaratorStructuralFrontierOwnership(
          node, owner, "definition has no exact initialized-name owner");
    }
    const SgNodePtrList nameSuccessors =
        name->get_traversalSuccessorContainer();
    const SgNodePtrList ownerSuccessors =
        owner->get_traversalSuccessorContainer();
    if (std::count(nameSuccessors.begin(), nameSuccessors.end(), definition) !=
            1 ||
        std::count(ownerSuccessors.begin(), ownerSuccessors.end(), name) != 1 ||
        !nodeHasExactSemanticDeclarationScopeProvenance(definition) ||
        !nodeHasExactSemanticDeclarationScopeProvenance(name)) {
      rejectSourceDeclaratorStructuralFrontierOwnership(
          node, owner,
          "definition has inconsistent traversal or semantic provenance");
    }
    const AttachedPreprocessingInfoType *preprocessing =
        definition->get_attachedPreprocessingInfoPtr();
    if (preprocessing != nullptr && !preprocessing->empty()) {
      rejectSourceDeclaratorStructuralFrontierOwnership(
          node, owner,
          "semantic definition owns independent preprocessing syntax");
    }
    return owner;
  }
  SgDeclarationScope *scope = isSgDeclarationScope(node);
  SgDeclarationStatement *owner = isSgDeclarationStatement(
      scope != nullptr ? scope->get_parent() : nullptr);
  if (scope == nullptr || owner == nullptr ||
      owner->get_source_declarator_scope() != scope) {
    rejectSourceDeclaratorStructuralFrontierOwnership(
        node, owner, "scope has no exact source-declarator owner edge");
  }

  const SgNodePtrList ownerSuccessors =
      owner->get_traversalSuccessorContainer();
  if (std::count(ownerSuccessors.begin(), ownerSuccessors.end(), scope) != 1) {
    rejectSourceDeclaratorStructuralFrontierOwnership(
        node, owner,
        "owner does not traverse the declarator scope exactly once");
  }
  if (!nodeHasExactSemanticDeclarationScopeProvenance(scope)) {
    rejectSourceDeclaratorStructuralFrontierOwnership(
        node, owner,
        "source-declarator wrapper does not have exact semantic provenance");
  }
  const AttachedPreprocessingInfoType *preprocessing =
      scope->get_attachedPreprocessingInfoPtr();
  if (preprocessing != nullptr && !preprocessing->empty()) {
    rejectSourceDeclaratorStructuralFrontierOwnership(
        node, owner, "source-declarator wrapper owns preprocessing syntax");
  }
  return owner;
}

static bool requireSourceDeclaratorStructuralFrontierTraversalRole(
    SgNode *node,
    const FrontierDetectionForTokenStreamMapping_InheritedAttribute
        &inheritedAttribute) {
  if (inheritedAttribute.isSourceDeclaratorStructuralWrapper) {
    SgNode *wrapper = inheritedAttribute.node;
    SgDeclarationStatement *owner =
        requireExactSourceDeclaratorStructuralFrontierOwner(wrapper);
    const SgNodePtrList successors = wrapper->get_traversalSuccessorContainer();
    if (node == nullptr || node->get_parent() != wrapper ||
        std::count(successors.begin(), successors.end(), node) != 1) {
      rejectSourceDeclaratorStructuralFrontierOwnership(
          node, owner, "wrapper payload has no exact source-declarator edge");
    }
    return false;
  }

  // An implicit declarator's semantic definition is structurally owned but
  // does not introduce a second lexical surface or hide its bitfield child.
  if (isSgVariableDefinition(node) != nullptr &&
      nodeHasExactSemanticDeclarationScopeProvenance(node)) {
    (void)requireExactSourceDeclaratorStructuralFrontierOwner(node);
    return true;
  }
  SgDeclarationScope *scope = isSgDeclarationScope(node);
  SgDeclarationStatement *owner = isSgDeclarationStatement(
      scope != nullptr ? scope->get_parent() : nullptr);
  if (scope != nullptr && owner != nullptr &&
      owner->get_source_declarator_scope() == scope) {
    (void)requireExactSourceDeclaratorStructuralFrontierOwner(node);
    return true;
  }
  return false;
}

[[noreturn]] static void rejectRangeForSemanticDeclarationFrontierOwnership(
    SgNode *node, SgRangeBasedForStatement *owner, const char *reason) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[frontier-range-for-semantic-owner]: "
          "node=%p/%s owner=%p reason=%s\n",
          static_cast<void *>(node),
          node != nullptr ? node->class_name().c_str() : "<null>",
          static_cast<void *>(owner), reason);
  ROSE_ABORT();
}

static SgRangeBasedForStatement *
requireExactRangeForSemanticDeclarationFrontierOwner(SgNode *node) {
  SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
  SgRangeBasedForStatement *owner = isSgRangeBasedForStatement(
      declaration != nullptr ? declaration->get_parent() : nullptr);
  const unsigned typedEdgeCount =
      owner != nullptr && owner->get_range_declaration() == declaration ? 1U
                                                                        : 0U;
  const unsigned completeTypedEdgeCount =
      typedEdgeCount +
      (owner != nullptr && owner->get_begin_declaration() == declaration ? 1U
                                                                         : 0U) +
      (owner != nullptr && owner->get_end_declaration() == declaration ? 1U
                                                                       : 0U);
  const SgNodePtrList successors =
      owner != nullptr ? owner->get_traversalSuccessorContainer()
                       : SgNodePtrList();
  if (declaration == nullptr || owner == nullptr ||
      completeTypedEdgeCount != 1U || declaration->get_scope() != owner ||
      std::count(successors.begin(), successors.end(), declaration) != 1 ||
      !nodeHasExactSemanticDeclarationScopeProvenance(declaration)) {
    rejectRangeForSemanticDeclarationFrontierOwnership(
        node, owner,
        "declaration has no single exact semantic range/begin/end owner edge");
  }
  const AttachedPreprocessingInfoType *preprocessing =
      declaration->get_attachedPreprocessingInfoPtr();
  if (preprocessing != nullptr && !preprocessing->empty()) {
    rejectRangeForSemanticDeclarationFrontierOwnership(
        node, owner, "semantic declaration shell owns preprocessing syntax");
  }
  return owner;
}

static bool requireRangeForSemanticDeclarationFrontierTraversalRole(
    SgNode *node,
    const FrontierDetectionForTokenStreamMapping_InheritedAttribute
        &inheritedAttribute) {
  if (inheritedAttribute.isRangeForSemanticDeclarationWrapper) {
    SgVariableDeclaration *wrapper =
        isSgVariableDeclaration(inheritedAttribute.node);
    SgRangeBasedForStatement *owner =
        requireExactRangeForSemanticDeclarationFrontierOwner(wrapper);
    const SgNodePtrList successors = wrapper->get_traversalSuccessorContainer();
    if (node == nullptr || node->get_parent() != wrapper ||
        std::count(successors.begin(), successors.end(), node) != 1) {
      rejectRangeForSemanticDeclarationFrontierOwnership(
          node, owner, "declaration payload has no exact structural edge");
    }
    return false;
  }

  SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
  SgRangeBasedForStatement *owner = isSgRangeBasedForStatement(
      declaration != nullptr ? declaration->get_parent() : nullptr);
  if (owner != nullptr && declaration != owner->get_iterator_declaration() &&
      (declaration == owner->get_range_declaration() ||
       declaration == owner->get_begin_declaration() ||
       declaration == owner->get_end_declaration())) {
    (void)requireExactRangeForSemanticDeclarationFrontierOwner(declaration);
    return true;
  }
  return false;
}

static SgVariableDeclaration *
requireExactRangeForSemanticDeclarationSubtreeOwner(SgNode *node) {
  if (node == nullptr) {
    rejectRangeForSemanticDeclarationFrontierOwnership(
        nullptr, nullptr, "semantic declaration subtree contains a null node");
  }

  SgNode *cursor = node;
  while (isSgVariableDeclaration(cursor) == nullptr) {
    SgNode *parent = cursor->get_parent();
    const SgNodePtrList parentSuccessors =
        parent != nullptr ? parent->get_traversalSuccessorContainer()
                          : SgNodePtrList();
    if (parent == nullptr || std::count(parentSuccessors.begin(),
                                        parentSuccessors.end(), cursor) != 1) {
      rejectRangeForSemanticDeclarationFrontierOwnership(
          node, nullptr,
          "semantic declaration payload has no exact structural parent edge");
    }
    cursor = parent;
  }

  SgVariableDeclaration *wrapper = isSgVariableDeclaration(cursor);
  (void)requireExactRangeForSemanticDeclarationFrontierOwner(wrapper);
  if (node == wrapper) {
    rejectRangeForSemanticDeclarationFrontierOwnership(
        node, isSgRangeBasedForStatement(wrapper->get_parent()),
        "semantic declaration wrapper was classified as its own payload");
  }
  return wrapper;
}

static bool requireRangeForSemanticDeclarationSubtreeTraversalRole(
    SgNode *node,
    const FrontierDetectionForTokenStreamMapping_InheritedAttribute
        &inheritedAttribute) {
  if (!inheritedAttribute.isRangeForSemanticDeclarationWrapper &&
      !inheritedAttribute.isPartOfRangeForSemanticDeclarationSubtree) {
    return false;
  }

  SgNode *parent = node != nullptr ? node->get_parent() : nullptr;
  const SgNodePtrList parentSuccessors =
      parent != nullptr ? parent->get_traversalSuccessorContainer()
                        : SgNodePtrList();
  if (node == nullptr || parent == nullptr ||
      parent != inheritedAttribute.node ||
      std::count(parentSuccessors.begin(), parentSuccessors.end(), node) != 1) {
    rejectRangeForSemanticDeclarationFrontierOwnership(
        node, nullptr,
        "semantic declaration subtree child has no exact traversal owner");
  }

  (void)requireExactRangeForSemanticDeclarationSubtreeOwner(node);
  return true;
}

[[noreturn]] static void
rejectImplicitControlFlowFrontierOwnership(SgNode *node, SgStatement *owner,
                                           const char *reason) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[frontier-implicit-control-flow-owner]: "
          "node=%p/%s owner=%p/%s reason=%s\n",
          static_cast<void *>(node),
          node != nullptr ? node->class_name().c_str() : "<null>",
          static_cast<void *>(owner),
          owner != nullptr ? owner->class_name().c_str() : "<null>", reason);
  ROSE_ABORT();
}

static SgStatement *requireExactImplicitControlFlowFrontierOwner(SgNode *node) {
  SgBasicBlock *block = isSgBasicBlock(node);
  SgStatement *owner =
      isSgStatement(block != nullptr ? block->get_parent() : nullptr);
  const bool exactTypedEdge =
      block != nullptr && owner != nullptr &&
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
      block != nullptr ? block->get_traversalSuccessorContainer()
                       : SgNodePtrList();
  const SgStatementPtrList *statements =
      block != nullptr ? &block->get_statements() : nullptr;
  SgStatement *payload = statements != nullptr && statements->size() == 1
                             ? statements->front()
                             : nullptr;
  SgDeclarationScopeList *declarationScopes =
      block != nullptr ? block->get_auxiliary_declaration_scopes() : nullptr;
  SgAuxiliaryDeclarationList *declarations =
      block != nullptr ? block->get_auxiliary_declarations() : nullptr;
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
  if (block == nullptr || owner == nullptr ||
      !block->get_is_implicit_control_flow_scope() || !exactTypedEdge ||
      std::count(ownerSuccessors.begin(), ownerSuccessors.end(), block) != 1 ||
      payload == nullptr || payload->get_parent() != block ||
      !exactBlockSuccessors || block->get_is_fortran_block_construct() ||
      !nodeHasExactSemanticDeclarationScopeProvenance(block)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[frontier-implicit-control-flow-detail]: "
            "block=%p owner=%p implicit=%d typed-edge=%d owner-edges=%zu "
            "statements=%zu payload=%p payload-parent=%p successors=%zu "
            "expected-successors=%zu declaration-scopes=%p "
            "declarations=%p exact-successors=%d fortran=%d semantic=%d\n",
            static_cast<void *>(block), static_cast<void *>(owner),
            block != nullptr
                ? static_cast<int>(block->get_is_implicit_control_flow_scope())
                : -1,
            static_cast<int>(exactTypedEdge),
            static_cast<size_t>(std::count(ownerSuccessors.begin(),
                                           ownerSuccessors.end(), block)),
            statements != nullptr ? statements->size() : 0,
            static_cast<void *>(payload),
            static_cast<void *>(payload != nullptr ? payload->get_parent()
                                                   : nullptr),
            blockSuccessors.size(), expectedSuccessorCount,
            static_cast<void *>(declarationScopes),
            static_cast<void *>(declarations),
            static_cast<int>(exactBlockSuccessors),
            block != nullptr
                ? static_cast<int>(block->get_is_fortran_block_construct())
                : -1,
            static_cast<int>(
                nodeHasExactSemanticDeclarationScopeProvenance(block)));
    rejectImplicitControlFlowFrontierOwnership(
        node, owner,
        "block has no exact typed owner, semantic wrapper role, and sole "
        "controlled statement edge");
  }
  return owner;
}

static bool requireImplicitControlFlowFrontierTraversalRole(
    SgNode *node,
    const FrontierDetectionForTokenStreamMapping_InheritedAttribute
        &inheritedAttribute) {
  if (inheritedAttribute.isImplicitControlFlowStructuralWrapper) {
    SgBasicBlock *block = isSgBasicBlock(inheritedAttribute.node);
    SgStatement *owner = requireExactImplicitControlFlowFrontierOwner(block);
    const SgNodePtrList successors = block->get_traversalSuccessorContainer();
    if (node == nullptr || node->get_parent() != block ||
        std::count(successors.begin(), successors.end(), node) != 1) {
      rejectImplicitControlFlowFrontierOwnership(
          node, owner, "wrapper payload has no exact implicit-scope edge");
    }
    return false;
  }

  SgBasicBlock *block = isSgBasicBlock(node);
  if (block != nullptr && block->get_is_implicit_control_flow_scope()) {
    (void)requireExactImplicitControlFlowFrontierOwner(node);
    return true;
  }
  return false;
}

[[noreturn]] static void rejectForInitDeclarationGroupFrontierOwnership(
    SgNode *node, SgForInitStatement *wrapper,
    SgDeclarationGroupStatement *group, const char *reason) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[frontier-for-init-declaration-owner]: "
          "node=%p/%s wrapper=%p group=%p reason=%s\n",
          static_cast<void *>(node),
          node != nullptr ? node->class_name().c_str() : "<null>",
          static_cast<void *>(wrapper), static_cast<void *>(group), reason);
  ROSE_ABORT();
}

static void requireExactForInitSourceSurface(SgLocatedNode *surface,
                                             SgForInitStatement *wrapper,
                                             SgDeclarationGroupStatement *group,
                                             const char *role) {
  if (surface == nullptr) {
    rejectForInitDeclarationGroupFrontierOwnership(
        surface, wrapper, group, "source surface is not a located node");
  }

  Sg_File_Info *positions[] = {surface->get_file_info(),
                               surface->get_startOfConstruct(),
                               surface->get_endOfConstruct()};
  int physicalFileId = Sg_File_Info::BAD_FILE_ID;
  for (Sg_File_Info *position : positions) {
    if (position == nullptr || position->get_parent() != surface ||
        position->isShared() || position->isCompilerGenerated() ||
        position->isFrontendSpecific() || position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        position->get_physical_file_id() < 0 ||
        (physicalFileId != Sg_File_Info::BAD_FILE_ID &&
         position->get_physical_file_id() != physicalFileId)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[frontier-for-init-declaration-owner]: "
              "role=%s surface=%p/%s position=%p owner=%p physical=%d "
              "shared/compiler/frontend/transformation/unavailable=%d/%d/%d/"
              "%d/%d lacks exact source provenance\n",
              role, static_cast<void *>(surface), surface->class_name().c_str(),
              static_cast<void *>(position),
              static_cast<void *>(position != nullptr ? position->get_parent()
                                                      : nullptr),
              position != nullptr ? position->get_physical_file_id()
                                  : Sg_File_Info::BAD_FILE_ID,
              position != nullptr && position->isShared() ? 1 : 0,
              position != nullptr && position->isCompilerGenerated() ? 1 : 0,
              position != nullptr && position->isFrontendSpecific() ? 1 : 0,
              position != nullptr && position->isTransformation() ? 1 : 0,
              position != nullptr &&
                      position->isSourcePositionUnavailableInFrontend()
                  ? 1
                  : 0);
      ROSE_ABORT();
    }
    physicalFileId = position->get_physical_file_id();
  }

  Sg_File_Info *start = surface->get_startOfConstruct();
  Sg_File_Info *end = surface->get_endOfConstruct();
  if (start->get_raw_line() <= 0 || end->get_raw_line() <= 0 ||
      start->get_raw_col() < 0 || end->get_raw_col() < 0 ||
      std::make_pair(end->get_raw_line(), end->get_raw_col()) <
          std::make_pair(start->get_raw_line(), start->get_raw_col())) {
    rejectForInitDeclarationGroupFrontierOwnership(
        surface, wrapper, group,
        "source surface has an invalid exact interval");
  }
}

static void requireExactForInitTransformationSurface(
    SgLocatedNode *surface, SgForStatement *owner, SgForInitStatement *wrapper,
    SgDeclarationGroupStatement *group, int physicalFileId, const char *role) {
  if (surface == nullptr || owner == nullptr || role == nullptr ||
      !owner->get_containsTransformation() || physicalFileId < 0) {
    rejectForInitDeclarationGroupFrontierOwnership(
        surface, wrapper, group,
        "generated payload has no exact structurally transformed loop owner");
  }

  Sg_File_Info *positions[] = {surface->get_file_info(),
                               surface->get_startOfConstruct(),
                               surface->get_endOfConstruct()};
  for (Sg_File_Info *position : positions) {
    if (position == nullptr || position->get_parent() != surface ||
        position->isShared() || position->isCompilerGenerated() ||
        position->isFrontendSpecific() || !position->isTransformation() ||
        !position->isOutputInCodeGeneration() ||
        position->get_file_id() != Sg_File_Info::TRANSFORMATION_FILE_ID ||
        position->get_physical_file_id() != physicalFileId) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[frontier-for-init-declaration-owner]: "
              "role=%s transformed surface=%p/%s position=%p owner=%p "
              "file/physical=%d/%d shared/compiler/frontend/transformation/"
              "unavailable/output=%d/%d/%d/%d/%d/%d lacks exact "
              "transformation "
              "provenance\n",
              role, static_cast<void *>(surface), surface->class_name().c_str(),
              static_cast<void *>(position),
              static_cast<void *>(position != nullptr ? position->get_parent()
                                                      : nullptr),
              position != nullptr ? position->get_file_id()
                                  : Sg_File_Info::BAD_FILE_ID,
              position != nullptr ? position->get_physical_file_id()
                                  : Sg_File_Info::BAD_FILE_ID,
              position != nullptr && position->isShared() ? 1 : 0,
              position != nullptr && position->isCompilerGenerated() ? 1 : 0,
              position != nullptr && position->isFrontendSpecific() ? 1 : 0,
              position != nullptr && position->isTransformation() ? 1 : 0,
              position != nullptr &&
                      position->isSourcePositionUnavailableInFrontend()
                  ? 1
                  : 0,
              position != nullptr && position->isOutputInCodeGeneration() ? 1
                                                                          : 0);
      ROSE_ABORT();
    }
  }
}

static SgDeclarationGroupStatement *
requireExactForInitDeclarationGroupFrontierOwner(SgForInitStatement *wrapper) {
  if (wrapper == nullptr) {
    return nullptr;
  }

  const SgStatementPtrList &initializers = wrapper->get_init_stmt();
  SgStatement *payload =
      initializers.size() == 1 ? initializers.front() : nullptr;
  SgDeclarationGroupStatement *group = nullptr;
  for (SgStatement *initializer : initializers) {
    if (SgDeclarationGroupStatement *candidate =
            isSgDeclarationGroupStatement(initializer)) {
      if (group != nullptr && group != candidate) {
        rejectForInitDeclarationGroupFrontierOwnership(
            wrapper, wrapper, candidate,
            "wrapper owns more than one declaration-group source surface");
      }
      group = candidate;
    }
  }
  SgForStatement *owner = isSgForStatement(wrapper->get_parent());
  const SgNodePtrList ownerSuccessors =
      owner != nullptr ? owner->get_traversalSuccessorContainer()
                       : SgNodePtrList();
  const SgNodePtrList wrapperSuccessors =
      wrapper->get_traversalSuccessorContainer();
  if (owner == nullptr || owner->get_for_init_stmt() != wrapper ||
      std::count(ownerSuccessors.begin(), ownerSuccessors.end(), wrapper) !=
          1 ||
      payload == nullptr || payload->get_parent() != wrapper ||
      wrapperSuccessors.size() != 1 || wrapperSuccessors.front() != payload ||
      std::count(wrapperSuccessors.begin(), wrapperSuccessors.end(), payload) !=
          1 ||
      (group != nullptr && (payload != group || group->get_scope() != owner))) {
    rejectForInitDeclarationGroupFrontierOwnership(
        wrapper, wrapper, group,
        "initializer payload has no single exact for-init structural owner");
  }

  const bool wrapperOwnsPhysicalSource =
      wrapper->get_file_info() != nullptr &&
      wrapper->get_file_info()->get_physical_file_id() >= 0;
  const bool payloadIsTransformation =
      payload->get_file_info() != nullptr &&
      payload->get_file_info()->isTransformation();
  if (wrapperOwnsPhysicalSource) {
    requireExactForInitSourceSurface(wrapper, wrapper, group, "wrapper");
    if (payloadIsTransformation) {
      requireExactForInitTransformationSurface(
          payload, owner, wrapper, group,
          wrapper->get_file_info()->get_physical_file_id(), "payload");
    } else {
      requireExactForInitSourceSurface(payload, wrapper, group, "payload");
    }
  } else {
    for (SgLocatedNode *surface : {static_cast<SgLocatedNode *>(wrapper),
                                   static_cast<SgLocatedNode *>(payload)}) {
      Sg_File_Info *positions[] = {surface->get_file_info(),
                                   surface->get_startOfConstruct(),
                                   surface->get_endOfConstruct()};
      for (Sg_File_Info *position : positions) {
        if (position == nullptr || position->get_parent() != surface ||
            position->isShared() || !position->isCompilerGenerated() ||
            !position->isFrontendSpecific() || position->isTransformation() ||
            position->isSourcePositionUnavailableInFrontend() ||
            position->get_file_id() !=
                Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
            position->get_physical_file_id() !=
                Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
          rejectForInitDeclarationGroupFrontierOwnership(
              surface, wrapper, group,
              "source-less initializer payload has contradictory semantic "
              "provenance");
        }
      }
    }
  }

  if (group != nullptr) {
    group->validate();
  }
  Sg_File_Info *wrapperStart = wrapper->get_startOfConstruct();
  Sg_File_Info *wrapperEnd = wrapper->get_endOfConstruct();
  Sg_File_Info *payloadStart = payload->get_startOfConstruct();
  Sg_File_Info *payloadEnd = payload->get_endOfConstruct();
  const bool inconsistentPhysicalOwner = wrapperStart->get_physical_file_id() !=
                                         payloadStart->get_physical_file_id();
  const bool inconsistentSourceInterval =
      !payloadIsTransformation &&
      (wrapperStart->get_raw_line() != payloadStart->get_raw_line() ||
       wrapperStart->get_raw_col() != payloadStart->get_raw_col() ||
       std::make_pair(payloadEnd->get_raw_line(), payloadEnd->get_raw_col()) <
           std::make_pair(wrapperEnd->get_raw_line(),
                          wrapperEnd->get_raw_col()));
  if (inconsistentPhysicalOwner || inconsistentSourceInterval) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[frontier-for-init-declaration-owner]: "
            "wrapper-range=%d:%d-%d:%d payload-range=%d:%d-%d:%d "
            "wrapper-physical=%d payload-physical=%d "
            "payload-transformation=%d\n",
            wrapperStart->get_raw_line(), wrapperStart->get_raw_col(),
            wrapperEnd->get_raw_line(), wrapperEnd->get_raw_col(),
            payloadStart->get_raw_line(), payloadStart->get_raw_col(),
            payloadEnd->get_raw_line(), payloadEnd->get_raw_col(),
            wrapperStart->get_physical_file_id(),
            payloadStart->get_physical_file_id(),
            static_cast<int>(payloadIsTransformation));
    rejectForInitDeclarationGroupFrontierOwnership(
        wrapper, wrapper, group,
        "wrapper and initializer payload disagree on their exact surface");
  }
  return group;
}

static bool requireForInitDeclarationGroupFrontierTraversalRole(
    SgNode *node,
    const FrontierDetectionForTokenStreamMapping_InheritedAttribute
        &inheritedAttribute) {
  if (inheritedAttribute.isForInitDeclarationGroupWrapper) {
    SgForInitStatement *wrapper = isSgForInitStatement(inheritedAttribute.node);
    SgDeclarationGroupStatement *group =
        requireExactForInitDeclarationGroupFrontierOwner(wrapper);
    const SgStatementPtrList &initializers = wrapper->get_init_stmt();
    SgStatement *payload =
        initializers.size() == 1 ? initializers.front() : nullptr;
    if (node != payload || node->get_parent() != wrapper) {
      rejectForInitDeclarationGroupFrontierOwnership(
          node, wrapper, group,
          "wrapper did not traverse its sole initializer payload");
    }
    // The role belongs only to the structural wrapper. Its exact payload
    // resumes normal statement traversal as the lexical token owner.
    return false;
  }

  if (SgForInitStatement *wrapper = isSgForInitStatement(node)) {
    (void)requireExactForInitDeclarationGroupFrontierOwner(wrapper);
    return true;
  }
  if (SgForInitStatement *wrapper = isSgForInitStatement(node->get_parent())) {
    SgDeclarationGroupStatement *group =
        requireExactForInitDeclarationGroupFrontierOwner(wrapper);
    rejectForInitDeclarationGroupFrontierOwnership(
        node, wrapper, group,
        "initializer payload traversal is missing its typed wrapper role");
  }
  return false;
}

static bool requireSourceLessStructuralPayloadTraversalRole(
    SgNode *node,
    const FrontierDetectionForTokenStreamMapping_InheritedAttribute
        &inheritedAttribute) {
  SgNullStatement *payload = isSgNullStatement(node);
  SgForInitStatement *wrapper = nullptr;
  SgDeclarationGroupStatement *group = nullptr;
  bool exactStructuralEdge = false;

  if (inheritedAttribute.isForInitDeclarationGroupWrapper) {
    wrapper = isSgForInitStatement(inheritedAttribute.node);
    group = requireExactForInitDeclarationGroupFrontierOwner(wrapper);
    const SgStatementPtrList &initializers = wrapper->get_init_stmt();
    SgStatement *initializer =
        initializers.size() == 1 ? initializers.front() : nullptr;
    if (node != initializer || node->get_parent() != wrapper) {
      rejectForInitDeclarationGroupFrontierOwnership(
          node, wrapper, group,
          "source-less payload role has no exact wrapper child");
    }
    Sg_File_Info *wrapperInfo = wrapper->get_file_info();
    if (wrapperInfo == nullptr || wrapperInfo->get_physical_file_id() >= 0) {
      return false;
    }
    exactStructuralEdge = group == nullptr && payload != nullptr;
  } else if (payload != nullptr) {
    SgForStatement *owner = isSgForStatement(payload->get_parent());
    if (owner == nullptr || owner->get_test() != payload) {
      return false;
    }
    const SgNodePtrList successors = owner->get_traversalSuccessorContainer();
    exactStructuralEdge =
        std::count(successors.begin(), successors.end(), payload) == 1;
  } else {
    return false;
  }

  // A source-less for-init wrapper may own an ordinary semantic declaration
  // payload.  That payload was validated by
  // requireExactForInitDeclarationGroupFrontierOwner and continues through the
  // normal semantic traversal; only the synthetic null test/initializer role
  // below is a transparent structural payload.
  if (!exactStructuralEdge) {
    return false;
  }

  Sg_File_Info *positions[] = {payload->get_file_info(),
                               payload->get_startOfConstruct(),
                               payload->get_endOfConstruct()};
  for (Sg_File_Info *position : positions) {
    if (!exactStructuralEdge || position == nullptr ||
        position->get_parent() != payload || position->isShared() ||
        !position->isCompilerGenerated() || !position->isFrontendSpecific() ||
        position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        !position->isOutputInCodeGeneration() ||
        position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        position->get_physical_file_id() !=
            Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
      rejectForInitDeclarationGroupFrontierOwnership(
          node, wrapper, group,
          "source-less structural payload has contradictory ownership or "
          "semantic provenance");
    }
  }
  return true;
}

static int requireStatementPhysicalFileId(SgStatement *statement,
                                          const char *context) {
  if (statement == nullptr || context == nullptr || context[0] == '\0') {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[frontier-physical-owner]: context=%s "
            "has no exact statement identity\n",
            context != nullptr ? context : "<null>");
    ROSE_ABORT();
  }

  Sg_File_Info *positions[] = {statement->get_file_info(),
                               statement->get_startOfConstruct(),
                               statement->get_endOfConstruct()};
  const int expectedPhysicalFileId = positions[0] != nullptr
                                         ? positions[0]->get_physical_file_id()
                                         : Sg_File_Info::BAD_FILE_ID;
  SgFunctionDefinition *enclosingDefinition = nullptr;
  for (SgNode *owner = statement->get_parent(); owner != nullptr;
       owner = owner->get_parent()) {
    if ((enclosingDefinition = isSgFunctionDefinition(owner)) != nullptr) {
      break;
    }
  }
  SgFunctionDeclaration *enclosingDeclaration =
      enclosingDefinition != nullptr ? enclosingDefinition->get_declaration()
                                     : nullptr;
  for (Sg_File_Info *position : positions) {
    if (position == nullptr || position->get_parent() != statement ||
        position->isShared() || position->get_physical_file_id() < 0 ||
        position->get_physical_file_id() != expectedPhysicalFileId) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[frontier-physical-owner]: context=%s "
              "statement=%p/%s name=%s parent=%p/%s position=%p "
              "position-parent=%p file-id=%d physical-file-id=%d expected=%d "
              "shared=%d output=%d compiler-generated=%d frontend-specific=%d "
              "transformation=%d enclosing-function=%p/%s name=%s "
              "function-parent=%p/%s function-scope=%p/%s "
              "function-source-owner=%d does not own one exact physical "
              "source identity\n",
              context, static_cast<void *>(statement),
              statement->class_name().c_str(),
              SageInterface::get_name(statement).c_str(),
              static_cast<void *>(statement->get_parent()),
              statement->get_parent() != nullptr
                  ? statement->get_parent()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(position),
              static_cast<void *>(position != nullptr ? position->get_parent()
                                                      : nullptr),
              position != nullptr ? position->get_file_id()
                                  : Sg_File_Info::BAD_FILE_ID,
              position != nullptr ? position->get_physical_file_id()
                                  : Sg_File_Info::BAD_FILE_ID,
              expectedPhysicalFileId,
              position != nullptr && position->isShared() ? 1 : 0,
              position != nullptr && position->isOutputInCodeGeneration() ? 1
                                                                          : 0,
              position != nullptr && position->isCompilerGenerated() ? 1 : 0,
              position != nullptr && position->isFrontendSpecific() ? 1 : 0,
              position != nullptr && position->isTransformation() ? 1 : 0,
              static_cast<void *>(enclosingDeclaration),
              enclosingDeclaration != nullptr
                  ? enclosingDeclaration->class_name().c_str()
                  : "<null>",
              enclosingDeclaration != nullptr
                  ? enclosingDeclaration->get_name().getString().c_str()
                  : "<null>",
              static_cast<void *>(enclosingDeclaration != nullptr
                                      ? enclosingDeclaration->get_parent()
                                      : nullptr),
              enclosingDeclaration != nullptr &&
                      enclosingDeclaration->get_parent() != nullptr
                  ? enclosingDeclaration->get_parent()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(enclosingDeclaration != nullptr
                                      ? enclosingDeclaration->get_scope()
                                      : nullptr),
              enclosingDeclaration != nullptr &&
                      enclosingDeclaration->get_scope() != nullptr
                  ? enclosingDeclaration->get_scope()->class_name().c_str()
                  : "<null>",
              enclosingDeclaration != nullptr
                  ? static_cast<int>(
                        enclosingDeclaration->get_frontend_source_ownership())
                  : -1);
      ROSE_ABORT();
    }
  }
  return expectedPhysicalFileId;
}

[[noreturn]] static void
rejectCatchSequenceFrontierOwnership(SgCatchStatementSeq *sequence,
                                     SgTryStmt *owner, const char *reason) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[frontier-catch-sequence-owner]: "
          "sequence=%p owner=%p reason=%s\n",
          static_cast<void *>(sequence), static_cast<void *>(owner), reason);
  ROSE_ABORT();
}

static SgTryStmt *
requireExactCatchSequenceFrontierOwner(SgCatchStatementSeq *sequence) {
  SgTryStmt *owner =
      isSgTryStmt(sequence != nullptr ? sequence->get_parent() : nullptr);
  if (sequence == nullptr || owner == nullptr ||
      owner->get_catch_statement_seq_root() != sequence) {
    rejectCatchSequenceFrontierOwnership(
        sequence, owner, "wrapper has no exact SgTryStmt owner edge");
  }
  const SgNodePtrList ownerSuccessors =
      owner->get_traversalSuccessorContainer();
  if (std::count(ownerSuccessors.begin(), ownerSuccessors.end(), sequence) !=
      1) {
    rejectCatchSequenceFrontierOwnership(
        sequence, owner, "owner does not traverse the wrapper exactly once");
  }

  Sg_File_Info *positions[] = {sequence->get_file_info(),
                               sequence->get_startOfConstruct(),
                               sequence->get_endOfConstruct()};
  Sg_File_Info *ownerPosition = owner->get_file_info();
  if (ownerPosition == nullptr || ownerPosition->get_parent() != owner ||
      ownerPosition->get_physical_file_id() < 0) {
    rejectCatchSequenceFrontierOwnership(
        sequence, owner, "owner has no exact physical source identity");
  }
  const int ownerPhysicalFileId = ownerPosition->get_physical_file_id();
  for (Sg_File_Info *position : positions) {
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
      rejectCatchSequenceFrontierOwnership(
          sequence, owner,
          "wrapper does not own one exact transparent structural role");
    }
  }

  const SgNodePtrList sequenceSuccessors =
      sequence->get_traversalSuccessorContainer();
  const SgStatementPtrList &handlers = sequence->get_catch_statement_seq();
  if (handlers.empty() || sequenceSuccessors.size() != handlers.size()) {
    rejectCatchSequenceFrontierOwnership(
        sequence, owner, "wrapper has no exact nonempty handler sequence");
  }
  for (std::size_t index = 0; index < handlers.size(); ++index) {
    SgCatchOptionStmt *handler = isSgCatchOptionStmt(handlers[index]);
    if (handler == nullptr || handler->get_parent() != sequence ||
        sequenceSuccessors[index] != handler) {
      rejectCatchSequenceFrontierOwnership(
          sequence, owner, "wrapper handler order or ownership is malformed");
    }
  }
  return owner;
}

static int requireSourceFilePhysicalFileId(SgSourceFile *sourceFile,
                                           const char *context) {
  Sg_File_Info *fileInfo =
      sourceFile != nullptr ? sourceFile->get_file_info() : nullptr;
  if (sourceFile == nullptr || context == nullptr || context[0] == '\0' ||
      fileInfo == nullptr || fileInfo->get_parent() != sourceFile ||
      fileInfo->isShared() || fileInfo->get_physical_file_id() < 0) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[frontier-source-file-owner]: context=%s "
            "source-file=%p name=%s file-info=%p position-parent=%p "
            "physical-file-id=%d shared=%d has no exact physical source "
            "identity\n",
            context != nullptr ? context : "<null>",
            static_cast<void *>(sourceFile),
            sourceFile != nullptr ? sourceFile->getFileName().c_str()
                                  : "<null>",
            static_cast<void *>(fileInfo),
            static_cast<void *>(fileInfo != nullptr ? fileInfo->get_parent()
                                                    : nullptr),
            fileInfo != nullptr ? fileInfo->get_physical_file_id()
                                : Sg_File_Info::BAD_FILE_ID,
            fileInfo != nullptr && fileInfo->isShared() ? 1 : 0);
    ROSE_ABORT();
  }
  return fileInfo->get_physical_file_id();
}

bool isFromSameFile(int physical_file_id_1, SgStatement *statement) {
  // This is a supporting function for the
  // isChildNodeFromSameFileAsCurrentNode() and isNodeFromCurrentFile()
  // functions.
  ASSERT_not_null(statement);

  bool return_value = false;
  if (physical_file_id_1 < 0) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[frontier-physical-owner]: source "
            "physical-file-id=%d is invalid\n",
            physical_file_id_1);
    ROSE_ABORT();
  }
  int physical_file_id_2 =
      requireStatementPhysicalFileId(statement, "same-file-comparison");

  return_value = (physical_file_id_1 == physical_file_id_2);

  return return_value;
}

bool FrontierDetectionForTokenStreamMapping::
    isChildNodeFromSameFileAsCurrentNode(SgNode *currentNode,
                                         SgStatement *child_statement) {
  // DQ (5/10/2021): Add test for if this child node is from the same physical
  // file.
  const int source_file_id =
      requireSourceFilePhysicalFileId(sourceFile, "frontier-source-file");
  const int child_file_id = requireStatementPhysicalFileId(
      child_statement, "same-file-child-statement");

  if (SgSourceFile *currentSourceFile = isSgSourceFile(currentNode)) {
    if (currentSourceFile != sourceFile) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[frontier-source-file-owner]: current "
              "source-file=%p name=%s does not match traversal source-file=%p "
              "name=%s\n",
              static_cast<void *>(currentSourceFile),
              currentSourceFile->getFileName().c_str(),
              static_cast<void *>(sourceFile),
              sourceFile->getFileName().c_str());
      ROSE_ABORT();
    }
    return source_file_id == child_file_id;
  }

  SgStatement *statement = isSgStatement(currentNode);
  if (statement == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[frontier-current-owner]: current-node=%p/%s "
            "is neither an exact statement nor source-file frontier owner\n",
            static_cast<void *>(currentNode),
            currentNode != nullptr ? currentNode->class_name().c_str()
                                   : "<null>");
    ROSE_ABORT();
  }

  if (isSgScopeStatement(statement) != nullptr &&
      child_file_id == source_file_id) {
    // Header files share lexical containers with the translation unit. The
    // container's own physical location therefore cannot hide a child owned
    // by the exact SgSourceFile whose frontier is being constructed.
    return true;
  }

  const int statement_file_id =
      requireStatementPhysicalFileId(statement, "same-file-parent-statement");
  return statement_file_id == child_file_id;
}

bool FrontierDetectionForTokenStreamMapping_InheritedAttribute::
    isNodeFromCurrentFile(SgStatement *statement) {
  ASSERT_not_null(statement);
  ASSERT_not_null(sourceFile);

  bool return_value = false;
  int sourceFile_physical_file_id =
      sourceFile->get_file_info()->get_physical_file_id();

  return_value = isFromSameFile(sourceFile_physical_file_id, statement);

  return return_value;
}

FrontierDetectionForTokenStreamMapping_InheritedAttribute
FrontierDetectionForTokenStreamMapping::evaluateInheritedAttribute(
    SgNode *n, FrontierDetectionForTokenStreamMapping_InheritedAttribute
                   inheritedAttribute) {
  // By default, all inherited attributes are marked as:
  //    isFrontier                                  = false
  //    unparseUsingTokenStream                     = true
  //    unparseFromTheAST                           = false
  //    containsNodesToBeUnparsedFromTheAST         = false (this is always
  //    false and never set; can be removed)
  // When a node that is marked as isTransformation() == true is seen, then we
  // set isFrontier = true

  SgStatement *statement = isSgStatement(n);

  bool isTemplateInstantiationNode = false;
  if (statement != nullptr) {
    isTemplateInstantiationNode = SageInterface::isTemplateInstantiationNode(n);
  }

  ASSERT_not_null(inheritedAttribute.sourceFile);

  FrontierDetectionForTokenStreamMapping_InheritedAttribute returnAttribute(
      inheritedAttribute.sourceFile, n);

  returnAttribute.isPartOfAuxiliaryDeclarationSubtree =
      requireAuxiliaryFrontierTraversalRole(n, inheritedAttribute);
  returnAttribute.isFunctionDeclarationStructuralWrapper =
      requireFunctionStructuralFrontierTraversalRole(n, inheritedAttribute);
  returnAttribute.isRangeForSemanticDeclarationWrapper =
      requireRangeForSemanticDeclarationFrontierTraversalRole(
          n, inheritedAttribute);
  returnAttribute.isPartOfRangeForSemanticDeclarationSubtree =
      requireRangeForSemanticDeclarationSubtreeTraversalRole(
          n, inheritedAttribute);
  returnAttribute.isSourceDeclaratorStructuralWrapper =
      requireSourceDeclaratorStructuralFrontierTraversalRole(
          n, inheritedAttribute) &&
      !returnAttribute.isPartOfRangeForSemanticDeclarationSubtree;
  returnAttribute.isImplicitControlFlowStructuralWrapper =
      requireImplicitControlFlowFrontierTraversalRole(n, inheritedAttribute);
  returnAttribute.isForInitDeclarationGroupWrapper =
      requireForInitDeclarationGroupFrontierTraversalRole(n,
                                                          inheritedAttribute);
  returnAttribute.isSourceLessForStructuralPayload =
      requireSourceLessStructuralPayloadTraversalRole(n, inheritedAttribute);

  if (isTemplateInstantiationNode == true ||
      inheritedAttribute.isPartOfTemplateInstantiation == true) {
    returnAttribute.isPartOfTemplateInstantiation = true;
    ASSERT_require(returnAttribute.isPartOfTemplateInstantiation == true);
  }

  if (statement != nullptr && isTemplateInstantiationNode == false &&
      inheritedAttribute.isPartOfTemplateInstantiation == false &&
      returnAttribute.isPartOfAuxiliaryDeclarationSubtree == false &&
      returnAttribute.isFunctionDeclarationStructuralWrapper == false &&
      returnAttribute.isSourceDeclaratorStructuralWrapper == false &&
      returnAttribute.isRangeForSemanticDeclarationWrapper == false &&
      returnAttribute.isPartOfRangeForSemanticDeclarationSubtree == false &&
      returnAttribute.isImplicitControlFlowStructuralWrapper == false &&
      returnAttribute.isForInitDeclarationGroupWrapper == false &&
      returnAttribute.isSourceLessForStructuralPayload == false &&
      isSgCatchStatementSeq(statement) == nullptr) {

    requireStatementPhysicalFileId(statement, "frontier-inherited-statement");

    bool nodeIsFromCurrentFile =
        inheritedAttribute.isNodeFromCurrentFile(statement);

    if (frontierContext.statementRequiresAstUnparse(statement)) {
      if (inheritedAttribute.unparseFromTheAST == false) {
        returnAttribute.isFrontier = true;
      }

      returnAttribute.unparseFromTheAST = true;
      returnAttribute.unparseUsingTokenStream = false;
    } else {
      returnAttribute.unparseUsingTokenStream = true;
      ASSERT_require(returnAttribute.unparseFromTheAST == false);
    }

    ASSERT_not_null(returnAttribute.sourceFile);

    if (nodeIsFromCurrentFile == false) {
      returnAttribute.isInCurrentFile = false;
    }

    // DQ (11/30/2013): Allow us to ignore class defintions in typedefs.
    // Mark the whole subtree as being unparsed from the AST so that synthizized
    // attributes can be more esily computed.
    if (inheritedAttribute.unparseFromTheAST == true) {
      returnAttribute.unparseFromTheAST = true;
      returnAttribute.unparseUsingTokenStream = false;
    }

  } else if (SgCatchStatementSeq *sequence =
                 inheritedAttribute.isPartOfTemplateInstantiation
                     ? nullptr
                     : isSgCatchStatementSeq(statement)) {
    SgTryStmt *owner = requireExactCatchSequenceFrontierOwner(sequence);
    const bool neutralAuxiliarySubtree =
        returnAttribute.isPartOfAuxiliaryDeclarationSubtree &&
        !inheritedAttribute.unparseUsingTokenStream &&
        !inheritedAttribute.unparseFromTheAST;
    const bool neutralNoncurrentHeader =
        !inheritedAttribute.unparseUsingTokenStream &&
        !inheritedAttribute.unparseFromTheAST &&
        !returnAttribute.isInCurrentFile &&
        !inheritedAttribute.sourceFile->get_unparseHeaderFiles();
    if (frontierContext.statementRequiresAstUnparse(sequence) ||
        (inheritedAttribute.unparseUsingTokenStream ==
             inheritedAttribute.unparseFromTheAST &&
         !neutralAuxiliarySubtree && !neutralNoncurrentHeader)) {
      rejectCatchSequenceFrontierOwnership(
          sequence, owner,
          "structural wrapper acquired an independent or ambiguous emission "
          "role");
    }
    returnAttribute.isFrontier = false;
    returnAttribute.isInCurrentFile = inheritedAttribute.isInCurrentFile;
    returnAttribute.unparseUsingTokenStream =
        inheritedAttribute.unparseUsingTokenStream;
    returnAttribute.unparseFromTheAST = inheritedAttribute.unparseFromTheAST;
  } else if (returnAttribute.isPartOfAuxiliaryDeclarationSubtree) {
    // SgAuxiliaryDeclarationList owns semantic declarations so analyses can
    // traverse them, but the subtree is not a lexical source-emission surface.
    // Preserve a neutral inherited state and let synthesis erase the subtree
    // from its lexical owner's frontier children.
    returnAttribute.isFrontier = false;
    returnAttribute.unparseUsingTokenStream = false;
    returnAttribute.unparseFromTheAST = false;
  } else if (returnAttribute.isFunctionDeclarationStructuralWrapper) {
    SgFunctionDeclaration *owner =
        requireExactFunctionStructuralFrontierOwner(n);
    if (statement == nullptr ||
        frontierContext.isStatementMarkedForAstUnparse(statement)) {
      rejectFunctionStructuralFrontierOwnership(
          n, owner,
          statement == nullptr
              ? "typed function wrapper is not an SgStatement"
              : "structural wrapper was explicitly selected as an independent "
                "AST-emission owner");
    }

    // The enclosing function owns the only lexical source boundary. Preserve
    // its already-decided emission mode for payload traversal, but never make
    // the structural list an independent frontier node.
    returnAttribute.isFrontier = false;
    returnAttribute.isInCurrentFile = inheritedAttribute.isInCurrentFile;
    returnAttribute.unparseUsingTokenStream =
        inheritedAttribute.unparseUsingTokenStream;
    returnAttribute.unparseFromTheAST = inheritedAttribute.unparseFromTheAST;
    if (returnAttribute.unparseUsingTokenStream ==
        returnAttribute.unparseFromTheAST) {
      rejectFunctionStructuralFrontierOwnership(
          n, owner,
          "enclosing function supplied no unique lexical emission mode");
    }
  } else if (returnAttribute.isSourceDeclaratorStructuralWrapper) {
    SgDeclarationStatement *owner =
        requireExactSourceDeclaratorStructuralFrontierOwner(n);
    if (statement == nullptr ||
        frontierContext.statementRequiresAstUnparse(statement)) {
      rejectSourceDeclaratorStructuralFrontierOwnership(
          n, owner,
          statement == nullptr
              ? "typed source-declarator wrapper is not an SgStatement"
              : "structural wrapper was selected as an AST-emission owner");
    }

    returnAttribute.isFrontier = false;
    returnAttribute.isInCurrentFile = inheritedAttribute.isInCurrentFile;
    returnAttribute.unparseUsingTokenStream =
        inheritedAttribute.unparseUsingTokenStream;
    returnAttribute.unparseFromTheAST = inheritedAttribute.unparseFromTheAST;
    if (returnAttribute.unparseUsingTokenStream ==
        returnAttribute.unparseFromTheAST) {
      rejectSourceDeclaratorStructuralFrontierOwnership(
          n, owner,
          "enclosing declaration supplied no unique lexical emission mode");
    }
  } else if (returnAttribute.isPartOfRangeForSemanticDeclarationSubtree) {
    SgVariableDeclaration *wrapper =
        requireExactRangeForSemanticDeclarationSubtreeOwner(n);
    SgRangeBasedForStatement *owner =
        requireExactRangeForSemanticDeclarationFrontierOwner(wrapper);
    if (returnAttribute.isRangeForSemanticDeclarationWrapper ||
        returnAttribute.isFrontier ||
        (statement != nullptr &&
         frontierContext.statementRequiresAstUnparse(statement))) {
      rejectRangeForSemanticDeclarationFrontierOwnership(
          n, owner,
          "semantic declaration payload acquired an independent lexical "
          "emission role");
    }

    const bool directWrapperPayload =
        inheritedAttribute.isRangeForSemanticDeclarationWrapper;
    const bool nestedSemanticPayload =
        inheritedAttribute.isPartOfRangeForSemanticDeclarationSubtree;
    const bool exactWrapperMode = inheritedAttribute.unparseUsingTokenStream !=
                                  inheritedAttribute.unparseFromTheAST;
    const bool neutralNestedMode =
        !inheritedAttribute.isFrontier &&
        !inheritedAttribute.unparseUsingTokenStream &&
        !inheritedAttribute.unparseFromTheAST;
    if ((directWrapperPayload == nestedSemanticPayload) ||
        (directWrapperPayload &&
         (inheritedAttribute.isFrontier || !exactWrapperMode)) ||
        (nestedSemanticPayload && !neutralNestedMode)) {
      rejectRangeForSemanticDeclarationFrontierOwnership(
          n, owner,
          "semantic declaration payload did not inherit its exact typed "
          "wrapper or nested role");
    }

    returnAttribute.isFrontier = false;
    returnAttribute.isInCurrentFile = inheritedAttribute.isInCurrentFile;
    returnAttribute.unparseUsingTokenStream = false;
    returnAttribute.unparseFromTheAST = false;
  } else if (returnAttribute.isRangeForSemanticDeclarationWrapper) {
    SgVariableDeclaration *wrapper = isSgVariableDeclaration(n);
    SgRangeBasedForStatement *owner =
        requireExactRangeForSemanticDeclarationFrontierOwner(wrapper);
    if (statement != wrapper ||
        frontierContext.statementRequiresAstUnparse(wrapper)) {
      rejectRangeForSemanticDeclarationFrontierOwnership(
          n, owner,
          "semantic declaration shell acquired an independent AST-emission "
          "role");
    }

    returnAttribute.isFrontier = false;
    returnAttribute.isInCurrentFile = inheritedAttribute.isInCurrentFile;
    returnAttribute.unparseUsingTokenStream =
        inheritedAttribute.unparseUsingTokenStream;
    returnAttribute.unparseFromTheAST = inheritedAttribute.unparseFromTheAST;
    if (returnAttribute.unparseUsingTokenStream ==
        returnAttribute.unparseFromTheAST) {
      rejectRangeForSemanticDeclarationFrontierOwnership(
          n, owner,
          "enclosing range-for supplied no unique lexical emission mode");
    }
  } else if (returnAttribute.isImplicitControlFlowStructuralWrapper) {
    SgStatement *owner = requireExactImplicitControlFlowFrontierOwner(n);
    if (statement == nullptr ||
        frontierContext.statementRequiresAstUnparse(statement)) {
      rejectImplicitControlFlowFrontierOwnership(
          n, owner,
          statement == nullptr
              ? "typed implicit control-flow wrapper is not an SgStatement"
              : "structural wrapper was selected as an AST-emission owner");
    }

    returnAttribute.isFrontier = false;
    returnAttribute.isInCurrentFile = inheritedAttribute.isInCurrentFile;
    returnAttribute.unparseUsingTokenStream =
        inheritedAttribute.unparseUsingTokenStream;
    returnAttribute.unparseFromTheAST = inheritedAttribute.unparseFromTheAST;
    if (returnAttribute.unparseUsingTokenStream ==
        returnAttribute.unparseFromTheAST) {
      rejectImplicitControlFlowFrontierOwnership(
          n, owner,
          "enclosing control statement supplied no unique lexical emission "
          "mode");
    }
  } else if (returnAttribute.isForInitDeclarationGroupWrapper) {
    SgForInitStatement *wrapper = isSgForInitStatement(n);
    SgDeclarationGroupStatement *group =
        requireExactForInitDeclarationGroupFrontierOwner(wrapper);
    if (statement != wrapper ||
        frontierContext.statementRequiresAstUnparse(wrapper)) {
      rejectForInitDeclarationGroupFrontierOwnership(
          n, wrapper, group,
          "structural wrapper acquired an independent AST-emission role");
    }

    returnAttribute.isFrontier = false;
    returnAttribute.isInCurrentFile = inheritedAttribute.isInCurrentFile;
    returnAttribute.unparseUsingTokenStream =
        inheritedAttribute.unparseUsingTokenStream;
    returnAttribute.unparseFromTheAST = inheritedAttribute.unparseFromTheAST;
    if (returnAttribute.unparseUsingTokenStream ==
        returnAttribute.unparseFromTheAST) {
      rejectForInitDeclarationGroupFrontierOwnership(
          n, wrapper, group,
          "enclosing for statement supplied no unique lexical emission mode");
    }
  } else if (returnAttribute.isSourceLessForStructuralPayload) {
    SgNullStatement *payload = isSgNullStatement(n);
    SgForInitStatement *wrapper =
        isSgForInitStatement(n != nullptr ? n->get_parent() : nullptr);
    SgDeclarationGroupStatement *group =
        wrapper != nullptr
            ? requireExactForInitDeclarationGroupFrontierOwner(wrapper)
            : nullptr;
    SgForStatement *forOwner =
        isSgForStatement(n != nullptr ? n->get_parent() : nullptr);
    const bool exactConditionEdge =
        forOwner != nullptr && forOwner->get_test() == payload;
    if (payload == nullptr || (wrapper == nullptr && !exactConditionEdge) ||
        group != nullptr ||
        frontierContext.statementRequiresAstUnparse(payload)) {
      rejectForInitDeclarationGroupFrontierOwnership(
          n, wrapper, group,
          "semantic null for-field acquired a lexical emission role");
    }
    returnAttribute.isFrontier = false;
    returnAttribute.isInCurrentFile = inheritedAttribute.isInCurrentFile;
    returnAttribute.unparseUsingTokenStream = false;
    returnAttribute.unparseFromTheAST = false;
  } else {

    if (inheritedAttribute.unparseFromTheAST == true) {
      returnAttribute.unparseFromTheAST = true;
      returnAttribute.unparseUsingTokenStream = false;
    } else {
      // Default setting for all non-SgStatements.
      returnAttribute.unparseUsingTokenStream = true;
    }
  }

  ASSERT_require((returnAttribute.isPartOfAuxiliaryDeclarationSubtree ||
                  returnAttribute.isPartOfRangeForSemanticDeclarationSubtree ||
                  returnAttribute.isSourceLessForStructuralPayload)
                     ? (!returnAttribute.isFrontier &&
                        !returnAttribute.unparseUsingTokenStream &&
                        !returnAttribute.unparseFromTheAST)
                     : ((returnAttribute.unparseUsingTokenStream == true &&
                         returnAttribute.unparseFromTheAST == false) ||
                        (returnAttribute.unparseUsingTokenStream == false &&
                         returnAttribute.unparseFromTheAST == true)));

  if (returnAttribute.isFrontier == true) {
    ASSERT_require(returnAttribute.unparseUsingTokenStream == false &&
                   returnAttribute.unparseFromTheAST == true);
  }

  ASSERT_not_null(returnAttribute.sourceFile);

  return returnAttribute;
}

FrontierDetectionForTokenStreamMapping_SynthesizedAttribute
FrontierDetectionForTokenStreamMapping::evaluateSynthesizedAttribute(
    SgNode *n,
    FrontierDetectionForTokenStreamMapping_InheritedAttribute
        inheritedAttribute,
    SubTreeSynthesizedAttributes synthesizedAttributeList) {
  // The goal of this function is to identify the node ranges in the frontier
  // that are associated with tokens stream unparsing, and AST node unparsing.
  // There ranges are saved and concatinated as we proceed in the evaluation of
  // the synthesized attributes up the AST.

  // We want to generate a IR node range in each node which contains children so
  // that we can concatinate the lists across the whole AST and define the
  // frontier in terms of IR nodes which will then be converted into token
  // ranges to be unparsed and specific IR nodes to be unparsed from the AST
  // directly.

  // DQ (5/15/2021): Unless a transformation is seen via the inherited
  // attribute, inheritedAttribute.isFrontier == false. Transfomations are
  // always marked as inheritedAttribute.isFrontier == true.

  ASSERT_not_null(n);

  ASSERT_require(n == inheritedAttribute.node);
  FrontierDetectionForTokenStreamMapping_SynthesizedAttribute returnAttribute(
      n, inheritedAttribute.sourceFile);

  const SgNodePtrList traversalSuccessors =
      n->get_traversalSuccessorContainer();
  if (traversalSuccessors.size() != synthesizedAttributeList.size()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[frontier-child-shape]: node=%p/%s has "
            "%zu traversal successors but %zu synthesized attributes\n",
            static_cast<void *>(n), n->class_name().c_str(),
            traversalSuccessors.size(), synthesizedAttributeList.size());
    ROSE_ABORT();
  }

  if (inheritedAttribute.isPartOfAuxiliaryDeclarationSubtree) {
    SgStatement *auxiliaryStatement = isSgStatement(n);
    if (inheritedAttribute.isFrontier ||
        inheritedAttribute.unparseUsingTokenStream ||
        inheritedAttribute.unparseFromTheAST ||
        (auxiliaryStatement != nullptr &&
         frontierContext.statementRequiresAstUnparse(auxiliaryStatement))) {
      Sg_File_Info *position = auxiliaryStatement != nullptr
                                   ? auxiliaryStatement->get_file_info()
                                   : nullptr;
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[frontier-auxiliary-emission]: "
          "node=%p/%s semantic-only auxiliary ownership cannot carry "
          "lexical frontier state; inherited=[frontier:%d,tokens:%d,ast:%d] "
          "statement=[transformation:%d,marked:%d] range=[%d:%d] "
          "flags=[compiler-generated:%d,frontend-specific:%d]\n",
          static_cast<void *>(n), n->class_name().c_str(),
          inheritedAttribute.isFrontier,
          inheritedAttribute.unparseUsingTokenStream,
          inheritedAttribute.unparseFromTheAST,
          position != nullptr && position->isTransformation(),
          auxiliaryStatement != nullptr &&
              frontierContext.isStatementMarkedForAstUnparse(
                  auxiliaryStatement),
          position != nullptr ? position->get_physical_line() : -1,
          position != nullptr ? position->get_col() : -1,
          position != nullptr && position->isCompilerGenerated(),
          position != nullptr && position->isFrontendSpecific());
      for (SgNode *ancestor = n; ancestor != nullptr;
           ancestor = ancestor->get_parent()) {
        fprintf(stderr, "  structural-ancestor=%p/%s\n",
                static_cast<void *>(ancestor), ancestor->class_name().c_str());
      }
      ROSE_ABORT();
    }
    for (size_t childIndex = 0; childIndex < synthesizedAttributeList.size();
         ++childIndex) {
      const auto &childAttribute = synthesizedAttributeList[childIndex];
      const bool neutralSemanticChild =
          childAttribute.node == nullptr && !childAttribute.isFrontier &&
          !childAttribute.unparseUsingTokenStream &&
          !childAttribute.unparseFromTheAST &&
          !childAttribute.containsNodesToBeUnparsedFromTheAST &&
          !childAttribute.containsNodesToBeUnparsedFromTheTokenStream;
      const bool exactOrAbsentSourceContext =
          childAttribute.sourceFile == nullptr ||
          childAttribute.sourceFile == inheritedAttribute.sourceFile;
      if (!neutralSemanticChild || !exactOrAbsentSourceContext) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[frontier-auxiliary-emission]: "
                "node=%p/%s child-index=%zu child=%p/%s has a non-neutral "
                "semantic child frontier attribute "
                "[node=%p,frontier=%d,tokens=%d,ast=%d,contains-ast=%d,"
                "contains-tokens=%d,source=%p,expected-source=%p]\n",
                static_cast<void *>(n), n->class_name().c_str(), childIndex,
                static_cast<void *>(traversalSuccessors[childIndex]),
                traversalSuccessors[childIndex] != nullptr
                    ? traversalSuccessors[childIndex]->class_name().c_str()
                    : "<null>",
                static_cast<void *>(childAttribute.node),
                childAttribute.isFrontier,
                childAttribute.unparseUsingTokenStream,
                childAttribute.unparseFromTheAST,
                childAttribute.containsNodesToBeUnparsedFromTheAST,
                childAttribute.containsNodesToBeUnparsedFromTheTokenStream,
                static_cast<void *>(childAttribute.sourceFile),
                static_cast<void *>(inheritedAttribute.sourceFile));
        ROSE_ABORT();
      }
    }

    returnAttribute.node = nullptr;
    returnAttribute.isFrontier = false;
    returnAttribute.unparseUsingTokenStream = false;
    returnAttribute.unparseFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheTokenStream = false;
    returnAttribute.sourceFile = inheritedAttribute.sourceFile;
    ASSERT_not_null(returnAttribute.sourceFile);
    return returnAttribute;
  }

  if (inheritedAttribute.isFunctionDeclarationStructuralWrapper) {
    SgFunctionDeclaration *owner =
        requireExactFunctionStructuralFrontierOwner(n);
    SgStatement *wrapper = isSgStatement(n);
    if (wrapper == nullptr || inheritedAttribute.isFrontier ||
        frontierContext.isStatementMarkedForAstUnparse(wrapper) ||
        inheritedAttribute.unparseUsingTokenStream ==
            inheritedAttribute.unparseFromTheAST) {
      rejectFunctionStructuralFrontierOwnership(
          n, owner,
          "structural wrapper acquired an independent lexical frontier role");
    }

    bool payloadRequiresAstUnparse = false;
    for (const auto &childAttribute : synthesizedAttributeList) {
      const bool neutralSemanticChild =
          childAttribute.node == nullptr && !childAttribute.isFrontier &&
          !childAttribute.unparseUsingTokenStream &&
          !childAttribute.unparseFromTheAST &&
          !childAttribute.containsNodesToBeUnparsedFromTheAST &&
          !childAttribute.containsNodesToBeUnparsedFromTheTokenStream;
      const bool exactLexicalChildMode =
          childAttribute.unparseUsingTokenStream !=
          childAttribute.unparseFromTheAST;
      if (childAttribute.sourceFile != inheritedAttribute.sourceFile ||
          (!neutralSemanticChild && !exactLexicalChildMode)) {
        rejectFunctionStructuralFrontierOwnership(
            n, owner,
            "wrapper payload has neither a neutral semantic role nor one exact "
            "lexical emission mode");
      }
      if (!neutralSemanticChild &&
          (childAttribute.unparseFromTheAST ||
           childAttribute.containsNodesToBeUnparsedFromTheAST)) {
        payloadRequiresAstUnparse = true;
      }
    }

    // Return no statement identity: the enclosing function declaration owns
    // this source interval. A source child transformation is preserved as a
    // synthesized signal so the function, rather than the wrapper, becomes the
    // exact AST-emission boundary.
    returnAttribute.node = nullptr;
    returnAttribute.isFrontier = false;
    returnAttribute.unparseUsingTokenStream = false;
    returnAttribute.unparseFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheAST =
        payloadRequiresAstUnparse;
    returnAttribute.containsNodesToBeUnparsedFromTheTokenStream = false;
    return returnAttribute;
  }

  if (inheritedAttribute.isSourceDeclaratorStructuralWrapper) {
    SgDeclarationStatement *owner =
        requireExactSourceDeclaratorStructuralFrontierOwner(n);
    SgStatement *wrapper = isSgStatement(n);
    if (wrapper == nullptr || inheritedAttribute.isFrontier ||
        frontierContext.statementRequiresAstUnparse(wrapper) ||
        inheritedAttribute.unparseUsingTokenStream ==
            inheritedAttribute.unparseFromTheAST) {
      rejectSourceDeclaratorStructuralFrontierOwnership(
          n, owner,
          "structural wrapper acquired an independent lexical frontier role");
    }

    bool payloadRequiresAstUnparse = false;
    for (const auto &childAttribute : synthesizedAttributeList) {
      const bool neutralSemanticChild =
          childAttribute.node == nullptr && !childAttribute.isFrontier &&
          !childAttribute.unparseUsingTokenStream &&
          !childAttribute.unparseFromTheAST &&
          !childAttribute.containsNodesToBeUnparsedFromTheAST &&
          !childAttribute.containsNodesToBeUnparsedFromTheTokenStream;
      const bool exactLexicalChildMode =
          childAttribute.unparseUsingTokenStream !=
          childAttribute.unparseFromTheAST;
      if (childAttribute.sourceFile != inheritedAttribute.sourceFile ||
          (!neutralSemanticChild && !exactLexicalChildMode)) {
        rejectSourceDeclaratorStructuralFrontierOwnership(
            n, owner,
            "wrapper payload has neither a neutral semantic role nor one exact "
            "lexical emission mode");
      }
      if (!neutralSemanticChild &&
          (childAttribute.unparseFromTheAST ||
           childAttribute.containsNodesToBeUnparsedFromTheAST)) {
        payloadRequiresAstUnparse = true;
      }
    }

    returnAttribute.node = nullptr;
    returnAttribute.isFrontier = false;
    returnAttribute.unparseUsingTokenStream = false;
    returnAttribute.unparseFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheAST =
        payloadRequiresAstUnparse;
    returnAttribute.containsNodesToBeUnparsedFromTheTokenStream = false;
    return returnAttribute;
  }

  if (inheritedAttribute.isPartOfRangeForSemanticDeclarationSubtree) {
    SgVariableDeclaration *wrapper =
        requireExactRangeForSemanticDeclarationSubtreeOwner(n);
    SgRangeBasedForStatement *owner =
        requireExactRangeForSemanticDeclarationFrontierOwner(wrapper);
    SgStatement *statement = isSgStatement(n);
    if (inheritedAttribute.isRangeForSemanticDeclarationWrapper ||
        inheritedAttribute.isFrontier ||
        inheritedAttribute.unparseUsingTokenStream ||
        inheritedAttribute.unparseFromTheAST ||
        (statement != nullptr &&
         frontierContext.statementRequiresAstUnparse(statement))) {
      rejectRangeForSemanticDeclarationFrontierOwnership(
          n, owner,
          "semantic declaration payload acquired an independent lexical "
          "frontier role");
    }

    for (const auto &childAttribute : synthesizedAttributeList) {
      if (childAttribute.node != nullptr || childAttribute.isFrontier ||
          childAttribute.unparseUsingTokenStream ||
          childAttribute.unparseFromTheAST ||
          childAttribute.containsNodesToBeUnparsedFromTheAST ||
          childAttribute.containsNodesToBeUnparsedFromTheTokenStream ||
          childAttribute.sourceFile != inheritedAttribute.sourceFile) {
        rejectRangeForSemanticDeclarationFrontierOwnership(
            n, owner,
            "semantic declaration payload has a non-neutral frontier child");
      }
    }

    returnAttribute.node = nullptr;
    returnAttribute.isFrontier = false;
    returnAttribute.unparseUsingTokenStream = false;
    returnAttribute.unparseFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheTokenStream = false;
    return returnAttribute;
  }

  if (inheritedAttribute.isRangeForSemanticDeclarationWrapper) {
    SgVariableDeclaration *wrapper = isSgVariableDeclaration(n);
    SgRangeBasedForStatement *owner =
        requireExactRangeForSemanticDeclarationFrontierOwner(wrapper);
    if (wrapper == nullptr || inheritedAttribute.isFrontier ||
        frontierContext.statementRequiresAstUnparse(wrapper) ||
        inheritedAttribute.unparseUsingTokenStream ==
            inheritedAttribute.unparseFromTheAST) {
      rejectRangeForSemanticDeclarationFrontierOwnership(
          n, owner,
          "semantic declaration shell acquired an independent lexical "
          "frontier role");
    }

    bool payloadRequiresAstUnparse = false;
    for (const auto &childAttribute : synthesizedAttributeList) {
      const bool neutralSemanticChild =
          childAttribute.node == nullptr && !childAttribute.isFrontier &&
          !childAttribute.unparseUsingTokenStream &&
          !childAttribute.unparseFromTheAST &&
          !childAttribute.containsNodesToBeUnparsedFromTheAST &&
          !childAttribute.containsNodesToBeUnparsedFromTheTokenStream;
      const bool exactLexicalChildMode =
          childAttribute.unparseUsingTokenStream !=
          childAttribute.unparseFromTheAST;
      if (childAttribute.sourceFile != inheritedAttribute.sourceFile ||
          (!neutralSemanticChild && !exactLexicalChildMode)) {
        rejectRangeForSemanticDeclarationFrontierOwnership(
            n, owner,
            "declaration payload has neither a neutral semantic role nor one "
            "exact lexical emission mode");
      }
      if (!neutralSemanticChild &&
          (childAttribute.unparseFromTheAST ||
           childAttribute.containsNodesToBeUnparsedFromTheAST)) {
        payloadRequiresAstUnparse = true;
      }
    }

    // The enclosing range-for owns the only lexical statement boundary.  Keep
    // any source-expression transformation signal, but erase the hidden
    // declaration shell's statement identity.
    returnAttribute.node = nullptr;
    returnAttribute.isFrontier = false;
    returnAttribute.unparseUsingTokenStream = false;
    returnAttribute.unparseFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheAST =
        payloadRequiresAstUnparse;
    returnAttribute.containsNodesToBeUnparsedFromTheTokenStream = false;
    return returnAttribute;
  }

  if (inheritedAttribute.isImplicitControlFlowStructuralWrapper) {
    SgStatement *owner = requireExactImplicitControlFlowFrontierOwner(n);
    SgStatement *wrapper = isSgStatement(n);
    if (wrapper == nullptr || inheritedAttribute.isFrontier ||
        frontierContext.statementRequiresAstUnparse(wrapper) ||
        inheritedAttribute.unparseUsingTokenStream ==
            inheritedAttribute.unparseFromTheAST) {
      rejectImplicitControlFlowFrontierOwnership(
          n, owner,
          "structural wrapper acquired an independent lexical frontier role");
    }

    bool payloadRequiresAstUnparse = false;
    size_t lexicalChildCount = 0;
    for (const auto &childAttribute : synthesizedAttributeList) {
      const bool neutralSemanticChild =
          childAttribute.node == nullptr && !childAttribute.isFrontier &&
          !childAttribute.unparseUsingTokenStream &&
          !childAttribute.unparseFromTheAST &&
          !childAttribute.containsNodesToBeUnparsedFromTheAST &&
          !childAttribute.containsNodesToBeUnparsedFromTheTokenStream;
      const bool exactLexicalChildMode =
          childAttribute.unparseUsingTokenStream !=
          childAttribute.unparseFromTheAST;
      if (childAttribute.sourceFile != inheritedAttribute.sourceFile ||
          (!neutralSemanticChild && !exactLexicalChildMode)) {
        rejectImplicitControlFlowFrontierOwnership(
            n, owner,
            "wrapper child has neither a neutral semantic role nor one exact "
            "lexical emission mode");
      }
      if (!neutralSemanticChild) {
        ++lexicalChildCount;
      }
      if (!neutralSemanticChild &&
          (childAttribute.unparseFromTheAST ||
           childAttribute.containsNodesToBeUnparsedFromTheAST)) {
        payloadRequiresAstUnparse = true;
      }
    }
    if (lexicalChildCount != 1) {
      rejectImplicitControlFlowFrontierOwnership(
          n, owner,
          "wrapper does not contain exactly one lexical controlled statement");
    }

    returnAttribute.node = nullptr;
    returnAttribute.isFrontier = false;
    returnAttribute.unparseUsingTokenStream = false;
    returnAttribute.unparseFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheAST =
        payloadRequiresAstUnparse;
    returnAttribute.containsNodesToBeUnparsedFromTheTokenStream = false;
    return returnAttribute;
  }

  if (SgCatchStatementSeq *sequence = isSgCatchStatementSeq(n)) {
    SgTryStmt *owner = requireExactCatchSequenceFrontierOwner(sequence);
    const SgStatementPtrList &handlers = sequence->get_catch_statement_seq();
    if (inheritedAttribute.isPartOfTemplateInstantiation) {
      if (inheritedAttribute.isFrontier ||
          inheritedAttribute.unparseUsingTokenStream ||
          inheritedAttribute.unparseFromTheAST ||
          frontierContext.statementRequiresAstUnparse(sequence) ||
          synthesizedAttributeList.size() != handlers.size()) {
        rejectCatchSequenceFrontierOwnership(
            sequence, owner,
            "template-instantiation wrapper acquired a lexical emission "
            "role");
      }
      for (std::size_t index = 0; index < handlers.size(); ++index) {
        const auto &child = synthesizedAttributeList[index];
        if (child.node != handlers[index] ||
            child.sourceFile != inheritedAttribute.sourceFile ||
            child.isFrontier || child.unparseUsingTokenStream ||
            child.unparseFromTheAST ||
            child.containsNodesToBeUnparsedFromTheAST ||
            child.containsNodesToBeUnparsedFromTheTokenStream) {
          rejectCatchSequenceFrontierOwnership(
              sequence, owner,
              "template-instantiation handler escaped its exact semantic "
              "subtree");
        }
      }
      returnAttribute.node = nullptr;
      returnAttribute.isFrontier = false;
      returnAttribute.unparseUsingTokenStream = false;
      returnAttribute.unparseFromTheAST = false;
      returnAttribute.containsNodesToBeUnparsedFromTheAST = false;
      returnAttribute.containsNodesToBeUnparsedFromTheTokenStream = false;
      return returnAttribute;
    }
    const bool neutralAuxiliarySubtree =
        inheritedAttribute.isPartOfAuxiliaryDeclarationSubtree &&
        !inheritedAttribute.unparseUsingTokenStream &&
        !inheritedAttribute.unparseFromTheAST;
    const bool neutralNoncurrentHeader =
        !inheritedAttribute.unparseUsingTokenStream &&
        !inheritedAttribute.unparseFromTheAST &&
        !inheritedAttribute.isInCurrentFile &&
        !inheritedAttribute.isNodeFromCurrentFile(owner) &&
        !inheritedAttribute.sourceFile->get_unparseHeaderFiles();
    if (neutralAuxiliarySubtree || neutralNoncurrentHeader) {
      if (frontierContext.statementRequiresAstUnparse(sequence) ||
          synthesizedAttributeList.size() != handlers.size()) {
        rejectCatchSequenceFrontierOwnership(
            sequence, owner,
            "neutral nonlexical wrapper acquired an output role");
      }
      for (std::size_t index = 0; index < handlers.size(); ++index) {
        const auto &child = synthesizedAttributeList[index];
        if (child.node != handlers[index] ||
            child.sourceFile != inheritedAttribute.sourceFile ||
            child.isFrontier || child.unparseUsingTokenStream ||
            child.unparseFromTheAST ||
            child.containsNodesToBeUnparsedFromTheAST ||
            child.containsNodesToBeUnparsedFromTheTokenStream) {
          rejectCatchSequenceFrontierOwnership(
              sequence, owner,
              "handler escaped its neutral nonlexical subtree");
        }
      }
      returnAttribute.node = nullptr;
      returnAttribute.isFrontier = false;
      returnAttribute.unparseUsingTokenStream = false;
      returnAttribute.unparseFromTheAST = false;
      returnAttribute.containsNodesToBeUnparsedFromTheAST = false;
      returnAttribute.containsNodesToBeUnparsedFromTheTokenStream = false;
      return returnAttribute;
    }
    if (inheritedAttribute.isFrontier ||
        frontierContext.statementRequiresAstUnparse(sequence) ||
        inheritedAttribute.unparseUsingTokenStream ==
            inheritedAttribute.unparseFromTheAST ||
        synthesizedAttributeList.size() != handlers.size()) {
      rejectCatchSequenceFrontierOwnership(
          sequence, owner,
          "structural wrapper acquired an invalid synthesized emission role");
    }

    bool containsAst = false;
    bool containsTokens = false;
    for (std::size_t index = 0; index < handlers.size(); ++index) {
      const auto &child = synthesizedAttributeList[index];
      if (child.node != handlers[index] ||
          child.sourceFile != inheritedAttribute.sourceFile ||
          child.unparseUsingTokenStream == child.unparseFromTheAST) {
        rejectCatchSequenceFrontierOwnership(
            sequence, owner,
            "handler did not publish one exact synthesized source role");
      }
      containsAst = containsAst || child.unparseFromTheAST ||
                    child.containsNodesToBeUnparsedFromTheAST;
      containsTokens = containsTokens || child.unparseUsingTokenStream ||
                       child.containsNodesToBeUnparsedFromTheTokenStream;
    }

    // The sequence is transparent to token ownership.  Its SgTryStmt parent
    // consumes these aggregate flags and owns the only lexical frontier.
    returnAttribute.node = nullptr;
    returnAttribute.isFrontier = false;
    returnAttribute.unparseUsingTokenStream = false;
    returnAttribute.unparseFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheAST = containsAst;
    returnAttribute.containsNodesToBeUnparsedFromTheTokenStream =
        containsTokens;
    return returnAttribute;
  }

  if (inheritedAttribute.isSourceLessForStructuralPayload) {
    SgNullStatement *payload = isSgNullStatement(n);
    SgForInitStatement *wrapper =
        isSgForInitStatement(n != nullptr ? n->get_parent() : nullptr);
    SgDeclarationGroupStatement *group =
        wrapper != nullptr
            ? requireExactForInitDeclarationGroupFrontierOwner(wrapper)
            : nullptr;
    SgForStatement *forOwner =
        isSgForStatement(n != nullptr ? n->get_parent() : nullptr);
    const bool exactConditionEdge =
        forOwner != nullptr && forOwner->get_test() == payload;
    if (payload == nullptr || (wrapper == nullptr && !exactConditionEdge) ||
        group != nullptr || inheritedAttribute.isFrontier ||
        inheritedAttribute.unparseUsingTokenStream ||
        inheritedAttribute.unparseFromTheAST ||
        frontierContext.statementRequiresAstUnparse(payload) ||
        !traversalSuccessors.empty() || !synthesizedAttributeList.empty()) {
      rejectForInitDeclarationGroupFrontierOwnership(
          n, wrapper, group,
          "semantic null for-field published a physical frontier state");
    }

    returnAttribute.node = nullptr;
    returnAttribute.isFrontier = false;
    returnAttribute.unparseUsingTokenStream = false;
    returnAttribute.unparseFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheTokenStream = false;
    return returnAttribute;
  }

  if (inheritedAttribute.isForInitDeclarationGroupWrapper) {
    SgForInitStatement *wrapper = isSgForInitStatement(n);
    if (wrapper == nullptr) {
      rejectForInitDeclarationGroupFrontierOwnership(
          n, nullptr, nullptr,
          "typed for-init wrapper role is not an SgForInitStatement");
    }
    SgDeclarationGroupStatement *group =
        requireExactForInitDeclarationGroupFrontierOwner(wrapper);
    const SgStatementPtrList &initializers = wrapper->get_init_stmt();
    SgStatement *payload =
        initializers.size() == 1 ? initializers.front() : nullptr;
    if (payload == nullptr || inheritedAttribute.isFrontier ||
        frontierContext.statementRequiresAstUnparse(wrapper) ||
        inheritedAttribute.unparseUsingTokenStream ==
            inheritedAttribute.unparseFromTheAST ||
        traversalSuccessors.size() != 1 ||
        traversalSuccessors.front() != payload ||
        synthesizedAttributeList.size() != 1) {
      rejectForInitDeclarationGroupFrontierOwnership(
          n, wrapper, group,
          "structural wrapper acquired an ambiguous lexical frontier role");
    }

    const FrontierDetectionForTokenStreamMapping_SynthesizedAttribute
        &payloadAttribute = synthesizedAttributeList.front();
    const bool sourceLess =
        wrapper->get_file_info() != nullptr &&
        wrapper->get_file_info()->get_physical_file_id() < 0;
    if (sourceLess) {
      if (group != nullptr || isSgNullStatement(payload) == nullptr ||
          payloadAttribute.node != nullptr || payloadAttribute.isFrontier ||
          payloadAttribute.sourceFile != inheritedAttribute.sourceFile ||
          payloadAttribute.unparseUsingTokenStream ||
          payloadAttribute.unparseFromTheAST ||
          payloadAttribute.containsNodesToBeUnparsedFromTheAST ||
          payloadAttribute.containsNodesToBeUnparsedFromTheTokenStream) {
        rejectForInitDeclarationGroupFrontierOwnership(
            n, wrapper, group,
            "source-less wrapper payload did not remain semantic-only");
      }
      returnAttribute.node = nullptr;
      returnAttribute.isFrontier = false;
      returnAttribute.unparseUsingTokenStream = false;
      returnAttribute.unparseFromTheAST = false;
      returnAttribute.containsNodesToBeUnparsedFromTheAST = false;
      returnAttribute.containsNodesToBeUnparsedFromTheTokenStream = false;
      return returnAttribute;
    }

    if (payloadAttribute.node != payload ||
        payloadAttribute.sourceFile == nullptr ||
        payloadAttribute.sourceFile != inheritedAttribute.sourceFile ||
        payloadAttribute.unparseUsingTokenStream ==
            payloadAttribute.unparseFromTheAST) {
      rejectForInitDeclarationGroupFrontierOwnership(
          n, wrapper, group,
          "wrapper payload did not publish one exact lexical "
          "frontier identity");
    }

    // The payload is the sole source owner. Reuse its complete synthesized
    // state so the enclosing SgForStatement cannot also publish the structural
    // SgForInitStatement for the same token interval.
    return payloadAttribute;
  }

  auto requireChildAttribute = [&](SgNode *child, const char *edge,
                                   bool required)
      -> const FrontierDetectionForTokenStreamMapping_SynthesizedAttribute * {
    if (child == nullptr) {
      if (!required) {
        return nullptr;
      }
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[frontier-child-shape]: node=%p/%s "
              "required edge=%s is null\n",
              static_cast<void *>(n), n->class_name().c_str(), edge);
      ROSE_ABORT();
    }

    const auto successor = std::find(traversalSuccessors.begin(),
                                     traversalSuccessors.end(), child);
    if (successor == traversalSuccessors.end()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[frontier-child-shape]: node=%p/%s "
              "edge=%s child=%p/%s is not an exact traversal successor\n",
              static_cast<void *>(n), n->class_name().c_str(), edge,
              static_cast<void *>(child), child->class_name().c_str());
      ROSE_ABORT();
    }

    const size_t childIndex = static_cast<size_t>(
        std::distance(traversalSuccessors.begin(), successor));
    const FrontierDetectionForTokenStreamMapping_SynthesizedAttribute
        &childAttribute = synthesizedAttributeList[childIndex];
    SgStatement *expectedStatement = isSgStatement(child);
    if (SgForInitStatement *forInit = isSgForInitStatement(child)) {
      (void)requireExactForInitDeclarationGroupFrontierOwner(forInit);
      const SgStatementPtrList &initializers = forInit->get_init_stmt();
      expectedStatement =
          initializers.size() == 1 ? initializers.front() : nullptr;
    }
    if ((childAttribute.node != nullptr &&
         childAttribute.node != expectedStatement) ||
        (childAttribute.sourceFile != nullptr &&
         childAttribute.sourceFile != inheritedAttribute.sourceFile)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[frontier-child-shape]: node=%p/%s "
              "edge=%s child=%p/%s synthesized-node=%p source-file=%p "
              "expected-source-file=%p has mismatched identity\n",
              static_cast<void *>(n), n->class_name().c_str(), edge,
              static_cast<void *>(child), child->class_name().c_str(),
              static_cast<void *>(childAttribute.node),
              static_cast<void *>(childAttribute.sourceFile),
              static_cast<void *>(inheritedAttribute.sourceFile));
      ROSE_ABORT();
    }
    return &childAttribute;
  };

  // SgFunctionDefinition is the semantic scope between a defining function
  // declaration and its compound body; it does not own a third source
  // surface.  Publishing it as an independent frontier node would alias the
  // body's exact token interval.  Delegate the structural wrapper to its one
  // body child and reject any producer that tries to transform the wrapper
  // itself instead of an exact lexical owner.
  if (SgFunctionDefinition *definition = isSgFunctionDefinition(n)) {
    SgFunctionDeclaration *declaration =
        isSgFunctionDeclaration(definition->get_parent());
    SgBasicBlock *body = definition->get_body();
    if (declaration == nullptr || declaration->get_definition() != definition ||
        definition->get_declaration() != declaration || body == nullptr ||
        body->get_parent() != definition) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[function-definition-frontier]: "
              "definition=%p declaration=%p body=%p has malformed exact "
              "structural ownership\n",
              static_cast<void *>(definition), static_cast<void *>(declaration),
              static_cast<void *>(body));
      ROSE_ABORT();
    }
    if (frontierContext.isStatementMarkedForAstUnparse(definition)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[function-definition-frontier]: "
              "structural definition=%p for declaration=%p was selected as "
              "an AST-emission owner; transform the exact declaration or "
              "body instead\n",
              static_cast<void *>(definition),
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }

    const FrontierDetectionForTokenStreamMapping_SynthesizedAttribute
        *bodyAttribute = nullptr;
    for (const FrontierDetectionForTokenStreamMapping_SynthesizedAttribute
             &childAttribute : synthesizedAttributeList) {
      if (childAttribute.node == nullptr) {
        continue;
      }
      if (childAttribute.node != body || bodyAttribute != nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[function-definition-frontier]: "
                "definition=%p has a non-body or duplicate statement child "
                "attribute=%p expected-body=%p\n",
                static_cast<void *>(definition),
                static_cast<void *>(childAttribute.node),
                static_cast<void *>(body));
        ROSE_ABORT();
      }
      bodyAttribute = &childAttribute;
    }
    if (bodyAttribute == nullptr || bodyAttribute->sourceFile == nullptr ||
        bodyAttribute->sourceFile != inheritedAttribute.sourceFile) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[function-definition-frontier]: "
              "definition=%p has no exact synthesized body attribute\n",
              static_cast<void *>(definition));
      ROSE_ABORT();
    }
    return *bodyAttribute;
  }

  // We need to handle SgStatement, plus the SgSourceFile because we need to
  // copy synthesized results from the SgGlobal to the SgSourceFile.
  SgStatement *statement = isSgStatement(n);
  SgSourceFile *sourceFile = isSgSourceFile(n);

  if ((statement != nullptr || sourceFile != nullptr) &&
      inheritedAttribute.isPartOfTemplateInstantiation == false) {

    // Mark these directly.
    returnAttribute.isFrontier = inheritedAttribute.isFrontier;

    // This is a reasonable default setting.
    if (inheritedAttribute.unparseFromTheAST == true) {
      // DQ (5/16/2021): If this was marked to be unparsed from the AST within
      // the inherited attribute, then nothing computed on the children should
      // change that.
      returnAttribute.containsNodesToBeUnparsedFromTheAST = true;
    }

    ASSERT_require(returnAttribute.unparseUsingTokenStream == false &&
                   returnAttribute.unparseFromTheAST == false);

    if (returnAttribute.containsNodesToBeUnparsedFromTheAST == false) {
      SgIfStmt *ifStatement = isSgIfStmt(statement);
      bool specialCaseNode = false;

      if (SgTryStmt *tryStatement = isSgTryStmt(statement)) {
        SgCatchStatementSeq *sequence =
            tryStatement->get_catch_statement_seq_root();
        (void)requireExactCatchSequenceFrontierOwner(sequence);
        const auto successor = std::find(traversalSuccessors.begin(),
                                         traversalSuccessors.end(), sequence);
        if (successor == traversalSuccessors.end()) {
          rejectCatchSequenceFrontierOwnership(
              sequence, tryStatement,
              "try statement does not traverse its catch sequence");
        }
        const std::size_t index = static_cast<std::size_t>(
            std::distance(traversalSuccessors.begin(), successor));
        const auto &sequenceAttribute = synthesizedAttributeList[index];
        if (sequenceAttribute.node != nullptr ||
            sequenceAttribute.sourceFile != inheritedAttribute.sourceFile ||
            sequenceAttribute.unparseUsingTokenStream ||
            sequenceAttribute.unparseFromTheAST) {
          rejectCatchSequenceFrontierOwnership(
              sequence, tryStatement,
              "catch sequence did not remain a transparent frontier "
              "container");
        }
        if (sequenceAttribute.containsNodesToBeUnparsedFromTheAST) {
          specialCaseNode = true;
        }
      }

      if (ifStatement != nullptr &&
          !ifStatement->get_is_if_consteval_statement()) {
        // There are special cases where we don't (at least presently) want to
        // mix the two types on unparsing. This is how those special cases are
        // handled.
        const auto *conditionalAttribute = requireChildAttribute(
            ifStatement->get_conditional(), "p_conditional", true);
        const auto *trueBodyAttribute = requireChildAttribute(
            ifStatement->get_true_body(), "p_true_body", true);
        if (conditionalAttribute->containsNodesToBeUnparsedFromTheAST !=
            trueBodyAttribute->containsNodesToBeUnparsedFromTheAST) {
          specialCaseNode = true;
        }
      }

      SgTypedefDeclaration *typedefDeclaration =
          isSgTypedefDeclaration(statement);
      if (typedefDeclaration != nullptr) {
        SgDeclarationStatement *baseTypeDefinition =
            typedefDeclaration->get_baseTypeDefiningDeclaration();
        if (typedefDeclaration
                ->get_typedefBaseTypeContainsDefiningDeclaration() &&
            baseTypeDefinition == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[frontier-child-shape]: "
                  "typedef=%p name=%s claims an inline base-type definition "
                  "but has no defining declaration\n",
                  static_cast<void *>(typedefDeclaration),
                  typedefDeclaration->get_name().str());
          ROSE_ABORT();
        }
        const auto *baseTypeAttribute =
            requireChildAttribute(baseTypeDefinition, "p_declaration", false);
        if (baseTypeAttribute != nullptr &&
            baseTypeAttribute->containsNodesToBeUnparsedFromTheAST &&
            baseTypeAttribute->containsNodesToBeUnparsedFromTheTokenStream) {
          specialCaseNode = true;
        }
      }

      // DQ (12/1/2013): This handles the details of the SgForStatement (which
      // has 3 relevant children (excluding the body).
      SgForStatement *forStatement = isSgForStatement(statement);
      if (forStatement != nullptr) {
        // There are special cases where we don't (at least presently) want to
        // mix the two types on unparsing. This is how those special cases are
        // handled.
        const auto *initializerAttribute = requireChildAttribute(
            forStatement->get_for_init_stmt(), "p_for_init_stmt", true);
        const auto *testAttribute =
            requireChildAttribute(forStatement->get_test(), "p_test", true);
        const auto *incrementAttribute = requireChildAttribute(
            forStatement->get_increment(), "p_increment", true);

        const bool containsAst =
            initializerAttribute->containsNodesToBeUnparsedFromTheAST ||
            testAttribute->containsNodesToBeUnparsedFromTheAST ||
            incrementAttribute->containsNodesToBeUnparsedFromTheAST;
        const bool containsTokens =
            initializerAttribute->containsNodesToBeUnparsedFromTheTokenStream ||
            testAttribute->containsNodesToBeUnparsedFromTheTokenStream ||
            incrementAttribute->containsNodesToBeUnparsedFromTheTokenStream;
        // Tokenless structural children are not an AST transformation.  Promote
        // the loop header only when its written components genuinely mix an
        // AST-unparsed subtree with a token-unparsed subtree.
        if (containsAst && containsTokens) {
          specialCaseNode = true;
        }
      }

      if (specialCaseNode == true) {
        // Mark the current node to be a frontier, instead of the child nodes.
        // Mark as to be unparse from the AST (no choice since subtrees must be
        // unparsed from the AST).
        returnAttribute.isFrontier = true;

        bool unparseUsingTokenStream = false;
        bool unparseFromTheAST = true;

        FrontierNode *frontierNode = new FrontierNode(
            statement, unparseUsingTokenStream, unparseFromTheAST);
        ASSERT_not_null(frontierNode);

        addFrontierNode(statement, frontierNode);
        returnAttribute.containsNodesToBeUnparsedFromTheAST = true;
        returnAttribute.containsNodesToBeUnparsedFromTheTokenStream = false;
      } else {
        // This is the non-special case.
        ASSERT_require(specialCaseNode == false);
        for (size_t i = 0; i < synthesizedAttributeList.size(); i++) {
          SgStatement *child_synthesized_attribute_statement =
              synthesizedAttributeList[i].node;

          if (child_synthesized_attribute_statement != nullptr &&
              inheritedAttribute.isPartOfTemplateInstantiation == false) {

            bool childNodeIsFromSameFileAsCurrentNode =
                isChildNodeFromSameFileAsCurrentNode(
                    n, child_synthesized_attribute_statement);
            // A foreign physical child is token-only unless its subtree is a
            // structural bridge to an AST transformation owned by this exact
            // materialized file frontier. This occurs when an include is the
            // first statement in a block but later statements return to the
            // including file.
            if (childNodeIsFromSameFileAsCurrentNode == false &&
                !frontierContext.statementContainsAstUnparse(
                    child_synthesized_attribute_statement)) {
              synthesizedAttributeList[i].unparseFromTheAST = false;
              synthesizedAttributeList[i].unparseUsingTokenStream = true;
            }

            if (synthesizedAttributeList[i].unparseUsingTokenStream == true) {
              FrontierNode *frontierNode = new FrontierNode(
                  child_synthesized_attribute_statement,
                  synthesizedAttributeList[i].unparseUsingTokenStream,
                  synthesizedAttributeList[i].unparseFromTheAST);
              ASSERT_not_null(frontierNode);
              addFrontierNode(synthesizedAttributeList[i].node, frontierNode);

              returnAttribute.containsNodesToBeUnparsedFromTheTokenStream =
                  true;
            } else {
              if (synthesizedAttributeList[i].unparseFromTheAST == true) {
                FrontierNode *frontierNode = new FrontierNode(
                    child_synthesized_attribute_statement,
                    synthesizedAttributeList[i].unparseUsingTokenStream,
                    synthesizedAttributeList[i].unparseFromTheAST);
                ASSERT_not_null(frontierNode);
                addFrontierNode(synthesizedAttributeList[i].node, frontierNode);
                returnAttribute.containsNodesToBeUnparsedFromTheAST = true;
              } else {
                // Lack of AST usage in the subtree implies that there are nodes
                // unparsed from the token stream.
                if (synthesizedAttributeList[i]
                        .containsNodesToBeUnparsedFromTheAST == false) {
                  returnAttribute.containsNodesToBeUnparsedFromTheTokenStream =
                      true;
                }
              }
            }
          }
        }
      }

    } else {
      ASSERT_require(returnAttribute.containsNodesToBeUnparsedFromTheAST ==
                     true);
    }

    // DQ (5/15/2021): Each should be false before this point.
    ASSERT_require(returnAttribute.unparseFromTheAST == false &&
                   returnAttribute.unparseUsingTokenStream == false);

    // DQ (5/15/2021): Here is where we mark the values
    // returnAttribute.unparseFromTheAST and
    // returnAttribute.unparseUsingTokenStream. To support both unparsing from
    // the token stream and from the AST, we need to mark
    // returnAttribute.unparseFromTheAST = true.
    if (returnAttribute.containsNodesToBeUnparsedFromTheAST == true &&
        returnAttribute.containsNodesToBeUnparsedFromTheTokenStream == true) {
      returnAttribute.isFrontier = true;
      returnAttribute.unparseFromTheAST = true;
    } else {
      if (returnAttribute.containsNodesToBeUnparsedFromTheAST == false &&
          returnAttribute.containsNodesToBeUnparsedFromTheTokenStream == true) {
        returnAttribute.unparseUsingTokenStream = true;
      } else {
        if (returnAttribute.containsNodesToBeUnparsedFromTheAST == true &&
            returnAttribute.containsNodesToBeUnparsedFromTheTokenStream ==
                false) {
          returnAttribute.unparseFromTheAST = true;
        } else {
          if (returnAttribute.containsNodesToBeUnparsedFromTheAST == false &&
              returnAttribute.containsNodesToBeUnparsedFromTheTokenStream ==
                  false) {
            // DQ (5/16/2021): Just because
            // returnAttribute.containsNodesToBeUnparsedFromTheTokenStream ==
            // false does not imply that this node should be unpared from the
            // AST (e.g. the case of an empty SgBasicBlock).
            returnAttribute.unparseUsingTokenStream = true;
          } else {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[token-frontier-state]: synthesized "
                    "frontier state is neither AST-backed nor token-backed\n");
            ASSERT_require(false);
          }
        }
      }
    }

    if (statement != nullptr &&
        returnAttribute.containsNodesToBeUnparsedFromTheAST == true) {
      FrontierNode *frontierNode =
          new FrontierNode(statement, returnAttribute.unparseUsingTokenStream,
                           returnAttribute.unparseFromTheAST);

      addFrontierNode(statement, frontierNode);
    } else {
      // This case happens for test_CplusplusMacro_C.c, which has nothing but
      // CPP directives.
    }

    ASSERT_require((returnAttribute.unparseUsingTokenStream == true &&
                    returnAttribute.unparseFromTheAST == false) ||
                   (returnAttribute.unparseUsingTokenStream == false &&
                    returnAttribute.unparseFromTheAST == true) ||
                   (returnAttribute.unparseUsingTokenStream == false &&
                    returnAttribute.unparseFromTheAST == false));
  } else {
    // DQ (5/15/2021): This characterizes this false branch.
    ASSERT_require((statement == nullptr && sourceFile == nullptr) ||
                   inheritedAttribute.isPartOfTemplateInstantiation == true);

    returnAttribute.isFrontier = false;
    returnAttribute.unparseUsingTokenStream = false;
    returnAttribute.unparseFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheAST = false;
    returnAttribute.containsNodesToBeUnparsedFromTheTokenStream = false;

    returnAttribute.isFrontier = inheritedAttribute.isFrontier;
    returnAttribute.unparseUsingTokenStream =
        inheritedAttribute.unparseUsingTokenStream;
    returnAttribute.unparseFromTheAST = inheritedAttribute.unparseFromTheAST;
    returnAttribute.containsNodesToBeUnparsedFromTheAST = false;
  }

  ASSERT_require((returnAttribute.unparseUsingTokenStream == true &&
                  returnAttribute.unparseFromTheAST == false) ||
                 (returnAttribute.unparseUsingTokenStream == false &&
                  returnAttribute.unparseFromTheAST == true) ||
                 (returnAttribute.unparseUsingTokenStream == false &&
                  returnAttribute.unparseFromTheAST == false));

  return returnAttribute;
}

void frontierDetectionForTokenStreamMapping(
    SgSourceFile *sourceFile, bool traverseHeaderFiles,
    TokenUnparseFrontierContext &context, SgNode *traversalRoot) {
  // This frontier detection happens before we associate token subsequences to
  // the AST (in a separate map).
  ASSERT_not_null(sourceFile);

  FrontierDetectionForTokenStreamMapping_InheritedAttribute inheritedAttribute(
      sourceFile);
  TokenUnparseFrontierFileContext &fileContext = context.file(sourceFile);
  FrontierDetectionForTokenStreamMapping fdTraversal(sourceFile, fileContext);

  FrontierDetectionForTokenStreamMapping_SynthesizedAttribute topAttribute;
  if (traversalRoot != nullptr) {
    topAttribute = fdTraversal.traverse(traversalRoot, inheritedAttribute);
  } else if (traverseHeaderFiles == false) {
    topAttribute =
        fdTraversal.traverseWithinFile(sourceFile, inheritedAttribute);
  } else {
    topAttribute = fdTraversal.traverse(sourceFile, inheritedAttribute);
  }
  ASSERT_require(fdTraversal.frontierNodes.size() > 0);
  const auto &tokenMappings = sourceFile->get_tokenSubsequenceMap();

  for (const auto &fileEntry : fdTraversal.frontierNodes) {
    std::map<SgStatement *, FrontierNode *> *frontierMap = fileEntry.second;
    ASSERT_not_null(frontierMap);

    // A frontier is an antichain.  Once an exact ancestor statement owns AST
    // emission, retaining any descendant as a second frontier region causes
    // the unparser to interleave the ancestor's AST text with tokens or AST
    // text from the same source interval.  Remove those dominated regions
    // before transferring ownership into the invocation context.
    std::vector<SgStatement *> dominatedStatements;
    for (const auto &entry : *frontierMap) {
      SgStatement *statement = entry.first;
      FrontierNode *statementFrontier = entry.second;
      for (SgNode *cursor = statement->get_parent(); cursor != nullptr;
           cursor = cursor->get_parent()) {
        SgStatement *ancestor = isSgStatement(cursor);
        if (ancestor == nullptr) {
          continue;
        }
        auto ancestorEntry = frontierMap->find(ancestor);
        if (ancestorEntry == frontierMap->end()) {
          continue;
        }
        if (fileContext.statementRequiresAstUnparse(ancestor)) {
          dominatedStatements.push_back(statement);
          break;
        }

        // A macro expansion can give every statement produced by the macro
        // the same invocation token interval. If both the ancestor and child
        // are token-backed, the outer statement is the unique structural
        // owner of that surface; the child is not a second frontier region.
        // Establish that ownership while constructing the frontier, and only
        // for an exact ancestor with the exact same nonempty interval.
        FrontierNode *ancestorFrontier = ancestorEntry->second;
        const auto statementMapping = tokenMappings.find(statement);
        const auto ancestorMapping = tokenMappings.find(ancestor);
        if (statementFrontier != nullptr && ancestorFrontier != nullptr &&
            statementFrontier->unparseUsingTokenStream &&
            ancestorFrontier->unparseUsingTokenStream &&
            statementMapping != tokenMappings.end() &&
            ancestorMapping != tokenMappings.end() &&
            statementMapping->second != nullptr &&
            ancestorMapping->second != nullptr) {
          const TokenStreamHalfOpenInterval &statementInterval =
              statementMapping->second->halfOpenInterval(
                  TokenStreamIntervalKind::token_subsequence);
          const TokenStreamHalfOpenInterval &ancestorInterval =
              ancestorMapping->second->halfOpenInterval(
                  TokenStreamIntervalKind::token_subsequence);
          if (!statementInterval.empty() &&
              statementInterval.begin == ancestorInterval.begin &&
              statementInterval.end == ancestorInterval.end) {
            dominatedStatements.push_back(statement);
            break;
          }
        }
      }
    }
    for (SgStatement *statement : dominatedStatements) {
      auto dominated = frontierMap->find(statement);
      ROSE_ASSERT(dominated != frontierMap->end());
      delete dominated->second;
      frontierMap->erase(dominated);
    }

    for (const auto &entry : *frontierMap) {
      fileContext.adoptFrontierNode(entry.first, entry.second);
    }
  }

  ASSERT_not_null(sourceFile);
  SgProject *project = SageInterface::getEnclosingNode<SgProject>(sourceFile);
  ASSERT_not_null(project);

  // Now traverse the AST and record the linked list of nodes to be unparsed as
  // tokens and from the AST. So that we can query next and last statements and
  // determine if they were unparsed from the token stream or the AST.  Not
  // clear if the edges of token-stream/AST unparsing should be unparsed from
  // the token stream leading trailing token information or from the AST using
  // the attached CPP info.
}

FrontierNode::FrontierNode(SgStatement *node, bool unparseUsingTokenStream,
                           bool unparseFromTheAST)
    : node(node), unparseUsingTokenStream(unparseUsingTokenStream),
      unparseFromTheAST(unparseFromTheAST) {
  // Enforce specific constraints.
  ASSERT_not_null(node);
  ASSERT_require(
      (unparseUsingTokenStream == true && unparseFromTheAST == false) ||
      (unparseUsingTokenStream == false && unparseFromTheAST == true) ||
      (unparseUsingTokenStream == false && unparseFromTheAST == false));
  // DQ (5/16/2021): Either one of the other of these should be true (we must
  // unparse from either the AST or the token stream).
  ASSERT_require(
      (unparseUsingTokenStream == true || unparseFromTheAST == true));
}

void FrontierDetectionForTokenStreamMapping::addFrontierNode(
    SgStatement *statement, FrontierNode *frontierNode) {
  int physical_file_id =
      requireStatementPhysicalFileId(statement, "add-frontier-node");
  ASSERT_require(physical_file_id >= 0);
  ASSERT_require(frontierNode->node == statement);

  if (frontierNodes.find(physical_file_id) == frontierNodes.end()) {
    frontierNodes[physical_file_id] = new map<SgStatement *, FrontierNode *>();
  }

  ASSERT_require(frontierNodes.find(physical_file_id) != frontierNodes.end());

  std::map<SgStatement *, FrontierNode *> *frontierMap =
      frontierNodes[physical_file_id];
  ASSERT_not_null(frontierMap);
  const auto existing = frontierMap->find(statement);
  if (existing == frontierMap->end()) {
    frontierMap->insert(
        std::pair<SgStatement *, FrontierNode *>(statement, frontierNode));
  } else {
    FrontierNode *existingFrontier = existing->second;
    if (existingFrontier == nullptr || existingFrontier->node != statement ||
        existingFrontier->unparseUsingTokenStream !=
            frontierNode->unparseUsingTokenStream ||
        existingFrontier->unparseFromTheAST !=
            frontierNode->unparseFromTheAST) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-frontier]: statement=%p type=%s "
              "received contradictory duplicate frontier modes "
              "(existing-token=%d existing-ast=%d new-token=%d new-ast=%d)\n",
              static_cast<void *>(statement), statement->class_name().c_str(),
              existingFrontier != nullptr &&
                      existingFrontier->unparseUsingTokenStream
                  ? 1
                  : 0,
              existingFrontier != nullptr && existingFrontier->unparseFromTheAST
                  ? 1
                  : 0,
              frontierNode->unparseUsingTokenStream ? 1 : 0,
              frontierNode->unparseFromTheAST ? 1 : 0);
      ROSE_ABORT();
    }
    delete frontierNode;
  }

  ASSERT_require(frontierNodes[physical_file_id]->find(statement) !=
                 frontierNodes[physical_file_id]->end());
}

FrontierNode *FrontierDetectionForTokenStreamMapping::getFrontierNode(
    SgStatement *statement) {
  ASSERT_not_null(statement);
  int physical_file_id =
      requireStatementPhysicalFileId(statement, "get-frontier-node");

  // Make sure that the std::map<SgStatement*,FrontierNode*> is available in the
  // frontierNodes.
  ASSERT_require(frontierNodes.find(physical_file_id) != frontierNodes.end());

  // Make sure that the std::map<SgStatement*,FrontierNode*> has a valid entry
  // for statement.
  ASSERT_require(frontierNodes[physical_file_id]->find(statement) !=
                 frontierNodes[physical_file_id]->end());

  std::map<SgStatement *, FrontierNode *> *frontierMap =
      frontierNodes[physical_file_id];
  ASSERT_not_null(frontierMap);

  FrontierNode *frontierNode = frontierMap->operator[](statement);
  return frontierNode;
}

FrontierNode::FrontierNode(const FrontierNode &X) { operator=(X); }

FrontierNode FrontierNode::operator=(const FrontierNode &X) {
  node = X.node;
  unparseUsingTokenStream = X.unparseUsingTokenStream;
  unparseFromTheAST = X.unparseFromTheAST;
  return *this;
}

void TokenUnparseFrontierFileContext::adoptFrontierNode(
    SgStatement *statement, FrontierNode *frontierNode) {
  ASSERT_not_null(statement);
  ASSERT_not_null(frontierNode);
  ASSERT_require(frontierNode->node == statement);
  const bool inserted = frontierNodes.emplace(statement, frontierNode).second;
  if (!inserted) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-frontier]: statement=%p appears "
            "more than once in an invocation frontier\n",
            static_cast<void *>(statement));
    ROSE_ABORT();
  }
  ownedFrontierNodes.emplace_back(frontierNode);
}

void TokenUnparseFrontierFileContext::markStatementForAstUnparse(
    SgStatement *statement) {
  ASSERT_not_null(statement);
  if (transformationAnalysisComplete) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-frontier]: statement=%p was marked "
            "for AST unparsing after transformation analysis completed\n",
            static_cast<void *>(statement));
    ROSE_ABORT();
  }
  statementsToUnparseFromAst.insert(statement);
}

void TokenUnparseFrontierFileContext::markStatementAsContainingAstUnparse(
    SgStatement *statement) {
  ASSERT_not_null(statement);
  if (transformationAnalysisComplete) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-frontier]: statement=%p was marked "
            "as containing AST-unparsed nodes after transformation analysis "
            "completed\n",
            static_cast<void *>(statement));
    ROSE_ABORT();
  }
  statementsContainingAstUnparse.insert(statement);
}

bool TokenUnparseFrontierFileContext::isStatementMarkedForAstUnparse(
    SgStatement *statement) const {
  ASSERT_not_null(statement);
  return statementsToUnparseFromAst.find(statement) !=
         statementsToUnparseFromAst.end();
}

bool TokenUnparseFrontierFileContext::statementContainsAstUnparse(
    SgStatement *statement) const {
  ASSERT_not_null(statement);
  return statementsContainingAstUnparse.find(statement) !=
         statementsContainingAstUnparse.end();
}

bool TokenUnparseFrontierFileContext::statementRequiresAstUnparse(
    SgStatement *statement) const {
  ASSERT_not_null(statement);
  if (!transformationAnalysisComplete) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-frontier]: statement=%p was "
            "classified before transformation analysis completed\n",
            static_cast<void *>(statement));
    ROSE_ABORT();
  }
  Sg_File_Info *fileInfo = statement->get_file_info();
  if (fileInfo == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-frontier]: statement=%p type=%s has "
            "no file information during transformation classification\n",
            static_cast<void *>(statement), statement->class_name().c_str());
    ROSE_ABORT();
  }
  return fileInfo->isTransformation() ||
         isStatementMarkedForAstUnparse(statement);
}

void TokenUnparseFrontierFileContext::finishTransformationAnalysis() {
  if (transformationAnalysisComplete) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-frontier]: transformation analysis "
            "was completed more than once\n");
    ROSE_ABORT();
  }
  transformationAnalysisComplete = true;
}

TokenUnparseFrontierFileContext &
TokenUnparseFrontierContext::beginFile(SgSourceFile *sourceFile) {
  ASSERT_not_null(sourceFile);
  auto result = files.try_emplace(sourceFile);
  if (!result.second) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-frontier]: file=%s frontier was "
            "built more than once in one unparse invocation\n",
            sourceFile->getFileName().c_str());
    ROSE_ABORT();
  }
  return result.first->second;
}

TokenUnparseFrontierFileContext &
TokenUnparseFrontierContext::file(SgSourceFile *sourceFile) {
  ASSERT_not_null(sourceFile);
  auto found = files.find(sourceFile);
  if (found == files.end()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-frontier]: file=%s has no frontier "
            "in this unparse invocation\n",
            sourceFile->getFileName().c_str());
    ROSE_ABORT();
  }
  return found->second;
}

const TokenUnparseFrontierFileContext &
TokenUnparseFrontierContext::file(SgSourceFile *sourceFile) const {
  ASSERT_not_null(sourceFile);
  auto found = files.find(sourceFile);
  if (found == files.end()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-frontier]: file=%s has no frontier "
            "in this unparse invocation\n",
            sourceFile->getFileName().c_str());
    ROSE_ABORT();
  }
  return found->second;
}

bool TokenUnparseFrontierContext::hasFile(SgSourceFile *sourceFile) const {
  ASSERT_not_null(sourceFile);
  return files.find(sourceFile) != files.end();
}
