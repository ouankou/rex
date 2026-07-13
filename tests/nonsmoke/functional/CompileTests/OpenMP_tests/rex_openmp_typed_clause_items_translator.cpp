#include "rose.h"

#include <string>
#include <vector>

namespace {

void requireVariableReference(SgExpression *expression, SgNode *owner,
                              const std::string &name) {
  ROSE_ASSERT(expression != nullptr && expression->get_parent() == owner);
  SgVarRefExp *reference = isSgVarRefExp(expression);
  ROSE_ASSERT(reference != nullptr && reference->get_symbol() != nullptr);
  ROSE_ASSERT(reference->get_symbol()->get_name().getString() == name);
}

void exerciseOwnedExpressionReplacement(SgExpression *owner,
                                        SgExpression *owned_child) {
  ROSE_ASSERT(owner != nullptr && owned_child != nullptr);
  SgIntVal *unowned_old = SageBuilder::buildIntVal(901);
  SgIntVal *unused_new = SageBuilder::buildIntVal(902);
  ROSE_ASSERT(owner->replace_expression(unowned_old, unused_new) == 0);
  ROSE_ASSERT(unowned_old->get_parent() == nullptr &&
              unused_new->get_parent() == nullptr);

  SgIntVal *replacement = SageBuilder::buildIntVal(903);
  ROSE_ASSERT(owner->replace_expression(owned_child, replacement) == 1);
  ROSE_ASSERT(replacement->get_parent() == owner &&
              owned_child->get_parent() == nullptr);
  ROSE_ASSERT(owner->replace_expression(replacement, replacement) == 1);
  ROSE_ASSERT(owner->replace_expression(replacement, owned_child) == 1);
  ROSE_ASSERT(owned_child->get_parent() == owner &&
              replacement->get_parent() == nullptr);
}

} // namespace

