
#ifndef LOOPTREE_DEPCOMP
#define LOOPTREE_DEPCOMP

#include "DepGraph.h"

#include "DepInfoAnal.h"

#include "DepRel.h"

#include "DomainInfo.h"

#include "GraphScope.h"

#include "LoopTransformInterface.h"

#include "LoopTree.h"

#include "LoopTreeObserver.h"

#include "MultiGraphObserve.h"

#include "PtrMap.h"

#include "PtrSet.h"

#include "TransDepGraph.h"
/*
 * QY: The LoopTreeDepComp class
 *  build a LoopTree and a corresponding dep graph from an AST
 */
class LoopTreeDepGraphNode
    : public MultiGraphObserveNodeTemplate<LoopTreeNode *>,
      public LoopTreeObserver {
  DepInfo loopMap;
  DomainCond domain;
  PtrMapWrap<LoopTreeNode, LoopTreeDepGraphNode> *sourceMap;
  ~LoopTreeDepGraphNode() {
    LoopTreeNode *s = GetInfo();
    if (s && IsSimpleStmt(s)) {
      ASSERT_require(sourceMap->Map(s) == this);
      sourceMap->RemoveMapping(s);
      s->DetachObserver(*this);
    }
  }
  void UpdateSwapStmtLoop(const SwapStmtLoopInfo &_info) {
    loopMap.SwapLoop(_info.GetLoop1(), _info.GetLoop2(), DEP_SINK);
    domain.SwapLoop(_info.GetLoop1(), _info.GetLoop2());
  }
  void UpdateMergeStmtLoop(const MergeStmtLoopInfo &_info) {
    loopMap.MergeLoop(_info.GetLoop1(), _info.GetLoop2(), DEP_SINK);
    domain.MergeLoop(_info.GetLoop1(), _info.GetLoop2());
    if (_info.GetMergeAlign() != 0) {
      loopMap.AlignLoop(_info.GetLoop1(), -_info.GetMergeAlign(), DEP_SINK);
      domain.AlignLoop(_info.GetLoop1(), -_info.GetMergeAlign());
    }
  }
  LoopTreeDepGraphNode(MultiGraphCreate *c, LoopTreeNode *s,
                       PtrMapWrap<LoopTreeNode, LoopTreeDepGraphNode> *mapping)
      : LoopTreeDepGraphNode(c, s, mapping, DomainCond(s->LoopLevel())) {}
  LoopTreeDepGraphNode(MultiGraphCreate *c, LoopTreeNode *s,
                       PtrMapWrap<LoopTreeNode, LoopTreeDepGraphNode> *mapping,
                       const DomainCond _dm)
      : MultiGraphObserveNodeTemplate<LoopTreeNode *>(c, s),
        loopMap(DepInfoGenerator::GetIDDepInfo(s->LoopLevel(), false)),
        domain(_dm), sourceMap(mapping) {
    if (IsSimpleStmt(s))
      s->AttachObserver(*this);
  }
  LoopTreeDepGraphNode(MultiGraphCreate *c, LoopTreeNode *s,
                       PtrMapWrap<LoopTreeNode, LoopTreeDepGraphNode> *mapping,
                       const DepInfo &_loopMap, const DomainCond _dm)
      : MultiGraphObserveNodeTemplate<LoopTreeNode *>(c, s), loopMap(_loopMap),
        domain(_dm), sourceMap(mapping) {
    if (IsSimpleStmt(s))
      s->AttachObserver(*this);
  }

  void UpdateInsertStmtLoop(const InsertStmtLoopInfo &_info) {
    int level = _info.GetLoop();
    loopMap.InsertLoop(level, DEP_SINK);
    domain.InsertLoop(level);
  }
  void UpdateDeleteStmtLoop(const DeleteStmtLoopInfo &_info) {
    int level = _info.GetLoop();
    loopMap.RemoveLoop(level, DEP_SINK);
    domain.RemoveLoop(level);
  }

public:
  int NumOfLoops() const { return loopMap.cols(); }
  int AstTreeNumOfLoops() const { return loopMap.rows(); }
  int AstTreeDim2LoopTreeDim(int index) {
    for (int i = 0; i < loopMap.cols(); ++i)
      if (!loopMap.Entry(index, i).IsBottom())
        return i;
    ROSE_ABORT();
  }
  int LoopTreeDim2AstTreeDim(int index) {
    for (int i = 0; i < loopMap.rows(); ++i)
      if (!loopMap.Entry(i, index).IsBottom())
        return i;
    ROSE_ABORT();
  }
  const DepInfo &GetLoopMap() const { return loopMap; }
  const DomainCond &GetDomain() const { return domain; }
  std::string toString() const { return GetInfo()->toString(); }

  friend class LoopTreeDepGraphCreate;
};

