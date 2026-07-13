// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "astPostProcessing/astPostProcessing.h"
#include "fixupCopy.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

// This file implementes support for the AST copy fixup.  It is specific to:
// 1) Scope pointer fixup
// 2) Parent pointer fixup
// 3 defining and not defining declarations fixup

void outputMap(SgCopyHelp &help) {
  int counter = 0;
  SgCopyHelp::copiedNodeMapTypeIterator i = help.get_copiedNodeMap().begin();

  printf("COPY MAP: \n");
  while (i != help.get_copiedNodeMap().end()) {
    if ((i->first->get_startOfConstruct() != NULL) &&
        (i->first->get_startOfConstruct()->isFrontendSpecific() == false)) {
      printf("entry %4d: i->first = %p  i->second = %p = %s\n", counter,
             i->first, i->second, i->first->class_name().c_str());
    }

    // printf ("entry %4d: i->first = %p  i->second = %p =
    // %s\n",counter,i->first,i->second,i->first->class_name().c_str());
    ROSE_ASSERT(i->first->variantT() == i->second->variantT());

    counter++;
    i++;
  }
}

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

  size_t sourceCount = 0;
  const SgNode *otherSource = NULL;
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (it->second == copy) {
      ++sourceCount;
      if (it->first != original) {
        otherSource = it->first;
      }
    }
  }
  if (sourceCount != 1) {
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
  return node == NULL || node->class_name() == "SgNode";
}

void validateExactCopiedNodeMap(SgCopyHelp &help) {
  auto mappedType = [&help](SgType *source) -> SgType * {
    if (source == NULL) {
      return NULL;
    }
    const SgCopyHelp::copiedNodeMapTypeIterator mapped =
        help.get_copiedNodeMap().find(source);
    return mapped != help.get_copiedNodeMap().end() ? isSgType(mapped->second)
                                                    : source;
  };
  auto isExactCanonicalTypeConvergence = [&mappedType](
                                             const SgNode *firstOriginal,
                                             const SgNode *secondOriginal,
                                             SgNode *copy) {
    SgType *first = isSgType(const_cast<SgNode *>(firstOriginal));
    SgType *second = isSgType(const_cast<SgNode *>(secondOriginal));
    SgType *target = isSgType(copy);
    if (first == NULL || second == NULL || target == NULL ||
        first->variantT() != second->variantT() ||
        first->variantT() != target->variantT()) {
      return false;
    }

    if (SgPointerMemberType *targetMember = isSgPointerMemberType(target)) {
      SgPointerMemberType *firstMember = isSgPointerMemberType(first);
      SgPointerMemberType *secondMember = isSgPointerMemberType(second);
      return firstMember != NULL && secondMember != NULL &&
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
  };

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
                                           copy)) {
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

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (it->second == candidate) {
      return true;
    }
  }

  return false;
}

