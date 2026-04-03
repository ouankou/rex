// Uninitialized field tester: when run with Valgrind, prints out warnings
// for ROSE AST fields that are not initialized

#include "rose.h"

#include <unordered_set>

#if ROSE_USE_VALGRIND

#include <valgrind/valgrind.h>

#include <valgrind/memcheck.h>

using namespace std;

namespace {
const std::unordered_set<SgNode *> *g_reachable_nodes = nullptr;

bool shouldSkipUninitTraversal(SgNode *node) {
  if (g_reachable_nodes != nullptr &&
      g_reachable_nodes->find(node) == g_reachable_nodes->end()) {
    return true;
  }
  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    if (SageInterface::insideSystemHeader(located)) {
      return true;
    }
  }
  return false;
}

struct MemoryPoolFilterGuard {
  Rose::MemoryPoolTraversalFilter previous;
  std::unordered_set<SgNode *> reachable;

  explicit MemoryPoolFilterGuard(SgNode *root)
      : previous(Rose::getMemoryPoolTraversalFilter()) {
    if (root == nullptr) {
      return;
    }
    struct Collector : public AstSimpleProcessing {
      std::unordered_set<SgNode *> &set;
      explicit Collector(std::unordered_set<SgNode *> &s) : set(s) {}
      void visit(SgNode *n) override { set.insert(n); }
    } collector(reachable);

    collector.traverse(root, preorder);
    g_reachable_nodes = &reachable;
    Rose::setMemoryPoolTraversalFilter(&shouldSkipUninitTraversal);
  }

  ~MemoryPoolFilterGuard() {
    g_reachable_nodes = nullptr;
    Rose::setMemoryPoolTraversalFilter(previous);
  }
};
} // namespace

struct Vis : public ROSE_VisitTraversal {
  void visit(SgNode *node) {
    // When Valgrind is enabled, this checks for uninitialized fields; we
    // do not need the answer from it
    node->roseRTI();
  }
};

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  int frontend_status = frontendExitStatus(project);
  if (frontend_status != 0) {
    return frontend_status;
  }

  MemoryPoolFilterGuard guard(project);
  Vis().traverseMemoryPool();
  return (VALGRIND_COUNT_ERRORS != 0) ? 1 : 0;
}

#else // !ROSE_USE_VALGRIND

#error                                                                         \
    "testUninitializedFields should not be built unless valgrind/valgrind.h is found"

#endif // ROSE_USE_VALGRIND
