#include "rose.h"

#include "AstInterface_ROSE.h"
#include "RoseAst.h"
#include "SymbolicVal.h"
#include "manglingSupport.h"

#include <string>
#include <vector>

namespace {

SgProject *parseProject(int argc, char **argv, int first_frontend_argument) {
  std::vector<char *> arguments;
  arguments.push_back(argv[0]);
  for (int i = first_frontend_argument; i < argc; ++i)
    arguments.push_back(argv[i]);
  return frontend(static_cast<int>(arguments.size()), arguments.data());
}

SgFunctionDeclaration *findFunction(SgNode *root, const std::string &name) {
  RoseAst ast(root);
  for (RoseAst::iterator current = ast.begin(); current != ast.end();
       ++current) {
    SgNode *node = *current;
    SgFunctionDeclaration *function = isSgFunctionDeclaration(node);
    if (function != nullptr && function->get_name().getString() == name &&
        function->get_definition() != nullptr)
      return function;
  }
  ROSE_ABORT();
}

SgExpression *returnExpression(SgFunctionDeclaration *function) {
  Rose_STL_Container<SgNode *> returns =
      NodeQuery::querySubTree(function->get_definition(), V_SgReturnStmt);
  ROSE_ASSERT(returns.size() == 1);
  SgExpression *expression = isSgReturnStmt(returns.front())->get_expression();
  ROSE_ASSERT(expression != nullptr);
  return expression;
}

SgCastExp *findConversion(
    SgNode *root, SgCastExp::semantic_conversion_kind_enum expected_kind,
    SgCastExp::cast_type_enum expected_surface = SgCastExp::e_unknown) {
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgCastExp)) {
    SgCastExp *cast = isSgCastExp(node);
    cast->validate_semantic_conversion();
    if (cast->get_semantic_conversion_kind() == expected_kind &&
        (expected_surface == SgCastExp::e_unknown ||
         cast->get_cast_type() == expected_surface))
      return cast;
  }
  ROSE_ABORT();
}

SgCastExp *
requireNestedConversion(SgNode *root, SgCastExp::cast_type_enum outer_surface,
                        SgCastExp::semantic_conversion_kind_enum inner_kind) {
  SgCastExp *outer = findConversion(root, SgCastExp::e_semantic_conversion_NoOp,
                                    outer_surface);
  SgCastExp *inner = isSgCastExp(outer->get_operand());
  ROSE_ASSERT(inner != nullptr);
  inner->validate_semantic_conversion();
  ROSE_ASSERT(inner->get_cast_type() == SgCastExp::e_implicit_cast);
  ROSE_ASSERT(inner->get_semantic_conversion_kind() == inner_kind);
  return inner;
}

int exercisePositiveContracts(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv, 1);
  ROSE_ASSERT(project != nullptr);

  SgCastExp *identity =
      findConversion(findFunction(project, "rex_cast_identity"),
                     SgCastExp::e_semantic_conversion_LValueToRValue,
                     SgCastExp::e_implicit_cast);
  AstInterfaceImpl implementation(identity);
  AstInterface interface(&implementation);
  AstInterface::OperatorEnum operation = AstInterface::OP_NONE;
  AstNodePtr operand;
  ROSE_ASSERT(interface.IsUnaryOp(AstNodePtr(identity), &operation, &operand));
  ROSE_ASSERT(operation == AstInterface::UOP_SEMANTIC_CONVERSION);
  ROSE_ASSERT(operand.get_ptr() == identity->get_operand());
  ROSE_ASSERT(AstInterface::SkipCasting(identity) == identity->get_operand());

  SymbolicVal converted =
      SymbolicValGenerator::GetSymbolicVal(interface, AstNodePtr(identity));
  SymbolicVal direct = SymbolicValGenerator::GetSymbolicVal(
      interface, AstNodePtr(identity->get_operand()));
  ROSE_ASSERT(!converted.toString().empty());
  ROSE_ASSERT(converted.toString() == direct.toString());

  SgExpression *narrowFalse =
      returnExpression(findFunction(project, "rex_cast_narrow_false"));
  SgExpression *narrowTrue =
      returnExpression(findFunction(project, "rex_cast_narrow_true"));
  ROSE_ASSERT(SageInterface::isConstantFalse(narrowFalse));
  ROSE_ASSERT(!SageInterface::isConstantTrue(narrowFalse));
  ROSE_ASSERT(SageInterface::isConstantTrue(narrowTrue));
  ROSE_ASSERT(!SageInterface::isConstantFalse(narrowTrue));
  SgCastExp *narrowIntegral = requireNestedConversion(
      findFunction(project, "rex_cast_narrow_false"), SgCastExp::e_static_cast,
      SgCastExp::e_semantic_conversion_IntegralCast);
  ROSE_ASSERT(narrowIntegral->cast_loses_precision());

  SgFunctionDeclaration *cFunction =
      findFunction(project, "rex_cast_c_surface");
  SgFunctionDeclaration *staticFunction =
      findFunction(project, "rex_cast_static_surface");
  SgCastExp *cCast =
      findConversion(cFunction, SgCastExp::e_semantic_conversion_NoOp,
                     SgCastExp::e_C_style_cast);
  SgCastExp *staticCast =
      findConversion(staticFunction, SgCastExp::e_semantic_conversion_NoOp,
                     SgCastExp::e_static_cast);
  SgCastExp *cNumeric =
      requireNestedConversion(cFunction, SgCastExp::e_C_style_cast,
                              SgCastExp::e_semantic_conversion_IntegralCast);
  SgCastExp *staticNumeric =
      requireNestedConversion(staticFunction, SgCastExp::e_static_cast,
                              SgCastExp::e_semantic_conversion_IntegralCast);
  ROSE_ASSERT(!cNumeric->cast_loses_precision());
  ROSE_ASSERT(!staticNumeric->cast_loses_precision());
  ROSE_ASSERT(mangleExpression(cCast) != mangleExpression(staticCast));
  SgCastExp *floatingNarrow = requireNestedConversion(
      findFunction(project, "rex_cast_floating_narrow"),
      SgCastExp::e_static_cast, SgCastExp::e_semantic_conversion_FloatingCast);
  SgCastExp *floatingWiden = requireNestedConversion(
      findFunction(project, "rex_cast_floating_widen"),
      SgCastExp::e_static_cast, SgCastExp::e_semantic_conversion_FloatingCast);
  ROSE_ASSERT(floatingNarrow->cast_loses_precision());
  ROSE_ASSERT(!floatingWiden->cast_loses_precision());
  SgTemplateArgument *cArgument =
      SageBuilder::buildTemplateArgument(SageInterface::copyExpression(cCast));
  SgTemplateArgument *staticArgument = SageBuilder::buildTemplateArgument(
      SageInterface::copyExpression(staticCast));
  ROSE_ASSERT(
      !SageInterface::templateArgumentEquivalence(cArgument, staticArgument));
  return 0;
}

