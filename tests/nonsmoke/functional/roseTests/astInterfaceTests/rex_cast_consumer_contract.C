#include "rose.h"

#include "AstInterface_ROSE.h"
#include "RoseAst.h"
#include "SymbolicVal.h"
#include "manglingSupport.h"

#include <limits>
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

void requireFunctionReferenceIdentity() {
  auto *project = new SgProject();
  project->get_originalCommandLineArgumentList() = {"c++", "-c"};
  auto makeFunction = [&](const char *filename) {
    SgSourceFile *source = SageBuilder::buildGeneratedSourceFile(filename, project);
    return SageBuilder::buildNondefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexical(),
        SgName("rex_reference"), SageBuilder::buildVoidType(),
        SageBuilder::buildFunctionParameterList(), source->get_globalScope(),
        false, nullptr, SgStorageModifier::e_static);
  };
  SgFunctionDeclaration *first = makeFunction("rex_reference_first.cpp");
  SgFunctionDeclaration *second = makeFunction("rex_reference_second.cpp");
  ROSE_ASSERT(first->get_qualified_name() == second->get_qualified_name());
  ROSE_ASSERT(first->get_type() == second->get_type());
  SgCastExp *cast = SageBuilder::buildCastExp(
      SageBuilder::buildFunctionRefExp(first),
      SageBuilder::buildAddressOfResultType(first->get_type()));
  SgExpression *cast_copy = SageInterface::copyExpression(cast);
  ROSE_ASSERT(cast_copy != cast && cast_copy->get_type() == cast->get_type());
  SageInterface::deleteAST({cast, cast_copy},
                           SageInterface::DeleteAstMode::kRequireIsolated);
  SgTemplateArgument *firstArgument = SageBuilder::buildTemplateArgument(
      SageBuilder::buildFunctionRefExp(first));
  SgTemplateArgument *sameArgument = SageBuilder::buildTemplateArgument(
      SageBuilder::buildFunctionRefExp(first));
  SgTemplateArgument *secondArgument = SageBuilder::buildTemplateArgument(
      SageBuilder::buildFunctionRefExp(second));
  SgTemplateArgument *addressArgument =
      SageBuilder::buildTemplateArgument(SageBuilder::buildAddressOfOp(
          SageBuilder::buildFunctionRefExp(first),
          SageBuilder::buildAddressOfResultType(first->get_type()),
          SgAddressOfOp::e_address_operand_object));
  SgTemplateArgument *qualifiedAddress =
      SageBuilder::buildTemplateArgument(SageBuilder::buildAddressOfOp(
          SageBuilder::buildFunctionRefExp(first),
          SageBuilder::buildAddressOfResultType(first->get_type()),
          SgAddressOfOp::e_address_operand_qualified_id));
  SgTemplateArgument *secondAddress =
      SageBuilder::buildTemplateArgument(SageBuilder::buildAddressOfOp(
          SageBuilder::buildFunctionRefExp(second),
          SageBuilder::buildAddressOfResultType(second->get_type()),
          SgAddressOfOp::e_address_operand_object));
  ROSE_ASSERT(
      SageInterface::templateArgumentEquivalence(firstArgument, sameArgument));
  ROSE_ASSERT(!SageInterface::templateArgumentEquivalence(firstArgument,
                                                          secondArgument));
  ROSE_ASSERT(!SageInterface::templateArgumentEquivalence(secondArgument,
                                                          firstArgument));
  ROSE_ASSERT(SageInterface::cxxSourceTemplateArgumentPrefixMatchesSemantic(
      {firstArgument}, {addressArgument}));
  ROSE_ASSERT(!SageInterface::cxxSourceTemplateArgumentPrefixMatchesSemantic(
      {secondArgument}, {addressArgument}));
  ROSE_ASSERT(SageInterface::templateArgumentEquivalence(addressArgument,
                                                         qualifiedAddress));
  ROSE_ASSERT(!SageInterface::templateArgumentEquivalence(addressArgument,
                                                          secondAddress));
  ROSE_ASSERT(!SageInterface::templateArgumentEquivalence(secondAddress,
                                                          addressArgument));
  for (SgTemplateArgument *argument :
       {firstArgument, sameArgument, secondArgument, addressArgument,
        qualifiedAddress, secondAddress})
    SageInterface::deleteAST(argument);
  SageInterface::deleteAST(project);
}

