#include "CallGraph.h"

#include "rose.h"

#include <iostream>
using namespace std;
using namespace Rose;
using namespace Rose::StringUtility;

int main(int argc, char *argv[]) {
  SgProject *project = new SgProject(argc, argv);
  ROSE_ASSERT(project != NULL);

  if (project->get_fileList().size() >= 1) {
    // Construct a call Graph
    CallGraphBuilder CGBuilder(project);
    CGBuilder.buildCallGraph(builtinFilter());

    // Output to a dot file
    AstDOTGeneration dotgen;
    SgFilePtrList file_list = project->get_fileList();
    std::string firstFileName =
        StringUtility::stripPathFromFileName(file_list[0]->getFileName());
    dotgen.writeIncidenceGraphToDOTFile(CGBuilder.getGraph(),
                                        firstFileName + "_callGraph.dot");
  }

  return 0;
}
