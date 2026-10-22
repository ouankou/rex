#include "DependenceGraph.h"
#include "SlicingInfo.h"
#include "rose.h"

#include <memory>

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  SlicingInfo si;
  si.traverse(project, preorder);

  std::unique_ptr<SystemDependenceGraph> sdg(new SystemDependenceGraph());
  sdg->parseProject(project);

  AstTests::runAllTests(project);

  sdg.reset(new SystemDependenceGraph());
  sdg->parseProject(project);

  project->unparse();
}
