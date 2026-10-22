#include <sstream>

#include <stdlib.h>

#include "CommandOptions.h"

#include "DepGraphTransform.h"

#include "LoopTransformInterface.h"

#include "LoopTreeLocality.h"

#include "ReuseAnalysis.h"

#include "StmtDepAnal.h"

#include "StmtInfoCollect.h"

bool DebugRefGraph() {
  static int r = 0;
  if (r == 0) {
    r = CmdOptions::GetInstance()->HasOption("-debugrefgraph") ? 1 : -1;
  }
  return r == 1;
}

LoopTreeLocalityAnal ::LoopTreeLocalityAnal(LoopTreeDepCompCreate &c)
    : comp(c), anal(comp.GetDepAnal()), inputCreate(c.GetTreeNodeMap()) {
  comp.GetLoopTreeCreate()->AttachObserver(inputCreate);
  LoopTreeNode *root = comp.GetLoopTreeRoot();
  DepCompAstRefAnal stmtorder(comp, root);
  for (auto p1 = comp.GetDepGraph()->GetNodeIterator(); !p1.ReachEnd(); ++p1) {
    LoopTreeDepGraphNode *n1 = p1.Current();
    if (n1->GetInfo()->GetRuntimeAst() == AST_NULL)
      continue;
    for (auto p2 = p1; !p2.ReachEnd(); ++p2) {
      LoopTreeDepGraphNode *n2 = p2.Current();
      if (n2->GetInfo()->GetRuntimeAst() == AST_NULL)
        continue;
      ComputeInputDep(n1, n2, stmtorder);
    }
  }
}

LoopTreeLocalityAnal ::~LoopTreeLocalityAnal() {
  comp.GetLoopTreeCreate()->DetachObserver(inputCreate);
}

void LoopTreeLocalityAnal ::ComputeInputDep(LoopTreeDepGraphNode *n1,
                                            LoopTreeDepGraphNode *n2,
                                            DepCompAstRefAnal &stmtorder) {
  if (!inputCreate.ContainNode(n1))
    inputCreate.AddNode(n1);
  if (!inputCreate.ContainNode(n2))
    inputCreate.AddNode(n2);
  if (stmtorder.CompareStmt(comp.GetTreeNode(n1), comp.GetTreeNode(n2)) > 0) {
    LoopTreeDepGraphNode *tmp = n1;
    n1 = n2;
    n2 = tmp;
  }
  GraphCrossEdgeIterator<LoopTreeDepGraph> p(&inputCreate, n1, n2);
  if (!p.ReachEnd())
    return;

  inputCreate.BuildDep(anal, n1, n2, DEPTYPE_INPUT);
}

class AccumulateSpatialReuse : public CollectObject<AstNodePtr> {
  float res;
  const SymbolicVar &induction;
  int linesize;

public:
  bool operator()(const AstNodePtr &cur) {
    res += SelfSpatialReuse(cur, induction, linesize);
    return true;
  }
  AccumulateSpatialReuse(const SymbolicVar &_induction, int _linesize)
      : res(0), induction(_induction), linesize(_linesize) {}
  float get_result() const { return res; }
};

float LoopTreeLocalityAnal ::SelfSpatialReuses(LoopTreeNode *n, int loop,
                                               int linesize) {
  AstInterface &fa = LoopTransformInterface::getAstInterface();
  int loop1 = comp.GetDepNode(n)->LoopTreeDim2AstTreeDim(loop);
  AstNodePtr s = n->GetRuntimeAst();
  ASSERT_require(s != AST_NULL);
  const SymbolicVar &induction = anal.GetStmtInfo(s).ivars.at(loop1);
  AccumulateSpatialReuse col(induction, linesize);
  AnalyzeConditionRefs(fa, s, col, col);
  return col.get_result();
}

class MapSrcSinkLooplevel : public Map2Object<AstNodePtr, DepDirection, int> {
  int srclevel, snklevel;
  int operator()(const AstNodePtr &, const DepDirection &dir) {
    return (dir == DEP_SRC) ? srclevel : snklevel;
  }

public:
  MapSrcSinkLooplevel(int _srclevel, int _snklevel)
      : srclevel(_srclevel), snklevel(_snklevel) {}
};

int LoopTreeLocalityAnal ::TemporaryReuseRefs(LoopTreeNode *s1, int loop1,
                                              LoopTreeNode *s2, int loop2,
                                              AstNodeSet &refSet,
                                              int reuseDist) {
  LoopTreeDepGraphNode *n1 = comp.GetDepNode(s1), *n2 = comp.GetDepNode(s2);

  LoopTreeDepGraph *graph = comp.GetDepGraph();
  GraphCrossEdgeIterator<LoopTreeDepGraph> iter1(graph, n1, n2);
  GraphCrossEdgeIterator<LoopTreeDepGraph> iter2(&inputCreate, n1, n2);
  MultiIterator<DepInfoEdge *, GraphCrossEdgeIterator<LoopTreeDepGraph>> iter3(
      iter1, iter2);
  DepInfoEdgeConstInfoIterator<
      MultiIterator<DepInfoEdge *, GraphCrossEdgeIterator<LoopTreeDepGraph>>>
      iter4(iter3);
  MapSrcSinkLooplevel loopmap(loop1, loop2);

  AppendSTLSet<AstNodePtr> col(refSet);
  ::TemporaryReuseRefs(iter4, loopmap, col, &reuseDist);
  return refSet.size();
}

std::string DepCompAstRef::toString() const {
  std::stringstream out;
  out << AstInterface::AstToString(orig) << " : " << stmt << ":"
      << stmt->toString();
  return out.str();
}

