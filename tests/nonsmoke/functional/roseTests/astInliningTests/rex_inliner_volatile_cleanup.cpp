#include "inlinerSupport.h"
#include "rose.h"

int main(int argc, char **argv) {
  auto *project = frontend(argc, argv);
  auto *function = SageInterface::findMain(project);
  ROSE_ASSERT(function != nullptr && function->get_definition() != nullptr);
  auto *block = function->get_definition()->get_body();
  auto *declaration = SageBuilder::buildVariableDeclaration(
      "value", SageBuilder::buildVolatileType(SageBuilder::buildIntType()),
      nullptr, block);
  SageInterface::appendStatement(declaration, block);
  auto *read = SageBuilder::buildExprStatement(
      SageBuilder::buildVarRefExp("value", block));
  auto *negation = SageBuilder::buildExprStatement(
      SageBuilder::buildNotOp(SageBuilder::buildVarRefExp("value", block),
                              SageBuilder::buildBoolType()));
  auto *dummy = SageBuilder::buildExprStatement(SageBuilder::buildIntVal(0));
  SageInterface::appendStatement(read, block);
  SageInterface::appendStatement(negation, block);
  const SgStatementPtrList expected = block->get_statements();
  SageInterface::appendStatement(dummy, block);

  removeNullStatements(block);
  ROSE_ASSERT(block->get_statements() == expected);
  ROSE_ASSERT(read->get_parent() == block && negation->get_parent() == block);
  ROSE_ASSERT(dummy->get_parent() == nullptr);
  SageInterface::deleteAST(dummy);

  auto *cleanup = SageBuilder::buildBasicBlock();
  SageInterface::appendStatement(cleanup, block);
  auto *volatile_type =
      SageBuilder::buildVolatileType(SageBuilder::buildIntType());
  auto append_variable = [&](const char *name, SgType *type,
                             SgExpression *initializer) {
    auto *variable = SageBuilder::buildVariableDeclaration(
        name, type,
        initializer != nullptr
            ? SageBuilder::buildAssignInitializer(initializer, type)
            : nullptr,
        cleanup);
    SageInterface::appendStatement(variable, cleanup);
    return variable;
  };
  append_variable("source", volatile_type, SageBuilder::buildIntVal(3));
  append_variable("snapshot", SageBuilder::buildIntType(),
                  SageBuilder::buildVarRefExp("source", cleanup));
  append_variable("stored", volatile_type, SageBuilder::buildIntVal(1));
  append_variable("pointer", SageBuilder::buildPointerType(volatile_type),
                  SageBuilder::buildAddressOfOp(
                      SageBuilder::buildVarRefExp("source", cleanup),
                      SageBuilder::buildPointerType(volatile_type),
                      SgAddressOfOp::e_address_operand_object));
  append_variable(
      "dereferenced", SageBuilder::buildIntType(),
      SageBuilder::buildPointerDerefExp(
          SageBuilder::buildVarRefExp("pointer", cleanup), volatile_type));
  const SgStatementPtrList retained = cleanup->get_statements();
  auto *unused_plain = append_variable(
      "unused_plain", SageBuilder::buildIntType(), SageBuilder::buildIntVal(4));
  auto *unused_name = unused_plain->get_variables().front();
  auto *unused_symbol = unused_name->get_symbol_from_symbol_table();
  ROSE_ASSERT(unused_symbol != nullptr);
  removeUnusedVariables(cleanup);
  ROSE_ASSERT(cleanup->get_statements() == retained);
  ROSE_ASSERT(cleanup->lookup_variable_symbol("unused_plain") == nullptr);
  ROSE_ASSERT(!SgNode::isLiveNode(unused_plain));
  ROSE_ASSERT(!SgNode::isLiveNode(unused_name));
  ROSE_ASSERT(!SgNode::isLiveNode(unused_symbol));

  append_variable("copy_source", volatile_type, SageBuilder::buildIntVal(5));
  append_variable("copy_snapshot", SageBuilder::buildIntType(),
                  SageBuilder::buildVarRefExp("copy_source", cleanup));
  auto *snapshot_reference =
      SageBuilder::buildVarRefExp("copy_snapshot", cleanup);
  auto *snapshot_read = SageBuilder::buildExprStatement(snapshot_reference);
  SageInterface::appendStatement(snapshot_read, cleanup);
  auto *source_reference = SageBuilder::buildVarRefExp("copy_source", cleanup);
  auto *source_read = SageBuilder::buildExprStatement(source_reference);
  SageInterface::appendStatement(source_read, cleanup);
  append_variable("plain_source", SageBuilder::buildIntType(),
                  SageBuilder::buildIntVal(9));
  append_variable("plain_copy", SageBuilder::buildIntType(),
                  SageBuilder::buildVarRefExp("plain_source", cleanup));
  auto *plain_read = SageBuilder::buildExprStatement(
      SageBuilder::buildVarRefExp("plain_copy", cleanup));
  SageInterface::appendStatement(plain_read, cleanup);
  simpleCopyAndConstantPropagation(cleanup);
  ROSE_ASSERT(snapshot_read->get_expression() == snapshot_reference);
  ROSE_ASSERT(source_read->get_expression() == source_reference);
  auto *constant = isSgIntVal(plain_read->get_expression());
  ROSE_ASSERT(constant != nullptr && constant->get_value() == 9);
  cleanupInlinedCode(project);
  ROSE_ASSERT(snapshot_read->get_parent() == cleanup);
  ROSE_ASSERT(snapshot_read->get_expression() == snapshot_reference);
  ROSE_ASSERT(source_read->get_parent() == cleanup);
  ROSE_ASSERT(source_read->get_expression() == source_reference);
  auto *lifetime_scope = SageBuilder::buildBasicBlock();
  SageInterface::appendStatement(lifetime_scope, cleanup);
  auto *scoped_variable = SageBuilder::buildVariableDeclaration(
      "scoped_value", SageBuilder::buildIntType(), nullptr, lifetime_scope);
  SageInterface::appendStatement(scoped_variable, lifetime_scope);
  auto *empty_scope = SageBuilder::buildBasicBlock();
  SageInterface::appendStatement(empty_scope, cleanup);
  flattenBlocks(cleanup);
  ROSE_ASSERT(SgNode::isLiveNode(lifetime_scope));
  ROSE_ASSERT(lifetime_scope->get_parent() == cleanup);
  ROSE_ASSERT(scoped_variable->get_variables().front()->get_scope() ==
              lifetime_scope);
  ROSE_ASSERT(!SgNode::isLiveNode(empty_scope));
  SageInterface::deleteAST(project);
}
