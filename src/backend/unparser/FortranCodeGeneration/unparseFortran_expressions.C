/* unparseFortran_expressions.C
 *
 * Code to unparse Sage/Fortran expression nodes.
 *
 */
#include "sage3basic.h"

#include "unparser.h"

using namespace std;
using namespace Rose;

void FortranCodeGeneration_locatedNode::unparseLanguageSpecificExpression(
    SgExpression *expr, SgUnparse_Info &info) {
  // This is the Fortran specific expression code generation

  switch (expr->variantT()) {
  case V_SgOmpNameExpression: {
    SgOmpNameExpression *name = isSgOmpNameExpression(expr);
    ASSERT_not_null(name);
    if (name->get_spelling().empty()) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-name]: empty OpenMP syntax "
                      "identifier\n");
      ROSE_ABORT();
    }
    curprint(name->get_spelling());
    break;
  }
  case V_SgOmpSourceExpression: {
    SgOmpSourceExpression *source = isSgOmpSourceExpression(expr);
    ASSERT_not_null(source);
    if (source->get_spelling().empty()) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-source-expression]: empty "
                      "source spelling\n");
      ROSE_ABORT();
    }
    // This node owns a complete Fortran token sequence, not a character
    // literal.  Route it through the normal token formatter so dotted
    // operators retain the required token boundaries.
    curprint(source->get_spelling());
    break;
  }
  case V_SgFortranCommonBlockRefExp: {
    SgFortranCommonBlockRefExp *reference = isSgFortranCommonBlockRefExp(expr);
    ASSERT_not_null(reference);
    SageInterface::validateFortranCommonBlockRef(reference);
    SgExprListExp *list = isSgExprListExp(reference->get_parent());
    SgNode *clause = list != nullptr ? list->get_parent() : nullptr;
    if (isSgOmpVariablesClause(clause) == nullptr &&
        isSgAccVariablesClause(clause) == nullptr &&
        isSgOmpThreadprivateStatement(reference->get_parent()) == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[fortran-common-block-context]: /%s/ "
              "is not owned by an OpenMP/OpenACC variable-list construct\n",
              reference->get_use_name().str());
      ROSE_ABORT();
    }
    curprint("/");
    curprint(reference->get_use_name().str());
    curprint("/");
    break;
  }

    // function, intrinsic calls
  case V_SgFunctionCallExp:
    unparseFuncCall(expr, info);
    break;

    // operators
  case V_SgUnaryOp:
    unparseUnaryExpr(expr, info);
    break;
  case V_SgBinaryOp:
    unparseBinaryExpr(expr, info);
    break;

  case V_SgAssignOp:
    unparseAssnOp(expr, info);
    break;
  case V_SgPlusAssignOp:
  case V_SgMinusAssignOp:
  case V_SgMultAssignOp:
  case V_SgDivAssignOp:
  case V_SgModAssignOp:
  case V_SgAndAssignOp:
  case V_SgIorAssignOp:
  case V_SgXorAssignOp:
  case V_SgLshiftAssignOp:
  case V_SgRshiftAssignOp:
    unparseCompoundAssignOp(isSgCompoundAssignOp(expr), info);
    break;
  case V_SgPointerAssignOp:
    unparsePointerAssnOp(expr, info);
    break;

  case V_SgNotOp:
    unparseNotOp(expr, info);
    break;
  case V_SgAndOp:
    unparseAndOp(expr, info);
    break;
  case V_SgOrOp:
    unparseOrOp(expr, info);
    break;

  case V_SgEqualityOp:
    unparseEqOp(expr, info);
    break;
  case V_SgNotEqualOp:
    unparseNeOp(expr, info);
    break;
  case V_SgLessThanOp:
    unparseLtOp(expr, info);
    break;
  case V_SgGreaterThanOp:
    unparseGtOp(expr, info);
    break;
  case V_SgLessOrEqualOp:
    unparseLeOp(expr, info);
    break;
  case V_SgGreaterOrEqualOp:
    unparseGeOp(expr, info);
    break;

  case V_SgMinusOp:
    unparseUnaryMinusOp(expr, info);
    break;
  case V_SgUnaryAddOp:
    unparseUnaryAddOp(expr, info);
    break;
  case V_SgAddOp:
    unparseAddOp(expr, info);
    break;
  case V_SgSubtractOp:
    unparseSubtOp(expr, info);
    break;
  case V_SgMultiplyOp:
    unparseMultOp(expr, info);
    break;
  case V_SgDivideOp:
    unparseDivOp(expr, info);
    break;
  case V_SgIntegerDivideOp:
    unparseIntDivOp(expr, info);
    break;
  case V_SgExponentiationOp:
    unparseExpOp(expr, info);
    break;
  case V_SgConcatenationOp:
    unparseConcatenationOp(expr, info);
    break;

    // intrinsics mapped to Sage operators
  case V_SgModOp:
    unparseModOp(expr, info);
    break;
  case V_SgBitXorOp:
    unparseBitXOrOp(expr, info);
    break;
  case V_SgBitAndOp:
    unparseBitAndOp(expr, info);
    break;
  case V_SgBitOrOp:
    unparseBitOrOp(expr, info);
    break;
  case V_SgLshiftOp:
    unparseLShiftOp(expr, info);
    break;
  case V_SgRshiftOp:
    unparseRShiftOp(expr, info);
    break;
  case V_SgBitComplementOp:
    unparseBitCompOp(expr, info);
    break;

    // operators, other
  case V_SgPntrArrRefExp:
    unparseArrayOp(expr, info);
    break;
  case V_SgDotExp:
    unparseRecRef(expr, info);
    break;
  case V_SgCastExp:
    unparseCastOp(expr, info);
    break;

  case V_SgNewExp:
    unparseNewOp(expr, info);
    break;
  case V_SgDeleteExp:
    unparseDeleteOp(expr, info);
    break;

  case V_SgArrowExp:
    unparsePointStOp(expr, info);
    break;
  case V_SgPointerDerefExp:
    unparseDerefOp(expr, info);
    break;
  case V_SgAddressOfOp:
    unparseAddrOp(expr, info);
    break;
  case V_SgRefExp:
    unparseTypeRef(expr, info);
    break;
  case V_SgTypeExpression:
    unparseTypeExpression(expr, info);
    break;

  case V_SgRangeExp:
    unparseRangeExp(expr, info);
    break;
  case V_SgSubscriptExpression:
    unparseSubscriptExpr(expr, info);
    break;
  case V_SgColonShapeExp:
    unparseColonShapeExp(expr, info);
    break;
  case V_SgAsteriskShapeExp:
    unparseAsteriskShapeExp(expr, info);
    break;
  case V_SgAssumedRankExp:
    curprint("..");
    break;

    // initializers
  case V_SgAggregateInitializer:
    unparseAggrInit(expr, info);
    break;
  case V_SgConstructorInitializer:
    unparseConInit(expr, info);
    break;
  case V_SgAssignInitializer:
    unparseAssnInit(expr, info);
    break;

    // IO
  case V_SgImpliedDo:
    unparseImpliedDo(expr, info);
    break;

    // symbol references
  case V_SgVarRefExp:
    unparseVarRef(expr, info);
    break;
  case V_SgFunctionRefExp:
    unparseFuncRef(expr, info);
    break;
  case V_SgMemberFunctionRefExp:
    unparseMFuncRef(expr, info);
    break;
  case V_SgClassNameRefExp:
    unparseClassRef(expr, info);
    break;

  case V_SgNullExpression:
    unparseNullExpression(expr, info);
    break;

    // DQ (11/24/2007): Support for unparsing the IR node which must be
    // post-processed into either an array reference or a function call.
  case V_SgUnknownArrayOrFunctionReference:
    unparseUnknownArrayOrFunctionReference(expr, info);
    break;

  case V_SgBoolValExp:
    unparseBoolVal(expr, info);
    break;
  case V_SgLabelRefExp:
    unparseLabelRefExp(expr, info);
    break;
  case V_SgActualArgumentExpression:
    unparseActualArgumentExpression(expr, info);
    break;

  case V_SgUserDefinedUnaryOp:
    unparseUserDefinedUnaryOp(expr, info);
    break;
  case V_SgUserDefinedBinaryOp:
    unparseUserDefinedBinaryOp(expr, info);
    break;
  case V_SgCAFCoExpression:
    unparseCoArrayExpression(expr, info);
    break;
  case V_SgCAFImageSelectorExp:
    unparseCAFImageSelectorExp(expr, info);
    break;
  case V_SgCudaKernelCallExp:
    unparseCudaKernelCall(expr, info);
    break;
  case V_SgCudaKernelExecConfig:
    unparseCudaKernelExecConfig(expr, info);
    break;

  default: {
    printf("FortranCodeGeneration_locatedNode::unparseExpression: Error: No "
           "handler for %s (variant: %d)\n",
           expr->sage_class_name(), expr->variantT());
    ROSE_ABORT();
  }
  }
}

