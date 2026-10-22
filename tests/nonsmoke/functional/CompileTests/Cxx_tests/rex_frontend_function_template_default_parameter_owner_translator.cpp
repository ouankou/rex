#include "RoseAst.h"
#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgTemplateFunctionDeclaration *definition = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgTemplateFunctionDeclaration *candidate =
        isSgTemplateFunctionDeclaration(node);
    if (candidate == nullptr ||
        candidate->get_name() !=
            "rex_function_template_default_parameter_owner" ||
        candidate->get_definition() == nullptr) {
      continue;
    }
    ROSE_ASSERT(definition == nullptr);
    definition = candidate;
  }
  ROSE_ASSERT(definition != nullptr);

  SgTemplateParameterPtrList &parameters = definition->get_templateParameters();
  ROSE_ASSERT(parameters.size() == 2);
  for (SgTemplateParameter *parameter : parameters) {
    ROSE_ASSERT(parameter != nullptr);
    ROSE_ASSERT(parameter->get_parent() == definition);
  }

  SgDeclarationScope *declaratorScope =
      definition->get_function_declarator_scope();
  ROSE_ASSERT(declaratorScope != nullptr);
  ROSE_ASSERT(SageBuilder::getDeclarationScopeOwner(declaratorScope) ==
              definition);

  SgType *defaultType = parameters[1]->get_defaultTypeParameter();
  ROSE_ASSERT(defaultType != nullptr);
  SgNonrealType *terminalType = isSgNonrealType(defaultType->findBaseType());
  ROSE_ASSERT(terminalType != nullptr);
  SgNonrealDecl *terminalDeclaration =
      isSgNonrealDecl(terminalType->get_declaration());
  ROSE_ASSERT(terminalDeclaration != nullptr);
  ROSE_ASSERT(terminalDeclaration->get_name() == "type");

  SgDeclarationScope *terminalScope =
      isSgDeclarationScope(terminalDeclaration->get_scope());
  ROSE_ASSERT(terminalScope != nullptr);
  SgNonrealDecl *baseDeclaration =
      isSgNonrealDecl(SageBuilder::getDeclarationScopeOwner(terminalScope));
  ROSE_ASSERT(baseDeclaration != nullptr);
  ROSE_ASSERT(baseDeclaration->get_name() == "TypeIdentity");
  ROSE_ASSERT(baseDeclaration->get_scope() == declaratorScope);

  SgTemplateFunctionDeclaration *memberDefinition = nullptr;
  for (SgNode *node : RoseAst(project)) {
    auto *candidate = isSgTemplateFunctionDeclaration(node);
    if (candidate != nullptr && candidate->get_definition() != nullptr &&
        candidate->get_name() == "rex_function_template_member_defaults") {
      ROSE_ASSERT(memberDefinition == nullptr);
      memberDefinition = candidate;
    }
  }
  ROSE_ASSERT(memberDefinition != nullptr);
  const auto &memberParameters = memberDefinition->get_templateParameters();
  ROSE_ASSERT(memberParameters.size() == 6);
  auto requireParameter = [](SgType *type, SgTemplateParameter *parameter) {
    auto *reference = isSgTemplateType(type);
    if (reference == nullptr) {
      std::cerr << "Member-default parameter reference has type "
                << (type != nullptr ? type->class_name() : "<null>")
                << "; expected parameter type "
                << parameter->get_type()->class_name() << std::endl;
    }
    ROSE_ASSERT(reference != nullptr);
    ROSE_ASSERT(reference->get_template_parameter() == parameter);
  };
  for (size_t index = 2; index < memberParameters.size(); ++index) {
    ROSE_ASSERT(memberParameters[index]->get_parent() == memberDefinition);
    SgType *type = memberParameters[index]->get_defaultTypeParameter();
    ROSE_ASSERT(type != nullptr);
    if (index == 5) {
      ROSE_ASSERT(type->variantT() == V_SgPointerType);
      type = isSgPointerType(type)->get_base_type();
    }
    auto *member = isSgPointerMemberType(type);
    ROSE_ASSERT(member != nullptr);
    if (index != 3)
      requireParameter(member->get_base_type(), memberParameters[1]);
    else
      ROSE_ASSERT(isSgTypeInt(member->get_base_type()) != nullptr);
    if (index != 4)
      requireParameter(member->get_class_type(), memberParameters[0]);
    else
      ROSE_ASSERT(isSgClassType(member->get_class_type()) != nullptr);
  }

  SgTemplateFunctionDeclaration *signatureDefinition = nullptr;
  for (SgNode *node : RoseAst(project)) {
    auto *candidate = isSgTemplateFunctionDeclaration(node);
    if (candidate != nullptr && candidate->get_definition() != nullptr &&
        candidate->get_name() == "rex_function_template_signature_defaults") {
      ROSE_ASSERT(signatureDefinition == nullptr);
      signatureDefinition = candidate;
    }
  }
  ROSE_ASSERT(signatureDefinition != nullptr);
  const auto &signatureParameters =
      signatureDefinition->get_templateParameters();
  ROSE_ASSERT(signatureParameters.size() == 6);
  for (size_t index = 2; index < signatureParameters.size(); ++index) {
    SgType *type = signatureParameters[index]->get_defaultTypeParameter();
    ROSE_ASSERT(type != nullptr);
    if (index == 2 || index == 4) {
      auto *pointer = isSgPointerType(type);
      ROSE_ASSERT(pointer != nullptr);
      type = pointer->get_base_type();
    }
    auto *function = isSgFunctionType(type);
    ROSE_ASSERT(function != nullptr);
    requireParameter(function->get_return_type(), signatureParameters[0]);
    if (index == 4) {
      auto *member = isSgMemberFunctionType(function);
      ROSE_ASSERT(member != nullptr);
      ROSE_ASSERT(member->get_mfunc_specifier() &
                  SgMemberFunctionType::e_const);
    }
    function->get_argument_list()->validate_argument_qualification_use_sites(
        "template-signature-default-regression");
    ROSE_ASSERT(function->get_arguments().size() == (index == 3 ? 2 : 1));
    SgType *argument = function->get_arguments().front();
    if (index == 5) {
      auto *pointer = isSgPointerType(argument);
      ROSE_ASSERT(pointer != nullptr);
      auto *nested = isSgFunctionType(pointer->get_base_type());
      ROSE_ASSERT(nested != nullptr);
      requireParameter(nested->get_return_type(), signatureParameters[0]);
      ROSE_ASSERT(nested->get_arguments().size() == 1);
      argument = nested->get_arguments().front();
    }
    requireParameter(argument, signatureParameters[1]);
    ROSE_ASSERT(function->get_has_ellipses() == (index == 3));
    if (index == 3)
      ROSE_ASSERT(isSgTypeEllipse(function->get_arguments().back()) != nullptr);
  }

  size_t checkedArrayDefaults = 0;
  for (auto *node : RoseAst(project)) {
    auto *function = isSgTemplateFunctionDeclaration(node);
    if (function == nullptr ||
        function->get_name() != "rex_function_template_array_default")
      continue;
    const auto &parameters = function->get_templateParameters();
    ROSE_ASSERT(parameters.size() == 4);
    for (size_t index = 1; index < parameters.size(); ++index) {
      auto *array =
          isSgArrayType(parameters[index]->get_defaultTypeParameter());
      ROSE_ASSERT(array != nullptr);
      if (index == 1)
        requireParameter(array->get_base_type(), parameters[0]);
      else
        ROSE_ASSERT(isSgTypeInt(array->get_base_type()) != nullptr);
      if (index == 3) {
        auto *bound = isSgAlignOfOp(array->get_index());
        ROSE_ASSERT(bound != nullptr && bound->get_parent() == array);
        requireParameter(bound->get_operand_type(), parameters[0]);
      } else {
        auto *bound = isSgSizeOfOp(array->get_index());
        ROSE_ASSERT(bound != nullptr && bound->get_parent() == array);
        requireParameter(bound->get_operand_type(), parameters[0]);
      }
      ++checkedArrayDefaults;
    }
  }
  ROSE_ASSERT(checkedArrayDefaults >= 6);

  size_t checkedExpressionDefaults = 0;
  for (auto *node : RoseAst(project)) {
    auto *function = isSgTemplateFunctionDeclaration(node);
    if (function == nullptr ||
        function->get_name() != "rex_function_template_expression_default")
      continue;
    const auto &parameters = function->get_templateParameters();
    ROSE_ASSERT(parameters.size() == 6);
    auto *defaultExpression =
        isSgSizeOfOp(parameters[3]->get_defaultExpressionParameter());
    ROSE_ASSERT(defaultExpression != nullptr &&
                defaultExpression->get_parent() == parameters[3]);
    requireParameter(defaultExpression->get_operand_type(), parameters[0]);
    auto requireValueParameter = [&](SgExpression *expression) {
      auto *value = isSgTemplateParameterVal(expression);
      ROSE_ASSERT(value != nullptr &&
                  value->get_template_parameter() == parameters[3]);
      ROSE_ASSERT(value->get_type() == parameters[3]->get_type());
    };
    requireValueParameter(parameters[4]->get_defaultExpressionParameter());
    auto *valueDefault =
        isSgNonrealType(parameters[5]->get_defaultTypeParameter());
    ROSE_ASSERT(valueDefault != nullptr);
    auto *valueDeclaration = isSgNonrealDecl(valueDefault->get_declaration());
    ROSE_ASSERT(valueDeclaration != nullptr &&
                valueDeclaration->get_tpl_args().size() == 1);
    requireValueParameter(
        valueDeclaration->get_tpl_args().front()->get_expression());
    for (size_t index = 1; index < 3; ++index) {
      auto *type =
          isSgNonrealType(parameters[index]->get_defaultTypeParameter());
      ROSE_ASSERT(type != nullptr);
      auto *declaration = isSgNonrealDecl(type->get_declaration());
      ROSE_ASSERT(declaration != nullptr &&
                  declaration->get_tpl_args().size() == 1);
      for (auto *expression : RoseAst(declaration->get_tpl_args().front())) {
        SgType *operand = nullptr;
        if (auto *size = isSgSizeOfOp(expression))
          operand = size->get_operand_type();
        else if (auto *alignment = isSgAlignOfOp(expression))
          operand = alignment->get_operand_type();
        if (operand != nullptr) {
          requireParameter(operand, parameters[0]);
          ++checkedExpressionDefaults;
        }
      }
    }
  }
  ROSE_ASSERT(checkedExpressionDefaults >= 4);

  size_t checkedDefaults = 0;
  for (auto *node : RoseAst(project)) {
    auto *owner = isSgTemplateClassDeclaration(node);
    if (owner == nullptr ||
        owner->get_name() != "RexNestedDefaultArgumentOwner")
      continue;
    const auto &parameters = owner->get_templateParameters();
    ROSE_ASSERT(parameters.size() == 3);
    for (size_t index = 1; index < parameters.size(); ++index) {
      auto *type = parameters[index]->get_defaultTypeParameter();
      if (type == nullptr)
        continue;
      auto *nonreal = isSgNonrealType(type);
      ROSE_ASSERT(nonreal != nullptr);
      auto *declaration = isSgNonrealDecl(nonreal->get_declaration());
      ROSE_ASSERT(declaration != nullptr &&
                  declaration->get_tpl_args().size() == 1);
      auto *argument = declaration->get_tpl_args().front()->get_type();
      auto *writtenArgument =
          declaration->get_tpl_args().front()->get_sourceSpelledType();
      ROSE_ASSERT(argument != nullptr && writtenArgument != nullptr);
      if (index == 2) {
        auto *pointer = isSgPointerType(argument);
        auto *writtenPointer = isSgPointerType(writtenArgument);
        ROSE_ASSERT(pointer != nullptr && writtenPointer != nullptr);
        argument = pointer->get_base_type();
        writtenArgument = writtenPointer->get_base_type();
      }
      auto *canonical = isSgTemplateClassDeclaration(
          owner->get_firstNondefiningDeclaration());
      ROSE_ASSERT(canonical != nullptr);
      ROSE_ASSERT(canonical->get_templateParameters().size() ==
                  parameters.size());
      requireParameter(argument,
                       canonical->get_templateParameters()[index - 1]);
      requireParameter(writtenArgument, parameters[index - 1]);
      ++checkedDefaults;
    }
  }
  ROSE_ASSERT(checkedDefaults >= 2);
  SgTemplateFunctionDeclaration *differentPosition = nullptr;
  for (auto *node : RoseAst(project)) {
    auto *function = isSgTemplateFunctionDeclaration(node);
    if (function != nullptr &&
        function->get_name() ==
            "rex_function_template_non_type_different_position")
      differentPosition = function;
  }
  ROSE_ASSERT(differentPosition != nullptr);
  size_t checkedNonType = 0;
  for (auto *node : RoseAst(project)) {
    auto *function = isSgTemplateFunctionDeclaration(node);
    if (function == nullptr ||
        function->get_name() != "rex_function_template_non_type_owner")
      continue;
    const auto &parameters = function->get_templateParameters();
    ROSE_ASSERT(parameters.size() == 4);
    auto *canonical = isSgTemplateFunctionDeclaration(
        function->get_firstNondefiningDeclaration());
    ROSE_ASSERT(canonical != nullptr);
    ROSE_ASSERT(function->get_type() == canonical->get_type());
    ROSE_ASSERT(SageInterface::templateParameterListEquivalence(
        parameters, canonical->get_templateParameters()));
    ROSE_ASSERT(SageInterface::templateParameterListEquivalence(
        canonical->get_templateParameters(), parameters));
    ROSE_ASSERT(!SageInterface::templateParameterListEquivalence(
        parameters, differentPosition->get_templateParameters()));
    ROSE_ASSERT(!SageInterface::templateParameterListEquivalence(
        differentPosition->get_templateParameters(), parameters));
    for (size_t index = 2; index < parameters.size(); ++index) {
      auto *parameter = parameters[index];
      ROSE_ASSERT(parameter->get_parameterType() ==
                  SgTemplateParameter::nontype_parameter);
      auto *name = parameter->get_initializedName();
      ROSE_ASSERT(name != nullptr && name->get_parent() == parameter);
      ROSE_ASSERT(name->get_type() == parameter->get_type());
      auto *type = parameter->get_type();
      if (index == 3) {
        auto *pointer = isSgPointerType(type);
        ROSE_ASSERT(pointer != nullptr);
        type = pointer->get_base_type();
      }
      requireParameter(type, parameters.front());
      ++checkedNonType;
    }
  }
  ROSE_ASSERT(checkedNonType >= 4);
  unsigned adjustedParameters = 0;
  for (SgNode *node : RoseAst(project)) {
    auto *function = isSgFunctionDeclaration(node);
    if (function == nullptr ||
        function->get_name() != "rex_adjusted_array_parameter" ||
        isSgGlobal(function->get_parent()) == nullptr)
      continue;
    ROSE_ASSERT(function->get_args().size() == 1);
    auto *parameter = function->get_args().front();
    ROSE_ASSERT(parameter->get_parent() == function->get_parameterList() &&
                parameter->get_declptr() == function);
    ROSE_ASSERT(isSgPointerType(parameter->get_type()) != nullptr);
    auto *syntax = function->get_parameterList_syntax();
    ROSE_ASSERT(syntax != nullptr && syntax->get_parent() == function &&
                syntax->get_args().size() == 1);
    auto *written = syntax->get_args().front();
    ROSE_ASSERT(written != parameter && written->get_parent() == syntax &&
                written->get_declptr() == function);
    auto *array = isSgArrayType(written->get_type());
    ROSE_ASSERT(array != nullptr);
    auto *bound = isSgIntVal(array->get_index());
    ROSE_ASSERT(bound != nullptr && bound->get_value() == 3);
    auto *returnType = isSgDeclType(function->get_orig_return_type());
    ROSE_ASSERT(returnType != nullptr);
    unsigned reads = 0;
    for (SgNode *expression : RoseAst(returnType->get_base_expression())) {
      if (auto *reference = isSgVarRefExp(expression)) {
        ROSE_ASSERT(reference->get_symbol()->get_declaration() == parameter &&
                    reference->get_type() == parameter->get_type());
        ++reads;
      }
    }
    ROSE_ASSERT(reads == 1);
    ++adjustedParameters;
  }
  ROSE_ASSERT(adjustedParameters == 1);
  unsigned qualifiedDefinitions = 0;
  for (SgNode *node : RoseAst(project)) {
    auto *function = isSgTemplateFunctionDeclaration(node);
    if (function == nullptr ||
        function->get_name() != "rex_cv_redeclared_parameter" ||
        function->get_definition() == nullptr)
      continue;
    ROSE_ASSERT(function->get_args().size() == 1);
    auto *parameter = function->get_args().front();
    ROSE_ASSERT(SageInterface::isConstType(parameter->get_type()));
    ROSE_ASSERT(SageInterface::isVolatileType(parameter->get_type()));
    ROSE_ASSERT(function->get_type()->get_arguments().size() == 1);
    ROSE_ASSERT(isSgTypeInt(function->get_type()->get_arguments().front()));
    auto *canonical = isSgTemplateFunctionDeclaration(
        function->get_firstNondefiningDeclaration());
    ROSE_ASSERT(canonical != nullptr && canonical != function);
    ROSE_ASSERT(canonical->get_type() == function->get_type());
    unsigned reads = 0;
    for (SgNode *bodyNode : RoseAst(function->get_definition())) {
      auto *reference = isSgVarRefExp(bodyNode);
      if (reference != nullptr &&
          reference->get_symbol()->get_declaration() == parameter) {
        ROSE_ASSERT(reference->get_type() == parameter->get_type());
        ++reads;
      }
    }
    ROSE_ASSERT(reads == 1);
    ++qualifiedDefinitions;
  }
  ROSE_ASSERT(qualifiedDefinitions == 1);
  AstTests::runAllTests(project);
  return backend(project);
}
