// tps : Switching from rose.h to sage3 changed size from 17,7 MB to 7,4MB
#include "sage3basic.h"

#include "CreateSlice.h"
#include "RoseAst.h"

#include <iostream>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
using namespace std;

namespace {
bool canRemoveStatementFromAst(SgStatement *statement) {
  if (statement == NULL) {
    return false;
  }

  // Mutation APIs only support statements that are attached to a statement
  // list.
  if (statement->get_parent() == NULL ||
      isSgStatement(statement->get_parent()) == NULL) {
    return false;
  }

  return SageInterface::isRemovableStatement(statement);
}

void requireExactOwnedStatementChild(SgStatement *statement) {
  ROSE_ASSERT(statement != NULL);
  SgNode *owner = statement->get_parent();
  const std::vector<SgNode *> successors =
      owner != NULL ? owner->get_traversalSuccessorContainer()
                    : std::vector<SgNode *>();
  if (owner == NULL ||
      std::count(successors.begin(), successors.end(), statement) != 1) {
    fprintf(stderr,
            "REX_SLICING_INVARIANT[owned-statement-child]: "
            "statement=%p/%s owner=%p/%s has no exact structural ownership "
            "edge\n",
            static_cast<void *>(statement), statement->class_name().c_str(),
            static_cast<void *>(owner),
            owner != NULL ? owner->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
}

using DeletionNodeSet = std::unordered_set<SgNode *>;

void collectDeletionSubtree(SgStatement *root, DeletionNodeSet &nodes) {
  ROSE_ASSERT(root != NULL && SgNode::isLiveNode(root));
  RoseAst ast(root);
  for (RoseAst::iterator current = ast.begin(); current != ast.end();
       ++current) {
    ROSE_ASSERT(*current != NULL && SgNode::isLiveNode(*current));
    nodes.insert(*current);
  }
}

void closeDeletionBatchOverOwnedFunctionTypeSyntax(DeletionNodeSet &nodes) {
  std::vector<SgFunctionDeclaration *> functions;
  for (SgNode *node : nodes) {
    if (SgFunctionDeclaration *function = isSgFunctionDeclaration(node))
      functions.push_back(function);
  }

  for (SgFunctionDeclaration *function : functions) {
    SgFunctionType *syntaxType = function->get_type_syntax();
    const bool syntaxAvailable = function->get_type_syntax_is_available();
    if (syntaxAvailable != (syntaxType != NULL) ||
        (syntaxType != NULL && (syntaxType == function->get_type() ||
                                syntaxType->get_parent() != function))) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[function-type-syntax-owner]: "
              "function=%p/%s semantic=%p syntax=%p available=%d parent=%p "
              "has no exact declaration-local source type tree\n",
              static_cast<void *>(function), function->class_name().c_str(),
              static_cast<void *>(function->get_type()),
              static_cast<void *>(syntaxType), syntaxAvailable ? 1 : 0,
              static_cast<void *>(syntaxType != NULL ? syntaxType->get_parent()
                                                     : NULL));
      ROSE_ABORT();
    }
    if (syntaxType == NULL)
      continue;

    std::vector<SgNode *> worklist(1, syntaxType);
    while (!worklist.empty()) {
      SgNode *owned = worklist.back();
      worklist.pop_back();
      if (owned == NULL || !SgNode::isLiveNode(owned) ||
          !nodes.insert(owned).second) {
        fprintf(stderr,
                "REX_SLICING_INVARIANT[function-type-syntax-owner]: "
                "function=%p source syntax contains null, dead, duplicate, "
                "or multiply-owned node=%p\n",
                static_cast<void *>(function), static_cast<void *>(owned));
        ROSE_ABORT();
      }
      for (const auto &edge : owned->returnDataMemberPointers()) {
        SgNode *child = edge.first;
        if (child != NULL && child->get_parent() == owned)
          worklist.push_back(child);
      }
    }
  }
}

std::vector<SgNode *>
closeDeletionBatchOverOwnedScopeSupport(DeletionNodeSet &nodes) {
  std::vector<SgScopeStatement *> scopes;
  for (SgNode *node : nodes) {
    if (SgScopeStatement *scope = isSgScopeStatement(node))
      scopes.push_back(scope);
  }

  std::vector<SgNode *> supportNodes;
  std::unordered_set<SgNode *> seen;
  auto includeExactOwnedSupport = [&](SgNode *owner, SgNode *support,
                                      const char *role) {
    if (support == NULL)
      return;
    if (!SgNode::isLiveNode(support) || support->get_parent() != owner ||
        !seen.insert(support).second) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[owned-scope-support]: owner=%p/%s "
              "role=%s support=%p has no exact destructor-owned identity\n",
              static_cast<void *>(owner), owner->class_name().c_str(), role,
              static_cast<void *>(support));
      ROSE_ABORT();
    }
    nodes.insert(support);
    supportNodes.push_back(support);
  };

  for (SgScopeStatement *scope : scopes) {
    SgSymbolTable *symbolTable = scope->get_symbol_table();
    if (symbolTable == NULL) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[owned-scope-support]: scope=%p/%s has "
              "no required symbol table\n",
              static_cast<void *>(scope), scope->class_name().c_str());
      ROSE_ABORT();
    }
    includeExactOwnedSupport(scope, symbolTable, "symbol-table");
    SgTypeTable *typeTable = scope->get_type_table();
    if (typeTable != NULL) {
      includeExactOwnedSupport(scope, typeTable, "type-table");
      SgSymbolTable *typeSymbolTable = typeTable->get_type_table();
      if (typeSymbolTable == NULL) {
        fprintf(stderr,
                "REX_SLICING_INVARIANT[owned-scope-support]: type-table=%p "
                "owned by scope=%p/%s has no internal symbol table\n",
                static_cast<void *>(typeTable), static_cast<void *>(scope),
                scope->class_name().c_str());
        ROSE_ABORT();
      }
      includeExactOwnedSupport(typeTable, typeSymbolTable,
                               "type-table-symbol-table");
    }
  }
  return supportNodes;
}

