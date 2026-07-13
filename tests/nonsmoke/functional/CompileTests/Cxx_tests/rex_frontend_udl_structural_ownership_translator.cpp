#include "RoseAst.h"
#include "rose.h"

#include <algorithm>
#include <map>
#include <string>

namespace {

SgExpression *semanticFunctionReference(SgFunctionCallExp *call) {
  ROSE_ASSERT(call != nullptr);
  SgCastExp *decay = isSgCastExp(call->get_function());
  ROSE_ASSERT(decay != nullptr);
  decay->validate_semantic_conversion();
  ROSE_ASSERT(decay->get_cast_type() == SgCastExp::e_implicit_cast);
  ROSE_ASSERT(decay->get_semantic_conversion_kind() ==
              SgCastExp::e_semantic_conversion_FunctionToPointerDecay);
  ROSE_ASSERT(decay->get_parent() == call);
  ROSE_ASSERT(decay->get_operand() != nullptr);
  ROSE_ASSERT(decay->get_operand()->get_parent() == decay);
  return decay->get_operand();
}

SgFunctionCallExp *variableInitializerCall(SgProject *project,
                                           const std::string &name) {
  SgFunctionCallExp *result = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgInitializedName *initialized_name = isSgInitializedName(node);
    if (initialized_name == nullptr || initialized_name->get_name() != name) {
      continue;
    }
    SgAssignInitializer *initializer =
        isSgAssignInitializer(initialized_name->get_initializer());
    ROSE_ASSERT(initializer != nullptr);
    SgFunctionCallExp *call = isSgFunctionCallExp(initializer->get_operand());
    ROSE_ASSERT(call != nullptr);
    ROSE_ASSERT(result == nullptr);
    result = call;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void requireLiteralOperatorDeclaration(SgFunctionCallExp *call,
                                       const std::string &name) {
  ROSE_ASSERT(call != nullptr);
  SgFunctionDeclaration *declaration = call->getAssociatedFunctionDeclaration();
  ROSE_ASSERT(declaration != nullptr);
  if (SgTemplateInstantiationFunctionDecl *instantiation =
          isSgTemplateInstantiationFunctionDecl(declaration)) {
    ROSE_ASSERT(instantiation->get_templateName() == name);
  } else {
    ROSE_ASSERT(declaration->get_name() == name);
  }
  ROSE_ASSERT(declaration->get_specialFunctionModifier().isUldOperator());
}

SgTemplateInstantiationFunctionDecl *
requireDeducedTemplateReference(SgFunctionCallExp *call) {
  ROSE_ASSERT(call != nullptr);
  SgExpression *reference = semanticFunctionReference(call);
  SgTemplateFunctionRefExp *template_reference =
      isSgTemplateFunctionRefExp(reference);
  ROSE_ASSERT(template_reference != nullptr);
  ROSE_ASSERT(isSgCastExp(reference->get_parent()) != nullptr);
  SgTemplateFunctionSymbol *symbol = template_reference->get_symbol();
  ROSE_ASSERT(symbol != nullptr);

  SgTemplateFunctionDeclaration *sourceTemplate =
      isSgTemplateFunctionDeclaration(symbol->get_declaration());
  ROSE_ASSERT(sourceTemplate != nullptr);
  ROSE_ASSERT(!sourceTemplate->get_templateParameters().empty());
  for (SgTemplateParameter *parameter :
       sourceTemplate->get_templateParameters()) {
    ROSE_ASSERT(parameter != nullptr);
    ROSE_ASSERT(parameter->get_parent() == sourceTemplate);
  }

  SgTemplateInstantiationFunctionDecl *semanticSpecialization =
      isSgTemplateInstantiationFunctionDecl(
          template_reference->get_semantic_function_declaration());
  ROSE_ASSERT(semanticSpecialization != nullptr);
  ROSE_ASSERT(call->getAssociatedFunctionDeclaration() ==
              semanticSpecialization);
  ROSE_ASSERT(static_cast<SgFunctionDeclaration *>(semanticSpecialization) !=
              static_cast<SgFunctionDeclaration *>(sourceTemplate));
  SgTemplateFunctionDeclaration *semanticTemplate =
      semanticSpecialization->get_templateDeclaration();
  ROSE_ASSERT(semanticTemplate != nullptr);
  ROSE_ASSERT(semanticSpecialization->get_specializedTemplateDeclaration() ==
              semanticTemplate);
  ROSE_ASSERT(semanticTemplate != sourceTemplate);
  ROSE_ASSERT(semanticTemplate->get_firstNondefiningDeclaration() ==
              sourceTemplate);
  ROSE_ASSERT(sourceTemplate->get_definingDeclaration() == semanticTemplate);
  return semanticSpecialization;
}

SgTemplateInstantiationFunctionDecl *
requireExplicitTemplateReferenceAndArguments(SgFunctionCallExp *call) {
  ROSE_ASSERT(call != nullptr);
  SgNonrealRefExp *reference =
      isSgNonrealRefExp(semanticFunctionReference(call));
  ROSE_ASSERT(reference != nullptr);
  SgFunctionDeclaration *resolved =
      SageInterface::requireResolvedFunctionTemplateReference(
          reference, "UDL structural ownership regression");
  SgTemplateInstantiationFunctionDecl *declaration =
      isSgTemplateInstantiationFunctionDecl(resolved);
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(call->getAssociatedFunctionDeclaration() ==
              declaration->get_firstNondefiningDeclaration());
  ROSE_ASSERT(!reference->get_templateArguments().empty());
  for (SgTemplateArgument *argument : reference->get_templateArguments()) {
    ROSE_ASSERT(argument != nullptr);
    ROSE_ASSERT(argument->get_parent() == reference);
    ROSE_ASSERT(argument->get_explicitlySpecified());
  }
  ROSE_ASSERT(!declaration->get_templateArguments().empty());
  for (SgTemplateArgument *argument : declaration->get_templateArguments()) {
    ROSE_ASSERT(argument != nullptr);
    ROSE_ASSERT(argument->get_parent() == declaration);
  }
  return declaration;
}

void requireCallOwnership(SgFunctionCallExp *call) {
  ROSE_ASSERT(call != nullptr);
  ROSE_ASSERT(call->get_function() != nullptr);
  ROSE_ASSERT(call->get_function()->get_parent() == call);
  ROSE_ASSERT(call->get_args() != nullptr);
  ROSE_ASSERT(call->get_args()->get_parent() == call);
  for (SgExpression *argument : call->get_args()->get_expressions()) {
    ROSE_ASSERT(argument != nullptr);
    ROSE_ASSERT(argument->get_parent() == call->get_args());
  }
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  struct ExpectedCall {
    const char *variable;
    const char *operator_name;
    const char *literal_suffix;
    bool template_reference;
    size_t semantic_arguments;
    size_t lexical_tokens;
    size_t suffix_occurrences;
  };
  const ExpectedCall expected_calls[] = {
      {"rex_udl_literal_cooked", "operator\"\"_rex_cooked", "_rex_cooked",
       false, 1, 1, 1},
      {"rex_udl_literal_raw", "operator\"\"_rex_raw", "_rex_raw", false, 1, 1,
       1},
      {"rex_udl_literal_template", "operator\"\"_rex_template", "_rex_template",
       true, 0, 1, 1},
      {"rex_udl_literal_joined", "operator\"\"_rex_joined", "_rex_joined",
       false, 2, 2, 2},
      {"rex_udl_explicit_cooked", "operator\"\"_rex_cooked", "", false, 1, 0,
       0},
      {"rex_udl_explicit_raw", "operator\"\"_rex_raw", "", false, 1, 0, 0},
      {"rex_udl_explicit_template", "operator\"\"_rex_template", "", true, 0, 0,
       0},
  };

  for (const ExpectedCall &expected : expected_calls) {
    SgFunctionCallExp *call =
        variableInitializerCall(project, expected.variable);
    requireCallOwnership(call);
    requireLiteralOperatorDeclaration(call, expected.operator_name);

    const std::string suffix =
        call->get_source_user_defined_literal_suffix().getString();
    ROSE_ASSERT(suffix == expected.literal_suffix);
    ROSE_ASSERT(call->get_uses_operator_syntax() == !suffix.empty());
    ROSE_ASSERT(call->get_args()->get_expressions().size() ==
                expected.semantic_arguments);

    SgExprListExp *lexical = call->get_source_user_defined_literal_operands();
    if (suffix.empty()) {
      ROSE_ASSERT(lexical == nullptr);
      ROSE_ASSERT(call->get_source_user_defined_literal_suffix_roles().empty());
    } else {
      ROSE_ASSERT(lexical != nullptr);
      ROSE_ASSERT(lexical->get_parent() == call);
      ROSE_ASSERT(lexical->get_expressions().size() == expected.lexical_tokens);
      const SgUnsignedCharList &suffix_roles =
          call->get_source_user_defined_literal_suffix_roles();
      ROSE_ASSERT(suffix_roles.size() == expected.lexical_tokens);
      ROSE_ASSERT(
          static_cast<size_t>(std::count(
              suffix_roles.begin(), suffix_roles.end(),
              SgFunctionCallExp::e_user_defined_literal_token_with_suffix)) ==
          expected.suffix_occurrences);
      for (SgExpression *operand : lexical->get_expressions()) {
        ROSE_ASSERT(operand != nullptr);
        ROSE_ASSERT(operand->get_parent() == lexical);
        ROSE_ASSERT(isSgValueExp(operand) != nullptr);
      }
      const SgUnsignedCharList &semantic_roles =
          call->get_source_operator_operand_roles();
      ROSE_ASSERT(semantic_roles.size() == expected.semantic_arguments);
      ROSE_ASSERT(std::all_of(
          semantic_roles.begin(), semantic_roles.end(), [](unsigned char role) {
            return role == SgFunctionCallExp::e_semantic_operator_operand;
          }));
    }

    if (expected.template_reference) {
      if (suffix.empty()) {
        SgTemplateInstantiationFunctionDecl *declaration =
            requireExplicitTemplateReferenceAndArguments(call);
        ROSE_ASSERT(declaration->get_templateName() == expected.operator_name);
        ROSE_ASSERT(declaration->get_specialFunctionModifier().isUldOperator());
      } else {
        SgTemplateInstantiationFunctionDecl *declaration =
            requireDeducedTemplateReference(call);
        ROSE_ASSERT(declaration->get_templateName() == expected.operator_name);
        ROSE_ASSERT(declaration->get_specialFunctionModifier().isUldOperator());
      }
    } else {
      SgFunctionRefExp *reference =
          isSgFunctionRefExp(semanticFunctionReference(call));
      ROSE_ASSERT(reference != nullptr);
      ROSE_ASSERT(reference->get_symbol() != nullptr);
    }
  }

  AstTests::runAllTests(project);
  return backend(project);
}
