#include "sage3basic.h"
#include "walrus.h"

#include <fstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace WalrusGraph {
namespace {
// Keep the traversal root even when it is a project/file wrapper. Carrying the
// nearest exported ancestor through filtered nodes produces one connected tree
// without inventing parent-map entries or dropping additional source files.
class Graph : public AstTopDownProcessing<size_t> {
  SgNode *root;
  std::unordered_set<SgNode *> exported;

public:
  size_t node_count = 0;
  std::vector<std::pair<size_t, size_t>> edges;
  explicit Graph(SgNode *root) : root(root) {}

  size_t evaluateInheritedAttribute(SgNode *node, size_t parent) override {
    bool visible = false;
    if (auto *located = isSgLocatedNode(node)) {
      ROSE_ASSERT(located->get_file_info() != nullptr);
      visible = !located->get_file_info()->isFrontendSpecific();
    }
    if (node != root && !visible)
      return parent;
    if (!exported.insert(node).second)
      throw std::runtime_error("Walrus traversal repeats an exported AST node");
    const size_t id = node_count++;
    if (node != root) {
      ROSE_ASSERT(parent < id);
      edges.emplace_back(parent, id);
    }
    return id;
  }
};

void writeGraph(const Graph &graph, std::ostream &output) {
  output << "Graph\n{\n   ### metadata ###\n   @name=\"ROSE AST\";\n   "
            "@description=\"Source-visible ROSE AST with its traversal "
            "root.\";\n   @numNodes="
         << graph.node_count << ";\n   @numLinks=" << graph.edges.size()
         << ";\n   @numPaths=0;\n   @numPathLinks=0;\n   ### "
            "structural data ###\n   @links=[\n";

  for (size_t i = 0; i < graph.edges.size(); ++i) {
    if (i != 0)
      output << ",\n";
    output << "            { @source=" << graph.edges[i].first
           << "; @destination=" << graph.edges[i].second << "; }";
  }
  output << '\n';
  output
      << "   ];\n   @paths=;\n\n   ### attribute data ###\n   "
         "@enumerations=;\n   @attributeDefinitions=[\n      {\n         "
         "@name=$root;\n         @type=bool;\n         @default=|| false ||;\n "
         "        @nodeValues=[ { @id=0; @value=T; } ];\n";
  output
      << "         @linkValues=;\n         @pathValues=;\n      },\n      {\n  "
         "       @name=$tree_link;\n         @type=bool;\n         @default=|| "
         "false ||;\n         @nodeValues=;\n         @linkValues=[\n";
  for (size_t i = 0; i < graph.edges.size(); ++i) {
    if (i != 0)
      output << ",\n";
    output << "            { @id=" << i << "; @value=T; }";
  }
  output << '\n';
  output << "         ];\n         @pathValues=;\n      }\n";
  output
      << "   ];\n   @qualifiers=[\n      {\n         @type=$spanning_tree;\n   "
         "      @name=$sample_spanning_tree;\n         @description=;\n        "
         " @attributes=[\n";
  output << "            { @attribute=0; @alias=$root; },\n            "
            "{ @attribute=1; @alias=$tree_link; }\n         ];\n      "
            "}\n   ];\n   ### visualization hints ###\n";
  output << "   @filters=;\n   @selectors=;\n   @displays=;\n   "
            "@presentations=;\n\n   ### interface hints ###\n   "
            "@presentationMenus=;\n   @displayMenus=;\n   @selectorMenus=;\n   "
            "@filterMenus=;\n   @attributeMenus=;\n}\n";
}
} // namespace

void generateWalrusGraph(SgNode *node, std::string outputFilename) {
  ROSE_ASSERT(node != nullptr);
  Graph graph(node);
  graph.traverse(node, 0);
  ROSE_ASSERT(graph.node_count == graph.edges.size() + 1);
  std::ofstream output(outputFilename);
  if (!output)
    throw std::runtime_error("Cannot open Walrus graph: " + outputFilename);
  writeGraph(graph, output);
  output.close();
  if (!output)
    throw std::runtime_error("Cannot write Walrus graph: " + outputFilename);
}
} // namespace WalrusGraph
