#include "sage3basic.h"

SgType *SgThisExp::get_type() const {
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr ||
      isSgPointerType(p_expression_type) == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[this-result-type]: SgThisExp has no exact "
            "semantic pointer result type\n");
    ROSE_ABORT();
  }
  if ((p_class_symbol == nullptr) == (p_nonreal_symbol == nullptr)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[this-symbol]: SgThisExp must have exactly one "
            "class or nonreal symbol\n");
    ROSE_ABORT();
  }
  SgType *pointee_type = isSgPointerType(p_expression_type)->get_base_type();
  SgSymbol *symbol = p_class_symbol != nullptr
                         ? static_cast<SgSymbol *>(p_class_symbol)
                         : static_cast<SgSymbol *>(p_nonreal_symbol);
  SgType *symbol_type = symbol->get_type();
  if (pointee_type != nullptr) {
    pointee_type = pointee_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                           SgType::STRIP_TYPEDEF_TYPE);
  }
  if (symbol_type != nullptr) {
    symbol_type = symbol_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                         SgType::STRIP_TYPEDEF_TYPE);
  }
  if (pointee_type == nullptr || symbol_type == nullptr ||
      pointee_type != symbol_type) {
    fprintf(stderr,
            "REX_AST_INVARIANT[this-symbol-type]: SgThisExp result pointee "
            "does not match its exact class/nonreal symbol type\n");
    ROSE_ABORT();
  }
  return p_expression_type;
}
