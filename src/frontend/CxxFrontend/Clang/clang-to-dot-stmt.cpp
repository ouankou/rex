#include "sage3basic.h"

// #include "clang-frontend-private.hpp"
#include "clang-to-dot-private.hpp"

static std::string escapeDotString(const std::string &input) {
  std::string escaped;
  escaped.reserve(input.size());
  for (char ch : input) {
    switch (ch) {
    case '\\':
      escaped += "\\\\";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '"':
      escaped += "\\\"";
      break;
    default:
      escaped += ch;
      break;
    }
  }
  return escaped;
}

// SgNode * ClangToDotTranslator::Traverse(clang::Stmt * stmt)
std::string ClangToDotTranslator::Traverse(clang::Stmt *stmt) {
  if (stmt == NULL)
    // return NULL;
    return "";

  // Look for previous translation
  std::map<clang::Stmt *, std::string>::iterator it =
      p_stmt_translation_map.find(stmt);
  if (it != p_stmt_translation_map.end())
    return it->second;

  // If first time, create a new entry
  std::string node_ident = genNextIdent();
  p_stmt_translation_map.insert(
      std::pair<clang::Stmt *, std::string>(stmt, node_ident));
  NodeDescriptor &node_desc =
      p_node_desc
          .insert(std::pair<std::string, NodeDescriptor>(
              node_ident, NodeDescriptor(node_ident)))
          .first->second;

  // SgNode * result = NULL;
  bool ret_status = false;

  // CLANG_ROSE_Graph::graph (stmt);

  switch (stmt->getStmtClass()) {
  case clang::Stmt::GCCAsmStmtClass:
    ret_status = VisitGCCAsmStmt((clang::GCCAsmStmt *)stmt, node_desc);
    break;
  case clang::Stmt::MSAsmStmtClass:
    ret_status = VisitMSAsmStmt((clang::MSAsmStmt *)stmt, node_desc);
    break;
  case clang::Stmt::BreakStmtClass:
    ret_status = VisitBreakStmt((clang::BreakStmt *)stmt, node_desc);
    break;
  case clang::Stmt::CapturedStmtClass:
    ret_status = VisitCapturedStmt((clang::CapturedStmt *)stmt, node_desc);
    break;
  case clang::Stmt::CompoundStmtClass:
    ret_status = VisitCompoundStmt((clang::CompoundStmt *)stmt, node_desc);
    break;
  case clang::Stmt::ContinueStmtClass:
    ret_status = VisitContinueStmt((clang::ContinueStmt *)stmt, node_desc);
    break;
  case clang::Stmt::CoreturnStmtClass:
    ret_status = VisitCoreturnStmt((clang::CoreturnStmt *)stmt, node_desc);
    break;
  case clang::Stmt::CXXCatchStmtClass:
    ret_status = VisitCXXCatchStmt((clang::CXXCatchStmt *)stmt, node_desc);
    break;
  case clang::Stmt::CXXForRangeStmtClass:
    ret_status =
        VisitCXXForRangeStmt((clang::CXXForRangeStmt *)stmt, node_desc);
    break;
  case clang::Stmt::CXXTryStmtClass:
    ret_status = VisitCXXTryStmt((clang::CXXTryStmt *)stmt, node_desc);
    break;
  case clang::Stmt::DeclStmtClass:
    ret_status = VisitDeclStmt((clang::DeclStmt *)stmt, node_desc);
    break;
  case clang::Stmt::DoStmtClass:
    ret_status = VisitDoStmt((clang::DoStmt *)stmt, node_desc);
    break;
  case clang::Stmt::ForStmtClass:
    ret_status = VisitForStmt((clang::ForStmt *)stmt, node_desc);
    break;
  case clang::Stmt::GotoStmtClass:
    ret_status = VisitGotoStmt((clang::GotoStmt *)stmt, node_desc);
    break;
  case clang::Stmt::IfStmtClass:
    ret_status = VisitIfStmt((clang::IfStmt *)stmt, node_desc);
    break;
  case clang::Stmt::IndirectGotoStmtClass:
    ret_status =
        VisitIndirectGotoStmt((clang::IndirectGotoStmt *)stmt, node_desc);
    break;
  case clang::Stmt::MSDependentExistsStmtClass:
    ret_status = VisitMSDependentExistsStmt(
        (clang::MSDependentExistsStmt *)stmt, node_desc);
    break;
  case clang::Stmt::NullStmtClass:
    ret_status = VisitNullStmt((clang::NullStmt *)stmt, node_desc);
    break;
  case clang::Stmt::OMPAtomicDirectiveClass:
    ret_status =
        VisitOMPAtomicDirective((clang::OMPAtomicDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPBarrierDirectiveClass:
    ret_status =
        VisitOMPBarrierDirective((clang::OMPBarrierDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPCancellationPointDirectiveClass:
    ret_status = VisitOMPCancellationPointDirective(
        (clang::OMPCancellationPointDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPCriticalDirectiveClass:
    ret_status = VisitOMPCriticalDirective((clang::OMPCriticalDirective *)stmt,
                                           node_desc);
    break;
  case clang::Stmt::OMPFlushDirectiveClass:
    ret_status =
        VisitOMPFlushDirective((clang::OMPFlushDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPDistributeDirectiveClass:
    ret_status = VisitOMPDistributeDirective(
        (clang::OMPDistributeDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPDistributeParallelForDirectiveClass:
    ret_status = VisitOMPDistributeParallelForDirective(
        (clang::OMPDistributeParallelForDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPDistributeParallelForSimdDirectiveClass:
    ret_status = VisitOMPDistributeParallelForSimdDirective(
        (clang::OMPDistributeParallelForSimdDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPDistributeSimdDirectiveClass:
    ret_status = VisitOMPDistributeSimdDirective(
        (clang::OMPDistributeSimdDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPForDirectiveClass:
    ret_status =
        VisitOMPForDirective((clang::OMPForDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPForSimdDirectiveClass:
    ret_status =
        VisitOMPForSimdDirective((clang::OMPForSimdDirective *)stmt, node_desc);
    break;
  // case clang::Stmt::OMPMasterTaskLoopDirectiveClass:
  //     ret_status =
  //     VisitOMPMasterTaskLoopDirective((clang::OMPMasterTaskLoopDirective
  //     *)stmt, node_desc); break;
  // case clang::Stmt::OMPMasterTaskLoopSimdDirectiveClass:
  //     ret_status =
  //     VisitOMPMasterTaskLoopSimdDirective((clang::OMPMasterTaskLoopSimdDirective
  //     *)stmt, node_desc); break;
  case clang::Stmt::OMPParallelForDirectiveClass:
    ret_status = VisitOMPParallelForDirective(
        (clang::OMPParallelForDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPParallelForSimdDirectiveClass:
    ret_status = VisitOMPParallelForSimdDirective(
        (clang::OMPParallelForSimdDirective *)stmt, node_desc);
    break;
  // case clang::Stmt::OMPParallelMasterTaskLoopDirectiveClass:
  //     ret_status =
  //     VisitOMPParallelMasterTaskLoopDirective((clang::OMPParallelMasterTaskLoopDirective
  //     *)stmt, node_desc); break;
  case clang::Stmt::OMPSimdDirectiveClass:
    ret_status =
        VisitOMPSimdDirective((clang::OMPSimdDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPTargetParallelForDirectiveClass:
    ret_status = VisitOMPTargetParallelForDirective(
        (clang::OMPTargetParallelForDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPTargetParallelForSimdDirectiveClass:
    ret_status = VisitOMPTargetParallelForSimdDirective(
        (clang::OMPTargetParallelForSimdDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPTargetSimdDirectiveClass:
    ret_status = VisitOMPTargetSimdDirective(
        (clang::OMPTargetSimdDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPTargetTeamsDistributeDirectiveClass:
    ret_status = VisitOMPTargetTeamsDistributeDirective(
        (clang::OMPTargetTeamsDistributeDirective *)stmt, node_desc);
    break;
  // case clang::Stmt::OMPTargetTeamsDistributeParallelForSimdDirectiveClass:
  //     ret_status =
  //     VisitOMPTargetTeamsDistributeParallelForSimdDirective((clang::OMPTargetTeamsDistributeParallelForSimdDirective
  //     *)stmt, node_desc); break;
  case clang::Stmt::OMPTargetTeamsDistributeSimdDirectiveClass:
    ret_status = VisitOMPTargetTeamsDistributeSimdDirective(
        (clang::OMPTargetTeamsDistributeSimdDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPTaskLoopDirectiveClass:
    ret_status = VisitOMPTaskLoopDirective((clang::OMPTaskLoopDirective *)stmt,
                                           node_desc);
    break;
  case clang::Stmt::OMPTaskLoopSimdDirectiveClass:
    ret_status = VisitOMPTaskLoopSimdDirective(
        (clang::OMPTaskLoopSimdDirective *)stmt, node_desc);
    break;
  // case clang::Stmt::OMPTeamDistributeDirectiveClass:
  //     ret_status =
  //     VisitOMPTeamDistributeDirective((clang::OMPTeamDistributeDirective
  //     *)stmt, node_desc); break;
  // case clang::Stmt::OMPTeamDistributeParallelForSimdDirectiveClass:
  //     ret_status =
  //     VisitOMPTeamDistributeParallelForSimdDirective((clang::OMPTeamDistributeParallelForSimdDirective
  //     *)stmt, node_desc); break;
  // case clang::Stmt::OMPTeamDistributeSimdDirectiveClass:
  //     ret_status =
  //     VisitOMPTeamDistributeSimdDirective((clang::OMPTeamDistributeSimdDirective
  //     *)stmt, node_desc); break;
  case clang::Stmt::OMPMasterDirectiveClass:
    ret_status =
        VisitOMPMasterDirective((clang::OMPMasterDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPOrderedDirectiveClass:
    ret_status =
        VisitOMPOrderedDirective((clang::OMPOrderedDirective *)stmt, node_desc);
    break;
  case clang::Stmt::OMPParallelDirectiveClass:
    ret_status = VisitOMPParallelDirective((clang::OMPParallelDirective *)stmt,
                                           node_desc);
    break;
  case clang::Stmt::OMPParallelSectionsDirectiveClass:
    ret_status = VisitOMPParallelSectionsDirective(
        (clang::OMPParallelSectionsDirective *)stmt, node_desc);
    break;
  case clang::Stmt::ReturnStmtClass:
    ret_status = VisitReturnStmt((clang::ReturnStmt *)stmt, node_desc);
    break;
  case clang::Stmt::SEHExceptStmtClass:
    ret_status = VisitSEHExceptStmt((clang::SEHExceptStmt *)stmt, node_desc);
    break;
  case clang::Stmt::SEHFinallyStmtClass:
    ret_status = VisitSEHFinallyStmt((clang::SEHFinallyStmt *)stmt, node_desc);
    break;
  case clang::Stmt::SEHLeaveStmtClass:
    ret_status = VisitSEHLeaveStmt((clang::SEHLeaveStmt *)stmt, node_desc);
    break;
  case clang::Stmt::SEHTryStmtClass:
    ret_status = VisitSEHTryStmt((clang::SEHTryStmt *)stmt, node_desc);
    break;
  case clang::Stmt::CaseStmtClass:
    ret_status = VisitCaseStmt((clang::CaseStmt *)stmt, node_desc);
    break;
  case clang::Stmt::DefaultStmtClass:
    ret_status = VisitDefaultStmt((clang::DefaultStmt *)stmt, node_desc);
    break;
  case clang::Stmt::SwitchStmtClass:
    ret_status = VisitSwitchStmt((clang::SwitchStmt *)stmt, node_desc);
    break;
  case clang::Stmt::AttributedStmtClass:
    ret_status = VisitAttributedStmt((clang::AttributedStmt *)stmt, node_desc);
    break;
  case clang::Stmt::BinaryConditionalOperatorClass:
    ret_status = VisitBinaryConditionalOperator(
        (clang::BinaryConditionalOperator *)stmt, node_desc);
    break;
  case clang::Stmt::ConditionalOperatorClass:
    ret_status =
        VisitConditionalOperator((clang::ConditionalOperator *)stmt, node_desc);
    break;
  case clang::Stmt::AddrLabelExprClass:
    ret_status = VisitAddrLabelExpr((clang::AddrLabelExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ArrayInitIndexExprClass:
    ret_status =
        VisitArrayInitIndexExpr((clang::ArrayInitIndexExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ArrayInitLoopExprClass:
    ret_status =
        VisitArrayInitLoopExpr((clang::ArrayInitLoopExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ArraySubscriptExprClass:
    ret_status =
        VisitArraySubscriptExpr((clang::ArraySubscriptExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ArrayTypeTraitExprClass:
    ret_status =
        VisitArrayTypeTraitExpr((clang::ArrayTypeTraitExpr *)stmt, node_desc);
    break;
  case clang::Stmt::AsTypeExprClass:
    ret_status = VisitAsTypeExpr((clang::AsTypeExpr *)stmt, node_desc);
    break;
  case clang::Stmt::AtomicExprClass:
    ret_status = VisitAtomicExpr((clang::AtomicExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CompoundAssignOperatorClass:
    ret_status = VisitCompoundAssignOperator(
        (clang::CompoundAssignOperator *)stmt, node_desc);
    break;
  case clang::Stmt::BlockExprClass:
    ret_status = VisitBlockExpr((clang::BlockExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CUDAKernelCallExprClass:
    ret_status =
        VisitCUDAKernelCallExpr((clang::CUDAKernelCallExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXMemberCallExprClass:
    ret_status =
        VisitCXXMemberCallExpr((clang::CXXMemberCallExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXOperatorCallExprClass:
    ret_status =
        VisitCXXOperatorCallExpr((clang::CXXOperatorCallExpr *)stmt, node_desc);
    break;
  case clang::Stmt::UserDefinedLiteralClass:
    ret_status =
        VisitUserDefinedLiteral((clang::UserDefinedLiteral *)stmt, node_desc);
    break;
  case clang::Stmt::BuiltinBitCastExprClass:
    ret_status =
        VisitBuiltinBitCastExpr((clang::BuiltinBitCastExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CStyleCastExprClass:
    ret_status = VisitCStyleCastExpr((clang::CStyleCastExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXFunctionalCastExprClass:
    ret_status = VisitCXXFunctionalCastExpr(
        (clang::CXXFunctionalCastExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXConstCastExprClass:
    ret_status =
        VisitCXXConstCastExpr((clang::CXXConstCastExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXDynamicCastExprClass:
    ret_status =
        VisitCXXDynamicCastExpr((clang::CXXDynamicCastExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXReinterpretCastExprClass:
    ret_status = VisitCXXReinterpretCastExpr(
        (clang::CXXReinterpretCastExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXStaticCastExprClass:
    ret_status =
        VisitCXXStaticCastExpr((clang::CXXStaticCastExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ImplicitCastExprClass:
    ret_status =
        VisitImplicitCastExpr((clang::ImplicitCastExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CharacterLiteralClass:
    ret_status =
        VisitCharacterLiteral((clang::CharacterLiteral *)stmt, node_desc);
    break;
  case clang::Stmt::ChooseExprClass:
    ret_status = VisitChooseExpr((clang::ChooseExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CompoundLiteralExprClass:
    ret_status =
        VisitCompoundLiteralExpr((clang::CompoundLiteralExpr *)stmt, node_desc);
    break;
  // case clang::Stmt::ConceptSpecializationExprClass:
  //     ret_status =
  //     VisitConceptSpecializationExpr((clang::ConceptSpecializationExpr
  //     *)stmt, node_desc); break;
  case clang::Stmt::ConvertVectorExprClass:
    ret_status =
        VisitConvertVectorExpr((clang::ConvertVectorExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CoawaitExprClass:
    ret_status = VisitCoawaitExpr((clang::CoawaitExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CoyieldExprClass:
    ret_status = VisitCoyieldExpr((clang::CoyieldExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXBindTemporaryExprClass:
    ret_status = VisitCXXBindTemporaryExpr((clang::CXXBindTemporaryExpr *)stmt,
                                           node_desc);
    break;
  case clang::Stmt::CXXBoolLiteralExprClass:
    ret_status =
        VisitCXXBoolLiteralExpr((clang::CXXBoolLiteralExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXConstructExprClass:
    ret_status =
        VisitCXXConstructExpr((clang::CXXConstructExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXTemporaryObjectExprClass:
    ret_status = VisitCXXTemporaryObjectExpr(
        (clang::CXXTemporaryObjectExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXDefaultArgExprClass:
    ret_status =
        VisitCXXDefaultArgExpr((clang::CXXDefaultArgExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXDefaultInitExprClass:
    ret_status =
        VisitCXXDefaultInitExpr((clang::CXXDefaultInitExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXDeleteExprClass:
    ret_status = VisitCXXDeleteExpr((clang::CXXDeleteExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXDependentScopeMemberExprClass:
    ret_status = VisitCXXDependentScopeMemberExpr(
        (clang::CXXDependentScopeMemberExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXFoldExprClass:
    ret_status = VisitCXXFoldExpr((clang::CXXFoldExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXInheritedCtorInitExprClass:
    ret_status = VisitCXXInheritedCtorInitExpr(
        (clang::CXXInheritedCtorInitExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXNewExprClass:
    ret_status = VisitCXXNewExpr((clang::CXXNewExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXNoexceptExprClass:
    ret_status =
        VisitCXXNoexceptExpr((clang::CXXNoexceptExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXNullPtrLiteralExprClass:
    ret_status = VisitCXXNullPtrLiteralExpr(
        (clang::CXXNullPtrLiteralExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXPseudoDestructorExprClass:
    ret_status = VisitCXXPseudoDestructorExpr(
        (clang::CXXPseudoDestructorExpr *)stmt, node_desc);
    break;
  // case clang::Stmt::CXXRewrittenBinaryOperatorClass:
  //     ret_status =
  //     VisitCXXRewrittenBinaryOperator((clang::CXXRewrittenBinaryOperator
  //     *)stmt, node_desc); break;
  case clang::Stmt::CXXScalarValueInitExprClass:
    ret_status = VisitCXXScalarValueInitExpr(
        (clang::CXXScalarValueInitExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXStdInitializerListExprClass:
    ret_status = VisitCXXStdInitializerListExpr(
        (clang::CXXStdInitializerListExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXThisExprClass:
    ret_status = VisitCXXThisExpr((clang::CXXThisExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXThrowExprClass:
    ret_status = VisitCXXThrowExpr((clang::CXXThrowExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXTypeidExprClass:
    ret_status = VisitCXXTypeidExpr((clang::CXXTypeidExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXUnresolvedConstructExprClass:
    ret_status = VisitCXXUnresolvedConstructExpr(
        (clang::CXXUnresolvedConstructExpr *)stmt, node_desc);
    break;
  case clang::Stmt::CXXUuidofExprClass:
    ret_status = VisitCXXUuidofExpr((clang::CXXUuidofExpr *)stmt, node_desc);
    break;
  case clang::Stmt::DeclRefExprClass:
    ret_status = VisitDeclRefExpr((clang::DeclRefExpr *)stmt, node_desc);
    break;
  case clang::Stmt::DependentCoawaitExprClass:
    ret_status = VisitDependentCoawaitExpr((clang::DependentCoawaitExpr *)stmt,
                                           node_desc);
    break;
  case clang::Stmt::DependentScopeDeclRefExprClass:
    ret_status = VisitDependentScopeDeclRefExpr(
        (clang::DependentScopeDeclRefExpr *)stmt, node_desc);
    break;
  case clang::Stmt::DesignatedInitExprClass:
    ret_status =
        VisitDesignatedInitExpr((clang::DesignatedInitExpr *)stmt, node_desc);
    break;
  case clang::Stmt::DesignatedInitUpdateExprClass:
    ret_status = VisitDesignatedInitUpdateExpr(
        (clang::DesignatedInitUpdateExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ExpressionTraitExprClass:
    ret_status =
        VisitExpressionTraitExpr((clang::ExpressionTraitExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ExtVectorElementExprClass:
    ret_status = VisitExtVectorElementExpr((clang::ExtVectorElementExpr *)stmt,
                                           node_desc);
    break;
  case clang::Stmt::FixedPointLiteralClass:
    ret_status =
        VisitFixedPointLiteral((clang::FixedPointLiteral *)stmt, node_desc);
    break;
  case clang::Stmt::FloatingLiteralClass:
    ret_status =
        VisitFloatingLiteral((clang::FloatingLiteral *)stmt, node_desc);
    break;
  case clang::Stmt::ConstantExprClass:
    ret_status = VisitConstantExpr((clang::ConstantExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ExprWithCleanupsClass:
    ret_status =
        VisitExprWithCleanups((clang::ExprWithCleanups *)stmt, node_desc);
    break;
  case clang::Stmt::FunctionParmPackExprClass:
    ret_status = VisitFunctionParmPackExpr((clang::FunctionParmPackExpr *)stmt,
                                           node_desc);
    break;
  case clang::Stmt::GenericSelectionExprClass:
    ret_status = VisitGenericSelectionExpr((clang::GenericSelectionExpr *)stmt,
                                           node_desc);
    break;
  case clang::Stmt::GNUNullExprClass:
    ret_status = VisitGNUNullExpr((clang::GNUNullExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ImaginaryLiteralClass:
    ret_status =
        VisitImaginaryLiteral((clang::ImaginaryLiteral *)stmt, node_desc);
    break;
  case clang::Stmt::ImplicitValueInitExprClass:
    ret_status = VisitImplicitValueInitExpr(
        (clang::ImplicitValueInitExpr *)stmt, node_desc);
    break;
  case clang::Stmt::InitListExprClass:
    ret_status = VisitInitListExpr((clang::InitListExpr *)stmt, node_desc);
    break;
  case clang::Stmt::IntegerLiteralClass:
    ret_status = VisitIntegerLiteral((clang::IntegerLiteral *)stmt, node_desc);
    break;
  case clang::Stmt::LambdaExprClass:
    ret_status = VisitLambdaExpr((clang::LambdaExpr *)stmt, node_desc);
    break;
  case clang::Stmt::MaterializeTemporaryExprClass:
    ret_status = VisitMaterializeTemporaryExpr(
        (clang::MaterializeTemporaryExpr *)stmt, node_desc);
    break;
  case clang::Stmt::MemberExprClass:
    ret_status = VisitMemberExpr((clang::MemberExpr *)stmt, node_desc);
    break;
  case clang::Stmt::MSPropertyRefExprClass:
    ret_status =
        VisitMSPropertyRefExpr((clang::MSPropertyRefExpr *)stmt, node_desc);
    break;
  case clang::Stmt::MSPropertySubscriptExprClass:
    ret_status = VisitMSPropertySubscriptExpr(
        (clang::MSPropertySubscriptExpr *)stmt, node_desc);
    break;
  case clang::Stmt::NoInitExprClass:
    ret_status = VisitNoInitExpr((clang::NoInitExpr *)stmt, node_desc);
    break;
  case clang::Stmt::OffsetOfExprClass:
    ret_status = VisitOffsetOfExpr((clang::OffsetOfExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ArraySectionExprClass:
    ret_status =
        VisitOMPArraySectionExpr((clang::ArraySectionExpr *)stmt, node_desc);
    break;
  case clang::Stmt::OpaqueValueExprClass:
    ret_status =
        VisitOpaqueValueExpr((clang::OpaqueValueExpr *)stmt, node_desc);
    break;
  case clang::Stmt::UnresolvedLookupExprClass:
    ret_status = VisitUnresolvedLookupExpr((clang::UnresolvedLookupExpr *)stmt,
                                           node_desc);
    break;
  case clang::Stmt::UnresolvedMemberExprClass:
    ret_status = VisitUnresolvedMemberExpr((clang::UnresolvedMemberExpr *)stmt,
                                           node_desc);
    break;
  case clang::Stmt::PackExpansionExprClass:
    ret_status =
        VisitPackExpansionExpr((clang::PackExpansionExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ParenExprClass:
    ret_status = VisitParenExpr((clang::ParenExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ParenListExprClass:
    ret_status = VisitParenListExpr((clang::ParenListExpr *)stmt, node_desc);
    break;
  case clang::Stmt::PredefinedExprClass:
    ret_status = VisitPredefinedExpr((clang::PredefinedExpr *)stmt, node_desc);
    break;
  case clang::Stmt::PseudoObjectExprClass:
    ret_status =
        VisitPseudoObjectExpr((clang::PseudoObjectExpr *)stmt, node_desc);
    break;
  case clang::Stmt::ShuffleVectorExprClass:
    ret_status =
        VisitShuffleVectorExpr((clang::ShuffleVectorExpr *)stmt, node_desc);
    break;
  case clang::Stmt::SizeOfPackExprClass:
    ret_status = VisitSizeOfPackExpr((clang::SizeOfPackExpr *)stmt, node_desc);
    break;
  case clang::Stmt::SourceLocExprClass:
    ret_status = VisitSourceLocExpr((clang::SourceLocExpr *)stmt, node_desc);
    break;
  case clang::Stmt::StmtExprClass:
    ret_status = VisitStmtExpr((clang::StmtExpr *)stmt, node_desc);
    break;
  case clang::Stmt::StringLiteralClass:
    ret_status = VisitStringLiteral((clang::StringLiteral *)stmt, node_desc);
    break;
  case clang::Stmt::SubstNonTypeTemplateParmPackExprClass:
    ret_status = VisitSubstNonTypeTemplateParmPackExpr(
        (clang::SubstNonTypeTemplateParmPackExpr *)stmt, node_desc);
    break;
  case clang::Stmt::TypeTraitExprClass:
    ret_status = VisitTypeTraitExpr((clang::TypeTraitExpr *)stmt, node_desc);
    break;
  // TypoExpr was removed in LLVM 20
  // case clang::Stmt::TypoExprClass:
  //     ret_status = VisitTypoExpr((clang::TypoExpr *)stmt, node_desc);
  //     break;
  case clang::Stmt::UnaryExprOrTypeTraitExprClass:
    ret_status = VisitUnaryExprOrTypeTraitExpr(
        (clang::UnaryExprOrTypeTraitExpr *)stmt, node_desc);
    break;
  case clang::Stmt::VAArgExprClass:
    ret_status = VisitVAArgExpr((clang::VAArgExpr *)stmt, node_desc);
    break;
  case clang::Stmt::LabelStmtClass:
    ret_status = VisitLabelStmt((clang::LabelStmt *)stmt, node_desc);
    break;
  case clang::Stmt::WhileStmtClass:
    ret_status = VisitWhileStmt((clang::WhileStmt *)stmt, node_desc);
    break;
  case clang::Stmt::UnaryOperatorClass:
    ret_status = VisitUnaryOperator((clang::UnaryOperator *)stmt, node_desc);
    break;
  case clang::Stmt::CallExprClass:
    ret_status = VisitCallExpr((clang::CallExpr *)stmt, node_desc);
    break;
  case clang::Stmt::BinaryOperatorClass:
    ret_status = VisitBinaryOperator((clang::BinaryOperator *)stmt, node_desc);
    break;
  default:
    std::cerr << "Unknown statement kind: " << stmt->getStmtClassName() << " !"
              << std::endl;
    ROSE_ABORT();
  }

  // ROSE_ASSERT(result != NULL);
  // p_stmt_translation_map.insert(std::pair<clang::Stmt *, SgNode *>(stmt,
  // result)); return result;

  assert(ret_status != false);

  return node_ident;
}

/********************/
/* Visit Statements */
/********************/

bool ClangToDotTranslator::VisitStmt(clang::Stmt *stmt,
                                     NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitStmt" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("Stmt");

  return true;
}

bool ClangToDotTranslator::VisitAsmStmt(clang::AsmStmt *asm_stmt,
                                        NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitAsmStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("AsmStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(asm_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitGCCAsmStmt(clang::GCCAsmStmt *gcc_asm_stmt,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitGCCAsmStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("GCCAsmStmt");

  // ROSE_ASSERT(FAIL_TODO == 0); // TODO
  printf(
      "ClangToDotTranslator::VisitGCCAsmStmt called but not implemented! \n");

  return VisitStmt(gcc_asm_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitMSAsmStmt(clang::MSAsmStmt *ms_asm_stmt,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitMSAsmStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("MSAsmStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(ms_asm_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitBreakStmt(clang::BreakStmt *break_stmt,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitBreakStmt" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("BreakStmt");

  return VisitStmt(break_stmt, node_desc);
}

bool ClangToDotTranslator::VisitCapturedStmt(clang::CapturedStmt *captured_stmt,
                                             NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCapturedStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CapturedStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(captured_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitCompoundStmt(clang::CompoundStmt *compound_stmt,
                                             NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCompoundStmt" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("CompoundStmt");

  clang::CompoundStmt::body_iterator it;
  unsigned cnt = 0;
  for (it = compound_stmt->body_begin(); it != compound_stmt->body_end();
       it++) {
    std::ostringstream oss;
    oss << "child[" << cnt++ << "]";
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it)));
  }

  return VisitStmt(compound_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitContinueStmt(clang::ContinueStmt *continue_stmt,
                                             NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitContinueStmt" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("ContinueStmt");

  return VisitStmt(continue_stmt, node_desc);
}

bool ClangToDotTranslator::VisitCoreturnStmt(
    clang::CoreturnStmt *core_turn_stmt, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCoreturnStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CoreturnStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(core_turn_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitCoroutineBodyStmt(
    clang::CoroutineBodyStmt *coroutine_body_stmt, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCoroutineBodyStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CoroutineBodyStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(coroutine_body_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXCatchStmt(
    clang::CXXCatchStmt *cxx_catch_stmt, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXCatchStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXCatchStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(cxx_catch_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXForRangeStmt(
    clang::CXXForRangeStmt *cxx_for_range_stmt, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXForRangeStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXForRangeStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(cxx_for_range_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXTryStmt(clang::CXXTryStmt *cxx_try_stmt,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXTryStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXTryStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(cxx_try_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitDeclStmt(clang::DeclStmt *decl_stmt,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitDeclStmt" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("DeclStmt");

  if (decl_stmt->isSingleDecl()) {
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        "declaration[0]", Traverse(decl_stmt->getSingleDecl())));
  } else {
    clang::DeclStmt::decl_iterator it;
    unsigned cnt = 0;
    for (it = decl_stmt->decl_begin(); it != decl_stmt->decl_end(); it++) {
      std::ostringstream oss;
      oss << "declaration[" << cnt++ << "]";
      node_desc.successors.push_back(
          std::pair<std::string, std::string>(oss.str(), Traverse(*it)));
    }
  }

  // DQ (11/27/2020): I think we need to call this instead (in Tristan's code).
  // return res;
  return VisitStmt(decl_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitDoStmt(clang::DoStmt *do_stmt,
                                       NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitDoStmt" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("DoStmt");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "condition", Traverse(do_stmt->getCond())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "body", Traverse(do_stmt->getBody())));

  return VisitStmt(do_stmt, node_desc);
}

bool ClangToDotTranslator::VisitForStmt(clang::ForStmt *for_stmt,
                                        NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitForStmt" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("ForStmt");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "init", Traverse(for_stmt->getInit())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "cond", Traverse(for_stmt->getCond())));

  node_desc.successors.push_back(
      std::pair<std::string, std::string>("inc", Traverse(for_stmt->getInc())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "body", Traverse(for_stmt->getBody())));

  return VisitStmt(for_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitGotoStmt(clang::GotoStmt *goto_stmt,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitGotoStmt" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("GotoStmt");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "label", Traverse(goto_stmt->getLabel()->getStmt())));

  return VisitStmt(goto_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitIfStmt(clang::IfStmt *if_stmt,
                                       NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitIfStmt" << std::endl;
#endif

  bool res = true;

  // TODO if_stmt->getConditionVariable() appears when a variable is declared in
  // the condition...

  node_desc.kind_hierarchy.push_back("IfStmt");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "cond", Traverse(if_stmt->getCond())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "then", Traverse(if_stmt->getThen())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "else", Traverse(if_stmt->getElse())));

  return VisitStmt(if_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitIndirectGotoStmt(
    clang::IndirectGotoStmt *indirect_goto_stmt, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitIndirectGotoStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("IndirectGotoStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(indirect_goto_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitMSDependentExistsStmt(
    clang::MSDependentExistsStmt *ms_dependent_exists_stmt,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitMSDependentExistsStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("MSDependentExistsStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(ms_dependent_exists_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitNullStmt(clang::NullStmt *null_stmt,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitNullStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("NullStmt");

  return VisitStmt(null_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPExecutableDirective(
    clang::OMPExecutableDirective *omp_executable_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPExecutableDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPExecutableDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(omp_executable_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPAtomicDirective(
    clang::OMPAtomicDirective *omp_atomic_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPAtomicDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPAtomicDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_atomic_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPBarrierDirective(
    clang::OMPBarrierDirective *omp_barrier_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPBarrierDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPAtomicDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_barrier_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPCancelDirective(
    clang::OMPCancelDirective *omp_cancel_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPCancelDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPCancelDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_cancel_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPCancellationPointDirective(
    clang::OMPCancellationPointDirective *omp_cancellation_point_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPCancellationPointDirective"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPCancellationPointDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_cancellation_point_directive,
                                     node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitOMPCriticalDirective(
    clang::OMPCriticalDirective *omp_critical_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPCriticalDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPCriticalDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_critical_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPFlushDirective(
    clang::OMPFlushDirective *omp_flush_directive, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPFlushDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPFlushDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_flush_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPLoopDirective(
    clang::OMPLoopDirective *omp_loop_directive, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPLoopDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPLoopDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_loop_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPDistributeDirective(
    clang::OMPDistributeDirective *omp_distribute_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPDistributeDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPDistributeDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_distribute_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPDistributeParallelForDirective(
    clang::OMPDistributeParallelForDirective
        *omp_distribute_parallel_for_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPDistributeParallelForDirective"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPDistributeParallelForDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_distribute_parallel_for_directive,
                                     node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitOMPDistributeParallelForSimdDirective(
    clang::OMPDistributeParallelForSimdDirective
        *omp_distribute_parallel_for_simd_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr
      << "ClangToDotTranslator::VisitOMPDistributeParallelForSimdDirective"
      << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPDistributeParallelForSimdDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_distribute_parallel_for_simd_directive,
                                     node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitOMPDistributeSimdDirective(
    clang::OMPDistributeSimdDirective *omp_distribute__simd_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPDistributeSimdDirective"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPDistributeSimdDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_distribute__simd_directive,
                                     node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitOMPForDirective(
    clang::OMPForDirective *omp_for_directive, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPForDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPForDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_for_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPForSimdDirective(
    clang::OMPForSimdDirective *omp_for_simd_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPForSimdDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPForSimdDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_for_simd_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPParallelForDirective(
    clang::OMPParallelForDirective *omp_parallel_for_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPParallelForDirective"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPParallelForDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_parallel_for_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPParallelForSimdDirective(
    clang::OMPParallelForSimdDirective *omp_parallel_for_simd_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPParallelForSimdDirective"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPParallelForSimdDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_parallel_for_simd_directive, node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitOMPSimdDirective(
    clang::OMPSimdDirective *omp_simd_directive, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPSimdDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPSimdDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_simd_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPTargetParallelForDirective(
    clang::OMPTargetParallelForDirective *omp_target_parallel_for_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPTargetParallelForDirective"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPTargetParallelForDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_target_parallel_for_directive, node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitOMPTargetParallelForSimdDirective(
    clang::OMPTargetParallelForSimdDirective
        *omp_target_parallel_for_simd_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPTargetParallelForSimdDirective"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPTargetParallelForSimdDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_target_parallel_for_simd_directive,
                               node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitOMPTargetSimdDirective(
    clang::OMPTargetSimdDirective *omp_target_simd_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPTargetSimdDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPTargetSimdDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_target_simd_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPTargetTeamsDistributeDirective(
    clang::OMPTargetTeamsDistributeDirective
        *omp_target_teams_distribute_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPTargetTeamsDistributeDirective"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPTargetTeamsDistributeDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_target_teams_distribute_directive,
                               node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitOMPTargetTeamsDistributeSimdDirective(
    clang::OMPTargetTeamsDistributeSimdDirective
        *omp_target_teams_distribute_simd_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr
      << "ClangToDotTranslator::VisitOMPTargetTeamsDistributeSimdDirective"
      << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPTargetTeamsDistributeSimdDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_target_teams_distribute_simd_directive,
                               node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitOMPTaskLoopDirective(
    clang::OMPTaskLoopDirective *omp_task_loop_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPTaskLoopDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPTaskLoopDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_task_loop_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPTaskLoopSimdDirective(
    clang::OMPTaskLoopSimdDirective *omp_task_loop_simd_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPTaskLoopSimdDirective"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPTaskLoopSimdDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_task_loop_simd_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPMasterDirective(
    clang::OMPMasterDirective *omp_master_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPMasterDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPMasterDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_master_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPOrderedDirective(
    clang::OMPOrderedDirective *omp_ordered_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPOrderedDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPOrderedDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_ordered_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPParallelDirective(
    clang::OMPParallelDirective *omp_parallel_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPParallelDirective" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPParallelDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_parallel_directive, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPParallelSectionsDirective(
    clang::OMPParallelSectionsDirective *omp_parallel_sections_directive,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPParallelSectionsDirective"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPParallelSectionsDirective");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_parallel_sections_directive,
                                     node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitReturnStmt(clang::ReturnStmt *return_stmt,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitReturnStmt" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("ReturnStmt");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "return_value", Traverse(return_stmt->getRetValue())));

  return VisitStmt(return_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitSEHExceptStmt(
    clang::SEHExceptStmt *seh_except_stmt, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitSEHExceptStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("SEHExceptStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(seh_except_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitSEHFinallyStmt(
    clang::SEHFinallyStmt *seh_finally_stmt, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitSEHFinallyStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("SEHFinallyStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(seh_finally_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitSEHLeaveStmt(
    clang::SEHLeaveStmt *seh_leave_stmt, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitSEHLeaveStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("SEHLeaveStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(seh_leave_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitSEHTryStmt(clang::SEHTryStmt *seh_try_stmt,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitSEHTryStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("SEHTryStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(seh_try_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitSwitchCase(clang::SwitchCase *switch_case,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitSwitchCase" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("SwitchStmt");

  // TODO

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "sub_stmt", Traverse(switch_case->getSubStmt())));

  return VisitStmt(switch_case, node_desc) && res;
}

bool ClangToDotTranslator::VisitCaseStmt(clang::CaseStmt *case_stmt,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCaseStmt" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("CaseStmt");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "lhs", Traverse(case_stmt->getLHS())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "rhs", Traverse(case_stmt->getRHS())));

  return VisitSwitchCase(case_stmt, node_desc);
}

bool ClangToDotTranslator::VisitDefaultStmt(clang::DefaultStmt *default_stmt,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitDefaultStmt" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("DefaultStmt");

  return VisitSwitchCase(default_stmt, node_desc);
}

bool ClangToDotTranslator::VisitSwitchStmt(clang::SwitchStmt *switch_stmt,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitSwitchStmt" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("SwitchStmt");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "cond", Traverse(switch_stmt->getCond())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "body", Traverse(switch_stmt->getBody())));

  return VisitStmt(switch_stmt, node_desc);
}

bool ClangToDotTranslator::VisitValueStmt(clang::ValueStmt *value_stmt,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitValueStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ValueStmt");

  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(value_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitAttributedStmt(
    clang::AttributedStmt *attributed_stmt, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitAttributedStmt" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("AttributedStmt");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitValueStmt(attributed_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitExpr(clang::Expr *expr,
                                     NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitExpr" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("Expr");

  // TODO Is there anything to be done? (maybe in relation with typing?)

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "type", Traverse(expr->getType().getTypePtr())));

  return VisitValueStmt(expr, node_desc);
}

bool ClangToDotTranslator::VisitAbstractConditionalOperator(
    clang::AbstractConditionalOperator *abstract_conditional_operator,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitAbstractConditionalOperator"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("AbstractConditionalOperator");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "condition", Traverse(abstract_conditional_operator->getCond())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "true_stmt", Traverse(abstract_conditional_operator->getTrueExpr())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "false_stmt", Traverse(abstract_conditional_operator->getFalseExpr())));

  // TODO

  return VisitStmt(abstract_conditional_operator, node_desc) && res;
}

bool ClangToDotTranslator::VisitBinaryConditionalOperator(
    clang::BinaryConditionalOperator *binary_conditional_operator,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitBinaryConditionalOperator"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("BinaryConditionalOperator");

  // TODO

  return VisitStmt(binary_conditional_operator, node_desc) && res;
}

bool ClangToDotTranslator::VisitConditionalOperator(
    clang::ConditionalOperator *conditional_operator,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitConditionalOperator" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("ConditionalOperator");

  return VisitAbstractConditionalOperator(conditional_operator, node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitAddrLabelExpr(
    clang::AddrLabelExpr *addr_label_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitAddrLabelExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("AddrLabelExpr");

  // TODO

  return VisitExpr(addr_label_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitArrayInitIndexExpr(
    clang::ArrayInitIndexExpr *array_init_index_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitArrayInitIndexExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ArrayInitIndexExpr");

  // TODO

  return VisitExpr(array_init_index_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitArrayInitLoopExpr(
    clang::ArrayInitLoopExpr *array_init_loop_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitArrayInitLoopExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ArrayInitLoopExpr");

  // TODO

  return VisitExpr(array_init_loop_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitArraySubscriptExpr(
    clang::ArraySubscriptExpr *array_subscript_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitArraySubscriptExpr" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("ArraySubscriptExpr");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "base", Traverse(array_subscript_expr->getBase())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "index", Traverse(array_subscript_expr->getIdx())));

  return VisitExpr(array_subscript_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitArrayTypeTraitExpr(
    clang::ArrayTypeTraitExpr *array_type_trait_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitArrayTypeTraitExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ArrayTypeTraitExpr");

  // TODO

  return VisitExpr(array_type_trait_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitAsTypeExpr(clang::AsTypeExpr *as_type_expr,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitAsTypeExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("AsTypeExpr");

  // TODO

  return VisitExpr(as_type_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitAtomicExpr(clang::AtomicExpr *atomic_expr,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitAtomicExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("AtomicExpr");

  // TODO

  return VisitExpr(atomic_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitBinaryOperator(
    clang::BinaryOperator *binary_operator, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitBinaryOperator" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("BinaryOperator");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "lhs", Traverse(binary_operator->getLHS())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "rhs", Traverse(binary_operator->getRHS())));

  node_desc.attributes.push_back(std::pair<std::string, std::string>(
      "opcode", binary_operator->getOpcodeStr()));

  return VisitExpr(binary_operator, node_desc) && res;
}

bool ClangToDotTranslator::VisitCompoundAssignOperator(
    clang::CompoundAssignOperator *compound_assign_operator,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCompoundAssignOperator" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CompoundAssignOperator");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "computation_lhs_type",
      Traverse(
          compound_assign_operator->getComputationLHSType().getTypePtr())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "computation_result_type",
      Traverse(
          compound_assign_operator->getComputationResultType().getTypePtr())));

  // TODO

  return VisitBinaryOperator(compound_assign_operator, node_desc) && res;
}

bool ClangToDotTranslator::VisitBlockExpr(clang::BlockExpr *block_expr,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitBlockExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("BlockExpr");

  // TODO

  return VisitExpr(block_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCallExpr(clang::CallExpr *call_expr,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCallExpr" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("CallExpr");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "callee", Traverse(call_expr->getCallee())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "callee_decl", Traverse(call_expr->getCalleeDecl())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "direct_callee", Traverse(call_expr->getDirectCallee())));

  clang::CallExpr::arg_iterator it;
  unsigned cnt = 0;
  for (it = call_expr->arg_begin(); it != call_expr->arg_end(); ++it) {
    std::ostringstream oss;
    oss << "argument[" << cnt++ << "]";
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it)));
  }

  return VisitExpr(call_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCUDAKernelCallExpr(
    clang::CUDAKernelCallExpr *cuda_kernel_call_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCUDAKernelCallExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CUDAKernelCallExpr");

  // TODO

  return VisitExpr(cuda_kernel_call_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXMemberCallExpr(
    clang::CXXMemberCallExpr *cxx_member_call_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXMemberCallExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXMemberCallExpr");

  // TODO

  return VisitExpr(cxx_member_call_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXOperatorCallExpr(
    clang::CXXOperatorCallExpr *cxx_operator_call_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXOperatorCallExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXOperatorCallExpr");

  // TODO

  return VisitExpr(cxx_operator_call_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitUserDefinedLiteral(
    clang::UserDefinedLiteral *user_defined_literal,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitUserDefinedLiteral" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("UserDefinedLiteral");

  // TODO

  return VisitExpr(user_defined_literal, node_desc) && res;
}

bool ClangToDotTranslator::VisitCastExpr(clang::CastExpr *cast_expr,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCastExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CastExpr");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "sub_expr", Traverse(cast_expr->getSubExpr())));

  // TODO

  return VisitExpr(cast_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitExplicitCastExpr(
    clang::ExplicitCastExpr *explicit_cast_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitExplicitCastExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ExplicitCastExpr");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "type_as_written",
      Traverse(explicit_cast_expr->getTypeAsWritten().getTypePtr())));

  // TODO

  return VisitCastExpr(explicit_cast_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitBuiltinBitCastExpr(
    clang::BuiltinBitCastExpr *builtin_bit_cast_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitBuiltinBitCastExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("BuiltinBitCastExpr");

  // TODO

  return VisitExplicitCastExpr(builtin_bit_cast_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCStyleCastExpr(
    clang::CStyleCastExpr *c_style_cast, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCStyleCastExpr" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("CStyleCastExpr");

  return VisitExplicitCastExpr(c_style_cast, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXFunctionalCastExpr(
    clang::CXXFunctionalCastExpr *cxx_functional_cast_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXFunctionalCastExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXFunctionalCastExpr");

  // TODO

  return VisitExplicitCastExpr(cxx_functional_cast_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXNamedCastExpr(
    clang::CXXNamedCastExpr *cxx_named_cast_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXNamedCastExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXNamedCastExpr");

  // TODO

  return VisitExplicitCastExpr(cxx_named_cast_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXConstCastExpr(
    clang::CXXConstCastExpr *cxx_const_cast_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXConstCastExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXConstCastExpr");

  // TODO

  return VisitCXXNamedCastExpr(cxx_const_cast_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXDynamicCastExpr(
    clang::CXXDynamicCastExpr *cxx_dynamic_cast_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXDynamicCastExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXDynamicCastExpr");

  // TODO

  return VisitCXXNamedCastExpr(cxx_dynamic_cast_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXReinterpretCastExpr(
    clang::CXXReinterpretCastExpr *cxx_reinterpret_cast_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXReinterpretCastExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXReinterpretCastExpr");

  // TODO

  return VisitCXXNamedCastExpr(cxx_reinterpret_cast_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXStaticCastExpr(
    clang::CXXStaticCastExpr *cxx_static_cast_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXStaticCastExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXStaticCastExpr");

  // TODO

  return VisitCXXNamedCastExpr(cxx_static_cast_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitImplicitCastExpr(
    clang::ImplicitCastExpr *implicit_cast_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitImplicitCastExpr" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("ImplicitCastExpr");

  return VisitCastExpr(implicit_cast_expr, node_desc);
}

bool ClangToDotTranslator::VisitCharacterLiteral(
    clang::CharacterLiteral *character_literal, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCharacterLiteral" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("CharacterLiteral");

  switch (character_literal->getKind()) {
  case clang::CharacterLiteralKind::Ascii:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("kind", "Ascii"));
    break;
  case clang::CharacterLiteralKind::Wide:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("kind", "Wide"));
    break;
  case clang::CharacterLiteralKind::UTF8:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("kind", "UTF8"));
    break;
  case clang::CharacterLiteralKind::UTF16:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("kind", "UTF16"));
    break;
  case clang::CharacterLiteralKind::UTF32:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("kind", "UTF32"));
    break;
  }

  std::ostringstream oss;
  oss << std::hex << character_literal->getValue();
  node_desc.attributes.push_back(
      std::pair<std::string, std::string>("hex_value", oss.str()));

  return VisitExpr(character_literal, node_desc);
}

bool ClangToDotTranslator::VisitChooseExpr(clang::ChooseExpr *choose_expr,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitChooseExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ChooseExpr");

  // TODO

  return VisitExpr(choose_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCompoundLiteralExpr(
    clang::CompoundLiteralExpr *compound_literal, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCompoundLiteralExpr" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("CompoundLiteralExpr");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "initializer", Traverse(compound_literal->getInitializer())));

  return VisitExpr(compound_literal, node_desc);
}

// bool
// ClangToDotTranslator::VisitConceptSpecializationExpr(clang::ConceptSpecializationExpr
// * concept_specialization_expr, SgNode ** node) { #if DEBUG_VISIT_STMT
//     std::cerr << "ClangToDotTranslator::VisitConceptSpecializationExpr" <<
//     std::endl;
// #endif
//     bool res = true;
//
//     // TODO
//
//     return VisitExpr(concept_specialization_expr, node) && res;
// }

bool ClangToDotTranslator::VisitConvertVectorExpr(
    clang::ConvertVectorExpr *convert_vector_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitConvertVectorExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ConvertVectorExpr");

  // TODO

  return VisitExpr(convert_vector_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCoroutineSuspendExpr(
    clang::CoroutineSuspendExpr *coroutine_suspend_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCoroutineSuspendExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CoroutineSuspendExpr");

  // TODO

  return VisitExpr(coroutine_suspend_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCoawaitExpr(clang::CoawaitExpr *coawait_expr,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCoawaitExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CoawaitExpr");

  // TODO

  return VisitCoroutineSuspendExpr(coawait_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCoyieldExpr(clang::CoyieldExpr *coyield_expr,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCoyieldExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CoyieldExpr");

  // TODO

  return VisitCoroutineSuspendExpr(coyield_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXBindTemporaryExpr(
    clang::CXXBindTemporaryExpr *cxx_bind_temporary_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXBindTemporaryExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXBindTemporaryExpr");

  // TODO

  return VisitExpr(cxx_bind_temporary_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXBoolLiteralExpr(
    clang::CXXBoolLiteralExpr *cxx_bool_literal_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXBoolLiteralExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXBoolLiteralExpr");

  // TODO

  return VisitExpr(cxx_bool_literal_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXConstructExpr(
    clang::CXXConstructExpr *cxx_construct_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXConstructExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXConstructExpr");

  // TODO

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "constructor", Traverse(cxx_construct_expr->getConstructor())));

  clang::CXXConstructExpr::arg_iterator it;
  unsigned cnt = 0;
  for (it = cxx_construct_expr->arg_begin();
       it != cxx_construct_expr->arg_end(); ++it) {
    std::ostringstream oss;
    oss << "argument[" << cnt++ << "]";
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it)));
  }

  return VisitExpr(cxx_construct_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXTemporaryObjectExpr(
    clang::CXXTemporaryObjectExpr *cxx_temporary_object_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXTemporaryObjectExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXTemporaryObjectExpr");

  // TODO

  return VisitCXXConstructExpr(cxx_temporary_object_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXDefaultArgExpr(
    clang::CXXDefaultArgExpr *cxx_default_arg_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXDefaultArgExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXDefaultArgExpr");

  // TODO

  return VisitExpr(cxx_default_arg_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXDefaultInitExpr(
    clang::CXXDefaultInitExpr *cxx_default_init_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXDefaultInitExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXDefaultInitExpr");

  // TODO

  return VisitExpr(cxx_default_init_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXDeleteExpr(
    clang::CXXDeleteExpr *cxx_delete_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXDeleteExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXDeleteExpr");

  // TODO

  return VisitExpr(cxx_delete_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXDependentScopeMemberExpr(
    clang::CXXDependentScopeMemberExpr *cxx_dependent_scope_member_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXDependentScopeMemberExpr"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXDependentScopeMemberExpr");

  // TODO

  return VisitExpr(cxx_dependent_scope_member_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXFoldExpr(clang::CXXFoldExpr *cxx_fold_expr,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXFoldExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXFoldExpr");

  // TODO

  return VisitExpr(cxx_fold_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXInheritedCtorInitExpr(
    clang::CXXInheritedCtorInitExpr *cxx_inherited_ctor_init_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXInheritedCtorInitExpr"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXInheritedCtorInitExpr");

  // TODO

  return VisitExpr(cxx_inherited_ctor_init_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXNewExpr(clang::CXXNewExpr *cxx_new_expr,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXNewExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXNewExpr");

  // TODO

  return VisitExpr(cxx_new_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXNoexceptExpr(
    clang::CXXNoexceptExpr *cxx_noexcept_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXNoexceptExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXNoexceptExpr");

  // TODO

  return VisitExpr(cxx_noexcept_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXNullPtrLiteralExpr(
    clang::CXXNullPtrLiteralExpr *cxx_null_ptr_literal_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXNullPtrLiteralExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXNullPtrLiteralExpr");

  // TODO

  return VisitExpr(cxx_null_ptr_literal_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXPseudoDestructorExpr(
    clang::CXXPseudoDestructorExpr *cxx_pseudo_destructor_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXPseudoDestructorExpr"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXPseudoDestructorExpr");

  // TODO

  return VisitExpr(cxx_pseudo_destructor_expr, node_desc) && res;
}

// bool
// ClangToDotTranslator::VisitCXXRewrittenBinaryOperator(clang::CXXRewrittenBinaryOperator
// * cxx_rewrite_binary_operator, SgNode ** node) { #if DEBUG_VISIT_STMT
//     std::cerr << "ClangToDotTranslator::VisitCXXRewrittenBinaryOperator" <<
//     std::endl;
// #endif
//     bool res = true;
//
//     // TODO
//
//     return VisitExpr(cxx_rewrite_binary_operator, node) && res;
// }

bool ClangToDotTranslator::VisitCXXScalarValueInitExpr(
    clang::CXXScalarValueInitExpr *cxx_scalar_value_init_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXScalarValueInitExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXScalarValueInitExpr");

  // TODO

  return VisitExpr(cxx_scalar_value_init_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXStdInitializerListExpr(
    clang::CXXStdInitializerListExpr *cxx_std_initializer_list_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXStdInitializerListExpr"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXStdInitializerListExpr");

  // TODO

  return VisitExpr(cxx_std_initializer_list_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXThisExpr(clang::CXXThisExpr *cxx_this_expr,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXThisExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXThisExpr");

  // TODO

  return VisitExpr(cxx_this_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXThrowExpr(
    clang::CXXThrowExpr *cxx_throw_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXThrowExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXThrowExpr");

  // TODO

  return VisitExpr(cxx_throw_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXTypeidExpr(
    clang::CXXTypeidExpr *cxx_typeid_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXTypeidExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXTypeidExpr");

  // TODO

  return VisitExpr(cxx_typeid_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXUnresolvedConstructExpr(
    clang::CXXUnresolvedConstructExpr *cxx_unresolved_construct_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXUnresolvedConstructExpr"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXUnresolvedConstructExpr");

  // TODO

  return VisitExpr(cxx_unresolved_construct_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXUuidofExpr(
    clang::CXXUuidofExpr *cxx_uuidof_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitCXXUuidofExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXUuidofExpr");

  // TODO

  return VisitExpr(cxx_uuidof_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitDeclRefExpr(clang::DeclRefExpr *decl_ref_expr,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitDeclRefExpr" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("DeclRefExpr");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "decl", Traverse(decl_ref_expr->getDecl())));

  return VisitExpr(decl_ref_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitDependentCoawaitExpr(
    clang::DependentCoawaitExpr *dependent_coawait_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitDependentCoawaitExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DependentCoawaitExpr");

  // TODO

  return VisitExpr(dependent_coawait_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitDependentScopeDeclRefExpr(
    clang::DependentScopeDeclRefExpr *dependent_scope_decl_ref_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitDependentScopeDeclRefExpr"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DependentScopeDeclRefExpr");

  // TODO

  return VisitExpr(dependent_scope_decl_ref_expr, node_desc) && res;
}

// bool
// ClangToDotTranslator::VisitDependentScopeDeclRefExpr(clang::DependentScopeDeclRefExpr
// * dependent_scope_decl_ref_expr);

bool ClangToDotTranslator::VisitDesignatedInitExpr(
    clang::DesignatedInitExpr *designated_init_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitDesignatedInitExpr" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("DesignatedInitExpr");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "init", Traverse(designated_init_expr->getInit())));

  // DQ (11/28/2020): There is no
  // clang::DesignatedInitExpr::designators_iterator in Clang 10, so this has to
  // be iterated over using a for loop and calling the getDesignator() function.
  unsigned cnt = 0;
  for (unsigned i = 0; i < designated_init_expr->size(); i++) {
    clang::DesignatedInitExpr::Designator *it =
        designated_init_expr->getDesignator(i);

    std::ostringstream oss;
    oss << "designator[" << cnt++ << "]";
    if (it->isFieldDesignator()) {
      oss << " field";
      node_desc.successors.push_back(std::pair<std::string, std::string>(
          oss.str(), Traverse(it->getFieldDecl())));
    } else {
      if (it->isArrayDesignator()) {
        oss << " array";
        node_desc.successors.push_back(std::pair<std::string, std::string>(
            oss.str(), Traverse(designated_init_expr->getArrayIndex(*it))));
      } else {
        if (it->isArrayRangeDesignator()) {
          oss << " range";
          std::ostringstream oss_;
          oss_ << oss.str() << "_end";
          oss << "_start";
          node_desc.successors.push_back(std::pair<std::string, std::string>(
              oss.str(),
              Traverse(designated_init_expr->getArrayRangeStart(*it))));
          node_desc.successors.push_back(std::pair<std::string, std::string>(
              oss_.str(),
              Traverse(designated_init_expr->getArrayRangeEnd(*it))));
        } else {
          ROSE_ABORT();
        }
      }
    }
  }

  return VisitExpr(designated_init_expr, node_desc);
}

bool ClangToDotTranslator::VisitDesignatedInitUpdateExpr(
    clang::DesignatedInitUpdateExpr *designated_init_update,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitDesignatedInitUpdateExpr"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DesignatedInitUpdateExpr");

  // TODO

  return VisitExpr(designated_init_update, node_desc) && res;
}

bool ClangToDotTranslator::VisitExpressionTraitExpr(
    clang::ExpressionTraitExpr *expression_trait_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitExpressionTraitExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ExpressionTraitExpr");

  // TODO

  return VisitExpr(expression_trait_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitExtVectorElementExpr(
    clang::ExtVectorElementExpr *ext_vector_element_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitExtVectorElementExpr" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("ExtVectorElementExpr");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "base", Traverse(ext_vector_element_expr->getBase())));

  if (ext_vector_element_expr->isArrow())
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("access_operator", "arrow"));
  else
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("access_operator", "dot"));

  clang::IdentifierInfo &ident_info = ext_vector_element_expr->getAccessor();
  std::string ident = ident_info.getName().str();

  node_desc.attributes.push_back(
      std::pair<std::string, std::string>("accessed_field", ident));

  return VisitExpr(ext_vector_element_expr, node_desc);
}

bool ClangToDotTranslator::VisitFixedPointLiteral(
    clang::FixedPointLiteral *fixed_point_literal, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitFixedPointLiteral" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("FixedPointLiteral");

  // TODO

  return VisitExpr(fixed_point_literal, node_desc) && res;
}

bool ClangToDotTranslator::VisitFloatingLiteral(
    clang::FloatingLiteral *floating_literal, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitFloatingLiteral" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("FloatingLiteral");

  // FIXME

  unsigned int precision = llvm::APFloat::semanticsPrecision(
      floating_literal->getValue().getSemantics());
  std::ostringstream oss;
  if (precision == 24) {
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("precision", "single"));
    oss << floating_literal->getValue().convertToFloat();
  } else if (precision == 53) {
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("precision", "double"));
    oss << floating_literal->getValue().convertToDouble();
  } else
    assert(!"In VisitFloatingLiteral: Unsupported float size");

  node_desc.attributes.push_back(
      std::pair<std::string, std::string>("value", oss.str()));

  return VisitExpr(floating_literal, node_desc);
}

bool ClangToDotTranslator::VisitFullExpr(clang::FullExpr *full_expr,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitFullExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("FullExpr");

  // TODO

  return VisitExpr(full_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitConstantExpr(clang::ConstantExpr *constant_expr,
                                             NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitConstantExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ConstantExpr");

  // TODO

  return VisitFullExpr(constant_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitExprWithCleanups(
    clang::ExprWithCleanups *expr_with_cleanups, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitExprWithCleanups" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ExprWithCleanups");

  // TODO

  return VisitFullExpr(expr_with_cleanups, node_desc) && res;
}

bool ClangToDotTranslator::VisitFunctionParmPackExpr(
    clang::FunctionParmPackExpr *function_parm_pack_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitFunctionParmPackExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("FunctionParmPackExpr");

  // TODO

  return VisitExpr(function_parm_pack_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitGenericSelectionExpr(
    clang::GenericSelectionExpr *generic_Selection_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitGenericSelectionExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("GenericSelectionExpr");

  // TODO

  return VisitExpr(generic_Selection_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitGNUNullExpr(clang::GNUNullExpr *gnu_null_expr,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitGNUNullExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("GNUNullExpr");

  // TODO

  return VisitExpr(gnu_null_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitImaginaryLiteral(
    clang::ImaginaryLiteral *imaginary_literal, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitImaginaryLiteral" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("ImaginaryLiteral");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "sub_expr", Traverse(imaginary_literal->getSubExpr())));

  return VisitExpr(imaginary_literal, node_desc);
}

bool ClangToDotTranslator::VisitImplicitValueInitExpr(
    clang::ImplicitValueInitExpr *implicit_value_init_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitImplicitValueInitExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ImplicitValueInitExpr");

  // TODO

  return VisitExpr(implicit_value_init_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitInitListExpr(
    clang::InitListExpr *init_list_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitInitListExpr" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("InitListExpr");

  clang::InitListExpr::iterator it;
  unsigned cnt = 0;
  for (it = init_list_expr->begin(); it != init_list_expr->end(); it++) {
    std::ostringstream oss;
    oss << "init[" << cnt++ << "]";
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it)));
  }

  return VisitExpr(init_list_expr, node_desc);
}

bool ClangToDotTranslator::VisitIntegerLiteral(
    clang::IntegerLiteral *integer_literal, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitIntegerLiteral" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("IntegerLiteral");

  // FIXME

  // DQ (11/28/2020): getHashValue is no longer available.
  // std::ostringstream oss;
  // oss << std::hex << integer_literal->getValue().getHashValue();
  // node_desc.attributes.push_back(std::pair<std::string,
  // std::string>("hex_hash_value", oss.str()));

  return VisitExpr(integer_literal, node_desc);
}

bool ClangToDotTranslator::VisitLambdaExpr(clang::LambdaExpr *lambda_expr,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitLambdaExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(lambda_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitMaterializeTemporaryExpr(
    clang::MaterializeTemporaryExpr *materialize_temporary_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitMaterializeTemporaryExpr"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("MaterializeTemporaryExpr");

  // TODO

  return VisitExpr(materialize_temporary_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitMemberExpr(clang::MemberExpr *member_expr,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitMemberExpr" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("MemberExpr");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "base", Traverse(member_expr->getBase())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "member_decl", Traverse(member_expr->getMemberDecl())));

  if (member_expr->isArrow())
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("access_operator", "arrow"));
  else
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("access_operator", "dot"));

  return VisitExpr(member_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitMSPropertyRefExpr(
    clang::MSPropertyRefExpr *ms_property_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitMSPropertyRefExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("MSPropertyRefExpr");

  // TODO

  return VisitExpr(ms_property_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitMSPropertySubscriptExpr(
    clang::MSPropertySubscriptExpr *ms_property_subscript_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitMSPropertySubscriptExpr"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("MSPropertySubscriptExpr");

  // TODO

  return VisitExpr(ms_property_subscript_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitNoInitExpr(clang::NoInitExpr *no_init_expr,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitNoInitExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("NoInitExpr");

  // TODO

  return VisitExpr(no_init_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitOffsetOfExpr(
    clang::OffsetOfExpr *offset_of_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOffsetOfExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OffsetOfExpr");

  // TODO

  return VisitExpr(offset_of_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPArraySectionExpr(
    clang::ArraySectionExpr *omp_array_section_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOMPArraySectionExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPArraySectionExpr");

  // TODO

  return VisitExpr(omp_array_section_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitOpaqueValueExpr(
    clang::OpaqueValueExpr *opaque_value_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOpaqueValueExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OpaqueValueExpr");

  // TODO

  return VisitExpr(opaque_value_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitOverloadExpr(clang::OverloadExpr *overload_expr,
                                             NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitOverloadExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OverloadExpr");

  // TODO

  return VisitExpr(overload_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitUnresolvedLookupExpr(
    clang::UnresolvedLookupExpr *unresolved_lookup_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitUnresolvedLookupExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("UnresolvedLookupExpr");

  // TODO

  return VisitOverloadExpr(unresolved_lookup_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitUnresolvedMemberExpr(
    clang::UnresolvedMemberExpr *unresolved_member_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitUnresolvedMemberExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("UnresolvedMemberExpr");

  // TODO

  return VisitOverloadExpr(unresolved_member_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitPackExpansionExpr(
    clang::PackExpansionExpr *pack_expansion_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitPackExpansionExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("PackExpansionExpr");

  // TODO

  return VisitExpr(pack_expansion_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitParenExpr(clang::ParenExpr *paren_expr,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitParenExpr" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("ParenExpr");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "sub_expr", Traverse(paren_expr->getSubExpr())));

  return VisitExpr(paren_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitParenListExpr(
    clang::ParenListExpr *paran_list_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitParenListExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ParenListExpr");

  // TODO

  return VisitExpr(paran_list_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitPredefinedExpr(
    clang::PredefinedExpr *predefined_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitPredefinedExpr" << std::endl;
#endif

  // FIXME It's get tricky here: PredefinedExpr represent compiler generateed
  // variables
  //    I choose to attach those variables on demand in the function definition
  //    scope

  // Traverse the scope's stack to find the last function definition:

  node_desc.kind_hierarchy.push_back("PredefinedExpr");

  // DQ (11/28/2020): Change of function name in Clang 10.
  // switch (predefined_expr->getIdentType())
  switch (predefined_expr->getIdentKind()) {
  case clang::PredefinedIdentKind::Func:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("ident_type", "func"));
    break;
  case clang::PredefinedIdentKind::Function:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("ident_type", "function"));
    break;
  case clang::PredefinedIdentKind::PrettyFunction:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("ident_type", "pretty_function"));
    break;
  case clang::PredefinedIdentKind::PrettyFunctionNoVirtual:
    node_desc.attributes.push_back(std::pair<std::string, std::string>(
        "ident_type", "pretty_function_no_virtual"));
    break;
  case clang::PredefinedIdentKind::LFunction:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("ident_type", "l_function"));
    break;
  case clang::PredefinedIdentKind::FuncDName:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("ident_type", "func_dname"));
    break;
  case clang::PredefinedIdentKind::FuncSig:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("ident_type", "func_sig"));
    break;
  case clang::PredefinedIdentKind::LFuncSig:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("ident_type", "l_func_sig"));
    break;
  default:
    ROSE_ASSERT(!"Unhandled clang::PredefinedIdentKind");
    break;
  }

  return VisitExpr(predefined_expr, node_desc);
}

bool ClangToDotTranslator::VisitPseudoObjectExpr(
    clang::PseudoObjectExpr *pseudo_object_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitPseudoObjectExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("PseudoObjectExpr");

  // TODO

  return VisitExpr(pseudo_object_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitShuffleVectorExpr(
    clang::ShuffleVectorExpr *shuffle_vector_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitShuffleVectorExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ShuffleVectorExpr");

  // TODO

  return VisitExpr(shuffle_vector_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitSizeOfPackExpr(
    clang::SizeOfPackExpr *size_of_pack_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitSizeOfPackExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("SizeOfPackExpr");

  // TODO

  return VisitExpr(size_of_pack_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitSourceLocExpr(
    clang::SourceLocExpr *source_loc_expr, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitSourceLocExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("SourceLocExpr");

  // TODO

  return VisitExpr(source_loc_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitStmtExpr(clang::StmtExpr *stmt_expr,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitStmtExpr" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("StmtExpr");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "sub_stmt", Traverse(stmt_expr->getSubStmt())));

  return VisitExpr(stmt_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitStringLiteral(
    clang::StringLiteral *string_literal, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitStringLiteral" << std::endl;
#endif

  const std::string raw = string_literal->getString().str();
  const std::string escaped = escapeDotString(raw);

  node_desc.kind_hierarchy.push_back("StringLiteral");

  node_desc.attributes.push_back(
      std::pair<std::string, std::string>("string", escaped));

  return VisitExpr(string_literal, node_desc);
}

bool ClangToDotTranslator::VisitSubstNonTypeTemplateParmExpr(
    clang::SubstNonTypeTemplateParmExpr *subst_non_type_template_parm_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitSubstNonTypeTemplateParmExpr"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("SubstNonTypeTemplateParmExpr");

  // TODO

  return VisitExpr(subst_non_type_template_parm_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitSubstNonTypeTemplateParmPackExpr(
    clang::SubstNonTypeTemplateParmPackExpr
        *subst_non_type_template_parm_pack_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitSubstNonTypeTemplateParmPackExpr"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("SubstNonTypeTemplateParmPackExpr");

  // TODO

  return VisitExpr(subst_non_type_template_parm_pack_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitTypeTraitExpr(clang::TypeTraitExpr *type_trait,
                                              NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitTypeTraitExpr" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("TypeTraitExpr");

  // TODO

  return VisitExpr(type_trait, node_desc) && res;
}

bool ClangToDotTranslator::VisitUnaryExprOrTypeTraitExpr(
    clang::UnaryExprOrTypeTraitExpr *unary_expr_or_type_trait_expr,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitUnaryExprOrTypeTraitExpr"
            << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("UnaryExprOrTypeTraitExpr");

  if (unary_expr_or_type_trait_expr->isArgumentType()) {
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        "argument_type",
        Traverse(
            unary_expr_or_type_trait_expr->getArgumentType().getTypePtr())));
  } else {
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        "argument_expr",
        Traverse(unary_expr_or_type_trait_expr->getArgumentExpr())));
  }

  switch (unary_expr_or_type_trait_expr->getKind()) {
  case clang::UETT_SizeOf:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("kind", "sizeof"));
    break;
  case clang::UETT_AlignOf:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("kind", "alignof"));
    break;
  case clang::UETT_VecStep:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("kind", "vecstep"));
    break;
  case clang::UETT_DataSizeOf:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("kind", "datasizeof"));
    break;
  case clang::UETT_PreferredAlignOf:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("kind", "preferred_alignof"));
    break;
  case clang::UETT_PtrAuthTypeDiscriminator:
    node_desc.attributes.push_back(std::pair<std::string, std::string>(
        "kind", "ptrauth_type_discriminator"));
    break;
  case clang::UETT_OpenMPRequiredSimdAlign:
    node_desc.attributes.push_back(std::pair<std::string, std::string>(
        "kind", "openmp_required_simd_align"));
    break;
  case clang::UETT_VectorElements:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("kind", "vector_elements"));
    break;
  default:
    ROSE_ASSERT(!"Unhandled clang::UnaryExprOrTypeTrait");
    break;
  }

  return VisitStmt(unary_expr_or_type_trait_expr, node_desc) && res;
}

bool ClangToDotTranslator::VisitUnaryOperator(
    clang::UnaryOperator *unary_operator, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitUnaryOperator" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("UnaryOperator");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "sub_expr", Traverse(unary_operator->getSubExpr())));

  switch (unary_operator->getOpcode()) {
  case clang::UO_PostInc:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "PostInc"));
    break;
  case clang::UO_PostDec:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "PostDec"));
    break;
  case clang::UO_PreInc:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "PreInc"));
    break;
  case clang::UO_PreDec:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "PreDec"));
    break;
  case clang::UO_AddrOf:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "AddrOf"));
    break;
  case clang::UO_Deref:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "Deref"));
    break;
  case clang::UO_Plus:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "Plus"));
    break;
  case clang::UO_Minus:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "Minus"));
    break;
  case clang::UO_Not:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "Not"));
    break;
  case clang::UO_LNot:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "LNot"));
    break;
  case clang::UO_Real:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "Real"));
    break;
  case clang::UO_Imag:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "Imag"));
    break;
  case clang::UO_Extension:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "Extension"));
    break;
  case clang::UO_Coawait:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("opcode", "Coawait"));
    break;
  default:
    ROSE_ASSERT(!"Unhandled clang::UnaryOperatorKind");
    break;
  }

  return VisitExpr(unary_operator, node_desc) && res;
}

bool ClangToDotTranslator::VisitVAArgExpr(clang::VAArgExpr *va_arg_expr,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitVAArgExpr" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("VAArgExpr");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "sub_expr", Traverse(va_arg_expr->getSubExpr())));

  return VisitExpr(va_arg_expr, node_desc);
}

bool ClangToDotTranslator::VisitLabelStmt(clang::LabelStmt *label_stmt,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitLabelStmt" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("LabelStmt");

  node_desc.attributes.push_back(
      std::pair<std::string, std::string>("name", label_stmt->getName()));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "sub_stmt", Traverse(label_stmt->getSubStmt())));

  return VisitStmt(label_stmt, node_desc) && res;
}

bool ClangToDotTranslator::VisitWhileStmt(clang::WhileStmt *while_stmt,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToDotTranslator::VisitWhileStmt" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("WhileStmt");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "cond", Traverse(while_stmt->getCond())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "body", Traverse(while_stmt->getBody())));

  return VisitStmt(while_stmt, node_desc);
}