int exercisePositiveContracts(int argc, char **argv, bool llp64 = false,
                              bool msp430 = false) {
  SgProject *project = parseProject(argc, argv, llp64 || msp430 ? 2 : 1);
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
  auto *longToInt = requireNestedConversion(
      findFunction(project, "rex_cast_long_to_int"), SgCastExp::e_static_cast,
      SgCastExp::e_semantic_conversion_IntegralCast);
  auto *sourceFile = SageInterface::getEnclosingSourceFile(longToInt);
  ROSE_ASSERT(sourceFile != nullptr && sourceFile->get_target_abi().available);
  for (const char *name :
       {"rex_sizeof_complex_float", "rex_sizeof_complex_integer"}) {
    auto *function = findFunction(project, name);
    const auto sizes = NodeQuery::querySubTree(function, V_SgSizeOfOp);
    ROSE_ASSERT(sizes.size() == 1);
    auto *size = isSgSizeOfOp(sizes.front());
    const auto &value = size->get_evaluated_value();
    ROSE_ASSERT(value && !value->isNegative() && value->words()[1] == 0);
    const unsigned char_bits = SageInterface::requireTargetScalarSizeBits(
        SageBuilder::buildCharType(), function);
    ROSE_ASSERT(SageInterface::requireTargetScalarSizeBits(
                    size->get_operand_type(), function) ==
                value->words()[0] * char_bits);
  }
  for (const char *name :
       {"rex_promote_bool", "rex_promote_char", "rex_promote_unsigned_char",
        "rex_promote_short", "rex_promote_unsigned_short", "rex_promote_wchar",
        "rex_promote_char16", "rex_promote_char32", "rex_promote_small_enum",
        "rex_promote_wide_enum", "rex_promote_fixed_enum",
        "rex_promote_opaque_enum"}) {
    auto *function = findFunction(project, name);
    const auto &parameters = function->get_parameterList()->get_args();
    ROSE_ASSERT(parameters.size() == 1);
    SgType *operand = parameters.front()->get_type();
    SgType *expected = returnExpression(function)->get_type();
    ROSE_ASSERT(SageInterface::isEquivalentType(
        SageInterface::usualArithmeticConversionType(operand, operand,
                                                     function),
        expected));
    ROSE_ASSERT(SageInterface::isEquivalentType(
        SageInterface::usualArithmeticConversionType(
            operand, SageBuilder::buildIntType(), function),
        expected));
    auto *declared = SageBuilder::buildDeclType(
        SageBuilder::buildVarRefExp(parameters.front()), operand);
    ROSE_ASSERT(SageInterface::isEquivalentType(
        SageInterface::usualArithmeticConversionType(
            declared, SageBuilder::buildIntType(), function),
        expected));
    SageInterface::deleteAST(declared,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }
  const auto &layouts = sourceFile->get_target_abi().scalars;
  const auto longLayout = layouts.find({V_SgTypeLong, 0});
  const auto intLayout = layouts.find({V_SgTypeInt, 0});
  ROSE_ASSERT(longLayout != layouts.end() && intLayout != layouts.end());
  ROSE_ASSERT(longToInt->cast_loses_precision() ==
              (intLayout->second.size_bits < longLayout->second.size_bits));
  if (llp64)
    ROSE_ASSERT(longLayout->second.size_bits == 32 &&
                intLayout->second.size_bits == 32 &&
                !longToInt->cast_loses_precision());
  if (msp430)
    ROSE_ASSERT(intLayout->second.size_bits == 16 &&
                longLayout->second.size_bits == 32 &&
                sourceFile->get_target_abi().pointer_size_bits == 16);
  ROSE_ASSERT(mangleExpression(cCast) != mangleExpression(staticCast));
  SgCastExp *floatingNarrow = requireNestedConversion(
      findFunction(project, "rex_cast_floating_narrow"),
      SgCastExp::e_static_cast, SgCastExp::e_semantic_conversion_FloatingCast);
  SgCastExp *floatingWiden = requireNestedConversion(
      findFunction(project, "rex_cast_floating_widen"),
      SgCastExp::e_static_cast, SgCastExp::e_semantic_conversion_FloatingCast);
  ROSE_ASSERT(floatingNarrow->cast_loses_precision());
  ROSE_ASSERT(!floatingWiden->cast_loses_precision());
  auto *longDoubleToDouble = requireNestedConversion(
      findFunction(project, "rex_cast_long_double_to_double"),
      SgCastExp::e_static_cast, SgCastExp::e_semantic_conversion_FloatingCast);
  const auto longDoubleLayout = layouts.find({V_SgTypeLongDouble, 0});
  const auto doubleLayout = layouts.find({V_SgTypeDouble, 0});
  ROSE_ASSERT(longDoubleLayout != layouts.end() &&
              doubleLayout != layouts.end());
  ROSE_ASSERT(
      longDoubleToDouble->cast_loses_precision() ==
      (doubleLayout->second.precision < longDoubleLayout->second.precision ||
       doubleLayout->second.max_exponent <
           longDoubleLayout->second.max_exponent));
  if (llp64)
    ROSE_ASSERT(longDoubleLayout->second.precision == 53 &&
                doubleLayout->second.precision == 53 &&
                !longDoubleToDouble->cast_loses_precision());
  SgTemplateArgument *cArgument =
      SageBuilder::buildTemplateArgument(SageInterface::copyExpression(cCast));
  SgTemplateArgument *staticArgument = SageBuilder::buildTemplateArgument(
      SageInterface::copyExpression(staticCast));
  ROSE_ASSERT(
      !SageInterface::templateArgumentEquivalence(cArgument, staticArgument));
  SgTemplateArgument *integer =
      SageBuilder::buildTemplateArgument(SageBuilder::buildIntVal(1));
  SgTemplateArgument *sameInteger =
      SageBuilder::buildTemplateArgument(SageBuilder::buildIntVal(1));
  SgTemplateArgument *boolean =
      SageBuilder::buildTemplateArgument(SageBuilder::buildBoolValExp(true));
  ROSE_ASSERT(SageInterface::templateArgumentEquivalence(integer, sameInteger));
  ROSE_ASSERT(!SageInterface::templateArgumentEquivalence(integer, boolean));
  ROSE_ASSERT(!SageInterface::templateArgumentEquivalence(boolean, integer));
  SgTemplateArgument *floating =
      SageBuilder::buildTemplateArgument(SageBuilder::buildDoubleVal(1.0));
  SgTemplateArgument *longInteger =
      SageBuilder::buildTemplateArgument(SageBuilder::buildLongIntVal(1));
  ROSE_ASSERT(!SageInterface::cxxSourceTemplateArgumentPrefixMatchesSemantic(
      {floating}, {integer}));
  ROSE_ASSERT(SageInterface::cxxSourceTemplateArgumentPrefixMatchesSemantic(
      {integer}, {longInteger}));
  auto compareIntegerSource = [](SgValueExp *source, SgValueExp *semantic,
                                 bool equal) {
    SgTemplateArgument *sourceArgument =
        SageBuilder::buildTemplateArgument(source);
    SgTemplateArgument *semanticArgument =
        SageBuilder::buildTemplateArgument(semantic);
    ROSE_ASSERT(SageInterface::cxxSourceTemplateArgumentPrefixMatchesSemantic(
                    {sourceArgument}, {semanticArgument}) == equal);
    SageInterface::deleteAST(sourceArgument);
    SageInterface::deleteAST(semanticArgument);
  };
  compareIntegerSource(SageBuilder::buildCharVal('A'),
                       SageBuilder::buildIntVal(65), true);
  compareIntegerSource(SageBuilder::buildChar16Val(65),
                       SageBuilder::buildIntVal(65), true);
  SgIntVal *negative = SageBuilder::buildIntVal(-1);
  ROSE_ASSERT(SageInterface::getIntegerConstantValue(negative) == ~0ULL);
  compareIntegerSource(negative,
                       SageBuilder::buildUnsignedLongLongIntVal(~0ULL), false);
  auto *enumValue =
      isSgEnumVal(returnExpression(findFunction(project, "rex_enum_identity")));
  ROSE_ASSERT(enumValue != nullptr);
  ROSE_ASSERT(SageInterface::getIntegerConstantValue(enumValue) == ~0ULL);
  auto *semanticEnum = SageBuilder::buildUnsignedLongLongIntVal(~0ULL);
  semanticEnum->set_literal_type(enumValue->get_type());
  compareIntegerSource(isSgValueExp(SageInterface::copyExpression(enumValue)),
                       semanticEnum, true);
  auto compareValues = [](auto build, auto left, auto right, bool equal) {
    SgTemplateArgument *leftArgument =
        SageBuilder::buildTemplateArgument(build(left));
    SgTemplateArgument *rightArgument =
        SageBuilder::buildTemplateArgument(build(right));
    ROSE_ASSERT(SageInterface::templateArgumentEquivalence(
                    leftArgument, rightArgument) == equal);
    ROSE_ASSERT(SageInterface::templateArgumentEquivalence(
                    rightArgument, leftArgument) == equal);
    ROSE_ASSERT(SageInterface::cxxSourceTemplateArgumentPrefixMatchesSemantic(
                    {leftArgument}, {rightArgument}) == equal);
    ROSE_ASSERT(SageInterface::cxxSourceTemplateArgumentPrefixMatchesSemantic(
                    {rightArgument}, {leftArgument}) == equal);
    SageInterface::deleteAST(leftArgument);
    SageInterface::deleteAST(rightArgument);
  };
  auto macroValue = [](int value) {
    auto *literal = SageBuilder::buildIntVal(value);
    auto *macro = new SgMacroExpansionExp("REX_VALUE", literal);
    literal->set_parent(macro);
    SageInterface::setOneSourcePositionNull(macro);
    return macro;
  };
  auto expressionValue = [](SgExpression *expression) { return expression; };
  const SgTemplateParameter *variableParameter = nullptr;
  size_t variableDeclarations = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateVariableDeclaration)) {
    const auto *declaration = isSgTemplateVariableDeclaration(node);
    const auto &names = declaration->get_variables();
    if (names.size() != 1 ||
        names.front()->get_name() != "rex_identity_variable")
      continue;
    if (declaration->get_specializedTemplateDeclaration() != nullptr) {
      ROSE_ASSERT(declaration->get_templateParameters().empty());
      ROSE_ASSERT(declaration->get_templateSpecializationArguments().size() ==
                  1);
      continue;
    }
    ROSE_ASSERT(declaration->get_templateParameters().size() == 1);
    const auto *canonical = declaration->get_templateParameters()
                                .front()
                                ->get_canonicalNonTypeParameter();
    if (variableParameter == nullptr)
      variableParameter = canonical;
    ROSE_ASSERT(variableParameter == canonical);
    ++variableDeclarations;
  }
  ROSE_ASSERT(variableDeclarations >= 2);
  const auto firstParameters = NodeQuery::querySubTree(
      findFunction(project, "rex_identity_function")->get_definition(),
      V_SgTemplateParameterVal);
  const auto secondParameters = NodeQuery::querySubTree(
      findFunction(project, "rex_distinct_identity_function")->get_definition(),
      V_SgTemplateParameterVal);
  ROSE_ASSERT(firstParameters.size() == 1 && secondParameters.size() == 1);
  auto *firstParameter = isSgTemplateParameterVal(firstParameters.front());
  auto *secondParameter = isSgTemplateParameterVal(secondParameters.front());
  auto *firstDeclaration = isSgTemplateFunctionDeclaration(
      findFunction(project, "rex_identity_function")
          ->get_firstNondefiningDeclaration());
  ASSERT_not_null(firstDeclaration);
  ROSE_ASSERT(firstDeclaration->get_templateParameters().size() == 1);
  ROSE_ASSERT(firstParameter->get_template_parameter()
                  ->get_canonicalNonTypeParameter() ==
              firstDeclaration->get_templateParameters()
                  .front()
                  ->get_canonicalNonTypeParameter());
  ROSE_ASSERT(firstParameter->get_template_parameter() !=
              secondParameter->get_template_parameter());
  compareValues(expressionValue, SageInterface::copyExpression(firstParameter),
                SageInterface::copyExpression(firstParameter), true);
  compareValues(expressionValue, SageInterface::copyExpression(firstParameter),
                SageInterface::copyExpression(secondParameter), false);
  auto *semanticParameter = firstParameter->get_template_parameter();
  auto *sourceName = SageBuilder::buildInitializedName(
      "renamed_parameter", semanticParameter->get_type());
  sourceName->set_scope(semanticParameter->get_initializedName()->get_scope());
  auto *sourceParameter = SageBuilder::buildTemplateParameter(
      SgTemplateParameter::nontype_parameter, semanticParameter->get_type(),
      SgTemplateParameter::keyword_unspecified, sourceName,
      semanticParameter->get_canonicalNonTypeParameter());
  compareValues(
      expressionValue, SageInterface::copyExpression(firstParameter),
      SageBuilder::buildTemplateParameterVal_nfi(
          firstParameter->get_template_parameter_position(), sourceParameter),
      true);
  ROSE_ASSERT(SageInterface::templateParameterListEquivalence(
      {semanticParameter}, {secondParameter->get_template_parameter()}));
  SageInterface::deleteAST(sourceParameter);
  for (const char *name :
       {"rex_identity_function_reference", "rex_identity_variable_reference"}) {
    const auto references = NodeQuery::querySubTree(
        findFunction(project, name)->get_definition(), V_SgNonrealRefExp);
    ROSE_ASSERT(references.size() == 1);
    auto *reference = isSgNonrealRefExp(references.front());
    SgExpression *semantic = nullptr;
    if (reference->get_resolved_function_declaration() != nullptr) {
      semantic = SageBuilder::buildFunctionRefExp(
          SageInterface::requireResolvedFunctionTemplateReference(
              reference, "identity contract"));
      compareValues(expressionValue, SageInterface::copyExpression(reference),
                    SageBuilder::buildFunctionRefExp(
                        findFunction(project, "rex_cast_identity")),
                    false);
    } else {
      semantic = SageBuilder::buildVarRefExp(
          SageInterface::requireResolvedVariableTemplateReference(
              reference, "identity contract"));
    }
    compareValues(expressionValue, SageInterface::copyExpression(reference),
                  semantic, true);
  }
  compareValues(expressionValue, macroValue(1), SageBuilder::buildIntVal(1),
                true);
  compareValues(expressionValue, macroValue(1), SageBuilder::buildIntVal(2),
                false);
  compareValues(expressionValue, macroValue(1), macroValue(2), false);
  compareValues(SageBuilder::buildFloatVal, 1.0f, 1.0f, true);
  compareValues(SageBuilder::buildFloatVal, 1.0f, 0x1.000002p0f, false);
  compareValues(SageBuilder::buildDoubleVal, 1.0, 1.0, true);
  compareValues(SageBuilder::buildDoubleVal, 1.0, 0x1.0000000000001p0, false);
  compareValues(SageBuilder::buildDoubleVal, 0.0, -0.0, false);
  auto buildSum = [](double value) {
    return SageBuilder::buildAddOp(SageBuilder::buildDoubleVal(value),
                                   SageBuilder::buildDoubleVal(0.0),
                                   SageBuilder::buildDoubleType());
  };
  compareValues(buildSum, 1.0, 1.0, true);
  compareValues(buildSum, 1.0, 0x1.0000000000001p0, false);
  auto buildInitializer = [](double value) {
    return SageBuilder::buildAssignInitializer(
        SageBuilder::buildDoubleVal(value), SageBuilder::buildDoubleType());
  };
  compareValues(buildInitializer, 1.0, 1.0, true);
  compareValues(buildInitializer, 1.0, 0x1.0000000000001p0, false);
  auto buildConstructor = [](double value) {
    return SageBuilder::buildConstructorInitializer_nfi(
        nullptr,
        SageBuilder::buildExprListExp(SageBuilder::buildDoubleVal(value)),
        SageBuilder::buildDoubleType(), true, false, true, true);
  };
  compareValues(buildConstructor, 1.0, 1.0, true);
  compareValues(buildConstructor, 1.0, 0x1.0000000000001p0, false);
  auto buildNew = [&](double value) {
    auto *type = SageBuilder::buildDoubleType();
    return SageBuilder::buildNewExp_nfi(type, SgPointerType::createType(type),
                                        nullptr, SgNewExp::e_no_array_bound,
                                        nullptr, nullptr,
                                        buildInitializer(value), 0, nullptr, 1);
  };
  compareValues(buildNew, 1.0, 1.0, true);
  compareValues(buildNew, 1.0, 0x1.0000000000001p0, false);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  compareValues(SageBuilder::buildDoubleVal, nan, nan, true);
  compareValues(SageBuilder::buildFloat16Val, 1.0f, 1.0f, true);
  compareValues(SageBuilder::buildBFloat16Val, 1.0f, 1.0f, true);
  using Value = Rose::FloatingValue;
  const auto extended = Value::fromBits(Value::Format::x87_binary80,
                                        {0x8000000000000000ULL, 0x3fff});
  compareValues(SageBuilder::buildFloat80Val, extended, extended, true);
  compareValues(SageBuilder::buildLongDoubleVal, extended, extended, true);
  auto compareQuad = [&](Value::Words left, Value::Words right, bool equal) {
    compareValues(SageBuilder::buildFloat128Val,
                  Value::fromBits(Value::Format::binary128, left),
                  Value::fromBits(Value::Format::binary128, right), equal);
  };
  compareQuad({0, 0x3fff000000000000ULL}, {0, 0x3fff000000000000ULL}, true);
  compareQuad({0, 0x3fff000000000000ULL}, {1, 0x3fff000000000000ULL}, false);
  compareQuad({0, 0}, {0, 0x8000000000000000ULL}, false);
  compareQuad({1, 0x7fff800000000000ULL}, {1, 0x7fff800000000000ULL}, true);
  compareQuad({1, 0x7fff800000000000ULL}, {2, 0x7fff800000000000ULL}, false);
  auto buildInteger128 = [](Rose::Integer128Value value) {
    auto *literal = new SgInteger128Val(value);
    SageInterface::setOneSourcePositionNull(literal);
    return literal;
  };
  const auto wide = Rose::Integer128Value::fromBits({0, 1}, true);
  compareValues(buildInteger128, wide, wide, true);
  compareValues(buildInteger128, wide,
                Rose::Integer128Value::fromBits({1, 1}, true), false);
  compareIntegerSource(
      SageBuilder::buildIntVal(1),
      buildInteger128(Rose::Integer128Value::fromBits({1, 0}, true)), true);
  for (SgTemplateArgument *argument :
       {cArgument, staticArgument, integer, sameInteger, boolean, floating,
        longInteger})
    SageInterface::deleteAST(argument);
  requireFunctionReferenceIdentity();
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

