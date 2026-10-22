#include "sage3basic.h"

void SgCallExpression::post_construction_initialization() {
  if ((p_function != nullptr && p_function == p_args) ||
      (p_function != nullptr && p_function->get_parent() != nullptr &&
       p_function->get_parent() != this) ||
      (p_args != nullptr && p_args->get_parent() != nullptr &&
       p_args->get_parent() != this)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[call-ownership]: callee and argument list "
            "must be distinct and exclusively owned\n");
    ROSE_ABORT();
  }
  if (p_function != nullptr && p_function->get_parent() != this)
    p_function->set_parent(this);
  if (p_args != nullptr && p_args->get_parent() != this)
    p_args->set_parent(this);
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
  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);
  const int edges = (get_function() == o) + (p_args == o);
  if (edges == 0)
    return 0;
  ROSE_ASSERT(edges == 1);
  ROSE_ASSERT(o->get_parent() == this);
  if (o == n)
    return 1;
  ROSE_ASSERT(n->get_parent() == nullptr);

  if (get_function() == o) {
    set_function(n);
  } else if (p_args == o) {
    SgExprListExp *expressionList = isSgExprListExp(n);
    ROSE_ASSERT(expressionList != NULL);
    set_args(expressionList);
  }
  n->set_parent(this);
  o->set_parent(nullptr);
  return 1;
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
