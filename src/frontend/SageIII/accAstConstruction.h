#ifndef ROSE_ACC_AST_CONSTRUCTION_H
#define ROSE_ACC_AST_CONSTRUCTION_H

#include "OpenACCIR.h"

class SgPragmaDeclaration;
class SgStatement;

void validateOpenACCDirectiveForSage(const openacc::Directive &);
SgStatement *convertOpenACCDirective(SgPragmaDeclaration *,
                                     const openacc::Directive &);

#endif
