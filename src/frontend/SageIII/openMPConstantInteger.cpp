#include "sage3basic.h"

#include "openMPConstantInteger.h"
#include "sageInterface.h"

#include <llvm/ADT/APInt.h>

#include <climits>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace Rose {
namespace OpenMP {
namespace Detail {

class ExactInteger {
  // OpenMP context-selector scores are base-language integer expressions.
  // One mebibit is far beyond every Sage integer type, but bounds hostile
  // shifts before they can request an unbounded allocation.
  static constexpr unsigned maximum_bit_width = 1U << 20;

  llvm::APInt value_;

  explicit ExactInteger(llvm::APInt value) : value_(std::move(value)) {
    value_ = value_.sextOrTrunc(value_.getSignificantBits());
  }

  static std::optional<unsigned> checkedWidth(std::uint64_t width) {
    if (width == 0 || width > maximum_bit_width) {
      return std::nullopt;
    }
    return static_cast<unsigned>(width);
  }

  static std::optional<std::pair<llvm::APInt, llvm::APInt>>
  aligned(const ExactInteger &lhs, const ExactInteger &rhs,
          unsigned extra_bits = 0) {
    const std::uint64_t width =
        static_cast<std::uint64_t>(
            std::max(lhs.value_.getBitWidth(), rhs.value_.getBitWidth())) +
        extra_bits;
    const std::optional<unsigned> checked = checkedWidth(width);
    if (!checked.has_value()) {
      return std::nullopt;
    }
    return std::make_pair(lhs.value_.sext(*checked), rhs.value_.sext(*checked));
  }

public:
  template <class Integer, std::enable_if_t<std::is_integral_v<Integer> &&
                                                std::is_signed_v<Integer>,
                                            int> = 0>
  explicit ExactInteger(Integer value)
      : ExactInteger(llvm::APInt(sizeof(Integer) * CHAR_BIT,
                                 static_cast<std::uint64_t>(value), true)) {}

  template <class Integer, std::enable_if_t<std::is_integral_v<Integer> &&
                                                std::is_unsigned_v<Integer>,
                                            int> = 0>
  explicit ExactInteger(Integer value)
      : ExactInteger(llvm::APInt(sizeof(Integer) * CHAR_BIT + 1,
                                 static_cast<std::uint64_t>(value), false)) {}

  bool isZero() const { return value_.isZero(); }
  bool isNegative() const { return value_.isNegative(); }

  std::optional<ExactInteger> negated() const {
    const std::optional<unsigned> width =
        checkedWidth(static_cast<std::uint64_t>(value_.getBitWidth()) + 1);
    if (!width.has_value()) {
      return std::nullopt;
    }
    return ExactInteger(-value_.sext(*width));
  }

  ExactInteger complemented() const { return ExactInteger(~value_); }

  std::optional<ExactInteger> add(const ExactInteger &rhs) const {
    auto operands = aligned(*this, rhs, 1);
    if (!operands.has_value()) {
      return std::nullopt;
    }
    return ExactInteger(operands->first + operands->second);
  }

  std::optional<ExactInteger> subtract(const ExactInteger &rhs) const {
    auto operands = aligned(*this, rhs, 1);
    if (!operands.has_value()) {
      return std::nullopt;
    }
    return ExactInteger(operands->first - operands->second);
  }

  std::optional<ExactInteger> multiply(const ExactInteger &rhs) const {
    const std::optional<unsigned> width =
        checkedWidth(static_cast<std::uint64_t>(value_.getBitWidth()) +
                     rhs.value_.getBitWidth());
    if (!width.has_value()) {
      return std::nullopt;
    }
    return ExactInteger(value_.sext(*width) * rhs.value_.sext(*width));
  }

  std::optional<ExactInteger> divide(const ExactInteger &rhs) const {
    if (rhs.isZero()) {
      return std::nullopt;
    }
    auto operands = aligned(*this, rhs, 1);
    if (!operands.has_value()) {
      return std::nullopt;
    }
    return ExactInteger(operands->first.sdiv(operands->second));
  }

  std::optional<ExactInteger> remainder(const ExactInteger &rhs) const {
    if (rhs.isZero()) {
      return std::nullopt;
    }
    auto operands = aligned(*this, rhs, 1);
    if (!operands.has_value()) {
      return std::nullopt;
    }
    return ExactInteger(operands->first.srem(operands->second));
  }

  std::optional<std::uint64_t> shiftAmount() const {
    if (isNegative() || value_.getActiveBits() > 64) {
      return std::nullopt;
    }
    return value_.getZExtValue();
  }

