#include <vector>

#include <string>

class SgNode;
class SgInitializedName;
class SgExpression;

// namespace VirtualCFG {

class CFGEdge;

class CFGNode {
  SgNode *node;
  unsigned int index;
};

class CFGEdge {
  CFGNode src, tgt;

public:
  CFGEdge(CFGNode src, CFGNode tgt) : src(src), tgt(tgt) {}
  std::string toString() const;
  std::string id() const;
  CFGNode source() const { return src; }
  CFGNode target() const { return tgt; }

  SgExpression *optionKey() const;
  std::vector<SgInitializedName *> scopesBeingExited() const;
  std::vector<SgInitializedName *> scopesBeingEntered() const;
  bool operator==(const CFGEdge &o) const { return id() == o.id(); }
  bool operator!=(const CFGEdge &o) const { return id() != o.id(); }
  bool operator<(const CFGEdge &o) const { return id() < o.id(); }
};
