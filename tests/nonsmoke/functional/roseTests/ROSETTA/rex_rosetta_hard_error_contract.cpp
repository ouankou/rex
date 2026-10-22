#include "Rose/StringUtility.h"
#include "sage3basic.h"

#include "resetParentPointers.h"
#include "sageBuilder.h"
#include "sageInterface.h"

#include <cstdio>
#include <cstring>

namespace {
class DeclarationGroupMutationProbe : public SgDeclarationGroupStatement {
public:
  using SgDeclarationGroupStatement::insert_child;
  using SgDeclarationGroupStatement::replace_child;
};

class BasicBlockMutationProbe : public SgBasicBlock {
public:
  using SgBasicBlock::insert_child;
};

struct DeclarationGroupFixture {
  SgBasicBlock *block = nullptr;
  SgNullStatement *before = nullptr;
  SgDeclarationGroupStatement *group = nullptr;
  SgVariableDeclaration *first = nullptr;
  SgVariableDeclaration *middle = nullptr;
  SgVariableDeclaration *last = nullptr;
  SgNullStatement *after = nullptr;
};

DeclarationGroupFixture buildDeclarationGroupFixture() {
  DeclarationGroupFixture fixture;
  fixture.block = SageBuilder::buildBasicBlock();
  fixture.before = SageBuilder::buildNullStatement();
  fixture.first = SageBuilder::buildVariableDeclaration(
      "first", SageBuilder::buildIntType(), nullptr, fixture.block);
  fixture.middle = SageBuilder::buildVariableDeclaration(
      "middle", SageBuilder::buildIntType(), nullptr, fixture.block);
  fixture.last = SageBuilder::buildVariableDeclaration(
      "last", SageBuilder::buildIntType(), nullptr, fixture.block);
  fixture.after = SageBuilder::buildNullStatement();

  // The builder establishes each declarator's semantic scope.  The typed
  // source group, rather than that scope, becomes its exact structural owner.
  fixture.first->set_parent(nullptr);
  fixture.middle->set_parent(nullptr);
  fixture.last->set_parent(nullptr);

  fixture.group = new SgDeclarationGroupStatement();
  SageInterface::setSourcePosition(fixture.group);
  fixture.group->set_scope(fixture.block);
  fixture.group->set_source_terminator(
      SgDeclarationGroupStatement::e_source_terminator_file_semicolon);
  fixture.group->append_declaration(fixture.first);
  fixture.group->append_declaration(fixture.middle);
  fixture.group->append_declaration(fixture.last);
  fixture.group->validate();

  fixture.block->append_statement(fixture.before);
  fixture.block->append_statement(fixture.group);
  fixture.block->append_statement(fixture.after);
  return fixture;
}

int verifyDeclarationGroupIterationAndAtomicMutation() {
  DeclarationGroupFixture fixture = buildDeclarationGroupFixture();
  if (SageInterface::getNextStatement(fixture.first) != fixture.middle ||
      SageInterface::getNextStatement(fixture.middle) != fixture.last ||
      SageInterface::getNextStatement(fixture.last) != fixture.after ||
      SageInterface::getPreviousStatement(fixture.last) != fixture.middle ||
      SageInterface::getPreviousStatement(fixture.middle) != fixture.first ||
      SageInterface::getPreviousStatement(fixture.first) != fixture.before) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: declaration-group iteration is not exact\n");
    return 4;
  }

