// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.
#include "fixupCopy.h"

#include "rose_config.h"

#include <set>

// This file implementes support for the AST copy fixup.  It is specific to:
// 1) variable reference expressions (SgVarRefExp)
// 2) function reference expressions (SgFunctionRefExp)
// 3) member function reference expressions (SgMemberFunctionRefExp)
// 4) nonreal references with exact resolved function or variable-template edges

namespace {

void requireExactCopiedReferenceChild(const SgNode *originalOwner,
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
        "REX_COPY_INVARIANT[reference-child-map]: relation=%s "
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
}

void fixupExactCopiedSupportReferences(const SgNode *original, SgNode *copy,
                                       SgCopyHelp &help,
                                       std::set<const SgNode *> &active) {
  ASSERT_not_null(original);
  ASSERT_not_null(copy);
  if (!active.insert(original).second) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[support-reference-cycle]: original=%p/%s "
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
            "REX_COPY_INVARIANT[support-reference-arity]: original=%p/%s "
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
          "REX_COPY_INVARIANT[support-reference-null-child]: "
          "owner=%p/%s copy=%p/%s index=%zu original-child=%p "
          "copy-child=%p\n",
          static_cast<const void *>(original), original->class_name().c_str(),
          static_cast<void *>(copy), copy->class_name().c_str(), index,
          static_cast<void *>(originalChild), static_cast<void *>(copiedChild));
      ROSE_ABORT();
    }
    if (originalChild == nullptr) {
      continue;
    }
    requireExactCopiedReferenceChild(original, originalChild, copy, copiedChild,
                                     help, "support-reference-child");
    if (isSgExpression(originalChild) != nullptr ||
        isSgStatement(originalChild) != nullptr) {
      originalChild->fixupCopy_references(copiedChild, help);
    } else {
      fixupExactCopiedSupportReferences(originalChild, copiedChild, help,
                                        active);
    }
  }
  active.erase(original);
}

void fixupExactCopiedSupportReferences(const SgNode *original, SgNode *copy,
                                       SgCopyHelp &help) {
  std::set<const SgNode *> active;
  fixupExactCopiedSupportReferences(original, copy, help, active);
}

void fixupExactCopiedOwnedSuccessorReferences(const SgNode *original,
                                              SgNode *copy, SgCopyHelp &help,
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
            "REX_COPY_INVARIANT[owned-reference-root]: relation=%s "
            "source=%p/%s copy=%p/%s expected=%p/%s\n",
            relation, static_cast<const void *>(original),
            original->class_name().c_str(), static_cast<void *>(copy),
            copy->class_name().c_str(), static_cast<void *>(expected),
            expected != nullptr ? expected->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  fixupExactCopiedSupportReferences(original, copy, help);
}

void requireExactCopiedReferenceRoot(const SgNode *original, SgNode *copy,
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
            "REX_COPY_INVARIANT[reference-root-map]: relation=%s "
            "original=%p/%s copy=%p/%s expected=%p/%s\n",
            relation, static_cast<const void *>(original),
            original->class_name().c_str(), static_cast<void *>(copy),
            copy->class_name().c_str(), static_cast<void *>(expected),
            expected != nullptr ? expected->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
}

bool hasNonidentityMappedReferenceOwner(const SgNode *node, SgCopyHelp &help) {
  std::set<const SgNode *> visited;
  for (const SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (!visited.insert(current).second) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[reference-semantic-owner-cycle]: "
              "node=%p/%s current=%p/%s\n",
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

void requireExactCopiedOrExternalReferenceEdge(const SgNode *originalEdge,
                                               const SgNode *copiedEdge,
                                               SgCopyHelp &help,
                                               const char *relation) {
  ASSERT_not_null(relation);
  if (originalEdge == nullptr) {
    if (copiedEdge != nullptr) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[reference-semantic-null]: relation=%s "
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
              "REX_COPY_INVARIANT[reference-semantic-map]: relation=%s "
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
      hasNonidentityMappedReferenceOwner(originalEdge, help)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[reference-semantic-external]: relation=%s "
            "source=%p/%s copy-edge=%p/%s inside-transaction=%d\n",
            relation, static_cast<const void *>(originalEdge),
            originalEdge->class_name().c_str(),
            static_cast<const void *>(copiedEdge),
            copiedEdge != nullptr ? copiedEdge->class_name().c_str() : "<null>",
            hasNonidentityMappedReferenceOwner(originalEdge, help) ? 1 : 0);
    ROSE_ABORT();
  }
}

SgNode *remapExactCopiedOrExternalReferenceEdge(const SgNode *originalEdge,
                                                SgNode *provisionalCopiedEdge,
                                                SgCopyHelp &help,
                                                const char *relation) {
  ASSERT_not_null(relation);
  if (originalEdge == nullptr) {
    if (provisionalCopiedEdge != nullptr) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[reference-semantic-null]: relation=%s "
              "copy-edge=%p/%s\n",
              relation, static_cast<void *>(provisionalCopiedEdge),
              provisionalCopiedEdge->class_name().c_str());
      ROSE_ABORT();
    }
    return nullptr;
  }

  SgCopyHelp::copiedNodeMapTypeIterator mapping =
      help.get_copiedNodeMap().find(const_cast<SgNode *>(originalEdge));
  if (mapping != help.get_copiedNodeMap().end() &&
      mapping->second != originalEdge) {
    SgNode *expected = mapping->second;
    if (expected == nullptr ||
        expected->variantT() != originalEdge->variantT() ||
        (provisionalCopiedEdge != originalEdge &&
         provisionalCopiedEdge != expected)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[reference-semantic-map]: relation=%s "
              "source=%p/%s provisional=%p/%s expected=%p/%s\n",
              relation, static_cast<const void *>(originalEdge),
              originalEdge->class_name().c_str(),
              static_cast<void *>(provisionalCopiedEdge),
              provisionalCopiedEdge != nullptr
                  ? provisionalCopiedEdge->class_name().c_str()
                  : "<null>",
              static_cast<void *>(expected), expected->class_name().c_str());
      ROSE_ABORT();
    }
    return expected;
  }

  if (provisionalCopiedEdge != originalEdge ||
      hasNonidentityMappedReferenceOwner(originalEdge, help)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[reference-semantic-external]: relation=%s "
            "source=%p/%s provisional=%p/%s inside-transaction=%d\n",
            relation, static_cast<const void *>(originalEdge),
            originalEdge->class_name().c_str(),
            static_cast<void *>(provisionalCopiedEdge),
            provisionalCopiedEdge != nullptr
                ? provisionalCopiedEdge->class_name().c_str()
                : "<null>",
            hasNonidentityMappedReferenceOwner(originalEdge, help) ? 1 : 0);
    ROSE_ABORT();
  }
  return const_cast<SgNode *>(originalEdge);
}

SgTemplateParameterPtrList *
templateParameterListForReferenceFixup(SgDeclarationStatement *declaration) {
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

void fixupExactCopiedTemplateParameterReferences(
    const SgDeclarationStatement *originalDeclaration,
    SgDeclarationStatement *copiedDeclaration, SgCopyHelp &help) {
  SgTemplateParameterPtrList *originalParameters =
      templateParameterListForReferenceFixup(
          const_cast<SgDeclarationStatement *>(originalDeclaration));
  SgTemplateParameterPtrList *copiedParameters =
      templateParameterListForReferenceFixup(copiedDeclaration);
  if ((originalParameters == nullptr) != (copiedParameters == nullptr)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-reference-kind]: "
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
            "REX_COPY_INVARIANT[template-parameter-reference-count]: "
            "original=%p count=%zu copy=%p count=%zu\n",
            static_cast<const void *>(originalDeclaration),
            originalParameters->size(), static_cast<void *>(copiedDeclaration),
            copiedParameters->size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalParameters->size(); ++index) {
    SgTemplateParameter *originalParameter = (*originalParameters)[index];
    SgTemplateParameter *copiedParameter = (*copiedParameters)[index];
    requireExactCopiedReferenceChild(originalDeclaration, originalParameter,
                                     copiedDeclaration, copiedParameter, help,
                                     "template-parameter");
    originalParameter->fixupCopy_references(copiedParameter, help);
  }
}

void fixupExactCopiedOwnedDeclarationScopeReferences(
    const SgDeclarationStatement *originalDeclaration,
    SgDeclarationStatement *copiedDeclaration, SgCopyHelp &help) {
  SgDeclarationScope *originalScope =
      originalDeclaration->get_nonreal_decl_scope();
  SgDeclarationScope *copiedScope = copiedDeclaration->get_nonreal_decl_scope();
  if ((originalScope == nullptr) != (copiedScope == nullptr)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[owned-declaration-scope-reference-null]: "
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
  requireExactCopiedReferenceChild(originalDeclaration, originalScope,
                                   copiedDeclaration, copiedScope, help,
                                   "owned-declaration-scope");
  const SgDeclarationStatementPtrList &originalChildren =
      originalScope->get_declarations();
  const SgDeclarationStatementPtrList &copiedChildren =
      copiedScope->get_declarations();
  if (originalChildren.size() != copiedChildren.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[owned-declaration-scope-reference-count]: "
            "original=%p count=%zu copy=%p count=%zu\n",
            static_cast<void *>(originalScope), originalChildren.size(),
            static_cast<void *>(copiedScope), copiedChildren.size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalChildren.size(); ++index) {
    SgDeclarationStatement *originalChild = originalChildren[index];
    SgDeclarationStatement *copiedChild = copiedChildren[index];
    requireExactCopiedReferenceChild(originalScope, originalChild, copiedScope,
                                     copiedChild, help,
                                     "owned-declaration-scope-declaration");
    originalChild->fixupCopy_references(copiedChild, help);
  }
  originalScope->fixupCopy_references(copiedScope, help);
}

void fixupExactCopiedFunctionDeclaratorScopeReferences(
    const SgFunctionDeclaration *originalDeclaration,
    SgFunctionDeclaration *copiedDeclaration, SgCopyHelp &help) {
  SgDeclarationScope *originalScope =
      originalDeclaration->get_function_declarator_scope();
  SgDeclarationScope *copiedScope =
      copiedDeclaration->get_function_declarator_scope();
  if ((originalScope == nullptr) != (copiedScope == nullptr)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[function-declarator-scope-reference-null]: "
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
  requireExactCopiedReferenceChild(originalDeclaration, originalScope,
                                   copiedDeclaration, copiedScope, help,
                                   "function-declarator-scope");
  const SgDeclarationStatementPtrList &originalChildren =
      originalScope->get_declarations();
  const SgDeclarationStatementPtrList &copiedChildren =
      copiedScope->get_declarations();
  if (originalChildren.size() != copiedChildren.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[function-declarator-scope-reference-count]: "
            "original=%p count=%zu copy=%p count=%zu\n",
            static_cast<void *>(originalScope), originalChildren.size(),
            static_cast<void *>(copiedScope), copiedChildren.size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalChildren.size(); ++index) {
    SgDeclarationStatement *originalChild = originalChildren[index];
    SgDeclarationStatement *copiedChild = copiedChildren[index];
    requireExactCopiedReferenceChild(originalScope, originalChild, copiedScope,
                                     copiedChild, help,
                                     "function-declarator-scope-declaration");
    originalChild->fixupCopy_references(copiedChild, help);
  }
  originalScope->fixupCopy_references(copiedScope, help);
}

SgVariableSymbol *
findCopiedVariableSymbolInLocalScope(SgInitializedName *initializedNameCopy,
                                     SgVariableSymbol *originalSymbol,
                                     SgCopyHelp &help) {
  if (initializedNameCopy != NULL) {
    if (SgScopeStatement *scope = initializedNameCopy->get_scope()) {
      if (SgVariableSymbol *symbol = isSgVariableSymbol(
              scope->find_symbol_from_declaration(initializedNameCopy))) {
        return symbol;
      }
    }
  }

  if (originalSymbol != NULL) {
    SgCopyHelp::copiedNodeMapTypeIterator symbolCopy =
        help.get_copiedNodeMap().find(originalSymbol);
    if (symbolCopy != help.get_copiedNodeMap().end()) {
      return isSgVariableSymbol(symbolCopy->second);
    }
  }

  return NULL;
}

void fixupCanonicalStatementCopiesForReferences(
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

    originalStatement->fixupCopy_references(copyStatement, help);
  }
}

void fixupCanonicalDeclarationCopiesForReferences(
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

    originalDeclaration->fixupCopy_references(copyDeclaration, help);
  }
}

} // namespace