void FortranCodeGeneration_locatedNode::unparseActualArgumentExpression(
    SgExpression *expr, SgUnparse_Info &info) {
  SgActualArgumentExpression *actualArgumentExpression =
      isSgActualArgumentExpression(expr);

  curprint(actualArgumentExpression->get_argument_name());

  // DQ (2/2/2011): Now we don't want to support the use of
  // SgActualArgumentExpression to hide a alternative return argument.  So the
  // name should never be "*". Now we use a newer implementation with
  // SgLabelRefExp instead (and a new SgTypeLabel IR node).
  ROSE_ASSERT(actualArgumentExpression->get_argument_name() != "*");
  curprint("=");

  unparseExpression(actualArgumentExpression->get_expression(), info);
}

void FortranCodeGeneration_locatedNode::unparseLabelRefExp(SgExpression *expr,
                                                           SgUnparse_Info &) {
  SgLabelRefExp *labelRefExp = isSgLabelRefExp(expr);
  ASSERT_not_null(labelRefExp);

  SgLabelSymbol *labelSymbol = labelRefExp->get_symbol();
  ASSERT_not_null(labelSymbol);

  int numericLabel = labelSymbol->get_numeric_label_value();
  ROSE_ASSERT(numericLabel >= 0);

  string numericLabelString = StringUtility::numberToString(numericLabel);

  // DQ (2/2/2011): We can't do this since it will effect where lables are
  // unparse in the OPEN statement (and likely other I/O statements). After some
  // email with Scott this is required to be handled via a special case so since
  // in all other case the SgLabelRefExp shuld have a IOStatement as a parent,
  // we will look for where the parent is part of an expression list.  This
  // could be improved later. the best way to handle this would be to do the
  // type checking. We distinguish alternative returns structurally from label
  // references owned by I/O and RETURN statements below.
  SgStatement *tmp_statement =
      SageInterface::getEnclosingStatement(labelRefExp);
  ASSERT_not_null(tmp_statement);

  // Check for either a SgIOStatement or a SgReturnStatement (not the special
  // case we are looking for)
  if (isSgIOStatement(tmp_statement) == nullptr &&
      isSgReturnStmt(tmp_statement) == nullptr) {
    // Output "*" if this is NOT a SgIOStatement (OK since I think that only
    // functions in a function CALL statement can be used with alternative IO,
    // is this true?
    curprint("*");

    // Instead of the numericLabelString, we output the index into the array of
    // arguments with type == SgLabelSymbol taken from the function
    // declaration's parameter list.
    curprint(numericLabelString);
  } else {
    if (isSgReturnStmt(tmp_statement) != NULL) {
      // This is a return statement, but we have to check if it is associated
      // with a function that has SgTypeLabel parameters. bool
      // functionHasAlternativeArgumentParameters = true;

      size_t alternativeReturnValue = 0;

      // This is always a valid value (but not be correct)... just testing for
      // now... We have to correlate this SgLabelRefExp with the SgLabelSymbol
      // of the correct parameter.

      // Note that this code is similar (copyied from) to R1236
      // c_action_return_stmt() in the ROSE Fortran support.
      SgFunctionDefinition *functionDefinition =
          SageInterface::getEnclosingFunctionDefinition(
              tmp_statement, /* includingSelf= */ true);
      ASSERT_not_null(functionDefinition);

      SgFunctionDeclaration *functionDeclaration =
          functionDefinition->get_declaration();
      ASSERT_not_null(functionDeclaration);

      SgInitializedNamePtrList &args = functionDeclaration->get_args();
      ROSE_ASSERT(alternativeReturnValue < args.size());

      // The Fortran world starts at one (not zero)!
      size_t counter = 1;

      for (size_t i = 0; i < args.size(); i++) {
        SgType *argumentType = args[i]->get_type();
        SgTypeLabel *labelType = isSgTypeLabel(argumentType);
        if (labelType != nullptr) {
          // Search the argument list for a matching symbol.
          SgSymbol *tmp_symbol = args[i]->get_symbol_from_symbol_table();
          if (tmp_symbol == labelSymbol && alternativeReturnValue < 1) {
            // We have a match.
            alternativeReturnValue = counter;
          }
          counter++;
        }
      }

      // This is using Fortran world numbering (which starts a one, not zero).
      // curprint("1");
      ROSE_ASSERT(alternativeReturnValue > 0);
      ROSE_ASSERT(alternativeReturnValue <= args.size());
      curprint(StringUtility::numberToString(alternativeReturnValue));
    } else {
      // This is the most common case.
      curprint(numericLabelString);
    }
  }
}

//----------------------------------------------------------------------------
//  FortranCodeGeneration_locatedNode::unparseExprRoot
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseExprRoot(SgExpression *,
                                                        SgUnparse_Info &) {
  std::cerr << "Error: SgExpressionRoot must not reach the Fortran unparser"
            << std::endl;
  ROSE_ABORT();
}

//----------------------------------------------------------------------------
//  FortranCodeGeneration_locatedNode::<function, intrinsic calls>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseFuncCall(SgExpression *expr,
                                                        SgUnparse_Info &info) {
  // Sage node corresponds to a Fortran function/subroutine call
  SgFunctionCallExp *func_call = isSgFunctionCallExp(expr);
  ASSERT_not_null(func_call);

  // -----------------------------------------------------
  // Unparse as pre-order subroutine/function call.
  // -----------------------------------------------------

  if (isSubroutineCall(func_call)) {
    curprint("CALL ");
  }

  // subroutine/function name
  unparseExpression(func_call->get_function(), info);

  // argument list
  SgUnparse_Info ninfo(info);
  curprint("(");
  if (func_call->get_args()) {
    SgExpressionPtrList &list = func_call->get_args()->get_expressions();
    SgExpressionPtrList::iterator arg = list.begin();
    while (arg != list.end()) {
      unparseExpression((*arg), ninfo);
      arg++;
      if (arg != list.end()) {
        curprint(",");
      }
    }
  }
  curprint(")");
}

//----------------------------------------------------------------------------
//  FortranCodeGeneration_locatedNode::<operators>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseUnaryOperator(
    SgExpression *expr, const char *op, SgUnparse_Info &info) {
  SgUnparse_Info ninfo(info);
  ninfo.set_operator_name(op);
  unparseUnaryExpr(expr, ninfo);
}

