#ifndef GENERAL_LANGUAGE_TRANSLATION_H
#define GENERAL_LANGUAGE_TRANSLATION_H

namespace LanguageTranslation {
enum FunctionModifier {
  e_function_modifier_none = 0,

  e_function_modifier_reentrant,
  e_function_modifier_recursive,

  // Fortran
  e_function_modifier_elemental,
  e_function_modifier_impure,
  e_function_modifier_module,
  e_function_modifier_pure,

  e_function_modifier_last
};

enum ExpressionKind {
  e_none = 0,
  e_unknown = 1,

  // Access modifiers
  // --------------
  e_access_modifier_public,
  e_access_modifier_private,

  // Storage modifiers
  // --------------
  e_storage_modifier_contiguous,
  e_storage_modifier_external,
  e_storage_modifier_static,
  e_storage_modifier_location,
  e_storage_modifier_cuda_constant,
  e_storage_modifier_cuda_device,
  e_storage_modifier_cuda_managed,
  e_storage_modifier_cuda_pinned,
  e_storage_modifier_cuda_shared,
  e_storage_modifier_cuda_texture,
  e_storage_modifier_cuda_unified,

  // Type modifiers
  // --------------
  e_type_modifier_list,
  e_type_modifier_allocatable,
  e_type_modifier_asynchronous,
  e_type_modifier_const,
  e_type_modifier_intent_in,
  e_type_modifier_intent_out,
  e_type_modifier_intent_inout,
  e_type_modifier_intrinsic,
  e_type_modifier_optional,
  e_type_modifier_pointer,
  e_type_modifier_protected,
  e_type_modifier_round,
  e_type_modifier_save,
  e_type_modifier_target,
  e_type_modifier_truncate,
  e_type_modifier_volatile,
  e_type_modifier_z,
  e_type_modifier_abstract,
  e_type_modifier_bind_c,
  e_type_modifier_parameter,

  // Function formal parameters
  // --------------------------
  e_param_binding_value,
};

typedef std::list<LanguageTranslation::FunctionModifier> FunctionModifierList;

} // namespace LanguageTranslation

#endif
