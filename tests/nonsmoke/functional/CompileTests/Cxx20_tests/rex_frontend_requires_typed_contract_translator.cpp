#include "RoseAst.h"
#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t requires_count = 0;
  size_t parameterized_requires_count = 0;
  size_t simple_count = 0;
  size_t type_count = 0;
  size_t compound_count = 0;
  size_t nested_count = 0;
  size_t typed_parameter_constraint_count = 0;
  bool saw_constrained_auto_value = false;
  bool saw_constrained_auto_result = false;

  auto require_exact_constrained_auto = [](SgType *type) {
    SgAutoType *auto_type = isSgAutoType(type);
    ROSE_ASSERT(auto_type != nullptr);
    ROSE_ASSERT(auto_type->get_is_constrained());
    SgNonrealRefExp *constraint = auto_type->get_type_constraint();
    ROSE_ASSERT(constraint != nullptr);
    ROSE_ASSERT(constraint->get_parent() == auto_type);
    ROSE_ASSERT(constraint->get_symbol() != nullptr);
    ROSE_ASSERT(constraint->get_symbol()->get_name() == "SameType");
    ROSE_ASSERT(constraint->get_symbol()->get_declaration()->get_is_concept());
    ROSE_ASSERT(constraint->get_templateArguments().size() == 1);
    SgTemplateArgument *argument = constraint->get_templateArguments().front();
    ROSE_ASSERT(argument->get_parent() == constraint);
    ROSE_ASSERT(argument->get_argumentType() ==
                SgTemplateArgument::type_argument);
    ROSE_ASSERT(isSgTypeInt(argument->get_type()) != nullptr);
  };

  RoseAst ast(project);
  for (RoseAst::iterator cursor = ast.begin(); cursor != ast.end(); ++cursor) {
    SgNode *node = *cursor;
    if (SgRequiresExpr *requires_expr = isSgRequiresExpr(node)) {
      ++requires_count;
      SgExprListExp *requirements = requires_expr->get_requirements();
      ROSE_ASSERT(requirements != nullptr);
      ROSE_ASSERT(requirements->get_parent() == requires_expr);
      ROSE_ASSERT(!requirements->get_expressions().empty());
      if (SgFunctionParameterList *parameters =
              requires_expr->get_local_parameter_list()) {
        ++parameterized_requires_count;
        ROSE_ASSERT(parameters->get_parent() == requires_expr);
        SgFunctionParameterScope *scope =
            requires_expr->get_local_parameter_scope();
        ROSE_ASSERT(scope != nullptr);
        ROSE_ASSERT(scope->get_parent() == requires_expr);
        for (SgInitializedName *parameter : parameters->get_args()) {
          ROSE_ASSERT(parameter != nullptr);
          ROSE_ASSERT(parameter->get_parent() == parameters);
          ROSE_ASSERT(parameter->get_scope() == scope);
          ROSE_ASSERT(parameter->get_type() != nullptr);
          ROSE_ASSERT(parameter->get_initializer() == nullptr);
        }
      } else {
        ROSE_ASSERT(requires_expr->get_local_parameter_scope() == nullptr);
      }
    } else if (SgSimpleRequirement *requirement = isSgSimpleRequirement(node)) {
      ++simple_count;
      ROSE_ASSERT(requirement->get_expression() != nullptr);
      ROSE_ASSERT(requirement->get_expression()->get_parent() == requirement);
      ROSE_ASSERT(isSgExprListExp(requirement->get_parent()) != nullptr);
    } else if (SgTypeRequirement *requirement = isSgTypeRequirement(node)) {
      ++type_count;
      ROSE_ASSERT(requirement->get_required_type() != nullptr);
      ROSE_ASSERT(isSgExprListExp(requirement->get_parent()) != nullptr);
      ROSE_ASSERT(requirement->get_explicit_name_qualification_present());
      ROSE_ASSERT(!requirement->get_explicit_global_qualification());
      ROSE_ASSERT(requirement->get_explicit_name_qualification_tokens() ==
                  SgStringList{"T::"});
    } else if (SgCompoundRequirement *requirement =
                   isSgCompoundRequirement(node)) {
      ++compound_count;
      ROSE_ASSERT(requirement->get_expression() != nullptr);
      ROSE_ASSERT(requirement->get_expression()->get_parent() == requirement);
      ROSE_ASSERT(requirement->get_noexcept_required());
      ROSE_ASSERT(requirement->get_type_constraint() != nullptr);
      ROSE_ASSERT(requirement->get_type_constraint()->get_parent() ==
                  requirement);
      ROSE_ASSERT(isSgNonrealRefExp(requirement->get_type_constraint()) !=
                  nullptr);
      ROSE_ASSERT(isSgExprListExp(requirement->get_parent()) != nullptr);
    } else if (SgNestedRequirement *requirement = isSgNestedRequirement(node)) {
      ++nested_count;
      ROSE_ASSERT(requirement->get_constraint() != nullptr);
      ROSE_ASSERT(requirement->get_constraint()->get_parent() == requirement);
      ROSE_ASSERT(isSgExprListExp(requirement->get_parent()) != nullptr);
    } else if (SgTemplateParameter *parameter = isSgTemplateParameter(node)) {
      if (SgExpression *constraint = parameter->get_typeConstraint()) {
        SgNonrealRefExp *concept_ref = isSgNonrealRefExp(constraint);
        ROSE_ASSERT(concept_ref != nullptr);
        ROSE_ASSERT(concept_ref->get_symbol() != nullptr);
        ROSE_ASSERT(concept_ref->get_parent() == parameter);
        ++typed_parameter_constraint_count;
      }
    } else if (SgInitializedName *name = isSgInitializedName(node)) {
      if (name->get_name() == "constrained_auto_value") {
        ROSE_ASSERT(isSgTypeInt(name->get_type()) != nullptr);
        require_exact_constrained_auto(name->get_auto_decltype());
        saw_constrained_auto_value = true;
      }
    } else if (SgFunctionDeclaration *function =
                   isSgFunctionDeclaration(node)) {
      if (function->get_name() == "constrained_auto_result") {
        ROSE_ASSERT(isSgTypeInt(function->get_type()->get_return_type()) !=
                    nullptr);
        SgFunctionDeclaration *written =
            isSgFunctionDeclaration(function->get_definingDeclaration());
        ROSE_ASSERT(written != nullptr && written->get_definition() != nullptr);
        ROSE_ASSERT(written->get_definition()->get_declaration() == written);
        ROSE_ASSERT(written->get_type_syntax_is_available());
        ROSE_ASSERT(written->get_type_syntax() != nullptr);
        require_exact_constrained_auto(
            written->get_type_syntax()->get_return_type());
        saw_constrained_auto_result = true;
      }
    }
  }

  ROSE_ASSERT(requires_count >= 3);
  ROSE_ASSERT(parameterized_requires_count >= 2);
  ROSE_ASSERT(simple_count >= 2);
  ROSE_ASSERT(type_count >= 2);
  ROSE_ASSERT(compound_count >= 1);
  ROSE_ASSERT(nested_count >= 1);
  ROSE_ASSERT(typed_parameter_constraint_count >= 3);
  ROSE_ASSERT(saw_constrained_auto_value);
  ROSE_ASSERT(saw_constrained_auto_result);
  return backend(project);
}
