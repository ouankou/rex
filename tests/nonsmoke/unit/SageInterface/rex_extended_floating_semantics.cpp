#include "rose.h"

#include <cfloat>
#include <string>
#include <type_traits>
#include <vector>

int main(int argc, char **argv) {
  // Check the native-storage API without native floating arithmetic or a
  // floating return ABI changing the represented values before import.
#if LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384
  static const long double subnormal = 0x1p-16445L;
  static const long double large = 0x1p1024L;
  static const long double precise = 0x1.000000000000001p0L;
  using Value = Rose::FloatingValue;
  ROSE_ASSERT((Value(subnormal).words() == Value::Words{1, 0}));
  ROSE_ASSERT(
      (Value(large).words() == Value::Words{0x8000000000000000, 0x43ff}));
  ROSE_ASSERT(
      (Value(precise).words() == Value::Words{0x8000000000000008, 0x3fff}));
#endif
  ROSE_ASSERT(argc >= 3);
  const bool mutate = std::string(argv[1]) == "mutate";
  ROSE_ASSERT(mutate || std::string(argv[1]) == "unchanged");
  std::vector<std::string> arguments{argv[0]};
  for (int i = 2; i < argc; ++i)
    arguments.emplace_back(argv[i]);
  auto *project = frontend(arguments);
  ROSE_ASSERT(project != nullptr && project->get_frontendErrorCode() == 0);
  AstTests::runAllTests(project);
  size_t checked = 0;
  size_t generated = 0;
  for (auto *node : NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    const auto name = function->get_name().getString();
    if (function->get_definition() != nullptr &&
        (name == "rex_quad_infinity" || name == "rex_quad_nan")) {
      const auto returns =
          NodeQuery::querySubTree(function->get_definition(), V_SgReturnStmt);
      ROSE_ASSERT(returns.size() == 1);
      using Value = Rose::FloatingValue;
      const auto value = Value::fromBits(
          Value::Format::binary128,
          name == "rex_quad_infinity"
              ? Value::Words{0, 0xffff000000000000}
              : Value::Words{0x123456789abcdef0, 0xffff800000005678});
      auto *statement = isSgReturnStmt(returns.front());
      SageInterface::replaceExpression(statement->get_expression(),
                                       SageBuilder::buildFloat128Val(value),
                                       false);
      ++generated;
      continue;
    }
    if (function->get_definition() == nullptr ||
        (name != "rex_extended_precision" && name != "rex_extended_exponent" &&
         name != "rex_quad_precision"))
      continue;
    const auto values =
        NodeQuery::querySubTree(function->get_definition(), V_SgValueExp);
    ROSE_ASSERT(values.size() == 1);
    auto check = [&](auto *literal) {
      ROSE_ASSERT(literal != nullptr);
      const auto value = literal->get_value();
      ROSE_ASSERT(value > 1 && !value.isInfinite());
      if (name != "rex_extended_exponent")
        ROSE_ASSERT(value < 2);
      SgTreeCopy copier;
      auto *copy = dynamic_cast<std::remove_pointer_t<decltype(literal)> *>(
          literal->copy(copier));
      ROSE_ASSERT(copy != nullptr);
      ROSE_ASSERT(copy->get_value().format() == value.format());
      ROSE_ASSERT(copy->get_value().words() == value.words());
      if (mutate) {
        literal->set_value(value + 1);
        ROSE_ASSERT(literal->get_value().format() == value.format());
        ROSE_ASSERT(literal->get_valueString().empty());
        ROSE_ASSERT(copy->get_value().words() == value.words());
      }
      SageInterface::deleteAST(copy);
    };
    if (name == "rex_quad_precision")
      check(isSgFloat128Val(values.front()));
    else
      check(isSgLongDoubleVal(values.front()));
    ++checked;
  }
  ROSE_ASSERT(checked == 3 && generated == 2);
  AstTests::runAllTests(project);
  project->unparse();
  SageInterface::deleteAST(project);
}
