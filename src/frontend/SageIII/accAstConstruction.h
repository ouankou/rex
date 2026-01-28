#include "sage3basic.h"

#include "OpenACCIR.h"

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
