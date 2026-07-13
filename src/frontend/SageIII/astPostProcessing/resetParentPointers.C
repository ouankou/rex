#include "sage3basic.h"

#include "astPostProcessing.h"
#include "resetParentPointers.h"

#include <set>

using namespace std;

namespace {

[[noreturn]] void ownershipFailure(const char *context, const SgNode *node,
                                   const SgNode *expected, const SgNode *actual,
                                   size_t edges);

thread_local const SgNode *freshOwnershipBoundary = nullptr;

class FreshOwnershipBoundaryGuard {
public:
  explicit FreshOwnershipBoundaryGuard(const SgNode *boundary)
      : previous(freshOwnershipBoundary) {
    if (boundary == nullptr || previous != nullptr) {
      ownershipFailure("fresh-transaction-boundary", boundary, nullptr,
                       boundary != nullptr ? boundary->get_parent() : nullptr,
                       0);
    }
    freshOwnershipBoundary = boundary;
  }

  ~FreshOwnershipBoundaryGuard() { freshOwnershipBoundary = previous; }

private:
  const SgNode *previous;
};

[[noreturn]] void ownershipFailure(const char *context, const SgNode *node,
                                   const SgNode *expected, const SgNode *actual,
                                   size_t edges) {
  const SgInitializedName *initialized = isSgInitializedName(node);
  fprintf(stderr,
          "REX_AST_INVARIANT[%s]: node=%p type=%s expected-parent=%p "
          "type=%s actual-parent=%p type=%s direct-edges=%zu",
          context, static_cast<const void *>(node),
          node != nullptr ? node->sage_class_name() : "<null>",
          static_cast<const void *>(expected),
          expected != nullptr ? expected->sage_class_name() : "<null>",
          static_cast<const void *>(actual),
          actual != nullptr ? actual->sage_class_name() : "<null>", edges);
  if (initialized != nullptr) {
    fprintf(stderr, " name=%s", initialized->get_name().str());
  }
  fputc('\n', stderr);
  ROSE_ABORT();
}

size_t countDirectOwnerEdges(const SgNode *owner, const SgNode *child) {
  if (owner == nullptr || child == nullptr) {
    return 0;
  }

  size_t edges = 0;
  for (const auto &successor : owner->returnDataMemberPointers()) {
    if (successor.first == child) {
      ++edges;
    }
  }
  return edges;
}

void requireExactParent(const SgNode *child, const SgNode *owner,
                        const char *context, bool requireDirectEdge = true) {
  if (child == nullptr || owner == nullptr) {
    ownershipFailure(context, child, owner,
                     child != nullptr ? child->get_parent() : nullptr, 0);
  }

  const size_t edges = countDirectOwnerEdges(owner, child);
  if (child->get_parent() != owner || (requireDirectEdge && edges != 1)) {
    ownershipFailure(context, child, owner, child->get_parent(), edges);
  }
}

void requirePublishedParent(const SgNode *node, const char *context) {
  if (node == nullptr || node->get_parent() == nullptr) {
    ownershipFailure(context, node, nullptr,
                     node != nullptr ? node->get_parent() : nullptr, 0);
  }

  const SgNode *owner = node->get_parent();
  const size_t edges = countDirectOwnerEdges(owner, node);
  if (edges != 1) {
    ownershipFailure(context, node, owner, owner, edges);
  }
}

void requireFileInfoOwner(const SgNode *node, const Sg_File_Info *info,
                          const char *context) {
  if (info != nullptr && info->get_parent() != node) {
    ownershipFailure(context, info, node, info->get_parent(),
                     countDirectOwnerEdges(node, info));
  }
}

void requireDeclarationFileInfoOwner(const SgDeclarationStatement *declaration,
                                     const Sg_File_Info *info,
                                     const char *context) {
  if (info == nullptr || info->get_parent() != declaration) {
    ownershipFailure(context, info, declaration,
                     info != nullptr ? info->get_parent() : nullptr,
                     countDirectOwnerEdges(declaration, info));
  }
}

void validateSymbolTable(SgScopeStatement *scope) {
  if (scope == nullptr) {
    ownershipFailure("symbol-table-scope", nullptr, nullptr, nullptr, 0);
  }

  SgSymbolTable *table = scope->get_symbol_table();
  requireExactParent(table, scope, "symbol-table-owner", false);

  SgSymbolTable::BaseHashType *entries = table->get_table();
  if (entries == nullptr) {
    ownershipFailure("symbol-table-storage", table, scope, table->get_parent(),
                     0);
  }

  std::map<SgSymbol *, size_t> occurrences;
  for (const auto &entry : *entries) {
    SgSymbol *symbol = entry.second;
    if (symbol == nullptr) {
      ownershipFailure("null-symbol-entry", nullptr, table, nullptr, 0);
    }
    requireExactParent(symbol, table, "symbol-table-entry-owner", false);
    ++occurrences[symbol];
  }

  for (const auto &entry : occurrences) {
    if (entry.second != 1) {
      ownershipFailure("duplicate-symbol-entry", entry.first, table,
                       entry.first->get_parent(), entry.second);
    }
  }
}

void validateSymbol(SgSymbol *symbol, const char *context) {
  if (symbol == nullptr) {
    ownershipFailure(context, nullptr, nullptr, nullptr, 0);
  }

  SgSymbolTable *table = isSgSymbolTable(symbol->get_parent());
  if (table == nullptr || table->get_table() == nullptr) {
    ownershipFailure(context, symbol, table, symbol->get_parent(), 0);
  }

  size_t occurrences = 0;
  for (const auto &entry : *table->get_table()) {
    if (entry.second == symbol) {
      ++occurrences;
    }
  }
  if (occurrences != 1) {
    ownershipFailure(context, symbol, table, symbol->get_parent(), occurrences);
  }
}

void validateOptionalDeclarationChainMember(SgDeclarationStatement *declaration,
                                            const char *context) {
  if (declaration == nullptr) {
    return;
  }
  requirePublishedParent(declaration, context);
}

void validateNamedTypeDeclaration(SgType *type, const char *context) {
  if (type == nullptr) {
    ownershipFailure(context, nullptr, nullptr, nullptr, 0);
  }

  SgType *baseType = type->findBaseType();
  SgNamedType *namedType = isSgNamedType(baseType);
  if (namedType == nullptr) {
    return;
  }

  SgDeclarationStatement *declaration = namedType->get_declaration();
  requirePublishedParent(declaration, context);

  if (SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(declaration)) {
    if (SgClassDefinition *definition = classDeclaration->get_definition()) {
      SgClassDeclaration *definingDeclaration =
          isSgClassDeclaration(classDeclaration->get_definingDeclaration());
      if (definingDeclaration == nullptr) {
        ownershipFailure("class-definition-canonical-owner", definition,
                         classDeclaration, definition->get_parent(),
                         countDirectOwnerEdges(classDeclaration, definition));
      }
      requireExactParent(definition, definingDeclaration,
                         "class-definition-owner");
    }
  }
}

void validateTraversalEdge(SgNode *node, SgNode *traversalParent) {
  if (isSgType(node) != nullptr) {
    return;
  }

  if (SgSymbol *symbol = isSgSymbol(node)) {
    validateSymbol(symbol, "traversed-symbol-owner");
    return;
  }

  if (traversalParent == nullptr) {
    if (isSgProject(node) == nullptr && isSgFile(node) == nullptr) {
      ownershipFailure("detached-validation-root", node, nullptr,
                       node->get_parent(), 0);
    }
    if (node->get_parent() != nullptr) {
      ownershipFailure("root-parent", node, nullptr, node->get_parent(), 0);
    }
    return;
  }

  const size_t directEdges = countDirectOwnerEdges(traversalParent, node);
  if (directEdges != 0) {
    requireExactParent(node, traversalParent, "traversal-owner");
  } else {
    // Some ROSE traversals expose reference edges (for example, declarations
    // referenced by named types).  A reference must never be used to infer a
    // new owner; the referenced node must already have one exact owner.
    requirePublishedParent(node, "referenced-node-owner");
  }
}

} // namespace

