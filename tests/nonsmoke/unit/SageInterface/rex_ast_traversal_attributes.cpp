#include "sage3basic.h"

#include "AstReverseProcessing.h"
#include "AstReverseSimpleProcessing.h"
#include "SgGraphTemplate.h"
#include "graphProcessing.h"

#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using Nodes = std::vector<SgNode *>;
using Depths = std::map<SgNode *, size_t>;

struct Count {
  size_t value = 0;
};

class GraphTraversal : public SgGraphTraversal<myGraph> {
public:
  std::vector<std::vector<Vertex>> paths;
  void analyzePath(std::vector<Vertex> &path) override {
    paths.push_back(path);
  }
};

template <class Action> void requireMissingGraphIndex(Action action) {
  bool rejected = false;
  try {
    action();
  } catch (const std::out_of_range &) {
    rejected = true;
  }
  ROSE_ASSERT(rejected);
}

template <class Attributes> Count countNodes(Attributes children) {
  Count count{1};
  for (const Count &child : children)
    count.value += child.value;
  return count;
}

template <class Base> class Inherited : public Base {
public:
  Nodes visited;
  Depths depths;

  size_t evaluateInheritedAttribute(SgNode *node, size_t depth) override {
    visited.push_back(node);
    depths.emplace(node, depth);
    return depth + 1;
  }
};

template <class Base> class Synthesized : public Base {
public:
  Nodes visited;

  Count evaluateSynthesizedAttribute(
      SgNode *node,
      typename Base::SubTreeSynthesizedAttributes children) override {
    visited.push_back(node);
    return countNodes(children);
  }
};

template <class Base> class Combined : public Base {
public:
  Nodes before, after;
  Depths depths;

  size_t evaluateInheritedAttribute(SgNode *node, size_t depth) override {
    before.push_back(node);
    depths.emplace(node, depth);
    return depth + 1;
  }

  Count evaluateSynthesizedAttribute(
      SgNode *node, size_t depth,
      typename Base::SubTreeSynthesizedAttributes children) override {
    ROSE_ASSERT(depth == depths.at(node) + 1);
    after.push_back(node);
    return countNodes(children);
  }
};

template <class Base> class Simple : public Base {
public:
  Nodes visited;
  void visit(SgNode *node) override { visited.push_back(node); }
};

template <class Inh, class Syn, class Both, class Visit>
void check(SgNode *start, const Nodes &before, const Nodes &after,
           const Depths &depths) {
  Inherited<Inh> inherited;
  inherited.traverse(start, 0);
  ROSE_ASSERT(inherited.visited == before);
  ROSE_ASSERT(inherited.depths == depths);

  Synthesized<Syn> synthesized;
  ROSE_ASSERT(synthesized.traverse(start).value == before.size());
  ROSE_ASSERT(synthesized.visited == after);

  Combined<Both> combined;
  ROSE_ASSERT(combined.traverse(start, 0).value == before.size());
  ROSE_ASSERT(combined.before == before);
  ROSE_ASSERT(combined.after == after);
  ROSE_ASSERT(combined.depths == depths);

  Simple<Visit> simplePre;
  simplePre.traverse(start, preorder);
  ROSE_ASSERT(simplePre.visited == before);
  Simple<Visit> simplePost;
  simplePost.traverse(start, postorder);
  ROSE_ASSERT(simplePost.visited == after);
}
} // namespace

