#include "sage3basic.h"

void SgTemplateParameterVal::post_construction_initialization() {
  (void)get_type();
}

SgName SgTemplateParameterVal::get_template_parameter_name() const {
  (void)get_type();
  const SgName name =
      get_template_parameter()->get_initializedName()->get_name();
  if (name.is_null()) {
    std::cerr << "REX_AST_INVARIANT[template-parameter-value-identity]: "
                 "referenced parameter has no source name"
              << std::endl;
    ROSE_ABORT();
  }
  return name;
}

SgType *SgTemplateParameterVal::get_type() const {
  SgTemplateParameter *parameter = get_template_parameter();
  SgInitializedName *name =
      parameter != nullptr ? parameter->get_initializedName() : nullptr;
  SgType *type = parameter != nullptr ? parameter->get_type() : nullptr;
  if (get_template_parameter_position() < 0 || parameter == nullptr ||
      parameter->get_parameterType() !=
          SgTemplateParameter::nontype_parameter ||
      name == nullptr || name->get_parent() != parameter || type == nullptr ||
      name->get_type() != type || SageInterface::containsUnknownType(type) ||
      isSgTypeDefault(type) != nullptr) {
    std::cerr << "REX_AST_INVARIANT[template-parameter-value-type]: "
                 "value has no exact non-type template parameter type"
              << std::endl;
    ROSE_ABORT();
  }

  if (get_literal_type() != nullptr && get_literal_type() != type) {
    std::cerr << "REX_AST_INVARIANT[template-parameter-literal-type]: "
                 "explicit literal type differs from the exact non-type "
                 "template parameter type"
              << std::endl;
    ROSE_ABORT();
  }

  return type;
}