typedef GraphAccessTemplate<LoopTreeDepGraphNode, DepInfoEdge> LoopTreeDepGraph;
typedef TransDepGraphCreate<LoopTreeDepGraphNode> LoopTreeTransDepGraphCreate;

class LoopTreeNodeDepMap {
  PtrMapWrap<LoopTreeNode, LoopTreeDepGraphNode> *map;
  void InsertMapping(const LoopTreeNode *_info, LoopTreeDepGraphNode *n) {
    map->InsertMapping(_info, n);
  }

public:
  LoopTreeNodeDepMap(PtrMapWrap<LoopTreeNode, LoopTreeDepGraphNode> *m)
      : map(m) {}
  LoopTreeNodeDepMap(const LoopTreeNodeDepMap &that) : map(that.map) {}
  LoopTreeNodeDepMap() { map = 0; }

  LoopTreeDepGraphNode *GetDepNode(const LoopTreeNode *s) const {
    return map->Map(s);
  }
  LoopTreeNode *GetTreeNode(const LoopTreeDepGraphNode *n) const {
    return n->GetInfo();
  }
  friend class LoopTreeDepGraphCreate;
};

class LoopTreeDepGraphCreate : public DepInfoGraphCreate<LoopTreeDepGraphNode>,
                               public LoopTreeObserver {
  LoopTreeNodeDepMap map;
  typedef DepInfoGraphCreate<LoopTreeDepGraphNode> GraphCreateBase;
  LoopTreeDepGraphNode *GetObservedNode(const LoopTreeNode *node) const {
    LoopTreeDepGraphNode *result = map.GetDepNode(node);
    ASSERT_require(result != nullptr && ContainNode(result));
    return result;
  }

  void UpdateDeleteNode(const LoopTreeNode *n) override;
  void UpdateSplitStmt(const SplitStmtInfo &info) override;
  void UpdateSplitStmt2(const SplitStmtInfo2 &info) override;
  void UpdateSwapStmtLoop(const SwapStmtLoopInfo &info) override;
  void UpdateMergeStmtLoop(const MergeStmtLoopInfo &info) override;
  void UpdateDeleteStmtLoop(const DeleteStmtLoopInfo &info) override;
  void UpdateDistNode(const DistNodeInfo &info) override;
  void UpdateInsertLoop(const InsertLoopInfo &info) override;

public:
  bool ObservesNode(const LoopTreeNode *node) const override {
    return !IsSimpleStmt(node) || ContainNode(map.GetDepNode(node));
  }
  LoopTreeDepGraphCreate(const LoopTreeNodeDepMap &m, BaseGraphCreate *b = 0)
      : DepInfoGraphCreate<LoopTreeDepGraphNode>(b), map(m) {}
  ~LoopTreeDepGraphCreate() override;

  void BuildDep(DepInfoAnal &anal, LoopTreeDepGraphNode *n1,
                LoopTreeDepGraphNode *n2, DepType t);

  void AddNode(LoopTreeDepGraphNode *result);
  LoopTreeDepGraphNode *CreateNode(LoopTreeNode *s,
                                   LoopTreeDepGraphNode *that = 0);
  LoopTreeDepGraphNode *CreateNode(LoopTreeNode *s, const DomainCond &c);
  LoopTreeDepGraphNode *CreateNode(LoopTreeNode *s, const DepInfo &m,
                                   const DomainCond &c);
  bool DeleteNode(LoopTreeDepGraphNode *n);
  DepInfoEdge *CreateEdge(LoopTreeDepGraphNode *n1, LoopTreeDepGraphNode *n2,
                          const DepInfo &info);
  DepInfoEdge *CreateEdgeFromOrigAst(LoopTreeDepGraphNode *n1,
                                     LoopTreeDepGraphNode *n2,
                                     const DepInfo &info);
};

