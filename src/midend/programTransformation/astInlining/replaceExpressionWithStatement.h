#ifndef REPLACEEXPRESSIONWITHSTATEMENT_H
#define REPLACEEXPRESSIONWITHSTATEMENT_H

//! Get the topmost enclosing expression of a given expression.
SgExpression *getRootOfExpression(SgExpression *n);

//! Rewrites a loop so that the official test is changed to
//! "true" and what had previously been the test is now an if-break
//! combination at the beginning of a while/for body or the end of a do-while
//! body.
//!
//! For example, "while (a < 5) ++a;" becomes:
//! "while (true) {bool temp = (a < 5); if (!temp) break; ++a;}"
void pushTestIntoBody(SgScopeStatement *loopStmt);

//! Insert a new statement before or after a target statement.  If
//! allowForInit is true, the new statement can be inserted into the
//! initializer of a for statement.
void myStatementInsert(SgStatement *target, SgStatement *newstmt, bool before,
                       bool allowForInit = false);

//! Replace the expression "from" with another expression "to", wherever it
//! appears in the AST.  The expression "from" is not deleted, and may be
//! reused elsewhere in the AST.
void replaceExpressionWithExpression(SgExpression *from, SgExpression *to);

//! Convert something like "int a = foo();" into "int a; a = foo();"
SgAssignOp *convertInitializerIntoAssignment(SgAssignInitializer *init);

//! Get the statement of an expression.
SgStatement *getStatementOfExpression(SgNode *n);

#endif // REPLACEEXPRESSIONWITHSTATEMENT_H
