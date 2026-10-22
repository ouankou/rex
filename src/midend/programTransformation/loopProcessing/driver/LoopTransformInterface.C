#include "LoopTransformInterface.h"

#include "LoopTransformOptions.h"

#include "ProcessAstTree.h"

#include <iostream>

#include <string>

#include "BreakupStmt.h"

#include "LoopUnroll.h"

AstInterface *LoopTransformInterface::fa = 0;
int LoopTransformInterface::configIndex = 0;
AliasAnalysisInterface *LoopTransformInterface::aliasInfo = 0;
FunctionSideEffectInterface *LoopTransformInterface::funcInfo = 0;
ArrayAbstractionInterface *LoopTransformInterface::arrayInfo = 0;

using namespace std;

// This function is defined in TransformComputation.C.
extern bool LoopTransformation(const AstNodePtr &head, AstNodePtr &result);

bool ArrayUseAccessFunction::IsUniqueArray(AstInterface &fa,
                                           const AstNodePtr &array) {
  if (prev != 0 && prev->IsUniqueArray(fa, array))
    return true;
  AstNodeType t;
  if (!fa.IsVarRef(array, &t))
    ROSE_ABORT();

  return false;
}

bool ArrayUseAccessFunction::IsArrayAccess(AstInterface &fa,
                                           const AstNodePtr &s,
                                           AstNodePtr *array,
                                           AstInterface::AstNodeList *index) {
  AstInterface::AstNodeList args;
  if (prev != 0 && prev->IsArrayAccess(fa, s, array, index))
    return true;
  std::string sig;
  AstNodePtr f;
  if (fa.IsFunctionCall(s, &f, &args) && fa.IsVarRef(f, 0, &sig) &&
      sig == funcname) {
    ROSE_ASSERT(!args.empty());
    AstInterface::AstNodeList::const_iterator p = args.begin();
    if (array != 0)
      *array = *p;
    if (index != 0) {
      for (++p; p != args.end(); ++p) {
        index->push_back(*p);
      }
    }
    return true;
  }
  return false;
}

bool ArrayUseAccessFunction::GetArrayBound(AstInterface &fa,
                                           const AstNodePtr &array, int dim,
                                           int &lb, int &ub) {
  if (prev != 0)
    return prev->GetArrayBound(fa, array, dim, lb, ub);
  return fa.GetArrayBound(array, dim, lb, ub);
}

bool ArrayUseAccessFunction::get_modify(AstInterface &fa, const AstNodePtr &fc,
                                        CollectObject<AstNodePtr> *collect) {
  AstInterface::AstNodeList args;
  if (prev1 != 0 && prev1->get_modify(fa, fc, collect))
    return true;
  std::string sig;
  AstNodePtr f;
  if (fa.IsFunctionCall(fc, &f, &args) && fa.IsVarRef(f, 0, &sig) &&
      sig == funcname) {
    ROSE_ASSERT(!args.empty());
    return true;
  }
  return false;
}

bool ArrayUseAccessFunction::get_read(AstInterface &fa, const AstNodePtr &fc,
                                      CollectObject<AstNodePtr> *collect) {
  AstInterface::AstNodeList args;
  if (prev1 != 0 && prev1->get_read(fa, fc, collect))
    return true;
  std::string sig;
  AstNodePtr f;
  if (fa.IsFunctionCall(fc, &f, &args) && fa.IsVarRef(f, 0, &sig) &&
      sig == funcname) {
    ROSE_ASSERT(!args.empty());
    if (collect != 0) {
      AstInterface::AstNodeList::const_iterator argp = args.begin();
      for (++argp; argp != args.end(); ++argp) {
        (*collect)(*argp);
      }
    }
    return true;
  }
  return false;
}

SymbolicVal
ArrayUseAccessFunction::CreateArrayAccess(const SymbolicVal &arr,
                                          const SymbolicVal &index) {
  return SymbolicFunction(AstInterface::OP_ARRAY_ACCESS, funcname, arr, index);
}

