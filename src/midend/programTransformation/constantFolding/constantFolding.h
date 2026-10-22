#ifndef ROSE_CONSTANT_FOLDING_H
#define ROSE_CONSTANT_FOLDING_H

#include "AstProcessing.h"

#include "sageInterface.h"

#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

namespace ConstantFolding {

// Build an inherited attribute for the tree traversal to skip constant folded
// expressions
class ConstantFoldingInheritedAttribute {
public:
  bool isConstantFoldedValue;

  ConstantFoldingInheritedAttribute() : isConstantFoldedValue(false) {}

  ConstantFoldingInheritedAttribute(const ConstantFoldingInheritedAttribute &) =
      default;
};

class ConstantFoldingSynthesizedAttribute {
public:
  SgValueExp *newValueExp;

  ConstantFoldingSynthesizedAttribute() : newValueExp(NULL) {};
  ConstantFoldingSynthesizedAttribute(
      const ConstantFoldingSynthesizedAttribute &X)
      : newValueExp(X.newValueExp) {};
};

class ConstantFoldingTraversal
    : public SgTopDownBottomUpProcessing<ConstantFoldingInheritedAttribute,
                                         ConstantFoldingSynthesizedAttribute> {
public:
  // Functions required by the rewrite mechanism
  ConstantFoldingInheritedAttribute evaluateInheritedAttribute(
      SgNode *n, ConstantFoldingInheritedAttribute inheritedAttribute);
  ConstantFoldingSynthesizedAttribute evaluateSynthesizedAttribute(
      SgNode *n, ConstantFoldingInheritedAttribute inheritedAttribute,
      SubTreeSynthesizedAttributes synthesizedAttributeList);

  ~ConstantFoldingTraversal();

private:
  void replaceExpressionWithDeferredDelete(SgExpression *oldExp,
                                           SgExpression *newExp);

  std::vector<SgExpression *> deferred_deletes_;
};
ROSE_DLL_API void constantFoldingOptimization(SgNode *n);

ROSE_DLL_API SgValueExp *returnConstantFoldedValueExpression(SgNode *n);

// ***************************************************************************
// Constant un-folding is implemented here as a test to verify the
// correctness of the constant folding.
//    As an example: "int x = 3;" is transformed to be "int x = 1 + 2;"
// This transformation permits automated testing of the constant folding.
// ***************************************************************************

// Build a synthesized attribute for the tree traversal to unfold constants
// so that we can test the constant folding
class ConstantUnFoldingSynthesizedAttribute {
public:
  SgExpression *newExp;

