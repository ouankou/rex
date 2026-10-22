#include "sage3basic.h"

#include <algorithm>

SgClassDeclaration *SgConstructorInitializer::get_class_decl() const {
  SgClassDeclaration *class_decl = nullptr;
  if (SgMemberFunctionDeclaration *fdecl = get_declaration()) {
    ROSE_ASSERT(isSgClassDefinition(fdecl->get_scope()) != nullptr);
    class_decl = isSgClassDeclaration(fdecl->get_associatedClassDeclaration());
    ROSE_ASSERT(class_decl != nullptr);
  } else {
    SgType *expr_type = get_expression_type();
    ROSE_ASSERT(expr_type != nullptr);
    SgType *stripped_type = expr_type->stripType(
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
        SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);

    if (SgClassType *class_type = isSgClassType(stripped_type)) {
      class_decl = isSgClassDeclaration(class_type->get_declaration());
      ROSE_ASSERT(class_decl != NULL);
    } else if (SgNonrealType *nonreal_type = isSgNonrealType(stripped_type)) {
      auto *nonreal_decl = isSgNonrealDecl(nonreal_type->get_declaration());
      ROSE_ASSERT(nonreal_decl != nullptr);
      class_decl =
          isSgClassDeclaration(nonreal_decl->get_templateDeclaration());
    }
  }
  if (class_decl == nullptr)
    return nullptr;
  auto *canonical =
      isSgClassDeclaration(class_decl->get_firstNondefiningDeclaration());
  ROSE_ASSERT(canonical != nullptr);
  return canonical;
}

void SgConstructorInitializer::post_construction_initialization() {
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[exact-expression-type]: "
            "node=SgConstructorInitializer has no exact semantic result "
            "type\n");
    ROSE_ABORT();
  }
  if (p_declaration == NULL) {
    SgType *associated_type = p_expression_type->stripType(
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
        SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
    bool has_associated_class = false;
    if (SgClassType *class_type = isSgClassType(associated_type)) {
      has_associated_class = class_type->get_declaration() != NULL;
    } else if (SgNonrealType *nonreal_type = isSgNonrealType(associated_type)) {
      if (SgNonrealDecl *nonreal_decl =
              isSgNonrealDecl(nonreal_type->get_declaration())) {
        has_associated_class =
            isSgClassDeclaration(nonreal_decl->get_templateDeclaration()) !=
            NULL;
      }
    }
    ROSE_ASSERT(has_associated_class || (p_associated_class_unknown == true));
  } else {
    ROSE_ASSERT(p_expression_type != NULL);
  }

  if (p_args == NULL) {
    fprintf(stderr, "REX_AST_INVARIANT[constructor-initializer-arguments]: "
                    "constructor initializer has no exact argument list\n");
    ROSE_ABORT();
  }
  if (p_args->get_parent() != nullptr && p_args->get_parent() != this) {
    fprintf(stderr, "REX_AST_INVARIANT[constructor-initializer-arguments]: "
                    "constructor initializer argument list is already owned\n");
    ROSE_ABORT();
  }
  ROSE_ASSERT(SgCxxDefaultInitExp::is_valid_binding(
      p_declaration, p_args->get_expressions(), 0, p_expression_type));
  ROSE_ASSERT(is_valid_argument_source(p_declaration, p_args->get_expressions(),
                                       p_argument_source));
  if (p_args->get_source_form() == SgExprListExp::e_expression_list_braced) {
    set_is_braced_initialized(true);
    p_args->set_source_form(SgExprListExp::e_expression_list_plain);
  }
  p_args->set_parent(this);
}

bool SgConstructorInitializer::is_valid_argument_source(
    SgMemberFunctionDeclaration *declaration,
    const SgExpressionPtrList &arguments, argument_source_enum source) {
  if (declaration != nullptr && declaration->get_functionModifier().isDeleted())
    return false;
  switch (source) {
  case e_constructor_argument_expressions:
    return true;
  case e_constructor_inherited_arguments:
  case e_constructor_inherited_virtual_base_arguments:
    return arguments.empty() && declaration != nullptr &&
           declaration->get_specialFunctionModifier().isConstructor() &&
           isSgClassDefinition(declaration->get_scope()) != nullptr;
  }
  return false;
}

bool SgConstructorInitializer::is_valid_elision_arguments(
    SgMemberFunctionDeclaration *declaration,
    const SgExpressionPtrList &arguments) {
  if (declaration == nullptr ||
      declaration->get_functionModifier().isDeleted() ||
      !declaration->get_specialFunctionModifier().isConstructor() ||
      isSgClassDefinition(declaration->get_scope()) == nullptr)
    return false;
  const auto role = declaration->get_functionModifier().getCopyMoveKind();
  if (role != SgFunctionModifier::e_copy_constructor &&
      role != SgFunctionModifier::e_move_constructor)
    return false;
  if (arguments.empty() || arguments.front() == nullptr ||
      isSgCxxDefaultInitExp(arguments.front()) != nullptr ||
      !SgCxxDefaultInitExp::is_valid_binding(declaration, arguments))
    return false;
  return std::all_of(arguments.begin() + 1, arguments.end(),
                     [](SgExpression *argument) {
                       return isSgCxxDefaultInitExp(argument) != nullptr;
                     });
}