void SgInitializedName::fixupCopy_references(SgNode *copy,
                                             SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgInitializedName::fixupCopy_references() %p = %s \n", this,
         this->get_name().str());
#endif

  SgInitializedName *initializedNameCopy = isSgInitializedName(copy);
  if (initializedNameCopy == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[initialized-name-copy]: original=%p copy=%p "
            "does not preserve the initialized-name node kind\n",
            static_cast<const void *>(this), static_cast<void *>(copy));
    ROSE_ABORT();
  }

  SgInitializedName *crayPointee = get_cray_pointer_pointee();
  if (crayPointee == nullptr) {
    initializedNameCopy->set_cray_pointer_pointee(nullptr);
  } else {
    SgCopyHelp::copiedNodeMapTypeIterator copiedPointee =
        help.get_copiedNodeMap().find(crayPointee);
    if (copiedPointee == help.get_copiedNodeMap().end()) {
      fprintf(stderr,
              "REX_AST_INVARIANT[cray-pointer-copy]: pointer=%p name=%s was "
              "copied without its exact pointee=%p\n",
              static_cast<const void *>(this), get_name().str(),
              static_cast<void *>(crayPointee));
      ROSE_ABORT();
    }
    SgInitializedName *crayPointeeCopy =
        isSgInitializedName(copiedPointee->second);
    SgExprListExp *sourceShape = get_fortran_cray_pointer_pointee_shape();
    SgExprListExp *copiedShape =
        initializedNameCopy->get_fortran_cray_pointer_pointee_shape();
    if (crayPointeeCopy == nullptr ||
        (sourceShape == nullptr) != (copiedShape == nullptr) ||
        (copiedShape != nullptr &&
         copiedShape->get_parent() != initializedNameCopy)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[cray-pointer-copy]: pointer=%p name=%s "
              "does not preserve its exact pointee or owned shape\n",
              static_cast<const void *>(this), get_name().str());
      ROSE_ABORT();
    }
    initializedNameCopy->set_cray_pointer_pointee(crayPointeeCopy);
  }

  SgStatement *pointerOwner = get_fortran_separate_pointer_declaration();
  if (pointerOwner == nullptr) {
    initializedNameCopy->set_fortran_separate_pointer_declaration(nullptr);
  } else {
    SgCopyHelp::copiedNodeMapTypeIterator copiedPointerOwner =
        help.get_copiedNodeMap().find(pointerOwner);
    if (copiedPointerOwner == help.get_copiedNodeMap().end()) {
      fprintf(stderr,
              "REX_AST_INVARIANT[fortran-pointer-copy]: entity=%p name=%s "
              "was copied without its exact POINTER statement=%p\n",
              static_cast<const void *>(this), get_name().str(),
              static_cast<void *>(pointerOwner));
      ROSE_ABORT();
    }
    SgAttributeSpecificationStatement *pointerOwnerCopy =
        isSgAttributeSpecificationStatement(copiedPointerOwner->second);
    if (pointerOwnerCopy == nullptr ||
        pointerOwnerCopy->get_attribute_kind() !=
            SgAttributeSpecificationStatement::e_pointerStatement) {
      fprintf(stderr,
              "REX_AST_INVARIANT[fortran-pointer-copy]: POINTER owner=%p "
              "mapped to an invalid copied node=%p\n",
              static_cast<void *>(pointerOwner),
              static_cast<void *>(copiedPointerOwner->second));
      ROSE_ABORT();
    }
    initializedNameCopy->set_fortran_separate_pointer_declaration(
        pointerOwnerCopy);
  }

  SgStatement *shapeOwner = get_fortran_separate_shape_declaration();
  if (shapeOwner == nullptr) {
    initializedNameCopy->set_fortran_separate_shape_declaration(nullptr);
    return;
  }

  SgCopyHelp::copiedNodeMapTypeIterator copiedOwner =
      help.get_copiedNodeMap().find(shapeOwner);
  if (copiedOwner == help.get_copiedNodeMap().end()) {
    // A copied declaration without its separate shape statement must emit its
    // semantic array shape directly. Keeping the original cross-edge would
    // suppress that shape and leave the copy dependent on another AST.
    initializedNameCopy->set_fortran_separate_shape_declaration(nullptr);
    return;
  }

  SgStatement *shapeOwnerCopy = isSgStatement(copiedOwner->second);
  SgAttributeSpecificationStatement *attributeOwnerCopy =
      isSgAttributeSpecificationStatement(shapeOwnerCopy);
  SgVariableDeclaration *crayPointerOwnerCopy =
      isSgVariableDeclaration(shapeOwnerCopy);
  if (shapeOwnerCopy == nullptr ||
      (attributeOwnerCopy == nullptr &&
       isSgCommonBlock(shapeOwnerCopy) == nullptr &&
       crayPointerOwnerCopy == nullptr) ||
      (attributeOwnerCopy != nullptr &&
       attributeOwnerCopy->get_attribute_kind() !=
           SgAttributeSpecificationStatement::e_dimensionStatement &&
       attributeOwnerCopy->get_attribute_kind() !=
           SgAttributeSpecificationStatement::e_allocatableStatement &&
       attributeOwnerCopy->get_attribute_kind() !=
           SgAttributeSpecificationStatement::e_pointerStatement) ||
      (crayPointerOwnerCopy != nullptr &&
       (crayPointerOwnerCopy->get_variables().size() != 1 ||
        isSgTypeCrayPointer(crayPointerOwnerCopy->get_variables()
                                .front()
                                ->get_fortran_source_type()) == nullptr))) {
    fprintf(stderr,
            "REX_AST_INVARIANT[fortran-separate-shape-copy]: owner=%p "
            "mapped to an invalid copied node=%p\n",
            static_cast<void *>(shapeOwner),
            static_cast<void *>(copiedOwner->second));
    ROSE_ABORT();
  }
  initializedNameCopy->set_fortran_separate_shape_declaration(shapeOwnerCopy);
}

void SgStatement::fixupCopy_references(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf(
      "Inside of SgStatement::fixupCopy_references() for %p = %s copy = %p \n",
      this, this->class_name().c_str(), copy);
#endif

  SgLocatedNode::fixupCopy_references(copy, help);
}

void SgOmpClauseStatement::fixupCopy_references(SgNode *copy,
                                                SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help,
                                           "openmp-clause-statement");
}

void SgOmpTaskwaitStatement::fixupCopy_references(SgNode *copy,
                                                  SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help,
                                           "openmp-taskwait-statement");
}

void SgAccClauseStatement::fixupCopy_references(SgNode *copy,
                                                SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help,
                                           "openacc-clause-statement");
}

void SgOmpBodyStatement::fixupCopy_references(SgNode *copy,
                                              SgCopyHelp &help) const {
  SgOmpBodyStatement *copiedBodyOwner = isSgOmpBodyStatement(copy);
  if (copiedBodyOwner == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openmp-body-reference-kind]: original=%p/%s "
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
            "REX_COPY_INVARIANT[openmp-body-reference-null]: original=%p "
            "body=%p copy=%p body=%p\n",
            static_cast<const void *>(this), static_cast<void *>(originalBody),
            static_cast<void *>(copiedBodyOwner),
            static_cast<void *>(copiedBody));
    ROSE_ABORT();
  }
  if (originalBody != nullptr) {
    requireExactCopiedReferenceChild(this, originalBody, copiedBodyOwner,
                                     copiedBody, help, "openmp-body-reference");
  }
  fixupExactCopiedSupportReferences(this, copy, help);
}

void SgAccBodyStatement::fixupCopy_references(SgNode *copy,
                                              SgCopyHelp &help) const {
  SgAccBodyStatement *copiedBodyOwner = isSgAccBodyStatement(copy);
  if (copiedBodyOwner == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openacc-body-reference-kind]: original=%p/%s "
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
            "REX_COPY_INVARIANT[openacc-body-reference-null]: original=%p "
            "body=%p copy=%p body=%p\n",
            static_cast<const void *>(this), static_cast<void *>(originalBody),
            static_cast<void *>(copiedBodyOwner),
            static_cast<void *>(copiedBody));
    ROSE_ABORT();
  }
  if (originalBody != nullptr) {
    requireExactCopiedReferenceChild(this, originalBody, copiedBodyOwner,
                                     copiedBody, help,
                                     "openacc-body-reference");
  }
  fixupExactCopiedSupportReferences(this, copy, help);
}