void FortranCodeGeneration_locatedNode::unparseBinaryOperator(
    SgExpression *expr, const char *op, SgUnparse_Info &info) {
  SgUnparse_Info ninfo(info);
  ninfo.set_operator_name(op);
  unparseBinaryExpr(expr, ninfo);
}

void FortranCodeGeneration_locatedNode::unparseCompoundAssignOp(
    SgCompoundAssignOp *expr, SgUnparse_Info &info) {
  ASSERT_not_null(expr);

  SgExpression *lhs = expr->get_lhs_operand();
  SgExpression *rhs = expr->get_rhs_operand();
  ASSERT_not_null(lhs);
  ASSERT_not_null(rhs);

  SgUnparse_Info operand_info(info);
  operand_info.set_nested_expression();

  auto unparse_intrinsic_assign = [&](const char *intrinsic_name) {
    curprint(intrinsic_name);
    curprint("(");
    unparseExpression(lhs, operand_info);
    curprint(", ");
    unparseExpression(rhs, operand_info);
    curprint(")");
  };

  unparseExpression(lhs, operand_info);
  curprint(" = ");

  switch (expr->variantT()) {
  case V_SgPlusAssignOp:
    unparseExpression(lhs, operand_info);
    curprint(" + ");
    unparseExpression(rhs, operand_info);
    break;
  case V_SgMinusAssignOp:
    unparseExpression(lhs, operand_info);
    curprint(" - ");
    unparseExpression(rhs, operand_info);
    break;
  case V_SgMultAssignOp:
    unparseExpression(lhs, operand_info);
    curprint(" * ");
    unparseExpression(rhs, operand_info);
    break;
  case V_SgDivAssignOp:
    unparseExpression(lhs, operand_info);
    curprint(" / ");
    unparseExpression(rhs, operand_info);
    break;
  case V_SgModAssignOp:
    unparse_intrinsic_assign("MOD");
    break;
  case V_SgAndAssignOp:
    unparse_intrinsic_assign("IAND");
    break;
  case V_SgIorAssignOp:
    unparse_intrinsic_assign("IOR");
    break;
  case V_SgXorAssignOp:
    unparse_intrinsic_assign("IEOR");
    break;
  case V_SgLshiftAssignOp:
    unparse_intrinsic_assign("ISHFT");
    break;
  case V_SgRshiftAssignOp:
    curprint("ISHFT(");
    unparseExpression(lhs, operand_info);
    curprint(", -(");
    unparseExpression(rhs, operand_info);
    curprint("))");
    break;
  default:
    printf("Error: unsupported Fortran compound assignment operator: %s\n",
           expr->class_name().c_str());
    ROSE_ABORT();
  }
}

void FortranCodeGeneration_locatedNode::unparseAssnOp(SgExpression *expr,
                                                      SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "=", info);
}

void FortranCodeGeneration_locatedNode::unparsePointerAssnOp(
    SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "=>", info);
}

void FortranCodeGeneration_locatedNode::unparseNotOp(SgExpression *expr,
                                                     SgUnparse_Info &info) {
  unparseUnaryOperator(expr, ".NOT.", info);
}

void FortranCodeGeneration_locatedNode::unparseAndOp(SgExpression *expr,
                                                     SgUnparse_Info &info) {
  // Sage node corresponds to Fortran logical-and operator
  unparseBinaryOperator(expr, ".AND.", info);
}

void FortranCodeGeneration_locatedNode::unparseOrOp(SgExpression *expr,
                                                    SgUnparse_Info &info) {
  // Sage node corresponds to Fortran logical-or operator
  unparseBinaryOperator(expr, ".OR.", info);
}

// DQ (8/6/2010): Output the logical operator when the operands are logical
// (SgBoolType) the type of the expression is not enough to test, we have to
// test the lhs and rhs type.
bool outputLogicalOperator(SgExpression *expr) {
  SgBinaryOp *binaryOp = isSgBinaryOp(expr);
  if (binaryOp == nullptr) {
    std::cerr << "Error: logical-operator selection requires SgBinaryOp"
              << std::endl;
    ROSE_ABORT();
  }

  SgExpression *lhs = binaryOp->get_lhs_operand();
  SgExpression *rhs = binaryOp->get_rhs_operand();
  ASSERT_not_null(lhs);
  ASSERT_not_null(rhs);
  SgType *lhsType = lhs->get_type();
  SgType *rhsType = rhs->get_type();
  if (lhsType == nullptr || rhsType == nullptr ||
      isSgTypeUnknown(lhsType) != nullptr ||
      isSgTypeUnknown(rhsType) != nullptr) {
    std::cerr << "Error: equality operands must have resolved types before "
                 "Fortran unparsing"
              << std::endl;
    ROSE_ABORT();
  }

  auto isLogicalType = [](SgType *type) {
    while (type != nullptr) {
      if (SgModifierType *modifier = isSgModifierType(type)) {
        type = modifier->get_base_type();
      } else if (SgArrayType *array = isSgArrayType(type)) {
        type = array->get_base_type();
      } else {
        break;
      }
    }
    return isSgTypeBool(type) != nullptr;
  };
  const bool lhsIsBool = isLogicalType(lhsType);
  const bool rhsIsBool = isLogicalType(rhsType);
  if (lhsIsBool != rhsIsBool) {
    std::cerr << "Error: Fortran equality has one LOGICAL and one non-LOGICAL "
                 "operand"
              << std::endl;
    ROSE_ABORT();
  }
  return lhsIsBool;
}

void FortranCodeGeneration_locatedNode::unparseEqOp(SgExpression *expr,
                                                    SgUnparse_Info &info) {
  // Sage node corresponds to Fortran equals operator
  ASSERT_not_null(expr);

  if (outputLogicalOperator(expr) == true) {
    unparseBinaryOperator(expr, ".EQV.", info);
  } else {
    unparseBinaryOperator(expr, ".EQ.", info);
  }
}

void FortranCodeGeneration_locatedNode::unparseNeOp(SgExpression *expr,
                                                    SgUnparse_Info &info) {
  ASSERT_not_null(expr);

  if (outputLogicalOperator(expr) == true) {
    unparseBinaryOperator(expr, ".NEQV.", info);
  } else {
    unparseBinaryOperator(expr, ".NE.", info);
  }
}

void FortranCodeGeneration_locatedNode::unparseLtOp(SgExpression *expr,
                                                    SgUnparse_Info &info) {
  // Sage node corresponds to Fortran less-than operator
  unparseBinaryOperator(expr, "<", info);
}

void FortranCodeGeneration_locatedNode::unparseGtOp(SgExpression *expr,
                                                    SgUnparse_Info &info) {
  // Sage node corresponds to Fortran greater-than operator
  unparseBinaryOperator(expr, ">", info);
}

void FortranCodeGeneration_locatedNode::unparseLeOp(SgExpression *expr,
                                                    SgUnparse_Info &info) {
  // Sage node corresponds to Fortran less-than-or-equals operator
  unparseBinaryOperator(expr, "<=", info);
}

void FortranCodeGeneration_locatedNode::unparseGeOp(SgExpression *expr,
                                                    SgUnparse_Info &info) {
  // Sage node corresponds to Fortran greater-than-or-equals operator
  unparseBinaryOperator(expr, ">=", info);
}

void FortranCodeGeneration_locatedNode::unparseUnaryMinusOp(
    SgExpression *expr, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran unary-minus operator
  unparseUnaryOperator(expr, "-", info);
}

void FortranCodeGeneration_locatedNode::unparseUnaryAddOp(
    SgExpression *expr, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran unary-plus operator
  unparseUnaryOperator(expr, "+", info);
}