SgDeclarationStatement *
originalDeclarationForCopiedValue(SgCopyHelp &help,
                                  SgDeclarationStatement *candidate) {
  if (candidate == NULL) {
    return NULL;
  }

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (it->second == candidate) {
      if (SgDeclarationStatement *originalDecl =
              isSgDeclarationStatement(const_cast<SgNode *>(it->first))) {
        return originalDecl;
      }
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
  case V_SgTemplateFunctionDefinition:
  case V_SgCAFWithTeamStatement: {
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

bool referencedDeclarationMayRemainShared(
    const SgDeclarationStatement *originalDecl) {
  if (originalDecl == NULL) {
    return false;
  }

  return isSgNonrealDecl(originalDecl) != NULL ||
         originalDecl->get_parent() == NULL;
}

SgNode *copyNodeIntoCurrentRoot(SgCopyHelp &help, const SgNode *originalNode) {
  if (originalNode == NULL) {
    return NULL;
  }

  if (SgNode *existingCopy = lookupCopiedNode(help, originalNode)) {
    return existingCopy;
  }

  // Extend the active root-copy map without recursively finalizing each
  // newly synthesized child as its own standalone copy.
  help.incrementDepth();
  SgNode *copiedNode = help.copyOrLookupAst(originalNode);
  help.decrementDepth();
  return copiedNode;
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
    canonicalDefinition->set_declaration(canonicalDefiningDecl);

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
          canonicalFirstNondefiningDecl->get_scope() !=
              canonicalDefiningDecl->get_scope() ||
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
    if (copyTypeDecl == NULL) {
      ROSE_ABORT();
    }
    copyType->set_declaration(copyTypeDecl);
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
         name == "rvalue_ref_to" || name == "decltype_ref_to" ||
         name == "typeof_ref_to";
}

bool isTypeReverseOwnershipField(const SgName &field) {
  return field.getString() == "parent";
}

class ExactCopiedTypeGraphMaterializer {
  SgCopyHelp &help_;
  std::unordered_map<const SgNode *, SgNode *> &replacements_;
  std::unordered_map<const SgType *, SgType *> provisionalCopySources_;
  std::set<const SgType *> active_;

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
      SgCopyHelp::copiedNodeMapTypeIterator mapped =
          materializer_.help_.get_copiedNodeMap().find(edge);
      if (mapped != materializer_.help_.get_copiedNodeMap().end() &&
          mapped->second != edge) {
        needsCopy = true;
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
        // These are lazily populated reverse caches, not semantic constituents
        // of the type.  A copy-local type must rebuild them on demand instead
        // of retaining links into the source type graph.
        edge = NULL;
        return;
      }
      if (SgType *edgeType = isSgType(edge)) {
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

public:
  ExactCopiedTypeGraphMaterializer(
      SgCopyHelp &help,
      std::unordered_map<const SgNode *, SgNode *> &replacements)
      : help_(help), replacements_(replacements) {}

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
    bool ambiguousMappedSource = false;
    for (const auto &mapping : help_.get_copiedNodeMap()) {
      if (mapping.first == mapping.second || mapping.second != source ||
          isSgType(mapping.first) == NULL) {
        continue;
      }
      if (mappedSource != NULL && mappedSource != mapping.first) {
        ambiguousMappedSource = true;
        continue;
      }
      mappedSource = isSgType(mapping.first);
    }
    if (ambiguousMappedSource) {
      // Multiple source-owned syntax wrappers may legitimately canonicalize
      // to one copy-local semantic wrapper.  Accept that convergence only
      // when the target is provably the canonical cache identity for its exact
      // copied constituent(s); an incidental copied shell remains ambiguous.
      bool canonicalMappedTarget = false;
      if (SgPointerMemberType *member = isSgPointerMemberType(source)) {
        canonicalMappedTarget =
            SgPointerMemberType::isCanonicalSemanticType(member);
      } else if (source->variantT() == V_SgPointerType) {
        SgPointerType *pointer = isSgPointerType(source);
        SgType *base = pointer != NULL ? pointer->get_base_type() : NULL;
        SgPointerType *cached = base != NULL ? base->get_ptr_to() : NULL;
        canonicalMappedTarget =
            base != NULL && (cached == NULL || cached == pointer);
        if (canonicalMappedTarget && cached == NULL)
          base->set_ptr_to(pointer);
      } else if (SgReferenceType *reference = isSgReferenceType(source)) {
        SgType *base = reference->get_base_type();
        SgReferenceType *cached = base != NULL ? base->get_ref_to() : NULL;
        canonicalMappedTarget =
            base != NULL && (cached == NULL || cached == reference);
        if (canonicalMappedTarget && cached == NULL)
          base->set_ref_to(reference);
      } else if (SgRvalueReferenceType *reference =
                     isSgRvalueReferenceType(source)) {
        SgType *base = reference->get_base_type();
        SgRvalueReferenceType *cached =
            base != NULL ? base->get_rvalue_ref_to() : NULL;
        canonicalMappedTarget =
            base != NULL && (cached == NULL || cached == reference);
        if (canonicalMappedTarget && cached == NULL)
          base->set_rvalue_ref_to(reference);
      }
      if (!canonicalMappedTarget) {
        SgPointerType *pointer = isSgPointerType(source);
        fprintf(stderr,
                "REX_COPY_INVARIANT[copied-type-reverse-map]: copy=%p/%s "
                "base=%p base-cache=%p has multiple source types but is not "
                "one canonical copy-local wrapper\n",
                static_cast<void *>(source), source->class_name().c_str(),
                static_cast<void *>(pointer != NULL ? pointer->get_base_type()
                                                    : NULL),
                static_cast<void *>(pointer != NULL &&
                                            pointer->get_base_type() != NULL
                                        ? pointer->get_base_type()->get_ptr_to()
                                        : NULL));
        ROSE_ABORT();
      }
      return source;
    }
    if (mappedSource != NULL) {
      return materialize(const_cast<SgType *>(mappedSource));
    }

    // Class and enum types are owned by their complete declaration families.
    // The family pass above publishes their replacements only after the
    // canonical and defining declarations participate in the same copy
    // transaction.  Recursively cloning one merely because a copied wrapper
    // type points at it would manufacture a target-file type for a partial
    // forward surface and split the declaration family.
    if (isSgClassType(source) != NULL || isSgEnumType(source) != NULL) {
      const auto mappedNamedType = help_.get_copiedNodeMap().find(source);
      if (mappedNamedType != help_.get_copiedNodeMap().end() &&
          mappedNamedType->second != source) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[named-type-before-family-publication]: "
                "source=%p/%s mapped=%p/%s\n",
                static_cast<void *>(source), source->class_name().c_str(),
                static_cast<void *>(mappedNamedType->second),
                mappedNamedType->second != NULL
                    ? mappedNamedType->second->class_name().c_str()
                    : "<null>");
        ROSE_ABORT();
      }
      return source;
    }

    if (!active_.insert(source).second) {
      // Forward semantic type cycles are resolved by the caller that first
      // entered the cycle. Reverse caches and ownership links never reach
      // this path.
      return source;
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

    DiscoverDependencies discover(*this, source);
    source->processDataMemberReferenceToPointers(&discover);
    if (!discover.needsCopy) {
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
        existing->second = source;
      }
      active_.erase(source);
      return source;
    }

    SgType *sourceWrapperBase = NULL;
    SgType *sourceMemberClass = NULL;
    if (SgPointerMemberType *pointer = isSgPointerMemberType(source)) {
      sourceWrapperBase = pointer->get_base_type();
      sourceMemberClass = pointer->get_class_type();
    } else if (SgPointerType *pointer = isSgPointerType(source)) {
      sourceWrapperBase = pointer->get_base_type();
    } else if (SgReferenceType *reference = isSgReferenceType(source)) {
      sourceWrapperBase = reference->get_base_type();
    } else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(source)) {
      sourceWrapperBase = reference->get_base_type();
    }
    if (sourceWrapperBase != NULL) {
      SgType *copiedWrapperBase = materialize(sourceWrapperBase);
      SgType *copiedMemberClass =
          sourceMemberClass != NULL ? materialize(sourceMemberClass) : NULL;

      // A target-file typedef can construct the canonical wrapper around a
      // copied named type before the accumulated type-graph pass reaches that
      // wrapper.  Canonicalizing the named type clears its reverse caches so
      // they cannot retain source-translation-unit identities.  If the
      // already mapped wrapper has the exact copied constituents, restore
      // that target identity to the cleared cache before consulting the
      // canonical factory.  Creating a second wrapper here would split the
      // target type graph.
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

    if (existingCopy != NULL) {
      provisionalCopySources_.erase(existingCopy);
      if (!replacements_.emplace(source, existingCopy).second) {
        ROSE_ABORT();
      }
      CanonicalizeCopiedType canonicalize(*this);
      existingCopy->processDataMemberReferenceToPointers(&canonicalize);
      active_.erase(source);
      return existingCopy;
    }

    if (existing != help_.get_copiedNodeMap().end()) {
      help_.get_copiedNodeMap().erase(existing);
    }

    std::set<const SgNode *> mappingsBeforeCopy;
    for (const auto &mapping : help_.get_copiedNodeMap()) {
      mappingsBeforeCopy.insert(mapping.first);
    }
    const Rose_STL_Container<SgNode *> sourceChildren =
        source->get_traversalSuccessorContainer();

    help_.incrementDepth();
    SgType *copy = isSgType(source->copy(help_));
    help_.decrementDepth();
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
      sourceChild->fixupCopy_scopes(copiedChild, help_);
      sourceChild->fixupCopy_symbols(copiedChild, help_);
      sourceChild->fixupCopy_references(copiedChild, help_);
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
    const SgClassDeclaration *source, const SgClassDeclaration *destination) {
  return source != NULL && destination != NULL && source != destination &&
         isSgGlobal(source->get_scope()) != NULL &&
         isSgGlobal(destination->get_scope()) != NULL &&
         !source->get_isUnNamed() && !destination->get_isUnNamed() &&
         source->get_class_type() == destination->get_class_type() &&
         source->get_name() == destination->get_name() &&
         !source->get_mangled_name().is_null() &&
         source->get_mangled_name() == destination->get_mangled_name() &&
         source->get_qualified_name() == destination->get_qualified_name() &&
         hasExactTranslationUnitStructuralAncestry(source) &&
         hasExactTranslationUnitStructuralAncestry(destination);
}

bool copiedEnumDeclarationsHaveExactSharedProjectIdentity(
    const SgEnumDeclaration *source, const SgEnumDeclaration *destination) {
  return source != NULL && destination != NULL && source != destination &&
         isSgGlobal(source->get_scope()) != NULL &&
         isSgGlobal(destination->get_scope()) != NULL &&
         !source->get_isUnNamed() && !destination->get_isUnNamed() &&
         source->get_name() == destination->get_name() &&
         !source->get_mangled_name().is_null() &&
         source->get_mangled_name() == destination->get_mangled_name() &&
         source->get_qualified_name() == destination->get_qualified_name() &&
         hasExactTranslationUnitStructuralAncestry(source) &&
         hasExactTranslationUnitStructuralAncestry(destination);
}

void materializeExactCopiedNamedTypes(SgCopyHelp &help) {
  std::vector<std::pair<const SgNonrealDecl *, SgNonrealDecl *>>
      copiedNonrealDeclarations;
  copiedNonrealDeclarations.reserve(help.get_copiedNodeMap().size());
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgNonrealDecl *original = isSgNonrealDecl(it->first);
    SgNonrealDecl *copy = isSgNonrealDecl(it->second);
    if (original == NULL || copy == NULL || original == copy) {
      continue;
    }
    copiedNonrealDeclarations.push_back(std::make_pair(original, copy));
  }

  std::unordered_map<const SgNode *, SgNode *> replacements;
  replacements.reserve(copiedNonrealDeclarations.size());
  for (const auto &declaration : copiedNonrealDeclarations) {
    const SgNonrealDecl *originalDeclaration = declaration.first;
    SgNonrealDecl *copiedDeclaration = declaration.second;
    SgNonrealType *originalType =
        isSgNonrealType(originalDeclaration->get_type());
    const auto existingTypeMapping =
        originalType != NULL ? help.get_copiedNodeMap().find(originalType)
                             : help.get_copiedNodeMap().end();
    SgNonrealType *copiedType =
        existingTypeMapping != help.get_copiedNodeMap().end()
            ? isSgNonrealType(existingTypeMapping->second)
            : NULL;
    if (originalType == NULL ||
        originalType->get_declaration() != originalDeclaration ||
        originalType->get_parent() != originalDeclaration ||
        (copiedDeclaration->get_type() != originalType &&
         copiedDeclaration->get_type() != copiedType) ||
        (existingTypeMapping != help.get_copiedNodeMap().end() &&
         (copiedType == NULL || copiedType == originalType)) ||
        (copiedType != NULL &&
         (copiedType->get_declaration() != copiedDeclaration ||
          copiedType->get_parent() != copiedDeclaration))) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[copied-nonreal-type-input]: "
          "source-declaration=%p source-type=%p owner=%p parent=%p "
          "copy-declaration=%p provisional-type=%p mapped-type=%p "
          "mapped-owner=%p mapped-parent=%p\n",
          static_cast<const void *>(originalDeclaration),
          static_cast<void *>(originalType),
          static_cast<void *>(
              originalType != NULL ? originalType->get_declaration() : NULL),
          static_cast<void *>(originalType != NULL ? originalType->get_parent()
                                                   : NULL),
          static_cast<void *>(copiedDeclaration),
          static_cast<void *>(copiedDeclaration->get_type()),
          static_cast<void *>(copiedType),
          static_cast<void *>(copiedType != NULL ? copiedType->get_declaration()
                                                 : NULL),
          static_cast<void *>(copiedType != NULL ? copiedType->get_parent()
                                                 : NULL));
      ROSE_ABORT();
    }

    if (copiedType == NULL) {
      copiedType = new SgNonrealType(*originalType);
      copiedType->set_declaration(copiedDeclaration);
      copiedType->set_parent(copiedDeclaration);
      help.insertCopiedNodePair(originalType, copiedType);
    }
    copiedDeclaration->set_type(copiedType);
    if (lookupCopiedNode(help, originalType) != copiedType ||
        copiedType == originalType ||
        copiedType->get_declaration() != copiedDeclaration ||
        copiedType->get_parent() != copiedDeclaration ||
        copiedDeclaration->get_type() != copiedType ||
        !replacements.emplace(originalType, copiedType).second) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[copied-nonreal-type-publication]: "
              "source-declaration=%p source-type=%p copy-declaration=%p "
              "copy-type=%p owner=%p parent=%p mapped=%p\n",
              static_cast<const void *>(originalDeclaration),
              static_cast<void *>(originalType),
              static_cast<void *>(copiedDeclaration),
              static_cast<void *>(copiedType),
              static_cast<void *>(copiedType->get_declaration()),
              static_cast<void *>(copiedType->get_parent()),
              static_cast<void *>(lookupCopiedNode(help, originalType)));
      ROSE_ABORT();
    }
  }

  std::vector<std::pair<const SgClassDeclaration *, SgClassDeclaration *>>
      copiedCanonicalDeclarations;
  copiedCanonicalDeclarations.reserve(help.get_copiedNodeMap().size());
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgClassDeclaration *original = isSgClassDeclaration(it->first);
    SgClassDeclaration *copy = isSgClassDeclaration(it->second);
    if (original == NULL || copy == NULL || original == copy ||
        original->get_firstNondefiningDeclaration() != original) {
      continue;
    }
    copiedCanonicalDeclarations.push_back(std::make_pair(original, copy));
  }

  replacements.reserve(replacements.size() +
                       copiedCanonicalDeclarations.size());
  for (const auto &family : copiedCanonicalDeclarations) {
    const SgClassDeclaration *originalFirst = family.first;
    SgClassDeclaration *copyFirst = family.second;
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
        fprintf(
            stderr,
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
      continue;
    }
    SgClassDeclaration *copyDefining =
        originalDefining != NULL ? mappedDefining : NULL;
    SgClassType *originalType = isSgClassType(originalFirst->get_type());
    auto existingTypeMapping = originalType != NULL
                                   ? help.get_copiedNodeMap().find(originalType)
                                   : help.get_copiedNodeMap().end();
    const bool exactSharedProjectIdentity =
        copiedClassDeclarationsHaveExactSharedProjectIdentity(originalFirst,
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
         (originalDefining->get_firstNondefiningDeclaration() !=
              originalFirst ||
          originalDefining->get_definingDeclaration() != originalDefining ||
          originalDefining->get_type() != originalType ||
          copyDefining == NULL ||
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
              static_cast<void *>(originalType != NULL
                                      ? originalType->get_declaration()
                                      : NULL),
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
          (copyMember->get_type() != originalType &&
           (!alreadyMaterialized ||
            copyMember->get_type() != existingCopyType))) {
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
      help.insertCopiedNodePair(originalType, copyType);
    }
    for (SgClassDeclaration *member : copiedFamilyMembers) {
      member->set_type(copyType);
    }
    if (copyType == originalType) {
      if (!copiedClassDeclarationsHaveExactSharedProjectIdentity(originalFirst,
                                                                 copyFirst) ||
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
      continue;
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
              static_cast<void *>(
                  copyDefining != NULL ? copyDefining->get_type() : NULL));
      ROSE_ABORT();
    }

    if (lookupCopiedNode(help, originalType) != copyType ||
        !replacements.emplace(originalType, copyType).second) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[copied-class-type-publication]: "
              "source-type=%p copy-type=%p mapped=%p\n",
              static_cast<void *>(originalType), static_cast<void *>(copyType),
              static_cast<void *>(lookupCopiedNode(help, originalType)));
      ROSE_ABORT();
    }
  }

  std::vector<std::pair<const SgEnumDeclaration *, SgEnumDeclaration *>>
      copiedCanonicalEnums;
  copiedCanonicalEnums.reserve(help.get_copiedNodeMap().size());
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgEnumDeclaration *original = isSgEnumDeclaration(it->first);
    SgEnumDeclaration *copy = isSgEnumDeclaration(it->second);
    if (original == NULL || copy == NULL || original == copy ||
        original->get_firstNondefiningDeclaration() != original) {
      continue;
    }
    copiedCanonicalEnums.push_back(std::make_pair(original, copy));
  }

  replacements.reserve(replacements.size() + copiedCanonicalEnums.size());
  for (const auto &family : copiedCanonicalEnums) {
    const SgEnumDeclaration *originalFirst = family.first;
    SgEnumDeclaration *copyFirst = family.second;
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
              static_cast<void *>(originalType != NULL
                                      ? originalType->get_declaration()
                                      : NULL),
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
          (copyMember->get_type() != originalType &&
           (!alreadyMaterialized ||
            copyMember->get_type() != existingCopyType))) {
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
      help.insertCopiedNodePair(originalType, copyType);
    }
    for (SgEnumDeclaration *member : copiedFamilyMembers) {
      member->set_type(copyType);
    }
    if (copyType == originalType) {
      if (!copiedEnumDeclarationsHaveExactSharedProjectIdentity(originalFirst,
                                                                copyFirst) ||
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
      continue;
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

    if (lookupCopiedNode(help, originalType) != copyType ||
        !replacements.emplace(originalType, copyType).second) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[copied-enum-type-publication]: "
              "source-type=%p copy-type=%p mapped=%p\n",
              static_cast<void *>(originalType), static_cast<void *>(copyType),
              static_cast<void *>(lookupCopiedNode(help, originalType)));
      ROSE_ABORT();
    }
  }

  // A type or template-template parameter owns one declaration-specific
  // SgTemplateType identity.  The type/default-type fields on
  // SgTemplateParameter are intentionally non-traversed semantic edges, so
  // they cannot enter the closed dependent-type transaction through the
  // generic pointer rewriter.  Seed the transaction from every exact
  // parameter-owned type that the pre-fixup publication phase created.
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

  if (replacements.empty()) {
    return;
  }

  ExactCopiedTypeGraphMaterializer typeMaterializer(help, replacements);
  ExactCopiedTypeEdgeRewriter rewriter(typeMaterializer);

  // Close the two non-traversed SgTemplateParameter type edges explicitly.
  // Every changed dependency is materialized by the same graph transaction as
  // ordinary copied-node type edges, so wrappers and distinct source-spelled
  // template-parameter uses retain their own target identities while binding
  // to the copied semantic parameter.
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
    if (provisionalParameterType != originalParameterType &&
        provisionalParameterType != finalParameterType) {
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
    copiedParameter->set_type(finalParameterType);

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
      if (provisionalDefaultType != originalDefaultType &&
          provisionalDefaultType != finalDefaultType) {
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
      copiedParameter->set_defaultTypeParameter(finalDefaultType);
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
    if (copiedArgumentType == NULL || copiedPayloadType == NULL ||
        !SageInterface::isEquivalentType(copiedArgumentType,
                                         copiedPayloadType)) {
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
  if (copiedFirstNondef == NULL || copiedDefining == NULL) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[enum-chain-output]: declaration=%p copy=%p "
            "first=%p defining=%p\n",
            static_cast<const void *>(originalEnum),
            static_cast<void *>(copyEnum),
            static_cast<void *>(copiedFirstNondef),
            static_cast<void *>(copiedDefining));
    ROSE_ABORT();
  }

  copiedFirstNondef->set_firstNondefiningDeclaration(copiedFirstNondef);
  copiedFirstNondef->set_definingDeclaration(copiedDefining);
  copiedDefining->set_firstNondefiningDeclaration(copiedFirstNondef);
  copiedDefining->set_definingDeclaration(copiedDefining);
  copyEnum->set_firstNondefiningDeclaration(copiedFirstNondef);
  copyEnum->set_definingDeclaration(copiedDefining);

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

void finalizeExactCopiedFunctionDefinitions(SgCopyHelp &help) {
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

    copyFunctionDef->set_declaration(canonicalDecl);
    canonicalDecl->set_definition(copyFunctionDef);
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
      existing->second = copy;
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
          !originalTemplateType->get_tpl_args().empty() ||
          !originalTemplateType->get_part_spec_tpl_args().empty()) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[template-parameter-owned-type-input]: "
                "source=%p kind=%d type=%p/%s semantic-parameter=%p "
                "copy=%p semantic-parameter=%p args=%zu partial-args=%zu\n",
                static_cast<const void *>(originalParameter),
                static_cast<int>(parameterKind),
                static_cast<void *>(originalType),
                originalType->class_name().c_str(),
                static_cast<void *>(originalSemanticParameter),
                static_cast<void *>(copiedParameter),
                static_cast<void *>(copiedSemanticParameter),
                originalTemplateType != NULL
                    ? originalTemplateType->get_tpl_args().size()
                    : 0,
                originalTemplateType != NULL
                    ? originalTemplateType->get_part_spec_tpl_args().size()
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

      if (copiedType == originalType) {
        copiedTemplateType =
            new SgTemplateType(originalTemplateType->get_name());
        copiedTemplateType->set_template_parameter_depth(sourceDepth);
        copiedTemplateType->set_template_parameter_position(sourcePosition);
        if (originalTemplateType->get_canonical_source_identity().has_value()) {
          copiedTemplateType->initialize_canonical_source_identity(
              *originalTemplateType->get_canonical_source_identity());
        }
        copiedTemplateType->set_class_type(
            originalTemplateType->get_class_type());
        copiedTemplateType->set_parent_class_type(
            originalTemplateType->get_parent_class_type());
        copiedTemplateType->set_packed(originalTemplateType->get_packed());
        copiedTemplateType->set_template_parameter(copiedParameter);
        copiedParameter->set_type(copiedTemplateType);
        copiedType = copiedTemplateType;
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
          copiedTemplateType->get_class_type() !=
              originalTemplateType->get_class_type() ||
          copiedTemplateType->get_parent_class_type() !=
              originalTemplateType->get_parent_class_type() ||
          copiedTemplateType->get_packed() !=
              originalTemplateType->get_packed() ||
          !copiedTemplateType->get_tpl_args().empty() ||
          !copiedTemplateType->get_part_spec_tpl_args().empty()) {
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
      publishOwnedParameterType(originalParameter, copiedParameter);

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
        const auto templateMapping =
            help.get_copiedNodeMap().find(originalTemplate);
        if (copiedTemplate == originalTemplate &&
            templateMapping != help.get_copiedNodeMap().end() &&
            templateMapping->second != originalTemplate) {
          copiedTemplate = isSgDeclarationStatement(templateMapping->second);
          if (copiedTemplate == NULL) {
            fprintf(stderr,
                    "REX_COPY_INVARIANT[template-semantic-parameter-"
                    "declaration-map]: source=%p declaration=%p mapped=%p/%s "
                    "changed declaration kind\n",
                    static_cast<const void *>(originalParameter),
                    static_cast<void *>(originalTemplate),
                    static_cast<void *>(templateMapping->second),
                    templateMapping->second != NULL
                        ? templateMapping->second->class_name().c_str()
                        : "<null>");
            ROSE_ABORT();
          }
          copiedParameter->set_templateDeclaration(copiedTemplate);
        }
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

  // A template parameter can also be the structural root of a copy
  // transaction.  Such a copy has no enclosing declaration worklist entry,
  // but it still owns an independent template-type identity.  Process every
  // published parameter pair once more so the standalone and nested cases
  // obey the same ownership contract.
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
    if (plannedExternalEdge->variantT() != originalEdge->variantT() ||
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
    if (plannedExternalEdge->variantT() != originalEdge->variantT() ||
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

void fixupExactCopiedInitializedNameSemanticEdges(
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
    if (resolvedScope == NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[initialized-name-scope-kind]: source=%p/%s "
              "scope=%p copy=%p scope=%p resolved=%p\n",
              static_cast<const void *>(originalName),
              originalName->get_name().str(),
              static_cast<void *>(originalScope),
              static_cast<void *>(copiedName), static_cast<void *>(copiedScope),
              static_cast<void *>(resolvedScope));
      ROSE_ABORT();
    }
    copiedName->set_scope(resolvedScope);
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
    copiedName->set_declptr(resolvedDeclaration);
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

void finalizeCanonicalCopyLinks(SgCopyHelp &help) {
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
      if (SgRequiresExpr *requiresExpression = isSgRequiresExpr(parent)) {
        SgFunctionParameterList *parameterList =
            isSgFunctionParameterList(statement);
        if (parameterList == NULL ||
            requiresExpression->get_local_parameter_list() != parameterList ||
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
        copiedScope->variantT() != originalScope->variantT()) {
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
      fixupExactCopiedInitializedNameSemanticEdges(originalName, copiedName,
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
      if (originalTemplate != NULL) {
        copiedTemplate =
            isSgDeclarationStatement(remapExactCopiedOrExternalSemanticEdge(
                originalTemplate, copiedTemplate, help,
                "template-argument-declaration"));
        if (copiedTemplate == NULL) {
          fprintf(stderr,
                  "REX_COPY_INVARIANT[template-argument-declaration]: "
                  "source=%p kind=%d declaration=%p copy=%p remapped to a "
                  "non-declaration semantic edge\n",
                  static_cast<const void *>(originalArgument),
                  static_cast<int>(originalArgument->get_argumentType()),
                  static_cast<void *>(originalTemplate),
                  static_cast<void *>(copiedArgument));
          ROSE_ABORT();
        }
        copiedArgument->set_templateDeclaration(copiedTemplate);
      }
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
    SgNode *copiedOwner = plannedOwner != NULL
                              ? plannedOwner
                              : lookupCopiedNode(help, originalOwner);
    const size_t originalEdges =
        countDirectOwnerEdges(originalOwner, originalNode);
    if (copiedOwner != NULL && originalEdges != 0 &&
        (copyNode->get_parent() != copiedOwner ||
         countDirectOwnerEdges(copiedOwner, copyNode) != originalEdges)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[structural-owner-map]: original=%p/%s "
              "owner=%p/%s edges=%zu copy=%p/%s owner=%p/%s expected=%p/%s "
              "edges=%zu expected-typedef-seq=%p\n",
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
              countDirectOwnerEdges(copiedOwner, copyNode),
              static_cast<void *>(isSgType(copiedOwner) != nullptr
                                      ? isSgType(copiedOwner)->get_typedefs()
                                      : nullptr));
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
  finalizeExactCopiedFunctionDefinitions(help);
  materializeExactCopiedNamedTypes(help);
  finalizeExactCopiedClassTypes(help);
  finalizeExactCopiedSymbols(help);
  validateExactCopiedNodeMap(help);

  // A nonreal reference and its synthetic spelling declaration are separate
  // structural subgraphs.  Their owned template arguments and the declaration
  // families referenced by those arguments must all be fixed before comparing
  // the two semantic views.  Validate that cross-graph contract only at the
  // completed copy-transaction boundary.
  for (const auto &mapping : help.get_copiedNodeMap()) {
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

void SgCopyHelp::prepareRootCopyForFixup() {
  if (!hasNontrivialCopiedNodes(*this)) {
    return;
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
        (copyInitializedName->get_declptr() != originalDeclaration &&
         copyInitializedName->get_declptr() != targetDeclaration)) {
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
      copyInitializedName->set_declptr(targetDeclaration);
      break;
    }
    if (variableDefinition_original == NULL ||
        originalInitializedName->get_variable_definition() !=
            variableDefinition_original ||
        variableDefinition_original->get_vardefn() != originalInitializedName ||
        variableDefinition_original->get_parent() != originalInitializedName) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[variable-definition-input]: name=%p/%s "
          "definition=%p owned-definition=%p vardefn=%p parent=%p\n",
          static_cast<const void *>(originalInitializedName),
          originalInitializedName->get_name().str(),
          static_cast<void *>(variableDefinition_original),
          static_cast<void *>(
              originalInitializedName->get_variable_definition()),
          static_cast<void *>(variableDefinition_original != NULL
                                  ? variableDefinition_original->get_vardefn()
                                  : NULL),
          static_cast<void *>(variableDefinition_original != NULL
                                  ? variableDefinition_original->get_parent()
                                  : NULL));
      ROSE_ABORT();
    }

    SgVariableDefinition *variableDefinition_copy = isSgVariableDefinition(
        requireExactMappedNode(variableDefinition_original, help,
                               "initialized-name-owned-variable-definition"));

    if (variableDefinition_copy == NULL ||
        variableDefinition_copy == variableDefinition_original ||
        lookupCopiedNode(help, variableDefinition_original) !=
            variableDefinition_copy ||
        provisionalVariableDefinition != variableDefinition_copy ||
        variableDefinition_copy->get_vardefn() != copyInitializedName) {
      fprintf(
          stderr,
          "REX_COPY_INVARIANT[variable-definition-map]: source=%p "
          "name=%p copy=%p owned-definition=%p vardefn=%p "
          "expected-name=%p mapped=%p\n",
          static_cast<void *>(variableDefinition_original),
          static_cast<const void *>(originalInitializedName),
          static_cast<void *>(variableDefinition_copy),
          static_cast<void *>(copyInitializedName->get_variable_definition()),
          static_cast<void *>(variableDefinition_copy != NULL
                                  ? variableDefinition_copy->get_vardefn()
                                  : NULL),
          static_cast<void *>(copyInitializedName),
          static_cast<void *>(
              lookupCopiedNode(help, variableDefinition_original)));
      ROSE_ABORT();
    }

    if (variableDefinition_copy->get_parent() != copyInitializedName) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[variable-definition-owner]: copy=%p "
              "definition=%p parent=%p\n",
              static_cast<void *>(copyInitializedName),
              static_cast<void *>(variableDefinition_copy),
              static_cast<void *>(variableDefinition_copy->get_parent()));
      ROSE_ABORT();
    }

    SgExpression *originalBitfield =
        variableDefinition_original->get_bitfield();
    SgExpression *copiedBitfield = variableDefinition_copy->get_bitfield();
    if ((originalBitfield == NULL) != (copiedBitfield == NULL) ||
        (originalBitfield != NULL &&
         (lookupCopiedNode(help, originalBitfield) != copiedBitfield ||
          copiedBitfield == originalBitfield ||
          copiedBitfield->get_parent() != variableDefinition_copy))) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[variable-definition-bitfield]: source=%p "
              "bitfield=%p copy=%p bitfield=%p parent=%p mapped=%p\n",
              static_cast<void *>(variableDefinition_original),
              static_cast<void *>(originalBitfield),
              static_cast<void *>(variableDefinition_copy),
              static_cast<void *>(copiedBitfield),
              static_cast<void *>(
                  copiedBitfield != NULL ? copiedBitfield->get_parent() : NULL),
              static_cast<void *>(lookupCopiedNode(help, originalBitfield)));
      ROSE_ABORT();
    }

    copyInitializedName->set_declptr(targetDeclaration);
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
        (copyInitializedName->get_declptr() != enumDeclaration_original &&
         copyInitializedName->get_declptr() != targetDeclaration)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[initialized-name-enum-declaration]: "
              "source=%p copy=%p declptr=%p target=%p\n",
              static_cast<void *>(enumDeclaration_original),
              static_cast<void *>(copyInitializedName),
              static_cast<void *>(copyInitializedName->get_declptr()),
              static_cast<void *>(targetDeclaration));
      ROSE_ABORT();
    }
    copyInitializedName->set_declptr(targetDeclaration);

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
        (copyInitializedName->get_declptr() != functionDeclaration_original &&
         copyInitializedName->get_declptr() != targetDeclaration)) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[initialized-name-function-declaration]: "
              "source=%p copy=%p declptr=%p target=%p\n",
              static_cast<void *>(functionDeclaration_original),
              static_cast<void *>(copyInitializedName),
              static_cast<void *>(copyInitializedName->get_declptr()),
              static_cast<void *>(targetDeclaration));
      ROSE_ABORT();
    }
    copyInitializedName->set_declptr(targetDeclaration);

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

