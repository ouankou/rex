#ifndef REX_OPENMP_MODIFIER_VALIDATION_H
#define REX_OPENMP_MODIFIER_VALIDATION_H

#include "Cxx_Grammar.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace Rose {
namespace OpenMP {
namespace Detail {

enum class InitModifierContext { InteropInit, DepobjInit, AppendArgs };

inline bool
validInductionItemFields(SgOmpClause::omp_induction_item_kind_enum kind,
                         const std::string &label) {
  switch (kind) {
  case SgOmpClause::e_omp_induction_item_binding:
    return !label.empty();
  case SgOmpClause::e_omp_induction_item_step:
  case SgOmpClause::e_omp_induction_item_expression:
    return label.empty();
  default:
    return false;
  }
}

inline bool
validApplyTransformationFields(SgOmpClause::omp_apply_transform_kind_enum kind,
                               const std::string &name, bool has_argument,
                               bool has_nested) {
  switch (kind) {
  case SgOmpClause::e_omp_apply_transform_unroll:
  case SgOmpClause::e_omp_apply_transform_unroll_full:
  case SgOmpClause::e_omp_apply_transform_reverse:
  case SgOmpClause::e_omp_apply_transform_interchange:
  case SgOmpClause::e_omp_apply_transform_nothing:
    return name.empty() && !has_argument && !has_nested;
  case SgOmpClause::e_omp_apply_transform_unroll_partial:
  case SgOmpClause::e_omp_apply_transform_tile_sizes:
    return name.empty() && has_argument && !has_nested;
  case SgOmpClause::e_omp_apply_transform_nested_apply:
    return name.empty() && !has_argument && has_nested;
  case SgOmpClause::e_omp_apply_transform_named:
    return !name.empty() && !has_argument && !has_nested;
  default:
    return false;
  }
}

inline bool
validMapDistDataPolicyFields(SgOmpClause::omp_map_dist_data_enum kind,
                             bool has_expression) {
  switch (kind) {
  case SgOmpClause::e_omp_map_dist_data_duplicate:
    return !has_expression;
  case SgOmpClause::e_omp_map_dist_data_block:
  case SgOmpClause::e_omp_map_dist_data_cyclic:
    return true;
  default:
    return false;
  }
}

inline bool
validMapDistDataPolicyPayload(const SgOmpMapDistDataPolicy *policy) {
  if (policy == nullptr)
    return false;
  auto *expression = policy->get_expression();
  return validMapDistDataPolicyFields(policy->get_policy(),
                                      expression != nullptr) &&
         (expression == nullptr || expression->get_parent() == policy);
}

inline bool modifierValidationFailure(std::string *detail,
                                      const std::string &message) {
  if (detail != nullptr) {
    *detail = message;
  }
  return false;
}

struct MapModifierFields {
  bool mapper = false;
  bool iterator = false;
};

inline bool validateMapModifiers(
    const std::array<SgOmpClause::omp_map_modifier_enum, 3> &modifiers,
    MapModifierFields &fields, std::string *detail) {
  fields = {};
  bool saw_unspecified = false;
  for (size_t i = 0; i != modifiers.size(); ++i) {
    const auto modifier = modifiers[i];
    switch (modifier) {
    case SgOmpClause::e_omp_map_modifier_unspecified:
      saw_unspecified = true;
      continue;
    case SgOmpClause::e_omp_map_modifier_always:
    case SgOmpClause::e_omp_map_modifier_close:
    case SgOmpClause::e_omp_map_modifier_present:
    case SgOmpClause::e_omp_map_modifier_self:
    case SgOmpClause::e_omp_map_modifier_mapper:
    case SgOmpClause::e_omp_map_modifier_iterator:
      break;
    default:
      return modifierValidationFailure(detail,
                                       "map clause has an invalid modifier");
    }
    if (saw_unspecified || std::find(modifiers.begin(), modifiers.begin() + i,
                                     modifier) != modifiers.begin() + i)
      return modifierValidationFailure(
          detail, "map modifiers are not a unique contiguous sequence");
    if (modifier == SgOmpClause::e_omp_map_modifier_mapper)
      fields.mapper = true;
    if (modifier == SgOmpClause::e_omp_map_modifier_iterator)
      fields.iterator = true;
  }
  return true;
}

inline bool isDepinfoModifier(SgOmpClause::omp_init_modifier_kind_enum kind) {
  switch (kind) {
  case SgOmpClause::e_omp_init_modifier_depinfo_in:
  case SgOmpClause::e_omp_init_modifier_depinfo_out:
  case SgOmpClause::e_omp_init_modifier_depinfo_inout:
  case SgOmpClause::e_omp_init_modifier_depinfo_inoutset:
  case SgOmpClause::e_omp_init_modifier_depinfo_mutexinoutset:
    return true;
  default:
    return false;
  }
}

inline bool
validateInitModifierExpression(SgOmpClause::omp_init_modifier_kind_enum kind,
                               SgExpression *expression, std::string *detail) {
  const bool expression_modifier =
      kind == SgOmpClause::e_omp_init_modifier_prefer_type ||
      isDepinfoModifier(kind);
  const bool plain_modifier =
      kind == SgOmpClause::e_omp_init_modifier_depobj ||
      kind == SgOmpClause::e_omp_init_modifier_interop ||
      kind == SgOmpClause::e_omp_init_modifier_target ||
      kind == SgOmpClause::e_omp_init_modifier_targetsync;
  if (!expression_modifier && !plain_modifier) {
    return modifierValidationFailure(detail, "modifier kind is invalid");
  }
  if (expression_modifier && expression == nullptr) {
    return modifierValidationFailure(
        detail, "expression modifier has no exactly owned payload");
  }
  if (plain_modifier && expression != nullptr) {
    return modifierValidationFailure(detail, "plain modifier has a payload");
  }
  if (kind == SgOmpClause::e_omp_init_modifier_prefer_type) {
    SgOmpSourceExpression *source = isSgOmpSourceExpression(expression);
    if (source == nullptr || source->get_spelling().empty()) {
      return modifierValidationFailure(
          detail, "prefer_type has no exact nonempty source payload");
    }
  }
  if (isDepinfoModifier(kind) && !expression->has_semantic_value_type()) {
    return modifierValidationFailure(
        detail, "depinfo modifier has no semantic locator payload");
  }
  return true;
}

inline bool validateInitModifierPayload(SgOmpInitModifier *modifier,
                                        SgNode *expected_parent,
                                        std::string *detail) {
  if (modifier == nullptr)
    return modifierValidationFailure(detail, "modifier is null");
  if (modifier->get_parent() != expected_parent)
    return modifierValidationFailure(detail,
                                     "modifier has the wrong exact owner");
  auto *expression = modifier->get_expression();
  const auto kind = modifier->get_kind();
  if (expression != nullptr &&
      (kind == SgOmpClause::e_omp_init_modifier_prefer_type ||
       isDepinfoModifier(kind)) &&
      expression->get_parent() != modifier)
    return modifierValidationFailure(
        detail, "expression modifier has no exactly owned payload");
  return validateInitModifierExpression(kind, expression, detail);
}

inline bool
validateInitModifierComposition(const SgOmpInitModifierPtrList &modifiers,
                                InitModifierContext context,
                                std::string *detail) {
  size_t prefer_type_count = 0;
  size_t depinfo_count = 0;
  size_t depobj_name_count = 0;
  size_t interop_name_count = 0;
  size_t target_count = 0;
  size_t targetsync_count = 0;
  for (SgOmpInitModifier *modifier : modifiers) {
    if (modifier == nullptr) {
      return modifierValidationFailure(detail, "modifier is null");
    }
    switch (modifier->get_kind()) {
    case SgOmpClause::e_omp_init_modifier_prefer_type:
      ++prefer_type_count;
      break;
    case SgOmpClause::e_omp_init_modifier_depobj:
      ++depobj_name_count;
      break;
    case SgOmpClause::e_omp_init_modifier_interop:
      ++interop_name_count;
      break;
    case SgOmpClause::e_omp_init_modifier_target:
      ++target_count;
      break;
    case SgOmpClause::e_omp_init_modifier_targetsync:
      ++targetsync_count;
      break;
    case SgOmpClause::e_omp_init_modifier_depinfo_in:
    case SgOmpClause::e_omp_init_modifier_depinfo_out:
    case SgOmpClause::e_omp_init_modifier_depinfo_inout:
    case SgOmpClause::e_omp_init_modifier_depinfo_inoutset:
    case SgOmpClause::e_omp_init_modifier_depinfo_mutexinoutset:
      ++depinfo_count;
      break;
    default:
      return modifierValidationFailure(detail, "modifier kind is invalid");
    }
  }

  if (prefer_type_count > 1 || depinfo_count > 1 ||
      depobj_name_count + interop_name_count > 1 || target_count > 1 ||
      targetsync_count > 1) {
    return modifierValidationFailure(
        detail, "modifier names and interop types are not unique");
  }

  const bool interop_context = context != InitModifierContext::DepobjInit;
  const size_t interop_type_count = target_count + targetsync_count;
  if (interop_context && interop_type_count == 0) {
    return modifierValidationFailure(detail, "interop type is missing");
  }
  if (!interop_context && interop_type_count != 0) {
    return modifierValidationFailure(
        detail, "interop type is invalid for depobj initialization");
  }
  if (context == InitModifierContext::DepobjInit && depinfo_count == 0) {
    return modifierValidationFailure(detail, "depinfo modifier is missing");
  }
  if (context != InitModifierContext::DepobjInit && depinfo_count != 0) {
    return modifierValidationFailure(
        detail, "depinfo modifier is invalid in an interop context");
  }
  if (context == InitModifierContext::DepobjInit && prefer_type_count != 0) {
    return modifierValidationFailure(
        detail, "prefer_type is invalid for depobj initialization");
  }
  if (context == InitModifierContext::DepobjInit && interop_name_count != 0) {
    return modifierValidationFailure(
        detail, "interop directive-name modifier is invalid for depobj");
  }
  if (context != InitModifierContext::DepobjInit && depobj_name_count != 0) {
    return modifierValidationFailure(
        detail, "depobj directive-name modifier is invalid for interop");
  }
  return true;
}

inline bool validateInitModifierList(SgOmpInitModifierList *modifier_list,
                                     SgNode *expected_parent,
                                     InitModifierContext context,
                                     std::string *detail) {
  if (modifier_list == nullptr) {
    return modifierValidationFailure(detail, "modifier list is null");
  }
  if (modifier_list->get_parent() != expected_parent) {
    return modifierValidationFailure(detail,
                                     "modifier list has the wrong exact owner");
  }
  const SgOmpInitModifierPtrList &modifiers = modifier_list->get_modifiers();
  for (SgOmpInitModifier *modifier : modifiers) {
    if (std::count(modifiers.begin(), modifiers.end(), modifier) != 1) {
      return modifierValidationFailure(detail,
                                       "modifier is aliased in its list");
    }
    if (!validateInitModifierPayload(modifier, modifier_list, detail)) {
      return false;
    }
  }
  return validateInitModifierComposition(modifiers, context, detail);
}

inline bool initModifierContextForClause(SgOmpInitClause *clause,
                                         InitModifierContext *context,
                                         std::string *detail) {
  if (clause == nullptr || context == nullptr) {
    return modifierValidationFailure(detail,
                                     "init clause or context result is null");
  }
  SgOmpClauseList *clause_list = isSgOmpClauseList(clause->get_parent());
  SgNode *directive =
      clause_list != nullptr ? clause_list->get_parent() : nullptr;
  const bool on_interop = isSgOmpInteropStatement(directive) != nullptr;
  const bool on_depobj = isSgOmpDepobjStatement(directive) != nullptr;
  if (on_interop == on_depobj) {
    return modifierValidationFailure(
        detail, "init clause has no unique interop or depobj context");
  }
  *context = on_interop ? InitModifierContext::InteropInit
                        : InitModifierContext::DepobjInit;
  return true;
}

inline bool validateInitClause(SgOmpInitClause *clause, std::string *detail) {
  InitModifierContext context = InitModifierContext::InteropInit;
  if (!initModifierContextForClause(clause, &context, detail)) {
    return false;
  }
  if (clause->get_operand() == nullptr ||
      clause->get_operand()->get_parent() != clause) {
    return modifierValidationFailure(detail, "operand is not exactly owned");
  }
  if (!clause->get_operand()->has_semantic_value_type()) {
    return modifierValidationFailure(
        detail, "operand has no semantic value-expression role");
  }
  return validateInitModifierList(clause->get_modifier_list(), clause, context,
                                  detail);
}

inline bool validateAdjustArgsClause(SgOmpAdjustArgsClause *clause,
                                     std::string *detail) {
  if (clause == nullptr) {
    return modifierValidationFailure(detail, "adjust_args clause is null");
  }
  switch (clause->get_modifier()) {
  case SgOmpClause::e_omp_adjust_args_modifier_need_device_addr:
  case SgOmpClause::e_omp_adjust_args_modifier_need_device_ptr:
  case SgOmpClause::e_omp_adjust_args_modifier_nothing:
    break;
  default:
    return modifierValidationFailure(detail, "adjust_args modifier is invalid");
  }
  SgExprListExp *arguments = clause->get_arguments();
  if (arguments == nullptr || arguments->get_parent() != clause ||
      arguments->get_expressions().empty()) {
    return modifierValidationFailure(
        detail, "adjust_args argument list is missing, empty, or misowned");
  }
  const SgExpressionPtrList &expressions = arguments->get_expressions();
  for (SgExpression *expression : expressions) {
    if (expression == nullptr || expression->get_parent() != arguments ||
        std::count(expressions.begin(), expressions.end(), expression) != 1) {
      return modifierValidationFailure(
          detail, "adjust_args argument is null, aliased, or misowned");
    }
    if (!expression->has_semantic_value_type()) {
      return modifierValidationFailure(
          detail, "adjust_args argument has no semantic value-expression "
                  "role");
    }
  }
  return true;
}

inline bool validateAppendArgsClause(SgOmpAppendArgsClause *clause,
                                     std::string *detail) {
  if (clause == nullptr) {
    return modifierValidationFailure(detail, "append_args clause is null");
  }
  const SgOmpAppendArgsOperationPtrList &operations =
      clause->get_interop_operations();
  if (operations.empty()) {
    return modifierValidationFailure(detail,
                                     "append_args operation list is empty");
  }
  for (SgOmpAppendArgsOperation *operation : operations) {
    if (operation == nullptr || operation->get_parent() != clause ||
        std::count(operations.begin(), operations.end(), operation) != 1) {
      return modifierValidationFailure(
          detail, "append_args operation is null, aliased, or misowned");
    }
    if (!validateInitModifierList(operation->get_modifier_list(), operation,
                                  InitModifierContext::AppendArgs, detail)) {
      return false;
    }
  }
  return true;
}

} // namespace Detail
} // namespace OpenMP
} // namespace Rose

#endif
