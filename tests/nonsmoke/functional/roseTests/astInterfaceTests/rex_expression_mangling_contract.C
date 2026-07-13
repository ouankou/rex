#include "rose.h"

#include "manglingSupport.h"

#include <string>

namespace {

SgExprListExp *integerArguments(int value) {
  SgExprListExp *arguments = SageBuilder::buildExprListExp_nfi();
  ROSE_ASSERT(arguments != nullptr);
  arguments->append_expression(SageBuilder::buildIntVal_nfi(value));
  return arguments;
}

SgConstructorInitializer *integerConstructor(int value) {
  return SageBuilder::buildConstructorInitializer_nfi(
      nullptr, integerArguments(value), SageBuilder::buildIntType(), true,
      false, true, true);
}

SgNewExp *integerNewExpression(int value, short globalSpecifier) {
  return SageBuilder::buildNewExp_nfi(SageBuilder::buildIntType(), nullptr,
                                      integerConstructor(value), nullptr,
                                      globalSpecifier, nullptr);
}

} // namespace

int main() {
  SgConstructorInitializer *firstConstructor = integerConstructor(7);
  SgConstructorInitializer *sameConstructor = integerConstructor(7);
  SgConstructorInitializer *differentConstructor = integerConstructor(9);
  ROSE_ASSERT(mangleExpression(firstConstructor) ==
              mangleExpression(sameConstructor));
  ROSE_ASSERT(mangleExpression(firstConstructor) !=
              mangleExpression(differentConstructor));

  SgNewExp *firstNew = integerNewExpression(7, 0);
  SgNewExp *sameNew = integerNewExpression(7, 0);
  SgNewExp *differentArgumentNew = integerNewExpression(9, 0);
  SgNewExp *globalNew = integerNewExpression(7, 1);
  ROSE_ASSERT(mangleExpression(firstNew) == mangleExpression(sameNew));
  ROSE_ASSERT(mangleExpression(firstNew) !=
              mangleExpression(differentArgumentNew));
  ROSE_ASSERT(mangleExpression(firstNew) != mangleExpression(globalNew));

  SgFunctionParameterRefExp *firstParameter =
      SageBuilder::buildFunctionParameterRefExp_nfi(
          2, 1, SageBuilder::buildIntType());
  SgFunctionParameterRefExp *sameParameter =
      SageBuilder::buildFunctionParameterRefExp_nfi(
          2, 1, SageBuilder::buildIntType());
  SgFunctionParameterRefExp *differentPosition =
      SageBuilder::buildFunctionParameterRefExp_nfi(
          3, 1, SageBuilder::buildIntType());
  SgFunctionParameterRefExp *differentLevel =
      SageBuilder::buildFunctionParameterRefExp_nfi(
          2, 0, SageBuilder::buildIntType());
  SgFunctionParameterRefExp *differentType =
      SageBuilder::buildFunctionParameterRefExp_nfi(
          2, 1, SageBuilder::buildLongType());
  ROSE_ASSERT(mangleExpression(firstParameter) ==
              mangleExpression(sameParameter));
  ROSE_ASSERT(mangleExpression(firstParameter) !=
              mangleExpression(differentPosition));
  ROSE_ASSERT(mangleExpression(firstParameter) !=
              mangleExpression(differentLevel));
  ROSE_ASSERT(mangleExpression(firstParameter) !=
              mangleExpression(differentType));
  return 0;
}
