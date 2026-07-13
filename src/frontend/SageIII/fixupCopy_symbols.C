// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "fixupCopy.h"

#include <unordered_set>

// This file implementes support for the AST copy fixup.  It is specific to:
// 1) Construction of symbols, and
// 2) setup of symbol tables

namespace {

SgNode *requireExactCopiedSymbolChild(const SgNode *originalOwner,
                                      const SgNode *originalChild,
                                      SgNode *copiedOwner, SgNode *copiedChild,
                                      SgCopyHelp &help, const char *relation) {
  ASSERT_not_null(originalOwner);
  ASSERT_not_null(originalChild);
  ASSERT_not_null(copiedOwner);
  ASSERT_not_null(relation);
  SgCopyHelp::copiedNodeMapTypeIterator mapping =
      help.get_copiedNodeMap().find(const_cast<SgNode *>(originalChild));
  SgNode *expected =
      mapping != help.get_copiedNodeMap().end() ? mapping->second : nullptr;
  if (expected == nullptr || expected == originalChild ||
      expected != copiedChild || originalChild->get_parent() != originalOwner ||
      copiedChild == nullptr || copiedChild->get_parent() != copiedOwner ||
      copiedChild->variantT() != originalChild->variantT()) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[symbol-child-map]: relation=%s "
        "original-owner=%p/%s child=%p/%s parent=%p copy-owner=%p/%s "
        "child=%p/%s parent=%p expected=%p\n",
        relation, static_cast<const void *>(originalOwner),
        originalOwner->class_name().c_str(),
        static_cast<const void *>(originalChild),
        originalChild->class_name().c_str(),
        static_cast<void *>(originalChild->get_parent()),
        static_cast<void *>(copiedOwner), copiedOwner->class_name().c_str(),
        static_cast<void *>(copiedChild),
        copiedChild != nullptr ? copiedChild->class_name().c_str() : "<null>",
        static_cast<void *>(copiedChild != nullptr ? copiedChild->get_parent()
                                                   : nullptr),
        static_cast<void *>(expected));
    ROSE_ABORT();
  }
  return copiedChild;
}

void fixupExactCopiedSupportSymbols(
    const SgNode *original, SgNode *copy, SgCopyHelp &help,
    std::unordered_set<const SgNode *> &active) {
  ASSERT_not_null(original);
  ASSERT_not_null(copy);
  if (!active.insert(original).second) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[support-symbol-cycle]: original=%p/%s "
            "copy=%p/%s\n",
            static_cast<const void *>(original), original->class_name().c_str(),
            static_cast<void *>(copy), copy->class_name().c_str());
    ROSE_ABORT();
  }

  const Rose_STL_Container<SgNode *> originalChildren =
      const_cast<SgNode *>(original)->get_traversalSuccessorContainer();
  const Rose_STL_Container<SgNode *> copiedChildren =
      copy->get_traversalSuccessorContainer();
  if (originalChildren.size() != copiedChildren.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[support-symbol-arity]: original=%p/%s "
            "children=%zu copy=%p/%s children=%zu\n",
            static_cast<const void *>(original), original->class_name().c_str(),
            originalChildren.size(), static_cast<void *>(copy),
            copy->class_name().c_str(), copiedChildren.size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalChildren.size(); ++index) {
    SgNode *originalChild = originalChildren[index];
    SgNode *copiedChild = copiedChildren[index];
    if ((originalChild == nullptr) != (copiedChild == nullptr)) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[support-symbol-null-child]: owner=%p/%s "
          "copy=%p/%s index=%zu original-child=%p copy-child=%p\n",
          static_cast<const void *>(original), original->class_name().c_str(),
          static_cast<void *>(copy), copy->class_name().c_str(), index,
          static_cast<void *>(originalChild), static_cast<void *>(copiedChild));
      ROSE_ABORT();
    }
    if (originalChild == nullptr) {
      continue;
    }
    requireExactCopiedSymbolChild(original, originalChild, copy, copiedChild,
                                  help, "support-symbol-child");
    if (isSgExpression(originalChild) != nullptr ||
        isSgStatement(originalChild) != nullptr) {
      originalChild->fixupCopy_symbols(copiedChild, help);
    } else {
      if (const SgLocatedNode *originalLocated =
              isSgLocatedNode(originalChild)) {
        originalLocated->SgLocatedNode::fixupCopy_symbols(copiedChild, help);
      }
      fixupExactCopiedSupportSymbols(originalChild, copiedChild, help, active);
    }
  }
  active.erase(original);
}

void fixupExactCopiedSupportSymbols(const SgNode *original, SgNode *copy,
                                    SgCopyHelp &help) {
  std::unordered_set<const SgNode *> active;
  fixupExactCopiedSupportSymbols(original, copy, help, active);
}

void fixupExactCopiedOwnedSuccessorSymbols(const SgNode *original, SgNode *copy,
                                           SgCopyHelp &help,
                                           const char *relation) {
  ASSERT_not_null(original);
  ASSERT_not_null(copy);
  ASSERT_not_null(relation);
  SgCopyHelp::copiedNodeMapTypeIterator mapping =
      help.get_copiedNodeMap().find(const_cast<SgNode *>(original));
  SgNode *expected =
      mapping != help.get_copiedNodeMap().end() ? mapping->second : nullptr;
  if (expected == nullptr || expected == original || expected != copy ||
      copy->variantT() != original->variantT()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[owned-symbol-root]: relation=%s source=%p/%s "
            "copy=%p/%s expected=%p/%s\n",
            relation, static_cast<const void *>(original),
            original->class_name().c_str(), static_cast<void *>(copy),
            copy->class_name().c_str(), static_cast<void *>(expected),
            expected != nullptr ? expected->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  fixupExactCopiedSupportSymbols(original, copy, help);
}

void requireExactCopiedSymbolRoot(const SgNode *original, SgNode *copy,
                                  SgCopyHelp &help, const char *relation) {
  ASSERT_not_null(original);
  ASSERT_not_null(copy);
  ASSERT_not_null(relation);
  SgCopyHelp::copiedNodeMapTypeIterator mapping =
      help.get_copiedNodeMap().find(const_cast<SgNode *>(original));
  SgNode *expected =
      mapping != help.get_copiedNodeMap().end() ? mapping->second : nullptr;
  if (expected == nullptr || expected == original || expected != copy ||
      copy->variantT() != original->variantT()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[symbol-root-map]: relation=%s "
            "original=%p/%s copy=%p/%s expected=%p/%s\n",
            relation, static_cast<const void *>(original),
            original->class_name().c_str(), static_cast<void *>(copy),
            copy->class_name().c_str(), static_cast<void *>(expected),
            expected != nullptr ? expected->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
}

bool hasNonidentityMappedSymbolOwner(const SgNode *node, SgCopyHelp &help) {
  std::unordered_set<const SgNode *> visited;
  for (const SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (!visited.insert(current).second) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[symbol-semantic-owner-cycle]: node=%p/%s "
              "current=%p/%s\n",
              static_cast<const void *>(node), node->class_name().c_str(),
              static_cast<const void *>(current),
              current->class_name().c_str());
      ROSE_ABORT();
    }
    SgCopyHelp::copiedNodeMapTypeIterator mapping =
        help.get_copiedNodeMap().find(const_cast<SgNode *>(current));
    if (mapping != help.get_copiedNodeMap().end() &&
        mapping->second != current) {
      return true;
    }
  }
  return false;
}

void requireExactCopiedOrExternalSymbolEdge(const SgNode *originalEdge,
                                            const SgNode *copiedEdge,
                                            SgCopyHelp &help,
                                            const char *relation) {
  ASSERT_not_null(relation);
  if (originalEdge == nullptr) {
    if (copiedEdge != nullptr) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[symbol-semantic-null]: relation=%s "
              "copy-edge=%p/%s\n",
              relation, static_cast<const void *>(copiedEdge),
              copiedEdge->class_name().c_str());
      ROSE_ABORT();
    }
    return;
  }

  SgCopyHelp::copiedNodeMapTypeIterator mapping =
      help.get_copiedNodeMap().find(const_cast<SgNode *>(originalEdge));
  if (mapping != help.get_copiedNodeMap().end() &&
      mapping->second != originalEdge) {
    if (mapping->second == nullptr || copiedEdge != mapping->second ||
        copiedEdge->variantT() != originalEdge->variantT()) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[symbol-semantic-map]: relation=%s "
              "source=%p/%s copy-edge=%p/%s expected=%p/%s\n",
              relation, static_cast<const void *>(originalEdge),
              originalEdge->class_name().c_str(),
              static_cast<const void *>(copiedEdge),
              copiedEdge != nullptr ? copiedEdge->class_name().c_str()
                                    : "<null>",
              static_cast<void *>(mapping->second),
              mapping->second != nullptr ? mapping->second->class_name().c_str()
                                         : "<null>");
      ROSE_ABORT();
    }
    return;
  }

  if (copiedEdge != originalEdge ||
      hasNonidentityMappedSymbolOwner(originalEdge, help)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[symbol-semantic-external]: relation=%s "
            "source=%p/%s copy-edge=%p/%s inside-transaction=%d\n",
            relation, static_cast<const void *>(originalEdge),
            originalEdge->class_name().c_str(),
            static_cast<const void *>(copiedEdge),
            copiedEdge != nullptr ? copiedEdge->class_name().c_str() : "<null>",
            hasNonidentityMappedSymbolOwner(originalEdge, help) ? 1 : 0);
    ROSE_ABORT();
  }
}

SgTemplateParameterPtrList *
templateParameterListForSymbolFixup(SgDeclarationStatement *declaration) {
  ASSERT_not_null(declaration);
  switch (declaration->variantT()) {
  case V_SgTemplateDeclaration:
    return &isSgTemplateDeclaration(declaration)->get_templateParameters();
  case V_SgTemplateClassDeclaration:
    return &isSgTemplateClassDeclaration(declaration)->get_templateParameters();
  case V_SgTemplateFunctionDeclaration:
    return &isSgTemplateFunctionDeclaration(declaration)
                ->get_templateParameters();
  case V_SgTemplateMemberFunctionDeclaration:
    return &isSgTemplateMemberFunctionDeclaration(declaration)
                ->get_templateParameters();
  case V_SgTemplateVariableDeclaration:
    return &isSgTemplateVariableDeclaration(declaration)
                ->get_templateParameters();
  case V_SgTemplateTypedefDeclaration:
    return &isSgTemplateTypedefDeclaration(declaration)
                ->get_templateParameters();
  case V_SgNonrealDecl:
    return &isSgNonrealDecl(declaration)->get_tpl_params();
  default:
    return nullptr;
  }
}

