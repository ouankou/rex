#include "rose.h"

namespace {
class ExactOwnedAttribute final : public AstAttribute {
public:
  ~ExactOwnedAttribute() override { ++destructionCount; }

  AstAttribute *copy() const override { return new ExactOwnedAttribute(); }

  std::string attribute_class_name() const override {
    return "ExactOwnedAttribute";
  }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  static int destructionCount;
};

int ExactOwnedAttribute::destructionCount = 0;
} // namespace

int main() {
  SgBasicBlock *root = SageBuilder::buildBasicBlock();
  ROSE_ASSERT(root != nullptr && root->get_attributeMechanism() == nullptr);
  root->addNewAttribute("rex:test:exact-owned-attribute",
                        new ExactOwnedAttribute());
  ROSE_ASSERT(root->get_attributeMechanism() != nullptr &&
              ExactOwnedAttribute::destructionCount == 0);

  SageInterface::deleteAST(root,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(root) &&
              ExactOwnedAttribute::destructionCount == 1);
  return 0;
}
