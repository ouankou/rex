#include "CallGraph.h"

#include "sage3basic.h"

ClassHierarchyWrapper::ClassHierarchyWrapper(SgNode *node) {
  ASSERT_not_null(isSgProject(node));

  for (SgNode *classNode : NodeQuery::querySubTree(node, V_SgClassDefinition)) {
    if (isSgTemplateClassDefinition(classNode) != nullptr)
      continue;
    auto *definition = isSgClassDefinition(classNode);
    ClassDefSet &classParents = directParents[definition];
    for (SgBaseClass *base : definition->get_inheritances()) {
      ASSERT_not_null(base);
      SgClassDeclaration *baseDecl = base->get_base_class();
      if (baseDecl == nullptr) {
        ASSERT_require(isSgNonrealBaseClass(base) || isSgExpBaseClass(base));
        continue;
      }
      auto *baseDefining =
          isSgClassDeclaration(baseDecl->get_definingDeclaration());
      ASSERT_not_null(baseDefining);
      auto *baseDefinition = baseDefining->get_definition();
      ASSERT_not_null(baseDefinition);
      classParents.insert(baseDefinition);
      directChildren[baseDefinition].insert(definition);
    }
  }

  buildAncestorsMap(directParents, ancestorClasses);
  buildAncestorsMap(directChildren, subclasses);
}

const ClassHierarchyWrapper::ClassDefSet &
ClassHierarchyWrapper::getSubclasses(SgClassDefinition *cls) const {
  ASSERT_not_null(cls);
  auto classes = subclasses.find(cls);
  if (classes != subclasses.end())
    return classes->second;
  static const ClassDefSet emptySet;
  return emptySet;
}

const ClassHierarchyWrapper::ClassDefSet &
ClassHierarchyWrapper::getAncestorClasses(SgClassDefinition *cls) const {
  ASSERT_not_null(cls);
  auto classes = ancestorClasses.find(cls);
  if (classes != ancestorClasses.end())
    return classes->second;
  static const ClassDefSet emptySet;
  return emptySet;
}

const ClassHierarchyWrapper::ClassDefSet &
ClassHierarchyWrapper::getDirectSubclasses(SgClassDefinition *cls) const {
  ASSERT_not_null(cls);
  auto classes = directChildren.find(cls);
  if (classes != directChildren.end())
    return classes->second;
  static const ClassDefSet emptySet;
  return emptySet;
}

void ClassHierarchyWrapper::buildAncestorsMap(const ClassDefMap &parents,
                                              ClassDefMap &transitiveParents) {
  transitiveParents.clear();
  for (const auto &[node, direct] : parents) {
    auto &closure = transitiveParents[node];
    std::vector<SgClassDefinition *> pending(direct.begin(), direct.end());
    while (!pending.empty()) {
      auto *parent = pending.back();
      pending.pop_back();
      ASSERT_require(parent != node);
      if (!closure.insert(parent).second)
        continue;
      auto ancestors = parents.find(parent);
      if (ancestors != parents.end())
        pending.insert(pending.end(), ancestors->second.begin(),
                       ancestors->second.end());
    }
  }
}