void fixupExactCopiedTemplateParameterSymbols(
    const SgDeclarationStatement *originalDeclaration,
    SgDeclarationStatement *copiedDeclaration, SgCopyHelp &help) {
  SgTemplateParameterPtrList *originalParameters =
      templateParameterListForSymbolFixup(
          const_cast<SgDeclarationStatement *>(originalDeclaration));
  SgTemplateParameterPtrList *copiedParameters =
      templateParameterListForSymbolFixup(copiedDeclaration);
  if ((originalParameters == nullptr) != (copiedParameters == nullptr)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-symbol-kind]: "
            "original=%p/%s copy=%p/%s\n",
            static_cast<const void *>(originalDeclaration),
            originalDeclaration->class_name().c_str(),
            static_cast<void *>(copiedDeclaration),
            copiedDeclaration->class_name().c_str());
    ROSE_ABORT();
  }
  if (originalParameters == nullptr) {
    return;
  }
  if (originalParameters->size() != copiedParameters->size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-symbol-count]: "
            "original=%p count=%zu copy=%p count=%zu\n",
            static_cast<const void *>(originalDeclaration),
            originalParameters->size(), static_cast<void *>(copiedDeclaration),
            copiedParameters->size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalParameters->size(); ++index) {
    SgTemplateParameter *originalParameter = (*originalParameters)[index];
    SgTemplateParameter *copiedParameter = (*copiedParameters)[index];
    requireExactCopiedSymbolChild(originalDeclaration, originalParameter,
                                  copiedDeclaration, copiedParameter, help,
                                  "template-parameter");
    originalParameter->fixupCopy_symbols(copiedParameter, help);
  }
}

void fixupExactCopiedOwnedDeclarationScopeSymbols(
    const SgDeclarationStatement *originalDeclaration,
    SgDeclarationStatement *copiedDeclaration, SgCopyHelp &help) {
  SgDeclarationScope *originalScope =
      originalDeclaration->get_nonreal_decl_scope();
  SgDeclarationScope *copiedScope = copiedDeclaration->get_nonreal_decl_scope();
  if ((originalScope == nullptr) != (copiedScope == nullptr)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[owned-declaration-scope-symbol-null]: "
            "original=%p scope=%p copy=%p scope=%p\n",
            static_cast<const void *>(originalDeclaration),
            static_cast<void *>(originalScope),
            static_cast<void *>(copiedDeclaration),
            static_cast<void *>(copiedScope));
    ROSE_ABORT();
  }
  if (originalScope == nullptr) {
    return;
  }
  requireExactCopiedSymbolChild(originalDeclaration, originalScope,
                                copiedDeclaration, copiedScope, help,
                                "owned-declaration-scope");
  const SgDeclarationStatementPtrList &originalChildren =
      originalScope->get_declarations();
  const SgDeclarationStatementPtrList &copiedChildren =
      copiedScope->get_declarations();
  if (originalChildren.size() != copiedChildren.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[owned-declaration-scope-symbol-count]: "
            "original=%p count=%zu copy=%p count=%zu\n",
            static_cast<void *>(originalScope), originalChildren.size(),
            static_cast<void *>(copiedScope), copiedChildren.size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalChildren.size(); ++index) {
    SgDeclarationStatement *originalChild = originalChildren[index];
    SgDeclarationStatement *copiedChild = copiedChildren[index];
    requireExactCopiedSymbolChild(originalScope, originalChild, copiedScope,
                                  copiedChild, help,
                                  "owned-declaration-scope-declaration");
    originalChild->fixupCopy_symbols(copiedChild, help);
  }
  originalScope->fixupCopy_symbols(copiedScope, help);
}

void fixupExactCopiedFunctionDeclaratorScopeSymbols(
    const SgFunctionDeclaration *originalDeclaration,
    SgFunctionDeclaration *copiedDeclaration, SgCopyHelp &help) {
  SgDeclarationScope *originalScope =
      originalDeclaration->get_function_declarator_scope();
  SgDeclarationScope *copiedScope =
      copiedDeclaration->get_function_declarator_scope();
  if ((originalScope == nullptr) != (copiedScope == nullptr)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[function-declarator-scope-symbol-null]: "
            "original=%p scope=%p copy=%p scope=%p\n",
            static_cast<const void *>(originalDeclaration),
            static_cast<void *>(originalScope),
            static_cast<void *>(copiedDeclaration),
            static_cast<void *>(copiedScope));
    ROSE_ABORT();
  }
  if (originalScope == nullptr) {
    return;
  }
  requireExactCopiedSymbolChild(originalDeclaration, originalScope,
                                copiedDeclaration, copiedScope, help,
                                "function-declarator-scope");
  const SgDeclarationStatementPtrList &originalChildren =
      originalScope->get_declarations();
  const SgDeclarationStatementPtrList &copiedChildren =
      copiedScope->get_declarations();
  if (originalChildren.size() != copiedChildren.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[function-declarator-scope-symbol-count]: "
            "original=%p count=%zu copy=%p count=%zu\n",
            static_cast<void *>(originalScope), originalChildren.size(),
            static_cast<void *>(copiedScope), copiedChildren.size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalChildren.size(); ++index) {
    SgDeclarationStatement *originalChild = originalChildren[index];
    SgDeclarationStatement *copiedChild = copiedChildren[index];
    requireExactCopiedSymbolChild(originalScope, originalChild, copiedScope,
                                  copiedChild, help,
                                  "function-declarator-scope-declaration");
    originalChild->fixupCopy_symbols(copiedChild, help);
  }
  originalScope->fixupCopy_symbols(copiedScope, help);
}

void fixupCanonicalStatementCopiesForSymbols(
    const SgStatementPtrList &statementList_original,
    SgScopeStatement *copyScopeStatement, SgCopyHelp &help) {
  ROSE_ASSERT(copyScopeStatement != NULL);

  for (SgStatementPtrList::const_iterator i_original =
           statementList_original.begin();
       i_original != statementList_original.end(); ++i_original) {
    SgStatement *originalStatement = *i_original;
    if (originalStatement == NULL) {
      continue;
    }

    SgCopyHelp::copiedNodeMapTypeIterator copyIter =
        help.get_copiedNodeMap().find(originalStatement);
    if (copyIter == help.get_copiedNodeMap().end()) {
      continue;
    }

    SgStatement *copyStatement = isSgStatement(copyIter->second);
    if (copyStatement == NULL ||
        copyStatement->get_parent() != copyScopeStatement) {
      continue;
    }

    originalStatement->fixupCopy_symbols(copyStatement, help);
  }
}

void fixupCanonicalDeclarationCopiesForSymbols(
    const SgDeclarationStatementPtrList &statementList_original,
    SgScopeStatement *copyScopeStatement, SgCopyHelp &help) {
  ROSE_ASSERT(copyScopeStatement != NULL);

  for (SgDeclarationStatementPtrList::const_iterator i_original =
           statementList_original.begin();
       i_original != statementList_original.end(); ++i_original) {
    SgDeclarationStatement *originalDeclaration = *i_original;
    if (originalDeclaration == NULL) {
      continue;
    }

    SgCopyHelp::copiedNodeMapTypeIterator copyIter =
        help.get_copiedNodeMap().find(originalDeclaration);
    if (copyIter == help.get_copiedNodeMap().end()) {
      continue;
    }

    SgDeclarationStatement *copyDeclaration =
        isSgDeclarationStatement(copyIter->second);
    if (copyDeclaration == NULL ||
        copyDeclaration->get_parent() != copyScopeStatement) {
      continue;
    }

    originalDeclaration->fixupCopy_symbols(copyDeclaration, help);
  }
}

