#include "clang-integer-value.hpp"

#include "sage3basic.h"
#include "sageBuilder.h"
#include <llvm/ADT/APSInt.h>

Rose::Integer128Value integer128ValueFromClang(const llvm::APSInt &value) {
  ROSE_ASSERT(value.getBitWidth() <= 128);
  const llvm::APInt bits =
      value.isUnsigned() ? value.zextOrTrunc(128) : value.sextOrTrunc(128);
  return Rose::Integer128Value::fromBits(
      {bits.getRawData()[0], bits.getRawData()[1]}, value.isUnsigned());
}

long long enumValueFromClang(const llvm::APSInt &value) {
  if (value.isSigned() ? !value.isSignedIntN(64) : !value.isIntN(64)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[enumerator-value-width]: enum value "
            "exceeds the SgEnumVal 64-bit domain\n");
    ROSE_ABORT();
  }
  return value.isSigned() ? value.getSExtValue()
                          : static_cast<long long>(value.getZExtValue());
}

// Both semantic template values and defaults use the same numeric producer.
SgValueExp *buildIntegralTemplateArgExpr(const llvm::APSInt &value,
                                         SgType *int_type) {
  ASSERT_not_null(int_type);
  auto finish_semantic_literal =
      [int_type](SgValueExp *value_expression) -> SgValueExp * {
    if (value_expression == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-integral-literal]: "
              "integral semantic argument has no exact literal and converted "
              "type\n");
      ROSE_ABORT();
    }
    value_expression->set_literal_type(int_type);
    if (value_expression->get_type() != int_type) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-integral-literal]: "
              "integral semantic literal did not retain its exact converted "
              "type\n");
      ROSE_ABORT();
    }
    value_expression->set_literal_spelling_form(
        SgValueExp::e_literal_canonical_generated);
    return value_expression;
  };

  SgType *base_type = int_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                          SgType::STRIP_TYPEDEF_TYPE);

  ROSE_ASSERT(base_type != nullptr);
  if (isSgTypeBool(base_type) != nullptr) {
    return finish_semantic_literal(
        SageBuilder::buildBoolValExp_nfi(value.getBoolValue()));
  }

  if (value.getBitWidth() == 128) {
    auto *literal = new SgInteger128Val(integer128ValueFromClang(value));
    SageInterface::setOneSourcePositionNull(literal);
    return finish_semantic_literal(literal);
  }
  ROSE_ASSERT(value.getBitWidth() <= 64);
  SgValueExp *expr = nullptr;

  if (isSgTypeChar(base_type) != nullptr) {
    ROSE_ASSERT(value.isSigned() || value.isIntN(63));
    long long raw = value.isSigned()
                        ? value.getSExtValue()
                        : static_cast<long long>(value.getZExtValue());
    expr = SageBuilder::buildCharVal_nfi(raw, "");
  } else if (isSgTypeSignedChar(base_type) != nullptr) {
    expr = SageBuilder::buildSignedCharVal_nfi(
        static_cast<signed char>(value.getSExtValue()), "");
  } else if (isSgTypeUnsignedChar(base_type) != nullptr ||
             isSgTypeChar8(base_type) != nullptr) {
    expr = SageBuilder::buildUnsignedCharVal_nfi(
        static_cast<unsigned char>(value.getZExtValue()), "");
  } else if (isSgTypeWchar(base_type) != nullptr) {
    ROSE_ASSERT(value.isSigned() || value.isIntN(63));
    const int64_t raw = value.isSigned()
                            ? value.getSExtValue()
                            : static_cast<int64_t>(value.getZExtValue());
    expr = SageBuilder::buildWcharVal_nfi(raw, "");
  } else if (isSgTypeChar16(base_type) != nullptr) {
    expr = SageBuilder::buildChar16Val_nfi(
        static_cast<unsigned short>(value.getZExtValue()), "");
  } else if (isSgTypeChar32(base_type) != nullptr) {
    expr = SageBuilder::buildChar32Val_nfi(
        static_cast<unsigned int>(value.getZExtValue()), "");
  } else if (isSgTypeShort(base_type) != nullptr) {
    expr = SageBuilder::buildShortVal_nfi(
        static_cast<short>(value.getSExtValue()), "");
  } else if (isSgTypeUnsignedShort(base_type) != nullptr) {
    expr = SageBuilder::buildUnsignedShortVal_nfi(
        static_cast<unsigned short>(value.getZExtValue()), "");
  } else if (isSgTypeInt(base_type) != nullptr) {
    expr = SageBuilder::buildIntVal_nfi(static_cast<int>(value.getSExtValue()));
  } else if (isSgTypeUnsignedInt(base_type) != nullptr) {
    expr = SageBuilder::buildUnsignedIntVal_nfi(
        static_cast<unsigned int>(value.getZExtValue()), "");
  } else if (isSgTypeLong(base_type) != nullptr) {
    expr = SageBuilder::buildLongIntVal_nfi(
        static_cast<long>(value.getSExtValue()), "");
  } else if (isSgTypeUnsignedLong(base_type) != nullptr) {
    expr = SageBuilder::buildUnsignedLongVal_nfi(
        static_cast<unsigned long>(value.getZExtValue()), "");
  } else if (isSgTypeLongLong(base_type) != nullptr) {
    expr = SageBuilder::buildLongLongIntVal_nfi(value.getSExtValue(), "");
  } else if (isSgTypeUnsignedLongLong(base_type) != nullptr) {
    expr =
        SageBuilder::buildUnsignedLongLongIntVal_nfi(value.getZExtValue(), "");
  } else {
    // Enum values carry the exact enum semantic type above and their numeric
    // value in the signed or unsigned storage selected by Clang.
    ROSE_ASSERT(isSgEnumType(base_type) != nullptr);
    if (value.isSigned()) {
      expr = SageBuilder::buildLongLongIntVal_nfi(value.getSExtValue(), "");
    } else {
      expr = SageBuilder::buildUnsignedLongLongIntVal_nfi(value.getZExtValue(),
                                                          "");
    }
  }

  return finish_semantic_literal(expr);
}
