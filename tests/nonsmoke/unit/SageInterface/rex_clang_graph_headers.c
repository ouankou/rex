#include <stddef.h>

#ifndef REX_GRAPH_VALUE
#error The graph must use the frontend's command-line definitions.
#endif

struct RexGraph {
  size_t count;
};

size_t rex_graph_count(struct RexGraph value) {
  return value.count + sizeof(struct RexGraph) + REX_GRAPH_VALUE;
}