void fixupAuxiliaryDeclarationScopeCopiesForSymbols(
    const SgScopeStatement *originalOwner, SgScopeStatement *copiedOwner,
    SgCopyHelp &help) {
  ROSE_ASSERT(originalOwner != NULL);
  ROSE_ASSERT(copiedOwner != NULL);

  SgDeclarationScopeList *originalContainer =
      originalOwner->get_auxiliary_declaration_scopes();
  SgDeclarationScopeList *copiedContainer =
      copiedOwner->get_auxiliary_declaration_scopes();
  if ((originalContainer == NULL) != (copiedContainer == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[auxiliary-scope-symbol-container]: "
            "original=%p container=%p copy=%p container=%p\n",
            static_cast<const void *>(originalOwner),
            static_cast<void *>(originalContainer),
            static_cast<void *>(copiedOwner),
            static_cast<void *>(copiedContainer));
    ROSE_ABORT();
  }
  if (originalContainer == NULL) {
    return;
  }

  SgCopyHelp::copiedNodeMapTypeIterator containerCopy =
      help.get_copiedNodeMap().find(originalContainer);
  const SgDeclarationScopePtrList &originalScopes =
      originalContainer->get_scopes();
  const SgDeclarationScopePtrList &copiedScopes = copiedContainer->get_scopes();
  if (containerCopy == help.get_copiedNodeMap().end() ||
      containerCopy->second != copiedContainer ||
      originalContainer->get_parent() != originalOwner ||
      copiedContainer->get_parent() != copiedOwner ||
      originalScopes.size() != copiedScopes.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[auxiliary-scope-symbol-owner]: original=%p "
            "container=%p parent=%p scopes=%zu copy=%p container=%p "
            "parent=%p scopes=%zu mapped=%p\n",
            static_cast<const void *>(originalOwner),
            static_cast<void *>(originalContainer),
            static_cast<void *>(originalContainer->get_parent()),
            originalScopes.size(), static_cast<void *>(copiedOwner),
            static_cast<void *>(copiedContainer),
            static_cast<void *>(copiedContainer->get_parent()),
            copiedScopes.size(),
            static_cast<void *>(containerCopy != help.get_copiedNodeMap().end()
                                    ? containerCopy->second
                                    : NULL));
    ROSE_ABORT();
  }

  for (size_t scopeIndex = 0; scopeIndex < originalScopes.size();
       ++scopeIndex) {
    SgDeclarationScope *originalScope = originalScopes[scopeIndex];
    SgDeclarationScope *copiedScope = copiedScopes[scopeIndex];
    SgCopyHelp::copiedNodeMapTypeIterator scopeCopy =
        help.get_copiedNodeMap().find(originalScope);
    if (originalScope == NULL || copiedScope == NULL ||
        scopeCopy == help.get_copiedNodeMap().end() ||
        scopeCopy->second != copiedScope ||
        originalScope->get_parent() != originalContainer ||
        copiedScope->get_parent() != copiedContainer) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[auxiliary-scope-symbol-map]: index=%zu "
              "original=%p parent=%p copy=%p parent=%p mapped=%p\n",
              scopeIndex, static_cast<void *>(originalScope),
              static_cast<void *>(
                  originalScope != NULL ? originalScope->get_parent() : NULL),
              static_cast<void *>(copiedScope),
              static_cast<void *>(
                  copiedScope != NULL ? copiedScope->get_parent() : NULL),
              static_cast<void *>(scopeCopy != help.get_copiedNodeMap().end()
                                      ? scopeCopy->second
                                      : NULL));
      ROSE_ABORT();
    }

    const SgDeclarationStatementPtrList &originalDeclarations =
        originalScope->get_declarations();
    const SgDeclarationStatementPtrList &copiedDeclarations =
        copiedScope->get_declarations();
    if (originalDeclarations.size() != copiedDeclarations.size()) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[auxiliary-scope-symbol-declarations]: "
              "scope=%p count=%zu copy=%p count=%zu\n",
              static_cast<void *>(originalScope), originalDeclarations.size(),
              static_cast<void *>(copiedScope), copiedDeclarations.size());
      ROSE_ABORT();
    }
    for (size_t declarationIndex = 0;
         declarationIndex < originalDeclarations.size(); ++declarationIndex) {
      SgDeclarationStatement *originalDeclaration =
          originalDeclarations[declarationIndex];
      SgDeclarationStatement *copiedDeclaration =
          copiedDeclarations[declarationIndex];
      SgCopyHelp::copiedNodeMapTypeIterator declarationCopy =
          help.get_copiedNodeMap().find(originalDeclaration);
      if (originalDeclaration == NULL || copiedDeclaration == NULL ||
          declarationCopy == help.get_copiedNodeMap().end() ||
          declarationCopy->second != copiedDeclaration ||
          originalDeclaration->get_parent() != originalScope ||
          copiedDeclaration->get_parent() != copiedScope) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[auxiliary-scope-symbol-declaration-map]: "
                "scope-index=%zu declaration-index=%zu original=%p "
                "parent=%p copy=%p parent=%p mapped=%p\n",
                scopeIndex, declarationIndex,
                static_cast<void *>(originalDeclaration),
                static_cast<void *>(originalDeclaration != NULL
                                        ? originalDeclaration->get_parent()
                                        : NULL),
                static_cast<void *>(copiedDeclaration),
                static_cast<void *>(copiedDeclaration != NULL
                                        ? copiedDeclaration->get_parent()
                                        : NULL),
                static_cast<void *>(declarationCopy !=
                                            help.get_copiedNodeMap().end()
                                        ? declarationCopy->second
                                        : NULL));
        ROSE_ABORT();
      }
      originalDeclaration->fixupCopy_symbols(copiedDeclaration, help);
    }

    originalScope->fixupCopy_symbols(copiedScope, help);
  }
}

void fixupAuxiliaryDeclarationCopiesForSymbols(
    const SgScopeStatement *originalOwner, SgScopeStatement *copiedOwner,
    SgCopyHelp &help) {
  ROSE_ASSERT(originalOwner != NULL);
  ROSE_ASSERT(copiedOwner != NULL);

  SgAuxiliaryDeclarationList *originalContainer =
      originalOwner->get_auxiliary_declarations();
  SgAuxiliaryDeclarationList *copiedContainer =
      copiedOwner->get_auxiliary_declarations();
  if ((originalContainer == NULL) != (copiedContainer == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[auxiliary-declaration-symbol-container]: "
            "original=%p container=%p copy=%p container=%p\n",
            static_cast<const void *>(originalOwner),
            static_cast<void *>(originalContainer),
            static_cast<void *>(copiedOwner),
            static_cast<void *>(copiedContainer));
    ROSE_ABORT();
  }
  if (originalContainer == NULL) {
    return;
  }

  SgCopyHelp::copiedNodeMapTypeIterator containerCopy =
      help.get_copiedNodeMap().find(originalContainer);
  const SgDeclarationStatementPtrList &originalDeclarations =
      originalContainer->get_declarations();
  const SgDeclarationStatementPtrList &copiedDeclarations =
      copiedContainer->get_declarations();
  if (containerCopy == help.get_copiedNodeMap().end() ||
      containerCopy->second != copiedContainer ||
      originalContainer->get_parent() != originalOwner ||
      copiedContainer->get_parent() != copiedOwner ||
      originalDeclarations.size() != copiedDeclarations.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[auxiliary-declaration-symbol-owner]: "
            "original=%p container=%p parent=%p count=%zu copy=%p "
            "container=%p parent=%p count=%zu mapped=%p\n",
            static_cast<const void *>(originalOwner),
            static_cast<void *>(originalContainer),
            static_cast<void *>(originalContainer->get_parent()),
            originalDeclarations.size(), static_cast<void *>(copiedOwner),
            static_cast<void *>(copiedContainer),
            static_cast<void *>(copiedContainer->get_parent()),
            copiedDeclarations.size(),
            static_cast<void *>(containerCopy != help.get_copiedNodeMap().end()
                                    ? containerCopy->second
                                    : NULL));
    ROSE_ABORT();
  }

  for (size_t index = 0; index < originalDeclarations.size(); ++index) {
    SgDeclarationStatement *originalDeclaration = originalDeclarations[index];
    SgDeclarationStatement *copiedDeclaration = copiedDeclarations[index];
    SgCopyHelp::copiedNodeMapTypeIterator declarationCopy =
        help.get_copiedNodeMap().find(originalDeclaration);
    if (originalDeclaration == NULL || copiedDeclaration == NULL ||
        declarationCopy == help.get_copiedNodeMap().end() ||
        declarationCopy->second != copiedDeclaration ||
        originalDeclaration->get_parent() != originalContainer ||
        copiedDeclaration->get_parent() != copiedContainer ||
        originalDeclaration->get_scope() != originalOwner ||
        copiedDeclaration->get_scope() != copiedOwner) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[auxiliary-declaration-symbol-map]: "
          "index=%zu original=%p parent=%p scope=%p copy=%p parent=%p "
          "scope=%p mapped=%p\n",
          index, static_cast<void *>(originalDeclaration),
          static_cast<void *>(originalDeclaration != NULL
                                  ? originalDeclaration->get_parent()
                                  : NULL),
          static_cast<void *>(originalDeclaration != NULL
                                  ? originalDeclaration->get_scope()
                                  : NULL),
          static_cast<void *>(copiedDeclaration),
          static_cast<void *>(copiedDeclaration != NULL
                                  ? copiedDeclaration->get_parent()
                                  : NULL),
          static_cast<void *>(copiedDeclaration != NULL
                                  ? copiedDeclaration->get_scope()
                                  : NULL),
          static_cast<void *>(declarationCopy != help.get_copiedNodeMap().end()
                                  ? declarationCopy->second
                                  : NULL));
      ROSE_ABORT();
    }
    originalDeclaration->fixupCopy_symbols(copiedDeclaration, help);
  }
}

void mirrorNamespaceFragmentSymbolsToGlobalDefinition(
    SgNamespaceDefinitionStatement *namespaceDefinition) {
  ROSE_ASSERT(namespaceDefinition != NULL);

  SgNamespaceDefinitionStatement *globalDefinition =
      namespaceDefinition->get_global_definition();
  if (globalDefinition == NULL || globalDefinition == namespaceDefinition) {
    return;
  }

  SgSymbolTable *localTable = namespaceDefinition->get_symbol_table();
  SgSymbolTable *globalTable = globalDefinition->get_symbol_table();
  if (localTable == NULL || globalTable == NULL) {
    return;
  }

  SgSymbolTable::BaseHashType *globalEntries = globalTable->get_table();
  ROSE_ASSERT(globalEntries != NULL);

  std::set<SgNode *> localSymbols = localTable->get_symbols();
  for (SgNode *entry : localSymbols) {
    SgSymbol *localSymbol = isSgSymbol(entry);
    if (localSymbol == NULL || isSgAliasSymbol(localSymbol) != NULL) {
      continue;
    }

    const SgName &name = localSymbol->get_name();
    const SgNode *basis = localSymbol->get_symbol_basis();

    bool alreadyMirrored = false;
    std::pair<SgSymbolTable::BaseHashType::iterator,
              SgSymbolTable::BaseHashType::iterator>
        matches = globalEntries->equal_range(name);
    for (SgSymbolTable::BaseHashType::iterator it = matches.first;
         it != matches.second; ++it) {
      SgSymbol *existingSymbol = it->second;
      if (existingSymbol != NULL &&
          existingSymbol->get_symbol_basis() == basis) {
        alreadyMirrored = true;
        break;
      }
    }

    if (alreadyMirrored) {
      continue;
    }

    SgAliasSymbol *aliasSymbol = new SgAliasSymbol(localSymbol);
    aliasSymbol->get_causal_nodes().push_back(globalDefinition);
    globalTable->insert(aliasSymbol->get_name(), aliasSymbol);
    aliasSymbol->set_parent(globalTable);
  }
}

} // namespace

void SgInitializedName::fixupCopy_symbols(SgNode *, SgCopyHelp &) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgInitializedName::fixupCopy_symbols() %p = %s \n", this,
         this->get_name().str());
#endif

  // DQ (11/7/2007): I think that there is nothing to do here specific to symbol
  // and symbol tables.
}

void SgStatement::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
  // We need to call the fixupCopy function from the parent of a
  // SgVariableDeclaration because the copy function in the parent of the
  // variable declaration sets the parent of the SgVariableDeclaration and we
  // need this parent in the fixupCopy function in the SgInitializedName.