DepCompAstRefGraphNode *
DepCompAstRefGraphCreate::CreateNode(const AstNodePtr &r, LoopTreeNode *stmt) {
  DepCompAstRef cur(r, stmt);
  AstRefNodeMap::const_iterator rp = refNodeMap.find(cur);

  DepCompAstRefGraphNode *rnode = 0;
  if (rp == refNodeMap.end()) {
    rnode = new DepCompAstRefGraphNode(this, cur);
    AddNode(rnode);
    refNodeMap[cur] = rnode;
  } else
    rnode = (*rp).second;
  return rnode;
}

DepInfoEdge *DepCompAstRefGraphCreate::CreateEdge(DepCompAstRefGraphNode *src,
                                                  DepCompAstRefGraphNode *snk,
                                                  const DepInfo &dep) {
  DepInfoEdge *e = new DepInfoEdge(this, dep);
  AddEdge(src, snk, e);
  if (DebugRefGraph()) {
    std::cerr << "creating edge in ref graph: " << src->toString() << "->"
              << snk->toString() << " : " << e->toString() << std::endl;
  }
  return e;
}

template <class Graph, class EdgeIterator>
void CreateEdgeWrap(DepCompAstRefGraphCreate *res, LoopTreeDepComp &comp,
                    Graph *g, EdgeIterator deps) {
  for (; !deps.ReachEnd(); ++deps) {
    DepInfoEdge *cur = deps.Current();
    const DepInfo &curdep = cur->GetInfo();
    LoopTreeDepGraphNode *depsrc =
        g->GetEdgeEndPoint(cur, GraphAccess::EdgeOut);
    LoopTreeDepGraphNode *depsnk = g->GetEdgeEndPoint(cur, GraphAccess::EdgeIn);
    AstNodePtr src = curdep.SrcRef(), snk = curdep.SnkRef();
    if (src == 0 && snk == 0)
      continue;
    if (LoopTransformInterface::IsArrayAccess(src) ||
        LoopTransformInterface::IsArrayAccess(snk)) {
      DepCompAstRefGraphNode *srcnode =
          res->CreateNode(src, comp.GetTreeNode(depsrc));
      DepCompAstRefGraphNode *snknode =
          res->CreateNode(snk, comp.GetTreeNode(depsnk));
      ;
      res->CreateEdge(srcnode, snknode, curdep);
    }
  }
}

void DepCompAstRefGraphCreate::Build(LoopTreeLocalityAnal &tc,
                                     LoopTreeNode *root) {
  LoopTreeDepComp &comp = tc.GetDepComp();
  LoopTreeDepGraphSubtree depgraph(root, comp.GetDepGraph());
  GraphEdgeIterator<LoopTreeDepGraphSubtree> edgep(&depgraph);
  CreateEdgeWrap(this, comp, &depgraph, edgep);

  LoopTreeDepGraphSubtree inputgraph(root, tc.GetInputGraph());
  GraphEdgeIterator<LoopTreeDepGraphSubtree> edgep1(&inputgraph);
  CreateEdgeWrap(this, comp, &inputgraph, edgep1);
}

void DepCompAstRefAnal::Append(const LoopTreeDepComp &comp,
                               LoopTreeNode *_root) {
  std::function<bool(AstNodePtr, AstNodePtr)> modcollect =
      [this](AstNodePtr mod_first, AstNodePtr mod_second) {
        if (refmap.find(mod_first) == refmap.end()) {
          int num = refmap.size();
          refmap[mod_first] = -(num + 1);
          return true;
        } else if (refmap[mod_first] > 0)
          refmap[mod_first] = -refmap[mod_first];
        return false;
      };
  std::function<bool(AstNodePtr, AstNodePtr)> readcollect =
      [this](AstNodePtr read_first, AstNodePtr read_second) {
        if (refmap.find(read_first) == refmap.end()) {
          int num = refmap.size();
          refmap[read_first] = num + 1;
          return true;
        }
        return false;
      };
  int stmtnum = stmtmap.size();
  AstInterface &fa = LoopTransformInterface::getAstInterface();
  for (LoopTreeTraverse stmts(_root, LoopTreeTraverse::PostOrder);
       !stmts.ReachEnd(); stmts.Advance(), ++stmtnum) {
    LoopTreeNode *curstmt = stmts.Current();
    assert(stmtmap.find(curstmt) == stmtmap.end());
    stmtmap[curstmt] = stmtnum;
  }
  LoopTreeDepGraphSubtree graph(_root, comp.GetDepGraph());
  for (auto nodes = graph.GetNodeIterator(); !nodes.ReachEnd(); ++nodes) {
    LoopTreeNode *curstmt = comp.GetTreeNode(nodes.Current());
    AstNodePtr evaluation = curstmt->GetRuntimeAst();
    if (evaluation == AST_NULL)
      continue;
    StmtSideEffectCollect effect(
        fa, LoopTransformInterface::getSideEffectInterface());
    effect.set_modify_collect(modcollect);
    effect.set_read_collect(readcollect);
    effect(evaluation);
  }
}

bool DepCompAstRefGraphCreate::SelfReuseLevel(const DepCompAstRefGraphNode *n,
                                              int level) const {
  GraphCrossEdgeIterator<DepCompAstRefGraphCreate> edges(this, n, n);
  for (; !edges.ReachEnd(); ++edges) {
    int cur = (*edges)->GetInfo().CarryLevel();
    if (cur == level)
      return true;
  }
  return false;
}
