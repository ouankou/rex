#ifndef REX_CLANG_FLOATING_VALUE_HPP
#define REX_CLANG_FLOATING_VALUE_HPP

#include "ROSE_ASSERT.h"
#include "Rose/FloatingValue.h"
#include <llvm/ADT/APFloat.h>

inline Rose::FloatingValue floatingValueFromClang(const llvm::APFloat &value) {
  using Value = Rose::FloatingValue;
  const auto &semantics = value.getSemantics();
  Value::Format format;
  if (&semantics == &llvm::APFloat::IEEEdouble())
    format = Value::Format::binary64;
  else if (&semantics == &llvm::APFloat::x87DoubleExtended())
    format = Value::Format::x87_binary80;
  else if (&semantics == &llvm::APFloat::IEEEquad())
    format = Value::Format::binary128;
  else {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[floating-value-format]: "
                    "unsupported exact extended floating format\n");
    ROSE_ABORT();
  }
  const auto bits = value.bitcastToAPInt();
  Value::Words words{};
  ROSE_ASSERT(bits.getNumWords() <= words.size());
  for (unsigned index = 0; index < bits.getNumWords(); ++index)
    words[index] = bits.getRawData()[index];
  return Value::fromBits(format, words);
}
#endif
