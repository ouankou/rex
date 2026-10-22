#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  Rose_STL_Container<SgNode *> folds =
      NodeQuery::querySubTree(project, V_SgFoldExpression);
  ROSE_ASSERT(!folds.empty());

  bool found_type_pack = false;
  bool found_template_pack = false;
  bool found_value_pack = false;
  bool found_function_pack = false;
  Rose_STL_Container<SgNode *> sizeops =
      NodeQuery::querySubTree(project, V_SgSizeOfOp);
  for (SgNode *node : sizeops) {
    SgSizeOfOp *sizeof_op = isSgSizeOfOp(node);
    if (sizeof_op == nullptr ||
        sizeof_op->get_operator_kind() != SgExpression::e_sizeof_pack) {
      continue;
    }
    (void)sizeof_op->get_type();
    if (SgTemplateType *type =
            isSgTemplateType(sizeof_op->get_operand_type())) {
      SgTemplateParameter *parameter = type->get_template_parameter();
      ROSE_ASSERT(parameter != nullptr && parameter->get_is_parameter_pack());
      if (parameter->get_parameterType() ==
          SgTemplateParameter::template_parameter) {
        found_template_pack = true;
      } else {
        ROSE_ASSERT(parameter->get_parameterType() ==
                    SgTemplateParameter::type_parameter);
        found_type_pack = true;
      }
    } else if (SgTemplateParameterVal *value =
                   isSgTemplateParameterVal(sizeof_op->get_operand_expr())) {
      ROSE_ASSERT(value->get_template_parameter() != nullptr);
      ROSE_ASSERT(value->get_template_parameter()->get_is_parameter_pack());
      found_value_pack = true;
    } else if (SgVarRefExp *value =
                   isSgVarRefExp(sizeof_op->get_operand_expr())) {
      ROSE_ASSERT(value->get_symbol() != nullptr);
      ROSE_ASSERT(value->get_symbol()->get_declaration() != nullptr);
      ROSE_ASSERT(
          value->get_symbol()->get_declaration()->get_is_parameter_pack());
      found_function_pack = true;
    } else {
      ROSE_ABORT();
    }
  }
  ROSE_ASSERT(found_type_pack);
  ROSE_ASSERT(found_template_pack);
  ROSE_ASSERT(found_value_pack);
  ROSE_ASSERT(found_function_pack);

  Rose_STL_Container<SgNode *> noexcept_ops =
      NodeQuery::querySubTree(project, V_SgNoexceptOp);
  ROSE_ASSERT(!noexcept_ops.empty());

  return backend(project);
}
