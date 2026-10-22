#include "sage3basic.h"
#include "walrus.h"

#include <memory>
#include <stdexcept>
#include <string>

struct DeleteAst {
  void operator()(SgNode *node) const { SageInterface::deleteAST(node); }
};

int main(int argc, char **argv) {
  ROSE_ASSERT(argc == 2);
  const std::string directory = argv[1];
  auto *filtered = SageBuilder::buildBasicBlock(
      SageBuilder::buildNullStatement(), SageBuilder::buildNullStatement());
  std::unique_ptr<SgBasicBlock, DeleteAst> root(
      SageBuilder::buildBasicBlock(filtered));
  filtered->get_file_info()->setFrontendSpecific();
  WalrusGraph::generateWalrusGraph(root.get(), directory + "/first.graph");

  std::unique_ptr<SgBasicBlock, DeleteAst> single(
      SageBuilder::buildBasicBlock());
  WalrusGraph::generateWalrusGraph(single.get(), directory + "/single.graph");
  WalrusGraph::generateWalrusGraph(root.get(), directory + "/repeated.graph");

  try {
    WalrusGraph::generateWalrusGraph(root.get(),
                                     directory + "/absent/output.graph");
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()).find("Cannot open Walrus graph:") !=
        std::string::npos)
      return 0;
    throw;
  }
  throw std::runtime_error("Walrus output-open failure was ignored");
}