class ExactParentOwnedDeletionClosure : public ROSE_VisitTraversal {
public:
  explicit ExactParentOwnedDeletionClosure(const DeletionNodeSet &owners)
      : owners_(owners) {}

  void visit(SgNode *node) override {
    if (!SgNode::isLiveNode(node) || owners_.count(node) != 0 ||
        isSgSymbol(node) != NULL || isSgType(node) != NULL) {
      return;
    }
    SgNode *owner = node->get_parent();
    if (owner == NULL || owners_.count(owner) == 0)
      return;
    if (!SgNode::isLiveNode(owner) || !additionsSeen_.insert(node).second) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[parent-owned-deletion-closure]: "
              "node=%p/%s has invalid or duplicate exact owner=%p\n",
              static_cast<void *>(node), node->class_name().c_str(),
              static_cast<void *>(owner));
      ROSE_ABORT();
    }
    additions_.push_back(node);
  }

  std::vector<SgNode *> collect() {
    traverseMemoryPool();
    return additions_;
  }

private:
  const DeletionNodeSet &owners_;
  std::unordered_set<SgNode *> additionsSeen_;
  std::vector<SgNode *> additions_;
};

std::vector<SgNode *>
closeDeletionBatchOverExactParentOwnership(DeletionNodeSet &nodes) {
  std::vector<SgNode *> ownedNodes;
  std::unordered_set<SgNode *> seen;
  while (true) {
    std::vector<SgNode *> additions =
        ExactParentOwnedDeletionClosure(nodes).collect();
    if (additions.empty())
      break;
    for (SgNode *node : additions) {
      SgNode *owner = node->get_parent();
      if (owner == NULL || nodes.count(owner) == 0 ||
          !SgNode::isLiveNode(owner) || !seen.insert(node).second ||
          !nodes.insert(node).second) {
        fprintf(stderr,
                "REX_SLICING_INVARIANT[parent-owned-deletion-closure]: "
                "node=%p/%s did not close exactly over owner=%p\n",
                static_cast<void *>(node), node->class_name().c_str(),
                static_cast<void *>(owner));
        ROSE_ABORT();
      }
      ownedNodes.push_back(node);
    }
  }
  return ownedNodes;
}

struct RetiringSymbol {
  SgSymbolTable *table;
  SgSymbol *symbol;
};

class RetiringSharedTypeParentCollector : public ROSE_VisitTraversal {
public:
  explicit RetiringSharedTypeParentCollector(
      const DeletionNodeSet &deletionNodes)
      : deletionNodes_(deletionNodes) {}

  void visit(SgNode *node) override {
    SgType *type = isSgType(node);
    if (type == NULL || !SgNode::isLiveNode(type) ||
        deletionNodes_.count(type) != 0 ||
        deletionNodes_.count(type->get_parent()) == 0) {
      return;
    }
    if (!seen_.insert(type).second || !SgNode::isLiveNode(type->get_parent())) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[shared-type-parent-retirement]: "
              "type=%p/%s has invalid exact retiring parent=%p\n",
              static_cast<void *>(type), type->class_name().c_str(),
              static_cast<void *>(type->get_parent()));
      ROSE_ABORT();
    }
    types_.push_back(type);
  }

  std::vector<SgType *> collect() {
    traverseMemoryPool();
    return types_;
  }

private:
  const DeletionNodeSet &deletionNodes_;
  std::unordered_set<SgType *> seen_;
  std::vector<SgType *> types_;
};

class RetiringSymbolCollector : public ROSE_VisitTraversal {
public:
  explicit RetiringSymbolCollector(const DeletionNodeSet &deletionNodes)
      : deletionNodes_(deletionNodes) {}

  void visit(SgNode *node) override {
    SgSymbol *symbol = isSgSymbol(node);
    SgSymbolTable *table =
        symbol != NULL ? isSgSymbolTable(symbol->get_parent()) : NULL;
    const bool basisRetires =
        symbol != NULL && deletionNodes_.count(symbol->get_symbol_basis()) != 0;
    const bool tableRetires = table != NULL && deletionNodes_.count(table) != 0;
    if (symbol == NULL || !SgNode::isLiveNode(symbol) ||
        (!basisRetires && !tableRetires) || !seen_.insert(symbol).second) {
      return;
    }

    size_t tableOccurrences = 0;
    if (table != NULL && table->get_table() != NULL) {
      for (const auto &entry : *table->get_table())
        tableOccurrences += entry.second == symbol ? 1 : 0;
    }
    if (table == NULL || !SgNode::isLiveNode(table) || !table->exists(symbol) ||
        tableOccurrences != 1) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[retiring-symbol-owner]: symbol=%p/%s "
              "basis=%p table=%p parent=%p occurrences=%zu has no exact "
              "symbol-table owner\n",
              static_cast<void *>(symbol), symbol->class_name().c_str(),
              static_cast<void *>(symbol->get_symbol_basis()),
              static_cast<void *>(table),
              static_cast<void *>(symbol->get_parent()), tableOccurrences);
      ROSE_ABORT();
    }
    result_.push_back({table, symbol});
  }

  std::vector<RetiringSymbol> collect() {
    traverseMemoryPool();
    return result_;
  }

