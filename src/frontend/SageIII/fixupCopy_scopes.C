// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "astPostProcessing/astPostProcessing.h"
#include "fixupCopy.h"

#include <algorithm>
#include <cstdlib>
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

bool traceCopyFixupEnabled() {
  static const bool enabled = std::getenv("ROSE_TRACE_COPY_FIXUP") != NULL;
  return enabled;
}

bool traceTrackedEmptyDeclarationEnabled() {
  static const bool enabled = std::getenv("ROSE_TRACE_COPY_EMPTY_DECL") != NULL;
  return enabled;
}

bool traceTrackedMemberFunctionEnabled() {
  static const bool enabled =
      std::getenv("ROSE_TRACE_COPY_MEMBER_DECL") != NULL;
  return enabled;
}

bool isTrackedEmptyDeclaration(const SgDeclarationStatement *decl) {
  if (traceTrackedEmptyDeclarationEnabled() == false ||
      isSgEmptyDeclaration(decl) == NULL) {
    return false;
  }

  Sg_File_Info *info = decl->get_file_info();
  if (info == NULL) {
    return false;
  }

  const std::string filename = info->get_filenameString();
  return filename.find("test2004_128.C") != std::string::npos &&
         info->get_line() == 31 && info->get_col() == 11;
}

bool isTrackedMemberFunctionDeclaration(const SgDeclarationStatement *decl) {
  if (traceTrackedMemberFunctionEnabled() == false) {
    return false;
  }

  const SgMemberFunctionDeclaration *memberDecl =
      isSgMemberFunctionDeclaration(decl);
  if (memberDecl == NULL ||
      memberDecl->get_name().getString() != "numberOfDimensions") {
    return false;
  }

  Sg_File_Info *info = memberDecl->get_file_info();
  return info != NULL && info->get_line() == 25 && info->get_col() == 12;
}

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

typedef std::unordered_map<SgNode *, SgNode *> CopiedNodeReplacementMap;

std::unordered_map<SgCopyHelp *, CopiedNodeReplacementMap> &
pendingCopiedNodeReplacementsByHelp() {
  static std::unordered_map<SgCopyHelp *, CopiedNodeReplacementMap>
      replacements;
  return replacements;
}

void notePendingCopiedNodeReplacement(SgCopyHelp &help, SgNode *staleCopy,
                                      SgNode *canonicalCopy) {
  if (staleCopy == NULL || canonicalCopy == NULL ||
      staleCopy == canonicalCopy) {
    return;
  }

  pendingCopiedNodeReplacementsByHelp()[&help][staleCopy] = canonicalCopy;
}

CopiedNodeReplacementMap
consumePendingCopiedNodeReplacements(SgCopyHelp &help) {
  std::unordered_map<SgCopyHelp *, CopiedNodeReplacementMap> &allReplacements =
      pendingCopiedNodeReplacementsByHelp();
  std::unordered_map<SgCopyHelp *, CopiedNodeReplacementMap>::iterator it =
      allReplacements.find(&help);
  if (it == allReplacements.end()) {
    return CopiedNodeReplacementMap();
  }

  CopiedNodeReplacementMap replacements = it->second;
  allReplacements.erase(it);

  for (CopiedNodeReplacementMap::iterator replacement = replacements.begin();
       replacement != replacements.end(); ++replacement) {
    std::unordered_set<SgNode *> seen;
    seen.reserve(4);

    SgNode *canonicalCopy = replacement->second;
    while (canonicalCopy != NULL && seen.insert(canonicalCopy).second) {
      CopiedNodeReplacementMap::const_iterator next =
          replacements.find(canonicalCopy);
      if (next == replacements.end() || next->second == canonicalCopy) {
        break;
      }

      canonicalCopy = next->second;
    }

    replacement->second = canonicalCopy;
  }

  return replacements;
}

void traceCopyFixupRoot(const char *phase, SgCopyHelp &help) {
  if (!traceCopyFixupEnabled()) {
    return;
  }

  std::vector<const SgNode *> roots;
  roots.reserve(4);

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgNode *original = it->first;
    if (original == NULL) {
      continue;
    }

    const SgNode *parent = original->get_parent();
    if (parent == NULL || help.get_copiedNodeMap().find(const_cast<SgNode *>(
                              parent)) == help.get_copiedNodeMap().end()) {
      roots.push_back(original);
      if (roots.size() == 4) {
        break;
      }
    }
  }

  fprintf(stderr, "[copy-root] %s copied-nodes=%zu", phase,
          help.get_copiedNodeMap().size());
  for (const SgNode *root : roots) {
    fprintf(stderr, " %s@%p", root->class_name().c_str(), root);
  }
  fputc('\n', stderr);
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

SgDeclarationStatement *
lookupCopiedDeclaration(SgCopyHelp &help,
                        const SgDeclarationStatement *original) {
  return isSgDeclarationStatement(lookupCopiedNode(help, original));
}

bool declarationMustDefineItself(const SgDeclarationStatement *decl);

void repairCopiedDeclarationChainLinks(
    const SgDeclarationStatement *originalDecl,
    SgDeclarationStatement *copiedDecl, SgCopyHelp &help) {
  if (originalDecl == NULL || copiedDecl == NULL) {
    return;
  }

  if (declarationMustDefineItself(originalDecl)) {
    copiedDecl->set_definingDeclaration(copiedDecl);
    if (originalDecl->get_firstNondefiningDeclaration() == NULL ||
        originalDecl->get_firstNondefiningDeclaration() == originalDecl ||
        copiedDecl->get_firstNondefiningDeclaration() == NULL) {
      copiedDecl->set_firstNondefiningDeclaration(copiedDecl);
    }
    return;
  }

  const SgDeclarationStatement *originalDefining =
      originalDecl->get_definingDeclaration();
  const SgDeclarationStatement *originalFirstNondefining =
      originalDecl->get_firstNondefiningDeclaration();

  SgDeclarationStatement *copiedDefining = NULL;
  if (originalDefining == originalDecl) {
    copiedDefining = copiedDecl;
  } else {
    copiedDefining = lookupCopiedDeclaration(help, originalDefining);
  }

  SgDeclarationStatement *copiedFirstNondefining = NULL;
  if (originalFirstNondefining == originalDecl) {
    copiedFirstNondefining = copiedDecl;
  } else {
    copiedFirstNondefining =
        lookupCopiedDeclaration(help, originalFirstNondefining);
  }

  if (originalDefining == NULL) {
    copiedDecl->set_definingDeclaration(NULL);
  } else if (copiedDefining != NULL &&
             copiedDecl->get_definingDeclaration() != copiedDefining) {
    copiedDecl->set_definingDeclaration(copiedDefining);
  }

  if (originalFirstNondefining == NULL) {
    copiedDecl->set_firstNondefiningDeclaration(NULL);
  } else if (copiedFirstNondefining != NULL &&
             copiedDecl->get_firstNondefiningDeclaration() !=
                 copiedFirstNondefining) {
    copiedDecl->set_firstNondefiningDeclaration(copiedFirstNondefining);
  }

  if (copiedFirstNondefining != NULL && originalFirstNondefining != NULL &&
      originalFirstNondefining->get_firstNondefiningDeclaration() ==
          originalFirstNondefining &&
      copiedFirstNondefining->get_firstNondefiningDeclaration() !=
          copiedFirstNondefining) {
    copiedFirstNondefining->set_firstNondefiningDeclaration(
        copiedFirstNondefining);
  }

  if (copiedDefining != NULL && originalDefining != NULL &&
      originalDefining->get_definingDeclaration() == originalDefining &&
      copiedDefining->get_definingDeclaration() != copiedDefining) {
    copiedDefining->set_definingDeclaration(copiedDefining);
  }

  if (copiedDefining != NULL && copiedFirstNondefining != NULL &&
      originalDefining != NULL &&
      originalDefining->get_firstNondefiningDeclaration() ==
          originalFirstNondefining &&
      copiedDefining->get_firstNondefiningDeclaration() !=
          copiedFirstNondefining) {
    copiedDefining->set_firstNondefiningDeclaration(copiedFirstNondefining);
  }

  if (copiedDefining != NULL && copiedFirstNondefining != NULL &&
      originalFirstNondefining != NULL &&
      originalFirstNondefining->get_definingDeclaration() == originalDefining &&
      copiedFirstNondefining->get_definingDeclaration() != copiedDefining) {
    copiedFirstNondefining->set_definingDeclaration(copiedDefining);
  }
}

void remapCopiedNodePair(SgCopyHelp &help, const SgNode *original,
                         SgNode *canonicalCopy) {
  if (original == NULL || canonicalCopy == NULL) {
    return;
  }

  SgCopyHelp::copiedNodeMapType &copiedNodeMap = help.get_copiedNodeMap();
  SgCopyHelp::copiedNodeMapTypeIterator existing =
      copiedNodeMap.find(const_cast<SgNode *>(original));
  if (existing != copiedNodeMap.end() && existing->second != canonicalCopy) {
    if (!isSharedOriginalMappedNode(help, existing->second)) {
      notePendingCopiedNodeReplacement(help, existing->second, canonicalCopy);
      help.noteSupersededCopiedNode(existing->second);
    }
  }

  copiedNodeMap[const_cast<SgNode *>(original)] = canonicalCopy;
}

bool copiedNodeLooksDeleted(const SgNode *node) {
  return node == NULL || node->class_name() == "SgNode";
}

SgNode *
chooseCanonicalCopiedAliasChainValue(const std::vector<SgNode *> &chainValues) {
  for (SgNode *candidate : chainValues) {
    if (!copiedNodeLooksDeleted(candidate) && candidate->get_parent() != NULL) {
      return candidate;
    }
  }

  for (SgNode *candidate : chainValues) {
    if (!copiedNodeLooksDeleted(candidate)) {
      return candidate;
    }
  }

  return chainValues.empty() ? NULL : chainValues.front();
}

void removeCopiedNodeMapAliasEntries(SgCopyHelp &help) {
  std::unordered_set<const SgNode *> copiedValues;
  copiedValues.reserve(help.get_copiedNodeMap().size());
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (it->second != NULL) {
      copiedValues.insert(it->second);
    }
  }

  std::vector<const SgNode *> rootKeys;
  rootKeys.reserve(help.get_copiedNodeMap().size());
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (copiedValues.find(it->first) == copiedValues.end()) {
      rootKeys.push_back(it->first);
    }
  }

  std::unordered_set<const SgNode *> aliasKeys;
  aliasKeys.reserve(help.get_copiedNodeMap().size());
  for (const SgNode *rootKey : rootKeys) {
    SgCopyHelp::copiedNodeMapTypeIterator root =
        help.get_copiedNodeMap().find(rootKey);
    if (root == help.get_copiedNodeMap().end() || root->second == NULL) {
      continue;
    }

    std::vector<SgNode *> chainValues;
    chainValues.reserve(4);
    std::unordered_set<const SgNode *> seenKeys;
    seenKeys.reserve(4);

    SgNode *currentValue = root->second;
    chainValues.push_back(currentValue);

    while (currentValue != NULL && seenKeys.insert(currentValue).second) {
      SgCopyHelp::copiedNodeMapTypeIterator alias =
          help.get_copiedNodeMap().find(currentValue);
      if (alias == help.get_copiedNodeMap().end()) {
        break;
      }

      aliasKeys.insert(currentValue);
      currentValue = alias->second;
      if (currentValue != NULL) {
        chainValues.push_back(currentValue);
      }
    }

    if (isTrackedMemberFunctionDeclaration(isSgDeclarationStatement(rootKey))) {
      fprintf(stderr, "[copy-member] alias-chain root=%p", rootKey);
      for (SgNode *chainValue : chainValues) {
        fprintf(stderr, " -> %p(%s parent=%p)", chainValue,
                chainValue != NULL ? chainValue->class_name().c_str()
                                   : "<null>",
                chainValue != NULL ? chainValue->get_parent() : NULL);
      }
      fprintf(stderr, "\n");
      fflush(stderr);
    }

    SgNode *canonicalValue = chooseCanonicalCopiedAliasChainValue(chainValues);
    if (canonicalValue != NULL && root->second != canonicalValue) {
      remapCopiedNodePair(help, rootKey, canonicalValue);
    }

    for (SgNode *chainValue : chainValues) {
      if (chainValue != NULL && chainValue != canonicalValue &&
          !isSharedOriginalMappedNode(help, chainValue)) {
        notePendingCopiedNodeReplacement(help, chainValue, canonicalValue);
        help.noteSupersededCopiedNode(chainValue);
      }
    }
  }

  for (const SgNode *aliasKey : aliasKeys) {
    help.get_copiedNodeMap().erase(aliasKey);
  }
}

