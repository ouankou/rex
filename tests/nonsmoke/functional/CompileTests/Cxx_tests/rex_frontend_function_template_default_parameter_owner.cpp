template <class T> struct RexFunctionTemplateTypeIdentity {
  using type = T;
};

template <class TypeIdentity, class NestedType = typename TypeIdentity::type>
constexpr typename TypeIdentity::type
rex_function_template_default_parameter_owner(TypeIdentity) {
  return {};
}

int rex_function_template_default_parameter_owner_result =
    rex_function_template_default_parameter_owner(
        RexFunctionTemplateTypeIdentity<int>{});
