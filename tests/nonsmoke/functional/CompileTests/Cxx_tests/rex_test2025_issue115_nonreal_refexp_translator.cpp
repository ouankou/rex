#include "nodeQuery.h"
#include "sage3basic.h"
#include "sageInterface.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  auto refs = NodeQuery::querySubTree(project, V_SgNonrealRefExp);
  ROSE_ASSERT(!refs.empty());
  return backend(project);
}
