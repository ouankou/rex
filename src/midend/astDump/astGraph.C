#include "astGraph.h"

#include "sage3basic.h"
using namespace std;

void AST_Graph::writeGraphOfMemoryPoolToFile(
    std::string filename, AST_Graph::pointerHandling graphNullPointers) {
  writeGraphOfMemoryPoolToFile(filename, graphNullPointers,
                               defaultFilterUnary{}, defaultFilterBinary{});
}

void AST_Graph::writeGraphOfAstSubGraphToFile(
    std::string filename, SgNode *node,
    AST_Graph::pointerHandling graphNullPointers, int depth) {
  writeGraphOfAstSubGraphToFile(filename, node, graphNullPointers,
                                defaultFilterUnary{}, defaultFilterBinary{},
                                depth);
}
AST_Graph::defaultFilterUnary::result_type
AST_Graph::defaultFilterUnary::operator()(
    defaultFilterUnary::argument_type nodeFilter) {
  result_type defaultReturn;
  defaultReturn.addToGraph = true;
  if (nodeFilter.first != NULL)
    defaultReturn.DOTLabel = nodeFilter.first->class_name();
  return defaultReturn;
};

AST_Graph::defaultFilterBinary::result_type
AST_Graph::defaultFilterBinary::operator()(
    defaultFilterBinary::first_argument_type nodeFilter,
    defaultFilterBinary::second_argument_type edgeFilter) {
  result_type defaultReturn;
  defaultReturn.addToGraph = true;
  return defaultReturn;
};

AST_Graph::nodePartOfGraph::result_type
AST_Graph::nodePartOfGraph::operator()(pair<SgNode *, std::string> &x) {
  result_type functionalReturn;
  functionalReturn.addToGraph = true;

  SgLocatedNode *locatedNode = isSgLocatedNode(x.first);
  if (locatedNode != NULL) {
    Sg_File_Info *fileInfo = locatedNode->get_file_info();
    ROSE_ASSERT(fileInfo != NULL);
    const std::string filename =
        Rose::utility_stripPathFromFileName(fileInfo->get_filename());
    if (filename == "clang-builtin-c.h" || fileInfo->isCompilerGenerated()) {
      functionalReturn.addToGraph = false;
    }
  }

  return functionalReturn;
}

AST_Graph::filterSgFileInfo::result_type
AST_Graph::filterSgFileInfo::operator()(pair<SgNode *, std::string> &x) {
  result_type functionalReturn;
  functionalReturn.addToGraph = true;

  if (isSg_File_Info(x.first) != NULL)
    functionalReturn.addToGraph = false;

  return functionalReturn;
}

AST_Graph::filterSgFileInfo::result_type
AST_Graph::filterSgFileInfoAndGnuCompatabilityNode::operator()(
    pair<SgNode *, std::string> &x) {
  result_type functionalReturn;
  filterSgFileInfo value1;
  nodePartOfGraph value2;
  functionalReturn.addToGraph = (value1(x).addToGraph && value2(x).addToGraph);

  return functionalReturn;
}
