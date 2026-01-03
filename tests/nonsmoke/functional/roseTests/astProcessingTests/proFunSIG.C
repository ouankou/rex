#include <iostream>
#include <fstream>
// #include <rose.h>
#include "SgGraphTemplate.h"
#include "graphProcessing.h"
#include "staticCFG.h"
#include <err.h>
#include <memory>
#include <string>
/* Testing the graph traversal mechanism now implementing in AstProcessing.h
 * (inside src/midend/astProcessing/)*/
#include <sys/time.h>
#include <sys/resource.h>
using namespace std;
using namespace Rose;







typedef myGraph CFGforT;






class visitorTraversal : public SgGraphTraversal<CFGforT>
   {
     public:
          int tltnodes;
	  int paths;
          myGraph* orig;
          std::vector<SgFunctionDefinition*> defs;
          std::vector<string> defstr;
          //std::vector<std::vector<VertexID> > pathstore;
	  void analyzePath(vector<VertexID>& pth);
   };


void visitorTraversal::analyzePath(vector<VertexID>& pth) {
    std::vector<VertexID> pathR = pth;
    std::vector<SgGraphNode*> path;
    for (unsigned int j = 0; j < pathR.size(); j++) {
        SgGraphNode* R = (*orig)[pathR[j]].sg;
        path.push_back(R);
    }
    for (unsigned int k = 0; k < path.size(); k++) {
        if (isSgFunctionRefExp(path[k]->get_SgNode())) {
            SgFunctionRefExp* sfrd = isSgFunctionRefExp(path[k]->get_SgNode());
            SgFunctionDeclaration* fd = sfrd->getAssociatedFunctionDeclaration();
            fd = isSgFunctionDeclaration(fd->get_definingDeclaration());
            assert(fd!=NULL);
            SgFunctionDefinition* fdd = fd->get_definition();
            SgName sname = fdd->get_mangled_name();
            string sn = sname.getString();
            if (find(defstr.begin(), defstr.end(), sn) == defstr.end()) {
                defstr.push_back(sn);
                defs.push_back(fdd);
                std::cout << "found new sn: " << sn << std::endl;
            }
            else {
                std::cout << "found old sn: " << sn << std::endl;
            }
        }
    }
#pragma omp atomic
    paths++;
}

double timeDifference(const struct timeval& end, const struct timeval& begin)
{
    return (end.tv_sec + end.tv_usec / 1.0e6) - (begin.tv_sec + begin.tv_usec / 1.0e6);
}

static inline timeval getCPUTime() {
  rusage ru;
  getrusage(RUSAGE_SELF, &ru);
  return ru.ru_utime;
}


int main(int argc, char *argv[]) {
  
  struct timeval t1, t2;
  SgProject* proj = frontend(argc,argv);
  ROSE_ASSERT (proj != NULL); 

  SgFunctionDeclaration* mainDefDecl = SageInterface::findMain(proj);

  SgFunctionDefinition* mainDef = mainDefDecl->get_definition(); 
   visitorTraversal* vis = new visitorTraversal();
    StaticCFG::CFG cfg(mainDef);
   //cfg.buildFullCFG();
    stringstream ss;
    string fileName= StringUtility::stripPathFromFileName(mainDef->get_file_info()->get_filenameString());
    string dotFileName1=fileName+"."+ mainDef->get_declaration()->get_name() +".dot";

    cfgToDot(mainDef,dotFileName1); 
    //cfg->buildFullCFG();
    SgIncidenceDirectedGraph *g = cfg.getGraph();
    auto mg = instantiateGraph(g, cfg);
    vis->orig = mg.get();
    vis->tltnodes = 0;
    vis->paths = 0;
    //vis->firstPrepGraph(constcfg);
    t1 = getCPUTime();
    vis->constructPathAnalyzer(mg.get(), true, 0, 0, true);
    t2 = getCPUTime();
    std::cout << "took: " << timeDifference(t2, t1) << std::endl;
    //cfg.clearNodesAndEdges();
    std::cout << "finished" << std::endl;
    std::cout << "tltnodes: " << vis->tltnodes << " paths: " << vis->paths << std::endl;
    std::vector<std::unique_ptr<StaticCFG::CFG>> cfgs(vis->defs.size());
    std::vector<SgIncidenceDirectedGraph *> sgs(vis->defs.size());
    std::vector<std::unique_ptr<myGraph>> mgs(vis->defs.size());
    std::vector<std::unique_ptr<visitorTraversal>> visps(vis->defs.size());
    for (size_t i = 0; i < vis->defs.size(); i++) {
        ROSE_ASSERT(isSgFunctionDefinition(vis->defs[i]));
        cfgs[i] = std::make_unique<StaticCFG::CFG>(
            isSgFunctionDefinition(vis->defs[i]));
        // ROSE_ASSERT(gpart != NULL);
        sgs[i] = cfgs[i]->getGraph();
        ROSE_ASSERT(sgs[i] != NULL);
        mgs[i] = instantiateGraph(sgs[i], *cfgs[i]);
        ROSE_ASSERT(mgs[i] != NULL);
        visps[i] = std::make_unique<visitorTraversal>();
        visps[i]->orig = mgs[i].get();
        visps[i]->tltnodes = 0;
        visps[i]->paths = 0;
        std::cout << "fun: " << vis->defstr[i] << std::endl;
        visps[i]->constructPathAnalyzer(mgs[i].get(), true, 0, 0, true);
        std::cout << "paths: " << visps[i]->paths << std::endl;
    string dotFileName1=vis->defs[i]->get_declaration()->get_name() +".dot";

    cfgToDot(vis->defs[i],dotFileName1);

   }
   delete vis;
}