void SgOmpClauseBodyStatement::fixupCopy_references(SgNode *copy,
                                                    SgCopyHelp &help) const {
  SgOmpClauseBodyStatement *copiedClauseBody = isSgOmpClauseBodyStatement(copy);
  if (copiedClauseBody == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openmp-clause-reference-kind]: "
            "original=%p/%s copy=%p/%s\n",
            static_cast<const void *>(this), class_name().c_str(),
            static_cast<void *>(copy),
            copy != nullptr ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  requireExactCopiedReferenceChild(this, get_clause_list(), copiedClauseBody,
                                   copiedClauseBody->get_clause_list(), help,
                                   "openmp-clause-list-reference");
  SgOmpBodyStatement::fixupCopy_references(copy, help);
}

void SgAccClauseBodyStatement::fixupCopy_references(SgNode *copy,
                                                    SgCopyHelp &help) const {
  SgAccClauseBodyStatement *copiedClauseBody = isSgAccClauseBodyStatement(copy);
  if (copiedClauseBody == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openacc-clause-reference-kind]: "
            "original=%p/%s copy=%p/%s\n",
            static_cast<const void *>(this), class_name().c_str(),
            static_cast<void *>(copy),
            copy != nullptr ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  const SgAccClausePtrList &originalClauses = get_clauses();
  const SgAccClausePtrList &copiedClauses = copiedClauseBody->get_clauses();
  if (originalClauses.size() != copiedClauses.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openacc-clause-reference-count]: "
            "original=%p clauses=%zu copy=%p clauses=%zu\n",
            static_cast<const void *>(this), originalClauses.size(),
            static_cast<void *>(copiedClauseBody), copiedClauses.size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalClauses.size(); ++index) {
    requireExactCopiedReferenceChild(this, originalClauses[index],
                                     copiedClauseBody, copiedClauses[index],
                                     help, "openacc-clause-reference");
  }
  SgAccBodyStatement::fixupCopy_references(copy, help);
}

void SgOmpDeclareSimdStatement::fixupCopy_references(SgNode *copy,
                                                     SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help,
                                           "openmp-declare-simd");
}

void SgOmpDeclareVariantStatement::fixupCopy_references(
    SgNode *copy, SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help,
                                           "openmp-declare-variant");
}

void SgOmpBeginDeclareVariantStatement::fixupCopy_references(
    SgNode *copy, SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help,
                                           "openmp-begin-declare-variant");
}

void SgOmpDeclareMapperStatement::fixupCopy_references(SgNode *copy,
                                                       SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help,
                                           "openmp-declare-mapper");
}

void SgOmpDeclareTargetStatement::fixupCopy_references(SgNode *copy,
                                                       SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help,
                                           "openmp-declare-target");
}

void SgOmpRequiresStatement::fixupCopy_references(SgNode *copy,
                                                  SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help, "openmp-requires");
}

void SgOmpAssumesStatement::fixupCopy_references(SgNode *copy,
                                                 SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help, "openmp-assumes");
}

void SgOmpBeginAssumesStatement::fixupCopy_references(SgNode *copy,
                                                      SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help,
                                           "openmp-begin-assumes");
}

void SgOmpGroupprivateStatement::fixupCopy_references(SgNode *copy,
                                                      SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help,
                                           "openmp-groupprivate");
}

void SgOmpThreadprivateStatement::fixupCopy_references(SgNode *copy,
                                                       SgCopyHelp &help) const {
  fixupExactCopiedOwnedSuccessorReferences(this, copy, help,
                                           "openmp-threadprivate");
}

void SgExpression::fixupCopy_references(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf(
      "Inside of SgExpression::fixupCopy_references() for %p = %s copy = %p \n",
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
    (*i_original)->fixupCopy_references(*i_copy, help);
  }

  SgLocatedNode::fixupCopy_references(copy, help);
}

