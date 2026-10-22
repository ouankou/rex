#include "VariableRenaming.h"
#include "midend/programAnalysis/genericDataflow/rwAccessLabeler/rwAccessLabeler.h"
#include "rose.h"
#include <stdexcept>

namespace {
class ExactOwnedAttribute final : public AstAttribute {
public:
  explicit ExactOwnedAttribute(bool rejectPolicy = false)
      : rejectPolicy_(rejectPolicy) {}

  ~ExactOwnedAttribute() override { ++destructionCount; }

  AstAttribute *copy() const override { return new ExactOwnedAttribute(); }

  std::string attribute_class_name() const override {
    return "ExactOwnedAttribute";
  }

  OwnershipPolicy getOwnershipPolicy() const override {
    if (rejectPolicy_)
      throw std::runtime_error("interrupted attribute copy");
    return CONTAINER_OWNERSHIP;
  }

  void allowPolicy() { rejectPolicy_ = false; }

  static int destructionCount;

private:
  bool rejectPolicy_;
};

int ExactOwnedAttribute::destructionCount = 0;
} // namespace

int main() {
  {
    ROSEAttributesListContainer container;
    auto *original = new ROSEAttributesList();
    original->addElement(PreprocessingInfo::CplusplusStyleComment,
                         "// owned record\n", "rex_ownership.cpp", 1, 1, 1);
    Sg_File_Info *position = (*original)[0]->get_file_info();
    container.addList("rex_ownership.cpp", original);
    container.addList("rex_ownership.cpp", original);
    ROSE_ASSERT(SgNode::isLiveNode(position));
    ROSE_ASSERT(&container["rex_ownership.cpp"] == original);
    auto *replacement = new ROSEAttributesList();
    container.addList("rex_ownership.cpp", replacement);
    ROSE_ASSERT(!SgNode::isLiveNode(position));
    ROSE_ASSERT(&container["rex_ownership.cpp"] == replacement);
    try {
      (void)container["missing.cpp"];
      ROSE_ABORT();
    } catch (const std::out_of_range &) {
    }
    ROSE_ASSERT(container.getList().size() == 1);
  }

  SgBasicBlock *root = SageBuilder::buildBasicBlock();
  ROSE_ASSERT(root != nullptr && root->get_attributeMechanism() == nullptr);
  auto *interior = reinterpret_cast<const SgNode *>(
      reinterpret_cast<const unsigned char *>(root) + 1);
  ROSE_ASSERT(SgNode::variantFromPool(root) == V_SgBasicBlock);
  ROSE_ASSERT(SgNode::variantFromPool(interior) == static_cast<VariantT>(0));
  ROSE_ASSERT(!SgNode::isLiveNode(interior));
  root->addNewAttribute("rex:test:exact-owned-attribute",
                        new ExactOwnedAttribute());
  ROSE_ASSERT(root->get_attributeMechanism() != nullptr &&
              ExactOwnedAttribute::destructionCount == 0);

  SageInterface::deleteAST(root,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(root) &&
              ExactOwnedAttribute::destructionCount == 1);

  {
    AstAttributeMechanism source;
    AstAttributeMechanism destination;
    auto *original = new ExactOwnedAttribute(true);
    auto *retained = new AstIntAttribute(7);
    source.set("owned", original);
    destination.set("retained", retained);
    try {
      destination = source;
      ROSE_ABORT();
    } catch (const std::runtime_error &) {
    }
    ROSE_ASSERT(ExactOwnedAttribute::destructionCount == 2 &&
                destination.size() == 1 && destination["retained"] == retained);
    original->allowPolicy();
  }
  ROSE_ASSERT(ExactOwnedAttribute::destructionCount == 3);

  SgInitializedName *variable = SageBuilder::buildInitializedName_nfi(
      "rex_attribute_variable", SageBuilder::buildIntType(), nullptr);
  {
    AstAttributeMechanism source;
    auto *name = new VarUniqueName(variable);
    name->setUsesThis(true);
    source.set(VariableRenaming::varKeyTag, name);
    AstAttributeMechanism copied(source);
    auto *copiedName =
        dynamic_cast<VarUniqueName *>(copied[VariableRenaming::varKeyTag]);
    ROSE_ASSERT(copiedName != nullptr && copiedName != name);
    ROSE_ASSERT(copiedName->getKey() == name->getKey());
    ROSE_ASSERT(copiedName->getUsesThis());
  }
  SageInterface::deleteAST(variable,
                           SageInterface::DeleteAstMode::kRequireIsolated);

  SgAddOp *expression = SageBuilder::buildAddOp(SageBuilder::buildIntVal(1),
                                                SageBuilder::buildIntVal(2),
                                                SageBuilder::buildIntType());
  ROSE_ASSERT(VariableRenaming::getUniqueName(expression) == nullptr);
  ROSE_ASSERT(VariableRenaming::getVarName(expression).empty());
  ROSE_ASSERT(VariableRenaming::getVarName(nullptr).empty());
  rwAccessLabeler::addRWAnnotations(expression);
  AstAttribute *access = expression->getAttribute("rwAccess");
  ROSE_ASSERT(access != nullptr);
  ROSE_ASSERT(access->getOwnershipPolicy() ==
              AstAttribute::CONTAINER_OWNERSHIP);
  auto *copiedExpression = isSgAddOp(SageInterface::copyExpression(expression));
  ROSE_ASSERT(copiedExpression != nullptr);
  ROSE_ASSERT(copiedExpression->getAttribute("rwAccess") != access);
  ROSE_ASSERT(rwAccessLabeler::getAccessType(copiedExpression) ==
              rwAccessLabeler::readAccess);
  ROSE_ASSERT(
      rwAccessLabeler::getAccessType(copiedExpression->get_lhs_operand()) ==
      rwAccessLabeler::readAccess);
  expression->removeAttribute("rwAccess");
  ROSE_ASSERT(rwAccessLabeler::getAccessType(copiedExpression) ==
              rwAccessLabeler::readAccess);
  SageInterface::deleteAST(expression,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  SageInterface::deleteAST(copiedExpression,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  return 0;
}
