#include "sage3basic.h"
#include "sageInterface.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// A distinct subclass ensures this header contract instantiates its own query,
// rather than using a template instantiation exported by the existing library.
struct EnclosingRoot : SgBasicBlock {};

int main(int argc, char **argv) {
  ROSE_ASSERT(argc == 2);
  const std::string mode = argv[1];
  auto root_owner = std::make_unique<EnclosingRoot>();
  EnclosingRoot &root = *root_owner;
  ROSE_ASSERT(SageInterface::getEnclosingNode<EnclosingRoot>(nullptr) ==
              nullptr);
  ROSE_ASSERT(SageInterface::getEnclosingNode<EnclosingRoot>(&root) == nullptr);
  ROSE_ASSERT(SageInterface::getEnclosingNode<EnclosingRoot>(&root, true) ==
              &root);
  if (mode == "scope_cycle" || mode == "ancestor_cycle") {
    auto first = std::make_unique<SgNullStatement>();
    auto second = std::make_unique<SgNullStatement>();
    first->set_parent(second.get());
    second->set_parent(first.get());
    if (mode == "ancestor_cycle")
      (void)SageInterface::isAncestor(&root, first.get());
    else
      (void)first->get_scope();
    return 0;
  }
  if (mode == "scope_deep") {
    std::vector<std::unique_ptr<SgNullStatement>> statements;
    SgNode *parent = &root;
    for (int i = 0; i < 1002; ++i) {
      auto statement = std::make_unique<SgNullStatement>();
      statement->set_parent(parent);
      parent = statement.get();
      statements.push_back(std::move(statement));
    }
    ROSE_ASSERT(statements.back()->get_scope() == &root);
    ROSE_ASSERT(SageInterface::isAncestor(&root, statements.back().get()));
    ROSE_ASSERT(!SageInterface::isAncestor(statements.back().get(), &root));
    ROSE_ASSERT(!SageInterface::isAncestor(&root, &root));
    for (auto &statement : statements)
      statement->set_parent(nullptr);
    return 0;
  }
  if (mode == "cycle") {
    auto first_owner = std::make_unique<SgBasicBlock>();
    SgBasicBlock &first = *first_owner;
    auto second_owner = std::make_unique<SgBasicBlock>();
    SgBasicBlock &second = *second_owner;
    first.set_parent(&second);
    second.set_parent(&first);
    auto *result = SageInterface::getEnclosingNode<EnclosingRoot>(&first);
    std::printf("cycle accepted as %p\n", static_cast<void *>(result));
    first.set_parent(nullptr);
    second.set_parent(nullptr);
    return 0;
  }
  if (mode == "declarations" || mode == "semantic_cycle") {
    auto definition_root_owner = std::make_unique<EnclosingRoot>();
    EnclosingRoot &definition_root = *definition_root_owner;
    auto first_owner = std::make_unique<SgClassDeclaration>(
        "Record", SgClassDeclaration::e_struct, nullptr, nullptr);
    SgClassDeclaration &first = *first_owner;
    auto definition_owner = std::make_unique<SgClassDeclaration>(
        "Record", SgClassDeclaration::e_struct, nullptr, nullptr);
    SgClassDeclaration &definition = *definition_owner;
    auto redeclaration_owner = std::make_unique<SgClassDeclaration>(
        "Record", SgClassDeclaration::e_struct, nullptr, nullptr);
    SgClassDeclaration &redeclaration = *redeclaration_owner;
    first.set_parent(&root);
    definition.set_parent(&definition_root);
    redeclaration.set_parent(&root);
    for (auto *declaration : {&first, &definition, &redeclaration}) {
      declaration->set_firstNondefiningDeclaration(&first);
      declaration->set_definingDeclaration(&definition);
    }
    auto occurrence_owner = std::make_unique<SgNullStatement>();
    SgNullStatement &occurrence = *occurrence_owner;
    occurrence.set_parent(&first);
    ROSE_ASSERT(SageInterface::getEnclosingNode<EnclosingRoot>(&occurrence) ==
                &root);
    occurrence.set_parent(&redeclaration);
    ROSE_ASSERT(SageInterface::getEnclosingNode<EnclosingRoot>(&occurrence) ==
                &definition_root);
    ROSE_ASSERT(SageInterface::getEnclosingNode<SgClassDeclaration>(
                    &occurrence) == &definition);
    auto intermediate_owner = std::make_unique<SgBasicBlock>();
    SgBasicBlock &intermediate = *intermediate_owner;
    occurrence.set_parent(&intermediate);
    intermediate.set_parent(&redeclaration);
    ROSE_ASSERT(SageInterface::getEnclosingNode<EnclosingRoot>(&occurrence) ==
                &definition_root);
    ROSE_ASSERT(SageInterface::getEnclosingNode<SgClassDeclaration>(
                    &occurrence) == &redeclaration);
    if (mode == "semantic_cycle") {
      auto cycle_entry_owner = std::make_unique<SgBasicBlock>();
      SgBasicBlock &cycle_entry = *cycle_entry_owner;
      definition.set_parent(&cycle_entry);
      cycle_entry.set_parent(&redeclaration);
      (void)SageInterface::getEnclosingNode<EnclosingRoot>(&occurrence);
      cycle_entry.set_parent(nullptr);
      definition.set_parent(nullptr);
    }
    occurrence.set_parent(nullptr);
    intermediate.set_parent(nullptr);
    return 0;
  }
  ROSE_ASSERT(mode == "deep" || mode == "benchmark");
  const int depth = mode == "deep" ? 100002 : 12;
  std::vector<std::unique_ptr<SgBasicBlock>> blocks;
  blocks.reserve(depth);
  SgBasicBlock *parent = &root;
  for (int i = 0; i < depth; ++i) {
    auto block = std::make_unique<SgBasicBlock>();
    parent->append_statement(block.get());
    parent = block.get();
    blocks.push_back(std::move(block));
  }
  const auto start = std::chrono::steady_clock::now();
  const int repeats = mode == "benchmark" ? 1000000 : 1;
  for (int i = 0; i < repeats; ++i) {
    ROSE_ASSERT(SageInterface::getEnclosingNode<EnclosingRoot>(parent) ==
                &root);
  }
  const auto seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  std::printf("queries=%d depth=%d seconds=%.6f\n", repeats, depth, seconds);
  root.get_statements().clear();
  for (auto &block : blocks) {
    block->get_statements().clear();
    block->set_parent(nullptr);
  }
}
