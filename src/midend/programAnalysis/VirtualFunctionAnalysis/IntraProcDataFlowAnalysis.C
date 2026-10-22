#ifndef TEMPLATE_ONLY
#include "sage3basic.h"
#endif
#include "IntraProcDataFlowAnalysis.h"

template <class Node, class Data>
IntraProcDataFlowAnalysis<Node, Data>::IntraProcDataFlowAnalysis(
    SgNode *_head) {
  head = _head;
}

template <class Node, class Data>
void IntraProcDataFlowAnalysis<Node, Data>::run() {
  bool change = true;
  while (change) {
    change = false;
    std::vector<Node *> nodes = getAllNodes();
    for (unsigned int index = 0; index < nodes.size(); index++) {
      Node *cur = nodes[index];
      Data inOrig = getCFGInData(cur);
      // Get the list of Predecessors of s
      std::vector<Node *> preds = getPredecessors(cur);
      Data in = preds.empty() ? inOrig : Data{};
      // In[s] = Union of Out[p], where p is a predecessor of s.
      for (unsigned int p_index = 0; p_index < preds.size(); p_index++) {
        Node *pred = preds[p_index];
        Data predout = getCFGOutData(pred);
        in = meet_data(in, predout);
      }
      // Out[s]=Gen[s] Union (In[s]-Kill[s]), s is current node
      if (in != inOrig)
        setCFGInData(cur, in);

      Data outOrig = getCFGOutData(cur);
      applyCFGTransferFunction(cur);

      if (outOrig != getCFGOutData(cur)) {
        change = true;
      }
    }
  }
}
