#include "expressionTreeEqual.h"

#include "sage3basic.h"

#include <cmath>

bool expressionTreeEqualStar(const SgExpressionPtrList &as,
                             const SgExpressionPtrList &bs) {
  SgExpressionPtrList::const_iterator i, j;
  for (i = as.begin(), j = bs.begin(); i != as.end() && j != bs.end(); ++i, ++j)
    if (!expressionTreeEqual(*i, *j))
      return false;
  if (i != as.end() || j != bs.end())
    return false; // Mismatched lengths
  return true;
}

bool expressionTreeEqual(SgExpression *a, SgExpression *b) {
  ROSE_ASSERT(a != nullptr && b != nullptr);
  if (a->variantT() != b->variantT())
    return false;
  // PRE cannot equate effects or reads whose storage may change through
  // aliases.
  if (isSgAssignOp(a) || isSgCompoundAssignOp(a) || isSgPlusPlusOp(a) ||
      isSgMinusMinusOp(a) || isSgThrowOp(a) || isSgPointerDerefExp(a) ||
      isSgPntrArrRefExp(a) || isSgMemberAccessExp(a))
    return false;
  if (isSgBinaryOp(a)) {
    ROSE_ASSERT(isSgBinaryOp(b));
    if (a->get_type() != b->get_type())
      return false;
    return expressionTreeEqual(isSgBinaryOp(a)->get_lhs_operand(),
                               isSgBinaryOp(b)->get_lhs_operand()) &&
           expressionTreeEqual(isSgBinaryOp(a)->get_rhs_operand(),
                               isSgBinaryOp(b)->get_rhs_operand());
  }
  if (isSgUnaryOp(a)) {
    ROSE_ASSERT(isSgUnaryOp(b));
    SgUnaryOp *unary_a = isSgUnaryOp(a);
    SgUnaryOp *unary_b = isSgUnaryOp(b);
    if (SgCastExp *cast_a = isSgCastExp(a)) {
      SgCastExp *cast_b = isSgCastExp(b);
      cast_a->validate_semantic_conversion();
      cast_b->validate_semantic_conversion();
      if (cast_a->get_cast_type() != cast_b->get_cast_type() ||
          cast_a->get_semantic_conversion_kind() !=
              cast_b->get_semantic_conversion_kind() ||
          cast_a->get_value_category() != cast_b->get_value_category() ||
          cast_a->get_conversion_base_path() !=
              cast_b->get_conversion_base_path())
        return false;
    }
    return unary_a->get_type() == unary_b->get_type() &&
           unary_a->get_mode() == unary_b->get_mode() &&
           expressionTreeEqual(unary_a->get_operand(), unary_b->get_operand());
  }
  if (isSgConditionalExp(a)) {
    ROSE_ASSERT(isSgConditionalExp(b));
    SgConditionalExp *conditional_a = isSgConditionalExp(a);
    SgConditionalExp *conditional_b = isSgConditionalExp(b);
    conditional_a->validate();
    conditional_b->validate();
    if (conditional_a->get_operator_kind() !=
        conditional_b->get_operator_kind())
      return false;
    return expressionTreeEqual(conditional_a->get_conditional_exp(),
                               conditional_b->get_conditional_exp()) &&
           (conditional_a->get_operator_kind() ==
                SgConditionalExp::e_conditional_operator_gnu_binary ||
            expressionTreeEqual(conditional_a->get_true_exp(),
                                conditional_b->get_true_exp())) &&
           expressionTreeEqual(conditional_a->get_false_exp(),
                               conditional_b->get_false_exp());
  }
  if (isSgDeleteExp(a)) {
    ROSE_ASSERT(isSgDeleteExp(b));
    return false;
  }
  if (isSgExprListExp(a)) {
    ROSE_ASSERT(isSgExprListExp(b));
    return expressionTreeEqualStar(isSgExprListExp(a)->get_expressions(),
                                   isSgExprListExp(b)->get_expressions());
  }
  if (isSgFunctionCallExp(a)) {
    ROSE_ASSERT(isSgFunctionCallExp(b));
    return false;
  }
  if (isSgFunctionRefExp(a)) {
    ROSE_ASSERT(isSgFunctionRefExp(b));
    return (isSgFunctionRefExp(a)->get_symbol()->get_declaration() ==
            isSgFunctionRefExp(b)->get_symbol()->get_declaration());
  }
  if (isSgAssignInitializer(a)) {
    ROSE_ASSERT(isSgAssignInitializer(b));
    return expressionTreeEqual(isSgAssignInitializer(a)->get_operand(),
                               isSgAssignInitializer(b)->get_operand());
  }
  if (isSgAggregateInitializer(a)) {
    ROSE_ASSERT(isSgAggregateInitializer(b));
    return expressionTreeEqual(isSgAggregateInitializer(a)->get_initializers(),
                               isSgAggregateInitializer(b)->get_initializers());
  }
  if (isSgConstructorInitializer(a)) {
    ROSE_ASSERT(isSgConstructorInitializer(b));
    return false;
  }
  if (isSgMemberFunctionRefExp(a)) {
    ROSE_ASSERT(isSgMemberFunctionRefExp(b));
    return (isSgMemberFunctionRefExp(a)->get_symbol()->get_declaration() ==
            isSgMemberFunctionRefExp(b)->get_symbol()->get_declaration());
  }
  if (isSgNewExp(a)) {
    ROSE_ASSERT(isSgNewExp(b));
    return false;
  }
  if (isSgSizeOfOp(a)) {
    ROSE_ASSERT(isSgSizeOfOp(b));
    SgSizeOfOp *size_a = isSgSizeOfOp(a);
    SgSizeOfOp *size_b = isSgSizeOfOp(b);
    if (size_a->get_type() != size_b->get_type() ||
        size_a->get_operator_kind() != size_b->get_operator_kind())
      return false;
    if (size_a->get_operand_type() || size_b->get_operand_type())
      return size_a->get_operand_type() == size_b->get_operand_type();
    return expressionTreeEqual(size_a->get_operand_expr(),
                               size_b->get_operand_expr());
  }
  if (auto *align_a = isSgAlignOfOp(a)) {
    auto *align_b = isSgAlignOfOp(b);
    ROSE_ASSERT(align_b != nullptr);
    if (align_a->get_type() != align_b->get_type() ||
        align_a->get_operator_kind() != align_b->get_operator_kind())
      return false;
    if (align_a->get_operand_type() || align_b->get_operand_type())
      return align_a->get_operand_type() == align_b->get_operand_type();
    return expressionTreeEqual(align_a->get_operand_expr(),
                               align_b->get_operand_expr());
  }
  if (isSgThisExp(a)) {
    ROSE_ASSERT(isSgThisExp(b));
    SgThisExp *this_a = isSgThisExp(a);
    SgThisExp *this_b = isSgThisExp(b);
    SgType *type_a = this_a->get_type();
    SgType *type_b = this_b->get_type();
    return this_a->get_class_symbol() == this_b->get_class_symbol() &&
           this_a->get_nonreal_symbol() == this_b->get_nonreal_symbol() &&
           type_a == type_b;
  }
  if (isSgValueExp(a)) {
    ROSE_ASSERT(isSgValueExp(b));
    if (a->get_type() != b->get_type())
      return false;
    switch (a->variantT()) {
#define HANDLEVAL(type)                                                        \
  case V_##type:                                                               \
    return is##type(a)->get_value() == is##type(b)->get_value();

      HANDLEVAL(SgBoolValExp);
      HANDLEVAL(SgCharVal);
      HANDLEVAL(SgEnumVal);
      HANDLEVAL(SgIntVal);
      HANDLEVAL(SgLongIntVal);
      HANDLEVAL(SgLongLongIntVal);
      HANDLEVAL(SgShortVal);
      HANDLEVAL(SgStringVal);
      HANDLEVAL(SgSignedCharVal);
      HANDLEVAL(SgUnsignedCharVal);
      HANDLEVAL(SgUnsignedIntVal);
      HANDLEVAL(SgUnsignedLongLongIntVal);
      HANDLEVAL(SgUnsignedLongVal);
      HANDLEVAL(SgUnsignedShortVal);
      HANDLEVAL(SgWcharVal);
#undef HANDLEVAL

    case V_SgFloatVal: {
      const float left = isSgFloatVal(a)->get_value();
      const float right = isSgFloatVal(b)->get_value();
      return left == right && std::signbit(left) == std::signbit(right);
    }
    case V_SgDoubleVal: {
      const double left = isSgDoubleVal(a)->get_value();
      const double right = isSgDoubleVal(b)->get_value();
      return left == right && std::signbit(left) == std::signbit(right);
    }
    case V_SgLongDoubleVal: {
      const auto left = isSgLongDoubleVal(a)->get_value();
      const auto right = isSgLongDoubleVal(b)->get_value();
      return left.format() == right.format() && left.words() == right.words();
    }
    default:
      return false;
    }
  }
  if (isSgVarRefExp(a)) {
    ROSE_ASSERT(isSgVarRefExp(b));
    SgType *type = a->get_type();
    SgType *qualified = type->stripType(SgType::STRIP_TYPEDEF_TYPE);
    while (auto *modifier = isSgModifierType(qualified)) {
      if (modifier->get_typeModifier().isAtomic())
        return false;
      qualified =
          modifier->get_base_type()->stripType(SgType::STRIP_TYPEDEF_TYPE);
    }
    SgType *semantic_type = type->stripTypedefsAndModifiers();
    ROSE_ASSERT(semantic_type != nullptr);
    if (SageInterface::isVolatileType(type) ||
        isSgReferenceType(semantic_type) ||
        isSgRvalueReferenceType(semantic_type))
      return false;
    return (isSgVarRefExp(a)->get_symbol()->get_declaration() ==
            isSgVarRefExp(b)->get_symbol()->get_declaration());
  }
  return false;
}