#if DEBUG_FIXUP_COPY
  printf("Inside of SgStatement::fixupCopy_symbols() for %p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgLocatedNode::fixupCopy_symbols(copy, help);
}

void SgOmpClauseStatement::fixupCopy_symbols(SgNode *copy,
                                             SgCopyHelp &help) const {
  SgStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help,
                                        "openmp-clause-statement");
}

void SgOmpTaskwaitStatement::fixupCopy_symbols(SgNode *copy,
                                               SgCopyHelp &help) const {
  SgStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help,
                                        "openmp-taskwait-statement");
}

void SgAccClauseStatement::fixupCopy_symbols(SgNode *copy,
                                             SgCopyHelp &help) const {
  SgStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help,
                                        "openacc-clause-statement");
}

void SgOmpBodyStatement::fixupCopy_symbols(SgNode *copy,
                                           SgCopyHelp &help) const {
  SgOmpBodyStatement *copiedBodyOwner = isSgOmpBodyStatement(copy);
  if (copiedBodyOwner == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openmp-body-symbol-kind]: original=%p/%s "
            "copy=%p/%s\n",
            static_cast<const void *>(this), class_name().c_str(),
            static_cast<void *>(copy),
            copy != nullptr ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  SgStatement *originalBody = get_body();
  SgStatement *copiedBody = copiedBodyOwner->get_body();
  if ((originalBody == nullptr) != (copiedBody == nullptr)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openmp-body-symbol-null]: original=%p body=%p "
            "copy=%p body=%p\n",
            static_cast<const void *>(this), static_cast<void *>(originalBody),
            static_cast<void *>(copiedBodyOwner),
            static_cast<void *>(copiedBody));
    ROSE_ABORT();
  }
  if (originalBody != nullptr) {
    requireExactCopiedSymbolChild(this, originalBody, copiedBodyOwner,
                                  copiedBody, help, "openmp-body-symbol");
    originalBody->fixupCopy_symbols(copiedBody, help);
  }
  SgStatement::fixupCopy_symbols(copy, help);
}

void SgAccBodyStatement::fixupCopy_symbols(SgNode *copy,
                                           SgCopyHelp &help) const {
  SgAccBodyStatement *copiedBodyOwner = isSgAccBodyStatement(copy);
  if (copiedBodyOwner == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openacc-body-symbol-kind]: original=%p/%s "
            "copy=%p/%s\n",
            static_cast<const void *>(this), class_name().c_str(),
            static_cast<void *>(copy),
            copy != nullptr ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  SgStatement *originalBody = get_body();
  SgStatement *copiedBody = copiedBodyOwner->get_body();
  if ((originalBody == nullptr) != (copiedBody == nullptr)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openacc-body-symbol-null]: original=%p body=%p "
            "copy=%p body=%p\n",
            static_cast<const void *>(this), static_cast<void *>(originalBody),
            static_cast<void *>(copiedBodyOwner),
            static_cast<void *>(copiedBody));
    ROSE_ABORT();
  }
  if (originalBody != nullptr) {
    requireExactCopiedSymbolChild(this, originalBody, copiedBodyOwner,
                                  copiedBody, help, "openacc-body-symbol");
    originalBody->fixupCopy_symbols(copiedBody, help);
  }
  SgStatement::fixupCopy_symbols(copy, help);
}

void SgOmpClauseBodyStatement::fixupCopy_symbols(SgNode *copy,
                                                 SgCopyHelp &help) const {
  SgOmpClauseBodyStatement *copiedClauseBody = isSgOmpClauseBodyStatement(copy);
  if (copiedClauseBody == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openmp-clause-symbol-kind]: original=%p/%s "
            "copy=%p/%s\n",
            static_cast<const void *>(this), class_name().c_str(),
            static_cast<void *>(copy),
            copy != nullptr ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  SgOmpBodyStatement::fixupCopy_symbols(copy, help);
  const SgOmpClauseList *originalClauses = get_clause_list();
  SgOmpClauseList *copiedClauses = copiedClauseBody->get_clause_list();
  requireExactCopiedSymbolChild(this, originalClauses, copiedClauseBody,
                                copiedClauses, help,
                                "openmp-clause-list-symbol");
  fixupExactCopiedSupportSymbols(originalClauses, copiedClauses, help);
}

void SgAccClauseBodyStatement::fixupCopy_symbols(SgNode *copy,
                                                 SgCopyHelp &help) const {
  SgAccClauseBodyStatement *copiedClauseBody = isSgAccClauseBodyStatement(copy);
  if (copiedClauseBody == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openacc-clause-symbol-kind]: original=%p/%s "
            "copy=%p/%s\n",
            static_cast<const void *>(this), class_name().c_str(),
            static_cast<void *>(copy),
            copy != nullptr ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  SgAccBodyStatement::fixupCopy_symbols(copy, help);
  const SgAccClausePtrList &originalClauses = get_clauses();
  const SgAccClausePtrList &copiedClauses = copiedClauseBody->get_clauses();
  if (originalClauses.size() != copiedClauses.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openacc-clause-symbol-count]: original=%p "
            "clauses=%zu copy=%p clauses=%zu\n",
            static_cast<const void *>(this), originalClauses.size(),
            static_cast<void *>(copiedClauseBody), copiedClauses.size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalClauses.size(); ++index) {
    SgAccClause *originalClause = originalClauses[index];
    SgAccClause *copiedClause = copiedClauses[index];
    requireExactCopiedSymbolChild(this, originalClause, copiedClauseBody,
                                  copiedClause, help, "openacc-clause-symbol");
    fixupExactCopiedSupportSymbols(originalClause, copiedClause, help);
  }
}

void SgOmpDeclareSimdStatement::fixupCopy_symbols(SgNode *copy,
                                                  SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help,
                                        "openmp-declare-simd");
}

void SgOmpDeclareVariantStatement::fixupCopy_symbols(SgNode *copy,
                                                     SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help,
                                        "openmp-declare-variant");
}

void SgOmpBeginDeclareVariantStatement::fixupCopy_symbols(
    SgNode *copy, SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help,
                                        "openmp-begin-declare-variant");
}

void SgOmpDeclareMapperStatement::fixupCopy_symbols(SgNode *copy,
                                                    SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help,
                                        "openmp-declare-mapper");
}

void SgOmpDeclareTargetStatement::fixupCopy_symbols(SgNode *copy,
                                                    SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help,
                                        "openmp-declare-target");
}

void SgOmpRequiresStatement::fixupCopy_symbols(SgNode *copy,
                                               SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help, "openmp-requires");
}

void SgOmpAssumesStatement::fixupCopy_symbols(SgNode *copy,
                                              SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help, "openmp-assumes");
}

void SgOmpBeginAssumesStatement::fixupCopy_symbols(SgNode *copy,
                                                   SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help,
                                        "openmp-begin-assumes");
}

void SgOmpGroupprivateStatement::fixupCopy_symbols(SgNode *copy,
                                                   SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help,
                                        "openmp-groupprivate");
}

void SgOmpThreadprivateStatement::fixupCopy_symbols(SgNode *copy,
                                                    SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_symbols(copy, help);
  fixupExactCopiedOwnedSuccessorSymbols(this, copy, help,
                                        "openmp-threadprivate");
}

void SgExpression::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgExpression::fixupCopy_symbols() for %p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  Rose_STL_Container<SgNode *> children_original =
      const_cast<SgExpression *>(this)->get_traversalSuccessorContainer();
  Rose_STL_Container<SgNode *> children_copy =
      const_cast<SgNode *>(copy)->get_traversalSuccessorContainer();
  ROSE_ASSERT(children_original.size() == children_copy.size());

  for (Rose_STL_Container<SgNode *>::const_iterator
           i_original = children_original.begin(),
           i_copy = children_copy.begin();
       i_original != children_original.end(); ++i_original, ++i_copy) {
    if (*i_original == NULL)
      continue;
    (*i_original)->fixupCopy_symbols(*i_copy, help);
  }

  SgLocatedNode::fixupCopy_symbols(copy, help);
}

void SgThisExp::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
  SgExpression::fixupCopy_symbols(copy, help);
  SgThisExp *copiedThis = isSgThisExp(copy);
  requireExactCopiedSymbolRoot(this, copiedThis, help, "this-expression");
}

void SgLocatedNode::fixupCopy_symbols(SgNode *, SgCopyHelp &) const {
#if DEBUG_FIXUP_COPY
  printf(
      "Inside of SgLocatedNode::fixupCopy_symbols() for %p = %s copy = %p \n",
      this, this->class_name().c_str(), copy);
#endif
}

void SgScopeStatement::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgScopeStatement::fixupCopy_symbols() for %p = %s copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Call the base class fixupCopy member function
  SgStatement::fixupCopy_symbols(copy, help);

  SgScopeStatement *copyScopeStatement = isSgScopeStatement(copy);
  ROSE_ASSERT(copyScopeStatement != NULL);

  fixupAuxiliaryDeclarationScopeCopiesForSymbols(this, copyScopeStatement,
                                                 help);
  fixupAuxiliaryDeclarationCopiesForSymbols(this, copyScopeStatement, help);

  // Canonical copied scopes can now be reached from multiple original edges
  // (e.g., declaration links reusing an already-copied subtree). Once a copied
  // scope's symbol table has been rebuilt, revisiting it should be a no-op.
  if (copyScopeStatement->symbol_table_size() != 0) {
    return;
  }

  // The source symbol table is the semantic inventory for this scope.  Copy
  // its exact entries; rescanning lexical statements loses symbols whose
  // declarations are owned by typed edges, namespace reopenings, catch
  // conditions, or compiler-generated semantic nodes.
  SageInterface::fixupReferencesToSymbols(this, copyScopeStatement, help);

  // printf ("\nLeaving SgScopeStatement::fixupCopy_symbols() for %p = %s copy =
  // %p \n\n",this,this->class_name().c_str(),copy);
}

void SgGlobal::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgGlobal::fixupCopy_symbols() for %p copy = %p \n", this,
         copy);
#endif

  SgGlobal *global_copy = isSgGlobal(copy);
  ROSE_ASSERT(global_copy != NULL);

  fixupCanonicalDeclarationCopiesForSymbols(this->getDeclarationList(),
                                            global_copy, help);

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_symbols(copy, help);

  // printf ("\nLeaving SgGlobal::fixupCopy_symbols() this = %p = %s  copy = %p
  // \n",this,this->class_name().c_str(),copy);
}