void ValidateParentPointers::traceBackToRoot(SgNode *node) {
  if (node == nullptr) {
    ownershipFailure("trace-null", nullptr, nullptr, nullptr, 0);
  }

  std::set<SgNode *> path;
  SgNode *root = node;
  while (root->get_parent() != nullptr) {
    if (!path.insert(root).second) {
      ownershipFailure("parent-cycle", root, nullptr, root->get_parent(), 0);
    }
    root = root->get_parent();
  }

  if (root != freshOwnershipBoundary && isSgProject(root) == nullptr &&
      isSgFile(root) == nullptr) {
    ownershipFailure("unreachable-root", node, nullptr, root, 0);
  }
}

void ValidateParentPointers::validateParentPointersInDeclaration(
    SgDeclarationStatement *declaration, SgNode *inputParent) {
  if (countDirectOwnerEdges(inputParent, declaration) == 1) {
    requireExactParent(declaration, inputParent, "declaration-owner");
  } else {
    requirePublishedParent(declaration, "referenced-declaration-owner");
  }
  requireDeclarationFileInfoOwner(declaration,
                                  declaration->get_startOfConstruct(),
                                  "declaration-start-info-owner");
  requireDeclarationFileInfoOwner(declaration,
                                  declaration->get_endOfConstruct(),
                                  "declaration-end-info-owner");

  validateOptionalDeclarationChainMember(declaration->get_definingDeclaration(),
                                         "defining-declaration-owner");
  validateOptionalDeclarationChainMember(
      declaration->get_firstNondefiningDeclaration(),
      "first-nondefining-declaration-owner");

  if (SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(declaration)) {
    if (SgClassDefinition *definition = classDeclaration->get_definition()) {
      SgClassDeclaration *definingDeclaration =
          isSgClassDeclaration(classDeclaration->get_definingDeclaration());
      if (definingDeclaration == nullptr) {
        ownershipFailure("class-definition-canonical-owner", definition,
                         classDeclaration, definition->get_parent(),
                         countDirectOwnerEdges(classDeclaration, definition));
      }
      requireExactParent(definition, definingDeclaration,
                         "class-definition-owner");
    }
  }
}

