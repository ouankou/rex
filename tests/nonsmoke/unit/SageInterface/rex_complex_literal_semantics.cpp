#include "rose.h"

#include <limits>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  ROSE_ASSERT(argc >= 3);
  const bool mutate = std::string(argv[1]) == "mutate";
  ROSE_ASSERT(mutate || std::string(argv[1]) == "unchanged");
  std::vector<std::string> arguments{argv[0]};
  for (int i = 2; i < argc; ++i)
    arguments.emplace_back(argv[i]);
  SgProject *project = frontend(arguments);
  ROSE_ASSERT(project != nullptr && project->get_frontendErrorCode() == 0);
  AstTests::runAllTests(project);
  size_t literals = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgComplexVal)) {
    auto *complex = isSgComplexVal(node);
    ROSE_ASSERT(complex->get_real_value() == nullptr);
    auto *component = complex->get_imaginary_value();
    ROSE_ASSERT(component != nullptr && component->get_parent() == complex);
    auto check = [&](auto *value) {
      ROSE_ASSERT(value != nullptr);
      // The child is a scalar. Its source token must not apply the imaginary
      // operation already represented by its typed owner.
      ROSE_ASSERT(value->get_valueString().find_first_of("iIjJ") ==
                  std::string::npos);
      if (mutate)
        value->set_value(value->get_value() + 1);
    };
    if (auto *value = isSgFloatVal(component))
      check(value);
    else if (auto *value = isSgDoubleVal(component))
      check(value);
    else if (auto *value = isSgLongDoubleVal(component))
      check(value);
    else
      check(isSgIntVal(component));
    ++literals;
  }
  ROSE_ASSERT(literals == 6);
  size_t generated = 0;
  size_t generated_integers = 0;
  size_t arithmetic_macros = 0;
  size_t conditional_macros = 0;
  size_t type_trait_macros = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    if (function->get_definition() == nullptr)
      continue;
    const std::string name = function->get_name().getString();
    if (name == "rex_generated_min_int" || name == "rex_generated_min_long" ||
        name == "rex_generated_min_long_long" ||
        name == "rex_generated_narrow_signed_char" ||
        name == "rex_generated_narrow_unsigned_char" ||
        name == "rex_generated_narrow_short" ||
        name == "rex_generated_narrow_unsigned_short") {
      const auto returns =
          NodeQuery::querySubTree(function->get_definition(), V_SgReturnStmt);
      ROSE_ASSERT(returns.size() == 1);
      auto *call = isSgFunctionCallExp(
          isSgReturnStmt(returns.front())->get_expression());
      ROSE_ASSERT(call != nullptr &&
                  call->get_args()->get_expressions().size() == 1);
      SgExpression *value = nullptr;
      if (name == "rex_generated_min_int")
        value = SageBuilder::buildIntVal(std::numeric_limits<int>::min());
      else if (name == "rex_generated_min_long")
        value = SageBuilder::buildLongIntVal(std::numeric_limits<long>::min());
      else if (name == "rex_generated_min_long_long")
        value = SageBuilder::buildLongLongIntVal(
            std::numeric_limits<long long>::min());
      else if (name == "rex_generated_narrow_signed_char")
        value = SageBuilder::buildSignedCharVal(2);
      else if (name == "rex_generated_narrow_unsigned_char")
        value = SageBuilder::buildUnsignedCharVal(2);
      else if (name == "rex_generated_narrow_short")
        value = SageBuilder::buildShortVal(2);
      else
        value = SageBuilder::buildUnsignedShortVal(2);
      SageInterface::replaceExpression(
          call->get_args()->get_expressions().front(), value, false);
      ++generated_integers;
      continue;
    }
    if (name == "rex_conditional_lvalue") {
      const auto conditionals = NodeQuery::querySubTree(
          function->get_definition(), V_SgConditionalExp);
      ROSE_ASSERT(conditionals.size() == 1);
      ROSE_ASSERT(isSgConditionalExp(conditionals.front())->get_lvalue());
      continue;
    }
    if (name == "rex_sizeof_macro" || name == "rex_alignof_macro") {
      const auto macros = NodeQuery::querySubTree(function->get_definition(),
                                                  V_SgMacroExpansionExp);
      ROSE_ASSERT(macros.size() == 1);
      auto *macro = isSgMacroExpansionExp(macros.front());
      auto *expanded = macro->get_expanded_expression_checked();
      ROSE_ASSERT(name == "rex_sizeof_macro"
                      ? isSgSizeOfOp(expanded) != nullptr
                      : isSgAlignOfOp(expanded) != nullptr);
      ++type_trait_macros;
      continue;
    }
    if (name == "rex_conditional_macro" ||
        name == "rex_binary_conditional_macro") {
      const auto macros = NodeQuery::querySubTree(function->get_definition(),
                                                  V_SgMacroExpansionExp);
      ROSE_ASSERT(macros.size() == 1);
      auto *macro = isSgMacroExpansionExp(macros.front());
      auto *conditional =
          isSgConditionalExp(macro->get_expanded_expression_checked());
      ROSE_ASSERT(conditional != nullptr);
      auto *operand = isSgIntVal(name == "rex_conditional_macro"
                                     ? conditional->get_true_exp()
                                     : conditional->get_conditional_exp());
      ROSE_ASSERT(operand != nullptr && operand->get_value() == 8);
      if (mutate)
        operand->set_value(9);
      ++conditional_macros;
      continue;
    }
    if (name == "rex_arithmetic_macro") {
      const auto macros = NodeQuery::querySubTree(function->get_definition(),
                                                  V_SgMacroExpansionExp);
      ROSE_ASSERT(macros.size() == 1);
      auto *macro = isSgMacroExpansionExp(macros.front());
      auto *sum = isSgAddOp(macro->get_expanded_expression_checked());
      ROSE_ASSERT(sum != nullptr);
      auto *operand = isSgIntVal(sum->get_lhs_operand_i());
      ROSE_ASSERT(operand != nullptr && operand->get_value() == 8);
      if (mutate)
        operand->set_value(9);
      ++arithmetic_macros;
      continue;
    }
    if (name != "rex_generated_infinity" && name != "rex_generated_nan" &&
        name != "rex_generated_negative_float" &&
        name != "rex_generated_negative_double")
      continue;
    const auto returns =
        NodeQuery::querySubTree(function->get_definition(), V_SgReturnStmt);
    ROSE_ASSERT(returns.size() == 1);
    auto *statement = isSgReturnStmt(returns.front());
    if (name == "rex_generated_negative_float" ||
        name == "rex_generated_negative_double") {
      SgExpression *value = nullptr;
      if (name == "rex_generated_negative_float") {
        value =
            SageBuilder::buildFloatVal(-std::numeric_limits<float>::infinity());
      } else {
        value = SageBuilder::buildDoubleVal(
            -std::numeric_limits<double>::infinity());
      }
      SageInterface::replaceExpression(statement->get_expression(), value,
                                       false);
      ++generated;
      continue;
    }
    auto *value = SageBuilder::buildComplexVal(
        SageBuilder::buildFloatVal(-0.0f),
        SageBuilder::buildFloatVal(
            name == "rex_generated_infinity"
                ? std::numeric_limits<float>::infinity()
                : std::numeric_limits<float>::quiet_NaN()),
        SageBuilder::buildFloatType());
    SageInterface::replaceExpression(statement->get_expression(), value, false);
    ++generated;
  }
  ROSE_ASSERT(generated == 4);
  ROSE_ASSERT(generated_integers == (SageInterface::is_Cxx_language() ? 7 : 0));
  ROSE_ASSERT(arithmetic_macros == 1);
  ROSE_ASSERT(conditional_macros == 2);
  ROSE_ASSERT(type_trait_macros == 2);
  AstTests::runAllTests(project);
  project->unparse();
  SageInterface::deleteAST(project);
}