int main() {
  myGraph graph;
  const auto first = graph.add_vertex();
  const auto last = graph.add_vertex();
  graph.add_edge(first, last);
  myGraph *graphPointer = &graph;
  GraphTraversal graphSource, graphCopy;
  ROSE_ASSERT(graphSource.pathnum == 0);
  graphSource.constructPathAnalyzer(graphPointer, true);
  ROSE_ASSERT(
      (graphSource.paths ==
       std::vector<std::vector<GraphTraversal::Vertex>>{{first, last}}));
  graphSource.pathnum = 7;
  graphCopy = graphSource;
  ROSE_ASSERT(graphCopy.vertintmap == graphSource.vertintmap);
  ROSE_ASSERT(graphCopy.intvertmap == graphSource.intvertmap);
  ROSE_ASSERT(graphCopy.edgeintmap == graphSource.edgeintmap);
  ROSE_ASSERT(graphCopy.intedgemap == graphSource.intedgemap);
  ROSE_ASSERT(graphCopy.pathnum == graphSource.pathnum);
  graphCopy.vertintmap.at(first) = 6;
  ROSE_ASSERT(graphSource.vertintmap.at(first) == 1);
  const auto &source = graphSource;
  GraphTraversal graphConstructed(source);
  ROSE_ASSERT(graphConstructed.vertintmap == source.vertintmap);
  ROSE_ASSERT(graphConstructed.intvertmap == source.intvertmap);
  ROSE_ASSERT(graphConstructed.edgeintmap == source.edgeintmap);
  ROSE_ASSERT(graphConstructed.intedgemap == source.intedgemap);
  ROSE_ASSERT(graphConstructed.pathnum == source.pathnum);
  const auto &graphSelf = graphConstructed;
  graphConstructed = graphSelf;
  graphConstructed.vertintmap.clear();
  ROSE_ASSERT(source.vertintmap.size() == 2);
  int missing = 100;
  requireMissingGraphIndex(
      [&] { graphSource.getSource(missing, graphPointer); });
  requireMissingGraphIndex(
      [&] { graphSource.getTarget(missing, graphPointer); });
  requireMissingGraphIndex(
      [&] { graphSource.getInEdges(missing, graphPointer); });
  requireMissingGraphIndex(
      [&] { graphSource.getOutEdges(missing, graphPointer); });
  ROSE_ASSERT(graphSource.vertintmap.size() == 2);
  ROSE_ASSERT(graphSource.intvertmap.size() == 2);
  ROSE_ASSERT(graphSource.edgeintmap.size() == 1);
  ROSE_ASSERT(graphSource.intedgemap.size() == 1);

  std::vector<std::unique_ptr<SgBasicBlock>> storage;
  for (int i = 0; i < 6; ++i)
    storage.push_back(std::make_unique<SgBasicBlock>());
  auto *root = storage[0].get();
  auto *left = storage[1].get();
  auto *leftLeaf = storage[2].get();
  auto *right = storage[3].get();
  auto *rightFirst = storage[4].get();
  auto *rightLast = storage[5].get();
  root->append_statement(left);
  root->append_statement(right);
  left->append_statement(leftLeaf);
  right->append_statement(rightFirst);
  right->append_statement(rightLast);

  Synthesized<AstBottomUpProcessing<Count>> original;
  ROSE_ASSERT(original.traverse(root).value == storage.size());
  auto copied = original;
  ROSE_ASSERT(copied.traverse(left).value == 2);
  copied = original;
  const auto &self = copied;
  copied = self;
  copied.visited.clear();
  ROSE_ASSERT(copied.traverse(root).value == storage.size());
  ROSE_ASSERT(copied.visited.size() == storage.size());
  ROSE_ASSERT(original.traverse(right).value == 3);

  check<AstTopDownProcessing<size_t>, AstBottomUpProcessing<Count>,
        AstTopDownBottomUpProcessing<size_t, Count>, AstSimpleProcessing>(
      root, {root, left, leftLeaf, right, rightFirst, rightLast},
      {leftLeaf, left, rightFirst, rightLast, right, root},
      {{root, 0},
       {left, 1},
       {leftLeaf, 2},
       {right, 1},
       {rightFirst, 2},
       {rightLast, 2}});

  // Reverse traversals follow a left sibling, or the parent when there is
  // no left sibling. They do not descend into the sibling's subtree.
  const Nodes before{rightLast, rightFirst, right, left, root};
  const Nodes after{root, left, right, rightFirst, rightLast};
  const Depths depths{
      {rightLast, 0}, {rightFirst, 1}, {right, 2}, {left, 3}, {root, 4}};
  check<AstReversePrefixInhProcessing<size_t>,
        AstReversePrefixSynProcessing<Count>,
        AstReversePrefixInhSynProcessing<size_t, Count>,
        AstReversePrefixSimpleProcessing>(rightLast, before, after, depths);
  check<AstReverseBranchInhProcessing<size_t>,
        AstReverseBranchSynProcessing<Count>,
        AstReverseBranchInhSynProcessing<size_t, Count>,
        AstReverseBranchSimpleProcessing>(rightLast, before, after, depths);
}
