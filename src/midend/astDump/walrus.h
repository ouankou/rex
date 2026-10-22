#ifndef ROSE_WALRUS_H
#define ROSE_WALRUS_H

#include <string>

class SgNode;

namespace WalrusGraph {
// Export the traversal root and source-visible descendants as one tree.
// Throws std::runtime_error on repeated exported nodes or output failure.
void generateWalrusGraph(SgNode *node, std::string outputFilename);
} // namespace WalrusGraph

#endif
