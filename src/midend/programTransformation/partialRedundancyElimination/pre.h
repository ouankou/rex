#ifndef PRE_H
#define PRE_H

#include "expressionTreeEqual.h"

#include "inlinerSupport.h"

#include "preControlFlowGraph.h"

#include "replaceExpressionWithStatement.h"

#include <vector>

namespace legacy {

// DQ (3/21/2006): Moved this outside of PRE namespace since it is also used in
// the finiteDifferencing transformation.
//! Are any variables in syms modified anywhere within n, or is n a declaration
//! of one of them?
bool anyOfListPotentiallyModifiedIn(const std::vector<SgVariableSymbol *> &syms,
                                    SgNode *n);

//! Is expr guaranteed to be computed when root completes normally?
//! Changes to expr's inputs require a separate check.
bool expressionComputedIn(SgExpression *expr, SgNode *root);

//! Count guaranteed computations, respecting conditional and unevaluated
//! operands. Changes to expr's inputs require a separate check.
int countComputationsOfExpressionIn(SgExpression *expr, SgNode *root);

namespace PRE {

//! Do partial redundancy elimination, looking for copies of one expression expr
//! within the basic block root.  A control flow graph for root must be provided
//! in cfg, with a map from nodes to their statements in node_statements, a map
//! from edges to their CFG edge types in edge_type, and a map from edges to
//! their insertion points in edge_insertion_point.  The algorithm used is that
//! of Paleri, Srikant, and Shankar ("Partial redundancy elimination: a simple,
//! pragmatic, and provably correct algorithm", Science of Computer Programming
//! 48 (2003) 1--20).
//! Removed roots are appended to retired_expressions. The caller owns them
//! until its expression worklist and equality scans have finished, then must
//! delete them. Their former AST parents are cleared at replacement.
void partialRedundancyEliminationOne(
    SgExpression *expr, SgBasicBlock *root, const myControlFlowGraph &cfg,
    std::vector<SgExpression *> &retired_expressions);

//! Do partial redundancy for all expressions within a given function, whose
//! definition is given in n.
void partialRedundancyEliminationFunction(SgFunctionDefinition *n);

//! Do partial redundancy elimination on all functions within the scope n.
void ROSE_DLL_API partialRedundancyElimination(SgNode *n);

// closing scope of namespace: PRE
} // namespace PRE

} // namespace legacy

#endif // PRE_H
