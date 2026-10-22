#ifndef REX_BUILTIN_OPERAND_VALIDATION_H
#define REX_BUILTIN_OPERAND_VALIDATION_H

#include "Cxx_Grammar.h"
#include <algorithm>
#include <unordered_set>

namespace Rose {
namespace BuiltinOperands {

inline bool isTypeOperand(SgExpression *argument) {
  auto *occurrence = isSgTypeExpression(argument);
  SgType *type = occurrence ? occurrence->get_represented_type() : nullptr;
  return type != nullptr && isSgTypeUnknown(type) == nullptr &&
         isSgTypeDefault(type) == nullptr;
}

inline bool isValueOperand(SgExpression *argument) {
  return argument != nullptr && isSgTypeExpression(argument) == nullptr;
}

inline bool
haveValidRoles(SgTypeTraitBuiltinOperator::builtin_operator_kind_enum kind,
               const SgName &name, const SgExpressionPtrList &arguments) {
  switch (kind) {
  case SgTypeTraitBuiltinOperator::e_type_trait_builtin:
    return !name.is_null() && !arguments.empty() &&
           std::all_of(arguments.begin(), arguments.end(),
                       [](SgExpression *arg) {
                         return isTypeOperand(arg) || isValueOperand(arg);
                       });
  case SgTypeTraitBuiltinOperator::e_offsetof_builtin:
    return name.getString() == "__builtin_offsetof" && arguments.size() == 2 &&
           isTypeOperand(arguments[0]) && isValueOperand(arguments[1]);
  case SgTypeTraitBuiltinOperator::e_convert_vector_builtin:
    return name.getString() == "__builtin_convertvector" &&
           arguments.size() == 2 && isValueOperand(arguments[0]) &&
           isTypeOperand(arguments[1]);
  }
  return false;
}

inline bool haveExclusiveOwnership(const SgExpressionPtrList &arguments,
                                   SgNode *owner) {
  std::unordered_set<SgExpression *> seen;
  for (SgExpression *argument : arguments) {
    if (argument == nullptr || argument->get_parent() != owner ||
        !seen.insert(argument).second)
      return false;
  }
  return true;
}

} // namespace BuiltinOperands
} // namespace Rose

#endif