void SgConstructorInitializer::validate_argument_context() const {
  ROSE_ASSERT(p_args != nullptr);
  ROSE_ASSERT(is_valid_argument_source(p_declaration, p_args->get_expressions(),
                                       p_argument_source));
  if (p_argument_source != e_constructor_argument_expressions) {
    const auto *name = isSgInitializedName(get_parent());
    ROSE_ASSERT(name != nullptr && name->get_initializer() == this);
    ROSE_ASSERT(name->get_preinitialization() ==
                    SgInitializedName::e_virtual_base_class ||
                name->get_preinitialization() ==
                    SgInitializedName::e_nonvirtual_base_class);
    ROSE_ASSERT(!name->get_ctor_initializer_source_order().has_value());
    const auto *list = isSgCtorInitializerList(name->get_parent());
    ROSE_ASSERT(list != nullptr);
    ROSE_ASSERT(std::count(list->get_ctors().begin(), list->get_ctors().end(),
                           name) == 1);
    const auto *owner = isSgMemberFunctionDeclaration(list->get_parent());
    ROSE_ASSERT(owner != nullptr && owner->get_CtorInitializerList() == list);
    ROSE_ASSERT(owner->get_specialFunctionModifier().isConstructor());
  }
}

void SgConstructorInitializer::validate_elision() const {
  ROSE_ASSERT(p_args != nullptr);
  ROSE_ASSERT(is_valid_argument_source(p_declaration, p_args->get_expressions(),
                                       p_argument_source));
  ROSE_ASSERT(p_argument_source == e_constructor_argument_expressions ||
              (!p_is_elidable && !p_elision_permitted));
  ROSE_ASSERT(!p_elision_permitted || p_is_elidable);
  if (!p_is_elidable)
    return;

  ROSE_ASSERT(p_declaration != nullptr);
  ROSE_ASSERT(p_declaration->get_specialFunctionModifier().isConstructor());
  ROSE_ASSERT(isSgClassDefinition(p_declaration->get_scope()) != nullptr);
  ROSE_ASSERT(p_expression_type != nullptr);
  ROSE_ASSERT(isSgTypeUnknown(p_expression_type) == nullptr);
  ROSE_ASSERT(isSgTypeDefault(p_expression_type) == nullptr);
  ROSE_ASSERT(p_args != nullptr && p_args->get_parent() == this);
  ROSE_ASSERT(
      is_valid_elision_arguments(p_declaration, p_args->get_expressions()));
  SgExpression *operand = p_args->get_expressions().front();
  ROSE_ASSERT(operand != nullptr && operand->get_parent() == p_args);
  ROSE_ASSERT(!p_need_name && !p_need_qualifier);
  ROSE_ASSERT(!p_need_parenthesis_after_name || !get_is_braced_initialized());
  ROSE_ASSERT(!p_explicit_name_qualification_present);
  ROSE_ASSERT(!p_explicit_global_qualification);
  ROSE_ASSERT(p_explicit_name_qualification_tokens.empty());
  ROSE_ASSERT(p_source_type_elaboration_kind ==
              SgNonrealDecl::e_source_elaboration_unspecified);
}

SgExpression *SgConstructorInitializer::get_next(int &n) const {
  if (n == 0) {
    n++;
    return get_args();
  }
  return 0;
}

int SgConstructorInitializer::replace_expression(SgExpression *o,
                                                 SgExpression *n) {
  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);
  if (get_args() != o)
    return 0;
  ROSE_ASSERT(o->get_parent() == this);
  if (o == n)
    return 1;

  auto *arguments = isSgExprListExp(n);
  ROSE_ASSERT(arguments != nullptr);
  ROSE_ASSERT(n->get_parent() == nullptr);
  ROSE_ASSERT(is_valid_argument_source(
      p_declaration, arguments->get_expressions(), p_argument_source));
  if (arguments->get_source_form() == SgExprListExp::e_expression_list_braced) {
    ROSE_ASSERT(get_is_braced_initialized());
    arguments->set_source_form(SgExprListExp::e_expression_list_plain);
  }
  set_args(arguments);
  n->set_parent(this);
  o->set_parent(nullptr);
  return 1;
}

// DQ (6/11/2015): Moved these six access functions, they should not be
// generated by ROSETTA so that we could avoid them setting the isModified flag
// which is a problem in the name qualification support for C++ (interfering
// with the token-based unparsing).
int SgConstructorInitializer::get_name_qualification_length() const {
  return p_name_qualification_length;
}

void SgConstructorInitializer::set_name_qualification_length(
    int name_qualification_length) {
  p_name_qualification_length = name_qualification_length;
}

bool SgConstructorInitializer::get_type_elaboration_required() const {

  return p_type_elaboration_required;
}

void SgConstructorInitializer::set_type_elaboration_required(
    bool type_elaboration_required) {
  p_type_elaboration_required = type_elaboration_required;
}

bool SgConstructorInitializer::get_global_qualification_required() const {
  return p_global_qualification_required;
}

void SgConstructorInitializer::set_global_qualification_required(
    bool global_qualification_required) {

  p_global_qualification_required = global_qualification_required;
}
