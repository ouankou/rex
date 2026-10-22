#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  const auto statements = NodeQuery::querySubTree(project, V_SgAsmStmt);
  ROSE_ASSERT(statements.size() == 2);
  auto *statement = isSgAsmStmt(statements.front());
  ROSE_ASSERT(statement->get_isVolatile());
  ROSE_ASSERT(statement->unparseToString().find("__asm__ volatile") !=
              std::string::npos);
  const auto operands = NodeQuery::querySubTree(project, V_SgAsmOp);
  ROSE_ASSERT(operands.size() == 6);
  auto *operand = isSgAsmOp(operands.front());
  ROSE_ASSERT(operand->get_isOutputOperand());
  ROSE_ASSERT(operand->get_name() == "out");
  ROSE_ASSERT(
      operand->get_modifiers() ==
      (SgAsmOp::e_modify | SgAsmOp::e_earlyclobber | SgAsmOp::e_poor_choice));
  operand = isSgAsmOp(operands[1]);
  ROSE_ASSERT(!operand->get_isOutputOperand());
  ROSE_ASSERT(operand->get_name() == "in");
  ROSE_ASSERT(operand->get_modifiers() ==
              (SgAsmOp::e_input | SgAsmOp::e_poor_choice));
  operand = isSgAsmOp(operands[2]);
  ROSE_ASSERT(operand->get_isOutputOperand());
  ROSE_ASSERT(operand->get_constraint().allows_memory);
  ROSE_ASSERT(operand->get_constraint().allows_register);
  operand = isSgAsmOp(operands[3]);
  ROSE_ASSERT(operand->get_isOutputOperand());
  ROSE_ASSERT(operand->get_constraint().has_matching_input);
  operand = isSgAsmOp(operands[4]);
  ROSE_ASSERT(!operand->get_isOutputOperand());
  ROSE_ASSERT(operand->get_constraint().tied_output == 1);
  operand = isSgAsmOp(operands[5]);
  ROSE_ASSERT(operand->get_constraint().requires_immediate);
  auto *original = isSgAsmStmt(statements.back());
  SgTreeCopy copyHelp;
  auto *copied = isSgAsmStmt(original->copy(copyHelp));
  ROSE_ASSERT(copied != nullptr && copied != original);
  ROSE_ASSERT(copied->get_operands().size() == original->get_operands().size());
  for (size_t i = 0; i < original->get_operands().size(); ++i) {
    auto *source = isSgAsmOp(original->get_operands()[i]);
    auto *copy = isSgAsmOp(copied->get_operands()[i]);
    ROSE_ASSERT(copy != source && copy->get_parent() == copied);
    ROSE_ASSERT(copy->get_expression() != source->get_expression());
    ROSE_ASSERT(copy->get_expression()->get_parent() == copy);
    ROSE_ASSERT(copy->get_constraint().spelling ==
                source->get_constraint().spelling);
    ROSE_ASSERT(copy->get_constraint().tied_output ==
                source->get_constraint().tied_output);
    ROSE_ASSERT(copy->get_constraint().allows_memory ==
                source->get_constraint().allows_memory);
    ROSE_ASSERT(copy->get_constraint().requires_immediate ==
                source->get_constraint().requires_immediate);
  }
  SageInterface::deleteAST(copied,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  return backend(project);
}
