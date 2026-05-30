#ifndef ROSE_SG_LOGICAL_BINARY_OP_SUPPORT_H
#define ROSE_SG_LOGICAL_BINARY_OP_SUPPORT_H

class SgExpression;
class SgType;

SgType *resolveLogicalBinaryOpType(const SgExpression *lhs,
                                   const SgExpression *rhs);

#endif