  std::optional<ExactInteger> shiftLeft(const ExactInteger &rhs) const {
    const std::optional<std::uint64_t> amount = rhs.shiftAmount();
    if (!amount.has_value()) {
      return std::nullopt;
    }
    const std::optional<unsigned> width = checkedWidth(
        static_cast<std::uint64_t>(value_.getBitWidth()) + *amount);
    if (!width.has_value()) {
      return std::nullopt;
    }
    return ExactInteger(
        value_.sext(*width).shl(static_cast<unsigned>(*amount)));
  }

  std::optional<ExactInteger> shiftRight(const ExactInteger &rhs) const {
    const std::optional<std::uint64_t> amount = rhs.shiftAmount();
    if (!amount.has_value()) {
      return std::nullopt;
    }
    if (*amount >= value_.getBitWidth()) {
      return ExactInteger(static_cast<std::int64_t>(isNegative() ? -1 : 0));
    }
    return ExactInteger(value_.ashr(static_cast<unsigned>(*amount)));
  }

  std::optional<ExactInteger> bitAnd(const ExactInteger &rhs) const {
    auto operands = aligned(*this, rhs);
    if (!operands.has_value()) {
      return std::nullopt;
    }
    return ExactInteger(operands->first & operands->second);
  }

  std::optional<ExactInteger> bitOr(const ExactInteger &rhs) const {
    auto operands = aligned(*this, rhs);
    if (!operands.has_value()) {
      return std::nullopt;
    }
    return ExactInteger(operands->first | operands->second);
  }

  std::optional<ExactInteger> bitXor(const ExactInteger &rhs) const {
    auto operands = aligned(*this, rhs);
    if (!operands.has_value()) {
      return std::nullopt;
    }
    return ExactInteger(operands->first ^ operands->second);
  }

