#include "CallGraph.h"
#include "rose.h"
#include <cstring>
#include <memory>

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  if (std::strcmp(argv[1], "duplicate_argument") == 0) {
    auto scope_storage = std::make_unique<SgGlobal>();
    auto &scope = *scope_storage;
    auto argument_storage = std::make_unique<SgTemplateArgument>(
        SageBuilder::buildIntType(), false);
    auto &argument = *argument_storage;
    SgTemplateArgumentPtrList arguments{&argument, &argument};
    (void)SageBuilder::buildSemanticNonrealType("Box", &scope, &arguments,
                                                nullptr);
    return 0;
  }
  if (std::strcmp(argv[1], "pack_marker") == 0) {
    auto primary_storage = std::make_unique<SgTemplateClassDeclaration>(
        "Box", SgClassDeclaration::e_class, nullptr, nullptr);
    auto &primary = *primary_storage;
    auto request_storage = std::make_unique<SgNonrealDecl>("Box", &primary);
    auto &request = *request_storage;
    auto marker = std::make_unique<SgTemplateArgument>(false);
    request.get_tpl_args().push_back(marker.get());
    marker->set_parent(&request);
    SgNonrealType type(&request);
    std::vector<SgTemplateParameter *> parameters;
    std::vector<SgTemplateArgument *> arguments;
    (void)Rose::Builder::Templates::instantiateNonrealTypes(&type, parameters,
                                                            arguments, nullptr);
    return 0;
  }
  if (std::strcmp(argv[1], "template_expression_cycle") == 0) {
    SgExprListExp *expression = SageBuilder::buildExprListExp();
    expression->get_expressions().push_back(expression);
    (void)SageInterface::templateExpressionEquivalence(expression, expression);
    return 0;
  }
  auto declaration = std::make_unique<SgNonrealDecl>("Dependent", nullptr);
  if (std::strcmp(argv[1], "missing") == 0) {
    auto base = std::make_unique<SgNonrealBaseClass>();
    auto derived = std::make_unique<SgClassDeclaration>(
        static_cast<Sg_File_Info *>(nullptr));
    auto definition = std::make_unique<SgClassDefinition>();
    definition->set_parent(derived.get());
    base->set_parent(definition.get());
    base->set_base_class_nonreal(declaration.get());
    declaration->set_parent(base.get());
    (void)declaration->get_mangled_name();
    return 0;
  }
  const bool cyclicOwners = std::strcmp(argv[1], "cycle") == 0;
  if (std::strcmp(argv[1], "valid") != 0 && !cyclicOwners)
    return 2;
  {
    auto *global = new SgGlobal;
    auto *scope = SageBuilder::buildDeclarationScope();
    SageBuilder::attachSemanticDeclarationScope(global, scope);
    SgTemplateArgumentPtrList arguments;
    auto *templateType =
        SageBuilder::buildSemanticNonrealType("Box", scope, nullptr);
    auto *templateDeclaration =
        isSgNonrealDecl(templateType->get_declaration());
    ROSE_ASSERT(templateDeclaration != nullptr);
    templateDeclaration->set_nonreal_template_role(
        SgNonrealDecl::e_nonreal_template_declaration);
    auto *specialization = SageBuilder::buildSemanticNonrealType(
                               "Box", scope, &arguments, templateDeclaration)
                               ->get_declaration();
    auto *plainType =
        SageBuilder::buildSemanticNonrealType("Box", scope, nullptr);
    auto *plain = plainType->get_declaration();
    ROSE_ASSERT(plain != specialization);
    ROSE_ASSERT(plain != templateDeclaration);
    ROSE_ASSERT(SageBuilder::buildSemanticNonrealType("Box", scope, nullptr) ==
                plainType);
    ROSE_ASSERT(scope->getDeclarationList().size() == 3);
    ROSE_ASSERT(plain->get_mangled_name() !=
                specialization->get_mangled_name());
    SageInterface::deleteAST(global);
  }
  {
    auto *firstOwner = SageBuilder::buildBasicBlock();
    auto *secondOwner = SageBuilder::buildBasicBlock();
    auto *useOwner = SageBuilder::buildBasicBlock();
    const auto ownership =
        SageBuilder::typedef_declaration_ownership::semanticAuxiliary();
    auto *firstAlias = SageBuilder::buildTypedefDeclaration_nfi(
        ownership, SgTypedefDeclaration::e_using, "Shared",
        SageBuilder::buildIntType(), firstOwner);
    auto *secondAlias = SageBuilder::buildTypedefDeclaration_nfi(
        ownership, SgTypedefDeclaration::e_using, "Shared",
        SageBuilder::buildDoubleType(), secondOwner);
    auto *firstType = SageBuilder::buildSemanticNonrealType(
        "Shared", useOwner, nullptr, firstAlias);
    auto *secondType = SageBuilder::buildSemanticNonrealType(
        "Shared", useOwner, nullptr, secondAlias);
    auto *unboundType =
        SageBuilder::buildSemanticNonrealType("Shared", useOwner, nullptr);
    ROSE_ASSERT(firstType != secondType && firstType != unboundType);
    ROSE_ASSERT(secondType != unboundType);
    ROSE_ASSERT(!SageInterface::isEquivalentType(firstType, secondType));
    auto *firstDeclaration = isSgNonrealDecl(firstType->get_declaration());
    auto *secondDeclaration = isSgNonrealDecl(secondType->get_declaration());
    auto *unboundDeclaration = isSgNonrealDecl(unboundType->get_declaration());
    ROSE_ASSERT(firstDeclaration != nullptr);
    ROSE_ASSERT(secondDeclaration != nullptr);
    ROSE_ASSERT(unboundDeclaration != nullptr);
    ROSE_ASSERT(firstDeclaration->get_templateDeclaration() == firstAlias);
    ROSE_ASSERT(secondDeclaration->get_templateDeclaration() == secondAlias);
    ROSE_ASSERT(unboundDeclaration->get_templateDeclaration() == nullptr);
    ROSE_ASSERT(SageBuilder::buildSemanticNonrealType(
                    "Shared", useOwner, nullptr, firstAlias) == firstType);
    ROSE_ASSERT(SageBuilder::buildSemanticNonrealType(
                    "Shared", useOwner, nullptr, secondAlias) == secondType);
    auto *sameTargetType = SageBuilder::buildSemanticNonrealType(
        "Shared", secondOwner, nullptr, firstAlias);
    ROSE_ASSERT(sameTargetType != firstType);
    ROSE_ASSERT(SageInterface::isEquivalentType(firstType, sameTargetType));
    auto signature = [](SgType *parameter) {
      auto *arguments = new SgFunctionParameterTypeList();
      arguments->append_argument(parameter);
      return SageBuilder::buildFunctionType(SageBuilder::buildVoidType(),
                                            arguments);
    };
    auto *firstSignature = signature(firstType);
    auto *sameSignature = signature(sameTargetType);
    auto *otherSignature = signature(secondType);
    {
      auto function_storage = std::make_unique<SgFunctionDeclaration>(
          "rex_nonreal_signature", firstSignature, nullptr, nullptr);
      auto &function = *function_storage;
      function.set_firstNondefiningDeclaration(&function);
      function.set_scope(useOwner);
      const auto targets = CallTargetSet::solveFunctionPointerCallsFunctional(
          &function, sameSignature);
      ROSE_ASSERT(targets.size() == 1 && targets.front() == &function);
      ROSE_ASSERT(CallTargetSet::solveFunctionPointerCallsFunctional(
                      &function, otherSignature)
                      .empty());
    }
    for (auto *type : {firstSignature, sameSignature, otherSignature}) {
      SgNode::get_globalFunctionTypeTable()->remove_function_type(type);
      delete type;
    }
    SageInterface::deleteAST(useOwner);
    SageInterface::deleteAST(firstOwner);
    SageInterface::deleteAST(secondOwner);
  }
  {
    SgBasicBlock *root = SageBuilder::buildBasicBlock();
    auto buildNestedMember = [&]() {
      SgDeclarationScope *memberScope = SageBuilder::buildDeclarationScope();
      SageBuilder::attachSemanticDeclarationScope(root, memberScope);
      SgNonrealDecl *member = nullptr;
      for (int depth = 0; depth != 160; ++depth) {
        member = SageBuilder::buildNonrealDecl("Member", memberScope, nullptr);
        memberScope = member->get_nonreal_decl_scope();
      }
      return member;
    };
    SgNonrealDecl *left = buildNestedMember();
    SgNonrealDecl *right = buildNestedMember();
    if (cyclicOwners) {
      left->set_parent(left->get_nonreal_decl_scope());
      right->set_parent(right->get_nonreal_decl_scope());
    }
    ROSE_ASSERT(SageInterface::cxxSourceTypeMatchesSemanticType(
        left->get_type(), right->get_type()));
    {
      SgNonrealSymbol leftSymbol(left);
      SgNonrealSymbol rightSymbol(right);
      auto left_reference_storage =
          std::make_unique<SgNonrealRefExp>(&leftSymbol, false);
      auto &leftReference = *left_reference_storage;
      auto right_reference_storage =
          std::make_unique<SgNonrealRefExp>(&rightSymbol, false);
      auto &rightReference = *right_reference_storage;
      SgType *castType = SageBuilder::buildIntType();
      std::unique_ptr<SgCastExp> left_cast_storage(new SgCastExp(
          &leftReference, castType, SgUnaryOp::prefix, SgCastExp::e_static_cast,
          SgCastExp::e_semantic_conversion_Dependent,
          SgCastExp::e_value_category_prvalue, {}, castType));
      auto &leftCast = *left_cast_storage;
      std::unique_ptr<SgCastExp> right_cast_storage(new SgCastExp(
          &rightReference, castType, SgUnaryOp::prefix,
          SgCastExp::e_static_cast, SgCastExp::e_semantic_conversion_Dependent,
          SgCastExp::e_value_category_prvalue, {}, castType));
      auto &rightCast = *right_cast_storage;
      leftReference.set_parent(&leftCast);
      rightReference.set_parent(&rightCast);
      auto left_argument_storage =
          std::make_unique<SgTemplateArgument>(&leftCast, false);
      auto &leftArgument = *left_argument_storage;
      auto right_argument_storage =
          std::make_unique<SgTemplateArgument>(&rightCast, false);
      auto &rightArgument = *right_argument_storage;
      ROSE_ASSERT(SageInterface::templateArgumentEquivalence(&leftArgument,
                                                             &rightArgument));
      right->set_nonreal_template_role(SgNonrealDecl::e_nonreal_template_id);
      ROSE_ASSERT(!SageInterface::templateArgumentEquivalence(&leftArgument,
                                                              &rightArgument));
    }
    SageInterface::deleteAST(root);
  }
  auto owner = std::make_unique<SgClassDeclaration>(
      static_cast<Sg_File_Info *>(nullptr));
  auto scope = std::make_unique<SgDeclarationScope>();
  scope->set_parent(owner.get());
  declaration->set_scope(scope.get());
  for (int repeat = 0; repeat != 100; ++repeat) {
    ROSE_ASSERT(declaration->get_mangled_name().getString() == "Dependent");
  }
  declaration->set_is_template_param(true);
  declaration->set_template_parameter_depth(0);
  declaration->set_template_parameter_position(1);
  ROSE_ASSERT(declaration->get_mangled_name().getString() ==
              "__templ_param_0_1__");
  SgNonrealType parameterType(declaration.get());
  declaration->set_type(&parameterType);
  {
    SgNonrealSymbol symbol(declaration.get());
    auto first_reference_storage =
        std::make_unique<SgNonrealRefExp>(&symbol, false);
    auto &firstReference = *first_reference_storage;
    auto second_reference_storage =
        std::make_unique<SgNonrealRefExp>(&symbol, false);
    auto &secondReference = *second_reference_storage;
    auto first_argument_storage =
        std::make_unique<SgTemplateArgument>(&firstReference, false);
    auto &firstReferenceArgument = *first_argument_storage;
    auto second_argument_storage =
        std::make_unique<SgTemplateArgument>(&secondReference, false);
    auto &secondReferenceArgument = *second_argument_storage;
    ROSE_ASSERT(SageInterface::templateArgumentEquivalence(
        &firstReferenceArgument, &secondReferenceArgument));
    secondReference.set_explicit_template_argument_list(true);
    ROSE_ASSERT(!SageInterface::templateArgumentEquivalence(
        &firstReferenceArgument, &secondReferenceArgument));
    secondReference.set_explicit_template_argument_list(false);
    secondReference.set_semantic_role(SgNonrealRefExp::e_dependent_callable);
    ROSE_ASSERT(!SageInterface::templateArgumentEquivalence(
        &firstReferenceArgument, &secondReferenceArgument));
  }
  auto first_argument_storage =
      std::make_unique<SgTemplateArgument>(SageBuilder::buildIntType(), false);
  auto &firstArgument = *first_argument_storage;
  auto second_argument_storage = std::make_unique<SgTemplateArgument>(
      SageBuilder::buildDoubleType(), false);
  auto &secondArgument = *second_argument_storage;
  std::vector<SgTemplateParameter *> parameters;
  std::vector<SgTemplateArgument *> arguments{&firstArgument, &secondArgument};
  ROSE_ASSERT(Rose::Builder::Templates::instantiateNonrealTypes(
                  &parameterType, parameters, arguments, nullptr) ==
              secondArgument.get_type());
  declaration->set_template_parameter_position(0);
  ROSE_ASSERT(Rose::Builder::Templates::instantiateNonrealTypes(
                  &parameterType, parameters, arguments, nullptr) ==
              firstArgument.get_type());
  auto *parameterName = SageBuilder::buildInitializedName(
      "N", SageBuilder::buildIntType(), nullptr);
  auto *parameter = SageBuilder::buildTemplateParameter(
      SgTemplateParameter::nontype_parameter, parameterName->get_type(),
      SgTemplateParameter::keyword_unspecified, parameterName);
  auto *renamedName = SageBuilder::buildInitializedName(
      "M", SageBuilder::buildIntType(), nullptr);
  auto *renamed = SageBuilder::buildTemplateParameter(
      SgTemplateParameter::nontype_parameter, renamedName->get_type(),
      SgTemplateParameter::keyword_unspecified, renamedName, parameter);
  const size_t referencesBefore = SgTemplateParameterVal::numberOfNodes();
  auto *value = SageBuilder::buildTemplateParameterVal(0, renamed);
  auto *supplied = SageBuilder::buildIntVal(17);
  ROSE_ASSERT(SageInterface::templateExpressionEquivalence(supplied, supplied));
  auto supplied_argument_storage =
      std::make_unique<SgTemplateArgument>(supplied, false);
  auto &suppliedArgument = *supplied_argument_storage;
  parameters = {parameter};
  arguments = {&suppliedArgument};
  {
    auto primary_storage = std::make_unique<SgTemplateClassDeclaration>(
        "Box", SgClassDeclaration::e_class, nullptr, nullptr);
    auto &primary = *primary_storage;
    primary.set_scope(scope.get());
    primary.set_firstNondefiningDeclaration(&primary);
    primary.get_templateParameters().push_back(parameter);
    auto *semanticArgument =
        Rose::Builder::Templates::buildTemplateArgument(17);
    std::unique_ptr<SgTemplateInstantiationDecl> specialization_storage(
        new SgTemplateInstantiationDecl("Box<17>", SgClassDeclaration::e_class,
                                        nullptr, nullptr, &primary, {},
                                        {semanticArgument}, false, {}));
    auto &specialization = *specialization_storage;
    specialization.set_scope(scope.get());
    specialization.set_firstNondefiningDeclaration(&specialization);
    SgClassType specializationType(&specialization);
    specialization.set_type(&specializationType);
    auto *symbol = new SgClassSymbol(&specialization);
    scope->insert_symbol(specialization.get_name(), symbol);
    auto request_storage = std::make_unique<SgNonrealDecl>("Box", &primary);
    auto &request = *request_storage;
    request.set_nonreal_template_role(SgNonrealDecl::e_nonreal_template_id);
    auto *writtenArgument = Rose::Builder::Templates::buildTemplateArgument(17);
    request.get_tpl_args().push_back(writtenArgument);
    writtenArgument->set_parent(&request);
    SgNonrealType requestType(&request);
    const size_t argumentCount = SgTemplateArgument::numberOfNodes();
    const size_t literalCount = SgIntVal::numberOfNodes();
    ROSE_ASSERT(Rose::Builder::Templates::instantiateNonrealTypes(
                    &requestType, parameters, arguments, nullptr) ==
                &specializationType);
    ROSE_ASSERT(SgTemplateArgument::numberOfNodes() == argumentCount);
    ROSE_ASSERT(SgIntVal::numberOfNodes() == literalCount);
    ROSE_ASSERT(semanticArgument->get_parent() == &specialization);
    SgTemplateArgumentPtrList replacementArguments{writtenArgument};
    SageBuilder::setTemplateArgumentsInDeclaration(&specialization,
                                                   &replacementArguments);
    ROSE_ASSERT(specialization.get_semanticTemplateArguments() ==
                SgTemplateArgumentPtrList{semanticArgument});
    ROSE_ASSERT(specialization.get_deducedTemplateArguments().empty());
    replacementArguments = specialization.get_templateArguments();
    const size_t replacementArgumentCount = SgTemplateArgument::numberOfNodes();
    const size_t replacementLiteralCount = SgIntVal::numberOfNodes();
    SageBuilder::setTemplateArgumentsInDeclaration(&specialization,
                                                   &replacementArguments);
    ROSE_ASSERT(SgTemplateArgument::numberOfNodes() ==
                replacementArgumentCount);
    ROSE_ASSERT(SgIntVal::numberOfNodes() == replacementLiteralCount);
    ROSE_ASSERT(specialization.get_templateArguments().front()->get_parent() ==
                &specialization);
    scope->remove_symbol(symbol);
    delete symbol;
    for (SgTemplateArgument *argument : specialization.get_templateArguments())
      SageInterface::deleteAST(argument);
    for (SgTemplateArgument *argument :
         specialization.get_deducedTemplateArguments())
      SageInterface::deleteAST(argument);
    SageInterface::deleteAST(writtenArgument);
    SageInterface::deleteAST(semanticArgument);
  }
  auto *replacement =
      SageBuilder::instantiateNonrealRefExps(value, parameters, arguments);
  ROSE_ASSERT(replacement != supplied);
  ROSE_ASSERT(isSgIntVal(replacement)->get_value() == 17);
  ROSE_ASSERT(supplied->get_parent() == &suppliedArgument);
  ROSE_ASSERT(SgTemplateParameterVal::numberOfNodes() == referencesBefore);
  auto *sum = SageBuilder::buildAddOp(
      SageBuilder::buildTemplateParameterVal(0, renamed),
      SageBuilder::buildIntVal(1), SageBuilder::buildIntType());
  ROSE_ASSERT(SageBuilder::instantiateNonrealRefExps(sum, parameters,
                                                     arguments) == sum);
  ROSE_ASSERT(isSgIntVal(sum->get_lhs_operand())->get_value() == 17);
  ROSE_ASSERT(sum->get_lhs_operand()->get_parent() == sum);
  ROSE_ASSERT(SgTemplateParameterVal::numberOfNodes() == referencesBefore);
  auto *unrelated = SageBuilder::buildInitializedName(
      "N", SageBuilder::buildIntType(), nullptr);
  SgVariableSymbol unrelatedSymbol(unrelated);
  auto unrelated_reference_storage =
      std::make_unique<SgVarRefExp>(&unrelatedSymbol);
  auto &unrelatedReference = *unrelated_reference_storage;
  ROSE_ASSERT(SageBuilder::instantiateNonrealRefExps(&unrelatedReference,
                                                     parameters, arguments) ==
              &unrelatedReference);
  auto *defaultName = SageBuilder::buildInitializedName(
      "D", SageBuilder::buildIntType(), nullptr);
  auto *defaultParameter = SageBuilder::buildTemplateParameter(
      SgTemplateParameter::nontype_parameter, defaultName->get_type(),
      SgTemplateParameter::keyword_unspecified, defaultName);
  auto *defaultValue = SageBuilder::buildTemplateParameterVal(0, parameter);
  defaultParameter->set_defaultExpressionParameter(defaultValue);
  defaultValue->set_parent(defaultParameter);
  parameters.push_back(defaultParameter);
  auto *defaultResult = SageBuilder::instantiateNonrealRefExps(
      SageBuilder::buildTemplateParameterVal(1, defaultParameter), parameters,
      arguments);
  ROSE_ASSERT(isSgIntVal(defaultResult)->get_value() == 17);
  ROSE_ASSERT(defaultParameter->get_defaultExpressionParameter() ==
              defaultValue);
  ROSE_ASSERT(defaultValue->get_parent() == defaultParameter);
  ROSE_ASSERT(SgTemplateParameterVal::numberOfNodes() == referencesBefore + 1);
  SageInterface::deleteAST(defaultResult);
  SageInterface::deleteAST(defaultParameter);
  auto *trait = SageBuilder::buildTypeTraitBuiltinOperator(
      "__is_same", SgTypeTraitBuiltinOperator::e_type_trait_builtin,
      SageBuilder::buildBoolType(),
      {SageBuilder::buildTypeExpression(SageBuilder::buildIntType()),
       SageBuilder::buildTypeExpression(SageBuilder::buildIntType())});
  auto *newType =
      SageBuilder::buildTypeExpression(SageBuilder::buildDoubleType());
  SageInterface::replaceExpression(trait->get_args().front(), newType);
  ROSE_ASSERT(trait->get_args().front() == newType);
  ROSE_ASSERT(newType->get_parent() == trait);
  SageInterface::deleteAST(trait);
  SageInterface::deleteAST(replacement);
  SageInterface::deleteAST(sum);
  SageInterface::deleteAST(renamed);
  SageInterface::deleteAST(parameter);
  SageInterface::deleteAST(unrelated);
  SageInterface::deleteAST(supplied);
  auto global = std::make_unique<SgGlobal>();
  auto conceptDeclaration = std::make_unique<SgNonrealDecl>("Concept", nullptr);
  conceptDeclaration->set_is_concept(true);
  conceptDeclaration->set_parent(global.get());
  ROSE_ASSERT(conceptDeclaration->get_scope() == global.get());
  ROSE_ASSERT(conceptDeclaration->get_mangled_name().getString() == "Concept");
}
