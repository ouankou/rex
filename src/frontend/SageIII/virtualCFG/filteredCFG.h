#ifndef FILTEREDCFGVIEW_H
#define FILTEREDCFGVIEW_H

#include "virtualCFG.h"

#include <string>

#include <vector>

namespace VirtualCFG {
// Function call node and edge filters

template <typename FilterFunction> class FilteredCFGEdge;

template <typename FilterFunction> class FilteredCFGNode {
  CFGNode n;
  FilterFunction filter;

public:
  FilteredCFGNode(const CFGNode &n,
                  const FilterFunction &filter = FilterFunction{})
      : n(n), filter(filter) {}

  /**  Need a default constructor to use with graph wrappers. Underlying
   * node will be invalid. */
  FilteredCFGNode() : n(), filter() {}

  void setFilter(const FilterFunction &f) { filter = f; }

  const CFGNode &toNode() const { return n; }
  std::string toString() const {
    std::string nToString = n.toString();
    const std::size_t close = nToString.find('>');
    if (close != std::string::npos)
      nToString.insert(close + 1, "\n");
    return nToString;
  }
  std::string toStringForDebugging() const { return n.toStringForDebugging(); }
  std::string id() const { return n.id(); }
  SgNode *getNode() const { return n.getNode(); }
  unsigned int getIndex() const { return n.getIndex(); }
  std::vector<FilteredCFGEdge<FilterFunction>> outEdges() const;
  std::vector<FilteredCFGEdge<FilterFunction>> inEdges() const;
  bool isInteresting() const { return true; }
  bool operator==(const FilteredCFGNode &o) const { return n == o.n; }
  bool operator!=(const FilteredCFGNode &o) const { return !(*this == o); }
  bool operator<(const FilteredCFGNode &o) const { return n < o.n; }
};

template <typename FilterFunction> class FilteredCFGEdge {
  CFGPath p;
  FilterFunction filter;

public:
  FilteredCFGEdge(CFGPath p, const FilterFunction &filter = FilterFunction{})
      : p(p), filter(filter) {}

  /**  Need a default constructor to use with graph wrappers. Underlying
   * path will be invalid. */
  FilteredCFGEdge() : filter() {}

  void setFilter(const FilterFunction &f) { filter = f; }

  std::string toString() const { return p.toString(); }
  std::string toStringForDebugging() const { return p.toStringForDebugging(); }
  std::string id() const { return p.id(); }
  FilteredCFGNode<FilterFunction> source() const {
    return FilteredCFGNode<FilterFunction>(p.source(), filter);
  }
  FilteredCFGNode<FilterFunction> target() const {
    return FilteredCFGNode<FilterFunction>(p.target(), filter);
  }
  EdgeConditionKind condition() const { return p.condition(); }
  SgExpression *caseLabel() const { return p.caseLabel(); }
  std::vector<SgInitializedName *> scopesBeingExited() const {
    return p.scopesBeingExited();
  }
  std::vector<SgInitializedName *> scopesBeingEntered() const {
    return p.scopesBeingEntered();
  }
  bool operator==(const FilteredCFGEdge<FilterFunction> &o) const {
    return p == o.p;
  }
  bool operator!=(const FilteredCFGEdge<FilterFunction> &o) const {
    return p != o.p;
  }
  bool operator<(const FilteredCFGEdge<FilterFunction> &o) const {
    return p < o.p;
  }

  //! The underlying path in the full CFG represented by this edge in the
  //! filtered CFG.
  const CFGPath &getPath() const { return p; }
};

template <typename FilterFunction>
std::ostream &cfgToDot(std::ostream &o, std::string graphName,
                       FilteredCFGNode<FilterFunction> start);
} // namespace VirtualCFG

#include "filteredCFGImpl.h"
#endif // FILTEREDCFGVIEW_H
