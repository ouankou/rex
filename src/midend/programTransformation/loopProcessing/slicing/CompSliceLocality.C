#include "CompSliceLocality.h"

#include "CompSliceRegistry.h"

#include "LoopTreeDepComp.h"

#include "DepInfoAnal.h"

// not precise since inter-statement spatial reuses not considered
float CompSliceLocalityAnal::SpatialReuses(const CompSlice *slice) {
  float reuse = 0;
  CompSlice::ConstLoopIterator sliceIter = slice->GetConstLoopIterator();
  for (LoopTreeNode *n; (n = sliceIter.Current()); sliceIter++) {
    int level = n->LoopLevel();
    CompSlice::ConstStmtIterator iter1 = sliceIter.GetConstStmtIterator();
    for (LoopTreeNode *s; (s = iter1.Current()); iter1++) {
      reuse += anal.SelfSpatialReuses(s, level, linesize);
    }
  }
  return reuse;
}

int CompSliceLocalityAnal::TemporaryReuses(const CompSlice *slice1,
                                           const CompSlice *slice2,
                                           AstNodeSet &refSet) {
  CompSlice::ConstStmtIterator stmtIter2 = slice2->GetConstStmtIterator();
  for (LoopTreeNode *n2; (n2 = stmtIter2.Current()); stmtIter2++) {
    LoopTreeNode *l2 = stmtIter2.CurrentInfo().loop;
    int level2 = l2->LoopLevel();
    CompSlice::ConstStmtIterator stmtIter1 = slice1->GetConstStmtIterator();
    for (LoopTreeNode *n1; (n1 = stmtIter1.Current()); stmtIter1++) {
      LoopTreeNode *l1 = stmtIter1.CurrentInfo().loop;
      int level1 = l1->LoopLevel();
      anal.TemporaryReuseRefs(n1, level1, n2, level2, refSet, reuseDist);
    }
  }
  return refSet.size();
}

class CollectArrayRefs : public CollectObject<AstNodePtr> {
  CompSliceLocalityAnal::AstNodeSet &result;

public:
  CollectArrayRefs(CompSliceLocalityAnal::AstNodeSet &r) : result(r) {}
  bool operator()(const AstNodePtr &r) {
    AstNodePtr arr;
    if (LoopTransformInterface::IsArrayAccess(r, &arr)) {
      result.insert(arr);
      return true;
    }
    return false;
  }
};

class CollectRegisteredArrayRefs : public CollectObject<AstNodePtr> {
  const CompSliceLocalityAnal::AstNodeSet &reg;
  CompSliceLocalityAnal::AstNodeSet &refset;

public:
  CollectRegisteredArrayRefs(const CompSliceLocalityAnal::AstNodeSet &r,
                             CompSliceLocalityAnal::AstNodeSet &s)
      : reg(r), refset(s) {}
  bool operator()(const AstNodePtr &r) {
    AstNodePtr arr;
    AstInterface &fa = LoopTransformInterface::getAstInterface();
    if (LoopTransformInterface::IsArrayAccess(r, &arr)) {
      for (const AstNodePtr &registered : reg) {
        if (fa.IsSameVarRef(arr, registered)) {
          refset.insert(r);
          return true;
        }
      }
    }
    return false;
  }
};

int CompSliceLocalityAnal::SpatialReuses(const CompSlice *slice1,
                                         const CompSlice *slice2,
                                         AstNodeSet &refSet) {
  AstInterface &fa = LoopTransformInterface::getAstInterface();
  AstNodeSet arrays;
  CompSlice::ConstStmtIterator stmtIter1 = slice1->GetConstStmtIterator();
  CollectArrayRefs op(arrays);
  for (LoopTreeNode *n1; (n1 = stmtIter1.Current()); stmtIter1++) {
    AstNodePtr s1 = n1->GetRuntimeAst();
    if (s1 != AST_NULL)
      AnalyzeConditionRefs(fa, s1, op, op);
  }
  CollectRegisteredArrayRefs op2(arrays, refSet);
  CompSlice::ConstStmtIterator stmtIter2 = slice2->GetConstStmtIterator();
  for (LoopTreeNode *n2; (n2 = stmtIter2.Current()); stmtIter2++) {
    AstNodePtr s2 = n2->GetRuntimeAst();
    if (s2 != AST_NULL)
      AnalyzeConditionRefs(fa, s2, op2, op2);
  }
  return refSet.size();
}

inline std::string
toString(const CompSliceLocalityRegistry::SliceSelfInfo &info) {
  return info.toString();
}

inline std::string
toString(const CompSliceLocalityRegistry::SliceRelInfo &info) {
  return info.toString();
}

class CompSliceLocalityRegistry::Impl
    : public CompSliceRegistry<SliceSelfInfo, SliceRelInfo,
                               CompSliceLocalityAnal> {
public:
  Impl(CompSliceLocalityAnal &a)
      : CompSliceRegistry<SliceSelfInfo, SliceRelInfo, CompSliceLocalityAnal>(
            a) {}
};

CompSliceLocalityRegistry::CompSliceLocalityRegistry(LoopTreeLocalityAnal &a,
                                                     unsigned ls,
                                                     unsigned reuseDist)
    : CompSliceLocalityAnal(a, ls, reuseDist) {
  impl = new Impl(*this);
}

CompSliceLocalityRegistry::~CompSliceLocalityRegistry() { delete impl; }

int CompSliceLocalityRegistry::TemporaryReuses(const CompSlice *slice1,
                                               const CompSlice *slice2,
                                               AstNodeSet *refset) {
  return (slice1 == slice2)
             ? impl->CreateNode(slice1)->GetInfo().TemporaryReuses(refset)
             : impl->CreateEdge(slice1, slice2)
                   ->GetInfo()
                   .TemporaryReuses(refset);
}

int CompSliceLocalityRegistry::SpatialReuses(const CompSlice *slice1,
                                             const CompSlice *slice2) {
  assert(slice1 != slice2);
  return impl->CreateEdge(slice1, slice2)->GetInfo().SpatialReuses();
}

float CompSliceLocalityRegistry::SpatialReuses(const CompSlice *slice) {
  return impl->CreateNode(slice)->GetInfo().SpatialReuses();
}

int CompSliceLocalityRegistry::TemporaryReuses(const CompSlice *slice) {
  return impl->CreateNode(slice)->GetInfo().TemporaryReuses();
}