// JJW 2/1/2008 Added support for statement expressions
void SgExprStatement::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgSgExprStatement::fixupCopy_symbols() for %p = %s copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgExprStatement *es_copy = isSgExprStatement(copy);
  ROSE_ASSERT(es_copy != NULL);

  SgExpression *expression_original = this->get_expression();
  SgExpression *expression_copy = es_copy->get_expression();

  expression_original->fixupCopy_symbols(expression_copy, help);

  // Call the base class fixupCopy member function
  SgStatement::fixupCopy_symbols(copy, help);

  // printf ("\nLeaving SgExprStatement::fixupCopy_symbols() this = %p = %s copy
  // = %p \n",this,this->class_name().c_str(),copy);
}

// DQ (10/6/2007): Added fixup function to set scopes not set properly by the
// ROSETTA generated copy!
void SgBasicBlock::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgBasicBlock::fixupCopy_symbols() for %p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgBasicBlock *block_copy = isSgBasicBlock(copy);
  ROSE_ASSERT(block_copy != NULL);

  fixupCanonicalStatementCopiesForSymbols(this->getStatementList(), block_copy,
                                          help);

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_symbols(copy, help);

  // printf ("\nLeaving SgBasicBlock::fixupCopy_symbols() this = %p = %s  copy =
  // %p \n",this,this->class_name().c_str(),copy);
}

void SgDeclarationStatement::fixupCopy_symbols(SgNode *copy,
                                               SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgDeclarationStatement::fixupCopy_symbols() for %p = %s "
         "copy = %p (defining = %p firstNondefining = %p) \n",
         this, this->class_name().c_str(), copy,
         this->get_definingDeclaration(),
         this->get_firstNondefiningDeclaration());
#endif

  SgDeclarationStatement *copiedDeclaration = isSgDeclarationStatement(copy);
  if (copiedDeclaration == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[declaration-symbol-copy]: original=%p/%s "
            "copy=%p\n",
            static_cast<const void *>(this), class_name().c_str(),
            static_cast<void *>(copy));
    ROSE_ABORT();
  }
  requireExactCopiedSymbolRoot(this, copiedDeclaration, help, "declaration");
  fixupExactCopiedOwnedDeclarationScopeSymbols(this, copiedDeclaration, help);
  fixupExactCopiedTemplateParameterSymbols(this, copiedDeclaration, help);

  // Call the base class fixupCopy member function (this will setup the parent)
  SgStatement::fixupCopy_symbols(copy, help);
}

void SgFunctionDeclaration::fixupCopy_symbols(SgNode *copy,
                                              SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("\nIn SgFunctionDeclaration::fixupCopy_symbols(): for function = %s = "
         "%p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  SgFunctionDeclaration *functionDeclaration_copy =
      isSgFunctionDeclaration(copy);
  ROSE_ASSERT(functionDeclaration_copy != NULL);

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_symbols(copy, help);

  fixupExactCopiedFunctionDeclaratorScopeSymbols(this, functionDeclaration_copy,
                                                 help);

  // Setup the scopes of the SgInitializedName objects in the paraleter list
  ROSE_ASSERT(get_parameterList() != NULL);
  get_parameterList()->fixupCopy_symbols(
      functionDeclaration_copy->get_parameterList(), help);

  // Setup the details in the SgFunctionDefinition (this may have to rebuild the
  // sysmbol table) printf ("In SgFunctionDeclaration::fixupCopy_symbols():
  // this->get_definition() = %p \n",this->get_definition());
  if (this->get_definition() != NULL) {
    // DQ (3/15/2014): The defining declaration should not be marked
    // (isForward() == true).
    if (isForward() == true) {
      printf("Error: The defining declaration should not be marked "
             "(isForward() == true) \n");
      printf("SgFunctionDeclaration::fixupCopy_symbols(): (isForward() == "
             "true): functionDeclaration_copy = %p = %s \n",
             functionDeclaration_copy,
             functionDeclaration_copy->class_name().c_str());
      printf(
          "   --- functionDeclaration_copy->get_firstNondefiningDeclaration() "
          "= %p \n",
          functionDeclaration_copy->get_firstNondefiningDeclaration());
      printf("   --- functionDeclaration_copy->get_definingDeclaration()       "
             "  = %p \n",
             functionDeclaration_copy->get_definingDeclaration());

      functionDeclaration_copy->get_file_info()->display(
          "SgFunctionDeclaration::fixupCopy_scopes(): (isForward() == true): "
          "debug");

      // Reset this!
      // functionDeclaration_copy->unsetForward();
    }
    ROSE_ASSERT(isForward() == false);

    // DQ (2/26/2009): Handle special cases where the copyHelp function is
    // non-trivial. Is every version of copyHelp object going to be a problem?

    // For the outlining, our copyHelp object does not copy defining function
    // declarations and substitutes a non-defining declarations, so if the copy
    // has been built this way then skip trying to reset the
    // SgFunctionDefinition. printf ("In
    // SgFunctionDeclaration::fixupCopy_symbols():
    // functionDeclaration_copy->get_definition() = %p
    // \n",functionDeclaration_copy->get_definition());
    // this->get_definition()->fixupCopy_symbols(functionDeclaration_copy->get_definition(),help);
    if (functionDeclaration_copy->get_definition() != NULL) {
      this->get_definition()->fixupCopy_symbols(
          functionDeclaration_copy->get_definition(), help);
    }

    // If this is a declaration with a definition then it is a defining
    // declaration
    // functionDeclaration_copy->set_definingDeclaration(functionDeclaration_copy);
  }

  // printf ("\nLeaving SgFunctionDeclaration::fixupCopy_symbols(): for function
  // = %s = %p = %s copy = %p
  // \n",this->get_name().str(),this,this->class_name().c_str(),copy);
}

// DQ (10/6/2007): Added fixup function to set scopes not set properly by the
// ROSETTA generated copy!
void SgFunctionParameterList::fixupCopy_symbols(SgNode *copy,
                                                SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgFunctionParameterList::fixupCopy_symbols() for %p = %s "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Also call the base class version of the fixupCopycopy() member function
  SgDeclarationStatement::fixupCopy_symbols(copy, help);

  SgFunctionParameterList *copyFunctionParameterList =
      isSgFunctionParameterList(copy);
  ROSE_ASSERT(copyFunctionParameterList != NULL);

  const SgInitializedNamePtrList &parameterList_original = this->get_args();
  SgInitializedNamePtrList &parameterList_copy =
      copyFunctionParameterList->get_args();

  SgInitializedNamePtrList::const_iterator i_original =
      parameterList_original.begin();
  SgInitializedNamePtrList::iterator i_copy = parameterList_copy.begin();

  // Iterate over both lists to match up the correct pairs of SgInitializedName
  // objects
  while ((i_original != parameterList_original.end()) &&
         (i_copy != parameterList_copy.end())) {
    (*i_original)->fixupCopy_symbols(*i_copy, help);

    i_original++;
    i_copy++;
  }
}

void SgMemberFunctionDeclaration::fixupCopy_symbols(SgNode *copy,
                                                    SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("In SgMemberFunctionDeclaration::fixupCopy_symbols(): for function = "
         "%s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  SgMemberFunctionDeclaration *memberFunctionDeclaration_copy =
      isSgMemberFunctionDeclaration(copy);
  ROSE_ASSERT(memberFunctionDeclaration_copy != NULL);

  // Setup the scopes of the SgInitializedName objects in the paraleter list
  ROSE_ASSERT(get_CtorInitializerList() != NULL);
  get_CtorInitializerList()->fixupCopy_symbols(
      memberFunctionDeclaration_copy->get_CtorInitializerList(), help);

  // Call the base class fixupCopy member function
  SgFunctionDeclaration::fixupCopy_symbols(copy, help);
}

void SgTemplateParameter::fixupCopy_symbols(SgNode *copy,
                                            SgCopyHelp &help) const {
  SgTemplateParameter *copiedParameter = isSgTemplateParameter(copy);
  if (copiedParameter == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-symbol-copy]: "
            "original=%p copy=%p\n",
            static_cast<const void *>(this), static_cast<void *>(copy));
    ROSE_ABORT();
  }
  requireExactCopiedSymbolRoot(this, copiedParameter, help,
                               "template-parameter");
  requireExactCopiedOrExternalSymbolEdge(
      get_templateDeclaration(), copiedParameter->get_templateDeclaration(),
      help, "template-parameter-declaration");
  fixupExactCopiedSupportSymbols(this, copiedParameter, help);
}

void SgTemplateDeclaration::fixupCopy_symbols(SgNode *copy,
                                              SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("\nInside of SgTemplateDeclaration::fixupCopy_symbols() for %p = %s "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Also call the base class version of the fixupCopy() member function
  SgDeclarationStatement::fixupCopy_symbols(copy, help);
}

void SgTemplateInstantiationDefn::fixupCopy_symbols(SgNode *copy,
                                                    SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgTemplateInstantiationDefn::fixupCopy_symbols() for class "
         "= %s class definition %p = %s copy = %p \n",
         this->get_declaration()->get_name().str(), this,
         this->class_name().c_str(), copy);
#endif

  // Call the base class fixupCopy member function
  SgClassDefinition::fixupCopy_symbols(copy, help);
}