void FortranCodeGeneration_locatedNode::unparseAddOp(SgExpression *expr,
                                                     SgUnparse_Info &info) {
  // Sage node corresponds to Fortran addition operator
  unparseBinaryOperator(expr, "+", info);
}

void FortranCodeGeneration_locatedNode::unparseSubtOp(SgExpression *expr,
                                                      SgUnparse_Info &info) {
  // Sage node corresponds to Fortran subtraction operator
  unparseBinaryOperator(expr, "-", info);
}

void FortranCodeGeneration_locatedNode::unparseMultOp(SgExpression *expr,
                                                      SgUnparse_Info &info) {
  // Sage node corresponds to Fortran multiplication operator
  unparseBinaryOperator(expr, "*", info);
}

void FortranCodeGeneration_locatedNode::unparseDivOp(SgExpression *expr,
                                                     SgUnparse_Info &info) {
  // Sage node corresponds to Fortran division operator
  unparseBinaryOperator(expr, "/", info);
}

void FortranCodeGeneration_locatedNode::unparseIntDivOp(SgExpression *expr,
                                                        SgUnparse_Info &info) {
  // Sage node corresponds to Fortran int-division operator
  unparseBinaryOperator(expr, "/", info);
}

void FortranCodeGeneration_locatedNode::unparseExpOp(SgExpression *expr,
                                                     SgUnparse_Info &info) {
  // Sage node corresponds to Fortran exponentiation operator
  unparseBinaryOperator(expr, "**", info);
}

//----------------------------------------------------------------------------
//  FortranCodeGeneration_locatedNode::<FIXME> (Intrinsics mapped to Sage nodes)
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseModOp(SgExpression *expr,
                                                     SgUnparse_Info &info) {
  // Sage node corresponds to Fortran mod intrinsic (remainder function)
  SgModOp *operation = isSgModOp(expr);
  ROSE_ASSERT(operation != nullptr);
  curprint("MOD(");
  unparseExpression(operation->get_lhs_operand(), info);
  curprint(", ");
  unparseExpression(operation->get_rhs_operand(), info);
  curprint(")");
}

void FortranCodeGeneration_locatedNode::unparseBitXOrOp(SgExpression *expr,
                                                        SgUnparse_Info &info) {
  // Sage node corresponds to Fortran ieor intrinsic
  SgBitXorOp *operation = isSgBitXorOp(expr);
  ROSE_ASSERT(operation != nullptr);
  curprint("IEOR(");
  unparseExpression(operation->get_lhs_operand(), info);
  curprint(", ");
  unparseExpression(operation->get_rhs_operand(), info);
  curprint(")");
}

void FortranCodeGeneration_locatedNode::unparseBitAndOp(SgExpression *expr,
                                                        SgUnparse_Info &info) {
  // Sage node corresponds to Fortran iand intrinsic
  SgBitAndOp *operation = isSgBitAndOp(expr);
  ROSE_ASSERT(operation != nullptr);
  curprint("IAND(");
  unparseExpression(operation->get_lhs_operand(), info);
  curprint(", ");
  unparseExpression(operation->get_rhs_operand(), info);
  curprint(")");
}

void FortranCodeGeneration_locatedNode::unparseBitOrOp(SgExpression *expr,
                                                       SgUnparse_Info &info) {
  // Sage node corresponds to Fortran ior intrinsic
  SgBitOrOp *operation = isSgBitOrOp(expr);
  ROSE_ASSERT(operation != nullptr);
  curprint("IOR(");
  unparseExpression(operation->get_lhs_operand(), info);
  curprint(", ");
  unparseExpression(operation->get_rhs_operand(), info);
  curprint(")");
}

void FortranCodeGeneration_locatedNode::unparseLShiftOp(SgExpression *expr,
                                                        SgUnparse_Info &info) {
  // Sage node corresponds to the Fortran left-shift intrinsic.
  SgLshiftOp *operation = isSgLshiftOp(expr);
  ROSE_ASSERT(operation != nullptr);
  curprint("ISHFT(");
  unparseExpression(operation->get_lhs_operand(), info);
  curprint(", ");
  unparseExpression(operation->get_rhs_operand(), info);
  curprint(")");
}

void FortranCodeGeneration_locatedNode::unparseRShiftOp(SgExpression *expr,
                                                        SgUnparse_Info &info) {
  // Sage node corresponds to the Fortran right-shift intrinsic.
  SgRshiftOp *operation = isSgRshiftOp(expr);
  ROSE_ASSERT(operation != nullptr);
  curprint("ISHFT(");
  unparseExpression(operation->get_lhs_operand(), info);
  curprint(", -(");
  unparseExpression(operation->get_rhs_operand(), info);
  curprint("))");
}

void FortranCodeGeneration_locatedNode::unparseBitCompOp(SgExpression *expr,
                                                         SgUnparse_Info &info) {
  // Sage node corresponds to Fortran not intrinsic
  SgBitComplementOp *operation = isSgBitComplementOp(expr);
  ROSE_ASSERT(operation != nullptr);
  curprint("NOT(");
  unparseExpression(operation->get_operand(), info);
  curprint(")");
}

void FortranCodeGeneration_locatedNode::unparseConcatenationOp(
    SgExpression *expr, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran addition operator
  unparseBinaryOperator(expr, "//", info);
}

//----------------------------------------------------------------------------
//  FortranCodeGeneration_locatedNode::<operators, other>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseArrayOp(SgExpression *expr,
                                                       SgUnparse_Info &info) {
  // Sage node corresponds to Fortran array indicing
  SgPntrArrRefExp *arrayRefExp = isSgPntrArrRefExp(expr);

  unparseExpression(arrayRefExp->get_lhs_operand(), info);

  SgUnparse_Info ninfo(info);
  ninfo.set_SkipParen();

  curprint("(");
  unparseExpression(arrayRefExp->get_rhs_operand(), ninfo);
  curprint(")");
}

void FortranCodeGeneration_locatedNode::unparseRecRef(SgExpression *expr,
                                                      SgUnparse_Info &info) {

  // FMZ (7/16/2009):
  //     cannot treat the operator "%" in same way with C/C++ modulo operator
  //     for example: X%(Y(1,2)) is not legal fortran expression
  SgDotExp *dotExpr = isSgDotExp(expr);
  unparseExpression(dotExpr->get_lhs_operand(), info);
  curprint("%");
  SgPntrArrRefExp *arrayRefExp = isSgPntrArrRefExp(dotExpr->get_rhs_operand());
  if (arrayRefExp != NULL) {
    unparseExpression(arrayRefExp->get_lhs_operand(), info);
    curprint("(");
    unparseExpression(arrayRefExp->get_rhs_operand(), info);
    curprint(")");
  } else
    unparseExpression(dotExpr->get_rhs_operand(), info);
}

