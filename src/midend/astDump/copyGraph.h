#ifndef COPYGRAPH_H
#define COPYGRAPH_H

#include <set>

#include <string>

std::set<SgNode *> getAllNodes(SgNode *node);

// Run this after copy, using the data structure produced before.
// Throws std::runtime_error if writing the graph fails.
void graphNodesAfterCopy(const std::set<SgNode *> &nodesBeforeCopy,
                         std::string filename);

#endif // COPYGRAPH_H
