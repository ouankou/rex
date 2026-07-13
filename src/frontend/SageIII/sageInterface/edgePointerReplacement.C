#include "edgePointerReplacement.h"

#include "sage3basic.h"

#include <utility>

template <typename HandlerT, typename TraversalT>
struct EdgeTraversal : public TraversalT {
  HandlerT handler;

  template <typename... Args>
  explicit EdgeTraversal(Args &&...args)
      : handler(std::forward<Args>(args)...) {}

  void visit(SgNode *node) override {
    node->processDataMemberReferenceToPointers(&handler);
  }
};

template <typename HandlerT>
using EdgeMemoryPoolTraversal = EdgeTraversal<HandlerT, ROSE_VisitTraversal>;
template <typename HandlerT>
using EdgeTreeTraversal = EdgeTraversal<HandlerT, SgSimpleProcessing>;

struct EdgeReplacer : public SimpleReferenceToPointerHandler {
  explicit EdgeReplacer(replacement_map_t const &replacements)
      : replacements(replacements) {}

  void operator()(SgNode *&node, const SgName &, bool) override {
    if (node == nullptr) {
      return;
    }
    auto found = replacements.find(node);
    if (found != replacements.end()) {
      node = found->second;
    }
  }

  replacement_map_t const &replacements;
};

void edgePointerReplacement(replacement_map_t const &replacements) {
  if (replacements.empty()) {
    return;
  }
  EdgeMemoryPoolTraversal<EdgeReplacer> traversal(replacements);
  traversal.traverseMemoryPool();
}

void edgePointerReplacement(SgNode *root,
                            replacement_map_t const &replacements) {
  ROSE_ASSERT(root != nullptr);
  if (replacements.empty()) {
    return;
  }
  EdgeTreeTraversal<EdgeReplacer> traversal(replacements);
  traversal.traverse(root, preorder);
}
