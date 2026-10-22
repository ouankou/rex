#ifndef CLASS_HIERARCHY_GRAPH_H
#define CLASS_HIERARCHY_GRAPH_H

#include <unordered_map>
#include <unordered_set>

class ROSE_DLL_API ClassHierarchyWrapper {
public:
  typedef std::unordered_set<SgClassDefinition *> ClassDefSet;

  typedef std::unordered_map<SgClassDefinition *, ClassDefSet> ClassDefMap;

private:
  /** Map from each class to all its immediate superclasses. */
  ClassDefMap directParents;

  /** Map from each class to all its immediate subclasses. */
  ClassDefMap directChildren;

  /** Map from class to all (strict) ancestors. */
  ClassDefMap ancestorClasses;

  /** Map from class to all (strict) subclasses. */
  ClassDefMap subclasses;

public:
  explicit ClassHierarchyWrapper(SgNode *node);

  const ClassDefSet &getSubclasses(SgClassDefinition *) const;
  const ClassDefSet &getDirectSubclasses(SgClassDefinition *) const;
  const ClassDefSet &getAncestorClasses(SgClassDefinition *) const;

private:
  /** Computes the transitive closure of the child-parent class relationship.
   * @param parents map from each class to its parents.
   * @param transitiveParents map from each class to all its ancestors */
  static void buildAncestorsMap(const ClassDefMap &parents,
                                ClassDefMap &transitiveParents);
};

#endif