void removeDeletedCopiedNodeMapEntries(SgCopyHelp &help) {
  std::vector<const SgNode *> deadKeys;
  deadKeys.reserve(help.get_copiedNodeMap().size());

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (!copiedNodeLooksDeleted(it->second)) {
      continue;
    }

    if (traceCopyFixupEnabled()) {
      fprintf(stderr,
              "[copy-root] prune-dead-map-entry original=%p(%s) copy=%p\n",
              it->first,
              it->first != NULL ? it->first->class_name().c_str() : "<null>",
              it->second);
      fflush(stderr);
    }

    help.noteSupersededCopiedNode(it->second);
    deadKeys.push_back(it->first);
  }

  for (const SgNode *deadKey : deadKeys) {
    help.get_copiedNodeMap().erase(deadKey);
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

bool declarationIsStructurallyAttachedToScope(SgDeclarationStatement *decl,
                                              SgScopeStatement *scope);
SgScopeStatement *
lookupScopeForAttachedDeclaration(const SgDeclarationStatement *decl);
SgDeclarationStatement *
resolveCopiedReferencedDeclaration(const SgDeclarationStatement *originalDecl,
                                   SgCopyHelp &help);
bool namespaceDeclarationsMatch(
    const SgNamespaceDeclarationStatement *originalDecl,
    SgNamespaceDeclarationStatement *candidateDecl);
bool declarationMustDefineItself(const SgDeclarationStatement *decl);
SgNode *copyNodeIntoCurrentRoot(SgCopyHelp &help, const SgNode *originalNode);
bool declarationsMatchForCanonicalCopy(
    const SgDeclarationStatement *originalDecl,
    const SgDeclarationStatement *candidateDecl);

void synchronizeCopiedDeclarationList(
    const SgDeclarationStatementPtrList &originalDeclarations,
    SgDeclarationStatementPtrList &copiedDeclarations,
    SgScopeStatement *copiedScope, SgCopyHelp &help) {
  if (copiedScope == NULL) {
    return;
  }

  std::unordered_set<SgDeclarationStatement *> canonicalDeclarations;
  canonicalDeclarations.reserve(originalDeclarations.size());

  SgDeclarationStatementPtrList rebuiltDeclarations;
  rebuiltDeclarations.reserve(originalDeclarations.size());

  for (size_t index = 0; index < originalDeclarations.size(); ++index) {
    SgDeclarationStatement *originalDecl = originalDeclarations[index];
    if (originalDecl == NULL) {
      rebuiltDeclarations.push_back(NULL);
      continue;
    }

    SgDeclarationStatement *copiedDecl = NULL;
    if (index < copiedDeclarations.size()) {
      SgDeclarationStatement *existingDecl = copiedDeclarations[index];
      if (copiedNodeLooksDeleted(existingDecl)) {
        help.noteSupersededCopiedNode(existingDecl);
      } else if (declarationIsStructurallyAttachedToScope(existingDecl,
                                                          copiedScope) &&
                 declarationsMatchForCanonicalCopy(originalDecl,
                                                   existingDecl)) {
        copiedDecl = existingDecl;
        remapCopiedNodePair(help, originalDecl, copiedDecl);
      }
    }

    if (copiedDecl == NULL) {
      copiedDecl = isSgDeclarationStatement(help.copyOrLookupAst(originalDecl));
      if (copiedDecl == NULL) {
        copiedDecl = resolveCopiedReferencedDeclaration(originalDecl, help);
      }
    }
    if (copiedDecl == NULL) {
      fprintf(
          stderr,
          "[copy-sync] null copy original=%p class=%s name=%s file=%s:%d:%d "
          "parent=%p scope=%p copiedScope=%p index=%zu\n",
          originalDecl, originalDecl->class_name().c_str(),
          SageInterface::get_name(originalDecl).c_str(),
          originalDecl->get_file_info() != NULL
              ? originalDecl->get_file_info()->get_filenameString().c_str()
              : "<null>",
          originalDecl->get_file_info() != NULL
              ? originalDecl->get_file_info()->get_line()
              : -1,
          originalDecl->get_file_info() != NULL
              ? originalDecl->get_file_info()->get_col()
              : -1,
          originalDecl->get_parent(), originalDecl->get_scope(), copiedScope,
          index);
      fflush(stderr);
    }
    ROSE_ASSERT(copiedDecl != NULL);
    ROSE_ASSERT(!copiedNodeLooksDeleted(copiedDecl));

    if (isTrackedMemberFunctionDeclaration(originalDecl)) {
      fprintf(stderr,
              "[copy-member] sync original=%p copy=%p copyClass=%s "
              "copiedScope=%p copyParent=%p copyScope=%p index=%zu\n",
              originalDecl, copiedDecl, copiedDecl->class_name().c_str(),
              copiedScope, copiedDecl->get_parent(), copiedDecl->get_scope(),
              index);
      fflush(stderr);
    }

    if (isTrackedEmptyDeclaration(originalDecl)) {
      fprintf(stderr,
              "[copy-empty] sync original=%p copy=%p copiedScope=%p "
              "copyParent=%p copyScope=%p index=%zu\n",
              originalDecl, copiedDecl, copiedScope, copiedDecl->get_parent(),
              copiedDecl->get_scope(), index);
      fflush(stderr);
    }

    const bool firstCanonicalOccurrence =
        canonicalDeclarations.insert(copiedDecl).second;
    if (!firstCanonicalOccurrence) {
      continue;
    }

    rebuiltDeclarations.push_back(copiedDecl);

    if (copiedDecl->get_parent() == NULL) {
      copiedDecl->set_parent(copiedScope);
      if (isTrackedEmptyDeclaration(originalDecl)) {
        fprintf(stderr,
                "[copy-empty] sync attached original=%p copy=%p newParent=%p\n",
                originalDecl, copiedDecl, copiedDecl->get_parent());
        fflush(stderr);
      }
    }
  }

  copiedDeclarations.swap(rebuiltDeclarations);
}

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

bool declarationFixupAvailable(const SgDeclarationStatement *originalOwner,
                               const SgDeclarationStatement *originalTarget,
                               SgCopyHelp &help) {
  if (originalTarget == NULL) {
    return true;
  }

  // Self-links are resolved by the current declaration fixup and do not
  // require a recursive visit to a distinct declaration node.
  if (originalTarget == originalOwner) {
    return true;
  }

  SgDeclarationStatement *mappedTarget =
      isSgDeclarationStatement(lookupCopiedNode(help, originalTarget));
  if (mappedTarget == NULL) {
    return false;
  }

  return help.isFixupNodePairActive(originalTarget, mappedTarget) ||
         help.isFixupNodePairCompleted(originalTarget, mappedTarget);
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

bool hasMatchingSourceLocation(const SgLocatedNode *original,
                               const SgLocatedNode *candidate) {
  if (original == NULL || candidate == NULL) {
    return false;
  }

  Sg_File_Info *originalInfo = original->get_file_info();
  Sg_File_Info *candidateInfo = candidate->get_file_info();
  if (originalInfo == NULL || candidateInfo == NULL) {
    return false;
  }

  if (originalInfo->isCompilerGenerated() ||
      candidateInfo->isCompilerGenerated() ||
      originalInfo->isSourcePositionUnavailableInFrontend() ||
      candidateInfo->isSourcePositionUnavailableInFrontend()) {
    return false;
  }

  return originalInfo->get_filenameString() ==
             candidateInfo->get_filenameString() &&
         originalInfo->get_line() == candidateInfo->get_line() &&
         originalInfo->get_col() == candidateInfo->get_col();
}

bool nodeHasAttachedParentChain(const SgNode *node) {
  for (const SgNode *cursor = node; cursor != NULL;
       cursor = cursor->get_parent()) {
    if (isSgProject(const_cast<SgNode *>(cursor)) != NULL ||
        isSgFile(const_cast<SgNode *>(cursor)) != NULL) {
      return true;
    }
  }

  return false;
}

bool functionDeclarationsMatch(const SgFunctionDeclaration *originalDecl,
                               SgFunctionDeclaration *candidateDecl) {
  if (originalDecl == NULL || candidateDecl == NULL) {
    return false;
  }

  if (originalDecl->variantT() != candidateDecl->variantT()) {
    return false;
  }

  if (originalDecl->get_name() != candidateDecl->get_name()) {
    return false;
  }

  if (originalDecl->get_parameterList() != NULL &&
      candidateDecl->get_parameterList() != NULL &&
      originalDecl->get_parameterList()->get_args().size() !=
          candidateDecl->get_parameterList()->get_args().size()) {
    return false;
  }

  if (hasMatchingSourceLocation(originalDecl, candidateDecl)) {
    return true;
  }

  // Detached copied declarations can reach here before their enclosing
  // namespace/class scopes have been reattached. Mangled-name generation walks
  // the scope chain and will assert on those transient orphaned parents. At
  // this point we are already traversing direct defining/nondefining links, so
  // fall back to direct name/type identity until attachment is complete.
  if (!nodeHasAttachedParentChain(originalDecl) ||
      !nodeHasAttachedParentChain(candidateDecl)) {
    return originalDecl->get_name() == candidateDecl->get_name() &&
           originalDecl->get_type() == candidateDecl->get_type();
  }

  return originalDecl->get_mangled_name() == candidateDecl->get_mangled_name();
}

bool functionDeclarationsHaveSameDefinitionPresence(
    const SgFunctionDeclaration *originalDecl,
    const SgFunctionDeclaration *candidateDecl) {
  if (originalDecl == NULL || candidateDecl == NULL) {
    return false;
  }

  return (originalDecl->get_definition() != NULL) ==
         (candidateDecl->get_definition() != NULL);
}

int declarationChainLinkRole(const SgDeclarationStatement *owner,
                             const SgDeclarationStatement *link) {
  if (link == NULL) {
    return 0;
  }

  return link == owner ? 2 : 1;
}

bool functionDeclarationsHaveMatchingChainRole(
    const SgFunctionDeclaration *originalDecl,
    const SgFunctionDeclaration *candidateDecl) {
  if (originalDecl == NULL || candidateDecl == NULL) {
    return false;
  }

  return declarationChainLinkRole(originalDecl,
                                  originalDecl->get_definingDeclaration()) ==
             declarationChainLinkRole(
                 candidateDecl, candidateDecl->get_definingDeclaration()) &&
         declarationChainLinkRole(
             originalDecl, originalDecl->get_firstNondefiningDeclaration()) ==
             declarationChainLinkRole(
                 candidateDecl,
                 candidateDecl->get_firstNondefiningDeclaration());
}

bool locatedNodeHasUsableSourceLocation(const SgLocatedNode *node) {
  if (node == NULL) {
    return false;
  }

  Sg_File_Info *info = node->get_file_info();
  return info != NULL && !info->isCompilerGenerated() &&
         !info->isSourcePositionUnavailableInFrontend();
}

bool functionDeclarationsMatchForCanonicalCopy(
    const SgFunctionDeclaration *originalDecl,
    SgFunctionDeclaration *candidateDecl) {
  if (!functionDeclarationsMatch(originalDecl, candidateDecl) ||
      !functionDeclarationsHaveSameDefinitionPresence(originalDecl,
                                                      candidateDecl) ||
      !functionDeclarationsHaveMatchingChainRole(originalDecl, candidateDecl)) {
    return false;
  }

  if (hasMatchingSourceLocation(originalDecl, candidateDecl)) {
    return true;
  }

  if (locatedNodeHasUsableSourceLocation(originalDecl) ||
      locatedNodeHasUsableSourceLocation(candidateDecl)) {
    return false;
  }

  return true;
}

bool namespaceDeclarationsMatch(
    const SgNamespaceDeclarationStatement *originalDecl,
    SgNamespaceDeclarationStatement *candidateDecl) {
  if (originalDecl == NULL || candidateDecl == NULL) {
    return false;
  }

  if (originalDecl->variantT() != candidateDecl->variantT()) {
    return false;
  }

  if (originalDecl->get_name() != candidateDecl->get_name()) {
    return false;
  }

  if (hasMatchingSourceLocation(originalDecl, candidateDecl)) {
    return true;
  }

  return hasMatchingSourceLocation(originalDecl->get_definition(),
                                   candidateDecl->get_definition());
}

bool declarationIsStructurallyAttachedToScope(SgDeclarationStatement *decl,
                                              SgScopeStatement *scope) {
  if (decl == NULL || scope == NULL || decl->get_parent() != scope) {
    return false;
  }

  return scopeDirectlyContainsStatement(scope, decl);
}

SgScopeStatement *
lookupScopeForAttachedDeclaration(const SgDeclarationStatement *decl) {
  if (decl == NULL || decl->get_parent() == NULL) {
    return NULL;
  }

  SgDeclarationStatement *nonConstDecl =
      const_cast<SgDeclarationStatement *>(decl);
  SgScopeStatement *parentScope = isSgScopeStatement(decl->get_parent());
  if (declarationIsStructurallyAttachedToScope(nonConstDecl, parentScope)) {
    return parentScope;
  }

  SgScopeStatement *explicitScope = decl->get_scope();
  if (declarationIsStructurallyAttachedToScope(nonConstDecl, explicitScope)) {
    return explicitScope;
  }

  return NULL;
}

bool declarationsMatchForCanonicalCopy(
    const SgDeclarationStatement *originalDecl,
    const SgDeclarationStatement *candidateDecl) {
  if (originalDecl == NULL || candidateDecl == NULL ||
      originalDecl->variantT() != candidateDecl->variantT()) {
    return false;
  }

  if (const SgFunctionDeclaration *originalFunction =
          isSgFunctionDeclaration(originalDecl)) {
    return functionDeclarationsMatchForCanonicalCopy(
        originalFunction, const_cast<SgFunctionDeclaration *>(
                              isSgFunctionDeclaration(candidateDecl)));
  }

  const SgName originalName = SageInterface::get_name(originalDecl);
  const SgName candidateName = SageInterface::get_name(candidateDecl);
  if (originalName.is_null() == false || candidateName.is_null() == false) {
    if (originalName != candidateName) {
      return false;
    }
  }

  return hasMatchingSourceLocation(originalDecl, candidateDecl);
}

bool enumFieldsMatchForCanonicalCopy(const SgInitializedName *originalField,
                                     const SgInitializedName *candidateField) {
  if (originalField == NULL || candidateField == NULL) {
    return originalField == candidateField;
  }

  return originalField->get_name() == candidateField->get_name();
}

const SgDeclarationStatementPtrList *
scopeOwnedDeclarationsForCanonicalLookup(SgScopeStatement *scope) {
  if (scope == NULL) {
    return NULL;
  }

  if (SgGlobal *globalScope = isSgGlobal(scope)) {
    return &globalScope->get_declarations();
  }

  if (SgNamespaceDefinitionStatement *namespaceScope =
          isSgNamespaceDefinitionStatement(scope)) {
    return &namespaceScope->get_declarations();
  }

  if (SgClassDefinition *classScope = isSgClassDefinition(scope)) {
    return &classScope->get_members();
  }

  if (SgDeclarationScope *declarationScope = isSgDeclarationScope(scope)) {
    return &declarationScope->get_declarations();
  }

  return NULL;
}

SgFunctionDeclaration *
findAttachedCopiedFunctionDeclaration(const SgFunctionDeclaration *originalDecl,
                                      SgCopyHelp &help) {
  if (originalDecl == NULL) {
    return NULL;
  }

  SgFunctionDeclaration *nonConstOriginal =
      const_cast<SgFunctionDeclaration *>(originalDecl);
  SgScopeStatement *originalScope =
      lookupScopeForAttachedDeclaration(nonConstOriginal);
  if (!declarationIsStructurallyAttachedToScope(nonConstOriginal,
                                                originalScope)) {
    return NULL;
  }

  SgScopeStatement *copiedScope =
      isSgScopeStatement(lookupCopiedNode(help, originalScope));
  if (copiedScope == NULL) {
    return NULL;
  }

  SgFunctionDeclaration *bestCandidate = NULL;
  const SgDeclarationStatementPtrList *copiedDecls =
      scopeOwnedDeclarationsForCanonicalLookup(copiedScope);
  if (copiedDecls == NULL) {
    return NULL;
  }

  for (SgDeclarationStatement *copiedDecl : *copiedDecls) {
    SgFunctionDeclaration *copiedFunction = isSgFunctionDeclaration(copiedDecl);
    if (copiedFunction == NULL ||
        !declarationIsStructurallyAttachedToScope(copiedFunction,
                                                  copiedScope) ||
        !functionDeclarationsMatchForCanonicalCopy(originalDecl,
                                                   copiedFunction)) {
      continue;
    }

    if (originalDecl->get_definition() != NULL &&
        copiedFunction->get_definition() != NULL &&
        copiedFunction->get_definition()->get_parent() == copiedFunction) {
      return copiedFunction;
    }

    if (bestCandidate == NULL) {
      bestCandidate = copiedFunction;
    }
  }

  return bestCandidate;
}

SgNamespaceDeclarationStatement *findAttachedCopiedNamespaceDeclaration(
    const SgNamespaceDeclarationStatement *originalDecl, SgCopyHelp &help) {
  if (originalDecl == NULL) {
    return NULL;
  }

  SgNamespaceDeclarationStatement *nonConstOriginal =
      const_cast<SgNamespaceDeclarationStatement *>(originalDecl);
  SgScopeStatement *originalScope =
      lookupScopeForAttachedDeclaration(nonConstOriginal);
  if (!declarationIsStructurallyAttachedToScope(nonConstOriginal,
                                                originalScope)) {
    return NULL;
  }

  SgScopeStatement *copiedScope =
      isSgScopeStatement(lookupCopiedNode(help, originalScope));
  if (copiedScope == NULL) {
    return NULL;
  }

  const SgDeclarationStatementPtrList *copiedDecls =
      scopeOwnedDeclarationsForCanonicalLookup(copiedScope);
  if (copiedDecls == NULL) {
    return NULL;
  }

  SgNamespaceDeclarationStatement *bestCandidate = NULL;
  for (SgDeclarationStatement *copiedDecl : *copiedDecls) {
    SgNamespaceDeclarationStatement *copiedNamespaceDecl =
        isSgNamespaceDeclarationStatement(copiedDecl);
    if (copiedNamespaceDecl == NULL ||
        !declarationIsStructurallyAttachedToScope(copiedNamespaceDecl,
                                                  copiedScope) ||
        !namespaceDeclarationsMatch(originalDecl, copiedNamespaceDecl)) {
      continue;
    }

    if (SgNamespaceDefinitionStatement *copiedDefinition =
            copiedNamespaceDecl->get_definition()) {
      if (copiedDefinition->get_namespaceDeclaration() == copiedNamespaceDecl) {
        return copiedNamespaceDecl;
      }
    }

    if (bestCandidate == NULL) {
      bestCandidate = copiedNamespaceDecl;
    }
  }

  return bestCandidate;
}

void repairCopiedNamespaceScopeDeclarationChains(
    const SgDeclarationStatement *originalDecl,
    SgDeclarationStatement *copiedDecl, SgCopyHelp &help) {
  if (originalDecl == NULL || copiedDecl == NULL) {
    return;
  }

  if (declarationMustDefineItself(originalDecl) ||
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
  if (originalDefining == NULL || originalFirstNondefining == NULL ||
      copiedDefining == NULL || copiedFirstNondefining == NULL) {
    return;
  }

  if (originalDefining->get_parent() == NULL ||
      originalFirstNondefining->get_parent() == NULL ||
      copiedDefining->get_parent() == NULL ||
      copiedFirstNondefining->get_parent() == NULL) {
    return;
  }

  SgNamespaceDefinitionStatement *originalDefiningNamespace =
      isSgNamespaceDefinitionStatement(originalDefining->get_scope());
  SgNamespaceDefinitionStatement *originalFirstNondefiningNamespace =
      isSgNamespaceDefinitionStatement(originalFirstNondefining->get_scope());
  SgNamespaceDefinitionStatement *copiedDefiningNamespace =
      isSgNamespaceDefinitionStatement(copiedDefining->get_scope());
  SgNamespaceDefinitionStatement *copiedFirstNondefiningNamespace =
      isSgNamespaceDefinitionStatement(copiedFirstNondefining->get_scope());
  if (originalDefiningNamespace == NULL ||
      originalFirstNondefiningNamespace == NULL ||
      copiedDefiningNamespace == NULL ||
      copiedFirstNondefiningNamespace == NULL) {
    return;
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
      copiedFirstNondefiningNamespaceDecl == NULL) {
    return;
  }

  const SgNamespaceDeclarationStatement *originalCanonicalFirstNamespaceDecl =
      isSgNamespaceDeclarationStatement(
          originalDefiningNamespaceDecl->get_firstNondefiningDeclaration());
  if (originalCanonicalFirstNamespaceDecl == NULL) {
    originalCanonicalFirstNamespaceDecl = isSgNamespaceDeclarationStatement(
        originalFirstNondefiningNamespaceDecl
            ->get_firstNondefiningDeclaration());
  }
  if (originalCanonicalFirstNamespaceDecl == NULL) {
    return;
  }

  SgNamespaceDeclarationStatement *copiedCanonicalFirstNamespaceDecl =
      findAttachedCopiedNamespaceDeclaration(
          originalCanonicalFirstNamespaceDecl, help);
  if (copiedCanonicalFirstNamespaceDecl == NULL) {
    copiedCanonicalFirstNamespaceDecl = isSgNamespaceDeclarationStatement(
        lookupCopiedNode(help, originalCanonicalFirstNamespaceDecl));
  }
  if (copiedCanonicalFirstNamespaceDecl != NULL) {
    SgScopeStatement *copiedCanonicalFirstNamespaceScope =
        isSgScopeStatement(copiedCanonicalFirstNamespaceDecl->get_parent());
    if (!declarationIsStructurallyAttachedToScope(
            copiedCanonicalFirstNamespaceDecl,
            copiedCanonicalFirstNamespaceScope)) {
      copiedCanonicalFirstNamespaceDecl = NULL;
    }
  }
  if (copiedCanonicalFirstNamespaceDecl == NULL) {
    if (namespaceDeclarationsMatch(originalCanonicalFirstNamespaceDecl,
                                   copiedDefiningNamespaceDecl)) {
      copiedCanonicalFirstNamespaceDecl = copiedDefiningNamespaceDecl;
    } else if (namespaceDeclarationsMatch(
                   originalCanonicalFirstNamespaceDecl,
                   copiedFirstNondefiningNamespaceDecl)) {
      copiedCanonicalFirstNamespaceDecl = copiedFirstNondefiningNamespaceDecl;
    }
  }
  auto chooseAttachedCanonicalNamespaceDecl =
      [](SgNamespaceDeclarationStatement *candidate)
      -> SgNamespaceDeclarationStatement * {
    if (candidate == NULL) {
      return NULL;
    }

    if (SgNamespaceDeclarationStatement *firstNondefiningCandidate =
            isSgNamespaceDeclarationStatement(
                candidate->get_firstNondefiningDeclaration())) {
      if (declarationIsStructurallyAttachedToScope(
              firstNondefiningCandidate,
              isSgScopeStatement(firstNondefiningCandidate->get_parent()))) {
        return firstNondefiningCandidate;
      }
    }

    if (declarationIsStructurallyAttachedToScope(
            candidate, isSgScopeStatement(candidate->get_parent()))) {
      return candidate;
    }

    return NULL;
  };
  if (copiedCanonicalFirstNamespaceDecl == NULL) {
    copiedCanonicalFirstNamespaceDecl = chooseAttachedCanonicalNamespaceDecl(
        copiedFirstNondefiningNamespaceDecl);
  }
  if (copiedCanonicalFirstNamespaceDecl == NULL) {
    copiedCanonicalFirstNamespaceDecl =
        chooseAttachedCanonicalNamespaceDecl(copiedDefiningNamespaceDecl);
  }
  if (copiedCanonicalFirstNamespaceDecl == NULL) {
    return;
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
    if (copiedDecl != originalDecl && !copiedNodeLooksDeleted(copiedDecl)) {
      return copiedDecl;
    }
  }

  SgDeclarationStatement *nonConstOriginal =
      const_cast<SgDeclarationStatement *>(originalDecl);
  SgScopeStatement *originalScope =
      lookupScopeForAttachedDeclaration(nonConstOriginal);
  if (originalScope == NULL) {
    return NULL;
  }

  SgScopeStatement *copiedScope =
      isSgScopeStatement(lookupCopiedNode(help, originalScope));
  if (copiedScope == NULL) {
    return NULL;
  }

  const SgDeclarationStatementPtrList *copiedDecls =
      scopeOwnedDeclarationsForCanonicalLookup(copiedScope);
  if (copiedDecls == NULL) {
    return NULL;
  }

  for (SgDeclarationStatement *candidate : *copiedDecls) {
    if (candidate == NULL ||
        !declarationIsStructurallyAttachedToScope(candidate, copiedScope) ||
        !declarationsMatchForCanonicalCopy(originalDecl, candidate)) {
      continue;
    }

    return candidate;
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

void remapCopiedFunctionParameterSubtree(
    const SgFunctionDeclaration *originalDecl,
    SgFunctionDeclaration *copiedDecl, SgCopyHelp &help) {
  if (originalDecl == NULL || copiedDecl == NULL) {
    return;
  }

  SgFunctionParameterList *originalParams = originalDecl->get_parameterList();
  SgFunctionParameterList *copiedParams = copiedDecl->get_parameterList();
  if (originalParams == NULL || copiedParams == NULL) {
    return;
  }

  remapCopiedNodePair(help, originalParams, copiedParams);

  const SgInitializedNamePtrList &originalArgs = originalParams->get_args();
  const SgInitializedNamePtrList &copiedArgs = copiedParams->get_args();
  const size_t argCount = std::min(originalArgs.size(), copiedArgs.size());
  for (size_t i = 0; i < argCount; ++i) {
    if (originalArgs[i] != NULL && copiedArgs[i] != NULL) {
      remapCopiedNodePair(help, originalArgs[i], copiedArgs[i]);
    }
  }
}

SgNode *copyNodeIntoCurrentRoot(SgCopyHelp &help, const SgNode *originalNode) {
  if (originalNode == NULL) {
    return NULL;
  }

  if (SgNode *existingCopy = lookupCopiedNode(help, originalNode)) {
    return existingCopy;
  }

  // Extend the active root-copy map without recursively finalizing each newly
  // synthesized child as its own standalone copy.
  help.incrementDepth();
  SgNode *copiedNode = help.copyOrLookupAst(originalNode);
  help.decrementDepth();
  return copiedNode;
}

void populateCopiedMemberFunctionCtorInitializerSubtree(
    const SgMemberFunctionDeclaration *originalDecl,
    SgMemberFunctionDeclaration *copiedDecl, SgCopyHelp &help) {
  if (originalDecl == NULL || copiedDecl == NULL) {
    return;
  }

  const SgCtorInitializerList *originalCtorList =
      originalDecl->get_CtorInitializerList();
  SgCtorInitializerList *copiedCtorList = copiedDecl->get_CtorInitializerList();
  if (originalCtorList == NULL || copiedCtorList == NULL) {
    return;
  }

  if (copiedCtorList->get_parent() != copiedDecl) {
    copiedCtorList->set_parent(copiedDecl);
  }

  const SgInitializedNamePtrList &originalCtors = originalCtorList->get_ctors();
  SgInitializedNamePtrList &copiedCtors = copiedCtorList->get_ctors();

  if (copiedCtors.size() != originalCtors.size()) {
    for (SgInitializedName *existingCtor : copiedCtors) {
      if (existingCtor != NULL &&
          existingCtor->get_parent() == copiedCtorList) {
        existingCtor->set_parent(NULL);
      }
    }

    copiedCtors.clear();

    for (SgInitializedName *originalCtor : originalCtors) {
      if (originalCtor == NULL) {
        copiedCtors.push_back(NULL);
        continue;
      }

      SgInitializedName *copiedCtor =
          isSgInitializedName(copyNodeIntoCurrentRoot(help, originalCtor));
      ROSE_ASSERT(copiedCtor != NULL);
      copiedCtor->set_parent(copiedCtorList);
      copiedCtors.push_back(copiedCtor);
    }
  }

  remapCopiedNodePair(help, originalCtorList, copiedCtorList);

  const size_t ctorCount = std::min(originalCtors.size(), copiedCtors.size());
  for (size_t i = 0; i < ctorCount; ++i) {
    if (originalCtors[i] != NULL && copiedCtors[i] != NULL) {
      if (copiedCtors[i]->get_parent() != copiedCtorList) {
        copiedCtors[i]->set_parent(copiedCtorList);
      }
      remapCopiedNodePair(help, originalCtors[i], copiedCtors[i]);
    }
  }
}

void remapCopiedMemberFunctionCtorInitializerSubtree(
    const SgMemberFunctionDeclaration *originalDecl,
    SgMemberFunctionDeclaration *copiedDecl, SgCopyHelp &help) {
  if (originalDecl == NULL || copiedDecl == NULL) {
    return;
  }

  const SgCtorInitializerList *originalCtorList =
      originalDecl->get_CtorInitializerList();
  SgCtorInitializerList *copiedCtorList = copiedDecl->get_CtorInitializerList();
  if (originalCtorList == NULL || copiedCtorList == NULL) {
    return;
  }

  if (copiedCtorList->get_parent() != copiedDecl) {
    copiedCtorList->set_parent(copiedDecl);
  }

  remapCopiedNodePair(help, originalCtorList, copiedCtorList);

  const SgInitializedNamePtrList &originalCtors = originalCtorList->get_ctors();
  SgInitializedNamePtrList &copiedCtors = copiedCtorList->get_ctors();
  const size_t ctorCount = std::min(originalCtors.size(), copiedCtors.size());
  for (size_t i = 0; i < ctorCount; ++i) {
    if (originalCtors[i] != NULL && copiedCtors[i] != NULL) {
      if (copiedCtors[i]->get_parent() != copiedCtorList) {
        copiedCtors[i]->set_parent(copiedCtorList);
      }
      remapCopiedNodePair(help, originalCtors[i], copiedCtors[i]);
    }
  }
}

void canonicalizeCopiedFunctionDeclarationMapEntries(SgCopyHelp &help) {
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

  for (const SgFunctionDeclaration *originalDecl : originalFunctionDecls) {
    SgFunctionDeclaration *attachedCopiedDecl =
        findAttachedCopiedFunctionDeclaration(originalDecl, help);
    if (attachedCopiedDecl == NULL) {
      continue;
    }

    remapCopiedNodePair(help, originalDecl, attachedCopiedDecl);
    remapCopiedFunctionParameterSubtree(originalDecl, attachedCopiedDecl, help);
    if (const SgMemberFunctionDeclaration *originalMemberDecl =
            isSgMemberFunctionDeclaration(originalDecl)) {
      if (SgMemberFunctionDeclaration *attachedCopiedMemberDecl =
              isSgMemberFunctionDeclaration(attachedCopiedDecl)) {
        populateCopiedMemberFunctionCtorInitializerSubtree(
            originalMemberDecl, attachedCopiedMemberDecl, help);
        remapCopiedMemberFunctionCtorInitializerSubtree(
            originalMemberDecl, attachedCopiedMemberDecl, help);
      }
    }

    if (SgFunctionDefinition *originalDef = originalDecl->get_definition()) {
      if (SgFunctionDefinition *attachedCopiedDef =
              attachedCopiedDecl->get_definition()) {
        remapCopiedNodePair(help, originalDef, attachedCopiedDef);
      }
    }
  }
}

void canonicalizeCopiedNamespaceDeclarationMapEntries(SgCopyHelp &help) {
  std::vector<const SgNamespaceDeclarationStatement *> originalNamespaceDecls;
  originalNamespaceDecls.reserve(help.get_copiedNodeMap().size());

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (const SgNamespaceDeclarationStatement *originalDecl =
            isSgNamespaceDeclarationStatement(it->first)) {
      originalNamespaceDecls.push_back(originalDecl);
    }
  }

  for (const SgNamespaceDeclarationStatement *originalDecl :
       originalNamespaceDecls) {
    SgNamespaceDeclarationStatement *attachedCopiedDecl =
        findAttachedCopiedNamespaceDeclaration(originalDecl, help);
    if (attachedCopiedDecl == NULL) {
      continue;
    }

    remapCopiedNodePair(help, originalDecl, attachedCopiedDecl);
    if (SgNamespaceDefinitionStatement *originalDef =
            originalDecl->get_definition()) {
      if (SgNamespaceDefinitionStatement *attachedCopiedDef =
              attachedCopiedDecl->get_definition()) {
        remapCopiedNodePair(help, originalDef, attachedCopiedDef);
      }
    }
  }
}

bool explicitDeclarationScopeMatchesStructuralParent(
    SgScopeStatement *declScope, SgScopeStatement *parentScope) {
  if (declScope == NULL || parentScope == NULL) {
    return false;
  }

  if (declScope == parentScope) {
    return true;
  }

  if (isSgGlobal(declScope) != NULL && isSgGlobal(parentScope) != NULL) {
    return true;
  }

  if (SgNamespaceDefinitionStatement *declNamespaceDef =
          isSgNamespaceDefinitionStatement(declScope)) {
    if (SgNamespaceDefinitionStatement *parentNamespaceDef =
            isSgNamespaceDefinitionStatement(parentScope)) {
      if (declNamespaceDef->isSameNamespace(parentNamespaceDef)) {
        return true;
      }

      SgNamespaceDefinitionStatement *declGlobalDef =
          declNamespaceDef->get_global_definition();
      SgNamespaceDefinitionStatement *parentGlobalDef =
          parentNamespaceDef->get_global_definition();
      if (declGlobalDef != NULL && parentGlobalDef != NULL &&
          declGlobalDef == parentGlobalDef) {
        return true;
      }
    }
  }

  SgDeclarationStatement *declOwner = scopeOwningDeclaration(declScope);
  SgDeclarationStatement *parentOwner = scopeOwningDeclaration(parentScope);
  return declOwner != NULL && declOwner == parentOwner;
}

SgDeclarationScope *
materializeCopiedDeclarationScope(const SgDeclarationScope *originalScope,
                                  SgCopyHelp &help) {
  if (originalScope == NULL) {
    return NULL;
  }

  if (SgDeclarationScope *copiedScope =
          isSgDeclarationScope(lookupCopiedNode(help, originalScope))) {
    return copiedScope;
  }

  SgDeclarationStatement *owningDecl =
      isSgDeclarationStatement(originalScope->get_parent());
  if (owningDecl != NULL) {
    return NULL;
  }

  SgScopeStatement *originalScopeParent =
      isSgScopeStatement(originalScope->get_parent());
  if (originalScopeParent == NULL) {
    return NULL;
  }

  SgScopeStatement *copiedScopeParent =
      isSgScopeStatement(lookupCopiedNode(help, originalScopeParent));
  if (copiedScopeParent == NULL) {
    return NULL;
  }

  SgDeclarationScope *copiedScope = SageBuilder::buildDeclarationScope();
  ROSE_ASSERT(copiedScope != NULL);

  if (copiedScope->get_parent() != copiedScopeParent) {
    copiedScope->set_parent(copiedScopeParent);
  }

  help.insertCopiedNodePair(originalScope, copiedScope);
  return copiedScope;
}

SgScopeStatement *resolveCopiedExplicitDeclarationScope(
    const SgDeclarationStatement *originalDecl,
    SgDeclarationStatement *copiedDecl, SgCopyHelp &help) {
  if (originalDecl == NULL || copiedDecl == NULL ||
      originalDecl->hasExplicitScope() == false) {
    return NULL;
  }

  SgScopeStatement *originalParentScope =
      isSgScopeStatement(originalDecl->get_parent());
  SgScopeStatement *copiedParentScope =
      isSgScopeStatement(copiedDecl->get_parent());
  SgScopeStatement *originalDeclScope = originalDecl->get_scope();

  // Preserve the copied equivalent of a distinct namespace scope. Re-entrant
  // namespace declarations can be structurally nested under one namespace
  // definition while explicitly belonging to that namespace's shared global
  // definition. Collapsing those onto the copied parent scope creates local
  // duplicate symbols that are later detached from any symbol table.
  if (isSgNamespaceDefinitionStatement(originalDeclScope) != NULL &&
      isSgNamespaceDefinitionStatement(originalParentScope) != NULL &&
      originalDeclScope != originalParentScope) {
    if (SgScopeStatement *copiedScope =
            isSgScopeStatement(lookupCopiedNode(help, originalDeclScope))) {
      return copiedScope;
    }
  }

  SgScopeStatement *copiedOriginalParentScope =
      isSgScopeStatement(lookupCopiedNode(help, originalParentScope));
  if (copiedParentScope != NULL &&
      copiedParentScope == copiedOriginalParentScope &&
      explicitDeclarationScopeMatchesStructuralParent(originalDeclScope,
                                                      originalParentScope)) {
    return copiedParentScope;
  }

  if (SgScopeStatement *copiedScope =
          isSgScopeStatement(lookupCopiedNode(help, originalDeclScope))) {
    return copiedScope;
  }

  if (SgDeclarationScope *originalDeclarationScope =
          isSgDeclarationScope(originalDeclScope)) {
    if (SgDeclarationScope *copiedScope =
            materializeCopiedDeclarationScope(originalDeclarationScope, help)) {
      return copiedScope;
    }
  }

  return NULL;
}

void canonicalizeCopiedNodeEdges(SgCopyHelp &help) {
  struct EdgeRewriter : public SimpleReferenceToPointerHandler {
    const std::unordered_map<SgNode *, SgNode *> &replacements;

    explicit EdgeRewriter(
        const std::unordered_map<SgNode *, SgNode *> &replacementMap)
        : replacements(replacementMap) {}

    void operator()(SgNode *&key, const SgName &, bool) override {
      if (key == NULL) {
        return;
      }

      std::unordered_map<SgNode *, SgNode *>::const_iterator it =
          replacements.find(key);
      if (it != replacements.end() && it->second != key) {
        key = it->second;
      }
    }
  };

  std::unordered_map<SgNode *, SgNode *> replacements;
  replacements.reserve(help.get_copiedNodeMap().size());

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    replacements.emplace(const_cast<SgNode *>(it->first), it->second);
  }

  EdgeRewriter rewriter(replacements);

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (it->first == it->second) {
      continue;
    }

    SgNode *copyNode = it->second;
    ROSE_ASSERT(copyNode != NULL);
    ROSE_ASSERT(!copiedNodeLooksDeleted(copyNode));
    copyNode->processDataMemberReferenceToPointers(&rewriter);
  }
}

void rewriteCopiedNodeEdges(
    SgCopyHelp &help,
    const std::unordered_map<SgNode *, SgNode *> &replacements) {
  if (replacements.empty()) {
    return;
  }

  struct EdgeRewriter : public SimpleReferenceToPointerHandler {
    const std::unordered_map<SgNode *, SgNode *> &replacements;

    explicit EdgeRewriter(
        const std::unordered_map<SgNode *, SgNode *> &replacementMap)
        : replacements(replacementMap) {}

    void operator()(SgNode *&key, const SgName &, bool) override {
      if (key == NULL) {
        return;
      }

      std::unordered_map<SgNode *, SgNode *>::const_iterator it =
          replacements.find(key);
      if (it != replacements.end() && it->second != key) {
        key = it->second;
      }
    }
  };

  EdgeRewriter rewriter(replacements);

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (it->first == it->second) {
      continue;
    }

    SgNode *copyNode = it->second;
    ROSE_ASSERT(copyNode != NULL);
    ROSE_ASSERT(!copiedNodeLooksDeleted(copyNode));
    copyNode->processDataMemberReferenceToPointers(&rewriter);
  }
}

void collectRelatedCopiedFunctionDeclarations(
    SgFunctionDeclaration *seed,
    std::unordered_set<SgFunctionDeclaration *> &chain) {
  if (seed == NULL) {
    return;
  }

  std::vector<SgFunctionDeclaration *> worklist(1, seed);
  while (!worklist.empty()) {
    SgFunctionDeclaration *current = worklist.back();
    worklist.pop_back();
    if (current == NULL ||
        (current != seed && !functionDeclarationsMatch(seed, current)) ||
        !chain.insert(current).second) {
      continue;
    }

    if (SgFunctionDeclaration *first = isSgFunctionDeclaration(
            current->get_firstNondefiningDeclaration())) {
      if (first == seed || functionDeclarationsMatch(seed, first)) {
        worklist.push_back(first);
      }
    }

    if (SgFunctionDeclaration *defining =
            isSgFunctionDeclaration(current->get_definingDeclaration())) {
      if (defining == seed || functionDeclarationsMatch(seed, defining)) {
        worklist.push_back(defining);
      }
    }
  }
}

void addFunctionParameterSubtreeReplacements(
    SgFunctionDeclaration *fromDecl, SgFunctionDeclaration *toDecl,
    std::unordered_map<SgNode *, SgNode *> &replacements) {
  if (fromDecl == NULL || toDecl == NULL || fromDecl == toDecl) {
    return;
  }

  SgFunctionParameterList *fromParams = fromDecl->get_parameterList();
  SgFunctionParameterList *toParams = toDecl->get_parameterList();
  if (fromParams == NULL || toParams == NULL || fromParams == toParams) {
    return;
  }

  replacements[fromParams] = toParams;

  const SgInitializedNamePtrList &fromArgs = fromParams->get_args();
  const SgInitializedNamePtrList &toArgs = toParams->get_args();
  const size_t argCount = std::min(fromArgs.size(), toArgs.size());
  for (size_t i = 0; i < argCount; ++i) {
    if (fromArgs[i] != NULL && toArgs[i] != NULL && fromArgs[i] != toArgs[i]) {
      replacements[fromArgs[i]] = toArgs[i];
    }
  }
}

void addMemberFunctionCtorInitializerSubtreeReplacements(
    SgMemberFunctionDeclaration *fromDecl, SgMemberFunctionDeclaration *toDecl,
    std::unordered_map<SgNode *, SgNode *> &replacements) {
  if (fromDecl == NULL || toDecl == NULL || fromDecl == toDecl) {
    return;
  }

  SgCtorInitializerList *fromCtorList = fromDecl->get_CtorInitializerList();
  SgCtorInitializerList *toCtorList = toDecl->get_CtorInitializerList();
  if (fromCtorList == NULL || toCtorList == NULL ||
      fromCtorList == toCtorList) {
    return;
  }

  replacements[fromCtorList] = toCtorList;

  const SgInitializedNamePtrList &fromCtors = fromCtorList->get_ctors();
  const SgInitializedNamePtrList &toCtors = toCtorList->get_ctors();
  const size_t ctorCount = std::min(fromCtors.size(), toCtors.size());
  for (size_t i = 0; i < ctorCount; ++i) {
    if (fromCtors[i] != NULL && toCtors[i] != NULL &&
        fromCtors[i] != toCtors[i]) {
      replacements[fromCtors[i]] = toCtors[i];
    }
  }
}

void canonicalizeCopiedFunctionDeclarationChains(SgCopyHelp &help) {
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

  std::unordered_set<SgFunctionDeclaration *> processedDefiningDecls;
  std::unordered_map<SgNode *, SgNode *> replacements;

  for (const SgFunctionDeclaration *originalDecl : originalFunctionDecls) {
    SgFunctionDeclaration *canonicalDefiningDecl =
        findAttachedCopiedFunctionDeclaration(originalDecl, help);
    if (canonicalDefiningDecl == NULL ||
        canonicalDefiningDecl->get_definition() == NULL ||
        canonicalDefiningDecl->get_definition()->get_parent() !=
            canonicalDefiningDecl ||
        !processedDefiningDecls.insert(canonicalDefiningDecl).second) {
      continue;
    }

    std::unordered_set<SgFunctionDeclaration *> functionChain;
    collectRelatedCopiedFunctionDeclarations(canonicalDefiningDecl,
                                             functionChain);

    SgFunctionDeclaration *canonicalFirstNondefiningDecl =
        isSgFunctionDeclaration(
            canonicalDefiningDecl->get_firstNondefiningDeclaration());
    if (canonicalFirstNondefiningDecl == canonicalDefiningDecl) {
      canonicalFirstNondefiningDecl = NULL;
    }
    if (canonicalFirstNondefiningDecl == NULL) {
      for (SgFunctionDeclaration *candidate : functionChain) {
        if (candidate != canonicalDefiningDecl &&
            candidate->get_definition() == NULL) {
          canonicalFirstNondefiningDecl = candidate;
          break;
        }
      }
    }

    SgFunctionDefinition *canonicalDefinition =
        canonicalDefiningDecl->get_definition();
    ROSE_ASSERT(canonicalDefinition != NULL);

    if (canonicalFirstNondefiningDecl != NULL) {
      canonicalFirstNondefiningDecl->set_firstNondefiningDeclaration(
          canonicalFirstNondefiningDecl);
      canonicalFirstNondefiningDecl->set_definingDeclaration(
          canonicalDefiningDecl);
    }

    canonicalDefiningDecl->set_definingDeclaration(canonicalDefiningDecl);
    canonicalDefiningDecl->set_firstNondefiningDeclaration(
        canonicalFirstNondefiningDecl);
    canonicalDefinition->set_declaration(canonicalDefiningDecl);

    for (SgFunctionDeclaration *candidate : functionChain) {
      if (candidate == NULL || candidate == canonicalDefiningDecl ||
          candidate == canonicalFirstNondefiningDecl) {
        continue;
      }

      replacements[candidate] = canonicalDefiningDecl;
      addFunctionParameterSubtreeReplacements(candidate, canonicalDefiningDecl,
                                              replacements);
      if (SgMemberFunctionDeclaration *candidateMemberDecl =
              isSgMemberFunctionDeclaration(candidate)) {
        if (SgMemberFunctionDeclaration *canonicalMemberDecl =
                isSgMemberFunctionDeclaration(canonicalDefiningDecl)) {
          addMemberFunctionCtorInitializerSubtreeReplacements(
              candidateMemberDecl, canonicalMemberDecl, replacements);
        }
      }

      if (candidate->get_definition() != NULL &&
          candidate->get_definition() != canonicalDefinition) {
        replacements[candidate->get_definition()] = canonicalDefinition;
      }
    }
  }

  rewriteCopiedNodeEdges(help, replacements);
}

bool symbolIsStillOwnedByParentTable(SgSymbol *symbol) {
  if (symbol == NULL) {
    return false;
  }

  SgSymbolTable *parentTable = isSgSymbolTable(symbol->get_parent());
  return parentTable != NULL && parentTable->exists(symbol);
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
  case V_SgFunctionParameterList:
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

void normalizeSelfDefiningCopiedDeclarations(SgCopyHelp &help) {
  std::unordered_set<SgDeclarationStatement *> seen;
  seen.reserve(help.get_copiedNodeMap().size());

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    SgDeclarationStatement *copiedDecl = isSgDeclarationStatement(it->second);
    if (copiedDecl == NULL || !seen.insert(copiedDecl).second ||
        !declarationMustDefineItself(copiedDecl)) {
      continue;
    }

    copiedDecl->set_definingDeclaration(copiedDecl);
    copiedDecl->set_firstNondefiningDeclaration(copiedDecl);
  }
}

void discardSupersededCopiedDeclarations(
    SgCopyHelp &help, const std::unordered_set<SgNode *> &canonicalCopies) {
  for (SgCopyHelp::supersededNodeSetTypeIterator it =
           help.get_supersededNodeCopies().begin();
       it != help.get_supersededNodeCopies().end(); ++it) {
    SgDeclarationStatement *staleDeclaration = isSgDeclarationStatement(*it);
    if (staleDeclaration == NULL) {
      continue;
    }

    if (isTrackedEmptyDeclaration(staleDeclaration)) {
      fprintf(stderr,
              "[copy-empty] discard stale=%p parent=%p scope=%p canonical=%s\n",
              staleDeclaration, staleDeclaration->get_parent(),
              staleDeclaration->get_scope(),
              canonicalCopies.find(staleDeclaration) != canonicalCopies.end()
                  ? "true"
                  : "false");
      fflush(stderr);
    }

    if (canonicalCopies.find(staleDeclaration) != canonicalCopies.end()) {
      continue;
    }

    SgScopeStatement *parentScope =
        isSgScopeStatement(staleDeclaration->get_parent());
    if (declarationIsStillOwnedByScope(staleDeclaration, parentScope)) {
      continue;
    }

    if (parentScope == NULL) {
      deleteDetachedCopiedSubtree(staleDeclaration);
      continue;
    }

    SgScopeStatement *declScope = staleDeclaration->get_scope();
    if (declarationIsStillOwnedByScope(staleDeclaration, declScope)) {
      if (staleDeclaration->get_parent() != declScope) {
        staleDeclaration->set_parent(declScope);
      }
      continue;
    }

    SageInterface::deleteAST(staleDeclaration);
  }
}

void discardSupersededCopiedSymbols(
    SgCopyHelp &help, const std::unordered_set<SgNode *> &canonicalCopies) {
  for (SgCopyHelp::supersededNodeSetTypeIterator it =
           help.get_supersededNodeCopies().begin();
       it != help.get_supersededNodeCopies().end(); ++it) {
    SgSymbol *staleSymbol = isSgSymbol(*it);
    if (staleSymbol == NULL) {
      continue;
    }

    if (canonicalCopies.find(staleSymbol) != canonicalCopies.end()) {
      continue;
    }

    if (symbolIsStillOwnedByParentTable(staleSymbol)) {
      continue;
    }

    if (staleSymbol->get_parent() != NULL &&
        isSgSymbolTable(staleSymbol->get_parent()) == NULL) {
      continue;
    }
    move_symbol_to_orphan_table(staleSymbol);
  }

  help.get_supersededNodeCopies().clear();
}

SgClassDeclaration *
canonicalFirstNondefiningClassDeclaration(SgClassDeclaration *classDecl) {
  if (classDecl == NULL) {
    return NULL;
  }

  if (SgClassDeclaration *firstNondefining =
          isSgClassDeclaration(classDecl->get_firstNondefiningDeclaration())) {
    return firstNondefining;
  }

  return classDecl;
}

SgClassDeclaration *
canonicalDefiningClassDeclaration(SgClassDeclaration *classDecl) {
  if (classDecl == NULL) {
    return NULL;
  }

  SgClassDeclaration *firstNondefining =
      canonicalFirstNondefiningClassDeclaration(classDecl);
  if (firstNondefining == NULL) {
    return NULL;
  }

  if (SgClassDeclaration *definingDecl =
          isSgClassDeclaration(firstNondefining->get_definingDeclaration())) {
    return definingDecl;
  }

  return isSgClassDeclaration(classDecl->get_definingDeclaration());
}

bool classDeclarationsShareCopyChain(SgClassDeclaration *candidate,
                                     SgClassDeclaration *firstNondefining,
                                     SgClassDeclaration *definingDecl) {
  if (candidate == NULL || firstNondefining == NULL) {
    return false;
  }

  SgClassDeclaration *candidateFirst =
      canonicalFirstNondefiningClassDeclaration(candidate);
  if (candidateFirst != firstNondefining) {
    return false;
  }

  SgClassDeclaration *candidateDef =
      canonicalDefiningClassDeclaration(candidate);
  if (definingDecl == NULL || definingDecl == firstNondefining) {
    return candidateDef == NULL || candidateDef == candidateFirst ||
           candidateDef == definingDecl;
  }

  return candidateDef == definingDecl;
}

SgClassType *canonicalizeCopiedClassType(SgClassDeclaration *classDecl) {
  if (classDecl == NULL) {
    return NULL;
  }

  SgClassDeclaration *firstNondefining =
      canonicalFirstNondefiningClassDeclaration(classDecl);
  if (firstNondefining == NULL) {
    return NULL;
  }

  SgClassDeclaration *definingDecl =
      canonicalDefiningClassDeclaration(classDecl);

  auto typeBelongsToCurrentChain = [&](SgClassType *candidateType) {
    if (candidateType == NULL) {
      return false;
    }

    SgClassDeclaration *typeDecl =
        isSgClassDeclaration(candidateType->get_declaration());
    return classDeclarationsShareCopyChain(typeDecl, firstNondefining,
                                           definingDecl);
  };

  auto selectCanonicalType = [&](SgClassDeclaration *candidateDecl) {
    if (candidateDecl == NULL) {
      return static_cast<SgClassType *>(NULL);
    }

    SgClassType *candidateType = isSgClassType(candidateDecl->get_type());
    return typeBelongsToCurrentChain(candidateType)
               ? candidateType
               : static_cast<SgClassType *>(NULL);
  };

  SgClassType *canonicalType = selectCanonicalType(firstNondefining);
  if (canonicalType == NULL) {
    canonicalType = selectCanonicalType(classDecl);
  }
  if (canonicalType == NULL) {
    canonicalType = selectCanonicalType(definingDecl);
  }
  if (canonicalType == NULL) {
    canonicalType = SgClassType::createType(firstNondefining);
  }

  ROSE_ASSERT(canonicalType != NULL);

  auto attachType = [&](SgClassDeclaration *candidate) {
    if (candidate != NULL && candidate->get_type() != canonicalType) {
      candidate->set_type(canonicalType);
    }
  };

  attachType(firstNondefining);
  attachType(definingDecl);
  attachType(classDecl);

  if (canonicalType->get_declaration() != firstNondefining) {
    canonicalType->set_declaration(firstNondefining);
  }

  return canonicalType;
}

void canonicalizeCopiedClassTypes(SgCopyHelp &help) {
  std::unordered_map<SgNode *, SgNode *> replacements;
  replacements.reserve(help.get_copiedNodeMap().size());

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgClassDeclaration *originalClassDecl =
        isSgClassDeclaration(it->first);
    SgClassDeclaration *copyClassDecl = isSgClassDeclaration(it->second);
    if (originalClassDecl == NULL || copyClassDecl == NULL) {
      continue;
    }

    SgClassType *staleCopyType = isSgClassType(copyClassDecl->get_type());
    SgClassType *canonicalType = canonicalizeCopiedClassType(copyClassDecl);
    if (canonicalType == NULL) {
      continue;
    }

    if (SgClassType *mappedCopyType = isSgClassType(
            lookupCopiedNode(help, originalClassDecl->get_type()));
        mappedCopyType != NULL && mappedCopyType != canonicalType) {
      replacements[mappedCopyType] = canonicalType;
    }

    if (staleCopyType != NULL && staleCopyType != canonicalType) {
      replacements[staleCopyType] = canonicalType;
    }

    remapCopiedNodePair(help, originalClassDecl->get_type(), canonicalType);
  }

  if (!replacements.empty()) {
    rewriteCopiedNodeEdges(help, replacements);
  }
}