  ConstantUnFoldingSynthesizedAttribute() : newExp(NULL) {};
  ConstantUnFoldingSynthesizedAttribute(
      const ConstantUnFoldingSynthesizedAttribute &X)
      : newExp(X.newExp) {};
};

class ConstantUnFoldingTraversal
    : public SgBottomUpProcessing<ConstantUnFoldingSynthesizedAttribute> {
public:
  // Functions required by the traversal mechanism
  ConstantUnFoldingSynthesizedAttribute evaluateSynthesizedAttribute(
      SgNode *n, SubTreeSynthesizedAttributes synthesizedAttributeList);
};

// This test transforms the AST to turn constant values into expressions that
// have the same value. This is a test for the constant folding optimization
// implemented above.
ROSE_DLL_API void constantUnFoldingTest(SgNode *n);

// A helper function: get value if it fits into int type
// T1: the internal value's type
// T2: the concrete value exp type
template <typename T1, typename T2> T1 cf_get_value_t(T2 *sg_value_exp) {
  return sg_value_exp->get_value();
}

//! Calculate the  result of a binary operation on two constant float-kind
//! values,
//   Integer-type-only binary operations are excluded : %, <<, >>, &, ^, |
// We ignore pointer and complex types for constant folding
//   none_integer types: floating point types
// The operators compatible with floating-point types are
//  + - * / <, > <=, >=, ==, !=  &&, ||
//
template <typename T>
T calculate_float_t(SgBinaryOp *binaryOperator, T lhsValue, T rhsValue) {
  T foldedValue{};

  switch (binaryOperator->variantT()) {
  // Basic arithmetic
  case V_SgAddOp: {
    foldedValue = lhsValue + rhsValue;
    break;
  }
  case V_SgSubtractOp: {
    foldedValue = lhsValue - rhsValue;
    break;
  }
  case V_SgMultiplyOp: {
    foldedValue = lhsValue * rhsValue;
    break;
  }
  case V_SgDivideOp: {
    foldedValue = lhsValue / rhsValue;
    break;
  }
  case V_SgIntegerDivideOp: // TODO what is this ??
  {
    foldedValue = lhsValue / rhsValue;
    break;
  }
  // logic operations
  case V_SgAndOp: {
    foldedValue = lhsValue && rhsValue;
    break;
  }

  case V_SgOrOp: {
    foldedValue = lhsValue || rhsValue;
    break;
  }

    // relational operations
  case V_SgEqualityOp: {
    foldedValue = (lhsValue == rhsValue);
    break;
  }
  case V_SgNotEqualOp: {
    foldedValue = (lhsValue != rhsValue);
    break;
  }
  case V_SgGreaterOrEqualOp: {
    foldedValue = (lhsValue >= rhsValue);
    break;
  }
  case V_SgGreaterThanOp: {
    foldedValue = (lhsValue > rhsValue);
    break;
  }
  case V_SgLessOrEqualOp: {
    foldedValue = (lhsValue <= rhsValue);
    break;
  }
  case V_SgLessThanOp: {
    foldedValue = (lhsValue < rhsValue);
    break;
  }
  default: {
    ROSE_ABORT();
  }

  } // end switch

  return foldedValue;
}
template <typename T> bool checkedIntegralProduct(T lhs, T rhs, T &result) {
  bool overflow = __builtin_mul_overflow(lhs, rhs, &result);
  return !std::is_signed_v<T> || !overflow;
}

template <typename T>
std::optional<T> calculateIntegral(SgBinaryOp *op, T lhs, T rhs) {
  static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);
  T result{};
  switch (op->variantT()) {
  case V_SgModOp:
  case V_SgDivideOp:
  case V_SgIntegerDivideOp:
    if (rhs == 0)
      return std::nullopt;
    if constexpr (std::is_signed_v<T>) {
      if (lhs == std::numeric_limits<T>::min() && rhs == -1)
        return std::nullopt;
    }
    if (op->variantT() == V_SgModOp)
      return static_cast<T>(lhs % rhs);
    return static_cast<T>(lhs / rhs);

  case V_SgLshiftOp:
    if constexpr (std::is_signed_v<T>) {
      if (lhs < 0 || rhs < 0)
        return std::nullopt;
    }
    if (static_cast<unsigned long long>(rhs) >= std::numeric_limits<T>::digits)
      return std::nullopt;
    if constexpr (std::is_signed_v<T>) {
      if (lhs > (std::numeric_limits<T>::max() >> rhs))
        return std::nullopt;
    }
    return static_cast<T>(static_cast<unsigned long long>(lhs) << rhs);

  case V_SgRshiftOp:
    if constexpr (std::is_signed_v<T>) {
      if (lhs < 0 || rhs < 0)
        return std::nullopt;
    }
    if (static_cast<unsigned long long>(rhs) >=
        std::numeric_limits<T>::digits + std::is_signed_v<T>)
      return std::nullopt;
    return static_cast<T>(lhs >> rhs);

  case V_SgBitAndOp:
    return static_cast<T>(lhs & rhs);
  case V_SgBitOrOp:
    return static_cast<T>(lhs | rhs);
  case V_SgBitXorOp:
    return static_cast<T>(lhs ^ rhs);

  case V_SgAddOp:
    if (__builtin_add_overflow(lhs, rhs, &result) && std::is_signed_v<T>)
      return std::nullopt;
    return result;
  case V_SgSubtractOp:
    if (__builtin_sub_overflow(lhs, rhs, &result) && std::is_signed_v<T>)
      return std::nullopt;
    return result;
  case V_SgMultiplyOp:
    if (!checkedIntegralProduct(lhs, rhs, result))
      return std::nullopt;
    return result;

  case V_SgAndOp:
    return static_cast<T>(lhs && rhs);
  case V_SgOrOp:
    return static_cast<T>(lhs || rhs);
  case V_SgEqualityOp:
    return static_cast<T>(lhs == rhs);
  case V_SgNotEqualOp:
    return static_cast<T>(lhs != rhs);
  case V_SgGreaterOrEqualOp:
    return static_cast<T>(lhs >= rhs);
  case V_SgGreaterThanOp:
    return static_cast<T>(lhs > rhs);
  case V_SgLessOrEqualOp:
    return static_cast<T>(lhs <= rhs);
  case V_SgLessThanOp:
    return static_cast<T>(lhs < rhs);

  case V_SgExponentiationOp: {
    if constexpr (std::is_signed_v<T>) {
      if (rhs < 0)
        return std::nullopt;
    }
    unsigned long long exponent = static_cast<unsigned long long>(rhs);
    T base = lhs;
    result = 1;
    while (exponent != 0) {
      if ((exponent & 1) != 0) {
        T product;
        if (!checkedIntegralProduct(result, base, product))
          return std::nullopt;
        result = product;
      }
      exponent >>= 1;
      if (exponent != 0) {
        T square;
        if (!checkedIntegralProduct(base, base, square))
          return std::nullopt;
        base = square;
      }
    }
    return result;
  }

  default:
    ROSE_ABORT();
  }
}

