#ifndef REX_OPENMP_MODIFIER_VALIDATION_H
#define REX_OPENMP_MODIFIER_VALIDATION_H

#include "Cxx_Grammar.h"

#include <algorithm>
#include <cstddef>
#include <string>

namespace Rose {
namespace OpenMP {
namespace Detail {

enum class InitModifierContext { InteropInit, DepobjInit, AppendArgs };

inline bool modifierValidationFailure(std::string *detail,
                                      const std::string &message) {
  if (detail != nullptr) {
    *detail = message;
  }
  return false;
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

inline bool validateInitModifierPayload(SgOmpInitModifier *modifier,
                                        SgNode *expected_parent,
                                        std::string *detail) {
  if (modifier == nullptr) {
    return modifierValidationFailure(detail, "modifier is null");
  }
  if (modifier->get_parent() != expected_parent) {
    return modifierValidationFailure(detail,
                                     "modifier has the wrong exact owner");
  }

  const bool expression_modifier =
      modifier->get_kind() == SgOmpClause::e_omp_init_modifier_prefer_type ||
      isDepinfoModifier(modifier->get_kind());
  const bool plain_modifier =
      modifier->get_kind() == SgOmpClause::e_omp_init_modifier_depobj ||
      modifier->get_kind() == SgOmpClause::e_omp_init_modifier_interop ||
      modifier->get_kind() == SgOmpClause::e_omp_init_modifier_target ||
      modifier->get_kind() == SgOmpClause::e_omp_init_modifier_targetsync;
  if (!expression_modifier && !plain_modifier) {
    return modifierValidationFailure(detail, "modifier kind is invalid");
  }
  if (expression_modifier &&
      (modifier->get_expression() == nullptr ||
       modifier->get_expression()->get_parent() != modifier)) {
    return modifierValidationFailure(
        detail, "expression modifier has no exactly owned payload");
  }
  if (plain_modifier && modifier->get_expression() != nullptr) {
    return modifierValidationFailure(detail, "plain modifier has a payload");
  }
  if (modifier->get_kind() == SgOmpClause::e_omp_init_modifier_prefer_type) {
    SgOmpSourceExpression *source =
        isSgOmpSourceExpression(modifier->get_expression());
    if (source == nullptr || source->get_spelling().empty()) {
      return modifierValidationFailure(
          detail, "prefer_type has no exact nonempty source payload");
    }
  }
  if (isDepinfoModifier(modifier->get_kind()) &&
      !modifier->get_expression()->has_semantic_value_type()) {
    return modifierValidationFailure(
        detail, "depinfo modifier has no semantic locator payload");
  }
  return true;
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