void SgLocatedNode::fixupCopy_references(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgLocatedNode::fixupCopy_references() for %p = %s copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  // printf ("Inside of SgLocatedNode::fixupCopy_references() for %p = %s copy =
  // %p \n",this,this->class_name().c_str(),copy);

#ifndef CXX_IS_ROSE_CODE_GENERATION
  // Fixup references in SgStatements and SgExpressions
  // Define a traversal to update the references to symbols (per statement)
  class Traversal : public AstSimpleProcessing {
  private:
    SgCopyHelp &helpSupport;

  public:
    Traversal(SgCopyHelp &help) : helpSupport(help) {}

    void visit(SgNode *n) {
      switch (n->variantT()) {
      case V_SgThisExp: {
        SgThisExp *thisExpression = isSgThisExp(n);
        ROSE_ASSERT(thisExpression != nullptr);
        SgClassSymbol *sourceClassSymbol = thisExpression->get_class_symbol();
        SgNonrealSymbol *sourceNonrealSymbol =
            thisExpression->get_nonreal_symbol();
        SgClassSymbol *copiedClassSymbol =
            isSgClassSymbol(remapExactCopiedOrExternalReferenceEdge(
                sourceClassSymbol, sourceClassSymbol, helpSupport,
                "this-expression-class-symbol"));
        SgNonrealSymbol *copiedNonrealSymbol =
            isSgNonrealSymbol(remapExactCopiedOrExternalReferenceEdge(
                sourceNonrealSymbol, sourceNonrealSymbol, helpSupport,
                "this-expression-nonreal-symbol"));
        if ((sourceClassSymbol != nullptr) ==
                (sourceNonrealSymbol != nullptr) ||
            (sourceClassSymbol != nullptr && copiedClassSymbol == nullptr) ||
            (sourceNonrealSymbol != nullptr &&
             copiedNonrealSymbol == nullptr)) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[this-expression-symbol-kind]: "
                  "expression=%p class=%p nonreal=%p copied-class=%p "
                  "copied-nonreal=%p\n",
                  static_cast<void *>(thisExpression),
                  static_cast<void *>(sourceClassSymbol),
                  static_cast<void *>(sourceNonrealSymbol),
                  static_cast<void *>(copiedClassSymbol),
                  static_cast<void *>(copiedNonrealSymbol));
          ROSE_ABORT();
        }
        thisExpression->set_class_symbol(copiedClassSymbol);
        thisExpression->set_nonreal_symbol(copiedNonrealSymbol);
        requireExactCopiedOrExternalReferenceEdge(
            sourceClassSymbol, thisExpression->get_class_symbol(), helpSupport,
            "this-expression-class-symbol");
        requireExactCopiedOrExternalReferenceEdge(
            sourceNonrealSymbol, thisExpression->get_nonreal_symbol(),
            helpSupport, "this-expression-nonreal-symbol");
        break;
      }

      case V_SgVarRefExp: {
        SgVarRefExp *varRefExp = isSgVarRefExp(n);
        ROSE_ASSERT(varRefExp != NULL);
        SgVariableSymbol *variableSymbol_original = varRefExp->get_symbol();
        ROSE_ASSERT(variableSymbol_original != NULL);
        SgInitializedName *initializedName_original =
            variableSymbol_original->get_declaration();
        if (initializedName_original !=
            NULL) { // Try to do this first, because the symbol may not have
                    // been copied
          // ROSE_ASSERT(initializedName_original->get_symbol_from_symbol_table());
          SgCopyHelp::copiedNodeMapTypeIterator i =
              helpSupport.get_copiedNodeMap().find(initializedName_original);
          if (i != helpSupport.get_copiedNodeMap().end()) {
            SgInitializedName *initializedName_copy =
                isSgInitializedName(i->second);
            ROSE_ASSERT(initializedName_copy != NULL);
            SgVariableSymbol *symbol_copy =
                findCopiedVariableSymbolInLocalScope(
                    initializedName_copy, variableSymbol_original, helpSupport);
            if (symbol_copy) {
              // printf ("Inside of SgStatement::fixupCopy_references():
              // symbol_copy = %p \n",symbol_copy);
              varRefExp->set_symbol(symbol_copy);
            }
          }
        } else { // This is used for cases such as __PRETTY_FUNCTION__ whose
                 // symbols are not in a symbol table
          SgCopyHelp::copiedNodeMapTypeIterator i =
              helpSupport.get_copiedNodeMap().find(variableSymbol_original);
          if (i != helpSupport.get_copiedNodeMap().end()) {
            SgVariableSymbol *symbol_copy = isSgVariableSymbol(i->second);
            if (symbol_copy) {
              // printf ("Inside of SgStatement::fixupCopy_references():
              // symbol_copy = %p \n",symbol_copy);
              varRefExp->set_symbol(symbol_copy);
            }
          }
        }

        // printf ("Inside of SgStatement::fixupCopy_references(): i !=
        // helpSupport.get_copiedNodeMap().end() = %s \n",
        //      (i != helpSupport.get_copiedNodeMap().end()) ? "true" :
        //      "false");

        break;
      }

      case V_SgTemplateFunctionRefExp: {
        SgTemplateFunctionRefExp *reference = isSgTemplateFunctionRefExp(n);
        ROSE_ASSERT(reference != nullptr);
        SgTemplateFunctionSymbol *original_symbol = reference->get_symbol();
        SgTemplateFunctionDeclaration *original_source =
            original_symbol != nullptr ? isSgTemplateFunctionDeclaration(
                                             original_symbol->get_declaration())
                                       : nullptr;
        SgFunctionDeclaration *original_semantic =
            reference->get_semantic_function_declaration();
        if (original_source == nullptr || original_semantic == nullptr) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[template-function-copy-reference]: "
                  "copied reference=%p has no exact source or semantic "
                  "function identity\n",
                  static_cast<void *>(reference));
          ROSE_ABORT();
        }

        auto source_copy =
            helpSupport.get_copiedNodeMap().find(original_source);
        if (source_copy != helpSupport.get_copiedNodeMap().end()) {
          SgTemplateFunctionDeclaration *copied_source =
              isSgTemplateFunctionDeclaration(source_copy->second);
          SgTemplateFunctionSymbol *copied_symbol =
              copied_source != nullptr
                  ? isSgTemplateFunctionSymbol(
                        copied_source->get_symbol_from_symbol_table())
                  : nullptr;
          if (copied_symbol == nullptr ||
              copied_symbol->get_declaration() != copied_source) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[template-function-copy-reference]: "
                    "source=%p/%s name=%s scope=%p parent=%p copy=%p/%s "
                    "scope=%p parent=%p symbol=%p declaration=%p has no "
                    "exact copied symbol\n",
                    static_cast<void *>(original_source),
                    original_source->class_name().c_str(),
                    original_source->get_name().getString().c_str(),
                    static_cast<void *>(original_source->get_scope()),
                    static_cast<void *>(original_source->get_parent()),
                    static_cast<void *>(copied_source),
                    copied_source != nullptr
                        ? copied_source->class_name().c_str()
                        : "<null>",
                    static_cast<void *>(copied_source != nullptr
                                            ? copied_source->get_scope()
                                            : nullptr),
                    static_cast<void *>(copied_source != nullptr
                                            ? copied_source->get_parent()
                                            : nullptr),
                    static_cast<void *>(copied_symbol),
                    static_cast<void *>(copied_symbol != nullptr
                                            ? copied_symbol->get_declaration()
                                            : nullptr));
            ROSE_ABORT();
          }
          reference->set_symbol(copied_symbol);
        }

        auto semantic_copy =
            helpSupport.get_copiedNodeMap().find(original_semantic);
        if (semantic_copy != helpSupport.get_copiedNodeMap().end()) {
          SgFunctionDeclaration *copied_semantic =
              isSgFunctionDeclaration(semantic_copy->second);
          if (copied_semantic == nullptr) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[template-function-copy-reference]: "
                    "semantic callable edge maps to a non-function node\n");
            ROSE_ABORT();
          }
          reference->set_semantic_function_declaration(copied_semantic);
        }
        if (reference->getAssociatedFunctionDeclaration() == nullptr) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[template-function-copy-reference]: "
                  "copied reference lost its exact semantic callable\n");
          ROSE_ABORT();
        }
        break;
      }

      case V_SgNonrealRefExp: {
        SgNonrealRefExp *reference = isSgNonrealRefExp(n);
        ROSE_ASSERT(reference != nullptr);
        if (reference->get_resolved_function_declaration() != nullptr &&
            reference->get_resolved_variable_declaration() != nullptr) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[nonreal-copy-reference]: copied "
                  "reference=%p resolves to both a function and a variable "
                  "template\n",
                  static_cast<void *>(reference));
          ROSE_ABORT();
        }

        SgNonrealSymbol *original_symbol = reference->get_symbol();
        SgNonrealDecl *original_spelling =
            original_symbol != nullptr ? original_symbol->get_declaration()
                                       : nullptr;
        if ((reference->get_resolved_function_declaration() != nullptr ||
             reference->get_resolved_variable_declaration() != nullptr) &&
            original_spelling == nullptr) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[nonreal-copy-reference]: resolved "
                  "reference has no exact synthetic symbol declaration\n");
          ROSE_ABORT();
        }
        SgNonrealDecl *copied_spelling = nullptr;
        if (original_spelling != nullptr) {
          SgCopyHelp::copiedNodeMapTypeIterator spelling_copy =
              helpSupport.get_copiedNodeMap().find(original_spelling);
          if (spelling_copy != helpSupport.get_copiedNodeMap().end()) {
            copied_spelling = isSgNonrealDecl(spelling_copy->second);
            SgNonrealSymbol *copied_symbol =
                copied_spelling != nullptr
                    ? isSgNonrealSymbol(
                          copied_spelling->get_symbol_from_symbol_table())
                    : nullptr;
            if (copied_symbol == nullptr ||
                copied_symbol->get_declaration() != copied_spelling) {
              fprintf(stderr,
                      "REX_AST_INVARIANT[nonreal-copy-reference]: copied "
                      "synthetic declaration has no exact copied symbol\n");
              ROSE_ABORT();
            }
            reference->set_symbol(copied_symbol);

            if (SgDeclarationStatement *original_template =
                    original_spelling->get_templateDeclaration()) {
              SgCopyHelp::copiedNodeMapTypeIterator template_copy =
                  helpSupport.get_copiedNodeMap().find(original_template);
              if (template_copy != helpSupport.get_copiedNodeMap().end()) {
                SgDeclarationStatement *copied_template =
                    isSgDeclarationStatement(template_copy->second);
                if (copied_template == nullptr) {
                  fprintf(stderr,
                          "REX_AST_INVARIANT[nonreal-copy-reference]: "
                          "synthetic semantic edge maps to a non-declaration "
                          "node\n");
                  ROSE_ABORT();
                }
                copied_spelling->set_templateDeclaration(copied_template);
              }
            }
          }
        }

        if (SgFunctionDeclaration *original_function =
                reference->get_resolved_function_declaration()) {
          SgCopyHelp::copiedNodeMapTypeIterator function_copy =
              helpSupport.get_copiedNodeMap().find(original_function);
          if (function_copy != helpSupport.get_copiedNodeMap().end()) {
            SgFunctionDeclaration *copied_function =
                isSgFunctionDeclaration(function_copy->second);
            if (copied_function == nullptr || copied_spelling == nullptr) {
              fprintf(stderr,
                      "REX_AST_INVARIANT[nonreal-copy-reference]: resolved "
                      "function edge cannot be rebound without exact copied "
                      "function and synthetic identities\n");
              ROSE_ABORT();
            }
            reference->set_resolved_function_declaration(copied_function);
          }
        }

        if (SgTemplateVariableDeclaration *original_variable =
                reference->get_resolved_variable_declaration()) {
          SgCopyHelp::copiedNodeMapTypeIterator variable_copy =
              helpSupport.get_copiedNodeMap().find(original_variable);
          if (variable_copy != helpSupport.get_copiedNodeMap().end()) {
            SgTemplateVariableDeclaration *copied_variable =
                isSgTemplateVariableDeclaration(variable_copy->second);
            if (copied_variable == nullptr || copied_spelling == nullptr ||
                original_spelling == nullptr ||
                original_spelling->get_templateDeclaration() !=
                    original_variable) {
              fprintf(stderr,
                      "REX_AST_INVARIANT[nonreal-copy-reference]: resolved "
                      "variable edge cannot be rebound without the exact "
                      "copied synthetic identity\n");
              ROSE_ABORT();
            }
            reference->set_resolved_variable_declaration(copied_variable);
            copied_spelling->set_templateDeclaration(copied_variable);

            std::set<SgTemplateVariableDeclaration *> visited_templates;
            SgTemplateVariableDeclaration *original_link = original_variable;
            while (original_link != nullptr &&
                   original_link->get_specializedTemplateDeclaration() !=
                       nullptr) {
              if (!visited_templates.insert(original_link).second) {
                fprintf(stderr,
                        "REX_AST_INVARIANT[nonreal-copy-reference]: original "
                        "variable-template specialization chain contains a "
                        "cycle\n");
                ROSE_ABORT();
              }
              SgTemplateVariableDeclaration *original_primary =
                  isSgTemplateVariableDeclaration(
                      original_link->get_specializedTemplateDeclaration());
              if (original_primary == nullptr) {
                fprintf(stderr,
                        "REX_AST_INVARIANT[nonreal-copy-reference]: original "
                        "variable specialization has a wrong-kind source "
                        "template edge\n");
                ROSE_ABORT();
              }
              SgCopyHelp::copiedNodeMapTypeIterator copied_link_entry =
                  helpSupport.get_copiedNodeMap().find(original_link);
              SgCopyHelp::copiedNodeMapTypeIterator copied_primary_entry =
                  helpSupport.get_copiedNodeMap().find(original_primary);
              if (copied_link_entry != helpSupport.get_copiedNodeMap().end() &&
                  copied_primary_entry !=
                      helpSupport.get_copiedNodeMap().end()) {
                SgTemplateVariableDeclaration *copied_link =
                    isSgTemplateVariableDeclaration(copied_link_entry->second);
                SgTemplateVariableDeclaration *copied_primary =
                    isSgTemplateVariableDeclaration(
                        copied_primary_entry->second);
                if (copied_link == nullptr || copied_primary == nullptr) {
                  fprintf(
                      stderr,
                      "REX_AST_INVARIANT[nonreal-copy-reference]: copied "
                      "variable-template chain maps to a wrong-kind node\n");
                  ROSE_ABORT();
                }
                copied_link->set_specializedTemplateDeclaration(copied_primary);
              }
              original_link = original_primary;
            }
          }
        }
        break;
      }

      case V_SgFunctionRefExp: {
        SgFunctionRefExp *functionRefExp = isSgFunctionRefExp(n);
        ROSE_ASSERT(functionRefExp != NULL);
        SgFunctionSymbol *functionSymbol_original =
            functionRefExp->get_symbol();
        ROSE_ASSERT(functionSymbol_original != NULL);
        SgFunctionSymbol *sourceVisibleSymbol_original =
            functionRefExp->get_fortran_source_visible_symbol();
        SgFunctionDeclaration *functionDeclaration_original =
            functionSymbol_original->get_declaration();
        ROSE_ASSERT(functionDeclaration_original != NULL);
        SgCopyHelp::copiedNodeMapTypeIterator i =
            helpSupport.get_copiedNodeMap().find(functionDeclaration_original);

        // printf ("Inside of SgStatement::fixupCopy_references(): (case
        // SgFunctionRefExp) i != helpSupport.get_copiedNodeMap().end() = %s
        // \n",
        //      (i != helpSupport.get_copiedNodeMap().end()) ? "true" :
        //      "false");

        // If the declaration is in the map then it is because we have copied it
        // previously and thus it symbol should be updated to reflect the copied
        // declaration. ROSE_ASSERT(i != help.get_copiedNodeMap().end());
        if (i != helpSupport.get_copiedNodeMap().end()) {
          SgFunctionDeclaration *functionDeclaration_copy =
              isSgFunctionDeclaration(i->second);
          ROSE_ASSERT(functionDeclaration_copy != NULL);
          SgSymbol *symbol_copy =
              functionDeclaration_copy->get_symbol_from_symbol_table();
          // printf ("Inside of SgStatement::fixupCopy_references(): symbol_copy
          // = %p \n",symbol_copy);
          if (symbol_copy != NULL) {
            SgFunctionSymbol *functionSymbol_copy =
                isSgFunctionSymbol(symbol_copy);
            ROSE_ASSERT(functionSymbol_copy != NULL);
            functionRefExp->set_symbol(functionSymbol_copy);
          } else {
            if (SgProject::get_verbose() > 0)
              printf("Error: could not find symbol associated with "
                     "functionDeclaration_copy = %p = %s = %s \n",
                     functionDeclaration_copy,
                     functionDeclaration_copy->class_name().c_str(),
                     SageInterface::get_name(functionDeclaration_copy).c_str());
            // ROSE_ABORT();
          }
        }
        if (sourceVisibleSymbol_original != nullptr) {
          SgFunctionDeclaration *sourceVisibleDeclaration_original =
              sourceVisibleSymbol_original->get_declaration();
          if (sourceVisibleDeclaration_original == nullptr) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[fortran-function-copy-reference]: "
                    "source-visible symbol=%p has no exact declaration\n",
                    static_cast<void *>(sourceVisibleSymbol_original));
            ROSE_ABORT();
          }
          auto sourceVisibleCopy = helpSupport.get_copiedNodeMap().find(
              sourceVisibleDeclaration_original);
          if (sourceVisibleCopy != helpSupport.get_copiedNodeMap().end()) {
            SgFunctionDeclaration *sourceVisibleDeclaration_copy =
                isSgFunctionDeclaration(sourceVisibleCopy->second);
            SgFunctionSymbol *sourceVisibleSymbol_copy =
                sourceVisibleDeclaration_copy != nullptr
                    ? isSgFunctionSymbol(sourceVisibleDeclaration_copy
                                             ->get_symbol_from_symbol_table())
                    : nullptr;
            if (sourceVisibleSymbol_copy == nullptr ||
                sourceVisibleSymbol_copy->get_declaration() !=
                    sourceVisibleDeclaration_copy) {
              fprintf(stderr,
                      "REX_AST_INVARIANT[fortran-function-copy-reference]: "
                      "copied source-visible declaration has no exact copied "
                      "function symbol\n");
              ROSE_ABORT();
            }
            functionRefExp->set_fortran_source_visible_symbol(
                sourceVisibleSymbol_copy);
          }
        }
        break;
      }

      case V_SgTemplateMemberFunctionRefExp: {
        SgTemplateMemberFunctionRefExp *reference =
            isSgTemplateMemberFunctionRefExp(n);
        ROSE_ASSERT(reference != nullptr);
        SgTemplateMemberFunctionSymbol *original_symbol =
            reference->get_symbol();
        SgTemplateMemberFunctionDeclaration *original_source =
            original_symbol != nullptr ? isSgTemplateMemberFunctionDeclaration(
                                             original_symbol->get_declaration())
                                       : nullptr;
        SgMemberFunctionDeclaration *original_semantic =
            reference->get_semantic_member_function_declaration();
        if (original_source == nullptr || original_semantic == nullptr) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[template-member-function-copy-reference]: "
                  "copied reference=%p has no exact source or semantic "
                  "member-function identity\n",
                  static_cast<void *>(reference));
          ROSE_ABORT();
        }

        auto source_copy =
            helpSupport.get_copiedNodeMap().find(original_source);
        if (source_copy != helpSupport.get_copiedNodeMap().end()) {
          SgTemplateMemberFunctionDeclaration *copied_source =
              isSgTemplateMemberFunctionDeclaration(source_copy->second);
          SgTemplateMemberFunctionSymbol *copied_symbol =
              copied_source != nullptr
                  ? isSgTemplateMemberFunctionSymbol(
                        copied_source->get_symbol_from_symbol_table())
                  : nullptr;
          if (copied_symbol == nullptr ||
              copied_symbol->get_declaration() != copied_source) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[template-member-function-copy-"
                    "reference]: copied source template has no exact copied "
                    "symbol\n");
            ROSE_ABORT();
          }
          reference->set_symbol(copied_symbol);
        }

        auto semantic_copy =
            helpSupport.get_copiedNodeMap().find(original_semantic);
        if (semantic_copy != helpSupport.get_copiedNodeMap().end()) {
          SgMemberFunctionDeclaration *copied_semantic =
              isSgMemberFunctionDeclaration(semantic_copy->second);
          if (copied_semantic == nullptr) {
            fprintf(stderr, "REX_AST_INVARIANT[template-member-function-copy-"
                            "reference]: semantic callable edge maps to a "
                            "non-member-function node\n");
            ROSE_ABORT();
          }
          reference->set_semantic_member_function_declaration(copied_semantic);
        }
        if (reference->getAssociatedMemberFunctionDeclaration() == nullptr) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[template-member-function-copy-reference]: "
                  "copied reference lost its exact semantic callable\n");
          ROSE_ABORT();
        }
        break;
      }

      case V_SgMemberFunctionRefExp: {
        SgMemberFunctionRefExp *functionRefExp = isSgMemberFunctionRefExp(n);
        ROSE_ASSERT(functionRefExp != NULL);
        SgMemberFunctionSymbol *functionSymbol_original =
            functionRefExp->get_symbol();
        ROSE_ASSERT(functionSymbol_original != NULL);
        SgMemberFunctionDeclaration *functionDeclaration_original =
            functionSymbol_original->get_declaration();
        ROSE_ASSERT(functionDeclaration_original != NULL);
        SgCopyHelp::copiedNodeMapTypeIterator i =
            helpSupport.get_copiedNodeMap().find(functionDeclaration_original);
        // If the declaration is in the map then it is because we have copied it
        // previously and thus it symbol should be updated to reflect the copied
        // declaration. ROSE_ASSERT(i != help.get_copiedNodeMap().end());
        if (i != helpSupport.get_copiedNodeMap().end()) {
          SgMemberFunctionDeclaration *functionDeclaration_copy =
              isSgMemberFunctionDeclaration(i->second);
          ROSE_ASSERT(functionDeclaration_copy != NULL);
          SgSymbol *symbol_copy =
              functionDeclaration_copy->get_symbol_from_symbol_table();
          // printf ("Inside of SgStatement::fixupCopy_references(): symbol_copy
          // = %p \n",symbol_copy);
          if (symbol_copy != NULL) {
            SgMemberFunctionSymbol *functionSymbol_copy =
                isSgMemberFunctionSymbol(symbol_copy);
            ROSE_ASSERT(functionSymbol_copy != NULL);
            functionRefExp->set_symbol(functionSymbol_copy);
          } else {
            if (SgProject::get_verbose() > 0)
              printf("Error: could not find symbol associated with (member) "
                     "functionDeclaration_copy = %p = %s = %s \n",
                     functionDeclaration_copy,
                     functionDeclaration_copy->class_name().c_str(),
                     SageInterface::get_name(functionDeclaration_copy).c_str());
            // ROSE_ABORT();
          }
        }
        break;
      }

        // DQ (10/16/2007): Note that labels are handled by the SgLabelStatement
        // and the SgGotoStatement directly and are not required to be processed
        // here in the SgStatement.
      case V_SgLabelStatement: {
        // printf ("Inside of SgStatement::fixupCopy_references(): we might have
        // to handle SgLabelStatement \n");
        break;
      }

      default: {
        // Nothing to do for this case
      }
      }
    }
  };

  // Build an run the traversal defined above.
  Traversal t(help);
  t.traverse(copy, preorder);

  SgLocatedNode *copyLocatedNode = isSgLocatedNode(copy);
  ROSE_ASSERT(copyLocatedNode != NULL);

  // DQ (10/24/2007): New test.
  ROSE_ASSERT(copyLocatedNode->variantT() == this->variantT());
