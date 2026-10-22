#include "RoseAst.h"
#include "rose.h"
#include "sageAstJson.h"
#include <memory>
#include <set>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

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
    SgExpression *operand = initializer->get_operand();
    if (name == "rex_udl_literal_template") {
      auto *macro = isSgMacroExpansionExp(operand);
      ROSE_ASSERT(macro != nullptr && macro->get_parent() == initializer);
      ROSE_ASSERT(macro->get_spelling() == "REX_UDL_TEMPLATE");
      operand = macro->get_expanded_expression_checked();
      ROSE_ASSERT(operand->get_parent() == macro);
      ROSE_ASSERT(macro->get_type() == operand->get_type());
    }
    SgFunctionCallExp *call = isSgFunctionCallExp(operand);
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
  ROSE_ASSERT(declaration->get_templateArguments().empty());
  const auto &semantic_arguments = declaration->get_deducedTemplateArguments();
  ROSE_ASSERT(!semantic_arguments.empty());
  ROSE_ASSERT(SageInterface::templateArgumentListEquivalence(
      reference->get_templateArguments(), semantic_arguments));
  for (SgTemplateArgument *argument : semantic_arguments) {
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

void verify(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  for (SgNode *node : RoseAst(project)) {
    auto *declaration = isSgTypedefDeclaration(node);
    if (declaration != nullptr &&
        isSgDeclType(declaration->get_base_type()) != nullptr) {
      ROSE_ASSERT(declaration->get_base_type()->get_parent() == declaration);
    }
  }
  struct ExpectedCall {
    const char *variable;
    const char *operator_name;
    const char *literal_suffix;
    bool template_reference;
    size_t semantic_arguments;
    size_t lexical_tokens;
    size_t suffix_occurrences;
    SgCxxLiteralToken::token_kind_enum token_kind =
        SgCxxLiteralToken::e_numeric_token;
    std::vector<std::string> written_tokens = {};
  };
  const ExpectedCall expected_calls[] = {
      {"rex_udl_literal_cooked", "operator\"\"_rex_cooked", "_rex_cooked",
       false, 1, 1, 1},
      {"rex_udl_literal_raw", "operator\"\"_rex_raw", "_rex_raw", false, 1, 1,
       1},
      {"rex_udl_literal_template", "operator\"\"_rex_template", "_rex_template",
       true, 0, 1, 1},
      {"rex_udl_literal_joined", "operator\"\"_rex_joined", "_rex_joined",
       false, 2, 2, 2, SgCxxLiteralToken::e_string_token},
      {"rex_udl_literal_huge_raw", "operator\"\"_rex_raw", "_rex_raw", false, 1,
       1, 1},
      {"rex_udl_literal_huge_float", "operator\"\"_rex_raw", "_rex_raw", false,
       1, 1, 1},
      {"rex_udl_literal_escaped", "operator\"\"_rex_joined", "_rex_joined",
       false, 2, 2, 2, SgCxxLiteralToken::e_string_token},
      {"rex_udl_literal_ucn",
       "operator\"\"_rex_π",
       "_rex_π",
       false,
       1,
       1,
       1,
       SgCxxLiteralToken::e_numeric_token,
       {R"(17_rex_\u03c0)"}},
      {"rex_udl_literal_ucn_character",
       "operator\"\"_rex_π",
       "_rex_π",
       false,
       1,
       1,
       1,
       SgCxxLiteralToken::e_character_token,
       {R"('A'_rex_\U000003c0)"}},
      {"rex_udl_literal_ucn_joined",
       "operator\"\"_rex_π",
       "_rex_π",
       false,
       2,
       2,
       2,
       SgCxxLiteralToken::e_string_token,
       {R"("left"_rex_\u03c0)", R"("right"_rex_π)"}},
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

    SgCxxLiteralTokenList *lexical =
        call->get_source_user_defined_literal_operands();
    if (suffix.empty()) {
      ROSE_ASSERT(lexical == nullptr);
      ROSE_ASSERT(call->get_source_user_defined_literal_suffix_roles().empty());
    } else {
      ROSE_ASSERT(lexical != nullptr);
      ROSE_ASSERT(lexical->get_parent() == call);
      ROSE_ASSERT(lexical->get_tokens().size() == expected.lexical_tokens);
      const SgUnsignedCharList &suffix_roles =
          call->get_source_user_defined_literal_suffix_roles();
      ROSE_ASSERT(suffix_roles.size() == expected.lexical_tokens);
      ROSE_ASSERT(
          static_cast<size_t>(std::count(
              suffix_roles.begin(), suffix_roles.end(),
              SgFunctionCallExp::e_user_defined_literal_token_with_suffix)) ==
          expected.suffix_occurrences);
      for (SgCxxLiteralToken *operand : lexical->get_tokens()) {
        ROSE_ASSERT(operand != nullptr);
        ROSE_ASSERT(operand->get_parent() == lexical);
        ROSE_ASSERT(isSgExpression(operand) == nullptr);
        operand->validate_literal_token();
        ROSE_ASSERT(operand->get_token_kind() == expected.token_kind);
      }
      if (!expected.written_tokens.empty()) {
        ROSE_ASSERT(expected.written_tokens.size() ==
                    lexical->get_tokens().size());
        std::string written;
        for (size_t index = 0; index < expected.written_tokens.size();
             ++index) {
          ROSE_ASSERT(lexical->get_tokens()[index]->get_spelling() ==
                      expected.written_tokens[index]);
          if (index != 0)
            written += ' ';
          written += expected.written_tokens[index];
        }
        ROSE_ASSERT(call->unparseToString() == written);
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

  const std::map<std::string, SgStringCodeUnitList> string_values = {
      {"rex_code_units_bytes", {65, 10, 0, 255}},
      {"rex_code_units_utf8", {0xc3, 0xa9}},
      {"rex_code_units_utf16", {0xd83d, 0xde42}},
      {"rex_code_units_utf32", {0x1f642}},
      {"rex_code_units_utf16_surrogate", {0xd800}},
      {"rex_code_units_utf32_numeric", {0xffffffff}},
      {"rex_code_units_wide", {0x3a9}},
      {"rex_code_units_raw", {65, 92, 110}}};
  std::set<std::string> observed_strings;
  for (SgNode *node : RoseAst(project)) {
    auto *name = isSgInitializedName(node);
    if (name == nullptr)
      continue;
    auto expected = string_values.find(name->get_name().getString());
    if (expected == string_values.end())
      continue;
    ROSE_ASSERT(observed_strings.insert(expected->first).second);
    auto *initializer = isSgAssignInitializer(name->get_initializer());
    ROSE_ASSERT(initializer != nullptr);
    auto *literal = isSgStringVal(initializer->get_operand());
    ROSE_ASSERT(literal != nullptr && literal->get_value() == expected->second);
  }
  ROSE_ASSERT(observed_strings.size() == string_values.size());

  const std::map<std::string, std::string> type_suffixes = {
      {"RexUdlCookedType", "_rex_cooked"},
      {"RexUdlRawType", "_rex_raw"},
      {"RexUdlTemplateType", "_rex_template"},
      {"RexUdlStringType", "_rex_joined"}};
  std::set<std::string> observed_types;
  for (SgNode *node : RoseAst(project)) {
    auto *declaration = isSgTypedefDeclaration(node);
    if (declaration == nullptr)
      continue;
    const auto expected =
        type_suffixes.find(declaration->get_name().getString());
    if (expected == type_suffixes.end())
      continue;
    ROSE_ASSERT(observed_types.insert(expected->first).second);
    auto *type = isSgDeclType(declaration->get_base_type());
    ROSE_ASSERT(type != nullptr);
    auto *call = isSgFunctionCallExp(type->get_base_expression());
    ROSE_ASSERT(call != nullptr && call->get_parent() == type);
    requireCallOwnership(call);
    ROSE_ASSERT(call->get_source_user_defined_literal_suffix() ==
                expected->second);
    ROSE_ASSERT(!call->get_source_user_defined_literal_is_modified());
    auto *tokens = call->get_source_user_defined_literal_operands();
    ROSE_ASSERT(tokens != nullptr && tokens->get_parent() == call);
    ROSE_ASSERT(tokens->get_tokens().size() == 1);
    auto *token = tokens->get_tokens().front();
    ROSE_ASSERT(token != nullptr && token->get_parent() == tokens);
    token->validate_literal_token();
  }
  ROSE_ASSERT(observed_types.size() == type_suffixes.size());
  AstTests::runAllTests(project);
}

int main(int argc, char **argv) {
  std::unique_ptr<SgProject> project(frontend(argc, argv));
  ROSE_ASSERT(frontendExitStatus(project.get()) == 0);
  verify(project.get());
  SgProject *copy = SageInterface::deepCopy(project.get());
  verify(copy);
  SgFileList *copied_files = copy->get_fileList_ptr();
  SageInterface::deleteAST(copy,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(copy));
  ROSE_ASSERT(!SgNode::isLiveNode(copied_files));
  verify(project.get());
  auto *file = isSgSourceFile(&project->get_file(0));
  file->get_originalCommandLineArgumentList().push_back(
      "-rex:ast-json-checkpoint=pre-omp-construction");
  auto *restored = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction);
  ROSE_ASSERT(restored != nullptr && restored != file);
  verify(project.get());
  return backend(project.get());
}
