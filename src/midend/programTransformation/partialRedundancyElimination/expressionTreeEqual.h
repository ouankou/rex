#ifndef EXPRESSIONTREEEQUAL_H
#define EXPRESSIONTREEEQUAL_H

#include "Cxx_Grammar.h"
//! Are two expressions equal (using a deep comparison)?
bool expressionTreeEqual(SgExpression *, SgExpression *);

//! Are corresponding expressions in two lists equal (using a deep comparison)?
bool expressionTreeEqualStar(const SgExpressionPtrList &,
                             const SgExpressionPtrList &);

#endif // EXPRESSIONTREEEQUAL_H
