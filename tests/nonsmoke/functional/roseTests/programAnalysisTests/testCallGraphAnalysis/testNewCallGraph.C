#include "rose.h"

#include "newCallGraph.h"

#define DEBUG_CALLGRAPH 0

// using namespace NewCallGraph;

int main(int argc, char *argv[]) {
  // This builds the AST.
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  // Call function representing the Call Graph API.
  int status = NewCallGraph::buildCallGraph(project);

  printf("Program Terminated Normally! \n");

  return status;
}
