#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#ifndef ANALYSIS_COMMON
#define ANALYSIS_COMMON

class SgProject;
class SgIncidenceDirectedGraph;

// initializes the compiler analysis framework
void initAnalysis(SgProject *p);

// Retire analysis state and its graph; AST references remain borrowed.
void clearAnalysis();

// returns the call graph of the current project
SgIncidenceDirectedGraph *getCallGraph();

#endif
#endif
