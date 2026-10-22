#ifndef ROSE_FORTRAN_INTEGER_SELECTOR_H
#define ROSE_FORTRAN_INTEGER_SELECTOR_H

#include <Rose/Integer128Value.h>
#include <optional>

class SgExpression;

namespace SageInterface {
namespace Detail {
std::optional<Rose::Integer128Value>
fortranIntegerSelectorValue(const SgExpression *);
bool isValidFortranKindSelector(const SgExpression *);
bool isValidFortranLengthSelector(const SgExpression *);
} // namespace Detail
} // namespace SageInterface

#endif
