#include "sage3basic.h"

#include "RoseAst.h"
#include "astPostProcessing/astPostProcessing.h"
#include "fixupCopy.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

// This file implementes support for the AST copy fixup.  It is specific to:
// 1) Scope pointer fixup
// 2) Parent pointer fixup
// 3 defining and not defining declarations fixup

namespace {

bool hasNontrivialCopiedNodes(SgCopyHelp &help) {
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (it->first != it->second) {
      return true;
    }
  }

  return false;
}

bool copiedNodeLooksDeleted(const SgNode *node);
bool isGloballyCanonicalBuiltinType(const SgNode *node);

bool isSharedOriginalMappedNode(SgCopyHelp &help, const SgNode *node) {
  if (node == NULL) {
    return false;
  }

  SgCopyHelp::copiedNodeMapTypeIterator it =
      help.get_copiedNodeMap().find(const_cast<SgNode *>(node));
  return it != help.get_copiedNodeMap().end() && it->second == node;
}

SgNode *lookupCopiedNode(SgCopyHelp &help, const SgNode *original) {
  if (original == NULL) {
    return NULL;
  }

  SgCopyHelp::copiedNodeMapTypeIterator it =
      help.get_copiedNodeMap().find(const_cast<SgNode *>(original));
  if (it == help.get_copiedNodeMap().end()) {
    return NULL;
  }

  if (copiedNodeLooksDeleted(it->second)) {
    return NULL;
  }

  return it->second;
}

void printCopyParentChain(const char *label, const SgNode *node,
                          SgCopyHelp &help) {
  fprintf(stderr, "REX_COPY_INVARIANT[%s-parent-chain]:", label);
  std::set<const SgNode *> visited;
  for (const SgNode *current = node;
       current != NULL && visited.insert(current).second;
       current = current->get_parent()) {
    size_t parentEdges = 0;
    if (current->get_parent() != NULL) {
      for (const auto &edge :
           current->get_parent()->returnDataMemberPointers()) {
        if (edge.first == current) {
          ++parentEdges;
        }
      }
    }
    fprintf(stderr, " %p(%s,parent-edges=%zu)->%p",
            static_cast<const void *>(current), current->class_name().c_str(),
            parentEdges, static_cast<void *>(lookupCopiedNode(help, current)));
    if (const SgDeclarationStatement *declaration =
            isSgDeclarationStatement(current)) {
      SgDeclarationStatement *copyDeclaration =
          isSgDeclarationStatement(lookupCopiedNode(help, declaration));
      fprintf(
          stderr, "{scope=%p copy-parent=%p/%s copy-nonreal-scope=%p}",
          static_cast<void *>(declaration->get_nonreal_decl_scope()),
          static_cast<void *>(
              copyDeclaration != NULL ? copyDeclaration->get_parent() : NULL),
          copyDeclaration != NULL && copyDeclaration->get_parent() != NULL
              ? copyDeclaration->get_parent()->class_name().c_str()
              : "<null>",
          static_cast<void *>(copyDeclaration != NULL
                                  ? copyDeclaration->get_nonreal_decl_scope()
                                  : NULL));
      if (const SgTemplateDeclaration *templateDeclaration =
              isSgTemplateDeclaration(declaration)) {
        fprintf(stderr, "{name=%s}", templateDeclaration->get_name().str());
      } else if (const SgFunctionDeclaration *functionDeclaration =
                     isSgFunctionDeclaration(declaration)) {
        const SgTemplateFunctionDeclaration *templateFunction =
            isSgTemplateFunctionDeclaration(functionDeclaration);
        SgDeclarationScope *sourceNonrealScope =
            functionDeclaration->get_nonreal_decl_scope();
        SgDeclarationScope *copyNonrealScope =
            copyDeclaration != NULL ? copyDeclaration->get_nonreal_decl_scope()
                                    : NULL;
        fprintf(stderr,
                "{name=%s template-parameters=%zu nonreal-declarations=%zu "
                "copy-nonreal-declarations=%zu}",
                functionDeclaration->get_name().str(),
                templateFunction != NULL
                    ? templateFunction->get_templateParameters().size()
                    : 0,
                sourceNonrealScope != NULL
                    ? sourceNonrealScope->get_declarations().size()
                    : 0,
                copyNonrealScope != NULL
                    ? copyNonrealScope->get_declarations().size()
                    : 0);
        if (templateFunction != NULL) {
          for (const SgTemplateParameter *parameter :
               templateFunction->get_templateParameters()) {
            const auto rawParameterMapping = help.get_copiedNodeMap().find(
                const_cast<SgTemplateParameter *>(parameter));
            SgNode *rawCopyParameter =
                rawParameterMapping != help.get_copiedNodeMap().end()
                    ? rawParameterMapping->second
                    : NULL;
            SgTemplateParameter *copyParameter =
                isSgTemplateParameter(lookupCopiedNode(help, parameter));
            const SgTemplateFunctionDeclaration *copyTemplateFunction =
                isSgTemplateFunctionDeclaration(copyDeclaration);
            SgTemplateParameter *ownedCopyParameter =
                copyTemplateFunction != NULL &&
                        copyTemplateFunction->get_templateParameters().size() ==
                            1
                    ? copyTemplateFunction->get_templateParameters().front()
                    : NULL;
            fprintf(stderr,
                    "{parameter=%p kind=%d raw-mapped=%p mapped=%p "
                    "owned-copy=%p declaration=%p declaration-map=%p "
                    "copy-declaration=%p owned-copy-declaration=%p}",
                    static_cast<const void *>(parameter),
                    static_cast<int>(parameter->get_parameterType()),
                    static_cast<void *>(rawCopyParameter),
                    static_cast<void *>(copyParameter),
                    static_cast<void *>(ownedCopyParameter),
                    static_cast<void *>(parameter->get_templateDeclaration()),
                    static_cast<void *>(lookupCopiedNode(
                        help, parameter->get_templateDeclaration())),
                    static_cast<void *>(
                        copyParameter != NULL
                            ? copyParameter->get_templateDeclaration()
                            : NULL),
                    static_cast<void *>(
                        ownedCopyParameter != NULL
                            ? ownedCopyParameter->get_templateDeclaration()
                            : NULL));
          }
        }
      }
    }
  }
  fputc('\n', stderr);
}

SgDeclarationStatement *
lookupCopiedDeclaration(SgCopyHelp &help,
                        const SgDeclarationStatement *original) {
  return isSgDeclarationStatement(lookupCopiedNode(help, original));
}

SgNode *requireExactMappedNode(const SgNode *original, SgCopyHelp &help,
                               const char *relation) {
  if (original == NULL || relation == NULL) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[exact-role-input]: relation=%s original=%p "
            "must identify one non-null semantic role\n",
            relation != NULL ? relation : "<null>",
            static_cast<const void *>(original));
    ROSE_ABORT();
  }

  SgCopyHelp::copiedNodeMapTypeIterator mapping =
      help.get_copiedNodeMap().find(const_cast<SgNode *>(original));
  SgNode *copy =
      mapping != help.get_copiedNodeMap().end() ? mapping->second : NULL;
  if (copy == NULL || copiedNodeLooksDeleted(copy) || copy == original ||
      copy->variantT() != original->variantT()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[exact-role-map]: relation=%s original=%p/%s "
            "copy=%p/%s\n",
            relation, static_cast<const void *>(original),
            original->class_name().c_str(), static_cast<void *>(copy),
            copy != NULL ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }

  const SgCopyHelp::originalNodeSetType &sources =
      help.get_originalNodesForCopy(copy);
  const size_t sourceCount = sources.size();
  if (sourceCount != 1) {
    const SgNode *otherSource = NULL;
    for (const SgNode *source : sources) {
      if (source != original) {
        otherSource = source;
      }
    }
    fprintf(stderr,
            "REX_COPY_INVARIANT[ambiguous-role-map]: relation=%s "
            "original=%p/%s copy=%p/%s sources=%zu other=%p/%s\n",
            relation, static_cast<const void *>(original),
            original->class_name().c_str(), static_cast<void *>(copy),
            copy->class_name().c_str(), sourceCount,
            static_cast<const void *>(otherSource),
            otherSource != NULL ? otherSource->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }

  return copy;
}

SgDeclarationStatement *
requireExactMappedDeclaration(const SgDeclarationStatement *original,
                              SgCopyHelp &help, const char *relation) {
  SgDeclarationStatement *copy = isSgDeclarationStatement(
      requireExactMappedNode(original, help, relation));
  if (copy == NULL) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[exact-declaration-role]: relation=%s "
            "original=%p/%s did not map to a declaration\n",
            relation, static_cast<const void *>(original),
            original->class_name().c_str());
    ROSE_ABORT();
  }
  return copy;
}

size_t countDirectOwnerEdges(const SgNode *owner, const SgNode *child) {
  if (owner == NULL || child == NULL) {
    return 0;
  }
  size_t count = 0;
  // Structural ownership is broader than default AST traversal.  In
  // particular, originalExpressionTree is intentionally non-traversal to
  // avoid visiting both folded semantic values and their exact source tree,
  // but it is a CLONE_TREE data-member edge with an exact parent.  Copy
  // validation must use the complete declared pointer topology, as reset
  // parent and unparser provenance validation do, or a valid cloned source
  // expression appears spuriously unowned.
  for (const std::pair<SgNode *, std::string> &edge :
       owner->returnDataMemberPointers()) {
    // OpenMP's typed execution graph is a semantic overlay.  A structured
    // child can therefore appear once through its owning `body` field and a
    // second time in `omp_children`; only the former is a structural owner.
    // Likewise `omp_parent` is never an ownership edge.
    if (edge.second == "omp_parent" || edge.second == "omp_children") {
      continue;
    }
    if (edge.first == child) {
      ++count;
    }
  }
  const auto *function = isSgFunctionDeclaration(const_cast<SgNode *>(owner));
  if (function != nullptr && function->get_oldStyleDefinition() &&
      function->get_parameterList() == child &&
      function->get_parameterList_syntax() == child) {
    ROSE_ASSERT(count >= 2);
    --count;
  }
  return count;
}

SgNode *requireExactCopiedOwnedChild(const SgNode *originalOwner,
                                     const SgNode *originalChild,
                                     SgNode *copiedOwner, SgNode *copiedChild,
                                     SgCopyHelp &help, const char *relation) {
  ASSERT_not_null(originalOwner);
  ASSERT_not_null(originalChild);
  ASSERT_not_null(copiedOwner);
  ASSERT_not_null(relation);

  SgNode *mappedChild = requireExactMappedNode(originalChild, help, relation);
  const size_t originalEdges =
      countDirectOwnerEdges(originalOwner, originalChild);
  const size_t copiedEdges = countDirectOwnerEdges(copiedOwner, copiedChild);
  if (mappedChild != copiedChild ||
      originalChild->get_parent() != originalOwner || copiedChild == NULL ||
      copiedChild->get_parent() != copiedOwner || originalEdges != 1 ||
      copiedEdges != 1) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[owned-child-map]: relation=%s original-owner=%p/%s "
        "child=%p/%s parent=%p edges=%zu copy-owner=%p/%s child=%p/%s "
        "parent=%p expected=%p edges=%zu\n",
        relation, static_cast<const void *>(originalOwner),
        originalOwner->class_name().c_str(),
        static_cast<const void *>(originalChild),
        originalChild->class_name().c_str(),
        static_cast<void *>(originalChild->get_parent()), originalEdges,
        static_cast<void *>(copiedOwner), copiedOwner->class_name().c_str(),
        static_cast<void *>(copiedChild),
        copiedChild != NULL ? copiedChild->class_name().c_str() : "<null>",
        static_cast<void *>(copiedChild != NULL ? copiedChild->get_parent()
                                                : NULL),
        static_cast<void *>(mappedChild), copiedEdges);
    ROSE_ABORT();
  }
  return copiedChild;
}

void fixupExactCopiedSupportScopes(const SgNode *original, SgNode *copy,
                                   SgCopyHelp &help,
                                   std::unordered_set<const SgNode *> &active) {
  ASSERT_not_null(original);
  ASSERT_not_null(copy);
  if (!active.insert(original).second) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[support-scope-cycle]: original=%p/%s "
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
            "REX_COPY_INVARIANT[support-scope-arity]: original=%p/%s "
            "children=%zu copy=%p/%s children=%zu\n",
            static_cast<const void *>(original), original->class_name().c_str(),
            originalChildren.size(), static_cast<void *>(copy),
            copy->class_name().c_str(), copiedChildren.size());
    ROSE_ABORT();
  }

  for (size_t index = 0; index < originalChildren.size(); ++index) {
    SgNode *originalChild = originalChildren[index];
    SgNode *copiedChild = copiedChildren[index];
    if ((originalChild == NULL) != (copiedChild == NULL)) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[support-scope-null-child]: owner=%p/%s "
          "copy=%p/%s index=%zu original-child=%p copy-child=%p\n",
          static_cast<const void *>(original), original->class_name().c_str(),
          static_cast<void *>(copy), copy->class_name().c_str(), index,
          static_cast<void *>(originalChild), static_cast<void *>(copiedChild));
      ROSE_ABORT();
    }
    if (originalChild == NULL) {
      continue;
    }
    requireExactCopiedOwnedChild(original, originalChild, copy, copiedChild,
                                 help, "support-scope-child");
    if (isSgExpression(originalChild) != NULL ||
        isSgStatement(originalChild) != NULL) {
      originalChild->fixupCopy_scopes(copiedChild, help);
    } else {
      if (const SgLocatedNode *originalLocated =
              isSgLocatedNode(originalChild)) {
        originalLocated->SgLocatedNode::fixupCopy_scopes(copiedChild, help);
      }
      fixupExactCopiedSupportScopes(originalChild, copiedChild, help, active);
    }
  }
  active.erase(original);
}

void fixupExactCopiedSupportScopes(const SgNode *original, SgNode *copy,
                                   SgCopyHelp &help) {
  std::unordered_set<const SgNode *> active;
  fixupExactCopiedSupportScopes(original, copy, help, active);
}

bool declarationMustDefineItself(const SgDeclarationStatement *decl);
SgNode *remapExactCopiedOrExternalSemanticEdge(const SgNode *originalEdge,
                                               SgNode *provisionalCopiedEdge,
                                               SgCopyHelp &help,
                                               const char *relation);
SgNode *requireExactCopiedOrExternalSemanticEdge(const SgNode *originalEdge,
                                                 SgNode *copiedEdge,
                                                 SgCopyHelp &help,
                                                 const char *relation);

void finalizeExactCopiedDeclarationChainLinks(
    const SgDeclarationStatement *originalDecl,
    SgDeclarationStatement *copiedDecl, SgCopyHelp &help) {
  ASSERT_not_null(originalDecl);
  ASSERT_not_null(copiedDecl);

  if (requireExactMappedDeclaration(originalDecl, help, "declaration-self") !=
      copiedDecl) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[declaration-self-map]: original=%p/%s "
            "argument-copy=%p mapped-copy=%p\n",
            static_cast<const void *>(originalDecl),
            originalDecl->class_name().c_str(), static_cast<void *>(copiedDecl),
            static_cast<void *>(lookupCopiedNode(help, originalDecl)));
    ROSE_ABORT();
  }

  // SgFunctionParameterList is a structural support node that inherits from
  // SgDeclarationStatement for historical reasons.  It has no declaration
  // family: the owning function initializes its first-nondefining link to the
  // list itself and deliberately leaves the defining link null.  Preserve
  // that exact contract when the list is copied, including when it is the
  // detached root of a copy transaction.
  if (isSgFunctionParameterList(originalDecl) != NULL) {
    if (originalDecl->get_definingDeclaration() != NULL ||
        originalDecl->get_firstNondefiningDeclaration() != originalDecl) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[function-parameter-list-chain]: "
          "original=%p defining=%p first=%p\n",
          static_cast<const void *>(originalDecl),
          static_cast<const void *>(originalDecl->get_definingDeclaration()),
          static_cast<const void *>(
              originalDecl->get_firstNondefiningDeclaration()));
      ROSE_ABORT();
    }
    copiedDecl->set_definingDeclaration(NULL);
    copiedDecl->set_firstNondefiningDeclaration(copiedDecl);
    return;
  }

  if (declarationMustDefineItself(originalDecl)) {
    if (originalDecl->get_definingDeclaration() != originalDecl ||
        originalDecl->get_firstNondefiningDeclaration() != originalDecl) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[self-defining-input]: original=%p/%s "
          "defining=%p first=%p\n",
          static_cast<const void *>(originalDecl),
          originalDecl->class_name().c_str(),
          static_cast<const void *>(originalDecl->get_definingDeclaration()),
          static_cast<const void *>(
              originalDecl->get_firstNondefiningDeclaration()));
      ROSE_ABORT();
    }
    copiedDecl->set_definingDeclaration(copiedDecl);
    copiedDecl->set_firstNondefiningDeclaration(copiedDecl);
    return;
  }

  const SgDeclarationStatement *originalDefining =
      originalDecl->get_definingDeclaration();
  const SgDeclarationStatement *originalFirstNondefining =
      originalDecl->get_firstNondefiningDeclaration();

  SgDeclarationStatement *copiedDefining =
      isSgDeclarationStatement(remapExactCopiedOrExternalSemanticEdge(
          originalDefining, copiedDecl->get_definingDeclaration(), help,
          "defining-declaration"));
  SgDeclarationStatement *copiedFirstNondefining =
      isSgDeclarationStatement(remapExactCopiedOrExternalSemanticEdge(
          originalFirstNondefining,
          copiedDecl->get_firstNondefiningDeclaration(), help,
          "first-nondefining-declaration"));

  if ((originalDefining != NULL && copiedDefining == NULL) ||
      (originalFirstNondefining != NULL && copiedFirstNondefining == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[declaration-chain-kind]: original=%p/%s "
            "defining=%p first=%p copy=%p defining=%p first=%p\n",
            static_cast<const void *>(originalDecl),
            originalDecl->class_name().c_str(),
            static_cast<const void *>(originalDefining),
            static_cast<const void *>(originalFirstNondefining),
            static_cast<void *>(copiedDecl),
            static_cast<void *>(copiedDefining),
            static_cast<void *>(copiedFirstNondefining));
    ROSE_ABORT();
  }

  copiedDecl->set_definingDeclaration(copiedDefining);
  copiedDecl->set_firstNondefiningDeclaration(copiedFirstNondefining);
}

bool copiedNodeLooksDeleted(const SgNode *node) {
  return !SgNode::isLiveNode(node);
}

bool isExactCanonicalTypeConvergence(const SgNode *firstOriginal,
                                     const SgNode *secondOriginal, SgNode *copy,
                                     SgCopyHelp &help) {
  auto mappedType = [&help](SgType *source) -> SgType * {
    if (source == NULL) {
      return NULL;
    }
    const SgCopyHelp::copiedNodeMapTypeIterator mapped =
        help.get_copiedNodeMap().find(source);
    return mapped != help.get_copiedNodeMap().end() ? isSgType(mapped->second)
                                                    : source;
  };
  SgType *first = isSgType(const_cast<SgNode *>(firstOriginal));
  SgType *second = isSgType(const_cast<SgNode *>(secondOriginal));
  SgType *target = isSgType(copy);
  if (first == NULL || second == NULL || target == NULL ||
      first->variantT() != second->variantT() ||
      first->variantT() != target->variantT()) {
    return false;
  }

  if (auto *targetModifier = isSgModifierType(target)) {
    auto *firstModifier = isSgModifierType(first);
    auto *secondModifier = isSgModifierType(second);
    return firstModifier->get_type_kind() == nullptr &&
           secondModifier->get_type_kind() == nullptr &&
           targetModifier->get_type_kind() == nullptr &&
           targetModifier->get_base_type() ==
               mappedType(firstModifier->get_base_type()) &&
           targetModifier->get_base_type() ==
               mappedType(secondModifier->get_base_type()) &&
           targetModifier->get_typeModifier() ==
               firstModifier->get_typeModifier() &&
           targetModifier->get_typeModifier() ==
               secondModifier->get_typeModifier() &&
           SageBuilder::buildModifierType(targetModifier->get_base_type(),
                                          targetModifier->get_typeModifier()) ==
               targetModifier;
  }
  if (SgPointerMemberType *targetMember = isSgPointerMemberType(target)) {
    SgPointerMemberType *firstMember = isSgPointerMemberType(first);
    SgPointerMemberType *secondMember = isSgPointerMemberType(second);
    return firstMember != NULL && secondMember != NULL &&
           SgPointerMemberType::isCanonicalSemanticType(firstMember) &&
           SgPointerMemberType::isCanonicalSemanticType(secondMember) &&
           targetMember->get_base_type() ==
               mappedType(firstMember->get_base_type()) &&
           targetMember->get_base_type() ==
               mappedType(secondMember->get_base_type()) &&
           targetMember->get_class_type() ==
               mappedType(firstMember->get_class_type()) &&
           targetMember->get_class_type() ==
               mappedType(secondMember->get_class_type()) &&
           SgPointerMemberType::isCanonicalSemanticType(targetMember);
  }
  if (target->variantT() == V_SgPointerType) {
    SgPointerType *firstPointer = isSgPointerType(first);
    SgPointerType *secondPointer = isSgPointerType(second);
    SgPointerType *targetPointer = isSgPointerType(target);
    return firstPointer != NULL && secondPointer != NULL &&
           targetPointer != NULL &&
           targetPointer->get_base_type() ==
               mappedType(firstPointer->get_base_type()) &&
           targetPointer->get_base_type() ==
               mappedType(secondPointer->get_base_type()) &&
           targetPointer->get_base_type()->get_ptr_to() == targetPointer;
  }
  if (SgReferenceType *targetReference = isSgReferenceType(target)) {
    SgReferenceType *firstReference = isSgReferenceType(first);
    SgReferenceType *secondReference = isSgReferenceType(second);
    return firstReference != NULL && secondReference != NULL &&
           targetReference->get_base_type() ==
               mappedType(firstReference->get_base_type()) &&
           targetReference->get_base_type() ==
               mappedType(secondReference->get_base_type()) &&
           targetReference->get_base_type()->get_ref_to() == targetReference;
  }
  if (SgRvalueReferenceType *targetReference =
          isSgRvalueReferenceType(target)) {
    SgRvalueReferenceType *firstReference = isSgRvalueReferenceType(first);
    SgRvalueReferenceType *secondReference = isSgRvalueReferenceType(second);
    return firstReference != NULL && secondReference != NULL &&
           targetReference->get_base_type() ==
               mappedType(firstReference->get_base_type()) &&
           targetReference->get_base_type() ==
               mappedType(secondReference->get_base_type()) &&
           targetReference->get_base_type()->get_rvalue_ref_to() ==
               targetReference;
  }
  return false;
}

void validateExactCopiedNodeMap(SgCopyHelp &help) {
  std::unordered_map<const SgNode *, const SgNode *> copySources;
  copySources.reserve(help.get_copiedNodeMap().size());
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgNode *original = it->first;
    SgNode *copy = it->second;
    if (original == NULL || copiedNodeLooksDeleted(copy) ||
        original->variantT() != copy->variantT()) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[exact-copy-map-entry]: original=%p/%s "
              "copy=%p/%s\n",
              static_cast<const void *>(original),
              original != NULL ? original->class_name().c_str() : "<null>",
              static_cast<void *>(copy),
              copy != NULL ? copy->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }

    if (original != copy) {
      SgCopyHelp::copiedNodeMapTypeIterator alias =
          help.get_copiedNodeMap().find(copy);
      if (alias != help.get_copiedNodeMap().end()) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[copy-of-copy-map]: original=%p/%s "
                "copy=%p/%s remapped-to=%p\n",
                static_cast<const void *>(original),
                original->class_name().c_str(), static_cast<void *>(copy),
                copy->class_name().c_str(), static_cast<void *>(alias->second));
        ROSE_ABORT();
      }

      const auto inserted = copySources.emplace(copy, original);
      if (!inserted.second && inserted.first->second != original &&
          !isExactCanonicalTypeConvergence(inserted.first->second, original,
                                           copy, help)) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[ambiguous-copy-map]: first-original=%p/%s "
                "second-original=%p/%s copy=%p/%s\n",
                static_cast<const void *>(inserted.first->second),
                inserted.first->second->class_name().c_str(),
                static_cast<const void *>(original),
                original->class_name().c_str(), static_cast<void *>(copy),
                copy->class_name().c_str());
        ROSE_ABORT();
      }
    }
  }
}

bool isCopiedNodeValue(SgCopyHelp &help, const SgNode *candidate) {
  if (candidate == NULL) {
    return false;
  }

  return !help.get_originalNodesForCopy(candidate).empty();
}

SgDeclarationStatement *
originalDeclarationForCopiedValue(SgCopyHelp &help,
                                  SgDeclarationStatement *candidate) {
  if (candidate == NULL) {
    return NULL;
  }

  for (const SgNode *source : help.get_originalNodesForCopy(candidate)) {
    if (SgDeclarationStatement *originalDecl =
            isSgDeclarationStatement(const_cast<SgNode *>(source))) {
      return originalDecl;
    }
  }

  return candidate;
}

bool scopeDirectlyContainsStatement(SgScopeStatement *scope,
                                    SgStatement *statement) {
  if (scope == NULL || statement == NULL) {
    return false;
  }

  if (scope->containsOnlyDeclarations()) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(statement);
    if (declaration == NULL) {
      return false;
    }

    const SgDeclarationStatementPtrList &declarations =
        scope->getDeclarationList();
    return std::find(declarations.begin(), declarations.end(), declaration) !=
           declarations.end();
  }

  switch (scope->variantT()) {
  case V_SgAssociateStatement:
  case V_SgBasicBlock:
  case V_SgCatchOptionStmt:
  case V_SgFortranDo:
  case V_SgForAllStatement:
  case V_SgFunctionDefinition:
  case V_SgTemplateFunctionDefinition: {
    const SgStatementPtrList &statements = scope->getStatementList();
    return std::find(statements.begin(), statements.end(), statement) !=
           statements.end();
  }

  default:
    break;
  }

  std::vector<SgNode *> children = scope->get_traversalSuccessorContainer();
  return std::find(children.begin(), children.end(), statement) !=
         children.end();
}

SgDeclarationStatement *
resolveCopiedReferencedDeclaration(const SgDeclarationStatement *originalDecl,
                                   SgCopyHelp &help);
bool declarationMustDefineItself(const SgDeclarationStatement *decl);
SgNode *copyNodeIntoCurrentRoot(SgCopyHelp &help, const SgNode *originalNode);

SgDeclarationStatement *resolveCurrentCopiedDeclarationForFixup(
    const SgDeclarationStatement *originalDecl, SgCopyHelp &help) {
  if (originalDecl == NULL) {
    return NULL;
  }

  if (SgDeclarationStatement *copiedDecl = lookupCopiedDeclaration(
          help, const_cast<SgDeclarationStatement *>(originalDecl))) {
    return copiedDecl;
  }

  return resolveCopiedReferencedDeclaration(originalDecl, help);
}

SgDeclarationStatement *scopeOwningDeclaration(SgScopeStatement *scope) {
  if (scope == NULL) {
    return NULL;
  }

  if (SgClassDefinition *classDef = isSgClassDefinition(scope)) {
    return classDef->get_declaration();
  }
  if (SgTemplateClassDefinition *templateClassDef =
          isSgTemplateClassDefinition(scope)) {
    return templateClassDef->get_declaration();
  }
  if (SgTemplateInstantiationDefn *templateInstDef =
          isSgTemplateInstantiationDefn(scope)) {
    return templateInstDef->get_declaration();
  }
  if (SgNamespaceDefinitionStatement *namespaceDef =
          isSgNamespaceDefinitionStatement(scope)) {
    return namespaceDef->get_namespaceDeclaration();
  }

  return NULL;
}

bool isClassLikeScope(SgScopeStatement *scope) {
  return isSgClassDefinition(scope) != NULL ||
         isSgTemplateClassDefinition(scope) != NULL ||
         isSgTemplateInstantiationDefn(scope) != NULL;
}

bool isNamespaceLikeScope(SgScopeStatement *scope) {
  return isSgNamespaceDefinitionStatement(scope) != NULL ||
         isSgGlobal(scope) != NULL;
}

bool namespaceLikeScopesMatch(SgScopeStatement *lhs, SgScopeStatement *rhs) {
  if (lhs == NULL || rhs == NULL) {
    return false;
  }
  if (lhs == rhs) {
    return true;
  }
  if (isSgGlobal(lhs) != NULL && isSgGlobal(rhs) != NULL) {
    return true;
  }

  SgNamespaceDefinitionStatement *lhsNamespace =
      isSgNamespaceDefinitionStatement(lhs);
  SgNamespaceDefinitionStatement *rhsNamespace =
      isSgNamespaceDefinitionStatement(rhs);
  if (lhsNamespace == NULL || rhsNamespace == NULL) {
    return false;
  }

  SgNamespaceDeclarationStatement *lhsDeclaration =
      lhsNamespace->get_namespaceDeclaration();
  SgNamespaceDeclarationStatement *rhsDeclaration =
      rhsNamespace->get_namespaceDeclaration();
  if (lhsDeclaration != NULL && rhsDeclaration != NULL &&
      lhsDeclaration->get_firstNondefiningDeclaration() ==
          rhsDeclaration->get_firstNondefiningDeclaration()) {
    return true;
  }

  return lhsNamespace->isSameNamespace(rhsNamespace) ||
         (lhsNamespace->get_global_definition() != NULL &&
          lhsNamespace->get_global_definition() ==
              rhsNamespace->get_global_definition());
}

SgScopeStatement *enclosingNamespaceLikeScope(SgScopeStatement *scope) {
  std::unordered_set<SgScopeStatement *> visitedScopes;
  while (scope != NULL) {
    if (!visitedScopes.insert(scope).second) {
      return NULL;
    }
    if (isNamespaceLikeScope(scope)) {
      return scope;
    }

    if (SgDeclarationStatement *owner = scopeOwningDeclaration(scope)) {
      SgScopeStatement *ownerScope = owner->get_scope();
      if (ownerScope != NULL && ownerScope != scope) {
        scope = ownerScope;
        continue;
      }
    }

    scope = isSgScopeStatement(scope->get_parent());
  }

  return NULL;
}

bool isHiddenFriendLexicalSemanticScopePair(SgDeclarationStatement *lhs,
                                            SgDeclarationStatement *rhs) {
  SgFunctionDeclaration *lhsFunction = isSgFunctionDeclaration(lhs);
  SgFunctionDeclaration *rhsFunction = isSgFunctionDeclaration(rhs);
  if (lhsFunction == NULL || rhsFunction == NULL) {
    return false;
  }
  if (!lhsFunction->get_declarationModifier().isFriend() &&
      !rhsFunction->get_declarationModifier().isFriend()) {
    return false;
  }
  if (lhsFunction->get_name() != rhsFunction->get_name()) {
    return false;
  }

  SgScopeStatement *lhsScope = lhsFunction->get_scope();
  SgScopeStatement *rhsScope = rhsFunction->get_scope();
  if (lhsScope == NULL || rhsScope == NULL) {
    return false;
  }

  SgScopeStatement *lexicalClassScope = NULL;
  SgScopeStatement *semanticNamespaceScope = NULL;
  if (isClassLikeScope(lhsScope) && isNamespaceLikeScope(rhsScope)) {
    lexicalClassScope = lhsScope;
    semanticNamespaceScope = rhsScope;
  } else if (isClassLikeScope(rhsScope) && isNamespaceLikeScope(lhsScope)) {
    lexicalClassScope = rhsScope;
    semanticNamespaceScope = lhsScope;
  } else {
    return false;
  }

  return namespaceLikeScopesMatch(
      enclosingNamespaceLikeScope(lexicalClassScope), semanticNamespaceScope);
}

void finalizeExactCopiedNamespaceScopeDeclarationChains(
    const SgDeclarationStatement *originalDecl,
    SgDeclarationStatement *copiedDecl, SgCopyHelp &help) {
  ASSERT_not_null(originalDecl);
  ASSERT_not_null(copiedDecl);

  if (isSgFunctionParameterList(originalDecl) != NULL ||
      isSgFunctionParameterList(copiedDecl) != NULL ||
      isSgNamespaceDeclarationStatement(originalDecl) != NULL ||
      isSgNamespaceDeclarationStatement(copiedDecl) != NULL ||
      declarationMustDefineItself(originalDecl) ||
      declarationMustDefineItself(copiedDecl)) {
    return;
  }

  const SgDeclarationStatement *originalDefining =
      originalDecl->get_definingDeclaration();
  const SgDeclarationStatement *originalFirstNondefining =
      originalDecl->get_firstNondefiningDeclaration();
  SgDeclarationStatement *copiedDefining =
      copiedDecl->get_definingDeclaration();
  SgDeclarationStatement *copiedFirstNondefining =
      copiedDecl->get_firstNondefiningDeclaration();

  auto hasNamespaceRole = [](const SgDeclarationStatement *declaration,
                             const SgDeclarationStatement *defining,
                             const SgDeclarationStatement *first) {
    auto hasNamespaceScope = [](const SgDeclarationStatement *candidate) {
      return candidate != NULL &&
             isSgNamespaceDefinitionStatement(candidate->get_scope()) != NULL;
    };
    return hasNamespaceScope(declaration) || hasNamespaceScope(defining) ||
           hasNamespaceScope(first);
  };
  const bool originalHasNamespaceRole = hasNamespaceRole(
      originalDecl, originalDefining, originalFirstNondefining);
  // The source declaration family is authoritative for whether this
  // namespace-specific finalizer applies.  A copied declaration root is
  // intentionally detached until its publication transaction completes, so
  // asking get_scope() to infer a role that the source family does not own is
  // both premature and semantically meaningless.
  if (!originalHasNamespaceRole) {
    return;
  }
  auto ownsCopiedNamespaceScope =
      [&](const SgDeclarationStatement *declaration) {
        SgNamespaceDefinitionStatement *originalNamespace =
            declaration != NULL
                ? isSgNamespaceDefinitionStatement(declaration->get_scope())
                : NULL;
        SgNode *copiedNamespace =
            originalNamespace != NULL
                ? lookupCopiedNode(help, originalNamespace)
                : NULL;
        return copiedNamespace != NULL && copiedNamespace != originalNamespace;
      };
  const bool copiedNamespaceTransaction =
      ownsCopiedNamespaceScope(originalDecl) ||
      ownsCopiedNamespaceScope(originalDefining) ||
      ownsCopiedNamespaceScope(originalFirstNondefining);
  // Declaration-chain pointers are semantic edges, not structural ownership.
  // A subtree copy can therefore include a declaration whose namespace scope
  // and canonical peers remain outside the transaction.  The generic chain
  // finalizer above has already required those exact external semantic edges.
  // Namespace-fragment reconciliation is a separate operation and is valid
  // only when this copy transaction owns at least one namespace-definition
  // scope from the declaration family.
  if (!copiedNamespaceTransaction) {
    return;
  }
  if (originalDefining == NULL && originalFirstNondefining == NULL &&
      copiedDefining == NULL && copiedFirstNondefining == NULL) {
    return;
  }
  const bool definingNullRoleMatches =
      (originalDefining == NULL) == (copiedDefining == NULL);
  const bool firstNullRoleMatches =
      (originalFirstNondefining == NULL) == (copiedFirstNondefining == NULL);
  const bool definingRoleMatches =
      originalDefining == NULL ||
      requireExactMappedDeclaration(
          originalDefining, help, "namespace-chain-defining") == copiedDefining;
  const bool firstRoleMatches =
      originalFirstNondefining == NULL ||
      requireExactMappedDeclaration(originalFirstNondefining, help,
                                    "namespace-chain-first-nondefining") ==
          copiedFirstNondefining;
  if (!definingNullRoleMatches || !firstNullRoleMatches ||
      !definingRoleMatches || !firstRoleMatches) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[namespace-chain-role-map]: original=%p/%s "
            "defining=%p first=%p copy=%p defining=%p first=%p\n",
            static_cast<const void *>(originalDecl),
            originalDecl->class_name().c_str(),
            static_cast<const void *>(originalDefining),
            static_cast<const void *>(originalFirstNondefining),
            static_cast<void *>(copiedDecl),
            static_cast<void *>(copiedDefining),
            static_cast<void *>(copiedFirstNondefining));
    ROSE_ABORT();
  }
  // Declarations such as namespace-scope variables can have one canonical
  // role without the other (first-nondefining=self, defining=null).  Their
  // exact role mapping is complete above; cross-fragment namespace-chain
  // reconciliation applies only to a declaration family that owns both ends.
  if (originalDefining == NULL || originalFirstNondefining == NULL) {
    return;
  }

  SgNamespaceDefinitionStatement *originalDefiningNamespace =
      isSgNamespaceDefinitionStatement(originalDefining->get_scope());
  SgNamespaceDefinitionStatement *originalFirstNondefiningNamespace =
      isSgNamespaceDefinitionStatement(originalFirstNondefining->get_scope());
  if (originalDefiningNamespace == NULL &&
      originalFirstNondefiningNamespace == NULL) {
    return;
  }
  SgNamespaceDefinitionStatement *copiedDefiningNamespace =
      isSgNamespaceDefinitionStatement(copiedDefining->get_scope());
  SgNamespaceDefinitionStatement *copiedFirstNondefiningNamespace =
      isSgNamespaceDefinitionStatement(copiedFirstNondefining->get_scope());
  if (originalDefiningNamespace == NULL ||
      originalFirstNondefiningNamespace == NULL ||
      copiedDefiningNamespace == NULL ||
      copiedFirstNondefiningNamespace == NULL ||
      requireExactMappedNode(originalDefiningNamespace, help,
                             "defining-namespace-scope") !=
          copiedDefiningNamespace ||
      requireExactMappedNode(originalFirstNondefiningNamespace, help,
                             "first-nondefining-namespace-scope") !=
          copiedFirstNondefiningNamespace) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[namespace-chain-scope-map]: original=%p/%s "
            "defining-scope=%p first-scope=%p copy=%p defining-scope=%p "
            "first-scope=%p\n",
            static_cast<const void *>(originalDecl),
            originalDecl->class_name().c_str(),
            static_cast<void *>(originalDefiningNamespace),
            static_cast<void *>(originalFirstNondefiningNamespace),
            static_cast<void *>(copiedDecl),
            static_cast<void *>(copiedDefiningNamespace),
            static_cast<void *>(copiedFirstNondefiningNamespace));
    ROSE_ABORT();
  }

  SgNamespaceDeclarationStatement *originalDefiningNamespaceDecl =
      originalDefiningNamespace->get_namespaceDeclaration();
  SgNamespaceDeclarationStatement *originalFirstNondefiningNamespaceDecl =
      originalFirstNondefiningNamespace->get_namespaceDeclaration();
  SgNamespaceDeclarationStatement *copiedDefiningNamespaceDecl =
      copiedDefiningNamespace->get_namespaceDeclaration();
  SgNamespaceDeclarationStatement *copiedFirstNondefiningNamespaceDecl =
      copiedFirstNondefiningNamespace->get_namespaceDeclaration();
  if (originalDefiningNamespaceDecl == NULL ||
      originalFirstNondefiningNamespaceDecl == NULL ||
      copiedDefiningNamespaceDecl == NULL ||
      copiedFirstNondefiningNamespaceDecl == NULL ||
      requireExactMappedNode(originalDefiningNamespaceDecl, help,
                             "defining-namespace-declaration") !=
          copiedDefiningNamespaceDecl ||
      requireExactMappedNode(originalFirstNondefiningNamespaceDecl, help,
                             "first-namespace-declaration") !=
          copiedFirstNondefiningNamespaceDecl) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[namespace-chain-declaration-map]: "
            "original-defining=%p original-first=%p copied-defining=%p "
            "copied-first=%p\n",
            static_cast<void *>(originalDefiningNamespaceDecl),
            static_cast<void *>(originalFirstNondefiningNamespaceDecl),
            static_cast<void *>(copiedDefiningNamespaceDecl),
            static_cast<void *>(copiedFirstNondefiningNamespaceDecl));
    ROSE_ABORT();
  }

  const SgNamespaceDeclarationStatement *originalCanonicalFirstNamespaceDecl =
      isSgNamespaceDeclarationStatement(
          originalDefiningNamespaceDecl->get_firstNondefiningDeclaration());
  const SgNamespaceDeclarationStatement *otherOriginalCanonicalFirst =
      isSgNamespaceDeclarationStatement(
          originalFirstNondefiningNamespaceDecl
              ->get_firstNondefiningDeclaration());
  if (originalCanonicalFirstNamespaceDecl == NULL ||
      otherOriginalCanonicalFirst != originalCanonicalFirstNamespaceDecl) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[namespace-canonical-first-input]: "
            "defining-namespace=%p first=%p other-first=%p\n",
            static_cast<void *>(originalDefiningNamespaceDecl),
            static_cast<const void *>(originalCanonicalFirstNamespaceDecl),
            static_cast<const void *>(otherOriginalCanonicalFirst));
    ROSE_ABORT();
  }
  SgNamespaceDeclarationStatement *copiedCanonicalFirstNamespaceDecl =
      isSgNamespaceDeclarationStatement(
          requireExactMappedNode(originalCanonicalFirstNamespaceDecl, help,
                                 "canonical-first-namespace-declaration"));
  if (copiedCanonicalFirstNamespaceDecl == NULL) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[namespace-canonical-first-output]: "
            "original=%p copy=%p\n",
            static_cast<const void *>(originalCanonicalFirstNamespaceDecl),
            static_cast<void *>(
                lookupCopiedNode(help, originalCanonicalFirstNamespaceDecl)));
    ROSE_ABORT();
  }

  copiedCanonicalFirstNamespaceDecl->set_firstNondefiningDeclaration(
      copiedCanonicalFirstNamespaceDecl);
  copiedDefiningNamespaceDecl->set_firstNondefiningDeclaration(
      copiedCanonicalFirstNamespaceDecl);
  copiedFirstNondefiningNamespaceDecl->set_firstNondefiningDeclaration(
      copiedCanonicalFirstNamespaceDecl);
}

SgDeclarationStatement *
resolveCopiedReferencedDeclaration(const SgDeclarationStatement *originalDecl,
                                   SgCopyHelp &help) {
  if (originalDecl == NULL) {
    return NULL;
  }

  if (SgDeclarationStatement *copiedDecl =
          isSgDeclarationStatement(lookupCopiedNode(help, originalDecl))) {
    if (copiedDecl != originalDecl) {
      return copiedDecl;
    }
  }

  if (SgDeclarationStatement *copiedDecl = isSgDeclarationStatement(
          copyNodeIntoCurrentRoot(help, originalDecl))) {
    if (copiedDecl != originalDecl && !copiedNodeLooksDeleted(copiedDecl) &&
        lookupCopiedNode(help, originalDecl) == copiedDecl) {
      return copiedDecl;
    }
  }

  return NULL;
}

class CopyDepthScope {
  SgCopyHelp &help_;

public:
  explicit CopyDepthScope(SgCopyHelp &help) : help_(help) {
    help_.incrementDepth();
  }
  ~CopyDepthScope() { help_.decrementDepth(); }
};

SgNode *copyNodeIntoCurrentRoot(SgCopyHelp &help, const SgNode *originalNode) {
  if (originalNode == NULL) {
    return NULL;
  }

  if (SgNode *existingCopy = lookupCopiedNode(help, originalNode)) {
    return existingCopy;
  }

  // Extend the active root-copy map without recursively finalizing each
  // newly synthesized child as its own standalone copy.
  CopyDepthScope depth(help);
  return help.copyOrLookupAst(originalNode);
}

void validateExactCopiedMemberFunctionCtorInitializerSubtree(
    const SgMemberFunctionDeclaration *originalDecl,
    SgMemberFunctionDeclaration *copiedDecl, SgCopyHelp &help) {
  ASSERT_not_null(originalDecl);
  ASSERT_not_null(copiedDecl);

  const SgCtorInitializerList *originalCtorList =
      originalDecl->get_CtorInitializerList();
  SgCtorInitializerList *copiedCtorList = copiedDecl->get_CtorInitializerList();
  if (originalCtorList == NULL || copiedCtorList == NULL ||
      requireExactMappedNode(originalCtorList, help,
                             "member-ctor-initializer-list") !=
          copiedCtorList ||
      originalCtorList->get_parent() != originalDecl ||
      countDirectOwnerEdges(originalDecl, originalCtorList) != 1 ||
      copiedCtorList->get_parent() != copiedDecl ||
      countDirectOwnerEdges(copiedDecl, copiedCtorList) != 1) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[member-ctor-initializer-list]: original=%p "
        "list=%p owner=%p copy=%p list=%p owner=%p\n",
        static_cast<const void *>(originalDecl),
        static_cast<const void *>(originalCtorList),
        static_cast<void *>(
            originalCtorList != NULL ? originalCtorList->get_parent() : NULL),
        static_cast<void *>(copiedDecl), static_cast<void *>(copiedCtorList),
        static_cast<void *>(
            copiedCtorList != NULL ? copiedCtorList->get_parent() : NULL));
    ROSE_ABORT();
  }

  const SgInitializedNamePtrList &originalCtors = originalCtorList->get_ctors();
  const SgInitializedNamePtrList &copiedCtors = copiedCtorList->get_ctors();
  if (originalCtors.size() != copiedCtors.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[member-ctor-count]: original=%p count=%zu "
            "copy=%p count=%zu\n",
            static_cast<const void *>(originalDecl), originalCtors.size(),
            static_cast<void *>(copiedDecl), copiedCtors.size());
    ROSE_ABORT();
  }

  for (size_t index = 0; index < originalCtors.size(); ++index) {
    SgInitializedName *originalCtor = originalCtors[index];
    SgInitializedName *copiedCtor = copiedCtors[index];
    if (originalCtor == NULL || copiedCtor == NULL ||
        requireExactMappedNode(originalCtor, help, "member-ctor-initializer") !=
            copiedCtor ||
        originalCtor->get_parent() != originalCtorList ||
        countDirectOwnerEdges(originalCtorList, originalCtor) != 1 ||
        copiedCtor->get_parent() != copiedCtorList ||
        countDirectOwnerEdges(copiedCtorList, copiedCtor) != 1) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[member-ctor-order-owner]: index=%zu "
              "original=%p owner=%p copy=%p owner=%p\n",
              index, static_cast<void *>(originalCtor),
              static_cast<void *>(
                  originalCtor != NULL ? originalCtor->get_parent() : NULL),
              static_cast<void *>(copiedCtor),
              static_cast<void *>(copiedCtor != NULL ? copiedCtor->get_parent()
                                                     : NULL));
      ROSE_ABORT();
    }
  }
}

void finalizeExactCopiedFunctionDeclarationChains(SgCopyHelp &help) {
  std::vector<const SgFunctionDeclaration *> originalFunctionDecls;
  originalFunctionDecls.reserve(help.get_copiedNodeMap().size());

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (const SgFunctionDeclaration *originalDecl =
            isSgFunctionDeclaration(it->first)) {
      originalFunctionDecls.push_back(originalDecl);
    }
  }

  std::unordered_set<const SgFunctionDeclaration *> processedDefiningDecls;

  for (const SgFunctionDeclaration *originalDecl : originalFunctionDecls) {
    const SgFunctionDeclaration *originalDefiningDecl =
        isSgFunctionDeclaration(originalDecl->get_definingDeclaration());
    if (originalDefiningDecl == NULL ||
        originalDefiningDecl->get_definition() == NULL) {
      continue;
    }
    if (!processedDefiningDecls.insert(originalDefiningDecl).second) {
      continue;
    }

    const SgFunctionDeclaration *originalFirstNondefiningDecl =
        isSgFunctionDeclaration(
            originalDefiningDecl->get_firstNondefiningDeclaration());
    const SgFunctionDefinition *originalDefinition =
        originalDefiningDecl->get_definition();
    if (originalDefiningDecl->get_definingDeclaration() !=
            originalDefiningDecl ||
        originalFirstNondefiningDecl == NULL ||
        originalFirstNondefiningDecl->get_firstNondefiningDeclaration() !=
            originalFirstNondefiningDecl ||
        originalFirstNondefiningDecl->get_definingDeclaration() !=
            originalDefiningDecl ||
        originalDefinition->get_declaration() != originalDefiningDecl ||
        originalDefinition->get_parent() != originalDefiningDecl ||
        countDirectOwnerEdges(originalDefiningDecl, originalDefinition) != 1) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[function-declaration-chain-input]: "
              "function=%p name=%s first=%p defining=%p definition=%p "
              "definition-owner=%p is not an exact reciprocal declaration "
              "family before copy\n",
              static_cast<const void *>(originalDefiningDecl),
              originalDefiningDecl->get_name().str(),
              static_cast<const void *>(originalFirstNondefiningDecl),
              static_cast<const void *>(
                  originalDefiningDecl->get_definingDeclaration()),
              static_cast<const void *>(originalDefinition),
              static_cast<void *>(originalDefinition->get_parent()));
      fprintf(stderr,
              "REX_COPY_INVARIANT[function-declaration-chain-input]: "
              "defining-self=%d first-self=%d first-defining=%p "
              "definition-declaration=%p owner-edges=%zu\n",
              originalDefiningDecl->get_definingDeclaration() ==
                      originalDefiningDecl
                  ? 1
                  : 0,
              originalFirstNondefiningDecl->get_firstNondefiningDeclaration() ==
                      originalFirstNondefiningDecl
                  ? 1
                  : 0,
              static_cast<void *>(
                  originalFirstNondefiningDecl->get_definingDeclaration()),
              static_cast<void *>(originalDefinition->get_declaration()),
              countDirectOwnerEdges(
                  const_cast<SgFunctionDeclaration *>(originalDefiningDecl),
                  const_cast<SgFunctionDefinition *>(originalDefinition)));
      ROSE_ABORT();
    }

    SgFunctionDeclaration *copiedCurrentDecl =
        isSgFunctionDeclaration(requireExactMappedNode(
            originalDecl, help, "current-function-declaration"));
    const bool copiedDefiningRole =
        lookupCopiedNode(help, originalDefiningDecl) != NULL &&
        lookupCopiedNode(help, originalDefiningDecl) != originalDefiningDecl;
    const bool copiedDefinitionRole =
        lookupCopiedNode(help, originalDefinition) != NULL &&
        lookupCopiedNode(help, originalDefinition) != originalDefinition;

    // Copying a declaration subtree does not imply copying a definition that
    // is structurally owned elsewhere.  The canonical example is an in-class
    // member prototype whose out-of-class definition is outside the copied
    // class.  In that exact partial-family case, retain and validate the
    // external defining edge while requiring every family role that is inside
    // the transaction to map to its unique copy.  A transaction that copied
    // only one of the defining declaration/definition owner pair is malformed.
    if (!copiedDefiningRole && !copiedDefinitionRole) {
      if (SgNode *plannedEdge = help.plannedExternalSemanticCopyEdge(
              originalFirstNondefiningDecl)) {
        auto *planned = isSgFunctionDeclaration(plannedEdge);
        ROSE_ASSERT(
            planned != nullptr && planned != originalFirstNondefiningDecl &&
            planned->get_firstNondefiningDeclaration() == planned &&
            planned->get_definingDeclaration() == originalDefiningDecl &&
            planned->get_scope() == copiedCurrentDecl->get_scope() &&
            copiedCurrentDecl->get_firstNondefiningDeclaration() == planned &&
            copiedCurrentDecl->get_definingDeclaration() ==
                originalDefiningDecl);
        continue;
      }
      // A copied redeclaration whose defining owner lies outside this
      // transaction remains a member of that exact external family.  In
      // particular, copying the canonical prototype itself must not reinterpret
      // its self edge as a new canonical family while retaining the old
      // defining edge; that combination is intrinsically non-reciprocal.
      copiedCurrentDecl->set_firstNondefiningDeclaration(
          const_cast<SgFunctionDeclaration *>(originalFirstNondefiningDecl));
      copiedCurrentDecl->set_definingDeclaration(
          const_cast<SgFunctionDeclaration *>(originalDefiningDecl));
      if (copiedCurrentDecl->get_firstNondefiningDeclaration() !=
              originalFirstNondefiningDecl ||
          copiedCurrentDecl->get_definingDeclaration() !=
              originalDefiningDecl ||
          copiedCurrentDecl->get_definition() != NULL) {
        fprintf(
            stderr,
            "REX_COPY_INVARIANT[partial-function-family]: original=%p "
            "name=%s first=%p defining=%p definition=%p copy=%p first=%p "
            "defining=%p definition=%p does not preserve an exact "
            "external declaration-family boundary\n",
            static_cast<const void *>(originalDecl),
            originalDecl->get_name().str(),
            static_cast<const void *>(
                originalDecl->get_firstNondefiningDeclaration()),
            static_cast<const void *>(originalDefiningDecl),
            static_cast<const void *>(originalDefinition),
            static_cast<void *>(copiedCurrentDecl),
            static_cast<void *>(
                copiedCurrentDecl->get_firstNondefiningDeclaration()),
            static_cast<void *>(copiedCurrentDecl->get_definingDeclaration()),
            static_cast<void *>(copiedCurrentDecl->get_definition()));
        ROSE_ABORT();
      }
      continue;
    }
    if (copiedDefiningRole != copiedDefinitionRole) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[partial-function-owner-pair]: original=%p "
              "name=%s defining=%p mapped=%p definition=%p mapped=%p\n",
              static_cast<const void *>(originalDecl),
              originalDecl->get_name().str(),
              static_cast<const void *>(originalDefiningDecl),
              static_cast<void *>(lookupCopiedNode(help, originalDefiningDecl)),
              static_cast<const void *>(originalDefinition),
              static_cast<void *>(lookupCopiedNode(help, originalDefinition)));
      ROSE_ABORT();
    }

    SgFunctionDeclaration *canonicalDefiningDecl =
        isSgFunctionDeclaration(requireExactMappedNode(
            originalDefiningDecl, help, "defining-function-declaration"));
    SgFunctionDeclaration *mappedFirstNondefiningDecl = isSgFunctionDeclaration(
        lookupCopiedNode(help, originalFirstNondefiningDecl));
    if (mappedFirstNondefiningDecl == NULL ||
        mappedFirstNondefiningDecl == originalFirstNondefiningDecl) {
      mappedFirstNondefiningDecl = isSgFunctionDeclaration(
          copyNodeIntoCurrentRoot(help, originalFirstNondefiningDecl));
    }
    const bool copiedFirstNondefiningRole =
        mappedFirstNondefiningDecl != NULL &&
        mappedFirstNondefiningDecl != originalFirstNondefiningDecl;
    SgFunctionDeclaration *canonicalFirstNondefiningDecl =
        mappedFirstNondefiningDecl;
    if (copiedFirstNondefiningRole &&
        canonicalFirstNondefiningDecl->get_parent() == NULL) {
      SgAuxiliaryDeclarationList *pendingOwner =
          new SgAuxiliaryDeclarationList();
      pendingOwner->get_declarations().push_back(canonicalFirstNondefiningDecl);
      canonicalFirstNondefiningDecl->set_parent(pendingOwner);
      if (pendingOwner->get_parent() != NULL ||
          pendingOwner->get_declarations().size() != 1 ||
          pendingOwner->get_declarations().front() !=
              canonicalFirstNondefiningDecl ||
          canonicalFirstNondefiningDecl->get_parent() != pendingOwner) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[pending-function-canonical-owner]: "
                "canonical=%p owner=%p does not form one detached auxiliary "
                "publication transaction\n",
                static_cast<void *>(canonicalFirstNondefiningDecl),
                static_cast<void *>(pendingOwner));
        ROSE_ABORT();
      }
      pendingOwner->validate_semantic_non_output_role();
    }
    SgFunctionDefinition *canonicalDefinition =
        isSgFunctionDefinition(requireExactMappedNode(originalDefinition, help,
                                                      "function-definition"));
    if (canonicalDefiningDecl == NULL ||
        canonicalFirstNondefiningDecl == NULL || canonicalDefinition == NULL ||
        canonicalDefiningDecl->variantT() != originalDefiningDecl->variantT() ||
        canonicalFirstNondefiningDecl->variantT() !=
            originalFirstNondefiningDecl->variantT() ||
        canonicalDefinition->get_declaration() != canonicalDefiningDecl ||
        canonicalDefinition->get_parent() != canonicalDefiningDecl ||
        countDirectOwnerEdges(canonicalDefiningDecl, canonicalDefinition) !=
            1) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[function-declaration-chain-output]: "
              "original=%p copy=%p name=%s original-first=%p copied-first=%p "
              "original-definition=%p copied-definition=%p definition-owner=%p "
              "did not copy the exact canonical declaration family\n",
              static_cast<const void *>(originalDefiningDecl),
              static_cast<void *>(canonicalDefiningDecl),
              canonicalDefiningDecl->get_name().str(),
              static_cast<const void *>(originalFirstNondefiningDecl),
              static_cast<void *>(canonicalFirstNondefiningDecl),
              static_cast<const void *>(originalDefinition),
              static_cast<void *>(canonicalDefinition),
              static_cast<void *>(canonicalDefinition != NULL
                                      ? canonicalDefinition->get_parent()
                                      : NULL));
      ROSE_ABORT();
    }

    if (copiedFirstNondefiningRole) {
      canonicalFirstNondefiningDecl->set_firstNondefiningDeclaration(
          canonicalFirstNondefiningDecl);
      canonicalFirstNondefiningDecl->set_definingDeclaration(
          canonicalDefiningDecl);
    }

    canonicalDefiningDecl->set_definingDeclaration(canonicalDefiningDecl);
    canonicalDefiningDecl->set_firstNondefiningDeclaration(
        canonicalFirstNondefiningDecl);

    if (canonicalFirstNondefiningDecl->get_firstNondefiningDeclaration() !=
            canonicalFirstNondefiningDecl ||
        canonicalFirstNondefiningDecl->get_definingDeclaration() !=
            canonicalDefiningDecl ||
        canonicalDefiningDecl->get_firstNondefiningDeclaration() !=
            canonicalFirstNondefiningDecl ||
        canonicalDefiningDecl->get_definingDeclaration() !=
            canonicalDefiningDecl ||
        canonicalDefiningDecl->get_definition() != canonicalDefinition ||
        canonicalDefinition->get_declaration() != canonicalDefiningDecl) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[function-declaration-chain-final]: "
          "first=%p first->first=%p first->defining=%p defining=%p "
          "defining->first=%p defining->defining=%p definition=%p "
          "definition->declaration=%p\n",
          static_cast<void *>(canonicalFirstNondefiningDecl),
          static_cast<void *>(
              canonicalFirstNondefiningDecl->get_firstNondefiningDeclaration()),
          static_cast<void *>(
              canonicalFirstNondefiningDecl->get_definingDeclaration()),
          static_cast<void *>(canonicalDefiningDecl),
          static_cast<void *>(
              canonicalDefiningDecl->get_firstNondefiningDeclaration()),
          static_cast<void *>(canonicalDefiningDecl->get_definingDeclaration()),
          static_cast<void *>(canonicalDefinition),
          static_cast<void *>(canonicalDefinition->get_declaration()));
      ROSE_ABORT();
    }

    if (copiedFirstNondefiningRole &&
        isSgAuxiliaryDeclarationList(
            originalFirstNondefiningDecl->get_parent()) != NULL) {
      SgAuxiliaryDeclarationList *copiedOwner = isSgAuxiliaryDeclarationList(
          canonicalFirstNondefiningDecl->get_parent());
      const bool exactPendingOwner =
          (copiedOwner == NULL &&
           canonicalFirstNondefiningDecl->get_parent() == NULL) ||
          (copiedOwner != NULL && copiedOwner->get_parent() == NULL &&
           copiedOwner->get_declarations().size() == 1 &&
           copiedOwner->get_declarations().front() ==
               canonicalFirstNondefiningDecl);
      const bool exactPublishedOwner =
          copiedOwner != NULL &&
          canonicalFirstNondefiningDecl->get_scope() != NULL &&
          copiedOwner->get_parent() ==
              canonicalFirstNondefiningDecl->get_scope() &&
          canonicalFirstNondefiningDecl->get_scope()
                  ->get_auxiliary_declarations() == copiedOwner;
      if ((!exactPendingOwner && !exactPublishedOwner) ||
          !SgScopeStatement::isEquivalentScope(
              canonicalFirstNondefiningDecl->get_scope(),
              canonicalDefiningDecl->get_scope()) ||
          canonicalFirstNondefiningDecl->get_file_info() == NULL ||
          originalFirstNondefiningDecl->get_file_info() == NULL ||
          canonicalFirstNondefiningDecl->get_file_info()
                  ->isOutputInCodeGeneration() !=
              originalFirstNondefiningDecl->get_file_info()
                  ->isOutputInCodeGeneration() ||
          canonicalDefiningDecl->get_file_info() == NULL ||
          originalDefiningDecl->get_file_info() == NULL ||
          canonicalDefiningDecl->get_file_info()->isOutputInCodeGeneration() !=
              originalDefiningDecl->get_file_info()
                  ->isOutputInCodeGeneration()) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[function-declaration-canonical-owner]: "
                "canonical=%p defining=%p name=%s owner=%p owner-scope=%p "
                "canonical-scope=%p defining-scope=%p output=%d "
                "original-output=%d defining-output=%d "
                "original-defining-output=%d\n",
                static_cast<void *>(canonicalFirstNondefiningDecl),
                static_cast<void *>(canonicalDefiningDecl),
                canonicalDefiningDecl->get_name().str(),
                static_cast<void *>(copiedOwner),
                static_cast<void *>(
                    copiedOwner != NULL ? copiedOwner->get_parent() : NULL),
                static_cast<void *>(canonicalFirstNondefiningDecl->get_scope()),
                static_cast<void *>(canonicalDefiningDecl->get_scope()),
                canonicalFirstNondefiningDecl->get_file_info() != NULL &&
                        canonicalFirstNondefiningDecl->get_file_info()
                            ->isOutputInCodeGeneration()
                    ? 1
                    : 0,
                originalFirstNondefiningDecl->get_file_info() != NULL &&
                        originalFirstNondefiningDecl->get_file_info()
                            ->isOutputInCodeGeneration()
                    ? 1
                    : 0,
                canonicalDefiningDecl->get_file_info() != NULL &&
                        canonicalDefiningDecl->get_file_info()
                            ->isOutputInCodeGeneration()
                    ? 1
                    : 0,
                originalDefiningDecl->get_file_info() != NULL &&
                        originalDefiningDecl->get_file_info()
                            ->isOutputInCodeGeneration()
                    ? 1
                    : 0);
        ROSE_ABORT();
      }
    }
  }
}

bool declarationIsStillOwnedByScope(SgDeclarationStatement *declaration,
                                    SgScopeStatement *scope) {
  if (declaration == NULL || scope == NULL) {
    return false;
  }

  return scopeDirectlyContainsStatement(scope, declaration);
}

void collectDetachedSubtreeNodes(SgNode *node, std::vector<SgNode *> &nodes,
                                 std::unordered_set<SgNode *> &seen) {
  if (node == NULL || !SgNode::isLiveNode(node) || !seen.insert(node).second) {
    return;
  }

  const std::vector<SgNode *> children =
      node->get_traversalSuccessorContainer();
  for (SgNode *child : children) {
    if (child != NULL && child->get_parent() == node) {
      collectDetachedSubtreeNodes(child, nodes, seen);
    }
  }

  nodes.push_back(node);
}

void deleteDetachedCopiedSubtree(SgNode *root) {
  if (root == NULL) {
    return;
  }

  std::vector<SgNode *> nodes;
  nodes.reserve(16);
  std::unordered_set<SgNode *> seen;
  seen.reserve(16);
  collectDetachedSubtreeNodes(root, nodes, seen);

  for (SgNode *node : nodes) {
    delete node;
  }
}

bool declarationMustDefineItself(const SgDeclarationStatement *decl) {
  if (decl == NULL) {
    return false;
  }

  switch (decl->variantT()) {
  case V_SgAsmStmt:
  case V_SgCtorInitializerList:
  case V_SgVariableDefinition:
  case V_SgPragmaDeclaration:
  case V_SgUsingDirectiveStatement:
  case V_SgUsingDeclarationStatement:
  case V_SgNamespaceAliasDeclarationStatement:
  case V_SgTemplateInstantiationDirectiveStatement:
    return true;

  default:
    return false;
  }
}

SgClassType *
finalizeExactCopiedClassType(const SgClassDeclaration *originalClassDecl,
                             SgClassDeclaration *copyClassDecl,
                             SgCopyHelp &help) {
  ASSERT_not_null(originalClassDecl);
  ASSERT_not_null(copyClassDecl);

  const SgClassDeclaration *originalFirst = isSgClassDeclaration(
      originalClassDecl->get_firstNondefiningDeclaration());
  const SgClassDeclaration *originalDefining =
      isSgClassDeclaration(originalClassDecl->get_definingDeclaration());
  if (originalFirst == NULL ||
      originalFirst->get_firstNondefiningDeclaration() != originalFirst ||
      originalFirst->get_definingDeclaration() != originalDefining ||
      (originalDefining != NULL &&
       (originalDefining->get_definingDeclaration() != originalDefining ||
        originalDefining->get_firstNondefiningDeclaration() !=
            originalFirst))) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[class-chain-input]: declaration=%p/%s "
            "name=%s first=%p first-first=%p first-defining=%p "
            "defining=%p declaration-parent=%p/%s first-parent=%p/%s "
            "forward=%d first-forward=%d\n",
            static_cast<const void *>(originalClassDecl),
            originalClassDecl->class_name().c_str(),
            originalClassDecl->get_name().getString().c_str(),
            static_cast<const void *>(originalFirst),
            static_cast<const void *>(
                originalFirst != NULL
                    ? originalFirst->get_firstNondefiningDeclaration()
                    : NULL),
            static_cast<const void *>(
                originalFirst != NULL ? originalFirst->get_definingDeclaration()
                                      : NULL),
            static_cast<const void *>(originalDefining),
            static_cast<const void *>(originalClassDecl->get_parent()),
            originalClassDecl->get_parent() != NULL
                ? originalClassDecl->get_parent()->class_name().c_str()
                : "<null>",
            static_cast<const void *>(
                originalFirst != NULL ? originalFirst->get_parent() : NULL),
            originalFirst != NULL && originalFirst->get_parent() != NULL
                ? originalFirst->get_parent()->class_name().c_str()
                : "<null>",
            originalClassDecl->isForward() ? 1 : 0,
            originalFirst != NULL && originalFirst->isForward() ? 1 : 0);
    ROSE_ABORT();
  }

  SgClassDeclaration *copyFirst =
      isSgClassDeclaration(requireExactCopiedOrExternalSemanticEdge(
          originalFirst, copyClassDecl->get_firstNondefiningDeclaration(), help,
          "first-nondefining-class-declaration"));
  SgClassDeclaration *copyDefining =
      originalDefining != NULL
          ? isSgClassDeclaration(requireExactCopiedOrExternalSemanticEdge(
                originalDefining, copyClassDecl->get_definingDeclaration(),
                help, "defining-class-declaration"))
          : NULL;
  if (copyFirst == NULL || (originalDefining != NULL && copyDefining == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[class-chain-output]: declaration=%p copy=%p "
            "first=%p defining=%p\n",
            static_cast<const void *>(originalClassDecl),
            static_cast<void *>(copyClassDecl), static_cast<void *>(copyFirst),
            static_cast<void *>(copyDefining));
    ROSE_ABORT();
  }

  // A copied declaration family is canonicalized only across declarations
  // that actually participate in the structural transaction. External
  // family members retain their exact source state; mutating them would
  // corrupt the source AST and create a cross-root declaration family.
  if (copyFirst != originalFirst) {
    copyFirst->set_firstNondefiningDeclaration(copyFirst);
    copyFirst->set_definingDeclaration(copyDefining);
  }
  if (copyDefining != NULL && copyDefining != originalDefining) {
    copyDefining->set_firstNondefiningDeclaration(copyFirst);
    copyDefining->set_definingDeclaration(copyDefining);
  }
  copyClassDecl->set_firstNondefiningDeclaration(copyFirst);
  copyClassDecl->set_definingDeclaration(copyDefining);

  SgClassType *originalType = isSgClassType(originalFirst->get_type());
  if (originalType == NULL || originalClassDecl->get_type() != originalType ||
      (originalDefining != NULL &&
       originalDefining->get_type() != originalType)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[class-canonical-type-input]: "
            "declaration=%p type=%p first=%p type=%p defining=%p type=%p\n",
            static_cast<const void *>(originalClassDecl),
            static_cast<void *>(originalClassDecl->get_type()),
            static_cast<const void *>(originalFirst),
            static_cast<void *>(
                originalFirst != NULL ? originalFirst->get_type() : NULL),
            static_cast<const void *>(originalDefining),
            static_cast<void *>(originalDefining != NULL
                                    ? originalDefining->get_type()
                                    : NULL));
    ROSE_ABORT();
  }

  SgClassType *copyType = NULL;
  SgCopyHelp::copiedNodeMapTypeIterator typeMapping =
      help.get_copiedNodeMap().find(originalType);
  if (typeMapping == help.get_copiedNodeMap().end() ||
      typeMapping->second == originalType) {
    copyType = originalType;
  } else {
    copyType =
        isSgClassType(requireExactMappedNode(originalType, help, "class-type"));
  }
  if (copyType == NULL || copyClassDecl->get_type() != copyType ||
      copyFirst->get_type() != copyType ||
      (copyDefining != NULL && copyDefining->get_type() != copyType)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[class-canonical-type-output]: "
            "declaration=%p type=%p first=%p type=%p defining=%p type=%p "
            "expected-type=%p\n",
            static_cast<void *>(copyClassDecl),
            static_cast<void *>(copyClassDecl->get_type()),
            static_cast<void *>(copyFirst),
            static_cast<void *>(copyFirst->get_type()),
            static_cast<void *>(copyDefining),
            static_cast<void *>(copyDefining != NULL ? copyDefining->get_type()
                                                     : NULL),
            static_cast<void *>(copyType));
    ROSE_ABORT();
  }

  SgClassDeclaration *originalTypeDecl =
      isSgClassDeclaration(originalType->get_declaration());
  if (originalTypeDecl == NULL) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[class-type-declaration-input]: type=%p "
            "declaration=%p\n",
            static_cast<void *>(originalType),
            static_cast<void *>(originalType->get_declaration()));
    ROSE_ABORT();
  }
  if (copyType != originalType) {
    SgClassDeclaration *copyTypeDecl =
        isSgClassDeclaration(requireExactMappedNode(originalTypeDecl, help,
                                                    "class-type-declaration"));
    if (copyTypeDecl == NULL || copyType->get_declaration() != copyTypeDecl) {
      ROSE_ABORT();
    }
  }

  return copyType;
}

void finalizeExactCopiedClassTypes(SgCopyHelp &help) {
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgClassDeclaration *originalClassDecl =
        isSgClassDeclaration(it->first);
    SgClassDeclaration *copyClassDecl = isSgClassDeclaration(it->second);
    if (originalClassDecl == NULL || copyClassDecl == NULL) {
      continue;
    }

    finalizeExactCopiedClassType(originalClassDecl, copyClassDecl, help);
  }
}

bool isTypeReverseCacheField(const SgName &field) {
  const std::string name = field.getString();
  return name == "ref_to" || name == "ptr_to" || name == "modifiers" ||
         name == "rvalue_ref_to";
}

bool isTypeReverseOwnershipField(const SgName &field) {
  return field.getString() == "parent";
}

bool hasNonidentityMappedOwner(const SgNode *node, SgCopyHelp &help);

class ExactCopiedTypeGraphMaterializer {
  SgCopyHelp &help_;
  std::unordered_map<const SgNode *, SgNode *> &replacements_;
  std::unordered_map<const SgType *, SgType *> provisionalCopySources_;
  std::set<const SgType *> active_;
  std::unordered_set<const SgType *> fortranSourceTypes_;
  const bool copyOwnedSyntax_;

  class CollectFortranSourceTypes : public SimpleReferenceToPointerHandler {
    std::unordered_set<const SgType *> &types_;

  public:
    explicit CollectFortranSourceTypes(
        std::unordered_set<const SgType *> &types)
        : types_(types) {}

    void operator()(SgNode *&edge, const SgName &field, bool) override {
      if (isTypeReverseCacheField(field) || isTypeReverseOwnershipField(field))
        return;
      auto *type = isSgType(edge);
      if (type != nullptr && type->get_fortran_source_syntax() &&
          types_.insert(type).second)
        type->processDataMemberReferenceToPointers(this);
    }
  };

  class DiscoverDependencies : public SimpleReferenceToPointerHandler {
    ExactCopiedTypeGraphMaterializer &materializer_;
    const SgType *source_;

  public:
    bool needsCopy = false;

    DiscoverDependencies(ExactCopiedTypeGraphMaterializer &materializer,
                         const SgType *source)
        : materializer_(materializer), source_(source) {}

    void operator()(SgNode *&edge, const SgName &field, bool) override {
      if (edge == NULL || isTypeReverseCacheField(field) ||
          isTypeReverseOwnershipField(field)) {
        return;
      }
      if (SgType *childType = isSgType(edge)) {
        if (childType != source_ &&
            materializer_.materialize(childType) != childType) {
          needsCopy = true;
        }
        return;
      }
      if (auto *function = isSgFunctionType(source_)) {
        if (edge == function->get_argument_list())
          return;
      }
      if (auto *symbol = isSgSymbol(edge);
          isSgVariableSymbol(symbol) != nullptr ||
          isSgNonrealSymbol(symbol) != nullptr) {
        auto *basis = symbol->get_symbol_basis();
        ASSERT_not_null(basis);
        if (materializer_.copyOwnedSyntax_ &&
            lookupCopiedNode(materializer_.help_, symbol) == nullptr &&
            hasNonidentityMappedOwner(basis, materializer_.help_)) {
          const SgNode *owner = basis;
          if (auto *declaration = isSgVariableDeclaration(basis->get_parent()))
            owner = declaration;
          SgNode *copiedOwner = materializer_.help_.copyOrLookupOwnedAst(owner);
          ROSE_ASSERT(copiedOwner != owner);
          SgNode *copiedSymbol =
              materializer_.help_.copyOrLookupOwnedAst(symbol);
          ROSE_ASSERT(copiedSymbol != symbol);
        }
      }
      SgCopyHelp::copiedNodeMapTypeIterator mapped =
          materializer_.help_.get_copiedNodeMap().find(edge);
      if (mapped != materializer_.help_.get_copiedNodeMap().end() &&
          mapped->second != edge) {
        needsCopy = true;
      }
      if (isSgExpression(edge) != nullptr && edge->get_parent() == source_) {
        RoseAst expression(edge);
        for (SgNode *node : expression) {
          auto *value = isSgExpression(node);
          if (value != nullptr && value->has_semantic_value_type()) {
            SgType *type = value->get_type();
            if (type != source_ && materializer_.materialize(type) != type)
              needsCopy = true;
          }
          node->processDataMemberReferenceToPointers(this);
        }
      }
    }
  };

  class CanonicalizeCopiedType : public SimpleReferenceToPointerHandler {
    ExactCopiedTypeGraphMaterializer &materializer_;

  public:
    explicit CanonicalizeCopiedType(
        ExactCopiedTypeGraphMaterializer &materializer)
        : materializer_(materializer) {}

    void operator()(SgNode *&edge, const SgName &field, bool) override {
      if (edge == NULL) {
        return;
      }
      if (isTypeReverseCacheField(field)) {
        // Copy construction starts these caches empty. Any entries now present
        // were published for this type identity; clearing them would allow a
        // later factory call to create a competing canonical wrapper.
        return;
      }
      if (SgType *edgeType = isSgType(edge);
          edgeType != nullptr && !isTypeReverseOwnershipField(field)) {
        edge = materializer_.materialize(edgeType);
        return;
      }
      SgCopyHelp::copiedNodeMapTypeIterator mapped =
          materializer_.help_.get_copiedNodeMap().find(edge);
      if (mapped != materializer_.help_.get_copiedNodeMap().end() &&
          mapped->second != NULL) {
        edge = mapped->second;
      }
    }
  };

  void bindCopiedSourceOwner(const SgType *source, SgType *copy) {
    const bool prototype = source->has_source_prototype();
    auto *member = isSgPointerMemberType(source);
    if (!prototype && isSgDeclType(source) == nullptr &&
        isSgTypeOfType(source) == nullptr &&
        (member == nullptr ||
         SgPointerMemberType::isCanonicalSemanticType(member))) {
      return;
    }
    auto *owner = isSgTypedefDeclaration(source->get_parent());
    if (owner == nullptr) {
      return;
    }
    ROSE_ASSERT(owner->get_base_type() == source);
    auto mapped = help_.get_copiedNodeMap().find(owner);
    if (mapped == help_.get_copiedNodeMap().end() || mapped->second == owner) {
      return;
    }
    auto *copiedOwner = isSgTypedefDeclaration(mapped->second);
    ROSE_ASSERT(copiedOwner != nullptr);
    if (prototype) {
      ROSE_ASSERT(copiedOwner->get_base_type() == copy &&
                  copy->get_parent() == copiedOwner);
      return;
    }
    ROSE_ASSERT(copy->get_parent() == nullptr || copy->get_parent() == owner ||
                copy->get_parent() == copiedOwner);
    copy->set_parent(copiedOwner);
  }

public:
  ExactCopiedTypeGraphMaterializer(
      SgCopyHelp &help,
      std::unordered_map<const SgNode *, SgNode *> &replacements,
      bool copyOwnedSyntax = true)
      : help_(help), replacements_(replacements),
        copyOwnedSyntax_(copyOwnedSyntax) {
    CollectFortranSourceTypes collect(fortranSourceTypes_);
    for (const auto &mapping : help_.get_copiedNodeMap()) {
      if (mapping.first == mapping.second)
        continue;
      if (const auto *name = isSgInitializedName(mapping.first)) {
        SgNode *syntax = name->get_fortran_source_type();
        collect(syntax, SgName(), false);
      }
    }
  }

  SgNode *materializeCopiedEdge(SgNode *source) {
    if (source == NULL) {
      return NULL;
    }
    if (SgType *sourceType = isSgType(source)) {
      return materialize(sourceType);
    }
    SgCopyHelp::copiedNodeMapTypeIterator mapped =
        help_.get_copiedNodeMap().find(source);
    if (mapped != help_.get_copiedNodeMap().end() && mapped->second != NULL) {
      return mapped->second;
    }
    return source;
  }

  SgType *materialize(SgType *source) {
    ASSERT_not_null(source);
    SgNode *dependency = nullptr;
    if (auto *templateType = isSgTemplateType(source))
      dependency = templateType->get_template_parameter();
    else if (auto *namedType = isSgNamedType(source))
      dependency = namedType->get_declaration();
    const auto &originals = help_.get_originalNodesForCopy(source);
    const bool copiedType = std::any_of(
        originals.begin(), originals.end(), [source](const SgNode *original) {
          return original != source && isSgType(original) != nullptr;
        });
    auto *projectionOwner =
        copiedType ? nullptr : isSgNonrealType(source->get_parent());
    if (projectionOwner != nullptr) {
      auto *declaration = isSgNonrealDecl(projectionOwner->get_declaration());
      ROSE_ASSERT(declaration != nullptr &&
                  declaration->get_type() == projectionOwner);
      dependency = declaration;
    }
    if (auto *nonreal = isSgNonrealDecl(dependency))
      if (auto *record =
              isSgClassDeclaration(nonreal->get_templateDeclaration()))
        materialize(record->get_type());
    SgDeclarationStatement *definition = nullptr;
    if (isSgClassDeclaration(dependency) || isSgEnumDeclaration(dependency))
      definition =
          isSgDeclarationStatement(dependency)->get_definingDeclaration();
    const bool needsDefinition =
        definition != nullptr && definition != dependency &&
        lookupCopiedNode(help_, definition) == nullptr &&
        !help_.isCopyingOriginal(definition);
    if (copyOwnedSyntax_ && dependency != nullptr &&
        (lookupCopiedNode(help_, dependency) == nullptr || needsDefinition) &&
        !help_.isCopyingOriginal(dependency)) {
      for (SgNode *owner = dependency->get_parent(); owner != nullptr;
           owner = owner->get_parent()) {
        SgNode *copiedOwner = lookupCopiedNode(help_, owner);
        if (help_.isCopyingOriginal(owner) ||
            (copiedOwner != nullptr && copiedOwner != owner)) {
          SgCopyHelp::ConstructionScope ownership(help_, owner);
          if (auto *nonreal = isSgNonrealDecl(dependency)) {
            if (!nonreal->get_is_concept()) {
              auto *scope = nonreal->get_declarationScope();
              ASSERT_not_null(scope);
              ROSE_ASSERT(nonreal->get_parent() == scope);
              SgNode *copiedScope = help_.copyOrLookupOwnedAst(scope);
              ROSE_ASSERT(copiedScope != scope);
            }
          }
          SgNode *constructed = help_.copyOrLookupOwnedAst(dependency);
          ROSE_ASSERT(constructed != dependency);
          if (needsDefinition) {
            SgNode *constructedDefinition =
                help_.copyOrLookupOwnedAst(definition);
            ROSE_ASSERT(constructedDefinition != definition);
          }
          ROSE_ASSERT(lookupCopiedNode(help_, dependency) != nullptr);
          ROSE_ASSERT(!needsDefinition ||
                      (lookupCopiedNode(help_, definition) != nullptr &&
                       lookupCopiedNode(help_, definition) != definition));
          if (isSgClassType(source) || isSgEnumType(source)) {
            auto *copy = isSgType(lookupCopiedNode(help_, source));
            ROSE_ASSERT(copy != nullptr && copy != source &&
                        copy->variantT() == source->variantT());
            const auto inserted = replacements_.emplace(source, copy);
            ROSE_ASSERT(inserted.second || inserted.first->second == copy);
          }
          break;
        }
      }
    }
    if (copyOwnedSyntax_ && projectionOwner != nullptr &&
        !help_.isCopyingOriginal(projectionOwner)) {
      auto *copiedOwner =
          isSgNonrealType(help_.copyOrLookupOwnedAst(projectionOwner));
      ROSE_ASSERT(copiedOwner != nullptr && copiedOwner != projectionOwner);
    }
    const auto replacement = replacements_.find(source);
    if (replacement != replacements_.end()) {
      SgType *copy = isSgType(replacement->second);
      if (copy == NULL) {
        ROSE_ABORT();
      }
      return copy;
    }

    const auto provisionalSource = provisionalCopySources_.find(source);
    if (provisionalSource != provisionalCopySources_.end()) {
      return materialize(provisionalSource->second);
    }

    // An exclusively owned type shell can be cloned directly by its
    // containing node before the semantic type graph is canonicalized.  When
    // the rewriter later encounters that copied shell, continue the original
    // source transaction in place; treating the copy as a fresh source would
    // manufacture a copy-of-a-copy and split its owner edge.
    const SgType *mappedSource = NULL;
    bool canonicalMappedTarget = false;
    for (const SgNode *original : help_.get_originalNodesForCopy(source)) {
      if (original == source || isSgType(original) == NULL) {
        continue;
      }
      if (mappedSource != NULL && mappedSource != original) {
        if (!isExactCanonicalTypeConvergence(mappedSource, original, source,
                                             help_)) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[copied-type-reverse-map]: copy=%p/%s "
                  "has multiple source types without exact canonical "
                  "constituents\n",
                  static_cast<void *>(source), source->class_name().c_str());
          ROSE_ABORT();
        }
        canonicalMappedTarget = true;
        continue;
      }
      mappedSource = isSgType(original);
    }
    if (canonicalMappedTarget) {
      return source;
    }
    if (mappedSource != NULL) {
      return materialize(const_cast<SgType *>(mappedSource));
    }

    // Class and enum wrappers require complete declaration-family publication.
    if (isSgClassType(source) != NULL || isSgEnumType(source) != NULL) {
      const auto mappedNamedType = help_.get_copiedNodeMap().find(source);
      if (mappedNamedType != help_.get_copiedNodeMap().end() &&
          mappedNamedType->second != source) {
        auto *copy = isSgNamedType(
            requireExactMappedNode(source, help_, "named-type-publication"));
        ASSERT_not_null(copy);
        auto *owner = isSgNamedType(source)->get_declaration();
        auto *copiedOwner = isSgDeclarationStatement(requireExactMappedNode(
            owner, help_, "named-type-publication-owner"));
        ROSE_ASSERT(copiedOwner != nullptr &&
                    owner->get_firstNondefiningDeclaration() == owner &&
                    copiedOwner->get_firstNondefiningDeclaration() ==
                        copiedOwner &&
                    copy->get_declaration() == copiedOwner);
        if (auto *classOwner = isSgClassDeclaration(owner)) {
          auto *copiedClass = isSgClassDeclaration(copiedOwner);
          ROSE_ASSERT(copiedClass != nullptr &&
                      classOwner->get_type() == source &&
                      copiedClass->get_type() == copy);
          help_.constructClassTypeCopy(classOwner);
        } else {
          auto *enumOwner = isSgEnumDeclaration(owner);
          auto *copiedEnum = isSgEnumDeclaration(copiedOwner);
          ROSE_ASSERT(enumOwner != nullptr && copiedEnum != nullptr &&
                      enumOwner->get_type() == source &&
                      copiedEnum->get_type() == copy);
          help_.constructEnumTypeCopy(enumOwner);
        }
        ROSE_ASSERT(lookupCopiedNode(help_, source) == copy &&
                    copy->get_declaration() == copiedOwner);
        ROSE_ASSERT(replacements_.emplace(source, copy).second);
        return copy;
      }
      return source;
    }

    if (!active_.insert(source).second) {
      // Forward semantic type cycles are resolved by the caller that first
      // entered the cycle. Reverse caches and ownership links never reach
      // this path.
      return source;
    }

    SgFunctionType *sourceFunction = isSgFunctionType(source);
    const bool sourcePrototype = source->has_source_prototype();
    auto *memberSource = isSgPointerMemberType(source);
    std::vector<SgNode *> ownedChildren;
    if (const SgNode *owner = source->get_parent())
      owner->appendDestructorOwnedNodes(ownedChildren);
    const bool ownsSyntax =
        std::find(ownedChildren.begin(), ownedChildren.end(), source) !=
            ownedChildren.end() ||
        fortranSourceTypes_.count(source) != 0 || sourcePrototype ||
        isSgTypeOfType(source) != nullptr || isSgDeclType(source) != nullptr ||
        (memberSource != nullptr &&
         !SgPointerMemberType::isCanonicalSemanticType(memberSource) &&
         isSgTypedefDeclaration(source->get_parent()) != nullptr);
    if (!copyOwnedSyntax_ && ownsSyntax) {
      // Ownership transfer keeps the occurrence and its children. Only its
      // semantic edges change to the identities prepared for the destination.
      CanonicalizeCopiedType project(*this);
      RoseAst syntax(source);
      for (SgNode *node : syntax)
        if (node == source || isSgType(node) == nullptr)
          node->processDataMemberReferenceToPointers(&project);
      if (sourceFunction != nullptr && sourcePrototype)
        sourceFunction->get_argument_list()->validate_source_prototype();
      active_.erase(source);
      return source;
    }
    if (sourcePrototype) {
      auto *existingCopy = isSgType(lookupCopiedNode(help_, source));
      ROSE_ASSERT(existingCopy != nullptr && existingCopy != source);
      ROSE_ASSERT(existingCopy->has_source_prototype());
      ROSE_ASSERT(existingCopy->variantT() == source->variantT());
      if (sourceFunction != nullptr)
        sourceFunction->get_argument_list()->validate_source_prototype();
      ROSE_ASSERT(replacements_.emplace(source, existingCopy).second);
      bindCopiedSourceOwner(source, existingCopy);
      active_.erase(source);
      return existingCopy;
    }
    DiscoverDependencies discover(*this, source);
    // Source-form types keep their use-site syntax when semantic constituents
    // are shared. Copy the owned occurrence before rebinding its typed edges.
    discover.needsCopy = ownsSyntax;
    source->processDataMemberReferenceToPointers(&discover);
    if (sourceFunction != NULL) {
      for (SgType *argument : sourceFunction->get_arguments()) {
        if (materialize(argument) != argument)
          discover.needsCopy = true;
      }
    }
    SgCopyHelp::copiedNodeMapTypeIterator existing =
        help_.get_copiedNodeMap().find(source);
    SgType *existingCopy = NULL;
    if (existing != help_.get_copiedNodeMap().end() &&
        existing->second != source) {
      existingCopy = isSgType(existing->second);
      if (existingCopy == NULL ||
          existingCopy->variantT() != source->variantT()) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[dependent-type-existing-map]: "
                "source=%p/%s mapped=%p/%s\n",
                static_cast<void *>(source), source->class_name().c_str(),
                static_cast<void *>(existing->second),
                existing->second != NULL
                    ? existing->second->class_name().c_str()
                    : "<null>");
        ROSE_ABORT();
      }
    }
    // An explicit function-type copy owns fresh parameter metadata even when
    // its semantic constituent types are shared.
    if (!discover.needsCopy &&
        (sourceFunction == nullptr || existingCopy == nullptr)) {
      if (existingCopy != NULL) {
        provisionalCopySources_[existingCopy] = source;
        const auto alias = replacements_.emplace(existingCopy, source);
        if (!alias.second && alias.first->second != source) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[shared-type-alias]: source=%p/%s "
                  "incidental-copy=%p existing-replacement=%p\n",
                  static_cast<void *>(source), source->class_name().c_str(),
                  static_cast<void *>(existingCopy),
                  static_cast<void *>(alias.first->second));
          ROSE_ABORT();
        }
        help_.assignCopiedNodePair(source, source);
      } else {
        help_.insertCopiedNodePair(source, source);
      }
      active_.erase(source);
      return source;
    }

    SgType *sourceWrapperBase = NULL;
    SgType *sourceMemberClass = NULL;
    if (SgPointerMemberType *pointer = isSgPointerMemberType(source)) {
      // TypeLoc wrappers own source syntax such as typedef pointees. Copy
      // those through the ordinary typed-node path below; only semantic
      // pointer-member types belong in the canonical constituent cache.
      if (SgPointerMemberType::isCanonicalSemanticType(pointer)) {
        sourceWrapperBase = pointer->get_base_type();
        sourceMemberClass = pointer->get_class_type();
      }
    } else if (SgPointerType *pointer = isSgPointerType(source)) {
      sourceWrapperBase = pointer->get_base_type();
    } else if (SgReferenceType *reference = isSgReferenceType(source)) {
      sourceWrapperBase = reference->get_base_type();
    } else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(source)) {
      sourceWrapperBase = reference->get_base_type();
    } else if (SgModifierType *modifier = isSgModifierType(source)) {
      if (modifier->get_type_kind() == nullptr)
        sourceWrapperBase = modifier->get_base_type();
    }
    if (sourceWrapperBase != NULL && !ownsSyntax &&
        !source->get_fortran_source_syntax()) {
      SgType *copiedWrapperBase = materialize(sourceWrapperBase);
      SgType *copiedMemberClass =
          sourceMemberClass != NULL ? materialize(sourceMemberClass) : NULL;

      // An already copied wrapper must have the exact target constituents.
      // Publish its identity when the target cache is still empty, or require
      // agreement with the wrapper already published there.
      if (existingCopy != NULL) {
        bool exactExistingWrapper = false;
        if (SgPointerMemberType *existingMember =
                isSgPointerMemberType(existingCopy)) {
          exactExistingWrapper =
              existingMember->get_base_type() == copiedWrapperBase &&
              existingMember->get_class_type() == copiedMemberClass &&
              SgPointerMemberType::isCanonicalSemanticType(existingMember);
        } else if (SgPointerType *existingPointer =
                       isSgPointerType(existingCopy)) {
          SgPointerType *cached = copiedWrapperBase->get_ptr_to();
          exactExistingWrapper =
              existingPointer->get_base_type() == copiedWrapperBase &&
              (cached == NULL || cached == existingPointer);
          if (exactExistingWrapper && cached == NULL)
            copiedWrapperBase->set_ptr_to(existingPointer);
        } else if (SgReferenceType *existingReference =
                       isSgReferenceType(existingCopy)) {
          SgReferenceType *cached = copiedWrapperBase->get_ref_to();
          exactExistingWrapper =
              existingReference->get_base_type() == copiedWrapperBase &&
              (cached == NULL || cached == existingReference);
          if (exactExistingWrapper && cached == NULL)
            copiedWrapperBase->set_ref_to(existingReference);
        } else if (SgRvalueReferenceType *existingReference =
                       isSgRvalueReferenceType(existingCopy)) {
          SgRvalueReferenceType *cached =
              copiedWrapperBase->get_rvalue_ref_to();
          exactExistingWrapper =
              existingReference->get_base_type() == copiedWrapperBase &&
              (cached == NULL || cached == existingReference);
          if (exactExistingWrapper && cached == NULL)
            copiedWrapperBase->set_rvalue_ref_to(existingReference);
        } else if (auto *modifier = isSgModifierType(source)) {
          exactExistingWrapper =
              existingCopy ==
              SageBuilder::buildModifierType(copiedWrapperBase,
                                             modifier->get_typeModifier());
        }
        if (!exactExistingWrapper) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[canonical-wrapper-existing]: "
                  "source=%p/%s base=%p copied-base=%p member-class=%p "
                  "copied-member-class=%p existing=%p/%s has no exact "
                  "target canonical constituent identity\n",
                  static_cast<void *>(source), source->class_name().c_str(),
                  static_cast<void *>(sourceWrapperBase),
                  static_cast<void *>(copiedWrapperBase),
                  static_cast<void *>(sourceMemberClass),
                  static_cast<void *>(copiedMemberClass),
                  static_cast<void *>(existingCopy),
                  existingCopy->class_name().c_str());
          ROSE_ABORT();
        }
      }

      SgType *canonicalWrapper = NULL;
      if (sourceMemberClass != NULL) {
        canonicalWrapper = SgPointerMemberType::createType(copiedWrapperBase,
                                                           copiedMemberClass);
      } else if (isSgPointerType(source) != NULL) {
        canonicalWrapper = SgPointerType::createType(copiedWrapperBase);
      } else if (isSgReferenceType(source) != NULL) {
        canonicalWrapper = SgReferenceType::createType(copiedWrapperBase);
      } else if (auto *modifier = isSgModifierType(source)) {
        canonicalWrapper = SageBuilder::buildModifierType(
            copiedWrapperBase, modifier->get_typeModifier());
      } else {
        canonicalWrapper = SgRvalueReferenceType::createType(copiedWrapperBase);
      }
      const bool constituentChanged = copiedWrapperBase != sourceWrapperBase ||
                                      copiedMemberClass != sourceMemberClass;
      if (!constituentChanged || canonicalWrapper == NULL ||
          canonicalWrapper == source ||
          canonicalWrapper->variantT() != source->variantT() ||
          (existingCopy != NULL && existingCopy != canonicalWrapper)) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[canonical-wrapper-type]: source=%p/%s "
                "base=%p copied-base=%p member-class=%p "
                "copied-member-class=%p existing=%p canonical=%p/%s\n",
                static_cast<void *>(source), source->class_name().c_str(),
                static_cast<void *>(sourceWrapperBase),
                static_cast<void *>(copiedWrapperBase),
                static_cast<void *>(sourceMemberClass),
                static_cast<void *>(copiedMemberClass),
                static_cast<void *>(existingCopy),
                static_cast<void *>(canonicalWrapper),
                canonicalWrapper != NULL
                    ? canonicalWrapper->class_name().c_str()
                    : "<null>");
        ROSE_ABORT();
      }
      if (existing == help_.get_copiedNodeMap().end()) {
        help_.insertCopiedNodePair(source, canonicalWrapper);
      }
      if (!replacements_.emplace(source, canonicalWrapper).second) {
        ROSE_ABORT();
      }
      active_.erase(source);
      return canonicalWrapper;
    }

    if (sourceFunction != NULL) {
      SgFunctionTypeTable *table = SgNode::get_globalFunctionTypeTable();
      SgMemberFunctionType *sourceMember =
          isSgMemberFunctionType(sourceFunction);
      SgType *sourceClass =
          sourceMember != NULL ? sourceMember->get_class_type() : NULL;
      const unsigned int specifier =
          sourceMember != NULL ? sourceMember->get_mfunc_specifier() : 0;
      const bool indexed =
          sourceFunction->get_semantic_signature() == nullptr &&
          !sourceFunction->get_exception_specification_is_written() &&
          !sourceFunction->get_exception_specification_source_is_self() &&
          table != nullptr &&
          table->lookup_exact_function_type(sourceFunction) == sourceFunction;
      if (indexed && discover.needsCopy) {
        SgType *copiedReturn = materialize(sourceFunction->get_return_type());
        SgType *copiedClass =
            sourceClass != NULL ? materialize(sourceClass) : NULL;
        SgFunctionParameterTypeList *sourceArguments =
            sourceFunction->get_argument_list();
        sourceArguments->validate_argument_qualification_use_sites(
            "copied-semantic-function-type");
        auto copiedArguments = std::make_unique<SgFunctionParameterTypeList>();
        for (const SgFunctionTypeArgument *argument :
             sourceArguments->get_argument_qualification_use_sites()) {
          copiedArguments->append_argument(materialize(argument->get_type()),
                                           *argument);
        }
        auto candidate = SageBuilder::copyFunctionSignature(
            sourceFunction, copiedReturn, std::move(copiedArguments),
            copiedClass, specifier, help_, nullptr);
        SgFunctionType *canonical =
            SageBuilder::internFunctionType(std::move(candidate));
        ROSE_ASSERT(canonical != NULL);
        if (existingCopy != NULL && existingCopy != canonical) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[canonical-function-provisional-copy]: "
                  "source=%p copy=%p canonical=%p has a noncanonical copy\n",
                  static_cast<void *>(source),
                  static_cast<void *>(existingCopy),
                  static_cast<void *>(canonical));
          ROSE_ABORT();
        }
        if (canonical == source ||
            canonical->variantT() != source->variantT()) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[canonical-function-type]: source=%p/%s "
                  "copy=%p/%s did not bind copied constituents\n",
                  static_cast<void *>(source), source->class_name().c_str(),
                  static_cast<void *>(canonical),
                  canonical != NULL ? canonical->class_name().c_str()
                                    : "<null>");
          ROSE_ABORT();
        }
        if (existing != help_.get_copiedNodeMap().end())
          help_.eraseCopiedNodePair(existing->first);
        help_.insertCopiedNodePair(source, canonical);
        if (!replacements_.emplace(source, canonical).second)
          ROSE_ABORT();
        active_.erase(source);
        return canonical;
      }
    }

    if (existingCopy != NULL) {
      provisionalCopySources_.erase(existingCopy);
      if (!replacements_.emplace(source, existingCopy).second) {
        ROSE_ABORT();
      }
      CanonicalizeCopiedType canonicalize(*this);
      existingCopy->processDataMemberReferenceToPointers(&canonicalize);
      bindCopiedSourceOwner(source, existingCopy);
      active_.erase(source);
      return existingCopy;
    }

    if (existing != help_.get_copiedNodeMap().end()) {
      help_.eraseCopiedNodePair(existing->first);
    }

    std::set<const SgNode *> mappingsBeforeCopy;
    for (const auto &mapping : help_.get_copiedNodeMap()) {
      mappingsBeforeCopy.insert(mapping.first);
    }
    const Rose_STL_Container<SgNode *> sourceChildren =
        source->get_traversalSuccessorContainer();

    SgType *copy;
    {
      CopyDepthScope depth(help_);
      copy = isSgType(source->copy(help_));
    }
    if (copy == NULL || copy == source ||
        copy->variantT() != source->variantT() ||
        lookupCopiedNode(help_, source) != copy) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[dependent-type-copy]: source=%p/%s "
              "copy=%p/%s mapped=%p\n",
              static_cast<void *>(source), source->class_name().c_str(),
              static_cast<void *>(copy),
              copy != NULL ? copy->class_name().c_str() : "<null>",
              static_cast<void *>(lookupCopiedNode(help_, source)));
      ROSE_ABORT();
    }

    if (!replacements_.emplace(source, copy).second) {
      ROSE_ABORT();
    }
    for (const auto &mapping : help_.get_copiedNodeMap()) {
      if (mapping.first == source ||
          mappingsBeforeCopy.find(mapping.first) != mappingsBeforeCopy.end() ||
          mapping.first == mapping.second) {
        continue;
      }
      SgType *copiedDependency = isSgType(mapping.second);
      const SgType *sourceDependency = isSgType(mapping.first);
      if (sourceDependency != NULL && copiedDependency != NULL) {
        provisionalCopySources_.emplace(copiedDependency,
                                        const_cast<SgType *>(sourceDependency));
      }
    }

    CanonicalizeCopiedType canonicalize(*this);
    copy->processDataMemberReferenceToPointers(&canonicalize);
    bindCopiedSourceOwner(source, copy);

    const Rose_STL_Container<SgNode *> copiedChildren =
        copy->get_traversalSuccessorContainer();
    if (sourceChildren.size() != copiedChildren.size()) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[dependent-type-child-count]: source=%p/%s "
              "children=%zu copy=%p children=%zu\n",
              static_cast<void *>(source), source->class_name().c_str(),
              sourceChildren.size(), static_cast<void *>(copy),
              copiedChildren.size());
      ROSE_ABORT();
    }
    for (size_t index = 0; index < sourceChildren.size(); ++index) {
      SgNode *sourceChild = sourceChildren[index];
      SgNode *copiedChild = copiedChildren[index];
      if (sourceChild == NULL || isSgType(sourceChild) != NULL ||
          mappingsBeforeCopy.find(sourceChild) != mappingsBeforeCopy.end()) {
        continue;
      }
      if (copiedChild == NULL || copiedChild == sourceChild ||
          lookupCopiedNode(help_, sourceChild) != copiedChild ||
          copiedChild->get_parent() != copy) {
        fprintf(
            stderr,
            "REX_COPY_INVARIANT[dependent-type-child-copy]: "
            "source-type=%p/%s index=%zu child=%p/%s copy-type=%p "
            "child=%p/%s parent=%p mapped=%p\n",
            static_cast<void *>(source), source->class_name().c_str(), index,
            static_cast<void *>(sourceChild), sourceChild->class_name().c_str(),
            static_cast<void *>(copy), static_cast<void *>(copiedChild),
            copiedChild != NULL ? copiedChild->class_name().c_str() : "<null>",
            static_cast<void *>(copiedChild != NULL ? copiedChild->get_parent()
                                                    : NULL),
            static_cast<void *>(lookupCopiedNode(help_, sourceChild)));
        ROSE_ABORT();
      }
    }

    active_.erase(source);
    return copy;
  }
};

class ExactCopiedTypeEdgeRewriter : public SimpleReferenceToPointerHandler {
public:
  explicit ExactCopiedTypeEdgeRewriter(
      ExactCopiedTypeGraphMaterializer &materializer)
      : materializer_(materializer) {}

  void operator()(SgNode *&node, const SgName &, bool) override {
    node = materializer_.materializeCopiedEdge(node);
  }

private:
  ExactCopiedTypeGraphMaterializer &materializer_;
};

class StaleExactCopiedTypeEdgeDetector
    : public SimpleReferenceToPointerHandler {
public:
  explicit StaleExactCopiedTypeEdgeDetector(
      const std::unordered_map<const SgNode *, SgNode *> &replacements)
      : replacements_(replacements) {}

  void operator()(SgNode *&node, const SgName &, bool) override {
    if (staleSource_ == NULL &&
        replacements_.find(node) != replacements_.end()) {
      staleSource_ = node;
    }
  }

  const SgNode *staleSource() const { return staleSource_; }

private:
  const std::unordered_map<const SgNode *, SgNode *> &replacements_;
  const SgNode *staleSource_ = NULL;
};

bool hasExactTranslationUnitStructuralAncestry(const SgNode *node) {
  std::unordered_set<const SgNode *> visited;
  for (const SgNode *current = node; current != NULL;
       current = current->get_parent()) {
    if (!visited.insert(current).second) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[named-type-publication-ancestry]: "
              "node=%p/%s has a structural parent cycle at %p/%s\n",
              static_cast<const void *>(node),
              node != NULL ? node->class_name().c_str() : "<null>",
              static_cast<const void *>(current),
              current->class_name().c_str());
      ROSE_ABORT();
    }
    if (isSgGlobal(current) != NULL) {
      return true;
    }
  }
  return false;
}

bool copiedClassDeclarationsHaveExactSharedProjectIdentity(
    SgCopyHelp &help, const SgClassDeclaration *source,
    const SgClassDeclaration *destination) {
  if (const auto *instance = isSgTemplateInstantiationDecl(source)) {
    const auto *copiedInstance = isSgTemplateInstantiationDecl(destination);
    if (copiedInstance == nullptr ||
        !SageInterface::templateArgumentListEquivalence(
            instance->get_semanticTemplateArguments(),
            copiedInstance->get_semanticTemplateArguments()))
      return false;
  }
  const SgClassType *sourceType =
      source != NULL ? isSgClassType(source->get_type()) : NULL;
  return source != NULL && destination != NULL && source != destination &&
         lookupCopiedNode(help, source) == destination &&
         SageInterface::getProject(source) != nullptr &&
         SageInterface::getProject(source) ==
             SageInterface::getProject(destination) &&
         isSgGlobal(source->get_scope()) != NULL &&
         source->get_scope() == destination->get_scope() &&
         !source->get_isUnNamed() && !destination->get_isUnNamed() &&
         source->get_class_type() == destination->get_class_type() &&
         sourceType != NULL && sourceType->get_declaration() == source &&
         destination->get_type() == sourceType &&
         hasExactTranslationUnitStructuralAncestry(source) &&
         hasExactTranslationUnitStructuralAncestry(destination);
}

bool copiedEnumDeclarationsHaveExactSharedProjectIdentity(
    SgCopyHelp &help, const SgEnumDeclaration *source,
    const SgEnumDeclaration *destination) {
  const SgEnumType *sourceType =
      source != NULL ? isSgEnumType(source->get_type()) : NULL;
  return source != NULL && destination != NULL && source != destination &&
         lookupCopiedNode(help, source) == destination &&
         SageInterface::getProject(source) != nullptr &&
         SageInterface::getProject(source) ==
             SageInterface::getProject(destination) &&
         isSgGlobal(source->get_scope()) != NULL &&
         isSgGlobal(destination->get_scope()) != NULL &&
         !source->get_isUnNamed() && !destination->get_isUnNamed() &&
         sourceType != NULL && sourceType->get_declaration() == source &&
         destination->get_type() == sourceType &&
         hasExactTranslationUnitStructuralAncestry(source) &&
         hasExactTranslationUnitStructuralAncestry(destination);
}

void constructCopiedClassType(const SgClassDeclaration *originalFirst,
                              SgClassDeclaration *copyFirst, SgCopyHelp &help) {
  const SgClassDeclaration *originalDefining =
      isSgClassDeclaration(originalFirst->get_definingDeclaration());
  SgClassDeclaration *mappedDefining =
      originalDefining != NULL
          ? isSgClassDeclaration(lookupCopiedNode(help, originalDefining))
          : NULL;

  // A detached copy of only the canonical nondefining surface remains a
  // declaration surface for the external semantic family.  It must retain
  // that family's shared type; creating a new type would split a declaration
  // chain whose defining member is outside this structural transaction.
  if (originalDefining != NULL && mappedDefining == NULL) {
    SgClassType *externalType = isSgClassType(originalFirst->get_type());
    if (externalType == NULL ||
        externalType->get_declaration() != originalFirst ||
        copyFirst->get_firstNondefiningDeclaration() != copyFirst ||
        copyFirst->get_definingDeclaration() != originalDefining ||
        copyFirst->get_type() != externalType) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[partial-class-family]: source-first=%p "
              "source-defining=%p source-type=%p copy-first=%p first=%p "
              "defining=%p type=%p\n",
              static_cast<const void *>(originalFirst),
              static_cast<const void *>(originalDefining),
              static_cast<void *>(externalType), static_cast<void *>(copyFirst),
              static_cast<void *>(copyFirst->get_firstNondefiningDeclaration()),
              static_cast<void *>(copyFirst->get_definingDeclaration()),
              static_cast<void *>(copyFirst->get_type()));
      ROSE_ABORT();
    }
    if (help.get_copiedNodeMap().count(externalType) == 0)
      help.insertCopiedNodePair(externalType, externalType);
    return;
  }
  SgClassDeclaration *copyDefining =
      originalDefining != NULL ? mappedDefining : NULL;
  SgClassType *originalType = isSgClassType(originalFirst->get_type());
  auto existingTypeMapping = originalType != NULL
                                 ? help.get_copiedNodeMap().find(originalType)
                                 : help.get_copiedNodeMap().end();
  const bool exactSharedProjectIdentity =
      copiedClassDeclarationsHaveExactSharedProjectIdentity(help, originalFirst,
                                                            copyFirst);
  if (originalType != NULL &&
      existingTypeMapping == help.get_copiedNodeMap().end() &&
      exactSharedProjectIdentity) {
    // A copied physical declaration of the same project entity republishes
    // the canonical project-wide type. Materializing a second SgClassType
    // splits one semantic class identity merely because it has another
    // output-file surface.
    help.insertCopiedNodePair(originalType, originalType);
    existingTypeMapping = help.get_copiedNodeMap().find(originalType);
  }
  SgClassType *existingCopyType =
      existingTypeMapping != help.get_copiedNodeMap().end()
          ? isSgClassType(existingTypeMapping->second)
          : NULL;
  const bool alreadySharedProjectType = existingCopyType == originalType;
  const bool alreadyMaterialized =
      existingCopyType != NULL && existingCopyType != originalType;
  if (originalType == NULL ||
      originalType->get_declaration() != originalFirst ||
      copyFirst->get_firstNondefiningDeclaration() != copyFirst ||
      copyFirst->get_definingDeclaration() != copyDefining ||
      copyFirst->get_type() !=
          (alreadyMaterialized ? existingCopyType : originalType) ||
      (existingTypeMapping != help.get_copiedNodeMap().end() &&
       !alreadyMaterialized && !alreadySharedProjectType) ||
      (alreadySharedProjectType && !exactSharedProjectIdentity) ||
      (alreadyMaterialized &&
       existingCopyType->get_declaration() != copyFirst) ||
      (originalDefining != NULL &&
       (originalDefining->get_firstNondefiningDeclaration() != originalFirst ||
        originalDefining->get_definingDeclaration() != originalDefining ||
        originalDefining->get_type() != originalType || copyDefining == NULL ||
        copyDefining->get_firstNondefiningDeclaration() != copyFirst ||
        copyDefining->get_definingDeclaration() != copyDefining ||
        copyDefining->get_type() !=
            (alreadyMaterialized ? existingCopyType : originalType)))) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[copied-class-type-family]: source-first=%p "
            "source-defining=%p source-type=%p source-type-owner=%p "
            "copy-first=%p copy-first-link=%p copy-defining=%p "
            "copy-type=%p\n",
            static_cast<const void *>(originalFirst),
            static_cast<const void *>(originalDefining),
            static_cast<void *>(originalType),
            static_cast<void *>(
                originalType != NULL ? originalType->get_declaration() : NULL),
            static_cast<void *>(copyFirst),
            static_cast<void *>(copyFirst->get_firstNondefiningDeclaration()),
            static_cast<void *>(copyDefining),
            static_cast<void *>(copyFirst->get_type()));
    ROSE_ABORT();
  }

  std::vector<SgClassDeclaration *> copiedFamilyMembers;
  for (SgCopyHelp::copiedNodeMapTypeIterator member =
           help.get_copiedNodeMap().begin();
       member != help.get_copiedNodeMap().end(); ++member) {
    const SgClassDeclaration *originalMember =
        isSgClassDeclaration(member->first);
    SgClassDeclaration *copyMember = isSgClassDeclaration(member->second);
    if (originalMember == NULL || copyMember == NULL ||
        originalMember == copyMember ||
        originalMember->get_firstNondefiningDeclaration() != originalFirst) {
      continue;
    }
    if (originalMember->get_type() != originalType ||
        copyMember->get_type() !=
            (alreadyMaterialized ? existingCopyType : originalType)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[copied-class-type-member]: "
              "source-member=%p type=%p copy-member=%p type=%p "
              "expected=%p\n",
              static_cast<const void *>(originalMember),
              static_cast<void *>(originalMember->get_type()),
              static_cast<void *>(copyMember),
              static_cast<void *>(copyMember->get_type()),
              static_cast<void *>(originalType));
      ROSE_ABORT();
    }
    copiedFamilyMembers.push_back(copyMember);
  }
  if (copiedFamilyMembers.empty()) {
    ROSE_ABORT();
  }

  SgClassType *copyType = existingCopyType;
  if (!alreadyMaterialized && !alreadySharedProjectType) {
    for (SgClassDeclaration *member : copiedFamilyMembers) {
      member->set_type(NULL);
    }
    copyType = hasExactTranslationUnitStructuralAncestry(copyFirst)
                   ? SgClassType::createType(copyFirst, NULL)
                   : new SgClassType(copyFirst);
    for (SgClassDeclaration *member : copiedFamilyMembers) {
      member->set_type(copyType);
    }
    help.insertCopiedNodePair(originalType, copyType);
  }
  if (copyType == originalType) {
    if (!copiedClassDeclarationsHaveExactSharedProjectIdentity(
            help, originalFirst, copyFirst) ||
        copyType->get_declaration() != originalFirst ||
        copyFirst->get_type() != copyType ||
        (copyDefining != NULL && copyDefining->get_type() != copyType) ||
        lookupCopiedNode(help, originalType) != originalType) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[shared-project-class-type]: "
              "source=%p copy=%p type=%p owner=%p first-type=%p "
              "defining-type=%p mapped=%p\n",
              static_cast<const void *>(originalFirst),
              static_cast<void *>(copyFirst), static_cast<void *>(copyType),
              static_cast<void *>(copyType->get_declaration()),
              static_cast<void *>(copyFirst->get_type()),
              static_cast<void *>(
                  copyDefining != NULL ? copyDefining->get_type() : NULL),
              static_cast<void *>(lookupCopiedNode(help, originalType)));
      ROSE_ABORT();
    }
    return;
  }
  if (copyType == NULL || copyType->get_declaration() != copyFirst ||
      copyFirst->get_type() != copyType ||
      (copyDefining != NULL && copyDefining->get_type() != copyType)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[copied-class-type-owner]: source-type=%p "
            "copy-type=%p owner=%p expected-owner=%p first-type=%p "
            "defining-type=%p\n",
            static_cast<void *>(originalType), static_cast<void *>(copyType),
            static_cast<void *>(copyType != NULL ? copyType->get_declaration()
                                                 : NULL),
            static_cast<void *>(copyFirst),
            static_cast<void *>(copyFirst->get_type()),
            static_cast<void *>(copyDefining != NULL ? copyDefining->get_type()
                                                     : NULL));
    ROSE_ABORT();
  }

  if (lookupCopiedNode(help, originalType) != copyType) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[copied-class-type-publication]: "
            "source-type=%p copy-type=%p mapped=%p\n",
            static_cast<void *>(originalType), static_cast<void *>(copyType),
            static_cast<void *>(lookupCopiedNode(help, originalType)));
    ROSE_ABORT();
  }
}

void constructCopiedEnumType(const SgEnumDeclaration *originalFirst,
                             SgEnumDeclaration *copyFirst, SgCopyHelp &help) {
  const SgEnumDeclaration *originalDefining =
      isSgEnumDeclaration(originalFirst->get_definingDeclaration());
  SgEnumDeclaration *copyDefining =
      originalDefining != NULL
          ? isSgEnumDeclaration(requireExactMappedNode(
                originalDefining, help, "copied-enum-type-defining-owner"))
          : NULL;
  SgEnumType *originalType = isSgEnumType(originalFirst->get_type());
  const auto existingTypeMapping =
      originalType != NULL ? help.get_copiedNodeMap().find(originalType)
                           : help.get_copiedNodeMap().end();
  SgEnumType *existingCopyType =
      existingTypeMapping != help.get_copiedNodeMap().end()
          ? isSgEnumType(existingTypeMapping->second)
          : NULL;
  const bool alreadySharedProjectType = existingCopyType == originalType;
  const bool alreadyMaterialized =
      existingCopyType != NULL && existingCopyType != originalType;
  if (originalType == NULL ||
      originalType->get_declaration() != originalFirst ||
      originalDefining == NULL ||
      originalDefining->get_firstNondefiningDeclaration() != originalFirst ||
      originalDefining->get_definingDeclaration() != originalDefining ||
      originalDefining->get_type() != originalType || copyDefining == NULL ||
      copyFirst->get_firstNondefiningDeclaration() != copyFirst ||
      copyFirst->get_definingDeclaration() != copyDefining ||
      copyFirst->get_type() !=
          (alreadyMaterialized ? existingCopyType : originalType) ||
      (existingTypeMapping != help.get_copiedNodeMap().end() &&
       !alreadyMaterialized && !alreadySharedProjectType) ||
      (alreadyMaterialized &&
       existingCopyType->get_declaration() != copyFirst) ||
      copyDefining->get_firstNondefiningDeclaration() != copyFirst ||
      copyDefining->get_definingDeclaration() != copyDefining ||
      copyDefining->get_type() !=
          (alreadyMaterialized ? existingCopyType : originalType)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[copied-enum-type-family]: source-first=%p "
            "source-defining=%p source-type=%p source-type-owner=%p "
            "copy-first=%p copy-first-link=%p copy-defining=%p "
            "copy-type=%p\n",
            static_cast<const void *>(originalFirst),
            static_cast<const void *>(originalDefining),
            static_cast<void *>(originalType),
            static_cast<void *>(
                originalType != NULL ? originalType->get_declaration() : NULL),
            static_cast<void *>(copyFirst),
            static_cast<void *>(copyFirst->get_firstNondefiningDeclaration()),
            static_cast<void *>(copyDefining),
            static_cast<void *>(copyFirst->get_type()));
    ROSE_ABORT();
  }

  std::vector<SgEnumDeclaration *> copiedFamilyMembers;
  for (SgCopyHelp::copiedNodeMapTypeIterator member =
           help.get_copiedNodeMap().begin();
       member != help.get_copiedNodeMap().end(); ++member) {
    const SgEnumDeclaration *originalMember =
        isSgEnumDeclaration(member->first);
    SgEnumDeclaration *copyMember = isSgEnumDeclaration(member->second);
    if (originalMember == NULL || copyMember == NULL ||
        originalMember == copyMember ||
        originalMember->get_firstNondefiningDeclaration() != originalFirst) {
      continue;
    }
    if (originalMember->get_type() != originalType ||
        copyMember->get_type() !=
            (alreadyMaterialized ? existingCopyType : originalType)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[copied-enum-type-member]: "
              "source-member=%p type=%p copy-member=%p type=%p "
              "expected=%p\n",
              static_cast<const void *>(originalMember),
              static_cast<void *>(originalMember->get_type()),
              static_cast<void *>(copyMember),
              static_cast<void *>(copyMember->get_type()),
              static_cast<void *>(originalType));
      ROSE_ABORT();
    }
    copiedFamilyMembers.push_back(copyMember);
  }
  if (copiedFamilyMembers.empty()) {
    ROSE_ABORT();
  }

  SgEnumType *copyType = existingCopyType;
  if (!alreadyMaterialized && !alreadySharedProjectType) {
    for (SgEnumDeclaration *member : copiedFamilyMembers) {
      member->set_type(NULL);
    }
    copyType = hasExactTranslationUnitStructuralAncestry(copyFirst)
                   ? SgEnumType::createType(copyFirst, NULL)
                   : new SgEnumType(copyFirst);
    for (SgEnumDeclaration *member : copiedFamilyMembers) {
      member->set_type(copyType);
    }
    help.insertCopiedNodePair(originalType, copyType);
  }
  if (copyType == originalType) {
    if (!copiedEnumDeclarationsHaveExactSharedProjectIdentity(
            help, originalFirst, copyFirst) ||
        copyType->get_declaration() != originalFirst ||
        copyFirst->get_type() != copyType ||
        copyDefining->get_type() != copyType ||
        lookupCopiedNode(help, originalType) != originalType) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[shared-project-enum-type]: "
              "source=%p copy=%p type=%p owner=%p first-type=%p "
              "defining-type=%p mapped=%p\n",
              static_cast<const void *>(originalFirst),
              static_cast<void *>(copyFirst), static_cast<void *>(copyType),
              static_cast<void *>(copyType->get_declaration()),
              static_cast<void *>(copyFirst->get_type()),
              static_cast<void *>(copyDefining->get_type()),
              static_cast<void *>(lookupCopiedNode(help, originalType)));
      ROSE_ABORT();
    }
    return;
  }
  if (copyType == NULL || copyType->get_declaration() != copyFirst ||
      copyFirst->get_type() != copyType ||
      copyDefining->get_type() != copyType) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[copied-enum-type-owner]: source-type=%p "
            "copy-type=%p owner=%p expected-owner=%p first-type=%p "
            "defining-type=%p\n",
            static_cast<void *>(originalType), static_cast<void *>(copyType),
            static_cast<void *>(copyType != NULL ? copyType->get_declaration()
                                                 : NULL),
            static_cast<void *>(copyFirst),
            static_cast<void *>(copyFirst->get_type()),
            static_cast<void *>(copyDefining->get_type()));
    ROSE_ABORT();
  }

  if (lookupCopiedNode(help, originalType) != copyType) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[copied-enum-type-publication]: "
            "source-type=%p copy-type=%p mapped=%p\n",
            static_cast<void *>(originalType), static_cast<void *>(copyType),
            static_cast<void *>(lookupCopiedNode(help, originalType)));
    ROSE_ABORT();
  }
}

void materializeExactCopiedNamedTypes(SgCopyHelp &help) {
  std::unordered_map<const SgNode *, SgNode *> replacements;
  for (const auto &mapping : help.get_copiedNodeMap()) {
    if ((isSgClassType(mapping.first) != nullptr ||
         isSgEnumType(mapping.first) != nullptr) &&
        mapping.first != mapping.second) {
      ROSE_ASSERT(mapping.first->variantT() == mapping.second->variantT());
      ROSE_ASSERT(replacements.emplace(mapping.first, mapping.second).second);
    }
    const SgNonrealDecl *original = isSgNonrealDecl(mapping.first);
    SgNonrealDecl *copy = isSgNonrealDecl(mapping.second);
    if (original == NULL || copy == NULL || original == copy) {
      continue;
    }
    SgNonrealType *originalType = original->get_type();
    SgNonrealType *copiedType = copy->get_type();
    ROSE_ASSERT(originalType != nullptr && copiedType != nullptr &&
                copiedType != originalType &&
                originalType->get_declaration() == original &&
                originalType->get_parent() == original &&
                copiedType->get_declaration() == copy &&
                copiedType->get_parent() == copy &&
                lookupCopiedNode(help, originalType) == copiedType);
    ROSE_ASSERT(replacements.emplace(originalType, copiedType).second);
  }

  // Typedef constructors publish their declaration-owned type before copying
  // children. Seed that identity even when this transaction has no class,
  // enum or nonreal types, so every copied semantic use follows its owner.
  for (const auto &mapping : help.get_copiedNodeMap()) {
    const auto *originalType = isSgTypedefType(mapping.first);
    auto *copiedType = isSgTypedefType(mapping.second);
    if (originalType == nullptr || copiedType == nullptr ||
        originalType == copiedType)
      continue;
    const auto *original =
        isSgTypedefDeclaration(originalType->get_declaration());
    auto *copy = isSgTypedefDeclaration(copiedType->get_declaration());
    if (original == nullptr || copy == nullptr || copy == original ||
        original->get_type() != originalType ||
        copy->get_type() != copiedType ||
        lookupCopiedNode(help, original) != copy ||
        !replacements.emplace(originalType, copiedType).second) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[typedef-type-publication]: "
              "copied typedef and its type have conflicting identities\n");
      ROSE_ABORT();
    }
  }

  for (const auto &mapping : help.get_copiedNodeMap()) {
    const SgTemplateType *originalType = isSgTemplateType(mapping.first);
    SgTemplateType *copiedType = isSgTemplateType(mapping.second);
    if (originalType == NULL || copiedType == NULL ||
        originalType == copiedType) {
      continue;
    }
    const SgTemplateParameter *originalParameter =
        originalType->get_template_parameter();
    if (originalParameter == NULL ||
        originalParameter->get_type() != originalType) {
      continue;
    }
    SgTemplateParameter *copiedParameter =
        isSgTemplateParameter(lookupCopiedNode(help, originalParameter));
    if (copiedParameter == NULL || copiedParameter == originalParameter ||
        copiedParameter->get_type() != copiedType ||
        copiedType->get_template_parameter() != copiedParameter) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[copied-template-parameter-type-seed]: "
              "source-parameter=%p type=%p copy-parameter=%p type=%p "
              "type-owner=%p mapped-parameter=%p\n",
              static_cast<const void *>(originalParameter),
              static_cast<const void *>(originalType),
              static_cast<void *>(copiedParameter),
              static_cast<void *>(copiedType),
              static_cast<void *>(copiedType->get_template_parameter()),
              static_cast<void *>(lookupCopiedNode(help, originalParameter)));
      ROSE_ABORT();
    }
    const auto inserted = replacements.emplace(originalType, copiedType);
    if (!inserted.second && inserted.first->second != copiedType) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[copied-template-parameter-type-seed-map]: "
              "source-type=%p first-target=%p second-target=%p\n",
              static_cast<const void *>(originalType),
              static_cast<void *>(inserted.first->second),
              static_cast<void *>(copiedType));
      ROSE_ABORT();
    }
  }

  // Lexical type operands require their own copy even when the transaction
  // contains no named declaration family to seed the replacement map.
  ExactCopiedTypeGraphMaterializer typeMaterializer(help, replacements);
  ExactCopiedTypeEdgeRewriter rewriter(typeMaterializer);

  // Parameter datatypes and defaults bind before their dependent children.
  for (const auto &mapping : help.get_copiedNodeMap()) {
    const SgTemplateParameter *originalParameter =
        isSgTemplateParameter(mapping.first);
    SgTemplateParameter *copiedParameter =
        isSgTemplateParameter(mapping.second);
    if (originalParameter == NULL || copiedParameter == NULL ||
        originalParameter == copiedParameter) {
      continue;
    }

    SgType *originalParameterType = originalParameter->get_type();
    SgType *provisionalParameterType = copiedParameter->get_type();
    if (originalParameterType == NULL || provisionalParameterType == NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[copied-template-parameter-type-null]: "
              "source=%p type=%p copy=%p type=%p\n",
              static_cast<const void *>(originalParameter),
              static_cast<void *>(originalParameterType),
              static_cast<void *>(copiedParameter),
              static_cast<void *>(provisionalParameterType));
      ROSE_ABORT();
    }
    SgType *finalParameterType =
        typeMaterializer.materialize(originalParameterType);
    if (provisionalParameterType != finalParameterType) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[copied-template-parameter-type-final]: "
              "source=%p type=%p/%s copy=%p provisional=%p/%s final=%p/%s\n",
              static_cast<const void *>(originalParameter),
              static_cast<void *>(originalParameterType),
              originalParameterType->class_name().c_str(),
              static_cast<void *>(copiedParameter),
              static_cast<void *>(provisionalParameterType),
              provisionalParameterType->class_name().c_str(),
              static_cast<void *>(finalParameterType),
              finalParameterType->class_name().c_str());
      ROSE_ABORT();
    }

    SgType *originalDefaultType = originalParameter->get_defaultTypeParameter();
    SgType *provisionalDefaultType =
        copiedParameter->get_defaultTypeParameter();
    if ((originalDefaultType == NULL) != (provisionalDefaultType == NULL)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[copied-template-parameter-default-shape]: "
              "source=%p default=%p copy=%p default=%p\n",
              static_cast<const void *>(originalParameter),
              static_cast<void *>(originalDefaultType),
              static_cast<void *>(copiedParameter),
              static_cast<void *>(provisionalDefaultType));
      ROSE_ABORT();
    }
    if (originalDefaultType != NULL) {
      SgType *finalDefaultType =
          typeMaterializer.materialize(originalDefaultType);
      if (provisionalDefaultType != finalDefaultType) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[copied-template-parameter-default-final]: "
                "source=%p default=%p/%s copy=%p provisional=%p/%s "
                "final=%p/%s\n",
                static_cast<const void *>(originalParameter),
                static_cast<void *>(originalDefaultType),
                originalDefaultType->class_name().c_str(),
                static_cast<void *>(copiedParameter),
                static_cast<void *>(provisionalDefaultType),
                provisionalDefaultType->class_name().c_str(),
                static_cast<void *>(finalDefaultType),
                finalDefaultType->class_name().c_str());
        ROSE_ABORT();
      }
    }
  }

  size_t previousReplacementCount = 0;
  do {
    previousReplacementCount = replacements.size();
    for (SgCopyHelp::copiedNodeMapTypeIterator it =
             help.get_copiedNodeMap().begin();
         it != help.get_copiedNodeMap().end(); ++it) {
      if (it->first != it->second && isSgType(it->second) == NULL) {
        it->second->processDataMemberReferenceToPointers(&rewriter);
      }
    }
  } while (replacements.size() != previousReplacementCount);

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgVariableDeclaration *sourceVariable =
        isSgVariableDeclaration(it->first);
    SgVariableDeclaration *copiedVariable = isSgVariableDeclaration(it->second);
    if (sourceVariable == NULL || copiedVariable == NULL ||
        sourceVariable == copiedVariable) {
      continue;
    }
    SgDeclarationStatement *sourceTag =
        sourceVariable->get_baseTypeDefiningDeclaration();
    if (sourceTag == NULL) {
      sourceTag = sourceVariable->get_baseTypeNondefiningDeclaration();
    }
    SgDeclarationStatement *copiedTag =
        copiedVariable->get_baseTypeDefiningDeclaration();
    if (copiedTag == NULL) {
      copiedTag = copiedVariable->get_baseTypeNondefiningDeclaration();
    }
    if (sourceTag == NULL || copiedTag == NULL) {
      continue;
    }
    const SgInitializedNamePtrList &sourceNames =
        sourceVariable->get_variables();
    const SgInitializedNamePtrList &copiedNames =
        copiedVariable->get_variables();
    if (sourceNames.size() != copiedNames.size()) {
      ROSE_ABORT();
    }
    for (size_t index = 0; index < sourceNames.size(); ++index) {
      SgInitializedName *sourceName = sourceNames[index];
      SgInitializedName *copiedName = copiedNames[index];
      if (sourceName == NULL || copiedName == NULL ||
          SageInterface::isExactTagTypeIdentity(copiedName->get_type(),
                                                copiedTag)) {
        continue;
      }
      SgNamedType *sourceBase =
          isSgNamedType(sourceName->get_type() != NULL
                            ? sourceName->get_type()->findBaseType()
                            : NULL);
      SgNamedType *copiedBase =
          isSgNamedType(copiedName->get_type() != NULL
                            ? copiedName->get_type()->findBaseType()
                            : NULL);
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[copied-variable-owned-tag-type]: "
          "source-variable=%p tag=%p first=%p type=%p base=%p owner=%p "
          "mapped-tag=%p mapped-type=%p copy-variable=%p tag=%p first=%p "
          "type=%p base=%p owner=%p\n",
          static_cast<const void *>(sourceVariable),
          static_cast<void *>(sourceTag),
          static_cast<void *>(sourceTag->get_firstNondefiningDeclaration()),
          static_cast<void *>(sourceName->get_type()),
          static_cast<void *>(sourceBase),
          static_cast<void *>(sourceBase != NULL ? sourceBase->get_declaration()
                                                 : NULL),
          static_cast<void *>(lookupCopiedNode(help, sourceTag)),
          static_cast<void *>(lookupCopiedNode(help, sourceName->get_type())),
          static_cast<void *>(copiedVariable), static_cast<void *>(copiedTag),
          static_cast<void *>(copiedTag->get_firstNondefiningDeclaration()),
          static_cast<void *>(copiedName->get_type()),
          static_cast<void *>(copiedBase),
          static_cast<void *>(copiedBase != NULL ? copiedBase->get_declaration()
                                                 : NULL));
      ROSE_ABORT();
    }
  }

  // A non-type template argument owns its value expression, but its declared
  // parameter type is a separate semantic edge.  Finalize both edges in one
  // transaction after the dependent type graph has reached closure.  A
  // one-pass graph rewrite is order-dependent when the argument edge is what
  // first causes a source type to be materialized: the already-visited copied
  // literal would otherwise retain the source type.
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgTemplateArgument *originalArgument =
        isSgTemplateArgument(it->first);
    SgTemplateArgument *copiedArgument = isSgTemplateArgument(it->second);
    if (originalArgument == NULL || copiedArgument == NULL ||
        originalArgument == copiedArgument ||
        originalArgument->get_argumentType() !=
            SgTemplateArgument::nontype_argument ||
        originalArgument->get_expression() == NULL) {
      continue;
    }

    SgExpression *copiedExpression = copiedArgument->get_expression();
    if (copiedExpression == NULL ||
        requireExactMappedNode(originalArgument->get_expression(), help,
                               "template-argument-expression") !=
            copiedExpression) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[template-argument-expression]: "
              "source=%p expression=%p copy=%p expression=%p mapped=%p\n",
              static_cast<const void *>(originalArgument),
              static_cast<void *>(originalArgument->get_expression()),
              static_cast<void *>(copiedArgument),
              static_cast<void *>(copiedExpression),
              static_cast<void *>(
                  lookupCopiedNode(help, originalArgument->get_expression())));
      ROSE_ABORT();
    }

    copiedExpression->processDataMemberReferenceToPointers(&rewriter);
    SgType *copiedArgumentType = copiedArgument->get_type();
    SgType *copiedPayloadType = copiedExpression->get_type();
    // A reference parameter binds an expression of its referent type, and a
    // pointer parameter may own a nullptr expression. Preserve each original
    // semantic edge independently; the two roles need not have equal types.
    SgType *sourceArgumentType = originalArgument->get_type();
    SgType *sourcePayloadType = originalArgument->get_expression()->get_type();
    ASSERT_not_null(sourceArgumentType);
    ASSERT_not_null(sourcePayloadType);
    SgType *expectedArgumentType =
        typeMaterializer.materialize(sourceArgumentType);
    SgType *expectedPayloadType =
        typeMaterializer.materialize(sourcePayloadType);
    if (copiedArgumentType != expectedArgumentType ||
        copiedPayloadType != expectedPayloadType) {
      SgType *originalArgumentType = originalArgument->get_type();
      SgType *originalPayloadType =
          originalArgument->get_expression()->get_type();
      SgEnumType *originalArgumentEnum = isSgEnumType(originalArgumentType);
      SgEnumType *originalPayloadEnum = isSgEnumType(originalPayloadType);
      SgEnumType *copiedArgumentEnum = isSgEnumType(copiedArgumentType);
      SgEnumType *copiedPayloadEnum = isSgEnumType(copiedPayloadType);
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[template-argument-payload-type]: "
          "source=%p source-declared=%p/%s source-declaration=%p "
          "source-payload=%p/%s source-payload-declaration=%p "
          "mapped-declared=%p mapped-payload=%p copy=%p declared=%p/%s "
          "declaration=%p expression=%p/%s payload=%p/%s "
          "payload-declaration=%p\n",
          static_cast<const void *>(originalArgument),
          static_cast<void *>(originalArgumentType),
          originalArgumentType != NULL
              ? originalArgumentType->class_name().c_str()
              : "<null>",
          static_cast<void *>(originalArgumentEnum != NULL
                                  ? originalArgumentEnum->get_declaration()
                                  : NULL),
          static_cast<void *>(originalPayloadType),
          originalPayloadType != NULL
              ? originalPayloadType->class_name().c_str()
              : "<null>",
          static_cast<void *>(originalPayloadEnum != NULL
                                  ? originalPayloadEnum->get_declaration()
                                  : NULL),
          static_cast<void *>(lookupCopiedNode(help, originalArgumentType)),
          static_cast<void *>(lookupCopiedNode(help, originalPayloadType)),
          static_cast<void *>(copiedArgument),
          static_cast<void *>(copiedArgumentType),
          copiedArgumentType != NULL ? copiedArgumentType->class_name().c_str()
                                     : "<null>",
          static_cast<void *>(copiedArgumentEnum != NULL
                                  ? copiedArgumentEnum->get_declaration()
                                  : NULL),
          static_cast<void *>(copiedExpression),
          copiedExpression->class_name().c_str(),
          static_cast<void *>(copiedPayloadType),
          copiedPayloadType != NULL ? copiedPayloadType->class_name().c_str()
                                    : "<null>",
          static_cast<void *>(copiedPayloadEnum != NULL
                                  ? copiedPayloadEnum->get_declaration()
                                  : NULL));
      ROSE_ABORT();
    }
  }

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (it->first == it->second || isSgType(it->second) != NULL) {
      continue;
    }
    StaleExactCopiedTypeEdgeDetector detector(replacements);
    it->second->processDataMemberReferenceToPointers(&detector);
    if (detector.staleSource() != NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[stale-copied-named-type-edge]: "
              "copy-node=%p/%s source-type=%p replacement=%p\n",
              static_cast<void *>(it->second), it->second->class_name().c_str(),
              static_cast<const void *>(detector.staleSource()),
              static_cast<void *>(replacements.at(detector.staleSource())));
      ROSE_ABORT();
    }
  }
}

void validateExactCopiedEnumEnumerators(const SgEnumDeclaration *originalEnum,
                                        SgEnumDeclaration *copyEnum,
                                        SgCopyHelp &help) {
  ASSERT_not_null(originalEnum);
  ASSERT_not_null(copyEnum);

  const SgInitializedNamePtrList &originalFields =
      originalEnum->get_enumerators();
  const SgInitializedNamePtrList &copyFields = copyEnum->get_enumerators();
  if (copyFields.size() != originalFields.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[enum-field-count]: original=%p fields=%zu "
            "copy=%p fields=%zu\n",
            static_cast<const void *>(originalEnum), originalFields.size(),
            static_cast<void *>(copyEnum), copyFields.size());
    ROSE_ABORT();
  }

  for (size_t index = 0; index < originalFields.size(); ++index) {
    SgInitializedName *originalField = originalFields[index];
    SgInitializedName *copyField = copyFields[index];
    if (originalField == NULL || copyField == NULL ||
        requireExactMappedNode(originalField, help, "enum-field") !=
            copyField ||
        originalField->get_parent() != originalEnum ||
        countDirectOwnerEdges(originalEnum, originalField) != 1 ||
        copyField->get_parent() != copyEnum ||
        countDirectOwnerEdges(copyEnum, copyField) != 1) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[enum-field-order-owner]: index=%zu "
              "original-enum=%p field=%p owner=%p copy-enum=%p field=%p "
              "owner=%p\n",
              index, static_cast<const void *>(originalEnum),
              static_cast<void *>(originalField),
              static_cast<void *>(
                  originalField != NULL ? originalField->get_parent() : NULL),
              static_cast<void *>(copyEnum), static_cast<void *>(copyField),
              static_cast<void *>(copyField != NULL ? copyField->get_parent()
                                                    : NULL));
      ROSE_ABORT();
    }
  }
}

const SgEnumDeclaration *
requireExactOriginalEnumDefinition(const SgEnumDeclaration *originalEnum) {
  ASSERT_not_null(originalEnum);

  const SgEnumDeclaration *originalDefining =
      isSgEnumDeclaration(originalEnum->get_definingDeclaration());
  if (originalDefining == NULL ||
      originalDefining->get_definingDeclaration() != originalDefining) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[enum-definition-input]: declaration=%p "
            "defining=%p defining-self=%p\n",
            static_cast<const void *>(originalEnum),
            static_cast<const void *>(originalDefining),
            static_cast<const void *>(
                originalDefining != NULL
                    ? originalDefining->get_definingDeclaration()
                    : NULL));
    ROSE_ABORT();
  }
  return originalDefining;
}

SgEnumDeclaration *
finalizeExactCopiedEnumDeclarationChain(const SgEnumDeclaration *originalEnum,
                                        SgEnumDeclaration *copyEnum,
                                        SgCopyHelp &help) {
  ASSERT_not_null(originalEnum);
  ASSERT_not_null(copyEnum);
  if (requireExactMappedNode(originalEnum, help, "enum-declaration") !=
      copyEnum) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[enum-self-map]: original=%p copy=%p "
            "mapped=%p\n",
            static_cast<const void *>(originalEnum),
            static_cast<void *>(copyEnum),
            static_cast<void *>(lookupCopiedNode(help, originalEnum)));
    ROSE_ABORT();
  }

  const SgEnumDeclaration *originalFirstNondef =
      isSgEnumDeclaration(originalEnum->get_firstNondefiningDeclaration());
  const SgEnumDeclaration *originalDefining =
      requireExactOriginalEnumDefinition(originalEnum);
  if (originalFirstNondef == NULL ||
      originalFirstNondef->get_firstNondefiningDeclaration() !=
          originalFirstNondef ||
      originalFirstNondef->get_definingDeclaration() != originalDefining ||
      originalDefining->get_firstNondefiningDeclaration() !=
          originalFirstNondef) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[enum-chain-input]: declaration=%p first=%p "
            "defining=%p\n",
            static_cast<const void *>(originalEnum),
            static_cast<const void *>(originalFirstNondef),
            static_cast<const void *>(originalDefining));
    ROSE_ABORT();
  }

  SgEnumDeclaration *copiedFirstNondef =
      isSgEnumDeclaration(requireExactMappedNode(
          originalFirstNondef, help, "first-nondefining-enum-declaration"));
  SgEnumDeclaration *copiedDefining =
      isSgEnumDeclaration(requireExactMappedNode(originalDefining, help,
                                                 "defining-enum-declaration"));
  if (copiedFirstNondef == NULL || copiedDefining == NULL ||
      copiedFirstNondef->get_firstNondefiningDeclaration() !=
          copiedFirstNondef ||
      copiedFirstNondef->get_definingDeclaration() != copiedDefining ||
      copiedDefining->get_firstNondefiningDeclaration() != copiedFirstNondef ||
      copiedDefining->get_definingDeclaration() != copiedDefining ||
      copyEnum->get_firstNondefiningDeclaration() != copiedFirstNondef ||
      copyEnum->get_definingDeclaration() != copiedDefining) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[enum-chain-output]: declaration=%p copy=%p "
            "first=%p defining=%p\n",
            static_cast<const void *>(originalEnum),
            static_cast<void *>(copyEnum),
            static_cast<void *>(copiedFirstNondef),
            static_cast<void *>(copiedDefining));
    ROSE_ABORT();
  }

  if (copyEnum->isForward() != originalEnum->isForward() ||
      copiedDefining->isForward() != originalDefining->isForward()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[enum-forward-state]: original=%p forward=%d "
            "copy=%p forward=%d defining=%p forward=%d copy-defining=%p "
            "forward=%d\n",
            static_cast<const void *>(originalEnum), originalEnum->isForward(),
            static_cast<void *>(copyEnum), copyEnum->isForward(),
            static_cast<const void *>(originalDefining),
            originalDefining->isForward(), static_cast<void *>(copiedDefining),
            copiedDefining->isForward());
    ROSE_ABORT();
  }

  validateExactCopiedEnumEnumerators(originalEnum, copyEnum, help);
  return copiedDefining;
}

void validateExactCopiedFunctionDefinitions(SgCopyHelp &help) {
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgFunctionDefinition *originalFunctionDef =
        isSgFunctionDefinition(it->first);
    SgFunctionDefinition *copyFunctionDef = isSgFunctionDefinition(it->second);
    if (originalFunctionDef == NULL || copyFunctionDef == NULL) {
      continue;
    }

    const SgFunctionDeclaration *originalDecl =
        originalFunctionDef->get_declaration();
    if (originalDecl == NULL ||
        originalDecl->get_definition() != originalFunctionDef ||
        originalFunctionDef->get_parent() != originalDecl ||
        countDirectOwnerEdges(originalDecl, originalFunctionDef) != 1) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[function-definition-input]: definition=%p "
              "declaration=%p parent=%p\n",
              static_cast<const void *>(originalFunctionDef),
              static_cast<const void *>(originalDecl),
              static_cast<void *>(originalFunctionDef->get_parent()));
      ROSE_ABORT();
    }

    SgFunctionDeclaration *canonicalDecl =
        isSgFunctionDeclaration(requireExactMappedNode(
            originalDecl, help, "function-definition-declaration"));
    if (canonicalDecl == NULL ||
        copyFunctionDef->get_declaration() != canonicalDecl ||
        canonicalDecl->get_definition() != copyFunctionDef ||
        copyFunctionDef->get_parent() != canonicalDecl ||
        countDirectOwnerEdges(canonicalDecl, copyFunctionDef) != 1) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[function-definition-output]: definition=%p "
              "declaration=%p parent=%p\n",
              static_cast<void *>(copyFunctionDef),
              static_cast<void *>(canonicalDecl),
              static_cast<void *>(copyFunctionDef->get_parent()));
      ROSE_ABORT();
    }
  }
}

template <class NodeList>
void validateExactCopiedOwnedList(const SgNode *originalOwner,
                                  const NodeList &originalList,
                                  SgNode *copyOwner, const NodeList &copyList,
                                  SgCopyHelp &help, const char *relation) {
  ASSERT_not_null(originalOwner);
  ASSERT_not_null(copyOwner);
  ASSERT_not_null(relation);
  if (originalList.size() != copyList.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[owned-list-size]: relation=%s original=%p/%s "
            "size=%zu copy=%p/%s size=%zu\n",
            relation, static_cast<const void *>(originalOwner),
            originalOwner->class_name().c_str(), originalList.size(),
            static_cast<void *>(copyOwner), copyOwner->class_name().c_str(),
            copyList.size());
    ROSE_ABORT();
  }

  std::unordered_set<const SgNode *> seenCopies;
  seenCopies.reserve(copyList.size());
  for (size_t index = 0; index < originalList.size(); ++index) {
    const SgNode *originalChild = originalList[index];
    SgNode *copyChild = copyList[index];
    if ((originalChild == NULL) != (copyChild == NULL)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[owned-list-null]: relation=%s index=%zu "
              "original-child=%p copy-child=%p\n",
              relation, index, static_cast<const void *>(originalChild),
              static_cast<void *>(copyChild));
      ROSE_ABORT();
    }
    if (originalChild == NULL) {
      continue;
    }

    SgNode *expectedCopy =
        requireExactMappedNode(originalChild, help, relation);
    if (copyChild != expectedCopy || !seenCopies.insert(copyChild).second ||
        originalChild->get_parent() != originalOwner ||
        countDirectOwnerEdges(originalOwner, originalChild) != 1 ||
        copyChild->get_parent() != copyOwner ||
        countDirectOwnerEdges(copyOwner, copyChild) != 1) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[owned-list-order-owner]: relation=%s "
              "index=%zu original-owner=%p child=%p parent=%p copy-owner=%p "
              "child=%p expected=%p parent=%p\n",
              relation, index, static_cast<const void *>(originalOwner),
              static_cast<const void *>(originalChild),
              static_cast<void *>(originalChild->get_parent()),
              static_cast<void *>(copyOwner), static_cast<void *>(copyChild),
              static_cast<void *>(expectedCopy),
              static_cast<void *>(copyChild != NULL ? copyChild->get_parent()
                                                    : NULL));
      ROSE_ABORT();
    }
  }
}

SgTemplateParameterPtrList *
templateParameterListForDeclaration(SgDeclarationStatement *declaration) {
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
    return NULL;
  }
}

void publishExactCopiedTemplateSemanticSurfaces(SgCopyHelp &help) {
  auto publishExactPair = [&](const SgNode *original, SgNode *copy,
                              const char *relation) {
    ASSERT_not_null(relation);
    SgCopyHelp::copiedNodeMapTypeIterator existing =
        original != NULL
            ? help.get_copiedNodeMap().find(const_cast<SgNode *>(original))
            : help.get_copiedNodeMap().end();
    if (original == NULL || copy == NULL || original == copy ||
        original->variantT() != copy->variantT() ||
        (existing != help.get_copiedNodeMap().end() &&
         existing->second != copy &&
         !copiedNodeLooksDeleted(existing->second))) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[template-semantic-surface-map]: "
              "relation=%s source=%p/%s copy=%p/%s mapped=%p/%s has no "
              "unique exact target\n",
              relation, static_cast<const void *>(original),
              original != NULL ? original->class_name().c_str() : "<null>",
              static_cast<void *>(copy),
              copy != NULL ? copy->class_name().c_str() : "<null>",
              static_cast<void *>(existing != help.get_copiedNodeMap().end()
                                      ? existing->second
                                      : NULL),
              existing != help.get_copiedNodeMap().end() &&
                      existing->second != NULL
                  ? existing->second->class_name().c_str()
                  : "<null>");
      ROSE_ABORT();
    }
    if (existing == help.get_copiedNodeMap().end()) {
      help.insertCopiedNodePair(original, copy);
    } else if (existing->second != copy) {
      help.assignCopiedNodePair(original, copy);
    }
  };

  auto publishOwnedParameterType = [&](const SgTemplateParameter
                                           *originalParameter,
                                       SgTemplateParameter *copiedParameter) {
    ASSERT_not_null(originalParameter);
    ASSERT_not_null(copiedParameter);
    SgType *originalType = originalParameter->get_type();
    SgType *copiedType = copiedParameter->get_type();
    const SgTemplateParameter::template_parameter_enum parameterKind =
        originalParameter->get_parameterType();
    if (originalParameter == copiedParameter ||
        copiedParameter->get_parameterType() != parameterKind ||
        originalType == NULL || copiedType == NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[template-parameter-type-surface]: "
              "source=%p kind=%d type=%p copy=%p kind=%d type=%p\n",
              static_cast<const void *>(originalParameter),
              static_cast<int>(parameterKind),
              static_cast<void *>(originalType),
              static_cast<void *>(copiedParameter),
              static_cast<int>(copiedParameter->get_parameterType()),
              static_cast<void *>(copiedType));
      ROSE_ABORT();
    }

    if (parameterKind == SgTemplateParameter::type_parameter ||
        parameterKind == SgTemplateParameter::template_parameter) {
      SgTemplateType *originalTemplateType = isSgTemplateType(originalType);
      SgTemplateType *copiedTemplateType = isSgTemplateType(copiedType);
      SgTemplateParameter *originalSemanticParameter =
          originalTemplateType != NULL
              ? originalTemplateType->get_template_parameter()
              : NULL;
      SgTemplateParameter *copiedSemanticParameter =
          originalSemanticParameter != NULL
              ? isSgTemplateParameter(
                    lookupCopiedNode(help, originalSemanticParameter))
              : NULL;
      if (originalTemplateType == NULL ||
          originalSemanticParameter != originalParameter ||
          copiedSemanticParameter != copiedParameter ||
          !originalTemplateType->get_tpl_args().empty()) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[template-parameter-owned-type-input]: "
                "source=%p kind=%d type=%p/%s semantic-parameter=%p "
                "copy=%p semantic-parameter=%p args=%zu\n",
                static_cast<const void *>(originalParameter),
                static_cast<int>(parameterKind),
                static_cast<void *>(originalType),
                originalType->class_name().c_str(),
                static_cast<void *>(originalSemanticParameter),
                static_cast<void *>(copiedParameter),
                static_cast<void *>(copiedSemanticParameter),
                originalTemplateType != NULL
                    ? originalTemplateType->get_tpl_args().size()
                    : 0);
        ROSE_ABORT();
      }

      const int sourceDepth =
          originalTemplateType->get_template_parameter_depth();
      const int sourcePosition =
          originalTemplateType->get_template_parameter_position();
      const bool hasSourceCoordinates = sourceDepth >= 0 && sourcePosition >= 0;
      if ((sourceDepth < 0) != (sourcePosition < 0) ||
          hasSourceCoordinates !=
              originalTemplateType->get_canonical_source_identity()
                  .has_value()) {
        fprintf(
            stderr,
            "REX_COPY_INVARIANT[template-parameter-owned-type-"
            "identity]: source=%p type=%p depth=%d position=%d "
            "has-source-identity=%d\n",
            static_cast<const void *>(originalParameter),
            static_cast<void *>(originalTemplateType), sourceDepth,
            sourcePosition,
            originalTemplateType->get_canonical_source_identity().has_value());
        ROSE_ABORT();
      }

      if (copiedTemplateType == NULL ||
          copiedTemplateType == originalTemplateType ||
          copiedTemplateType->get_template_parameter() != copiedParameter ||
          copiedTemplateType->get_name() != originalTemplateType->get_name() ||
          copiedTemplateType->get_template_parameter_depth() != sourceDepth ||
          copiedTemplateType->get_template_parameter_position() !=
              sourcePosition ||
          !(copiedTemplateType->get_canonical_source_identity() ==
            originalTemplateType->get_canonical_source_identity()) ||
          copiedTemplateType->get_packed() !=
              originalTemplateType->get_packed() ||
          !copiedTemplateType->get_tpl_args().empty()) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[template-parameter-owned-type-output]: "
                "source=%p type=%p copy=%p type=%p semantic-parameter=%p "
                "failed exact copied identity publication\n",
                static_cast<const void *>(originalParameter),
                static_cast<void *>(originalTemplateType),
                static_cast<void *>(copiedParameter),
                static_cast<void *>(copiedTemplateType),
                static_cast<void *>(
                    copiedTemplateType != NULL
                        ? copiedTemplateType->get_template_parameter()
                        : NULL));
        ROSE_ABORT();
      }
      publishExactPair(originalTemplateType, copiedTemplateType,
                       "template-parameter-owned-type");
      return;
    }

    if (parameterKind == SgTemplateParameter::nontype_parameter) {
      if (originalType != copiedType) {
        publishExactPair(originalType, copiedType,
                         "template-nontype-parameter-type");
      }
      return;
    }

    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-kind]: source=%p "
            "kind=%d copy=%p\n",
            static_cast<const void *>(originalParameter),
            static_cast<int>(parameterKind),
            static_cast<void *>(copiedParameter));
    ROSE_ABORT();
  };

  std::vector<
      std::pair<const SgDeclarationStatement *, SgDeclarationStatement *>>
      declarationWorklist;
  std::set<std::pair<const SgDeclarationStatement *, SgDeclarationStatement *>>
      queuedDeclarations;
  for (const auto &mapping : help.get_copiedNodeMap()) {
    const SgDeclarationStatement *originalDeclaration =
        isSgDeclarationStatement(mapping.first);
    SgDeclarationStatement *copiedDeclaration =
        isSgDeclarationStatement(mapping.second);
    const auto pair = std::make_pair(originalDeclaration, copiedDeclaration);
    if (originalDeclaration != NULL && copiedDeclaration != NULL &&
        originalDeclaration != copiedDeclaration &&
        queuedDeclarations.insert(pair).second) {
      declarationWorklist.push_back(pair);
    }
  }

  for (size_t declarationIndex = 0;
       declarationIndex < declarationWorklist.size(); ++declarationIndex) {
    const SgDeclarationStatement *originalDeclaration =
        declarationWorklist[declarationIndex].first;
    SgDeclarationStatement *copiedDeclaration =
        declarationWorklist[declarationIndex].second;
    ASSERT_not_null(originalDeclaration);
    ASSERT_not_null(copiedDeclaration);

    auto publishOwnedDeclarationScope = [&](SgDeclarationScope *originalScope,
                                            SgDeclarationScope *copiedScope,
                                            const char *scopeRelation,
                                            const char *declarationRelation) {
      if ((originalScope == NULL) != (copiedScope == NULL)) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[template-semantic-scope-shape]: "
                "relation=%s source=%p/%s scope=%p copy=%p/%s scope=%p\n",
                scopeRelation, static_cast<const void *>(originalDeclaration),
                originalDeclaration->class_name().c_str(),
                static_cast<void *>(originalScope),
                static_cast<void *>(copiedDeclaration),
                copiedDeclaration->class_name().c_str(),
                static_cast<void *>(copiedScope));
        ROSE_ABORT();
      }
      if (originalScope == NULL) {
        return;
      }
      if (originalScope->get_parent() != originalDeclaration ||
          copiedScope->get_parent() != copiedDeclaration) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[template-semantic-scope-owner]: "
                "relation=%s source=%p scope=%p parent=%p copy=%p "
                "scope=%p parent=%p\n",
                scopeRelation, static_cast<const void *>(originalDeclaration),
                static_cast<void *>(originalScope),
                static_cast<void *>(originalScope->get_parent()),
                static_cast<void *>(copiedDeclaration),
                static_cast<void *>(copiedScope),
                static_cast<void *>(copiedScope->get_parent()));
        ROSE_ABORT();
      }
      publishExactPair(originalScope, copiedScope, scopeRelation);

      const SgDeclarationStatementPtrList &originalDeclarations =
          originalScope->get_declarations();
      const SgDeclarationStatementPtrList &copiedDeclarations =
          copiedScope->get_declarations();
      if (originalDeclarations.size() != copiedDeclarations.size()) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[template-semantic-scope-list]: "
                "relation=%s source=%p declarations=%zu copy=%p "
                "declarations=%zu\n",
                declarationRelation, static_cast<void *>(originalScope),
                originalDeclarations.size(), static_cast<void *>(copiedScope),
                copiedDeclarations.size());
        ROSE_ABORT();
      }
      for (size_t index = 0; index < originalDeclarations.size(); ++index) {
        const SgDeclarationStatement *originalChild =
            originalDeclarations[index];
        SgDeclarationStatement *copiedChild = copiedDeclarations[index];
        if (originalChild == NULL || copiedChild == NULL ||
            originalChild->get_parent() != originalScope ||
            copiedChild->get_parent() != copiedScope) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[template-semantic-scope-child]: "
                  "relation=%s index=%zu source=%p parent=%p copy=%p "
                  "parent=%p\n",
                  declarationRelation, index,
                  static_cast<const void *>(originalChild),
                  static_cast<void *>(originalChild != NULL
                                          ? originalChild->get_parent()
                                          : NULL),
                  static_cast<void *>(copiedChild),
                  static_cast<void *>(
                      copiedChild != NULL ? copiedChild->get_parent() : NULL));
          ROSE_ABORT();
        }
        publishExactPair(originalChild, copiedChild, declarationRelation);
        const auto childPair = std::make_pair(originalChild, copiedChild);
        if (queuedDeclarations.insert(childPair).second) {
          declarationWorklist.push_back(childPair);
        }
      }
    };

    publishOwnedDeclarationScope(originalDeclaration->get_nonreal_decl_scope(),
                                 copiedDeclaration->get_nonreal_decl_scope(),
                                 "template-owner-nonreal-scope",
                                 "template-owner-nonreal-declaration");

    SgTemplateParameterPtrList *originalParameters =
        templateParameterListForDeclaration(
            const_cast<SgDeclarationStatement *>(originalDeclaration));
    SgTemplateParameterPtrList *copiedParameters =
        templateParameterListForDeclaration(copiedDeclaration);
    if ((originalParameters == NULL) != (copiedParameters == NULL)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[template-semantic-parameter-list]: "
              "source=%p/%s parameters=%p copy=%p/%s parameters=%p\n",
              static_cast<const void *>(originalDeclaration),
              originalDeclaration->class_name().c_str(),
              static_cast<void *>(originalParameters),
              static_cast<void *>(copiedDeclaration),
              copiedDeclaration->class_name().c_str(),
              static_cast<void *>(copiedParameters));
      ROSE_ABORT();
    }
    if (originalParameters == NULL) {
      continue;
    }
    if (originalParameters->size() != copiedParameters->size()) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[template-semantic-parameter-count]: "
          "source=%p/%s parameters=%zu copy=%p/%s parameters=%zu\n",
          static_cast<const void *>(originalDeclaration),
          originalDeclaration->class_name().c_str(), originalParameters->size(),
          static_cast<void *>(copiedDeclaration),
          copiedDeclaration->class_name().c_str(), copiedParameters->size());
      ROSE_ABORT();
    }

    for (size_t parameterIndex = 0; parameterIndex < originalParameters->size();
         ++parameterIndex) {
      const SgTemplateParameter *originalParameter =
          (*originalParameters)[parameterIndex];
      SgTemplateParameter *copiedParameter =
          (*copiedParameters)[parameterIndex];
      if (originalParameter == NULL || copiedParameter == NULL ||
          originalParameter->get_parent() != originalDeclaration ||
          copiedParameter->get_parent() != copiedDeclaration ||
          originalParameter->get_parameterType() !=
              copiedParameter->get_parameterType()) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[template-semantic-parameter]: "
                "index=%zu source=%p kind=%d parent=%p copy=%p kind=%d "
                "parent=%p\n",
                parameterIndex, static_cast<const void *>(originalParameter),
                originalParameter != NULL
                    ? static_cast<int>(originalParameter->get_parameterType())
                    : -1,
                static_cast<void *>(originalParameter != NULL
                                        ? originalParameter->get_parent()
                                        : NULL),
                static_cast<void *>(copiedParameter),
                copiedParameter != NULL
                    ? static_cast<int>(copiedParameter->get_parameterType())
                    : -1,
                static_cast<void *>(copiedParameter != NULL
                                        ? copiedParameter->get_parent()
                                        : NULL));
        ROSE_ABORT();
      }
      publishExactPair(originalParameter, copiedParameter,
                       "template-parameter");
      SgDeclarationStatement *originalTemplate =
          originalParameter->get_templateDeclaration();
      SgDeclarationStatement *copiedTemplate =
          copiedParameter->get_templateDeclaration();
      if ((originalTemplate == NULL) != (copiedTemplate == NULL)) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[template-semantic-parameter-"
                "declaration]: source=%p declaration=%p copy=%p "
                "declaration=%p\n",
                static_cast<const void *>(originalParameter),
                static_cast<void *>(originalTemplate),
                static_cast<void *>(copiedParameter),
                static_cast<void *>(copiedTemplate));
        ROSE_ABORT();
      }
      if (originalTemplate != NULL) {
        publishExactPair(originalTemplate, copiedTemplate,
                         "template-parameter-declaration");
        const auto templatePair = std::make_pair(
            static_cast<const SgDeclarationStatement *>(originalTemplate),
            copiedTemplate);
        if (queuedDeclarations.insert(templatePair).second) {
          declarationWorklist.push_back(templatePair);
        }
      }
    }
  }

  std::vector<std::pair<const SgTemplateParameter *, SgTemplateParameter *>>
      copiedParameterPairs;
  for (const auto &mapping : help.get_copiedNodeMap()) {
    const SgTemplateParameter *originalParameter =
        isSgTemplateParameter(mapping.first);
    SgTemplateParameter *copiedParameter =
        isSgTemplateParameter(mapping.second);
    if (originalParameter != NULL && copiedParameter != NULL &&
        originalParameter != copiedParameter) {
      copiedParameterPairs.emplace_back(originalParameter, copiedParameter);
    }
  }
  for (const auto &parameterPair : copiedParameterPairs) {
    publishOwnedParameterType(parameterPair.first, parameterPair.second);
  }
}

void validateExactCopiedTemplateParameters(
    const SgDeclarationStatement *originalDeclaration,
    SgDeclarationStatement *copiedDeclaration, SgCopyHelp &help) {
  ASSERT_not_null(originalDeclaration);
  ASSERT_not_null(copiedDeclaration);
  SgTemplateParameterPtrList *originalParameters =
      templateParameterListForDeclaration(
          const_cast<SgDeclarationStatement *>(originalDeclaration));
  SgTemplateParameterPtrList *copiedParameters =
      templateParameterListForDeclaration(copiedDeclaration);
  if ((originalParameters == NULL) != (copiedParameters == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-list-kind]: original=%p/%s "
            "copy=%p/%s\n",
            static_cast<const void *>(originalDeclaration),
            originalDeclaration->class_name().c_str(),
            static_cast<void *>(copiedDeclaration),
            copiedDeclaration->class_name().c_str());
    ROSE_ABORT();
  }
  if (originalParameters == NULL) {
    return;
  }

  validateExactCopiedOwnedList(originalDeclaration, *originalParameters,
                               copiedDeclaration, *copiedParameters, help,
                               "template-parameter");
  for (size_t index = 0; index < originalParameters->size(); ++index) {
    SgTemplateParameter *originalParameter = (*originalParameters)[index];
    SgTemplateParameter *copiedParameter = (*copiedParameters)[index];
    ASSERT_not_null(originalParameter);
    ASSERT_not_null(copiedParameter);
    originalParameter->fixupCopy_scopes(copiedParameter, help);
  }
}

void validateExactCopiedOwnedDeclarationScope(
    const SgDeclarationStatement *originalDeclaration,
    SgDeclarationStatement *copiedDeclaration, SgCopyHelp &help) {
  ASSERT_not_null(originalDeclaration);
  ASSERT_not_null(copiedDeclaration);
  SgDeclarationScope *originalNonrealScope =
      originalDeclaration->get_nonreal_decl_scope();
  SgDeclarationScope *copiedNonrealScope =
      copiedDeclaration->get_nonreal_decl_scope();
  SgDeclarationScope *originalSourceDeclaratorScope =
      originalDeclaration->get_source_declarator_scope();
  SgDeclarationScope *copiedSourceDeclaratorScope =
      copiedDeclaration->get_source_declarator_scope();
  if (originalNonrealScope != NULL &&
      originalNonrealScope == originalSourceDeclaratorScope) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[owned-declaration-scope-alias]: "
            "original=%p/%s uses scope=%p for both semantic nonreal and "
            "physical source-declarator ownership\n",
            static_cast<const void *>(originalDeclaration),
            originalDeclaration->class_name().c_str(),
            static_cast<void *>(originalNonrealScope));
    ROSE_ABORT();
  }

  auto validateScope =
      [&](SgDeclarationScope *originalScope, SgDeclarationScope *copiedScope,
          const char *scopeContext, const char *declarationContext) {
        if ((originalScope == NULL) != (copiedScope == NULL)) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[owned-declaration-scope-null]: "
                  "context=%s original=%p/%s scope=%p copy=%p/%s scope=%p\n",
                  scopeContext, static_cast<const void *>(originalDeclaration),
                  originalDeclaration->class_name().c_str(),
                  static_cast<void *>(originalScope),
                  static_cast<void *>(copiedDeclaration),
                  copiedDeclaration->class_name().c_str(),
                  static_cast<void *>(copiedScope));
          ROSE_ABORT();
        }
        if (originalScope == NULL) {
          return;
        }

        requireExactCopiedOwnedChild(originalDeclaration, originalScope,
                                     copiedDeclaration, copiedScope, help,
                                     scopeContext);
        originalScope->fixupCopy_scopes(copiedScope, help);

        const SgDeclarationStatementPtrList &originalDeclarations =
            originalScope->get_declarations();
        const SgDeclarationStatementPtrList &copiedDeclarations =
            copiedScope->get_declarations();
        validateExactCopiedOwnedList(originalScope, originalDeclarations,
                                     copiedScope, copiedDeclarations, help,
                                     declarationContext);
        for (size_t index = 0; index < originalDeclarations.size(); ++index) {
          SgDeclarationStatement *originalChild = originalDeclarations[index];
          SgDeclarationStatement *copiedChild = copiedDeclarations[index];
          ASSERT_not_null(originalChild);
          ASSERT_not_null(copiedChild);
          originalChild->fixupCopy_scopes(copiedChild, help);
        }
      };

  validateScope(originalNonrealScope, copiedNonrealScope,
                "owned-nonreal-declaration-scope",
                "owned-nonreal-declaration-scope-declaration");
  validateScope(originalSourceDeclaratorScope, copiedSourceDeclaratorScope,
                "owned-source-declarator-scope",
                "owned-source-declarator-scope-declaration");
}

void validateExactCopiedFunctionDeclaratorScope(
    const SgFunctionDeclaration *originalDeclaration,
    SgFunctionDeclaration *copiedDeclaration, SgCopyHelp &help) {
  ASSERT_not_null(originalDeclaration);
  ASSERT_not_null(copiedDeclaration);
  SgDeclarationScope *originalScope =
      originalDeclaration->get_function_declarator_scope();
  SgDeclarationScope *copiedScope =
      copiedDeclaration->get_function_declarator_scope();
  if ((originalScope == NULL) != (copiedScope == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[function-declarator-scope-null]: "
            "original=%p/%s scope=%p copy=%p/%s scope=%p\n",
            static_cast<const void *>(originalDeclaration),
            originalDeclaration->class_name().c_str(),
            static_cast<void *>(originalScope),
            static_cast<void *>(copiedDeclaration),
            copiedDeclaration->class_name().c_str(),
            static_cast<void *>(copiedScope));
    ROSE_ABORT();
  }
  if (originalScope == NULL) {
    return;
  }

  requireExactCopiedOwnedChild(originalDeclaration, originalScope,
                               copiedDeclaration, copiedScope, help,
                               "function-declarator-scope");
  originalScope->fixupCopy_scopes(copiedScope, help);

  const SgDeclarationStatementPtrList &originalDeclarations =
      originalScope->get_declarations();
  const SgDeclarationStatementPtrList &copiedDeclarations =
      copiedScope->get_declarations();
  validateExactCopiedOwnedList(originalScope, originalDeclarations, copiedScope,
                               copiedDeclarations, help,
                               "function-declarator-scope-declaration");
  for (size_t index = 0; index < originalDeclarations.size(); ++index) {
    SgDeclarationStatement *originalChild = originalDeclarations[index];
    SgDeclarationStatement *copiedChild = copiedDeclarations[index];
    ASSERT_not_null(originalChild);
    ASSERT_not_null(copiedChild);
    originalChild->fixupCopy_scopes(copiedChild, help);
  }
}

void validateExactCopiedScopeList(const SgScopeStatement *originalScope,
                                  SgScopeStatement *copyScope,
                                  SgCopyHelp &help) {
  ASSERT_not_null(originalScope);
  ASSERT_not_null(copyScope);
  switch (originalScope->variantT()) {
  case V_SgBasicBlock:
    validateExactCopiedOwnedList(
        originalScope, isSgBasicBlock(originalScope)->get_statements(),
        copyScope, isSgBasicBlock(copyScope)->get_statements(), help,
        "basic-block-statement");
    break;

  case V_SgClassDefinition:
  case V_SgTemplateInstantiationDefn:
    validateExactCopiedOwnedList(
        originalScope, isSgClassDefinition(originalScope)->get_members(),
        copyScope, isSgClassDefinition(copyScope)->get_members(), help,
        "class-member");
    break;

  case V_SgGlobal:
    validateExactCopiedOwnedList(
        originalScope, isSgGlobal(originalScope)->get_declarations(), copyScope,
        isSgGlobal(copyScope)->get_declarations(), help, "global-declaration");
    break;

  case V_SgNamespaceDefinitionStatement:
    validateExactCopiedOwnedList(
        originalScope,
        isSgNamespaceDefinitionStatement(originalScope)->get_declarations(),
        copyScope,
        isSgNamespaceDefinitionStatement(copyScope)->get_declarations(), help,
        "namespace-declaration");
    break;

  case V_SgDeclarationScope:
    validateExactCopiedOwnedList(
        originalScope, isSgDeclarationScope(originalScope)->get_declarations(),
        copyScope, isSgDeclarationScope(copyScope)->get_declarations(), help,
        "nonreal-declaration");
    break;

  default:
    break;
  }
}

bool hasNonidentityMappedOwner(const SgNode *node, SgCopyHelp &help) {
  std::unordered_set<const SgNode *> visited;
  for (const SgNode *current = node; current != NULL;
       current = current->get_parent()) {
    if (!visited.insert(current).second) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[semantic-owner-cycle]: node=%p/%s "
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

bool isGloballyCanonicalBuiltinType(const SgNode *node) {
  const SgType *type = isSgType(node);
  if (type == NULL) {
    return false;
  }

  switch (type->variantT()) {
  case V_SgTypeUnknown:
  case V_SgTypeChar:
  case V_SgTypeSignedChar:
  case V_SgTypeUnsignedChar:
  case V_SgTypeShort:
  case V_SgTypeSignedShort:
  case V_SgTypeUnsignedShort:
  case V_SgTypeInt:
  case V_SgTypeSignedInt:
  case V_SgTypeUnsignedInt:
  case V_SgTypeLong:
  case V_SgTypeSignedLong:
  case V_SgTypeUnsignedLong:
  case V_SgTypeLongLong:
  case V_SgTypeSignedLongLong:
  case V_SgTypeUnsignedLongLong:
  case V_SgTypeSigned128bitInteger:
  case V_SgTypeUnsigned128bitInteger:
  case V_SgTypeFloat:
  case V_SgTypeDouble:
  case V_SgTypeLongDouble:
  case V_SgTypeFloat80:
  case V_SgTypeFloat128:
  case V_SgTypeBool:
  case V_SgTypeVoid:
  case V_SgTypeWchar:
  case V_SgTypeChar16:
  case V_SgTypeChar32:
  case V_SgTypeChar8:
  case V_SgTypeNullptr:
  case V_SgTypeTargetBuiltin:
    return true;

  default:
    return false;
  }
}

SgNode *remapExactCopiedOrExternalSemanticEdge(const SgNode *originalEdge,
                                               SgNode *provisionalCopiedEdge,
                                               SgCopyHelp &help,
                                               const char *relation) {
  ASSERT_not_null(relation);
  if (originalEdge == NULL) {
    if (provisionalCopiedEdge != NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[semantic-edge-null]: relation=%s "
              "source=null copy-edge=%p/%s\n",
              relation, static_cast<void *>(provisionalCopiedEdge),
              provisionalCopiedEdge->class_name().c_str());
      ROSE_ABORT();
    }
    return NULL;
  }

  SgCopyHelp::copiedNodeMapTypeIterator mapping =
      help.get_copiedNodeMap().find(const_cast<SgNode *>(originalEdge));
  if (mapping != help.get_copiedNodeMap().end() &&
      mapping->second != originalEdge) {
    SgNode *expected = mapping->second;
    if (expected == NULL || copiedNodeLooksDeleted(expected) ||
        expected->variantT() != originalEdge->variantT() ||
        (provisionalCopiedEdge != originalEdge &&
         provisionalCopiedEdge != expected)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[semantic-edge-remap]: relation=%s "
              "source=%p/%s provisional=%p/%s expected=%p/%s\n",
              relation, static_cast<const void *>(originalEdge),
              originalEdge->class_name().c_str(),
              static_cast<void *>(provisionalCopiedEdge),
              provisionalCopiedEdge != NULL
                  ? provisionalCopiedEdge->class_name().c_str()
                  : "<null>",
              static_cast<void *>(expected),
              expected != NULL ? expected->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }
    return expected;
  }

  SgNode *plannedExternalEdge =
      help.plannedExternalSemanticCopyEdge(originalEdge);
  if (plannedExternalEdge != NULL) {
    if ((plannedExternalEdge->variantT() != originalEdge->variantT() &&
         (isSgScopeStatement(originalEdge) == nullptr ||
          isSgScopeStatement(plannedExternalEdge) == nullptr)) ||
        (provisionalCopiedEdge != originalEdge &&
         provisionalCopiedEdge != plannedExternalEdge) ||
        hasNonidentityMappedOwner(originalEdge, help)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[semantic-edge-remap-plan]: relation=%s "
              "source=%p/%s provisional=%p/%s planned=%p/%s "
              "inside-transaction=%d\n",
              relation, static_cast<const void *>(originalEdge),
              originalEdge->class_name().c_str(),
              static_cast<void *>(provisionalCopiedEdge),
              provisionalCopiedEdge != NULL
                  ? provisionalCopiedEdge->class_name().c_str()
                  : "<null>",
              static_cast<void *>(plannedExternalEdge),
              plannedExternalEdge != NULL
                  ? plannedExternalEdge->class_name().c_str()
                  : "<null>",
              hasNonidentityMappedOwner(originalEdge, help) ? 1 : 0);
      ROSE_ABORT();
    }
    return plannedExternalEdge;
  }

  if (provisionalCopiedEdge != originalEdge ||
      (!isGloballyCanonicalBuiltinType(originalEdge) &&
       hasNonidentityMappedOwner(originalEdge, help))) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[semantic-edge-remap-external]: relation=%s "
            "source=%p/%s provisional=%p/%s mapped=%p "
            "inside-transaction=%d\n",
            relation, static_cast<const void *>(originalEdge),
            originalEdge->class_name().c_str(),
            static_cast<void *>(provisionalCopiedEdge),
            provisionalCopiedEdge != NULL
                ? provisionalCopiedEdge->class_name().c_str()
                : "<null>",
            static_cast<void *>(mapping != help.get_copiedNodeMap().end()
                                    ? mapping->second
                                    : NULL),
            hasNonidentityMappedOwner(originalEdge, help) ? 1 : 0);
    printCopyParentChain("semantic-edge-remap-external", originalEdge, help);
    ROSE_ABORT();
  }
  return const_cast<SgNode *>(originalEdge);
}

void fixupExactCopiedOwnedSuccessorScopes(const SgNode *original, SgNode *copy,
                                          SgCopyHelp &help,
                                          const char *relation) {
  ASSERT_not_null(original);
  ASSERT_not_null(copy);
  ASSERT_not_null(relation);
  SgNode *expected = requireExactMappedNode(original, help, relation);
  if (expected != copy || copy->variantT() != original->variantT()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[owned-successor-root]: relation=%s "
            "source=%p/%s copy=%p/%s expected=%p/%s\n",
            relation, static_cast<const void *>(original),
            original->class_name().c_str(), static_cast<void *>(copy),
            copy->class_name().c_str(), static_cast<void *>(expected),
            expected != NULL ? expected->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  fixupExactCopiedSupportScopes(original, copy, help);
}

SgStatement *exactCopiedOrExternalStatementEdge(const SgStatement *originalEdge,
                                                SgStatement *copiedEdge,
                                                SgCopyHelp &help,
                                                const char *relation,
                                                size_t index, bool applyRemap) {
  ASSERT_not_null(relation);
  if (originalEdge == NULL) {
    if (copiedEdge != NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[statement-semantic-null]: relation=%s "
              "index=%zu source=null copy=%p/%s\n",
              relation, index, static_cast<void *>(copiedEdge),
              copiedEdge->class_name().c_str());
      ROSE_ABORT();
    }
    return NULL;
  }

  SgCopyHelp::copiedNodeMapTypeIterator mapping =
      help.get_copiedNodeMap().find(const_cast<SgStatement *>(originalEdge));
  SgStatement *mappedEdge = mapping != help.get_copiedNodeMap().end()
                                ? isSgStatement(mapping->second)
                                : NULL;
  const bool internal = mapping != help.get_copiedNodeMap().end() &&
                        mapping->second != originalEdge;
  if (internal) {
    if (mappedEdge == NULL || copiedNodeLooksDeleted(mappedEdge) ||
        mappedEdge->variantT() != originalEdge->variantT() ||
        (copiedEdge != originalEdge && copiedEdge != mappedEdge) ||
        (!applyRemap && copiedEdge != mappedEdge)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[statement-semantic-map]: relation=%s "
              "index=%zu source=%p/%s copy=%p/%s mapped=%p/%s "
              "apply-remap=%d\n",
              relation, index, static_cast<const void *>(originalEdge),
              originalEdge->class_name().c_str(),
              static_cast<void *>(copiedEdge),
              copiedEdge != NULL ? copiedEdge->class_name().c_str() : "<null>",
              static_cast<void *>(mappedEdge),
              mappedEdge != NULL ? mappedEdge->class_name().c_str() : "<null>",
              applyRemap ? 1 : 0);
      ROSE_ABORT();
    }
    return mappedEdge;
  }

  const bool insideTransaction = hasNonidentityMappedOwner(originalEdge, help);
  if (mapping != help.get_copiedNodeMap().end() || insideTransaction ||
      copiedEdge != originalEdge) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[statement-semantic-external]: relation=%s "
            "index=%zu source=%p/%s copy=%p/%s mapped=%p "
            "inside-transaction=%d\n",
            relation, index, static_cast<const void *>(originalEdge),
            originalEdge->class_name().c_str(), static_cast<void *>(copiedEdge),
            copiedEdge != NULL ? copiedEdge->class_name().c_str() : "<null>",
            static_cast<void *>(mapping != help.get_copiedNodeMap().end()
                                    ? mapping->second
                                    : NULL),
            insideTransaction ? 1 : 0);
    ROSE_ABORT();
  }
  return const_cast<SgStatement *>(originalEdge);
}

void fixupOmpExecSemanticEdges(const SgOmpExecStatement *original,
                               SgOmpExecStatement *copy, SgCopyHelp &help,
                               bool applyRemap) {
  ASSERT_not_null(original);
  ASSERT_not_null(copy);
  SgStatement *expectedParent = exactCopiedOrExternalStatementEdge(
      original->get_omp_parent(), copy->get_omp_parent(), help, "openmp-parent",
      0, applyRemap);
  if (applyRemap) {
    copy->set_omp_parent(expectedParent);
  }

  const SgStatementPtrList &originalChildren = original->get_omp_children();
  SgStatementPtrList &copiedChildren = copy->get_omp_children();
  if (originalChildren.size() != copiedChildren.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openmp-children-count]: source=%p/%s "
            "children=%zu copy=%p/%s children=%zu\n",
            static_cast<const void *>(original), original->class_name().c_str(),
            originalChildren.size(), static_cast<void *>(copy),
            copy->class_name().c_str(), copiedChildren.size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalChildren.size(); ++index) {
    SgStatement *expectedChild = exactCopiedOrExternalStatementEdge(
        originalChildren[index], copiedChildren[index], help, "openmp-child",
        index, applyRemap);
    if (applyRemap) {
      copiedChildren[index] = expectedChild;
    }
  }
}

void fixupOmpDeclareTargetSemanticEdges(
    const SgOmpDeclareTargetStatement *original,
    SgOmpDeclareTargetStatement *copy, SgCopyHelp &help, bool applyRemap) {
  ASSERT_not_null(original);
  ASSERT_not_null(copy);
  const SgStatementPtrList &originalStatements = original->get_statements();
  SgStatementPtrList &copiedStatements = copy->get_statements();
  if (originalStatements.size() != copiedStatements.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[declare-target-statements-count]: source=%p "
            "statements=%zu copy=%p statements=%zu\n",
            static_cast<const void *>(original), originalStatements.size(),
            static_cast<void *>(copy), copiedStatements.size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalStatements.size(); ++index) {
    SgStatement *expectedStatement = exactCopiedOrExternalStatementEdge(
        originalStatements[index], copiedStatements[index], help,
        "declare-target-statement", index, applyRemap);
    if (applyRemap) {
      copiedStatements[index] = expectedStatement;
    }
  }
}

SgNode *requireExactCopiedOrExternalSemanticEdge(const SgNode *originalEdge,
                                                 SgNode *copiedEdge,
                                                 SgCopyHelp &help,
                                                 const char *relation) {
  ASSERT_not_null(originalEdge);
  ASSERT_not_null(relation);
  SgCopyHelp::copiedNodeMapTypeIterator mapping =
      help.get_copiedNodeMap().find(const_cast<SgNode *>(originalEdge));
  if (mapping != help.get_copiedNodeMap().end() &&
      mapping->second != originalEdge) {
    SgNode *expected = mapping->second;
    if (expected == NULL || copiedNodeLooksDeleted(expected) ||
        expected->variantT() != originalEdge->variantT() ||
        copiedEdge != expected) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[semantic-edge-map]: relation=%s "
              "source=%p/%s copy-edge=%p/%s expected=%p/%s\n",
              relation, static_cast<const void *>(originalEdge),
              originalEdge->class_name().c_str(),
              static_cast<void *>(copiedEdge),
              copiedEdge != NULL ? copiedEdge->class_name().c_str() : "<null>",
              static_cast<void *>(expected),
              expected != NULL ? expected->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }
    return expected;
  }

  SgNode *plannedExternalEdge =
      help.plannedExternalSemanticCopyEdge(originalEdge);
  if (plannedExternalEdge != NULL) {
    if ((plannedExternalEdge->variantT() != originalEdge->variantT() &&
         (isSgScopeStatement(originalEdge) == nullptr ||
          isSgScopeStatement(plannedExternalEdge) == nullptr)) ||
        copiedEdge != plannedExternalEdge ||
        hasNonidentityMappedOwner(originalEdge, help)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[semantic-edge-plan]: relation=%s "
              "source=%p/%s copy-edge=%p/%s planned=%p/%s "
              "inside-transaction=%d\n",
              relation, static_cast<const void *>(originalEdge),
              originalEdge->class_name().c_str(),
              static_cast<void *>(copiedEdge),
              copiedEdge != NULL ? copiedEdge->class_name().c_str() : "<null>",
              static_cast<void *>(plannedExternalEdge),
              plannedExternalEdge != NULL
                  ? plannedExternalEdge->class_name().c_str()
                  : "<null>",
              hasNonidentityMappedOwner(originalEdge, help) ? 1 : 0);
      ROSE_ABORT();
    }
    return plannedExternalEdge;
  }

  // An edge may retain its source identity only when that semantic target is
  // outside the copied structural transaction. If any owner is copied, the
  // missing mapping is an incomplete deep copy, not an external reference.
  if (copiedEdge != originalEdge ||
      (!isGloballyCanonicalBuiltinType(originalEdge) &&
       hasNonidentityMappedOwner(originalEdge, help))) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[semantic-edge-external]: relation=%s "
            "source=%p/%s copy-edge=%p/%s mapped=%p inside-transaction=%d\n",
            relation, static_cast<const void *>(originalEdge),
            originalEdge->class_name().c_str(), static_cast<void *>(copiedEdge),
            copiedEdge != NULL ? copiedEdge->class_name().c_str() : "<null>",
            static_cast<void *>(mapping != help.get_copiedNodeMap().end()
                                    ? mapping->second
                                    : NULL),
            hasNonidentityMappedOwner(originalEdge, help) ? 1 : 0);
    printCopyParentChain("semantic-edge-external", originalEdge, help);
    ROSE_ABORT();
  }
  return const_cast<SgNode *>(originalEdge);
}

// Syntax shells are declaration-owned, while their semantic type edges use
// the copy transaction's identities. A typedef may already be copied when the
// shell is constructed; other named types are published by the complete type
// graph transaction. Validate the semantic edges after that publication.
void validateExactCopiedFunctionSyntaxTypes(SgCopyHelp &help) {
  for (const auto &mapping : help.get_copiedNodeMap()) {
    const auto *source = isSgFunctionDeclaration(mapping.first);
    auto *copy = isSgFunctionDeclaration(mapping.second);
    if (source == nullptr || source == copy)
      continue;
    ROSE_ASSERT(copy != nullptr);
    source->validate_explicit_specifier();
    copy->validate_explicit_specifier();
    const auto *sourceCondition = source->get_explicit_specifier_expression();
    const auto *copyCondition = copy->get_explicit_specifier_expression();
    ROSE_ASSERT((sourceCondition == nullptr) == (copyCondition == nullptr));
    if (sourceCondition != nullptr) {
      const auto conditionMapping =
          help.get_copiedNodeMap().find(sourceCondition);
      ROSE_ASSERT(conditionMapping != help.get_copiedNodeMap().end());
      ROSE_ASSERT(conditionMapping->second == copyCondition &&
                  sourceCondition != copyCondition);
    }
    const SgFunctionType *sourceType = source->get_type_syntax();
    const SgFunctionType *copyType = copy->get_type_syntax();
    ROSE_ASSERT((sourceType == nullptr) == (copyType == nullptr));
    if (sourceType == nullptr)
      continue;
    requireExactCopiedOrExternalSemanticEdge(sourceType->get_return_type(),
                                             copyType->get_return_type(), help,
                                             "function-syntax-return-type");
    ROSE_ASSERT(sourceType->get_argument_list() != nullptr);
    ROSE_ASSERT(copyType->get_argument_list() != nullptr);
    const auto &sourceArguments = sourceType->get_arguments();
    const auto &copyArguments = copyType->get_arguments();
    ROSE_ASSERT(sourceArguments.size() == copyArguments.size());
    for (size_t index = 0; index < sourceArguments.size(); ++index)
      requireExactCopiedOrExternalSemanticEdge(sourceArguments[index],
                                               copyArguments[index], help,
                                               "function-syntax-argument-type");
    const auto *sourceMember = isSgMemberFunctionType(sourceType);
    const auto *copyMember = isSgMemberFunctionType(copyType);
    ROSE_ASSERT((sourceMember == nullptr) == (copyMember == nullptr));
    if (sourceMember != nullptr)
      requireExactCopiedOrExternalSemanticEdge(
          sourceMember->get_class_type(), copyMember->get_class_type(), help,
          "function-syntax-member-class-type");
  }
}

void validateExactCopiedInitializedNameSemanticEdges(
    const SgInitializedName *originalName, SgInitializedName *copiedName,
    SgCopyHelp &help) {
  ASSERT_not_null(originalName);
  ASSERT_not_null(copiedName);

  SgScopeStatement *originalScope = originalName->get_scope();
  SgScopeStatement *copiedScope = copiedName->get_scope();
  if ((originalScope == NULL) != (copiedScope == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[initialized-name-scope-null]: source=%p/%s "
            "scope=%p copy=%p scope=%p\n",
            static_cast<const void *>(originalName),
            originalName->get_name().str(), static_cast<void *>(originalScope),
            static_cast<void *>(copiedName), static_cast<void *>(copiedScope));
    ROSE_ABORT();
  }
  if (originalScope != NULL) {
    SgScopeStatement *resolvedScope =
        isSgScopeStatement(remapExactCopiedOrExternalSemanticEdge(
            originalScope, copiedScope, help, "initialized-name-scope"));
    if (resolvedScope == NULL || copiedScope != resolvedScope) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[initialized-name-scope-binding]: source=%p/%s "
          "scope=%p/%s copy=%p scope=%p resolved=%p/%s\n",
          static_cast<const void *>(originalName),
          originalName->get_name().str(), static_cast<void *>(originalScope),
          originalScope->class_name().c_str(), static_cast<void *>(copiedName),
          static_cast<void *>(copiedScope), static_cast<void *>(resolvedScope),
          resolvedScope != NULL ? resolvedScope->class_name().c_str()
                                : "<null>");
      ROSE_ABORT();
    }
  }

  SgDeclarationStatement *originalDeclaration = originalName->get_declptr();
  SgDeclarationStatement *copiedDeclaration = copiedName->get_declptr();
  if ((originalDeclaration == NULL) != (copiedDeclaration == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[initialized-name-declptr-null]: source=%p/%s "
            "declptr=%p copy=%p declptr=%p\n",
            static_cast<const void *>(originalName),
            originalName->get_name().str(),
            static_cast<void *>(originalDeclaration),
            static_cast<void *>(copiedName),
            static_cast<void *>(copiedDeclaration));
    ROSE_ABORT();
  }
  if (originalDeclaration != NULL) {
    SgDeclarationStatement *resolvedDeclaration =
        isSgDeclarationStatement(remapExactCopiedOrExternalSemanticEdge(
            originalDeclaration, copiedDeclaration, help,
            "initialized-name-declptr"));
    if (resolvedDeclaration == NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[initialized-name-declptr-kind]: "
              "source=%p/%s declaration=%p copy=%p declaration=%p "
              "resolved=%p\n",
              static_cast<const void *>(originalName),
              originalName->get_name().str(),
              static_cast<void *>(originalDeclaration),
              static_cast<void *>(copiedName),
              static_cast<void *>(copiedDeclaration),
              static_cast<void *>(resolvedDeclaration));
      ROSE_ABORT();
    }
    ROSE_ASSERT(copiedName->get_declptr() == resolvedDeclaration);
  }

  SgVariableDefinition *originalDefinition = originalName->get_definition();
  SgVariableDefinition *copiedDefinition = copiedName->get_definition();
  if ((originalDefinition == NULL) != (copiedDefinition == NULL)) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[initialized-name-definition-null]: "
        "source=%p/%s definition=%p copy=%p definition=%p\n",
        static_cast<const void *>(originalName), originalName->get_name().str(),
        static_cast<void *>(originalDefinition),
        static_cast<void *>(copiedName), static_cast<void *>(copiedDefinition));
    ROSE_ABORT();
  }
  if (originalDefinition != NULL) {
    SgVariableDefinition *mappedDefinition = isSgVariableDefinition(
        requireExactMappedNode(originalDefinition, help,
                               "initialized-name-owned-variable-definition"));
    SgExpression *originalBitfield = originalDefinition->get_bitfield();
    SgExpression *copiedBitfield = copiedDefinition->get_bitfield();
    if (mappedDefinition != copiedDefinition ||
        copiedDefinition == originalDefinition ||
        originalDefinition->get_vardefn() != originalName ||
        originalDefinition->get_parent() != originalName ||
        copiedDefinition->get_vardefn() != copiedName ||
        copiedDefinition->get_parent() != copiedName ||
        (originalBitfield == NULL) != (copiedBitfield == NULL) ||
        (originalBitfield != NULL &&
         (lookupCopiedNode(help, originalBitfield) != copiedBitfield ||
          copiedBitfield == originalBitfield ||
          copiedBitfield->get_parent() != copiedDefinition))) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[initialized-name-variable-definition]: "
              "source-name=%p definition=%p vardefn=%p parent=%p bitfield=%p "
              "copy-name=%p definition=%p vardefn=%p parent=%p bitfield=%p "
              "mapped-bitfield=%p\n",
              static_cast<const void *>(originalName),
              static_cast<void *>(originalDefinition),
              static_cast<void *>(originalDefinition->get_vardefn()),
              static_cast<void *>(originalDefinition->get_parent()),
              static_cast<void *>(originalBitfield),
              static_cast<void *>(copiedName),
              static_cast<void *>(copiedDefinition),
              static_cast<void *>(copiedDefinition != NULL
                                      ? copiedDefinition->get_vardefn()
                                      : NULL),
              static_cast<void *>(copiedDefinition != NULL
                                      ? copiedDefinition->get_parent()
                                      : NULL),
              static_cast<void *>(copiedBitfield),
              static_cast<void *>(lookupCopiedNode(help, originalBitfield)));
      ROSE_ABORT();
    }
  }
}

void finalizeExactCopiedSymbols(SgCopyHelp &help) {
  std::vector<std::pair<const SgScopeStatement *, SgScopeStatement *>>
      copiedScopes;
  for (const auto &mapping : help.get_copiedNodeMap()) {
    const SgScopeStatement *originalScope = isSgScopeStatement(mapping.first);
    SgScopeStatement *copiedScope = isSgScopeStatement(mapping.second);
    if (originalScope != NULL && copiedScope != NULL &&
        originalScope != copiedScope) {
      copiedScopes.emplace_back(originalScope, copiedScope);
    }
  }

  // Legacy recursive symbol fixup follows statement lists and therefore
  // misses scopes owned through expression edges, notably a lambda's closure
  // class and call operator.  Complete the copy transaction from the exact
  // scope map instead of inferring reachability from one particular AST view.
  for (const auto &scopePair : copiedScopes) {
    const SgScopeStatement *originalScope = scopePair.first;
    SgScopeStatement *copiedScope = scopePair.second;
    const size_t originalSymbolCount =
        const_cast<SgScopeStatement *>(originalScope)->symbol_table_size();
    const size_t copiedSymbolCount = copiedScope->symbol_table_size();
    if (copiedSymbolCount == 0) {
      SageInterface::fixupReferencesToSymbols(originalScope, copiedScope, help);
    } else if (copiedSymbolCount != originalSymbolCount) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[symbol-table-finalization]: source=%p/%s "
              "symbols=%zu copy=%p/%s symbols=%zu\n",
              static_cast<const void *>(originalScope),
              originalScope->class_name().c_str(), originalSymbolCount,
              static_cast<void *>(copiedScope),
              copiedScope->class_name().c_str(), copiedSymbolCount);
      ROSE_ABORT();
    }
  }

  std::unordered_map<SgNode *, SgNode *> symbolReplacements;
  for (const auto &mapping : help.get_copiedNodeMap()) {
    SgSymbol *originalSymbol = isSgSymbol(const_cast<SgNode *>(mapping.first));
    SgSymbol *copiedSymbol = isSgSymbol(mapping.second);
    if (originalSymbol == NULL) {
      continue;
    }
    if (copiedSymbol == NULL ||
        copiedSymbol->variantT() != originalSymbol->variantT()) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[symbol-edge-finalization-kind]: "
              "source=%p/%s copy=%p/%s\n",
              static_cast<void *>(originalSymbol),
              originalSymbol->class_name().c_str(),
              static_cast<void *>(copiedSymbol),
              copiedSymbol != NULL ? copiedSymbol->class_name().c_str()
                                   : "<null>");
      ROSE_ABORT();
    }
    if (originalSymbol != copiedSymbol) {
      const bool inserted =
          symbolReplacements.emplace(originalSymbol, copiedSymbol).second;
      if (!inserted) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[symbol-edge-finalization-map]: "
                "source=%p/%s has multiple copied symbol identities\n",
                static_cast<void *>(originalSymbol),
                originalSymbol->class_name().c_str());
        ROSE_ABORT();
      }
    }
  }

  class ReplaceMappedSymbolEdges : public SimpleReferenceToPointerHandler {
    const std::unordered_map<SgNode *, SgNode *> &replacements_;

  public:
    explicit ReplaceMappedSymbolEdges(
        const std::unordered_map<SgNode *, SgNode *> &replacements)
        : replacements_(replacements) {}

    void operator()(SgNode *&edge, const SgName &, bool) override {
      const auto replacement = replacements_.find(edge);
      if (replacement != replacements_.end()) {
        edge = replacement->second;
      }
    }
  };

  ReplaceMappedSymbolEdges replace(symbolReplacements);
  std::unordered_set<SgNode *> rewrittenCopies;
  for (const auto &mapping : help.get_copiedNodeMap()) {
    SgNode *copiedNode = mapping.second;
    if (mapping.first == copiedNode ||
        !rewrittenCopies.insert(copiedNode).second) {
      continue;
    }
    copiedNode->processDataMemberReferenceToPointers(&replace);
  }

  for (SgNode *copiedNode : rewrittenCopies) {
    for (const auto &edge : copiedNode->returnDataMemberPointers()) {
      const auto stale = symbolReplacements.find(edge.first);
      if (stale != symbolReplacements.end()) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[symbol-edge-finalization]: copy=%p/%s "
                "edge=%s retains source symbol=%p/%s expected=%p/%s\n",
                static_cast<void *>(copiedNode),
                copiedNode->class_name().c_str(), edge.second.c_str(),
                static_cast<void *>(edge.first),
                edge.first->class_name().c_str(),
                static_cast<void *>(stale->second),
                stale->second->class_name().c_str());
        ROSE_ABORT();
      }
    }
  }
}

class CopiedSignatureDependencies : public SimpleReferenceToPointerHandler {
  SgCopyHelp &help_;
  std::unordered_set<const SgNode *> visited_;

public:
  bool changed = false;
  std::unordered_set<const SgDeclarationStatement *> declarations;

  explicit CopiedSignatureDependencies(SgCopyHelp &help) : help_(help) {}

  void visit(SgNode *node) {
    if (node == nullptr || !visited_.insert(node).second)
      return;
    if (auto *symbol = isSgSymbol(node)) {
      visit(symbol->get_symbol_basis());
      return;
    }
    auto *copy = lookupCopiedNode(help_, node);
    if (copy != nullptr && (isSgType(node) != nullptr ||
                            isSgDeclarationStatement(node) != nullptr ||
                            isSgInitializedName(node) != nullptr)) {
      if (copy != node) {
        auto *sourceClass = isSgClassDeclaration(node);
        auto *copiedClass = isSgClassDeclaration(copy);
        auto *sourceEnum = isSgEnumDeclaration(node);
        auto *copiedEnum = isSgEnumDeclaration(copy);
        if ((sourceClass != nullptr && copiedClass != nullptr &&
             copiedClassDeclarationsHaveExactSharedProjectIdentity(
                 help_, sourceClass, copiedClass)) ||
            (sourceEnum != nullptr && copiedEnum != nullptr &&
             copiedEnumDeclarationsHaveExactSharedProjectIdentity(
                 help_, sourceEnum, copiedEnum)))
          return;
        changed = true;
        return;
      }
    }
    if (auto *named = isSgNamedType(node)) {
      visit(named->get_declaration());
    }
    if (auto *declaration = isSgDeclarationStatement(node)) {
      declarations.insert(declaration);
    }
    const SgTemplateArgumentPtrList *arguments = nullptr;
    if (auto *instance = isSgTemplateInstantiationDecl(node)) {
      arguments = &instance->get_semanticTemplateArguments();
    } else if (auto *instance = isSgTemplateInstantiationFunctionDecl(node)) {
      arguments = &instance->get_deducedTemplateArguments();
    } else if (auto *instance =
                   isSgTemplateInstantiationMemberFunctionDecl(node)) {
      arguments = &instance->get_deducedTemplateArguments();
    }
    if (arguments != nullptr) {
      for (auto *argument : *arguments) {
        ASSERT_not_null(argument);
        visit(argument);
      }
    }
    if (auto *function = isSgFunctionDeclaration(node)) {
      ASSERT_not_null(function->get_type());
      visit(function->get_type());
      return;
    }
    if (isSgTemplateInstantiationDecl(node) != nullptr)
      return;
    node->processDataMemberReferenceToPointers(this);
  }

  void operator()(SgNode *&edge, const SgName &field, bool) override {
    if (edge == nullptr || isTypeReverseCacheField(field) ||
        isTypeReverseOwnershipField(field))
      return;
    // Declaration bodies and enclosing scopes do not determine a signature.
    // Template argument expressions do: retain their typed references as well
    // as the forward type graph, including nested class specializations.
    if (isSgType(edge) != nullptr || isSgTemplateArgument(edge) != nullptr ||
        isSgFunctionParameterTypeList(edge) != nullptr ||
        isSgExpression(edge) != nullptr || isSgSymbol(edge) != nullptr)
      visit(edge);
  }
};

using SemanticCopyRoots = std::vector<
    std::pair<const SgDeclarationStatement *, SgDeclarationStatement *>>;

SemanticCopyRoots constructCopiedSignatureDependencies(SgCopyHelp &help) {
  SemanticCopyRoots roots;
  std::unordered_set<const SgNode *> examined;
  std::unordered_set<const SgDeclarationStatement *> candidates;
  size_t previousSize;
  do {
    previousSize = help.get_copiedNodeMap().size();
    for (const auto &mapping : help.get_copiedNodeMap()) {
      if (mapping.first == mapping.second ||
          !examined.insert(mapping.first).second)
        continue;
      for (const auto &edge : mapping.first->returnDataMemberPointers()) {
        if (isTypeReverseOwnershipField(SgName(edge.second)))
          continue;
        SgNode *node = edge.first;
        if (auto *symbol = isSgSymbol(node))
          node = symbol->get_symbol_basis();
        if (auto *declaration = isSgDeclarationStatement(node))
          candidates.insert(declaration);
        if (isSgType(node) != nullptr) {
          CopiedSignatureDependencies dependencies(help);
          dependencies.visit(node);
          candidates.insert(dependencies.declarations.begin(),
                            dependencies.declarations.end());
        }
      }
    }
    std::vector<const SgDeclarationStatement *> pending(candidates.begin(),
                                                        candidates.end());
    for (size_t index = 0; index < pending.size(); ++index) {
      CopiedSignatureDependencies dependencies(help);
      dependencies.visit(const_cast<SgDeclarationStatement *>(pending[index]));
      for (const auto *dependency : dependencies.declarations) {
        if (candidates.insert(dependency).second)
          pending.push_back(dependency);
      }
    }
    std::stable_partition(pending.begin(), pending.end(), [](const auto *node) {
      return isSgTemplateInstantiationDecl(node) != nullptr;
    });
    for (const auto *candidate : pending) {
      if (lookupCopiedNode(help, candidate) != nullptr ||
          (isSgFunctionDeclaration(candidate) == nullptr &&
           isSgTemplateInstantiationDecl(candidate) == nullptr))
        continue;
      CopiedSignatureDependencies dependencies(help);
      dependencies.visit(const_cast<SgDeclarationStatement *>(candidate));
      if (!dependencies.changed)
        continue;

      const auto *first = candidate->get_firstNondefiningDeclaration();
      const auto *defining = candidate->get_definingDeclaration();
      ASSERT_not_null(first);
      ROSE_ASSERT(first->get_firstNondefiningDeclaration() == first);
      ROSE_ASSERT(first->get_definingDeclaration() == defining);
      SgCopyHelp::ConstructionScope family(help, defining);
      for (const auto *source : {first, defining, candidate}) {
        if (source == nullptr || lookupCopiedNode(help, source) != nullptr)
          continue;
        auto *copy =
            isSgDeclarationStatement(copyNodeIntoCurrentRoot(help, source));
        ASSERT_not_null(copy);
        ROSE_ASSERT(copy != source && copy->get_parent() == nullptr);
        roots.emplace_back(source, copy);
      }
      auto *symbol = first->get_symbol_from_symbol_table();
      ASSERT_not_null(symbol);
      ROSE_ASSERT(symbol->get_symbol_basis() == first);
      auto *copiedSymbol = isSgSymbol(copyNodeIntoCurrentRoot(help, symbol));
      ASSERT_not_null(copiedSymbol);
      ROSE_ASSERT(copiedSymbol != symbol);
    }
  } while (help.get_copiedNodeMap().size() != previousSize);

  if (roots.empty())
    return roots;
  publishExactCopiedTemplateSemanticSurfaces(help);
  for (const auto &root : roots)
    root.first->fixupCopy_scopes(root.second, help);
  for (const auto &root : roots)
    root.first->fixupCopy_symbols(root.second, help);
  for (const auto &root : roots)
    root.first->fixupCopy_references(root.second, help);
  return roots;
}

void publishCopiedSignatureDependencies(const SemanticCopyRoots &roots,
                                        SgCopyHelp &help) {
  for (const auto &root : roots) {
    auto *copy = root.second;
    auto *pending = isSgAuxiliaryDeclarationList(copy->get_parent());
    if (pending != nullptr && pending->get_parent() == nullptr) {
      ROSE_ASSERT(pending->get_declarations().size() == 1);
      ROSE_ASSERT(pending->get_declarations().front() == copy);
      pending->get_declarations().clear();
      copy->set_parent(nullptr);
      delete pending;
    }
    ASSERT_not_null(copy->get_scope());
    if (copy->get_parent() == nullptr ||
        copy->get_parent() == copy->get_scope()->get_auxiliary_declarations()) {
      SageBuilder::attachAuxiliaryDeclaration(copy->get_scope(), copy);
    } else {
      ROSE_ASSERT(copy->get_parent() ==
                  lookupCopiedNode(help, root.first->get_parent()));
      ROSE_ASSERT(countDirectOwnerEdges(copy->get_parent(), copy) == 1);
    }
    if (root.first->get_firstNondefiningDeclaration() == root.first) {
      auto *originalSymbol = root.first->get_symbol_from_symbol_table();
      auto *copiedSymbol = isSgSymbol(lookupCopiedNode(help, originalSymbol));
      ASSERT_not_null(copiedSymbol);
      ROSE_ASSERT(copiedSymbol->get_symbol_basis() == copy);
      if (copiedSymbol->get_parent() == nullptr) {
        copy->get_scope()->insert_symbol(copiedSymbol->get_name(),
                                         copiedSymbol);
      } else {
        ROSE_ASSERT(copiedSymbol->get_parent() ==
                    copy->get_scope()->get_symbol_table());
        ROSE_ASSERT(copy->get_scope()->symbol_exists(copiedSymbol));
      }
    }
  }
}

void finalizeCanonicalCopyLinks(SgCopyHelp &help) {
  const auto semanticRoots = constructCopiedSignatureDependencies(help);
  validateExactCopiedNodeMap(help);

  auto derivedScopePathEndsAtDetachedCopyRoot = [](SgDeclarationStatement
                                                       *declaration) {
    ASSERT_not_null(declaration);
    if (declaration->hasExplicitScope()) {
      return false;
    }

    std::set<SgNode *> visited;
    SgStatement *statement = declaration;
    while (true) {
      if (!visited.insert(statement).second) {
        fprintf(
            stderr,
            "REX_COPY_INVARIANT[derived-scope-path]: declaration=%p/"
            "%s has a structural parent cycle at statement=%p/%s\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            static_cast<void *>(statement), statement->class_name().c_str());
        ROSE_ABORT();
      }
      if (statement != declaration && statement->hasExplicitScope()) {
        return false;
      }

      SgNode *parent = statement->get_parent();
      if (parent == NULL) {
        return true;
      }
      if (isSgScopeStatement(parent) != NULL) {
        return false;
      }
      if (SgStatement *parentStatement = isSgStatement(parent)) {
        statement = parentStatement;
        continue;
      }
      if (SgInitializedName *initializedName = isSgInitializedName(parent)) {
        SgStatement *initializedNameOwner =
            isSgStatement(initializedName->get_parent());
        if (initializedNameOwner == NULL) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[derived-scope-path]: declaration=%p/"
                  "%s reaches initialized-name=%p without an exact "
                  "statement owner\n",
                  static_cast<void *>(declaration),
                  declaration->class_name().c_str(),
                  static_cast<void *>(initializedName));
          ROSE_ABORT();
        }
        statement = initializedNameOwner;
        continue;
      }
      if (SgAuxiliaryDeclarationList *auxiliary =
              isSgAuxiliaryDeclarationList(parent)) {
        SgDeclarationStatement *ownedDeclaration =
            isSgDeclarationStatement(statement);
        if (ownedDeclaration == NULL ||
            std::count(auxiliary->get_declarations().begin(),
                       auxiliary->get_declarations().end(),
                       ownedDeclaration) != 1) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[derived-scope-path]: "
                  "declaration=%p/%s reaches auxiliary owner=%p without "
                  "one exact declaration edge\n",
                  static_cast<void *>(declaration),
                  declaration->class_name().c_str(),
                  static_cast<void *>(auxiliary));
          ROSE_ABORT();
        }

        SgNode *auxiliaryParent = auxiliary->get_parent();
        if (auxiliaryParent == NULL) {
          // The auxiliary container itself is the detached copy root.
          // Its exact declaration edge is already validated above; scope
          // validation resumes when the owning scope adopts the root.
          return true;
        }
        SgScopeStatement *auxiliaryScope = isSgScopeStatement(auxiliaryParent);
        if (auxiliaryScope == NULL ||
            auxiliaryScope->get_auxiliary_declarations() != auxiliary) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[derived-scope-path]: "
                  "declaration=%p/%s auxiliary owner=%p has malformed "
                  "scope owner=%p/%s\n",
                  static_cast<void *>(declaration),
                  declaration->class_name().c_str(),
                  static_cast<void *>(auxiliary),
                  static_cast<void *>(auxiliaryParent),
                  auxiliaryParent->class_name().c_str());
          ROSE_ABORT();
        }
        return false;
      }
      if (auto *prototype = isSgFunctionParameterTypeList(parent)) {
        auto *parameters = isSgFunctionParameterList(statement);
        auto *scope = prototype->get_source_parameter_scope();
        ROSE_ASSERT(parameters != nullptr &&
                    prototype->get_source_parameters() == parameters &&
                    scope != nullptr && scope->get_parent() == prototype);
        return false;
      }
      if (SgRequiresExpr *requiresExpression = isSgRequiresExpr(parent)) {
        SgFunctionParameterList *parameterList =
            isSgFunctionParameterList(statement);
        if (parameterList == NULL ||
            (requiresExpression->get_local_parameter_list() != parameterList &&
             requiresExpression->get_local_parameter_list_syntax() !=
                 parameterList) ||
            countDirectOwnerEdges(requiresExpression, parameterList) != 1) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[derived-scope-path]: "
                  "declaration=%p/%s reaches requires-expression=%p "
                  "without one exact local-parameter-list edge\n",
                  static_cast<void *>(declaration),
                  declaration->class_name().c_str(),
                  static_cast<void *>(requiresExpression));
          ROSE_ABORT();
        }

        SgNode *expressionNode = requiresExpression;
        while (true) {
          SgNode *expressionParent = expressionNode->get_parent();
          if (expressionParent == NULL) {
            return true;
          }
          if (isSgScopeStatement(expressionParent) != NULL) {
            return false;
          }
          if (SgStatement *expressionOwner = isSgStatement(expressionParent)) {
            if (countDirectOwnerEdges(expressionOwner, expressionNode) != 1) {
              fprintf(stderr,
                      "REX_COPY_INVARIANT[derived-scope-path]: "
                      "declaration=%p/%s expression=%p/%s reaches "
                      "statement=%p/%s without one exact structural edge\n",
                      static_cast<void *>(declaration),
                      declaration->class_name().c_str(),
                      static_cast<void *>(expressionNode),
                      expressionNode->class_name().c_str(),
                      static_cast<void *>(expressionOwner),
                      expressionOwner->class_name().c_str());
              ROSE_ABORT();
            }
            statement = expressionOwner;
            break;
          }
          if (auto *name = isSgInitializedName(expressionParent)) {
            ROSE_ASSERT(name->get_initializer() == expressionNode &&
                        countDirectOwnerEdges(name, expressionNode) == 1);
            expressionNode = name;
            continue;
          }
          SgExpression *parentExpression = isSgExpression(expressionParent);
          if (parentExpression == NULL ||
              countDirectOwnerEdges(parentExpression, expressionNode) != 1) {
            fprintf(stderr,
                    "REX_COPY_INVARIANT[derived-scope-path]: "
                    "declaration=%p/%s expression=%p/%s has unsupported "
                    "owner=%p/%s while deriving its requires-parameter "
                    "scope\n",
                    static_cast<void *>(declaration),
                    declaration->class_name().c_str(),
                    static_cast<void *>(expressionNode),
                    expressionNode->class_name().c_str(),
                    static_cast<void *>(expressionParent),
                    expressionParent->class_name().c_str());
            ROSE_ABORT();
          }
          expressionNode = parentExpression;
        }
        continue;
      }

      fprintf(stderr,
              "REX_COPY_INVARIANT[derived-scope-path]: declaration=%p/%s "
              "has unsupported non-statement structural owner=%p/%s\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str(), static_cast<void *>(parent),
              parent->class_name().c_str());
      ROSE_ABORT();
    }
  };

  std::vector<std::pair<const SgNode *, SgNode *>> copiedNodePairs;
  copiedNodePairs.reserve(help.get_copiedNodeMap().size());
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    copiedNodePairs.push_back(std::make_pair(it->first, it->second));
  }

  // Derived declaration scopes (for example SgCtorInitializerList) delegate
  // through their structural owner.  Pointer-order iteration can encounter
  // such a child before its owning function declaration, so canonicalize
  // every explicit declaration-scope edge first.  This is one phase of the
  // copy transaction, not a scope-inference repair: the source edge and the
  // exact copied-node map must identify the sole destination.
  for (const std::pair<const SgNode *, SgNode *> &pair : copiedNodePairs) {
    const SgDeclarationStatement *originalDeclaration =
        isSgDeclarationStatement(pair.first);
    SgDeclarationStatement *copiedDeclaration =
        isSgDeclarationStatement(pair.second);
    if (originalDeclaration == nullptr || copiedDeclaration == nullptr ||
        originalDeclaration == copiedDeclaration ||
        !originalDeclaration->hasExplicitScope()) {
      continue;
    }

    SgScopeStatement *originalScope = originalDeclaration->get_scope();
    SgScopeStatement *provisionalScope = copiedDeclaration->get_scope();
    const auto scopeMapping = help.get_copiedNodeMap().find(originalScope);
    SgNode *plannedScope = help.plannedExternalSemanticCopyEdge(originalScope);
    if (originalScope != nullptr && provisionalScope != originalScope &&
        (scopeMapping == help.get_copiedNodeMap().end() ||
         scopeMapping->second == originalScope) &&
        plannedScope == nullptr) {
      Sg_File_Info *originalPosition =
          originalDeclaration->get_startOfConstruct();
      Sg_File_Info *copiedPosition = copiedDeclaration->get_startOfConstruct();
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[explicit-declaration-scope-plan]: "
          "original=%p/%s parent=%p first=%p defining=%p "
          "file=%d physical=%d scope=%p/%s copy=%p/%s parent=%p "
          "first=%p defining=%p file=%d physical=%d provisional=%p/%s "
          "has no exact external scope plan\n",
          static_cast<const void *>(originalDeclaration),
          originalDeclaration->class_name().c_str(),
          static_cast<const void *>(originalDeclaration->get_parent()),
          static_cast<const void *>(
              originalDeclaration->get_firstNondefiningDeclaration()),
          static_cast<const void *>(
              originalDeclaration->get_definingDeclaration()),
          originalPosition != nullptr ? originalPosition->get_file_id() : -1,
          originalPosition != nullptr ? originalPosition->get_physical_file_id()
                                      : -1,
          static_cast<void *>(originalScope),
          originalScope->class_name().c_str(),
          static_cast<void *>(copiedDeclaration),
          copiedDeclaration->class_name().c_str(),
          static_cast<void *>(copiedDeclaration->get_parent()),
          static_cast<void *>(
              copiedDeclaration->get_firstNondefiningDeclaration()),
          static_cast<void *>(copiedDeclaration->get_definingDeclaration()),
          copiedPosition != nullptr ? copiedPosition->get_file_id() : -1,
          copiedPosition != nullptr ? copiedPosition->get_physical_file_id()
                                    : -1,
          static_cast<void *>(provisionalScope),
          provisionalScope != nullptr ? provisionalScope->class_name().c_str()
                                      : "<null>");
    }
    SgScopeStatement *copiedScope =
        isSgScopeStatement(remapExactCopiedOrExternalSemanticEdge(
            originalScope, provisionalScope, help,
            "explicit-declaration-scope-finalization"));
    if (originalScope == nullptr || copiedScope == nullptr ||
        (copiedScope->variantT() != originalScope->variantT() &&
         copiedScope != plannedScope)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[explicit-declaration-scope-finalization]: "
              "original=%p/%s scope=%p/%s copy=%p/%s provisional=%p/%s "
              "resolved=%p/%s\n",
              static_cast<const void *>(originalDeclaration),
              originalDeclaration->class_name().c_str(),
              static_cast<void *>(originalScope),
              originalScope != nullptr ? originalScope->class_name().c_str()
                                       : "<null>",
              static_cast<void *>(copiedDeclaration),
              copiedDeclaration->class_name().c_str(),
              static_cast<void *>(provisionalScope),
              provisionalScope != nullptr
                  ? provisionalScope->class_name().c_str()
                  : "<null>",
              static_cast<void *>(copiedScope),
              copiedScope != nullptr ? copiedScope->class_name().c_str()
                                     : "<null>");
      ROSE_ABORT();
    }
    copiedDeclaration->set_scope(copiedScope);
  }

  for (const std::pair<const SgNode *, SgNode *> &pair : copiedNodePairs) {
    const SgNode *originalNode = pair.first;
    SgNode *copyNode = pair.second;
    ASSERT_not_null(originalNode);
    ASSERT_not_null(copyNode);

    // Types and declarations outside the structural copy transaction may be
    // recorded as explicitly shared identity mappings. They are not copied
    // nodes and must never be rewritten by copy finalization.
    if (originalNode == copyNode) {
      continue;
    }

    if (const SgInitializedName *originalName =
            isSgInitializedName(originalNode)) {
      SgInitializedName *copiedName = isSgInitializedName(copyNode);
      if (copiedName == NULL) {
        ROSE_ABORT();
      }
      validateExactCopiedInitializedNameSemanticEdges(originalName, copiedName,
                                                      help);
    }

    if (const SgOmpExecStatement *originalOmp =
            isSgOmpExecStatement(originalNode)) {
      SgOmpExecStatement *copiedOmp = isSgOmpExecStatement(copyNode);
      if (copiedOmp == NULL) {
        ROSE_ABORT();
      }
      fixupOmpExecSemanticEdges(originalOmp, copiedOmp, help, false);
    }

    if (const SgOmpDeclareTargetStatement *originalDeclareTarget =
            isSgOmpDeclareTargetStatement(originalNode)) {
      SgOmpDeclareTargetStatement *copiedDeclareTarget =
          isSgOmpDeclareTargetStatement(copyNode);
      if (copiedDeclareTarget == NULL) {
        ROSE_ABORT();
      }
      fixupOmpDeclareTargetSemanticEdges(originalDeclareTarget,
                                         copiedDeclareTarget, help, false);
    }

    if (const SgTemplateArgument *originalArgument =
            isSgTemplateArgument(originalNode)) {
      SgTemplateArgument *copiedArgument = isSgTemplateArgument(copyNode);
      if (copiedArgument == NULL || originalArgument->get_argumentType() !=
                                        copiedArgument->get_argumentType()) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[template-argument-kind]: source=%p "
                "kind=%d copy=%p kind=%d\n",
                static_cast<const void *>(originalArgument),
                static_cast<int>(originalArgument->get_argumentType()),
                static_cast<void *>(copiedArgument),
                copiedArgument != NULL
                    ? static_cast<int>(copiedArgument->get_argumentType())
                    : -1);
        ROSE_ABORT();
      }
      SgDeclarationStatement *originalTemplate =
          originalArgument->get_templateDeclaration();
      SgDeclarationStatement *copiedTemplate =
          copiedArgument->get_templateDeclaration();
      if ((originalTemplate == NULL) != (copiedTemplate == NULL) ||
          (originalTemplate != NULL &&
           requireExactCopiedOrExternalSemanticEdge(
               originalTemplate, copiedTemplate, help,
               "template-argument-declaration") != copiedTemplate)) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[template-argument-declaration]: "
                "source=%p kind=%d declaration=%p copy=%p declaration=%p\n",
                static_cast<const void *>(originalArgument),
                static_cast<int>(originalArgument->get_argumentType()),
                static_cast<void *>(originalTemplate),
                static_cast<void *>(copiedArgument),
                static_cast<void *>(copiedTemplate));
        ROSE_ABORT();
      }
    }

    const SgNode *originalOwner = originalNode->get_parent();
    SgNode *plannedOwner = help.plannedExternalSemanticCopyEdge(originalOwner);
    SgNode *structuralOwner = lookupCopiedNode(help, originalOwner);
    SgNode *copiedOwner =
        structuralOwner != nullptr ? structuralOwner : plannedOwner;
    const bool detachedRoot =
        structuralOwner == nullptr && copyNode->get_parent() == nullptr;
    const size_t originalEdges =
        countDirectOwnerEdges(originalOwner, originalNode);
    if (!detachedRoot && copiedOwner != NULL && originalEdges != 0 &&
        (copyNode->get_parent() != copiedOwner ||
         countDirectOwnerEdges(copiedOwner, copyNode) != originalEdges)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[structural-owner-map]: original=%p/%s "
              "owner=%p/%s edges=%zu copy=%p/%s owner=%p/%s expected=%p/%s "
              "edges=%zu\n",
              static_cast<const void *>(originalNode),
              originalNode->class_name().c_str(),
              static_cast<const void *>(originalOwner),
              originalOwner != nullptr ? originalOwner->class_name().c_str()
                                       : "<null>",
              originalEdges, static_cast<void *>(copyNode),
              copyNode->class_name().c_str(),
              static_cast<void *>(copyNode->get_parent()),
              copyNode->get_parent() != nullptr
                  ? copyNode->get_parent()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(copiedOwner),
              copiedOwner != nullptr ? copiedOwner->class_name().c_str()
                                     : "<null>",
              countDirectOwnerEdges(copiedOwner, copyNode));
      ROSE_ABORT();
    }

    if (const SgDeclarationStatement *originalDecl =
            isSgDeclarationStatement(originalNode)) {
      SgDeclarationStatement *copyDecl = isSgDeclarationStatement(copyNode);
      if (copyDecl == NULL) {
        ROSE_ABORT();
      }
      finalizeExactCopiedDeclarationChainLinks(originalDecl, copyDecl, help);
      finalizeExactCopiedNamespaceScopeDeclarationChains(originalDecl, copyDecl,
                                                         help);

      // A parameter list derives its scope from its owning function instead
      // of storing an independent semantic scope edge.  Calling get_scope()
      // on a deliberately detached parameter-list root is therefore invalid.
      // When the owner participates in the same copy transaction, the generic
      // declaration-scope validation below remains both available and
      // required.
      if (isSgFunctionParameterList(originalDecl) != NULL) {
        const SgNode *originalParameterOwner = originalDecl->get_parent();
        SgNode *copiedParameterOwner =
            lookupCopiedNode(help, originalParameterOwner);
        const bool ownerWasStructurallyCopied =
            copiedParameterOwner != NULL &&
            copiedParameterOwner != originalParameterOwner;
        if (!ownerWasStructurallyCopied) {
          if (copyDecl->get_parent() != NULL ||
              copyDecl->get_definingDeclaration() != NULL ||
              copyDecl->get_firstNondefiningDeclaration() != copyDecl) {
            fprintf(stderr,
                    "REX_COPY_INVARIANT[detached-function-parameter-list]: "
                    "original=%p owner=%p mapped-owner=%p copy=%p owner=%p "
                    "defining=%p first=%p\n",
                    static_cast<const void *>(originalDecl),
                    static_cast<const void *>(originalParameterOwner),
                    static_cast<void *>(copiedParameterOwner),
                    static_cast<void *>(copyDecl),
                    static_cast<void *>(copyDecl->get_parent()),
                    static_cast<void *>(copyDecl->get_definingDeclaration()),
                    static_cast<void *>(
                        copyDecl->get_firstNondefiningDeclaration()));
            ROSE_ABORT();
          }
          continue;
        }
      }

      const SgNode *originalDeclarationOwner = originalDecl->get_parent();
      SgNode *copiedDeclarationOwner =
          lookupCopiedNode(help, originalDeclarationOwner);
      const bool declarationOwnerWasStructurallyCopied =
          copiedDeclarationOwner != NULL &&
          copiedDeclarationOwner != originalDeclarationOwner;
      if (!copyDecl->hasExplicitScope() &&
          derivedScopePathEndsAtDetachedCopyRoot(copyDecl)) {
        if (copyDecl->get_parent() == NULL &&
            declarationOwnerWasStructurallyCopied) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[detached-declaration-owner]: "
                  "original=%p/%s owner=%p mapped-owner=%p copy=%p lost its "
                  "copied structural owner\n",
                  static_cast<const void *>(originalDecl),
                  originalDecl->class_name().c_str(),
                  static_cast<const void *>(originalDeclarationOwner),
                  static_cast<void *>(copiedDeclarationOwner),
                  static_cast<void *>(copyDecl));
          ROSE_ABORT();
        }
        // This declaration is either the copy root or is structurally nested
        // below that root.  Its scope path ends at an owner intentionally left
        // outside this copy transaction, so asking get_scope() before the root
        // is published is invalid.  Every direct copied-owner edge was already
        // validated above; exact scope validation resumes after attachment.
        continue;
      }

      SgScopeStatement *originalScope = originalDecl->get_scope();
      SgScopeStatement *mappedOriginalScope =
          isSgScopeStatement(lookupCopiedNode(help, originalScope));
      if (originalScope != NULL && mappedOriginalScope != NULL &&
          mappedOriginalScope != originalScope &&
          copyDecl->get_scope() != mappedOriginalScope) {
        fprintf(
            stderr,
            "REX_COPY_INVARIANT[declaration-scope-before-validation]: "
            "original=%p/%s name=%s parent=%p/%s scope=%p copy=%p/%s "
            "parent=%p/%s scope=%p expected=%p first=%p defining=%p\n",
            static_cast<const void *>(originalDecl),
            originalDecl->class_name().c_str(),
            SageInterface::get_name(originalDecl).c_str(),
            static_cast<void *>(originalDecl->get_parent()),
            originalDecl->get_parent() != NULL
                ? originalDecl->get_parent()->class_name().c_str()
                : "<null>",
            static_cast<void *>(originalScope), static_cast<void *>(copyDecl),
            copyDecl->class_name().c_str(),
            static_cast<void *>(copyDecl->get_parent()),
            copyDecl->get_parent() != NULL
                ? copyDecl->get_parent()->class_name().c_str()
                : "<null>",
            static_cast<void *>(copyDecl->get_scope()),
            static_cast<void *>(mappedOriginalScope),
            static_cast<void *>(copyDecl->get_firstNondefiningDeclaration()),
            static_cast<void *>(copyDecl->get_definingDeclaration()));
        ROSE_ABORT();
      }
      SgScopeStatement *expectedCopyScope =
          originalScope != NULL
              ? isSgScopeStatement(requireExactCopiedOrExternalSemanticEdge(
                    originalScope, copyDecl->get_scope(), help,
                    "declaration-scope"))
              : NULL;
      if (copyDecl->get_scope() != expectedCopyScope) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[declaration-scope-map]: original=%p/%s "
                "scope=%p copy=%p scope=%p expected=%p\n",
                static_cast<const void *>(originalDecl),
                originalDecl->class_name().c_str(),
                static_cast<void *>(originalScope),
                static_cast<void *>(copyDecl),
                static_cast<void *>(copyDecl->get_scope()),
                static_cast<void *>(expectedCopyScope));
        ROSE_ABORT();
      }
    }

    if (const SgClassDefinition *originalClassDef =
            isSgClassDefinition(originalNode)) {
      SgClassDefinition *copyClassDef = isSgClassDefinition(copyNode);
      SgClassDeclaration *copyClassDecl = isSgClassDeclaration(
          requireExactMappedNode(originalClassDef->get_declaration(), help,
                                 "class-definition-declaration"));
      if (copyClassDef == NULL || copyClassDecl == NULL) {
        ROSE_ABORT();
      }
      copyClassDef->set_declaration(copyClassDecl);
    }

    if (const SgNamespaceDefinitionStatement *originalNamespaceDef =
            isSgNamespaceDefinitionStatement(originalNode)) {
      SgNamespaceDefinitionStatement *copyNamespaceDef =
          isSgNamespaceDefinitionStatement(copyNode);
      SgNamespaceDeclarationStatement *copyNamespaceDecl =
          isSgNamespaceDeclarationStatement(requireExactMappedNode(
              originalNamespaceDef->get_namespaceDeclaration(), help,
              "namespace-definition-declaration"));
      if (copyNamespaceDef == NULL || copyNamespaceDecl == NULL) {
        ROSE_ABORT();
      }
      copyNamespaceDef->set_namespaceDeclaration(copyNamespaceDecl);
    }

    if (const SgTemplateClassDeclaration *originalTemplate =
            isSgTemplateClassDeclaration(originalNode)) {
      SgTemplateClassDeclaration *copiedTemplate =
          isSgTemplateClassDeclaration(copyNode);
      SgTemplateClassDeclaration *originalSpecializedTemplate =
          originalTemplate->get_specializedTemplateDeclaration();
      if (copiedTemplate == NULL) {
        ROSE_ABORT();
      }
      if (originalSpecializedTemplate != NULL) {
        SgTemplateClassDeclaration *copiedSpecializedTemplate =
            isSgTemplateClassDeclaration(remapExactCopiedOrExternalSemanticEdge(
                originalSpecializedTemplate,
                copiedTemplate->get_specializedTemplateDeclaration(), help,
                "class-partial-specialization-primary"));
        if (copiedSpecializedTemplate == NULL) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[class-partial-specialization-primary]: "
                  "source=%p primary=%p copy=%p acquired a wrong-kind primary "
                  "edge\n",
                  static_cast<const void *>(originalTemplate),
                  static_cast<void *>(originalSpecializedTemplate),
                  static_cast<void *>(copiedTemplate));
          ROSE_ABORT();
        }
        copiedTemplate->set_specializedTemplateDeclaration(
            copiedSpecializedTemplate);
        requireExactCopiedOrExternalSemanticEdge(
            originalSpecializedTemplate, copiedSpecializedTemplate, help,
            "class-partial-specialization-primary");
      } else if (copiedTemplate->get_specializedTemplateDeclaration() != NULL) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[class-partial-specialization-primary]: "
                "source=%p has no primary edge but copy=%p links primary=%p\n",
                static_cast<const void *>(originalTemplate),
                static_cast<void *>(copiedTemplate),
                static_cast<void *>(
                    copiedTemplate->get_specializedTemplateDeclaration()));
        ROSE_ABORT();
      }
    }

    if (const SgTemplateInstantiationDecl *originalTemplateInst =
            isSgTemplateInstantiationDecl(originalNode)) {
      SgTemplateInstantiationDecl *copyTemplateInst =
          isSgTemplateInstantiationDecl(copyNode);
      SgTemplateClassDeclaration *originalTemplateDecl =
          originalTemplateInst->get_templateDeclaration();
      SgTemplateClassDeclaration *copyTemplateDecl =
          isSgTemplateClassDeclaration(remapExactCopiedOrExternalSemanticEdge(
              originalTemplateDecl,
              copyTemplateInst != NULL
                  ? copyTemplateInst->get_templateDeclaration()
                  : NULL,
              help, "class-template-declaration"));
      if (copyTemplateInst == NULL || originalTemplateDecl == NULL ||
          copyTemplateDecl == NULL) {
        ROSE_ABORT();
      }
      copyTemplateInst->set_templateDeclaration(copyTemplateDecl);
      requireExactCopiedOrExternalSemanticEdge(originalTemplateDecl,
                                               copyTemplateDecl, help,
                                               "class-template-declaration");
    }

    if (const SgTemplateInstantiationFunctionDecl *originalTemplateInst =
            isSgTemplateInstantiationFunctionDecl(originalNode)) {
      SgTemplateInstantiationFunctionDecl *copyTemplateInst =
          isSgTemplateInstantiationFunctionDecl(copyNode);
      if (copyTemplateInst == NULL) {
        ROSE_ABORT();
      }
      SgTemplateFunctionDeclaration *originalTemplateDecl =
          originalTemplateInst->get_templateDeclaration();
      const SgDeclarationStatementPtrList &originalCandidates =
          originalTemplateInst->get_dependentTemplateCandidates();
      if (originalTemplateDecl != NULL) {
        if (!originalCandidates.empty()) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[function-template-identity]: source=%p "
                  "has both primary=%p and %zu dependent candidates\n",
                  static_cast<const void *>(originalTemplateInst),
                  static_cast<void *>(originalTemplateDecl),
                  originalCandidates.size());
          ROSE_ABORT();
        }
        SgTemplateFunctionDeclaration *copyTemplateDecl =
            isSgTemplateFunctionDeclaration(
                remapExactCopiedOrExternalSemanticEdge(
                    originalTemplateDecl,
                    copyTemplateInst->get_templateDeclaration(), help,
                    "function-template-declaration"));
        if (copyTemplateDecl == NULL) {
          ROSE_ABORT();
        }
        copyTemplateInst->set_templateDeclaration(copyTemplateDecl);
      } else {
        if (originalCandidates.size() < 2 ||
            copyTemplateInst->get_templateDeclaration() != NULL ||
            originalTemplateInst->get_specializedTemplateDeclaration() !=
                NULL) {
          fprintf(
              stderr,
              "REX_COPY_INVARIANT[function-template-identity]: source=%p "
              "unresolved template-id has primary=%p specialized=%p "
              "candidates=%zu\n",
              static_cast<const void *>(originalTemplateInst),
              static_cast<void *>(originalTemplateDecl),
              static_cast<void *>(
                  originalTemplateInst->get_specializedTemplateDeclaration()),
              originalCandidates.size());
          ROSE_ABORT();
        }
        SgDeclarationStatementPtrList remappedCandidates;
        for (SgDeclarationStatement *candidate : originalCandidates) {
          SgTemplateFunctionDeclaration *remapped =
              isSgTemplateFunctionDeclaration(
                  remapExactCopiedOrExternalSemanticEdge(
                      candidate, candidate, help,
                      "dependent-function-template-candidate"));
          if (remapped == NULL ||
              std::find(remappedCandidates.begin(), remappedCandidates.end(),
                        remapped) != remappedCandidates.end()) {
            fprintf(stderr,
                    "REX_COPY_INVARIANT[function-template-identity]: "
                    "source=%p candidate=%p did not map to one distinct "
                    "template declaration\n",
                    static_cast<const void *>(originalTemplateInst),
                    static_cast<void *>(candidate));
            ROSE_ABORT();
          }
          remappedCandidates.push_back(remapped);
        }
        copyTemplateInst->get_dependentTemplateCandidates() =
            remappedCandidates;
      }
    }

    if (const SgTemplateInstantiationMemberFunctionDecl *originalTemplateInst =
            isSgTemplateInstantiationMemberFunctionDecl(originalNode)) {
      SgTemplateInstantiationMemberFunctionDecl *copyTemplateInst =
          isSgTemplateInstantiationMemberFunctionDecl(copyNode);
      if (copyTemplateInst == NULL) {
        ROSE_ABORT();
      }
      SgTemplateMemberFunctionDeclaration *originalTemplateDecl =
          originalTemplateInst->get_templateDeclaration();
      if (originalTemplateDecl == NULL) {
        // This node kind also represents a non-template member of a class-
        // template specialization.  That typed role has class-template
        // arguments but deliberately has no function-template declaration.
        if (originalTemplateInst->get_specializedTemplateDeclaration() !=
                NULL ||
            copyTemplateInst->get_templateDeclaration() != NULL ||
            copyTemplateInst->get_specializedTemplateDeclaration() != NULL) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[member-function-template-role]: "
                  "source=%p copy=%p non-template class-specialization member "
                  "acquired a function-template declaration\n",
                  static_cast<const void *>(originalTemplateInst),
                  static_cast<void *>(copyTemplateInst));
          ROSE_ABORT();
        }
      } else {
        SgTemplateMemberFunctionDeclaration *copyTemplateDecl =
            isSgTemplateMemberFunctionDeclaration(
                remapExactCopiedOrExternalSemanticEdge(
                    originalTemplateDecl,
                    copyTemplateInst->get_templateDeclaration(), help,
                    "member-function-template-declaration"));
        if (copyTemplateDecl == NULL) {
          ROSE_ABORT();
        }
        copyTemplateInst->set_templateDeclaration(copyTemplateDecl);
        requireExactCopiedOrExternalSemanticEdge(
            originalTemplateDecl, copyTemplateDecl, help,
            "member-function-template-declaration");
      }
    }

    if (const SgEnumDeclaration *originalEnum =
            isSgEnumDeclaration(originalNode)) {
      SgEnumDeclaration *copyEnum = isSgEnumDeclaration(copyNode);
      if (copyEnum == NULL) {
        ROSE_ABORT();
      }
      finalizeExactCopiedEnumDeclarationChain(originalEnum, copyEnum, help);
    }

    if (const SgScopeStatement *originalScope =
            isSgScopeStatement(originalNode)) {
      SgScopeStatement *copyScope = isSgScopeStatement(copyNode);
      if (copyScope == NULL) {
        ROSE_ABORT();
      }
      validateExactCopiedScopeList(originalScope, copyScope, help);
    }
  }

  finalizeExactCopiedFunctionDeclarationChains(help);
  validateExactCopiedFunctionDefinitions(help);
  materializeExactCopiedNamedTypes(help);
  validateExactCopiedFunctionSyntaxTypes(help);
  finalizeExactCopiedClassTypes(help);
  publishCopiedSignatureDependencies(semanticRoots, help);
  finalizeExactCopiedSymbols(help);
  validateExactCopiedNodeMap(help);

  // A nonreal reference and its synthetic spelling declaration are separate
  // structural subgraphs.  Their owned template arguments and the declaration
  // families referenced by those arguments must all be fixed before comparing
  // the two semantic views.  Validate that cross-graph contract only at the
  // completed copy-transaction boundary.
  for (const auto &mapping : help.get_copiedNodeMap()) {
    if (mapping.first != mapping.second) {
      if (auto *cast = isSgCastExp(mapping.second))
        cast->validate_semantic_conversion();
    }
    const SgNonrealRefExp *originalReference = isSgNonrealRefExp(mapping.first);
    SgNonrealRefExp *copiedReference = isSgNonrealRefExp(mapping.second);
    if (originalReference == NULL || copiedReference == NULL ||
        originalReference == copiedReference) {
      continue;
    }
    if (copiedReference->get_resolved_function_declaration() != NULL) {
      SageInterface::requireResolvedFunctionTemplateReference(
          copiedReference, "finalizeRootCopy");
    }
    if (copiedReference->get_resolved_variable_declaration() != NULL) {
      SageInterface::requireResolvedVariableTemplateReference(
          copiedReference, "finalizeRootCopy");
    }
  }
}

} // namespace

void SgCopyHelp::constructClassTypeCopy(const SgClassDeclaration *declaration) {
  ASSERT_not_null(declaration);
  const auto *canonical =
      isSgClassDeclaration(declaration->get_firstNondefiningDeclaration());
  ASSERT_not_null(canonical);
  auto *copy = isSgClassDeclaration(
      requireExactMappedNode(declaration, *this, "class-type-copy-owner"));
  ASSERT_not_null(copy);
  if (lookupCopiedNode(*this, canonical) == nullptr &&
      hasNonidentityMappedOwner(canonical, *this)) {
    SgNode *copiedCanonical = copyOrLookupOwnedAst(canonical);
    ROSE_ASSERT(copiedCanonical != canonical);
  }
  auto *copiedCanonical =
      isSgClassDeclaration(requireExactCopiedOrExternalSemanticEdge(
          canonical, copy->get_firstNondefiningDeclaration(), *this,
          "class-type-copy-canonical-owner"));
  ASSERT_not_null(copiedCanonical);
  if (copiedCanonical == canonical) {
    ROSE_ASSERT(copy->get_type() == canonical->get_type());
    ROSE_ASSERT(canonical->get_type()->get_declaration() == canonical);
  } else {
    constructCopiedClassType(canonical, copiedCanonical, *this);
    ROSE_ASSERT(get_copiedNodeMap().count(canonical->get_type()) != 0);
  }
  if (const auto *definition = declaration->get_definition()) {
    if (isCopyingOriginal(definition)) {
      const auto copyParameters =
          [&](const SgDeclarationStatementPtrList &declarations) {
            for (auto *member : declarations) {
              if (isSgTemplateFunctionDeclaration(member) == nullptr &&
                  isSgTemplateMemberFunctionDeclaration(member) == nullptr)
                continue;
              for (const auto *parameter :
                   *SageBuilder::getTemplateParameterList(member)) {
                ROSE_ASSERT(parameter != nullptr &&
                            parameter->get_parent() == member);
                SgNode *copiedParameter = copyOrLookupOwnedAst(parameter);
                ROSE_ASSERT(copiedParameter != parameter);
              }
            }
          };
      copyParameters(definition->get_members());
      if (const auto *auxiliary = definition->get_auxiliary_declarations())
        copyParameters(auxiliary->get_declarations());
    }
  }
}

void SgCopyHelp::constructEnumTypeCopy(const SgEnumDeclaration *declaration) {
  ASSERT_not_null(declaration);
  const auto *canonical =
      isSgEnumDeclaration(declaration->get_firstNondefiningDeclaration());
  ASSERT_not_null(canonical);
  auto *copy = isSgEnumDeclaration(
      requireExactMappedNode(declaration, *this, "enum-type-copy-owner"));
  SgNode *constructedCanonical = copyOrLookupOwnedAst(canonical);
  ROSE_ASSERT(constructedCanonical != canonical);
  auto *copiedCanonical = isSgEnumDeclaration(requireExactMappedNode(
      canonical, *this, "enum-type-copy-canonical-owner"));
  ROSE_ASSERT(copy != nullptr && copiedCanonical != nullptr &&
              copy->get_firstNondefiningDeclaration() == copiedCanonical);
  constructCopiedEnumType(canonical, copiedCanonical, *this);
  ROSE_ASSERT(get_copiedNodeMap().count(canonical->get_type()) != 0);
}

void SageInterface::prepareTemplateArgumentCopy(
    const SgTemplateArgument *argument, SgCopyHelp &help) {
  ASSERT_not_null(argument);
  if (argument->get_argumentType() == SgTemplateArgument::type_argument) {
    materializeTypeWithCopyMap(argument->get_type(), help, true);
    if (auto *written = argument->get_sourceSpelledType())
      materializeTypeWithCopyMap(written, help, true);
  } else if (argument->get_argumentType() ==
             SgTemplateArgument::template_template_argument) {
    auto *declaration = argument->get_templateDeclaration();
    ASSERT_not_null(declaration);
    for (SgNode *owner = declaration; owner != nullptr;
         owner = owner->get_parent()) {
      SgNode *mapped = lookupCopiedNode(help, owner);
      if (help.isCopyingOriginal(owner) ||
          (mapped != nullptr && mapped != owner)) {
        ROSE_ASSERT(help.copyOrLookupOwnedAst(declaration) != declaration);
        break;
      }
    }
  }
}

void SageInterface::prepareTemplateParameterTypeCopy(
    const SgTemplateParameter *parameter, SgCopyHelp &help) {
  ASSERT_not_null(parameter);
  if (auto *defaultType = parameter->get_defaultTypeParameter())
    SageInterface::materializeTypeWithCopyMap(defaultType, help, true);
  if (parameter->get_parameterType() != SgTemplateParameter::nontype_parameter)
    return;
  for (SgType *dependency : parameter->get_type()->getInternalTypes()) {
    auto *named = isSgNamedType(dependency);
    if (named == nullptr)
      continue;
    SgDeclarationStatement *declaration = named->get_declaration();
    ASSERT_not_null(declaration);
    SgNode *mapped = lookupCopiedNode(help, declaration);
    bool owned = mapped != nullptr && mapped != declaration;
    for (SgNode *owner = declaration; owner != nullptr;
         owner = owner->get_parent())
      owned |= help.isCopyingOriginal(owner);
    if (!owned)
      continue;
    auto *canonical = declaration->get_firstNondefiningDeclaration();
    ASSERT_not_null(canonical);
    ROSE_ASSERT(help.copyOrLookupOwnedAst(canonical) != canonical);
    if (auto *definition = declaration->get_definingDeclaration())
      declaration = definition;
    ROSE_ASSERT(help.copyOrLookupOwnedAst(declaration) != declaration);
  }
  SageInterface::materializeTypeWithCopyMap(parameter->get_type(), help, true);
}

void SgCopyHelp::constructTemplateParameterTypeCopy(
    const SgTemplateParameter *parameter) {
  ASSERT_not_null(parameter);
  const auto kind = parameter->get_parameterType();
  auto *copy = isSgTemplateParameter(requireExactMappedNode(
      parameter, *this, "template-parameter-type-copy-owner"));
  ROSE_ASSERT(copy != nullptr && copy != parameter &&
              copy->get_parameterType() == kind);
  if (kind == SgTemplateParameter::nontype_parameter) {
    requireExactCopiedOrExternalSemanticEdge(
        parameter->get_type(), copy->get_type(), *this,
        "template-parameter-type-construction");
  } else {
    ROSE_ASSERT(kind == SgTemplateParameter::type_parameter ||
                kind == SgTemplateParameter::template_parameter);
    auto *sourceType = isSgTemplateType(parameter->get_type());
    ROSE_ASSERT(sourceType != nullptr &&
                sourceType->get_template_parameter() == parameter &&
                sourceType->get_tpl_args().empty());
    const int depth = sourceType->get_template_parameter_depth();
    const int position = sourceType->get_template_parameter_position();
    ROSE_ASSERT((depth < 0) == (position < 0) &&
                (depth >= 0) ==
                    sourceType->get_canonical_source_identity().has_value());
    SgNode *mapped = lookupCopiedNode(*this, sourceType);
    auto *type = isSgTemplateType(mapped);
    if (mapped == nullptr) {
      type = new SgTemplateType(sourceType->get_name());
      type->set_template_parameter_depth(depth);
      type->set_template_parameter_position(position);
      if (sourceType->get_canonical_source_identity().has_value())
        type->initialize_canonical_source_identity(
            *sourceType->get_canonical_source_identity());
      type->set_packed(sourceType->get_packed());
      type->set_template_parameter(copy);
      insertCopiedNodePair(sourceType, type);
    }
    ROSE_ASSERT(type != nullptr && type != sourceType &&
                type->get_template_parameter() == copy &&
                copy->get_type() == type && type->get_tpl_args().empty() &&
                type->get_template_parameter_depth() == depth &&
                type->get_template_parameter_position() == position &&
                type->get_canonical_source_identity() ==
                    sourceType->get_canonical_source_identity() &&
                type->get_packed() == sourceType->get_packed());
  }
}

SgType *SageInterface::materializeTypeWithCopyMap(SgType *type,
                                                  SgCopyHelp &help,
                                                  bool copyOwnedSyntax) {
  ASSERT_not_null(type);
  SgCopyHelp::ConstructionScope construction(help);
  const auto mapping = help.get_copiedNodeMap().find(type);
  if (copyOwnedSyntax && type->has_source_prototype() &&
      (mapping == help.get_copiedNodeMap().end() || mapping->second == type)) {
    if (mapping != help.get_copiedNodeMap().end())
      help.eraseCopiedNodePair(type);
    CopyDepthScope depth(help);
    auto *copy = isSgType(type->copy(help));
    ROSE_ASSERT(copy != nullptr && copy != type &&
                copy->variantT() == type->variantT() &&
                copy->has_source_prototype());
  }
  std::unordered_map<const SgNode *, SgNode *> replacements;
  for (const auto &mapping : help.get_copiedNodeMap()) {
    if (isSgType(mapping.first) != nullptr) {
      ROSE_ASSERT(isSgType(mapping.second) != nullptr);
      if (mapping.first != mapping.second)
        replacements.emplace(mapping.first, mapping.second);
    }
  }
  ExactCopiedTypeGraphMaterializer materializer(help, replacements,
                                                copyOwnedSyntax);
  return materializer.materialize(type);
}

void SgCopyHelp::prepareRootCopyForFixup() {
  if (!hasNontrivialCopiedNodes(*this)) {
    return;
  }

  for (const auto &mapping : get_copiedNodeMap()) {
    const auto *source = isSgClassDeclaration(mapping.first);
    if (source != nullptr && source != mapping.second &&
        source->get_firstNondefiningDeclaration() == source) {
      auto *copy = isSgClassDeclaration(mapping.second);
      ASSERT_not_null(copy);
      constructCopiedClassType(source, copy, *this);
    }
    const auto *sourceEnum = isSgEnumDeclaration(mapping.first);
    if (sourceEnum != nullptr && sourceEnum != mapping.second &&
        sourceEnum->get_firstNondefiningDeclaration() == sourceEnum) {
      auto *copyEnum = isSgEnumDeclaration(mapping.second);
      ASSERT_not_null(copyEnum);
      constructCopiedEnumType(sourceEnum, copyEnum, *this);
    }
  }
  publishExactCopiedTemplateSemanticSurfaces(*this);
  validateExactCopiedNodeMap(*this);
}

void SgCopyHelp::finalizeRootCopy() {
  if (!hasNontrivialCopiedNodes(*this)) {
    return;
  }

  finalizeCanonicalCopyLinks(*this);
}

void SgTreeCopy::prepareRootCopyForFixup() {
  SgCopyHelp::prepareRootCopyForFixup();
}

void SgTreeCopy::finalizeRootCopy() { SgCopyHelp::finalizeRootCopy(); }

void resetVariableDefinitionSupport(
    const SgInitializedName *originalInitializedName,
    SgInitializedName *copyInitializedName,
    SgDeclarationStatement *targetDeclaration, SgCopyHelp &help) {
  ASSERT_not_null(originalInitializedName);
  ASSERT_not_null(copyInitializedName);
  SgDeclarationStatement *originalDeclaration =
      originalInitializedName->get_declptr();
  ASSERT_not_null(originalDeclaration);
  ASSERT_not_null(copyInitializedName->get_declptr());

  switch (originalDeclaration->variantT()) {
  case V_SgTemplateVariableDeclaration:
  case V_SgVariableDeclaration: {
    if (targetDeclaration == NULL) {
      targetDeclaration =
          isSgDeclarationStatement(copyInitializedName->get_parent());
    }
    if (targetDeclaration == NULL ||
        requireExactMappedDeclaration(
            originalDeclaration, help,
            "initialized-name-variable-declaration") != targetDeclaration ||
        copyInitializedName->get_declptr() != targetDeclaration) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[initialized-name-variable-declaration]: "
              "source=%p copy=%p declptr=%p target=%p mapped=%p\n",
              static_cast<void *>(originalDeclaration),
              static_cast<void *>(copyInitializedName),
              static_cast<void *>(copyInitializedName->get_declptr()),
              static_cast<void *>(targetDeclaration),
              static_cast<void *>(lookupCopiedNode(help, originalDeclaration)));
      ROSE_ABORT();
    }

    SgVariableDefinition *variableDefinition_original =
        originalInitializedName->get_definition();
    SgVariableDefinition *provisionalVariableDefinition =
        copyInitializedName->get_definition();
    if (variableDefinition_original == NULL) {
      if (isSgFunctionType(originalInitializedName->get_type()) == NULL ||
          provisionalVariableDefinition != NULL) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[variable-definition-absence]: "
                "source=%p/%s type=%p copy=%p definition=%p\n",
                static_cast<const void *>(originalInitializedName),
                originalInitializedName->get_name().str(),
                static_cast<void *>(originalInitializedName->get_type()),
                static_cast<void *>(copyInitializedName),
                static_cast<void *>(provisionalVariableDefinition));
        ROSE_ABORT();
      }
      break;
    }
    break;
  }

  case V_SgEnumDeclaration: {
    // In this case the target declaration is a SgEnumDeclaration, and it
    // has already been copied so it need not be created.
    ROSE_ASSERT(targetDeclaration != NULL);

    SgEnumDeclaration *enumDeclaration_original =
        isSgEnumDeclaration(originalInitializedName->get_declptr());
    if (originalInitializedName->get_definition() != NULL ||
        copyInitializedName->get_definition() != NULL ||
        requireExactMappedDeclaration(enumDeclaration_original, help,
                                      "initialized-name-enum-declaration") !=
            targetDeclaration ||
        copyInitializedName->get_declptr() != targetDeclaration) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[initialized-name-enum-declaration]: "
              "source=%p copy=%p declptr=%p target=%p\n",
              static_cast<void *>(enumDeclaration_original),
              static_cast<void *>(copyInitializedName),
              static_cast<void *>(copyInitializedName->get_declptr()),
              static_cast<void *>(targetDeclaration));
      ROSE_ABORT();
    }
    break;
  }

    // DQ (12/23/2012): Added support for templates.
  case V_SgTemplateFunctionDeclaration:
  case V_SgTemplateMemberFunctionDeclaration:

  case V_SgFunctionDeclaration:
  case V_SgMemberFunctionDeclaration:
  case V_SgTemplateInstantiationFunctionDecl:
  case V_SgTemplateInstantiationMemberFunctionDecl:
  case V_SgProcedureHeaderStatement:
  case V_SgProgramHeaderStatement: {
    // In this case the target declaration is a SgFunctionDeclaration, and
    // it has already been copied so it need not be created.
    ROSE_ASSERT(targetDeclaration != NULL);

    SgFunctionDeclaration *functionDeclaration_original =
        isSgFunctionDeclaration(originalInitializedName->get_declptr());
    if (originalInitializedName->get_definition() != NULL ||
        copyInitializedName->get_definition() != NULL ||
        requireExactMappedDeclaration(
            functionDeclaration_original, help,
            "initialized-name-function-declaration") != targetDeclaration ||
        copyInitializedName->get_declptr() != targetDeclaration) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[initialized-name-function-declaration]: "
              "source=%p copy=%p declptr=%p target=%p\n",
              static_cast<void *>(functionDeclaration_original),
              static_cast<void *>(copyInitializedName),
              static_cast<void *>(copyInitializedName->get_declptr()),
              static_cast<void *>(targetDeclaration));
      ROSE_ABORT();
    }
    break;
  }

  default: {
    printf("Error: default reached in resetVariableDefinitionSupport() "
           "originalDeclaration = %p = %s \n",
           originalDeclaration, originalDeclaration->class_name().c_str());
    ROSE_ABORT();
  }
  }
}

void SgInitializedName::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  // Guard against shared nodes. When nodes are shared (not copied), this ==
  // copy.
  if (this == copy) {
    // Node is shared, not copied. Skip fixup to avoid corrupting the
    // original.
    return;
  }

  SgInitializedName *initializedName_copy = isSgInitializedName(copy);
  ROSE_ASSERT(initializedName_copy != NULL);

  SgNode *originalParent = get_parent();
  SgNode *copiedParent = initializedName_copy->get_parent();
  SgFunctionParameterList *originalParameterList =
      isSgFunctionParameterList(originalParent);
  SgFunctionParameterList *copiedParameterList =
      isSgFunctionParameterList(copiedParent);
  SgNode *originalOwner =
      originalParent != nullptr ? originalParent->get_parent() : nullptr;
  SgNode *copiedOwner =
      copiedParent != nullptr ? copiedParent->get_parent() : nullptr;
  SgFunctionDeclaration *originalFunction =
      isSgFunctionDeclaration(originalOwner);
  SgFunctionDeclaration *copiedFunction = isSgFunctionDeclaration(copiedOwner);
  SgRequiresExpr *originalRequires = isSgRequiresExpr(originalOwner);
  SgRequiresExpr *copiedRequires = isSgRequiresExpr(copiedOwner);
  const bool exactRequiresLocalParameter =
      originalParameterList != nullptr && copiedParameterList != nullptr &&
      originalRequires != nullptr && copiedRequires != nullptr &&
      ((originalRequires->get_local_parameter_list() == originalParameterList &&
        copiedRequires->get_local_parameter_list() == copiedParameterList) ||
       (originalRequires->get_local_parameter_list_syntax() ==
            originalParameterList &&
        copiedRequires->get_local_parameter_list_syntax() ==
            copiedParameterList)) &&
      get_declptr() == nullptr &&
      initializedName_copy->get_declptr() == nullptr &&
      std::count(originalParameterList->get_args().begin(),
                 originalParameterList->get_args().end(), this) == 1 &&
      std::count(copiedParameterList->get_args().begin(),
                 copiedParameterList->get_args().end(),
                 initializedName_copy) == 1;
  const bool exactDetachedFunctionParameter =
      originalParameterList != nullptr && copiedParameterList != nullptr &&
      originalFunction != nullptr && copiedOwner == nullptr &&
      copiedParameterList->get_parent() == nullptr &&
      (originalFunction->get_parameterList() == originalParameterList ||
       originalFunction->get_parameterList_syntax() == originalParameterList) &&
      lookupCopiedNode(help, originalParameterList) == copiedParameterList &&
      lookupCopiedNode(help, originalFunction) == nullptr &&
      get_declptr() == originalFunction &&
      initializedName_copy->get_declptr() == originalFunction &&
      get_definition() == nullptr &&
      initializedName_copy->get_definition() == nullptr &&
      std::count(originalParameterList->get_args().begin(),
                 originalParameterList->get_args().end(), this) == 1 &&
      std::count(copiedParameterList->get_args().begin(),
                 copiedParameterList->get_args().end(),
                 initializedName_copy) == 1;
  if (initializedName_copy->get_declptr() == NULL &&
      !exactRequiresLocalParameter) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[initialized-name-declaration-null]: "
        "source=%p/%s name=%s parent=%p/%s owner=%p/%s function=%s "
        "declptr=%p scope=%p copy=%p parent=%p/%s owner=%p/%s function=%s "
        "scope=%p has no exact copied declaration identity\n",
        static_cast<const void *>(this), class_name().c_str(), get_name().str(),
        static_cast<void *>(originalParent),
        originalParent != nullptr ? originalParent->class_name().c_str()
                                  : "<null>",
        static_cast<void *>(originalOwner),
        originalOwner != nullptr ? originalOwner->class_name().c_str()
                                 : "<null>",
        originalFunction != nullptr
            ? originalFunction->get_qualified_name().str()
            : "<null>",
        static_cast<void *>(get_declptr()), static_cast<void *>(get_scope()),
        static_cast<void *>(initializedName_copy),
        static_cast<void *>(copiedParent),
        copiedParent != nullptr ? copiedParent->class_name().c_str() : "<null>",
        static_cast<void *>(copiedOwner),
        copiedOwner != nullptr ? copiedOwner->class_name().c_str() : "<null>",
        copiedFunction != nullptr ? copiedFunction->get_qualified_name().str()
                                  : "<null>",
        static_cast<void *>(initializedName_copy->get_scope()));
    ROSE_ABORT();
  }

  validateExactCopiedInitializedNameSemanticEdges(this, initializedName_copy,
                                                  help);

  SgNode *parent = initializedName_copy->get_parent();
  // A variable declaration may publish this parent after copying its name.
  if (parent != NULL) {
    ROSE_ASSERT(parent != NULL);
    switch (parent->variantT()) {
    case V_SgTemplateVariableDeclaration:

    case V_SgVariableDeclaration: {
      resetVariableDefinitionSupport(this, initializedName_copy, NULL, help);
      break;
    }

    case V_SgEnumDeclaration: {
      SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(parent);
      ROSE_ASSERT(enumDeclaration != NULL);
      resetVariableDefinitionSupport(this, initializedName_copy,
                                     enumDeclaration, help);
      break;
    }

    case V_SgFunctionParameterList: {
      SgNode *parentFunction = parent->get_parent();
      SgFunctionDeclaration *functionDeclaration =
          isSgFunctionDeclaration(parentFunction);
      SgRequiresExpr *requiresExpression = isSgRequiresExpr(parentFunction);

      if (functionDeclaration != NULL) {
        ROSE_ASSERT(initializedName_copy != NULL);

        ROSE_ASSERT(initializedName_copy->get_declptr() != NULL);

        resetVariableDefinitionSupport(this, initializedName_copy,
                                       functionDeclaration, help);
      } else if (exactDetachedFunctionParameter) {
        if (copiedParameterList == nullptr ||
            copiedParameterList->get_parent() != nullptr ||
            copiedParameterList->get_definingDeclaration() != nullptr ||
            copiedParameterList->get_firstNondefiningDeclaration() !=
                copiedParameterList ||
            initializedName_copy->get_scope() != get_scope() ||
            initializedName_copy->get_declptr() != originalFunction) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[detached-function-parameter]: "
                  "source=%p/%s function=%p list=%p scope=%p copy=%p "
                  "list=%p owner=%p scope=%p declptr=%p does not preserve "
                  "one exact external semantic edge\n",
                  static_cast<const void *>(this), get_name().str(),
                  static_cast<void *>(originalFunction),
                  static_cast<void *>(originalParameterList),
                  static_cast<void *>(get_scope()),
                  static_cast<void *>(initializedName_copy),
                  static_cast<void *>(copiedParameterList),
                  static_cast<void *>(copiedParameterList->get_parent()),
                  static_cast<void *>(initializedName_copy->get_scope()),
                  static_cast<void *>(initializedName_copy->get_declptr()));
          ROSE_ABORT();
        }
      } else if (requiresExpression != NULL) {
        if (!exactRequiresLocalParameter ||
            requiresExpression != copiedRequires ||
            initializedName_copy->get_declptr() != nullptr) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[requires-local-parameter]: source=%p/%s "
                  "list=%p requires=%p copy=%p list=%p requires=%p "
                  "declptr=%p does not preserve one exact requires-expression "
                  "parameter role\n",
                  static_cast<const void *>(this), get_name().str(),
                  static_cast<void *>(originalParameterList),
                  static_cast<void *>(originalRequires),
                  static_cast<void *>(initializedName_copy),
                  static_cast<void *>(copiedParameterList),
                  static_cast<void *>(copiedRequires),
                  static_cast<void *>(initializedName_copy->get_declptr()));
          ROSE_ABORT();
        }
      } else {
        fprintf(stderr,
                "REX_COPY_INVARIANT[function-parameter-owner]: source=%p/%s "
                "copy=%p parameter-list=%p owner=%p/%s has neither an exact "
                "function nor requires-expression owner\n",
                static_cast<const void *>(this), get_name().str(),
                static_cast<void *>(initializedName_copy),
                static_cast<void *>(parent),
                static_cast<void *>(parentFunction),
                parentFunction != nullptr ? parentFunction->class_name().c_str()
                                          : "<null>");
        ROSE_ABORT();
      }
      break;
    }

    case V_SgCtorInitializerList: {
      SgNode *parentFunction = parent->get_parent();
      SgMemberFunctionDeclaration *memberFunctionDeclaration =
          isSgMemberFunctionDeclaration(parentFunction);

      if (memberFunctionDeclaration != NULL) {
        ROSE_ASSERT(initializedName_copy != NULL);
        ROSE_ASSERT(initializedName_copy->get_declptr() != NULL);

        resetVariableDefinitionSupport(this, initializedName_copy,
                                       memberFunctionDeclaration, help);
      }
      break;
    }

    default: {
      printf("default reached in SgInitializedName::fixupCopy_scopes() "
             "parent = %p = %s \n",
             parent, parent->class_name().c_str());
      ROSE_ABORT();
    }
    }
  }

  // Initializers can own declarations and scopes, including lambda closures.
  // Construct those exact copies before reference binding consumes their
  // symbols.
  fixupExactCopiedOwnedSuccessorScopes(this, initializedName_copy, help,
                                       "initialized-name");
}

// DQ (11/1/2007): Build lighter weight versions of
// SgStatement::fixupCopy_scopes() and SgExpression::fixupCopy_scopes() and
// refactor code into the SgLocatedNode::fixupCopy_scopes().

void SgStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  SgLocatedNode::fixupCopy_scopes(copy, help);
}

void SgOmpExecStatement::fixupCopy_scopes(SgNode *copy,
                                          SgCopyHelp &help) const {
  SgOmpExecStatement *copiedExec = isSgOmpExecStatement(copy);
  if (copiedExec == NULL ||
      requireExactMappedNode(this, help, "openmp-exec") != copiedExec) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openmp-exec-kind]: source=%p/%s copy=%p/%s\n",
            static_cast<const void *>(this), class_name().c_str(),
            static_cast<void *>(copy),
            copy != NULL ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  SgStatement::fixupCopy_scopes(copy, help);
  fixupOmpExecSemanticEdges(this, copiedExec, help, true);
}

void SgOmpClauseStatement::fixupCopy_scopes(SgNode *copy,
                                            SgCopyHelp &help) const {
  SgOmpExecStatement::fixupCopy_scopes(copy, help);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help,
                                       "openmp-clause-statement");
}

void SgOmpTaskwaitStatement::fixupCopy_scopes(SgNode *copy,
                                              SgCopyHelp &help) const {
  SgOmpExecStatement::fixupCopy_scopes(copy, help);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help,
                                       "openmp-taskwait-statement");
}

void SgAccClauseStatement::fixupCopy_scopes(SgNode *copy,
                                            SgCopyHelp &help) const {
  SgStatement::fixupCopy_scopes(copy, help);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help,
                                       "openacc-clause-statement");
}

void SgOmpBodyStatement::fixupCopy_scopes(SgNode *copy,
                                          SgCopyHelp &help) const {
  SgOmpBodyStatement *copiedBodyOwner = isSgOmpBodyStatement(copy);
  if (copiedBodyOwner == NULL) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openmp-body-kind]: original=%p/%s copy=%p/%s\n",
            static_cast<const void *>(this), class_name().c_str(),
            static_cast<void *>(copy),
            copy != NULL ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  SgOmpExecStatement::fixupCopy_scopes(copy, help);

  SgStatement *originalBody = get_body();
  SgStatement *copiedBody = copiedBodyOwner->get_body();
  if ((originalBody == NULL) != (copiedBody == NULL)) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[openmp-body-null]: original=%p/%s body=%p "
        "copy=%p/%s body=%p\n",
        static_cast<const void *>(this), class_name().c_str(),
        static_cast<void *>(originalBody), static_cast<void *>(copiedBodyOwner),
        copiedBodyOwner->class_name().c_str(), static_cast<void *>(copiedBody));
    ROSE_ABORT();
  }
  if (originalBody != NULL) {
    requireExactCopiedOwnedChild(this, originalBody, copiedBodyOwner,
                                 copiedBody, help, "openmp-body");
    originalBody->fixupCopy_scopes(copiedBody, help);
  }
}

void SgAccBodyStatement::fixupCopy_scopes(SgNode *copy,
                                          SgCopyHelp &help) const {
  SgAccBodyStatement *copiedBodyOwner = isSgAccBodyStatement(copy);
  if (copiedBodyOwner == NULL) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[openacc-body-kind]: original=%p/%s copy=%p/%s\n",
        static_cast<const void *>(this), class_name().c_str(),
        static_cast<void *>(copy),
        copy != NULL ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  SgStatement::fixupCopy_scopes(copy, help);

  SgStatement *originalBody = get_body();
  SgStatement *copiedBody = copiedBodyOwner->get_body();
  if ((originalBody == NULL) != (copiedBody == NULL)) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[openacc-body-null]: original=%p/%s body=%p "
        "copy=%p/%s body=%p\n",
        static_cast<const void *>(this), class_name().c_str(),
        static_cast<void *>(originalBody), static_cast<void *>(copiedBodyOwner),
        copiedBodyOwner->class_name().c_str(), static_cast<void *>(copiedBody));
    ROSE_ABORT();
  }
  if (originalBody != NULL) {
    requireExactCopiedOwnedChild(this, originalBody, copiedBodyOwner,
                                 copiedBody, help, "openacc-body");
    originalBody->fixupCopy_scopes(copiedBody, help);
  }
}

void SgOmpClauseBodyStatement::fixupCopy_scopes(SgNode *copy,
                                                SgCopyHelp &help) const {
  SgOmpClauseBodyStatement *copiedClauseBody = isSgOmpClauseBodyStatement(copy);
  if (copiedClauseBody == NULL) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openmp-clause-body-kind]: original=%p/%s "
            "copy=%p/%s\n",
            static_cast<const void *>(this), class_name().c_str(),
            static_cast<void *>(copy),
            copy != NULL ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  SgOmpBodyStatement::fixupCopy_scopes(copy, help);

  const SgOmpClauseList *originalClauses = get_clause_list();
  SgOmpClauseList *copiedClauses = copiedClauseBody->get_clause_list();
  requireExactCopiedOwnedChild(this, originalClauses, copiedClauseBody,
                               copiedClauses, help, "openmp-clause-list");
  fixupExactCopiedSupportScopes(originalClauses, copiedClauses, help);
}

void SgAccClauseBodyStatement::fixupCopy_scopes(SgNode *copy,
                                                SgCopyHelp &help) const {
  SgAccClauseBodyStatement *copiedClauseBody = isSgAccClauseBodyStatement(copy);
  if (copiedClauseBody == NULL) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openacc-clause-body-kind]: original=%p/%s "
            "copy=%p/%s\n",
            static_cast<const void *>(this), class_name().c_str(),
            static_cast<void *>(copy),
            copy != NULL ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  SgAccBodyStatement::fixupCopy_scopes(copy, help);

  const SgAccClausePtrList &originalClauses = get_clauses();
  const SgAccClausePtrList &copiedClauses = copiedClauseBody->get_clauses();
  if (originalClauses.size() != copiedClauses.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[openacc-clause-count]: original=%p clauses=%zu "
            "copy=%p clauses=%zu\n",
            static_cast<const void *>(this), originalClauses.size(),
            static_cast<void *>(copiedClauseBody), copiedClauses.size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < originalClauses.size(); ++index) {
    SgAccClause *originalClause = originalClauses[index];
    SgAccClause *copiedClause = copiedClauses[index];
    requireExactCopiedOwnedChild(this, originalClause, copiedClauseBody,
                                 copiedClause, help, "openacc-clause");
    fixupExactCopiedSupportScopes(originalClause, copiedClause, help);
  }
}

void SgOmpDeclareSimdStatement::fixupCopy_scopes(SgNode *copy,
                                                 SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_scopes(copy, help);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help, "openmp-declare-simd");
}

void SgOmpDeclareVariantStatement::fixupCopy_scopes(SgNode *copy,
                                                    SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_scopes(copy, help);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help,
                                       "openmp-declare-variant");
}

void SgOmpBeginDeclareVariantStatement::fixupCopy_scopes(
    SgNode *copy, SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_scopes(copy, help);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help,
                                       "openmp-begin-declare-variant");
}

void SgOmpDeclareMapperStatement::fixupCopy_scopes(SgNode *copy,
                                                   SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_scopes(copy, help);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help,
                                       "openmp-declare-mapper");
}

void SgOmpDeclareTargetStatement::fixupCopy_scopes(SgNode *copy,
                                                   SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_scopes(copy, help);
  SgOmpDeclareTargetStatement *copiedTarget =
      isSgOmpDeclareTargetStatement(copy);
  if (copiedTarget == NULL) {
    ROSE_ABORT();
  }
  fixupOmpDeclareTargetSemanticEdges(this, copiedTarget, help, true);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help,
                                       "openmp-declare-target");
}

void SgOmpRequiresStatement::fixupCopy_scopes(SgNode *copy,
                                              SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_scopes(copy, help);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help, "openmp-requires");
}

void SgOmpAssumesStatement::fixupCopy_scopes(SgNode *copy,
                                             SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_scopes(copy, help);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help, "openmp-assumes");
}

void SgOmpBeginAssumesStatement::fixupCopy_scopes(SgNode *copy,
                                                  SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_scopes(copy, help);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help,
                                       "openmp-begin-assumes");
}

void SgOmpGroupprivateStatement::fixupCopy_scopes(SgNode *copy,
                                                  SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_scopes(copy, help);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help, "openmp-groupprivate");
}

void SgOmpThreadprivateStatement::fixupCopy_scopes(SgNode *copy,
                                                   SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_scopes(copy, help);
  fixupExactCopiedOwnedSuccessorScopes(this, copy, help,
                                       "openmp-threadprivate");
}

void SgExpression::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

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
    (*i_original)->fixupCopy_scopes(*i_copy, help);
  }

  SgLocatedNode::fixupCopy_scopes(copy, help);
}

// DQ (11/1/2007): Make this work on SgLocatedNode (so that it can work on
// SgStatement and SgExpression).

// DQ (10/5/2007): Added IR node specific function to permit copies, via AST
// copy(), to be fixedup Usually this will correct scopes and in a few cases
// build child IR nodes that are not traversed (and thus shared in the
// result from the automatically generated copy function).
void SgLocatedNode::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  if (this == copy) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[located-node-identity]: original=%p type=%s "
            "was shared instead of copied\n",
            static_cast<const void *>(this), this->class_name().c_str());
    ROSE_ABORT();
  }

  SgLocatedNode *copyLocatedNode = isSgLocatedNode(copy);
  ROSE_ASSERT(copyLocatedNode != NULL);

  // DQ (10/24/2007): New test.
  ROSE_ASSERT(copyLocatedNode->variantT() == this->variantT());

  SgNode *originalParent = this->get_parent();
  const size_t originalOwnerEdges = countDirectOwnerEdges(originalParent, this);
  if ((originalParent == NULL && originalOwnerEdges != 0) ||
      (originalParent != NULL && originalOwnerEdges != 1)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[located-node-original-owner]: original=%p "
            "type=%s parent=%p edges=%zu is not exactly published\n",
            static_cast<const void *>(this), this->class_name().c_str(),
            static_cast<void *>(originalParent), originalOwnerEdges);
    ROSE_ABORT();
  }

  SgNode *plannedParent = help.plannedExternalSemanticCopyEdge(originalParent);
  SgNode *structuralParent = lookupCopiedNode(help, originalParent);
  SgNode *copiedParent =
      structuralParent != nullptr ? structuralParent : plannedParent;
  // A scope projection does not copy its lexical ownership list. Independent
  // roots can remain detached; copied children must retain their exact owner.
  const bool copiedTransactionRoot = structuralParent == nullptr;
  const bool detachedRoot =
      copiedTransactionRoot && copyLocatedNode->get_parent() == nullptr;
  if (!detachedRoot &&
      (copiedParent == nullptr ||
       copyLocatedNode->get_parent() != copiedParent ||
       countDirectOwnerEdges(copiedParent, copyLocatedNode) != 1)) {
    const SgDeclarationStatement *originalDeclaration =
        isSgDeclarationStatement(const_cast<SgLocatedNode *>(this));
    const SgClassDeclaration *originalClass = isSgClassDeclaration(
        const_cast<SgDeclarationStatement *>(originalDeclaration));
    const SgEnumDeclaration *originalEnum = isSgEnumDeclaration(
        const_cast<SgDeclarationStatement *>(originalDeclaration));
    const std::string originalName =
        originalClass != NULL
            ? originalClass->get_name().str()
            : (originalEnum != NULL ? originalEnum->get_name().str()
                                    : "<unnamed>");
    const Sg_File_Info *originalPosition = this->get_file_info();
    fprintf(stderr,
            "REX_COPY_INVARIANT[located-node-copy-owner]: original=%p type=%s "
            "name=%s at %s:%d:%d original-parent=%p copy=%p parent=%p "
            "original-parent-type=%s copy-parent-type=%s expected=%p "
            "transaction-root=%d edges=%zu\n",
            static_cast<const void *>(this), this->class_name().c_str(),
            originalName.c_str(),
            originalPosition != NULL
                ? originalPosition->get_filenameString().c_str()
                : "<unknown>",
            originalPosition != NULL ? originalPosition->get_line() : 0,
            originalPosition != NULL ? originalPosition->get_col() : 0,
            static_cast<void *>(originalParent),
            static_cast<void *>(copyLocatedNode),
            static_cast<void *>(copyLocatedNode->get_parent()),
            originalParent != NULL ? originalParent->class_name().c_str()
                                   : "<null>",
            copyLocatedNode->get_parent() != NULL
                ? copyLocatedNode->get_parent()->class_name().c_str()
                : "<null>",
            static_cast<void *>(copiedParent), copiedTransactionRoot ? 1 : 0,
            countDirectOwnerEdges(copiedParent, copyLocatedNode));
    ROSE_ABORT();
  }
}

void SgScopeStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  // Guard against shared nodes. When nodes are shared (not copied), this ==
  // copy.
  if (this == copy) {
    // Node is shared, not copied. Skip fixup to avoid corrupting the
    // original.
    return;
  }

  // We need to call the fixupCopy function from the parent of a
  // SgVariableDeclaration because the copy function in the parent of the
  // variable declaration sets the parent of the SgVariableDeclaration and
  // we need this parent in the fixupCopy function in the SgInitializedName.

  SgScopeStatement *copyScopeStatement = isSgScopeStatement(copy);
  ROSE_ASSERT(copyScopeStatement != NULL);
  ROSE_ASSERT(copyScopeStatement->variantT() == this->variantT());

  // Preserve case-sensitivity semantics across deep copies (critical for
  // Fortran scopes and symbol-table lookup behavior).
  if (copyScopeStatement->isCaseInsensitive() != this->isCaseInsensitive()) {
    copyScopeStatement->setCaseInsensitive(this->isCaseInsensitive());
  }

  // A completed GNU statement-expression body retains an explicit outer
  // semantic scope because its expression can be owned through a shared type
  // edge.  Remap that semantic reference independently of the copied
  // structural owner.  An active construction transaction is never a valid
  // copy input.
  if (const SgBasicBlock *originalBlock = isSgBasicBlock(this)) {
    SgBasicBlock *copiedBlock = isSgBasicBlock(copyScopeStatement);
    ASSERT_not_null(copiedBlock);
    SgScopeStatement *originalConstruction =
        originalBlock->get_statement_expression_construction_scope();
    SgScopeStatement *copiedConstruction =
        copiedBlock->get_statement_expression_construction_scope();
    SgScopeStatement *originalSemantic =
        originalBlock->get_statement_expression_semantic_scope();
    SgScopeStatement *provisionalSemantic =
        copiedBlock->get_statement_expression_semantic_scope();
    if (originalConstruction != NULL || copiedConstruction != NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[statement-expression-construction-scope]: "
              "original=%p construction=%p copy=%p construction=%p cannot "
              "copy an active semantic-scope transaction\n",
              static_cast<const void *>(originalBlock),
              static_cast<void *>(originalConstruction),
              static_cast<void *>(copiedBlock),
              static_cast<void *>(copiedConstruction));
      ROSE_ABORT();
    }
    SgScopeStatement *resolvedSemantic =
        isSgScopeStatement(remapExactCopiedOrExternalSemanticEdge(
            originalSemantic, provisionalSemantic, help,
            "statement-expression-semantic-scope"));
    if ((originalSemantic == NULL) != (resolvedSemantic == NULL)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[statement-expression-semantic-scope]: "
              "original=%p semantic=%p copy=%p provisional=%p resolved=%p "
              "lost its exact semantic role\n",
              static_cast<const void *>(originalBlock),
              static_cast<void *>(originalSemantic),
              static_cast<void *>(copiedBlock),
              static_cast<void *>(provisionalSemantic),
              static_cast<void *>(resolvedSemantic));
      ROSE_ABORT();
    }
    copiedBlock->set_statement_expression_semantic_scope(resolvedSemantic);

    SgScopeStatement *originalImpliedConstruction =
        originalBlock->get_implied_do_construction_scope();
    SgScopeStatement *copiedImpliedConstruction =
        copiedBlock->get_implied_do_construction_scope();
    SgScopeStatement *originalForallConstruction =
        originalBlock->get_forall_construction_scope();
    SgScopeStatement *copiedForallConstruction =
        copiedBlock->get_forall_construction_scope();
    SgScopeStatement *originalImpliedSemantic =
        originalBlock->get_implied_do_semantic_scope();
    SgScopeStatement *provisionalImpliedSemantic =
        copiedBlock->get_implied_do_semantic_scope();
    if (originalImpliedConstruction != NULL ||
        copiedImpliedConstruction != NULL ||
        originalForallConstruction != NULL ||
        copiedForallConstruction != NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[typed-block-construction-scope]: "
              "original=%p implied-do=%p forall=%p copy=%p implied-do=%p "
              "forall=%p cannot copy an active semantic-scope transaction\n",
              static_cast<const void *>(originalBlock),
              static_cast<void *>(originalImpliedConstruction),
              static_cast<void *>(originalForallConstruction),
              static_cast<void *>(copiedBlock),
              static_cast<void *>(copiedImpliedConstruction),
              static_cast<void *>(copiedForallConstruction));
      ROSE_ABORT();
    }
    SgScopeStatement *resolvedImpliedSemantic =
        isSgScopeStatement(remapExactCopiedOrExternalSemanticEdge(
            originalImpliedSemantic, provisionalImpliedSemantic, help,
            "implied-do-semantic-scope"));
    if ((originalImpliedSemantic == NULL) !=
        (resolvedImpliedSemantic == NULL)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[implied-do-semantic-scope]: original=%p "
              "semantic=%p copy=%p provisional=%p resolved=%p lost its exact "
              "semantic role\n",
              static_cast<const void *>(originalBlock),
              static_cast<void *>(originalImpliedSemantic),
              static_cast<void *>(copiedBlock),
              static_cast<void *>(provisionalImpliedSemantic),
              static_cast<void *>(resolvedImpliedSemantic));
      ROSE_ABORT();
    }
    copiedBlock->set_implied_do_semantic_scope(resolvedImpliedSemantic);
  }

  // Auxiliary declaration scopes are structurally owned by every scope
  // through an SgDeclarationScopeList, but they are intentionally outside
  // each source-emission statement list.  Consequently none of the
  // statement-list-specific fixup implementations below can reach them.
  // Fix their exact structural children here before symbol rebuilding: a
  // copied SgNonrealDecl must name its copied SgDeclarationScope, never the
  // original semantic scope retained by the generated pointer copy.
  SgDeclarationScopeList *originalDeclarationScopes =
      this->get_auxiliary_declaration_scopes();
  SgDeclarationScopeList *copiedDeclarationScopes =
      copyScopeStatement->get_auxiliary_declaration_scopes();
  if ((originalDeclarationScopes == NULL) !=
      (copiedDeclarationScopes == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[auxiliary-scope-container]: original=%p "
            "container=%p copy=%p container=%p\n",
            static_cast<const void *>(this),
            static_cast<void *>(originalDeclarationScopes),
            static_cast<void *>(copyScopeStatement),
            static_cast<void *>(copiedDeclarationScopes));
    ROSE_ABORT();
  }
  if (originalDeclarationScopes != NULL) {
    if (originalDeclarationScopes->get_parent() != this ||
        copiedDeclarationScopes->get_parent() != copyScopeStatement ||
        requireExactMappedNode(originalDeclarationScopes, help,
                               "auxiliary-scope-container") !=
            copiedDeclarationScopes) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[auxiliary-scope-container-owner]: "
              "original=%p container=%p parent=%p copy=%p container=%p "
              "parent=%p\n",
              static_cast<const void *>(this),
              static_cast<void *>(originalDeclarationScopes),
              static_cast<void *>(originalDeclarationScopes->get_parent()),
              static_cast<void *>(copyScopeStatement),
              static_cast<void *>(copiedDeclarationScopes),
              static_cast<void *>(copiedDeclarationScopes->get_parent()));
      ROSE_ABORT();
    }

    const SgDeclarationScopePtrList &originalScopes =
        originalDeclarationScopes->get_scopes();
    const SgDeclarationScopePtrList &copiedScopes =
        copiedDeclarationScopes->get_scopes();
    validateExactCopiedOwnedList(originalDeclarationScopes, originalScopes,
                                 copiedDeclarationScopes, copiedScopes, help,
                                 "auxiliary-declaration-scope");

    for (size_t scopeIndex = 0; scopeIndex < originalScopes.size();
         ++scopeIndex) {
      SgDeclarationScope *originalDeclarationScope = originalScopes[scopeIndex];
      SgDeclarationScope *copiedDeclarationScope = copiedScopes[scopeIndex];
      ROSE_ASSERT(originalDeclarationScope != NULL);
      ROSE_ASSERT(copiedDeclarationScope != NULL);

      originalDeclarationScope->fixupCopy_scopes(copiedDeclarationScope, help);

      const SgDeclarationStatementPtrList &originalDeclarations =
          originalDeclarationScope->get_declarations();
      const SgDeclarationStatementPtrList &copiedDeclarations =
          copiedDeclarationScope->get_declarations();
      validateExactCopiedOwnedList(originalDeclarationScope,
                                   originalDeclarations, copiedDeclarationScope,
                                   copiedDeclarations, help,
                                   "auxiliary-scope-declaration");
      for (size_t declarationIndex = 0;
           declarationIndex < originalDeclarations.size(); ++declarationIndex) {
        originalDeclarations[declarationIndex]->fixupCopy_scopes(
            copiedDeclarations[declarationIndex], help);
      }
    }
  }

  // Auxiliary declarations are a structural part of a scope even though
  // they are intentionally absent from its source-emission declaration
  // list.  The generated copy operation clones the auxiliary container and
  // its children, but only the owning scope knows which copied scope those
  // declarations must name semantically.  Fix them here, while the
  // original-to-copy scope map is exact, instead of repairing stale scope
  // pointers during symbol rebuilding.
  SgAuxiliaryDeclarationList *originalAuxiliary =
      this->get_auxiliary_declarations();
  SgAuxiliaryDeclarationList *copiedAuxiliary =
      copyScopeStatement->get_auxiliary_declarations();
  if ((originalAuxiliary == NULL) != (copiedAuxiliary == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[auxiliary-container]: original-scope=%p "
            "copy-scope=%p original-container=%p copy-container=%p\n",
            static_cast<const void *>(this),
            static_cast<void *>(copyScopeStatement),
            static_cast<void *>(originalAuxiliary),
            static_cast<void *>(copiedAuxiliary));
    ROSE_ABORT();
  }
  if (originalAuxiliary != NULL) {
    if (originalAuxiliary->get_parent() != this ||
        copiedAuxiliary->get_parent() != copyScopeStatement ||
        originalAuxiliary == copiedAuxiliary) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[auxiliary-container-owner]: "
              "original-scope=%p original-container=%p original-parent=%p "
              "copy-scope=%p copy-container=%p copy-parent=%p\n",
              static_cast<const void *>(this),
              static_cast<void *>(originalAuxiliary),
              static_cast<void *>(originalAuxiliary->get_parent()),
              static_cast<void *>(copyScopeStatement),
              static_cast<void *>(copiedAuxiliary),
              static_cast<void *>(copiedAuxiliary->get_parent()));
      ROSE_ABORT();
    }

    const SgDeclarationStatementPtrList &originalDeclarations =
        originalAuxiliary->get_declarations();
    const SgDeclarationStatementPtrList &copiedDeclarations =
        copiedAuxiliary->get_declarations();
    if (originalDeclarations.size() != copiedDeclarations.size()) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[auxiliary-declaration-count]: "
              "original-scope=%p copy-scope=%p original-count=%zu "
              "copy-count=%zu\n",
              static_cast<const void *>(this),
              static_cast<void *>(copyScopeStatement),
              originalDeclarations.size(), copiedDeclarations.size());
      ROSE_ABORT();
    }

    for (size_t index = 0; index < originalDeclarations.size(); ++index) {
      SgDeclarationStatement *originalDeclaration = originalDeclarations[index];
      SgDeclarationStatement *copiedDeclaration = copiedDeclarations[index];
      if (originalDeclaration == NULL || copiedDeclaration == NULL ||
          originalDeclaration == copiedDeclaration ||
          originalDeclaration->get_parent() != originalAuxiliary ||
          copiedDeclaration->get_parent() != copiedAuxiliary) {
        fprintf(
            stderr,
            "REX_COPY_INVARIANT[auxiliary-declaration-owner]: "
            "index=%zu original-scope=%p/%s original-declaration=%p/%s "
            "name=%s original-parent=%p/%s copy-scope=%p/%s "
            "copy-declaration=%p/%s copy-parent=%p/%s\n",
            index, static_cast<const void *>(this), this->class_name().c_str(),
            static_cast<void *>(originalDeclaration),
            originalDeclaration != NULL
                ? originalDeclaration->class_name().c_str()
                : "<null>",
            originalDeclaration != NULL
                ? SageInterface::get_name(originalDeclaration).c_str()
                : "<null>",
            static_cast<void *>(originalDeclaration != NULL
                                    ? originalDeclaration->get_parent()
                                    : NULL),
            originalDeclaration != NULL &&
                    originalDeclaration->get_parent() != NULL
                ? originalDeclaration->get_parent()->class_name().c_str()
                : "<null>",
            static_cast<void *>(copyScopeStatement),
            copyScopeStatement->class_name().c_str(),
            static_cast<void *>(copiedDeclaration),
            copiedDeclaration != NULL ? copiedDeclaration->class_name().c_str()
                                      : "<null>",
            static_cast<void *>(copiedDeclaration != NULL
                                    ? copiedDeclaration->get_parent()
                                    : NULL),
            copiedDeclaration != NULL && copiedDeclaration->get_parent() != NULL
                ? copiedDeclaration->get_parent()->class_name().c_str()
                : "<null>");
        ROSE_ABORT();
      }

      originalDeclaration->fixupCopy_scopes(copiedDeclaration, help);
      if (copiedDeclaration->get_scope() != copyScopeStatement) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[auxiliary-declaration-scope]: "
                "index=%zu original-scope=%p copy-scope=%p "
                "copy-declaration=%p semantic-scope=%p\n",
                index, static_cast<const void *>(this),
                static_cast<void *>(copyScopeStatement),
                static_cast<void *>(copiedDeclaration),
                static_cast<void *>(copiedDeclaration->get_scope()));
        ROSE_ABORT();
      }
    }
  }

  SgStatement::fixupCopy_scopes(copy, help);
}

void SgGlobal::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgGlobal *global_copy = isSgGlobal(copy);
  ROSE_ASSERT(global_copy != NULL);

  const SgDeclarationStatementPtrList &statementList_original =
      this->get_declarations();
  SgDeclarationStatementPtrList &statementList_copy =
      global_copy->get_declarations();
  validateExactCopiedOwnedList(this, statementList_original, global_copy,
                               statementList_copy, help, "global-declaration");
  const SgDeclarationStatementPtrList originalDeclarations =
      statementList_original;
  const size_t declarationCount = originalDeclarations.size();

  for (size_t i = 0; i < declarationCount; ++i) {
    SgDeclarationStatement *originalDecl = originalDeclarations[i];
    if (originalDecl == NULL) {
      continue;
    }

    SgDeclarationStatement *copiedDecl =
        resolveCurrentCopiedDeclarationForFixup(originalDecl, help);
    ROSE_ASSERT(copiedDecl != NULL);
    originalDecl->fixupCopy_scopes(copiedDecl, help);
  }

  SgScopeStatement::fixupCopy_scopes(copy, help);
}

void SgExprStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgExprStatement *es_copy = isSgExprStatement(copy);
  ROSE_ASSERT(es_copy != NULL);

  SgExpression *expression_original = this->get_expression();
  SgExpression *expression_copy = es_copy->get_expression();

  expression_original->fixupCopy_scopes(expression_copy, help);

  SgStatement::fixupCopy_scopes(copy, help);
}

void SgBasicBlock::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgBasicBlock *block_copy = isSgBasicBlock(copy);
  ROSE_ASSERT(block_copy != NULL);

  const SgStatementPtrList &statementList_original = this->getStatementList();
  const SgStatementPtrList &statementList_copy = block_copy->getStatementList();

  if (this->containsOnlyDeclarations()) {
    ROSE_ASSERT(this->getDeclarationList().size() ==
                statementList_original.size());
  }
  ROSE_ASSERT(statementList_original.size() == statementList_copy.size());

  SgStatementPtrList::const_iterator i_original =
      statementList_original.begin();
  SgStatementPtrList::const_iterator i_copy = statementList_copy.begin();

  // Iterate over both lists to match up the correct pairs of SgStatement
  // objects
  while (i_original != statementList_original.end()) {
    (*i_original)->fixupCopy_scopes(*i_copy, help);

    i_original++;
    i_copy++;
  }

  SgScopeStatement::fixupCopy_scopes(copy, help);
}

void SgDeclarationStatement::fixupCopy_scopes(SgNode *copy,
                                              SgCopyHelp &help) const {
  if (this == copy) {
    return;
  }

  // Fixup must only walk original nodes. Re-entering an already-copied
  // declaration as if it were an original declaration manufactures
  // copy-of-copy map chains and eventually pairs declarations with deleted
  // second-generation copies.
  if (isCopiedNodeValue(help, this)) {
    return;
  }

  SgDeclarationStatement *copyDeclarationStatement =
      isSgDeclarationStatement(copy);
  if (copyDeclarationStatement == NULL) {
    copyDeclarationStatement = resolveCopiedReferencedDeclaration(this, help);
    copy = copyDeclarationStatement;
  }
  ROSE_ASSERT(copyDeclarationStatement != NULL);
  if (this == copyDeclarationStatement) {
    return;
  }

  if (help.isFixupNodePairCompleted(this, copyDeclarationStatement)) {
    return;
  }

  if (!help.beginFixupNodePair(this, copyDeclarationStatement)) {
    return;
  }

  struct DeclarationFixupGuard {
    SgCopyHelp &help;
    const SgDeclarationStatement *original;
    SgDeclarationStatement *copy;

    ~DeclarationFixupGuard() { help.completeFixupNodePair(original, copy); }
  } declarationFixupGuard{help, this, copyDeclarationStatement};
  (void)declarationFixupGuard;

  // Declaration-chain fields are semantic edges, not structural ownership.
  // The generated copy initially preserves each source pointer. Remap an edge
  // only when its target belongs to this structural copy transaction; an
  // exact source pointer is the sole valid representation of an external
  // semantic target. Fixup must never manufacture a declaration merely to
  // satisfy one of these links.
  const SgDeclarationStatement *originalDefiningDeclaration =
      this->get_definingDeclaration();
  const SgDeclarationStatement *originalFirstNondefiningDeclaration =
      this->get_firstNondefiningDeclaration();
  if (originalDefiningDeclaration != NULL &&
      originalFirstNondefiningDeclaration != NULL &&
      originalDefiningDeclaration->variantT() !=
          originalFirstNondefiningDeclaration->variantT()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[declaration-chain-input-kind]: "
            "original=%p/%s defining=%p/%s first=%p/%s\n",
            static_cast<const void *>(this), this->class_name().c_str(),
            static_cast<const void *>(originalDefiningDeclaration),
            originalDefiningDeclaration->class_name().c_str(),
            static_cast<const void *>(originalFirstNondefiningDeclaration),
            originalFirstNondefiningDeclaration->class_name().c_str());
    ROSE_ABORT();
  }

  SgDeclarationStatement *resolvedDefiningDeclaration =
      isSgDeclarationStatement(remapExactCopiedOrExternalSemanticEdge(
          originalDefiningDeclaration,
          copyDeclarationStatement->get_definingDeclaration(), help,
          "defining-declaration"));
  SgDeclarationStatement *resolvedFirstNondefiningDeclaration =
      isSgDeclarationStatement(remapExactCopiedOrExternalSemanticEdge(
          originalFirstNondefiningDeclaration,
          copyDeclarationStatement->get_firstNondefiningDeclaration(), help,
          "first-nondefining-declaration"));
  if ((originalDefiningDeclaration != NULL &&
       resolvedDefiningDeclaration == NULL) ||
      (originalFirstNondefiningDeclaration != NULL &&
       resolvedFirstNondefiningDeclaration == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[declaration-chain-output-kind]: "
            "original=%p/%s defining=%p first=%p copy=%p defining=%p "
            "first=%p\n",
            static_cast<const void *>(this), this->class_name().c_str(),
            static_cast<const void *>(originalDefiningDeclaration),
            static_cast<const void *>(originalFirstNondefiningDeclaration),
            static_cast<void *>(copyDeclarationStatement),
            static_cast<void *>(resolvedDefiningDeclaration),
            static_cast<void *>(resolvedFirstNondefiningDeclaration));
    ROSE_ABORT();
  }
  copyDeclarationStatement->set_definingDeclaration(
      resolvedDefiningDeclaration);
  copyDeclarationStatement->set_firstNondefiningDeclaration(
      resolvedFirstNondefiningDeclaration);

  // Dedicated declaration scopes and template parameters are structural
  // children in the ROSETTA schema.  A copy must therefore contain their exact
  // generated copies before fixup starts; fixup only validates and recursively
  // resolves semantic edges inside those children.
  validateExactCopiedOwnedDeclarationScope(this, copyDeclarationStatement,
                                           help);
  validateExactCopiedTemplateParameters(this, copyDeclarationStatement, help);

  if (this->hasExplicitScope() == true) {
    SgScopeStatement *originalScope = this->get_scope();
    if (originalScope == NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[explicit-declaration-scope]: original=%p "
              "type=%s scope=null copy=%p parent=%p\n",
              static_cast<const void *>(this), this->class_name().c_str(),
              static_cast<void *>(copyDeclarationStatement),
              static_cast<void *>(copyDeclarationStatement->get_parent()));
      ROSE_ABORT();
    }

    requireExactCopiedOrExternalSemanticEdge(
        originalScope, copyDeclarationStatement->get_scope(), help,
        "explicit-declaration-scope");
  }

  // DQ (10/19/2007): Added test...
  if (this->get_definingDeclaration() != NULL &&
      this->get_firstNondefiningDeclaration() != NULL) {
    // DQ (11/6/2007): If these are in the same namespace but different in
    // different instances of the namespace definition
    // (SgNamespaceDefinitionStatement objects), then the test for the same
    // scope is more complex.
    SgNamespaceDefinitionStatement *definingNamespace =
        isSgNamespaceDefinitionStatement(
            this->get_definingDeclaration()->get_scope());
    SgNamespaceDefinitionStatement *firstNondefiningNamespace =
        isSgNamespaceDefinitionStatement(
            this->get_firstNondefiningDeclaration()->get_scope());
    if (definingNamespace != NULL && firstNondefiningNamespace != NULL) {
      SgNamespaceDeclarationStatement *definingNamespaceDeclaration =
          definingNamespace->get_namespaceDeclaration();
      SgNamespaceDeclarationStatement *firstNondefiningNamespaceDeclaration =
          firstNondefiningNamespace->get_namespaceDeclaration();
      ROSE_ASSERT(definingNamespaceDeclaration != NULL);
      ROSE_ASSERT(firstNondefiningNamespaceDeclaration != NULL);
      const bool sameNamespaceDeclarationChain =
          definingNamespaceDeclaration->get_firstNondefiningDeclaration() ==
          firstNondefiningNamespaceDeclaration
              ->get_firstNondefiningDeclaration();
      const bool sameLogicalNamespace =
          definingNamespace->isSameNamespace(firstNondefiningNamespace) ||
          (definingNamespace->get_global_definition() != NULL &&
           definingNamespace->get_global_definition() ==
               firstNondefiningNamespace->get_global_definition());
      if (!sameNamespaceDeclarationChain && !sameLogicalNamespace) {
        fprintf(stderr,
                "Namespace declaration chain mismatch for %p = %s = %s\n", this,
                this->class_name().c_str(),
                SageInterface::get_name(this).c_str());
        fprintf(stderr, "  defining decl              = %p (%s)\n",
                this->get_definingDeclaration(),
                this->get_definingDeclaration()->class_name().c_str());
        fprintf(stderr, "  first nondef decl          = %p (%s)\n",
                this->get_firstNondefiningDeclaration(),
                this->get_firstNondefiningDeclaration()->class_name().c_str());
        fprintf(stderr, "  defining namespace def     = %p\n",
                definingNamespace);
        fprintf(stderr, "  first nondef namespace def = %p\n",
                firstNondefiningNamespace);
        fprintf(stderr,
                "  defining namespace decl    = %p first-nondef=%p scope=%p\n",
                definingNamespaceDeclaration,
                definingNamespaceDeclaration->get_firstNondefiningDeclaration(),
                definingNamespaceDeclaration->get_scope());
        fprintf(stderr,
                "  first nondef namespace decl= %p first-nondef=%p scope=%p\n",
                firstNondefiningNamespaceDeclaration,
                firstNondefiningNamespaceDeclaration
                    ->get_firstNondefiningDeclaration(),
                firstNondefiningNamespaceDeclaration->get_scope());
        fflush(stderr);
      }
      ROSE_ASSERT(sameNamespaceDeclarationChain || sameLogicalNamespace);
    } else {
      const bool hiddenFriendLexicalSemanticScopePair =
          isHiddenFriendLexicalSemanticScopePair(
              this->get_definingDeclaration(),
              this->get_firstNondefiningDeclaration());
      if (!hiddenFriendLexicalSemanticScopePair &&
          this->get_definingDeclaration()->get_scope()->variantT() !=
              this->get_firstNondefiningDeclaration()
                  ->get_scope()
                  ->variantT()) {
        printf("SCOPE VARIANT MISMATCH: this = %p = %s = %s \n", this,
               this->class_name().c_str(),
               SageInterface::get_name(this).c_str());
        printf("     this->get_definingDeclaration()         = %p \n",
               this->get_definingDeclaration());
        printf("     this->get_firstNondefiningDeclaration() = %p \n",
               this->get_firstNondefiningDeclaration());
        printf(
            "     defining scope         = %p = %s (variant %d)\n",
            this->get_definingDeclaration()->get_scope(),
            this->get_definingDeclaration()->get_scope()->class_name().c_str(),
            this->get_definingDeclaration()->get_scope()->variantT());
        printf(
            "     non-defining scope     = %p = %s (variant %d)\n",
            this->get_firstNondefiningDeclaration()->get_scope(),
            this->get_firstNondefiningDeclaration()
                ->get_scope()
                ->class_name()
                .c_str(),
            this->get_firstNondefiningDeclaration()->get_scope()->variantT());
      }
      ROSE_ASSERT(
          hiddenFriendLexicalSemanticScopePair ||
          this->get_definingDeclaration()->get_scope()->variantT() ==
              this->get_firstNondefiningDeclaration()->get_scope()->variantT());
    }
  }

  if (copyDeclarationStatement->get_definingDeclaration() != NULL &&
      copyDeclarationStatement->get_firstNondefiningDeclaration() != NULL) {
    ROSE_ASSERT(copyDeclarationStatement->get_firstNondefiningDeclaration()
                    ->get_definingDeclaration() != NULL);
    ROSE_ASSERT(copyDeclarationStatement->get_definingDeclaration()
                    ->get_firstNondefiningDeclaration() != NULL);
  }

  // Call the base class fixupCopy member function (this will setup the
  // parent)
  SgStatement::fixupCopy_scopes(copy, help);
}

void SgFunctionDeclaration::fixupCopy_scopes(SgNode *copy,
                                             SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

  SgFunctionDeclaration *functionDeclaration_copy =
      isSgFunctionDeclaration(copy);
  ROSE_ASSERT(functionDeclaration_copy != NULL);

  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  validateExactCopiedFunctionDeclaratorScope(this, functionDeclaration_copy,
                                             help);

  SgFunctionParameterScope *originalParameterScope =
      get_functionParameterScope();
  if (originalParameterScope != NULL) {
    SgFunctionParameterScope *copiedParameterScope = isSgFunctionParameterScope(
        lookupCopiedNode(help, originalParameterScope));
    const bool ownsParameterScope =
        originalParameterScope->get_parent() == this;
    SgFunctionParameterScope *expectedParameterScope =
        copiedParameterScope != NULL ? copiedParameterScope
                                     : originalParameterScope;
    if (functionDeclaration_copy->get_functionParameterScope() !=
            expectedParameterScope ||
        (ownsParameterScope &&
         (copiedParameterScope == NULL ||
          copiedParameterScope == originalParameterScope ||
          copiedParameterScope->get_parent() != functionDeclaration_copy))) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[function-parameter-scope-copy]: "
              "function=%p copy=%p scope=%p owner=%d mapped=%p\n",
              static_cast<const void *>(this),
              static_cast<void *>(functionDeclaration_copy),
              static_cast<void *>(originalParameterScope),
              ownsParameterScope ? 1 : 0,
              static_cast<void *>(copiedParameterScope));
      ROSE_ABORT();
    }
  } else if (functionDeclaration_copy->get_functionParameterScope() != NULL) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[function-parameter-scope-copy]: "
            "function=%p has no source scope but copy=%p owns scope=%p\n",
            static_cast<const void *>(this),
            static_cast<void *>(functionDeclaration_copy),
            static_cast<void *>(
                functionDeclaration_copy->get_functionParameterScope()));
    ROSE_ABORT();
  }

  SgFunctionType *sourceSyntaxType = get_type_syntax();
  if (get_type_syntax_is_available() != (sourceSyntaxType != NULL) ||
      (sourceSyntaxType != NULL && (sourceSyntaxType == get_type() ||
                                    sourceSyntaxType->get_parent() != this))) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[function-type-syntax-copy]: function=%p/%s "
            "has malformed source syntax type=%p available=%d owner=%p\n",
            static_cast<const void *>(this), get_name().str(),
            static_cast<void *>(sourceSyntaxType),
            get_type_syntax_is_available() ? 1 : 0,
            static_cast<void *>(sourceSyntaxType != NULL
                                    ? sourceSyntaxType->get_parent()
                                    : NULL));
    ROSE_ABORT();
  }
  SgFunctionType *copiedSyntaxType =
      functionDeclaration_copy->get_type_syntax();
  SgFunctionParameterTypeList *sourceSyntaxArguments =
      sourceSyntaxType != NULL ? sourceSyntaxType->get_argument_list() : NULL;
  SgFunctionParameterTypeList *copiedSyntaxArguments =
      copiedSyntaxType != NULL ? copiedSyntaxType->get_argument_list() : NULL;
  const SgMemberFunctionType *sourceSyntaxMember =
      isSgMemberFunctionType(sourceSyntaxType);
  SgMemberFunctionType *copiedSyntaxMember =
      isSgMemberFunctionType(copiedSyntaxType);
  const bool exactSyntaxCopy =
      sourceSyntaxType == NULL
          ? copiedSyntaxType == NULL
          : copiedSyntaxType != NULL &&
                lookupCopiedNode(help, sourceSyntaxType) == copiedSyntaxType &&
                copiedSyntaxType != sourceSyntaxType &&
                copiedSyntaxType->get_parent() == functionDeclaration_copy &&
                sourceSyntaxArguments != NULL &&
                sourceSyntaxArguments->get_parent() == sourceSyntaxType &&
                copiedSyntaxArguments != NULL &&
                copiedSyntaxArguments != sourceSyntaxArguments &&
                copiedSyntaxArguments->get_parent() == copiedSyntaxType &&
                copiedSyntaxArguments->get_arguments().size() ==
                    sourceSyntaxArguments->get_arguments().size() &&
                copiedSyntaxType->get_has_ellipses() ==
                    sourceSyntaxType->get_has_ellipses() &&
                copiedSyntaxType->get_fortran_source_syntax() ==
                    sourceSyntaxType->get_fortran_source_syntax() &&
                ((sourceSyntaxMember != NULL) ==
                 (copiedSyntaxMember != NULL)) &&
                (sourceSyntaxMember == NULL ||
                 copiedSyntaxMember->get_mfunc_specifier() ==
                     sourceSyntaxMember->get_mfunc_specifier());
  if (!exactSyntaxCopy ||
      functionDeclaration_copy->get_type_syntax_is_available() !=
          (copiedSyntaxType != NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[function-type-syntax-copy]: function=%p/%s "
            "syntax=%p arguments=%p copy=%p syntax=%p owner=%p arguments=%p "
            "mapped=%p was not published by the copy constructor\n",
            static_cast<const void *>(this), get_name().str(),
            static_cast<void *>(sourceSyntaxType),
            static_cast<void *>(sourceSyntaxArguments),
            static_cast<void *>(functionDeclaration_copy),
            static_cast<void *>(copiedSyntaxType),
            static_cast<void *>(copiedSyntaxType != NULL
                                    ? copiedSyntaxType->get_parent()
                                    : NULL),
            static_cast<void *>(copiedSyntaxArguments),
            static_cast<void *>(sourceSyntaxType != NULL
                                    ? lookupCopiedNode(help, sourceSyntaxType)
                                    : NULL));
    ROSE_ABORT();
  }

  SgFunctionParameterList *sourceSyntaxParameters = get_parameterList_syntax();
  SgFunctionParameterList *copiedSyntaxParameters = NULL;
  if (sourceSyntaxParameters == get_parameterList()) {
    copiedSyntaxParameters = functionDeclaration_copy->get_parameterList();
  } else if (sourceSyntaxParameters != NULL) {
    if (sourceSyntaxParameters->get_parent() != this) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[function-parameter-syntax-copy]: "
              "function=%p/%s syntax-list=%p has owner=%p\n",
              static_cast<const void *>(this), get_name().str(),
              static_cast<void *>(sourceSyntaxParameters),
              static_cast<void *>(sourceSyntaxParameters->get_parent()));
      ROSE_ABORT();
    }
    SgNode *mappedSyntaxParameters =
        lookupCopiedNode(help, sourceSyntaxParameters);
    copiedSyntaxParameters = isSgFunctionParameterList(mappedSyntaxParameters);
    if (copiedSyntaxParameters == NULL ||
        copiedSyntaxParameters == sourceSyntaxParameters) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[function-parameter-syntax-copy]: "
              "function=%p/%s syntax-list=%p mapped=%p is not one fresh "
              "typed parameter list\n",
              static_cast<const void *>(this), get_name().str(),
              static_cast<void *>(sourceSyntaxParameters),
              static_cast<void *>(mappedSyntaxParameters));
      ROSE_ABORT();
    }
    ROSE_ASSERT(copiedSyntaxParameters->get_parent() ==
                functionDeclaration_copy);
  }
  ROSE_ASSERT(functionDeclaration_copy->get_parameterList_syntax() ==
              copiedSyntaxParameters);

  // Setup the scopes of the SgInitializedName objects in the paraleter list
  ROSE_ASSERT(get_parameterList() != NULL);
  get_parameterList()->fixupCopy_scopes(
      functionDeclaration_copy->get_parameterList(), help);
  if (functionDeclaration_copy->get_parameterList() == NULL ||
      functionDeclaration_copy->get_scope() == NULL ||
      functionDeclaration_copy->get_parameterList()->get_scope() !=
          functionDeclaration_copy->get_scope()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[function-parameter-scope]: function=%p "
            "scope=%p parameter-list=%p scope=%p\n",
            static_cast<void *>(functionDeclaration_copy),
            static_cast<void *>(functionDeclaration_copy->get_scope()),
            static_cast<void *>(functionDeclaration_copy->get_parameterList()),
            static_cast<void *>(
                functionDeclaration_copy->get_parameterList() != NULL
                    ? functionDeclaration_copy->get_parameterList()->get_scope()
                    : NULL));
    ROSE_ABORT();
  }

  if (this->get_definition() != NULL) {
    if (isForward() || functionDeclaration_copy->isForward() ||
        this->get_definition()->get_declaration() != this) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[defining-function-forward-state]: "
              "original=%p copy=%p original-forward=%d copy-forward=%d "
              "definition-declaration=%p\n",
              static_cast<const void *>(this),
              static_cast<void *>(functionDeclaration_copy),
              isForward() ? 1 : 0,
              functionDeclaration_copy->isForward() ? 1 : 0,
              static_cast<void *>(this->get_definition()->get_declaration()));
      ROSE_ABORT();
    }
    ROSE_ASSERT(isForward() == false);

    // For the outlining, our copyHelp object does not copy defining
    // function declarations and substitutes a non-defining declarations, so
    // if the copy has been built this way then skip trying to reset the
    // SgFunctionDefinition.
    if (functionDeclaration_copy->get_definition() != NULL) {
      this->get_definition()->fixupCopy_scopes(
          functionDeclaration_copy->get_definition(), help);
    }
  }
}

// DQ (10/6/2007): Added fixup function to set scopes not set properly by
// the ROSETTA generated copy!
void SgFunctionParameterList::fixupCopy_scopes(SgNode *copy,
                                               SgCopyHelp &help) const {

  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  SgFunctionParameterList *copyFunctionParameterList =
      isSgFunctionParameterList(copy);
  ROSE_ASSERT(copyFunctionParameterList != NULL);

  const SgInitializedNamePtrList &parameterList_original = this->get_args();
  SgInitializedNamePtrList &parameterList_copy =
      copyFunctionParameterList->get_args();
  ROSE_ASSERT(parameterList_original.size() == parameterList_copy.size());

  SgInitializedNamePtrList::const_iterator i_original =
      parameterList_original.begin();
  SgInitializedNamePtrList::iterator i_copy = parameterList_copy.begin();

  // Iterate over both lists to match up the correct pairs of
  // SgInitializedName objects
  while (i_original != parameterList_original.end()) {
    (*i_original)->fixupCopy_scopes(*i_copy, help);

    i_original++;
    i_copy++;
  }
}

void SgMemberFunctionDeclaration::fixupCopy_scopes(SgNode *copy,
                                                   SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

  SgMemberFunctionDeclaration *memberFunctionDeclaration_copy =
      isSgMemberFunctionDeclaration(copy);
  if (memberFunctionDeclaration_copy == NULL) {
    fprintf(
        stderr,
        "[copy-member] mismatch this=%p name=%s file=%s:%d:%d copy=%p "
        "copyClass=%s\n",
        this, this->get_name().str(),
        this->get_file_info() != NULL
            ? this->get_file_info()->get_filenameString().c_str()
            : "<null>",
        this->get_file_info() != NULL ? this->get_file_info()->get_line() : -1,
        this->get_file_info() != NULL ? this->get_file_info()->get_col() : -1,
        copy, copy != NULL ? copy->class_name().c_str() : "<null>");
    fflush(stderr);
  }
  ROSE_ASSERT(memberFunctionDeclaration_copy != NULL);

  // Rebind the copied member declaration's explicit semantic scope before
  // validating children whose derived scope delegates to that declaration.
  // In particular, SgCtorInitializerList::get_scope() is defined by its
  // owning member function and must never observe the shallow source scope.
  SgFunctionDeclaration::fixupCopy_scopes(copy, help);

  // Setup the scopes of the SgInitializedName objects in the paraleter list
  ROSE_ASSERT(get_CtorInitializerList() != NULL);
  validateExactCopiedMemberFunctionCtorInitializerSubtree(
      this, memberFunctionDeclaration_copy, help);
  get_CtorInitializerList()->fixupCopy_scopes(
      memberFunctionDeclaration_copy->get_CtorInitializerList(), help);

  // The associated class and semantic scope must select the same exact family.
  // A class outside the copy transaction retains its original identity.
  SgClassDeclaration *originalAssociatedClass =
      isSgClassDeclaration(get_associatedClassDeclaration());
  SgClassDeclaration *copiedAssociatedClass =
      originalAssociatedClass != NULL
          ? isSgClassDeclaration(remapExactCopiedOrExternalSemanticEdge(
                originalAssociatedClass,
                memberFunctionDeclaration_copy
                    ->get_associatedClassDeclaration(),
                help, "member-associated-class"))
          : NULL;
  SgClassDefinition *copiedSemanticClass =
      isSgClassDefinition(memberFunctionDeclaration_copy->get_scope());
  SgClassDeclaration *copiedScopeClass =
      copiedSemanticClass != NULL ? copiedSemanticClass->get_declaration()
                                  : NULL;
  SgClassDeclaration *copiedAssociatedCanonical =
      copiedAssociatedClass != NULL
          ? isSgClassDeclaration(
                copiedAssociatedClass->get_firstNondefiningDeclaration())
          : NULL;
  SgClassDeclaration *copiedScopeCanonical =
      copiedScopeClass != NULL
          ? isSgClassDeclaration(
                copiedScopeClass->get_firstNondefiningDeclaration())
          : NULL;
  if (originalAssociatedClass == NULL || copiedAssociatedClass == NULL ||
      (copiedSemanticClass != get_scope() &&
       copiedAssociatedClass == originalAssociatedClass) ||
      copiedSemanticClass == NULL || copiedScopeClass == NULL ||
      copiedAssociatedCanonical == NULL || copiedScopeCanonical == NULL ||
      copiedAssociatedCanonical != copiedScopeCanonical) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[member-associated-class]: original=%p member=%p "
        "associated=%p copy=%p associated=%p scope=%p scope-class=%p "
        "does not identify one exact class family\n",
        static_cast<const void *>(this),
        static_cast<const void *>(originalAssociatedClass),
        static_cast<const void *>(originalAssociatedClass),
        static_cast<void *>(memberFunctionDeclaration_copy),
        static_cast<void *>(copiedAssociatedClass),
        static_cast<void *>(copiedSemanticClass),
        static_cast<void *>(copiedScopeClass));
    ROSE_ABORT();
  }
  memberFunctionDeclaration_copy->set_associatedClassDeclaration(
      copiedAssociatedClass);
  if (memberFunctionDeclaration_copy->get_associatedClassDeclaration() !=
      copiedAssociatedClass) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[member-associated-class]: copy=%p failed to "
            "publish associated class=%p\n",
            static_cast<void *>(memberFunctionDeclaration_copy),
            static_cast<void *>(copiedAssociatedClass));
    ROSE_ABORT();
  }
}

void SgTemplateDeclaration::fixupCopy_scopes(SgNode *copy,
                                             SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

  SgTemplateDeclaration *templateDeclaration_copy =
      isSgTemplateDeclaration(copy);
  ROSE_ASSERT(templateDeclaration_copy != NULL);

  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  // I don't think that this applies to the defining declaration...
  if (this != this->get_definingDeclaration()) {
    ROSE_ASSERT(templateDeclaration_copy->get_firstNondefiningDeclaration()
                    ->get_definingDeclaration() ==
                templateDeclaration_copy->get_definingDeclaration());
  }
}

void SgTemplateInstantiationDefn::fixupCopy_scopes(SgNode *copy,
                                                   SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

  ROSE_ASSERT(this->get_declaration() != NULL);

  SgClassDefinition::fixupCopy_scopes(copy, help);
}

void SgTemplateInstantiationDecl::fixupCopy_scopes(SgNode *copy,
                                                   SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

  SgTemplateInstantiationDecl *templateClassDeclaration_copy =
      isSgTemplateInstantiationDecl(copy);
  ROSE_ASSERT(templateClassDeclaration_copy != NULL);

  SgClassDeclaration::fixupCopy_scopes(copy, help);

  ROSE_ASSERT(this->get_templateDeclaration() != NULL);
  ROSE_ASSERT(templateClassDeclaration_copy->get_templateDeclaration() != NULL);

}

void SgTemplateInstantiationMemberFunctionDecl::fixupCopy_scopes(
    SgNode *copy, SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

  SgTemplateInstantiationMemberFunctionDecl
      *templateMemberFunctionDeclaration_copy =
          isSgTemplateInstantiationMemberFunctionDecl(copy);
  ROSE_ASSERT(templateMemberFunctionDeclaration_copy != NULL);

  SgMemberFunctionDeclaration::fixupCopy_scopes(copy, help);

  if (this->get_templateDeclaration() == NULL) {
    // A SgTemplateInstantiationMemberFunctionDecl can also represent a
    // non-template member function of a class-template specialization. In
    // that case there is no SgTemplateMemberFunctionDeclaration for the
    // function itself; class-template arguments remain on the instantiation
    // node.
    templateMemberFunctionDeclaration_copy->set_templateDeclaration(NULL);
    templateMemberFunctionDeclaration_copy->set_specializedTemplateDeclaration(
        NULL);
    return;
  }
  ROSE_ASSERT(this->get_templateDeclaration() != NULL);
  ROSE_ASSERT(
      templateMemberFunctionDeclaration_copy->get_templateDeclaration() !=
      NULL);

}

void SgTemplateInstantiationFunctionDecl::fixupCopy_scopes(
    SgNode *copy, SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

  // DQ (12/28/2012): Added asseretion.

  SgTemplateInstantiationFunctionDecl *templateFunctionDeclaration_copy =
      isSgTemplateInstantiationFunctionDecl(copy);
  ROSE_ASSERT(templateFunctionDeclaration_copy != NULL);

  SgFunctionDeclaration::fixupCopy_scopes(copy, help);

  ROSE_ASSERT(this->get_templateDeclaration() != NULL);
  ROSE_ASSERT(templateFunctionDeclaration_copy->get_templateDeclaration() !=
              NULL);

}

void SgFunctionDefinition::fixupCopy_scopes(SgNode *copy,
                                            SgCopyHelp &help) const {

  SgFunctionDefinition *functionDefinition_copy = isSgFunctionDefinition(copy);
  ROSE_ASSERT(functionDefinition_copy != NULL);

  // Setup the scopes of the SgInitializedName objects in the paraleter list
  ROSE_ASSERT(get_body() != NULL);
  ROSE_ASSERT(functionDefinition_copy->get_body() != NULL);
  ROSE_ASSERT(functionDefinition_copy->get_body()->get_parent() ==
              functionDefinition_copy);
  get_body()->fixupCopy_scopes(functionDefinition_copy->get_body(), help);

  SgScopeStatement::fixupCopy_scopes(copy, help);
}

void SgVariableDeclaration::fixupCopy_scopes(SgNode *copy,
                                             SgCopyHelp &help) const {

  SgDeclarationStatement::fixupCopy_scopes(copy, help);

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
            "REX_COPY_INVARIANT[variable-base-type-forward-null]: "
            "source=%p child=%p copy=%p child=%p\n",
            static_cast<const void *>(this),
            static_cast<void *>(baseTypeNondefiningOriginal),
            static_cast<void *>(variableDeclaration_copy),
            static_cast<void *>(baseTypeNondefiningCopy));
    ROSE_ABORT();
  }
  if (baseTypeNondefiningOriginal != NULL) {
    requireExactCopiedOwnedChild(
        this, baseTypeNondefiningOriginal, variableDeclaration_copy,
        baseTypeNondefiningCopy, help, "variable-base-type-forward");
    baseTypeNondefiningOriginal->fixupCopy_scopes(baseTypeNondefiningCopy,
                                                  help);
  }

  // Preserve the exact inline base-type definition edge in the copied AST.
  if (this->get_baseTypeDefiningDeclaration() != NULL) {
    ROSE_ASSERT(variableDeclaration_copy->get_baseTypeDefiningDeclaration() !=
                NULL);
    SgDeclarationStatement *baseTypeDeclaration_original =
        this->get_baseTypeDefiningDeclaration();
    SgDeclarationStatement *baseTypeDeclaration_copy =
        variableDeclaration_copy->get_baseTypeDefiningDeclaration();

    ROSE_ASSERT(baseTypeDeclaration_original != NULL);
    ROSE_ASSERT(baseTypeDeclaration_copy != NULL);

    baseTypeDeclaration_original->fixupCopy_scopes(baseTypeDeclaration_copy,
                                                   help);
  }

  const SgInitializedNamePtrList &variableList_original = this->get_variables();
  SgInitializedNamePtrList &variableList_copy =
      variableDeclaration_copy->get_variables();

  ROSE_ASSERT(variableList_original.size() == variableList_copy.size());

  SgInitializedNamePtrList::const_iterator i_original =
      variableList_original.begin();
  SgInitializedNamePtrList::iterator i_copy = variableList_copy.begin();

  // Iterate over both lists to match up the correct pairs of
  // SgInitializedName objects
  while (i_original != variableList_original.end()) {
    ROSE_ASSERT((*i_copy)->get_declptr() != NULL);
    (*i_original)->fixupCopy_scopes(*i_copy, help);

    i_original++;
    i_copy++;
  }
}

void SgDeclarationGroupStatement::fixupCopy_scopes(SgNode *copy,
                                                   SgCopyHelp &help) const {
  if (this == copy) {
    return;
  }

  SgDeclarationGroupStatement *copiedGroup =
      isSgDeclarationGroupStatement(copy);
  if (copiedGroup == NULL) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[declaration-group-kind]: source=%p copy=%p/%s\n",
        static_cast<const void *>(this), static_cast<void *>(copy),
        copy != NULL ? copy->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }

  SgDeclarationStatement::fixupCopy_scopes(copiedGroup, help);

  const SgDeclarationStatementPtrList &sourceMembers = get_declarations();
  const SgDeclarationStatementPtrList &copiedMembers =
      copiedGroup->get_declarations();
  if (sourceMembers.size() != copiedMembers.size()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[declaration-group-members]: source=%p "
            "members=%zu copy=%p members=%zu\n",
            static_cast<const void *>(this), sourceMembers.size(),
            static_cast<void *>(copiedGroup), copiedMembers.size());
    ROSE_ABORT();
  }

  for (size_t index = 0; index < sourceMembers.size(); ++index) {
    SgDeclarationStatement *sourceMember = sourceMembers[index];
    SgDeclarationStatement *copiedMember = copiedMembers[index];
    if (sourceMember == NULL || copiedMember == NULL ||
        sourceMember->get_parent() != this ||
        copiedMember->get_parent() != copiedGroup ||
        std::count(sourceMembers.begin(), sourceMembers.end(), sourceMember) !=
            1 ||
        std::count(copiedMembers.begin(), copiedMembers.end(), copiedMember) !=
            1 ||
        requireExactMappedDeclaration(
            sourceMember, help, "declaration-group-member") != copiedMember) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[declaration-group-member]: source=%p "
              "copy=%p index=%zu source-member=%p parent=%p "
              "copy-member=%p parent=%p mapped=%p\n",
              static_cast<const void *>(this), static_cast<void *>(copiedGroup),
              index, static_cast<void *>(sourceMember),
              static_cast<void *>(
                  sourceMember != NULL ? sourceMember->get_parent() : NULL),
              static_cast<void *>(copiedMember),
              static_cast<void *>(
                  copiedMember != NULL ? copiedMember->get_parent() : NULL),
              static_cast<void *>(lookupCopiedNode(help, sourceMember)));
      ROSE_ABORT();
    }
    sourceMember->fixupCopy_scopes(copiedMember, help);
  }

  copiedGroup->validate();
}

void SgClassDeclaration::fixupCopy_scopes(SgNode *copy,
                                          SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    for (const SgNode *source : help.get_originalNodesForCopy(this)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[class-fixup-source-is-copy]: source=%p/%s "
              "requested-copy=%p/%s is also mapped from %p/%s identity=%d\n",
              static_cast<const void *>(this), this->class_name().c_str(),
              static_cast<void *>(copy),
              copy != NULL ? copy->class_name().c_str() : "<null>",
              static_cast<const void *>(source),
              source != NULL ? source->class_name().c_str() : "<null>",
              source == this ? 1 : 0);
    }
    ROSE_ABORT();
  }

  SgClassDeclaration *classDeclaration_copy = isSgClassDeclaration(copy);
  ROSE_ASSERT(classDeclaration_copy != NULL);

  SgClassDefinition *classDefinition_original = this->get_definition();
  SgClassDefinition *classDefinition_copy =
      classDeclaration_copy->get_definition();

  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  if (isForward() == false) {
    ROSE_ASSERT(classDefinition_original != NULL);
    ROSE_ASSERT(classDefinition_copy != NULL);

    classDefinition_original->fixupCopy_scopes(classDefinition_copy, help);
  }

  const SgClassDeclaration *classDeclaration_original =
      isSgClassDeclaration(this);

  SgClassDeclaration *classDeclaration_copy_firstNondefining =
      isSgClassDeclaration(
          classDeclaration_copy->get_firstNondefiningDeclaration());
  SgClassDeclaration *classDeclaration_copy_defining =
      isSgClassDeclaration(classDeclaration_copy->get_definingDeclaration());
  ROSE_ASSERT(classDeclaration_copy_firstNondefining != NULL);

  SgClassDeclaration *classDeclaration_original_firstNondefining =
      isSgClassDeclaration(
          classDeclaration_original->get_firstNondefiningDeclaration());
  SgClassDeclaration *classDeclaration_original_defining = isSgClassDeclaration(
      classDeclaration_original->get_definingDeclaration());
  ROSE_ASSERT(classDeclaration_original_firstNondefining != NULL);

  SgClassType *classType = finalizeExactCopiedClassType(
      classDeclaration_original, classDeclaration_copy, help);
  ROSE_ASSERT(classType != NULL);

  if (classDeclaration_copy_defining != NULL) {
    ROSE_ASSERT(classDeclaration_copy_firstNondefining->get_type() ==
                classDeclaration_copy_defining->get_type());
  }

  if (classDeclaration_original_defining != NULL) {
    if (classDeclaration_original_firstNondefining->get_type() !=
        classDeclaration_original_defining->get_type()) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[class-canonical-type]: first=%p type=%p "
          "defining=%p type=%p are not one canonical class type\n",
          static_cast<void *>(classDeclaration_original_firstNondefining),
          static_cast<void *>(
              classDeclaration_original_firstNondefining->get_type()),
          static_cast<void *>(classDeclaration_original_defining),
          static_cast<void *>(classDeclaration_original_defining->get_type()));
      ROSE_ABORT();
    }
  }
}

void SgClassDefinition::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  // Guard against shared nodes. SgClassDefinition nodes from non-defining
  // declarations are shared.
  if (this == copy) {
    // Node is shared, not copied. Skip fixup to avoid corrupting the
    // original.
    return;
  }

  if (isCopiedNodeValue(help, this)) {
    return;
  }

  ROSE_ASSERT(this->get_declaration() != NULL);

  SgClassDefinition *classDefinition_copy = isSgClassDefinition(copy);
  ROSE_ASSERT(classDefinition_copy != NULL);

  SgBaseClassPtrList::const_iterator i_original =
      this->get_inheritances().begin();
  SgBaseClassPtrList::iterator i_copy =
      classDefinition_copy->get_inheritances().begin();
  ROSE_ASSERT(this->get_inheritances().size() ==
              classDefinition_copy->get_inheritances().size());

  while (i_original != this->get_inheritances().end()) {
    // Check the parent pointer to make sure it is properly set
    ROSE_ASSERT((*i_original)->get_parent() != NULL);
    ROSE_ASSERT((*i_original)->get_parent() == this);

    (*i_original)->fixupCopy_scopes(*i_copy, help);

    (*i_copy)->set_parent(classDefinition_copy);

    ROSE_ASSERT((*i_copy)->get_parent() != NULL);
    ROSE_ASSERT((*i_copy)->get_parent() == classDefinition_copy);

    i_original++;
    i_copy++;
  }

  const SgDeclarationStatementPtrList &statementList_original =
      this->get_members();
  SgDeclarationStatementPtrList &statementList_copy =
      classDefinition_copy->get_members();
  validateExactCopiedOwnedList(this, statementList_original,
                               classDefinition_copy, statementList_copy, help,
                               "class-member");
  const SgDeclarationStatementPtrList originalDeclarations =
      statementList_original;
  const size_t declarationCount = originalDeclarations.size();

  for (size_t i = 0; i < declarationCount; ++i) {
    SgDeclarationStatement *originalDecl = originalDeclarations[i];
    if (originalDecl == NULL) {
      continue;
    }

    SgDeclarationStatement *copiedDecl =
        resolveCurrentCopiedDeclarationForFixup(originalDecl, help);
    ROSE_ASSERT(copiedDecl != NULL);
    originalDecl->fixupCopy_scopes(copiedDecl, help);
  }

  SgScopeStatement::fixupCopy_scopes(copy, help);
}

void SgBaseClass::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  auto *copiedBase = isSgBaseClass(copy);
  ROSE_ASSERT(copiedBase != nullptr && copiedBase->variantT() == variantT());
  if (auto *sourceType = get_source_type()) {
    requireExactCopiedOrExternalSemanticEdge(
        sourceType, copiedBase->get_source_type(), help, "base-source-type");
  } else {
    ROSE_ASSERT(copiedBase->get_source_type() == nullptr);
  }
  if (auto *declaration = get_base_class()) {
    ROSE_ASSERT(isSgNonrealBaseClass(this) == nullptr &&
                isSgExpBaseClass(this) == nullptr);
    requireExactCopiedOrExternalSemanticEdge(declaration,
                                             copiedBase->get_base_class(), help,
                                             "base-class-declaration");
  } else if (auto *nonreal = isSgNonrealBaseClass(this)) {
    auto *copiedNonreal = isSgNonrealBaseClass(copy);
    ASSERT_not_null(nonreal->get_base_class_nonreal());
    ASSERT_not_null(copiedNonreal);
    requireExactCopiedOrExternalSemanticEdge(
        nonreal->get_base_class_nonreal(),
        copiedNonreal->get_base_class_nonreal(), help,
        "nonreal-base-class-declaration");
  } else if (isSgExpBaseClass(this) != nullptr) {
    auto *sourceType = isSgDeclType(get_source_type());
    auto *copiedType = isSgDeclType(copiedBase->get_source_type());
    ASSERT_not_null(sourceType);
    ASSERT_not_null(copiedType);
    ASSERT_not_null(sourceType->get_base_expression());
    ROSE_ASSERT(copiedBase->get_base_class() == nullptr);
    ROSE_ASSERT(requireExactMappedNode(sourceType->get_base_expression(), help,
                                       "expression-base-operand") ==
                copiedType->get_base_expression());
  } else {
    ROSE_ABORT();
  }
}

void SgLabelStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgLabelStatement *labelStatement_copy = isSgLabelStatement(copy);
  ROSE_ASSERT(labelStatement_copy != NULL);

  ROSE_ASSERT(this->hasExplicitScope() == true);

  // Make sure that the copy sets the scopes to be the same type
  ROSE_ASSERT(labelStatement_copy->get_scope()->variantT() ==
              this->get_scope()->variantT());

  SgStatement::fixupCopy_scopes(copy, help);
}

void SgGotoStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgGotoStatement *gotoStatement_copy = isSgGotoStatement(copy);
  ROSE_ASSERT(gotoStatement_copy != NULL);

  SgStatement::fixupCopy_scopes(copy, help);
}

void SgTypedefDeclaration::fixupCopy_scopes(SgNode *copy,
                                            SgCopyHelp &help) const {
  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  SgTypedefDeclaration *typedefDeclaration_copy = isSgTypedefDeclaration(copy);
  ROSE_ASSERT(typedefDeclaration_copy != NULL);

  // DQ (10/14/2007): Handle the case of a type defined in the base type of
  // the typedef (similar problem for SgVariableDeclaration).
  if (this->get_typedefBaseTypeContainsDefiningDeclaration() == true) {
    ROSE_ASSERT(typedefDeclaration_copy
                    ->get_typedefBaseTypeContainsDefiningDeclaration() == true);
    SgDeclarationStatement *baseTypeDeclaration_original =
        this->get_baseTypeDefiningDeclaration();
    SgDeclarationStatement *baseTypeDeclaration_copy =
        typedefDeclaration_copy->get_declaration();
    ROSE_ASSERT(baseTypeDeclaration_original != NULL);
    if (baseTypeDeclaration_copy == NULL ||
        baseTypeDeclaration_copy->get_parent() != typedefDeclaration_copy ||
        lookupCopiedNode(help, baseTypeDeclaration_original) !=
            baseTypeDeclaration_copy) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[typedef-owned-definition-map]: "
              "source=%p definition=%p copy=%p definition=%p owner=%p "
              "mapped=%p\n",
              static_cast<const void *>(this),
              static_cast<void *>(baseTypeDeclaration_original),
              static_cast<void *>(typedefDeclaration_copy),
              static_cast<void *>(baseTypeDeclaration_copy),
              baseTypeDeclaration_copy != NULL
                  ? static_cast<void *>(baseTypeDeclaration_copy->get_parent())
                  : NULL,
              static_cast<void *>(
                  lookupCopiedNode(help, baseTypeDeclaration_original)));
      ROSE_ABORT();
    }

    // The strict typedef getter validates the owned definition's semantic
    // scope and declaration chain.  Those edges still intentionally hold
    // their source values until the owned declaration itself is fixed up, so
    // traverse the already-validated structural copy edge first and only then
    // publish it through the strict accessor contract.
    baseTypeDeclaration_original->fixupCopy_scopes(baseTypeDeclaration_copy,
                                                   help);
    if (typedefDeclaration_copy->get_baseTypeDefiningDeclaration() !=
        baseTypeDeclaration_copy) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[typedef-owned-definition-final]: "
              "source=%p definition=%p copy=%p definition=%p\n",
              static_cast<const void *>(this),
              static_cast<void *>(baseTypeDeclaration_original),
              static_cast<void *>(typedefDeclaration_copy),
              static_cast<void *>(baseTypeDeclaration_copy));
      ROSE_ABORT();
    }
  } else if (get_declaration() != nullptr) {
    requireExactCopiedOrExternalSemanticEdge(
        get_declaration(), typedefDeclaration_copy->get_declaration(), help,
        "typedef-reference-declaration");
  } else {
    ROSE_ASSERT(typedefDeclaration_copy->get_declaration() == nullptr);
  }
}

void SgEnumDeclaration::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  SgEnumDeclaration *enumDeclaration_copy = isSgEnumDeclaration(copy);
  ROSE_ASSERT(enumDeclaration_copy != NULL);

  SgEnumType *enum_type_original = this->get_type();
  ROSE_ASSERT(enum_type_original != NULL);

  SgEnumType *enum_type_copy = enumDeclaration_copy->get_type();
  ROSE_ASSERT(enum_type_copy != NULL);

  finalizeExactCopiedEnumDeclarationChain(this, enumDeclaration_copy, help);

  if (enum_type_copy != enum_type_original) {
    SgDeclarationStatement *local_copy = isSgDeclarationStatement(
        requireExactMappedNode(enum_type_original->get_declaration(), help,
                               "enum-type-declaration"));
    ROSE_ASSERT(local_copy != NULL);
    ROSE_ASSERT(enum_type_copy->get_declaration() == local_copy);
  }

  validateExactCopiedEnumEnumerators(this, enumDeclaration_copy, help);

  // Now reset the enum fields.
  const SgInitializedNamePtrList &enumFieldList_original =
      this->get_enumerators();
  SgInitializedNamePtrList &enumFieldList_copy =
      enumDeclaration_copy->get_enumerators();

  SgInitializedNamePtrList::const_iterator i_original =
      enumFieldList_original.begin();
  SgInitializedNamePtrList::iterator i_copy = enumFieldList_copy.begin();

  // Iterate over both lists to match up the correct pairs of
  // SgInitializedName objects
  while (i_original != enumFieldList_original.end()) {
    (*i_original)->fixupCopy_scopes(*i_copy, help);

    i_original++;
    i_copy++;
  }
}

void SgNamespaceDeclarationStatement::fixupCopy_scopes(SgNode *copy,
                                                       SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  SgNamespaceDeclarationStatement *namespaceDeclaration_copy =
      isSgNamespaceDeclarationStatement(copy);
  if (namespaceDeclaration_copy == NULL ||
      requireExactMappedNode(this, help, "namespace-declaration") !=
          namespaceDeclaration_copy) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[namespace-declaration-copy]: original=%p "
            "argument-copy=%p mapped-copy=%p\n",
            static_cast<const void *>(this),
            static_cast<void *>(namespaceDeclaration_copy),
            static_cast<void *>(lookupCopiedNode(help, this)));
    ROSE_ABORT();
  }

  if (this->get_firstNondefiningDeclaration() == this) {
    namespaceDeclaration_copy->set_firstNondefiningDeclaration(
        namespaceDeclaration_copy);
  }

  SgNamespaceDefinitionStatement *namespaceDefinition_original =
      this->get_definition();
  SgNamespaceDefinitionStatement *namespaceDefinition_copy =
      namespaceDeclaration_copy->get_definition();

  ROSE_ASSERT(namespaceDefinition_original != NULL);
  ROSE_ASSERT(namespaceDefinition_copy != NULL);

  namespaceDefinition_original->fixupCopy_scopes(namespaceDefinition_copy,
                                                 help);
}

void SgNamespaceDefinitionStatement::fixupCopy_scopes(SgNode *copy,
                                                      SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

  SgNamespaceDefinitionStatement *namespaceDefinition_copy =
      isSgNamespaceDefinitionStatement(copy);
  ROSE_ASSERT(namespaceDefinition_copy != NULL);

  SgNamespaceDeclarationStatement *owner =
      namespaceDefinition_copy->get_namespaceDeclaration();
  ROSE_ASSERT(owner != NULL &&
              requireExactMappedNode(this->get_namespaceDeclaration(), help,
                                     "namespace-definition-owner") == owner &&
              owner->get_definition() == namespaceDefinition_copy &&
              namespaceDefinition_copy->get_parent() == owner);

  const SgDeclarationStatementPtrList &statementList_original =
      this->get_declarations();
  SgDeclarationStatementPtrList &statementList_copy =
      namespaceDefinition_copy->get_declarations();
  validateExactCopiedOwnedList(this, statementList_original,
                               namespaceDefinition_copy, statementList_copy,
                               help, "namespace-declaration");
  const SgDeclarationStatementPtrList originalDeclarations =
      statementList_original;
  const size_t declarationCount = originalDeclarations.size();

  for (size_t i = 0; i < declarationCount; ++i) {
    SgDeclarationStatement *originalDecl = originalDeclarations[i];
    if (originalDecl == NULL) {
      continue;
    }

    SgDeclarationStatement *copiedDecl =
        resolveCurrentCopiedDeclarationForFixup(originalDecl, help);
    ROSE_ASSERT(copiedDecl != NULL);
    originalDecl->fixupCopy_scopes(copiedDecl, help);
  }

  SgScopeStatement::fixupCopy_scopes(copy, help);
}

void SgTemplateInstantiationDirectiveStatement::fixupCopy_scopes(
    SgNode *copy, SgCopyHelp &help) const {

  SgTemplateInstantiationDirectiveStatement
      *templateInstantiationDirectiveStatement_copy =
          isSgTemplateInstantiationDirectiveStatement(copy);

  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  ROSE_ASSERT(this->get_declaration() != NULL);

  SgDeclarationStatement *declaration_copy =
      templateInstantiationDirectiveStatement_copy->get_declaration();
  ROSE_ASSERT(declaration_copy != NULL);
  if (declaration_copy == this->get_declaration() ||
      declaration_copy->get_parent() == this->get_declaration()->get_parent()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[explicit-instantiation-owner]: "
            "directive=%p copy=%p declaration=%p copy=%p reused source "
            "ownership\n",
            static_cast<const void *>(this),
            static_cast<void *>(templateInstantiationDirectiveStatement_copy),
            static_cast<void *>(this->get_declaration()),
            static_cast<void *>(declaration_copy));
    ROSE_ABORT();
  }
  if (declaration_copy->get_parent() == NULL) {
    declaration_copy->set_parent(templateInstantiationDirectiveStatement_copy);
  }
  if (declaration_copy->get_parent() !=
          templateInstantiationDirectiveStatement_copy ||
      countDirectOwnerEdges(templateInstantiationDirectiveStatement_copy,
                            declaration_copy) != 1) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[explicit-instantiation-owner]: copied "
            "directive=%p declaration=%p parent=%p has no exact structural "
            "edge\n",
            static_cast<void *>(templateInstantiationDirectiveStatement_copy),
            static_cast<void *>(declaration_copy),
            static_cast<void *>(declaration_copy->get_parent()));
    ROSE_ABORT();
  }
  this->get_declaration()->fixupCopy_scopes(declaration_copy, help);
}

void SgProject::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgProject *project_copy = isSgProject(copy);
  ROSE_ASSERT(project_copy != NULL);

  // Call fixup on all fo the files (SgFile objects)
  for (int i = 0; i < numberOfFiles(); i++) {
    SgFile &file = get_file(i);
    SgFile &file_copy = project_copy->get_file(i);
    file.fixupCopy_scopes(&file_copy, help);
  }
}

void SgSourceFile::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgSourceFile *file_copy = isSgSourceFile(copy);
  ROSE_ASSERT(file_copy != NULL);

  SgFileList *external = get_frontendExternalFileList();
  SgFileList *copiedExternal = file_copy->get_frontendExternalFileList();
  ROSE_ASSERT((external == nullptr) == (copiedExternal == nullptr));
  if (external != nullptr) {
    ROSE_ASSERT(lookupCopiedNode(help, external) == copiedExternal);
    ROSE_ASSERT(external->get_listOfFiles().size() ==
                copiedExternal->get_listOfFiles().size());
    for (SgFile *file : external->get_listOfFiles()) {
      auto *copiedFile = isSgFile(lookupCopiedNode(help, file));
      ASSERT_not_null(file);
      ASSERT_not_null(copiedFile);
      file->fixupCopy_scopes(copiedFile, help);
    }
  }

  ROSE_ASSERT(get_globalScope() != NULL);
  ROSE_ASSERT(file_copy->get_globalScope() != NULL);
  get_globalScope()->fixupCopy_scopes(file_copy->get_globalScope(), help);
}

void SgIfStmt::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  SgScopeStatement::fixupCopy_scopes(copy, help);

  SgIfStmt *ifStatement_copy = isSgIfStmt(copy);
  ROSE_ASSERT(ifStatement_copy != NULL);

  ROSE_ASSERT(get_if_statement_kind() ==
              ifStatement_copy->get_if_statement_kind());
  ROSE_ASSERT((get_conditional() == nullptr) ==
              get_is_if_consteval_statement());
  if (get_conditional() != nullptr)
    get_conditional()->fixupCopy_scopes(ifStatement_copy->get_conditional(),
                                        help);
  else
    ROSE_ASSERT(ifStatement_copy->get_conditional() == nullptr);

  SgStatement *thsTruBody = this->get_true_body();
  ROSE_ASSERT(thsTruBody != NULL);
  SgStatement *ifStmtCopyTruBody = ifStatement_copy->get_true_body();
  ROSE_ASSERT(ifStmtCopyTruBody != NULL);
  SgScopeStatement *scopeIfStmtCopyTruBody =
      isSgScopeStatement(ifStmtCopyTruBody);
  if (scopeIfStmtCopyTruBody != NULL) {
    if (scopeIfStmtCopyTruBody->symbol_table_size() != 0) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[if-true-symbol-table-state]: body=%p "
              "copied symbol table contains %zu entries before exact body "
              "finalization\n",
              static_cast<void *>(scopeIfStmtCopyTruBody),
              scopeIfStmtCopyTruBody->symbol_table_size());
      ROSE_ABORT();
    }
  }

  thsTruBody->fixupCopy_scopes(ifStmtCopyTruBody, help);

  ROSE_ASSERT((this->get_false_body() != NULL) ==
              (ifStatement_copy->get_false_body() != NULL));
  if (SgScopeStatement *scopeStmnt =
          isSgScopeStatement(ifStatement_copy->get_false_body())) {
    if (scopeStmnt->symbol_table_size() != 0) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[if-false-symbol-table-state]: body=%p "
              "copied symbol table contains %zu entries before exact body "
              "finalization\n",
              static_cast<void *>(scopeStmnt), scopeStmnt->symbol_table_size());
      ROSE_ABORT();
    }
  }

  if (this->get_false_body() != NULL) {
    this->get_false_body()->fixupCopy_scopes(ifStatement_copy->get_false_body(),
                                             help);
  }
}

void SgForStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgForStatement *forStatement_copy = isSgForStatement(copy);
  ROSE_ASSERT(forStatement_copy != NULL);

  // Fix the initializer before the copied loop scope builds its symbols.
  ROSE_ASSERT(this->get_for_init_stmt() != NULL);
  this->get_for_init_stmt()->fixupCopy_scopes(
      forStatement_copy->get_for_init_stmt(), help);

  SgScopeStatement::fixupCopy_scopes(copy, help);

  // Fix the condition after those symbols are available.
  ROSE_ASSERT(this->get_test() != NULL);
  this->get_test()->fixupCopy_scopes(forStatement_copy->get_test(), help);

  ROSE_ASSERT(this->get_increment() != NULL);
  this->get_increment()->fixupCopy_scopes(forStatement_copy->get_increment(),
                                          help);

  ROSE_ASSERT(this->get_loop_body() != NULL);
  this->get_loop_body()->fixupCopy_scopes(forStatement_copy->get_loop_body(),
                                          help);
}

void SgRangeBasedForStatement::fixupCopy_scopes(SgNode *copy,
                                                SgCopyHelp &help) const {

  SgRangeBasedForStatement *forStatement_copy =
      isSgRangeBasedForStatement(copy);
  ROSE_ASSERT(forStatement_copy != NULL);

  // Fix the iterator and range before the copied scope builds its symbols.
  ROSE_ASSERT(this->get_iterator_declaration() != NULL);
  this->get_iterator_declaration()->fixupCopy_scopes(
      forStatement_copy->get_iterator_declaration(), help);

  ROSE_ASSERT(this->get_range_declaration() != NULL);
  this->get_range_declaration()->fixupCopy_scopes(
      forStatement_copy->get_range_declaration(), help);

  SgScopeStatement::fixupCopy_scopes(copy, help);

  ROSE_ASSERT(this->get_begin_declaration() != NULL);
  this->get_begin_declaration()->fixupCopy_scopes(
      forStatement_copy->get_begin_declaration(), help);

  ROSE_ASSERT(this->get_end_declaration() != NULL);
  this->get_end_declaration()->fixupCopy_scopes(
      forStatement_copy->get_end_declaration(), help);

  ROSE_ASSERT(this->get_not_equal_expression() != NULL);
  this->get_not_equal_expression()->fixupCopy_scopes(
      forStatement_copy->get_not_equal_expression(), help);

  ROSE_ASSERT(this->get_increment_expression() != NULL);
  this->get_increment_expression()->fixupCopy_scopes(
      forStatement_copy->get_increment_expression(), help);

  ROSE_ASSERT(this->get_loop_body() != NULL);
  this->get_loop_body()->fixupCopy_scopes(forStatement_copy->get_loop_body(),
                                          help);
}

void SgForInitStatement::fixupCopy_scopes(SgNode *copy,
                                          SgCopyHelp &help) const {

  SgStatement::fixupCopy_scopes(copy, help);

  SgForInitStatement *forStatement_copy = isSgForInitStatement(copy);
  ROSE_ASSERT(forStatement_copy != NULL);

  const SgStatementPtrList &statementList_original = this->get_init_stmt();
  const SgStatementPtrList &statementList_copy =
      forStatement_copy->get_init_stmt();
  ROSE_ASSERT(statementList_original.size() == statementList_copy.size());

  SgStatementPtrList::const_iterator i_original =
      statementList_original.begin();
  SgStatementPtrList::const_iterator i_copy = statementList_copy.begin();

  // Iterate over both lists to match up the correct pairs of SgStatement
  // objects
  while (i_original != statementList_original.end()) {
    (*i_original)->fixupCopy_scopes(*i_copy, help);

    i_original++;
    i_copy++;
  }
}

void SgCatchStatementSeq::fixupCopy_scopes(SgNode *copy,
                                           SgCopyHelp &help) const {

  SgStatement::fixupCopy_scopes(copy, help);

  SgCatchStatementSeq *catchStatement_copy = isSgCatchStatementSeq(copy);
  if (catchStatement_copy == NULL ||
      requireExactMappedNode(this, help, "catch-sequence") !=
          catchStatement_copy) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[catch-sequence-copy]: original=%p copy=%p\n",
            static_cast<const void *>(this),
            static_cast<void *>(catchStatement_copy));
    ROSE_ABORT();
  }

  const SgStatementPtrList &originalCatches = get_catch_statement_seq();
  const SgStatementPtrList &copiedCatches =
      catchStatement_copy->get_catch_statement_seq();
  validateExactCopiedOwnedList(this, originalCatches, catchStatement_copy,
                               copiedCatches, help, "catch-handler");
  for (size_t index = 0; index < originalCatches.size(); ++index) {
    SgCatchOptionStmt *originalCatch =
        isSgCatchOptionStmt(originalCatches[index]);
    SgCatchOptionStmt *copiedCatch = isSgCatchOptionStmt(copiedCatches[index]);
    if (originalCatch == NULL || copiedCatch == NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[catch-handler-kind]: index=%zu "
              "original=%p/%s copy=%p/%s\n",
              index, static_cast<void *>(originalCatches[index]),
              originalCatches[index] != NULL
                  ? originalCatches[index]->class_name().c_str()
                  : "<null>",
              static_cast<void *>(copiedCatches[index]),
              copiedCatches[index] != NULL
                  ? copiedCatches[index]->class_name().c_str()
                  : "<null>");
      ROSE_ABORT();
    }
    originalCatch->fixupCopy_scopes(copiedCatch, help);
  }
}

void SgWhileStmt::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgScopeStatement::fixupCopy_scopes(copy, help);

  SgWhileStmt *whileStatement_copy = isSgWhileStmt(copy);
  ROSE_ASSERT(whileStatement_copy != NULL);

  ROSE_ASSERT(this->get_condition() != NULL);
  this->get_condition()->fixupCopy_scopes(whileStatement_copy->get_condition(),
                                          help);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_scopes(whileStatement_copy->get_body(), help);
}

void SgDoWhileStmt::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgScopeStatement::fixupCopy_scopes(copy, help);

  SgDoWhileStmt *doWhileStatement_copy = isSgDoWhileStmt(copy);
  ROSE_ASSERT(doWhileStatement_copy != NULL);

  ROSE_ASSERT(this->get_condition() != NULL);
  this->get_condition()->fixupCopy_scopes(
      doWhileStatement_copy->get_condition(), help);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_scopes(doWhileStatement_copy->get_body(), help);
}

void SgSwitchStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgScopeStatement::fixupCopy_scopes(copy, help);

  SgSwitchStatement *switchStatement_copy = isSgSwitchStatement(copy);
  ROSE_ASSERT(switchStatement_copy != NULL);

  ROSE_ASSERT(this->get_item_selector() != NULL);
  this->get_item_selector()->fixupCopy_scopes(
      switchStatement_copy->get_item_selector(), help);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_scopes(switchStatement_copy->get_body(), help);
}

void SgTryStmt::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgStatement::fixupCopy_scopes(copy, help);

  SgTryStmt *tryStatement_copy = isSgTryStmt(copy);
  if (tryStatement_copy == NULL ||
      requireExactMappedNode(this, help, "try-statement") !=
          tryStatement_copy) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[try-statement-copy]: original=%p copy=%p\n",
            static_cast<const void *>(this),
            static_cast<void *>(tryStatement_copy));
    ROSE_ABORT();
  }

  SgStatement *originalBody = get_body();
  SgStatement *copiedBody = tryStatement_copy->get_body();
  SgCatchStatementSeq *originalCatches = get_catch_statement_seq_root();
  SgCatchStatementSeq *copiedCatches =
      tryStatement_copy->get_catch_statement_seq_root();
  if (originalBody == NULL || copiedBody == NULL || originalCatches == NULL ||
      copiedCatches == NULL) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[try-statement-children]: original=%p body=%p "
            "catches=%p copy=%p body=%p catches=%p\n",
            static_cast<const void *>(this), static_cast<void *>(originalBody),
            static_cast<void *>(originalCatches),
            static_cast<void *>(tryStatement_copy),
            static_cast<void *>(copiedBody),
            static_cast<void *>(copiedCatches));
    ROSE_ABORT();
  }
  requireExactCopiedOwnedChild(this, originalBody, tryStatement_copy,
                               copiedBody, help, "try-body");
  requireExactCopiedOwnedChild(this, originalCatches, tryStatement_copy,
                               copiedCatches, help, "try-catches");
  originalBody->fixupCopy_scopes(copiedBody, help);
  originalCatches->fixupCopy_scopes(copiedCatches, help);
}

void SgCatchOptionStmt::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgScopeStatement::fixupCopy_scopes(copy, help);

  SgCatchOptionStmt *catchOptionStatement_copy = isSgCatchOptionStmt(copy);
  if (catchOptionStatement_copy == NULL ||
      requireExactMappedNode(this, help, "catch-handler") !=
          catchOptionStatement_copy) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[catch-handler-copy]: original=%p copy=%p\n",
            static_cast<const void *>(this),
            static_cast<void *>(catchOptionStatement_copy));
    ROSE_ABORT();
  }

  SgTryStmt *originalTry = get_trystmt();
  SgTryStmt *copiedTry = isSgTryStmt(remapExactCopiedOrExternalSemanticEdge(
      originalTry, catchOptionStatement_copy->get_trystmt(), help,
      "catch-handler-try"));
  if (originalTry == NULL || copiedTry == NULL) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[catch-handler-try]: original-handler=%p "
            "try=%p copy-handler=%p try=%p\n",
            static_cast<const void *>(this), static_cast<void *>(originalTry),
            static_cast<void *>(catchOptionStatement_copy),
            static_cast<void *>(copiedTry));
    ROSE_ABORT();
  }
  catchOptionStatement_copy->set_trystmt(copiedTry);
  requireExactCopiedOrExternalSemanticEdge(originalTry, copiedTry, help,
                                           "catch-handler-try");

  SgVariableDeclaration *originalCondition = get_condition();
  SgVariableDeclaration *copiedCondition =
      catchOptionStatement_copy->get_condition();
  SgStatement *originalBody = get_body();
  SgStatement *copiedBody = catchOptionStatement_copy->get_body();
  if ((originalCondition == NULL) != (copiedCondition == NULL) ||
      originalBody == NULL || copiedBody == NULL) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[catch-handler-children]: original=%p "
        "condition=%p body=%p copy=%p condition=%p body=%p\n",
        static_cast<const void *>(this), static_cast<void *>(originalCondition),
        static_cast<void *>(originalBody),
        static_cast<void *>(catchOptionStatement_copy),
        static_cast<void *>(copiedCondition), static_cast<void *>(copiedBody));
    ROSE_ABORT();
  }
  if (originalCondition != NULL) {
    requireExactCopiedOwnedChild(this, originalCondition,
                                 catchOptionStatement_copy, copiedCondition,
                                 help, "catch-handler-condition");
  }
  requireExactCopiedOwnedChild(this, originalBody, catchOptionStatement_copy,
                               copiedBody, help, "catch-handler-body");
  if (originalCondition != NULL) {
    originalCondition->fixupCopy_scopes(copiedCondition, help);
  }
  originalBody->fixupCopy_scopes(copiedBody, help);
}

void SgCaseOptionStmt::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {

  SgStatement::fixupCopy_scopes(copy, help);

  SgCaseOptionStmt *caseOptionStatement_copy = isSgCaseOptionStmt(copy);
  ROSE_ASSERT(caseOptionStatement_copy != NULL);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_scopes(caseOptionStatement_copy->get_body(),
                                     help);
}

void SgDefaultOptionStmt::fixupCopy_scopes(SgNode *copy,
                                           SgCopyHelp &help) const {

  SgStatement::fixupCopy_scopes(copy, help);

  SgDefaultOptionStmt *defaultOptionStatement_copy =
      isSgDefaultOptionStmt(copy);
  ROSE_ASSERT(defaultOptionStatement_copy != NULL);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_scopes(defaultOptionStatement_copy->get_body(),
                                     help);
}

void SgTemplateArgument::fixupCopy_scopes(SgNode *copy,
                                          SgCopyHelp &help) const {
  SgTemplateArgument *templateArgument_copy = isSgTemplateArgument(copy);
  if (templateArgument_copy == NULL || templateArgument_copy == this ||
      requireExactMappedNode(this, help, "template-argument-self") !=
          templateArgument_copy ||
      templateArgument_copy->get_argumentType() != get_argumentType()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-argument-copy]: original=%p kind=%d "
            "copy=%p kind=%d\n",
            static_cast<const void *>(this),
            static_cast<int>(get_argumentType()),
            static_cast<void *>(templateArgument_copy),
            templateArgument_copy != NULL
                ? static_cast<int>(templateArgument_copy->get_argumentType())
                : -1);
    ROSE_ABORT();
  }

  SgDeclarationStatement *originalTemplate = get_templateDeclaration();
  SgDeclarationStatement *copiedTemplate =
      templateArgument_copy->get_templateDeclaration();
  if ((get_argumentType() == SgTemplateArgument::template_template_argument) !=
          (originalTemplate != NULL) ||
      (originalTemplate == NULL) != (copiedTemplate == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-argument-declaration-kind]: "
            "original=%p kind=%d declaration=%p copy=%p declaration=%p\n",
            static_cast<const void *>(this),
            static_cast<int>(get_argumentType()),
            static_cast<void *>(originalTemplate),
            static_cast<void *>(templateArgument_copy),
            static_cast<void *>(copiedTemplate));
    ROSE_ABORT();
  }
  if (originalTemplate != NULL) {
    requireExactCopiedOrExternalSemanticEdge(originalTemplate, copiedTemplate,
                                             help,
                                             "template-argument-declaration");
  }

  SgType *originalType = get_type();
  SgType *copiedType = templateArgument_copy->get_type();
  if ((get_argumentType() == SgTemplateArgument::type_argument ||
       get_argumentType() == SgTemplateArgument::nontype_argument) !=
          (originalType != NULL) ||
      (originalType == NULL) != (copiedType == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-argument-type-kind]: original=%p "
            "kind=%d type=%p copy=%p type=%p\n",
            static_cast<const void *>(this),
            static_cast<int>(get_argumentType()),
            static_cast<void *>(originalType),
            static_cast<void *>(templateArgument_copy),
            static_cast<void *>(copiedType));
    ROSE_ABORT();
  }
  if (originalType != nullptr)
    requireExactCopiedOrExternalSemanticEdge(originalType, copiedType, help,
                                             "template-argument-type");
  SgType *originalWrittenType = get_sourceSpelledType();
  SgType *copiedWrittenType = templateArgument_copy->get_sourceSpelledType();
  ROSE_ASSERT((originalWrittenType == nullptr) ==
              (copiedWrittenType == nullptr));
  if (originalWrittenType != nullptr)
    requireExactCopiedOrExternalSemanticEdge(originalWrittenType,
                                             copiedWrittenType, help,
                                             "template-argument-written-type");

  fixupExactCopiedSupportScopes(this, templateArgument_copy, help);
}

void SgTemplateParameter::fixupCopy_scopes(SgNode *copy,
                                           SgCopyHelp &help) const {
  SgTemplateParameter *templateParameter_copy = isSgTemplateParameter(copy);
  if (templateParameter_copy == NULL || templateParameter_copy == this ||
      requireExactMappedNode(this, help, "template-parameter-self") !=
          templateParameter_copy ||
      templateParameter_copy->get_parameterType() != get_parameterType()) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-copy]: original=%p kind=%d "
            "copy=%p kind=%d\n",
            static_cast<const void *>(this),
            static_cast<int>(get_parameterType()),
            static_cast<void *>(templateParameter_copy),
            templateParameter_copy != NULL
                ? static_cast<int>(templateParameter_copy->get_parameterType())
                : -1);
    ROSE_ABORT();
  }

  if (SgTemplateParameter *canonical = get_canonicalNonTypeParameter()) {
    const auto mapped = help.get_copiedNodeMap().find(canonical);
    SgTemplateParameter *copiedCanonical =
        mapped == help.get_copiedNodeMap().end()
            ? canonical
            : isSgTemplateParameter(mapped->second);
    ASSERT_not_null(copiedCanonical);
    ROSE_ASSERT(templateParameter_copy->get_canonicalNonTypeParameter() ==
                copiedCanonical);
  }

  SgType *originalParameterType = get_type();
  SgType *copiedParameterType = templateParameter_copy->get_type();
  if (originalParameterType == NULL || copiedParameterType == NULL) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-type-fixup-null]: "
            "original=%p kind=%d type=%p copy=%p type=%p\n",
            static_cast<const void *>(this),
            static_cast<int>(get_parameterType()),
            static_cast<void *>(originalParameterType),
            static_cast<void *>(templateParameter_copy),
            static_cast<void *>(copiedParameterType));
    ROSE_ABORT();
  }
  SgNode *mappedParameterType = lookupCopiedNode(help, originalParameterType);
  if (get_parameterType() == SgTemplateParameter::type_parameter ||
      get_parameterType() == SgTemplateParameter::template_parameter) {
    SgTemplateType *originalTemplateType =
        isSgTemplateType(originalParameterType);
    SgTemplateType *copiedTemplateType = isSgTemplateType(mappedParameterType);
    if (originalTemplateType == NULL || copiedTemplateType == NULL ||
        copiedTemplateType == originalTemplateType ||
        copiedParameterType != copiedTemplateType ||
        originalTemplateType->get_template_parameter() != this ||
        copiedTemplateType->get_template_parameter() !=
            templateParameter_copy) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[template-parameter-type-fixup]: "
          "original=%p kind=%d type=%p owner=%p copy=%p type=%p "
          "owner=%p mapped=%p\n",
          static_cast<const void *>(this),
          static_cast<int>(get_parameterType()),
          static_cast<void *>(originalTemplateType),
          static_cast<void *>(
              originalTemplateType != NULL
                  ? originalTemplateType->get_template_parameter()
                  : NULL),
          static_cast<void *>(templateParameter_copy),
          static_cast<void *>(copiedTemplateType),
          static_cast<void *>(copiedTemplateType != NULL
                                  ? copiedTemplateType->get_template_parameter()
                                  : NULL),
          static_cast<void *>(mappedParameterType));
      ROSE_ABORT();
    }
  } else if (get_parameterType() == SgTemplateParameter::nontype_parameter) {
    SgType *expectedType = mappedParameterType != nullptr
                               ? isSgType(mappedParameterType)
                               : originalParameterType;
    ROSE_ASSERT(expectedType != nullptr &&
                expectedType->variantT() == originalParameterType->variantT() &&
                copiedParameterType == expectedType);
  } else {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-kind-fixup]: original=%p "
            "kind=%d copy=%p\n",
            static_cast<const void *>(this),
            static_cast<int>(get_parameterType()),
            static_cast<void *>(templateParameter_copy));
    ROSE_ABORT();
  }

  if (get_parameterType() == SgTemplateParameter::template_parameter) {
    requireExactCopiedOrExternalSemanticEdge(
        get_templateDeclaration(),
        templateParameter_copy->get_templateDeclaration(), help,
        "template-parameter-declaration");
    SgTemplateDeclaration *originalTemplate =
        isSgTemplateDeclaration(get_templateDeclaration());
    SgTemplateDeclaration *copiedTemplate = isSgTemplateDeclaration(
        templateParameter_copy->get_templateDeclaration());
    if (originalTemplate == NULL || copiedTemplate == NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[template-template-parameter-identity]: "
              "original-parameter=%p declaration=%p copy-parameter=%p "
              "declaration=%p must both be template declarations\n",
              static_cast<const void *>(this),
              static_cast<void *>(get_templateDeclaration()),
              static_cast<void *>(templateParameter_copy),
              static_cast<void *>(
                  templateParameter_copy->get_templateDeclaration()));
      ROSE_ABORT();
    }
    SgDeclarationScope *originalOwner =
        isSgDeclarationScope(originalTemplate->get_parent());
    if (originalOwner == NULL ||
        originalTemplate->get_scope() != originalOwner ||
        countDirectOwnerEdges(originalOwner, originalTemplate) != 1) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[template-parameter-declaration-owner]: "
              "parameter=%p declaration=%p parent=%p scope=%p edges=%zu\n",
              static_cast<const void *>(this),
              static_cast<void *>(originalTemplate),
              static_cast<void *>(originalTemplate->get_parent()),
              static_cast<void *>(originalTemplate->get_scope()),
              countDirectOwnerEdges(originalOwner, originalTemplate));
      ROSE_ABORT();
    }
    if (copiedTemplate != originalTemplate) {
      SgDeclarationScope *copiedOwner =
          isSgDeclarationScope(lookupCopiedNode(help, originalOwner));
      requireExactCopiedOwnedChild(originalOwner, originalTemplate, copiedOwner,
                                   copiedTemplate, help,
                                   "template-parameter-declaration-owner");
      if (copiedTemplate->get_scope() != copiedOwner) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[template-parameter-declaration-scope]: "
                "declaration=%p scope=%p expected=%p\n",
                static_cast<void *>(copiedTemplate),
                static_cast<void *>(copiedTemplate->get_scope()),
                static_cast<void *>(copiedOwner));
        ROSE_ABORT();
      }
    }

    SgTemplateDeclaration *originalSource =
        get_sourceSpelledTemplateDeclaration();
    SgTemplateDeclaration *copiedSource =
        templateParameter_copy->get_sourceSpelledTemplateDeclaration();
    if ((originalSource == NULL) != (copiedSource == NULL)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[source-spelled-template-parameter]: "
              "original=%p source=%p copy=%p source=%p\n",
              static_cast<const void *>(this),
              static_cast<void *>(originalSource),
              static_cast<void *>(templateParameter_copy),
              static_cast<void *>(copiedSource));
      ROSE_ABORT();
    }
    if (originalSource != NULL) {
      requireExactCopiedOwnedChild(
          this, originalSource, templateParameter_copy, copiedSource, help,
          "source-spelled-template-parameter-declaration");
      if (originalSource == originalTemplate ||
          copiedSource == copiedTemplate ||
          originalSource->get_scope() == NULL ||
          copiedSource->get_scope() == NULL) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[source-spelled-template-parameter]: "
                "original=%p semantic=%p source=%p scope=%p copy=%p "
                "semantic=%p source=%p scope=%p\n",
                static_cast<const void *>(this),
                static_cast<void *>(originalTemplate),
                static_cast<void *>(originalSource),
                static_cast<void *>(originalSource->get_scope()),
                static_cast<void *>(templateParameter_copy),
                static_cast<void *>(copiedTemplate),
                static_cast<void *>(copiedSource),
                static_cast<void *>(copiedSource->get_scope()));
        ROSE_ABORT();
      }
    }
  } else if (get_templateDeclaration() != NULL ||
             templateParameter_copy->get_templateDeclaration() != NULL ||
             get_sourceSpelledTemplateDeclaration() != NULL ||
             templateParameter_copy->get_sourceSpelledTemplateDeclaration() !=
                 NULL) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[non-template-parameter-declaration]: "
        "original=%p kind=%d declaration=%p copy=%p declaration=%p\n",
        static_cast<const void *>(this), static_cast<int>(get_parameterType()),
        static_cast<void *>(get_templateDeclaration()),
        static_cast<void *>(templateParameter_copy),
        static_cast<void *>(templateParameter_copy->get_templateDeclaration()));
    ROSE_ABORT();
  }

  SgTemplateArgument *originalDefault = get_defaultTemplateArgument();
  SgTemplateArgument *copiedDefault =
      templateParameter_copy->get_defaultTemplateArgument();
  if ((originalDefault == NULL) != (copiedDefault == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-default-argument]: "
            "original=%p default=%p copy=%p default=%p\n",
            static_cast<const void *>(this),
            static_cast<void *>(originalDefault),
            static_cast<void *>(templateParameter_copy),
            static_cast<void *>(copiedDefault));
    ROSE_ABORT();
  }
  if (originalDefault != NULL) {
    requireExactCopiedOwnedChild(this, originalDefault, templateParameter_copy,
                                 copiedDefault, help,
                                 "template-parameter-default-argument");
  }

  SgType *originalDefaultType = get_defaultTypeParameter();
  SgType *copiedDefaultType =
      templateParameter_copy->get_defaultTypeParameter();
  if (originalDefaultType != NULL) {
    SgNode *mappedType = lookupCopiedNode(help, originalDefaultType);
    SgType *expectedType =
        mappedType != nullptr ? isSgType(mappedType) : originalDefaultType;
    ROSE_ASSERT(expectedType != nullptr &&
                expectedType->variantT() == originalDefaultType->variantT() &&
                copiedDefaultType == expectedType);
  }
  if ((originalDefaultType == NULL) != (copiedDefaultType == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-default-type]: "
            "original=%p default=%p/%s copy=%p default=%p changed type kind\n",
            static_cast<const void *>(this),
            static_cast<void *>(originalDefaultType),
            originalDefaultType != NULL
                ? originalDefaultType->class_name().c_str()
                : "<null>",
            static_cast<void *>(templateParameter_copy),
            static_cast<void *>(copiedDefaultType));
    ROSE_ABORT();
  }

  fixupExactCopiedSupportScopes(this, templateParameter_copy, help);
}