  std::optional<int> compare(const ExactInteger &rhs) const {
    auto operands = aligned(*this, rhs);
    if (!operands.has_value()) {
      return std::nullopt;
    }
    if (operands->first == operands->second) {
      return 0;
    }
    return operands->first.slt(operands->second) ? -1 : 1;
  }
};

template <class Integer> ExactInteger exactInteger(Integer value) {
  return ExactInteger(value);
}

inline std::optional<ExactInteger> evaluateConstantInteger(
    SgExpression *expression,
    std::unordered_set<const SgInitializedName *> *active_variables) {
  if (expression == nullptr || active_variables == nullptr) {
    return std::nullopt;
  }

  if (SgIntVal *value = isSgIntVal(expression))
    return exactInteger(value->get_value());
  if (SgLongIntVal *value = isSgLongIntVal(expression))
    return exactInteger(value->get_value());
  if (SgLongLongIntVal *value = isSgLongLongIntVal(expression))
    return exactInteger(value->get_value());
  if (SgShortVal *value = isSgShortVal(expression))
    return exactInteger(value->get_value());
  if (SgSignedCharVal *value = isSgSignedCharVal(expression))
    return exactInteger(value->get_value());
  if (SgCharVal *value = isSgCharVal(expression))
    return exactInteger(value->get_value());
  if (SgUnsignedIntVal *value = isSgUnsignedIntVal(expression))
    return exactInteger(value->get_value());
  if (SgUnsignedLongVal *value = isSgUnsignedLongVal(expression))
    return exactInteger(value->get_value());
  if (SgUnsignedLongLongIntVal *value = isSgUnsignedLongLongIntVal(expression))
    return exactInteger(value->get_value());
  if (SgUnsignedShortVal *value = isSgUnsignedShortVal(expression))
    return exactInteger(value->get_value());
  if (SgUnsignedCharVal *value = isSgUnsignedCharVal(expression))
    return exactInteger(value->get_value());
  if (SgWcharVal *value = isSgWcharVal(expression))
    return exactInteger(value->get_value());
  if (SgChar16Val *value = isSgChar16Val(expression))
    return exactInteger(value->get_value());
  if (SgChar32Val *value = isSgChar32Val(expression))
    return exactInteger(value->get_value());
  if (SgBoolValExp *value = isSgBoolValExp(expression))
    return exactInteger(value->get_value());
  if (SgEnumVal *value = isSgEnumVal(expression))
    return exactInteger(value->get_value());

  if (SgVarRefExp *reference = isSgVarRefExp(expression)) {
    SgVariableSymbol *symbol = reference->get_symbol();
    SgInitializedName *declaration =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    if (declaration == nullptr ||
        !SageInterface::isConstType(reference->get_type()) ||
        !active_variables->insert(declaration).second) {
      return std::nullopt;
    }
    SgAssignInitializer *initializer =
        isSgAssignInitializer(declaration->get_initializer());
    std::optional<ExactInteger> result =
        initializer != nullptr
            ? evaluateConstantInteger(initializer->get_operand(),
                                      active_variables)
            : std::nullopt;
    active_variables->erase(declaration);
    return result;
  }

  if (SgConditionalExp *conditional = isSgConditionalExp(expression)) {
    conditional->validate();
    std::optional<ExactInteger> condition = evaluateConstantInteger(
        conditional->get_conditional_exp(), active_variables);
    if (!condition.has_value()) {
      return std::nullopt;
    }
    if (!condition->isZero() &&
        conditional->get_operator_kind() ==
            SgConditionalExp::e_conditional_operator_gnu_binary) {
      return condition;
    }
    return evaluateConstantInteger(!condition->isZero()
                                       ? conditional->get_true_exp()
                                       : conditional->get_false_exp(),
                                   active_variables);
  }

  if (SgUnaryOp *unary = isSgUnaryOp(expression)) {
    if (unary->get_operand() == nullptr) {
      return std::nullopt;
    }
    std::optional<ExactInteger> operand =
        evaluateConstantInteger(unary->get_operand(), active_variables);
    if (!operand.has_value()) {
      return std::nullopt;
    }
    if (isSgUnaryAddOp(unary) != nullptr || isSgCastExp(unary) != nullptr)
      return operand;
    if (isSgMinusOp(unary) != nullptr)
      return operand->negated();
    if (isSgNotOp(unary) != nullptr)
      return exactInteger(operand->isZero());
    if (isSgBitComplementOp(unary) != nullptr)
      return operand->complemented();
    return std::nullopt;
  }

  SgBinaryOp *binary = isSgBinaryOp(expression);
  if (binary == nullptr || binary->get_lhs_operand() == nullptr ||
      binary->get_rhs_operand() == nullptr) {
    return std::nullopt;
  }
  std::optional<ExactInteger> lhs =
      evaluateConstantInteger(binary->get_lhs_operand(), active_variables);
  std::optional<ExactInteger> rhs =
      evaluateConstantInteger(binary->get_rhs_operand(), active_variables);
  if (!lhs.has_value() || !rhs.has_value()) {
    return std::nullopt;
  }
  if (isSgAddOp(binary) != nullptr)
    return lhs->add(*rhs);
  if (isSgSubtractOp(binary) != nullptr)
    return lhs->subtract(*rhs);
  if (isSgMultiplyOp(binary) != nullptr)
    return lhs->multiply(*rhs);
  if (isSgDivideOp(binary) != nullptr)
    return lhs->divide(*rhs);
  if (isSgModOp(binary) != nullptr)
    return lhs->remainder(*rhs);
  if (isSgLshiftOp(binary) != nullptr)
    return lhs->shiftLeft(*rhs);
  if (isSgRshiftOp(binary) != nullptr)
    return lhs->shiftRight(*rhs);
  if (isSgBitAndOp(binary) != nullptr)
    return lhs->bitAnd(*rhs);
  if (isSgBitOrOp(binary) != nullptr)
    return lhs->bitOr(*rhs);
  if (isSgBitXorOp(binary) != nullptr)
    return lhs->bitXor(*rhs);
  if (isSgAndOp(binary) != nullptr)
    return exactInteger(!lhs->isZero() && !rhs->isZero());
  if (isSgOrOp(binary) != nullptr)
    return exactInteger(!lhs->isZero() || !rhs->isZero());

  const std::optional<int> comparison = lhs->compare(*rhs);
  if (!comparison.has_value()) {
    return std::nullopt;
  }
  if (isSgEqualityOp(binary) != nullptr)
    return exactInteger(*comparison == 0);
  if (isSgNotEqualOp(binary) != nullptr)
    return exactInteger(*comparison != 0);
  if (isSgLessThanOp(binary) != nullptr)
    return exactInteger(*comparison < 0);
  if (isSgLessOrEqualOp(binary) != nullptr)
    return exactInteger(*comparison <= 0);
  if (isSgGreaterThanOp(binary) != nullptr)
    return exactInteger(*comparison > 0);
  if (isSgGreaterOrEqualOp(binary) != nullptr)
    return exactInteger(*comparison >= 0);
  return std::nullopt;
}

std::optional<ExactInteger> evaluateConstantInteger(SgExpression *expression) {
  std::unordered_set<const SgInitializedName *> active_variables;
  return evaluateConstantInteger(expression, &active_variables);
}

} // namespace Detail

bool isNonnegativeConstantInteger(SgExpression *expression) {
  const std::optional<Detail::ExactInteger> value =
      Detail::evaluateConstantInteger(expression);
  return value.has_value() && !value->isNegative();
}

} // namespace OpenMP
} // namespace Rose
