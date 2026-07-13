#include "sageAstJsonPrivate.h"

#include <functional>
#include <stdexcept>
#include <string>

namespace {

void requireFailure(const std::function<void()> &operation,
                    const std::string &expected) {
  try {
    operation();
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()).find(expected) != std::string::npos) {
      return;
    }
    throw;
  }
  throw std::runtime_error("malformed template parameter was accepted");
}

} // namespace

int main() {
  using Rose::AstJson::validateTemplateParameterContract;

  SgTemplateParameter *type_parameter =
      new SgTemplateParameter(SgTemplateParameter::type_parameter,
                              SageBuilder::buildTemplateType(SgName("T")));
  requireFailure(
      [&]() { validateTemplateParameterContract(type_parameter, "test"); },
      "no exact source keyword");
  type_parameter->set_templateParameterKeyword(
      SgTemplateParameter::keyword_typename);
  validateTemplateParameterContract(type_parameter, "test");
  SgTemplateDeclaration *invalid_type_owner =
      new SgTemplateDeclaration(SgName("invalid"));
  type_parameter->set_templateDeclaration(invalid_type_owner);
  requireFailure(
      [&]() { validateTemplateParameterContract(type_parameter, "test"); },
      "type parameter owns a template declaration");

  SgType *int_type = SageBuilder::buildIntType();
  SgTemplateParameter *non_type_parameter =
      new SgTemplateParameter(SgTemplateParameter::nontype_parameter, int_type);
  SgInitializedName *initialized_name =
      SageBuilder::buildInitializedName(SgName("N"), int_type);
  initialized_name->set_parent(non_type_parameter);
  non_type_parameter->set_initializedName(initialized_name);
  validateTemplateParameterContract(non_type_parameter, "test");
  non_type_parameter->set_initializedName(nullptr);
  requireFailure(
      [&]() { validateTemplateParameterContract(non_type_parameter, "test"); },
      "exactly one declaration or expression");
  SgInitializedName *wrong_type_name = SageBuilder::buildInitializedName(
      SgName("N"), SageBuilder::buildFloatType());
  wrong_type_name->set_parent(non_type_parameter);
  non_type_parameter->set_initializedName(wrong_type_name);
  requireFailure(
      [&]() { validateTemplateParameterContract(non_type_parameter, "test"); },
      "different semantic type");

  SgTemplateParameter *template_parameter =
      new SgTemplateParameter(SgTemplateParameter::template_parameter,
                              SageBuilder::buildTemplateType(SgName("C")));
  template_parameter->set_templateParameterKeyword(
      SgTemplateParameter::keyword_class);
  SgTemplateDeclaration *template_identity =
      new SgTemplateDeclaration(SgName("identity"));
  template_parameter->set_templateDeclaration(template_identity);
  validateTemplateParameterContract(template_parameter, "test");

  SgDeclarationScope *invalid_scope = SageBuilder::buildDeclarationScope();
  SgNonrealDecl *invalid_template_identity =
      SageBuilder::buildNonrealDecl(SgName("invalid"), invalid_scope);
  template_parameter->set_templateDeclaration(invalid_template_identity);
  requireFailure(
      [&]() { validateTemplateParameterContract(template_parameter, "test"); },
      "exact SgTemplateDeclaration");
  return 0;
}
