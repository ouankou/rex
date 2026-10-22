#include "sage3basic.h"

namespace {
SgNode *referenceOrigin(SgNode *node) {
  ROSE_ASSERT(node != nullptr);
  while (true) {
    if (auto *initializer = isSgAssignInitializer(node)) {
      node = initializer->get_operand();
      continue;
    }
    auto *cast = isSgCastExp(node);
    if (cast == nullptr)
      return node;
    cast->validate_semantic_conversion();
    if (cast->get_value_category() == SgCastExp::e_value_category_prvalue)
      return node;
    switch (cast->get_semantic_conversion_kind()) {
    case SgCastExp::e_semantic_conversion_NoOp:
    case SgCastExp::e_semantic_conversion_LValueBitCast:
    case SgCastExp::e_semantic_conversion_BaseToDerived:
    case SgCastExp::e_semantic_conversion_DerivedToBase:
    case SgCastExp::e_semantic_conversion_UncheckedDerivedToBase:
      node = cast->get_operand();
      break;
    default:
      return node;
    }
  }
}
} // namespace

bool containsNonConst(SgNode *n, SgExpression *lhs) {
  n = referenceOrigin(n);
  lhs = isSgExpression(referenceOrigin(lhs));
  ROSE_ASSERT(n != nullptr && lhs != nullptr);
  if (n == lhs)
    return true;
  if (isSgVarRefExp(lhs) && isSgVarRefExp(n)) {
    if (isSgVarRefExp(lhs)->get_symbol()->get_declaration() ==
        isSgVarRefExp(n)->get_symbol()->get_declaration())
      return true;
  }
  return isSgDotExp(n) &&
         containsNonConst(isSgDotExp(n)->get_lhs_operand(), lhs);
}

bool containsNonConst(SgNode *n, SgVariableSymbol *symbol) {
  n = referenceOrigin(n);
  ROSE_ASSERT(n != nullptr && symbol != nullptr);
  if (SgVarRefExp *reference = isSgVarRefExp(n))
    return reference->get_symbol()->get_declaration() ==
           symbol->get_declaration();
  return isSgDotExp(n) &&
         containsNonConst(isSgDotExp(n)->get_lhs_operand(), symbol);
}

template <typename Target>
bool isInitializerForReferenceVar(SgAssignInitializer *n, Target target) {
  SgInitializedName *in = isSgInitializedName(n->get_parent());
  if (!in)
    return false;
  ROSE_ASSERT(in->get_type() != nullptr);
  auto *type = in->get_type()->stripTypedefsAndModifiers();
  return (isSgReferenceType(type) || isSgRvalueReferenceType(type)) &&
         containsNonConst(n, target);
}

