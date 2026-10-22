#include "rose.h"

#include <cstring>
#include <memory>
#include <vector>

int main(int argc, char **argv) {
  const bool orphan = argc > 1 && std::strcmp(argv[argc - 1], "orphan") == 0;
  const bool typeOrphan =
      argc > 1 && std::strcmp(argv[argc - 1], "type-orphan") == 0;
  if (orphan || typeOrphan)
    --argc;
  std::unique_ptr<SgProject> project(frontend(argc, argv));
  ROSE_ASSERT(project->get_frontendErrorCode() == 0);
  auto loops = NodeQuery::querySubTree(project.get(), V_SgForStatement);
  ROSE_ASSERT(loops.size() == 1);
  auto *loop = isSgForStatement(loops.front());
  auto *increment = isSgPlusPlusOp(loop->get_increment());
  ROSE_ASSERT(increment != nullptr);
  auto *reference = isSgVarRefExp(increment->get_operand());
  ROSE_ASSERT(reference != nullptr);
  SgVariableSymbol *symbol = reference->get_symbol();
  auto names = NodeQuery::querySubTree(loop, V_SgInitializedName);
  ROSE_ASSERT(names.size() == 2);
  auto *lexicalType =
      isSgTypeOfType(isSgInitializedName(names.back())->get_type());
  ROSE_ASSERT(lexicalType != nullptr);
  SgExpression *lexicalOperand = lexicalType->get_base_expression();
  ROSE_ASSERT(lexicalOperand->get_parent() == lexicalType);
  SgPointerType *cachedPointer = SageBuilder::buildPointerType(lexicalType);
  SgModifierType *cachedConst = SageBuilder::buildConstType(lexicalType);
  SgModifierNodes *modifierCache = lexicalType->get_modifiers();
  ROSE_ASSERT(modifierCache != nullptr);
  ROSE_ASSERT(modifierCache->get_parent() == lexicalType);

  for (unsigned iteration = 0; iteration < 4; ++iteration) {
    Sg_File_Info *positions[] = {increment->get_startOfConstruct(),
                                 increment->get_endOfConstruct(),
                                 increment->get_operatorPosition()};
    SageInterface::setSourcePositionAsTransformation(increment);
    for (Sg_File_Info *position : positions)
      ROSE_ASSERT(position != nullptr && !SgNode::isLiveNode(position));
    ROSE_ASSERT(increment->get_startOfConstruct()->get_parent() == increment);
    ROSE_ASSERT(increment->get_endOfConstruct()->get_parent() == increment);
    ROSE_ASSERT(increment->get_operatorPosition()->get_parent() == increment);
  }

  std::vector<SgNode *> retired = {increment,
                                   reference,
                                   increment->get_startOfConstruct(),
                                   increment->get_endOfConstruct(),
                                   reference->get_startOfConstruct(),
                                   reference->get_endOfConstruct()};
  SageInterface::replaceExpression(
      increment, SageBuilder::buildPlusAssignOp(
                     SageBuilder::buildVarRefExp(symbol),
                     SageBuilder::buildIntVal(2), reference->get_type()));
  for (SgNode *node : retired)
    ROSE_ASSERT(node != nullptr && !SgNode::isLiveNode(node));
  ROSE_ASSERT(SgNode::isLiveNode(symbol));
  auto *declaration = isSgVariableDeclaration(loop->get_init_stmt().front());
  ROSE_ASSERT(declaration != nullptr);
  auto *initializer = isSgAssignInitializer(
      declaration->get_variables().front()->get_initializer());
  ROSE_ASSERT(initializer != nullptr);
  SgExpression *lower = initializer->get_operand();
  SgExpression *step = loop->get_increment();
  SageInterface::setLoopLowerBound(loop, SageBuilder::buildIntVal(1));
  ROSE_ASSERT(!SgNode::isLiveNode(lower));
  SageInterface::setLoopStride(loop, SageBuilder::buildIntVal(3));
  ROSE_ASSERT(!SgNode::isLiveNode(step));
  AstTests::runAllTests(project.get());

  if (orphan) {
    // This reference is outside every forward owning edge. A stale parent
    // pointing into the loop must not admit it to the deletion boundary.
    auto *external = SageBuilder::buildVarRefExp(symbol);
    external->set_parent(loop);
  }
  if (typeOrphan) {
    auto *external = SageBuilder::buildTypeExpression_nfi(cachedPointer);
    external->set_parent(loop);
  }
  SageInterface::removeStatement(loop, false);
  SageInterface::deleteAST(loop,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!orphan && !typeOrphan);
  ROSE_ASSERT(!SgNode::isLiveNode(loop));
  ROSE_ASSERT(!SgNode::isLiveNode(symbol));
  ROSE_ASSERT(!SgNode::isLiveNode(lexicalType));
  ROSE_ASSERT(!SgNode::isLiveNode(lexicalOperand));
  ROSE_ASSERT(!SgNode::isLiveNode(cachedPointer));
  ROSE_ASSERT(!SgNode::isLiveNode(cachedConst));
  ROSE_ASSERT(!SgNode::isLiveNode(modifierCache));
  AstTests::runAllTests(project.get());
  return 0;
}