void FortranCodeGeneration_locatedNode::unparseCastOp(SgExpression *expr,
                                                      SgUnparse_Info &info) {
  // DQ (8/16/2007): Allow SgCast operators to work since we wnat to test the
  // unparser using C code and we will later want to add cast operators to the
  // Fortran AST to explicitly mark implicit casts in fortran (marked as
  // compiler generated).

  // Rasmussen (5/17/2023): I think this means this code can be removed, added
  // abort to see if testing fails Perhaps the warning can be turned off as some
  // of the switch options don't abort.
  MLOG_FATAL_CXX(MLOG_UNPARSER)
      << "Case operators not defined for Fortran code generation! node = "
      << expr->class_name() << "\n";
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseNewOp(SgExpression *expr,
                                                     SgUnparse_Info &) {
  printf("Case operators not defined for Fortran code generation! node = %s \n",
         expr->class_name().c_str());
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseDeleteOp(SgExpression *expr,
                                                        SgUnparse_Info &) {
  printf("Case operators not defined for Fortran code generation! node = %s \n",
         expr->class_name().c_str());
  ROSE_ABORT();
}

//----------------------------------------------------------------------------
//  FortranCodeGeneration_locatedNode::<FIXME>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparsePointStOp(SgExpression *,
                                                         SgUnparse_Info &) {
  std::cerr << "Error: SgArrowExp has no Fortran source spelling" << std::endl;
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseDerefOp(SgExpression *,
                                                       SgUnparse_Info &) {
  std::cerr << "Error: SgPointerDerefExp has no Fortran source spelling"
            << std::endl;
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseAddrOp(SgExpression *,
                                                      SgUnparse_Info &) {
  std::cerr << "Error: SgAddressOfOp has no Fortran source spelling"
            << std::endl;
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseTypeRef(SgExpression *expr,
                                                       SgUnparse_Info &info) {
  SgRefExp *type_ref = isSgRefExp(expr);
  ASSERT_not_null(type_ref);

  SgUnparse_Info ninfo(info);
  ninfo.unset_PrintName();

  unp->u_fortran_type->unparseType(type_ref->get_type_name(), ninfo);
}

void FortranCodeGeneration_locatedNode::unparseTypeExpression(
    SgExpression *expr, SgUnparse_Info &info) {
  SgTypeExpression *type_expr = isSgTypeExpression(expr);
  ASSERT_not_null(type_expr);

  SgUnparse_Info ninfo(info);
  ninfo.unset_PrintName();
  unp->u_fortran_type->unparseType(type_expr->get_represented_type(), ninfo);
}

void FortranCodeGeneration_locatedNode::unparseRangeExp(SgExpression *expr,
                                                        SgUnparse_Info &info) {
  SgRangeExp *rangeExp = isSgRangeExp(expr);
  ASSERT_not_null(rangeExp);

  SgExpression *start = rangeExp->get_start();
  SgExpression *end = rangeExp->get_end();
  SgExpression *stride = rangeExp->get_stride();
  ASSERT_not_null(start);
  ASSERT_not_null(end);

  if (stride != nullptr && isSgNullExpression(stride) == nullptr) {
    std::cerr << "Error: SgRangeExp with a stride is not a Fortran case range"
              << std::endl;
    ROSE_ABORT();
  }

  if (isSgNullExpression(start) == nullptr) {
    unparseExpression(start, info);
  }
  curprint(":");
  if (isSgNullExpression(end) == nullptr) {
    unparseExpression(end, info);
  }
}

void FortranCodeGeneration_locatedNode::unparseSubscriptExpr(
    SgExpression *expr, SgUnparse_Info &info) {
  SgSubscriptExpression *sub_expr = isSgSubscriptExpression(expr);
  ASSERT_not_null(sub_expr);

  ASSERT_not_null(sub_expr->get_lowerBound());
  ASSERT_not_null(sub_expr->get_upperBound());
  ASSERT_not_null(sub_expr->get_stride());

  if (isSgNullExpression(sub_expr->get_lowerBound()) == NULL) {
    unparseExpression(sub_expr->get_lowerBound(), info);
    curprint(":");
  } else {
    curprint(":");
  }

  if (isSgNullExpression(sub_expr->get_upperBound()) == NULL) {
    unparseExpression(sub_expr->get_upperBound(), info);
  }

  SgExpression *strideExpression = sub_expr->get_stride();
  ASSERT_not_null(strideExpression);
  ASSERT_require(isSgNullExpression(strideExpression) == nullptr);

  SgIntVal *integerValue = isSgIntVal(strideExpression);

  // See if this is the default value for the stride (unit stride) and skip the
  // output in this case.
  bool defaultValue =
      ((integerValue != NULL) && (integerValue->get_value() == 1)) ? true
                                                                   : false;
  if (defaultValue == false) {
    curprint(":");
    ASSERT_not_null(sub_expr->get_stride());
    unparseExpression(sub_expr->get_stride(), info);
  }
}

void FortranCodeGeneration_locatedNode::unparseColonShapeExp(SgExpression *expr,
                                                             SgUnparse_Info &) {
  SgColonShapeExp *colon = isSgColonShapeExp(expr);
  ASSERT_not_null(colon);

  curprint(":");
}

void FortranCodeGeneration_locatedNode::unparseAsteriskShapeExp(
    SgExpression *expr, SgUnparse_Info &) {
  SgAsteriskShapeExp *sub_ast = isSgAsteriskShapeExp(expr);
  ASSERT_not_null(sub_ast);

  curprint("*");
}

//----------------------------------------------------------------------------
//  FortranCodeGeneration_locatedNode::<initializers>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseInitializerList(
    SgExpression *expr, SgUnparse_Info &info) {
  ROSE_ASSERT(expr);
  SgExprListExp *expr_list = isSgExprListExp(expr);

  info.set_nested_expression();

  bool paren = true;
  if (paren) {
    curprint("(/");
  }

  bool needComma = false;
  for (SgExpression *item : expr_list->get_expressions()) {
    if (needComma) {
      curprint(",");
    }
    unparseExpression(item, info);
    needComma = true;
  }

  if (paren) {
    curprint("/)");
  }

  info.unset_nested_expression();
}

void FortranCodeGeneration_locatedNode::unparseAggrInit(SgExpression *expr,
                                                        SgUnparse_Info &info) {
  SgAggregateInitializer *aggr_init = isSgAggregateInitializer(expr);
  ASSERT_not_null(aggr_init);

  if (aggr_init->get_source_form() ==
      SgAggregateInitializer::
          e_aggregate_initializer_source_fortran_structure) {
    SgExprListExp *arguments = aggr_init->get_initializers();
    SgType *semanticType = aggr_init->get_expression_type();
    SgType *sourceType = aggr_init->get_fortran_source_explicit_type();
    if (arguments == nullptr || arguments->get_parent() != aggr_init ||
        semanticType == nullptr || isSgClassType(semanticType) == nullptr ||
        !aggr_init->get_fortran_has_source_explicit_type() ||
        sourceType != semanticType ||
        aggr_init->get_fortran_source_derived_type_symbol() == nullptr) {
      std::cerr
          << "REX_UNPARSE_INVARIANT[fortran-structure-constructor]: "
             "structure constructor has no exact argument, type, and source "
             "binding contract"
          << std::endl;
      ROSE_ABORT();
    }
    SgUnparse_Info typeInfo(info);
    typeInfo.set_reference_node_for_qualification(aggr_init);
    unp->u_fortran_type->unparseType(sourceType, typeInfo, false);
    curprint("(");
    unparseExpression(arguments, info);
    curprint(")");
    return;
  }

  if (aggr_init->get_source_form() !=
      SgAggregateInitializer::e_aggregate_initializer_source_fortran) {
    std::cerr
        << "REX_UNPARSE_INVARIANT[fortran-array-constructor-source-form]: "
           "aggregate initializer has source form="
        << static_cast<int>(aggr_init->get_source_form()) << std::endl;
    ROSE_ABORT();
  }
  SgExprListExp *expr_list = aggr_init->get_initializers();
  if (expr_list == nullptr || expr_list->get_parent() != aggr_init) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-array-constructor-list]: "
                 "aggregate initializer has no exact expression list"
              << std::endl;
    ROSE_ABORT();
  }

  SgType *semanticType = aggr_init->get_expression_type();
  if (semanticType == nullptr || isSgTypeDefault(semanticType) != nullptr ||
      isSgTypeUnknown(semanticType) != nullptr) {
    std::cerr << "REX_UNPARSER_INVARIANT[fortran-array-constructor-type]: "
                 "aggregate initializer has no exact semantic expression "
                 "type"
              << std::endl;
    ROSE_ABORT();
  }

  const bool hasExplicitType =
      aggr_init->get_fortran_has_source_explicit_type();
  SgType *explicitType = aggr_init->get_fortran_source_explicit_type();
  if (hasExplicitType != (explicitType != nullptr) ||
      (explicitType != nullptr && (isSgTypeDefault(explicitType) != nullptr ||
                                   isSgTypeUnknown(explicitType) != nullptr))) {
    std::cerr
        << "REX_UNPARSER_INVARIANT[fortran-array-constructor-source-type]: "
           "explicit source-type flag and typed payload are inconsistent"
        << std::endl;
    ROSE_ABORT();
  }
  if (!hasExplicitType) {
    unparseInitializerList(expr_list, info);
    return;
  }

  info.set_nested_expression();
  curprint("(/");
  SgUnparse_Info typeInfo(info);
  typeInfo.set_reference_node_for_qualification(aggr_init);
  unp->u_fortran_type->unparseType(explicitType, typeInfo, false);
  curprint(" ::");

  bool needComma = false;
  for (SgExpression *item : expr_list->get_expressions()) {
    if (needComma) {
      curprint(",");
    }
    unparseExpression(item, info);
    needComma = true;
  }

  curprint("/)");
  info.unset_nested_expression();
}

void FortranCodeGeneration_locatedNode::unparseConInit(SgExpression *expr,
                                                       SgUnparse_Info &info) {
  // initialization of user-defined types
  SgConstructorInitializer *constructorInitializer =
      isSgConstructorInitializer(expr);
  ASSERT_not_null(constructorInitializer);

  SgType *type = constructorInitializer->get_expression_type();
  SgClassType *classType = isSgClassType(type);
  ASSERT_not_null(classType);

  string className = classType->get_name().getString();
  curprint(className);

  curprint("(");

  ASSERT_not_null(constructorInitializer->get_args());
  unparseExpression(constructorInitializer->get_args(), info);

  curprint(")");
}

void FortranCodeGeneration_locatedNode::unparseAssnInit(SgExpression *expr,
                                                        SgUnparse_Info &info) {
  // DQ (4/28/2008): This is used for simple initializers and we use the
  // SgAggregateInitializer for structures!
  SgAssignInitializer *assn_init = isSgAssignInitializer(expr);
  ASSERT_not_null(assn_init);

  unparseExpression(assn_init->get_operand(), info);
}

void FortranCodeGeneration_locatedNode::unparseImpliedDo(SgExpression *expr,
                                                         SgUnparse_Info &info) {
  // Sage node corresponds to a Fortran implied do
  SgImpliedDo *ioitem_expr = isSgImpliedDo(expr);
  ASSERT_not_null(ioitem_expr);

  SgExprListExp *object_list = ioitem_expr->get_object_list();
  SgExpression *lb = ioitem_expr->get_do_var_initialization();
  SgExpression *ub = ioitem_expr->get_last_val();
  SgExpression *step = ioitem_expr->get_increment();

  if (object_list == nullptr || object_list->empty() ||
      object_list->get_parent() != ioitem_expr) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-implied-do-object-list]: "
                 "implied-do requires one owned nonempty object list"
              << std::endl;
    ROSE_ABORT();
  }
  for (SgExpression *object : object_list->get_expressions()) {
    if (object == nullptr || object->get_parent() != object_list) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-implied-do-object-list]: "
                   "implied-do object list contains a null or foreign-owned "
                   "object"
                << std::endl;
      ROSE_ABORT();
    }
  }

  if (lb == nullptr || ub == nullptr || step == nullptr ||
      lb->get_parent() != ioitem_expr || ub->get_parent() != ioitem_expr ||
      step->get_parent() != ioitem_expr) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-implied-do-control]: "
                 "implied-do requires three exactly owned control "
                 "expressions"
              << std::endl;
    ROSE_ABORT();
  }

  curprint("(");
  unparseExprList(object_list, info);
  curprint(",");

  SgAssignOp *initialization = isSgAssignOp(lb);
  if (initialization == nullptr ||
      initialization->get_lhs_operand() == nullptr ||
      initialization->get_rhs_operand() == nullptr ||
      initialization->get_lhs_operand()->get_parent() != initialization ||
      initialization->get_rhs_operand()->get_parent() != initialization) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-implied-do-control]: "
                 "implied-do initialization must be one complete owned "
                 "assignment"
              << std::endl;
    ROSE_ABORT();
  }
  unparseExpression(initialization->get_lhs_operand(), info);
  curprint(" = ");
  unparseExpression(initialization->get_rhs_operand(), info);

  curprint(", ");
  unparseExpression(ub, info);

  // If there is an increment, and it is not the SgNullExpression, then unparse
  // it.
  if (step != NULL && isSgNullExpression(step) == NULL) {
    curprint(", ");
    unparseExpression(step, info);
  }
  curprint(")");
}

