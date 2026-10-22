#include "rose.h"

#include "newCallGraph.h"

#include <fstream>
#include <memory>

#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  // A writer that exits without releasing its lock must not block later runs.
  const pid_t child = fork();
  ROSE_ASSERT(child >= 0);
  if (child == 0) {
    alarm(10);
    (void)NewCallGraph::getLock();
    _exit(0);
  }
  int child_status = 0;
  ROSE_ASSERT(waitpid(child, &child_status, 0) == child);
  ROSE_ASSERT(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
  alarm(10);
  const int recovered_lock = NewCallGraph::getLock();
  alarm(0);
  NewCallGraph::releaseLock(recovered_lock);

  const char *emptyGraphPath = "rex_callgraph_empty.bin";
  {
    std::ofstream emptyGraph(emptyGraphPath, std::ios::binary);
    ROSE_ASSERT(emptyGraph.good());
  }
  std::unique_ptr<std::vector<NewCallGraph::CallGraphFileStructure>> emptyGraph(
      NewCallGraph::readCallGraphFile(emptyGraphPath));
  ROSE_ASSERT(emptyGraph != nullptr && emptyGraph->empty());
  ROSE_ASSERT(std::remove(emptyGraphPath) == 0);

  // This builds the AST.
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  // Call function representing the Call Graph API.
  int status = NewCallGraph::buildCallGraph(project);

  printf("Program Terminated Normally! \n");

  return status;
}