void restoreOriginalClassTypes(SgCopyHelp &help) {
  std::unordered_set<SgClassDeclaration *> originalClassDecls;
  originalClassDecls.reserve(help.get_copiedNodeMap().size());

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (const SgClassDeclaration *originalClassDecl =
            isSgClassDeclaration(it->first)) {
      originalClassDecls.insert(
          const_cast<SgClassDeclaration *>(originalClassDecl));
    }
  }

  for (SgClassDeclaration *originalClassDecl : originalClassDecls) {
    SgClassDeclaration *firstNondefining =
        canonicalFirstNondefiningClassDeclaration(originalClassDecl);
    if (firstNondefining == NULL) {
      continue;
    }

    SgClassDeclaration *definingDecl =
        canonicalDefiningClassDeclaration(originalClassDecl);

    auto typeBelongsToOriginalChain = [&](SgClassType *candidateType) {
      if (candidateType == NULL) {
        return false;
      }

      SgClassDeclaration *typeDecl =
          isSgClassDeclaration(candidateType->get_declaration());
      return classDeclarationsShareCopyChain(typeDecl, firstNondefining,
                                             definingDecl);
    };

    SgClassType *canonicalType =
        typeBelongsToOriginalChain(isSgClassType(firstNondefining->get_type()))
            ? isSgClassType(firstNondefining->get_type())
            : static_cast<SgClassType *>(NULL);
    if (canonicalType == NULL && typeBelongsToOriginalChain(isSgClassType(
                                     originalClassDecl->get_type()))) {
      canonicalType = isSgClassType(originalClassDecl->get_type());
    }
    if (canonicalType == NULL &&
        typeBelongsToOriginalChain(isSgClassType(
            definingDecl != NULL ? definingDecl->get_type() : NULL))) {
      canonicalType =
          isSgClassType(definingDecl != NULL ? definingDecl->get_type() : NULL);
    }
    if (canonicalType == NULL) {
      canonicalType = SgClassType::createType(firstNondefining);
    }

    if (firstNondefining->get_type() != canonicalType) {
      firstNondefining->set_type(canonicalType);
    }
    if (definingDecl != NULL && definingDecl->get_type() != canonicalType) {
      definingDecl->set_type(canonicalType);
    }
    if (originalClassDecl->get_type() != canonicalType) {
      originalClassDecl->set_type(canonicalType);
    }
    if (canonicalType->get_declaration() != firstNondefining) {
      canonicalType->set_declaration(firstNondefining);
    }
  }
}