//----------------------------------------------------------------------------
//  FortranCodeGeneration_locatedNode::<symbol references>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseVarRef(SgExpression *expr,
                                                      SgUnparse_Info &) {
  // Sage node corresponds to a Fortran variable reference
  SgVarRefExp *var_ref = isSgVarRefExp(expr);
  ASSERT_not_null(var_ref);
  ASSERT_not_null(var_ref->get_symbol());

  SgInitializedName *decl = var_ref->get_symbol()->get_declaration();
  ASSERT_not_null(decl);

  curprint(var_ref->get_symbol()->get_name().str());
}

void FortranCodeGeneration_locatedNode::unparseFuncRef(SgExpression *expr,
                                                       SgUnparse_Info &) {
  // Sage node corresponds to a Fortran function reference
  SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr);
  ASSERT_not_null(func_ref);
  SgFunctionSymbol *semantic = func_ref->get_symbol();
  SgFunctionSymbol *sourceVisible =
      func_ref->get_fortran_source_visible_symbol();
  SgScopeStatement *sourceScope =
      sourceVisible != nullptr ? sourceVisible->get_scope() : nullptr;
  SgSymbolTable *sourceTable =
      sourceScope != nullptr ? sourceScope->get_symbol_table() : nullptr;
  const auto bindingKind = func_ref->get_fortran_source_visible_binding_kind();
  const bool renamedBinding =
      bindingKind ==
          SgFunctionRefExp::e_fortran_source_visible_binding_use_rename ||
      bindingKind ==
          SgFunctionRefExp::e_fortran_source_visible_binding_generic_overload;
  SgRenameSymbol *sourceRename =
      renamedBinding ? isSgRenameSymbol(sourceVisible) : nullptr;
  if (semantic == nullptr || semantic->get_declaration() == nullptr ||
      sourceVisible == nullptr || sourceVisible->get_declaration() == nullptr ||
      sourceScope == nullptr || sourceTable == nullptr ||
      sourceVisible->get_parent() != sourceTable ||
      !sourceTable->exists(sourceVisible) ||
      bindingKind ==
          SgFunctionRefExp::e_fortran_source_visible_binding_not_applicable ||
      (sourceRename != nullptr &&
       sourceRename->get_original_symbol() != semantic) ||
      (renamedBinding && sourceRename == nullptr)) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-function-source-binding]: "
                 "function reference="
              << func_ref
              << " has no exact semantic and source-visible procedure "
                 "identities\n";
    ROSE_ABORT();
  }
  string func_name = sourceVisible->get_name().str();
  curprint(func_name);
}

