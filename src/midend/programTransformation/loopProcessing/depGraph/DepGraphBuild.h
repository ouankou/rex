
#ifndef BUILD_DEPGRAPH
#define BUILD_DEPGRAPH

#include "DepGraph.h"

#include "DepInfoAnal.h"

#include "ProcessAstTree.h"

#include "SinglyLinkedList.h"

class AstTreeDepGraphBuildImpl {
public:
  virtual GraphAccessInterface::Node *CreateNodeImpl(AstNodePtr s,
                                                     const DomainCond &c) = 0;
  virtual void CreateEdgeImpl(GraphAccessInterface::Node *n1,
                              GraphAccessInterface::Node *n2, DepInfo info) = 0;
  virtual DepInfoConstIterator
  GetDepInfoIteratorImpl(GraphAccessInterface::Edge *e, DepType t) = 0;
  virtual AstNodePtr GetNodeAst(GraphAccessInterface::Node *n) = 0;
  virtual const GraphAccessInterface *Access() const = 0;
  virtual ~AstTreeDepGraphBuildImpl() {}
};

class AstTreeDepGraphAnal {
  DepInfoAnal &impl;

protected:
  AstTreeDepGraphBuildImpl *graph;

public:
  AstTreeDepGraphAnal(AstTreeDepGraphBuildImpl *g, DepInfoAnal &_impl)
      : impl(_impl), graph(g) {}
  struct StmtNodeInfo {
    GraphAccessInterface::Node *node;
    AstNodePtr start;
    StmtNodeInfo(GraphAccessInterface::Node *n, const AstNodePtr &s)
        : node(n), start(s) {}
    StmtNodeInfo() {
      node = 0;
      start = AST_NULL;
    }
  };

  const DomainCond &GetStmtDomain(const AstNodePtr &s) {
    return impl.GetStmtInfo(s).domain;
  }

  void ComputeDataDep(const StmtNodeInfo &n1, const StmtNodeInfo &n2,
                      DepType t = DEPTYPE_ALL);
  void ComputeCtrlDep(const StmtNodeInfo &nc, const StmtNodeInfo &ns,
                      DepType t = DEPTYPE_CTRL);
};

class BuildAstTreeDepGraph : public AstTreeDepGraphAnal,
                             public ProcessAstTree<AstNodePtr> {
private:
  typedef SinglyLinkedListWrap<StmtNodeInfo> StmtStackType;
  StmtStackType stmtNodes, ctrlNodes, gotoNodes;
  bool supportNonFortranLoop;

public:
  BuildAstTreeDepGraph(AstTreeDepGraphBuildImpl *g, DepInfoAnal &_impl,
                       bool supportNonFortranLoop = true)
      : AstTreeDepGraphAnal(g, _impl),
        supportNonFortranLoop(supportNonFortranLoop) {}

  bool ProcessStmt(AstInterface &fa, const AstNodePtr &s) override;
  bool ProcessDecls(AstInterface &fa, const AstNodePtr &s) override {
    return ProcessStmt(fa, s);
  }
  bool ProcessGoto(AstInterface &fa, const AstNodePtr &s,
                   const AstNodePtr &dest) override;
  bool ProcessIf(AstInterface &fa, const AstNodePtr &l, const AstNodePtr &cond,
                 const AstNodePtr &truebody, const AstNodePtr &falsebody,
                 AstInterface::TraversalVisitType t) override;
  bool ProcessLoop(AstInterface &fa, const AstNodePtr &l,
                   const AstNodePtr &body,
                   AstInterface::TraversalVisitType t) override;
  GraphAccessInterface::Node *LastProcessedStmt() {
    return stmtNodes.First()->GetEntry().node;
  }
  void TranslateCtrlDeps();
};

#endif
