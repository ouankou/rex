#include "sage3basic.h"

SgType *SgUserDefinedBinaryOp::get_type() const {
  if (SgFunctionSymbol *symbol = get_symbol()) {
    SgType *symbolType = symbol->get_type();
    if (SgFunctionType *functionType = isSgFunctionType(symbolType)) {
      if (SgType *returnType = functionType->get_return_type()) {
        return returnType;
      }
    }
    if (SgMemberFunctionType *memberFunctionType =
            isSgMemberFunctionType(symbolType)) {
      if (SgType *returnType = memberFunctionType->get_return_type()) {
        return returnType;
      }
    }
  }

  return SgBinaryOp::get_type();
}