int preserveSymbolicIntegralCast(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv, 2);
  SgCastExp *cast = requireNestedConversion(
      findFunction(project, "rex_cast_static_surface"),
      SgCastExp::e_static_cast, SgCastExp::e_semantic_conversion_IntegralCast);
  AstInterfaceImpl implementation(cast);
  AstInterface interface(&implementation);
  SymbolicVal converted =
      SymbolicValGenerator::GetSymbolicVal(interface, AstNodePtr(cast));
  AstInterface::OperatorEnum operation = AstInterface::OP_NONE;
  std::vector<SymbolicVal> arguments;
  ROSE_ASSERT(converted.isFunction(&operation, nullptr, &arguments));
  ROSE_ASSERT(operation == AstInterface::UOP_SEMANTIC_CONVERSION);
  ROSE_ASSERT(arguments.size() == 1);

  SgCastExp *rebuilt = isSgCastExp(converted.CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuilt != nullptr);
  rebuilt->validate_semantic_conversion();
  ROSE_ASSERT(rebuilt->get_cast_type() == cast->get_cast_type());
  ROSE_ASSERT(rebuilt->get_semantic_conversion_kind() ==
              cast->get_semantic_conversion_kind());
  ROSE_ASSERT(rebuilt->get_value_category() == cast->get_value_category());
  ROSE_ASSERT(
      SageInterface::isEquivalentType(rebuilt->get_type(), cast->get_type()));
  return 0;
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
  SgType *target_type =
      SageBuilder::buildPointerType(SageBuilder::buildVoidType());
  SgCastExp *cast = SageBuilder::buildCastExp(
      SageBuilder::buildLongIntVal(1), target_type,
      SgCastExp::e_reinterpret_cast,
      SgCastExp::e_semantic_conversion_IntegralToPointer,
      SgCastExp::e_value_category_prvalue, {}, target_type);
  (void)cast->cast_loses_precision();
  ROSE_ABORT();
}

} // namespace

