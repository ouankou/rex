#ifndef REX_FRONTEND_FUNCTION_BODY_AUXILIARY_PROVENANCE_HPP
#define REX_FRONTEND_FUNCTION_BODY_AUXILIARY_PROVENANCE_HPP

namespace RexFunctionBodyAuxiliaryProvenance {

template <class Value> int rex_body_auxiliary_contract(const Value &value) {
  typedef Value RexLocalValue;
  return sizeof(RexLocalValue) == sizeof(value) ? 0 : 1;
}

} // namespace RexFunctionBodyAuxiliaryProvenance

#endif