private:
  const DeletionNodeSet &deletionNodes_;
  std::unordered_set<SgSymbol *> seen_;
  std::vector<RetiringSymbol> result_;
};

std::vector<RetiringSymbol>
collectRetiringSymbols(const DeletionNodeSet &deletionNodes) {
  // Semantic scope is not an ownership oracle. Discover every live symbol
  // whose basis retires or whose exact table itself retires, then require its
  // one concrete table edge before mutating any table.
  return RetiringSymbolCollector(deletionNodes).collect();
}

class ExternalDeletionReferenceProof : public ROSE_VisitTraversal {
public:
  explicit ExternalDeletionReferenceProof(const DeletionNodeSet &candidates)
      : candidates_(candidates) {}

  void visit(SgNode *node) override {
    if (offendingOwner_ != NULL || !SgNode::isLiveNode(node) ||
        candidates_.count(node) != 0) {
      return;
    }
    for (const auto &edge : node->returnDataMemberPointers()) {
      if (edge.first != NULL && candidates_.count(edge.first) != 0) {
        offendingOwner_ = node;
        offendingTarget_ = edge.first;
        offendingEdge_ = edge.second;
        return;
      }
    }
  }

  void requireIsolated() {
    traverseMemoryPool();
    if (offendingOwner_ != NULL) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[isolated-deletion-proof]: external=%p/"
              "%s edge=%s targets=%p/%s in the pending deletion batch\n",
              static_cast<void *>(offendingOwner_),
              offendingOwner_->class_name().c_str(), offendingEdge_.c_str(),
              static_cast<void *>(offendingTarget_),
              offendingTarget_->class_name().c_str());
      ROSE_ABORT();
    }
  }

private:
  const DeletionNodeSet &candidates_;
  SgNode *offendingOwner_ = NULL;
  SgNode *offendingTarget_ = NULL;
  std::string offendingEdge_;
};
} // namespace

void CreateSlice::queueStatementForRemoval(SgStatement *statement) {
  if (statement != NULL) {
    statementsPendingRemoval.push_back(statement);
  }
}

