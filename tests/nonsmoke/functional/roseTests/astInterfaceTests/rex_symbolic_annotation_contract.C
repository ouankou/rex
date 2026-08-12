#include "rose.h"

#include "ArrayAnnot.h"
#include "AstInterface_ROSE.h"
#include "OperatorDescriptors.h"
#include "RoseAst.h"
#include "SymbolicVal.h"
#include "ValueAnnot.h"

#include <string>
#include <vector>

namespace {

SgProject *parseProject(int argc, char **argv) {
  std::vector<char *> arguments;
  arguments.push_back(argv[0]);
  for (int i = 1; i < argc; ++i)
    arguments.push_back(argv[i]);
  return frontend(static_cast<int>(arguments.size()), arguments.data());
}

SgFunctionDeclaration *findFunction(SgNode *root, const std::string &name) {
  RoseAst ast(root);
  for (RoseAst::iterator current = ast.begin(); current != ast.end();
       ++current) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(*current);
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

void requireFunction(const SymbolicVal &value,
                     AstInterface::OperatorEnum expectedOperation,
                     const std::string &expectedName, size_t expectedArity) {
  AstInterface::OperatorEnum operation = AstInterface::OP_NONE;
  std::string name;
  std::vector<SymbolicVal> arguments;
  ROSE_ASSERT(value.isFunction(&operation, &name, &arguments));
  ROSE_ASSERT(operation == expectedOperation);
  ROSE_ASSERT(name == expectedName);
  ROSE_ASSERT(arguments.size() == expectedArity);
}

void requireExactTarget(const SymbolicValDescriptor &target,
                        SgExpression *expected) {
  AstNodePtr exactTarget;
  ROSE_ASSERT(target.get_val().isAstWrap(exactTarget));
  ROSE_ASSERT(exactTarget.get_ptr() == expected);
}

SgExpression *
rebuildAnnotationArgument(AstInterface &interface, SgExpression *expression,
                          const std::string &parameterType = "int") {
  ParameterDeclaration parameters;
  parameters.add_param(parameterType, "argument");
  AstInterface::AstNodeList arguments{AstNodePtr(expression)};
  ReplaceParams replacements(interface, parameters, arguments);
  SgExpression *rebuilt = isSgExpression(
      replacements.find("argument").CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuilt != nullptr);
  ROSE_ASSERT(rebuilt->get_type() != nullptr);
  return rebuilt;
}

SymbolicVal replaceAnnotationArgument(AstInterface &interface,
                                      SgExpression *expression,
                                      const std::string &parameterType) {
  ParameterDeclaration parameters;
  parameters.add_param(parameterType, "argument");
  AstInterface::AstNodeList arguments{AstNodePtr(expression)};
  ReplaceParams replacements(interface, parameters, arguments);
  return replacements.find("argument");
}

template <class SageNode, class Cast>
SageNode *findUniqueExpressionNode(SgExpression *root, Cast cast) {
  SageNode *result = nullptr;
  RoseAst traversal(root);
  for (RoseAst::iterator node = traversal.begin(); node != traversal.end();
       ++node) {
    if (SageNode *match = cast(*node)) {
      ROSE_ASSERT(result == nullptr);
      result = match;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgExpression *stripSemanticConversions(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  while (SgCastExp *conversion = isSgCastExp(expression)) {
    conversion->validate_semantic_conversion();
    ROSE_ASSERT(conversion->get_operand() != nullptr);
    expression = conversion->get_operand();
  }
  return expression;
}

int exerciseContracts(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv);
  ROSE_ASSERT(project != nullptr);

  SgExpression *memberExpression =
      returnExpression(findFunction(project, "rex_symbolic_annotation_member"));
  AstInterfaceImpl implementation(memberExpression);
  AstInterface interface(&implementation);

  ParameterDeclaration parameters;
  parameters.add_param("rex_symbolic_record", "argument");
  AstInterface::AstNodeList actualArguments{AstNodePtr(memberExpression)};
  ReplaceParams replacements(interface, parameters, actualArguments);
  SymbolicVal member = replacements.find("argument");
  requireFunction(member, AstInterface::BOP_DOT_ACCESS, ".", 2);

  SgExpression *variableExpression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_variable"));
  ParameterDeclaration targetParameters;
  targetParameters.add_param("int", "target");
  AstInterface::AstNodeList targetArguments{AstNodePtr(variableExpression)};
  ReplaceParams targetReplacements(interface, targetParameters,
                                   targetArguments);
  SymbolicValDescriptor target(SymbolicVar("target", AST_NULL));
  targetReplacements.replace_target(target);
  requireExactTarget(target, variableExpression);

  RestrictValueOpDescriptor restriction;
  RestrictValueDescriptor restrictedTarget;
  restrictedTarget.first = SymbolicVar("target", AST_NULL);
  restriction.push_back(restrictedTarget);
  restriction.replace_val(targetReplacements);
  requireExactTarget(restriction.front().first, variableExpression);

  ArrayModifyDescriptor modification;
  modification.first.get_val() = SymbolicVar("target", AST_NULL);
  modification.replace_val(targetReplacements);
  requireExactTarget(modification.first, variableExpression);

  ArrayConstructDescriptor construction;
  construction.first.push_back(
      SymbolicValDescriptor(SymbolicVar("target", AST_NULL)));
  construction.replace_val(targetReplacements);
  requireExactTarget(construction.first.front(), variableExpression);

  SgExpression *addressExpression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_address"));
  SymbolicVal address = SymbolicValGenerator::GetSymbolicVal(
      interface, AstNodePtr(addressExpression));
  std::string variableName;
  ROSE_ASSERT(address.isVar(variableName));
  ROSE_ASSERT(variableName == "pointer");

  SgExpression *conversionExpression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_conversion"));
  SymbolicVal conversion = SymbolicValGenerator::GetSymbolicVal(
      interface, AstNodePtr(conversionExpression));
  AstInterface::OperatorEnum conversionOperation = AstInterface::OP_NONE;
  std::vector<SymbolicVal> conversionArguments;
  ROSE_ASSERT(conversion.isFunction(&conversionOperation, nullptr,
                                    &conversionArguments));
  ROSE_ASSERT(conversionOperation == AstInterface::UOP_SEMANTIC_CONVERSION);
  ROSE_ASSERT(conversionArguments.size() == 1);
  SgCastExp *rebuiltConversion =
      isSgCastExp(conversion.CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuiltConversion != nullptr);
  rebuiltConversion->validate_semantic_conversion();
  ROSE_ASSERT(rebuiltConversion->get_semantic_conversion_kind() ==
              SgCastExp::e_semantic_conversion_IntegralToFloating);

  SymbolicFunction named(AstInterface::OP_NONE, "rex_symbolic_helper",
                         SymbolicVal(1));
  SymbolicVal namedClone = named.cloneFunction({SymbolicVal(2)});
  requireFunction(namedClone, AstInterface::OP_NONE, "rex_symbolic_helper", 1);

  SymbolicFunction modulo(AstInterface::BOP_MOD, "%", SymbolicVal(5),
                          SymbolicVal(2));
  SymbolicVal moduloClone =
      modulo.cloneFunction({SymbolicVal(9), SymbolicVal(4)});
  requireFunction(moduloClone, AstInterface::BOP_MOD, "%", 2);
  requireFunction(SymbolicValGenerator::GetSymbolicVal(
                      AstInterface::UOP_BIT_COMPLEMENT, {SymbolicVal(1)}),
                  AstInterface::UOP_BIT_COMPLEMENT, "~", 1);

  ROSE_ASSERT(isSgModOp(rebuildAnnotationArgument(
                  interface,
                  returnExpression(findFunction(
                      project, "rex_symbolic_annotation_modulo")))) != nullptr);
  ROSE_ASSERT(
      isSgBitAndOp(rebuildAnnotationArgument(
          interface, returnExpression(findFunction(
                         project, "rex_symbolic_annotation_bit_and")))) !=
      nullptr);
  ROSE_ASSERT(isSgBitOrOp(rebuildAnnotationArgument(
                  interface,
                  returnExpression(findFunction(
                      project, "rex_symbolic_annotation_bit_or")))) != nullptr);
  ROSE_ASSERT(isSgBitComplementOp(rebuildAnnotationArgument(
                  interface,
                  returnExpression(findFunction(
                      project, "rex_symbolic_annotation_bit_complement")))) !=
              nullptr);
  SgPlusPlusOp *prefixIncrement = findUniqueExpressionNode<SgPlusPlusOp>(
      rebuildAnnotationArgument(
          interface, returnExpression(findFunction(
                         project, "rex_symbolic_annotation_prefix_increment"))),
      [](SgNode *node) { return isSgPlusPlusOp(node); });
  ROSE_ASSERT(prefixIncrement->get_mode() == SgUnaryOp::Sgop_mode::prefix);
  SgMinusMinusOp *postfixDecrement = findUniqueExpressionNode<SgMinusMinusOp>(
      rebuildAnnotationArgument(
          interface,
          returnExpression(findFunction(
              project, "rex_symbolic_annotation_postfix_decrement"))),
      [](SgNode *node) { return isSgMinusMinusOp(node); });
  ROSE_ASSERT(postfixDecrement->get_mode() == SgUnaryOp::Sgop_mode::postfix);
  ROSE_ASSERT(
      isSgNewExp(rebuildAnnotationArgument(
          interface, returnExpression(findFunction(
                         project, "rex_symbolic_annotation_allocation")))) !=
      nullptr);
  SgExpression *observableZeroProduct = rebuildAnnotationArgument(
      interface,
      returnExpression(findFunction(
          project, "rex_symbolic_annotation_observable_zero_product")));
  (void)findUniqueExpressionNode<SgMultiplyOp>(
      observableZeroProduct, [](SgNode *node) { return isSgMultiplyOp(node); });
  SgPlusPlusOp *postfixIncrement = findUniqueExpressionNode<SgPlusPlusOp>(
      observableZeroProduct, [](SgNode *node) { return isSgPlusPlusOp(node); });
  ROSE_ASSERT(postfixIncrement->get_mode() == SgUnaryOp::Sgop_mode::postfix);
  SgExpression *volatileZeroProduct = rebuildAnnotationArgument(
      interface,
      returnExpression(findFunction(
          project, "rex_symbolic_annotation_volatile_zero_product")));
  (void)findUniqueExpressionNode<SgMultiplyOp>(
      volatileZeroProduct, [](SgNode *node) { return isSgMultiplyOp(node); });
  SgPointerDerefExp *volatileRead = findUniqueExpressionNode<SgPointerDerefExp>(
      volatileZeroProduct,
      [](SgNode *node) { return isSgPointerDerefExp(node); });
  ROSE_ASSERT(SageInterface::isVolatileType(volatileRead->get_type()));
  return 0;
}

int checkUnsignedMinus(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv);
  ROSE_ASSERT(project != nullptr);
  SgExpression *expression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_unsigned_minus"));
  AstInterfaceImpl implementation(expression);
  AstInterface interface(&implementation);
  SymbolicVal replacement =
      replaceAnnotationArgument(interface, expression, "unsigned");
  requireFunction(replacement, AstInterface::UOP_MINUS, "-", 1);
  SgMinusOp *rebuilt = isSgMinusOp(replacement.CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuilt != nullptr);
  ROSE_ASSERT(SageInterface::isEquivalentType(rebuilt->get_type(),
                                              expression->get_type()));
  return 0;
}

int checkFloatingGrouping(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv);
  ROSE_ASSERT(project != nullptr);
  SgExpression *expression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_floating_group"));
  AstInterfaceImpl implementation(expression);
  AstInterface interface(&implementation);
  SymbolicVal replacement =
      replaceAnnotationArgument(interface, expression, "double");
  AstNodePtr exactExpression;
  ROSE_ASSERT(replacement.isAstWrap(exactExpression));
  ROSE_ASSERT(exactExpression.get_ptr() == expression);
  SgAddOp *rebuilt = isSgAddOp(replacement.CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuilt != nullptr);
  ROSE_ASSERT(isSgAddOp(rebuilt->get_rhs_operand()) != nullptr);
  ROSE_ASSERT(isSgAddOp(rebuilt->get_lhs_operand()) == nullptr);
  ROSE_ASSERT(SageInterface::isEquivalentType(rebuilt->get_type(),
                                              expression->get_type()));
  return 0;
}

int checkArrayAccess(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv);
  ROSE_ASSERT(project != nullptr);
  SgExpression *expression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_array_access"));
  AstInterfaceImpl implementation(expression);
  AstInterface interface(&implementation);
  SymbolicVal replacement =
      replaceAnnotationArgument(interface, expression, "int");
  requireFunction(replacement, AstInterface::OP_ARRAY_ACCESS, "[]", 2);
  SgPntrArrRefExp *rebuiltOuter =
      isSgPntrArrRefExp(replacement.CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuiltOuter != nullptr);
  SgPntrArrRefExp *rebuiltInner = isSgPntrArrRefExp(
      stripSemanticConversions(rebuiltOuter->get_lhs_operand()));
  SgVarRefExp *array = rebuiltInner == nullptr
                           ? nullptr
                           : isSgVarRefExp(stripSemanticConversions(
                                 rebuiltInner->get_lhs_operand()));
  SgVarRefExp *row = rebuiltInner == nullptr
                         ? nullptr
                         : isSgVarRefExp(stripSemanticConversions(
                               rebuiltInner->get_rhs_operand()));
  SgVarRefExp *column =
      isSgVarRefExp(stripSemanticConversions(rebuiltOuter->get_rhs_operand()));
  ROSE_ASSERT(array != nullptr && row != nullptr && column != nullptr);
  ROSE_ASSERT(array->get_symbol()->get_name() == "array");
  ROSE_ASSERT(row->get_symbol()->get_name() == "row");
  ROSE_ASSERT(column->get_symbol()->get_name() == "column");
  ROSE_ASSERT(SageInterface::isEquivalentType(rebuiltOuter->get_type(),
                                              expression->get_type()));
  return 0;
}

int checkComplexFloatingGrouping(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv);
  ROSE_ASSERT(project != nullptr);
  SgExpression *expression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_complex_floating_group"));
  AstInterfaceImpl implementation(expression);
  AstInterface interface(&implementation);
  SymbolicVal replacement =
      replaceAnnotationArgument(interface, expression, "_Complex double");
  AstNodePtr exactExpression;
  ROSE_ASSERT(replacement.isAstWrap(exactExpression));
  ROSE_ASSERT(exactExpression.get_ptr() == expression);
  SgAddOp *rebuilt = isSgAddOp(replacement.CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuilt != nullptr);
  ROSE_ASSERT(isSgAddOp(rebuilt->get_rhs_operand()) != nullptr);
  ROSE_ASSERT(isSgAddOp(rebuilt->get_lhs_operand()) == nullptr);
  ROSE_ASSERT(isSgTypeComplex(
                  rebuilt->get_type()->stripTypedefsAndModifiers()) != nullptr);
  ROSE_ASSERT(SageInterface::isEquivalentType(rebuilt->get_type(),
                                              expression->get_type()));
  return 0;
}