// For T type which is compatible for all unary operators we are interested in.
template <typename T> T calculate_u_t(SgUnaryOp *unaryOperator, T theValue) {
  ROSE_ASSERT(isSgMinusOp(unaryOperator) != nullptr);
  return -theValue;
}

inline bool isPredicateBinaryOperator(VariantT kind) {
  switch (kind) {
  case V_SgAndOp:
  case V_SgOrOp:
  case V_SgEqualityOp:
  case V_SgNotEqualOp:
  case V_SgGreaterOrEqualOp:
  case V_SgGreaterThanOp:
  case V_SgLessOrEqualOp:
  case V_SgLessThanOp:
    return true;
  default:
    return false;
  }
}

inline SgValueExp *buildPredicateResult(SgBinaryOp *op, bool value) {
  SgType *resultType = op->get_type();
  ROSE_ASSERT(resultType != nullptr);
  SgValueExp *result = nullptr;
  if (SageInterface::isEquivalentType(resultType, SgTypeBool::createType()))
    result = new SgBoolValExp(value);
  else if (SageInterface::isEquivalentType(resultType, SgTypeInt::createType()))
    result = new SgIntVal(value, "");
  else
    return nullptr;
  SageInterface::setOneSourcePositionForTransformation(result);
  return result;
}

// T1: a SgValExp's child SAGE class type,
// T2: its internal storage C type for its value
template <typename T1, typename T2>
static SgValueExp *buildResultValueExp_t(SgBinaryOp *binaryOperator,
                                         SgValueExp *lhsValue,
                                         SgValueExp *rhsValue) {
  SgValueExp *result = NULL;
  T1 *lhs_v = dynamic_cast<T1 *>(lhsValue);
  T1 *rhs_v = dynamic_cast<T1 *>(rhsValue);
  ROSE_ASSERT(lhs_v);
  ROSE_ASSERT(rhs_v);

  T2 lhs = cf_get_value_t<T2>(lhs_v);
  T2 rhs = cf_get_value_t<T2>(rhs_v);
  if (!isPredicateBinaryOperator(binaryOperator->variantT()) &&
      !SageInterface::isEquivalentType(binaryOperator->get_type(),
                                       lhs_v->get_type())) {
    if constexpr (std::is_integral_v<T2> &&
                  std::numeric_limits<T2>::digits <
                      std::numeric_limits<int>::digits) {
      if (SageInterface::isEquivalentType(binaryOperator->get_type(),
                                          SgTypeInt::createType())) {
        std::optional<int> promoted = calculateIntegral(
            binaryOperator, static_cast<int>(lhs), static_cast<int>(rhs));
        if (!promoted)
          return nullptr;
        result = new SgIntVal(*promoted, "");
        SageInterface::setOneSourcePositionForTransformation(result);
        return result;
      }
    }
    return nullptr;
  }
  std::optional<T2> foldedValue = calculateIntegral(binaryOperator, lhs, rhs);
  if (!foldedValue)
    return nullptr;
  if (isPredicateBinaryOperator(binaryOperator->variantT()))
    return buildPredicateResult(binaryOperator, *foldedValue != 0);
  result = new T1(*foldedValue, "");
  ROSE_ASSERT(result != NULL);
  SageInterface::setOneSourcePositionForTransformation(result);
  return result;
}