void SgTemplateInstantiationDecl::fixupCopy_symbols(SgNode *copy,
                                                    SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("\nIn SgTemplateInstantiationDecl::fixupCopy_symbols(): for function "
         "= %s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  SgTemplateInstantiationDecl *templateClassDeclaration_copy =
      isSgTemplateInstantiationDecl(copy);
  ROSE_ASSERT(templateClassDeclaration_copy != NULL);

  // Also call the base class version of the fixupCopycopy() member function
  SgClassDeclaration::fixupCopy_symbols(copy, help);
}

void SgTemplateInstantiationMemberFunctionDecl::fixupCopy_symbols(
    SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("\nIn SgTemplateInstantiationMemberFunctionDecl::fixupCopy_symbols(): "
         "for function = %s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  SgTemplateInstantiationMemberFunctionDecl
      *templateMemberFunctionDeclaration_copy =
          isSgTemplateInstantiationMemberFunctionDecl(copy);
  ROSE_ASSERT(templateMemberFunctionDeclaration_copy != NULL);

  // Also call the base class version of the fixupCopycopy() member function
  SgMemberFunctionDeclaration::fixupCopy_symbols(copy, help);
}

void SgTemplateInstantiationFunctionDecl::fixupCopy_symbols(
    SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("\nIn SgTemplateInstantiationFunctionDecl::fixupCopy_symbols(): for "
         "function = %s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  SgTemplateInstantiationFunctionDecl *templateFunctionDeclaration_copy =
      isSgTemplateInstantiationFunctionDecl(copy);
  ROSE_ASSERT(templateFunctionDeclaration_copy != NULL);

  // Also call the base class version of the fixupCopycopy() member function
  SgFunctionDeclaration::fixupCopy_symbols(copy, help);
}

void SgFunctionDefinition::fixupCopy_symbols(SgNode *copy,
                                             SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgFunctionDefinition::fixupCopy_symbols() for %p = %s copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgFunctionDefinition *functionDefinition_copy = isSgFunctionDefinition(copy);
  ROSE_ASSERT(functionDefinition_copy != NULL);

  ROSE_ASSERT(get_body() != NULL);
  get_body()->fixupCopy_symbols(functionDefinition_copy->get_body(), help);

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_symbols(copy, help);

  // printf ("\nLeaving SgFunctionDefinition::fixupCopy_symbols() for %p = %s
  // copy = %p \n\n",this,this->class_name().c_str(),copy);
}

void SgVariableDeclaration::fixupCopy_symbols(SgNode *copy,
                                              SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgVariableDeclaration::fixupCopy_symbols() for %p = %s "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Also call the base class version of the fixupCopycopy() member function
  SgDeclarationStatement::fixupCopy_symbols(copy, help);

  SgVariableDeclaration *variableDeclaration_copy =
      isSgVariableDeclaration(copy);
  ROSE_ASSERT(variableDeclaration_copy != NULL);

  SgDeclarationStatement *baseTypeNondefiningOriginal =
      get_baseTypeNondefiningDeclaration();
  SgDeclarationStatement *baseTypeNondefiningCopy =
      variableDeclaration_copy->get_baseTypeNondefiningDeclaration();
  if ((baseTypeNondefiningOriginal == NULL) !=
      (baseTypeNondefiningCopy == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[variable-base-type-forward-symbol-null]: "
            "source=%p child=%p copy=%p child=%p\n",
            static_cast<const void *>(this),
            static_cast<void *>(baseTypeNondefiningOriginal),
            static_cast<void *>(variableDeclaration_copy),
            static_cast<void *>(baseTypeNondefiningCopy));
    ROSE_ABORT();
  }
  if (baseTypeNondefiningOriginal != NULL) {
    requireExactCopiedSymbolChild(
        this, baseTypeNondefiningOriginal, variableDeclaration_copy,
        baseTypeNondefiningCopy, help, "variable-base-type-forward");
    baseTypeNondefiningOriginal->fixupCopy_symbols(baseTypeNondefiningCopy,
                                                   help);
  }

  // Preserve symbols for the exact inline base-type definition child.
  if (this->get_baseTypeDefiningDeclaration() != NULL) {
    ROSE_ASSERT(variableDeclaration_copy->get_baseTypeDefiningDeclaration() !=
                NULL);
    SgDeclarationStatement *baseTypeDeclaration_original =
        this->get_baseTypeDefiningDeclaration();
    SgDeclarationStatement *baseTypeDeclaration_copy =
        variableDeclaration_copy->get_baseTypeDefiningDeclaration();

    ROSE_ASSERT(baseTypeDeclaration_original != NULL);
    ROSE_ASSERT(baseTypeDeclaration_copy != NULL);

    // printf ("In SgVariableDeclaration::fixupCopy_symbols(): Calling fixupCopy
    // on %p = %s
    // \n",baseTypeDeclaration_original,baseTypeDeclaration_original->class_name().c_str());

    baseTypeDeclaration_original->fixupCopy_symbols(baseTypeDeclaration_copy,
                                                    help);
  }

  const SgInitializedNamePtrList &variableList_original = this->get_variables();
  SgInitializedNamePtrList &variableList_copy =
      variableDeclaration_copy->get_variables();

  // printf ("Inside of SgVariableDeclaration::fixupCopy_symbols():
  // variableList_original.size() = %ld \n",(long)variableList_original.size());

  ROSE_ASSERT(variableList_original.size() == variableList_copy.size());

  SgInitializedNamePtrList::const_iterator i_original =
      variableList_original.begin();
  SgInitializedNamePtrList::iterator i_copy = variableList_copy.begin();

  // Iterate over both lists to match up the correct pairs of SgInitializedName
  // objects
  while ((i_original != variableList_original.end()) &&
         (i_copy != variableList_copy.end())) {
    // printf ("Looping over the initialized names in the variable declaration
    // variable = %p = %s \n",(*i_copy),(*i_copy)->get_name().str());

    (*i_original)->fixupCopy_symbols(*i_copy, help);

    i_original++;
    i_copy++;
  }
}

void SgClassDeclaration::fixupCopy_symbols(SgNode *copy,
                                           SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgClassDeclaration::fixupCopy_symbols() for class = %s = "
         "%p = %s copy = %p (defining = %p firstNondefining = %p) \n",
         this->get_name().str(), this, this->class_name().c_str(), copy,
         this->get_definingDeclaration(),
         this->get_firstNondefiningDeclaration());
#endif

  SgClassDeclaration *classDeclaration_copy = isSgClassDeclaration(copy);
  ROSE_ASSERT(classDeclaration_copy != NULL);

  SgClassDefinition *classDefinition_original = this->get_definition();
  SgClassDefinition *classDefinition_copy =
      classDeclaration_copy->get_definition();

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_symbols(copy, help);

  if (isForward() == false) {
    classDefinition_original->fixupCopy_symbols(classDefinition_copy, help);
  }
}

void SgClassDefinition::fixupCopy_symbols(SgNode *copy,
                                          SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgClassDefinition::fixupCopy_symbols() for class = %s "
         "class definition %p = %s copy = %p \n",
         this->get_declaration()->get_name().str(), this,
         this->class_name().c_str(), copy);
#endif

  SgClassDefinition *classDefinition_copy = isSgClassDefinition(copy);
  ROSE_ASSERT(classDefinition_copy != NULL);

  SgBaseClassPtrList::const_iterator i_original =
      this->get_inheritances().begin();
  SgBaseClassPtrList::iterator i_copy =
      classDefinition_copy->get_inheritances().begin();
  ROSE_ASSERT(this->get_inheritances().size() ==
              classDefinition_copy->get_inheritances().size());

  while ((i_original != this->get_inheritances().end()) &&
         (i_copy != classDefinition_copy->get_inheritances().end())) {
    // Check the parent pointer to make sure it is properly set
    ROSE_ASSERT((*i_original)->get_parent() != NULL);
    ROSE_ASSERT((*i_original)->get_parent() == this);

    (*i_original)->fixupCopy_symbols(*i_copy, help);

    // DQ (11/7/2007): This was already setup on fixupCopy_scopes, but we can
    // test it here.
    // (*i_copy)->set_parent(classDefinition_copy);
    ROSE_ASSERT((*i_copy)->get_parent() != NULL);
    ROSE_ASSERT((*i_copy)->get_parent() == classDefinition_copy);

    i_original++;
    i_copy++;
  }

  fixupCanonicalDeclarationCopiesForSymbols(this->getDeclarationList(),
                                            classDefinition_copy, help);

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_symbols(copy, help);

  // printf ("\nLeaving SgClassDefinition::fixupCopy_symbols() this = %p = %s
  // copy = %p \n",this,this->class_name().c_str(),copy);
}

void SgBaseClass::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgBaseClass::fixupCopy_symbols() for baseclass = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgBaseClass *baseClass_copy = isSgBaseClass(copy);
  ROSE_ASSERT(baseClass_copy != NULL);

  const SgNonrealBaseClass *nrBaseClass = isSgNonrealBaseClass(this);
  SgNonrealBaseClass *nrBaseClass_copy = isSgNonrealBaseClass(copy);

  if (this->get_base_class() != NULL) {
    ROSE_ASSERT(nrBaseClass == NULL);
    ROSE_ASSERT(nrBaseClass_copy == NULL);
    ROSE_ASSERT(baseClass_copy->get_base_class() != NULL);
  } else if (nrBaseClass != NULL) {
    ROSE_ASSERT(nrBaseClass->get_base_class_nonreal() != NULL);

    ROSE_ASSERT(nrBaseClass_copy != NULL);
    ROSE_ASSERT(nrBaseClass_copy->get_base_class_nonreal() != NULL);
  } else {
    ROSE_ABORT();
  }
}

void SgLabelStatement::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgLabelStatement::fixupCopy_symbols() for %p = %s copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgLabelStatement *labelStatement_copy = isSgLabelStatement(copy);
  ROSE_ASSERT(labelStatement_copy != NULL);

  // Also call the base class version of the fixupCopycopy() member function
  SgStatement::fixupCopy_symbols(copy, help);
}

void SgGotoStatement::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf(
      "Inside of SgGotoStatement::fixupCopy_symbols() for %p = %s copy = %p \n",
      this, this->class_name().c_str(), copy);
#endif

  // Also call the base class version of the fixupCopycopy() member function
  SgStatement::fixupCopy_symbols(copy, help);
}

void SgTypedefDeclaration::fixupCopy_symbols(SgNode *copy,
                                             SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgTypedefDeclaration::fixupCopy_symbols() for typedef name "
         "= %s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif
  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_symbols(copy, help);

  // DQ (10/14/2007): Handle the case of a type defined in the base type of the
  // typedef (similar problem for SgVariableDeclaration).
  if (this->get_typedefBaseTypeContainsDefiningDeclaration() == true) {
    SgTypedefDeclaration *typedefDeclaration_copy =
        isSgTypedefDeclaration(copy);
    ROSE_ASSERT(typedefDeclaration_copy != NULL);

    ROSE_ASSERT(typedefDeclaration_copy
                    ->get_typedefBaseTypeContainsDefiningDeclaration() == true);
    SgDeclarationStatement *baseTypeDeclaration_original =
        this->get_baseTypeDefiningDeclaration();
    SgDeclarationStatement *baseTypeDeclaration_copy =
        typedefDeclaration_copy->get_baseTypeDefiningDeclaration();
    ROSE_ASSERT(baseTypeDeclaration_original != NULL);
    ROSE_ASSERT(baseTypeDeclaration_copy != NULL);
    baseTypeDeclaration_original->fixupCopy_symbols(baseTypeDeclaration_copy,
                                                    help);
  }
}

