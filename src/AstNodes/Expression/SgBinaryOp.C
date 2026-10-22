#include "sage3basic.h"

namespace {
void invalidateSubscriptStorage(SgBinaryOp *expression) {
  if (!SgCopyHelp::isConstructingCopy(expression)) {
    if (auto *reference = isSgPntrArrRefExp(expression)) {
      reference->set_result_storage_size_bytes(std::nullopt);
      reference->set_subscript_stride_bits({});
    }
  }
}
} // namespace

void SgBinaryOp::post_construction_initialization() {
  if (get_lhs_operand())
    get_lhs_operand()->set_parent(this);
  if (get_rhs_operand())
    get_rhs_operand()->set_parent(this);
}

SgExpression *SgBinaryOp::get_lhs_operand() const {
  return get_lhs_operand_i();
}

SgExpression *SgBinaryOp::get_lhs_operand_i() const { return p_lhs_operand_i; }

void SgBinaryOp::set_lhs_operand_i(SgExpression *exp) {
  if (p_lhs_operand_i != exp)
    invalidateSubscriptStorage(this);
  p_lhs_operand_i = exp;
  markAsModified();
}

SgExpression *SgBinaryOp::get_rhs_operand_i() const { return p_rhs_operand_i; }

void SgBinaryOp::set_rhs_operand_i(SgExpression *exp) {
  if (p_rhs_operand_i != exp)
    invalidateSubscriptStorage(this);
  p_rhs_operand_i = exp;
  markAsModified();
}

void SgBinaryOp::set_lhs_operand(SgExpression *exp) {
  set_lhs_operand_i(exp);
  if (exp)
    exp->set_parent(this);
}

int SgBinaryOp::length() const { return 2; }

bool SgBinaryOp::empty() const { return false; }

SgExpression *SgBinaryOp::get_rhs_operand() const {
  return get_rhs_operand_i();
}

void SgBinaryOp::set_rhs_operand(SgExpression *exp) {
  set_rhs_operand_i(exp);
  if (exp)
    exp->set_parent(this);
}

SgType *SgBinaryOp::get_type() const {
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[binary-result-type]: expression=%s has no "
            "exact semantic result type\n",
            class_name().c_str());
    ROSE_ABORT();
  }
  return p_expression_type;
}

SgType *SgBinaryOp::get_expression_type() const { return p_expression_type; }

void SgBinaryOp::set_expression_type(SgType *type) {
  if (p_expression_type != type)
    invalidateSubscriptStorage(this);
  p_expression_type = type;
  markAsModified();
}

SgExpression *SgBinaryOp::get_next(int &n) const {
  if (n == 0) {
    n++;
    return get_lhs_operand();
  } else {
    if (n == 1) {
      n++;
      return get_rhs_operand();
    }
  }

  return 0;
}

int SgBinaryOp::replace_expression(SgExpression *o, SgExpression *n) {
  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);

  const bool lhs = get_lhs_operand() == o;
  const bool rhs = get_rhs_operand() == o;
  if (!lhs && !rhs) {
    fprintf(stderr,
            "REX_AST_INVARIANT[binary-replacement-edge]: expression=%s does "
            "not own the requested old operand\n",
            class_name().c_str());
    ROSE_ABORT();
  }
  ROSE_ASSERT(lhs != rhs);
  ROSE_ASSERT(o->get_parent() == this);
  if (o == n) {
    return 1;
  }

  ROSE_ASSERT(n->get_parent() == nullptr);
  if (lhs)
    set_lhs_operand(n);
  else
    set_rhs_operand(n);
  o->set_parent(nullptr);
  return 1;
}
