#ifndef EDGE_POINTER_REPLACEMENT_H
#define EDGE_POINTER_REPLACEMENT_H

#include <map>

class SgNode;

using replacement_map_t = std::map<SgNode *, SgNode *>;

void edgePointerReplacement(replacement_map_t const &replacements);
void edgePointerReplacement(SgNode *root,
                            replacement_map_t const &replacements);

#endif