[[noreturn]] void rejectUnaryCastRebuild() {
  SgIntVal *value = SageBuilder::buildIntVal(1);
  AstInterfaceImpl implementation(value);
  AstInterface interface(&implementation);
  (void)interface.CreateUnaryOP(AstInterface::UOP_SEMANTIC_CONVERSION,
                                AstNodePtr(value));
  ROSE_ABORT();
}

[[noreturn]] void rejectSymbolicIntegralCast(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv, 2);
  SgCastExp *cast = requireNestedConversion(
      findFunction(project, "rex_cast_static_surface"),
      SgCastExp::e_static_cast, SgCastExp::e_semantic_conversion_IntegralCast);
  AstInterfaceImpl implementation(cast);
  AstInterface interface(&implementation);
  (void)SymbolicValGenerator::GetSymbolicVal(interface, AstNodePtr(cast));
  ROSE_ABORT();
}

[[noreturn]] void rejectLoopCastDiscard(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv, 2);
  SgCastExp *cast = requireNestedConversion(
      findFunction(project, "rex_cast_static_surface"),
      SgCastExp::e_static_cast, SgCastExp::e_semantic_conversion_IntegralCast);
  (void)AstInterface::SkipCasting(cast);
  ROSE_ABORT();
}

[[noreturn]] void rejectDependentCastTarget(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv, 2);
  SgCastExp *cast = findConversion(
      findFunction(project, "rex_cast_dependent_target"),
      SgCastExp::e_semantic_conversion_Dependent, SgCastExp::e_static_cast);
  std::vector<SgTemplateParameter *> parameters;
  std::vector<SgTemplateArgument *> arguments;
  (void)SageBuilder::instantiateNonrealRefExps(cast, parameters, arguments);
  ROSE_ABORT();
}

[[noreturn]] void rejectUnsupportedPrecisionPolicy(int argc, char **argv) {
  (void)argc;
  (void)argv;
  SgCastExp *cast = SageBuilder::buildCastExp(
      SageBuilder::buildLongIntVal(1),
      SageBuilder::buildPointerType(SageBuilder::buildVoidType()),
      SgCastExp::e_reinterpret_cast,
      SgCastExp::e_semantic_conversion_IntegralToPointer,
      SgCastExp::e_value_category_prvalue, {});
  (void)cast->cast_loses_precision();
  ROSE_ABORT();
}

} // namespace

int main(int argc, char **argv) {
  ROSE_ASSERT(argc >= 2);
  const std::string mode = argv[1];
  if (mode == "--reject-unary-cast-rebuild")
    rejectUnaryCastRebuild();
  if (mode == "--reject-symbolic-integral-cast")
    rejectSymbolicIntegralCast(argc, argv);
  if (mode == "--reject-loop-cast-discard")
    rejectLoopCastDiscard(argc, argv);
  if (mode == "--reject-dependent-cast-target")
    rejectDependentCastTarget(argc, argv);
  if (mode == "--reject-unsupported-precision-policy")
    rejectUnsupportedPrecisionPolicy(argc, argv);
  return exercisePositiveContracts(argc, argv);
}
