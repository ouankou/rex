#include "TransDepGraphImpl.h"
#include <TransAnalysis.C>
#include <TransDepGraph.h>
#include <vector>

template class TransInfoGraph<DepInfoSet>;
template class std::vector<TransAnalSCCGraphNode<DepInfoSet>::TwinNode>;
template class GraphTransAnalysis<DepInfoSet>;
