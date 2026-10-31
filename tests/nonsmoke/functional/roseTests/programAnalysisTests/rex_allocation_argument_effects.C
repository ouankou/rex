#include "AstInterface_ROSE.h"
#include "RoseAst.h"
#include "StmtInfoCollect.h"
#include "rose.h"

#include <functional>
#include <set>
#include <vector>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  SgFunctionDefinition *function = nullptr;
  RoseAst tree(project);
  for (auto node = tree.begin(); node != tree.end(); ++node)
    if (auto *definition = isSgFunctionDefinition(*node))
      if (definition->get_declaration()->get_name() == "rex_allocation_effects")
        function = definition;
  ROSE_ASSERT(function != nullptr);

  AstInterfaceImpl implementation(function->get_body());
  AstInterface ast(&implementation);
  StmtSideEffectCollect collect(ast);
  std::vector<AstNodePtr> allocations, frees, modifications, calls;
  int storageReads = 0;
  std::function<bool(AstNodePtr, AstNodePtr)> read = [&](AstNodePtr node,
                                                         AstNodePtr) {
    if (auto *reference = isSgVarRefExp(node.get_ptr()))
      storageReads += reference->get_symbol()->get_name() == "storage";
    return true;
  };
  std::function<bool(AstNodePtr, AstNodePtr)> allocation = [&](AstNodePtr node,
                                                               AstNodePtr) {
    allocations.push_back(node);
    return true;
  };
  std::function<bool(AstNodePtr, AstNodePtr)> freed = [&](AstNodePtr node,
                                                          AstNodePtr) {
    frees.push_back(node);
    return true;
  };
  std::function<bool(AstNodePtr, AstNodePtr)> modified = [&](AstNodePtr node,
                                                             AstNodePtr) {
    modifications.push_back(node);
    return true;
  };
  std::function<bool(AstNodePtr, AstNodePtr)> called = [&](AstNodePtr node,
                                                           AstNodePtr) {
    calls.push_back(node);
    return true;
  };
  // Declaration bindings have their own channel, separate from writes below.
  std::function<bool(AstNodePtr, AstNodePtr)> declared =
      [](AstNodePtr, AstNodePtr) { return true; };
  collect.set_allocate_collect(allocation);
  collect.set_free_collect(freed);
  collect.set_modify_collect(modified);
  collect.set_call_collect(called);
  collect.set_new_var_collect(declared);
  collect.set_read_collect(read);
  collect(AstNodePtrImpl(function->get_body()));

  ROSE_ASSERT(allocations.size() == 3);
  ROSE_ASSERT(frees.size() == 2);
  ROSE_ASSERT(calls.size() == 2);
  std::set<SgNode *> uniqueAllocations;
  SgNode *placementDestination = nullptr;
  AstInterface::AstNodeList destinations;
  for (AstNodePtr allocationNode : allocations) {
    ROSE_ASSERT(uniqueAllocations.insert(allocationNode.get_ptr()).second);
    // Reuse the output list to check that a later ordinary allocation clears
    // it.
    destinations.push_back(AST_NULL);
    ROSE_ASSERT(AstInterface::IsMemoryAllocation(allocationNode, nullptr,
                                                 nullptr, &destinations));
    auto *newExpression = isSgNewExp(allocationNode.get_ptr());
    if (newExpression && newExpression->get_placement_args()) {
      ROSE_ASSERT(destinations.size() == 1);
      placementDestination = destinations.front().get_ptr();
      ROSE_ASSERT(
          placementDestination ==
          newExpression->get_placement_args()->get_expressions().front());
    } else {
      ROSE_ASSERT(destinations.empty());
    }
  }
  ROSE_ASSERT(placementDestination != nullptr);

  int valueWrites = 0, bytesWrites = 0;
  for (AstNodePtr modification : modifications) {
    auto *reference = isSgVarRefExp(modification.get_ptr());
    ROSE_ASSERT(reference != nullptr);
    const std::string name = reference->get_symbol()->get_name().getString();
    // Constructing at storage reads the pointer; it never assigns to it.
    ROSE_ASSERT(name == "value" || name == "bytes");
    valueWrites += name == "value";
    bytesWrites += name == "bytes";
  }
  ROSE_ASSERT(valueWrites == 2 && bytesWrites == 1 && storageReads == 1);
  return 0;
}