class LoopTreeDepComp {
  LoopTreeNode *root;
  LoopTreeCreate *tc;
  LoopTreeDepGraph *depGraph;
  LoopTreeNodeDepMap map;

protected:
  LoopTreeDepComp() {
    root = 0;
    depGraph = 0;
    tc = 0;
  }
  LoopTreeDepComp(LoopTreeDepComp &that) {
    map = that.map;
    root = that.root;
    tc = that.tc;
    depGraph = that.depGraph;
  }
  virtual ~LoopTreeDepComp() {}
  void SetTreeRoot(LoopTreeNode *r) { root = r; }
  void SetDepGraph(LoopTreeDepGraph *d) { depGraph = d; }
  void SetMap(const LoopTreeNodeDepMap &_map) { map = _map; }
  void SetTreeCreate(LoopTreeCreate *c) { tc = c; }

public:
  LoopTreeCreate *GetLoopTreeCreate() { return tc; }

  LoopTreeNode *GetLoopTreeRoot() const { return root; }
  LoopTreeDepGraph *GetDepGraph() const { return depGraph; }

  LoopTreeDepGraphNode *GetDepNode(const LoopTreeNode *s) const {
    return map.GetDepNode(s);
  }
  LoopTreeNode *GetTreeNode(const LoopTreeDepGraphNode *n) const {
    return n->GetInfo();
  }
  LoopTreeNodeDepMap GetTreeNodeMap() const { return map; }
  const DomainCond &GetDomain(LoopTreeNode *s) {
    return GetDepNode(s)->GetDomain();
  }

  std::string ToString() const { return TreeToString() + "\n" + DepToString(); }
  void DumpTree() const { std::cerr << TreeToString(); }
  void DumpDep() const { std::cerr << DepToString(); }
  std::string TreeToString() const;
  std::string DepToString() const;
  void OutputDep() const;
  void DumpNode(LoopTreeNode *s) const;
};

class LoopTreeDepGraphSubtree : public GraphSelect<LoopTreeDepGraph> {
  PtrSetWrap<const LoopTreeDepGraphNode> selset;
  int level;

public:
  LoopTreeDepGraphSubtree(LoopTreeNode *root, LoopTreeDepGraph *graph)
      : GraphSelect<LoopTreeDepGraph>(graph) {
    for (auto nodes = graph->GetNodeIterator(); !nodes.ReachEnd(); ++nodes) {
      const LoopTreeDepGraphNode *node = nodes.Current();
      for (LoopTreeNode *owner = node->GetInfo(); owner != nullptr;
           owner = owner->Parent()) {
        if (owner == root) {
          selset.insert(node);
          break;
        }
      }
    }
    level = root->LoopLevel();
  }
  virtual bool ContainNode(const LoopTreeDepGraphNode *n) const {
    return selset.IsMember(n);
  }
  virtual bool ContainEdge(const DepInfoEdge *e) const {
    return selset.IsMember(impl->GetEdgeEndPoint(e, GraphAccess::EdgeOut)) &&
           selset.IsMember(impl->GetEdgeEndPoint(e, GraphAccess::EdgeIn)) &&
           SelectDepLevel(e->GetInfo(), level);
  }
};

class LoopTreeDepCompSubtree : public LoopTreeDepComp {
public:
  LoopTreeDepCompSubtree(LoopTreeDepComp &c, LoopTreeNode *root);
  virtual ~LoopTreeDepCompSubtree() { delete GetDepGraph(); }
};

class LoopTreeDepCompCreate : public LoopTreeDepComp {
  DepInfoAnal anal;
  PtrMapWrap<LoopTreeNode, LoopTreeDepGraphNode> nodeMap;
  LoopTreeDepGraphCreate *depCreate;
  LoopTreeCreate treeCreate;
  AstNodePtr top;
  bool supportNonFortranLoop;

public:
  LoopTreeDepCompCreate(const AstNodePtr &top, bool buildDepGraph = true,
                        bool supportNonFortranLoop = false);
  ~LoopTreeDepCompCreate();

  void BuildDepGraph();
  void DetachDepGraph();

  DepInfoAnal &GetDepAnal() { return anal; }
  AstNodePtr CodeGen();
};

#endif