void repairCopiedExplicitDeclarationScopesFromParents(SgCopyHelp &help) {
  std::unordered_set<SgScopeStatement *> copiedScopes;
  copiedScopes.reserve(help.get_copiedNodeMap().size());

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (SgScopeStatement *copyScope = isSgScopeStatement(it->second)) {
      copiedScopes.insert(copyScope);
    }
  }

  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgDeclarationStatement *originalDecl =
        isSgDeclarationStatement(it->first);
    SgDeclarationStatement *copyDecl = isSgDeclarationStatement(it->second);
    if (originalDecl == NULL || copyDecl == NULL ||
        originalDecl->hasExplicitScope() == false) {
      continue;
    }

    SgScopeStatement *copiedParentScope =
        isSgScopeStatement(copyDecl->get_parent());
    if (copiedParentScope == NULL ||
        copiedScopes.find(copiedParentScope) == copiedScopes.end()) {
      continue;
    }

    SgScopeStatement *currentCopiedScope = copyDecl->get_scope();
    if (currentCopiedScope == copiedParentScope) {
      continue;
    }

    SgScopeStatement *originalParentScope =
        isSgScopeStatement(originalDecl->get_parent());
    if (explicitDeclarationScopeMatchesStructuralParent(
            originalDecl->get_scope(), originalParentScope) == false) {
      continue;
    }

    copyDecl->set_scope(copiedParentScope);
  }
}

void restoreCopiedReferencedDeclarationParentsToScope(
    SgDeclarationStatement *copiedDecl, SgCopyHelp &help) {
  if (copiedDecl == NULL) {
    return;
  }

  std::unordered_set<SgDeclarationStatement *> candidates;
  candidates.insert(copiedDecl);

  if (SgDeclarationStatement *copiedFirstNondef =
          copiedDecl->get_firstNondefiningDeclaration()) {
    candidates.insert(copiedFirstNondef);
  }

  if (SgDeclarationStatement *copiedDefining =
          copiedDecl->get_definingDeclaration()) {
    candidates.insert(copiedDefining);
  }

  for (SgDeclarationStatement *candidate : candidates) {
    if (candidate == NULL || isSgBaseClass(candidate->get_parent()) == NULL) {
      continue;
    }

    SgScopeStatement *copiedScope = candidate->get_scope();
    if (copiedScope == NULL) {
      continue;
    }

    if (candidate->get_parent() != copiedScope) {
      candidate->set_parent(copiedScope);
    }
  }
}

void repairCopiedDeclarationParentsFromScopes(SgCopyHelp &help) {
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgDeclarationStatement *originalDecl =
        isSgDeclarationStatement(it->first);
    SgDeclarationStatement *copyDecl = isSgDeclarationStatement(it->second);
    if (originalDecl == NULL || copyDecl == NULL) {
      continue;
    }

    SgScopeStatement *originalParentScope =
        isSgScopeStatement(originalDecl->get_parent());
    if (originalParentScope == NULL) {
      continue;
    }

    SgScopeStatement *copiedParentScope =
        isSgScopeStatement(lookupCopiedNode(help, originalParentScope));
    if (copiedParentScope == NULL ||
        !scopeDirectlyContainsStatement(copiedParentScope, copyDecl)) {
      continue;
    }

    if (copyDecl->get_parent() != copiedParentScope) {
      copyDecl->set_parent(copiedParentScope);
    }
  }
}

void canonicalizeCopiedEnumEnumerators(const SgEnumDeclaration *originalEnum,
                                       SgEnumDeclaration *copyEnum,
                                       SgCopyHelp &help) {
  ROSE_ASSERT(originalEnum != NULL);
  ROSE_ASSERT(copyEnum != NULL);

  const SgInitializedNamePtrList &originalFields =
      originalEnum->get_enumerators();
  SgInitializedNamePtrList &copyFields = copyEnum->get_enumerators();

  std::unordered_set<SgInitializedName *> canonicalFields;
  canonicalFields.reserve(originalFields.size());

  SgInitializedNamePtrList reorderedFields;
  reorderedFields.reserve(originalFields.size());
  for (size_t index = 0; index < originalFields.size(); ++index) {
    SgInitializedName *originalField = originalFields[index];
    if (originalField == NULL) {
      reorderedFields.push_back(NULL);
      continue;
    }

    SgInitializedName *copiedField =
        isSgInitializedName(lookupCopiedNode(help, originalField));
    if (copiedField == NULL) {
      if (index < copyFields.size()) {
        SgInitializedName *existingField = copyFields[index];
        if (enumFieldsMatchForCanonicalCopy(originalField, existingField)) {
          copiedField = existingField;
        }
      }
    }
    if (copiedField == NULL) {
      copiedField = isSgInitializedName(help.copyOrLookupAst(originalField));
    }
    ROSE_ASSERT(copiedField != NULL);

    canonicalFields.insert(copiedField);
    remapCopiedNodePair(help, originalField, copiedField);
    reorderedFields.push_back(copiedField);
  }

  for (SgInitializedName *field : copyFields) {
    if (field != NULL && canonicalFields.find(field) == canonicalFields.end() &&
        field->get_parent() == copyEnum) {
      field->set_parent(NULL);
    }
  }

  copyFields = reorderedFields;
  for (SgInitializedName *field : copyFields) {
    if (field != NULL && field->get_parent() != copyEnum) {
      field->set_parent(copyEnum);
    }
  }
}

const SgEnumDeclaration *
canonicalOriginalEnumDefinition(const SgEnumDeclaration *originalEnum) {
  ROSE_ASSERT(originalEnum != NULL);

  if (const SgEnumDeclaration *originalDefining =
          isSgEnumDeclaration(originalEnum->get_definingDeclaration())) {
    return originalDefining;
  }

  return originalEnum;
}

bool copiedEnumIsAttachedToAst(SgEnumDeclaration *copyEnum) {
  if (copyEnum == NULL) {
    return false;
  }

  SgScopeStatement *parentScope = isSgScopeStatement(copyEnum->get_parent());
  return parentScope != NULL &&
         scopeDirectlyContainsStatement(parentScope, copyEnum);
}

bool copiedEnumsShareSourceFile(SgEnumDeclaration *lhs,
                                SgEnumDeclaration *rhs) {
  if (lhs == NULL || rhs == NULL) {
    return false;
  }

  auto filenameForEnum = [](SgEnumDeclaration *decl) -> std::string {
    if (decl == NULL) {
      return "";
    }

    if (decl->get_file_info() != NULL) {
      std::string filename = decl->get_file_info()->get_filenameString();
      if (!filename.empty()) {
        return filename;
      }
    }

    if (SgSourceFile *sourceFile =
            SageInterface::getEnclosingSourceFile(decl)) {
      if (sourceFile->get_file_info() != NULL) {
        return sourceFile->get_file_info()->get_filenameString();
      }
    }

    return "";
  };

  std::string lhsFilename = filenameForEnum(lhs);
  std::string rhsFilename = filenameForEnum(rhs);
  if (!lhsFilename.empty() || !rhsFilename.empty()) {
    return lhsFilename == rhsFilename;
  }

  return false;
}

void repairCopiedOwnedDeclarationScope(
    const SgDeclarationStatement *originalDecl,
    SgDeclarationStatement *copiedDecl, SgCopyHelp &help) {
  if (originalDecl == NULL || copiedDecl == NULL) {
    return;
  }

  SgDeclarationScope *originalOwnedScope =
      originalDecl->get_nonreal_decl_scope();
  if (originalOwnedScope == NULL) {
    return;
  }

  SgDeclarationScope *copiedOwnedScope =
      isSgDeclarationScope(lookupCopiedNode(help, originalOwnedScope));
  if (copiedOwnedScope == NULL) {
    copiedOwnedScope = copiedDecl->get_nonreal_decl_scope();
    if (copiedNodeLooksDeleted(copiedOwnedScope)) {
      copiedOwnedScope = NULL;
    }
  }
  if (copiedOwnedScope == NULL) {
    copiedOwnedScope =
        SageBuilder::getOrCreateNonrealDeclarationScope(copiedDecl);
  }
  if (copiedOwnedScope == NULL) {
    return;
  }

  remapCopiedNodePair(help, originalOwnedScope, copiedOwnedScope);

  if (copiedDecl->get_nonreal_decl_scope() != copiedOwnedScope) {
    copiedDecl->set_nonreal_decl_scope(copiedOwnedScope);
  }

  if (copiedOwnedScope->get_parent() != copiedDecl) {
    copiedOwnedScope->set_parent(copiedDecl);
  }
}

void repairCopiedNonrealDeclScope(const SgNonrealDecl *originalNonreal,
                                  SgNonrealDecl *copiedNonreal,
                                  SgCopyHelp &help);

SgDeclarationScope *
resolveCopiedDeclarationScope(const SgDeclarationStatement *originalOwner,
                              SgCopyHelp &help) {
  if (originalOwner == NULL) {
    return NULL;
  }

  SgDeclarationStatement *copiedOwner =
      isSgDeclarationStatement(lookupCopiedNode(help, originalOwner));
  if (copiedOwner != NULL) {
    repairCopiedOwnedDeclarationScope(originalOwner, copiedOwner, help);
    if (const SgNonrealDecl *originalOwnerNonreal =
            isSgNonrealDecl(originalOwner)) {
      repairCopiedNonrealDeclScope(originalOwnerNonreal,
                                   isSgNonrealDecl(copiedOwner), help);
    }

    if (copiedOwner->get_nonreal_decl_scope() != NULL) {
      return copiedOwner->get_nonreal_decl_scope();
    }
  }

  if (SgDeclarationScope *copiedScope = isSgDeclarationScope(
          lookupCopiedNode(help, originalOwner->get_nonreal_decl_scope()))) {
    return copiedScope;
  }

  return NULL;
}

void repairCopiedNonrealDeclScope(const SgNonrealDecl *originalNonreal,
                                  SgNonrealDecl *copiedNonreal,
                                  SgCopyHelp &help) {
  if (originalNonreal == NULL || copiedNonreal == NULL) {
    return;
  }

  repairCopiedOwnedDeclarationScope(originalNonreal, copiedNonreal, help);

  SgDeclarationScope *originalScope =
      isSgDeclarationScope(originalNonreal->get_scope());
  if (originalScope == NULL) {
    return;
  }

  SgDeclarationStatement *originalOwner =
      isSgDeclarationStatement(originalScope->get_parent());
  if (originalOwner != NULL) {
    if (const SgNonrealDecl *originalOwnerNonreal =
            isSgNonrealDecl(originalOwner)) {
      repairCopiedNonrealDeclScope(
          originalOwnerNonreal,
          isSgNonrealDecl(lookupCopiedNode(help, originalOwnerNonreal)), help);
    } else {
      repairCopiedOwnedDeclarationScope(
          originalOwner,
          isSgDeclarationStatement(lookupCopiedNode(help, originalOwner)),
          help);
    }
  }

  SgDeclarationScope *copiedScope =
      materializeCopiedDeclarationScope(originalScope, help);
  if (copiedScope == NULL) {
    copiedScope = resolveCopiedDeclarationScope(originalOwner, help);
  }
  if (copiedScope == NULL) {
    if (SgDeclarationStatement *copiedContainerDecl =
            isSgDeclarationStatement(copiedNonreal->get_parent())) {
      copiedScope =
          SageBuilder::getOrCreateNonrealDeclarationScope(copiedContainerDecl);
    }
  }

  if (copiedScope == NULL) {
    return;
  }

  if (copiedNonreal->get_scope() != copiedScope) {
    copiedNonreal->set_scope(copiedScope);
  }

  if (!scopeDirectlyContainsStatement(copiedScope, copiedNonreal)) {
    copiedScope->insertStatementInScope(copiedNonreal, false);
  }

  if (copiedNonreal->get_parent() != copiedScope) {
    copiedNonreal->set_parent(copiedScope);
  }
}

void clearCopiedEnumEnumerators(SgEnumDeclaration *copyEnum) {
  ROSE_ASSERT(copyEnum != NULL);

  SgInitializedNamePtrList &copyEnumerators = copyEnum->get_enumerators();
  for (SgInitializedName *field : copyEnumerators) {
    if (field != NULL && field->get_parent() == copyEnum) {
      field->set_parent(NULL);
    }
  }
  copyEnumerators.clear();
}

SgEnumDeclaration *
canonicalizeCopiedEnumDeclarationChain(const SgEnumDeclaration *originalEnum,
                                       SgEnumDeclaration *copyEnum,
                                       SgCopyHelp &help) {
  ROSE_ASSERT(originalEnum != NULL);
  ROSE_ASSERT(copyEnum != NULL);

  const SgEnumDeclaration *originalFirstNondef =
      isSgEnumDeclaration(originalEnum->get_firstNondefiningDeclaration());
  const SgEnumDeclaration *originalDefining =
      canonicalOriginalEnumDefinition(originalEnum);

  SgEnumDeclaration *copiedFirstNondef = isSgEnumDeclaration(lookupCopiedNode(
      help, originalFirstNondef != NULL ? originalFirstNondef : originalEnum));
  if (copiedFirstNondef == NULL) {
    copiedFirstNondef = copyEnum;
  }

  SgEnumDeclaration *copiedDefining = isSgEnumDeclaration(lookupCopiedNode(
      help, originalDefining != NULL ? originalDefining : originalEnum));
  SgEnumDeclaration *localFirstNondef =
      copiedEnumsShareSourceFile(copyEnum, copiedFirstNondef)
          ? copiedFirstNondef
          : copyEnum;
  SgEnumDeclaration *localDefining =
      copiedEnumsShareSourceFile(copyEnum, copiedDefining) ? copiedDefining
                                                           : NULL;

  auto hasEnumerators = [](SgEnumDeclaration *candidate) -> bool {
    return candidate != NULL && !candidate->get_enumerators().empty();
  };

  SgEnumDeclaration *effectiveDefinition = NULL;

  if (hasEnumerators(copyEnum) &&
      (localDefining == NULL || localDefining == copyEnum) &&
      copiedEnumIsAttachedToAst(copyEnum)) {
    effectiveDefinition = copyEnum;
  } else if (hasEnumerators(localDefining) &&
             copiedEnumIsAttachedToAst(localDefining)) {
    effectiveDefinition = localDefining;
  } else if (hasEnumerators(copyEnum) && copiedEnumIsAttachedToAst(copyEnum)) {
    effectiveDefinition = copyEnum;
  } else if (hasEnumerators(localFirstNondef) &&
             copiedEnumIsAttachedToAst(localFirstNondef)) {
    effectiveDefinition = localFirstNondef;
  } else if (hasEnumerators(localDefining)) {
    effectiveDefinition = localDefining;
  } else if (hasEnumerators(copyEnum)) {
    effectiveDefinition = copyEnum;
  } else if (hasEnumerators(localFirstNondef)) {
    effectiveDefinition = localFirstNondef;
  } else if (localDefining != NULL) {
    effectiveDefinition = localDefining;
  } else {
    effectiveDefinition = copyEnum;
  }

  localFirstNondef->set_firstNondefiningDeclaration(localFirstNondef);
  localFirstNondef->set_definingDeclaration(effectiveDefinition);

  effectiveDefinition->set_firstNondefiningDeclaration(localFirstNondef);
  effectiveDefinition->set_definingDeclaration(effectiveDefinition);
  effectiveDefinition->unsetForward();

  auto normalizeCopy = [&](SgEnumDeclaration *candidate) {
    if (candidate == NULL) {
      return;
    }

    if (candidate->get_firstNondefiningDeclaration() != localFirstNondef) {
      candidate->set_firstNondefiningDeclaration(localFirstNondef);
    }
    if (candidate == effectiveDefinition) {
      if (candidate->get_definingDeclaration() != effectiveDefinition) {
        candidate->set_definingDeclaration(effectiveDefinition);
      }
      candidate->unsetForward();
    } else if (candidate->get_definingDeclaration() != effectiveDefinition) {
      candidate->set_definingDeclaration(effectiveDefinition);
    }
  };

  normalizeCopy(copyEnum);
  if (localFirstNondef != copyEnum) {
    normalizeCopy(localFirstNondef);
  }
  if (localDefining != NULL && localDefining != copyEnum &&
      localDefining != localFirstNondef) {
    normalizeCopy(localDefining);
  }

  auto clearDuplicateEnumerators = [&](SgEnumDeclaration *candidate) {
    if (candidate != NULL && candidate != effectiveDefinition &&
        copiedEnumsShareSourceFile(candidate, effectiveDefinition) &&
        !candidate->get_enumerators().empty()) {
      clearCopiedEnumEnumerators(candidate);
    }
  };

  clearDuplicateEnumerators(copyEnum);
  if (localFirstNondef != copyEnum) {
    clearDuplicateEnumerators(localFirstNondef);
  }
  if (localDefining != NULL && localDefining != copyEnum &&
      localDefining != localFirstNondef) {
    clearDuplicateEnumerators(localDefining);
  }

  return effectiveDefinition;
}

