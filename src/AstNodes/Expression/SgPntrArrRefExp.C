#include "sage3basic.h"

SgType *SgPntrArrRefExp::get_type() const { return SgBinaryOp::get_type(); }

std::optional<long long>
SgPntrArrRefExp::get_result_storage_size_bytes() const {
  return p_result_storage_size_bytes;
}

void SgPntrArrRefExp::set_result_storage_size_bytes(
    std::optional<long long> size) {
  ROSE_ASSERT(!size || *size >= 0);
  p_result_storage_size_bytes = size;
  markAsModified();
}

const std::vector<std::optional<long long>> &
SgPntrArrRefExp::get_subscript_stride_bits() const {
  return p_subscript_stride_bits;
}

void SgPntrArrRefExp::set_subscript_stride_bits(
    const std::vector<std::optional<long long>> &strides) {
  if (!strides.empty()) {
    ASSERT_not_null(get_rhs_operand());
    const auto *indices = isSgExprListExp(get_rhs_operand());
    ROSE_ASSERT(strides.size() ==
                (indices ? indices->get_expressions().size() : 1));
  }
  p_subscript_stride_bits = strides;
  markAsModified();
}
