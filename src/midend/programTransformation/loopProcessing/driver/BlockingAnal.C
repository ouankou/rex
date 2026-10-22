#include "BlockingAnal.h"

#include "CommandOptions.h"

#include "LoopTreeTransform.h"

DebugLog DebugBlocking("-debugblocking");

static int SliceNestReuseLevel(CompSliceLocalityRegistry *anal,
                               const CompSliceNest &n) {
  unsigned num = n.NumberOfEntries();
  size_t reuseLevel = 0;
  for (; reuseLevel < num; ++reuseLevel) {
    if (anal->TemporaryReuses(n[reuseLevel]) > 0 ||
        anal->SpatialReuses(n[reuseLevel]) > 0)
      break;
  }
  return reuseLevel;
}

static SymbolicVal GetDefaultBlockSize(const CompSlice *slice) {
  AstInterface &fa = LoopTransformInterface::getAstInterface();
  LoopTransformOptions *opt = LoopTransformOptions::GetInstance();
  if (!opt->DoDynamicTuning()) {
    return opt->GetDefaultBlockSize();
  } else {
    int dt = opt->GetDynamicTuningIndex();
    AstInterface::AstNodeList l;
    l.push_back(fa.CreateConstInt(dt).get_ptr());

    CompSlice::ConstLoopIterator iter = slice->GetConstLoopIterator();
    LoopTreeNode *loop = iter.Current();
    SymbolicBound b = loop->GetLoopInfo()->GetBound();
    SymbolicVal size = b.ub - b.lb + 1;

    l.push_back(fa.CreateConstInt(1).get_ptr());
    l.push_back(size.CodeGen(fa).get_ptr());
    std::string funname = "getTuningValue";
    AstNodePtr init = fa.CreateFunctionCall(funname, l.begin(), l.end());
    AstNodePtr symbol;
    std::string name =
        fa.NewVar(fa.GetType("int"), "", true, true, AST_NULL, init, &symbol);
    return SymbolicVar(name, AST_NULL, symbol);
  }
}

const CompSlice *LoopNoBlocking::SetBlocking(
    CompSliceLocalityRegistry *anal,
    const CompSliceDepGraphNode::FullNestInfo &nestInfo) {
  const CompSliceNest *n = nestInfo.GetNest();
  int loopnum = n->NumberOfEntries();
  if (loopnum == 0)
    return 0;
  blocksize.clear();
  for (int i = 0; i < loopnum; ++i)
    blocksize.push_back(1);
  return n->Entry(loopnum - 1);
}

const CompSlice *OuterLoopReuseBlocking ::SetBlocking(
    CompSliceLocalityRegistry *anal,
    const CompSliceDepGraphNode::FullNestInfo &nestInfo) {
  const CompSliceNest &n = *nestInfo.GetNest();
  blocksize.clear();
  size_t num = n.NumberOfEntries();
  int reuseLevel = SliceNestReuseLevel(anal, n);
  for (int i = 0; i < reuseLevel; ++i)
    blocksize.push_back(1);
  size_t index;
  for (index = reuseLevel; index < num - spill; ++index) {
    blocksize.push_back(GetDefaultBlockSize(n[index]));
  }
  for (; index < num; ++index)
    blocksize.push_back(1);
  return n[num - 1];
}

const CompSlice *InnerLoopReuseBlocking ::SetBlocking(
    CompSliceLocalityRegistry *anal,
    const CompSliceDepGraphNode::FullNestInfo &nestInfo) {
  const CompSliceNest &n = *nestInfo.GetNest();
  blocksize.clear();
  unsigned num = n.NumberOfEntries();
  int reuseLevel = SliceNestReuseLevel(anal, n);
  blocksize.resize(reuseLevel + 1, 1);
  for (size_t index = reuseLevel + 1; index < num; ++index)
    blocksize.push_back(GetDefaultBlockSize(n[index]));
  return n[num - 1];
}