SgFunctionDeclaration *resolveCanonicalCopiedDefiningFunctionDeclaration(
    const SgFunctionDefinition *originalFunctionDef, SgCopyHelp &help) {
  if (originalFunctionDef == NULL) {
    return NULL;
  }

  const SgFunctionDeclaration *originalDecl =
      originalFunctionDef->get_declaration();
  if (originalDecl == NULL) {
    return NULL;
  }

  const SgFunctionDeclaration *originalFirstNondef =
      isSgFunctionDeclaration(originalDecl->get_firstNondefiningDeclaration());
  if (originalFirstNondef == NULL) {
    originalFirstNondef = originalDecl;
  }

  if (SgFunctionDeclaration *copiedFirstNondef = isSgFunctionDeclaration(
          lookupCopiedNode(help, originalFirstNondef))) {
    if (SgFunctionDeclaration *copiedDefining = isSgFunctionDeclaration(
            copiedFirstNondef->get_definingDeclaration())) {
      return copiedDefining;
    }

    if (copiedFirstNondef->get_definingDeclaration() == copiedFirstNondef) {
      return copiedFirstNondef;
    }
  }

  return isSgFunctionDeclaration(lookupCopiedNode(help, originalDecl));
}

void repairCopiedFunctionDefinitionDeclarations(SgCopyHelp &help) {
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    const SgFunctionDefinition *originalFunctionDef =
        isSgFunctionDefinition(it->first);
    SgFunctionDefinition *copyFunctionDef = isSgFunctionDefinition(it->second);
    if (originalFunctionDef == NULL || copyFunctionDef == NULL) {
      continue;
    }

    SgFunctionDeclaration *canonicalDecl =
        resolveCanonicalCopiedDefiningFunctionDeclaration(originalFunctionDef,
                                                          help);
    if (canonicalDecl == NULL) {
      canonicalDecl = isSgFunctionDeclaration(copyFunctionDef->get_parent());
    }
    if (canonicalDecl == NULL) {
      continue;
    }

    if (copyFunctionDef->get_declaration() != canonicalDecl) {
      copyFunctionDef->set_declaration(canonicalDecl);
    }

    if (canonicalDecl->get_definition() != copyFunctionDef) {
      canonicalDecl->set_definition(copyFunctionDef);
    }
  }
}

void finalizeCanonicalCopyLinks(SgCopyHelp &help) {
  std::unordered_set<SgNode *> canonicalCopies;
  canonicalizeCopiedNamespaceDeclarationMapEntries(help);
  canonicalizeCopiedFunctionDeclarationMapEntries(help);
  removeCopiedNodeMapAliasEntries(help);
  removeDeletedCopiedNodeMapEntries(help);
  CopiedNodeReplacementMap pendingReplacements =
      consumePendingCopiedNodeReplacements(help);
  if (!pendingReplacements.empty()) {
    rewriteCopiedNodeEdges(help, pendingReplacements);
  }
  canonicalCopies.reserve(help.get_copiedNodeMap().size());
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    if (isTrackedEmptyDeclaration(isSgDeclarationStatement(it->second))) {
      fprintf(stderr,
              "[copy-empty] map original=%p copy=%p parent=%p scope=%p\n",
              it->first, it->second,
              it->second != NULL ? it->second->get_parent() : NULL,
              isSgDeclarationStatement(it->second) != NULL
                  ? isSgDeclarationStatement(it->second)->get_scope()
                  : NULL);
      fflush(stderr);
    }
    canonicalCopies.insert(it->second);
  }
  canonicalizeCopiedNodeEdges(help);

  std::vector<std::pair<const SgNode *, SgNode *>> copiedNodePairs;
  copiedNodePairs.reserve(help.get_copiedNodeMap().size());
  for (SgCopyHelp::copiedNodeMapTypeIterator it =
           help.get_copiedNodeMap().begin();
       it != help.get_copiedNodeMap().end(); ++it) {
    copiedNodePairs.push_back(std::make_pair(it->first, it->second));
  }

  for (std::vector<std::pair<const SgNode *, SgNode *>>::const_iterator it =
           copiedNodePairs.begin();
       it != copiedNodePairs.end(); ++it) {
    if (SgDeclarationStatement *copyDecl =
            isSgDeclarationStatement(it->second)) {
      restoreCopiedReferencedDeclarationParentsToScope(copyDecl, help);
    }
  }

  for (std::vector<std::pair<const SgNode *, SgNode *>>::const_iterator it =
           copiedNodePairs.begin();
       it != copiedNodePairs.end(); ++it) {
    const SgNode *originalNode = it->first;
    SgNode *copyNode = it->second;

    if (const SgDeclarationStatement *originalDecl =
            isSgDeclarationStatement(originalNode)) {
      SgDeclarationStatement *copyDecl = isSgDeclarationStatement(copyNode);
      ROSE_ASSERT(copyDecl != NULL);

      const SgDeclarationStatement *originalDefining =
          originalDecl->get_definingDeclaration();
      if (originalDefining == originalDecl) {
        copyDecl->set_definingDeclaration(copyDecl);
      } else if (SgDeclarationStatement *copiedDefining =
                     isSgDeclarationStatement(
                         lookupCopiedNode(help, originalDefining))) {
        copyDecl->set_definingDeclaration(copiedDefining);
      } else if (originalDefining == NULL) {
        copyDecl->set_definingDeclaration(NULL);
      }

      const SgDeclarationStatement *originalFirstNondef =
          originalDecl->get_firstNondefiningDeclaration();
      if (originalFirstNondef == originalDecl) {
        copyDecl->set_firstNondefiningDeclaration(copyDecl);
      } else if (SgDeclarationStatement *copiedFirstNondef =
                     isSgDeclarationStatement(
                         lookupCopiedNode(help, originalFirstNondef))) {
        copyDecl->set_firstNondefiningDeclaration(copiedFirstNondef);
      } else if (originalFirstNondef == NULL) {
        copyDecl->set_firstNondefiningDeclaration(NULL);
      }

      if (SgScopeStatement *copiedScope = resolveCopiedExplicitDeclarationScope(
              originalDecl, copyDecl, help)) {
        copyDecl->set_scope(copiedScope);
      }
    }

    if (const SgClassDefinition *originalClassDef =
            isSgClassDefinition(originalNode)) {
      SgClassDefinition *copyClassDef = isSgClassDefinition(copyNode);
      ROSE_ASSERT(copyClassDef != NULL);

      if (SgClassDeclaration *copiedClassDecl = isSgClassDeclaration(
              lookupCopiedNode(help, originalClassDef->get_declaration()))) {
        copyClassDef->set_declaration(copiedClassDecl);
      }
    }

    if (const SgNamespaceDefinitionStatement *originalNamespaceDef =
            isSgNamespaceDefinitionStatement(originalNode)) {
      SgNamespaceDefinitionStatement *copyNamespaceDef =
          isSgNamespaceDefinitionStatement(copyNode);
      ROSE_ASSERT(copyNamespaceDef != NULL);

      if (SgNamespaceDeclarationStatement *copiedNamespaceDecl =
              isSgNamespaceDeclarationStatement(lookupCopiedNode(
                  help, originalNamespaceDef->get_namespaceDeclaration()))) {
        copyNamespaceDef->set_namespaceDeclaration(copiedNamespaceDecl);
      }
    }

    if (const SgTemplateInstantiationDecl *originalTemplateInst =
            isSgTemplateInstantiationDecl(originalNode)) {
      SgTemplateInstantiationDecl *copyTemplateInst =
          isSgTemplateInstantiationDecl(copyNode);
      ROSE_ASSERT(copyTemplateInst != NULL);

      if (SgTemplateClassDeclaration *copiedTemplateDecl =
              isSgTemplateClassDeclaration(lookupCopiedNode(
                  help, originalTemplateInst->get_templateDeclaration()))) {
        copyTemplateInst->set_templateDeclaration(copiedTemplateDecl);
      }
    }

    if (const SgTemplateInstantiationFunctionDecl
            *originalTemplateFunctionInst =
                isSgTemplateInstantiationFunctionDecl(originalNode)) {
      SgTemplateInstantiationFunctionDecl *copyTemplateFunctionInst =
          isSgTemplateInstantiationFunctionDecl(copyNode);
      ROSE_ASSERT(copyTemplateFunctionInst != NULL);

      if (SgTemplateFunctionDeclaration *copiedTemplateDecl =
              isSgTemplateFunctionDeclaration(lookupCopiedNode(
                  help,
                  originalTemplateFunctionInst->get_templateDeclaration()))) {
        copyTemplateFunctionInst->set_templateDeclaration(copiedTemplateDecl);
      }
    }

    if (const SgTemplateInstantiationMemberFunctionDecl
            *originalTemplateMemberFunctionInst =
                isSgTemplateInstantiationMemberFunctionDecl(originalNode)) {
      SgTemplateInstantiationMemberFunctionDecl
          *copyTemplateMemberFunctionInst =
              isSgTemplateInstantiationMemberFunctionDecl(copyNode);
      ROSE_ASSERT(copyTemplateMemberFunctionInst != NULL);

      if (SgTemplateMemberFunctionDeclaration *copiedTemplateDecl =
              isSgTemplateMemberFunctionDeclaration(
                  lookupCopiedNode(help, originalTemplateMemberFunctionInst
                                             ->get_templateDeclaration()))) {
        copyTemplateMemberFunctionInst->set_templateDeclaration(
            copiedTemplateDecl);
      }
    }

    if (const SgEnumDeclaration *originalEnum =
            isSgEnumDeclaration(originalNode)) {
      SgEnumDeclaration *copyEnum = isSgEnumDeclaration(copyNode);
      ROSE_ASSERT(copyEnum != NULL);

      SgInitializedNamePtrList &copyEnumerators = copyEnum->get_enumerators();
      for (SgInitializedNamePtrList::iterator field = copyEnumerators.begin();
           field != copyEnumerators.end();) {
        SgInitializedName *copyField = *field;
        if (copyField != NULL &&
            canonicalCopies.find(copyField) == canonicalCopies.end()) {
          if (copyField->get_parent() == copyEnum) {
            copyField->set_parent(NULL);
          }
          field = copyEnumerators.erase(field);
          continue;
        }

        ++field;
      }

      canonicalizeCopiedEnumDeclarationChain(originalEnum, copyEnum, help);
    }

    if (SgScopeStatement *copyScope = isSgScopeStatement(copyNode)) {
      switch (copyScope->variantT()) {
      case V_SgBasicBlock: {
        SgBasicBlock *block = isSgBasicBlock(copyScope);
        ROSE_ASSERT(block != NULL);

        SgStatementPtrList &statementList = block->get_statements();
        std::unordered_set<SgStatement *> seenStatements;
        for (SgStatementPtrList::iterator stmt = statementList.begin();
             stmt != statementList.end();) {
          SgStatement *copyStmt = *stmt;
          if (copyStmt != NULL && !seenStatements.insert(copyStmt).second) {
            stmt = statementList.erase(stmt);
            continue;
          }

          SgDeclarationStatement *copyDecl = isSgDeclarationStatement(copyStmt);
          if (copyDecl != NULL &&
              canonicalCopies.find(copyDecl) == canonicalCopies.end()) {
            if (isTrackedEmptyDeclaration(copyDecl)) {
              fprintf(stderr,
                      "[copy-empty] finalize erase block copy=%p parent=%p "
                      "scope=%p\n",
                      copyDecl, copyDecl->get_parent(), copyDecl->get_scope());
              fflush(stderr);
            }
            help.noteSupersededCopiedNode(copyDecl);
            if (copyDecl->get_parent() == copyScope) {
              copyDecl->set_parent(NULL);
            }
            stmt = statementList.erase(stmt);
            continue;
          }

          ++stmt;
        }
        break;
      }

      case V_SgClassDefinition:
      case V_SgTemplateInstantiationDefn: {
        SgClassDefinition *classDef = isSgClassDefinition(copyScope);
        ROSE_ASSERT(classDef != NULL);

        SgDeclarationStatementPtrList &memberList = classDef->get_members();
        std::unordered_set<SgDeclarationStatement *> seenDeclarations;
        for (SgDeclarationStatementPtrList::iterator stmt = memberList.begin();
             stmt != memberList.end();) {
          SgDeclarationStatement *copyDecl = *stmt;
          if (copyDecl != NULL && !seenDeclarations.insert(copyDecl).second) {
            stmt = memberList.erase(stmt);
            continue;
          }
          if (canonicalCopies.find(copyDecl) == canonicalCopies.end()) {
            if (isTrackedEmptyDeclaration(copyDecl)) {
              fprintf(stderr,
                      "[copy-empty] finalize erase member copy=%p parent=%p "
                      "scope=%p\n",
                      copyDecl, copyDecl->get_parent(), copyDecl->get_scope());
              fflush(stderr);
            }
            help.noteSupersededCopiedNode(copyDecl);
            if (copyDecl->get_parent() == copyScope) {
              copyDecl->set_parent(NULL);
            }
            stmt = memberList.erase(stmt);
            continue;
          }

          ++stmt;
        }
        break;
      }

      case V_SgGlobal: {
        SgGlobal *global = isSgGlobal(copyScope);
        ROSE_ASSERT(global != NULL);

        SgDeclarationStatementPtrList &decls = global->get_declarations();
        std::unordered_set<SgDeclarationStatement *> seenDeclarations;
        for (SgDeclarationStatementPtrList::iterator stmt = decls.begin();
             stmt != decls.end();) {
          SgDeclarationStatement *copyDecl = *stmt;
          if (copyDecl != NULL && !seenDeclarations.insert(copyDecl).second) {
            stmt = decls.erase(stmt);
            continue;
          }
          if (canonicalCopies.find(copyDecl) == canonicalCopies.end()) {
            if (isTrackedEmptyDeclaration(copyDecl)) {
              fprintf(stderr,
                      "[copy-empty] finalize erase global copy=%p parent=%p "
                      "scope=%p\n",
                      copyDecl, copyDecl->get_parent(), copyDecl->get_scope());
              fflush(stderr);
            }
            help.noteSupersededCopiedNode(copyDecl);
            if (copyDecl->get_parent() == copyScope) {
              copyDecl->set_parent(NULL);
            }
            stmt = decls.erase(stmt);
            continue;
          }

          ++stmt;
        }
        break;
      }

      case V_SgNamespaceDefinitionStatement: {
        SgNamespaceDefinitionStatement *namespaceDef =
            isSgNamespaceDefinitionStatement(copyScope);
        ROSE_ASSERT(namespaceDef != NULL);

        SgDeclarationStatementPtrList &decls = namespaceDef->get_declarations();
        std::unordered_set<SgDeclarationStatement *> seenDeclarations;
        SgNamespaceDefinitionStatement *globalNamespaceDef =
            namespaceDef->get_global_definition();
        const bool isExtensionNamespaceDef =
            globalNamespaceDef != NULL && globalNamespaceDef != namespaceDef;
        for (SgDeclarationStatementPtrList::iterator stmt = decls.begin();
             stmt != decls.end();) {
          SgDeclarationStatement *copyDecl = *stmt;
          if (copyDecl != NULL && !seenDeclarations.insert(copyDecl).second) {
            stmt = decls.erase(stmt);
            continue;
          }
          const bool isNonCanonicalDeclaration =
              canonicalCopies.find(copyDecl) == canonicalCopies.end();
          const bool belongsToGlobalNamespaceDef =
              isExtensionNamespaceDef && copyDecl != NULL &&
              copyDecl->get_scope() == globalNamespaceDef;
          if (isNonCanonicalDeclaration || belongsToGlobalNamespaceDef) {
            if (isTrackedEmptyDeclaration(copyDecl)) {
              fprintf(stderr,
                      "[copy-empty] finalize erase namespace copy=%p parent=%p "
                      "scope=%p nonCanonical=%s globalDef=%s\n",
                      copyDecl,
                      copyDecl != NULL ? copyDecl->get_parent() : NULL,
                      copyDecl != NULL ? copyDecl->get_scope() : NULL,
                      isNonCanonicalDeclaration ? "true" : "false",
                      belongsToGlobalNamespaceDef ? "true" : "false");
              fflush(stderr);
            }
            if (isNonCanonicalDeclaration) {
              help.noteSupersededCopiedNode(copyDecl);
            }
            if (copyDecl->get_parent() == copyScope) {
              copyDecl->set_parent(NULL);
            }
            stmt = decls.erase(stmt);
            continue;
          }

          ++stmt;
        }
        break;
      }

      default:
        break;
      }
    }
  }

  canonicalizeCopiedFunctionDeclarationChains(help);
  repairCopiedFunctionDefinitionDeclarations(help);
  canonicalizeCopiedClassTypes(help);
  restoreOriginalClassTypes(help);
  repairCopiedDeclarationParentsFromScopes(help);
  repairCopiedExplicitDeclarationScopesFromParents(help);
  discardSupersededCopiedDeclarations(help, canonicalCopies);
  discardSupersededCopiedSymbols(help, canonicalCopies);
  normalizeSelfDefiningCopiedDeclarations(help);
}

} // namespace

void SgCopyHelp::prepareRootCopyForFixup() {
  if (!hasNontrivialCopiedNodes(*this)) {
    return;
  }

  traceCopyFixupRoot("prepare", *this);
  canonicalizeCopiedNamespaceDeclarationMapEntries(*this);
  canonicalizeCopiedFunctionDeclarationMapEntries(*this);
  removeCopiedNodeMapAliasEntries(*this);
  removeDeletedCopiedNodeMapEntries(*this);
}

void SgCopyHelp::finalizeRootCopy() {
  if (!hasNontrivialCopiedNodes(*this)) {
    return;
  }

  traceCopyFixupRoot("finalize:start", *this);
  finalizeCanonicalCopyLinks(*this);
  traceCopyFixupRoot("finalize:done", *this);
}

void SgTreeCopy::prepareRootCopyForFixup() {
  SgCopyHelp::prepareRootCopyForFixup();
}

void SgTreeCopy::finalizeRootCopy() { SgCopyHelp::finalizeRootCopy(); }