  SgForInitStatement *for_init = new SgForInitStatement();
  SageInterface::setSourcePosition(for_init);
  SgBasicBlock *for_block = SageBuilder::buildBasicBlock();
  SgExprStatement *init_before =
      SageBuilder::buildExprStatement(SageBuilder::buildIntVal(1));
  SgExprStatement *init_after =
      SageBuilder::buildExprStatement(SageBuilder::buildIntVal(2));
  SgVariableDeclaration *init_first = SageBuilder::buildVariableDeclaration(
      "init_first", SageBuilder::buildIntType(), nullptr, for_block);
  SgVariableDeclaration *init_last = SageBuilder::buildVariableDeclaration(
      "init_last", SageBuilder::buildIntType(), nullptr, for_block);
  init_first->set_parent(nullptr);
  init_last->set_parent(nullptr);
  SgDeclarationGroupStatement *init_group = new SgDeclarationGroupStatement();
  SageInterface::setSourcePosition(init_group);
  init_group->set_scope(for_block);
  init_group->set_source_terminator(
      SgDeclarationGroupStatement::e_source_terminator_file_semicolon);
  init_group->append_declaration(init_first);
  init_group->append_declaration(init_last);
  for_init->append_init_stmt(init_before);
  for_init->append_init_stmt(init_group);
  for_init->append_init_stmt(init_after);
  SgForStatement *for_statement = SageBuilder::buildForStatement(
      for_init, SageBuilder::buildNullStatement(),
      SageBuilder::buildNullExpression(
          SgNullExpression::e_null_expression_syntactic_absence),
      SageBuilder::buildNullStatement());
  for_block->append_statement(for_statement);
  init_group->set_scope(for_statement);
  init_first->get_variables().front()->set_scope(for_statement);
  init_last->get_variables().front()->set_scope(for_statement);
  init_group->validate();
  SgStatement *next_init_first = SageInterface::getNextStatement(init_first);
  SgStatement *next_init_last = SageInterface::getNextStatement(init_last);
  SgStatement *previous_init_last =
      SageInterface::getPreviousStatement(init_last);
  SgStatement *previous_init_first =
      SageInterface::getPreviousStatement(init_first);
  if (next_init_first != init_last || next_init_last != init_after ||
      previous_init_last != init_first || previous_init_first != init_before) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: for-init declaration-group iteration is "
                 "not exact\n");
    return 5;
  }

  SgNullStatement *replacement = SageBuilder::buildNullStatement();
  SageInterface::replaceStatement(fixture.group, replacement);
  const SgStatementPtrList &statements = fixture.block->get_statements();
  if (replacement->get_parent() != fixture.block || statements.size() != 3 ||
      statements[0] != fixture.before || statements[1] != replacement ||
      statements[2] != fixture.after ||
      fixture.group->get_parent() != nullptr) {
    std::fprintf(stderr, "REX_TEST_ERROR: atomic declaration-group replacement "
                         "failed\n");
    return 6;
  }
  return 0;
}

int verifyMixedTraversalSuccessorOrder() {
  SgOmpContextSelector *selector =
      new SgOmpContextSelector(SgOmpClause::e_omp_context_trait_construct);
  SgIntVal *score = SageBuilder::buildIntVal(7);
  SgNullStatement *construct = SageBuilder::buildNullStatement();
  SgOmpContextSelectorProperty *property = new SgOmpContextSelectorProperty();
  selector->set_score(score);
  selector->set_construct_directive(construct);
  selector->get_properties().push_back(property);
  score->set_parent(selector);
  construct->set_parent(selector);
  property->set_parent(selector);

  const SgNodePtrList successors = selector->get_traversalSuccessorContainer();
  if (successors.size() != 3 || successors[0] != score ||
      successors[1] != construct || successors[2] != property) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: mixed traversal successors are not exact\n");
    return 7;
  }
  return 0;
}

int verifyMultipleContainerTraversalSuccessorOrder() {
  SgTemplateVariableDeclaration *declaration =
      new SgTemplateVariableDeclaration(SgName("rex_multiple_containers"),
                                        SageBuilder::buildIntType(), nullptr,
                                        {});
  ROSE_ASSERT(declaration->get_variables().size() == 1);
  SgInitializedName *variable = declaration->get_variables().front();
  SgTemplateParameter *parameter = new SgTemplateParameter();
  declaration->get_templateParameters().push_back(parameter);
  parameter->set_parent(declaration);

  const SgNodePtrList successors =
      declaration->get_traversalSuccessorContainer();
  const std::vector<std::string> names =
      declaration->get_traversalSuccessorNamesContainer();
  if (declaration->get_numberOfTraversalSuccessors() != 2 ||
      successors.size() != 2 || names.size() != 2 ||
      successors[0] != variable || successors[1] != parameter ||
      declaration->get_traversalSuccessorByIndex(0) != variable ||
      declaration->get_traversalSuccessorByIndex(1) != parameter ||
      declaration->get_childIndex(variable) != 0 ||
      declaration->get_childIndex(parameter) != 1 || names[0] != "*[0]" ||
      names[1] != "*[1]") {
    std::fprintf(
        stderr,
        "REX_TEST_ERROR: multiple-container traversal order is not exact\n");
    return 11;
  }
  return 0;
}

