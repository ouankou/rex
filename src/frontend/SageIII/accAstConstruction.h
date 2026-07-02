#ifndef ROSE_ACC_AST_CONSTRUCTION_H
#define ROSE_ACC_AST_CONSTRUCTION_H

#include "OpenACCIR.h"

#include <utility>

class SgAccClause;
class SgAccClauseBodyStatement;
class SgAccExpressionClause;
class SgPragmaDeclaration;
class SgStatement;

bool checkOpenACCIR(OpenACCDirective *);
SgStatement *convertOpenACCDirective(
    std::pair<SgPragmaDeclaration *, OpenACCDirective *>);
SgAccClauseBodyStatement *convertOpenACCBodyDirective(
    std::pair<SgPragmaDeclaration *, OpenACCDirective *>);
SgAccExpressionClause *convertOpenACCExpressionClause(
    SgStatement *, std::pair<SgPragmaDeclaration *, OpenACCDirective *>,
    OpenACCClause *);
SgAccClause *
convertOpenACCClause(SgStatement *,
                     std::pair<SgPragmaDeclaration *, OpenACCDirective *>,
                     OpenACCClause *);

#endif