[[noreturn]] void rejectOperatorArity() {
  (void)SymbolicValGenerator::GetSymbolicVal(AstInterface::BOP_PLUS,
                                             {SymbolicVal(1)});
  ROSE_ABORT();
}

[[noreturn]] void rejectReplacementArity() {
  AstInterface interface(nullptr);
  ParameterDeclaration parameters;
  parameters.add_param("int", "argument");
  AstInterface::AstNodeList arguments;
  (void)ReplaceParams(interface, parameters, arguments);
  ROSE_ABORT();
}

[[noreturn]] void rejectUntypedAllocation() {
  (void)SymbolicValGenerator::GetSymbolicVal(AstInterface::UOP_ALLOCATE,
                                             {SymbolicVal(1)});
  ROSE_ABORT();
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2 && std::string(argv[1]) == "--reject-operator-arity")
    rejectOperatorArity();
  if (argc == 2 && std::string(argv[1]) == "--reject-replacement-arity")
    rejectReplacementArity();
  if (argc == 2 && std::string(argv[1]) == "--reject-untyped-allocation")
    rejectUntypedAllocation();
  if (argc >= 2 && std::string(argv[1]) == "--check-unsigned-minus")
    return checkUnsignedMinus(argc - 1, argv + 1);
  if (argc >= 2 && std::string(argv[1]) == "--check-floating-grouping")
    return checkFloatingGrouping(argc - 1, argv + 1);
  if (argc >= 2 && std::string(argv[1]) == "--check-array-access")
    return checkArrayAccess(argc - 1, argv + 1);
  if (argc >= 2 && std::string(argv[1]) == "--check-complex-floating-grouping")
    return checkComplexFloatingGrouping(argc - 1, argv + 1);
  return exerciseContracts(argc, argv);
}