int main(int argc, char **argv) {
  std::string mode;
  std::vector<char *> frontend_arguments;
  frontend_arguments.push_back(argv[0]);
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--item-type" || argument == "--bad-first-separator" ||
        argument == "--bad-later-separator" ||
        argument == "--wrong-sizes-type" || argument == "--empty-sizes" ||
        argument == "--null-sizes-element" ||
        argument == "--missing-init-modifier-list" ||
        argument == "--replacement-null" || argument == "--replacement-owned" ||
        argument == "--replacement-wrong-apply-payload" ||
        argument == "--replacement-wrong-init-payload" ||
        argument == "--replacement-list-wrong-type") {
      ROSE_ASSERT(mode.empty());
      mode = argument;
    } else {
      frontend_arguments.push_back(argv[i]);
    }
  }

  SgProject *project = frontend(static_cast<int>(frontend_arguments.size()),
                                frontend_arguments.data());
  ROSE_ASSERT(project != nullptr);

  const Rose_STL_Container<SgNode *> reduction_nodes =
      NodeQuery::querySubTree(project, V_SgOmpReductionClause);
  ROSE_ASSERT(reduction_nodes.size() == 1);
  SgOmpReductionClause *reduction =
      isSgOmpReductionClause(reduction_nodes.front());
  ROSE_ASSERT(reduction != nullptr &&
              reduction->get_modifier() ==
                  SgOmpClause::e_omp_reduction_original_private &&
              reduction->get_identifier() ==
                  SgOmpClause::e_omp_reduction_plus &&
              reduction->get_user_defined_identifier() == nullptr &&
              reduction->get_variables() != nullptr &&
              reduction->get_variables()->get_parent() == reduction &&
              reduction->get_variables()->get_expressions().size() == 1);
  requireVariableReference(
      reduction->get_variables()->get_expressions().front(),
      reduction->get_variables(), "reduction_sum");

  const Rose_STL_Container<SgNode *> apply_nodes =
      NodeQuery::querySubTree(project, V_SgOmpApplyClause);
  SgOmpApplyClause *outer_apply = nullptr;
  SgOmpApplyClause *nested_apply = nullptr;
  for (SgNode *node : apply_nodes) {
    SgOmpApplyClause *clause = isSgOmpApplyClause(node);
    ROSE_ASSERT(clause != nullptr);
    if (isSgOmpApplyTransformation(clause->get_parent()) != nullptr) {
      nested_apply = clause;
    } else {
      outer_apply = clause;
    }
  }
  ROSE_ASSERT(outer_apply != nullptr && nested_apply != nullptr);
  ROSE_ASSERT(outer_apply->get_label() == "grid");
  ROSE_ASSERT(outer_apply->get_transformations().size() == 2);
  ROSE_ASSERT(outer_apply->get_transformations()[0]->get_kind() ==
              SgOmpClause::e_omp_apply_transform_unroll_partial);
  ROSE_ASSERT(outer_apply->get_transformations()[0]->get_separator() ==
              SgOmpClause::e_omp_clause_separator_none);
  ROSE_ASSERT(outer_apply->get_transformations()[1]->get_kind() ==
              SgOmpClause::e_omp_apply_transform_nested_apply);
  ROSE_ASSERT(outer_apply->get_transformations()[1]->get_separator() ==
              SgOmpClause::e_omp_clause_separator_space);
  ROSE_ASSERT(nested_apply->get_transformations().size() == 1);
  ROSE_ASSERT(nested_apply->get_transformations()[0]->get_kind() ==
              SgOmpClause::e_omp_apply_transform_reverse);
  ROSE_ASSERT(nested_apply->get_transformations()[0]->get_separator() ==
              SgOmpClause::e_omp_clause_separator_none);
  SgOmpApplyTransformation *partial =
      outer_apply->get_transformations().front();
  SgIntVal *partial_argument = isSgIntVal(partial->get_argument());
  ROSE_ASSERT(partial_argument != nullptr &&
              partial_argument->get_value() == 2 &&
              partial_argument->get_parent() == partial);
  exerciseOwnedExpressionReplacement(partial, partial->get_argument());

  const Rose_STL_Container<SgNode *> sizes_nodes =
      NodeQuery::querySubTree(project, V_SgOmpSizesClause);
  ROSE_ASSERT(sizes_nodes.size() == 1);
  SgOmpSizesClause *sizes = isSgOmpSizesClause(sizes_nodes.front());
  ROSE_ASSERT(sizes != nullptr);
  SgExprListExp *sizes_list = isSgExprListExp(sizes->get_expression());
  ROSE_ASSERT(sizes_list != nullptr && sizes_list->get_parent() == sizes &&
              sizes_list->get_expressions().size() == 1);
  SgIntVal *size = isSgIntVal(sizes_list->get_expressions().front());
  ROSE_ASSERT(size != nullptr && size->get_value() == 4 &&
              size->get_parent() == sizes_list);

  const Rose_STL_Container<SgNode *> allocate_nodes =
      NodeQuery::querySubTree(project, V_SgOmpAllocateClause);
  ROSE_ASSERT(allocate_nodes.size() == 2);
  SgOmpAllocateClause *legacy_allocate = nullptr;
  SgOmpAllocateClause *modifier_allocate = nullptr;
  for (SgNode *node : allocate_nodes) {
    SgOmpAllocateClause *allocate = isSgOmpAllocateClause(node);
    ROSE_ASSERT(allocate != nullptr &&
                allocate->get_modifier() ==
                    SgOmpClause::e_omp_allocate_user_defined_modifier);
    if (allocate->get_uses_allocator_modifier_syntax()) {
      ROSE_ASSERT(modifier_allocate == nullptr);
      modifier_allocate = allocate;
    } else {
      ROSE_ASSERT(legacy_allocate == nullptr);
      legacy_allocate = allocate;
    }
  }
  ROSE_ASSERT(legacy_allocate != nullptr && modifier_allocate != nullptr);
  requireVariableReference(legacy_allocate->get_user_defined_modifier(),
                           legacy_allocate, "allocator");
  ROSE_ASSERT(legacy_allocate->get_alignment() == nullptr);
  requireVariableReference(modifier_allocate->get_user_defined_modifier(),
                           modifier_allocate, "allocator");
  SgIntVal *alignment = isSgIntVal(modifier_allocate->get_alignment());
  ROSE_ASSERT(alignment != nullptr && alignment->get_value() == 64 &&
              alignment->get_parent() == modifier_allocate);
  SgOmpAllocateClause *allocate_copy =
      isSgOmpAllocateClause(SageInterface::deepCopy(modifier_allocate));
  ROSE_ASSERT(allocate_copy != nullptr && allocate_copy != modifier_allocate &&
              allocate_copy->get_user_defined_modifier() !=
                  modifier_allocate->get_user_defined_modifier() &&
              allocate_copy->get_user_defined_modifier()->get_parent() ==
                  allocate_copy &&
              allocate_copy->get_alignment() != alignment &&
              allocate_copy->get_alignment()->get_parent() == allocate_copy);

  const Rose_STL_Container<SgNode *> nowait_nodes =
      NodeQuery::querySubTree(project, V_SgOmpNowaitClause);
  ROSE_ASSERT(nowait_nodes.size() == 1);
  SgOmpNowaitClause *nowait = isSgOmpNowaitClause(nowait_nodes.front());
  ROSE_ASSERT(nowait != nullptr);
  requireVariableReference(nowait->get_expression(), nowait, "is_deferred");

  const Rose_STL_Container<SgNode *> map_nodes =
      NodeQuery::querySubTree(project, V_SgOmpMapClause);
  ROSE_ASSERT(map_nodes.size() == 2);
  SgOmpMapClause *ordered_sections = nullptr;
  for (SgNode *node : map_nodes) {
    SgOmpMapClause *map_clause = isSgOmpMapClause(node);
    ROSE_ASSERT(map_clause != nullptr &&
                map_clause->get_variables() != nullptr &&
                map_clause->get_variables()->get_parent() == map_clause);
    if (map_clause->get_variables()->get_expressions().size() == 2) {
      ROSE_ASSERT(ordered_sections == nullptr);
      ordered_sections = map_clause;
    }
  }
  ROSE_ASSERT(ordered_sections != nullptr);
  SgVariableSymbol *ordered_symbol = nullptr;
  const int expected_lowers[] = {0, 4};
  for (size_t index = 0; index < 2; ++index) {
    SgOmpMapItem *item = isSgOmpMapItem(
        ordered_sections->get_variables()->get_expressions()[index]);
    ROSE_ASSERT(item != nullptr &&
                item->get_parent() == ordered_sections->get_variables() &&
                item->get_expression() != nullptr &&
                item->get_expression()->get_parent() == item &&
                item->get_policies().empty());
    SgPntrArrRefExp *section = isSgPntrArrRefExp(item->get_expression());
    ROSE_ASSERT(section != nullptr);
    SgVarRefExp *base = isSgVarRefExp(section->get_lhs_operand());
    SgSubscriptExpression *subscript =
        isSgSubscriptExpression(section->get_rhs_operand());
    ROSE_ASSERT(base != nullptr && base->get_symbol() != nullptr &&
                subscript != nullptr && subscript->get_parent() == section);
    SgVariableSymbol *symbol = isSgVariableSymbol(base->get_symbol());
    ROSE_ASSERT(symbol != nullptr &&
                symbol->get_name().getString() == "mapped_values");
    if (ordered_symbol == nullptr) {
      ordered_symbol = symbol;
    }
    ROSE_ASSERT(symbol == ordered_symbol);
    SgIntVal *lower = isSgIntVal(subscript->get_lowerBound());
    SgIntVal *length = isSgIntVal(subscript->get_upperBound());
    ROSE_ASSERT(lower != nullptr &&
                lower->get_value() == expected_lowers[index] &&
                lower->get_parent() == subscript && length != nullptr &&
                length->get_value() == 4 && length->get_parent() == subscript &&
                isSgNullExpression(subscript->get_stride()) != nullptr &&
                subscript->get_stride()->get_parent() == subscript);
  }
  SgOmpMapClause *ordered_copy =
      isSgOmpMapClause(SageInterface::deepCopy(ordered_sections));
  ROSE_ASSERT(ordered_copy != nullptr && ordered_copy != ordered_sections &&
              ordered_copy->get_variables() !=
                  ordered_sections->get_variables() &&
              ordered_copy->get_variables()->get_parent() == ordered_copy &&
              ordered_copy->get_variables()->get_expressions().size() == 2);
  for (SgExpression *expression :
       ordered_copy->get_variables()->get_expressions()) {
    SgOmpMapItem *item = isSgOmpMapItem(expression);
    ROSE_ASSERT(item != nullptr &&
                item->get_parent() == ordered_copy->get_variables() &&
                item->get_expression() != nullptr &&
                item->get_expression()->get_parent() == item);
  }

  const Rose_STL_Container<SgNode *> firstprivate_nodes =
      NodeQuery::querySubTree(project, V_SgOmpFirstprivateClause);
  ROSE_ASSERT(firstprivate_nodes.size() == 1);
  SgOmpFirstprivateClause *firstprivate =
      isSgOmpFirstprivateClause(firstprivate_nodes.front());
  ROSE_ASSERT(firstprivate != nullptr && firstprivate->get_saved());
  ROSE_ASSERT(firstprivate->get_directive_name_modifier() ==
              SgOmpClause::e_omp_directive_name_modifier_unspecified);
  ROSE_ASSERT(firstprivate->get_variables() != nullptr &&
              firstprivate->get_variables()->get_parent() == firstprivate &&
              firstprivate->get_variables()->get_expressions().size() == 1);
  requireVariableReference(firstprivate->get_variables()->get_expressions()[0],
                           firstprivate->get_variables(), "allocated_value");

  const Rose_STL_Container<SgNode *> adjust_nodes =
      NodeQuery::querySubTree(project, V_SgOmpAdjustArgsClause);
  ROSE_ASSERT(adjust_nodes.size() == 1);
  SgOmpAdjustArgsClause *adjust = isSgOmpAdjustArgsClause(adjust_nodes.front());
  ROSE_ASSERT(adjust != nullptr &&
              adjust->get_modifier() ==
                  SgOmpClause::e_omp_adjust_args_modifier_need_device_addr &&
              adjust->get_arguments() != nullptr &&
              adjust->get_arguments()->get_parent() == adjust &&
              adjust->get_arguments()->get_expressions().size() == 2);
  requireVariableReference(adjust->get_arguments()->get_expressions()[0],
                           adjust->get_arguments(), "first");
  requireVariableReference(adjust->get_arguments()->get_expressions()[1],
                           adjust->get_arguments(), "second");

  const Rose_STL_Container<SgNode *> append_nodes =
      NodeQuery::querySubTree(project, V_SgOmpAppendArgsClause);
  ROSE_ASSERT(append_nodes.size() == 1);
  SgOmpAppendArgsClause *append = isSgOmpAppendArgsClause(append_nodes.front());
  ROSE_ASSERT(append != nullptr &&
              append->get_interop_operations().size() == 2);
  for (SgOmpAppendArgsOperation *operation : append->get_interop_operations()) {
    ROSE_ASSERT(operation != nullptr && operation->get_parent() == append &&
                operation->get_modifier_list() != nullptr &&
                operation->get_modifier_list()->get_parent() == operation);
  }
  const SgOmpInitModifierPtrList &first_operation_modifiers =
      append->get_interop_operations()[0]->get_modifier_list()->get_modifiers();
  ROSE_ASSERT(first_operation_modifiers.size() == 2 &&
              first_operation_modifiers[0]->get_kind() ==
                  SgOmpClause::e_omp_init_modifier_target &&
              first_operation_modifiers[1]->get_kind() ==
                  SgOmpClause::e_omp_init_modifier_targetsync);
  const SgOmpInitModifierPtrList &second_operation_modifiers =
      append->get_interop_operations()[1]->get_modifier_list()->get_modifiers();
  ROSE_ASSERT(second_operation_modifiers.size() == 2 &&
              second_operation_modifiers[0]->get_kind() ==
                  SgOmpClause::e_omp_init_modifier_prefer_type &&
              second_operation_modifiers[1]->get_kind() ==
                  SgOmpClause::e_omp_init_modifier_target);
  SgOmpSourceExpression *prefer_type =
      isSgOmpSourceExpression(second_operation_modifiers[0]->get_expression());
  ROSE_ASSERT(prefer_type != nullptr &&
              prefer_type->get_spelling() == "rex_vendor_type" &&
              prefer_type->get_parent() == second_operation_modifiers[0]);

  SgOmpAppendArgsClause *append_copy =
      isSgOmpAppendArgsClause(SageInterface::deepCopy(append));
  ROSE_ASSERT(append_copy != nullptr && append_copy != append &&
              append_copy->get_interop_operations().size() == 2);
  for (size_t index = 0; index < 2; ++index) {
    SgOmpAppendArgsOperation *copy_operation =
        append_copy->get_interop_operations()[index];
    SgOmpAppendArgsOperation *original_operation =
        append->get_interop_operations()[index];
    ROSE_ASSERT(copy_operation != original_operation &&
                copy_operation->get_parent() == append_copy &&
                copy_operation->get_modifier_list() !=
                    original_operation->get_modifier_list() &&
                copy_operation->get_modifier_list()->get_parent() ==
                    copy_operation);
  }

  const Rose_STL_Container<SgNode *> induction_nodes =
      NodeQuery::querySubTree(project, V_SgOmpInductionClause);
  ROSE_ASSERT(induction_nodes.size() == 1);
  SgOmpInductionClause *induction =
      isSgOmpInductionClause(induction_nodes.front());
  ROSE_ASSERT(induction != nullptr && induction->get_items().size() == 3);
  ROSE_ASSERT(induction->get_items()[0]->get_kind() ==
              SgOmpClause::e_omp_induction_item_binding);
  ROSE_ASSERT(induction->get_items()[0]->get_label() == "*");
  ROSE_ASSERT(induction->get_items()[1]->get_kind() ==
              SgOmpClause::e_omp_induction_item_step);
  ROSE_ASSERT(induction->get_items()[2]->get_kind() ==
              SgOmpClause::e_omp_induction_item_expression);
  requireVariableReference(induction->get_items()[0]->get_expression(),
                           induction->get_items()[0], "induction_value");
  requireVariableReference(induction->get_items()[1]->get_expression(),
                           induction->get_items()[1], "step_value");
  requireVariableReference(induction->get_items()[2]->get_expression(),
                           induction->get_items()[2], "induction_value");
  exerciseOwnedExpressionReplacement(
      induction->get_items()[1], induction->get_items()[1]->get_expression());

  const Rose_STL_Container<SgNode *> init_nodes =
      NodeQuery::querySubTree(project, V_SgOmpInitClause);
  ROSE_ASSERT(init_nodes.size() == 1);
  SgOmpInitClause *init = isSgOmpInitClause(init_nodes.front());
  ROSE_ASSERT(init != nullptr);
  SgOmpInitModifierList *modifier_list = init->get_modifier_list();
  ROSE_ASSERT(modifier_list != nullptr && modifier_list->get_parent() == init);
  ROSE_ASSERT(modifier_list->get_modifiers().size() == 2);
  ROSE_ASSERT(modifier_list->get_modifiers()[0]->get_kind() ==
              SgOmpClause::e_omp_init_modifier_interop);
  ROSE_ASSERT(modifier_list->get_modifiers()[1]->get_kind() ==
              SgOmpClause::e_omp_init_modifier_targetsync);
  requireVariableReference(init->get_operand(), init, "object");

  SgOmpInitClause *init_copy = isSgOmpInitClause(SageInterface::deepCopy(init));
  ROSE_ASSERT(init_copy != nullptr && init_copy != init);
  ROSE_ASSERT(init_copy->get_modifier_list() != nullptr &&
              init_copy->get_modifier_list() != modifier_list &&
              init_copy->get_modifier_list()->get_parent() == init_copy);
  ROSE_ASSERT(init_copy->get_operand() != nullptr &&
              init_copy->get_operand() != init->get_operand() &&
              init_copy->get_operand()->get_parent() == init_copy);
  ROSE_ASSERT(init_copy->get_modifier_list()->get_modifiers().size() == 2);
  for (size_t index = 0; index < 2; ++index) {
    ROSE_ASSERT(init_copy->get_modifier_list()->get_modifiers()[index] !=
                modifier_list->get_modifiers()[index]);
    ROSE_ASSERT(
        init_copy->get_modifier_list()->get_modifiers()[index]->get_parent() ==
        init_copy->get_modifier_list());
  }

  SgIntVal *init_expression = SageBuilder::buildIntVal(7);
  SgOmpInitModifier *expression_modifier = new SgOmpInitModifier(
      SgOmpClause::e_omp_init_modifier_prefer_type, init_expression);
  init_expression->set_parent(expression_modifier);
  exerciseOwnedExpressionReplacement(expression_modifier, init_expression);

  SgOmpInitModifier *original_modifier = modifier_list->get_modifiers().back();
  SgOmpInitModifier *replacement_modifier =
      new SgOmpInitModifier(SgOmpClause::e_omp_init_modifier_target, nullptr);
  ROSE_ASSERT(modifier_list->replace_expression(original_modifier,
                                                replacement_modifier) == 1);
  ROSE_ASSERT(replacement_modifier->get_parent() == modifier_list &&
              original_modifier->get_parent() == nullptr);
  ROSE_ASSERT(modifier_list->replace_expression(replacement_modifier,
                                                original_modifier) == 1);
  ROSE_ASSERT(original_modifier->get_parent() == modifier_list &&
              replacement_modifier->get_parent() == nullptr);

  if (mode == "--item-type") {
    outer_apply->get_transformations()[0]->get_type();
  } else if (mode == "--bad-first-separator") {
    outer_apply->get_transformations()[0]->set_separator(
        SgOmpClause::e_omp_clause_separator_comma);
  } else if (mode == "--bad-later-separator") {
    outer_apply->get_transformations()[1]->set_separator(
        SgOmpClause::e_omp_clause_separator_none);
  } else if (mode == "--replacement-null") {
    induction->get_items()[1]->replace_expression(
        induction->get_items()[1]->get_expression(), nullptr);
  } else if (mode == "--replacement-owned") {
    induction->get_items()[1]->replace_expression(
        induction->get_items()[1]->get_expression(),
        outer_apply->get_transformations()[0]->get_argument());
  } else if (mode == "--replacement-wrong-apply-payload") {
    SgOmpApplyTransformation *partial = outer_apply->get_transformations()[0];
    partial->set_kind(SgOmpClause::e_omp_apply_transform_reverse);
    partial->replace_expression(partial->get_argument(),
                                SageBuilder::buildIntVal(5));
  } else if (mode == "--replacement-wrong-init-payload") {
    SgOmpInitModifier *named_modifier = modifier_list->get_modifiers().front();
    SgIntVal *invalid_expression = SageBuilder::buildIntVal(6);
    named_modifier->set_expression(invalid_expression);
    invalid_expression->set_parent(named_modifier);
    named_modifier->replace_expression(invalid_expression,
                                       SageBuilder::buildIntVal(7));
  } else if (mode == "--replacement-list-wrong-type") {
    modifier_list->replace_expression(modifier_list->get_modifiers().back(),
                                      SageBuilder::buildIntVal(8));
  } else if (mode == "--missing-init-modifier-list") {
    init->set_modifier_list(nullptr);
  } else if (mode == "--wrong-sizes-type" || mode == "--empty-sizes" ||
             mode == "--null-sizes-element") {
    if (mode == "--wrong-sizes-type") {
      SgIntVal *replacement = SageBuilder::buildIntVal(4);
      sizes->set_expression(replacement);
      replacement->set_parent(sizes);
    } else {
      SgExprListExp *list = isSgExprListExp(sizes->get_expression());
      ROSE_ASSERT(list != nullptr && !list->get_expressions().empty());
      if (mode == "--empty-sizes") {
        list->get_expressions().clear();
      } else {
        list->get_expressions()[0] = nullptr;
      }
    }
  }

  return backend(project);
}
