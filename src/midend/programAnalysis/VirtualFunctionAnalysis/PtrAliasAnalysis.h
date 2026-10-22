#ifndef PTR_ALIAS_ANALYSIS
#define PTR_ALIAS_ANALYSIS
#include "ClassHierarchyGraph.h"

#include "InterProcDataFlowAnalysis.h"

#include "IntraProcAliasAnalysis.h"

//! PtrAliasAnalysis computes Alias Information, which is used for
//! Virtual Function Resolving.
class ROSE_DLL_API PtrAliasAnalysis : public InterProcDataFlowAnalysis {

protected:
  void initializeAnalysis();

  //! The stored callGraph for internal processing
  SgIncidenceDirectedGraph *callGraph;

  //! A map from SgFunctionDeclaration to IntraProcAliasAnalysis
  std::unordered_map<SgFunctionDeclaration *, IntraProcAliasAnalysis *>
      intraAliases;

  //! A map which stores the function call resolve information
  std::unordered_map<SgExpression *, std::vector<SgFunctionDeclaration *>>
      resolver;

  //! ClassHierarchy of the project
  ClassHierarchyWrapper *classHierarchy;

  CallGraphBuilder *cgBuilder;

public:
  //! Enum used for Topological sorting
  enum COLOR { WHITE = 0, GREY, BLACK };
  //! Enum used for identifying traversal type
  enum TRAVERSAL_TYPE { TOPOLOGICAL = 0, REVERSE_TOPOLOGICAL };

  PtrAliasAnalysis(SgProject *__project);
  ~PtrAliasAnalysis();
  void run();
  //! Get the list of function declarations to perform interprocedural
  //! DataFlowAnalysis
  void getFunctionDeclarations(std::vector<SgFunctionDeclaration *> &);
  //! Execute IntraProc Analysis and check whether something changed
  bool runAndCheckIntraProcAnalysis(SgFunctionDeclaration *);

private:
  void SortCallGraphRecursive(
      SgFunctionDeclaration *targetFunction,
      std::unordered_map<SgGraphNode *, PtrAliasAnalysis::COLOR> &colors,
      std::vector<SgFunctionDeclaration *> &processingOrder);

  // Order of performing Dataflow Analysis
  TRAVERSAL_TYPE order;
};

#endif
