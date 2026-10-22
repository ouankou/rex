#include <cstddef>

#ifndef REX_GRAPH_VALUE
#error The graph must use the frontend's command-line definitions.
#endif

struct RexGraph {
  std::size_t count;
};

std::size_t rex_graph_count(const RexGraph &value) {
  return value.count + sizeof(RexGraph) + REX_GRAPH_VALUE;
}