void CreateSlice::applyPendingRemovals() {
  std::unordered_set<SgStatement *> pendingStatements;
  for (SgStatement *statement : statementsPendingRemoval) {
    if (statement != NULL && SgNode::isLiveNode(statement)) {
      if (SgFunctionParameterList *parameters =
              isSgFunctionParameterList(statement)) {
        SgFunctionDeclaration *function =
            isSgFunctionDeclaration(parameters->get_parent());
        if (function == NULL || function->get_parameterList() != parameters) {
          fprintf(stderr,
                  "REX_SLICING_INVARIANT[function-parameter-owner]: "
                  "parameter-list=%p parent=%p is not the exact owned "
                  "parameter surface of one function declaration\n",
                  static_cast<void *>(parameters),
                  static_cast<void *>(parameters->get_parent()));
          ROSE_ABORT();
        }
        fprintf(stderr,
                "REX_SLICING_INVARIANT[queued-function-parameter-list]: "
                "parameter-list=%p was queued despite being an owned "
                "function-declaration child\n",
                static_cast<void *>(parameters));
        ROSE_ABORT();
      }
      pendingStatements.insert(statement);
    }
  }

  // Declaration families are one semantic identity.  A slice may retain a
  // defining declaration through its selected body while the source prototype
  // itself has no selected descendant.  Close the retained set over every
  // function declaration that shares the exact canonical first declaration;
  // deleting only the prototype would leave the retained definition and call
  // symbols pointing into the deletion transaction.
  std::unordered_set<SgProject *> projects;
  for (SgStatement *statement : pendingStatements) {
    if (isSgFunctionDeclaration(statement) == NULL)
      continue;
    SgProject *project = SageInterface::getProject(statement);
    if (project == NULL) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[function-family-project]: pending "
              "function=%p has no exact owning project\n",
              static_cast<void *>(statement));
      ROSE_ABORT();
    }
    projects.insert(project);
  }
  std::unordered_map<SgFunctionDeclaration *,
                     std::vector<SgFunctionDeclaration *>>
      functionFamilies;
  for (SgProject *project : projects) {
    RoseAst ast(project);
    for (RoseAst::iterator current = ast.begin(); current != ast.end();
         ++current) {
      SgFunctionDeclaration *function = isSgFunctionDeclaration(*current);
      if (function == NULL)
        continue;
      SgFunctionDeclaration *first =
          isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
      if (first == NULL || first->get_firstNondefiningDeclaration() != first) {
        fprintf(stderr,
                "REX_SLICING_INVARIANT[function-family-canonical]: "
                "function=%p/%s first=%p has no exact canonical declaration\n",
                static_cast<void *>(function), function->get_name().str(),
                static_cast<void *>(first));
        ROSE_ABORT();
      }
      functionFamilies[first].push_back(function);
    }
  }
  for (const auto &family : functionFamilies) {
    bool hasPendingMember = false;
    bool hasRetainedMember = false;
    for (SgFunctionDeclaration *member : family.second) {
      const bool pending = pendingStatements.count(member) != 0;
      hasPendingMember = hasPendingMember || pending;
      hasRetainedMember = hasRetainedMember || !pending;
    }
    if (!hasPendingMember || !hasRetainedMember)
      continue;
    for (SgFunctionDeclaration *member : family.second)
      pendingStatements.erase(member);
  }

  // A defining declaration must be removed before its separately owned
  // canonical first declaration.  removeStatement then severs the family from
  // the retained symbol basis before either detached subtree is destroyed.
  std::vector<SgStatement *> orderedStatements;
  orderedStatements.reserve(pendingStatements.size());
  std::unordered_set<SgStatement *> scheduledStatements;
  for (SgStatement *statement : statementsPendingRemoval) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(statement);
    SgDeclarationStatement *first =
        declaration != NULL
            ? isSgDeclarationStatement(
                  declaration->get_firstNondefiningDeclaration())
            : NULL;
    if (declaration != NULL &&
        declaration->get_definingDeclaration() == declaration &&
        first != NULL && first != declaration &&
        pendingStatements.count(first) != 0 &&
        scheduledStatements.insert(statement).second) {
      orderedStatements.push_back(statement);
    }
  }
  for (SgStatement *statement : statementsPendingRemoval) {
    if (statement != NULL && pendingStatements.count(statement) != 0 &&
        scheduledStatements.insert(statement).second) {
      orderedStatements.push_back(statement);
    }
  }

  // Only the outermost pending statements are deletion roots.  Descendant
  // statements remain structurally owned by that root and are destroyed by
  // the same transaction; detaching them separately would create invalid
  // intermediate trees and duplicate deletion roots.
  std::vector<SgStatement *> deletionRoots;
  for (SgStatement *candidate : orderedStatements) {
    bool hasPendingAncestor = false;
    std::unordered_set<SgNode *> visited;
    for (SgNode *ancestor = candidate != NULL ? candidate->get_parent() : NULL;
         ancestor != NULL && visited.insert(ancestor).second;
         ancestor = ancestor->get_parent()) {
      SgStatement *ancestorStatement = isSgStatement(ancestor);
      if (ancestorStatement != NULL &&
          pendingStatements.count(ancestorStatement) != 0) {
        hasPendingAncestor = true;
        break;
      }
    }
    if (!hasPendingAncestor)
      deletionRoots.push_back(candidate);
  }

  DeletionNodeSet deletionNodes;
  for (SgStatement *candidate : deletionRoots)
    collectDeletionSubtree(candidate, deletionNodes);
  closeDeletionBatchOverOwnedFunctionTypeSyntax(deletionNodes);
  const std::vector<SgNode *> ownedScopeSupportNodes =
      closeDeletionBatchOverOwnedScopeSupport(deletionNodes);
  const std::vector<SgNode *> exactParentOwnedNodes =
      closeDeletionBatchOverExactParentOwnership(deletionNodes);
  const std::vector<SgType *> retiringSharedTypeParents =
      RetiringSharedTypeParentCollector(deletionNodes).collect();
  std::vector<RetiringSymbol> retiringSymbols =
      collectRetiringSymbols(deletionNodes);
  for (const RetiringSymbol &retiring : retiringSymbols)
    deletionNodes.insert(retiring.symbol);

  // Detach the complete batch first. Declaration-family mutation is therefore
  // finished before isolation is proved, and no nested deletion can observe a
  // half-severed defining/nondefining chain.
  for (SgStatement *candidate : deletionRoots) {
    ROSE_ASSERT(candidate != NULL && SgNode::isLiveNode(candidate));
    if (SgDeclarationStatement *declaration =
            isSgDeclarationStatement(candidate)) {
      SgDeclarationStatement *defining =
          isSgDeclarationStatement(declaration->get_definingDeclaration());
      if (declaration->get_firstNondefiningDeclaration() == declaration &&
          defining != NULL && defining != declaration &&
          SgNode::isLiveNode(defining) &&
          pendingStatements.count(defining) == 0) {
        fprintf(stderr,
                "REX_SLICING_INVARIANT[pending-removal-family]: canonical=%p/"
                "%s retains defining declaration=%p outside the slice "
                "removal transaction\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(),
                static_cast<void *>(defining));
        ROSE_ABORT();
      }
    }
    if (!canRemoveStatementFromAst(candidate)) {
      SgDeclarationStatement *declaration = isSgDeclarationStatement(candidate);
      fprintf(stderr,
              "REX_SLICING_INVARIANT[pending-removal-owner]: statement=%p/%s "
              "parent=%p/%s first=%p defining=%p is not attached to one "
              "removable statement list\n",
              static_cast<void *>(candidate), candidate->class_name().c_str(),
              static_cast<void *>(candidate->get_parent()),
              candidate->get_parent() != NULL
                  ? candidate->get_parent()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(
                  declaration != NULL
                      ? declaration->get_firstNondefiningDeclaration()
                      : NULL),
              static_cast<void *>(declaration != NULL
                                      ? declaration->get_definingDeclaration()
                                      : NULL));
      ROSE_ABORT();
    }

    SageInterface::removeStatement(candidate);
    if (candidate->get_parent() != NULL) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[pending-removal-detachment]: "
              "statement=%p/%s retained parent=%p after exact removal\n",
              static_cast<void *>(candidate), candidate->class_name().c_str(),
              static_cast<void *>(candidate->get_parent()));
      ROSE_ABORT();
    }
  }

  // Retire exact symbol-table ownership before proving isolation.  The symbol
  // nodes remain live during the proof, so any retained reference expression
  // or semantic edge still targeting them is detected as an external edge.
  for (const RetiringSymbol &retiring : retiringSymbols) {
    SgSymbolTable *table = retiring.table;
    if (table == NULL || retiring.symbol->get_parent() != table ||
        !table->exists(retiring.symbol)) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[retiring-symbol-detachment]: symbol=%p/"
              "%s table=%p lost its exact table edge\n",
              static_cast<void *>(retiring.symbol),
              retiring.symbol->class_name().c_str(),
              static_cast<void *>(table));
      ROSE_ABORT();
    }
    // Detach only this exact table edge. Scope-level removal also mutates and
    // deletes cross-file alias symbols as a side effect, which makes a
    // preflighted deletion batch order-dependent. Those aliases are collected
    // independently by their own exact table/basis edges above.
    table->remove(retiring.symbol);
    if (retiring.symbol->get_parent() != NULL ||
        table->exists(retiring.symbol)) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[retiring-symbol-detachment]: symbol=%p/"
              "%s remained published after exact removal\n",
              static_cast<void *>(retiring.symbol),
              retiring.symbol->class_name().c_str());
      ROSE_ABORT();
    }
  }

  // Shared/interned type nodes are semantic references, never deletion-batch
  // children. Their legacy parent field is a non-owning construction anchor;
  // detach that exact back-reference before its declaration retires so it
  // cannot become a dangling edge.
  for (SgType *type : retiringSharedTypeParents) {
    if (type == NULL || !SgNode::isLiveNode(type) ||
        deletionNodes.count(type->get_parent()) == 0) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[shared-type-parent-retirement]: "
              "type=%p lost its exact retiring parent before detachment\n",
              static_cast<void *>(type));
      ROSE_ABORT();
    }
    type->set_parent(NULL);
    if (type->get_parent() != NULL) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[shared-type-parent-retirement]: "
              "type=%p retained parent=%p after exact detachment\n",
              static_cast<void *>(type),
              static_cast<void *>(type->get_parent()));
      ROSE_ABORT();
    }
  }

  ExternalDeletionReferenceProof(deletionNodes).requireIsolated();

  for (SgStatement *candidate : deletionRoots) {
    SageInterface::deleteAST(
        candidate, SageInterface::DeleteAstMode::kSkipExternalReferences);
    if (SgNode::isLiveNode(candidate)) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[pending-removal-deletion]: "
              "detached statement=%p remains live after slice deletion\n",
              static_cast<void *>(candidate));
      ROSE_ABORT();
    }
  }
  auto retireNontraversalOwnedNodes = [&](const std::vector<SgNode *> &nodes,
                                          const char *contract) {
    for (SgNode *node : nodes) {
      if (!SgNode::isLiveNode(node))
        continue;
      SgNode *owner = node->get_parent();
      if (owner == NULL || deletionNodes.count(owner) == 0) {
        fprintf(stderr,
                "REX_SLICING_INVARIANT[%s]: node=%p/%s retained unexpected "
                "owner=%p after deletion-root retirement\n",
                contract, static_cast<void *>(node), node->class_name().c_str(),
                static_cast<void *>(owner));
        ROSE_ABORT();
      }
      node->set_parent(NULL);
      SageInterface::deleteAST(
          node, SageInterface::DeleteAstMode::kSkipExternalReferences);
      if (SgNode::isLiveNode(node)) {
        fprintf(stderr,
                "REX_SLICING_INVARIANT[%s]: detached node=%p remained live "
                "after exact retirement\n",
                contract, static_cast<void *>(node));
        ROSE_ABORT();
      }
    }
  };
  // These ownership edges are deliberately outside ordinary traversal. The
  // generic subtree deleter cannot see them, so the transaction must detach
  // and retire each preflighted node explicitly after its deletion root.
  retireNontraversalOwnedNodes(exactParentOwnedNodes, "parent-owned-deletion");
  retireNontraversalOwnedNodes(ownedScopeSupportNodes,
                               "owned-scope-support-deletion");
  for (SgNode *support : ownedScopeSupportNodes) {
    if (SgNode::isLiveNode(support)) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[owned-scope-support-deletion]: "
              "support=%p/%s remained live after its owning scope deletion\n",
              static_cast<void *>(support), support->class_name().c_str());
      ROSE_ABORT();
    }
  }
  for (SgNode *owned : exactParentOwnedNodes) {
    if (SgNode::isLiveNode(owned)) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[parent-owned-deletion]: owned=%p/%s "
              "remained live after exact owner deletion\n",
              static_cast<void *>(owned), owned->class_name().c_str());
      ROSE_ABORT();
    }
  }
  for (SgType *type : retiringSharedTypeParents) {
    if (!SgNode::isLiveNode(type) || type->get_parent() != NULL) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[shared-type-parent-retirement]: "
              "shared type=%p was deleted or rebound with its declaration\n",
              static_cast<void *>(type));
      ROSE_ABORT();
    }
  }
  for (const RetiringSymbol &retiring : retiringSymbols) {
    if (!SgNode::isLiveNode(retiring.symbol)) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[retiring-symbol-deletion]: symbol=%p "
              "was deleted outside the exact retirement transaction\n",
              static_cast<void *>(retiring.symbol));
      ROSE_ABORT();
    }
    delete retiring.symbol;
  }
  statementsPendingRemoval.clear();
}