void resetVariableDefinitionSupport(
    const SgInitializedName *originalInitializedName,
    SgInitializedName *copyInitializedName,
    SgDeclarationStatement *targetDeclaration) {
  // DQ (10/8/2007): This is s supporting function to the
  // SgInitializedName::fixupCopy_scopes() member function.

  // DQ (5/14/2012): Added simple test.
  ROSE_ASSERT(copyInitializedName != NULL);

  // Where the scope is reset, also build a new SgVariableDefinition (I forget
  // why).
  ROSE_ASSERT(copyInitializedName->get_declptr() != NULL);

  SgNode *originalDeclaration = originalInitializedName->get_declptr();
  switch (originalDeclaration->variantT()) {
  case V_SgVariableDefinition: {
    // Build a fresh variable-definition copy for the initialized name.  In some
    // Fortran forms (e.g., copied procedure parameters), callers can provide a
    // non-null target declaration, but it is not used for this declptr kind.

    SgVariableDefinition *variableDefinition_original =
        isSgVariableDefinition(originalInitializedName->get_declptr());
    ROSE_ASSERT(variableDefinition_original != NULL);

    // DQ (1/20/204): Moved to supporting the more general expression
    // (required), plus the expression from which it may have been generated.
    // SgUnsignedLongVal* bitfield =
    // variableDefinition_original->get_bitfield();
    SgExpression *bitfield = variableDefinition_original->get_bitfield();

    SgVariableDefinition *variableDefinition_copy =
        new SgVariableDefinition(copyInitializedName, bitfield);
    ROSE_ASSERT(variableDefinition_copy != NULL);
    copyInitializedName->set_declptr(variableDefinition_copy);
    variableDefinition_copy->set_parent(copyInitializedName);

    // This is the same way that Sg_File_Info objects are built in the copy
    // mechanism.
    Sg_File_Info *newStartOfConstruct = new Sg_File_Info(
        *(variableDefinition_original->get_startOfConstruct()));
    ROSE_ASSERT(variableDefinition_original->get_endOfConstruct() != NULL);
    Sg_File_Info *newEndOfConstruct =
        new Sg_File_Info(*(variableDefinition_original->get_endOfConstruct()));

    variableDefinition_copy->set_startOfConstruct(newStartOfConstruct);
    variableDefinition_copy->set_endOfConstruct(newEndOfConstruct);

    newStartOfConstruct->set_parent(variableDefinition_copy);
    newEndOfConstruct->set_parent(variableDefinition_copy);

    break;
  }

  case V_SgEnumDeclaration: {
    // In this case the target declaration is a SgEnumDeclaration, and it has
    // already been copied so it need not be created.
    ROSE_ASSERT(targetDeclaration != NULL);

    SgEnumDeclaration *enumDeclaration_original =
        isSgEnumDeclaration(originalInitializedName->get_declptr());
    if (copyInitializedName->get_declptr() == enumDeclaration_original) {
      // Then reset to the targetDeclaration.
      copyInitializedName->set_declptr(targetDeclaration);
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
    // In this case the target declaration is a SgFunctionDeclaration, and it
    // has already been copied so it need not be created.
    ROSE_ASSERT(targetDeclaration != NULL);

    SgFunctionDeclaration *functionDeclaration_original =
        isSgFunctionDeclaration(originalInitializedName->get_declptr());
    if (copyInitializedName->get_declptr() == functionDeclaration_original) {
      // Then reset to the targetDeclaration.
      copyInitializedName->set_declptr(targetDeclaration);
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

// DQ (10/5/2007): Added IR node specific function to permit copies, via AST
// copy(), to be fixedup Usually this will correct scopes and in a few cases
// build child IR nodes that are not traversed (and thus shared in the result
// from the automatically generated copy function).
void SgInitializedName::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  // Guard against shared nodes. When nodes are shared (not copied), this ==
  // copy.
  if (this == copy) {
    // Node is shared, not copied. Skip fixup to avoid corrupting the original.
    return;
  }

  // This is the empty default inplementation, not a problem if it is called!

#if DEBUG_FIXUP_COPY
  // printf ("Inside of SgInitializedName::fixupCopy_scopes() %p = %s
  // \n",this,SageInterface::get_name(this).c_str());
  printf("Inside of SgInitializedName::fixupCopy_scopes() %p = %s \n", this,
         this->get_name().str());
#endif

  // Need to fixup the scope and perhaps build the SgVariableDefinition object!
  SgInitializedName *initializedName_copy = isSgInitializedName(copy);
  ROSE_ASSERT(initializedName_copy != NULL);

  // DQ (4/21/2016): Replacing "__null" with more portable (non-gnu specific)
  // use using "NULL". ROSE_ASSERT (initializedName_copy->get_declptr() !=
  // __null);
  ROSE_ASSERT(initializedName_copy->get_declptr() != NULL);

  // fprintf(stderr, "SgInitializedName::fixupCopy_scopes(%p) this=%p\n", copy,
  // this); fprintf(stderr, "Copy's scope is %p, my scope is %p\n",
  // initializedName_copy->get_scope(), this->get_scope());

  // ROSE_ASSERT(this->get_symbol_from_symbol_table() != NULL);

  if (initializedName_copy->get_scope() == this->get_scope()) {
    FixupCopyDataMemberMacro(initializedName_copy, SgScopeStatement, get_scope,
                             set_scope)
    // fprintf(stderr, "After: copy's scope is %p, my scope is %p\n",
    // initializedName_copy->get_scope(), this->get_scope());
  } else {
#if DEBUG_FIXUP_COPY
    // fprintf (stderr, "Skipping resetting the scope for
    // initializedName_copy = %p = %s
    // \n",initializedName_copy,initializedName_copy->get_name().str());
    printf("Skipping resetting the scope for initializedName_copy = %p = %s \n",
           initializedName_copy, initializedName_copy->get_name().str());
#endif
  }

  // Fix up the associated declaration/definition regardless of whether the
  // scope pointer needed updating.  Some node kinds (notably enum
  // enumerators) can have their scope copied correctly while still holding a
  // declptr that points back into the original AST, which breaks
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
      resetVariableDefinitionSupport(this, initializedName_copy, NULL);
      break;
    }

    case V_SgEnumDeclaration: {
      SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(parent);
      ROSE_ASSERT(enumDeclaration != NULL);
      resetVariableDefinitionSupport(this, initializedName_copy,
                                     enumDeclaration);
      break;
    }

    case V_SgFunctionParameterList: {
      SgNode *parentFunction = parent->get_parent();
      SgFunctionDeclaration *functionDeclaration =
          isSgFunctionDeclaration(parentFunction);

      // The parent of the SgFunctionParameterList might not have been set
      // yet, so allow for this!
      if (functionDeclaration != NULL) {
        // DQ (5/14/2012): Added simple test.
        ROSE_ASSERT(initializedName_copy != NULL);

        // DQ (5/14/2012): Added simple test, required in
        // resetVariableDefinitionSupport().
        ROSE_ASSERT(initializedName_copy->get_declptr() != NULL);

        resetVariableDefinitionSupport(this, initializedName_copy,
                                       functionDeclaration);
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
                                       memberFunctionDeclaration);
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
  // variable declaration sets the parent of the SgVariableDeclaration and we
  // need this parent in the fixupCopy function in the SgInitializedName.

#if DEBUG_FIXUP_COPY
  printf("Inside of SgStatement::fixupCopy_scopes() for %p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgLocatedNode::fixupCopy_scopes(copy, help);
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
// build child IR nodes that are not traversed (and thus shared in the result
// from the automatically generated copy function).
void SgLocatedNode::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  // Guard against shared nodes. When nodes are shared (not copied), this ==
  // copy. Processing a shared node would corrupt the original AST by setting
  // its pointers to the copy's values.
  if (this == copy) {
    // Node is shared, not copied. Skip fixup to avoid corrupting the original.
    return;
  }

#if DEBUG_FIXUP_COPY
  printf("Inside of SgLocatedNode::fixupCopy_scopes() for %p = %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgLocatedNode *copyLocatedNode = isSgLocatedNode(copy);
  ROSE_ASSERT(copyLocatedNode != NULL);

  // DQ (10/24/2007): New test.
  ROSE_ASSERT(copyLocatedNode->variantT() == this->variantT());

  // DQ (7/15/2007): Added assertion...
  // ROSE_ASSERT(this->get_parent() != NULL);
  // ROSE_ASSERT(copyStatement->get_parent() != NULL);

  // DQ (10/15/2007): If the parent of the original AST is not set then we
  // will not process the parent in the copy, thus the AST copy mechanism can
  // handle incompletely setup AST (as required for use in the legacy
  // frontend/Sage translation) yet only return an AST of similar quality.
  if (this->get_parent() != NULL) {
    FixupCopyDataMemberMacro(copyLocatedNode, SgNode, get_parent, set_parent)

        // Debugging information
        SgNode *cpParent = copyLocatedNode->get_parent();
    if (cpParent == NULL) {
#if PRINT_DEVELOPER_WARNINGS
      printf("In SgLocatedNode::fixupCopy_scopes(): this->get_parent() != "
             "NULL, but copyLocatedNode->get_parent() == NULL for "
             "copyLocatedNode = %p = %s \n",
             copyLocatedNode, copyLocatedNode->class_name().c_str());
      printf("     this                        = %p = %s \n", this,
             this->class_name().c_str());
      printf("     this->get_parent()          = %p = %s \n",
             this->get_parent(), this->get_parent()->class_name().c_str());
#endif
    }

    // Debugging information
    if (cpParent != NULL &&
        cpParent->variantT() != this->get_parent()->variantT()) {
#if PRINT_DEVELOPER_WARNINGS
      printf("Warning: In SgLocatedNode::fixupCopy_scopes(): the parent of "
             "this and copyStatement are different \n");
      printf("     this                        = %p = %s \n", this,
             this->class_name().c_str());
      printf("     copyLocatedNode             = %p = %s \n", copyLocatedNode,
             copyLocatedNode->class_name().c_str());
      printf("     this->get_parent()          = %p = %s \n",
             this->get_parent(), this->get_parent()->class_name().c_str());
      printf("     copyStatement->get_parent() = %p = %s \n", cpParent,
             cpParent->class_name().c_str());

      this->get_startOfConstruct()->display(
          "this->get_startOfConstruct(): debug");
#endif
    }
    // ROSE_ASSERT(copyStatement->get_parent()->variantT() ==
    // this->get_parent()->variantT());
  } else if (SgProject::get_verbose() > 0) {
    printf("In SgLocatedNode::fixupCopy_scopes(): parent not set for original "
           "AST at %p = %s, thus copy left similarly incomplete \n",
           this, this->class_name().c_str());
  }

  // DQ (2/20/2009): Added assertion, I think it is up to the parent node copy
  // function to set the parent in the copying of any children.
  // ROSE_ASSERT(copy->get_parent() != NULL);
  if (copy->get_parent() == NULL) {
    // Note that using SageInterface::get_name(this) will work where
    // SageInterface::get_name(copy) will fail because sometimes the parent
    // pointer is required to be valid within SageInterface::get_name() (e.g.
    // between SgFunctionParameterList and it's parent: SgFunctionDeclaration).
  }

#if DEBUG_FIXUP_COPY
  printf("Leaving SgLocatedNode::fixupCopy_scopes() \n\n");
#endif
}

void SgScopeStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  // Guard against shared nodes. When nodes are shared (not copied), this ==
  // copy.
  if (this == copy) {
    // Node is shared, not copied. Skip fixup to avoid corrupting the original.
    return;
  }

  // We need to call the fixupCopy function from the parent of a
  // SgVariableDeclaration because the copy function in the parent of the
  // variable declaration sets the parent of the SgVariableDeclaration and we
  // need this parent in the fixupCopy function in the SgInitializedName.

#if DEBUG_FIXUP_COPY
  printf(
      "Inside of SgScopeStatement::fixupCopy_scopes() for %p = %s copy = %p \n",
      this, this->class_name().c_str(), copy);
#endif

  // printf ("\nInside of SgScopeStatement::fixupCopy_scopes() for %p = %s copy
  // = %p \n\n",this,this->class_name().c_str(),copy);
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

  // The symbol table should not have been setup yet!

  // DQ (5/21/2013): Restrict direct access to the symbol table.
  // if (copyScopeStatement->get_symbol_table()->size() != 0)
  if (copyScopeStatement->symbol_table_size() != 0) {
  }

  // DQ (2/6/2009): Comment this out since it fails for the case of the
  // reverseTraversal tests.
  // ROSE_ASSERT(copyScopeStatement->get_symbol_table()->size() == 0);

  // Call the base class fixupCopy member function
  SgStatement::fixupCopy_scopes(copy, help);

  // printf ("\nLeaving SgScopeStatement::fixupCopy_scopes() for %p = %s copy =
  // %p \n\n",this,this->class_name().c_str(),copy);
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
  synchronizeCopiedDeclarationList(statementList_original, statementList_copy,
                                   global_copy, help);
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

  // printf ("\nLeaving SgGlobal::fixupCopy_scopes() this = %p = %s  copy = %p
  // \n",this,this->class_name().c_str(),copy);
}

// JJW 2/1/2008 -- Added support to fixup statement expressions
void SgExprStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf(
      "Inside of SgExprStatement::fixupCopy_scopes() for %p = %s copy = %p \n",
      this, this->class_name().c_str(), copy);
#endif

  SgExprStatement *es_copy = isSgExprStatement(copy);
  ROSE_ASSERT(es_copy != NULL);

  SgExpression *expression_original = this->get_expression();
  SgExpression *expression_copy = es_copy->get_expression();

  expression_original->fixupCopy_scopes(expression_copy, help);

  // Call the base class fixupCopy member function
  SgStatement::fixupCopy_scopes(copy, help);

  // printf ("\nLeaving SgExprStatement::fixupCopy_scopes() this = %p = %s  copy
  // = %p \n",this,this->class_name().c_str(),copy);
}

// DQ (10/6/2007): Added fixup function to set scopes not set properly by the
// ROSETTA generated copy!
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

  // printf ("\nLeaving SgBasicBlock::fixupCopy_scopes() this = %p = %s  copy =
  // %p \n",this,this->class_name().c_str(),copy);
}

void SgDeclarationStatement::fixupCopy_scopes(SgNode *copy,
                                              SgCopyHelp &help) const {
  // Guard against shared nodes. When nodes are shared (not copied), this ==
  // copy. Processing a shared node would corrupt the original AST by setting
  // its pointers to the copy's values. This can happen for SgClassDefinition
  // nodes from non-defining declarations, which are shared by
  // SgTreeCopy::copyAst().
  if (this == copy) {
    // Node is shared, not copied. Skip fixup to avoid corrupting the original.
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

  // Normalize declaration-chain links before any legacy fixup path assumes they
  // are already wired to copied declarations.
  repairCopiedDeclarationChainLinks(this, copyDeclarationStatement, help);
  repairCopiedNamespaceScopeDeclarationChains(this, copyDeclarationStatement,
                                              help);

  // DQ (10/20/2007): This is an essential piece of the fixup of the AST copy.
  // This is a recursive construction of IR nodes that have defining and
  // non-defining parts used when either or both were not copied in the initial
  // call to the copy member functions. Since we can visit the IR nodes in any
  // order, and some declaration can have multiple parts (defining and
  // non-defining declaration) We have to make sure that these are processed
  // first (in some cases they may have escaped being copied by the AST Copy
  // mechanism when they were not part of the traversed AST). So we have to
  // build them if they don't exist (triggered by existance in the copy map).

  // Check if this IR node has been copied?  If so then it is in the copy map.
  // bool hasBeenCopied = (help.get_copiedNodeMap().find(this) !=
  // help.get_copiedNodeMap().end()); printf ("hasBeenCopied = %s
  // \n",hasBeenCopied ? "true" : "false"); if (hasBeenCopied == false)

  bool definingDeclarationCopied =
      declarationFixupAvailable(this, this->get_definingDeclaration(), help);

  bool firstNondefiningDeclarationCopied = declarationFixupAvailable(
      this, this->get_firstNondefiningDeclaration(), help);

  // DQ (3/2/2009): Handle the case of friend declaration.
  bool isFriendDeclaration = this->get_declarationModifier().isFriend();

  // DQ (3/2/2009): Modified to exclude copying of friend declaration defining
  // and non defining declarations. if (definingDeclarationCopied == true &&
  // firstNondefiningDeclarationCopied == true)
  if ((definingDeclarationCopied == true &&
       firstNondefiningDeclarationCopied == true) ||
      (isFriendDeclaration == true)) {
    // We can process the current non-defining declaration (which is not the
    // first non-defining declaration)
  } else {
    // Note sure if we need these variables
    // SgDeclarationStatement* copyOfDefiningDeclaration         = NULL;
    // SgDeclarationStatement* copyOfFirstNondefiningDeclaration = NULL;

    // Note: This will cause the first non-defining declaration to be copied
    // first when neither have been processed. if
    // (this->get_firstNondefiningDeclaration() != NULL &&
    // firstNondefiningDeclarationCopied == false) if
    // (this->get_firstNondefiningDeclaration() != NULL &&
    // firstNondefiningDeclarationCopied == false && this ==
    // this->get_definingDeclaration()) if
    // (this->get_firstNondefiningDeclaration() != NULL &&
    // firstNondefiningDeclarationCopied == false && this ==
    // this->get_firstNondefiningDeclaration()) if
    // (this->get_firstNondefiningDeclaration() != NULL &&
    // firstNondefiningDeclarationCopied == false && this ==
    // this->get_firstNondefiningDeclaration())
    if (this->get_firstNondefiningDeclaration() != NULL &&
        firstNondefiningDeclarationCopied == false) {
      SgDeclarationStatement *resolvedFirstNondefiningCopy = NULL;
      if (this->get_firstNondefiningDeclaration()->get_parent() == NULL) {
        resolvedFirstNondefiningCopy = resolveCopiedReferencedDeclaration(
            this->get_firstNondefiningDeclaration(), help);
      }

      if (resolvedFirstNondefiningCopy != NULL) {
        copyDeclarationStatement->set_firstNondefiningDeclaration(
            resolvedFirstNondefiningCopy);
      } else {
        SgNode *fallbackCopiedFirstNondefiningParent =
            copyDeclarationStatement->get_parent();
        if (fallbackCopiedFirstNondefiningParent == NULL) {
          fallbackCopiedFirstNondefiningParent =
              lookupCopiedNode(help, this->get_parent());
        }

        // Setup the firstNondefining declaration
        // ROSE_ASSERT(definingDeclarationCopied == true);

        // We could build vitual constructors, but then that is what the copy
        // function is so call copy! This also added the
        // firstNondefiningDeclaration to the copy map

        // DQ (10/21/2007): Use the copy help object so that it can control
        // copying of defining vs. non-defining declaration. SgNode*
        // copyOfFirstNondefiningDeclarationNode =
        // this->get_firstNondefiningDeclaration()->copy(help); printf ("Nested
        // copy of firstNondefiningDeclaration \n");
        SgNode *existingCopyOfFirstNondefiningDeclarationNode =
            lookupCopiedNode(help, this->get_firstNondefiningDeclaration());
        SgNode *copyOfFirstNondefiningDeclarationNode = copyNodeIntoCurrentRoot(
            help, this->get_firstNondefiningDeclaration());
        if (isSgDeclarationStatement(copyOfFirstNondefiningDeclarationNode) ==
            NULL) {
          if (SgDeclarationStatement *resolvedFirstNondefiningCopy =
                  resolveCopiedReferencedDeclaration(
                      this->get_firstNondefiningDeclaration(), help)) {
            copyOfFirstNondefiningDeclarationNode =
                resolvedFirstNondefiningCopy;
          }
        }
        // printf ("DONE: Nested copy of firstNondefiningDeclaration \n");

        ROSE_ASSERT(copyOfFirstNondefiningDeclarationNode != NULL);
        if (copyOfFirstNondefiningDeclarationNode !=
            existingCopyOfFirstNondefiningDeclarationNode) {
          ROSE_ASSERT(copyOfFirstNondefiningDeclarationNode->get_parent() ==
                      NULL);

          // Must reset the parent (semantics of AST copy), but this will be
          // done by reset.
          SgNode *copiedParent = lookupCopiedNode(
              help, this->get_firstNondefiningDeclaration()->get_parent());
          if (copiedParent == NULL) {
            copiedParent = fallbackCopiedFirstNondefiningParent;
          }
          if (copiedParent != NULL) {
            copyOfFirstNondefiningDeclarationNode->set_parent(copiedParent);
          }
        } else if (copyOfFirstNondefiningDeclarationNode->get_parent() ==
                   NULL) {
          SgNode *copiedParent = lookupCopiedNode(
              help, this->get_firstNondefiningDeclaration()->get_parent());
          if (copiedParent == NULL) {
            copiedParent = fallbackCopiedFirstNondefiningParent;
          }
          if (copiedParent != NULL) {
            copyOfFirstNondefiningDeclarationNode->set_parent(copiedParent);
          }
        }

        // DQ (3/14/2014): The parent might not be set if the non-defining
        // declaration has not be added to the AST (i.e. if it has only been
        // used to build a symbol).
        // ROSE_ASSERT(copyOfFirstNondefiningDeclarationNode->get_parent() !=
        // NULL);
        // printf ("Commented out setting of scopes on the
        // this->get_firstNondefiningDeclaration() \n");
        this->get_firstNondefiningDeclaration()->fixupCopy_scopes(
            copyOfFirstNondefiningDeclarationNode, help);

        // copyOfFirstNondefiningDeclaration =
        // isSgDeclarationStatement(copyOfFirstNondefiningDeclarationNode);
        SgDeclarationStatement *copyOfFirstNondefiningDeclaration =
            isSgDeclarationStatement(copyOfFirstNondefiningDeclarationNode);
        ROSE_ASSERT(copyOfFirstNondefiningDeclaration != NULL);
        copyDeclarationStatement->set_firstNondefiningDeclaration(
            copyOfFirstNondefiningDeclaration);
      }
    } else {
      // Note: This needs to be in the else case to handle the recursion
      // properly (else this case would be procesed twice)
      if (this->get_definingDeclaration() != NULL &&
          definingDeclarationCopied == false) {
        SgDeclarationStatement *resolvedDefiningCopy = NULL;
        if (this->get_definingDeclaration()->get_parent() == NULL) {
          resolvedDefiningCopy = resolveCopiedReferencedDeclaration(
              this->get_definingDeclaration(), help);
        }

        if (resolvedDefiningCopy != NULL) {
          copyDeclarationStatement->set_definingDeclaration(
              resolvedDefiningCopy);
        } else {
          SgNode *fallbackCopiedDefiningParent =
              copyDeclarationStatement->get_parent();
          if (fallbackCopiedDefiningParent == NULL) {
            fallbackCopiedDefiningParent =
                lookupCopiedNode(help, this->get_parent());
          }

          // DQ (2/19/2009): I don't think that the
          // firstNondefiningDeclarationCopied has to be true (here we are
          // copying the defining declaration). Setup the defining declaration
          // ROSE_ASSERT(firstNondefiningDeclarationCopied == true);
          // DQ (10/21/2007): Use the copy help object so that it can control
          // copying of defining vs. non-defining declaration. SgNode*
          // copyOfDefiningDeclarationNode =
          // this->get_definingDeclaration()->copy(help);
          SgNode *existingCopyOfDefiningDeclarationNode =
              lookupCopiedNode(help, this->get_definingDeclaration());
          SgNode *copyOfDefiningDeclarationNode =
              copyNodeIntoCurrentRoot(help, this->get_definingDeclaration());
          if (isSgDeclarationStatement(copyOfDefiningDeclarationNode) == NULL) {
            if (SgDeclarationStatement *resolvedDefiningCopy =
                    resolveCopiedReferencedDeclaration(
                        this->get_definingDeclaration(), help)) {
              copyOfDefiningDeclarationNode = resolvedDefiningCopy;
            }
          }
          ROSE_ASSERT(copyOfDefiningDeclarationNode != NULL);

          // If we didn't make a copy of the definingDeclaration then this is
          // still a valid pointer, so there is no need to reset the parent or
          // call
          if (copyOfDefiningDeclarationNode !=
              this->get_definingDeclaration()) {
            if (copyOfDefiningDeclarationNode !=
                existingCopyOfDefiningDeclarationNode) {
              // DQ (2/26/2009): Set the parent to NULL (before resetting to
              // valid value).
              copyOfDefiningDeclarationNode->set_parent(NULL);

              ROSE_ASSERT(copyOfDefiningDeclarationNode->get_parent() == NULL);

              if (this->get_definingDeclaration()->get_parent() == NULL) {
                printf("ERROR: this = %p = %s = %s "
                       "this->get_definingDeclaration() = %p \n",
                       this, this->class_name().c_str(),
                       SageInterface::get_name(this).c_str(),
                       this->get_definingDeclaration());
              }

              // Must reset the parent (semantics of AST copy), but this will be
              // done by reset.
              if (this->get_definingDeclaration()->get_parent() == NULL) {
                if (isSgTemplateFunctionDeclaration(
                        this->get_definingDeclaration()) != NULL ||
                    isSgTemplateMemberFunctionDeclaration(
                        this->get_definingDeclaration()) != NULL) {
                  printf(
                      "Warning: (inner scope) "
                      "this->get_definingDeclaration()->get_parent() == NULL "
                      "(OK for some SgTemplateFunctionDeclaration and "
                      "SgTemplateMemberFunctionDeclaration) \n");
                } else if (isSgTemplateClassDeclaration(
                               this->get_definingDeclaration()) != NULL) {
                  printf("WARNING: %p (%s) has no parent! \n",
                         this->get_definingDeclaration(),
                         this->get_definingDeclaration()->class_name().c_str());
                } else {
                  printf("ERROR: %p (%s) has no parent! \n",
                         this->get_definingDeclaration(),
                         this->get_definingDeclaration()->class_name().c_str());
                  ROSE_ASSERT(this->get_definingDeclaration()->get_parent() !=
                              NULL);
                }
              }

              SgNode *copiedParent = lookupCopiedNode(
                  help, this->get_definingDeclaration()->get_parent());
              if (copiedParent == NULL) {
                copiedParent = fallbackCopiedDefiningParent;
              }
              if (copiedParent != NULL) {
                copyOfDefiningDeclarationNode->set_parent(copiedParent);
              }
            } else if (copyOfDefiningDeclarationNode->get_parent() == NULL) {
              SgNode *copiedParent = lookupCopiedNode(
                  help, this->get_definingDeclaration()->get_parent());
              if (copiedParent == NULL) {
                copiedParent = fallbackCopiedDefiningParent;
              }
              if (copiedParent != NULL) {
                copyOfDefiningDeclarationNode->set_parent(copiedParent);
              }
            }
            // DQ (2/26/2009): This was valid code that was temporarily
            // commented out (turning it back on). DQ (10/21/2007): I think this
            // was a bug!
            // this->get_firstNondefiningDeclaration()->fixupCopy_scopes(copyOfDefiningDeclarationNode,help);
            this->get_definingDeclaration()->fixupCopy_scopes(
                copyOfDefiningDeclarationNode, help);
          }

          // DQ (9/10/2014): See not above.
          // DQ (2/20/2009): Added assertion!
          // ROSE_ASSERT(copyOfDefiningDeclarationNode->get_parent() != NULL);
          if (copyOfDefiningDeclarationNode->get_parent() == NULL) {
            printf("Warning: (outer scope) "
                   "this->get_definingDeclaration()->get_parent() == NULL (OK "
                   "for some SgTemplateFunctionDeclaration and "
                   "SgTemplateMemberFunctionDeclaration) \n");
          }
        }

        // copyOfDefiningDeclaration =
        // isSgDeclarationStatement(copyOfDefiningDeclarationNode);
      } else {
        // This is an acceptable case, but only if:
        ROSE_ASSERT(this->get_definingDeclaration() == NULL ||
                    this->get_firstNondefiningDeclaration() == NULL);
      }
    }
  }

  // If this is a declaration which is a defining declaration, then the copy
  // should be as well.
  if (this->get_definingDeclaration() == this) {
    copyDeclarationStatement->set_definingDeclaration(copyDeclarationStatement);

    // ISSUE-107 FIX: ROOT CAUSE
    // When copying a defining declaration, we MUST ensure the copy's
    // firstNondefiningDeclaration is correctly set. It should point to the
    // COPY of the original's firstNondefiningDeclaration.
    if (copyDeclarationStatement->get_firstNondefiningDeclaration() == NULL) {
      SgDeclarationStatement *originalFirstNonDef =
          this->get_firstNondefiningDeclaration();

      if (originalFirstNonDef) {
        SgNode *copyOfFirstNonDef = NULL;
        if (help.get_copiedNodeMap().find(originalFirstNonDef) !=
            help.get_copiedNodeMap().end()) {
          copyOfFirstNonDef = help.get_copiedNodeMap()[originalFirstNonDef];
        }

        // If the original's firstNondef is in the map (it should be!), use
        // it. If originalFirstNonDef == this, then copyOfFirstNonDef will be
        // copyDeclarationStatement (correct).
        if (copyOfFirstNonDef) {
          SgDeclarationStatement *copyDecl =
              isSgDeclarationStatement(copyOfFirstNonDef);
          ROSE_ASSERT(copyDecl != NULL);
          copyDeclarationStatement->set_firstNondefiningDeclaration(copyDecl);
        }
      }
    }
    if (this->get_firstNondefiningDeclaration() !=
        copyDeclarationStatement->get_firstNondefiningDeclaration()) {
      // printf ("This is the defining declaration, so we can reset the defining
      // declaration on the firstNondefiningDeclaration \n");
      FixupCopyDataMemberMacro(
          copyDeclarationStatement->get_firstNondefiningDeclaration(),
          SgDeclarationStatement, get_definingDeclaration,
          set_definingDeclaration)
    }
  } else {
    // DQ (10/19/2007): If there was a defining declaration then the copy's
    // defining declaration using the map printf ("NOT a DEFINING declaration:
    // this->get_definingDeclaration() = %p
    // \n",this->get_definingDeclaration());
    if (this->get_definingDeclaration() != NULL) {
      ROSE_ASSERT(copyDeclarationStatement->get_definingDeclaration() != NULL);

      FixupCopyDataMemberMacro(copyDeclarationStatement, SgDeclarationStatement,
                               get_definingDeclaration, set_definingDeclaration)
    } else {
      // printf ("In SgDeclarationStatement::fixupCopy_scopes():
      // this->get_definingDeclaration() == NULL \n");
    }
  }

  // DQ (10/12/2007): It is not always clear if this is a great idea.  This
  // uncovered a bug in the SageInterface::isOverloaded() function. Having two
  // declarations marked as the firstNondefiningDeclaration could be a problem
  // at some point.  But for now this preserves the concept of an exact copy, so
  // I am leaving it as is.

  // If this is a declaration which is a nondefining declaration, then the copy
  // should be as well. if (this->get_firstNondefiningDeclaration() == this)
  if (this->get_firstNondefiningDeclaration() == this) {
    // printf ("This is the FIRST-NON-DEFINING declaration this %p = %s
    // copyDeclarationStatement = %p = %s
    // \n",this,this->class_name().c_str(),copyDeclarationStatement,copyDeclarationStatement->class_name().c_str());
    copyDeclarationStatement->set_firstNondefiningDeclaration(
        copyDeclarationStatement);

    // If this is the firstNondefining declaration the we can reset the
    // firstNondefiningDeclaration on the definingDeclaration (if it has been
    // updated)
    if (this->get_definingDeclaration() !=
        copyDeclarationStatement->get_definingDeclaration()) {
      // printf ("This is the firstNondefining declaration, so  we can reset the
      // firstNondefiningDeclaration on the definingDeclaration \n");
      FixupCopyDataMemberMacro(
          copyDeclarationStatement->get_definingDeclaration(),
          SgDeclarationStatement, get_firstNondefiningDeclaration,
          set_firstNondefiningDeclaration)
    }
  } else {
    // DQ (10/19/2007): If there was a defining declaration then the copy's
    // defining declaration using the map printf ("NOT a FIRST_NON_DEFINING
    // declaration: this->get_firstNondefiningDeclaration() = %p
    // \n",this->get_firstNondefiningDeclaration()); if
    // (this->get_firstNondefiningDeclaration() != NULL)
    if (this->get_firstNondefiningDeclaration() != NULL &&
        firstNondefiningDeclarationCopied == true) {
      ROSE_ASSERT(copyDeclarationStatement->get_firstNondefiningDeclaration() !=
                  NULL);
      FixupCopyDataMemberMacro(copyDeclarationStatement, SgDeclarationStatement,
                               get_firstNondefiningDeclaration,
                               set_firstNondefiningDeclaration)
    } else {
      // printf ("In SgDeclarationStatement::fixupCopy_scopes():
      // this->get_firstNondefiningDeclaration() == NULL \n");
    }
  }

  repairCopiedDeclarationChainLinks(this, copyDeclarationStatement, help);

  // DQ (10/16/2007): Added assertion (see copytest2007_17.C)
  if (this->get_definingDeclaration() != NULL &&
      this->get_firstNondefiningDeclaration() != NULL) {
    // printf ("this->get_definingDeclaration()         = %p = %s
    // \n",this->get_definingDeclaration(),this->get_definingDeclaration()->class_name().c_str());
    // printf ("this->get_firstNondefiningDeclaration() = %p = %s
    // \n",this->get_firstNondefiningDeclaration(),this->get_firstNondefiningDeclaration()->class_name().c_str());
    ROSE_ASSERT(this->get_definingDeclaration()->variantT() ==
                this->get_firstNondefiningDeclaration()->variantT());
  }

  // DQ (10/16/2007): Added assertion (see copytest2007_17.C)
  if (copyDeclarationStatement->get_definingDeclaration() != NULL &&
      copyDeclarationStatement->get_firstNondefiningDeclaration() != NULL) {
    // printf ("copyDeclarationStatement->get_definingDeclaration()         = %p
    // = %s
    // \n",copyDeclarationStatement->get_definingDeclaration(),copyDeclarationStatement->get_definingDeclaration()->class_name().c_str());
    // printf ("copyDeclarationStatement->get_firstNondefiningDeclaration() = %p
    // = %s
    // \n",copyDeclarationStatement->get_firstNondefiningDeclaration(),copyDeclarationStatement->get_firstNondefiningDeclaration()->class_name().c_str());
    ROSE_ASSERT(
        copyDeclarationStatement->get_definingDeclaration()->variantT() ==
        copyDeclarationStatement->get_firstNondefiningDeclaration()
            ->variantT());
  }

  repairCopiedOwnedDeclarationScope(this, copyDeclarationStatement, help);
  repairCopiedNonrealDeclScope(isSgNonrealDecl(this),
                               isSgNonrealDecl(copyDeclarationStatement), help);

  if (isTrackedEmptyDeclaration(this)) {
    fprintf(stderr,
            "[copy-empty] declaration-fixup this=%p copy=%p parent=%p "
            "scope=%p defining=%p firstNondef=%p\n",
            this, copyDeclarationStatement,
            copyDeclarationStatement->get_parent(),
            copyDeclarationStatement->get_scope(),
            copyDeclarationStatement->get_definingDeclaration(),
            copyDeclarationStatement->get_firstNondefiningDeclaration());
    fflush(stderr);
  }

  // DQ (10/12/2007): Set the scope for those SgDeclarationStatements which
  // store their scope explicitly. printf ("this->hasExplicitScope() = %s
  // \n",this->hasExplicitScope() ? "true" : "false");
  if (this->hasExplicitScope() == true) {
    // Check if this is an orphaned copy (created during fixup but not in the
    // traversable AST). An orphaned copy either has:
    // 1. A NULL parent (hasn't been attached to any parent yet), or
    // 2. Its parent NOT being a copied node (i.e., the parent is NOT a VALUE
    // in the copy map) This happens when fixupCopy_scopes creates a nested
    // copy of a defining/non-defining declaration and either leaves its
    // parent NULL or sets it to the original's parent (see lines 616 and 682
    // in this file). If the copy's parent is not a copied node, we should
    // NOT update the scope, because the orphaned copy won't be deleted by
    // deepDelete and would have a stale scope pointer.
    SgNode *copyParent = copyDeclarationStatement->get_parent();
    // Check if copy's parent is a copied node (i.e., it's a VALUE in the
    // copy map) This determines if the copy is part of the proper copy tree
    // that will be deleted.
    bool copyParentIsACopiedNode = isCopiedNodeValue(help, copyParent);
    bool isOrphanedCopy = (copyParent == NULL || !copyParentIsACopiedNode);
#if DEBUG_FIXUP_COPY
    printf("  [scope fixup] this = %p, copy = %p, this->parent = %p, "
           "copy->parent = %p, copyParentIsACopiedNode = %s, isOrphanedCopy "
           "= %s \n",
           this, copyDeclarationStatement, this->get_parent(), copyParent,
           copyParentIsACopiedNode ? "true" : "false",
           isOrphanedCopy ? "true" : "false");
#endif
    if (SgScopeStatement *copiedScope = resolveCopiedExplicitDeclarationScope(
            this, copyDeclarationStatement, help)) {
      copyDeclarationStatement->set_scope(copiedScope);
    } else if (!isOrphanedCopy) {
      // printf ("Reset the scope of the copy for this = %p = %s
      // \n",this,this->class_name().c_str());
      FixupCopyDataMemberMacro(copyDeclarationStatement, SgScopeStatement,
                               get_scope, set_scope)
    }
#if DEBUG_FIXUP_COPY
    else {
      printf("Skipping scope fixup for orphaned declaration: this = %p = "
             "%s, copy = %p \n",
             this, this->class_name().c_str(), copy);
    }
#endif

    if (copyDeclarationStatement->get_scope() != NULL &&
        this->get_scope() != NULL &&
        copyDeclarationStatement->get_scope()->variantT() !=
            this->get_scope()->variantT()) {
      if (SgScopeStatement *copiedScope = resolveCopiedExplicitDeclarationScope(
              this, copyDeclarationStatement, help)) {
        fprintf(stderr, "Repairing copied explicit scope for %s to %p (%s)\n",
                SageInterface::get_name(this).c_str(), copiedScope,
                copiedScope->class_name().c_str());
        copyDeclarationStatement->set_scope(copiedScope);
      } else if (SgScopeStatement *mappedCopiedScope = isSgScopeStatement(
                     lookupCopiedNode(help, this->get_scope()))) {
        fprintf(stderr, "Repairing mapped scope for %s to %p (%s)\n",
                SageInterface::get_name(this).c_str(), mappedCopiedScope,
                mappedCopiedScope->class_name().c_str());
        copyDeclarationStatement->set_scope(mappedCopiedScope);
      }
    }

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

    // DQ (2025): Skip file consistency checks for orphaned declarations
    // since they still reference the original scope. DQ (2/28/2009): Make
    // sure that the declaration and the copy are in the same file.
    // ROSE_ASSERT(SageInterface::getEnclosingSourceFile(copyDeclarationStatement)
    // == SageInterface::getEnclosingSourceFile(this));
    // ROSE_ASSERT(SageInterface::getEnclosingSourceFile(this->get_firstNondefiningDeclaration())
    // == SageInterface::getEnclosingSourceFile(this));
    if (!isOrphanedCopy && this->get_definingDeclaration() != NULL) {
      // DQ (3/4/2009): This test fails for copytest2007_34.C
      if (SageInterface::getEnclosingSourceFile(
              this->get_definingDeclaration()) !=
          SageInterface::getEnclosingSourceFile(this)) {
        printf("Warning: "
               "SageInterface::getEnclosingSourceFile(this->get_"
               "definingDeclaration()) != "
               "SageInterface::getEnclosingSourceFile(this) \n");
        printf("Commented out failing test for copytest2007_34.C \n");
      }
      // ROSE_ASSERT(SageInterface::getEnclosingSourceFile(this->get_definingDeclaration())
      // == SageInterface::getEnclosingSourceFile(this));
    }

    // DQ (3/2/2009): Make sure this is not the non-defining declaration since
    // the defining declaration will not have been copied yet and so of course
    // the files will not match.
    // ROSE_ASSERT(SageInterface::getEnclosingSourceFile(copyDeclarationStatement->get_firstNondefiningDeclaration())
    // == SageInterface::getEnclosingSourceFile(copyDeclarationStatement)); if
    // (copyDeclarationStatement->get_definingDeclaration() != NULL)

    // Also skip for orphaned declarations.
    if (!isOrphanedCopy &&
        copyDeclarationStatement->get_definingDeclaration() != NULL &&
        this != this->get_firstNondefiningDeclaration()) {
      // ROSE_ASSERT(SageInterface::getEnclosingSourceFile(copyDeclarationStatement->get_definingDeclaration())
      // == SageInterface::getEnclosingSourceFile(copyDeclarationStatement));
      if (SageInterface::getEnclosingSourceFile(
              copyDeclarationStatement->get_definingDeclaration()) !=
          SageInterface::getEnclosingSourceFile(copyDeclarationStatement)) {
        printf("copyDeclarationStatement = %p = %s \n",
               copyDeclarationStatement,
               copyDeclarationStatement->class_name().c_str());
        printf(
            "############# Detected case of "
            "copyDeclarationStatement->get_definingDeclaration() in file %s \n",
            SageInterface::getEnclosingSourceFile(
                copyDeclarationStatement->get_definingDeclaration())
                ->getFileName()
                .c_str());
        printf("############# Detected case of copyDeclarationStatement in "
               "file %s \n",
               SageInterface::getEnclosingSourceFile(copyDeclarationStatement)
                   ->getFileName()
                   .c_str());

        // This is not what we want here!
        // copyDeclarationStatement->set_definingDeclaration(NULL);

        printf(
            "Error: source files don't match for copyDeclarationStatement = %p "
            "and copyDeclarationStatement->get_definingDeclaration() = %p \n",
            copyDeclarationStatement,
            copyDeclarationStatement->get_definingDeclaration());
        ROSE_ABORT();
      }
    }
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
      // printf ("Test for same namespace is more complex: definingNamespace =
      // %p firstNondefiningNamespace = %p
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
        // this->get_startOfConstruct()->display("Error: this scope mismatch:
        // debug");
        // this->get_definingDeclaration()->get_startOfConstruct()->display("Error:
        // definingDeclaration scope mismatch: debug");
        // this->get_firstNondefiningDeclaration()->get_startOfConstruct()->display("Error:
        // firstNondefiningDeclaration scope mismatch: debug");
      }

      // DQ (2/19/2009): Make sure that these are the same kind of IR nodes
      // since they might be in different files (instead of in the same file).
      // ROSE_ASSERT(this->get_definingDeclaration()->get_scope() ==
      // this->get_firstNondefiningDeclaration()->get_scope() );
      if (this->get_definingDeclaration()->get_scope()->variantT() !=
          this->get_firstNondefiningDeclaration()->get_scope()->variantT()) {
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

    // We can't assert this yet since this is part of the copy of the defining
    // declaration within the processing of the non-defining declaration
    // (recurssively called!)
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

  // Call the base class fixupCopy member function (this will setup the parent)
  SgStatement::fixupCopy_scopes(copy, help);

  // DQ (2/20/2009): Note: These are allowed to be NULL a warning is issued in
  // SgLocatedNode::fixupCopy_scopes(). ROSE_ASSERT(copy->get_parent() != NULL);
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

  // printf ("\nIn SgFunctionDeclaration::fixupCopy_scopes(): for function = %s
  // = %p = %s copy = %p
  // \n",this->get_name().str(),this,this->class_name().c_str(),copy);

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  // Setup the scopes of the SgInitializedName objects in the paraleter list
  ROSE_ASSERT(get_parameterList() != NULL);
  get_parameterList()->fixupCopy_scopes(
      functionDeclaration_copy->get_parameterList(), help);
  if (functionDeclaration_copy->get_parameterList() != NULL &&
      functionDeclaration_copy->get_parameterList()->get_scope() == NULL &&
      functionDeclaration_copy->get_scope() != NULL) {
    functionDeclaration_copy->get_parameterList()->set_scope(
        functionDeclaration_copy->get_scope());
  }

  // Setup the details in the SgFunctionDefinition (this may have to rebuild the
  // sysmbol table) printf ("In SgFunctionDeclaration::fixupCopy_scopes():
  // this->get_definition() = %p \n",this->get_definition());
  if (this->get_definition() != NULL) {
    // DQ (3/15/2014): The defining declaration should not be marked
    // (isForward() == true).
    if (isForward() == true) {
      printf("Warning: The defining declaration should not be marked "
             "(isForward() == true) \n");
      printf("SgFunctionDeclaration::fixupCopy_scopes(): (isForward() == "
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

      // functionDeclaration_copy->get_file_info()->display("SgFunctionDeclaration::fixupCopy_scopes():
      // (isForward() == true): debug");

      // Reset this!
      functionDeclaration_copy->unsetForward();

      // DQ (3/15/2014): I don't want to change the original version of
      // the statement we are copying.
      printf("TODO: isForward() status is being reset for the "
             "original SgFunctionDeclaration as part of "
             "SgFunctionDeclaration::fixupCopy_scopes(): this = %p = "
             "%s = %s \n",
             this, this->class_name().c_str(), this->get_name().str());
      // this->unsetForward();
      this->get_definition()->get_declaration()->unsetForward();
    }
    ROSE_ASSERT(isForward() == false);

    // DQ (2/26/2009): Handle special cases where the copyHelp function is
    // non-trivial. Is every version of copyHelp object going to be a problem?

    // For the outlining, our copyHelp object does not copy defining function
    // declarations and substitutes a non-defining declarations, so if the copy
    // has been built this way then skip trying to reset the
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

  // printf ("\nLeaving SgFunctionDeclaration::fixupCopy_scopes(): for function
  // = %s = %p = %s copy = %p
  // \n",this->get_name().str(),this,this->class_name().c_str(),copy);
}

// DQ (10/6/2007): Added fixup function to set scopes not set properly by the
// ROSETTA generated copy!
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

  // Iterate over both lists to match up the correct pairs of SgInitializedName
  // objects
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

  // Call the base class fixupCopy member function
  // SgFunctionDeclaration::fixupCopy_scopes(copy,help);

  // Setup the scopes of the SgInitializedName objects in the paraleter list
  ROSE_ASSERT(get_CtorInitializerList() != NULL);
  populateCopiedMemberFunctionCtorInitializerSubtree(
      this, memberFunctionDeclaration_copy, help);
  remapCopiedMemberFunctionCtorInitializerSubtree(
      this, memberFunctionDeclaration_copy, help);
  get_CtorInitializerList()->fixupCopy_scopes(
      memberFunctionDeclaration_copy->get_CtorInitializerList(), help);

  // Call the base class fixupCopy member function
  SgFunctionDeclaration::fixupCopy_scopes(copy, help);
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
    // should not be assered yet. We can't assert this yet since this is part fo
    // the copy of the defining declaration within the processing of the
    // non-defining declaration (recurssively called!)
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
  printf("\nIn SgTemplateInstantiationDecl::fixupCopy_scopes(): for function = "
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
    printf("this = %p templateMemberFunctionDeclaration_copy = %p name = %s \n",
           this, templateMemberFunctionDeclaration_copy,
           this->get_name().str());
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
    // Reused copied bodies can remain parented to an earlier copied function
    // definition in the same declaration chain. Re-anchor the body to the
    // current copy before fixing nested labels and rebuilding symbol tables.
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
  printf("Inside of SgVariableDeclaration::fixupCopy_scopes() for %p = %s copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Also call the base class version of the fixupCopycopy() member function
  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  SgVariableDeclaration *variableDeclaration_copy =
      isSgVariableDeclaration(copy);
  ROSE_ASSERT(variableDeclaration_copy != NULL);

  // DQ (10/14/2007): Handle the case of a type defined in the base type of the
  // typedef (similar problem for SgVariableDeclaration). printf ("this = %p
  // this->get_variableDeclarationContainsBaseTypeDefiningDeclaration() = %s
  // \n",this,this->get_variableDeclarationContainsBaseTypeDefiningDeclaration()
  // ? "true" : "false");
  if (this->get_variableDeclarationContainsBaseTypeDefiningDeclaration() ==
      true) {
    ROSE_ASSERT(
        variableDeclaration_copy
            ->get_variableDeclarationContainsBaseTypeDefiningDeclaration() ==
        true);
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
    // symbol table and other infor which is not setup correctly. printf ("Need
    // to compute the baseTypeDeclaration_copy better (perhaps we shoul look
    // into the map of copies? \n"); ROSE_ABORT();

    ROSE_ASSERT(baseTypeDeclaration_original != NULL);
    ROSE_ASSERT(baseTypeDeclaration_copy != NULL);

    // printf ("In SgVariableDeclaration::fixupCopy_scopes(): Calling fixupCopy
    // on %p = %s
    // \n",baseTypeDeclaration_original,baseTypeDeclaration_original->class_name().c_str());

    baseTypeDeclaration_original->fixupCopy_scopes(baseTypeDeclaration_copy,
                                                   help);
  }

  const SgInitializedNamePtrList &variableList_original = this->get_variables();
  SgInitializedNamePtrList &variableList_copy =
      variableDeclaration_copy->get_variables();

  // printf ("Inside of SgVariableDeclaration::fixupCopy_scopes():
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
    ROSE_ASSERT((*i_copy)->get_declptr() != NULL);
    (*i_original)->fixupCopy_scopes(*i_copy, help);

    i_original++;
    i_copy++;
  }
}

void SgClassDeclaration::fixupCopy_scopes(SgNode *copy,
                                          SgCopyHelp &help) const {
  if (isCopiedNodeValue(help, this)) {
    return;
  }

  // We need to call the fixupCopy function from the parent of a
  // SgVariableDeclaration because the copy function in the parent of the
  // variable declaration sets the parent of the SgVariableDeclaration and we
  // need this parent in the fixupCopy function in the SgInitializedName.

#if DEBUG_FIXUP_COPY
  printf("Inside of SgClassDeclaration::fixupCopy_scopes() for class = %s = %p "
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

  SgClassType *classType = canonicalizeCopiedClassType(classDeclaration_copy);
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
    // DQ (3/17/2014): Node that test2005_98.C fails this new test (so issue a
    // warning for now).
    if (classDeclaration_original_firstNondefining->get_type() !=
        classDeclaration_original_defining->get_type()) {
      printf("Warning: Testing of classDeclaration_original: "
             "firstNondefining->get_type() != defining->get_type() \n");
    }
    // ROSE_ASSERT(classDeclaration_original_firstNondefining->get_type() ==
    // classDeclaration_original_defining->get_type());
  }
}

void SgClassDefinition::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
  // Guard against shared nodes. SgClassDefinition nodes from non-defining
  // declarations are shared.
  if (this == copy) {
    // Node is shared, not copied. Skip fixup to avoid corrupting the original.
    return;
  }

  if (isCopiedNodeValue(help, this)) {
    return;
  }

  // DQ (10/19/2007): Added support to fixup the base class names

  ROSE_ASSERT(this->get_declaration() != NULL);

#if DEBUG_FIXUP_COPY
  printf("Inside of SgClassDefinition::fixupCopy_scopes() for class = %s class "
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
  synchronizeCopiedDeclarationList(statementList_original, statementList_copy,
                                   classDefinition_copy, help);
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
    ROSE_ASSERT(baseClass_copy->get_base_class());

    ROSE_ASSERT(nrBaseClass == NULL);
    ROSE_ASSERT(nrBaseClass_copy == NULL);

    this->get_base_class()->fixupCopy_scopes(baseClass_copy->get_base_class(),
                                             help);
    restoreCopiedReferencedDeclarationParentsToScope(
        baseClass_copy->get_base_class(), help);
  } else if (nrBaseClass != NULL) {
    ROSE_ASSERT(nrBaseClass->get_base_class_nonreal() != NULL);

    ROSE_ASSERT(nrBaseClass_copy != NULL);
    ROSE_ASSERT(nrBaseClass_copy->get_base_class_nonreal() != NULL);

    nrBaseClass->get_base_class_nonreal()->fixupCopy_scopes(
        nrBaseClass_copy->get_base_class_nonreal(), help);
    restoreCopiedReferencedDeclarationParentsToScope(
        nrBaseClass_copy->get_base_class_nonreal(), help);
  } else {
    ROSE_ABORT();
  }
}

void SgLabelStatement::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf(
      "Inside of SgLabelStatement::fixupCopy_scopes() for %p = %s copy = %p \n",
      this, this->class_name().c_str(), copy);
#endif

  SgLabelStatement *labelStatement_copy = isSgLabelStatement(copy);
  ROSE_ASSERT(labelStatement_copy != NULL);

  // I don't think there is anything to do here since we already make sure that
  // there is a new SgLabelSymbol and it is the SgGotoStatement that has to have
  // it's reference to the label fixed up if the label and the goto statement
  // have been copied.

  // DQ (10/25/2007): Added handling for the new explicit scope in
  // SgLabelStatement
  ROSE_ASSERT(this->hasExplicitScope() == true);

  FixupCopyDataMemberMacro(labelStatement_copy, SgScopeStatement, get_scope,
                           set_scope)

      // Reused copied labels can already point at an earlier copied function
      // definition, which prevents the generic original->copy remap above from
      // updating the explicit label scope. Re-anchor labels to the copied AST's
      // owning function before symbol-table rebuilding.
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
  printf(
      "Inside of SgGotoStatement::fixupCopy_scopes() for %p = %s copy = %p \n",
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
  // previously and thus it should be updated to reflect the copied declaration.
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
  // variable declaration sets the parent of the SgVariableDeclaration and we
  // need this parent in the fixupCopy function in the SgInitializedName.

#if DEBUG_FIXUP_COPY
  printf("Inside of SgTypedefDeclaration::fixupCopy_scopes() for typedef name "
         "= %s = %p = %s copy = %p \n",
         this->get_name().str(), this, this->class_name().c_str(), copy);
#endif

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_scopes(copy, help);

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

    baseTypeDeclaration_original->fixupCopy_scopes(baseTypeDeclaration_copy,
                                                   help);
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

  // DQ (10/17/2007): fixup the type used to make sure it has the declaration
  // set the AST copy.
  SgEnumType *enum_type_original = this->get_type();
  ROSE_ASSERT(enum_type_original != NULL);

  SgEnumType *enum_type_copy = enumDeclaration_copy->get_type();
  ROSE_ASSERT(enum_type_copy != NULL);

  const SgEnumDeclaration *originalEnumDefinition =
      canonicalOriginalEnumDefinition(this);
  SgEnumDeclaration *effectiveDefinitionCopy =
      canonicalizeCopiedEnumDeclarationChain(originalEnumDefinition,
                                             enumDeclaration_copy, help);

  // printf ("This is the non-defining declaration, so just fixup the SgEnumType
  // = %p with the correct SgEnumDeclaration declaration! \n",enum_type_copy);

  // Only modify declaration pointer if the type was actually copied (not
  // shared). Since SgTreeCopy shares types (types are not deep copied),
  // enum_type_copy == enum_type_original. Modifying the shared type's
  // declaration would corrupt the original AST.
  // FixupCopyDataMemberMacro_local_debug(enum_type_copy,SgDeclarationStatement,get_declaration,set_declaration)
  if (enum_type_copy != enum_type_original) {
    SgCopyHelp::copiedNodeMapTypeIterator i =
        help.get_copiedNodeMap().find(enum_type_original->get_declaration());
    // printf ("SgCopyHelp::copiedNodeMapTypeIterator i !=
    // help.get_copiedNodeMap().end() = %s \n",i !=
    // help.get_copiedNodeMap().end() ? "true" : "false");
    if (i != help.get_copiedNodeMap().end()) {
      SgNode *associated_node_copy = i->second;
      ROSE_ASSERT(associated_node_copy != NULL);
      SgDeclarationStatement *local_copy =
          isSgDeclarationStatement(associated_node_copy);
      ROSE_ASSERT(local_copy != NULL);
      // printf ("Resetting using local_copy = %p = %s
      // \n",local_copy,local_copy->class_name().c_str());
      enum_type_copy->set_declaration(local_copy);
    }
  }

  if (effectiveDefinitionCopy != NULL &&
      effectiveDefinitionCopy != enumDeclaration_copy &&
      enumDeclaration_copy->get_enumerators().empty()) {
    return;
  }

  canonicalizeCopiedEnumEnumerators(originalEnumDefinition,
                                    enumDeclaration_copy, help);

  // Now reset the enum fields.
  const SgInitializedNamePtrList &enumFieldList_original =
      originalEnumDefinition->get_enumerators();
  SgInitializedNamePtrList &enumFieldList_copy =
      enumDeclaration_copy->get_enumerators();

  SgInitializedNamePtrList::const_iterator i_original =
      enumFieldList_original.begin();
  SgInitializedNamePtrList::iterator i_copy = enumFieldList_copy.begin();

  // Iterate over both lists to match up the correct pairs of SgInitializedName
  // objects
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
  printf("Inside of SgNamespaceDeclarationStatement::fixupCopy_scopes() for %p "
         "= %s copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  // Call the base class fixupCopy member function
  SgDeclarationStatement::fixupCopy_scopes(copy, help);

  SgNamespaceDeclarationStatement *namespaceDeclaration_copy =
      isSgNamespaceDeclarationStatement(copy);
  if (namespaceDeclaration_copy == NULL) {
    namespaceDeclaration_copy =
        findAttachedCopiedNamespaceDeclaration(this, help);
  }
  if (namespaceDeclaration_copy == NULL) {
    namespaceDeclaration_copy = isSgNamespaceDeclarationStatement(
        resolveCopiedReferencedDeclaration(this, help));
  }
  ROSE_ASSERT(namespaceDeclaration_copy != NULL);

  // printf ("namespaceDeclaration_copy->get_firstNondefiningDeclaration() = %p
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
      // point into the copied tree so that symbol-table insertion and lookup
      // stay within the copy (Issue 69). NOTE: FixupCopyDataMemberMacro
      // assumes non-null pointers. The namespace linkage pointers can
      // legitimately be NULL at the ends of the reentrant chain, so we
      // repair them carefully.
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
  synchronizeCopiedDeclarationList(statementList_original, statementList_copy,
                                   namespaceDefinition_copy, help);
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
  printf(
      "Inside of SgTemplateInstantiationDirectiveStatement::fixupCopy_scopes() "
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
  if (declaration_copy == this->get_declaration()) {
    ROSE_ASSERT(referencedDeclarationMayRemainShared(this->get_declaration()));
    return;
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

  // printf ("Inside of SgIfStmt::fixupCopy_scopes() this = %p = %s  copy = %p
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
    // ROSE_ASSERT(scopeIfStmtCopyTruBody->get_symbol_table()->size()  == 0);
    if (scopeIfStmtCopyTruBody->symbol_table_size() != 0) {
      printf("Warning: (fails for g++ 4.2): "
             "scopeIfStmtCopyTruBody->symbol_table_size() = %zu \n",
             scopeIfStmtCopyTruBody->symbol_table_size());
      // ifStmtCopyTruBody->get_file_info()->display("ifStatement_copy->get_true_body():
      // debug");
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
      printf("Warning: (fails for g++ 4.2): "
             "isSgScopeStatement(ifStatement_copy->get_false_body())->symbol_"
             "table_size() = %zu \n",
             scopeStmnt->symbol_table_size());
      // ifStatement_copy->get_true_body()->get_file_info()->display("ifStatement_copy->get_false_body():
      // debug");
    }
    // DQ (3/3/12): This fails for the g++ version 4.2.4 compiler (newer
    // versions of g++ pass fine). ROSE_ASSERT(scopeStmnt->symbol_table_size()
    // == 0);
  }

  // printf ("\nProcess the FALSE body of the SgIfStmt \n\n");

  if (this->get_false_body() != NULL) {
    this->get_false_body()->fixupCopy_scopes(ifStatement_copy->get_false_body(),
                                             help);
  }

  // printf ("\nLeaving SgIfStmt::fixupCopy_scopes() this = %p = %s  copy = %p
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

  // DQ (11/1/2007): Force the symbol table to be setup so that references can
  // be made to it later. If we built it too early then the scope (on The
  // SgInitializedName objects) have not be setup, and if we build it too late
  // then we don't have the symbols in place to reset the references. printf
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

  // DQ (11/1/2007): Force the symbol table to be setup so that references can
  // be made to it later. If we built it too early then the scope (on The
  // SgInitializedName objects) have not be setup, and if we build it too late
  // then we don't have the symbols in place to reset the references. printf
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

  // printf ("SgForInitStatement::fixupCopy_scopes(): Sorry not implemented
  // \n");

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
  ROSE_ASSERT(catchStatement_copy != NULL);

  printf("SgCatchStatementSeq::fixupCopy_scopes(): Sorry not implemented \n");

  // The relavant data member here is a SgStatementPtrList p_catch_statement_seq

  // ROSE_ASSERT(this->get_body() != NULL);
  // this->get_body()->fixupCopy_scopes(catchStatement_copy->get_body(),help);
}

void SgWhileStmt::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf(
      "Inside of SgWhileStmt::fixupCopy_scopes() this = %p = %s  copy = %p \n",
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
  printf("Inside of SgSwitchStatement::fixupCopy_scopes() this = %p = %s  copy "
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
  printf("Inside of SgTryStmt::fixupCopy_scopes() this = %p = %s  copy = %p \n",
         this, this->class_name().c_str(), copy);
#endif

  SgStatement::fixupCopy_scopes(copy, help);

  SgTryStmt *tryStatement_copy = isSgTryStmt(copy);
  ROSE_ASSERT(tryStatement_copy != NULL);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_scopes(tryStatement_copy->get_body(), help);
}

void SgCatchOptionStmt::fixupCopy_scopes(SgNode *copy, SgCopyHelp &help) const {
#if DEBUG_FIXUP_COPY
  printf("Inside of SgCatchOptionStmt::fixupCopy_scopes() this = %p = %s  copy "
         "= %p \n",
         this, this->class_name().c_str(), copy);
#endif

  printf("SgCatchOptionStmt::fixupCopy_scopes(): Sorry not implemented \n");

  SgScopeStatement::fixupCopy_scopes(copy, help);

  SgCatchOptionStmt *catchOptionStatement_copy = isSgCatchOptionStmt(copy);
  ROSE_ASSERT(catchOptionStatement_copy != NULL);

  ROSE_ASSERT(this->get_trystmt() != NULL);
  // I think this might cause endless recursion, so comment out for now!
  // this->get_trystmt()->fixupCopy_scopes(catchOptionStatement_copy->get_trystmt(),help);

  ROSE_ASSERT(this->get_condition() != NULL);
  this->get_condition()->fixupCopy_scopes(
      catchOptionStatement_copy->get_condition(), help);

  ROSE_ASSERT(this->get_body() != NULL);
  this->get_body()->fixupCopy_scopes(catchOptionStatement_copy->get_body(),
                                     help);
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

void SgTemplateArgument::fixupCopy_scopes(SgNode *copy, SgCopyHelp &) const {
  SgTemplateArgument *templateArgument_copy = isSgTemplateArgument(copy);
  ROSE_ASSERT(templateArgument_copy != NULL);

#if DEBUG_FIXUP_COPY
  printf("\nIn SgTemplateArgument::fixupCopy_scopes(): this = %p = %s copy = "
         "%p \n",
         this, this->class_name().c_str(), copy);
#endif
}

void SgTemplateParameter::fixupCopy_scopes(SgNode *copy,
                                           SgCopyHelp &help) const {
  SgTemplateParameter *templateParameter_copy = isSgTemplateParameter(copy);
  ROSE_ASSERT(templateParameter_copy != NULL);

  if (get_parameterType() != SgTemplateParameter::template_parameter) {
    return;
  }

  SgNonrealDecl *original_nonreal = isSgNonrealDecl(get_templateDeclaration());
  SgNonrealDecl *copied_nonreal =
      isSgNonrealDecl(templateParameter_copy->get_templateDeclaration());
  if (original_nonreal == NULL || copied_nonreal == NULL) {
    return;
  }

  repairCopiedNonrealDeclScope(original_nonreal, copied_nonreal, help);

  const SgTemplateParameterPtrList &original_params =
      original_nonreal->get_tpl_params();
  SgTemplateParameterPtrList &copied_params = copied_nonreal->get_tpl_params();
  const size_t param_count =
      std::min(original_params.size(), copied_params.size());
  for (size_t i = 0; i < param_count; ++i) {
    SgTemplateParameter *original_param = original_params[i];
    SgTemplateParameter *copied_param = copied_params[i];
    if (original_param == NULL || copied_param == NULL) {
      continue;
    }
    if (copied_param->get_parent() == NULL) {
      copied_param->set_parent(copied_nonreal);
    }
    original_param->fixupCopy_scopes(copied_param, help);
  }
}
