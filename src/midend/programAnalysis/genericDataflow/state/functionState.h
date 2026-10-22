#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#if !defined(FUNCTION_STATE_H) && !defined(NO_FUNCTION_STATE_H)
#define FUNCTION_STATE_H

#include "CallGraphTraverse.h"

#include "analysisCommon.h"

#include "nodeState.h"

#include <map>

#include <set>

class FunctionState {
  friend class CollectFunctions;

public:
  Function func;
  NodeState state;
  // The lattices that describe the value of the function's return variables
  NodeState retState;

private:
  static std::set<FunctionState *> allDefinedFuncs;
  static std::set<FunctionState *> allFuncs;
  static bool allFuncsComputed;

public:
  FunctionState(Function &func) : func(func), state() {}

  Function &getFunc();

  // returns a set of all the functions whose bodies are in the project
  static std::set<FunctionState *> &getAllDefinedFuncs();

  // returns a set of all the functions whose declarations are in the project
  static std::set<FunctionState *> &getAllFuncs();

  // releases cached FunctionState objects and resets cached lookup sets
  static void clearFunctionStateCache();

  // returns the FunctionState associated with the given function
  // func may be any defined function
  static FunctionState *getDefinedFuncState(const Function &func);

  // returns the FunctionState associated with the given function
  // func may be any declared function
  static FunctionState *getFuncState(const Function &func);

  // given a function call, sets argParamMap to map all simple arguments to this
  // function to their corresponding parameters
  static void setArgParamMap(SgFunctionCallExp *call,
                             std::map<varID, varID> &argParamMap);

  // Map each parameter to its corresponding argument.
  static void setParamArgByRefMap(SgFunctionCallExp *call,
                                  std::map<varID, varID> &paramArgByRefMap);
};

class CollectFunctions : public TraverseCallGraphUnordered {
public:
  CollectFunctions(SgIncidenceDirectedGraph *graph)
      : TraverseCallGraphUnordered(graph) {}

  void visit(const CGFunction *func);

  virtual ~CollectFunctions() {}
};

#endif
#endif
