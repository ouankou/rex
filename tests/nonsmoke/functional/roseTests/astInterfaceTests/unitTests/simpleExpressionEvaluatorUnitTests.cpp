
#include "testSupport.h"

TEST(SimpleExpressionEvaluator, EvaluateSimpleIntegerLiteral42){
  SgExpression *expr = SageBuilder::buildIntVal(42);
  struct SageInterface::const_int_expr_t res = SageInterface::evaluateConstIntegerExpression(expr);
  ASSERT_EQ(res.hasValue_, true);
  EXPECT_EQ(res.value_, 42);
}

TEST(SimpleExpressionEvaluator, EvaluateSimpleIntegerAddition){
  SgExpression *lhs = SageBuilder::buildIntVal(42);
  SgExpression *rhs = SageBuilder::buildIntVal(21);
  SgExpression *bop =
      SageBuilder::buildAddOp(lhs, rhs, SageBuilder::buildIntType());
  struct SageInterface::const_int_expr_t res = SageInterface::evaluateConstIntegerExpression(bop);
  ASSERT_EQ(res.hasValue_, true);
  EXPECT_EQ(res.value_, 63);
}

TEST(SimpleExpressionEvaluator, EvaluateSimpleIntegerAdditionAddition){
  SgExpression *lhs = SageBuilder::buildIntVal(42);
  SgExpression *rhs = SageBuilder::buildIntVal(21);
  SgExpression *iVal13 = SageBuilder::buildIntVal(13);
  SgExpression *bop =
      SageBuilder::buildAddOp(lhs, rhs, SageBuilder::buildIntType());
  SgExpression *expr =
      SageBuilder::buildAddOp(bop, iVal13, SageBuilder::buildIntType());
  struct SageInterface::const_int_expr_t res = SageInterface::evaluateConstIntegerExpression(expr);
  ASSERT_EQ(res.hasValue_, true);
  EXPECT_EQ(res.value_, 76);
}

TEST(SimpleExpressionEvaluator, PreservesSignedIntermediate) {
  SgExpression *difference = SageBuilder::buildSubtractOp(
      SageBuilder::buildIntVal(3), SageBuilder::buildIntVal(5),
      SageBuilder::buildIntType());
  SgExpression *expression = SageBuilder::buildAddOp(
      difference, SageBuilder::buildIntVal(4), SageBuilder::buildIntType());

  auto result = SageInterface::evaluateConstIntegerExpression(expression);
  ASSERT_TRUE(result.hasValue_);
  EXPECT_EQ(result.value_, 2U);
  EXPECT_FALSE(
      SageInterface::evaluateConstIntegerExpression(difference).hasValue_);
}

TEST(SimpleExpressionEvaluator, EvaluatesCharacterIntegerLiterals) {
  auto character = SageInterface::evaluateConstIntegerExpression(
      SageBuilder::buildCharVal(42));
  ASSERT_TRUE(character.hasValue_);
  EXPECT_EQ(character.value_, 42U);

  auto unsignedCharacter = SageInterface::evaluateConstIntegerExpression(
      SageBuilder::buildUnsignedCharVal(200));
  ASSERT_TRUE(unsignedCharacter.hasValue_);
  EXPECT_EQ(unsignedCharacter.value_, 200U);
}

TEST(SimpleExpressionEvaluator, RejectsUnsupportedAndUndefinedValues) {
  EXPECT_FALSE(SageInterface::evaluateConstIntegerExpression(
                   SageBuilder::buildFloatVal(1.5F))
                   .hasValue_);

  SgExpression *division = SageBuilder::buildDivideOp(
      SageBuilder::buildIntVal(1), SageBuilder::buildIntVal(0),
      SageBuilder::buildIntType());
  EXPECT_FALSE(
      SageInterface::evaluateConstIntegerExpression(division).hasValue_);
}