void SgEnumDeclaration::fixupCopy_symbols(SgNode *copy,
                                          SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgEnumDeclaration::fixupCopy_symbols() for %p = %s copy = "
         "%p (defining = %p firstNondefining = %p) \n",
         this, this->class_name().c_str(), copy,
         this->get_definingDeclaration(),
         this->get_firstNondefiningDeclaration());
#endif

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_symbols(copy, help);

  SgEnumDeclaration *enumDeclaration_copy = isSgEnumDeclaration(copy);
  ROSE_ASSERT(enumDeclaration_copy != NULL);

  // DQ (10/17/2007): fixup the type used to make sure it has the declaration
  // set the AST copy.
  SgEnumType *enum_type_original = this->get_type();
  ROSE_ASSERT(enum_type_original != NULL);

  SgEnumType *enum_type_copy = enumDeclaration_copy->get_type();
  ROSE_ASSERT(enum_type_copy != NULL);

  // Now reset the enum fields.
  const SgInitializedNamePtrList &enumFieldList_original =
      this->get_enumerators();
  SgInitializedNamePtrList &enumFieldList_copy =
      enumDeclaration_copy->get_enumerators();

  SgInitializedNamePtrList::const_iterator i_original =
      enumFieldList_original.begin();
  SgInitializedNamePtrList::iterator i_copy = enumFieldList_copy.begin();

  // Iterate over both lists to match up the correct pairs of SgInitializedName
  // objects
  while ((i_original != enumFieldList_original.end()) &&
         (i_copy != enumFieldList_copy.end())) {
    (*i_original)->fixupCopy_symbols(*i_copy, help);

    i_original++;
    i_copy++;
  }
}

void SgNamespaceDeclarationStatement::fixupCopy_symbols(
    SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgNamespaceDeclarationStatement::fixupCopy_symbols() for "
         "%p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif
  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_symbols(copy, help);

  SgNamespaceDeclarationStatement *namespaceDeclaration_copy =
      isSgNamespaceDeclarationStatement(copy);
  ROSE_ASSERT(namespaceDeclaration_copy != NULL);

  SgNamespaceDefinitionStatement *namespaceDefinition_original =
      this->get_definition();
  SgNamespaceDefinitionStatement *namespaceDefinition_copy =
      namespaceDeclaration_copy->get_definition();

  ROSE_ASSERT(namespaceDefinition_original != NULL);
  ROSE_ASSERT(namespaceDefinition_copy != NULL);

  namespaceDefinition_original->fixupCopy_symbols(namespaceDefinition_copy,
                                                  help);
}

void SgNamespaceDefinitionStatement::fixupCopy_symbols(SgNode *copy,
                                                       SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgNamespaceDefinitionStatement::fixupCopy_symbols() for %p "
         "= %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif
  SgNamespaceDefinitionStatement *namespaceDefinition_copy =
      isSgNamespaceDefinitionStatement(copy);
  ROSE_ASSERT(namespaceDefinition_copy != NULL);

  fixupCanonicalDeclarationCopiesForSymbols(this->getDeclarationList(),
                                            namespaceDefinition_copy, help);
  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_symbols(copy, help);
  mirrorNamespaceFragmentSymbolsToGlobalDefinition(namespaceDefinition_copy);
}

void SgTemplateInstantiationDirectiveStatement::fixupCopy_symbols(
    SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of "
         "SgTemplateInstantiationDirectiveStatement::fixupCopy_symbols() for "
         "%p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_symbols(copy, help);
}

void SgProject::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgProject::fixupCopy_symbols() \n");
#endif

  SgProject *project_copy = isSgProject(copy);
  ROSE_ASSERT(project_copy != NULL);

  // Call fixup on all fo the files (SgFile objects)
  for (int i = 0; i < numberOfFiles(); i++) {
    SgFile &file = get_file(i);
    SgFile &file_copy = project_copy->get_file(i);
    file.fixupCopy_symbols(&file_copy, help);
  }
}

void SgSourceFile::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgFile::fixupCopy_symbols() \n");
#endif

  SgSourceFile *file_copy = isSgSourceFile(copy);
  ROSE_ASSERT(file_copy != NULL);

  // Call fixup on the global scope
  ROSE_ASSERT(get_globalScope() != NULL);
  ROSE_ASSERT(file_copy->get_globalScope() != NULL);
  get_globalScope()->fixupCopy_symbols(file_copy->get_globalScope(), help);
}

void SgIfStmt::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgIfStmt::fixupCopy_symbols() this = %p = %s  copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // SgStatement::fixupCopy_symbols(copy,help);
  SgScopeStatement::fixupCopy_symbols(copy, help);

  SgIfStmt *ifStatement_copy = isSgIfStmt(copy);
  ROSE_ASSERT(ifStatement_copy != NULL);

  // The symbol table should not have been setup yet!
  // ROSE_ASSERT(ifStatement_copy->get_symbol_table()->size() == 0);

  this->get_conditional()->fixupCopy_symbols(
      ifStatement_copy->get_conditional(), help);

  SgStatement *thsTruBody = this->get_true_body();
  ROSE_ASSERT(thsTruBody != NULL);
  SgStatement *ifStmtCopyTruBody = ifStatement_copy->get_true_body();
  ROSE_ASSERT(ifStmtCopyTruBody != NULL);
  SgScopeStatement *scopeStmntCopyTrueBody =
      isSgScopeStatement(ifStmtCopyTruBody);

  // printf ("\nProcess the TRUE body of the SgIfStmt \n\n");

  thsTruBody->fixupCopy_symbols(ifStmtCopyTruBody, help);

  ROSE_ASSERT((this->get_false_body() != NULL) ==
              (ifStatement_copy->get_false_body() != NULL));
  SgScopeStatement *scopeStmntCopyFalseBody =
      isSgScopeStatement(ifStatement_copy->get_false_body());

  // printf ("\nProcess the FALSE body of the SgIfStmt \n\n");

  if (this->get_false_body() != NULL) {
    this->get_false_body()->fixupCopy_symbols(
        ifStatement_copy->get_false_body(), help);
  }

  // printf ("\nLeaving SgIfStmt::fixupCopy_symbols() this = %p = %s  copy = %p
  // \n",this,this->class_name().c_str(),copy);
}

void SgForStatement::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgForStatement::fixupCopy_symbols() this = %p = %s  copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgForStatement *forStatement_copy = isSgForStatement(copy);
  ROSE_ASSERT(forStatement_copy != NULL);

  // DQ (11/7/2007): Now that we separate the fixup of scopes and parent from
  // the symbol table construction these can be called in any order.

  SgScopeStatement::fixupCopy_symbols(copy, help);

  // This are important because there could be a scope setup within this call
  // (e.g. "for ( class X {} x = 0; x != 1; x++) {}") but this is not a common
  // case.
  ROSE_ASSERT(this->get_for_init_stmt() != NULL);
  this->get_for_init_stmt()->fixupCopy_symbols(
      forStatement_copy->get_for_init_stmt(), help);

  ROSE_ASSERT(this->get_test() != NULL);
  this->get_test()->fixupCopy_symbols(forStatement_copy->get_test(), help);

  ROSE_ASSERT(this->get_increment() != NULL);
  this->get_increment()->fixupCopy_symbols(forStatement_copy->get_increment(),
                                           help);

  ROSE_ASSERT(this->get_loop_body() != NULL);
  this->get_loop_body()->fixupCopy_symbols(forStatement_copy->get_loop_body(),
                                           help);
}

void SgRangeBasedForStatement::fixupCopy_symbols(SgNode *copy,
                                                 SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgRangeBasedForStatement::fixupCopy_symbols() this = %p = "
         "%s  copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgRangeBasedForStatement *forStatement_copy =
      isSgRangeBasedForStatement(copy);
  ROSE_ASSERT(forStatement_copy != NULL);

  SgScopeStatement::fixupCopy_symbols(copy, help);

  ROSE_ASSERT(this->get_iterator_declaration() != NULL);
  this->get_iterator_declaration()->fixupCopy_symbols(
      forStatement_copy->get_iterator_declaration(), help);

  ROSE_ASSERT(this->get_range_declaration() != NULL);
  this->get_range_declaration()->fixupCopy_symbols(
      forStatement_copy->get_range_declaration(), help);

  ROSE_ASSERT(this->get_begin_declaration() != NULL);
  this->get_begin_declaration()->fixupCopy_symbols(
      forStatement_copy->get_begin_declaration(), help);

  ROSE_ASSERT(this->get_end_declaration() != NULL);
  this->get_end_declaration()->fixupCopy_symbols(
      forStatement_copy->get_end_declaration(), help);

  ROSE_ASSERT(this->get_not_equal_expression() != NULL);
  this->get_not_equal_expression()->fixupCopy_symbols(
      forStatement_copy->get_not_equal_expression(), help);

  ROSE_ASSERT(this->get_increment_expression() != NULL);
  this->get_increment_expression()->fixupCopy_symbols(
      forStatement_copy->get_increment_expression(), help);

  ROSE_ASSERT(this->get_loop_body() != NULL);
  this->get_loop_body()->fixupCopy_symbols(forStatement_copy->get_loop_body(),
                                           help);
}

void SgForInitStatement::fixupCopy_symbols(SgNode *copy,
                                           SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgForInitStatement::fixupCopy_symbols() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_symbols(copy, help);

  SgForInitStatement *forStatement_copy = isSgForInitStatement(copy);
  ROSE_ASSERT(forStatement_copy != NULL);

  const SgStatementPtrList &statementList_original = this->get_init_stmt();
  const SgStatementPtrList &statementList_copy =
      forStatement_copy->get_init_stmt();

  SgStatementPtrList::const_iterator i_original =
      statementList_original.begin();
  SgStatementPtrList::const_iterator i_copy = statementList_copy.begin();

  // Iterate over both lists to match up the correct pairs of SgStatement
  // objects
  while ((i_original != statementList_original.end()) &&
         (i_copy != statementList_copy.end())) {
    // printf ("In SgForInitStatement::fixupCopy_symbols(): Calling fixup for
    // *i_copy = %p = %s \n",(*i_copy),(*i_copy)->class_name().c_str());
    (*i_original)->fixupCopy_symbols(*i_copy, help);

    i_original++;
    i_copy++;
  }

  // Relavant data member is: SgStatementPtrList        p_init_stmt
}