#endif

#if DEBUG_FIXUP_COPY
  printf("Leaving SgLocatedNode::fixupCopy_references() \n\n");
#endif
}

void SgScopeStatement::fixupCopy_references(SgNode *copy,
                                            SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgScopeStatement::fixupCopy_references() for %p = %s copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgScopeStatement *copyScopeStatement = isSgScopeStatement(copy);
  ROSE_ASSERT(copyScopeStatement != NULL);

  // Call the base class fixupCopy member function
  SgStatement::fixupCopy_references(copy, help);
}

void SgGlobal::fixupCopy_references(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgGlobal::fixupCopy_references() for %p copy = %p \n", this,
         copy);
#endif

  SgGlobal *global_copy = isSgGlobal(copy);
  ROSE_ASSERT(global_copy != NULL);

  fixupCanonicalDeclarationCopiesForReferences(this->getDeclarationList(),
                                               global_copy, help);

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_references(copy, help);

  // printf ("\nLeaving SgGlobal::fixupCopy_references() this = %p = %s  copy =
  // %p \n",this,this->class_name().c_str(),copy);
}

// JJW 2/1/2008 -- Added fixup to allow statement expressions to be handled
void SgExprStatement::fixupCopy_references(SgNode *copy,
                                           SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgExprStatement::fixupCopy_references() for %p = %s copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgExprStatement *es_copy = isSgExprStatement(copy);
  ROSE_ASSERT(es_copy != NULL);

  SgExpression *expression_original = this->get_expression();
  SgExpression *expression_copy = es_copy->get_expression();

  expression_original->fixupCopy_references(expression_copy, help);

  // Call the base class fixupCopy member function
  SgStatement::fixupCopy_references(copy, help);

  // printf ("\nLeaving SgBasicBlock::fixupCopy_references() this = %p = %s copy
  // = %p \n",this,this->class_name().c_str(),copy);
}

// DQ (10/6/2007): Added fixup function to set scopes not set properly by the
// ROSETTA generated copy!
void SgBasicBlock::fixupCopy_references(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf(
      "Inside of SgBasicBlock::fixupCopy_references() for %p = %s copy = %p \n",
      this, this->class_name().c_str(), copy);
#endif

  SgBasicBlock *block_copy = isSgBasicBlock(copy);
  ROSE_ASSERT(block_copy != NULL);

  fixupCanonicalStatementCopiesForReferences(this->getStatementList(),
                                             block_copy, help);

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_references(copy, help);

  // printf ("\nLeaving SgBasicBlock::fixupCopy_references() this = %p = %s copy
  // = %p \n",this,this->class_name().c_str(),copy);
}

void SgDeclarationStatement::fixupCopy_references(SgNode *copy,
                                                  SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgDeclarationStatement::fixupCopy_references() for %p = %s "
         "copy = %p (defining = %p firstNondefining = %p) \n",
         this, this->class_name().c_str(), copy,
         this->get_definingDeclaration(),
         this->get_firstNondefiningDeclaration());
#endif

  SgDeclarationStatement *copiedDeclaration = isSgDeclarationStatement(copy);
  if (copiedDeclaration == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[declaration-reference-copy]: original=%p/%s "
            "copy=%p\n",
            static_cast<const void *>(this), class_name().c_str(),
            static_cast<void *>(copy));
    ROSE_ABORT();
  }
  requireExactCopiedReferenceRoot(this, copiedDeclaration, help, "declaration");
  fixupExactCopiedOwnedDeclarationScopeReferences(this, copiedDeclaration,
                                                  help);
  fixupExactCopiedTemplateParameterReferences(this, copiedDeclaration, help);

  // Call the base class fixupCopy member function (this will setup the parent)
  SgStatement::fixupCopy_references(copy, help);
}

