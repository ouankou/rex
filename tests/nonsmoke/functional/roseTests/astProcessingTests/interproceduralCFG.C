// Example translator to generate dot files of virtual, interprocedural control
// flow graphs

#include "SgGraphTemplate.h"
#include "graphProcessing.h"
#include "rose.h"
#include "staticCFG.h"
#include <iostream>
#include <memory>
#include <string>

using namespace std;
using namespace Rose;

typedef myGraph CFGforT;

class visitorTraversal : public SgGraphTraversal<CFGforT> {
public:
  int paths = 0;
  void analyzePath(vector<VertexID> &);
};

void visitorTraversal::analyzePath(vector<VertexID> &) { paths++; }

int main(int argc, char *argv[]) {
  // Build the AST used by ROSE
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  Rose_STL_Container<SgNode *> functionDefinitionList =
      NodeQuery::querySubTree(project, V_SgFunctionDefinition);

  int counter = 0;
  for (Rose_STL_Container<SgNode *>::iterator i =
           functionDefinitionList.begin();
       i != functionDefinitionList.end(); i++) {
    SgFunctionDefinition *fnc = isSgFunctionDefinition(*i);
    ROSE_ASSERT(fnc != nullptr);
    SgFunctionDeclaration *functionDeclaration = fnc->get_declaration();
    ROSE_ASSERT(functionDeclaration != nullptr);
    string fileName = functionDeclaration->get_name().str();
    string dotFileName1 = fileName + "." + to_string(counter++) + ".dot";
    StaticCFG::CFG cfg{fnc};
    visitorTraversal vis;
    SgIncidenceDirectedGraph *g = cfg.getGraph();
    std::unique_ptr<myGraph> mg{instantiateGraph(g, cfg)};

    std::cout << dotFileName1 << std::endl;
    VirtualCFG::cfgToDot(fnc, dotFileName1);
    vis.constructPathAnalyzer(mg.get(), true, 0, 0, true);

    std::cout << "function: " << fileName << std::endl;
    std::cout << "paths: " << vis.paths << std::endl;
  }
  SgFunctionDeclaration *mainDefDecl = SageInterface::findMain(project);
  if (mainDefDecl != NULL) {
    SgFunctionDefinition *mainDef = mainDefDecl->get_definition();
    ROSE_ASSERT(mainDef != nullptr);
    visitorTraversal vis;
    StaticCFG::CFG cfg(mainDef);
    string fileName = StringUtility::stripPathFromFileName(
        mainDef->get_file_info()->get_filenameString());
    string dotFileName1 =
        fileName + "." + mainDef->get_declaration()->get_name() + ".dot";

    VirtualCFG::cfgToDot(mainDef, dotFileName1);
    SgIncidenceDirectedGraph *g = cfg.getGraph();
    std::unique_ptr<myGraph> mg{instantiateGraph(g, cfg)};
    vis.constructPathAnalyzer(mg.get(), true, 0, 0, true);
    std::cout << "finished" << std::endl;
    std::cout << "paths: " << vis.paths << std::endl;
  }

  // Another way to query for collections of IR nodes
  VariantVector vv1 = V_SgClassDefinition;
  std::cout << "Number of class definitions in the memory pool is: "
            << NodeQuery::queryMemoryPool(vv1).size() << std::endl;

  // Another way to query for collections of multiple IR nodes.
  // VariantVector(V_SgType) is internally expanded to all IR nodes derived from
  // SgType.
  VariantVector vv2 =
      VariantVector(V_SgClassDefinition) + VariantVector(V_SgType);
  std::cout << "Number of class definitions AND types in the memory pool is: "
            << NodeQuery::queryMemoryPool(vv2).size() << std::endl;

  return 0;
}