// DQ (10/5/2007): Added IR node specific function to permit copies, via AST
// copy(), to be fixedup Usually this will correct scopes and in a few cases
// build child IR nodes that are not traversed (and thus shared in the
// result from the automatically generated copy function).
void SgInitializedName::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  // Guard against shared nodes. When nodes are shared (not copied), this ==
  // copy.
  if (this == copy) {
    // Node is shared, not copied. Skip fixup to avoid corrupting the
    // original.
    return;
  }

  // This is the empty default inplementation, not a problem if it is
  // called!

#if DEBUG_FIXUP_COPY
  // printf ("Inside of SgInitializedName::fixupCopy_scopes() %p = %s
  // \n",this,SageInterface::get_name(this).c_str());
  printf("Inside of SgInitializedName::fixupCopy_scopes() %p = %s \n", this,
         this->get_name().str());
#endif

  // Need to fixup the scope and perhaps build the SgVariableDefinition
  // object!
  SgInitializedName *initializedName_copy = isSgInitializedName(copy);
  ROSE_ASSERT(initializedName_copy != NULL);

  // DQ (4/21/2016): Replacing "__null" with more portable (non-gnu
  // specific) use using "NULL". ROSE_ASSERT
  // (initializedName_copy->get_declptr() !=
  // __null);
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
      originalRequires->get_local_parameter_list() == originalParameterList &&
      copiedRequires->get_local_parameter_list() == copiedParameterList &&
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

  // fprintf(stderr, "SgInitializedName::fixupCopy_scopes(%p) this=%p\n",
  // copy, this); fprintf(stderr, "Copy's scope is %p, my scope is %p\n",
  // initializedName_copy->get_scope(), this->get_scope());

  // ROSE_ASSERT(this->get_symbol_from_symbol_table() != NULL);

  SgScopeStatement *originalScope = get_scope();
  SgScopeStatement *provisionalScope = initializedName_copy->get_scope();
  if ((originalScope == NULL) != (provisionalScope == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[initialized-name-scope-null]: source=%p/%s "
            "scope=%p copy=%p scope=%p\n",
            static_cast<const void *>(this), get_name().str(),
            static_cast<void *>(originalScope),
            static_cast<void *>(initializedName_copy),
            static_cast<void *>(provisionalScope));
    ROSE_ABORT();
  }
  if (originalScope != NULL) {
    SgScopeStatement *resolvedScope =
        isSgScopeStatement(remapExactCopiedOrExternalSemanticEdge(
            originalScope, provisionalScope, help, "initialized-name-scope"));
    if (resolvedScope == NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[initialized-name-scope-kind]: source=%p/%s "
              "scope=%p copy=%p scope=%p resolved=%p\n",
              static_cast<const void *>(this), get_name().str(),
              static_cast<void *>(originalScope),
              static_cast<void *>(initializedName_copy),
              static_cast<void *>(provisionalScope),
              static_cast<void *>(resolvedScope));
      ROSE_ABORT();
    }
    initializedName_copy->set_scope(resolvedScope);
  }

  // Fix up the associated declaration/definition regardless of whether the
  // scope pointer needed updating.  Some node kinds (notably enum
  // enumerators) can have their scope copied correctly while still holding
  // a declptr that points back into the original AST, which breaks
  // symbol-table rebuilding in the copied tree (copyAST_copytest2007_50 /
  // Issue 69).
  SgNode *parent = initializedName_copy->get_parent();

  // printf ("In SgInitializedName::fixupCopy_scopes(): parent = %p
  // \n",parent);

  // Since the parent might not have been set yet we have to allow for this
  // case. In the case of a SgInitializedName in a SgVariableDeclaration the
  // SgInitializedName objects have their parents set after the
  // SgInitializedName is copied and in the copy function for the parent
  // (SgVariableDeclaration). fprintf (stderr, "In
  // SgInitializedName::fixupCopy_scopes(): parent = %p = %s
  // \n",parent,parent->class_name().c_str());
  if (parent != NULL) {
    ROSE_ASSERT(parent != NULL);
    // printf ("In SgInitializedName::fixupCopy_scopes(): parent = %p = %s
    // \n",parent,parent->class_name().c_str());

    switch (parent->variantT()) {
      // DQ (12/28/2012): Adding support for templates.
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

      // The parent of the SgFunctionParameterList might not have been set
      // yet, so allow for this!
      if (functionDeclaration != NULL) {
        // DQ (5/14/2012): Added simple test.
        ROSE_ASSERT(initializedName_copy != NULL);

        // DQ (5/14/2012): Added simple test, required in
        // resetVariableDefinitionSupport().
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

  if (this->get_prev_decl_item() != NULL) {
    FixupCopyDataMemberMacro(initializedName_copy, SgInitializedName,
                             get_prev_decl_item, set_prev_decl_item)
  }

#if DEBUG_FIXUP_COPY
  printf("Leaving SgInitializedName::fixupCopy_scopes() \n\n");
#endif
}

// DQ (11/1/2007): Build lighter weight versions of
// SgStatement::fixupCopy_scopes() and SgExpression::fixupCopy_scopes() and
// refactor code into the SgLocatedNode::fixupCopy_scopes().

void SgStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  // We need to call the fixupCopy function from the parent of a
  // SgVariableDeclaration because the copy function in the parent of the
  // variable declaration sets the parent of the SgVariableDeclaration and
  // we need this parent in the fixupCopy function in the SgInitializedName.

#if DEBUG_FIXUP_COPY
  printf("Inside of SgStatement::fixupCopy_scopes() for %p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

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
#if DEBUG_FIXUP_COPY
  printf("Inside of SgExpression::fixupCopy_scopes() for %p = %s copy = %p \n",
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

#if DEBUG_FIXUP_COPY
  printf("Inside of SgLocatedNode::fixupCopy_scopes() for %p = %s copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

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
  SgNode *copiedParent = plannedParent != NULL
                             ? plannedParent
                             : lookupCopiedNode(help, originalParent);
  const bool copiedTransactionRoot = copiedParent == NULL;
  if ((!copiedTransactionRoot &&
       (copyLocatedNode->get_parent() != copiedParent ||
        countDirectOwnerEdges(copiedParent, copyLocatedNode) != 1)) ||
      (copiedTransactionRoot && copyLocatedNode->get_parent() != NULL)) {
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

#if DEBUG_FIXUP_COPY
  printf("Leaving SgLocatedNode::fixupCopy_scopes() \n\n");
#endif
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

#if DEBUG_FIXUP_COPY
  printf("Inside of SgScopeStatement::fixupCopy_scopes() for %p = %s copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // printf ("\nInside of SgScopeStatement::fixupCopy_scopes() for %p = %s
  // copy = %p \n\n",this,this->class_name().c_str(),copy);
  // this->get_startOfConstruct()->display("Inside of
  // SgScopeStatement::fixupCopy_scopes()");

  SgScopeStatement *copyScopeStatement = isSgScopeStatement(copy);
  ROSE_ASSERT(copyScopeStatement != NULL);

  // DQ (10/24/2007): New test.
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

  // The symbol table should not have been setup yet!

  // DQ (5/21/2013): Restrict direct access to the symbol table.
  // if (copyScopeStatement->get_symbol_table()->size() != 0)
  if (copyScopeStatement->symbol_table_size() != 0) {
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

  // DQ (2/6/2009): Comment this out since it fails for the case of the
  // reverseTraversal tests.
  // ROSE_ASSERT(copyScopeStatement->get_symbol_table()->size() == 0);

  // Call the base class fixupCopy member function
  SgStatement::fixupCopy_scopes(copy, help);

  // printf ("\nLeaving SgScopeStatement::fixupCopy_scopes() for %p = %s
  // copy = %p \n\n",this,this->class_name().c_str(),copy);
}

void SgGlobal::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgGlobal::fixupCopy_scopes() for %p copy = %p \n", this,
         copy);
#endif

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

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_scopes(copy, help);

  // printf ("\nLeaving SgGlobal::fixupCopy_scopes() this = %p = %s  copy =
  // %p
  // \n",this,this->class_name().c_str(),copy);
}

// JJW 2/1/2008 -- Added support to fixup statement expressions
void SgExprStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgExprStatement::fixupCopy_scopes() for %p = %s copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgExprStatement *es_copy = isSgExprStatement(copy);
  ROSE_ASSERT(es_copy != NULL);

  SgExpression *expression_original = this->get_expression();
  SgExpression *expression_copy = es_copy->get_expression();

  expression_original->fixupCopy_scopes(expression_copy, help);

  // Call the base class fixupCopy member function
  SgStatement::fixupCopy_scopes(copy, help);

  // printf ("\nLeaving SgExprStatement::fixupCopy_scopes() this = %p = %s
  // copy = %p \n",this,this->class_name().c_str(),copy);
}

// DQ (10/6/2007): Added fixup function to set scopes not set properly by
// the ROSETTA generated copy!
void SgBasicBlock::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgBasicBlock::fixupCopy_scopes() for %p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgBasicBlock *block_copy = isSgBasicBlock(copy);
  ROSE_ASSERT(block_copy != NULL);

  const SgStatementPtrList &statementList_original = this->getStatementList();
  const SgStatementPtrList &statementList_copy = block_copy->getStatementList();

  // Check that this need not be handled as a special case such as SgIfStmt.
  if (this->containsOnlyDeclarations() == true) {
    ROSE_ASSERT(this->getDeclarationList().size() ==
                statementList_original.size());
  } else {
    ROSE_ASSERT(this->getStatementList().size() ==
                statementList_original.size());
  }

  SgStatementPtrList::const_iterator i_original =
      statementList_original.begin();
  SgStatementPtrList::const_iterator i_copy = statementList_copy.begin();

  // Iterate over both lists to match up the correct pairs of SgStatement
  // objects
  while ((i_original != statementList_original.end()) &&
         (i_copy != statementList_copy.end())) {
    (*i_original)->fixupCopy_scopes(*i_copy, help);

    i_original++;
    i_copy++;
  }

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_scopes(copy, help);

  // printf ("\nLeaving SgBasicBlock::fixupCopy_scopes() this = %p = %s copy
  // = %p \n",this,this->class_name().c_str(),copy);
}

void SgDeclarationStatement::fixupCopy_scopes(SgNode *copy,
                                              SgCopyHelp &help) const {
  // Guard against shared nodes. When nodes are shared (not copied), this ==
  // copy. Processing a shared node would corrupt the original AST by
  // setting its pointers to the copy's values. This can happen for
  // SgClassDefinition nodes from non-defining declarations, which are
  // shared by SgTreeCopy::copyAst().
  if (this == copy) {
    // Node is shared, not copied. Skip fixup to avoid corrupting the
    // original.
    return;
  }

  // Fixup must only walk original nodes. Re-entering an already-copied
  // declaration as if it were an original declaration manufactures
  // copy-of-copy map chains and eventually pairs declarations with deleted
  // second-generation copies.
  if (isCopiedNodeValue(help, this)) {
    return;
  }

#if DEBUG_FIXUP_COPY
  printf("Inside of SgDeclarationStatement::fixupCopy_scopes() for %p = %s "
         "copy = %p (defining = %p firstNondefining = %p) \n",
         this, this->class_name().c_str(), copy,
         this->get_definingDeclaration(),
         this->get_firstNondefiningDeclaration());
#endif

  // Need to fixup the scopes and defining and non-defining declaration.

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

  // DQ (10/12/2007): Set the scope for those SgDeclarationStatements which
  // store their scope explicitly. printf ("this->hasExplicitScope() = %s
  // \n",this->hasExplicitScope() ? "true" : "false");
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

    SgNode *mappedScope = lookupCopiedNode(help, originalScope);
    SgNode *plannedScope = help.plannedExternalSemanticCopyEdge(originalScope);
    SgNode *candidateScope =
        mappedScope != NULL
            ? mappedScope
            : (plannedScope != NULL ? plannedScope : originalScope);
    SgScopeStatement *provisionalScope = copyDeclarationStatement->get_scope();
    if (provisionalScope != originalScope &&
        provisionalScope != candidateScope) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[explicit-declaration-scope-state]: "
              "original=%p/%s scope=%p/%s copy=%p/%s provisional=%p/%s "
              "expected=%p/%s\n",
              static_cast<const void *>(this), this->class_name().c_str(),
              static_cast<void *>(originalScope),
              originalScope->class_name().c_str(),
              static_cast<void *>(copyDeclarationStatement),
              copyDeclarationStatement->class_name().c_str(),
              static_cast<void *>(provisionalScope),
              provisionalScope != NULL ? provisionalScope->class_name().c_str()
                                       : "<null>",
              static_cast<void *>(candidateScope),
              candidateScope != NULL ? candidateScope->class_name().c_str()
                                     : "<null>");
      ROSE_ABORT();
    }
    SgScopeStatement *copiedScope =
        isSgScopeStatement(requireExactCopiedOrExternalSemanticEdge(
            originalScope, candidateScope, help, "explicit-declaration-scope"));
    if (copiedScope == NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[explicit-declaration-scope-kind]: "
              "original=%p type=%s scope=%p/%s copy=%p mapped=%p/%s\n",
              static_cast<const void *>(this), this->class_name().c_str(),
              static_cast<void *>(originalScope),
              originalScope->class_name().c_str(),
              static_cast<void *>(copyDeclarationStatement),
              static_cast<void *>(candidateScope),
              candidateScope != NULL ? candidateScope->class_name().c_str()
                                     : "<null>");
      ROSE_ABORT();
    }
    copyDeclarationStatement->set_scope(copiedScope);

    // Make sure that the copy sets the scopes to be the same type
    if (copyDeclarationStatement->get_scope()->variantT() !=
        this->get_scope()->variantT()) {
      SgScopeStatement *mappedCopiedScope =
          isSgScopeStatement(lookupCopiedNode(help, this->get_scope()));
      fprintf(stderr, "Scope variant mismatch for %p = %s = %s\n", this,
              this->class_name().c_str(),
              SageInterface::get_name(this).c_str());
      fprintf(stderr, "  original scope = %p (%s)\n", this->get_scope(),
              this->get_scope()->class_name().c_str());
      fprintf(stderr, "  copied scope   = %p (%s)\n",
              copyDeclarationStatement->get_scope(),
              copyDeclarationStatement->get_scope()->class_name().c_str());
      SgNode *copyParent = copyDeclarationStatement->get_parent();
      fprintf(stderr, "  copy parent    = %p (%s)\n", copyParent,
              copyParent != NULL ? copyParent->class_name().c_str() : "null");
      fprintf(stderr, "  mapped scope   = %p (%s)\n", mappedCopiedScope,
              mappedCopiedScope != NULL
                  ? mappedCopiedScope->class_name().c_str()
                  : "null");
      fprintf(stderr, "  explicit scope = %s\n",
              this->hasExplicitScope() ? "true" : "false");
      fflush(stderr);
    }
    ROSE_ASSERT(copyDeclarationStatement->get_scope()->variantT() ==
                this->get_scope()->variantT());

    // A root copy is intentionally detached until its caller publishes it.
    // Structural source-file ancestry is therefore unavailable during this
    // fixup phase, and defining/nondefining declarations may legitimately
    // be spelled in different files.  Exact declaration-chain and namespace
    // identity are validated below without consulting transient ancestry.
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
      // printf ("Test for same namespace is more complex: definingNamespace
      // = %p firstNondefiningNamespace = %p
      // \n",definingNamespace,firstNondefiningNamespace);
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
      if (this->get_definingDeclaration()->get_scope() !=
          this->get_firstNondefiningDeclaration()->get_scope()) {
        // this->get_startOfConstruct()->display("Error: this scope
        // mismatch: debug");
        // this->get_definingDeclaration()->get_startOfConstruct()->display("Error:
        // definingDeclaration scope mismatch: debug");
        // this->get_firstNondefiningDeclaration()->get_startOfConstruct()->display("Error:
        // firstNondefiningDeclaration scope mismatch: debug");
      }

      // DQ (2/19/2009): Make sure that these are the same kind of IR nodes
      // since they might be in different files (instead of in the same
      // file). ROSE_ASSERT(this->get_definingDeclaration()->get_scope() ==
      // this->get_firstNondefiningDeclaration()->get_scope() );
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

    // ROSE_ASSERT(this->get_definingDeclaration()->get_scope() ==
    // this->get_firstNondefiningDeclaration()->get_scope() );
  }

  // DQ (10/19/2007): Added test...
  if (copyDeclarationStatement->get_definingDeclaration() != NULL &&
      copyDeclarationStatement->get_firstNondefiningDeclaration() != NULL) {
    // DQ (10/19/2007): Check the loop (not passible until both have been
    // processed)
    ROSE_ASSERT(copyDeclarationStatement->get_firstNondefiningDeclaration()
                    ->get_definingDeclaration() != NULL);
    ROSE_ASSERT(copyDeclarationStatement->get_definingDeclaration()
                    ->get_firstNondefiningDeclaration() != NULL);
    // ROSE_ASSERT(copyDeclarationStatement->get_firstNondefiningDeclaration()->get_definingDeclaration()
    // == copyDeclarationStatement->get_definingDeclaration());

    // We can't assert this yet since this is part of the copy of the
    // defining declaration within the processing of the non-defining
    // declaration (recurssively called!)
    // ROSE_ASSERT(copyDeclarationStatement->get_definingDeclaration()->get_firstNondefiningDeclaration()
    // == copyDeclarationStatement->get_firstNondefiningDeclaration());

    // If we can get to this point then we can perform this additional test
    // ROSE_ASSERT(copyDeclarationStatement->get_definingDeclaration()->get_scope()
    // ==
    // copyDeclarationStatement->get_firstNondefiningDeclaration()->get_scope()
    // );
  }

  // DQ (2/20/2009): Added assertion.
  // ROSE_ASSERT(copy->get_parent() != NULL);

  // Call the base class fixupCopy member function (this will setup the
  // parent)
  SgStatement::fixupCopy_scopes(copy, help);

  // DQ (2/20/2009): Note: These are allowed to be NULL a warning is issued
  // in SgLocatedNode::fixupCopy_scopes(). ROSE_ASSERT(copy->get_parent() !=
  // NULL);
}