namespace {
template <typename Target>
bool referenceArgumentsExpose(SgExprListExp *arguments,
                              const SgFunctionType *functionType,
                              Target target) {
  ROSE_ASSERT(arguments != nullptr);
  size_t index = 0;
  for (auto *argument : arguments->get_expressions()) {
    const size_t position = index++;
    if (!containsNonConst(argument, target))
      continue;
    if (functionType == nullptr)
      return true;
    const auto &parameters = functionType->get_arguments();
    if (position >= parameters.size()) {
      ROSE_ASSERT(functionType->get_has_ellipses());
      continue;
    }
    auto *type = parameters[position]->stripTypedefsAndModifiers();
    if (isSgReferenceType(type) || isSgRvalueReferenceType(type))
      return true;
  }
  return false;
}

SgFunctionType *referenceCallType(SgFunctionCallExp *call) {
  auto *callee = call->get_function();
  ROSE_ASSERT(callee != nullptr);
  if (isSgDotExp(callee) || isSgArrowExp(callee))
    callee = isSgBinaryOp(callee)->get_rhs_operand();
  if (auto *dependent = isSgNonrealRefExp(callee);
      dependent != nullptr &&
      dependent->get_semantic_role() == SgNonrealRefExp::e_dependent_callable) {
    ROSE_ASSERT(dependent->get_resolved_function_declaration() == nullptr);
    ROSE_ASSERT(dependent->get_resolved_variable_declaration() == nullptr);
    ROSE_ASSERT(isSgNonrealType(dependent->get_type()) != nullptr);
    return nullptr;
  }
  const unsigned char strip =
      SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE |
      SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE |
      SgType::STRIP_DECLTYPE_TYPE;
  auto *type = call->get_function()->get_type()->stripType(strip);
  if (auto *pointer = isSgPointerType(type))
    type = pointer->get_base_type()->stripType(strip);
  auto *functionType = isSgFunctionType(type);
  ROSE_ASSERT(functionType != nullptr);
  return functionType;
}

enum class EffectQuery { writes_or_escapes, escapes };

template <typename Target> class EffectVisitor : public AstSimpleProcessing {
  Target expr;
  bool &result;
  EffectQuery query;

public:
  EffectVisitor(Target expr, bool &result, EffectQuery query)
      : expr(expr), result(result), query(query) {}

  virtual void visit(SgNode *n) {
    if (result)
      return;
    switch (n->variantT()) {
    case V_SgAndAssignOp:
    case V_SgAssignOp:
    case V_SgDivAssignOp:
    case V_SgIorAssignOp:
    case V_SgLshiftAssignOp:
    case V_SgMinusAssignOp:
    case V_SgModAssignOp:
    case V_SgMultAssignOp:
    case V_SgPlusAssignOp:
    case V_SgRshiftAssignOp:
    case V_SgXorAssignOp:
    case V_SgDotStarOp:
    case V_SgPntrArrRefExp:
      if (query == EffectQuery::writes_or_escapes)
        result |= containsNonConst(isSgBinaryOp(n)->get_lhs_operand(), expr);
      break;

    case V_SgMinusMinusOp:
    case V_SgPlusPlusOp:
      if (query == EffectQuery::writes_or_escapes)
        result |= containsNonConst(isSgUnaryOp(n)->get_operand(), expr);
      break;

    case V_SgAddressOfOp:
      result |= containsNonConst(isSgUnaryOp(n)->get_operand(), expr);
      break;

    case V_SgFunctionCallExp: {
      auto *call = isSgFunctionCallExp(n);
      auto *functionType = referenceCallType(call);
      if (auto *member = isSgDotExp(call->get_function())) {
        auto *reference = isSgMemberFunctionRefExp(member->get_rhs_operand());
        bool exposesReceiver = functionType == nullptr;
        if (reference != nullptr) {
          auto *declaration = reference->get_symbol()->get_declaration();
          ROSE_ASSERT(declaration != nullptr);
          exposesReceiver |= !declaration->get_declarationModifier()
                                  .get_storageModifier()
                                  .isStatic();
        }
        if (exposesReceiver)
          result |= containsNonConst(member->get_lhs_operand(), expr);
      }
      result |= referenceArgumentsExpose(call->get_args(), functionType, expr);
      break;
    }

    case V_SgConstructorInitializer: {
      auto *initializer = isSgConstructorInitializer(n);
      auto *declaration = initializer->get_declaration();
      auto *type = initializer->get_type()->stripTypedefsAndModifiers();
      if (declaration == nullptr &&
          (SageInterface::isScalarType(type) || isSgPointerType(type) ||
           isSgPointerMemberType(type) || isSgEnumType(type) ||
           isSgTypeNullptr(type)))
        break;
      result |= referenceArgumentsExpose(
          initializer->get_args(),
          declaration != nullptr ? declaration->get_type() : nullptr, expr);
      break;
    }

    case V_SgLambdaCapture: {
      auto *capture = isSgLambdaCapture(n);
      if (capture->get_capture_by_reference())
        result |= containsNonConst(capture->get_capture_initializer(), expr);
      break;
    }

    case V_SgAssignInitializer:
      result |= isInitializerForReferenceVar(isSgAssignInitializer(n), expr);
      break;

    default: // Do nothing
      break;
    }
  }
};
} // namespace

bool isPotentiallyModified(SgExpression *expr, SgNode *root) {
  ROSE_ASSERT(expr != nullptr && root != nullptr);
  bool result = false;
  EffectVisitor(expr, result, EffectQuery::writes_or_escapes)
      .traverse(root, preorder);
  return result;
}

bool isPotentiallyModified(SgVariableSymbol *symbol, SgNode *root) {
  ROSE_ASSERT(symbol != nullptr && root != nullptr);
  bool result = false;
  EffectVisitor(symbol, result, EffectQuery::writes_or_escapes)
      .traverse(root, preorder);
  return result;
}

// Does expression expr potentially have its address taken somewhere in the
// tree pointed to by root?
bool hasAddressTaken(SgExpression *expr, SgNode *root) {
  ROSE_ASSERT(expr != nullptr && root != nullptr);
  bool result = false;
  if (isSgPntrArrRefExp(expr) || isSgPointerDerefExp(expr))
    return true;
  EffectVisitor(expr, result, EffectQuery::escapes).traverse(root, preorder);
  return result;
}