int main(int argc, char **argv) {
  ROSE_ASSERT(argc >= 2);
  const std::string mode = argv[1];
  if (mode == "--reject-new-owned-placement" ||
      mode == "--reject-new-owned-constructor" ||
      mode == "--reject-new-owned-builtin") {
    auto *type = SageBuilder::buildIntType();
    SgExprListExp *placement = nullptr;
    SgConstructorInitializer *constructor = nullptr;
    SgExpression *builtin = nullptr;
    if (mode == "--reject-new-owned-placement")
      placement = SageBuilder::buildExprListExp_nfi();
    else if (mode == "--reject-new-owned-constructor")
      constructor = SageBuilder::buildConstructorInitializer_nfi(
          nullptr, SageBuilder::buildExprListExp_nfi(), type, true, false, true,
          true);
    else
      builtin = SageBuilder::buildAssignInitializer_nfi(
          SageBuilder::buildIntVal(1), type);
    auto *result_type = SgPointerType::createType(type);
    (void)SageBuilder::buildNewExp_nfi(type, result_type, nullptr,
                                       SgNewExp::e_no_array_bound, placement,
                                       constructor, builtin, 0, nullptr, 1);
    (void)new SgNewExp(type, result_type, nullptr, SgNewExp::e_no_array_bound,
                       placement, constructor, builtin, 0, nullptr, 1);
    return 2;
  }
  if (mode == "--reject-braced-owned-list" ||
      mode == "--reject-braced-owned-list-direct") {
    auto *arguments = SageBuilder::buildExprListExp_nfi();
    auto *type = SageBuilder::buildIntType();
    (void)SageBuilder::buildBracedInitializer_nfi(arguments, type);
    if (mode == "--reject-braced-owned-list-direct")
      (void)new SgBracedInitializer(arguments, type);
    else
      (void)SageBuilder::buildBracedInitializer_nfi(arguments, type);
    return 2;
  }
  if (mode == "--reject-constructor-missing-arguments") {
    auto *arguments = SageBuilder::buildExprListExp_nfi();
    auto *initializer = SageBuilder::buildConstructorInitializer_nfi(
        nullptr, arguments, SageBuilder::buildIntType(), true, false, true,
        true);
    initializer->set_args(nullptr);
    (void)mangleExpression(initializer);
    return 2;
  }
  if (mode == "--reject-unary-cast-rebuild")
    rejectUnaryCastRebuild();
  if (mode == "--preserve-symbolic-integral-cast")
    return preserveSymbolicIntegralCast(argc, argv);
  if (mode == "--reject-loop-cast-discard")
    rejectLoopCastDiscard(argc, argv);
  if (mode == "--reject-dependent-cast-target")
    rejectDependentCastTarget(argc, argv);
  if (mode == "--reject-unsupported-precision-policy")
    rejectUnsupportedPrecisionPolicy(argc, argv);
  if (mode == "--reject-missing-target-abi") {
    auto *project = parseProject(argc, argv, 2);
    auto *cast = requireNestedConversion(
        findFunction(project, "rex_cast_long_to_int"), SgCastExp::e_static_cast,
        SgCastExp::e_semantic_conversion_IntegralCast);
    auto *source = SageInterface::getEnclosingSourceFile(cast);
    ASSERT_not_null(source);
    source->set_target_abi({});
    (void)cast->cast_loses_precision();
    return 2;
  }
  if (mode == "--check-llp64")
    return exercisePositiveContracts(argc, argv, true);
  if (mode == "--check-msp430")
    return exercisePositiveContracts(argc, argv, false, true);
  return exercisePositiveContracts(argc, argv);
}