void SgFunctionDeclaration::fixupCopy_references(SgNode *copy,
                                                 SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("\nIn SgFunctionDeclaration::fixupCopy_references(): for function = "
         "%s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  SgFunctionDeclaration *functionDeclaration_copy =
      isSgFunctionDeclaration(copy);
  ROSE_ASSERT(functionDeclaration_copy != NULL);

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_references(copy, help);

  fixupExactCopiedFunctionDeclaratorScopeReferences(
      this, functionDeclaration_copy, help);

  // Setup the scopes of the SgInitializedName objects in the paraleter list
  ROSE_ASSERT(get_parameterList() != NULL);
  get_parameterList()->fixupCopy_references(
      functionDeclaration_copy->get_parameterList(), help);

  // Setup the details in the SgFunctionDefinition (this may have to rebuild the
  // sysmbol table) printf ("In SgFunctionDeclaration::fixupCopy_references():
  // this->get_definition() = %p \n",this->get_definition());
  if (this->get_definition() != NULL) {
    ROSE_ASSERT(isForward() == false);

    // DQ (2/26/2009): Handle special cases where the copyHelp function is
    // non-trivial. Is every version of copyHelp object going to be a problem?

    // For the outlining, our copyHelp object does not copy defining function
    // declarations and substitutes a non-defining declarations, so if the copy
    // has been built this way then skip trying to reset the
    // SgFunctionDefinition. printf ("In
    // SgFunctionDeclaration::fixupCopy_references():
    // functionDeclaration_copy->get_definition() = %p
    // \n",functionDeclaration_copy->get_definition());
    // this->get_definition()->fixupCopy_references(functionDeclaration_copy->get_definition(),help);
    if (functionDeclaration_copy->get_definition() != NULL) {
      this->get_definition()->fixupCopy_references(
          functionDeclaration_copy->get_definition(), help);
    }

    // If this is a declaration with a definition then it is a defining
    // declaration
    // functionDeclaration_copy->set_definingDeclaration(functionDeclaration_copy);
  }

  // printf ("\nLeaving SgFunctionDeclaration::fixupCopy_references(): for
  // function = %s = %p = %s copy = %p
  // \n",this->get_name().str(),this,this->class_name().c_str(),copy);
}

// DQ (10/6/2007): Added fixup function to set scopes not set properly by the
// ROSETTA generated copy!
void SgFunctionParameterList::fixupCopy_references(SgNode *copy,
                                                   SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgFunctionParameterList::fixupCopy_references() for %p = "
         "%s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Also call the base class version of the fixupCopycopy() member function
  SgDeclarationStatement::fixupCopy_references(copy, help);

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
    (*i_original)->fixupCopy_references(*i_copy, help);

    i_original++;
    i_copy++;
  }
}

void SgMemberFunctionDeclaration::fixupCopy_references(SgNode *copy,
                                                       SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("In SgMemberFunctionDeclaration::fixupCopy_references(): for function "
         "= %s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  SgMemberFunctionDeclaration *memberFunctionDeclaration_copy =
      isSgMemberFunctionDeclaration(copy);
  ROSE_ASSERT(memberFunctionDeclaration_copy != NULL);

  // Setup the scopes of the SgInitializedName objects in the paraleter list
  ROSE_ASSERT(get_CtorInitializerList() != NULL);
  get_CtorInitializerList()->fixupCopy_references(
      memberFunctionDeclaration_copy->get_CtorInitializerList(), help);

  // Call the base class fixupCopy member function
  SgFunctionDeclaration::fixupCopy_references(copy, help);
}

void SgTemplateParameter::fixupCopy_references(SgNode *copy,
                                               SgCopyHelp &help) const {
  SgTemplateParameter *copiedParameter = isSgTemplateParameter(copy);
  if (copiedParameter == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-reference-copy]: "
            "original=%p copy=%p\n",
            static_cast<const void *>(this), static_cast<void *>(copy));
    ROSE_ABORT();
  }
  requireExactCopiedReferenceRoot(this, copiedParameter, help,
                                  "template-parameter");
  requireExactCopiedOrExternalReferenceEdge(
      get_templateDeclaration(), copiedParameter->get_templateDeclaration(),
      help, "template-parameter-declaration");
  fixupExactCopiedSupportReferences(this, copiedParameter, help);
}

void SgTemplateDeclaration::fixupCopy_references(SgNode *copy,
                                                 SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("\nInside of SgTemplateDeclaration::fixupCopy_references() for %p = "
         "%s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Also call the base class version of the fixupCopycopy() member function
  SgDeclarationStatement::fixupCopy_references(copy, help);
}

void SgTemplateInstantiationDefn::fixupCopy_references(SgNode *copy,
                                                       SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgTemplateInstantiationDefn::fixupCopy_references() for "
         "class = %s class definition %p = %s copy = %p \n",
         this->get_declaration()->get_name().str(), this,
         this->class_name().c_str(), copy);
#endif

  // Call the base class fixupCopy member function
  SgClassDefinition::fixupCopy_references(copy, help);
}

void SgTemplateInstantiationDecl::fixupCopy_references(SgNode *copy,
                                                       SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("\nIn SgTemplateInstantiationDecl::fixupCopy_references(): for "
         "function = %s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  SgTemplateInstantiationDecl *templateClassDeclaration_copy =
      isSgTemplateInstantiationDecl(copy);
  ROSE_ASSERT(templateClassDeclaration_copy != NULL);

  // Also call the base class version of the fixupCopycopy() member function
  SgClassDeclaration::fixupCopy_references(copy, help);
}

void SgTemplateInstantiationMemberFunctionDecl::fixupCopy_references(
    SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf(
      "\nIn SgTemplateInstantiationMemberFunctionDecl::fixupCopy_references(): "
      "for function = %s = %p = %s copy = %p \n",
      this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  SgTemplateInstantiationMemberFunctionDecl
      *templateMemberFunctionDeclaration_copy =
          isSgTemplateInstantiationMemberFunctionDecl(copy);
  ROSE_ASSERT(templateMemberFunctionDeclaration_copy != NULL);

  // Also call the base class version of the fixupCopycopy() member function
  SgMemberFunctionDeclaration::fixupCopy_references(copy, help);
}

void SgTemplateInstantiationFunctionDecl::fixupCopy_references(
    SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("\nIn SgTemplateInstantiationFunctionDecl::fixupCopy_references(): "
         "for function = %s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  SgTemplateInstantiationFunctionDecl *templateFunctionDeclaration_copy =
      isSgTemplateInstantiationFunctionDecl(copy);
  ROSE_ASSERT(templateFunctionDeclaration_copy != NULL);

  // Also call the base class version of the fixupCopycopy() member function
  SgFunctionDeclaration::fixupCopy_references(copy, help);
}

void SgFunctionDefinition::fixupCopy_references(SgNode *copy,
                                                SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgFunctionDefinition::fixupCopy_references() for %p = %s "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgFunctionDefinition *functionDefinition_copy = isSgFunctionDefinition(copy);
  ROSE_ASSERT(functionDefinition_copy != NULL);

  // Setup the scopes of the SgInitializedName objects in the paraleter list
  ROSE_ASSERT(get_body() != NULL);
  get_body()->fixupCopy_references(functionDefinition_copy->get_body(), help);

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_references(copy, help);

  // printf ("\nLeaving SgFunctionDefinition::fixupCopy_references() for %p = %s
  // copy = %p \n\n",this,this->class_name().c_str(),copy);
}

void SgVariableDeclaration::fixupCopy_references(SgNode *copy,
                                                 SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgVariableDeclaration::fixupCopy_references() for %p = %s "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Also call the base class version of the fixupCopycopy() member function
  SgDeclarationStatement::fixupCopy_references(copy, help);

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
            "REX_COPY_INVARIANT[variable-base-type-forward-reference-null]: "
            "source=%p child=%p copy=%p child=%p\n",
            static_cast<const void *>(this),
            static_cast<void *>(baseTypeNondefiningOriginal),
            static_cast<void *>(variableDeclaration_copy),
            static_cast<void *>(baseTypeNondefiningCopy));
    ROSE_ABORT();
  }
  if (baseTypeNondefiningOriginal != NULL) {
    requireExactCopiedReferenceChild(
        this, baseTypeNondefiningOriginal, variableDeclaration_copy,
        baseTypeNondefiningCopy, help, "variable-base-type-forward");
    baseTypeNondefiningOriginal->fixupCopy_references(baseTypeNondefiningCopy,
                                                      help);
  }

  // Preserve references for the exact inline base-type definition child.
  if (this->get_baseTypeDefiningDeclaration() != NULL) {
    ROSE_ASSERT(variableDeclaration_copy->get_baseTypeDefiningDeclaration() !=
                NULL);
    SgDeclarationStatement *baseTypeDeclaration_original =
        this->get_baseTypeDefiningDeclaration();
    SgDeclarationStatement *baseTypeDeclaration_copy =
        variableDeclaration_copy->get_baseTypeDefiningDeclaration();

    ROSE_ASSERT(baseTypeDeclaration_original != NULL);
    ROSE_ASSERT(baseTypeDeclaration_copy != NULL);

    // printf ("In SgVariableDeclaration::fixupCopy_references(): Calling
    // fixupCopy on %p = %s
    // \n",baseTypeDeclaration_original,baseTypeDeclaration_original->class_name().c_str());

    baseTypeDeclaration_original->fixupCopy_references(baseTypeDeclaration_copy,
                                                       help);
  }

  const SgInitializedNamePtrList &variableList_original = this->get_variables();
  SgInitializedNamePtrList &variableList_copy =
      variableDeclaration_copy->get_variables();

  // printf ("Inside of SgVariableDeclaration::fixupCopy_references():
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

    (*i_original)->fixupCopy_references(*i_copy, help);

    i_original++;
    i_copy++;
  }
}

void SgClassDeclaration::fixupCopy_references(SgNode *copy,
                                              SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgClassDeclaration::fixupCopy_references() for class = %s "
         "= %p = %s copy = %p (defining = %p firstNondefining = %p) \n",
         this->get_name().str(), this, this->class_name().c_str(), copy,
         this->get_definingDeclaration(),
         this->get_firstNondefiningDeclaration());
#endif

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_references(copy, help);

  if (isForward() == false) {
    SgClassDeclaration *classDeclaration_copy = isSgClassDeclaration(copy);
    ROSE_ASSERT(classDeclaration_copy != NULL);

    SgClassDefinition *classDefinition_original = this->get_definition();
    SgClassDefinition *classDefinition_copy =
        classDeclaration_copy->get_definition();

    classDefinition_original->fixupCopy_references(classDefinition_copy, help);
  }
}

