#ifndef ROSE_OPENMP_CONSTANT_INTEGER_H
#define ROSE_OPENMP_CONSTANT_INTEGER_H

class SgExpression;

namespace Rose {
namespace OpenMP {

bool isNonnegativeConstantInteger(SgExpression *expression);

} // namespace OpenMP
} // namespace Rose

#endif