void printSgNode(SgNode *node) {
  cout << "\"";
  if (isSgInitializedName(node))
    cout << isSgInitializedName(node)->get_qualified_name().getString();
  else if (!isSgPragma(node))
    cout << node->unparseToString();
  cout << "\" of class " << node->class_name();
}

BooleanSafeKeeper
CreateSlice::evaluateInheritedAttribute(SgNode *node,
                                        BooleanSafeKeeper partOfSlice) {

  if (isSgFile(node)) {
    currentFile = isSgFile(node);
  }
  if (isSgBasicBlock(node) || isSgGlobal(node)) {
    delayedRemoveListStack.push(std::list<SgNode *>());
  }
  // A selected node retains its complete owned subtree. Class and typedef
  // declarations are also retained by the slicer's explicit type policy, so
  // their definitions, access labels, and members must inherit that decision
  // instead of being pruned independently.  The frontend auxiliary declaration
  // list is semantic support rather than a source-emission surface; source
  // slicing must never queue its declarations for statement-list mutation.
  partOfSlice.boolean = partOfSlice.boolean || _toSave.count(node) != 0 ||
                        isSgClassDeclaration(node) != NULL ||
                        isSgTypedefDeclaration(node) != NULL ||
                        isSgAuxiliaryDeclarationList(node) != NULL;
  return partOfSlice;
}