void FortranCodeGeneration_locatedNode::unparseMFuncRef(SgExpression *expr,
                                                        SgUnparse_Info &) {
  printf("Case operators not defined for Fortran code generation! node = %s \n",
         expr->class_name().c_str());
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseClassRef(SgExpression *expr,
                                                        SgUnparse_Info &) {
  printf("Case operators not defined for Fortran code generation! node = %s \n",
         expr->class_name().c_str());
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseStringVal(SgExpression *expr,
                                                         SgUnparse_Info &) {
  // Note that string unparsing is language dependent so this is not handled by
  // the language independent base class.

  // Sage node corresponds to a Fortran string constant
  SgStringVal *str_val = isSgStringVal(expr);
  ASSERT_not_null(str_val);

  Unparser::FortranDirectiveKind expectedDirective =
      Unparser::FortranDirectiveKind::none;
  for (SgNode *ancestor = str_val->get_parent(); ancestor != nullptr;
       ancestor = ancestor->get_parent()) {
    if (isSgOmpClause(ancestor) != nullptr) {
      if (expectedDirective == Unparser::FortranDirectiveKind::openacc) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[fortran-string]: literal has both "
                "OpenMP and OpenACC clause ancestors\n");
        ROSE_ABORT();
      }
      expectedDirective = Unparser::FortranDirectiveKind::openmp;
    } else if (isSgAccClause(ancestor) != nullptr) {
      if (expectedDirective == Unparser::FortranDirectiveKind::openmp) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[fortran-string]: literal has both "
                "OpenMP and OpenACC clause ancestors\n");
        ROSE_ABORT();
      }
      expectedDirective = Unparser::FortranDirectiveKind::openacc;
    }
  }
  if (expectedDirective == Unparser::FortranDirectiveKind::none) {
    if (unp->getFortranDirectiveKind() !=
        Unparser::FortranDirectiveKind::none) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[fortran-directive-context]: ordinary "
              "string emitted inside a directive header\n");
      ROSE_ABORT();
    }
  } else {
    unp->requireFortranDirectiveKind(expectedDirective);
  }

  const char delimiter = str_val->get_stringDelimiter();
  if (delimiter == 'H') {
    const string &value = str_val->get_value();
    if (value.find_first_of("\r\n") != string::npos) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[fortran-string]: Hollerith payload "
              "contains a physical newline\n");
      ROSE_ABORT();
    }
    const std::string literal = std::to_string(value.size()) + "H" + value;
    if (unp->cur.get_compact_output()) {
      unp->cur.emit_literal(literal);
    } else {
      unp->emitFortranText(literal);
    }
    return;
  }
  if (delimiter != '\'' && delimiter != '"') {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[fortran-string]: SgStringVal has no "
                    "Fortran source delimiter\n");
    ROSE_ABORT();
  }
  unp->emitFortranCharacterLiteral(str_val->get_value(), delimiter);
}

//----------------------------------------------------------------------------
//  void FortranCodeGeneration_locatedNode::<constants>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseLongLongIntVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgLongLongIntVal *value = isSgLongLongIntVal(expr);
  ROSE_ASSERT(value != nullptr);
  UnparseLanguageIndependentConstructs::unparseLongLongIntVal(expr, info);
  if (value->get_valueString().empty())
    curprint("_8");
}

void FortranCodeGeneration_locatedNode::unparseBoolVal(SgExpression *expr,
                                                       SgUnparse_Info &) {
  // Sage node corresponds to a Fortran logical constant
  SgBoolValExp *bool_val = isSgBoolValExp(expr);
  ASSERT_not_null(bool_val);

  if (bool_val->get_value() == true) {
    curprint(".TRUE.");
  } else {
    curprint(".FALSE.");
  }
}

void FortranCodeGeneration_locatedNode::unparseNullptrVal(SgExpression *expr,
                                                          SgUnparse_Info &) {
  ASSERT_not_null(isSgNullptrValExp(expr));
  curprint("NULL()");
}

//----------------------------------------------------------------------------
//  helpers
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseExprList(SgExpression *expr,
                                                        SgUnparse_Info &info) {
  ROSE_ASSERT(expr);
  SgExprListExp *expr_list = isSgExprListExp(expr);

  // DQ (3/28/2017): Removed this from the function parameter list so that it
  // would match the base class virtual function. This is part of removing
  // warnings from ROSE specific to Clang.
  bool paren = false;

  info.set_nested_expression();

  if (paren) {
    curprint("(");
  }
  bool needComma = false;
  for (SgExpression *item : expr_list->get_expressions()) {
    if (needComma) {
      curprint(",");
    }
    unparseExpression(item, info);
    needComma = true;
  }
  if (paren) {
    curprint(")");
  }

  info.unset_nested_expression();
}

