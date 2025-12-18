/* 
 * File:   CustomFilteredCFG.h
 * Author: rahman2
 *
 * Created on July 20, 2011, 3:41 PM
 */

#ifndef CUSTOMFILTEREDCFG_H
#define CUSTOMFILTEREDCFG_H

#include "filteredCFG.h"
#include "staticCFG.h"

namespace StaticCFG 
{
//! A CFG implementation with Custom filters
template <typename _Filter>
class ROSE_DLL_API CustomFilteredCFG : public CFG {
    
public:
        CustomFilteredCFG(SgNode *node) : CFG(node, true) {
        }
        ~CustomFilteredCFG() {
        }
        virtual void buildFilteredCFG();
        
        
protected:        
        //! Virtual function Overloaded to print the Custom Filtered CFG Edges
  void printEdge(std::ostream &o, SgDirectedGraphEdge *edge,
                 bool isInEdge) override;

private:        
        template <class NodeT, class EdgeT>
        void buildTemplatedCFG(NodeT n, std::map<NodeT, SgGraphNode*>& all_nodes, std::set<NodeT>& explored);

    };

}
#endif /* CUSTOMFILTEREDCFG_H */
