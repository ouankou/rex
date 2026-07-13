#include "sage3basic.h"

void SgCallExpression::post_construction_initialization() {
  if (p_function != NULL)
    p_function->set_parent(this);
}

SgExpression *SgCallExpression::get_next(int &n) const {
  if (n == 0) {
    n++;
    return get_function();
  } else if (p_args && n == 1) {
    n++;
    return get_args();
  }

  return 0;
}

// DQ: trying to remove the nested iterator class
void SgCallExpression::append_arg(SgExpression *what) {
  if (p_args == nullptr || what == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[call-arguments]: %s cannot append argument=%p "
            "to list=%p; both must be non-null\n",
            class_name().c_str(), static_cast<void *>(what),
            static_cast<void *>(p_args));
    ROSE_ABORT();
  }
  p_args->append_expression(what);
}

int SgCallExpression::replace_expression(SgExpression *o, SgExpression *n) {
  // DQ (12/17/2006): This function should have the semantics that it will
  // represent a structural change to the AST, thus it is free to set the parent
  // of the new expression.

  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);

  if (get_function() == o) {
    set_function(n);
    n->set_parent(this);

    return 1;
  } else {
    if (p_args == o) {
      // DQ (12/17/2006): Make this code safer (avoid passing NULL pointers to
      // functions that we call! set_args(isSgExprListExp(n));
      SgExprListExp *expressionList = isSgExprListExp(n);
      ROSE_ASSERT(expressionList != NULL);
      set_args(expressionList);
      n->set_parent(this);
      return 1;
    }
  }

  return 0;
}

SgType *SgCallExpression::get_type() const {
  if (p_expression_type == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[call-result-type]: %s has no exact semantic "
            "result type\n",
            class_name().c_str());
    ROSE_ABORT();
  }
  if (isSgFunctionType(p_expression_type) != nullptr ||
      isSgMemberFunctionType(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[call-result-type]: %s stores a callable type "
            "instead of its exact semantic result type\n",
            class_name().c_str());
    ROSE_ABORT();
  }
  if (isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[call-result-type]: %s stores placeholder type "
            "%s instead of an exact semantic result type\n",
            class_name().c_str(), p_expression_type->class_name().c_str());
    ROSE_ABORT();
  }
  return p_expression_type;
}