int verifyBasicBlockExtractionOwnershipAndOrder() {
  BasicBlockMutationProbe *destination = new BasicBlockMutationProbe();
  SgNullStatement *before = SageBuilder::buildNullStatement();
  SgNullStatement *target = SageBuilder::buildNullStatement();
  SgNullStatement *after = SageBuilder::buildNullStatement();
  destination->append_statement(before);
  destination->append_statement(target);
  destination->append_statement(after);

  SgNullStatement *first = SageBuilder::buildNullStatement();
  SgNullStatement *second = SageBuilder::buildNullStatement();
  SgBasicBlock *source = SageBuilder::buildBasicBlock();
  source->append_statement(first);
  source->append_statement(second);
  ROSE_ASSERT(first->get_parent() == source);
  ROSE_ASSERT(second->get_parent() == source);
  destination->insert_child(target, source, true, true);

  const SgStatementPtrList &statements = destination->get_statements();
  if (statements.size() != 5 || statements[0] != before ||
      statements[1] != first || statements[2] != second ||
      statements[3] != target || statements[4] != after ||
      first->get_parent() != destination ||
      second->get_parent() != destination ||
      !source->get_statements().empty()) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: basic-block extraction did not preserve "
                 "order and exact ownership\n");
    return 9;
  }
  return 0;
}

SgClassDeclaration *
buildClassDeclarationForTypeContract(const char *name, SgScopeStatement *scope,
                                     bool publishFirstOwner) {
  SgClassDeclaration *declaration = new SgClassDeclaration(
      SgName(name), SgClassDeclaration::e_class, nullptr, nullptr);
  if (scope != nullptr) {
    declaration->set_parent(scope);
    declaration->set_scope(scope);
  }
  if (publishFirstOwner) {
    declaration->set_firstNondefiningDeclaration(declaration);
  }
  return declaration;
}

int verifyCanonicalClassTypeOwner() {
  SgGlobal *scope = new SgGlobal();
  SgClassDeclaration *declaration = buildClassDeclarationForTypeContract(
      "rex_canonical_class_type", scope, true);
  SgClassType *type = SgClassType::createType(declaration);
  if (type == nullptr || declaration->get_type() != type ||
      type->get_declaration() != declaration) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: class type has no exact canonical owner\n");
    return 8;
  }
  return 0;
}

struct RequiredDeclarationOwnershipFixture {
  SgBasicBlock *scope = nullptr;
  SgAccessLabelStatement *declaration = nullptr;
};

RequiredDeclarationOwnershipFixture buildRequiredDeclarationOwnershipFixture() {
  RequiredDeclarationOwnershipFixture fixture;
  fixture.scope = SageBuilder::buildBasicBlock();
  fixture.declaration =
      new SgAccessLabelStatement(SgAccessLabelStatement::e_access_label_public);
  SageInterface::setSourcePosition(fixture.declaration);
  fixture.scope->append_statement(fixture.declaration);
  return fixture;
}

SgClassType *buildPublishedNamedType(SgScopeStatement *scope) {
  SgClassDeclaration *declaration =
      new SgClassDeclaration(SgName("rex_parent_pointer_named_type"),
                             SgClassDeclaration::e_class, nullptr, nullptr);
  declaration->set_scope(scope);
  declaration->set_firstNondefiningDeclaration(declaration);
  SageInterface::setSourcePosition(declaration);
  scope->append_statement(declaration);
  return SgClassType::createType(declaration);
}

int verifyRequiredDeclarationOwnership() {
  RequiredDeclarationOwnershipFixture fixture =
      buildRequiredDeclarationOwnershipFixture();
  if (fixture.declaration->get_definingDeclaration() != nullptr ||
      fixture.declaration->get_firstNondefiningDeclaration() != nullptr) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: access label does not exercise optional "
                 "declaration-chain links\n");
    return 12;
  }

  validateFreshSubtreeOwnership(fixture.declaration, fixture.scope);
  SgClassType *namedType = buildPublishedNamedType(fixture.scope);
  auto *use = SageBuilder::buildVariableDeclaration(
      "rex_named_type_use", namedType, nullptr, fixture.scope);
  fixture.scope->append_statement(use);
  validateFreshSubtreeOwnership(use, fixture.scope);
  return 0;
}