// T1: a SgValExp's child SAGE class type,
// T2: its internal storage C type for its value
template <typename T1, typename T2>
static SgValueExp *buildResultValueExp_u_t(SgUnaryOp *unaryOperator,
                                           SgValueExp *theValue) {
  SgValueExp *result = NULL;
  T1 *t1_v = dynamic_cast<T1 *>(theValue);
  ROSE_ASSERT(t1_v);
  ROSE_ASSERT(unaryOperator->get_type() != nullptr);
  T2 t2_v = cf_get_value_t<T2>(t1_v);
  if (!SageInterface::isEquivalentType(unaryOperator->get_type(),
                                       t1_v->get_type())) {
    if constexpr (std::is_integral_v<T2> &&
                  std::numeric_limits<T2>::digits <
                      std::numeric_limits<int>::digits) {
      if (SageInterface::isEquivalentType(unaryOperator->get_type(),
                                          SgTypeInt::createType())) {
        result = new SgIntVal(-static_cast<int>(t2_v), "");
        SageInterface::setOneSourcePositionForTransformation(result);
        return result;
      }
    }
    return nullptr;
  }
  if constexpr (std::numeric_limits<T2>::is_integer &&
                std::numeric_limits<T2>::is_signed) {
    if (t2_v == std::numeric_limits<T2>::min())
      return nullptr;
  }
  T2 foldedValue;
  foldedValue = calculate_u_t(unaryOperator, t2_v);
  result = new T1(foldedValue, "");
  ROSE_ASSERT(result != NULL);
  SageInterface::setOneSourcePositionForTransformation(result);
  return result;
}

// T1: a SgValExp's child SAGE class type,
// T2: its internal storage C type for its value
// for T2 types (floating point types) which are incompatible with integer-only
// operations: e.g. if T2 is double, then % is now allowed
template <typename T1, typename T2>
static SgValueExp *buildResultValueExp_float_t(SgBinaryOp *binaryOperator,
                                               SgValueExp *lhsValue,
                                               SgValueExp *rhsValue) {
  SgValueExp *result = NULL;
  T1 *lhs_v = dynamic_cast<T1 *>(lhsValue);
  T1 *rhs_v = dynamic_cast<T1 *>(rhsValue);
  ROSE_ASSERT(lhs_v);
  ROSE_ASSERT(rhs_v);

  T2 lhs = cf_get_value_t<T2>(lhs_v);
  T2 rhs = cf_get_value_t<T2>(rhs_v);
  if (!isPredicateBinaryOperator(binaryOperator->variantT()) &&
      !SageInterface::isEquivalentType(binaryOperator->get_type(),
                                       lhs_v->get_type()))
    return nullptr;
  T2 foldedValue;
  foldedValue = calculate_float_t(binaryOperator, lhs, rhs);
  if (isPredicateBinaryOperator(binaryOperator->variantT()))
    return buildPredicateResult(binaryOperator, foldedValue != 0);
  result = new T1(foldedValue, "");
  ROSE_ASSERT(result != NULL);
  SageInterface::setOneSourcePositionForTransformation(result);
  return result;
}

// end of ConstantFolding namespace
} // namespace ConstantFolding

#endif
