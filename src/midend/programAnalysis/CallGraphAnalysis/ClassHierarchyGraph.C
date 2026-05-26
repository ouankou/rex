// tps : Switching from rose.h to sage3 changed size from 17,7 MB to 7,3MB
#include "CallGraph.h"

#include "sage3basic.h"

using namespace std;

ClassHierarchyWrapper::ClassHierarchyWrapper(SgNode *node) {
  ASSERT_not_null(isSgProject(node));

  Rose_STL_Container<SgNode *> allCls;
  allCls = NodeQuery::querySubTree(node, V_SgClassDefinition);

  // DQ (10/12/2012): I think we have to exclude class template defintions since
  // template should not be a part of the class hierarchy (only instantations of
  // templated).  The new IR design has developed SgTemplateClassDefinition
  // (derived from SgClassDefinition), and so template have gotten into the mix
  // (returned from the AST query).  so we have to filter them out to get a
  // semilar handling for the class hierarchy graph as before when template were
  // a well represented in the AST.
  for (Rose_STL_Container<SgNode *>::iterator it = allCls.begin();
       it != allCls.end(); it++) {
    SgTemplateClassDefinition *templateClassDefinition =
        isSgTemplateClassDefinition(*it);
    if (templateClassDefinition != NULL) {
      // Set to NULL so that we can remove all NULL values in the next step.
      *it = NULL;
    }
    // ROSE_ASSERT(templateClassDefinition == NULL);
  }

  // printf ("In ClassHierarchyWrapper(): Size before removing
  // SgTemplateClassDefinition = %" PRIuPTR "
  // \n",vectorSizeBeforeRemovingTemplates);
  allCls.erase(std::remove(allCls.begin(), allCls.end(), (SgNode *)NULL),
               allCls.end());
  // size_t vectorSizeAfterRemovingTemplates = allCls.size();
  // printf ("In ClassHierarchyWrapper(): Size after removing
  // SgTemplateClassDefinition = %" PRIuPTR "
  // \n",vectorSizeAfterRemovingTemplates);

  // build the class hierarchy
  // start by iterating through all the classes
  for (Rose_STL_Container<SgNode *>::iterator it = allCls.begin();
       it != allCls.end(); it++) {
    SgClassDefinition *clsDescDef = isSgClassDefinition(*it);
    SgBaseClassPtrList &baseClses = clsDescDef->get_inheritances();

    ClassDefSet &classParents = directParents
        [clsDescDef->get_declaration()->get_mangled_name().getString()];

    // for each iterate through their parents and add parent - child
    // relationship to the graph
    for (SgBaseClassPtrList::iterator it = baseClses.begin();
         it != baseClses.end(); it++) {
      if (*it == NULL) {
        continue;
      }

      SgClassDeclaration *baseDecl = (*it)->get_base_class();
      if (baseDecl == NULL) {
        // Unresolved/dependent base classes are represented without a concrete
        // declaration target and cannot participate in class hierarchy edges.
        continue;
      }

      // Prefer the defining declaration; fall back through first-nondefining if
      // needed so compiler-generated anchors still map to a real class node.
      SgClassDeclaration *baseCls =
          isSgClassDeclaration(baseDecl->get_definingDeclaration());
      if (baseCls == NULL) {
        if (SgClassDeclaration *firstNondef = isSgClassDeclaration(
                baseDecl->get_firstNondefiningDeclaration())) {
          baseCls =
              isSgClassDeclaration(firstNondef->get_definingDeclaration());
        }
      }
      if (baseCls == NULL) {
        continue;
      }

      SgClassDefinition *baseClsDef = baseCls->get_definition();
      if (baseClsDef == NULL) {
        continue;
      }

      classParents.insert(baseClsDef);
      directChildren[baseCls->get_mangled_name().getString()].insert(
          clsDescDef);
    }
  }

  // Now populate the ancestor/all subclasses maps
  buildAncestorsMap(directParents, ancestorClasses);
  buildAncestorsMap(directChildren, subclasses);
}

const ClassHierarchyWrapper::ClassDefSet &
ClassHierarchyWrapper::getSubclasses(SgClassDefinition *cls) const {
  const ClassDefSet *result = NULL;
  MangledNameToClassDefsMap::const_iterator children =
      subclasses.find(cls->get_declaration()->get_mangled_name());
  if (children == subclasses.end()) {
    static ClassDefSet emptySet;
    result = &emptySet;
  } else {
    result = &children->second;
  }

  return *result;
}

const ClassHierarchyWrapper::ClassDefSet &
ClassHierarchyWrapper::getAncestorClasses(SgClassDefinition *cls) const {
  const ClassDefSet *result = NULL;
  MangledNameToClassDefsMap::const_iterator children =
      ancestorClasses.find(cls->get_declaration()->get_mangled_name());
  if (children == ancestorClasses.end()) {
    static ClassDefSet emptySet;
    result = &emptySet;
  } else {
    result = &children->second;
  }

  return *result;
}

const ClassHierarchyWrapper::ClassDefSet &
ClassHierarchyWrapper::getDirectSubclasses(SgClassDefinition *cls) const {
  const ClassDefSet *result = NULL;

  MangledNameToClassDefsMap::const_iterator children =
      directChildren.find(cls->get_declaration()->get_mangled_name());
  if (children == directChildren.end()) {
    static ClassDefSet emptySet;
    result = &emptySet;
  } else {
    result = &children->second;
  }

  return *result;
}

void findParents(
    const string &classMangledName,
    const ClassHierarchyWrapper::MangledNameToClassDefsMap &parents,
    ClassHierarchyWrapper::MangledNameToClassDefsMap &transitiveParents,
    set<SgClassDefinition *> &processed) {
  ClassHierarchyWrapper::MangledNameToClassDefsMap::const_iterator
      currentParents = parents.find(classMangledName);

  if (currentParents == parents.end()) {
    ROSE_ASSERT(transitiveParents.find(classMangledName) ==
                transitiveParents.end());
    return;
  }

  ClassHierarchyWrapper::ClassDefSet &currentTransitiveParents =
      transitiveParents[classMangledName];

  // Our transitive parents are simply the union of our parents' transitive
  // parents
  for (SgClassDefinition *parent : currentParents->second) {
    std::string parentName = parent->get_declaration()->get_mangled_name();

    if (processed.find(parent) == processed.end()) {
      findParents(parentName, parents, transitiveParents, processed);
      processed.insert(parent);
    }

    ClassHierarchyWrapper::MangledNameToClassDefsMap::const_iterator
        grandparents = transitiveParents.find(parentName);
    if (grandparents != transitiveParents.end()) {
      currentTransitiveParents.insert(grandparents->second.begin(),
                                      grandparents->second.end());
    }

    currentTransitiveParents.insert(parent);
  }
}

void ClassHierarchyWrapper::buildAncestorsMap(
    const MangledNameToClassDefsMap &parents,
    MangledNameToClassDefsMap &transitiveParents) {
  transitiveParents.clear();

  // Iterate over all the classes and calculate the transitive parents for each
  // one
  set<SgClassDefinition *> processedNodes;
  MangledNameToClassDefsMap::const_iterator it = parents.begin();
  for (; it != parents.end(); ++it) {
    findParents(it->first, parents, transitiveParents, processedNodes);
  }
}