void SgFunctionDeclaration::fixupCopy_scopes(SgNode *copy,
                                             SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

  SgFunctionDeclaration *functionDeclaration_copy =
      isSgFunctionDeclaration(copy);
  ROSE_ASSERT(functionDeclaration_copy != NULL);

#if DEBUG_FIXUP_COPY
  printf("\nIn SgFunctionDeclaration::fixupCopy_scopes(): for function = %s = "
         "%p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  // printf ("\nIn SgFunctionDeclaration::fixupCopy_scopes(): for function =
  // %s = %p = %s copy = %p
  // \n",this->get_name().str(),this,this->class_name().c_str(),copy);

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  validateExactCopiedFunctionDeclaratorScope(this, functionDeclaration_copy,
                                             help);

  // functionParameterScope is a typed semantic edge rather than a generated
  // traversal successor because another declaration in the canonical family
  // may reference its owner's scope.  When this declaration is the exact
  // structural owner, copy that scope in the same transaction before fixing
  // the parameter initialized-name scope edges.  Leaving the NO_TRAVERSAL
  // pointer shallow made every copied prototype parameter point back into the
  // original function.
  SgFunctionParameterScope *originalParameterScope =
      get_functionParameterScope();
  if (originalParameterScope != NULL) {
    SgFunctionParameterScope *copiedParameterScope = isSgFunctionParameterScope(
        lookupCopiedNode(help, originalParameterScope));
    const bool ownsParameterScope =
        originalParameterScope->get_parent() == this;
    if (copiedParameterScope == NULL && ownsParameterScope) {
      help.incrementDepth();
      copiedParameterScope =
          isSgFunctionParameterScope(originalParameterScope->copy(help));
      help.decrementDepth();
    }
    if (copiedParameterScope != NULL) {
      functionDeclaration_copy->set_functionParameterScope(
          copiedParameterScope);
      if (ownsParameterScope) {
        copiedParameterScope->set_parent(functionDeclaration_copy);
      }
    } else if (ownsParameterScope ||
               functionDeclaration_copy->get_functionParameterScope() !=
                   originalParameterScope) {
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
                copiedSyntaxArguments->get_arguments() ==
                    sourceSyntaxArguments->get_arguments() &&
                copiedSyntaxType->get_return_type() ==
                    sourceSyntaxType->get_return_type() &&
                copiedSyntaxType->get_has_ellipses() ==
                    sourceSyntaxType->get_has_ellipses() &&
                copiedSyntaxType->get_fortran_source_syntax() ==
                    sourceSyntaxType->get_fortran_source_syntax() &&
                ((sourceSyntaxMember != NULL) ==
                 (copiedSyntaxMember != NULL)) &&
                (sourceSyntaxMember == NULL ||
                 (copiedSyntaxMember->get_class_type() ==
                      sourceSyntaxMember->get_class_type() &&
                  copiedSyntaxMember->get_mfunc_specifier() ==
                      sourceSyntaxMember->get_mfunc_specifier()));
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
    if (mappedSyntaxParameters == NULL) {
      help.incrementDepth();
      mappedSyntaxParameters = sourceSyntaxParameters->copy(help);
      help.decrementDepth();
    }
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
    copiedSyntaxParameters->set_parent(functionDeclaration_copy);
  }
  functionDeclaration_copy->set_parameterList_syntax(copiedSyntaxParameters);

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

  // Setup the details in the SgFunctionDefinition (this may have to rebuild
  // the sysmbol table) printf ("In
  // SgFunctionDeclaration::fixupCopy_scopes(): this->get_definition() = %p
  // \n",this->get_definition());
  if (this->get_definition() != NULL) {
    // DQ (3/15/2014): The defining declaration should not be marked
    // (isForward() == true).
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

    // DQ (2/26/2009): Handle special cases where the copyHelp function is
    // non-trivial. Is every version of copyHelp object going to be a
    // problem?

    // For the outlining, our copyHelp object does not copy defining
    // function declarations and substitutes a non-defining declarations, so
    // if the copy has been built this way then skip trying to reset the
    // SgFunctionDefinition. printf ("In
    // SgFunctionDeclaration::fixupCopy_scopes():
    // functionDeclaration_copy->get_definition() = %p
    // \n",functionDeclaration_copy->get_definition());
    // this->get_definition()->fixupCopy_scopes(functionDeclaration_copy->get_definition(),help);
    if (functionDeclaration_copy->get_definition() != NULL) {
      this->get_definition()->fixupCopy_scopes(
          functionDeclaration_copy->get_definition(), help);
    }

    // If this is a declaration with a definition then it is a defining
    // declaration
    // functionDeclaration_copy->set_definingDeclaration(functionDeclaration_copy);
  }

  // printf ("\nLeaving SgFunctionDeclaration::fixupCopy_scopes(): for
  // function = %s = %p = %s copy = %p
  // \n",this->get_name().str(),this,this->class_name().c_str(),copy);
}

// DQ (10/6/2007): Added fixup function to set scopes not set properly by
// the ROSETTA generated copy!
void SgFunctionParameterList::fixupCopy_scopes(SgNode *copy,
                                               SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgFunctionParameterList::fixupCopy_scopes() for %p = %s "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Also call the base class version of the fixupCopycopy() member function
  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  SgFunctionParameterList *copyFunctionParameterList =
      isSgFunctionParameterList(copy);
  ROSE_ASSERT(copyFunctionParameterList != NULL);

  const SgInitializedNamePtrList &parameterList_original = this->get_args();
  SgInitializedNamePtrList &parameterList_copy =
      copyFunctionParameterList->get_args();

  SgInitializedNamePtrList::const_iterator i_original =
      parameterList_original.begin();
  SgInitializedNamePtrList::iterator i_copy = parameterList_copy.begin();

  // Iterate over both lists to match up the correct pairs of
  // SgInitializedName objects
  while ((i_original != parameterList_original.end()) &&
         (i_copy != parameterList_copy.end())) {
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

#if DEBUG_FIXUP_COPY
  printf("In SgMemberFunctionDeclaration::fixupCopy_scopes(): for function = "
         "%s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

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

  // The associated class is a semantic edge independent of the member's
  // structural parent and explicit scope.  The generated copy preserves the
  // source pointer, so rebind it to the exact copied class-family member while
  // the copy transaction still carries that identity map.
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
      copiedAssociatedClass == originalAssociatedClass ||
      copiedSemanticClass == NULL || copiedScopeClass == NULL ||
      copiedAssociatedCanonical == NULL || copiedScopeCanonical == NULL ||
      copiedAssociatedCanonical != copiedScopeCanonical) {
    fprintf(
        stderr,
        "REX_COPY_INVARIANT[member-associated-class]: original=%p member=%p "
        "associated=%p copy=%p associated=%p scope=%p scope-class=%p "
        "does not identify one exact copied class family\n",
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

#if DEBUG_FIXUP_COPY
  printf("\nInside of SgTemplateDeclaration::fixupCopy_scopes() for %p = %s "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgTemplateDeclaration *templateDeclaration_copy =
      isSgTemplateDeclaration(copy);
  ROSE_ASSERT(templateDeclaration_copy != NULL);

  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  // I don't think that this applies to the defining declaration...
  if (this != this->get_definingDeclaration()) {
    ROSE_ASSERT(templateDeclaration_copy->get_firstNondefiningDeclaration()
                    ->get_definingDeclaration() ==
                templateDeclaration_copy->get_definingDeclaration());

    // DQ (10/20/2007): this is a problem with copytest2007_12.C, I think it
    // should not be assered yet. We can't assert this yet since this is
    // part fo the copy of the defining declaration within the processing of
    // the non-defining declaration (recurssively called!)
    // ROSE_ASSERT(templateDeclaration_copy->get_definingDeclaration()->get_firstNondefiningDeclaration()
    // == templateDeclaration_copy->get_firstNondefiningDeclaration());

    // If we can get to this point then we can perform this additional test
    // ROSE_ASSERT(templateDeclaration_copy->get_definingDeclaration()->get_scope()
    // ==
    // templateDeclaration_copy->get_firstNondefiningDeclaration()->get_scope()
    // );
  }
}

void SgTemplateInstantiationDefn::fixupCopy_scopes(SgNode *copy,
                                                   SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

#if DEBUG_FIXUP_COPY
  printf("Inside of SgTemplateInstantiationDefn::fixupCopy_scopes() for class "
         "= %s class definition %p = %s copy = %p \n",
         this->get_declaration()->get_name().str(), this,
         this->class_name().c_str(), copy);
#endif

  ROSE_ASSERT(this->get_declaration() != NULL);

  // Call the base class fixupCopy member function
  SgClassDefinition::fixupCopy_scopes(copy, help);
}

void SgTemplateInstantiationDecl::fixupCopy_scopes(SgNode *copy,
                                                   SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

#if DEBUG_FIXUP_COPY
  printf("\nIn SgTemplateInstantiationDecl::fixupCopy_scopes(): for "
         "function = "
         "%s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  SgTemplateInstantiationDecl *templateClassDeclaration_copy =
      isSgTemplateInstantiationDecl(copy);
  ROSE_ASSERT(templateClassDeclaration_copy != NULL);

  // Also call the base class version of the fixupCopycopy() member function
  SgClassDeclaration::fixupCopy_scopes(copy, help);

  ROSE_ASSERT(this->get_templateDeclaration() != NULL);
  ROSE_ASSERT(templateClassDeclaration_copy->get_templateDeclaration() != NULL);

  // FixupCopyDataMemberMacro(templateClassDeclaration_copy,SgTemplateDeclaration,get_templateDeclaration,set_templateDeclaration)
  FixupCopyDataMemberMacro(templateClassDeclaration_copy,
                           SgTemplateClassDeclaration, get_templateDeclaration,
                           set_templateDeclaration)
}

void SgTemplateInstantiationMemberFunctionDecl::fixupCopy_scopes(
    SgNode *copy, SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

#if DEBUG_FIXUP_COPY
  printf("\nIn SgTemplateInstantiationMemberFunctionDecl::fixupCopy_scopes(): "
         "for function = %s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  ROSE_ASSERT(this != NULL);
  SgTemplateInstantiationMemberFunctionDecl
      *templateMemberFunctionDeclaration_copy =
          isSgTemplateInstantiationMemberFunctionDecl(copy);
  ROSE_ASSERT(templateMemberFunctionDeclaration_copy != NULL);

  // Also call the base class version of the fixupCopycopy() member function
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

  // FixupCopyDataMemberMacro(templateMemberFunctionDeclaration_copy,SgTemplateDeclaration,get_templateDeclaration,set_templateDeclaration)
  FixupCopyDataMemberMacro(templateMemberFunctionDeclaration_copy,
                           SgTemplateMemberFunctionDeclaration,
                           get_templateDeclaration, set_templateDeclaration)
}

void SgTemplateInstantiationFunctionDecl::fixupCopy_scopes(
    SgNode *copy, SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

#if DEBUG_FIXUP_COPY
  printf("\nIn SgTemplateInstantiationFunctionDecl::fixupCopy_scopes(): for "
         "function = %s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  // DQ (12/28/2012): Added asseretion.
  ROSE_ASSERT(this != NULL);

  SgTemplateInstantiationFunctionDecl *templateFunctionDeclaration_copy =
      isSgTemplateInstantiationFunctionDecl(copy);
  ROSE_ASSERT(templateFunctionDeclaration_copy != NULL);

  // Also call the base class version of the fixupCopycopy() member function
  SgFunctionDeclaration::fixupCopy_scopes(copy, help);

  ROSE_ASSERT(this->get_templateDeclaration() != NULL);
  ROSE_ASSERT(templateFunctionDeclaration_copy->get_templateDeclaration() !=
              NULL);

  // FixupCopyDataMemberMacro(templateFunctionDeclaration_copy,SgTemplateDeclaration,get_templateDeclaration,set_templateDeclaration)
  FixupCopyDataMemberMacro(templateFunctionDeclaration_copy,
                           SgTemplateFunctionDeclaration,
                           get_templateDeclaration, set_templateDeclaration)
}

void SgFunctionDefinition::fixupCopy_scopes(SgNode *copy,
                                            SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgFunctionDefinition::fixupCopy_scopes() for %p = %s copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgFunctionDefinition *functionDefinition_copy = isSgFunctionDefinition(copy);
  ROSE_ASSERT(functionDefinition_copy != NULL);

  // Setup the scopes of the SgInitializedName objects in the paraleter list
  ROSE_ASSERT(get_body() != NULL);
  if (functionDefinition_copy->get_body() != NULL &&
      functionDefinition_copy->get_body()->get_parent() !=
          functionDefinition_copy) {
    // Reused copied bodies can remain parented to an earlier copied
    // function definition in the same declaration chain. Re-anchor the body
    // to the current copy before fixing nested labels and rebuilding symbol
    // tables.
    functionDefinition_copy->get_body()->set_parent(functionDefinition_copy);
  }
  get_body()->fixupCopy_scopes(functionDefinition_copy->get_body(), help);

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_scopes(copy, help);

  // printf ("\nLeaving SgFunctionDefinition::fixupCopy_scopes() for %p = %s
  // copy = %p \n\n",this,this->class_name().c_str(),copy);
}

void SgVariableDeclaration::fixupCopy_scopes(SgNode *copy,
                                             SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgVariableDeclaration::fixupCopy_scopes() for %p = %s "
         "copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Also call the base class version of the fixupCopycopy() member function
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

    // printf ("baseTypeDeclaration_original = %p = %s
    // \n",baseTypeDeclaration_original,baseTypeDeclaration_original->class_name().c_str());
    // printf ("baseTypeDeclaration_copy = %p = %s
    // \n",baseTypeDeclaration_copy,baseTypeDeclaration_copy->class_name().c_str());

    // DQ (10/17/2007): This is now fixed!
    // I think that calling get_baseTypeDefiningDeclaration() is a problem
    // because it calls compute_baseTypeDefiningDeclaration() which uses the
    // symbol table and other infor which is not setup correctly. printf
    // ("Need to compute the baseTypeDeclaration_copy better (perhaps we
    // shoul look into the map of copies? \n"); ROSE_ABORT();

    ROSE_ASSERT(baseTypeDeclaration_original != NULL);
    ROSE_ASSERT(baseTypeDeclaration_copy != NULL);

    // printf ("In SgVariableDeclaration::fixupCopy_scopes(): Calling
    // fixupCopy on %p = %s
    // \n",baseTypeDeclaration_original,baseTypeDeclaration_original->class_name().c_str());

    baseTypeDeclaration_original->fixupCopy_scopes(baseTypeDeclaration_copy,
                                                   help);
  }

  const SgInitializedNamePtrList &variableList_original = this->get_variables();
  SgInitializedNamePtrList &variableList_copy =
      variableDeclaration_copy->get_variables();

  // printf ("Inside of SgVariableDeclaration::fixupCopy_scopes():
  // variableList_original.size() = %ld
  // \n",(long)variableList_original.size());

  ROSE_ASSERT(variableList_original.size() == variableList_copy.size());

  SgInitializedNamePtrList::const_iterator i_original =
      variableList_original.begin();
  SgInitializedNamePtrList::iterator i_copy = variableList_copy.begin();

  // Iterate over both lists to match up the correct pairs of
  // SgInitializedName objects
  while ((i_original != variableList_original.end()) &&
         (i_copy != variableList_copy.end())) {
    // printf ("Looping over the initialized names in the variable
    // declaration variable = %p = %s
    // \n",(*i_copy),(*i_copy)->get_name().str());
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
    for (SgCopyHelp::copiedNodeMapTypeIterator it =
             help.get_copiedNodeMap().begin();
         it != help.get_copiedNodeMap().end(); ++it) {
      if (it->second == this) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[class-fixup-source-is-copy]: source=%p/%s "
                "requested-copy=%p/%s is also mapped from %p/%s identity=%d\n",
                static_cast<const void *>(this), this->class_name().c_str(),
                static_cast<void *>(copy),
                copy != NULL ? copy->class_name().c_str() : "<null>",
                static_cast<const void *>(it->first),
                it->first != NULL ? it->first->class_name().c_str() : "<null>",
                it->first == this ? 1 : 0);
      }
    }
    ROSE_ABORT();
  }

  // We need to call the fixupCopy function from the parent of a
  // SgVariableDeclaration because the copy function in the parent of the
  // variable declaration sets the parent of the SgVariableDeclaration and
  // we need this parent in the fixupCopy function in the SgInitializedName.

#if DEBUG_FIXUP_COPY
  printf("Inside of SgClassDeclaration::fixupCopy_scopes() for class = %s "
         "= %p "
         "= %s copy = %p (defining = %p firstNondefining = %p) \n",
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
  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  if (isForward() == false) {
    // DQ (12/24/2012): Added assertion (debugging template support).
    ROSE_ASSERT(classDefinition_original != NULL);
    ROSE_ASSERT(classDefinition_copy != NULL);

    classDefinition_original->fixupCopy_scopes(classDefinition_copy, help);
  }

  const SgClassDeclaration *classDeclaration_original =
      isSgClassDeclaration(this);

  // DQ (3/15/2014): Check the types since they should be equivalent (setup
  // intermediate variables).
  SgClassDeclaration *classDeclaration_copy_firstNondefining =
      isSgClassDeclaration(
          classDeclaration_copy->get_firstNondefiningDeclaration());
  if (classDeclaration_copy_firstNondefining == NULL) {
    classDeclaration_copy_firstNondefining = classDeclaration_copy;
  }
  SgClassDeclaration *classDeclaration_copy_defining =
      isSgClassDeclaration(classDeclaration_copy->get_definingDeclaration());
  ROSE_ASSERT(classDeclaration_copy_firstNondefining != NULL);
  // ROSE_ASSERT(classDeclaration_copy_defining != NULL);

  // DQ (3/15/2014): Check the types since they should be equivalent (setup
  // intermediate variables).
  SgClassDeclaration *classDeclaration_original_firstNondefining =
      isSgClassDeclaration(
          classDeclaration_original->get_firstNondefiningDeclaration());
  if (classDeclaration_original_firstNondefining == NULL) {
    classDeclaration_original_firstNondefining =
        const_cast<SgClassDeclaration *>(classDeclaration_original);
  }
  SgClassDeclaration *classDeclaration_original_defining = isSgClassDeclaration(
      classDeclaration_original->get_definingDeclaration());
  ROSE_ASSERT(classDeclaration_original_firstNondefining != NULL);
  // ROSE_ASSERT(classDeclaration_original_defining != NULL);

  SgClassType *classType = finalizeExactCopiedClassType(
      classDeclaration_original, classDeclaration_copy, help);
  ROSE_ASSERT(classType != NULL);

  // DQ (3/15/2014): Check the types since they should be equivalent (error
  // checking).
  if (classDeclaration_copy_defining != NULL) {
    ROSE_ASSERT(classDeclaration_copy_firstNondefining->get_type() ==
                classDeclaration_copy_defining->get_type());
  }

  // DQ (3/17/2014): These types should be equivalent.
  // DQ (3/15/2014): Check the types since they should be equivalent.
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
    // ROSE_ASSERT(classDeclaration_original_firstNondefining->get_type() ==
    // classDeclaration_original_defining->get_type());
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

  // DQ (10/19/2007): Added support to fixup the base class names

  ROSE_ASSERT(this->get_declaration() != NULL);

#if DEBUG_FIXUP_COPY
  printf("Inside of SgClassDefinition::fixupCopy_scopes() for class = %s "
         "class "
         "definition %p = %s copy = %p \n",
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

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_scopes(copy, help);

  // printf ("\nLeaving SgClassDefinition::fixupCopy_scopes() this = %p = %s
  // copy = %p \n",this,this->class_name().c_str(),copy);
}

void SgBaseClass::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  // DQ (10/19/2007): Added support to fixup the base class names

#if DEBUG_FIXUP_COPY
  printf("Inside of SgBaseClass::fixupCopy_scopes() for baseclass = %p = %s  "
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
    SgClassDeclaration *expected =
        isSgClassDeclaration(remapExactCopiedOrExternalSemanticEdge(
            this->get_base_class(), baseClass_copy->get_base_class(), help,
            "base-class-declaration"));
    if (expected == NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[base-class-declaration]: source=%p/%s "
              "copy=%p has no exact class declaration edge\n",
              static_cast<const void *>(this->get_base_class()),
              this->get_base_class()->class_name().c_str(),
              static_cast<void *>(baseClass_copy));
      ROSE_ABORT();
    }
    baseClass_copy->set_base_class(expected);
  } else if (nrBaseClass != NULL) {
    ROSE_ASSERT(nrBaseClass->get_base_class_nonreal() != NULL);

    ROSE_ASSERT(nrBaseClass_copy != NULL);
    SgNonrealDecl *expected =
        isSgNonrealDecl(remapExactCopiedOrExternalSemanticEdge(
            nrBaseClass->get_base_class_nonreal(),
            nrBaseClass_copy->get_base_class_nonreal(), help,
            "nonreal-base-class-declaration"));
    if (expected == NULL) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[nonreal-base-class-declaration]: "
              "source=%p/%s copy=%p has no exact nonreal declaration edge\n",
              static_cast<const void *>(nrBaseClass->get_base_class_nonreal()),
              nrBaseClass->get_base_class_nonreal()->class_name().c_str(),
              static_cast<void *>(nrBaseClass_copy));
      ROSE_ABORT();
    }
    nrBaseClass_copy->set_base_class_nonreal(expected);
  } else {
    ROSE_ABORT();
  }
}

void SgLabelStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgLabelStatement::fixupCopy_scopes() for %p = %s copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgLabelStatement *labelStatement_copy = isSgLabelStatement(copy);
  ROSE_ASSERT(labelStatement_copy != NULL);

  // I don't think there is anything to do here since we already make sure
  // that there is a new SgLabelSymbol and it is the SgGotoStatement that
  // has to have it's reference to the label fixed up if the label and the
  // goto statement have been copied.

  // DQ (10/25/2007): Added handling for the new explicit scope in
  // SgLabelStatement
  ROSE_ASSERT(this->hasExplicitScope() == true);

  FixupCopyDataMemberMacro(labelStatement_copy, SgScopeStatement, get_scope,
                           set_scope)

      // Reused copied labels can already point at an earlier copied
      // function definition, which prevents the generic original->copy
      // remap above from updating the explicit label scope. Re-anchor
      // labels to the copied AST's owning function before symbol-table
      // rebuilding.
      if (isSgFunctionDefinition(this->get_scope()) != NULL) {
    if (SgFunctionDefinition *copiedFunctionScope =
            SageInterface::getEnclosingFunctionDefinition(labelStatement_copy,
                                                          true)) {
      if (labelStatement_copy->get_scope() != copiedFunctionScope) {
        labelStatement_copy->set_scope(copiedFunctionScope);
      }
    }
  }

  // Make sure that the copy sets the scopes to be the same type
  ROSE_ASSERT(labelStatement_copy->get_scope()->variantT() ==
              this->get_scope()->variantT());

  // Also call the base class version of the fixupCopycopy() member function
  SgStatement::fixupCopy_scopes(copy, help);
}

void SgGotoStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgGotoStatement::fixupCopy_scopes() for %p = %s copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgGotoStatement *gotoStatement_copy = isSgGotoStatement(copy);
  ROSE_ASSERT(gotoStatement_copy != NULL);

  SgLabelStatement *labelStatement_original = get_label();

  SgCopyHelp::copiedNodeMapTypeIterator i =
      help.get_copiedNodeMap().find(labelStatement_original);

  // printf ("Inside of SgGotoStatement::fixupCopy_scopes(): i !=
  // help.get_copiedNodeMap().end() = %s \n",(i !=
  // help.get_copiedNodeMap().end()) ? "true" : "false");

  // If the declaration is in the map then it is because we have copied it
  // previously and thus it should be updated to reflect the copied
  // declaration.
  if (i != help.get_copiedNodeMap().end()) {
    SgLabelStatement *labelStatement_copy = isSgLabelStatement(i->second);
    ROSE_ASSERT(labelStatement_copy != NULL);
    gotoStatement_copy->set_label(labelStatement_copy);
  }

  // Also call the base class version of the fixupCopycopy() member function
  SgStatement::fixupCopy_scopes(copy, help);
}

void SgTypedefDeclaration::fixupCopy_scopes(SgNode *copy,
                                            SgCopyHelp &help) const {
  // We need to call the fixupCopy function from the parent of a
  // SgTypedefDeclaration because the copy function in the parent of the
  // variable declaration sets the parent of the SgVariableDeclaration and
  // we need this parent in the fixupCopy function in the SgInitializedName.

#if DEBUG_FIXUP_COPY
  printf("Inside of SgTypedefDeclaration::fixupCopy_scopes() for typedef name "
         "= %s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  // Call the base class fixupCopy member function
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
  } else {
    if (this->get_declaration() != NULL) {
      if (typedefDeclaration_copy->get_declaration() == NULL ||
          typedefDeclaration_copy->get_declaration() ==
              this->get_declaration()) {
        if (SgDeclarationStatement *copiedReferencedDecl =
                resolveCopiedReferencedDeclaration(this->get_declaration(),
                                                   help)) {
          typedefDeclaration_copy->set_declaration(copiedReferencedDecl);
        }
      }

      SgDeclarationStatement *copiedReferencedDecl =
          typedefDeclaration_copy->get_declaration();
      ROSE_ASSERT(copiedReferencedDecl != NULL);
      if (copiedReferencedDecl == this->get_declaration()) {
        ROSE_ASSERT(
            referencedDeclarationMayRemainShared(this->get_declaration()));
        return;
      }
      this->get_declaration()->fixupCopy_scopes(copiedReferencedDecl, help);
    }
  }
}

void SgEnumDeclaration::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgEnumDeclaration::fixupCopy_scopes() for %p = %s copy = "
         "%p (defining = %p firstNondefining = %p) \n",
         this, this->class_name().c_str(), copy,
         this->get_definingDeclaration(),
         this->get_firstNondefiningDeclaration());
#endif

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  SgEnumDeclaration *enumDeclaration_copy = isSgEnumDeclaration(copy);
  ROSE_ASSERT(enumDeclaration_copy != NULL);

  // DQ (10/17/2007): fixup the type used to make sure it has the
  // declaration set the AST copy.
  SgEnumType *enum_type_original = this->get_type();
  ROSE_ASSERT(enum_type_original != NULL);

  SgEnumType *enum_type_copy = enumDeclaration_copy->get_type();
  ROSE_ASSERT(enum_type_copy != NULL);

  finalizeExactCopiedEnumDeclarationChain(this, enumDeclaration_copy, help);

  // printf ("This is the non-defining declaration, so just fixup the
  // SgEnumType = %p with the correct SgEnumDeclaration declaration!
  // \n",enum_type_copy);

  // Only modify declaration pointer if the type was actually copied (not
  // shared). Since SgTreeCopy shares types (types are not deep copied),
  // enum_type_copy == enum_type_original. Modifying the shared type's
  // declaration would corrupt the original AST.
  // FixupCopyDataMemberMacro_local_debug(enum_type_copy,SgDeclarationStatement,get_declaration,set_declaration)
  if (enum_type_copy != enum_type_original) {
    SgDeclarationStatement *local_copy = isSgDeclarationStatement(
        requireExactMappedNode(enum_type_original->get_declaration(), help,
                               "enum-type-declaration"));
    ROSE_ASSERT(local_copy != NULL);
    enum_type_copy->set_declaration(local_copy);
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
  while ((i_original != enumFieldList_original.end()) &&
         (i_copy != enumFieldList_copy.end())) {
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

#if DEBUG_FIXUP_COPY
  printf("Inside of SgNamespaceDeclarationStatement::fixupCopy_scopes() "
         "for %p "
         "= %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Call the base class fixupCopy member function
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

  // printf ("namespaceDeclaration_copy->get_firstNondefiningDeclaration() =
  // %p
  // \n",namespaceDeclaration_copy->get_firstNondefiningDeclaration());
  if (this->get_firstNondefiningDeclaration() == this) {
    // printf ("&&&&& Resetting copy's firstNondefiningDeclaration of
    // SgNamespaceDeclarationStatement to copy \n");
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

#if DEBUG_FIXUP_COPY
  printf("Inside of SgNamespaceDefinitionStatement::fixupCopy_scopes() for %p "
         "= %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgNamespaceDefinitionStatement *namespaceDefinition_copy =
      isSgNamespaceDefinitionStatement(copy);
  ROSE_ASSERT(namespaceDefinition_copy != NULL);

  FixupCopyDataMemberMacro(namespaceDefinition_copy,
                           SgNamespaceDeclarationStatement,
                           get_namespaceDeclaration, set_namespaceDeclaration)

      // Namespace definitions are re-entrant and use a shared "global
      // definition" scope to accumulate symbols across reopenings.  The
      // ROSETTA-generated copy preserves the original pointer; repair it to
      // point into the copied tree so that symbol-table insertion and
      // lookup stay within the copy (Issue 69). NOTE:
      // FixupCopyDataMemberMacro assumes non-null pointers. The namespace
      // linkage pointers can legitimately be NULL at the ends of the
      // reentrant chain, so we repair them carefully.
      if (namespaceDefinition_copy->get_global_definition() ==
              this->get_global_definition() &&
          this->get_global_definition() != NULL) {
    SgCopyHelp::copiedNodeMapTypeIterator iter =
        help.get_copiedNodeMap().find(this->get_global_definition());
    if (iter != help.get_copiedNodeMap().end()) {
      SgNode *associated_node_copy = iter->second;
      ROSE_ASSERT(associated_node_copy != NULL);
      SgNamespaceDefinitionStatement *global_copy =
          isSgNamespaceDefinitionStatement(associated_node_copy);
      ROSE_ASSERT(global_copy != NULL);
      namespaceDefinition_copy->set_global_definition(global_copy);
    }
  }

  if (namespaceDefinition_copy->get_previousNamespaceDefinition() ==
          this->get_previousNamespaceDefinition() &&
      this->get_previousNamespaceDefinition() != NULL) {
    SgCopyHelp::copiedNodeMapTypeIterator iter =
        help.get_copiedNodeMap().find(this->get_previousNamespaceDefinition());
    if (iter != help.get_copiedNodeMap().end()) {
      SgNode *associated_node_copy = iter->second;
      ROSE_ASSERT(associated_node_copy != NULL);
      SgNamespaceDefinitionStatement *prev_copy =
          isSgNamespaceDefinitionStatement(associated_node_copy);
      ROSE_ASSERT(prev_copy != NULL);
      namespaceDefinition_copy->set_previousNamespaceDefinition(prev_copy);
    }
  }

  if (namespaceDefinition_copy->get_nextNamespaceDefinition() ==
          this->get_nextNamespaceDefinition() &&
      this->get_nextNamespaceDefinition() != NULL) {
    SgCopyHelp::copiedNodeMapTypeIterator iter =
        help.get_copiedNodeMap().find(this->get_nextNamespaceDefinition());
    if (iter != help.get_copiedNodeMap().end()) {
      SgNode *associated_node_copy = iter->second;
      ROSE_ASSERT(associated_node_copy != NULL);
      SgNamespaceDefinitionStatement *next_copy =
          isSgNamespaceDefinitionStatement(associated_node_copy);
      ROSE_ASSERT(next_copy != NULL);
      namespaceDefinition_copy->set_nextNamespaceDefinition(next_copy);
    }
  }

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

  // Call the base class fixupCopy member function
  SgScopeStatement::fixupCopy_scopes(copy, help);
}

void SgTemplateInstantiationDirectiveStatement::fixupCopy_scopes(
    SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of "
         "SgTemplateInstantiationDirectiveStatement::fixupCopy_scopes() "
         "for %p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgTemplateInstantiationDirectiveStatement
      *templateInstantiationDirectiveStatement_copy =
          isSgTemplateInstantiationDirectiveStatement(copy);

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  ROSE_ASSERT(this->get_declaration() != NULL);

  FixupCopyDataMemberMacro(templateInstantiationDirectiveStatement_copy,
                           SgDeclarationStatement, get_declaration,
                           set_declaration)

      // DQ (11/7/2007): Call fixup on the declaration stored internally (a
      // copy, not shared).
      if (templateInstantiationDirectiveStatement_copy->get_declaration() ==
              NULL ||
          templateInstantiationDirectiveStatement_copy->get_declaration() ==
              this->get_declaration()) {
    if (SgDeclarationStatement *copiedReferencedDecl =
            resolveCopiedReferencedDeclaration(this->get_declaration(), help)) {
      templateInstantiationDirectiveStatement_copy->set_declaration(
          copiedReferencedDecl);
    }
  }

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
#if DEBUG_FIXUP_COPY
  printf("Inside of SgProject::fixupCopy_scopes() \n");
#endif

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
#if DEBUG_FIXUP_COPY
  printf("Inside of SgSourceFile::fixupCopy_scopes() \n");
#endif

  SgSourceFile *file_copy = isSgSourceFile(copy);
  ROSE_ASSERT(file_copy != NULL);

  // Call fixup on the global scope
  ROSE_ASSERT(get_globalScope() != NULL);
  ROSE_ASSERT(file_copy->get_globalScope() != NULL);
  get_globalScope()->fixupCopy_scopes(file_copy->get_globalScope(), help);
}

void SgIfStmt::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgIfStmt::fixupCopy_scopes() this = %p = %s  copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // printf ("Inside of SgIfStmt::fixupCopy_scopes() this = %p = %s  copy =
  // %p
  // \n",this,this->class_name().c_str(),copy);

  // SgStatement::fixupCopy_scopes(copy,help);
  SgScopeStatement::fixupCopy_scopes(copy, help);

  SgIfStmt *ifStatement_copy = isSgIfStmt(copy);
  ROSE_ASSERT(ifStatement_copy != NULL);

  // The symbol table should not have been setup yet!
  // ROSE_ASSERT(ifStatement_copy->get_symbol_table()->size() == 0);

  this->get_conditional()->fixupCopy_scopes(ifStatement_copy->get_conditional(),
                                            help);

  SgStatement *thsTruBody = this->get_true_body();
  ROSE_ASSERT(thsTruBody != NULL);
  SgStatement *ifStmtCopyTruBody = ifStatement_copy->get_true_body();
  ROSE_ASSERT(ifStmtCopyTruBody != NULL);
  SgScopeStatement *scopeIfStmtCopyTruBody =
      isSgScopeStatement(ifStmtCopyTruBody);
  if (scopeIfStmtCopyTruBody != NULL) {
    // DQ (5/21/2013): Restrict direct access to the symbol table.
    // ROSE_ASSERT(scopeIfStmtCopyTruBody->get_symbol_table() != NULL);
    // ROSE_ASSERT(scopeIfStmtCopyTruBody->get_symbol_table()->size()  ==
    // 0);
    if (scopeIfStmtCopyTruBody->symbol_table_size() != 0) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[if-true-symbol-table-state]: body=%p "
              "copied symbol table contains %zu entries before exact body "
              "finalization\n",
              static_cast<void *>(scopeIfStmtCopyTruBody),
              scopeIfStmtCopyTruBody->symbol_table_size());
      ROSE_ABORT();
    }
    // DQ (3/3/12): This fails for the g++ version 4.2.4 compiler (newer
    // versions of g++ pass fine).
    // ROSE_ASSERT(scopeIfStmtCopyTruBody->symbol_table_size() == 0);
  }

  // printf ("\nProcess the TRUE body of the SgIfStmt \n\n");

  thsTruBody->fixupCopy_scopes(ifStmtCopyTruBody, help);

  ROSE_ASSERT((this->get_false_body() != NULL) ==
              (ifStatement_copy->get_false_body() != NULL));
  if (isSgScopeStatement(ifStatement_copy->get_false_body()) != NULL) {
    // DQ (5/21/2013): Restrict direct access to the symbol table.
    // ROSE_ASSERT(isSgScopeStatement(ifStatement_copy->get_false_body())->get_symbol_table()->size()
    // == 0);
    // ROSE_ASSERT(isSgScopeStatement(ifStatement_copy->get_false_body())->get_symbol_table()
    // != NULL);
    SgScopeStatement *scopeStmnt =
        isSgScopeStatement(ifStatement_copy->get_false_body());
    if (scopeStmnt->symbol_table_size() != 0) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[if-false-symbol-table-state]: body=%p "
              "copied symbol table contains %zu entries before exact body "
              "finalization\n",
              static_cast<void *>(scopeStmnt), scopeStmnt->symbol_table_size());
      ROSE_ABORT();
    }
    // DQ (3/3/12): This fails for the g++ version 4.2.4 compiler (newer
    // versions of g++ pass fine).
    // ROSE_ASSERT(scopeStmnt->symbol_table_size()
    // == 0);
  }

  // printf ("\nProcess the FALSE body of the SgIfStmt \n\n");

  if (this->get_false_body() != NULL) {
    this->get_false_body()->fixupCopy_scopes(ifStatement_copy->get_false_body(),
                                             help);
  }

  // printf ("\nLeaving SgIfStmt::fixupCopy_scopes() this = %p = %s  copy =
  // %p
  // \n",this,this->class_name().c_str(),copy);
}

void SgForStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgForStatement::fixupCopy_scopes() this = %p = %s  copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgForStatement *forStatement_copy = isSgForStatement(copy);
  ROSE_ASSERT(forStatement_copy != NULL);

  // This could generate a vaiable declaration, so wait to build the sysmbol
  // table.
  ROSE_ASSERT(this->get_for_init_stmt() != NULL);
  this->get_for_init_stmt()->fixupCopy_scopes(
      forStatement_copy->get_for_init_stmt(), help);

  // DQ (11/1/2007): Force the symbol table to be setup so that references
  // can be made to it later. If we built it too early then the scope (on
  // The SgInitializedName objects) have not be setup, and if we build it
  // too late then we don't have the symbols in place to reset the
  // references. printf
  // ("Calling SgScopeStatement::fixupCopy_scopes() \n");
  SgScopeStatement::fixupCopy_scopes(copy, help);
  // printf ("DONE: SgScopeStatement::fixupCopy_scopes() \n");

  // This could generate a vaiable declaration, so wait to build the sysmbol
  // table.
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
#if DEBUG_FIXUP_COPY
  printf("Inside of SgRangeBasedForStatement::fixupCopy_scopes() this = %p = "
         "%s  copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgRangeBasedForStatement *forStatement_copy =
      isSgRangeBasedForStatement(copy);
  ROSE_ASSERT(forStatement_copy != NULL);

  // This could generate a vaiable declaration, so wait to build the sysmbol
  // table.
  ROSE_ASSERT(this->get_iterator_declaration() != NULL);
  this->get_iterator_declaration()->fixupCopy_scopes(
      forStatement_copy->get_iterator_declaration(), help);

  ROSE_ASSERT(this->get_range_declaration() != NULL);
  this->get_range_declaration()->fixupCopy_scopes(
      forStatement_copy->get_range_declaration(), help);

  // DQ (11/1/2007): Force the symbol table to be setup so that references
  // can be made to it later. If we built it too early then the scope (on
  // The SgInitializedName objects) have not be setup, and if we build it
  // too late then we don't have the symbols in place to reset the
  // references. printf
  // ("Calling SgScopeStatement::fixupCopy_scopes() \n");
  SgScopeStatement::fixupCopy_scopes(copy, help);
  // printf ("DONE: SgScopeStatement::fixupCopy_scopes() \n");

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
#if DEBUG_FIXUP_COPY
  printf("Inside of SgForInitStatement::fixupCopy_scopes() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_scopes(copy, help);

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
    // printf ("In SgForInitStatement::fixupCopy_scopes(): Calling fixup for
    // *i_copy = %p = %s \n",(*i_copy),(*i_copy)->class_name().c_str());
    (*i_original)->fixupCopy_scopes(*i_copy, help);

    i_original++;
    i_copy++;
  }

  // Relavant data member is: SgStatementPtrList        p_init_stmt
}