void SgClassDefinition::fixupCopy_references(SgNode *copy,
                                             SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgClassDefinition::fixupCopy_references() for class = %s "
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

    (*i_original)->fixupCopy_references(*i_copy, help);

    // DQ (11/7/2007): This is no longer required now that we separate out the
    // fixup of the parent/scope from the symbols and the references.
    // (*i_copy)->set_parent(classDefinition_copy);

    ROSE_ASSERT((*i_copy)->get_parent() != NULL);
    ROSE_ASSERT((*i_copy)->get_parent() == classDefinition_copy);

    i_original++;
    i_copy++;
  }

  fixupCanonicalDeclarationCopiesForReferences(this->getDeclarationList(),
                                               classDefinition_copy, help);

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_references(copy, help);

  // printf ("\nLeaving SgClassDefinition::fixupCopy_references() this = %p = %s
  // copy = %p \n",this,this->class_name().c_str(),copy);
}

void SgBaseClass::fixupCopy_references(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgBaseClass::fixupCopy_references() for baseclass = %p = "
         "%s  copy = %p \n",
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

void SgLabelStatement::fixupCopy_references(SgNode *copy,
                                            SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgLabelStatement::fixupCopy_references() for %p = %s copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Also call the base class version of the fixupCopycopy() member function
  SgStatement::fixupCopy_references(copy, help);
}

void SgGotoStatement::fixupCopy_references(SgNode *copy,
                                           SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgGotoStatement::fixupCopy_references() for %p = %s copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Also call the base class version of the fixupCopycopy() member function
  SgStatement::fixupCopy_references(copy, help);
}

void SgTypedefDeclaration::fixupCopy_references(SgNode *copy,
                                                SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgTypedefDeclaration::fixupCopy_references() for typedef "
         "name = %s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_references(copy, help);

  SgTypedefDeclaration *typedefDeclaration_copy = isSgTypedefDeclaration(copy);
  ROSE_ASSERT(typedefDeclaration_copy != NULL);

  // DQ (10/14/2007): Handle the case of a type defined in the base type of the
  // typedef (similar problem for SgVariableDeclaration).
  if (this->get_typedefBaseTypeContainsDefiningDeclaration() == true) {
    ROSE_ASSERT(typedefDeclaration_copy
                    ->get_typedefBaseTypeContainsDefiningDeclaration() == true);
    SgDeclarationStatement *baseTypeDeclaration_original =
        this->get_baseTypeDefiningDeclaration();
    SgDeclarationStatement *baseTypeDeclaration_copy =
        typedefDeclaration_copy->get_baseTypeDefiningDeclaration();
    ROSE_ASSERT(baseTypeDeclaration_original != NULL);
    ROSE_ASSERT(baseTypeDeclaration_copy != NULL);

    baseTypeDeclaration_original->fixupCopy_references(baseTypeDeclaration_copy,
                                                       help);
  }
}

void SgEnumDeclaration::fixupCopy_references(SgNode *copy,
                                             SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgEnumDeclaration::fixupCopy_references() for %p = %s copy "
         "= %p (defining = %p firstNondefining = %p) \n",
         this, this->class_name().c_str(), copy,
         this->get_definingDeclaration(),
         this->get_firstNondefiningDeclaration());
#endif

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_references(copy, help);

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
    (*i_original)->fixupCopy_references(*i_copy, help);

    i_original++;
    i_copy++;
  }
}

void SgNamespaceDeclarationStatement::fixupCopy_references(
    SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgNamespaceDeclarationStatement::fixupCopy_references() "
         "for %p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_references(copy, help);

  SgNamespaceDeclarationStatement *namespaceDeclaration_copy =
      isSgNamespaceDeclarationStatement(copy);
  ROSE_ASSERT(namespaceDeclaration_copy != NULL);

  SgNamespaceDefinitionStatement *namespaceDefinition_original =
      this->get_definition();
  SgNamespaceDefinitionStatement *namespaceDefinition_copy =
      namespaceDeclaration_copy->get_definition();

  ROSE_ASSERT(namespaceDefinition_original != NULL);
  ROSE_ASSERT(namespaceDefinition_copy != NULL);

  namespaceDefinition_original->fixupCopy_references(namespaceDefinition_copy,
                                                     help);
}

void SgNamespaceDefinitionStatement::fixupCopy_references(
    SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgNamespaceDefinitionStatement::fixupCopy_references() for "
         "%p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgNamespaceDefinitionStatement *namespaceDefinition_copy =
      isSgNamespaceDefinitionStatement(copy);
  ROSE_ASSERT(namespaceDefinition_copy != NULL);

  fixupCanonicalDeclarationCopiesForReferences(this->getDeclarationList(),
                                               namespaceDefinition_copy, help);

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_references(copy, help);
}

void SgTemplateInstantiationDirectiveStatement::fixupCopy_references(
    SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of "
         "SgTemplateInstantiationDirectiveStatement::fixupCopy_references() "
         "for %p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_references(copy, help);
}

void SgProject::fixupCopy_references(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgProject::fixupCopy_references() \n");
#endif

  SgProject *project_copy = isSgProject(copy);
  ROSE_ASSERT(project_copy != NULL);

  // Call fixup on all fo the files (SgFile objects)
  for (int i = 0; i < numberOfFiles(); i++) {
    SgFile &file = get_file(i);
    SgFile &file_copy = project_copy->get_file(i);
    file.fixupCopy_references(&file_copy, help);
  }
}

void SgSourceFile::fixupCopy_references(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgFile::fixupCopy_references() \n");
#endif

  SgSourceFile *file_copy = isSgSourceFile(copy);
  ROSE_ASSERT(file_copy != NULL);

  // Call fixup on the global scope
  ROSE_ASSERT(get_globalScope() != NULL);
  ROSE_ASSERT(file_copy->get_globalScope() != NULL);
  get_globalScope()->fixupCopy_references(file_copy->get_globalScope(), help);
}

void SgIfStmt::fixupCopy_references(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf(
      "Inside of SgIfStmt::fixupCopy_references() this = %p = %s  copy = %p \n",
      this, this->class_name().c_str(), copy);
#endif

  // SgStatement::fixupCopy_references(copy,help);
  SgScopeStatement::fixupCopy_references(copy, help);

  SgIfStmt *ifStatement_copy = isSgIfStmt(copy);
  ROSE_ASSERT(ifStatement_copy != NULL);

  this->get_conditional()->fixupCopy_references(
      ifStatement_copy->get_conditional(), help);

  // printf ("\nProcess the true body of the SgIfStmt \n\n");

  this->get_true_body()->fixupCopy_references(ifStatement_copy->get_true_body(),
                                              help);

  // printf ("\nProcess the false body of the SgIfStmt \n\n");

  if (this->get_false_body()) {
    this->get_false_body()->fixupCopy_references(
        ifStatement_copy->get_false_body(), help);
  }

  // printf ("\nLeaving SgIfStmt::fixupCopy_references() this = %p = %s  copy =
  // %p \n",this,this->class_name().c_str(),copy);
}

void SgForStatement::fixupCopy_references(SgNode *copy,
                                          SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgForStatement::fixupCopy_references() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgScopeStatement::fixupCopy_references(copy, help);

  SgForStatement *forStatement_copy = isSgForStatement(copy);
  ROSE_ASSERT(forStatement_copy != NULL);

  ROSE_ASSERT(this->get_for_init_stmt() != NULL);
  this->get_for_init_stmt()->fixupCopy_references(
      forStatement_copy->get_for_init_stmt(), help);

  ROSE_ASSERT(this->get_test() != NULL);
  this->get_test()->fixupCopy_references(forStatement_copy->get_test(), help);

  ROSE_ASSERT(this->get_increment() != NULL);
  this->get_increment()->fixupCopy_references(
      forStatement_copy->get_increment(), help);

  ROSE_ASSERT(this->get_loop_body() != NULL);
  this->get_loop_body()->fixupCopy_references(
      forStatement_copy->get_loop_body(), help);
}

void SgRangeBasedForStatement::fixupCopy_references(SgNode *copy,
                                                    SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgRangeBasedForStatement::fixupCopy_references() this = %p "
         "= %s  copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgScopeStatement::fixupCopy_references(copy, help);

  SgRangeBasedForStatement *forStatement_copy =
      isSgRangeBasedForStatement(copy);
  ROSE_ASSERT(forStatement_copy != NULL);

  ROSE_ASSERT(this->get_iterator_declaration() != NULL);
  this->get_iterator_declaration()->fixupCopy_references(
      forStatement_copy->get_iterator_declaration(), help);

  ROSE_ASSERT(this->get_range_declaration() != NULL);
  this->get_range_declaration()->fixupCopy_references(
      forStatement_copy->get_range_declaration(), help);

  ROSE_ASSERT(this->get_begin_declaration() != NULL);
  this->get_begin_declaration()->fixupCopy_references(
      forStatement_copy->get_begin_declaration(), help);

  ROSE_ASSERT(this->get_end_declaration() != NULL);
  this->get_end_declaration()->fixupCopy_references(
      forStatement_copy->get_end_declaration(), help);

  ROSE_ASSERT(this->get_not_equal_expression() != NULL);
  this->get_not_equal_expression()->fixupCopy_references(
      forStatement_copy->get_not_equal_expression(), help);

  ROSE_ASSERT(this->get_increment_expression() != NULL);
  this->get_increment_expression()->fixupCopy_references(
      forStatement_copy->get_increment_expression(), help);

  ROSE_ASSERT(this->get_loop_body() != NULL);
  this->get_loop_body()->fixupCopy_references(
      forStatement_copy->get_loop_body(), help);
}

void SgForInitStatement::fixupCopy_references(SgNode *copy,
                                              SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgForInitStatement::fixupCopy_references() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_references(copy, help);

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
    // printf ("In SgForInitStatement::fixupCopy_references(): Calling fixup for
    // *i_copy = %p = %s \n",(*i_copy),(*i_copy)->class_name().c_str());
    (*i_original)->fixupCopy_references(*i_copy, help);

    i_original++;
    i_copy++;
  }
}

