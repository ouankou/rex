#include "rose.h"

#include "manglingSupport.h"

#include <memory>
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
  auto *type = SageBuilder::buildIntType();
  return SageBuilder::buildNewExp_nfi(type, SgPointerType::createType(type),
                                      nullptr, SgNewExp::e_no_array_bound,
                                      nullptr, integerConstructor(value),
                                      nullptr, globalSpecifier, nullptr, 1);
}

} // namespace

int main() {
  ROSE_ASSERT(trimSpaces("").empty());
  ROSE_ASSERT(trimSpaces(" \t\r\n").empty());
  ROSE_ASSERT(trimSpaces(" \toperator new\r\n") == "operator new");
  const std::string highByte(1, static_cast<char>(0xff));
  ROSE_ASSERT(trimSpaces(" " + highByte + " ") == highByte);
  ROSE_ASSERT(replaceNonAlphaNum(highByte) == "_255");

  auto global = std::make_unique<SgGlobal>();
  const SgTemplateArgumentPtrList noArguments;
  const std::string suffix = "__tas__no_templ_args__tae__";
  const std::pair<std::string, std::string> names[] = {
      {"box<__rexesc__tas>", "box__tas____rexesc__tas__tae__"},
      {"box<__rexesc____rexesc__quote>",
       "box__tas____rexesc____rexesc__quote__tae__"},
      {"box<__tas__>", "box__tas____tas____tae__"},
      {"box<_-3>", "box__tas____minus__3__tae__"},
      {"ns::box<int *, int &>",
       "ns__scope__box__tas__int__ptr____comma__int__ref____tae__"},
      {"_operator>", "_operator__tae__"},
      {"operator<=>", "operator__tas__=__tae__"},
      {"operator>>", "operator__tae____tae__"},
      {"operator<<", "operator__tas____tas__"},
      {"operator>=", "operator__tae__="},
      {"operator<=", "operator__tas__="},
      {"operator->*", "operator-__tae____ptr__"},
      {"operator->", "operator-__tae__"},
      {"operator&&", "operator__ref____ref__"},
      {"operator&", "operator__ref__"},
      {"operator,", "operator__comma__"},
      {"operator>", "operator__tae__"},
      {"operator<", "operator__tas__"},
      {"operator*", "operator__ptr__"}};
  for (const auto &name : names) {
    ROSE_ASSERT(mangleTemplateToString(name.first, noArguments, global.get()) ==
                name.second + suffix);
    ROSE_ASSERT(mangleTemplateToString("scope::" + name.first, noArguments,
                                       global.get()) ==
                "scope__scope__" + name.second + suffix);
  }

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
  firstParameter->set_explicitly_stored_type(differentType->get_type());
  ROSE_ASSERT(firstParameter->get_type() == differentType->get_type());
  ROSE_ASSERT(mangleExpression(firstParameter) ==
              mangleExpression(differentType));

  auto location = std::make_unique<Sg_File_Info>(
      "rex_expression_mangling_contract.C", 1, 1);
  const int fileId = location->get_file_id();
  auto *firstTemplate = SageBuilder::buildTemplateParameter(
      SgTemplateParameter::nontype_parameter, SageBuilder::buildIntType(),
      SgName("value"), nullptr, SgTemplateParameter::keyword_unspecified);
  auto *secondTemplate = SageBuilder::buildTemplateParameter(
      SgTemplateParameter::nontype_parameter, SageBuilder::buildIntType(),
      SgName("value"), nullptr, SgTemplateParameter::keyword_unspecified);
  firstTemplate->initialize_canonical_source_identity({fileId, 11, fileId, 11});
  secondTemplate->initialize_canonical_source_identity(
      {fileId, 17, fileId, 17});
  auto firstValue = std::make_unique<SgTemplateParameterVal>(0, firstTemplate);
  auto secondValue =
      std::make_unique<SgTemplateParameterVal>(0, secondTemplate);
  auto repeatedValue =
      std::make_unique<SgTemplateParameterVal>(0, firstTemplate);
  ROSE_ASSERT(mangleExpression(firstValue.get()) !=
              mangleExpression(secondValue.get()));
  ROSE_ASSERT(mangleExpression(firstValue.get()) ==
              mangleExpression(repeatedValue.get()));
  return 0;
}