const CompSlice *AllLoopReuseBlocking ::SetBlocking(
    CompSliceLocalityRegistry *anal,
    const CompSliceDepGraphNode::FullNestInfo &nestInfo) {
  const CompSliceNest &n = *nestInfo.GetNest();
  blocksize.clear();
  unsigned num = n.NumberOfEntries();
  if (num == 1) {
    blocksize.push_back(1);
    return n[0];
  }
  int reuseLevel = SliceNestReuseLevel(anal, n);
  for (int i = 0; i < reuseLevel; ++i)
    blocksize.push_back(1);
  for (size_t index = reuseLevel; index < num; ++index)
    blocksize.push_back(GetDefaultBlockSize(n[index]));
  return n[num - 1];
}

int LoopBlocking::SetIndex(int num) {
  if (block_index > 1) {
    for (; num >= 0; --num) {
      if (BlockSize(num) != 1)
        break;
    }
    if (num < 0) {
      block_index = 1;
      num = blocksize.size() - 1;
    }
  }
  if (block_index <= 1) {
    for (; num >= 0; num--) {
      if (BlockSize(num) == 1)
        break;
    }
  }
  return num;
}

LoopTreeNode *
LoopBlocking::apply(const CompSliceDepGraphNode::FullNestInfo &nestInfo,
                    LoopTreeDepComp &comp, DependenceHoisting &op,
                    LoopTreeNode *top) {

  if (nestInfo.GetNest()->NumberOfEntries() <= 0)
    return top;

  bool debugloop = DebugBlocking();

  if (debugloop) {
    std::cerr << "\n Apply Loop blocking: from\n";
    top->DumpTree();
  }
  LoopTreeNode *head = ApplyBlocking(nestInfo, comp, op, top);
  if (debugloop) {
    std::cerr << "\n transforming into\n";
    if (head == 0)
      top->DumpTree();
    else
      head->DumpTree();
  }
  return top;
}

LoopTreeNode *
LoopBlocking::ApplyBlocking(const CompSliceDepGraphNode::FullNestInfo &nestInfo,
                            LoopTreeDepComp &comp, DependenceHoisting &op,
                            LoopTreeNode *&top) {
  const CompSliceNest &slices = *nestInfo.GetNest();
  if (DebugBlocking()) {
    std::cerr << "\n Blocking slices: " << slices.toString() << "\n";
  }
  LoopTreeNode *head = 0;
  AstInterface &fa = LoopTransformInterface::getAstInterface();
  for (int j = FirstIndex(); j >= 0; j = NextIndex(j)) {
    top = op.Transform(comp, slices[j], top);
    SymbolicVal b = BlockSize(j);
    if (DebugBlocking()) {
      std::cerr << "\n after slice " << j << " : \n";
      comp.DumpTree();
      comp.DumpDep();
      std::cerr << "\n blocking size for this loop is " << b.toString() << "\n";
    }

    if (!(b == 1)) {
      AstNodePtr symbol;
      std::string name = fa.NewVar(fa.GetType("int"), "", false, true, AST_NULL,
                                   AST_NULL, &symbol);
      LoopTreeNode *n =
          LoopTreeBlockLoop()(top, SymbolicVar(name, AST_NULL, symbol), b);
      if (DebugBlocking()) {
        std::cerr << "\n after tiling loop with size " << b.toString()
                  << " : \n";
        comp.DumpTree();
        comp.DumpDep();
      }
      if (head == 0)
        head = n;
      else {
        while (n->FirstChild() != head)
          LoopTreeSwapNodePos()(n->Parent(), n);
      }
    }
  }
  return head;
}

const CompSliceDepGraphNode::NestInfo *
DoNonPerfectBlocking(const CompSliceDepGraphNode::FullNestInfo &nestInfo) {
  const CompSliceDepGraphNode::NestInfo *innerNest = 0;
  const CompSliceDepGraphNode::NestInfo *nonperfect = nestInfo.InnerNest();
  for (const CompSliceDepGraphNode::NestInfo *p = nonperfect; p != 0;
       p = p->InnerNest()) {
    const CompSliceNest *cur = p->GetNest();
    if (cur != 0) {
      if (innerNest == 0)
        innerNest = p;
      else {
        innerNest = 0;
        break; /*QY: only allow one nest */
      }
    }
  }
  return innerNest;
}