void SgCatchStatementSeq::fixupCopy_references(SgNode *copy,
                                               SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgCatchStatementSeq::fixupCopy_references() this = %p = %s "
         " copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_references(copy, help);

  SgCatchStatementSeq *catchStatement_copy = isSgCatchStatementSeq(copy);
  if (catchStatement_copy == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[catch-sequence-reference-copy]: original=%p "
            "copy=%p\n",
            static_cast<const void *>(this), static_cast<void *>(copy));
    ROSE_ABORT();
  }
  requireExactCopiedReferenceRoot(this, catchStatement_copy, help,
                                  "catch-sequence");

  const SgStatementPtrList &originalCatches = get_catch_statement_seq();
  const SgStatementPtrList &copiedCatches =
      catchStatement_copy->get_catch_statement_seq();
  if (originalCatches.size() != copiedCatches.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[catch-sequence-reference-count]: original=%p "
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
              "REX_COPY_INVARIANT[catch-handler-reference-kind]: index=%zu "
              "original=%p copy=%p\n",
              index, static_cast<void *>(originalCatches[index]),
              static_cast<void *>(copiedCatches[index]));
      ROSE_ABORT();
    }
    requireExactCopiedReferenceChild(this, originalCatch, catchStatement_copy,
                                     copiedCatch, help, "catch-handler");
    originalCatch->fixupCopy_references(copiedCatch, help);
  }
}

void SgWhileStmt::fixupCopy_references(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgWhileStmt::fixupCopy_references() this = %p = %s  copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgScopeStatement::fixupCopy_references(copy, help);

  SgWhileStmt *whileStatement_copy = isSgWhileStmt(copy);
  ROSE_ASSERT(whileStatement_copy != NULL);

  ROSE_ASSERT(this->get_condition() != NULL);
  this->get_condition()->fixupCopy_references(
      whileStatement_copy->get_condition(), help);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_references(whileStatement_copy->get_body(), help);
}

void SgDoWhileStmt::fixupCopy_references(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgDoWhileStmt::fixupCopy_references() this = %p = %s  copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgScopeStatement::fixupCopy_references(copy, help);

  SgDoWhileStmt *doWhileStatement_copy = isSgDoWhileStmt(copy);
  ROSE_ASSERT(doWhileStatement_copy != NULL);

  ROSE_ASSERT(this->get_condition() != NULL);
  this->get_condition()->fixupCopy_references(
      doWhileStatement_copy->get_condition(), help);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_references(doWhileStatement_copy->get_body(),
                                         help);
}

void SgSwitchStatement::fixupCopy_references(SgNode *copy,
                                             SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgSwitchStatement::fixupCopy_references() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgScopeStatement::fixupCopy_references(copy, help);

  SgSwitchStatement *switchStatement_copy = isSgSwitchStatement(copy);
  ROSE_ASSERT(switchStatement_copy != NULL);

  ROSE_ASSERT(this->get_item_selector() != NULL);
  this->get_item_selector()->fixupCopy_references(
      switchStatement_copy->get_item_selector(), help);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_references(switchStatement_copy->get_body(),
                                         help);
}

void SgTryStmt::fixupCopy_references(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgTryStmt::fixupCopy_references() this = %p = %s  copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_references(copy, help);

  SgTryStmt *tryStatement_copy = isSgTryStmt(copy);
  if (tryStatement_copy == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[try-statement-reference-copy]: original=%p "
            "copy=%p\n",
            static_cast<const void *>(this), static_cast<void *>(copy));
    ROSE_ABORT();
  }
  requireExactCopiedReferenceRoot(this, tryStatement_copy, help,
                                  "try-statement");
  SgStatement *originalBody = get_body();
  SgStatement *copiedBody = tryStatement_copy->get_body();
  SgCatchStatementSeq *originalCatches = get_catch_statement_seq_root();
  SgCatchStatementSeq *copiedCatches =
      tryStatement_copy->get_catch_statement_seq_root();
  if (originalBody == nullptr || copiedBody == nullptr ||
      originalCatches == nullptr || copiedCatches == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[try-statement-reference-children]: "
            "original=%p body=%p catches=%p copy=%p body=%p catches=%p\n",
            static_cast<const void *>(this), static_cast<void *>(originalBody),
            static_cast<void *>(originalCatches),
            static_cast<void *>(tryStatement_copy),
            static_cast<void *>(copiedBody),
            static_cast<void *>(copiedCatches));
    ROSE_ABORT();
  }
  requireExactCopiedReferenceChild(this, originalBody, tryStatement_copy,
                                   copiedBody, help, "try-body");
  requireExactCopiedReferenceChild(this, originalCatches, tryStatement_copy,
                                   copiedCatches, help, "try-catches");
  originalBody->fixupCopy_references(copiedBody, help);
  originalCatches->fixupCopy_references(copiedCatches, help);
}

void SgCatchOptionStmt::fixupCopy_references(SgNode *copy,
                                             SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgCatchOptionStmt::fixupCopy_references() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgScopeStatement::fixupCopy_references(copy, help);

  SgCatchOptionStmt *catchOptionStatement_copy = isSgCatchOptionStmt(copy);
  if (catchOptionStatement_copy == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[catch-handler-reference-copy]: original=%p "
            "copy=%p\n",
            static_cast<const void *>(this), static_cast<void *>(copy));
    ROSE_ABORT();
  }
  requireExactCopiedReferenceRoot(this, catchOptionStatement_copy, help,
                                  "catch-handler");
  SgTryStmt *originalTry = get_trystmt();
  SgTryStmt *copiedTry = catchOptionStatement_copy->get_trystmt();
  if (originalTry == nullptr || copiedTry == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[catch-handler-reference-try]: original=%p "
            "try=%p copy=%p try=%p\n",
            static_cast<const void *>(this), static_cast<void *>(originalTry),
            static_cast<void *>(catchOptionStatement_copy),
            static_cast<void *>(copiedTry));
    ROSE_ABORT();
  }
  requireExactCopiedOrExternalReferenceEdge(originalTry, copiedTry, help,
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
        "REX_COPY_INVARIANT[catch-handler-reference-children]: "
        "original=%p condition=%p body=%p copy=%p condition=%p body=%p\n",
        static_cast<const void *>(this), static_cast<void *>(originalCondition),
        static_cast<void *>(originalBody),
        static_cast<void *>(catchOptionStatement_copy),
        static_cast<void *>(copiedCondition), static_cast<void *>(copiedBody));
    ROSE_ABORT();
  }
  if (originalCondition != nullptr) {
    requireExactCopiedReferenceChild(this, originalCondition,
                                     catchOptionStatement_copy, copiedCondition,
                                     help, "catch-handler-condition");
  }
  requireExactCopiedReferenceChild(this, originalBody,
                                   catchOptionStatement_copy, copiedBody, help,
                                   "catch-handler-body");
  if (originalCondition != nullptr) {
    originalCondition->fixupCopy_references(copiedCondition, help);
  }
  originalBody->fixupCopy_references(copiedBody, help);
}

void SgCaseOptionStmt::fixupCopy_references(SgNode *copy,
                                            SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgCaseOptionStmt::fixupCopy_references() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_references(copy, help);

  SgCaseOptionStmt *caseOptionStatement_copy = isSgCaseOptionStmt(copy);
  ROSE_ASSERT(caseOptionStatement_copy != NULL);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_references(caseOptionStatement_copy->get_body(),
                                         help);
}

void SgDefaultOptionStmt::fixupCopy_references(SgNode *copy,
                                               SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgDefaultOptionStmt::fixupCopy_references() this = %p = %s "
         " copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_references(copy, help);

  SgDefaultOptionStmt *defaultOptionStatement_copy =
      isSgDefaultOptionStmt(copy);
  ROSE_ASSERT(defaultOptionStatement_copy != NULL);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_references(
      defaultOptionStatement_copy->get_body(), help);
}

void SgTemplateArgument::fixupCopy_references(SgNode *copy,
                                              SgCopyHelp &help) const {
  SgTemplateArgument *templateArgument_copy = isSgTemplateArgument(copy);
  if (templateArgument_copy == nullptr) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-argument-reference-copy]: "
            "original=%p copy=%p\n",
            static_cast<const void *>(this), static_cast<void *>(copy));
    ROSE_ABORT();
  }
  requireExactCopiedReferenceRoot(this, templateArgument_copy, help,
                                  "template-argument");
  requireExactCopiedOrExternalReferenceEdge(
      get_templateDeclaration(),
      templateArgument_copy->get_templateDeclaration(), help,
      "template-argument-declaration");

#if DEBUG_FIXUP_COPY
  printf("\nIn SgTemplateArgument::fixupCopy_references(): this = %p = %s copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  if (SgInitializedName *initializedName_original = get_initializedName()) {
    SgCopyHelp::copiedNodeMapTypeIterator i =
        help.get_copiedNodeMap().find(initializedName_original);
    if (i != help.get_copiedNodeMap().end()) {
      SgInitializedName *initializedName_copy = isSgInitializedName(i->second);
      ROSE_ASSERT(initializedName_copy != NULL);
      templateArgument_copy->set_initializedName(initializedName_copy);
    }
  }

  if (SgType *source_spelled_type_original = get_sourceSpelledType()) {
    SgCopyHelp::copiedNodeMapTypeIterator i =
        help.get_copiedNodeMap().find(source_spelled_type_original);
    if (i != help.get_copiedNodeMap().end()) {
      SgType *source_spelled_type_copy = isSgType(i->second);
      ROSE_ASSERT(source_spelled_type_copy != NULL);
      templateArgument_copy->set_sourceSpelledType(source_spelled_type_copy);
    }
  }

  if (SgTemplateArgument *previous_instance_original =
          get_previous_instance()) {
    SgCopyHelp::copiedNodeMapTypeIterator i =
        help.get_copiedNodeMap().find(previous_instance_original);
    if (i != help.get_copiedNodeMap().end()) {
      SgTemplateArgument *previous_instance_copy =
          isSgTemplateArgument(i->second);
      ROSE_ASSERT(previous_instance_copy != NULL);
      templateArgument_copy->set_previous_instance(previous_instance_copy);
    }
  }

  if (SgTemplateArgument *next_instance_original = get_next_instance()) {
    SgCopyHelp::copiedNodeMapTypeIterator i =
        help.get_copiedNodeMap().find(next_instance_original);
    if (i != help.get_copiedNodeMap().end()) {
      SgTemplateArgument *next_instance_copy = isSgTemplateArgument(i->second);
      ROSE_ASSERT(next_instance_copy != NULL);
      templateArgument_copy->set_next_instance(next_instance_copy);
    }
  }

  fixupExactCopiedSupportReferences(this, templateArgument_copy, help);
}