AstNodePtr
LoopTransformInterface::CreateArrayAccess(const AstNodePtr &arr,
                                          AstInterface::AstList &index) {
  assert(fa != 0);
  AstNodePtr res;
  if (arrayInfo != 0) {
    res = arrayInfo->CreateArrayAccess(*fa, arr, index);
  }
  if (res == AST_NULL) {
    res = arr;
    for (AstNodeList::const_reverse_iterator p = index.rbegin();
         p != index.rend(); ++p) {
      res = AstInterface::CreateArrayAccess(res, *p);
    }
  }
  return res;
}

AstNodePtr ArrayUseAccessFunction::CreateArrayAccess(AstInterface &fa,
                                                     const AstNodePtr &arr,
                                                     const AstNodeList &index) {
  AstNodePtr r;
  if (prev != 0)
    r = prev->CreateArrayAccess(fa, arr, index);
  if (r == AST_NULL) {
    AstNodeList nindex;
    nindex.push_back(arr.get_ptr());
    for (AstNodeList::const_iterator p = index.begin(); p != index.end(); ++p)
      nindex.push_back(*p);
    r = fa.CreateFunctionCall(funcname, nindex.begin(), nindex.end());
  }
  return r;
}

AstNodePtr LoopTransformInterface::CreateArrayAccess(
    const std::string &arrname, const std::vector<SymbolicVal> &arrindex) {
  assert(fa != 0);
  AstNodePtr res = fa->CreateVarRef(arrname);
  AstNodeList args;
  for (std::vector<SymbolicVal>::const_iterator indexp = arrindex.begin();
       indexp != arrindex.end(); ++indexp) {
    AstNodePtr cur = (*indexp).CodeGen(*fa);
    args.push_back(cur.get_ptr());
  }
  return CreateArrayAccess(res, args);
}

class LoopTransformationWrap : public TransformAstTree {
public:
  bool operator()(AstInterface &fa, const AstNodePtr &head,
                  AstNodePtr &result) {
    if (!fa.IsStatement(head))
      return false;
    fa.SetRoot(head);
    return LoopTransformation(head, result);
  }
};

void LoopTransformInterface::set_astInterface(AstInterface &_fa) { fa = &_fa; }

void LoopTransformInterface::cmdline_configure(std::vector<std::string> &argv) {
  std::vector<std::string> unknown;

  LoopUnrolling::cmdline_configure(argv, &unknown);
  argv.clear();
  BreakupStatement::cmdline_configure(unknown, &argv);
  unknown.clear();
  LoopTransformOptions::GetInstance()->SetOptions(argv, &unknown);
  argv.clear();

  for (unsigned index = 0; index < unknown.size(); ++index) {
    const std::string &opt = unknown[index];
    if (opt == "-arracc") {
      if (index + 1 == unknown.size() || unknown[index + 1].empty() ||
          unknown[index + 1][0] == '-') {
        std::cerr << "REX_LOOP_OPTION[array-access]: -arracc requires a "
                     "function name\n";
        ROSE_ABORT();
      }
      const std::string &name = unknown[++index];
      ArrayUseAccessFunction *r =
          new ArrayUseAccessFunction(name, arrayInfo, funcInfo);
      funcInfo = r;
      arrayInfo = r;
    } else if (opt == "-poet" || opt == "-bk_poet" || opt == "-cp_poet" ||
               opt == "-par_poet") {
      std::cerr << "REX_LOOP_OPTION[unsupported-poet]: " << opt
                << " requires the removed POET backend\n";
      ROSE_ABORT();
    } else {
      argv.push_back(opt);
    }
  }
}

AstNodePtr LoopTransformInterface::TransformTraverse(AstInterfaceImpl &scope,
                                                     const AstNodePtr &head) {
  ROSE_ASSERT(aliasInfo != nullptr);
  AstInterface _fa(&scope);
  fa = &_fa;

  NormalizeForLoop(_fa, head);
  AstNodePtr result = head;
  if (BreakupStatement::get_breaksize() > 0) {
    BreakupStatement bs;
    result = bs(head);
  }
  _fa.SetRoot(result);

  LoopTransformationWrap op;
  result = TransformAstTraverse(_fa, result, op, AstInterface::PreVisit);
  if (LoopUnrolling::get_unrollsize() > 1)
    result = LoopUnrolling()(result);
  _fa.SetRoot(result);

  fa = 0;
  return result;
}