void SgCatchStatementSeq::fixupCopy_symbols(SgNode *copy,
                                            SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgCatchStatementSeq::fixupCopy_symbols() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_symbols(copy, help);

  SgCatchStatementSeq *catchStatement_copy = isSgCatchStatementSeq(copy);
  if (catchStatement_copy == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[catch-sequence-symbol-copy]: original=%p "
            "copy=%p\n",
            static_cast<const void *>(this), static_cast<void *>(copy));
    ROSE_ABORT();
  }
  requireExactCopiedSymbolRoot(this, catchStatement_copy, help,
                               "catch-sequence");

  const SgStatementPtrList &originalCatches = get_catch_statement_seq();
  const SgStatementPtrList &copiedCatches =
      catchStatement_copy->get_catch_statement_seq();
  if (originalCatches.size() != copiedCatches.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[catch-sequence-symbol-count]: original=%p "
            "count=%zu copy=%p count=%zu\n",
            static_cast<const void *>(this), originalCatches.size(),
            static_cast<void *>(catchStatement_copy), copiedCatches.size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalCatches.size(); ++index) {
    SgCatchOptionStmt *originalCatch =
        isSgCatchOptionStmt(originalCatches[index]);
    SgCatchOptionStmt *copiedCatch = isSgCatchOptionStmt(copiedCatches[index]);
    if (originalCatch == nullptr || copiedCatch == nullptr) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[catch-handler-symbol-kind]: index=%zu "
              "original=%p copy=%p\n",
              index, static_cast<void *>(originalCatches[index]),
              static_cast<void *>(copiedCatches[index]));
      ROSE_ABORT();
    }
    requireExactCopiedSymbolChild(this, originalCatch, catchStatement_copy,
                                  copiedCatch, help, "catch-handler");
    originalCatch->fixupCopy_symbols(copiedCatch, help);
  }
}

void SgWhileStmt::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf(
      "Inside of SgWhileStmt::fixupCopy_symbols() this = %p = %s  copy = %p \n",
      this, this->class_name().c_str(), copy);
#endif

  SgScopeStatement::fixupCopy_symbols(copy, help);

  SgWhileStmt *whileStatement_copy = isSgWhileStmt(copy);
  ROSE_ASSERT(whileStatement_copy != NULL);

  ROSE_ASSERT(this->get_condition() != NULL);
  this->get_condition()->fixupCopy_symbols(whileStatement_copy->get_condition(),
                                           help);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_symbols(whileStatement_copy->get_body(), help);
}

void SgDoWhileStmt::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgDoWhileStmt::fixupCopy_symbols() this = %p = %s  copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgScopeStatement::fixupCopy_symbols(copy, help);

  SgDoWhileStmt *doWhileStatement_copy = isSgDoWhileStmt(copy);
  ROSE_ASSERT(doWhileStatement_copy != NULL);

  ROSE_ASSERT(this->get_condition() != NULL);
  this->get_condition()->fixupCopy_symbols(
      doWhileStatement_copy->get_condition(), help);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_symbols(doWhileStatement_copy->get_body(), help);
}

void SgSwitchStatement::fixupCopy_symbols(SgNode *copy,
                                          SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgSwitchStatement::fixupCopy_symbols() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgScopeStatement::fixupCopy_symbols(copy, help);

  SgSwitchStatement *switchStatement_copy = isSgSwitchStatement(copy);
  ROSE_ASSERT(switchStatement_copy != NULL);

  ROSE_ASSERT(this->get_item_selector() != NULL);
  this->get_item_selector()->fixupCopy_symbols(
      switchStatement_copy->get_item_selector(), help);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_symbols(switchStatement_copy->get_body(), help);
}

void SgTryStmt::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf(
      "Inside of SgTryStmt::fixupCopy_symbols() this = %p = %s  copy = %p \n",
      this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_symbols(copy, help);

  SgTryStmt *tryStatement_copy = isSgTryStmt(copy);
  if (tryStatement_copy == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[try-statement-symbol-copy]: original=%p "
            "copy=%p\n",
            static_cast<const void *>(this), static_cast<void *>(copy));
    ROSE_ABORT();
  }
  requireExactCopiedSymbolRoot(this, tryStatement_copy, help, "try-statement");
  SgStatement *originalBody = get_body();
  SgStatement *copiedBody = tryStatement_copy->get_body();
  SgCatchStatementSeq *originalCatches = get_catch_statement_seq_root();
  SgCatchStatementSeq *copiedCatches =
      tryStatement_copy->get_catch_statement_seq_root();
  if (originalBody == nullptr || copiedBody == nullptr ||
      originalCatches == nullptr || copiedCatches == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[try-statement-symbol-children]: original=%p "
            "body=%p catches=%p copy=%p body=%p catches=%p\n",
            static_cast<const void *>(this), static_cast<void *>(originalBody),
            static_cast<void *>(originalCatches),
            static_cast<void *>(tryStatement_copy),
            static_cast<void *>(copiedBody),
            static_cast<void *>(copiedCatches));
    ROSE_ABORT();
  }
  requireExactCopiedSymbolChild(this, originalBody, tryStatement_copy,
                                copiedBody, help, "try-body");
  requireExactCopiedSymbolChild(this, originalCatches, tryStatement_copy,
                                copiedCatches, help, "try-catches");
  originalBody->fixupCopy_symbols(copiedBody, help);
  originalCatches->fixupCopy_symbols(copiedCatches, help);
}

void SgCatchOptionStmt::fixupCopy_symbols(SgNode *copy,
                                          SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgCatchOptionStmt::fixupCopy_symbols() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgScopeStatement::fixupCopy_symbols(copy, help);

  SgCatchOptionStmt *catchOptionStatement_copy = isSgCatchOptionStmt(copy);
  if (catchOptionStatement_copy == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[catch-handler-symbol-copy]: original=%p "
            "copy=%p\n",
            static_cast<const void *>(this), static_cast<void *>(copy));
    ROSE_ABORT();
  }
  requireExactCopiedSymbolRoot(this, catchOptionStatement_copy, help,
                               "catch-handler");
  SgTryStmt *originalTry = get_trystmt();
  SgTryStmt *copiedTry = catchOptionStatement_copy->get_trystmt();
  if (originalTry == nullptr || copiedTry == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[catch-handler-symbol-try]: original=%p "
            "try=%p copy=%p try=%p\n",
            static_cast<const void *>(this), static_cast<void *>(originalTry),
            static_cast<void *>(catchOptionStatement_copy),
            static_cast<void *>(copiedTry));
    ROSE_ABORT();
  }
  requireExactCopiedOrExternalSymbolEdge(originalTry, copiedTry, help,
                                         "catch-handler-try");

  SgVariableDeclaration *originalCondition = get_condition();
  SgVariableDeclaration *copiedCondition =
      catchOptionStatement_copy->get_condition();
  SgStatement *originalBody = get_body();
  SgStatement *copiedBody = catchOptionStatement_copy->get_body();
  if ((originalCondition == nullptr) != (copiedCondition == nullptr) ||
      originalBody == nullptr || copiedBody == nullptr) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[catch-handler-symbol-children]: original=%p "
        "condition=%p body=%p copy=%p condition=%p body=%p\n",
        static_cast<const void *>(this), static_cast<void *>(originalCondition),
        static_cast<void *>(originalBody),
        static_cast<void *>(catchOptionStatement_copy),
        static_cast<void *>(copiedCondition), static_cast<void *>(copiedBody));
    ROSE_ABORT();
  }
  if (originalCondition != nullptr) {
    requireExactCopiedSymbolChild(this, originalCondition,
                                  catchOptionStatement_copy, copiedCondition,
                                  help, "catch-handler-condition");
  }
  requireExactCopiedSymbolChild(this, originalBody, catchOptionStatement_copy,
                                copiedBody, help, "catch-handler-body");
  if (originalCondition != nullptr) {
    originalCondition->fixupCopy_symbols(copiedCondition, help);
  }
  originalBody->fixupCopy_symbols(copiedBody, help);
}

void SgCaseOptionStmt::fixupCopy_symbols(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgCaseOptionStmt::fixupCopy_symbols() this = %p = %s  copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_symbols(copy, help);

  SgCaseOptionStmt *caseOptionStatement_copy = isSgCaseOptionStmt(copy);
  ROSE_ASSERT(caseOptionStatement_copy != NULL);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_symbols(caseOptionStatement_copy->get_body(),
                                      help);
}

void SgDefaultOptionStmt::fixupCopy_symbols(SgNode *copy,
                                            SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgDefaultOptionStmt::fixupCopy_symbols() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_symbols(copy, help);

  SgDefaultOptionStmt *defaultOptionStatement_copy =
      isSgDefaultOptionStmt(copy);
  ROSE_ASSERT(defaultOptionStatement_copy != NULL);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_symbols(defaultOptionStatement_copy->get_body(),
                                      help);
}

void SgTemplateArgument::fixupCopy_symbols(SgNode *copy,
                                           SgCopyHelp &help) const {
  SgTemplateArgument *templateArgument_copy = isSgTemplateArgument(copy);
  if (templateArgument_copy == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-argument-symbol-copy]: "
            "original=%p copy=%p\n",
            static_cast<const void *>(this), static_cast<void *>(copy));
    ROSE_ABORT();
  }
  requireExactCopiedSymbolRoot(this, templateArgument_copy, help,
                               "template-argument");
  requireExactCopiedOrExternalSymbolEdge(
      get_templateDeclaration(),
      templateArgument_copy->get_templateDeclaration(), help,
      "template-argument-declaration");
  fixupExactCopiedSupportSymbols(this, templateArgument_copy, help);

#if DEBUG_FIXUP_COPY
  printf("\nIn SgTemplateArgument::fixupCopy_symbols(): this = %p = %s copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif
}