bool FortranCodeGeneration_locatedNode::isSubroutineCall(
    SgFunctionCallExp *fcall) {
  // Returns true if this is a subroutine call (as opposed to a function call)

  // Subroutine calls appear as standalone expression statements; anything
  // nested in a larger expression must be treated as a function call.
  if (SgExprStatement *exprStmt = isSgExprStatement(fcall->get_parent())) {
    if (exprStmt->get_expression() != fcall) {
      return false;
    }
    SgExpression *expr = exprStmt->get_expression();
    SgNode *exprContext = exprStmt->get_parent();
    if (SgIfStmt *ifStmt = isSgIfStmt(exprContext)) {
      if (ifStmt->get_conditional() == exprStmt) {
        return false;
      }
    }
    if (SgWhileStmt *whileStmt = isSgWhileStmt(exprContext)) {
      if (whileStmt->get_condition() == exprStmt) {
        return false;
      }
    }
    if (SgDoWhileStmt *doWhileStmt = isSgDoWhileStmt(exprContext)) {
      if (doWhileStmt->get_condition() == exprStmt) {
        return false;
      }
    }
    if (SgForStatement *forStmt = isSgForStatement(exprContext)) {
      if (forStmt->get_test() == exprStmt || forStmt->get_increment() == expr) {
        return false;
      }
    }
    if (SgFortranDo *fortranDo = isSgFortranDo(exprContext)) {
      if (fortranDo->get_bound() == expr) {
        return false;
      }
    }
    if (SgWhereStatement *whereStmt = isSgWhereStatement(exprContext)) {
      if (whereStmt->get_condition() == expr) {
        return false;
      }
    }
    if (SgElseWhereStatement *elseWhereStmt =
            isSgElseWhereStatement(exprContext)) {
      if (elseWhereStmt->get_condition() == expr) {
        return false;
      }
    }
  } else {
    return false;
  }

  SgExpression *functionExpr = fcall->get_function();
  ASSERT_not_null(functionExpr);

  // The abstract expression node deliberately has no semantic type accessor.
  // Diagnose that malformed call target here instead of invoking its virtual
  // base implementation, whose generic abort would hide the Fortran contract.
  if (functionExpr->variantT() == V_SgExpression) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-call-target-type]: standalone "
            "call target type=%s has no exact semantic procedure type\n",
            functionExpr->class_name().c_str());
    ROSE_ABORT();
  }

  auto stripModifierType = [](SgType *type) {
    while (auto *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    }
    return type;
  };

  auto stripPointerType = [&](SgType *type) {
    type = stripModifierType(type);
    if (auto *pointer = isSgPointerType(type)) {
      type = pointer->get_base_type();
    }
    return stripModifierType(type);
  };

  SgType *targetType = functionExpr->get_type();
  if (targetType == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-call-target-type]: standalone "
            "call target type=%s has no exact semantic procedure type\n",
            functionExpr->class_name().c_str());
    ROSE_ABORT();
  }

  SgType *type = stripPointerType(targetType);
  if (SgFunctionType *funcType = isSgFunctionType(type)) {
    if (funcType->get_return_type() == nullptr) {
      std::cerr << "Error: indirect Fortran call has no return type"
                << std::endl;
      ROSE_ABORT();
    }
    return isSgTypeVoid(funcType->get_return_type()) != nullptr;
  }
  if (SgMemberFunctionType *funcType = isSgMemberFunctionType(type)) {
    if (funcType->get_return_type() == nullptr) {
      std::cerr << "Error: indirect Fortran member call has no return type"
                << std::endl;
      ROSE_ABORT();
    }
    return isSgTypeVoid(funcType->get_return_type()) != nullptr;
  }

  std::cerr << "Error: standalone Fortran call target has no resolved "
               "procedure type"
            << std::endl;
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseUnknownArrayOrFunctionReference(
    SgExpression *expr, SgUnparse_Info &) {
  ASSERT_not_null(isSgUnknownArrayOrFunctionReference(expr));
  std::cerr << "Error: unresolved SgUnknownArrayOrFunctionReference reached "
               "the Fortran unparser"
            << std::endl;
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseUserDefinedUnaryOp(
    SgExpression *expr, SgUnparse_Info &info) {
  SgUserDefinedUnaryOp *userDefinedUnaryOp = isSgUserDefinedUnaryOp(expr);

  unparseUnaryOperator(expr, userDefinedUnaryOp->get_operator_name().str(),
                       info);
}

void FortranCodeGeneration_locatedNode::unparseUserDefinedBinaryOp(
    SgExpression *expr, SgUnparse_Info &info) {
  SgUserDefinedBinaryOp *userDefinedBinaryOp = isSgUserDefinedBinaryOp(expr);
  ASSERT_not_null(userDefinedBinaryOp);

  SgUnparse_Info operandInfo(info);
  operandInfo.set_nested_expression();

  unparseExpression(userDefinedBinaryOp->get_lhs_operand(), operandInfo);
  curprint(" ");
  curprint(userDefinedBinaryOp->get_operator_name().str());
  curprint(" ");
  unparseExpression(userDefinedBinaryOp->get_rhs_operand(), operandInfo);
}

void FortranCodeGeneration_locatedNode::unparseCoArrayExpression(
    SgExpression *expr, SgUnparse_Info &info) {
  // get subparts
  SgCAFCoExpression *coExpr = isSgCAFCoExpression(expr);
  SgExpression *referData = coExpr->get_referData();
  SgExpression *teamRank = coExpr->get_teamRank();
  SgVarRefExp *teamVarRef = coExpr->get_teamId();

  // print the data reference
  ROSE_ASSERT(referData);
  unparseExpression(referData, info);

  // print the image selector
  curprint("[");

  if (auto *selector = isSgCAFImageSelectorExp(teamRank)) {
    unparseCAFImageSelectorExp(selector, info);
  } else if (auto *exprList = isSgExprListExp(teamRank)) {
    bool needComma = false;
    for (SgExpression *item : exprList->get_expressions()) {
      if (needComma) {
        curprint(", ");
      }
      unparseExpression(item, info);
      needComma = true;
    }
  } else {
    if (teamRank) {
      SgIntVal *val = isSgIntVal(teamRank);
      if (val)
        unparseIntVal(val, info);
      else
        unparseExpression(teamRank, info);
    }

    if (teamRank && teamVarRef)
      curprint(" ");

    if (teamVarRef) {
      string name = teamVarRef->get_symbol()->get_declaration()->get_name();
      if (name == "team_world" && !teamRank)
        curprint("*");
      else if (name == "team_default" && !teamRank)
        curprint("@");
      else {
        curprint("@");
        curprint(name);
      }
    }
  }

  curprint("]");
}

void FortranCodeGeneration_locatedNode::unparseCAFImageSelectorExp(
    SgExpression *expr, SgUnparse_Info &info) {
  SgCAFImageSelectorExp *selector = isSgCAFImageSelectorExp(expr);
  ASSERT_not_null(selector);

  bool needComma = false;
  if (SgExprListExp *cosubs = selector->get_cosubscripts()) {
    for (SgExpression *item : cosubs->get_expressions()) {
      if (needComma) {
        curprint(", ");
      }
      unparseExpression(item, info);
      needComma = true;
    }
  }

  if (SgExpression *statExpr = selector->get_stat_expression()) {
    curprint(needComma ? ", STAT = " : "STAT = ");
    unparseExpression(statExpr, info);
    needComma = true;
  }

  if (SgExpression *teamExpr = selector->get_team_expression()) {
    curprint(needComma ? ", TEAM = " : "TEAM = ");
    unparseExpression(teamExpr, info);
    needComma = true;
  }

  if (SgExpression *teamNumberExpr = selector->get_team_number_expression()) {
    curprint(needComma ? ", TEAM_NUMBER = " : "TEAM_NUMBER = ");
    unparseExpression(teamNumberExpr, info);
  }
}

void FortranCodeGeneration_locatedNode::unparseCudaKernelExecConfig(
    SgExpression *expr, SgUnparse_Info &info) {
  SgCudaKernelExecConfig *exec_config = isSgCudaKernelExecConfig(expr);
  ASSERT_not_null(exec_config);

  curprint("<<<");

  SgExpression *grid_exp = exec_config->get_grid();
  ASSERT_not_null(grid_exp);
  unparseExpression(grid_exp, info);
  curprint(",");

  SgExpression *blocks_exp = exec_config->get_blocks();
  ASSERT_not_null(blocks_exp);
  unparseExpression(blocks_exp, info);

  SgExpression *shared_exp = exec_config->get_shared();
  if (shared_exp != NULL) {
    curprint(",");
    unparseExpression(shared_exp, info);

    SgExpression *stream_exp = exec_config->get_stream();
    if (stream_exp != NULL) {
      curprint(",");
      unparseExpression(stream_exp, info);
    }
  }

  curprint(">>>");
}

void FortranCodeGeneration_locatedNode::unparseCudaKernelCall(
    SgExpression *expr, SgUnparse_Info &info) {
  SgCudaKernelCallExp *kernel_call = isSgCudaKernelCallExp(expr);
  ASSERT_not_null(kernel_call);

  if (isSubroutineCall(kernel_call)) {
    curprint("CALL ");
  }

  unparseExpression(kernel_call->get_function(), info);

  SgCudaKernelExecConfig *exec_config =
      isSgCudaKernelExecConfig(kernel_call->get_exec_config());
  ASSERT_not_null(exec_config);

  unparseCudaKernelExecConfig(exec_config, info);

  curprint("(");
  if (kernel_call->get_args() != NULL) {
    SgExpressionPtrList &list = kernel_call->get_args()->get_expressions();
    SgExpressionPtrList::iterator arg = list.begin();
    while (arg != list.end()) {
      unparseExpression((*arg), info);
      arg++;
      if (arg != list.end())
        curprint(",");
    }
  }
  curprint(")");
}

bool FortranCodeGeneration_locatedNode::requiresParentheses(
    SgExpression *expr, SgUnparse_Info &info) {
  // same as in base class except always respect 'need_paren' property of a node
  // seems like this would be a good idea in general, but it breaks C++
  // unparsing for some reason

  if (expr->get_need_paren())
    return true;
  else
    return UnparseLanguageIndependentConstructs::requiresParentheses(expr,
                                                                     info);
}

PrecedenceSpecifier
FortranCodeGeneration_locatedNode::getPrecedence(SgExpression *exp) {
  // same as in base class except unary plus/minus have equal precedence with
  // binary plus.
  return (isSgMinusOp(exp) || isSgUnaryAddOp(exp)
              ? additiveOperatorPrecedence()
              : UnparseLanguageIndependentConstructs::getPrecedence(exp));
}
