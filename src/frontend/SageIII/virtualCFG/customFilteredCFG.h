#ifndef CUSTOMFILTEREDCFG_H
#define CUSTOMFILTEREDCFG_H

#include "filteredCFG.h"

#include "staticCFG.h"

#include "Cxx_Grammar.h"
#include "nodeQuery.h"

namespace StaticCFG {
//! A CFG implementation with Custom filters
template <typename _Filter> class ROSE_DLL_API CustomFilteredCFG : public CFG {

public:
  CustomFilteredCFG(SgNode *node) : CFG(node, true, false) {
    buildFilteredCFG();
  }
  void buildFilteredCFG() override;

};

template <typename _filter>
void ROSE_DLL_API CustomFilteredCFG<_filter>::buildFilteredCFG() {
  clearNodesAndEdges();
  std::set<VirtualCFG::FilteredCFGNode<_filter>> explored;
  graph_ = new SgIncidenceDirectedGraph;

  if (SgProject *project = isSgProject(start_)) {
    Rose_STL_Container<SgNode *> functions =
        NodeQuery::querySubTree(project, V_SgFunctionDefinition);
    for (Rose_STL_Container<SgNode *>::const_iterator i = functions.begin();
         i != functions.end(); ++i) {
      SgFunctionDefinition *proc = isSgFunctionDefinition(*i);
      if (proc) {
        buildCFG<VirtualCFG::FilteredCFGNode<_filter>,
                 VirtualCFG::FilteredCFGEdge<_filter>>(proc->cfgForBeginning(),
                                                       explored);
      }
    }
  } else
    buildCFG<VirtualCFG::FilteredCFGNode<_filter>,
             VirtualCFG::FilteredCFGEdge<_filter>>(start_->cfgForBeginning(),
                                                   explored);
}

} // namespace StaticCFG
#endif /* CUSTOMFILTEREDCFG_H */