BooleanSafeKeeper
CreateSlice::evaluateSynthesizedAttribute(SgNode *node,
                                          BooleanSafeKeeper inherited,
                                          SubTreeSynthesizedAttributes atts) {
  if (isSgFile(node)) {
    currentFile = NULL;
  }
  /*
          if (isSgStatement(node))
                  cout << "processing statement ";
          else
                  cout <<"processing node ";

    printSgNode(node);
          cout <<endl;
  */

  BooleanSafeKeeper partOfSlice = inherited;
  //      partOfSlice=BooleanSafeKeeper(false);
  // calcualte the synth-val of this node
  // if any of the children of this node is in the slice, this node must also be
  // in the slice
  int count = 0;
  std::vector<std::pair<SgNode *, std::string>> mapping =
      node->returnDataMemberPointers();
  //              cout <<"\t synth-attrib count "<<atts.size()<<"\t mapping
  //              count "<<mapping.size()<<endl;
  for (SubTreeSynthesizedAttributes::iterator i = atts.begin(); i != atts.end();
       i++) {
    /*              if ((*i).boolean)
                    {
            //              cout <<"\t"<<count<<" is true"<<endl;
//                              SgNode * first=((mapping[count]).first);
    //                      if (first!=NULL)
    //                      cout
<<"\t"<<((mapping[count]).first)->class_name()<<endl;
                    }
                    */
    count++;
    partOfSlice.boolean |= (*i).boolean;
  }

  // if this is a bb or gb check for delayed decls
  if (isSgBasicBlock(node) || isSgGlobal(node)) {
    //              cout <<"current scope: >"<<node->unparseToString()<<"< of
    //              class "<<node->class_name()<<endl;
    bool deleteDelayedNode;
    Rose_STL_Container<SgNode *> varUseList =
        NodeQuery::querySubTree(node, V_SgVarRefExp);
    auto variableDeclarationIsUsed = [&](SgVariableDeclaration *declaration) {
      ROSE_ASSERT(declaration != NULL);
      std::unordered_set<SgInitializedName *> declaredNames;
      for (SgInitializedName *name : declaration->get_variables()) {
        if (name == NULL || name->get_parent() != declaration ||
            !declaredNames.insert(name).second) {
          fprintf(stderr,
                  "REX_SLICING_INVARIANT[variable-declaration-membership]: "
                  "declaration=%p has null, duplicate, or non-owned "
                  "initialized-name=%p\n",
                  static_cast<void *>(declaration), static_cast<void *>(name));
          ROSE_ABORT();
        }
      }
      if (declaredNames.empty()) {
        fprintf(stderr,
                "REX_SLICING_INVARIANT[variable-declaration-membership]: "
                "declaration=%p has no initialized names\n",
                static_cast<void *>(declaration));
        ROSE_ABORT();
      }
      for (SgNode *use : varUseList) {
        SgVarRefExp *reference = isSgVarRefExp(use);
        ROSE_ASSERT(reference != NULL);
        SgVariableSymbol *symbol = reference->get_symbol();
        if (symbol == NULL || symbol->get_declaration() == NULL) {
          fprintf(stderr,
                  "REX_SLICING_INVARIANT[variable-reference-symbol]: "
                  "reference=%p has no exact variable declaration\n",
                  static_cast<void *>(reference));
          ROSE_ABORT();
        }
        if (declaredNames.count(symbol->get_declaration()) != 0)
          return true;
      }
      return false;
    };
    std::unordered_set<SgDeclarationGroupStatement *> resolvedGroups;
    // this is a basic block
    // check if any of the nodes form  delayed remove list must be keept
    for (std::list<SgNode *>::iterator cand =
             delayedRemoveListStack.top().begin();
         cand != delayedRemoveListStack.top().end(); cand++) {
      deleteDelayedNode = true;
      // the list is not empty
      if (SgDeclarationGroupStatement *group =
              isSgDeclarationGroupStatement(*cand)) {
        group->validate();
        if (!resolvedGroups.insert(group).second)
          continue;
        for (SgDeclarationStatement *member : group->get_declarations()) {
          if (SgVariableDeclaration *variable =
                  isSgVariableDeclaration(member)) {
            if (variableDeclarationIsUsed(variable)) {
              deleteDelayedNode = false;
              break;
            }
          } else if (isSgTypedefDeclaration(member) != NULL) {
            // Typedef declarations are retained by the slicer's existing
            // semantic policy, so their atomic source group is retained too.
            deleteDelayedNode = false;
            break;
          } else if (isSgFunctionDeclaration(member) == NULL) {
            fprintf(stderr,
                    "REX_SLICING_INVARIANT[declaration-group-member]: "
                    "group=%p has unsupported delayed member=%p/%s\n",
                    static_cast<void *>(group), static_cast<void *>(member),
                    member != NULL ? member->class_name().c_str() : "<null>");
            ROSE_ABORT();
          }
        }
        if (deleteDelayedNode && currentFile &&
            group->get_file_info()->isSameFile(currentFile)) {
          queueStatementForRemoval(group);
        }
      } else if (isSgVariableDeclaration(*cand)) {
        //                              cout <<"delayed varDecl
        //                              >"<<(*cand)->unparseToString()<<"<"<<endl;
        SgVariableDeclaration *decl = isSgVariableDeclaration(*cand);
        deleteDelayedNode = !variableDeclarationIsUsed(decl);
        if (deleteDelayedNode) {
          //                      cout <<"\t*found no use, deleting"<<endl;
          // check if the node is from the sourcefile or goes to the source
          // file, if so kill it make shure to delete only stuff, the belongs to
          // an unparsed file
          //                                              if
          //                                              ((*cand)->get_file_info
          //                                              ()->isOutputInCodeGeneration
          //                                              ())
          if (currentFile &&
              (*cand)->get_file_info()->isSameFile(currentFile)) {
            SgStatement *candidateStatement = isSgStatement(*cand);
            // Defer AST mutation until traversal finishes to keep successor
            // container indexing stable.
            queueStatementForRemoval(candidateStatement);
            //                                      delete
            //                                      (*cand);
          }
        }
      } else if (isSgTypedefDeclaration(*cand) || isSgClassDeclaration(*cand)) {
        //                      cout <<"delayed struct/type:
        //                      "<<(*cand)->unparseToString()<<endl;
        //      cerr <<"CreateSlice.C:evaluateSynthesizedAttribute does not
        //      maintain delayed deleting for structs and typedefs, every struct
        //      and type will remain within the slice"<<endl;
      }
    }
    delayedRemoveListStack.top().clear();
    delayedRemoveListStack.pop();
  }

  // this is a statement, if any of its child-nodes is in the slice, keep it,
  // else discard id
  if (isSgStatement(node)) {

    // if any of its sythesized attributes is true, keep is, else dicard
    if (!partOfSlice.boolean) {
      if (SgClassDefinition *definition = isSgClassDefinition(node)) {
        SgClassDeclaration *declaration =
            isSgClassDeclaration(definition->get_parent());
        if (declaration == NULL ||
            declaration->get_definition() != definition) {
          fprintf(stderr,
                  "REX_SLICING_INVARIANT[class-definition-owner]: "
                  "definition=%p parent=%p is not the exact definition edge "
                  "of one class declaration\n",
                  static_cast<void *>(definition),
                  static_cast<void *>(definition->get_parent()));
          ROSE_ABORT();
        }
        // A class definition is an owned declaration child, not a statement
        // list entry. The declaration's delayed-retention rule therefore owns
        // the complete declaration/definition surface.
      } else if (SgFunctionDefinition *definition =
                     isSgFunctionDefinition(node)) {
        SgFunctionDeclaration *declaration =
            isSgFunctionDeclaration(definition->get_parent());
        if (declaration == NULL ||
            declaration->get_definition() != definition) {
          fprintf(stderr,
                  "REX_SLICING_INVARIANT[function-definition-owner]: "
                  "definition=%p parent=%p is not the exact definition edge "
                  "of one function declaration\n",
                  static_cast<void *>(definition),
                  static_cast<void *>(definition->get_parent()));
          ROSE_ABORT();
        }
        // Function definitions are likewise declaration-owned subtrees.  The
        // declaration-family retention closure decides whether the complete
        // function survives; the definition is never detached independently.
      } else if (isSgVariableDefinition(node)) {
        //                                      cout <<" is varDef"<<endl;
      } else if (isSgVariableDeclaration(node)) {
        //                                      cout <<" is varDec"<<endl;
        // remove initialization
        SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
        ROSE_ASSERT(declaration != NULL);
        for (SgInitializedName *name : declaration->get_variables()) {
          if (name == NULL || name->get_parent() != declaration) {
            fprintf(
                stderr,
                "REX_SLICING_INVARIANT[variable-declaration-owner]: "
                "declaration=%p contains initialized-name=%p with "
                "unexpected parent=%p\n",
                static_cast<void *>(declaration), static_cast<void *>(name),
                static_cast<void *>(name != NULL ? name->get_parent() : NULL));
            ROSE_ABORT();
          }
          SgInitializer *initializer = name->get_initializer();
          if (initializer == NULL)
            continue;
          if (initializer->get_parent() != name) {
            fprintf(stderr,
                    "REX_SLICING_INVARIANT[initializer-owner]: "
                    "initialized-name=%p initializer=%p/%s has unexpected "
                    "parent=%p\n",
                    static_cast<void *>(name), static_cast<void *>(initializer),
                    initializer->class_name().c_str(),
                    static_cast<void *>(initializer->get_parent()));
            ROSE_ABORT();
          }
          name->set_initializer(NULL);
          initializer->set_parent(NULL);
          SageInterface::deleteAST(
              initializer, SageInterface::DeleteAstMode::kRequireIsolated);
          if (SgNode::isLiveNode(initializer)) {
            fprintf(stderr,
                    "REX_SLICING_INVARIANT[initializer-deletion]: detached "
                    "initializer=%p remains live\n",
                    static_cast<void *>(initializer));
            ROSE_ABORT();
          }
        }
        // check if the variable is used lateron
        //  but remove the initialisation
        if (SgDeclarationGroupStatement *group =
                isSgDeclarationGroupStatement(node->get_parent())) {
          group->validate();
          if (std::count(group->get_declarations().begin(),
                         group->get_declarations().end(),
                         isSgVariableDeclaration(node)) != 1) {
            fprintf(stderr,
                    "REX_SLICING_INVARIANT[declaration-group-owner]: "
                    "variable declaration=%p has no exact group membership\n",
                    static_cast<void *>(node));
            ROSE_ABORT();
          }
        } else {
          delayedRemoveListStack.top().push_back(node);
        }
      } else if (SgDeclarationGroupStatement *group =
                     isSgDeclarationGroupStatement(node)) {
        group->validate();
        delayedRemoveListStack.top().push_back(group);
      } else if (isSgTypedefDeclaration(node) || isSgClassDeclaration(node)) {
        // POSTPONE DElete
        //                                      cout <<" is typedef or
        //                                      struct"<<endl;
        if (SgDeclarationGroupStatement *group =
                isSgDeclarationGroupStatement(node->get_parent())) {
          group->validate();
          if (std::count(group->get_declarations().begin(),
                         group->get_declarations().end(),
                         isSgDeclarationStatement(node)) != 1) {
            fprintf(stderr,
                    "REX_SLICING_INVARIANT[declaration-group-owner]: "
                    "declaration=%p/%s has no exact group membership\n",
                    static_cast<void *>(node), node->class_name().c_str());
            ROSE_ABORT();
          }
        } else {
          delayedRemoveListStack.top().push_back(node);
        }
      } else if (SgDeclarationStatement *declaration =
                     isSgDeclarationStatement(node)) {
        SgDeclarationGroupStatement *group =
            isSgDeclarationGroupStatement(declaration->get_parent());
        if (group == NULL) {
          if (currentFile && node->get_file_info()->isSameFile(currentFile)) {
            if (canRemoveStatementFromAst(declaration)) {
              queueStatementForRemoval(declaration);
            } else {
              // Parameter lists and other declaration-shaped syntax support
              // are structural children, not statement-list declarations.
              // Their owning declaration controls the complete subtree.
              requireExactOwnedStatementChild(declaration);
            }
          }
        } else {
          group->validate();
          if (std::count(group->get_declarations().begin(),
                         group->get_declarations().end(), declaration) != 1) {
            fprintf(stderr,
                    "REX_SLICING_INVARIANT[declaration-group-owner]: "
                    "declaration=%p/%s has no exact group membership\n",
                    static_cast<void *>(declaration),
                    declaration->class_name().c_str());
            ROSE_ABORT();
          }
        }
      } else {
        // make shure to delete only stuff, the belongs to an unparsed file
        //                                              if (node->get_file_info
        //                                              ()->isOutputInCodeGeneration
        //                                              ())
        if (currentFile && node->get_file_info()->isSameFile(currentFile)) {
          SgStatement *currentStatement = isSgStatement(node);
          if (canRemoveStatementFromAst(currentStatement)) {
            // Defer AST mutation until traversal finishes to keep successor
            // container indexing stable.
            queueStatementForRemoval(currentStatement);
          } else {
            requireExactOwnedStatementChild(currentStatement);
            // A non-list statement is inseparable syntax owned by its parent
            // (for example a function body, loop test, or branch body).  Its
            // nearest removable owner decides the complete subtree outcome.
          }
        }
        //      delete(node);
      }
    }
  }

  //              cout <<"\tinherited:"<<inherited.boolean<<endl;
  //              cout <<"\tsynthesized:"<<partOfSlice.boolean<<endl;
  //              return BooleanSafeKeeper(false);
  return partOfSlice;

  // if set::count(node)>0
  if (_toSave.count(
          node)) { /*
                                  if (isSgStatement(node))
                                  {
                                  SgPragmaDeclaration *pr =
                                  new SgPragmaDeclaration(Sg_File_Info::
                                  generateDefaultFileInfoForTransformationNode
                                  (), new SgPragma("slice"));

              ROSE_ASSERT(pr != NULL);
              LowLevelRewrite::insert(isSgStatement(node), isSgStatement(pr));
              _toSave.insert(pr);

          }*/
    partOfSlice = true;
  } else {
    for (SubTreeSynthesizedAttributes::iterator i = atts.begin();
         i != atts.end(); i++) {
      if ((*i).boolean == true) {
        partOfSlice = true;
        break;
      }
    }
  }

  if (!partOfSlice.boolean && isSgStatement(node)) {
    //    LowLevelRewrite::remove(isSgStatement(node));
  }
  /*
     if (isSgLocatedNode(node) && !partOfSlice) { Sg_File_Info * file_info =
     node->get_file_info();

     // file_info->unsetCompilerGeneratedNodeToBeUnparsed(); //
     file_info->setCompilerGenerated(); } */

  return partOfSlice;
}