void ValidateParentPointers::validateParentPointersInType(SgType *typeNode,
                                                          SgNode *) {
  validateNamedTypeDeclaration(typeNode, "named-type-declaration-owner");
}

void ValidateParentPointers::validateParentPointersInTemplateArgumentList(
    const SgTemplateArgumentPtrList &arguments) {
  for (SgTemplateArgument *argument : arguments) {
    if (argument == nullptr) {
      ownershipFailure("null-template-argument", nullptr, nullptr, nullptr, 0);
    }
    requirePublishedParent(argument, "template-argument-owner");

    switch (argument->get_argumentType()) {
    case SgTemplateArgument::argument_undefined:
      ownershipFailure("undefined-template-argument", argument,
                       argument->get_parent(), argument->get_parent(), 0);

    case SgTemplateArgument::type_argument:
      validateNamedTypeDeclaration(argument->get_type(),
                                   "template-type-declaration-owner");
      break;

    case SgTemplateArgument::nontype_argument:
      if (SgExpression *expression = argument->get_expression()) {
        if (argument->get_initializedName() != nullptr) {
          ownershipFailure("ambiguous-nontype-template-argument", argument,
                           argument->get_parent(), argument->get_parent(), 0);
        }
        requireExactParent(expression, argument,
                           "template-argument-expression-owner");
      } else {
        requireExactParent(argument->get_initializedName(), argument,
                           "template-argument-name-owner");
      }
      break;

    case SgTemplateArgument::template_template_argument:
      requirePublishedParent(argument->get_templateDeclaration(),
                             "template-template-declaration-owner");
      break;

    case SgTemplateArgument::start_of_pack_expansion_argument:
      if (SgExpression *expression = argument->get_expression()) {
        requireExactParent(expression, argument,
                           "pack-expansion-expression-owner");
      }
      break;
    }
  }
}

ValidateParentPointersInheritedAttribute
ValidateParentPointers::evaluateInheritedAttribute(
    SgNode *node, ValidateParentPointersInheritedAttribute inheritedAttribute) {
  if (node == nullptr) {
    ownershipFailure("null-traversal-node", nullptr,
                     inheritedAttribute.parentNode, nullptr, 0);
  }

  validateTraversalEdge(node, inheritedAttribute.parentNode);
  requireFileInfoOwner(node, node->get_file_info(), "file-info-owner");
  requireFileInfoOwner(node, node->get_endOfConstruct(), "end-file-info-owner");

  if (SgInitializedName *initializedName = isSgInitializedName(node)) {
    requireExactParent(&initializedName->get_storageModifier(), initializedName,
                       "storage-modifier-owner", false);
    if (SgInitializedName *previous = initializedName->get_prev_decl_item()) {
      requirePublishedParent(previous, "previous-declaration-item-owner");
    }
    if (initializedName->get_type() != nullptr) {
      validateNamedTypeDeclaration(initializedName->get_type(),
                                   "initialized-name-type-owner");
    }
  }

  if (SgConstructorInitializer *initializer =
          isSgConstructorInitializer(node)) {
    if (initializer->get_declaration() == nullptr &&
        initializer->get_class_decl() == nullptr &&
        !initializer->get_associated_class_unknown()) {
      ownershipFailure("constructor-associated-class", initializer,
                       initializer->get_parent(), initializer->get_parent(), 0);
    }
  }

  if (SgScopeStatement *scope = isSgScopeStatement(node)) {
    validateSymbolTable(scope);
  }

  if (SgClassDeclaration *declaration = isSgClassDeclaration(node)) {
    validateParentPointersInDeclaration(declaration,
                                        inheritedAttribute.parentNode);
  }

  if (SgTemplateInstantiationDecl *declaration =
          isSgTemplateInstantiationDecl(node)) {
    validateParentPointersInTemplateArgumentList(
        declaration->get_templateArguments());
  } else if (SgTemplateInstantiationMemberFunctionDecl *declaration =
                 isSgTemplateInstantiationMemberFunctionDecl(node)) {
    validateParentPointersInTemplateArgumentList(
        declaration->get_templateArguments());
  }

  if (SgVarRefExp *reference = isSgVarRefExp(node)) {
    SgVariableSymbol *symbol = reference->get_symbol();
    validateSymbol(symbol, "variable-reference-symbol-owner");
    SgInitializedName *declaration = symbol->get_declaration();
    requirePublishedParent(declaration, "variable-reference-declaration-owner");
  }

  if (SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node)) {
    if (declaration->get_orig_return_type() == nullptr) {
      ownershipFailure("function-return-type", declaration,
                       declaration->get_parent(), declaration->get_parent(), 0);
    }
    validateNamedTypeDeclaration(declaration->get_orig_return_type(),
                                 "function-return-declaration-owner");
  }

  if (SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node)) {
    validateNamedTypeDeclaration(declaration->get_base_type(),
                                 "typedef-base-declaration-owner");
    SgDeclarationStatement *nondefining =
        declaration->get_baseTypeNondefiningDeclaration();
    SgDeclarationStatement *defining =
        declaration->get_baseTypeDefiningDeclaration();
    if (nondefining != nullptr && defining != nullptr) {
      ownershipFailure("typedef-exclusive-base-tag-owner", declaration,
                       nondefining, defining, 2);
    }
    if (nondefining != nullptr || defining != nullptr) {
      requireExactParent(nondefining != nullptr ? nondefining : defining,
                         declaration, "typedef-embedded-declaration-owner");
    }
  }

  if (SgTemplateInstantiationDirectiveStatement *directive =
          isSgTemplateInstantiationDirectiveStatement(node)) {
    SgDeclarationStatement *declaration = directive->get_declaration();
    requireExactParent(declaration, directive, "explicit-instantiation-owner");
    if (SgMemberFunctionDeclaration *member =
            isSgMemberFunctionDeclaration(declaration)) {
      requireExactParent(member->get_CtorInitializerList(), member,
                         "constructor-initializer-list-owner");
    }
  }

  if (isSgType(node) == nullptr && isSgSymbol(node) == nullptr) {
    traceBackToRoot(node);
  }

  inheritedAttribute.parentNode = node;
  return inheritedAttribute;
}

