#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#include "functionState.h"

#include <map>
using std::map;
#include <set>
using std::set;

Function &FunctionState::getFunc() { return func; }

set<FunctionState *> FunctionState::allDefinedFuncs;
set<FunctionState *> FunctionState::allFuncs;
bool FunctionState::allFuncsComputed = false;

void FunctionState::clearFunctionStateCache() {
  set<FunctionState *> toDelete = allFuncs;
  toDelete.insert(allDefinedFuncs.begin(), allDefinedFuncs.end());
  for (set<FunctionState *>::iterator it = toDelete.begin();
       it != toDelete.end(); ++it) {
    delete *it;
  }

  allDefinedFuncs.clear();
  allFuncs.clear();
  allFuncsComputed = false;
}

namespace {
struct FunctionStateCleanupAtExit {
  ~FunctionStateCleanupAtExit() { FunctionState::clearFunctionStateCache(); }
};

FunctionStateCleanupAtExit functionStateCleanupAtExit;

bool allOmittedParametersHaveDefaults(
    const SgInitializedNamePtrList &params,
    SgExpressionPtrList::size_type firstOmittedIndex) {
  ROSE_ASSERT(firstOmittedIndex <= params.size());
  for (SgInitializedNamePtrList::size_type i = firstOmittedIndex;
       i < params.size(); ++i) {
    if (params[i]->get_initializer() == NULL)
      return false;
  }
  return true;
}
} // namespace

// returns a set of all the functions whose bodies are in the project
set<FunctionState *> &FunctionState::getAllDefinedFuncs() {
  if (allFuncsComputed)
    return allDefinedFuncs;
  CollectFunctions collect(getCallGraph());
  collect.traverse();
  allFuncsComputed = true;
  return allDefinedFuncs;
}

// returns a set of all the functions whose declarations are in the project
set<FunctionState *> &FunctionState::getAllFuncs() {
  if (!allFuncsComputed) {
    CollectFunctions collect(getCallGraph());
    collect.traverse();
    allFuncsComputed = true;
  }

  return allFuncs;
}

// returns the FunctionState associated with the given function
// func may be any defined function
FunctionState *FunctionState::getDefinedFuncState(const Function &func) {
  for (set<FunctionState *>::iterator it = allDefinedFuncs.begin();
       it != allDefinedFuncs.end(); it++)
    if ((*it)->func == func)
      return *it;
  return NULL;
}

// returns the FunctionState associated with the given function
// func may be any declared function
FunctionState *FunctionState::getFuncState(const Function &func) {
  if (!allFuncsComputed) {
    getAllFuncs();
  }

  for (set<FunctionState *>::iterator it = allFuncs.begin();
       it != allFuncs.end(); it++)
    if ((*it)->func == func)
      return *it;
  return NULL;
}

// given a function call, sets argParamMap to map all simple arguments to this
// function to their corresponding parameters
void FunctionState::setArgParamMap(SgFunctionCallExp *call,
                                   map<varID, varID> &argParamMap) {
  Function func(call);

  SgExpressionPtrList args = call->get_args()->get_expressions();
  SgInitializedNamePtrList params = func.get_params();
  SgFunctionType *funcType =
      func.get_declaration() ? func.get_declaration()->get_type() : NULL;
  bool hasEllipses = funcType && funcType->get_has_ellipses();
  bool variadicCallWithSurplus = hasEllipses && args.size() >= params.size();
  bool callOmitsOnlyDefaultedParams =
      args.size() < params.size() &&
      allOmittedParametersHaveDefaults(params, args.size());
  ROSE_ASSERT(args.size() == params.size() || variadicCallWithSurplus ||
              callOmitsOnlyDefaultedParams);

  SgExpressionPtrList::iterator itA;
  SgInitializedNamePtrList::iterator itP;
  for (itA = args.begin(), itP = params.begin();
       itA != args.end() && itP != params.end(); ++itA, ++itP) {
    varID arg = SgExpr2Var(*itA);
    varID param(*itP);
    argParamMap[arg] = param;
  }
}

// Map each parameter to its corresponding argument.
void FunctionState::setParamArgByRefMap(SgFunctionCallExp *call,
                                        map<varID, varID> &paramArgByRefMap) {
  Function func(call);

  SgExpressionPtrList args = call->get_args()->get_expressions();
  SgInitializedNamePtrList params = func.get_params();

  SgExpressionPtrList::iterator itArgs;
  SgInitializedNamePtrList::iterator itParams;
  for (itParams = params.begin(), itArgs = args.begin();
       itParams != params.end() && itArgs != args.end(); itParams++, itArgs++) {
    varID argVar = SgExpr2Var(*itArgs);
    varID paramVar(*itParams);

    paramArgByRefMap[paramVar] = argVar;
  }
}

void CollectFunctions::visit(const CGFunction *cgFunc) {
  Function func(*cgFunc);
  FunctionState *fs = new FunctionState(func);

  // if the function has a body
  if (func.get_definition())
    FunctionState::allDefinedFuncs.insert(fs);
  FunctionState::allFuncs.insert(fs);
}

#endif