void LoopTransformInterface::PrintTransformUsage(std::ostream &out) {
  out << "-debugloop: print debugging information for loop transformations; \n"
      << "-debugdep: print debugging information for dependence analysis; \n"
      << "-tmloop: print timing information for loop transformations; \n"
      << "-arracc <funcname>: use function <funcname> to denote "
         "multi-dimensional array access;\n"
      << "opt <level=0>: the level of loop optimizations to apply; by default, "
         "only the outermost level is optimized;\n"
      << LoopUnrolling::cmdline_help() << std::endl
      << BreakupStatement::cmdline_help() << std::endl;
  LoopTransformOptions::GetInstance()->PrintUsage(out);
}

bool LoopTransformInterface::GetFunctionCallSideEffect(
    const AstNodePtr &fc, CollectObject<AstNodePtr> &collectmod,
    CollectObject<AstNodePtr> &collectread) {
  ROSE_ASSERT(fa != nullptr);
  return funcInfo != 0 && funcInfo->get_modify(*fa, fc, &collectmod) &&
         funcInfo->get_read(*fa, fc, &collectread);
}

AstNodePtr LoopTransformInterface::CreateDynamicFusionConfig(
    const AstNodePtr &groupNum, AstInterface::AstNodeList &args, int &id) {
  ROSE_ASSERT(fa != nullptr);
  ++configIndex;
  args.push_back(fa->CreateConstInt(configIndex).get_ptr());
  args.push_back(fa->CreateConstInt(args.size()).get_ptr());
  std::string funname = "DynamicFusionConfig";
  AstNodePtr invoc = fa->CreateFunctionCall(funname, args.begin(), args.end());
  return fa->CreateAssignment(groupNum, invoc);
}

AstNodePtr LoopTransformInterface::CreateDynamicFusionEnd(int id) {
  ROSE_ASSERT(fa != nullptr);
  AstInterface::AstNodeList args;
  args.push_back(fa->CreateConstInt(id).get_ptr());
  std::string funname = "DynamicFusionEnd";
  return fa->CreateFunctionCall(funname, args.begin(), args.end());
}

bool LoopTransformInterface::IsDynamicFusionConfig(
    const AstNodePtr &n, AstNodePtr *configvar, int *configID,
    AstInterface::AstNodeList *params) {
  ROSE_ASSERT(fa != nullptr);
  AstNodePtr invoc;
  if (!fa->IsAssignment(n, configvar, &invoc))
    return false;
  AstInterface::AstNodeList args;
  std::string sig;
  AstNodePtr f;
  if (!fa->IsFunctionCall(invoc, &f, &args) || !fa->IsVarRef(f, 0, &sig))
    return false;
  if (sig != "DynamicFusionConfig")
    return false;
  ROSE_ASSERT(args.size() >= 2);
  int id = 0;
  int count = 0;
  ROSE_ASSERT(fa->IsConstInt(args[args.size() - 2], &id));
  ROSE_ASSERT(fa->IsConstInt(args.back(), &count));
  ROSE_ASSERT(count >= 0 && static_cast<size_t>(count) == args.size() - 1);
  if (configID != nullptr)
    *configID = id;
  if (params != nullptr)
    params->assign(args.begin(), args.end() - 2);
  return true;
}

bool LoopTransformInterface::IsDynamicFusionEnd(const AstNodePtr &n) {
  ROSE_ASSERT(fa != nullptr);
  std::string sig;
  AstNodePtr f;
  AstInterface::AstNodeList args;
  if (!fa->IsFunctionCall(n, &f, &args) || !fa->IsVarRef(f, 0, &sig))
    return false;
  if (sig != "DynamicFusionEnd")
    return false;
  ROSE_ASSERT(args.size() == 1);
  int id = 0;
  ROSE_ASSERT(fa->IsConstInt(args.front(), &id));
  return true;
}
