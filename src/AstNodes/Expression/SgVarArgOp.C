#include <sage3basic.h>

// This function helps to provide a uniform interface even though the type is
// help in a field called p_expression_type.
SgType *SgVarArgOp::get_type() const {
  // This function returns an explicitly stored type

  SgType *returnType = p_expression_type;

  ROSE_ASSERT(returnType != NULL);
  return returnType;
}

int SgVarArgOp::replace_expression(SgExpression *o, SgExpression *n) {

  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);

  if (get_operand_expr() == o) {
    set_operand_expr(n);
    return 1;
  } else {
    printf("Warning: inside of SgVarArgOp::replace_expression original "
           "SgExpression unidentified \n");
    return 0;
  }
}