void SgCatchStatementSeq::fixupCopy_scopes(SgNode *copy,
                                           SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgCatchStatementSeq::fixupCopy_scopes() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

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
#if DEBUG_FIXUP_COPY
  printf("Inside of SgWhileStmt::fixupCopy_scopes() this = %p = %s  copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

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
#if DEBUG_FIXUP_COPY
  printf("Inside of SgDoWhileStmt::fixupCopy_scopes() this = %p = %s  copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

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
#if DEBUG_FIXUP_COPY
  printf("Inside of SgSwitchStatement::fixupCopy_scopes() this = %p = %s  "
         "copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

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
#if DEBUG_FIXUP_COPY
  printf("Inside of SgTryStmt::fixupCopy_scopes() this = %p = %s  copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif

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
#if DEBUG_FIXUP_COPY
  printf("Inside of SgCatchOptionStmt::fixupCopy_scopes() this = %p = %s  "
         "copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

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
#if DEBUG_FIXUP_COPY
  printf("Inside of SgCaseOptionStmt::fixupCopy_scopes() this = %p = %s  copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_scopes(copy, help);

  SgCaseOptionStmt *caseOptionStatement_copy = isSgCaseOptionStmt(copy);
  ROSE_ASSERT(caseOptionStatement_copy != NULL);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_scopes(caseOptionStatement_copy->get_body(),
                                     help);
}

void SgDefaultOptionStmt::fixupCopy_scopes(SgNode *copy,
                                           SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgDefaultOptionStmt::fixupCopy_scopes() this = %p = %s  "
         "copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

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
      isSgDeclarationStatement(remapExactCopiedOrExternalSemanticEdge(
          originalTemplate, templateArgument_copy->get_templateDeclaration(),
          help, "template-argument-declaration"));
  if ((get_argumentType() == SgTemplateArgument::template_template_argument) !=
          (originalTemplate != NULL) ||
      (originalTemplate != NULL && copiedTemplate == NULL)) {
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
  templateArgument_copy->set_templateDeclaration(copiedTemplate);
  if (originalTemplate != NULL) {
    requireExactCopiedOrExternalSemanticEdge(originalTemplate, copiedTemplate,
                                             help,
                                             "template-argument-declaration");
  }

  SgType *originalType = get_type();
  SgType *copiedType = templateArgument_copy->get_type();
  if (originalType != NULL) {
    SgNode *mappedType = lookupCopiedNode(help, originalType);
    if (mappedType != NULL && mappedType != originalType) {
      SgType *exactMappedType = isSgType(mappedType);
      if (exactMappedType == NULL ||
          exactMappedType->variantT() != originalType->variantT() ||
          (copiedType != originalType && copiedType != exactMappedType)) {
        fprintf(
            stderr,
            "REX_COPY_INVARIANT[template-argument-type-map]: "
            "original=%p/%s copy=%p provisional=%p/%s mapped=%p/%s\n",
            static_cast<void *>(originalType),
            originalType->class_name().c_str(),
            static_cast<void *>(templateArgument_copy),
            static_cast<void *>(copiedType),
            copiedType != NULL ? copiedType->class_name().c_str() : "<null>",
            static_cast<void *>(mappedType), mappedType->class_name().c_str());
        ROSE_ABORT();
      }
      copiedType = exactMappedType;
    } else if (copiedType != originalType) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[template-argument-type-provisional]: "
              "original=%p/%s copy=%p provisional=%p/%s has no exact "
              "published type-graph mapping\n",
              static_cast<void *>(originalType),
              originalType->class_name().c_str(),
              static_cast<void *>(templateArgument_copy),
              static_cast<void *>(copiedType),
              copiedType != NULL ? copiedType->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }
    // Named/dependent types are materialized as one closed graph after all
    // structural scope fixups.  Until that explicit phase publishes a map,
    // the exact source type is the only legal provisional semantic edge.
  }
  if ((get_argumentType() == SgTemplateArgument::type_argument ||
       get_argumentType() == SgTemplateArgument::nontype_argument) !=
          (originalType != NULL) ||
      (originalType != NULL && copiedType == NULL)) {
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
  templateArgument_copy->set_type(copiedType);

  fixupExactCopiedSupportScopes(this, templateArgument_copy, help);

#if DEBUG_FIXUP_COPY
  printf("\nIn SgTemplateArgument::fixupCopy_scopes(): this = %p = %s copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif
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
    templateParameter_copy->set_type(copiedTemplateType);
  } else if (get_parameterType() == SgTemplateParameter::nontype_parameter) {
    if (mappedParameterType != NULL &&
        mappedParameterType != originalParameterType) {
      SgType *exactMappedType = isSgType(mappedParameterType);
      if (exactMappedType == NULL ||
          exactMappedType->variantT() != originalParameterType->variantT() ||
          (copiedParameterType != originalParameterType &&
           copiedParameterType != exactMappedType)) {
        fprintf(stderr,
                "REX_COPY_INVARIANT[template-nontype-parameter-type-map]: "
                "original=%p type=%p/%s copy=%p provisional=%p/%s "
                "mapped=%p/%s\n",
                static_cast<const void *>(this),
                static_cast<void *>(originalParameterType),
                originalParameterType->class_name().c_str(),
                static_cast<void *>(templateParameter_copy),
                static_cast<void *>(copiedParameterType),
                copiedParameterType->class_name().c_str(),
                static_cast<void *>(mappedParameterType),
                mappedParameterType->class_name().c_str());
        ROSE_ABORT();
      }
      copiedParameterType = exactMappedType;
    } else if (copiedParameterType != originalParameterType) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[template-nontype-parameter-type-"
              "provisional]: original=%p type=%p/%s copy=%p "
              "provisional=%p/%s has no exact published type mapping\n",
              static_cast<const void *>(this),
              static_cast<void *>(originalParameterType),
              originalParameterType->class_name().c_str(),
              static_cast<void *>(templateParameter_copy),
              static_cast<void *>(copiedParameterType),
              copiedParameterType->class_name().c_str());
      ROSE_ABORT();
    }
    templateParameter_copy->set_type(copiedParameterType);
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
    SgTemplateDeclaration *originalTemplate =
        isSgTemplateDeclaration(get_templateDeclaration());
    SgTemplateDeclaration *copiedTemplate =
        isSgTemplateDeclaration(remapExactCopiedOrExternalSemanticEdge(
            originalTemplate, templateParameter_copy->get_templateDeclaration(),
            help, "template-parameter-declaration"));
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
    templateParameter_copy->set_templateDeclaration(copiedTemplate);
    requireExactCopiedOrExternalSemanticEdge(originalTemplate, copiedTemplate,
                                             help,
                                             "template-parameter-declaration");

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

  SgDeclarationStatement *originalDefault =
      get_defaultTemplateDeclarationParameter();
  SgDeclarationStatement *copiedDefault =
      templateParameter_copy->get_defaultTemplateDeclarationParameter();
  if ((originalDefault == NULL) != (copiedDefault == NULL)) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[template-parameter-default-declaration]: "
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
                                 "template-parameter-default-declaration");
  }

  // A type default is a semantic edge, unlike the expression and template
  // defaults above, which are declaration-owned syntax children.  Named and
  // dependent type mappings are deliberately materialized only after every
  // copied declaration family has completed scope fixup.  Enforce the same
  // phase boundary used by SgTemplateArgument: consume an already-published
  // exact type mapping, or require the copy constructor to retain the exact
  // source edge as the sole legal provisional value.  The closed type-graph
  // pass then rewrites this field and rejects every stale source edge.
  SgType *originalDefaultType = get_defaultTypeParameter();
  SgType *copiedDefaultType =
      templateParameter_copy->get_defaultTypeParameter();
  if (originalDefaultType != NULL) {
    SgNode *mappedType = lookupCopiedNode(help, originalDefaultType);
    if (mappedType != NULL && mappedType != originalDefaultType) {
      SgType *exactMappedType = isSgType(mappedType);
      if (exactMappedType == NULL ||
          exactMappedType->variantT() != originalDefaultType->variantT() ||
          (copiedDefaultType != originalDefaultType &&
           copiedDefaultType != exactMappedType)) {
        fprintf(
            stderr,
            "REX_COPY_INVARIANT[template-parameter-default-type-map]: "
            "original=%p/%s copy=%p provisional=%p/%s mapped=%p/%s\n",
            static_cast<void *>(originalDefaultType),
            originalDefaultType->class_name().c_str(),
            static_cast<void *>(templateParameter_copy),
            static_cast<void *>(copiedDefaultType),
            copiedDefaultType != NULL ? copiedDefaultType->class_name().c_str()
                                      : "<null>",
            static_cast<void *>(mappedType), mappedType->class_name().c_str());
        ROSE_ABORT();
      }
      copiedDefaultType = exactMappedType;
    } else if (copiedDefaultType != originalDefaultType) {
      fprintf(stderr,
              "REX_COPY_INVARIANT[template-parameter-default-type-"
              "provisional]: original=%p/%s copy=%p provisional=%p/%s has "
              "no exact published type-graph mapping\n",
              static_cast<void *>(originalDefaultType),
              originalDefaultType->class_name().c_str(),
              static_cast<void *>(templateParameter_copy),
              static_cast<void *>(copiedDefaultType),
              copiedDefaultType != NULL
                  ? copiedDefaultType->class_name().c_str()
                  : "<null>");
      ROSE_ABORT();
    }
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
  templateParameter_copy->set_defaultTypeParameter(copiedDefaultType);

  fixupExactCopiedSupportScopes(this, templateParameter_copy, help);
}