void validateParentPointers(SgNode *node, SgNode *parent) {
  if (node == nullptr) {
    ownershipFailure("null-validation-root", nullptr, parent, nullptr, 0);
  }

  ValidateParentPointersInheritedAttribute inheritedAttribute;
  inheritedAttribute.parentNode = parent;
  ValidateParentPointers traversal;
  traversal.traverse(node, inheritedAttribute);
}

void validateFreshSubtreeOwnership(SgNode *node, SgNode *boundary) {
  if (node == nullptr || boundary == nullptr) {
    ownershipFailure("fresh-transaction-root", node, boundary,
                     node != nullptr ? node->get_parent() : nullptr, 0);
  }
  requireExactParent(node, boundary, "fresh-transaction-root");
  FreshOwnershipBoundaryGuard guard(boundary);
  ValidateParentPointersInheritedAttribute inheritedAttribute;
  inheritedAttribute.parentNode = boundary;
  ValidateParentPointers traversal;
  traversal.traverse(node, inheritedAttribute);
}

void ValidateParentPointersOfClassAndNamespaceDeclarations::visit(
    SgNode *node) {
  if (node == nullptr) {
    ownershipFailure("null-list-node", nullptr, nullptr, nullptr, 0);
  }

  if (SgClassDefinition *definition = isSgClassDefinition(node)) {
    for (SgDeclarationStatement *member : definition->get_members()) {
      requireExactParent(member, definition, "class-member-owner");
    }
  } else if (SgNamespaceDefinitionStatement *definition =
                 isSgNamespaceDefinitionStatement(node)) {
    for (SgDeclarationStatement *declaration : definition->get_declarations()) {
      requireExactParent(declaration, definition,
                         "namespace-declaration-owner");
    }
  } else if (SgGlobal *global = isSgGlobal(node)) {
    for (SgDeclarationStatement *declaration : global->get_declarations()) {
      requireExactParent(declaration, global, "global-declaration-owner");
    }
  } else if (SgBasicBlock *block = isSgBasicBlock(node)) {
    for (SgStatement *statement : block->get_statements()) {
      requireExactParent(statement, block, "basic-block-statement-owner");
    }
  }
}

void validateParentPointersOfClassOrNamespaceDeclarations(SgNode *node) {
  if (node == nullptr) {
    ownershipFailure("null-list-validation-root", nullptr, nullptr, nullptr, 0);
  }
  ValidateParentPointersOfClassAndNamespaceDeclarations traversal;
  traversal.traverse(node, preorder);
}

void topLevelValidateParentPointers(SgNode *node) {
  TimingPerformance timer("Validate parent pointers:");
  validateParentPointers(node, node != nullptr ? node->get_parent() : nullptr);
  validateParentPointersOfClassOrNamespaceDeclarations(node);
}
