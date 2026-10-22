#ifndef REX_CLANG_INTEGER_VALUE_HPP
#define REX_CLANG_INTEGER_VALUE_HPP

#include "Rose/Integer128Value.h"

namespace llvm {
class APSInt;
}
class SgType;
class SgValueExp;

Rose::Integer128Value integer128ValueFromClang(const llvm::APSInt &value);
long long enumValueFromClang(const llvm::APSInt &value);
SgValueExp *buildIntegralTemplateArgExpr(const llvm::APSInt &value,
                                         SgType *type);

#endif