int verifyAccessLabelConstruction() {
  const SgAccessLabelStatement::access_label_kind_enum kinds[] = {
      SgAccessLabelStatement::e_access_label_private,
      SgAccessLabelStatement::e_access_label_protected,
      SgAccessLabelStatement::e_access_label_public};
  for (SgAccessLabelStatement::access_label_kind_enum kind : kinds) {
    SgAccessLabelStatement *label = new SgAccessLabelStatement(kind);
    label->validate();
    const SgAccessModifier &legacy =
        label->get_declarationModifier().get_accessModifier();
    if (label->get_label_kind() != kind || !legacy.isUnknown() ||
        legacy.get_is_explicit() ||
        label->get_definingDeclaration() != nullptr ||
        label->get_firstNondefiningDeclaration() != nullptr) {
      std::fprintf(stderr, "REX_TEST_ERROR: fresh access label has conflicting "
                           "legacy declaration state\n");
      return 10;
    }
  }
  return 0;
}

int verifyStatementExpressionReplacement() {
  SgExpression *comparison = SageBuilder::buildIntVal(1);
  SgExpression *increment = SageBuilder::buildIntVal(2);
  SgRangeBasedForStatement *range =
      SageBuilder::buildRangeBasedForStatement_nfi(
          nullptr, nullptr, nullptr, nullptr, comparison, increment, nullptr);
  SgExpression *new_comparison = SageBuilder::buildIntVal(3);
  SgStatement *statement = range;
  statement->replace_expression(comparison, new_comparison);
  if (range->get_not_equal_expression() != new_comparison ||
      new_comparison->get_parent() != range ||
      comparison->get_parent() != nullptr ||
      range->get_increment_expression() != increment ||
      increment->get_parent() != range)
    return 11;

  SgExpression *new_increment = SageBuilder::buildIntVal(4);
  statement->replace_expression(increment, new_increment);
  if (range->get_increment_expression() != new_increment ||
      new_increment->get_parent() != range ||
      increment->get_parent() != nullptr)
    return 12;

  SgExpression *returned = SageBuilder::buildIntVal(5);
  SgReturnStmt *return_statement = new SgReturnStmt(returned, nullptr);
  if (returned->get_parent() != return_statement)
    return 13;
  SgExpression *new_returned = SageBuilder::buildIntVal(6);
  statement = return_statement;
  statement->replace_expression(returned, new_returned);
  if (return_statement->get_expression() != new_returned ||
      new_returned->get_parent() != return_statement ||
      returned->get_parent() != nullptr)
    return 14;

  SgExprListExp *list = SageBuilder::buildExprListExp();
  SgExpression *old_element = SageBuilder::buildIntVal(7);
  SgExpression *new_element = SageBuilder::buildIntVal(8);
  list->append_expression(old_element);
  if (list->replace_expression(old_element, new_element) != 1 ||
      list->get_expressions().size() != 1 ||
      list->get_expressions().front() != new_element ||
      new_element->get_parent() != list ||
      old_element->get_parent() != nullptr ||
      list->replace_expression(new_element, new_element) != 1)
    return 15;

  SgExpression *left = SageBuilder::buildIntVal(9);
  SgExpression *right = SageBuilder::buildIntVal(10);
  SgAddOp *binary = new SgAddOp(left, right, SageBuilder::buildIntType());
  left->set_parent(binary);
  right->set_parent(binary);
  SgExpression *replacement = SageBuilder::buildIntVal(11);
  if (binary->replace_expression(left, replacement) != 1 ||
      binary->get_lhs_operand() != replacement ||
      replacement->get_parent() != binary || left->get_parent() != nullptr ||
      binary->get_rhs_operand() != right || right->get_parent() != binary)
    return 16;

  SgExpression *prefix = SageBuilder::buildIntVal(12);
  if (replacement->get_parent() != binary)
    return 17;
  SgCommaOpExp *wrapper =
      SageInterface::insertBeforeUsingCommaOp(prefix, replacement);
  if (binary->get_lhs_operand() != wrapper || wrapper->get_parent() != binary ||
      wrapper->get_lhs_operand() != prefix || prefix->get_parent() != wrapper ||
      wrapper->get_rhs_operand() != replacement ||
      replacement->get_parent() != wrapper)
    return 17;

  SgExpression *old_operand = SageBuilder::buildIntVal(13);
  SgMinusOp *unary = new SgMinusOp(old_operand, SageBuilder::buildIntType(),
                                   SgUnaryOp::prefix);
  old_operand->set_parent(unary);
  SgExpression *new_operand = SageBuilder::buildIntVal(14);
  if (unary->replace_expression(old_operand, new_operand) != 1 ||
      unary->get_operand() != new_operand ||
      new_operand->get_parent() != unary ||
      old_operand->get_parent() != nullptr ||
      unary->replace_expression(new_operand, new_operand) != 1)
    return 18;

  SgExpression *condition = SageBuilder::buildIntVal(15);
  SgExpression *true_expression = SageBuilder::buildIntVal(16);
  SgExpression *false_expression = SageBuilder::buildIntVal(17);
  SgConditionalExp *conditional = SageBuilder::buildConditionalExp(
      condition, true_expression, false_expression,
      SageBuilder::buildIntType());
  SgExpression *new_true_expression = SageBuilder::buildIntVal(18);
  if (conditional->replace_expression(true_expression, new_true_expression) !=
          1 ||
      conditional->get_true_exp() != new_true_expression ||
      new_true_expression->get_parent() != conditional ||
      true_expression->get_parent() != nullptr ||
      conditional->get_conditional_exp() != condition ||
      condition->get_parent() != conditional ||
      conditional->get_false_exp() != false_expression ||
      false_expression->get_parent() != conditional)
    return 19;

  SgVarArgOp *vararg = SageBuilder::buildVarArgOp_nfi(
      SageBuilder::buildIntVal(19), SageBuilder::buildIntType());
  SgExpression *old_vararg_operand = vararg->get_operand_expr();
  SgExpression *new_vararg_operand = SageBuilder::buildIntVal(20);
  if (vararg->replace_expression(old_vararg_operand, new_vararg_operand) != 1 ||
      vararg->get_operand_expr() != new_vararg_operand ||
      new_vararg_operand->get_parent() != vararg ||
      old_vararg_operand->get_parent() != nullptr)
    return 20;

  SgAssignInitializer *initializer = SageBuilder::buildAssignInitializer(
      SageBuilder::buildIntVal(21), SageBuilder::buildIntType());
  SgExpression *old_initializer_operand = initializer->get_operand();
  SgExpression *new_initializer_operand = SageBuilder::buildIntVal(22);
  if (initializer->replace_expression(old_initializer_operand,
                                      new_initializer_operand) != 1 ||
      initializer->get_operand() != new_initializer_operand ||
      new_initializer_operand->get_parent() != initializer ||
      old_initializer_operand->get_parent() != nullptr)
    return 21;

  SgExprListExp *aggregate_list = SageBuilder::buildExprListExp();
  SgAggregateInitializer *aggregate = SageBuilder::buildAggregateInitializer(
      aggregate_list, SageBuilder::buildIntType(),
      SgAggregateInitializer::e_aggregate_initializer_source_braced);
  SgExprListExp *new_aggregate_list = SageBuilder::buildExprListExp();
  if (aggregate->replace_expression(aggregate_list, new_aggregate_list) != 1 ||
      aggregate->get_initializers() != new_aggregate_list ||
      new_aggregate_list->get_parent() != aggregate ||
      aggregate_list->get_parent() != nullptr)
    return 22;

  SgExprListExp *braced_list = SageBuilder::buildExprListExp();
  SgBracedInitializer *braced = SageBuilder::buildBracedInitializer(
      braced_list, SageBuilder::buildIntType());
  SgExprListExp *new_braced_list = SageBuilder::buildExprListExp();
  if (braced->replace_expression(braced_list, new_braced_list) != 1 ||
      braced->get_initializers() != new_braced_list ||
      new_braced_list->get_parent() != braced ||
      braced_list->get_parent() != nullptr)
    return 23;
  return 0;
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: expected one hard-error contract mode\n");
    return 2;
  }

  if (std::strcmp(argv[1], "empty-replace-search") == 0) {
    (void)Rose::StringUtility::replaceAllCopy("value", "", "new");
  } else if (std::strcmp(argv[1], "empty-copy-search") == 0) {
    (void)Rose::StringUtility::copyEdit("value", "", "new");
  } else if (std::strcmp(argv[1], "empty-copy-lines-search") == 0) {
    (void)Rose::StringUtility::copyEdit(
        Rose::StringUtility::FileWithLineNumbers{}, "", "new");
  } else if (std::strcmp(argv[1], "empty-insert-lines-search") == 0) {
    (void)Rose::StringUtility::copyEdit(
        Rose::StringUtility::FileWithLineNumbers{}, "",
        Rose::StringUtility::FileWithLineNumbers{});
  } else if (std::strcmp(argv[1], "function-argument-index") == 0) {
    SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
    arguments->insert_argument(1, SageBuilder::buildIntType());
  } else if (std::strcmp(argv[1], "null-symbol") == 0) {
    SgSymbolTable *symbols = new SgSymbolTable();
    symbols->insert(SgName("foo"), nullptr);
  } else if (std::strcmp(argv[1], "declaration-group-replace") == 0) {
    DeclarationGroupMutationProbe *group = new DeclarationGroupMutationProbe();
    SgNullStatement *target = new SgNullStatement();
    SgNullStatement *replacement = new SgNullStatement();
    group->replace_child(target, replacement);
  } else if (std::strcmp(argv[1], "declaration-group-insert") == 0) {
    DeclarationGroupMutationProbe *group = new DeclarationGroupMutationProbe();
    SgNullStatement *target = new SgNullStatement();
    SgNullStatement *inserted = new SgNullStatement();
    group->insert_child(target, inserted);
  } else if (std::strcmp(argv[1], "declaration-group-remove-member") == 0) {
    DeclarationGroupFixture fixture = buildDeclarationGroupFixture();
    SageInterface::removeStatement(fixture.first);
  } else if (std::strcmp(argv[1], "declaration-group-replace-member") == 0) {
    DeclarationGroupFixture fixture = buildDeclarationGroupFixture();
    SageInterface::replaceStatement(fixture.middle,
                                    SageBuilder::buildNullStatement());
  } else if (std::strcmp(argv[1], "declaration-group-insert-at-member") == 0) {
    DeclarationGroupFixture fixture = buildDeclarationGroupFixture();
    SageInterface::insertStatement(fixture.last,
                                   SageBuilder::buildNullStatement(), true);
  } else if (std::strcmp(argv[1], "declaration-group-iteration") == 0) {
    return verifyDeclarationGroupIterationAndAtomicMutation();
  } else if (std::strcmp(argv[1], "mixed-traversal-successor-order") == 0) {
    return verifyMixedTraversalSuccessorOrder();
  } else if (std::strcmp(argv[1],
                         "multiple-container-traversal-successor-order") == 0) {
    return verifyMultipleContainerTraversalSuccessorOrder();
  } else if (std::strcmp(argv[1], "basic-block-extraction-order") == 0) {
    return verifyBasicBlockExtractionOwnershipAndOrder();
  } else if (std::strcmp(argv[1], "basic-block-extraction-foreign-child") ==
             0) {
    BasicBlockMutationProbe *destination = new BasicBlockMutationProbe();
    SgNullStatement *target = SageBuilder::buildNullStatement();
    destination->append_statement(target);
    SgBasicBlock *source = SageBuilder::buildBasicBlock();
    SgNullStatement *foreign = SageBuilder::buildNullStatement();
    source->get_statements().push_back(foreign);
    destination->insert_child(target, source, true, true);
  } else if (std::strcmp(argv[1], "basic-block-extraction-null-child") == 0) {
    BasicBlockMutationProbe *destination = new BasicBlockMutationProbe();
    SgNullStatement *target = SageBuilder::buildNullStatement();
    destination->append_statement(target);
    SgBasicBlock *source = SageBuilder::buildBasicBlock();
    source->get_statements().push_back(nullptr);
    destination->insert_child(target, source, true, true);
  } else if (std::strcmp(argv[1], "basic-block-extraction-duplicate-child") ==
             0) {
    BasicBlockMutationProbe *destination = new BasicBlockMutationProbe();
    SgNullStatement *target = SageBuilder::buildNullStatement();
    destination->append_statement(target);
    SgBasicBlock *source = SageBuilder::buildBasicBlock();
    SgNullStatement *duplicate = SageBuilder::buildNullStatement();
    source->append_statement(duplicate);
    source->get_statements().push_back(duplicate);
    destination->insert_child(target, source, true, true);
  } else if (std::strcmp(argv[1],
                         "basic-block-extraction-destination-shared-child") ==
             0) {
    BasicBlockMutationProbe *destination = new BasicBlockMutationProbe();
    SgNullStatement *target = SageBuilder::buildNullStatement();
    destination->append_statement(target);
    SgBasicBlock *source = SageBuilder::buildBasicBlock();
    SgNullStatement *shared = SageBuilder::buildNullStatement();
    source->append_statement(shared);
    destination->get_statements().push_back(shared);
    destination->insert_child(target, source, true, true);
  } else if (std::strcmp(argv[1], "class-type-canonical-owner") == 0) {
    return verifyCanonicalClassTypeOwner();
  } else if (std::strcmp(argv[1], "required-declaration-ownership") == 0) {
    return verifyRequiredDeclarationOwnership();
  } else if (std::strcmp(argv[1], "declaration-start-null") == 0) {
    RequiredDeclarationOwnershipFixture fixture =
        buildRequiredDeclarationOwnershipFixture();
    fixture.declaration->set_startOfConstruct(nullptr);
    validateFreshSubtreeOwnership(fixture.declaration, fixture.scope);
  } else if (std::strcmp(argv[1], "declaration-end-null") == 0) {
    RequiredDeclarationOwnershipFixture fixture =
        buildRequiredDeclarationOwnershipFixture();
    fixture.declaration->set_endOfConstruct(nullptr);
    validateFreshSubtreeOwnership(fixture.declaration, fixture.scope);
  } else if (std::strcmp(argv[1], "declaration-start-foreign-owner") == 0) {
    RequiredDeclarationOwnershipFixture fixture =
        buildRequiredDeclarationOwnershipFixture();
    fixture.declaration->get_startOfConstruct()->set_parent(
        SageBuilder::buildBasicBlock());
    validateFreshSubtreeOwnership(fixture.declaration, fixture.scope);
  } else if (std::strcmp(argv[1], "declaration-end-foreign-owner") == 0) {
    RequiredDeclarationOwnershipFixture fixture =
        buildRequiredDeclarationOwnershipFixture();
    fixture.declaration->get_endOfConstruct()->set_parent(
        SageBuilder::buildBasicBlock());
    validateFreshSubtreeOwnership(fixture.declaration, fixture.scope);
  } else if (std::strcmp(argv[1], "named-type-null-declaration") == 0) {
    SgBasicBlock *scope = SageBuilder::buildBasicBlock();
    SgClassType *namedType = buildPublishedNamedType(scope);
    auto *use = SageBuilder::buildVariableDeclaration(
        "rex_named_type_use", namedType, nullptr, scope);
    scope->append_statement(use);
    namedType->set_declaration(nullptr);
    validateFreshSubtreeOwnership(use, scope);
  } else if (std::strcmp(argv[1], "access-label-construction") == 0) {
    return verifyAccessLabelConstruction();
  } else if (std::strcmp(argv[1], "statement-expression-replacement") == 0) {
    return verifyStatementExpressionReplacement();
  } else if (std::strcmp(argv[1], "access-label-conflicting-legacy") == 0) {
    SgAccessLabelStatement *label = new SgAccessLabelStatement(
        SgAccessLabelStatement::e_access_label_public);
    SgAccessModifier &legacy =
        label->get_declarationModifier().get_accessModifier();
    legacy.setPublic();
    legacy.set_is_explicit(true);
    label->validate();
  } else if (std::strcmp(argv[1], "class-type-missing-first-owner") == 0) {
    SgBasicBlock *scope = SageBuilder::buildBasicBlock();
    SgClassDeclaration *declaration = buildClassDeclarationForTypeContract(
        "rex_missing_first_owner", scope, false);
    (void)SgClassType::createType(declaration);
  } else if (std::strcmp(argv[1], "class-type-later-owner") == 0) {
    SgBasicBlock *scope = SageBuilder::buildBasicBlock();
    SgClassDeclaration *first =
        buildClassDeclarationForTypeContract("rex_later_owner", scope, true);
    SgClassDeclaration *later =
        buildClassDeclarationForTypeContract("rex_later_owner", scope, false);
    later->set_firstNondefiningDeclaration(first);
    (void)SgClassType::createType(later);
  } else if (std::strcmp(argv[1], "class-type-missing-scope") == 0) {
    SgClassDeclaration *declaration = buildClassDeclarationForTypeContract(
        "rex_missing_class_scope", nullptr, true);
    (void)SgClassType::createType(declaration);
  } else if (std::strcmp(argv[1], "variable-inline-null-owner") == 0) {
    SgBasicBlock *scope = SageBuilder::buildBasicBlock();
    SgVariableDeclaration *variable = SageBuilder::buildVariableDeclaration(
        "rex_inline_null", SageBuilder::buildIntType(), nullptr, scope);
    variable->set_baseTypeDefiningDeclaration(nullptr);
  } else if (std::strcmp(argv[1], "binary-foreign-replacement") == 0) {
    SgAddOp *expression =
        new SgAddOp(SageBuilder::buildIntVal(1), SageBuilder::buildIntVal(2),
                    SageBuilder::buildIntType());
    expression->replace_expression(SageBuilder::buildIntVal(3),
                                   SageBuilder::buildIntVal(4));
  } else if (std::strcmp(argv[1], "unary-foreign-replacement") == 0) {
    SgMinusOp *expression =
        new SgMinusOp(SageBuilder::buildIntVal(1), SageBuilder::buildIntType(),
                      SgUnaryOp::prefix);
    expression->replace_expression(SageBuilder::buildIntVal(2),
                                   SageBuilder::buildIntVal(3));
  } else if (std::strcmp(argv[1], "vararg-foreign-replacement") == 0) {
    SgVarArgOp *expression = new SgVarArgOp(SageBuilder::buildIntVal(1),
                                            SageBuilder::buildIntType());
    expression->replace_expression(SageBuilder::buildIntVal(2),
                                   SageBuilder::buildIntVal(3));
  } else if (std::strcmp(argv[1], "unary-null-operand") == 0) {
    (void)SageBuilder::buildMinusOp(nullptr, SageBuilder::buildIntType());
  } else if (std::strcmp(argv[1], "binary-null-operand") == 0) {
    (void)SageBuilder::buildAddOp(SageBuilder::buildIntVal(1), nullptr,
                                  SageBuilder::buildIntType());
  } else if (std::strcmp(argv[1], "throw-expression-null-operand") == 0) {
    (void)SageBuilder::buildThrowOp(nullptr, SageBuilder::buildVoidType(),
                                    SgThrowOp::throw_expression);
  } else if (std::strcmp(argv[1], "rethrow-nonnull-operand") == 0) {
    (void)SageBuilder::buildThrowOp(SageBuilder::buildIntVal(1),
                                    SageBuilder::buildVoidType(),
                                    SgThrowOp::rethrow);
  } else if (std::strcmp(argv[1], "unary-owned-operand") == 0) {
    SgIntVal *owned = SageBuilder::buildIntVal(1);
    SgMinusOp *owner =
        new SgMinusOp(owned, SageBuilder::buildIntType(), SgUnaryOp::prefix);
    owned->set_parent(owner);
    (void)SageBuilder::buildMinusOp(owned, SageBuilder::buildIntType());
  } else if (std::strcmp(argv[1], "binary-owned-operand") == 0) {
    SgIntVal *owned = SageBuilder::buildIntVal(1);
    SgAddOp *owner = new SgAddOp(owned, SageBuilder::buildIntVal(2),
                                 SageBuilder::buildIntType());
    owned->set_parent(owner);
    (void)SageBuilder::buildAddOp(owned, SageBuilder::buildIntVal(3),
                                  SageBuilder::buildIntType());
  } else if (std::strcmp(argv[1], "binary-duplicate-operand") == 0) {
    SgIntVal *shared = SageBuilder::buildIntVal(1);
    (void)SageBuilder::buildAddOp(shared, shared, SageBuilder::buildIntType());
  } else if (std::strcmp(argv[1], "throw-owned-operand") == 0) {
    SgIntVal *owned = SageBuilder::buildIntVal(1);
    SgMinusOp *owner =
        new SgMinusOp(owned, SageBuilder::buildIntType(), SgUnaryOp::prefix);
    owned->set_parent(owner);
    (void)SageBuilder::buildThrowOp(owned, SageBuilder::buildVoidType(),
                                    SgThrowOp::throw_expression);
  } else {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: unknown hard-error contract mode %s\n",
                 argv[1]);
    return 2;
  }

  std::fprintf(
      stderr,
      "REX_TEST_ERROR: malformed AST operation unexpectedly returned\n");
  return 3;
}
