#include "sage3basic.h"
#include "clang-frontend-private.hpp"
#include "clang-to-rose-support.hpp"
#include <regex>
#include <utility>

#include "clang/Lex/Lexer.h"

#include "sageInterface.h"

using llvm::isa;  // For LLVM type checking (isa<Type>)

SgNode * ClangToSageTranslator::Traverse(clang::Stmt * stmt) {
    if (stmt == NULL)
        return NULL;

    std::map<clang::Stmt *, SgNode *>::iterator it = p_stmt_translation_map.find(stmt);
    if (it != p_stmt_translation_map.end())
        return it->second; 

    SgNode * result = NULL;
    bool ret_status = false;

    switch (stmt->getStmtClass()) {
        case clang::Stmt::GCCAsmStmtClass:
            ret_status = VisitGCCAsmStmt((clang::GCCAsmStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::MSAsmStmtClass:
            ret_status = VisitMSAsmStmt((clang::MSAsmStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::BreakStmtClass:
            ret_status = VisitBreakStmt((clang::BreakStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CapturedStmtClass:
            ret_status = VisitCapturedStmt((clang::CapturedStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CompoundStmtClass:
            ret_status = VisitCompoundStmt((clang::CompoundStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ContinueStmtClass:
            ret_status = VisitContinueStmt((clang::ContinueStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CoreturnStmtClass:
            ret_status = VisitCoreturnStmt((clang::CoreturnStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXCatchStmtClass:
            ret_status = VisitCXXCatchStmt((clang::CXXCatchStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXForRangeStmtClass:
            ret_status = VisitCXXForRangeStmt((clang::CXXForRangeStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXTryStmtClass:
            ret_status = VisitCXXTryStmt((clang::CXXTryStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::DeclStmtClass:
            ret_status = VisitDeclStmt((clang::DeclStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::DoStmtClass:
            ret_status = VisitDoStmt((clang::DoStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ForStmtClass:
            ret_status = VisitForStmt((clang::ForStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::GotoStmtClass:
            ret_status = VisitGotoStmt((clang::GotoStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::IfStmtClass:
            ret_status = VisitIfStmt((clang::IfStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::IndirectGotoStmtClass:
            ret_status = VisitIndirectGotoStmt((clang::IndirectGotoStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::MSDependentExistsStmtClass:
            ret_status = VisitMSDependentExistsStmt((clang::MSDependentExistsStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::NullStmtClass:
            ret_status = VisitNullStmt((clang::NullStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPAtomicDirectiveClass:
            ret_status = VisitOMPAtomicDirective((clang::OMPAtomicDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPBarrierDirectiveClass:
            ret_status = VisitOMPBarrierDirective((clang::OMPBarrierDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPCancellationPointDirectiveClass:
            ret_status = VisitOMPCancellationPointDirective((clang::OMPCancellationPointDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPCriticalDirectiveClass:
            ret_status = VisitOMPCriticalDirective((clang::OMPCriticalDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPFlushDirectiveClass:
            ret_status = VisitOMPFlushDirective((clang::OMPFlushDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPDistributeDirectiveClass:
            ret_status = VisitOMPDistributeDirective((clang::OMPDistributeDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPDistributeParallelForDirectiveClass:
            ret_status = VisitOMPDistributeParallelForDirective((clang::OMPDistributeParallelForDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPDistributeParallelForSimdDirectiveClass:
            ret_status = VisitOMPDistributeParallelForSimdDirective((clang::OMPDistributeParallelForSimdDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPDistributeSimdDirectiveClass:
            ret_status = VisitOMPDistributeSimdDirective((clang::OMPDistributeSimdDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPForDirectiveClass:
            ret_status = VisitOMPForDirective((clang::OMPForDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPForSimdDirectiveClass:
            ret_status = VisitOMPForSimdDirective((clang::OMPForSimdDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        //case clang::Stmt::OMPMasterTaskLoopDirectiveClass:
        //    ret_status = VisitOMPMasterTaskLoopDirective((clang::OMPMasterTaskLoopDirective *)stmt, &result);
        //    break;
        //case clang::Stmt::OMPMasterTaskLoopSimdDirectiveClass:
        //    ret_status = VisitOMPMasterTaskLoopSimdDirective((clang::OMPMasterTaskLoopSimdDirective *)stmt, &result);
        //    break;
        case clang::Stmt::OMPParallelForDirectiveClass:
            ret_status = VisitOMPParallelForDirective((clang::OMPParallelForDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPParallelForSimdDirectiveClass:
            ret_status = VisitOMPParallelForSimdDirective((clang::OMPParallelForSimdDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        //case clang::Stmt::OMPParallelMasterTaskLoopDirectiveClass:
        //    ret_status = VisitOMPParallelMasterTaskLoopDirective((clang::OMPParallelMasterTaskLoopDirective *)stmt, &result);
        //    break;
        case clang::Stmt::OMPSimdDirectiveClass:
            ret_status = VisitOMPSimdDirective((clang::OMPSimdDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPTargetParallelForDirectiveClass:
            ret_status = VisitOMPTargetParallelForDirective((clang::OMPTargetParallelForDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPTargetParallelForSimdDirectiveClass:
            ret_status = VisitOMPTargetParallelForSimdDirective((clang::OMPTargetParallelForSimdDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPTargetSimdDirectiveClass:
            ret_status = VisitOMPTargetSimdDirective((clang::OMPTargetSimdDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPTargetTeamsDistributeDirectiveClass:
            ret_status = VisitOMPTargetTeamsDistributeDirective((clang::OMPTargetTeamsDistributeDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        //case clang::Stmt::OMPTargetTeamsDistributeParallelForSimdDirectiveClass:
        //    ret_status = VisitOMPTargetTeamsDistributeParallelForSimdDirective((clang::OMPTargetTeamsDistributeParallelForSimdDirective *)stmt, &result);
        //    break;
        case clang::Stmt::OMPTargetTeamsDistributeSimdDirectiveClass:
            ret_status = VisitOMPTargetTeamsDistributeSimdDirective((clang::OMPTargetTeamsDistributeSimdDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPTaskLoopDirectiveClass:
            ret_status = VisitOMPTaskLoopDirective((clang::OMPTaskLoopDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPTaskLoopSimdDirectiveClass:
            ret_status = VisitOMPTaskLoopSimdDirective((clang::OMPTaskLoopSimdDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        //case clang::Stmt::OMPTeamDistributeDirectiveClass:
        //    ret_status = VisitOMPTeamDistributeDirective((clang::OMPTeamDistributeDirective *)stmt, &result);
        //    break;
        //case clang::Stmt::OMPTeamDistributeParallelForSimdDirectiveClass:
        //    ret_status = VisitOMPTeamDistributeParallelForSimdDirective((clang::OMPTeamDistributeParallelForSimdDirective *)stmt, &result);
        //    break;
        //case clang::Stmt::OMPTeamDistributeSimdDirectiveClass:
        //    ret_status = VisitOMPTeamDistributeSimdDirective((clang::OMPTeamDistributeSimdDirective *)stmt, &result);
        //    break;
        case clang::Stmt::OMPMasterDirectiveClass:
            ret_status = VisitOMPMasterDirective((clang::OMPMasterDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPOrderedDirectiveClass:
            ret_status = VisitOMPOrderedDirective((clang::OMPOrderedDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPParallelDirectiveClass:
            ret_status = VisitOMPParallelDirective((clang::OMPParallelDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OMPParallelSectionsDirectiveClass:
            ret_status = VisitOMPParallelSectionsDirective((clang::OMPParallelSectionsDirective *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ReturnStmtClass:
            ret_status = VisitReturnStmt((clang::ReturnStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::SEHExceptStmtClass:
            ret_status = VisitSEHExceptStmt((clang::SEHExceptStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::SEHFinallyStmtClass:
            ret_status = VisitSEHFinallyStmt((clang::SEHFinallyStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::SEHLeaveStmtClass:
            ret_status = VisitSEHLeaveStmt((clang::SEHLeaveStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::SEHTryStmtClass:
            ret_status = VisitSEHTryStmt((clang::SEHTryStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CaseStmtClass:
            ret_status = VisitCaseStmt((clang::CaseStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::DefaultStmtClass:
            ret_status = VisitDefaultStmt((clang::DefaultStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::SwitchStmtClass:
            ret_status = VisitSwitchStmt((clang::SwitchStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::AttributedStmtClass:
            ret_status = VisitAttributedStmt((clang::AttributedStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::BinaryConditionalOperatorClass:
            ret_status = VisitBinaryConditionalOperator((clang::BinaryConditionalOperator *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ConditionalOperatorClass:
            ret_status = VisitConditionalOperator((clang::ConditionalOperator *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::AddrLabelExprClass:
            ret_status = VisitAddrLabelExpr((clang::AddrLabelExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ArrayInitIndexExprClass:
            ret_status = VisitArrayInitIndexExpr((clang::ArrayInitIndexExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ArrayInitLoopExprClass:
            ret_status = VisitArrayInitLoopExpr((clang::ArrayInitLoopExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ArraySubscriptExprClass:
            ret_status = VisitArraySubscriptExpr((clang::ArraySubscriptExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ArrayTypeTraitExprClass:
            ret_status = VisitArrayTypeTraitExpr((clang::ArrayTypeTraitExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::AsTypeExprClass:
            ret_status = VisitAsTypeExpr((clang::AsTypeExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::AtomicExprClass:
            ret_status = VisitAtomicExpr((clang::AtomicExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CompoundAssignOperatorClass:
            ret_status = VisitCompoundAssignOperator((clang::CompoundAssignOperator *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::BlockExprClass:
            ret_status = VisitBlockExpr((clang::BlockExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CUDAKernelCallExprClass:
            ret_status = VisitCUDAKernelCallExpr((clang::CUDAKernelCallExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXMemberCallExprClass:
            ret_status = VisitCXXMemberCallExpr((clang::CXXMemberCallExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXOperatorCallExprClass:
            ret_status = VisitCXXOperatorCallExpr((clang::CXXOperatorCallExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::UserDefinedLiteralClass:
            ret_status = VisitUserDefinedLiteral((clang::UserDefinedLiteral *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::BuiltinBitCastExprClass:
            ret_status = VisitBuiltinBitCastExpr((clang::BuiltinBitCastExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CStyleCastExprClass:
            ret_status = VisitCStyleCastExpr((clang::CStyleCastExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXFunctionalCastExprClass:
            ret_status = VisitCXXFunctionalCastExpr((clang::CXXFunctionalCastExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXConstCastExprClass:
            ret_status = VisitCXXConstCastExpr((clang::CXXConstCastExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXDynamicCastExprClass:
            ret_status = VisitCXXDynamicCastExpr((clang::CXXDynamicCastExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXReinterpretCastExprClass:
            ret_status = VisitCXXReinterpretCastExpr((clang::CXXReinterpretCastExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXStaticCastExprClass:
            ret_status = VisitCXXStaticCastExpr((clang::CXXStaticCastExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ImplicitCastExprClass:
            ret_status = VisitImplicitCastExpr((clang::ImplicitCastExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CharacterLiteralClass:
            ret_status = VisitCharacterLiteral((clang::CharacterLiteral *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ChooseExprClass:
            ret_status = VisitChooseExpr((clang::ChooseExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CompoundLiteralExprClass:
            ret_status = VisitCompoundLiteralExpr((clang::CompoundLiteralExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        //case clang::Stmt::ConceptSpecializationExprClass:
        //    ret_status = VisitConceptSpecializationExpr((clang::ConceptSpecializationExpr *)stmt, &result);
        //    break;
        case clang::Stmt::ConvertVectorExprClass:
            ret_status = VisitConvertVectorExpr((clang::ConvertVectorExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CoawaitExprClass:
            ret_status = VisitCoawaitExpr((clang::CoawaitExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CoyieldExprClass:
            ret_status = VisitCoyieldExpr((clang::CoyieldExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXBindTemporaryExprClass:
            ret_status = VisitCXXBindTemporaryExpr((clang::CXXBindTemporaryExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXBoolLiteralExprClass:
            ret_status = VisitCXXBoolLiteralExpr((clang::CXXBoolLiteralExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXConstructExprClass:
            ret_status = VisitCXXConstructExpr((clang::CXXConstructExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXTemporaryObjectExprClass:
            ret_status = VisitCXXTemporaryObjectExpr((clang::CXXTemporaryObjectExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXDefaultArgExprClass:
            ret_status = VisitCXXDefaultArgExpr((clang::CXXDefaultArgExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXDefaultInitExprClass:
            ret_status = VisitCXXDefaultInitExpr((clang::CXXDefaultInitExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXDeleteExprClass:
            ret_status = VisitCXXDeleteExpr((clang::CXXDeleteExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXDependentScopeMemberExprClass:
            ret_status = VisitCXXDependentScopeMemberExpr((clang::CXXDependentScopeMemberExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXFoldExprClass:
            ret_status = VisitCXXFoldExpr((clang::CXXFoldExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXInheritedCtorInitExprClass:
            ret_status = VisitCXXInheritedCtorInitExpr((clang::CXXInheritedCtorInitExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXNewExprClass:
            ret_status = VisitCXXNewExpr((clang::CXXNewExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXNoexceptExprClass:
            ret_status = VisitCXXNoexceptExpr((clang::CXXNoexceptExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXNullPtrLiteralExprClass:
            ret_status = VisitCXXNullPtrLiteralExpr((clang::CXXNullPtrLiteralExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXPseudoDestructorExprClass:
            ret_status = VisitCXXPseudoDestructorExpr((clang::CXXPseudoDestructorExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        //case clang::Stmt::CXXRewrittenBinaryOperatorClass:
        //    ret_status = VisitCXXRewrittenBinaryOperator((clang::CXXRewrittenBinaryOperator *)stmt, &result);
        //    break;
        case clang::Stmt::CXXScalarValueInitExprClass:
            ret_status = VisitCXXScalarValueInitExpr((clang::CXXScalarValueInitExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXStdInitializerListExprClass:
            ret_status = VisitCXXStdInitializerListExpr((clang::CXXStdInitializerListExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXThisExprClass:
            ret_status = VisitCXXThisExpr((clang::CXXThisExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXThrowExprClass:
            ret_status = VisitCXXThrowExpr((clang::CXXThrowExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXTypeidExprClass:
            ret_status = VisitCXXTypeidExpr((clang::CXXTypeidExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXUnresolvedConstructExprClass:
            ret_status = VisitCXXUnresolvedConstructExpr((clang::CXXUnresolvedConstructExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CXXUuidofExprClass:
            ret_status = VisitCXXUuidofExpr((clang::CXXUuidofExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::DeclRefExprClass:
            ret_status = VisitDeclRefExpr((clang::DeclRefExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::DependentCoawaitExprClass:
            ret_status = VisitDependentCoawaitExpr((clang::DependentCoawaitExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::DependentScopeDeclRefExprClass:
            ret_status = VisitDependentScopeDeclRefExpr((clang::DependentScopeDeclRefExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::DesignatedInitExprClass:
            ret_status = VisitDesignatedInitExpr((clang::DesignatedInitExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::DesignatedInitUpdateExprClass:
            ret_status = VisitDesignatedInitUpdateExpr((clang::DesignatedInitUpdateExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ExpressionTraitExprClass:
            ret_status = VisitExpressionTraitExpr((clang::ExpressionTraitExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ExtVectorElementExprClass:
            ret_status = VisitExtVectorElementExpr((clang::ExtVectorElementExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::FixedPointLiteralClass:
            ret_status = VisitFixedPointLiteral((clang::FixedPointLiteral *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::FloatingLiteralClass:
            ret_status = VisitFloatingLiteral((clang::FloatingLiteral *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ConstantExprClass:
            ret_status = VisitConstantExpr((clang::ConstantExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ExprWithCleanupsClass:
            ret_status = VisitExprWithCleanups((clang::ExprWithCleanups *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::FunctionParmPackExprClass:
            ret_status = VisitFunctionParmPackExpr((clang::FunctionParmPackExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::GenericSelectionExprClass:
            ret_status = VisitGenericSelectionExpr((clang::GenericSelectionExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::GNUNullExprClass:
            ret_status = VisitGNUNullExpr((clang::GNUNullExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ImaginaryLiteralClass:
            ret_status = VisitImaginaryLiteral((clang::ImaginaryLiteral *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ImplicitValueInitExprClass:
            ret_status = VisitImplicitValueInitExpr((clang::ImplicitValueInitExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::InitListExprClass:
            ret_status = VisitInitListExpr((clang::InitListExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::IntegerLiteralClass:
            ret_status = VisitIntegerLiteral((clang::IntegerLiteral *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::LambdaExprClass:
            ret_status = VisitLambdaExpr((clang::LambdaExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::MaterializeTemporaryExprClass:
            ret_status = VisitMaterializeTemporaryExpr((clang::MaterializeTemporaryExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::MemberExprClass:
            ret_status = VisitMemberExpr((clang::MemberExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::MSPropertyRefExprClass:
            ret_status = VisitMSPropertyRefExpr((clang::MSPropertyRefExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::MSPropertySubscriptExprClass:
            ret_status = VisitMSPropertySubscriptExpr((clang::MSPropertySubscriptExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::NoInitExprClass:
            ret_status = VisitNoInitExpr((clang::NoInitExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OffsetOfExprClass:
            ret_status = VisitOffsetOfExpr((clang::OffsetOfExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ArraySectionExprClass:
            ret_status = VisitOMPArraySectionExpr((clang::ArraySectionExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::OpaqueValueExprClass:
            ret_status = VisitOpaqueValueExpr((clang::OpaqueValueExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::UnresolvedLookupExprClass:
            ret_status = VisitUnresolvedLookupExpr((clang::UnresolvedLookupExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::UnresolvedMemberExprClass:
            ret_status = VisitUnresolvedMemberExpr((clang::UnresolvedMemberExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::PackExpansionExprClass:
            ret_status = VisitPackExpansionExpr((clang::PackExpansionExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ParenExprClass:
            ret_status = VisitParenExpr((clang::ParenExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ParenListExprClass:
            ret_status = VisitParenListExpr((clang::ParenListExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::PredefinedExprClass:
            ret_status = VisitPredefinedExpr((clang::PredefinedExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::PseudoObjectExprClass:
            ret_status = VisitPseudoObjectExpr((clang::PseudoObjectExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::ShuffleVectorExprClass:
            ret_status = VisitShuffleVectorExpr((clang::ShuffleVectorExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::SizeOfPackExprClass:
            ret_status = VisitSizeOfPackExpr((clang::SizeOfPackExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::SourceLocExprClass:
            ret_status = VisitSourceLocExpr((clang::SourceLocExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::StmtExprClass:
            ret_status = VisitStmtExpr((clang::StmtExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::StringLiteralClass:
            ret_status = VisitStringLiteral((clang::StringLiteral *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::SubstNonTypeTemplateParmPackExprClass:
            ret_status = VisitSubstNonTypeTemplateParmPackExpr((clang::SubstNonTypeTemplateParmPackExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::SubstNonTypeTemplateParmExprClass:
            ret_status = VisitSubstNonTypeTemplateParmExpr((clang::SubstNonTypeTemplateParmExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::TypeTraitExprClass:
            ret_status = VisitTypeTraitExpr((clang::TypeTraitExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        // TypoExpr was removed in LLVM 20
        // case clang::Stmt::TypoExprClass:
        //     ret_status = VisitTypoExpr((clang::TypoExpr *)stmt, &result);
        //     ROSE_ASSERT(result != NULL);
        //     break;
        case clang::Stmt::UnaryExprOrTypeTraitExprClass:
            ret_status = VisitUnaryExprOrTypeTraitExpr((clang::UnaryExprOrTypeTraitExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::VAArgExprClass:
            ret_status = VisitVAArgExpr((clang::VAArgExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::LabelStmtClass:
            ret_status = VisitLabelStmt((clang::LabelStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::WhileStmtClass:
            ret_status = VisitWhileStmt((clang::WhileStmt *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::UnaryOperatorClass:
            ret_status = VisitUnaryOperator((clang::UnaryOperator *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::CallExprClass:
            ret_status = VisitCallExpr((clang::CallExpr *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::BinaryOperatorClass:
            ret_status = VisitBinaryOperator((clang::BinaryOperator *)stmt, &result);
            ROSE_ASSERT(result != NULL);
            break;
        case clang::Stmt::RecoveryExprClass:
            result = SageBuilder::buildIntVal(42);
            ROSE_ASSERT(FAIL_FIXME == 0); // There is no concept of recovery expression in ROSE
            break;

        default:
            std::cerr << "Unknown statement kind: " << stmt->getStmtClassName() << " !" << std::endl;
            ROSE_ABORT();
    }

    ROSE_ASSERT(result != NULL);

    p_stmt_translation_map.insert(std::pair<clang::Stmt *, SgNode *>(stmt, result));

    return result;
}

/********************/
/* Visit Statements */
/********************/

bool ClangToSageTranslator::VisitStmt(clang::Stmt * stmt, SgNode ** node)
   {
#if DEBUG_VISIT_STMT
     std::cerr << "ClangToSageTranslator::VisitStmt" << std::endl;
#endif

     if (*node == NULL)
        {
          std::cerr << "Runtime error: No Sage node associated with the Statement: " << stmt->getStmtClassName() << std::endl;
          stmt->dump();
          return false;
        }

  // TODO Is there anything else todo?

     if (isSgLocatedNode(*node) != NULL && (isSgLocatedNode(*node)->get_file_info() == NULL || !(isSgLocatedNode(*node)->get_file_info()->isCompilerGenerated()) ))
        {
          applySourceRange(*node, stmt->getSourceRange());
        }

     return true;
   }

bool ClangToSageTranslator::VisitAsmStmt(clang::AsmStmt * asm_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitAsmStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO
    return VisitStmt(asm_stmt, node) && res;
}


bool ClangToSageTranslator::VisitGCCAsmStmt(clang::GCCAsmStmt * gcc_asm_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitGCCAsmStmt" << std::endl;
#endif
    bool res = true;

    unsigned asmNumInput = gcc_asm_stmt->getNumInputs();
    unsigned asmNumOutput = gcc_asm_stmt->getNumOutputs();
    unsigned asmClobber = gcc_asm_stmt->getNumClobbers();

    // LLVM 20 returns StringLiteral*, LLVM 21 returns std::string
    std::string AsmString;
#if LLVM_VERSION_MAJOR >= 21
    AsmString = gcc_asm_stmt->getAsmString();
#else
    if (auto* str_lit = gcc_asm_stmt->getAsmString()) {
        AsmString = str_lit->getString().str();
    }
#endif

    std::cout << "input op:" << asmNumInput << " output op: " << asmNumOutput<< std::endl;
#if DEBUG_VISIT_STMT
    std::cerr << "AsmString:" << AsmString << std::endl;
#endif

    SgAsmStmt* asmStmt = SageBuilder::buildAsmStatement(AsmString); 
    asmStmt->set_firstNondefiningDeclaration(asmStmt);
    asmStmt->set_definingDeclaration(asmStmt);
    asmStmt->set_parent(SageBuilder::topScopeStack());
    asmStmt->set_useGnuExtendedFormat(true);


    // Pei-Hung (03/22/2022)  The clobber string is available.
    // The implementation adding clobber into ROSE AST is not in place.
    for(unsigned i=0; i < asmClobber; ++i)
    {
      std::string clobberStr = static_cast<std::string>(gcc_asm_stmt->getClobber(i));
#if DEBUG_VISIT_STMT
      std::cerr << "AsmOp clobber["<< i<<  "]: " << clobberStr << std::endl;
#endif
      // Pei-Hung "cc" clobber is skipped by EDG
      if(clobberStr.compare(0, sizeof(clobberStr), "cc") == 0)
        continue;
  
      SgInitializedName::asm_register_name_enum sageRegisterName = get_sgAsmRegister(clobberStr);
      asmStmt->get_clobberRegisterList().push_back(sageRegisterName);
    }

    // Pei-Hung (03/22/2022) use regular expression to check the first modifier, + and =, for ouput Ops.  
    // Then the second modifier for both input and output Ops.  The rest is for constraints.
    // regex_match should report 4 matched results:
    // 1. the whole matched string
    // 2. first modifier: =, +, or empty
    // 3. second modifier: empty or &, %, *, #, ?, !
    // 4. The constraint
    std::regex e ("([\\=\\+]*)([\\&\\%\\*\\#\\?\\!]*)(.+)", std::regex_constants::ECMAScript | std::regex_constants::icase);

    // process output
    for(unsigned i=0; i < asmNumOutput; ++i)
    {
      SgNode* tmp_node = Traverse(gcc_asm_stmt->getOutputExpr(i));
      SgExpression * outputExpr = isSgExpression(tmp_node);
      ROSE_ASSERT(outputExpr != NULL);

      std::string outputConstraintStr = static_cast<std::string>(gcc_asm_stmt->getOutputConstraint(i));
// Clang's constraint is equivalent to ROSE's modifier + operand constraints 
#if DEBUG_VISIT_STMT
      std::cerr << "AsmOp output constraint["<< i<<  "]: " << outputConstraintStr << std::endl;
#endif

      std::smatch sm; 
      std::regex_match (outputConstraintStr,sm,e);
#if DEBUG_VISIT_STMT
        std::cout << "string literal: "<< outputConstraintStr  <<"  with " << sm.size() << " matches\n";
        if(sm.size())
          std::cout << "the matches were: ";
        for (unsigned i=0; i<sm.size(); ++i) {
          std::cout << "[" << sm[i] << "] \n";
        }
        if(sm.size())
          std::cout << std::endl;
#endif

      SgAsmOp::asm_operand_constraint_enum constraint = (SgAsmOp::asm_operand_constraint_enum) SgAsmOp::e_any;
      SgAsmOp::asm_operand_modifier_enum   modifiers  = (SgAsmOp::asm_operand_modifier_enum)   SgAsmOp::e_unknown;
      SgAsmOp* sageAsmOp = new SgAsmOp(constraint,modifiers,outputExpr);
      outputExpr->set_parent(sageAsmOp);

      sageAsmOp->set_recordRawAsmOperandDescriptions(false);

      // set as an output AsmOp
      sageAsmOp->set_isOutputOperand (true);

      ROSE_ASSERT(sm.size() == 4);

      unsigned modifierVal = static_cast<int>(modifiers);
      if(!sm[1].str().empty())
        modifierVal += static_cast<int>(get_sgAsmOperandModifier(sm[1].str()));

      if(!sm[2].str().empty())
        modifierVal += static_cast<int>(get_sgAsmOperandModifier(sm[2].str()));
     
      sageAsmOp->set_modifiers(static_cast<SgAsmOp::asm_operand_modifier_enum>(modifierVal));

      // set constraint
      sageAsmOp->set_constraint(get_sgAsmOperandConstraint(sm[3].str()));
      sageAsmOp->set_constraintString(sm[3]);


      Sg_File_Info * start_fi = Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
      start_fi->setCompilerGenerated();
      sageAsmOp->set_startOfConstruct(start_fi);

      Sg_File_Info * end_fi   = Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
      end_fi->setCompilerGenerated();
      sageAsmOp->set_endOfConstruct(end_fi);
      
      asmStmt->get_operands().push_back(sageAsmOp);
      sageAsmOp->set_parent(asmStmt);
    }

    // process input
    for(unsigned i=0; i < asmNumInput; ++i)
    {
      SgNode* tmp_node = Traverse(gcc_asm_stmt->getInputExpr(i));
      SgExpression * inputExpr = isSgExpression(tmp_node);
      ROSE_ASSERT(inputExpr != NULL);

      std::string inputConstraintStr = static_cast<std::string>(gcc_asm_stmt->getInputConstraint(i));
// Clang's constraint is equivalent to ROSE's modifier + operand constraints 
#if DEBUG_VISIT_STMT
      std::cerr << "AsmOp input constraint["<< i<<  "]: " << inputConstraintStr << std::endl;
#endif

      std::smatch sm; 
      std::regex_match (inputConstraintStr,sm,e);
#if DEBUG_VISIT_STMT
        std::cout << "string literal: "<< inputConstraintStr  <<"  with " << sm.size() << " matches\n";
        if(sm.size())
          std::cout << "the matches were: ";
        for (unsigned i=0; i<sm.size(); ++i) {
          std::cout << "[" << sm[i] << "] \n";
        }
        if(sm.size())
          std::cout << std::endl;
#endif

      SgAsmOp::asm_operand_constraint_enum constraint = (SgAsmOp::asm_operand_constraint_enum) SgAsmOp::e_any;
      SgAsmOp::asm_operand_modifier_enum   modifiers  = (SgAsmOp::asm_operand_modifier_enum)   SgAsmOp::e_unknown;
      SgAsmOp* sageAsmOp = new SgAsmOp(constraint,modifiers,inputExpr);
      inputExpr->set_parent(sageAsmOp);

      sageAsmOp->set_recordRawAsmOperandDescriptions(false);

      // set as an input AsmOp
      sageAsmOp->set_isOutputOperand (false);

      ROSE_ASSERT(sm.size() == 4);

      unsigned modifierVal = static_cast<int>(modifiers);

      // "+" and "=" should not be part of the input AsmOp.  Skip checking sm[1] for the inputs.

//      if(!sm[1].str().empty())
//        modifierVal += static_cast<int>(get_sgAsmOperandModifier(sm[1].str()));
//        modifiers &= get_sgAsmOperandModifier(sm[1].str());

      if(!sm[2].str().empty())
        modifierVal += static_cast<int>(get_sgAsmOperandModifier(sm[2].str()));
     
      sageAsmOp->set_modifiers(static_cast<SgAsmOp::asm_operand_modifier_enum>(modifierVal));

      // set constraint
      sageAsmOp->set_constraint(get_sgAsmOperandConstraint(sm[3].str()));
      sageAsmOp->set_constraintString(sm[3]);


      Sg_File_Info * start_fi = Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
      start_fi->setCompilerGenerated();
      sageAsmOp->set_startOfConstruct(start_fi);

      Sg_File_Info * end_fi   = Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
      end_fi->setCompilerGenerated();
      sageAsmOp->set_endOfConstruct(end_fi);
      
      asmStmt->get_operands().push_back(sageAsmOp);
      sageAsmOp->set_parent(asmStmt);
    }
    *node = asmStmt;

    return VisitStmt(gcc_asm_stmt, node) && res;
}

bool ClangToSageTranslator::VisitMSAsmStmt(clang::MSAsmStmt * ms_asm_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitMSAsmStmt" << std::endl;
#endif
    bool res = true;

    return VisitStmt(ms_asm_stmt, node) && res;
}

bool ClangToSageTranslator::VisitBreakStmt(clang::BreakStmt * break_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitBreakStmt" << std::endl;
#endif

    *node = SageBuilder::buildBreakStmt();
    return VisitStmt(break_stmt, node);
}

bool ClangToSageTranslator::VisitCapturedStmt(clang::CapturedStmt * captured_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCapturedStmt" << std::endl;
#endif
    bool res = true;

    SgNode * tmp_stmt = Traverse(captured_stmt->getCapturedStmt());
    SgStatement * body = isSgStatement(tmp_stmt);
    if (tmp_stmt != NULL && body == NULL) {
        std::cerr << "Runtime error: CapturedStmt child did not translate into an SgStatement." << std::endl;
        res = false;
    }

    if (body == NULL) {
        body = SageBuilder::buildNullStatement();
    }

    *node = body;

    return VisitStmt(captured_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCompoundStmt(clang::CompoundStmt * compound_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCompoundStmt" << std::endl;
#endif

    bool res = true;

    SgBasicBlock * block = SageBuilder::buildBasicBlock();

    block->set_parent(SageBuilder::topScopeStack());

    SageBuilder::pushScopeStack(block);

    clang::CompoundStmt::body_iterator it;
    for (it = compound_stmt->body_begin(); it != compound_stmt->body_end(); it++) {
        SgNode * tmp_node = Traverse(*it);

#if DEBUG_VISIT_STMT
        if (tmp_node != NULL)
          std::cerr << "In VisitCompoundStmt : child is " << tmp_node->class_name() << std::endl;
        else
          std::cerr << "In VisitCompoundStmt : child is NULL" << std::endl;
#endif

        SgClassDeclaration * class_decl = isSgClassDeclaration(tmp_node);
        if (class_decl != NULL && (class_decl->get_name() == "" || class_decl->get_isUnNamed())) continue;
        SgEnumDeclaration * enum_decl = isSgEnumDeclaration(tmp_node);
        if (enum_decl != NULL && (enum_decl->get_name() == "" || enum_decl->get_isUnNamed())) continue;
#if DEBUG_VISIT_STMT
        else if (enum_decl != NULL)
          std::cerr << "enum_decl = " << enum_decl << " >> name: " << enum_decl->get_name() << std::endl;
#endif

        SgStatement * stmt  = isSgStatement(tmp_node);
        SgExpression * expr = isSgExpression(tmp_node);
        if (tmp_node != NULL && stmt == NULL && expr == NULL) {
            std::cerr << "Runtime error: tmp_node != NULL && stmt == NULL && expr == NULL" << std::endl;
            res = false;
        }
        else if (stmt != NULL) {
            block->append_statement(stmt);
        }
        else if (expr != NULL) {
            SgExprStatement * expr_stmt = SageBuilder::buildExprStatement(expr);
            block->append_statement(expr_stmt);
        }
    }

    SageBuilder::popScopeStack();

    *node = block;

    return VisitStmt(compound_stmt, node) && res;
}

bool ClangToSageTranslator::VisitContinueStmt(clang::ContinueStmt * continue_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitContinueStmt" << std::endl;
#endif

    *node = SageBuilder::buildContinueStmt();
    return VisitStmt(continue_stmt, node);
}

bool ClangToSageTranslator::VisitCoreturnStmt(clang::CoreturnStmt * core_turn_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCoreturnStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO
    return VisitStmt(core_turn_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCoroutineBodyStmt(clang::CoroutineBodyStmt * coroutine_body_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCoroutineBodyStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO
    return VisitStmt(coroutine_body_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCXXCatchStmt(clang::CXXCatchStmt * cxx_catch_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXCatchStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO
    return VisitStmt(cxx_catch_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCXXForRangeStmt(clang::CXXForRangeStmt * cxx_for_range_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXForRangeStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO
    return VisitStmt(cxx_for_range_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCXXTryStmt(clang::CXXTryStmt * cxx_try_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXTryStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO
    return VisitStmt(cxx_try_stmt, node) && res;
}

bool ClangToSageTranslator::VisitDeclStmt(clang::DeclStmt * decl_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitDeclStmt" << std::endl;
#endif

    bool res = true;

    if (decl_stmt->isSingleDecl()) {
        *node = Traverse(decl_stmt->getSingleDecl());
#if DEBUG_VISIT_STMT
        printf ("In VisitDeclStmt(): *node = %p = %s \n",*node,(*node)->class_name().c_str());
#endif
    }
    else {
        std::vector<SgNode *> tmp_decls;
        //SgDeclarationStatement * decl;
        clang::DeclStmt::decl_iterator it;

        SgScopeStatement * scope = SageBuilder::topScopeStack();

        for (it = decl_stmt->decl_begin(); it != decl_stmt->decl_end()-1; it++) {
            clang::Decl* decl = (*it);
            if (decl == nullptr) continue;
            SgNode * child = Traverse(decl);

            SgDeclarationStatement * sub_decl_stmt = isSgDeclarationStatement(child);
            if (sub_decl_stmt == NULL && child != NULL) {
                std::cerr << "Runtime error: the node produce for a clang::Decl is not a SgDeclarationStatement !" << std::endl;
                std::cerr << "    class = " << child->class_name() << std::endl;
                res = false;
                continue;
            }
            else if (child != NULL) {
                // FIXME This is a hack to avoid autonomous decl of unnamed type to being added to the global scope....
                SgClassDeclaration * class_decl = isSgClassDeclaration(child);
                if (class_decl != NULL && (class_decl->get_name() == "" || class_decl->get_isUnNamed())) continue;

                SgEnumDeclaration * enum_decl = isSgEnumDeclaration(child);
                if (enum_decl != NULL && (enum_decl->get_name() == "" || enum_decl->get_isUnNamed())) continue;
                if(clang::TagDecl::classof(decl))
                {
                  clang::TagDecl* tagDecl = (clang::TagDecl*)decl;
                  if(tagDecl->isEmbeddedInDeclarator())  continue;
                }

            }
            scope->append_statement(sub_decl_stmt);
            sub_decl_stmt->set_parent(scope);
        }
        // last declaration in scope
        it = decl_stmt->decl_end();
        --it;
        SgNode * lastDecl = Traverse((clang::Decl*)(*it));
        SgDeclarationStatement * last_decl_Stmt = isSgDeclarationStatement(lastDecl);
        if (lastDecl != NULL && last_decl_Stmt == NULL) {
            std::cerr << "Runtime error: lastDecl != NULL && last_decl_Stmt == NULL" << std::endl;
            res = false;
        }
        *node = last_decl_Stmt;
    }

#if DEBUG_VISIT_STMT
    printf ("In VisitDeclStmt(): identify where the parent is not set: *node = %p = %s \n",*node,(*node)->class_name().c_str());
    printf (" --- *node parent = %p \n",(*node)->get_parent());
#endif

    return res;
}

bool ClangToSageTranslator::VisitDoStmt(clang::DoStmt * do_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitDoStmt" << std::endl;
#endif

    SgNode * tmp_cond = Traverse(do_stmt->getCond());
    SgExpression * cond = isSgExpression(tmp_cond);
    ROSE_ASSERT(cond != NULL);

    SgStatement * expr_stmt = SageBuilder::buildExprStatement(cond);

    ROSE_ASSERT(expr_stmt != NULL);

    SgDoWhileStmt * sg_do_stmt = SageBuilder::buildDoWhileStmt_nfi(expr_stmt, NULL);

    sg_do_stmt->set_condition(expr_stmt);

    cond->set_parent(expr_stmt);
    expr_stmt->set_parent(sg_do_stmt);

    SageBuilder::pushScopeStack(sg_do_stmt);

    SgNode * tmp_body = Traverse(do_stmt->getBody());
    SgStatement * body = isSgStatement(tmp_body);
    SgExpression * expr = isSgExpression(tmp_body);
    if (expr != NULL) {
        body =  SageBuilder::buildExprStatement(expr);
        applySourceRange(body, do_stmt->getBody()->getSourceRange());
    }
    ROSE_ASSERT(body != NULL);

    body->set_parent(sg_do_stmt);

    SageBuilder::popScopeStack();

    sg_do_stmt->set_body(body);

    *node = sg_do_stmt;

    return VisitStmt(do_stmt, node); 
}

bool ClangToSageTranslator::VisitForStmt(clang::ForStmt * for_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitForStmt" << std::endl;
#endif

    bool res = true;

 // DQ (11/28/2020): We have to build the scope first, and then build the rest bottom up.
    SgForStatement* sg_for_stmt = new SgForStatement((SgStatement*)NULL,(SgExpression*)NULL,(SgStatement*)NULL);

#if DEBUG_VISIT_STMT
    printf ("In VisitForStmt(): Setting the parent of the sg_for_stmt \n");
#endif

 // DQ (11/28/2020): this is required for test2012_127.c.
    sg_for_stmt->set_parent(SageBuilder::topScopeStack());

 // DQ (11/28/2020): Adding asertion.
    ROSE_ASSERT(sg_for_stmt->get_parent() != NULL);

    SageBuilder::pushScopeStack(sg_for_stmt);

  // Initialization

    SgForInitStatement * for_init_stmt = NULL;

    {
        SgStatementPtrList for_init_stmt_list;
        SgNode * tmp_init = Traverse(for_stmt->getInit());
        SgStatement * init_stmt = isSgStatement(tmp_init);
        SgExpression * init_expr = isSgExpression(tmp_init);
        if (tmp_init != NULL && init_stmt == NULL && init_expr == NULL) {
            std::cerr << "Runtime error: tmp_init != NULL && init_stmt == NULL && init_expr == NULL (" << tmp_init->class_name() << ")" << std::endl;
            res = false;
        }
        else if (init_expr != NULL) {
            init_stmt = SageBuilder::buildExprStatement(init_expr);
            applySourceRange(init_stmt, for_stmt->getInit()->getSourceRange());
        }
        if (init_stmt != NULL)
            for_init_stmt_list.push_back(init_stmt);

        if(for_init_stmt_list.size() == 0)
        {
          SgNullStatement* nullStmt = SageBuilder::buildNullStatement_nfi();
          setCompilerGeneratedFileInfo(nullStmt, true);
          for_init_stmt_list.push_back(nullStmt);
        }

        for_init_stmt = SageBuilder::buildForInitStatement_nfi(for_init_stmt_list);

#if DEBUG_VISIT_STMT
        printf ("In VisitForStmt(): for_init_stmt = %p  \n");
#endif

        if (for_stmt->getInit() != NULL)
            applySourceRange(for_init_stmt, for_stmt->getInit()->getSourceRange());
        else
            setCompilerGeneratedFileInfo(for_init_stmt, true);
    }

  // Condition

    SgStatement * cond_stmt = NULL;

    {
        SgNode * tmp_cond = Traverse(for_stmt->getCond());
        SgExpression * cond = isSgExpression(tmp_cond);
        if (tmp_cond != NULL && cond == NULL) {
            std::cerr << "Runtime error: tmp_cond != NULL && cond == NULL" << std::endl;
            res = false;
        }
        if (cond != NULL) {
            cond_stmt = SageBuilder::buildExprStatement(cond);
            applySourceRange(cond_stmt, for_stmt->getCond()->getSourceRange());
        }
        else {
            cond_stmt = SageBuilder::buildNullStatement_nfi();
            setCompilerGeneratedFileInfo(cond_stmt, true);
        }

        if (cond_stmt != NULL) {
            auto *expr_stmt = isSgExprStatement(cond_stmt);
            if (expr_stmt != NULL) {
                auto simplifyOperand = [](SgExpression *operand) -> SgExpression * {
                    SgExpression *current = operand;
                    while (auto cast = isSgCastExp(current)) {
                        current = cast->get_operand_i();
                    }
                    if (isSgVarRefExp(current) != NULL || isSgIntVal(current) != NULL ||
                        isSgUnsignedIntVal(current) != NULL || isSgLongLongIntVal(current) != NULL ||
                        isSgUnsignedLongLongIntVal(current) != NULL) {
                        return SageInterface::copyExpression(current);
                    }
                    return nullptr;
                };

                if (auto *less_than = isSgLessThanOp(expr_stmt->get_expression())) {
                    SgExpression *lhs_simplified = simplifyOperand(less_than->get_lhs_operand());
                    SgExpression *rhs_simplified = simplifyOperand(less_than->get_rhs_operand());
                    if (lhs_simplified != NULL && rhs_simplified != NULL) {
                        SgExpression *new_cond = SageBuilder::buildLessThanOp(lhs_simplified, rhs_simplified);
                        applySourceRange(new_cond, for_stmt->getCond()->getSourceRange());
                        expr_stmt->set_expression(new_cond);
                        new_cond->set_parent(expr_stmt);
                    }
                }
            }
        }
    }

  // Increment

    SgExpression * inc = NULL;

    {
        SgNode * tmp_inc  = Traverse(for_stmt->getInc());
        inc = isSgExpression(tmp_inc);
        if (tmp_inc != NULL && inc == NULL) {
            std::cerr << "Runtime error: tmp_inc != NULL && inc == NULL" << std::endl;
            res = false;
        }
        if (inc == NULL) {
            inc = SageBuilder::buildNullExpression_nfi();
            setCompilerGeneratedFileInfo(inc, true);
        }
    }

  // Body

    SgStatement * body = NULL;

    {
        SgNode * tmp_body = Traverse(for_stmt->getBody());
        body = isSgStatement(tmp_body);
        if (body == NULL) {
            SgExpression * body_expr = isSgExpression(tmp_body);
            if (body_expr != NULL) {
                body = SageBuilder::buildExprStatement(body_expr);
                applySourceRange(body, for_stmt->getBody()->getSourceRange());
            }
        }
        if (tmp_body != NULL && body == NULL) {
            std::cerr << "Runtime error: tmp_body != NULL && body == NULL" << std::endl;
            res = false;
        }
        if (body == NULL) {
            body = SageBuilder::buildNullStatement_nfi();
            setCompilerGeneratedFileInfo(body);
        }
    }

    SageBuilder::popScopeStack();

  // Attach sub trees to the for statement

    for_init_stmt->set_parent(sg_for_stmt);
    if (sg_for_stmt->get_for_init_stmt() != NULL)
        SageInterface::deleteAST(sg_for_stmt->get_for_init_stmt());
    sg_for_stmt->set_for_init_stmt(for_init_stmt);

    if (cond_stmt != NULL) {
        cond_stmt->set_parent(sg_for_stmt);
        sg_for_stmt->set_test(cond_stmt);
    }

    if (inc != NULL) {
        inc->set_parent(sg_for_stmt);
        sg_for_stmt->set_increment(inc);
    }

    if (body != NULL) {
        body->set_parent(sg_for_stmt);
        sg_for_stmt->set_loop_body(body);
    }

 // DQ (11/28/2020): Now we want to use the scope that is already on the stack (instead of adding a new one).
    SageBuilder::buildForStatement_nfi(sg_for_stmt, for_init_stmt, cond_stmt, inc, body);

 // DQ (11/28/2020): Adding asertion.
    ROSE_ASSERT(sg_for_stmt->get_parent() != NULL);

    *node = sg_for_stmt;

    return VisitStmt(for_stmt, node) && res;
}

bool ClangToSageTranslator::VisitGotoStmt(clang::GotoStmt * goto_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitGotoStmt" << std::endl;
#endif

    bool res = true;

    SgSymbol * tmp_sym = GetSymbolFromSymbolTable(goto_stmt->getLabel());
    SgLabelSymbol * sym = isSgLabelSymbol(tmp_sym);
    if (sym == NULL) {
        SgNode * tmp_label = Traverse(goto_stmt->getLabel()->getStmt());
        SgLabelStatement * label_stmt = isSgLabelStatement(tmp_label);
        if (label_stmt == NULL) {
            std::cerr << "Runtime error: Cannot find the symbol for the label: \"" << goto_stmt->getLabel()->getStmt()->getName() << "\"." << std::endl;
            std::cerr << "Runtime Error: Cannot find the label: \"" << goto_stmt->getLabel()->getStmt()->getName() << "\"." << std::endl;
            res = false;
        }
        else {
            *node = SageBuilder::buildGotoStatement(label_stmt);
        }
    }
    else {
        *node = SageBuilder::buildGotoStatement(sym->get_declaration());
    }

/*
    SgNode * tmp_label = Traverse(goto_stmt->getLabel()->getStmt());
    SgLabelStatement * label_stmt = isSgLabelStatement(tmp_label);
    if (label_stmt == NULL) {
        std::cerr << "Runtime Error: Cannot find the label: \"" << goto_stmt->getLabel()->getStmt()->getName() << "\"." << std::endl;
        res = false;
    }
    else {
        *node = SageBuilder::buildGotoStatement(label_stmt);
    }
*/
    return VisitStmt(goto_stmt, node) && res;
}


bool ClangToSageTranslator::VisitIfStmt(clang::IfStmt * if_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitIfStmt" << std::endl;
#endif

    bool res = true;

    // TODO if_stmt->getConditionVariable() appears when a variable is declared in the condition...

    *node = SageBuilder::buildIfStmt_nfi(NULL, NULL, NULL);

    // Pei-Hung (04/22/22) Needs to setup parent node before processing the operands.
    // Needed for test2013_55.c and other similar tests
    (*node)->set_parent(SageBuilder::topScopeStack());
    SageBuilder::pushScopeStack(isSgScopeStatement(*node));

    SgNode * tmp_cond = Traverse(if_stmt->getCond());
    SgExpression * cond_expr = isSgExpression(tmp_cond);
    SgStatement * cond_stmt = SageBuilder::buildExprStatement(cond_expr);
    applySourceRange(cond_stmt, if_stmt->getCond()->getSourceRange());

    SgNode * tmp_then = Traverse(if_stmt->getThen());
    SgStatement * then_stmt = isSgStatement(tmp_then);
    if (then_stmt == NULL) {
        SgExpression * then_expr = isSgExpression(tmp_then);
        ROSE_ASSERT(then_expr != NULL);
        then_stmt = SageBuilder::buildExprStatement(then_expr);
    }
    applySourceRange(then_stmt, if_stmt->getThen()->getSourceRange());

    SgNode * tmp_else = Traverse(if_stmt->getElse());
    SgStatement * else_stmt = isSgStatement(tmp_else);
    if (else_stmt == NULL) {
        SgExpression * else_expr = isSgExpression(tmp_else);
        if (else_expr != NULL)
            else_stmt = SageBuilder::buildExprStatement(else_expr);
    }
    if (else_stmt != NULL) applySourceRange(else_stmt, if_stmt->getElse()->getSourceRange());

    SageBuilder::popScopeStack();

    cond_stmt->set_parent(*node);
    isSgIfStmt(*node)->set_conditional(cond_stmt);

    then_stmt->set_parent(*node);
    isSgIfStmt(*node)->set_true_body(then_stmt);
    if (else_stmt != NULL) {
      else_stmt->set_parent(*node);
      isSgIfStmt(*node)->set_false_body(else_stmt);
    }

    return VisitStmt(if_stmt, node) && res;
}

bool ClangToSageTranslator::VisitIndirectGotoStmt(clang::IndirectGotoStmt * indirect_goto_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitIndirectGotoStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

     return VisitStmt(indirect_goto_stmt, node) && res;
}

bool ClangToSageTranslator::VisitMSDependentExistsStmt(clang::MSDependentExistsStmt * ms_dependent_exists_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitMSDependentExistsStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

     return VisitStmt(ms_dependent_exists_stmt, node) && res;
}

bool ClangToSageTranslator::VisitNullStmt(clang::NullStmt * null_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitNullStmt" << std::endl;
#endif
    bool res = true;

    *node = SageBuilder::buildNullStatement();

    return VisitStmt(null_stmt, node) && res;
}

bool ClangToSageTranslator::VisitOMPExecutableDirective(clang::OMPExecutableDirective * omp_executable_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPExecutableDirective" << std::endl;
#endif
    bool res = true;
    SgStatement *associated_stmt = nullptr;

    if (clang::Stmt *clang_associated_stmt = omp_executable_directive->getAssociatedStmt()) {
        SgNode *tmp_stmt = Traverse(clang_associated_stmt);
        associated_stmt = isSgStatement(tmp_stmt);
        if (tmp_stmt != NULL && associated_stmt == NULL) {
            std::cerr << "Runtime error: associated OpenMP statement did not translate into an SgStatement." << std::endl;
            res = false;
        }
    }

    SgStatement *target_stmt = associated_stmt;

    if (target_stmt == nullptr) {
        target_stmt = SageBuilder::buildNullStatement();
        target_stmt->set_parent(SageBuilder::topScopeStack());
    }

    {
        clang::SourceLocation begin = omp_executable_directive->getBeginLoc();
        clang::SourceLocation end = omp_executable_directive->getEndLoc();
        if (begin.isValid() && end.isValid()) {
            clang::SourceManager &sm = p_compiler_instance->getSourceManager();
            clang::LangOptions &lang_opts = p_compiler_instance->getLangOpts();
            auto range = clang::CharSourceRange::getTokenRange(begin, end);
            std::string directive_text = clang::Lexer::getSourceText(range, sm, lang_opts).str();

            if (!directive_text.empty()) {
                auto first_non_ws = directive_text.find_first_not_of(" \t");
                if (first_non_ws != std::string::npos && first_non_ws > 0) {
                    directive_text.erase(0, first_non_ws);
                }
                auto last_non_ws = directive_text.find_last_not_of(" \t\r\n");
                if (last_non_ws != std::string::npos && last_non_ws + 1 < directive_text.size()) {
                    directive_text.erase(last_non_ws + 1);
                }
                if (!directive_text.empty() && directive_text.rfind("#pragma", 0) != 0) {
                    directive_text.insert(0, "#pragma ");
                }
                if (!directive_text.empty()) {
                    auto filename_ref = sm.getFilename(begin);
                    std::string filename = filename_ref.empty() ? std::string("<unknown>") : filename_ref.str();
                    unsigned line = sm.getPresumedLineNumber(begin);
                    unsigned column = sm.getPresumedColumnNumber(begin);

                    size_t search_pos = 0;
                    while (true) {
                        size_t newline_pos = directive_text.find_first_of("\r\n", search_pos);
                        if (newline_pos == std::string::npos) {
                            directive_text.push_back('\n');
                            break;
                        }

                        size_t check_pos = newline_pos;
                        while (check_pos > 0 && (directive_text[check_pos - 1] == '\r' || directive_text[check_pos - 1] == '\n'))
                            --check_pos;
                        while (check_pos > 0 && (directive_text[check_pos - 1] == ' ' || directive_text[check_pos - 1] == '\t'))
                            --check_pos;

                        bool continued = (check_pos > 0 && directive_text[check_pos - 1] == '\\');
                        if (continued) {
                            search_pos = newline_pos + 1;
                            continue;
                        }

                        size_t end_pos = newline_pos + 1;
                        if (directive_text[newline_pos] == '\r' &&
                            end_pos < directive_text.size() &&
                            directive_text[end_pos] == '\n') {
                            ++end_pos;
                        }
                        directive_text.erase(end_pos);
                        break;
                    }

                    PreprocessingInfo *info = new PreprocessingInfo(
                        PreprocessingInfo::CMacroCallStatement,
                        directive_text,
                        filename,
                        line,
                        column,
                        0,
                        PreprocessingInfo::before);

                    info->get_file_info()->setTransformation();
                    target_stmt->addToAttachedPreprocessingInfo(info, PreprocessingInfo::before);
                }
            }
        }
    }

    *node = target_stmt;

    return VisitStmt(omp_executable_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPAtomicDirective(clang::OMPAtomicDirective * omp_atomic_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPAtomicDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_atomic_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPBarrierDirective(clang::OMPBarrierDirective * omp_barrier_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPBarrierDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_barrier_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPCancelDirective(clang::OMPCancelDirective * omp_cancel_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPCancelDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_cancel_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPCancellationPointDirective(clang::OMPCancellationPointDirective * omp_cancellation_point_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPCancellationPointDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_cancellation_point_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPCriticalDirective(clang::OMPCriticalDirective * omp_critical_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPCriticalDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_critical_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPFlushDirective(clang::OMPFlushDirective * omp_flush_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPFlushDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_flush_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPLoopDirective(clang::OMPLoopDirective * omp_loop_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPLoopDirective" << std::endl;
#endif
    bool res = true;

    return VisitOMPExecutableDirective(omp_loop_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPDistributeDirective(clang::OMPDistributeDirective * omp_distribute_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPDistributeDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPLoopDirective(omp_distribute_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPDistributeParallelForDirective(clang::OMPDistributeParallelForDirective * omp_distribute_parallel_for_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPDistributeParallelForDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_distribute_parallel_for_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPDistributeParallelForSimdDirective(clang::OMPDistributeParallelForSimdDirective * omp_distribute_parallel_for_simd_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPDistributeParallelForSimdDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_distribute_parallel_for_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPDistributeSimdDirective(clang::OMPDistributeSimdDirective * omp_distribute__simd_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPDistributeSimdDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_distribute__simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPForDirective(clang::OMPForDirective * omp_for_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPForDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPLoopDirective(omp_for_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPForSimdDirective(clang::OMPForSimdDirective * omp_for_simd_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPForSimdDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPLoopDirective(omp_for_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPParallelForDirective(clang::OMPParallelForDirective * omp_parallel_for_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPParallelForDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPLoopDirective(omp_parallel_for_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPParallelForSimdDirective(clang::OMPParallelForSimdDirective * omp_parallel_for_simd_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPParallelForSimdDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPLoopDirective(omp_parallel_for_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPSimdDirective(clang::OMPSimdDirective * omp_simd_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPSimdDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPLoopDirective(omp_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTargetParallelForDirective(clang::OMPTargetParallelForDirective * omp_target_parallel_for_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPTargetParallelForDirective" << std::endl;
#endif
    bool res = true;

    return VisitOMPLoopDirective(omp_target_parallel_for_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTargetParallelForSimdDirective(clang::OMPTargetParallelForSimdDirective * omp_target_parallel_for_simd_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPTargetParallelForSimdDirective" << std::endl;
#endif
    bool res = true;

    return VisitOMPLoopDirective(omp_target_parallel_for_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTargetSimdDirective(clang::OMPTargetSimdDirective * omp_target_simd_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPTargetSimdDirective" << std::endl;
#endif
    bool res = true;

    return VisitOMPLoopDirective(omp_target_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTargetTeamsDistributeDirective(clang::OMPTargetTeamsDistributeDirective * omp_target_teams_distribute_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPTargetTeamsDistributeDirective" << std::endl;
#endif
    bool res = true;

    return VisitOMPLoopDirective(omp_target_teams_distribute_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTargetTeamsDistributeSimdDirective(clang::OMPTargetTeamsDistributeSimdDirective * omp_target_teams_distribute_simd_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPTargetTeamsDistributeSimdDirective" << std::endl;
#endif
    bool res = true;

    return VisitOMPLoopDirective(omp_target_teams_distribute_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTaskLoopDirective(clang::OMPTaskLoopDirective * omp_task_loop_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPTaskLoopDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPLoopDirective(omp_task_loop_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTaskLoopSimdDirective(clang::OMPTaskLoopSimdDirective * omp_task_loop_simd_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPTaskLoopSimdDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPLoopDirective(omp_task_loop_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPMasterDirective(clang::OMPMasterDirective * omp_master_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPMasterDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_master_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPOrderedDirective(clang::OMPOrderedDirective * omp_ordered_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPOrderedDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_ordered_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPParallelDirective(clang::OMPParallelDirective * omp_parallel_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPParallelDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_parallel_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPParallelSectionsDirective(clang::OMPParallelSectionsDirective * omp_parallel_sections_directive, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPParallelSectionsDirective" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitOMPExecutableDirective(omp_parallel_sections_directive, node) && res;
}

bool ClangToSageTranslator::VisitReturnStmt(clang::ReturnStmt * return_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitReturnStmt" << std::endl;
#endif

    bool res = true;

    SgNode * tmp_expr = Traverse(return_stmt->getRetValue());
    SgExpression * expr = isSgExpression(tmp_expr);
    if (tmp_expr != NULL && expr == NULL) {
        std::cerr << "Runtime error: tmp_expr != NULL && expr == NULL" << std::endl;
        res = false;
    }
    *node = SageBuilder::buildReturnStmt(expr);

    return VisitStmt(return_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSEHExceptStmt(clang::SEHExceptStmt * seh_except_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitSEHExceptStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitStmt(seh_except_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSEHFinallyStmt(clang::SEHFinallyStmt * seh_finally_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitSEHFinallyStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitStmt(seh_finally_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSEHLeaveStmt(clang::SEHLeaveStmt * seh_leave_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitSEHLeaveStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitStmt(seh_leave_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSEHTryStmt(clang::SEHTryStmt * seh_try_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitSEHTryStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitStmt(seh_try_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSwitchCase(clang::SwitchCase * switch_case, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitSwitchCase" << std::endl;
#endif
    bool res = true;
    
    // TODO

    return VisitStmt(switch_case, node) && res;
}

bool ClangToSageTranslator::VisitCaseStmt(clang::CaseStmt * case_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCaseStmt" << std::endl;
#endif

    SgNode * tmp_stmt = Traverse(case_stmt->getSubStmt());
    SgStatement * stmt = isSgStatement(tmp_stmt);
    SgExpression * expr = isSgExpression(tmp_stmt);
    if (expr != NULL) {
        stmt = SageBuilder::buildExprStatement(expr);
        applySourceRange(stmt, case_stmt->getSubStmt()->getSourceRange());
    }
    ROSE_ASSERT(stmt != NULL);

    SgNode * tmp_lhs = Traverse(case_stmt->getLHS());
    SgExpression * lhs = isSgExpression(tmp_lhs);
    ROSE_ASSERT(lhs != NULL);

/*  FIXME GNU extension not-handled by ROSE
    SgNode * tmp_rhs = Traverse(case_stmt->getRHS());
    SgExpression * rhs = isSgExpression(tmp_rhs);
    ROSE_ASSERT(rhs != NULL);
*/
    ROSE_ASSERT(case_stmt->getRHS() == NULL);

    *node = SageBuilder::buildCaseOptionStmt_nfi(lhs, stmt);

    return VisitSwitchCase(case_stmt, node);
}

bool ClangToSageTranslator::VisitDefaultStmt(clang::DefaultStmt * default_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitDefaultStmt" << std::endl;
#endif

    SgNode * tmp_stmt = Traverse(default_stmt->getSubStmt());
    SgStatement * stmt = isSgStatement(tmp_stmt);
    SgExpression * expr = isSgExpression(tmp_stmt);
    if (expr != NULL) {
        stmt = SageBuilder::buildExprStatement(expr);
        applySourceRange(stmt, default_stmt->getSubStmt()->getSourceRange());
    }
    ROSE_ASSERT(stmt != NULL);

    *node = SageBuilder::buildDefaultOptionStmt_nfi(stmt);

    return VisitSwitchCase(default_stmt, node);
}

bool ClangToSageTranslator::VisitSwitchStmt(clang::SwitchStmt * switch_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitSwitchStmt" << std::endl;
#endif

    SgNode * tmp_cond = Traverse(switch_stmt->getCond());
    SgExpression * cond = isSgExpression(tmp_cond);
    ROSE_ASSERT(cond != NULL);
    
    SgStatement * expr_stmt = SageBuilder::buildExprStatement(cond);
        applySourceRange(expr_stmt, switch_stmt->getCond()->getSourceRange());

    SgSwitchStatement * sg_switch_stmt = SageBuilder::buildSwitchStatement_nfi(expr_stmt, NULL);

    sg_switch_stmt->set_parent(SageBuilder::topScopeStack());

    cond->set_parent(expr_stmt);
    expr_stmt->set_parent(sg_switch_stmt);

    SageBuilder::pushScopeStack(sg_switch_stmt);

    SgNode * tmp_body = Traverse(switch_stmt->getBody());
    SgStatement * body = isSgStatement(tmp_body);
    ROSE_ASSERT(body != NULL);

    SageBuilder::popScopeStack();

    sg_switch_stmt->set_body(body);

    *node = sg_switch_stmt;

    return VisitStmt(switch_stmt, node);
}

bool ClangToSageTranslator::VisitValueStmt(clang::ValueStmt * value_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitValueStmt" << std::endl;
#endif
    bool res = true;

 // DQ (11/28/2020): In test2020_45.c: I think this is the enum field.
 // clang::Expr* expr = value_stmt->getExprStmt();
 // ROSE_ASSERT(expr != NULL);

 // DQ (11/28/2020): Note that value_stmt->getExprStmt() == value_stmt, but not sure why.

 // DQ (11/28/2020): This was previously commented out, and I think there is nothing to do here.
 // The actual implementation was done in VisitFullExp
 // ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitStmt(value_stmt, node) && res;
}

bool ClangToSageTranslator::VisitAttributedStmt(clang::AttributedStmt * attributed_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitAttributedStmt" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_TODO == 0); // TODO

    return VisitValueStmt(attributed_stmt, node) && res;
}

bool ClangToSageTranslator::VisitExpr(clang::Expr * expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitExpr" << std::endl;
#endif

     // TODO Is there anything to be done? (maybe in relation with typing?)

     return VisitValueStmt(expr, node);
}

bool ClangToSageTranslator::VisitAbstractConditionalOperator(clang::AbstractConditionalOperator * abstract_conditional_operator, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitAbstractConditionalOperator" << std::endl;
#endif
     bool res = true;

     // TODO 

     return VisitStmt(abstract_conditional_operator, node) && res;
}

bool ClangToSageTranslator::VisitBinaryConditionalOperator(clang::BinaryConditionalOperator * binary_conditional_operator, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitBinaryConditionalOperator" << std::endl;
#endif
     bool res = true;

     // TODO 

     return VisitStmt(binary_conditional_operator, node) && res;
}

bool ClangToSageTranslator::VisitConditionalOperator(clang::ConditionalOperator * conditional_operator, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitConditionalOperator" << std::endl;
#endif

    bool res = true;

    SgNode * tmp_cond  = Traverse(conditional_operator->getCond());
    SgExpression * cond_expr = isSgExpression(tmp_cond);
    ROSE_ASSERT(cond_expr);
    SgNode * tmp_true  = Traverse(conditional_operator->getTrueExpr());
    SgExpression * true_expr = isSgExpression(tmp_true);
    ROSE_ASSERT(true_expr);
    SgNode * tmp_false = Traverse(conditional_operator->getFalseExpr());
    SgExpression * false_expr = isSgExpression(tmp_false);
    ROSE_ASSERT(false_expr);

    *node = SageBuilder::buildConditionalExp(cond_expr, true_expr, false_expr);

    return VisitAbstractConditionalOperator(conditional_operator, node) && res;
}

bool ClangToSageTranslator::VisitAddrLabelExpr(clang::AddrLabelExpr * addr_label_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitAddrLabelExpr" << std::endl;
#endif
     bool res = true;

     // TODO 

     return VisitExpr(addr_label_expr, node) && res;
}

bool ClangToSageTranslator::VisitArrayInitIndexExpr(clang::ArrayInitIndexExpr * array_init_index_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitArrayInitIndexExpr" << std::endl;
#endif
     bool res = true;

     // TODO 

     return VisitExpr(array_init_index_expr, node) && res;
}

bool ClangToSageTranslator::VisitArrayInitLoopExpr(clang::ArrayInitLoopExpr * array_init_loop_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitArrayInitLoopExpr" << std::endl;
#endif
     bool res = true;

     // TODO 

     return VisitExpr(array_init_loop_expr, node) && res;
}

bool ClangToSageTranslator::VisitArraySubscriptExpr(clang::ArraySubscriptExpr * array_subscript_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitArraySubscriptExpr" << std::endl;
#endif

    bool res = true;

    SgNode * tmp_base = Traverse(array_subscript_expr->getBase());
    SgExpression * base = isSgExpression(tmp_base);
    if (tmp_base != NULL && base == NULL) {
        std::cerr << "Runtime error: tmp_base != NULL && base == NULL" << std::endl;
        res = false;
    }
    if (SgCastExp *cast = isSgCastExp(base)) {
        auto pointerInfo = [](SgType *type) -> std::pair<int, SgType *> {
            int depth = 0;
            SgType *current = type;
            while (current != nullptr) {
                current = current->stripTypedefsAndModifiers();
                SgPointerType *ptrType = isSgPointerType(current);
                if (ptrType == nullptr) {
                    break;
                }
                ++depth;
                current = ptrType->get_base_type();
            }
            return std::make_pair(depth, current != nullptr ? current->stripTypedefsAndModifiers() : nullptr);
        };

        SgType *operandType = cast->get_operand_i()->get_type();
        if (operandType != nullptr) {
            operandType = operandType->stripTypedefsAndModifiers();
            if (SgArrayType *arrayType = isSgArrayType(operandType)) {
                SgType *elementType = arrayType->get_base_type();
                ROSE_ASSERT(elementType != nullptr);
                SgType *targetType = SgPointerType::createType(elementType);
                ROSE_ASSERT(targetType != nullptr);
                if (pointerInfo(cast->get_type()) != pointerInfo(targetType)) {
                    cast->set_type(targetType);
                }
            }
        }
    }

    SgNode * tmp_idx = Traverse(array_subscript_expr->getIdx());
    SgExpression * idx = isSgExpression(tmp_idx);
    if (tmp_idx != NULL && idx == NULL) {
        std::cerr << "Runtime error: tmp_idx != NULL && idx == NULL" << std::endl;
        res = false;
    }

    *node = SageBuilder::buildPntrArrRefExp(base, idx);

    return VisitExpr(array_subscript_expr, node) && res;
}

bool ClangToSageTranslator::VisitArrayTypeTraitExpr(clang::ArrayTypeTraitExpr * array_type_trait_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitArrayTypeTraitExpr" << std::endl;
#endif
     bool res = true;

     // TODO 

     return VisitExpr(array_type_trait_expr, node) && res;
}

bool ClangToSageTranslator::VisitAsTypeExpr(clang::AsTypeExpr * as_type_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitAsTypeExpr" << std::endl;
#endif
     bool res = true;

     // TODO 

     return VisitExpr(as_type_expr, node) && res;
}

bool ClangToSageTranslator::VisitAtomicExpr(clang::AtomicExpr * atomic_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitAtomicExpr" << std::endl;
#endif
     bool res = true;

     // TODO 

     return VisitExpr(atomic_expr, node) && res;
}

bool ClangToSageTranslator::VisitBinaryOperator(clang::BinaryOperator * binary_operator, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitBinaryOperator" << std::endl;
#endif

    bool res = true;

    SgNode * tmp_lhs = Traverse(binary_operator->getLHS());
    SgExpression * lhs = isSgExpression(tmp_lhs);
    if (tmp_lhs != NULL && lhs == NULL) {
        std::cerr << "Runtime error: tmp_lhs != NULL && lhs == NULL" << std::endl;
        res = false;
    }

    SgNode * tmp_rhs = Traverse(binary_operator->getRHS());
    SgExpression * rhs = isSgExpression(tmp_rhs);
    if (tmp_rhs != NULL && rhs == NULL) {
        std::cerr << "Runtime error: tmp_rhs != NULL && rhs == NULL" << std::endl;
        res = false;
    }

    switch (binary_operator->getOpcode()) {
        case clang::BO_PtrMemD:   ROSE_ASSERT(!"clang::BO_PtrMemD:");//*node = SageBuilder::build(lhs, rhs); break;
        case clang::BO_PtrMemI:   ROSE_ASSERT(!"clang::BO_PtrMemI:");//*node = SageBuilder::build(lhs, rhs); break;
        case clang::BO_Mul:       *node = SageBuilder::buildMultiplyOp(lhs, rhs); break;
        case clang::BO_Div:       *node = SageBuilder::buildDivideOp(lhs, rhs); break;
        case clang::BO_Rem:       *node = SageBuilder::buildModOp(lhs, rhs); break;
        case clang::BO_Add:       *node = SageBuilder::buildAddOp(lhs, rhs); break;
        case clang::BO_Sub:       *node = SageBuilder::buildSubtractOp(lhs, rhs); break;
        case clang::BO_Shl:       *node = SageBuilder::buildLshiftOp(lhs, rhs); break;
        case clang::BO_Shr:       *node = SageBuilder::buildRshiftOp(lhs, rhs); break;
        case clang::BO_LT:        *node = SageBuilder::buildLessThanOp(lhs, rhs); break;
        case clang::BO_GT:        *node = SageBuilder::buildGreaterThanOp(lhs, rhs); break;
        case clang::BO_LE:        *node = SageBuilder::buildLessOrEqualOp(lhs, rhs); break;
        case clang::BO_GE:        *node = SageBuilder::buildGreaterOrEqualOp(lhs, rhs); break;
        case clang::BO_EQ:        *node = SageBuilder::buildEqualityOp(lhs, rhs); break;
        case clang::BO_NE:        *node = SageBuilder::buildNotEqualOp(lhs, rhs); break;
        case clang::BO_And:       *node = SageBuilder::buildBitAndOp(lhs, rhs); break;
        case clang::BO_Xor:       *node = SageBuilder::buildBitXorOp(lhs, rhs); break;
        case clang::BO_Or:        *node = SageBuilder::buildBitOrOp(lhs, rhs); break;
        case clang::BO_LAnd:      *node = SageBuilder::buildAndOp(lhs, rhs); break;
        case clang::BO_LOr:       *node = SageBuilder::buildOrOp(lhs, rhs); break;
        case clang::BO_Assign:    *node = SageBuilder::buildAssignOp(lhs, rhs); break;
        case clang::BO_MulAssign: *node = SageBuilder::buildMultAssignOp(lhs, rhs); break;
        case clang::BO_DivAssign: *node = SageBuilder::buildDivAssignOp(lhs, rhs); break;
        case clang::BO_RemAssign: *node = SageBuilder::buildModAssignOp(lhs, rhs); break;
        case clang::BO_AddAssign: *node = SageBuilder::buildPlusAssignOp(lhs, rhs); break;
        case clang::BO_SubAssign: *node = SageBuilder::buildMinusAssignOp(lhs, rhs); break;
        case clang::BO_ShlAssign: *node = SageBuilder::buildLshiftAssignOp(lhs, rhs); break;
        case clang::BO_ShrAssign: *node = SageBuilder::buildRshiftAssignOp(lhs, rhs); break;
        case clang::BO_AndAssign: *node = SageBuilder::buildAndAssignOp(lhs, rhs); break;
        case clang::BO_XorAssign: *node = SageBuilder::buildXorAssignOp(lhs, rhs); break;
        case clang::BO_OrAssign:  *node = SageBuilder::buildIorAssignOp(lhs, rhs); break;
        case clang::BO_Comma:     *node = SageBuilder::buildCommaOpExp(lhs, rhs); break;
        default:
            std::cerr << "Unknown opcode for binary operator: " << binary_operator->getOpcodeStr().str() << std::endl;
            res = false;
    }

    return VisitExpr(binary_operator, node) && res;
}

bool ClangToSageTranslator::VisitCompoundAssignOperator(clang::CompoundAssignOperator * compound_assign_operator, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCompoundAssignOperator" << std::endl;
#endif
     bool res = true;

     // TODO 

     return VisitBinaryOperator(compound_assign_operator, node) && res;
}

bool ClangToSageTranslator::VisitBlockExpr(clang::BlockExpr * block_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitBlockExpr" << std::endl;
#endif
     bool res = true;

     // TODO 

     return VisitExpr(block_expr, node) && res;
}

bool ClangToSageTranslator::VisitCallExpr(clang::CallExpr * call_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCallExpr" << std::endl;
#endif

    bool res = true;

    SgNode * tmp_expr = Traverse(call_expr->getCallee());
    SgExpression * expr = isSgExpression(tmp_expr);
    if (tmp_expr != NULL && expr == NULL) {
        std::cerr << "Runtime error: tmp_expr != NULL && expr == NULLL" << std::endl;
        res = false;
    }

    SgExprListExp * param_list = SageBuilder::buildExprListExp_nfi();
        applySourceRange(param_list, call_expr->getSourceRange());

    clang::CallExpr::arg_iterator it;
    for (it = call_expr->arg_begin(); it != call_expr->arg_end(); ++it) {
        SgNode * tmp_expr = Traverse(*it);
        SgExpression * expr = isSgExpression(tmp_expr);
        if (tmp_expr != NULL && expr == NULL) {
            std::cerr << "Runtime error: tmp_expr != NULL && expr == NULL" << std::endl;
            res = false;
            continue;
        }
        param_list->append_expression(expr);
    }

    *node = SageBuilder::buildFunctionCallExp_nfi(expr, param_list);

    return VisitExpr(call_expr, node) && res;
}

bool ClangToSageTranslator::VisitCUDAKernelCallExpr(clang::CUDAKernelCallExpr * cuda_kernel_call_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCUDAKernelCallExpr" << std::endl;
#endif
     bool res = true;

     // TODO 

     return VisitExpr(cuda_kernel_call_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * cxx_member_call_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXMemberCallExpr" << std::endl;
#endif
     bool res = true;

     // CXXMemberCallExpr represents calls to member functions (e.g., obj.method() or ptr->method())
     // Delegate to CallExpr handler which will handle function call expression generation
     return VisitCallExpr(cxx_member_call_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * cxx_operator_call_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXOperatorCallExpr" << std::endl;
#endif
     bool res = true;

     // C++ overloaded operators (operator+, operator[], etc.) are represented as function calls
     // Delegate to CallExpr handler for proper function call expression generation
     return VisitCallExpr(cxx_operator_call_expr, node) && res;
}

bool ClangToSageTranslator::VisitUserDefinedLiteral(clang::UserDefinedLiteral * user_defined_literal, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitUserDefinedLiteral" << std::endl;
#endif
     bool res = true;

     // TODO 

     return VisitExpr(user_defined_literal, node) && res;
}

bool ClangToSageTranslator::VisitCastExpr(clang::CastExpr * cast_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCastExpr" << std::endl;
#endif
    bool res = true;

    // Process the sub-expression being cast
    SgNode * tmp_expr = Traverse(cast_expr->getSubExpr());
    SgExpression * sg_expr = isSgExpression(tmp_expr);
    ROSE_ASSERT(sg_expr != NULL);

    // Get the target type
    SgType * sg_type = buildTypeFromQualifiedType(cast_expr->getType());

    // Create the cast expression
    *node = SageBuilder::buildCastExp(sg_expr, sg_type);

    return VisitExpr(cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitExplicitCastExpr(clang::ExplicitCastExpr * explicit_cast_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitExplicitCastExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitCastExpr(explicit_cast_expr, node) && res;
}
    
bool ClangToSageTranslator::VisitBuiltinBitCastExpr(clang::BuiltinBitCastExpr * builtin_bit_cast_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitBuiltinBitCastExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExplicitCastExpr(builtin_bit_cast_expr, node) && res;
}
    
bool ClangToSageTranslator::VisitCStyleCastExpr(clang::CStyleCastExpr * c_style_cast, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCStyleCastExpr" << std::endl;
#endif

    bool res = true;

    SgNode * tmp_expr = Traverse(c_style_cast->getSubExpr());
    SgExpression * expr = isSgExpression(tmp_expr);

    ROSE_ASSERT(expr);

    SgType * type = buildTypeFromQualifiedType(c_style_cast->getTypeAsWritten());

    *node = SageBuilder::buildCastExp(expr, type, SgCastExp::e_C_style_cast);

    return VisitExplicitCastExpr(c_style_cast, node) && res;
}
    
bool ClangToSageTranslator::VisitCXXFunctionalCastExpr(clang::CXXFunctionalCastExpr * cxx_functional_cast_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXFunctionalCastExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExplicitCastExpr(cxx_functional_cast_expr, node) && res;
}
    
bool ClangToSageTranslator::VisitCXXNamedCastExpr(clang::CXXNamedCastExpr * cxx_named_cast_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXNamedCastExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExplicitCastExpr(cxx_named_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXConstCastExpr(clang::CXXConstCastExpr * cxx_const_cast_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXConstCastExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitCXXNamedCastExpr(cxx_const_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDynamicCastExpr(clang::CXXDynamicCastExpr * cxx_dynamic_cast_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXDynamicCastExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitCXXNamedCastExpr(cxx_dynamic_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXReinterpretCastExpr(clang::CXXReinterpretCastExpr * cxx_reinterpret_cast_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXReinterpretCastExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitCXXNamedCastExpr(cxx_reinterpret_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXStaticCastExpr(clang::CXXStaticCastExpr * cxx_static_cast_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXStaticCastExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitCXXNamedCastExpr(cxx_static_cast_expr, node) && res;
}


bool ClangToSageTranslator::VisitImplicitCastExpr(clang::ImplicitCastExpr * implicit_cast_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitImplicitCastExpr" << std::endl;
#endif

    SgNode * tmp_expr = Traverse(implicit_cast_expr->getSubExpr());
    SgExpression * expr = isSgExpression(tmp_expr);

    ROSE_ASSERT(expr != NULL);

    // NOTE: Implicit casts are currently passed through as the sub-expression
    // Creating explicit SgCastExp nodes causes file_id mapping issues
    *node = expr;

    return VisitCastExpr(implicit_cast_expr, node);
}

bool ClangToSageTranslator::VisitCharacterLiteral(clang::CharacterLiteral * character_literal, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCharacterLiteral" << std::endl;
#endif

    *node = SageBuilder::buildCharVal(character_literal->getValue());

    return VisitExpr(character_literal, node);
}

bool ClangToSageTranslator::VisitChooseExpr(clang::ChooseExpr * choose_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitChooseExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(choose_expr, node) && res;
}

bool ClangToSageTranslator::VisitCompoundLiteralExpr(clang::CompoundLiteralExpr * compound_literal, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCompoundLiteralExpr" << std::endl;
#endif

    SgNode * tmp_node = Traverse(compound_literal->getInitializer());
    SgExprListExp * expr = isSgExprListExp(tmp_node);
    ROSE_ASSERT(expr != NULL);

    SgType * type = buildTypeFromQualifiedType(compound_literal->getType());
    ROSE_ASSERT(type != NULL);

    SgAggregateInitializer * initializer = SageBuilder::buildAggregateInitializer_nfi(expr,type);

    initializer->set_uses_compound_literal(true);

    SgName name = std::string("compound_literal_") + Rose::StringUtility::numberToString(compound_literal);
    SgInitializedName* iname = SageBuilder::buildInitializedName_nfi(name, type, initializer);

    SgScopeStatement* scope = SageBuilder::topScopeStack();
    iname->set_scope(scope);
    iname->set_parent(scope);

    SgVariableSymbol * vsym = new SgVariableSymbol(iname);
    ROSE_ASSERT(vsym != nullptr);

    scope->insert_symbol(name,vsym);

    *node = SageBuilder::buildCompoundLiteralExp_nfi(vsym);

    return VisitExpr(compound_literal, node);
}

//bool ClangToSageTranslator::VisitConceptSpecializationExpr(clang::ConceptSpecializationExpr * concept_specialization_expr, SgNode ** node) {
//#if DEBUG_VISIT_STMT
//    std::cerr << "ClangToSageTranslator::VisitConceptSpecializationExpr" << std::endl;
//#endif
//    bool res = true;
//
//    // TODO
//
//    return VisitExpr(concept_specialization_expr, node) && res;
//}

bool ClangToSageTranslator::VisitConvertVectorExpr(clang::ConvertVectorExpr * convert_vector_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitConvertVectorExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(convert_vector_expr, node) && res;
}

bool ClangToSageTranslator::VisitCoroutineSuspendExpr(clang::CoroutineSuspendExpr * coroutine_suspend_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCoroutineSuspendExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(coroutine_suspend_expr, node) && res;
}

bool ClangToSageTranslator::VisitCoawaitExpr(clang::CoawaitExpr * coawait_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCoawaitExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitCoroutineSuspendExpr(coawait_expr, node) && res;
}

bool ClangToSageTranslator::VisitCoyieldExpr(clang::CoyieldExpr * coyield_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCoyieldExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitCoroutineSuspendExpr(coyield_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXBindTemporaryExpr(clang::CXXBindTemporaryExpr * cxx_bind_temporary_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXBindTemporaryExpr" << std::endl;
#endif
    bool res = true;

    // ROOT CAUSE FIX: CXXBindTemporaryExpr extends lifetime of temporary object
    // ROSE handles temporaries differently - just traverse the subexpression
    clang::Expr* sub_expr = cxx_bind_temporary_expr->getSubExpr();
    if (sub_expr != NULL) {
        *node = Traverse(sub_expr);
        if (*node == NULL) {
            return false;
        }
    } else {
        return false;
    }

    return VisitExpr(cxx_bind_temporary_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXBoolLiteralExpr(clang::CXXBoolLiteralExpr * cxx_bool_literal_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXBoolLiteralExpr" << std::endl;
#endif
    bool res = true;

    // C++ boolean literals (true/false)
    bool value = cxx_bool_literal_expr->getValue();
    *node = SageBuilder::buildBoolValExp(value);

    return VisitExpr(cxx_bool_literal_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXConstructExpr(clang::CXXConstructExpr * cxx_construct_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXConstructExpr" << std::endl;
#endif
    bool res = true;

    // Get the constructor being called
    clang::CXXConstructorDecl *ctor_decl = cxx_construct_expr->getConstructor();

    if (ctor_decl != nullptr) {
        // Get the type being constructed
        SgType *constructed_type = buildTypeFromQualifiedType(cxx_construct_expr->getType());

        // Build argument list for constructor call
        // Note: Empty argument lists are intentional and valid for default constructors
        // or when all arguments fail traversal (e.g., template-dependent arguments)
        SgExprListExp *args = SageBuilder::buildExprListExp_nfi();

        // Traverse constructor arguments
        for (unsigned i = 0; i < cxx_construct_expr->getNumArgs(); ++i) {
            clang::Expr *arg = cxx_construct_expr->getArg(i);
            if (arg != nullptr) {
                SgNode *sg_arg = Traverse(arg);
                if (SgExpression *sg_expr = isSgExpression(sg_arg)) {
                    args->append_expression(sg_expr);
                }
            }
        }

        // Use SgConstructorInitializer to properly represent constructor calls
        // This ensures the expression has the constructed class type, not void

        // Check if the type satisfies SgConstructorInitializer requirements
        // The assertion requires: isSgTypedefType or isSgClassType or associated_class_unknown==true
        bool class_unknown = false;
        if (constructed_type != nullptr) {
            if (isSgTypedefType(constructed_type) == nullptr && isSgClassType(constructed_type) == nullptr) {
                // Type is neither typedef nor class type, set flag to true
                class_unknown = true;
            }
        } else {
            // No type available, set flag to true
            class_unknown = true;
        }

        SgConstructorInitializer *ctor_init = SageBuilder::buildConstructorInitializer_nfi(
            NULL,  // declaration (filled in later by AST fixup if needed)
            args,
            constructed_type,
            false,  // need_name
            false,  // need_qualifier
            false,  // need_parenthesis_after_name
            class_unknown   // associated_class_unknown
        );

        *node = ctor_init;
    } else {
        // No constructor available, create a null expression
        *node = SageBuilder::buildNullExpression();
    }

    return VisitExpr(cxx_construct_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXTemporaryObjectExpr(clang::CXXTemporaryObjectExpr * cxx_temporary_object_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXTemporaryObjectExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitCXXConstructExpr(cxx_temporary_object_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDefaultArgExpr(clang::CXXDefaultArgExpr * cxx_default_arg_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXDefaultArgExpr" << std::endl;
#endif
    bool res = true;

    // CXXDefaultArgExpr represents use of a default argument in a function call
    // Traverse to the actual default expression
    if (cxx_default_arg_expr->getExpr() != nullptr) {
        *node = Traverse(cxx_default_arg_expr->getExpr());
    } else {
        // No expression available, use null expression as placeholder
        *node = SageBuilder::buildNullExpression();
    }

    return VisitExpr(cxx_default_arg_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDefaultInitExpr(clang::CXXDefaultInitExpr * cxx_default_init_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXDefaultInitExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(cxx_default_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDeleteExpr(clang::CXXDeleteExpr * cxx_delete_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXDeleteExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(cxx_delete_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDependentScopeMemberExpr(clang::CXXDependentScopeMemberExpr * cxx_dependent_scope_member_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXDependentScopeMemberExpr" << std::endl;
#endif
    bool res = true;

    // CXXDependentScopeMemberExpr represents member access on a template-dependent type
    // (e.g., obj.begin(), obj->data())
    // Extract the base expression and member name to create proper member access

    SgExpression* base_expr = NULL;
    if (cxx_dependent_scope_member_expr->getBase() != NULL) {
        // Traverse the base expression
        clang::Expr* base = const_cast<clang::Expr*>(cxx_dependent_scope_member_expr->getBase());
        SgNode* tmp_base = Traverse(base);
        base_expr = isSgExpression(tmp_base);
    }

    // Get the member name
    std::string member_name = cxx_dependent_scope_member_expr->getMember().getAsString();

    if (base_expr != NULL) {
        // Create an arrow or dot expression depending on the operator used
        if (cxx_dependent_scope_member_expr->isArrow()) {
            // Use arrow expression (obj->member)
            *node = SageBuilder::buildArrowExp(base_expr, SageBuilder::buildVarRefExp(member_name));
        } else {
            // Use dot expression (obj.member)
            *node = SageBuilder::buildDotExp(base_expr, SageBuilder::buildVarRefExp(member_name));
        }
    } else {
        // If we can't get the base expression, use a simple variable reference
        *node = SageBuilder::buildVarRefExp(member_name);
    }

    // Set source position
    SgExpression* expr = isSgExpression(*node);
    if (expr != NULL) {
        applySourceRange(expr, cxx_dependent_scope_member_expr->getSourceRange());
    }

    return VisitExpr(cxx_dependent_scope_member_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXFoldExpr(clang::CXXFoldExpr * cxx_fold_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXFoldExpr" << std::endl;
#endif
    bool res = true;

    // CXXFoldExpr represents C++17 fold expressions like (... && args)
    // These are template-dependent, use placeholder for now
    *node = SageBuilder::buildNullExpression();

    return VisitExpr(cxx_fold_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXInheritedCtorInitExpr(clang::CXXInheritedCtorInitExpr * cxx_inherited_ctor_init_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXInheritedCtorInitExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(cxx_inherited_ctor_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXNewExpr(clang::CXXNewExpr * cxx_new_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXNewExpr" << std::endl;
#endif
    bool res = true;

    // ROOT CAUSE FIX: Implement new expression support
    // Get the allocated type
    SgType* allocated_type = buildTypeFromQualifiedType(cxx_new_expr->getAllocatedType());

    // Handle array size if this is array new
    SgExpression* array_size = NULL;
    if (cxx_new_expr->isArray()) {
        if (clang::Expr* size_expr = cxx_new_expr->getArraySize().value_or(nullptr)) {
            SgNode* tmp_size = Traverse(size_expr);
            array_size = isSgExpression(tmp_size);
        }
    }

    // Handle initializer (constructor call)
    SgConstructorInitializer* ctor_init = NULL;
    if (cxx_new_expr->hasInitializer()) {
        clang::Expr* initializer = cxx_new_expr->getInitializer();
        if (initializer != NULL) {
            SgNode* tmp_init = Traverse(initializer);
            // The initializer might be a CXXConstructExpr or other expression
            ctor_init = isSgConstructorInitializer(tmp_init);
        }
    }

    // Build the new expression
    // buildNewExp(type, exprListExp, constInit, expr, val, funcDecl)
    SgNewExp* new_exp = SageBuilder::buildNewExp(
        allocated_type,      // type
        NULL,                // exprListExp (for arrays)
        ctor_init,           // constInit (constructor initializer)
        NULL,                // expr (placement new expression)
        0,                   // val (need_global_specifier as short)
        NULL                 // funcDecl (operator new function)
    );

    *node = new_exp;

    return VisitExpr(cxx_new_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXNoexceptExpr(clang::CXXNoexceptExpr * cxx_noexcept_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXNoexceptExpr" << std::endl;
#endif
    bool res = true;

    // ROOT CAUSE FIX: noexcept operator evaluates at compile-time whether an expression can throw
    // Get the compile-time result and create a bool literal
    bool can_throw = cxx_noexcept_expr->getValue();

    // Build a bool literal expression with the compile-time result
    *node = SageBuilder::buildBoolValExp(can_throw);

    return VisitExpr(cxx_noexcept_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXNullPtrLiteralExpr(clang::CXXNullPtrLiteralExpr * cxx_null_ptr_literal_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXNullPtrLiteralExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(cxx_null_ptr_literal_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXPseudoDestructorExpr(clang::CXXPseudoDestructorExpr * cxx_pseudo_destructor_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXPseudoDestructorExpr" << std::endl;
#endif
    bool res = true;

    // ROOT CAUSE FIX: CXXPseudoDestructorExpr represents a call to a destructor on a non-class type
    // Example: ptr->~T() where T is a primitive type (used in templates)
    // Get the destroyed type
    clang::QualType destroyed_type = cxx_pseudo_destructor_expr->getDestroyedType();
    SgType* sg_type = buildTypeFromQualifiedType(destroyed_type);
    ROSE_ASSERT(sg_type != NULL);

    // Create source location info
    Sg_File_Info* file_info = Sg_File_Info::generateDefaultFileInfoForTransformationNode();
    ROSE_ASSERT(file_info != NULL);

    // Create the pseudo destructor reference expression
    SgPseudoDestructorRefExp* pseudo_dtor = new SgPseudoDestructorRefExp(file_info, sg_type);
    ROSE_ASSERT(pseudo_dtor != NULL);

    // Call post_construction_initialization which sets up the member function type
    pseudo_dtor->post_construction_initialization();

    *node = pseudo_dtor;

    return VisitExpr(cxx_pseudo_destructor_expr, node) && res;
}

//bool ClangToSageTranslator::VisitCXXRewrittenBinaryOperator(clang::CXXRewrittenBinaryOperator * cxx_rewrite_binary_operator, SgNode ** node) {
//#if DEBUG_VISIT_STMT
//    std::cerr << "ClangToSageTranslator::VisitCXXRewrittenBinaryOperator" << std::endl;
//#endif
//    bool res = true;
//
//    // TODO
//
//    return VisitExpr(cxx_rewrite_binary_operator, node) && res;
//}

bool ClangToSageTranslator::VisitCXXScalarValueInitExpr(clang::CXXScalarValueInitExpr * cxx_scalar_value_init_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXScalarValueInitExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(cxx_scalar_value_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXStdInitializerListExpr(clang::CXXStdInitializerListExpr * cxx_std_initializer_list_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXStdInitializerListExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(cxx_std_initializer_list_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXThisExpr(clang::CXXThisExpr * cxx_this_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXThisExpr" << std::endl;
#endif
    bool res = true;

    // CXXThisExpr represents the 'this' pointer in C++ member functions
    // For now, use a placeholder variable reference named "this" since buildThisExp doesn't properly set type
    SgType* this_type = buildTypeFromQualifiedType(cxx_this_expr->getType());
    if (this_type == nullptr) {
        // Fallback to opaque type if we can't determine the type
        this_type = SageBuilder::buildOpaqueType("this_type", getGlobalScope());
    }

    // Create a placeholder "this" variable
    SgInitializedName* this_var = SageBuilder::buildInitializedName("this", this_type);
    this_var->get_file_info()->setCompilerGenerated();
    SgScopeStatement* scope = SageBuilder::topScopeStack();
    this_var->set_scope(scope);
    this_var->set_parent(scope);
    SgVariableSymbol* this_sym = new SgVariableSymbol(this_var);

    *node = SageBuilder::buildVarRefExp(this_sym);

    return VisitExpr(cxx_this_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXThrowExpr(clang::CXXThrowExpr * cxx_throw_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXThrowExpr" << std::endl;
#endif
    bool res = true;

    // ROOT CAUSE FIX: CXXThrowExpr represents C++ throw expressions
    // Can be either "throw expr;" or bare "throw;" (rethrow)
    SgExpression* throw_operand = NULL;
    SgThrowOp::e_throw_kind throw_kind;

    // Check if this is a rethrow (bare "throw;") or throw with expression
    clang::Expr* sub_expr = cxx_throw_expr->getSubExpr();
    if (sub_expr != NULL) {
        // Regular throw with an expression
        SgNode* tmp_expr = Traverse(sub_expr);
        throw_operand = isSgExpression(tmp_expr);
        if (throw_operand == NULL) {
            std::cerr << "Error: Failed to convert throw operand expression" << std::endl;
            return false;
        }
        throw_kind = SgThrowOp::throw_expression;
    } else {
        // Rethrow (bare "throw;")
        throw_kind = SgThrowOp::rethrow;
    }

    // Build the throw operation
    SgThrowOp* throw_op = SageBuilder::buildThrowOp(throw_operand, throw_kind);
    ROSE_ASSERT(throw_op != NULL);

    *node = throw_op;

    return VisitExpr(cxx_throw_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXTypeidExpr(clang::CXXTypeidExpr * cxx_typeid_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXTypeidExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(cxx_typeid_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXUnresolvedConstructExpr(clang::CXXUnresolvedConstructExpr * cxx_unresolved_construct_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXUnresolvedConstructExpr" << std::endl;
#endif
    bool res = true;

    // ROOT CAUSE FIX: Template-dependent constructor calls (e.g., T(args) where T is a template parameter)
    // Build a proper constructor call expression instead of using null placeholder

    // Get the type being constructed (may be a dependent type)
    SgType* type = buildTypeFromQualifiedType(cxx_unresolved_construct_expr->getTypeAsWritten());

    // Build expression list for constructor arguments
    SgExprListExp* args = SageBuilder::buildExprListExp_nfi();
    for (unsigned i = 0; i < cxx_unresolved_construct_expr->getNumArgs(); i++) {
        SgNode* tmp_expr = Traverse(cxx_unresolved_construct_expr->getArg(i));
        SgExpression* arg = isSgExpression(tmp_expr);
        if (arg != NULL) {
            args->append_expression(arg);
        }
    }

    // Build constructor initializer for the unresolved construct
    SgConstructorInitializer* ctor_init = SageBuilder::buildConstructorInitializer_nfi(
        NULL,  // declaration will be NULL for unresolved/dependent constructors
        args,
        type,
        false, // need_name
        false, // need_qualifier
        false, // need_parenthesis_after_name
        true   // associated_class_unknown - set to true for template-dependent types
    );

    *node = ctor_init;

    return VisitExpr(cxx_unresolved_construct_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXUuidofExpr(clang::CXXUuidofExpr * cxx_uuidof_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitCXXUuidofExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(cxx_uuidof_expr, node) && res;
}

bool ClangToSageTranslator::VisitDeclRefExpr(clang::DeclRefExpr * decl_ref_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitDeclRefExpr" << std::endl;
#endif

    bool res = true;

    // SgNode * tmp_node = Traverse(decl_ref_expr->getDecl());
    // DONE: Do not use Traverse(...) as the declaration can not be complete (recursive functions)
    //       Instead use SymbolTable from ROSE as the symbol should be ready (cannot have a reference before the declaration)
    // FIXME: This fix will not work for C++ (methods/fields can be use before they are declared...)
    // FIXME: I feel like it could work now, we will see ....

     SgSymbol * sym = GetSymbolFromSymbolTable(decl_ref_expr->getDecl());

     if (sym == NULL) 
        {
          SgNode * tmp_decl = Traverse(decl_ref_expr->getDecl());

       // DQ (11/29/2020): Added assertion.
          ROSE_ASSERT(tmp_decl != NULL);

#if DEBUG_VISIT_STMT
          printf ("tmp_decl = %p = %s \n",tmp_decl,tmp_decl->class_name().c_str());
#endif
          SgInitializedName* initializedName = isSgInitializedName(tmp_decl);
#if DEBUG_VISIT_STMT
          if (initializedName != NULL)
             {
               printf ("Found SgInitializedName: initializedName->get_name() = %s \n",initializedName->get_name().str());
             }
#endif

          if (tmp_decl != NULL)
             {
               sym = GetSymbolFromSymbolTable(decl_ref_expr->getDecl());
             }

       // FIXME hack Traverse have added the symbol but we cannot find it (probably: problem with type and function lookup)

          if (sym == NULL && isSgFunctionDeclaration(tmp_decl) != NULL)
             {
               sym = new SgFunctionSymbol(isSgFunctionDeclaration(tmp_decl));
               sym->set_parent(tmp_decl);
             }
          // ROOT CAUSE FIX: Handle SgVariableDeclaration from VisitVarDecl
          // Extract the InitializedName and create symbol if needed
          if (sym == NULL && isSgVariableDeclaration(tmp_decl) != NULL)
             {
               SgVariableDeclaration* var_decl_result = isSgVariableDeclaration(tmp_decl);
               if (var_decl_result->get_variables().size() > 0)
                  {
                    SgInitializedName* init_name = var_decl_result->get_variables()[0];
                    if (init_name != NULL)
                       {
                         // Try to get existing symbol first
                         SgScopeStatement* init_scope = init_name->get_scope();
                         if (init_scope != NULL)
                            {
                              sym = init_scope->lookup_variable_symbol(init_name->get_name());
                            }
                         // If still not found, create new symbol
                         if (sym == NULL)
                            {
                              sym = new SgVariableSymbol(init_name);
                              sym->set_parent(init_name);
                              if (init_scope != NULL)
                                 {
                                   init_scope->insert_symbol(init_name->get_name(), sym);
                                 }
                            }
                       }
                  }
             }
          // Pei-Hung (04/07/2022) sym can be NULL in the case for C99 VLA
          if (sym == NULL && isSgInitializedName(tmp_decl) != NULL)
             {
               sym = new SgVariableSymbol(isSgInitializedName(tmp_decl));
               sym->set_parent(tmp_decl);
               SageBuilder::topScopeStack()->insert_symbol(isSgInitializedName(tmp_decl)->get_name(), sym);
             }
        }

     if (sym != NULL) 
        {
       // Not else if it was NULL we have try to traverse it....
          SgVariableSymbol  * var_sym  = isSgVariableSymbol(sym);
          SgFunctionSymbol  * func_sym = isSgFunctionSymbol(sym);
          SgEnumFieldSymbol * enum_sym = isSgEnumFieldSymbol(sym);

          if (var_sym != NULL) 
             {
               *node = SageBuilder::buildVarRefExp(var_sym);
             }
            else 
             {
               if (func_sym != NULL)
                  {
                    *node = SageBuilder::buildFunctionRefExp(func_sym);

                    // ROOT CAUSE FIX: Set qualified name prefix (namespace) from Clang declaration
                    // This preserves namespace information (e.g., std::) even when scope is global
                    SgFunctionRefExp* func_ref = isSgFunctionRefExp(*node);
                    if (func_ref != NULL) {
                        clang::FunctionDecl* func_decl = llvm::dyn_cast<clang::FunctionDecl>(decl_ref_expr->getDecl());
                        if (func_decl != NULL) {
                            std::string qualified_name = func_decl->getQualifiedNameAsString();
                            std::string simple_name = func_decl->getNameAsString();
                            // Extract namespace prefix by removing simple name from qualified name
                            if (qualified_name.length() > simple_name.length() &&
                                qualified_name.substr(qualified_name.length() - simple_name.length()) == simple_name) {
                                // Remove the simple name and the trailing ::
                                std::string namespace_prefix = qualified_name.substr(0, qualified_name.length() - simple_name.length());
                                if (namespace_prefix.length() >= 2 && namespace_prefix.substr(namespace_prefix.length() - 2) == "::") {
                                    namespace_prefix = namespace_prefix.substr(0, namespace_prefix.length() - 2);
                                }
                                if (!namespace_prefix.empty()) {
                                    // Add to global qualified name map so unparser can retrieve it
                                    SgNode::get_globalQualifiedNameMapForNames()[func_ref] = namespace_prefix + "::";
                                }
                            }
                        }
                    }
                  }
                 else
                  {
                    if (enum_sym != NULL)
                       {
                         SgEnumDeclaration * enum_decl = isSgEnumDeclaration(enum_sym->get_declaration()->get_parent());
                         ROSE_ASSERT(enum_decl != NULL);
                         SgName name = enum_sym->get_name();
                         *node = SageBuilder::buildEnumVal_nfi(0, enum_decl, name);
                       }
                      else
                       {
                         if (sym != NULL)
                            {
                              std::cerr << "Runtime error: Unknown type of symbol for a declaration reference." << std::endl;
                              std::cerr << "    sym->class_name() = " << sym->class_name()  << std::endl;
                              ROSE_ABORT();
                            }
                       }
                  }
             }
        }
       else
        {
          // ROOT CAUSE FIX: Handle template-dependent and unresolved declarations
          clang::Decl* clang_decl = decl_ref_expr->getDecl();
          std::string decl_name = "unresolved_symbol";

          // Get declaration name and type info for better handling
          if (clang_decl && clang::isa<clang::NamedDecl>(clang_decl)) {
              clang::NamedDecl* named_decl = clang::cast<clang::NamedDecl>(clang_decl);
              decl_name = named_decl->getNameAsString();

              // Log what type of declaration couldn't be resolved
              std::cerr << "Warning: Cannot resolve symbol for " << clang_decl->getDeclKindName()
                        << " '" << decl_name << "', using placeholder" << std::endl;
          } else {
              std::cerr << "Warning: Cannot resolve symbol for declaration reference, using placeholder" << std::endl;
          }

          // Create a placeholder variable with unknown type
          SgType* unknown_type = SageBuilder::buildOpaqueType(decl_name + "_type", getGlobalScope());
          SgInitializedName* placeholder_var = SageBuilder::buildInitializedName(decl_name, unknown_type);
          placeholder_var->get_file_info()->setCompilerGenerated();
          SgScopeStatement* scope = SageBuilder::topScopeStack();
          placeholder_var->set_scope(scope);
          placeholder_var->set_parent(scope);

          SgVariableSymbol* placeholder_sym = new SgVariableSymbol(placeholder_var);
          *node = SageBuilder::buildVarRefExp(placeholder_sym);
        }

    return VisitExpr(decl_ref_expr, node) && res;
}

bool ClangToSageTranslator::VisitDependentCoawaitExpr(clang::DependentCoawaitExpr * dependent_coawait_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitDependentCoawaitExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(dependent_coawait_expr, node) && res;
}

bool ClangToSageTranslator::VisitDependentScopeDeclRefExpr(clang::DependentScopeDeclRefExpr * dependent_scope_decl_ref_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitDependentScopeDeclRefExpr" << std::endl;
#endif
    bool res = true;

    // DependentScopeDeclRefExpr represents a reference to a declaration that depends on template parameters
    // (e.g., variable references like 'x' or 'y' in template-dependent contexts)
    // Extract the name and create a variable reference expression

    std::string decl_name = dependent_scope_decl_ref_expr->getDeclName().getAsString();

    // Check for qualified names (e.g., namespace::var)
    if (dependent_scope_decl_ref_expr->getQualifier() != NULL) {
        std::string qualifier_str;
        llvm::raw_string_ostream qualifier_stream(qualifier_str);
        dependent_scope_decl_ref_expr->getQualifier()->print(qualifier_stream, clang::PrintingPolicy(clang::LangOptions()));
        decl_name = qualifier_stream.str() + decl_name;
    }

    // Create a variable reference expression
    // NOTE: Using topScopeStack() may not correctly resolve variables in nested scopes
    // since dependent scope information isn't always available at this stage.
    // Ideally, the variable lookup should search upward through parent scopes,
    // but for template-dependent contexts, complete scope information may not be
    // available until instantiation time.
    SgName sg_name(decl_name);
    *node = SageBuilder::buildVarRefExp(sg_name, SageBuilder::topScopeStack());

    // Set source position
    SgExpression* expr = isSgExpression(*node);
    if (expr != NULL) {
        applySourceRange(expr, dependent_scope_decl_ref_expr->getSourceRange());
    }

    return VisitExpr(dependent_scope_decl_ref_expr, node) && res;
}

// bool ClangToSageTranslator::VisitDependentScopeDeclRefExpr(clang::DependentScopeDeclRefExpr * dependent_scope_decl_ref_expr);

bool ClangToSageTranslator::VisitDesignatedInitExpr(clang::DesignatedInitExpr * designated_init_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitDesignatedInitExpr" << std::endl;
#endif

    SgInitializer * base_init = NULL;    
    SgDesignatedInitializer * designated_init = NULL;    
    SgExprListExp * expr_list_exp = NULL; 
    {
        SgNode * tmp_expr = Traverse(designated_init_expr->getInit());
        SgExpression * expr = isSgExpression(tmp_expr);
        ROSE_ASSERT(expr != NULL);
        SgExprListExp * expr_list_exp = isSgExprListExp(expr);
        if (expr_list_exp != NULL) {
            // FIXME get the type right...
            base_init = SageBuilder::buildAggregateInitializer_nfi(expr_list_exp, NULL);
        }
        else {
            base_init = SageBuilder::buildAssignInitializer_nfi(expr, expr->get_type());
        }
        ROSE_ASSERT(base_init != NULL);
        applySourceRange(base_init, designated_init_expr->getInit()->getSourceRange());
    }


/* Pei-Hung (06/10/2022) revision to handle Initializer in test2013_37.c
 *  After calling getSyntacticForm from InitListExpr, the type and multidimensional array hierarchy is missing.
 *  This version can construct the array structure but need additional support to grab the type structure from 
 *  parent AST node, such as VarDecl.
 */

    auto designatorSize = designated_init_expr->size();

    for (auto it=designatorSize; it > 0; it--) {
        expr_list_exp = SageBuilder::buildExprListExp_nfi();
        SgExpression * expr = NULL;
        clang::DesignatedInitExpr::Designator * D = designated_init_expr->getDesignator(it-1);
        if (D->isFieldDesignator()) {
            // In LLVM 20, getField() was renamed to getFieldDecl()
            SgSymbol * symbol = GetSymbolFromSymbolTable(D->getFieldDecl());
            SgVariableSymbol * var_sym = isSgVariableSymbol(symbol);
            ROSE_ASSERT(var_sym != NULL);
            expr = SageBuilder::buildVarRefExp_nfi(var_sym);
        }
        else if (D->isArrayDesignator()) {
            SgNode * tmp_expr = NULL;
            if(clang::ConstantExpr::classof(designated_init_expr->getArrayIndex(*D)))
            {
               clang::FullExpr* fullExpr = (clang::FullExpr*) designated_init_expr->getArrayIndex(*D);
               clang::IntegerLiteral* integerLiteral = (clang::IntegerLiteral*) fullExpr->getSubExpr();
               tmp_expr = SageBuilder::buildUnsignedLongVal((unsigned long) integerLiteral->getValue().getSExtValue());
            }
            else
            {
               tmp_expr = Traverse(designated_init_expr->getArrayIndex(*D));
            }
            expr = isSgExpression(tmp_expr);
            ROSE_ASSERT(expr != NULL);

        }
        else if (D->isArrayRangeDesignator()) {
            ROSE_ASSERT(!"I don't believe range designator initializer are supported by ROSE...");    
        }
        else ROSE_ABORT();

        ROSE_ASSERT(expr != NULL);

        applySourceRange(expr, D->getSourceRange());
        expr->set_parent(expr_list_exp);
        expr_list_exp->append_expression(expr);
        if(it > 1)
        {
            SgDesignatedInitializer * design_init = new SgDesignatedInitializer(expr_list_exp, base_init);
            applySourceRange(design_init, designated_init_expr->getDesignatorsSourceRange());
            expr_list_exp->set_parent(design_init);
            base_init->set_parent(design_init);
            SgExprListExp* aggListExp = SageBuilder::buildExprListExp_nfi();
            design_init->set_parent(aggListExp);
            aggListExp->append_expression(design_init);
            SgAggregateInitializer* newAggInit = SageBuilder::buildAggregateInitializer_nfi(aggListExp, NULL);
            expr_list_exp = SageBuilder::buildExprListExp_nfi(); 
            base_init = newAggInit; 
        }

    }

    applySourceRange(expr_list_exp, designated_init_expr->getDesignatorsSourceRange());
    designated_init = new SgDesignatedInitializer(expr_list_exp, base_init);
    expr_list_exp->set_parent(base_init);
    base_init->set_parent(designated_init);

    *node = designated_init;

    return VisitExpr(designated_init_expr, node);

// Pei-Hung (06/10/2022) keep the original implementation which has the array information stored in the list
/*
    for (auto it=0; it < designatorSize; it++) {
        SgExpression * expr = NULL;
        clang::DesignatedInitExpr::Designator * D = designated_init_expr->getDesignator(it);
        if (D->isFieldDesignator()) {
            SgSymbol * symbol = GetSymbolFromSymbolTable(D->getField());
            SgVariableSymbol * var_sym = isSgVariableSymbol(symbol);
            ROSE_ASSERT(var_sym != NULL);
            expr = SageBuilder::buildVarRefExp_nfi(var_sym);
            applySourceRange(expr, D->getSourceRange());
        }
        else if (D->isArrayDesignator()) {
            SgNode * tmp_expr = NULL;
            if(clang::ConstantExpr::classof(designated_init_expr->getArrayIndex(*D)))
            {
               clang::FullExpr* fullExpr = (clang::FullExpr*) designated_init_expr->getArrayIndex(*D);
               clang::IntegerLiteral* integerLiteral = (clang::IntegerLiteral*) fullExpr->getSubExpr();
               tmp_expr = SageBuilder::buildUnsignedLongVal((unsigned long) integerLiteral->getValue().getSExtValue());
std::cerr << "idx:" << integerLiteral->getValue().getSExtValue() << std::endl;
            }
            else
            {
               tmp_expr = Traverse(designated_init_expr->getArrayIndex(*D));
            }
            expr = isSgExpression(tmp_expr);
            ROSE_ASSERT(expr != NULL);
        }
        else if (D->isArrayRangeDesignator()) {
            ROSE_ASSERT(!"I don't believe range designator initializer are supported by ROSE...");    
        }
        else ROSE_ABORT();

        ROSE_ASSERT(expr != NULL);

        expr->set_parent(expr_list_exp);
        expr_list_exp->append_expression(expr);
    }

    applySourceRange(expr_list_exp, designated_init_expr->getDesignatorsSourceRange());

    SgDesignatedInitializer * design_init = new SgDesignatedInitializer(expr_list_exp, init);
    expr_list_exp->set_parent(design_init);
    init->set_parent(design_init);

    *node = design_init;

    return VisitExpr(designated_init_expr, node);
*/
}

bool ClangToSageTranslator::VisitDesignatedInitUpdateExpr(clang::DesignatedInitUpdateExpr * designated_init_update, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitDesignatedInitUpdateExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(designated_init_update, node) && res;
}

bool ClangToSageTranslator::VisitExpressionTraitExpr(clang::ExpressionTraitExpr * expression_trait_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitExpressionTraitExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(expression_trait_expr, node) && res;
}

bool ClangToSageTranslator::VisitExtVectorElementExpr(clang::ExtVectorElementExpr * ext_vector_element_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitExtVectorElementExpr" << std::endl;
#endif

    SgNode * tmp_base = Traverse(ext_vector_element_expr->getBase());
    SgExpression * base = isSgExpression(tmp_base);

    ROSE_ASSERT(base != NULL);

    SgType * type = buildTypeFromQualifiedType(ext_vector_element_expr->getType());

    clang::IdentifierInfo & ident_info = ext_vector_element_expr->getAccessor();
    std::string ident = ident_info.getName().str();

    SgScopeStatement * scope = SageBuilder::ScopeStack.front();
    SgGlobal * global = isSgGlobal(scope);
    ROSE_ASSERT(global != NULL);

  // Build Manually a SgVarRefExp to have the same Accessor (text version) TODO ExtVectorAccessor and ExtVectorType
    SgInitializedName * init_name = SageBuilder::buildInitializedName(ident, SageBuilder::buildVoidType(), NULL);
    setCompilerGeneratedFileInfo(init_name);
    init_name->set_scope(global);
    SgVariableSymbol * var_symbol = new SgVariableSymbol(init_name);
    SgVarRefExp * pseudo_field = new SgVarRefExp(var_symbol);
    setCompilerGeneratedFileInfo(pseudo_field, true);
    init_name->set_parent(pseudo_field);

    SgExpression * res = NULL;
    if (ext_vector_element_expr->isArrow())
        res = SageBuilder::buildArrowExp(base, pseudo_field);
    else
        res = SageBuilder::buildDotExp(base, pseudo_field);

    ROSE_ASSERT(res != NULL);

    *node = res;

   return VisitExpr(ext_vector_element_expr, node);
}

bool ClangToSageTranslator::VisitFixedPointLiteral(clang::FixedPointLiteral * fixed_point_literal, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitFixedPointLiteral" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(fixed_point_literal, node) && res;
}

bool ClangToSageTranslator::VisitFloatingLiteral(clang::FloatingLiteral * floating_literal, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitFloatingLiteral" << std::endl;
#endif

    unsigned int precision =  llvm::APFloat::semanticsPrecision(floating_literal->getValue().getSemantics());
    if (precision == 24) {
        // 32-bit float
        *node = SageBuilder::buildFloatVal(floating_literal->getValue().convertToFloat());
    } else if (precision == 53) {
        // 64-bit double
        *node = SageBuilder::buildDoubleVal(floating_literal->getValue().convertToDouble());
    } else if (precision == 64 || precision == 113) {
        // 80-bit or 128-bit long double - use double as approximation
        *node = SageBuilder::buildLongDoubleVal(floating_literal->getValue().convertToDouble());
    } else if (precision == 11) {
        // 16-bit half precision - use float
        *node = SageBuilder::buildFloatVal(floating_literal->getValue().convertToFloat());
    } else {
        // Fallback for other sizes - use double
        std::cerr << "Warning: Unsupported float precision " << precision << ", using double" << std::endl;
        *node = SageBuilder::buildDoubleVal(floating_literal->getValue().convertToDouble());
    }

    return VisitExpr(floating_literal, node);
}

bool ClangToSageTranslator::VisitFullExpr(clang::FullExpr * full_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitFullExpr" << std::endl;
#endif
    bool res = true;

    SgNode * tmp_expr = Traverse(full_expr->getSubExpr());
    SgExpression * expr = isSgExpression(tmp_expr);

 // printf ("In VisitFullExpr(): built: expr = %p = %s \n",expr,expr->class_name().c_str());

    *node = expr;

    // TODO

    return VisitExpr(full_expr, node) && res;
}

bool ClangToSageTranslator::VisitConstantExpr(clang::ConstantExpr * constant_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitConstantExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitFullExpr(constant_expr, node) && res;
}

bool ClangToSageTranslator::VisitExprWithCleanups(clang::ExprWithCleanups * expr_with_cleanups, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitExprWithCleanups" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitFullExpr(expr_with_cleanups, node) && res;
}

bool ClangToSageTranslator::VisitFunctionParmPackExpr(clang::FunctionParmPackExpr * function_parm_pack_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitFunctionParmPackExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(function_parm_pack_expr, node) && res;
}

bool ClangToSageTranslator::VisitGenericSelectionExpr(clang::GenericSelectionExpr * generic_Selection_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitGenericSelectionExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(generic_Selection_expr, node) && res;
}

bool ClangToSageTranslator::VisitGNUNullExpr(clang::GNUNullExpr * gnu_null_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitGNUNullExpr" << std::endl;
#endif
    bool res = true;

    // ROOT CAUSE FIX: GNUNullExpr is GNU's __null extension, which represents a null pointer constant
    // It has type long (or long long on 64-bit) but behaves as a null pointer
    // Create an integer literal with value 0, VisitExpr will handle the type
    *node = SageBuilder::buildIntVal(0);

    return VisitExpr(gnu_null_expr, node) && res;
}

bool ClangToSageTranslator::VisitImaginaryLiteral(clang::ImaginaryLiteral * imaginary_literal, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitImaginaryLiteral" << std::endl;
#endif

    SgNode * tmp_imag_val = Traverse(imaginary_literal->getSubExpr());
    SgValueExp * imag_val = isSgValueExp(tmp_imag_val);
    ROSE_ASSERT(imag_val != NULL);

    SgComplexVal * comp_val = new SgComplexVal(NULL, imag_val, imag_val->get_type(), "");

    *node = comp_val;

    return VisitExpr(imaginary_literal, node);
}

bool ClangToSageTranslator::VisitImplicitValueInitExpr(clang::ImplicitValueInitExpr * implicit_value_init_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitImplicitValueInitExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(implicit_value_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitInitListExpr(clang::InitListExpr * init_list_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitInitListExpr" << std::endl;
#endif

    // We use the syntactic version of the initializer if it exists
    if (init_list_expr->getSyntacticForm() != NULL) return VisitInitListExpr(init_list_expr->getSyntacticForm(), node);

    SgExprListExp * expr_list_expr = SageBuilder::buildExprListExp_nfi();

    clang::InitListExpr::iterator it;
    for (it = init_list_expr->begin(); it != init_list_expr->end(); it++) {
        SgNode * tmp_expr = Traverse(*it);
        SgExpression * expr = isSgExpression(tmp_expr);
        ROSE_ASSERT(expr != NULL);

        // Pei-Hung (05/13/2022) the expr can another InitListExpr
        SgExprListExp * child_expr_list_expr = isSgExprListExp(expr);
        SgInitializer * init = NULL;
        if (child_expr_list_expr != NULL)
        {
            SgType * type = expr->get_type();
            init = SageBuilder::buildAggregateInitializer(child_expr_list_expr, type);
        }

        if (init != NULL)
        {
            applySourceRange(init, (*it)->getSourceRange());
            expr_list_expr->append_expression(init);
        }
        else
            expr_list_expr->append_expression(expr);
    }

    *node = expr_list_expr;

    return VisitExpr(init_list_expr, node);
}

bool ClangToSageTranslator::VisitIntegerLiteral(clang::IntegerLiteral * integer_literal, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitIntegerLiteral" << std::endl;
#endif

    *node = SageBuilder::buildIntVal(integer_literal->getValue().getSExtValue());

    return VisitExpr(integer_literal, node);
}

bool ClangToSageTranslator::VisitLambdaExpr(clang::LambdaExpr * lambda_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitLambdaExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(lambda_expr, node) && res;
}

bool ClangToSageTranslator::VisitMaterializeTemporaryExpr(clang::MaterializeTemporaryExpr * materialize_temporary_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitMaterializeTemporaryExpr" << std::endl;
#endif
    bool res = true;

    // MaterializeTemporaryExpr creates a temporary object from a prvalue
    // For now, just traverse the temporary expression itself
    // The temporary materialization is implicit in C++ and doesn't need explicit AST representation in ROSE
    *node = Traverse(materialize_temporary_expr->getSubExpr());

    return VisitExpr(materialize_temporary_expr, node) && res;
}

bool ClangToSageTranslator::VisitMemberExpr(clang::MemberExpr * member_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitMemberExpr" << std::endl;
#endif

    bool res = true;

    SgNode * tmp_base = Traverse(member_expr->getBase());
    SgExpression * base = isSgExpression(tmp_base);
    ROSE_ASSERT(base != NULL);

    SgSymbol * sym = GetSymbolFromSymbolTable(member_expr->getMemberDecl());

    SgVariableSymbol * var_sym  = isSgVariableSymbol(sym);
    SgMemberFunctionSymbol * func_sym = isSgMemberFunctionSymbol(sym);
    SgFunctionSymbol * plain_func_sym = isSgFunctionSymbol(sym); // Regular function symbol (not member)
    SgClassSymbol * class_sym  = isSgClassSymbol(sym);

    SgExpression * sg_member_expr = NULL;

    bool successful_cast = var_sym || func_sym || plain_func_sym || class_sym;
    if (sym != NULL && !successful_cast) {
        std::cerr << "Runtime error: Unknown type of symbol for a member reference." << std::endl;
        std::cerr << "    sym->class_name() = " << sym->class_name()  << std::endl;
        res = false;
    }
    else if (var_sym != NULL) {
        sg_member_expr = SageBuilder::buildVarRefExp(var_sym);
    }
    else if (func_sym != NULL) { // C++ member function
        sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(func_sym, false, false); // FIXME 2nd and 3rd params ?
    }
    else if (plain_func_sym != NULL) { // Regular function treated as member (e.g., static member or inherited)
        sg_member_expr = SageBuilder::buildFunctionRefExp(plain_func_sym);
    }
    else if (class_sym != NULL) {
        SgClassDeclaration* classDecl = class_sym->get_declaration();
        SgClassDeclaration* classDefDecl = isSgClassDeclaration(classDecl->get_definition());
        SgType* classType = classDecl->get_type();
//        if(classDecl->get_isUnNamed())
        {
          SgName varName(generate_name_for_variable(member_expr));
          std::cerr << "build varName:" << varName << std::endl;
          SgVariableDeclaration * var_decl = SageBuilder::buildVariableDeclaration(varName, classType, NULL,SageBuilder::topScopeStack());
          var_decl->set_baseTypeDefiningDeclaration(classDefDecl);
          var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(true);
          var_decl->set_parent(SageBuilder::topScopeStack());

          sg_member_expr = SageBuilder::buildVarRefExp(var_decl);
        }
    }
    else if (sym == NULL) {
        // Symbol not found - try to traverse the member declaration
        SgNode* tmp_member = Traverse(member_expr->getMemberDecl());
#if DEBUG_VISIT_STMT
        if (tmp_member != NULL) {
            std::cerr << "DEBUG VisitMemberExpr: Traversed member, got node type: " << tmp_member->class_name() << std::endl;
        } else {
            std::cerr << "DEBUG VisitMemberExpr: Traverse returned NULL" << std::endl;
        }
#endif
        if (tmp_member != NULL) {
            // Try again to get symbol after traversal
            sym = GetSymbolFromSymbolTable(member_expr->getMemberDecl());
            if (isSgVariableSymbol(sym)) {
                sg_member_expr = SageBuilder::buildVarRefExp(isSgVariableSymbol(sym));
            } else if (isSgMemberFunctionSymbol(sym)) {
                sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(isSgMemberFunctionSymbol(sym), false, false);
            } else if (isSgFunctionSymbol(sym)) {
                // ROOT CAUSE FIX: Handle plain SgFunctionSymbol (not member function symbol)
                // This happens when VisitFunctionDecl creates a regular function declaration
                sg_member_expr = SageBuilder::buildFunctionRefExp(isSgFunctionSymbol(sym));
            } else if (isSgInitializedName(tmp_member)) {
                // Create a temporary symbol if we got an initialized name
                SgVariableSymbol* temp_sym = new SgVariableSymbol(isSgInitializedName(tmp_member));
                sg_member_expr = SageBuilder::buildVarRefExp(temp_sym);
            }
            // ROOT CAUSE FIX: Handle SgMemberFunctionDeclaration from VisitCXXMethodDecl
            else if (sym == NULL && isSgMemberFunctionDeclaration(tmp_member) != NULL) {
                SgMemberFunctionDeclaration* member_func_decl = isSgMemberFunctionDeclaration(tmp_member);
                // Try to find existing symbol in the class scope
                SgScopeStatement* decl_scope = member_func_decl->get_scope();
                if (decl_scope != NULL) {
                    sym = decl_scope->lookup_function_symbol(member_func_decl->get_name());
                }
                // If still not found, create new member function symbol
                if (sym == NULL) {
                    SgMemberFunctionSymbol* new_func_sym = new SgMemberFunctionSymbol(member_func_decl);
                    new_func_sym->set_parent(member_func_decl);
                    if (decl_scope != NULL) {
                        decl_scope->insert_symbol(member_func_decl->get_name(), new_func_sym);
                    }
                    sym = new_func_sym;
                }
                if (isSgMemberFunctionSymbol(sym)) {
                    sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(isSgMemberFunctionSymbol(sym), false, false);
                }
            }
            // Also handle regular function declarations that might be static members
            else if (sym == NULL && isSgFunctionDeclaration(tmp_member) != NULL) {
                SgFunctionDeclaration* func_decl = isSgFunctionDeclaration(tmp_member);
                // Try to find existing symbol
                SgScopeStatement* decl_scope = func_decl->get_scope();
                if (decl_scope != NULL) {
                    sym = decl_scope->lookup_function_symbol(func_decl->get_name());
                }
                // If not found, create new function symbol
                if (sym == NULL) {
                    SgFunctionSymbol* new_func_sym = new SgFunctionSymbol(func_decl);
                    new_func_sym->set_parent(func_decl);
                    if (decl_scope != NULL) {
                        decl_scope->insert_symbol(func_decl->get_name(), new_func_sym);
                    }
                    sym = new_func_sym;
                }
                if (isSgMemberFunctionSymbol(sym)) {
                    sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(isSgMemberFunctionSymbol(sym), false, false);
                } else if (isSgFunctionSymbol(sym)) {
                    sg_member_expr = SageBuilder::buildFunctionRefExp(isSgFunctionSymbol(sym));
                }
            }
        }

        // If still NULL, create a placeholder
        if (sg_member_expr == NULL) {
            std::string member_name = member_expr->getMemberNameInfo().getAsString();
            clang::ValueDecl* member_decl = member_expr->getMemberDecl();
            if (member_decl) {
                std::cerr << "Warning: Cannot resolve " << member_decl->getDeclKindName()
                          << " member '" << member_name << "'";
                if (tmp_member != NULL) {
                    std::cerr << " (traversed to " << tmp_member->class_name() << ")";
                } else {
                    std::cerr << " (traverse returned NULL)";
                }
                std::cerr << ", using placeholder" << std::endl;
            } else {
                std::cerr << "Warning: Cannot resolve member '" << member_name << "', using placeholder" << std::endl;
            }
            SgType* unknown_type = SageBuilder::buildOpaqueType(member_name + "_type", getGlobalScope());
            SgInitializedName* placeholder_var = SageBuilder::buildInitializedName(member_name, unknown_type);
            placeholder_var->get_file_info()->setCompilerGenerated();
            SgScopeStatement* scope = SageBuilder::topScopeStack();
            placeholder_var->set_scope(scope);
            placeholder_var->set_parent(scope);
            SgVariableSymbol* placeholder_sym = new SgVariableSymbol(placeholder_var);
            sg_member_expr = SageBuilder::buildVarRefExp(placeholder_sym);
        }
    }

    ROSE_ASSERT(sg_member_expr != NULL);

    // TODO (C++) member_expr->getQualifier() : for 'a->Base::foo'

    if (member_expr->isArrow())
        *node = SageBuilder::buildArrowExp(base, sg_member_expr);
    else
        *node = SageBuilder::buildDotExp(base, sg_member_expr);

    return VisitExpr(member_expr, node) && res;
}

bool ClangToSageTranslator::VisitMSPropertyRefExpr(clang::MSPropertyRefExpr * ms_property_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitMSPropertyRefExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(ms_property_expr, node) && res;
}

bool ClangToSageTranslator::VisitMSPropertySubscriptExpr(clang::MSPropertySubscriptExpr * ms_property_subscript_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitMSPropertySubscriptExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(ms_property_subscript_expr, node) && res;
}

bool ClangToSageTranslator::VisitNoInitExpr(clang::NoInitExpr * no_init_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitNoInitExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(no_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitOffsetOfExpr(clang::OffsetOfExpr * offset_of_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOffsetOfExpr" << std::endl;
#endif
    bool res = true;

    SgNodePtrList nodePtrList;

    SgType* type = buildTypeFromQualifiedType(offset_of_expr->getTypeSourceInfo()->getType());

    nodePtrList.push_back(type);

    SgExpression* topExp = nullptr;
 
    for (unsigned i = 0, n = offset_of_expr->getNumComponents(); i < n; ++i) {
        clang::OffsetOfNode ON = offset_of_expr->getComponent(i);
          
        switch(ON.getKind()) {
           case clang::OffsetOfNode::Array: {
               // Array node
               SgExpression* arrayIdx = isSgExpression(Traverse(offset_of_expr->getIndexExpr(ON.getArrayExprIndex())));
               SgPntrArrRefExp* pntrArrRefExp = SageBuilder::buildPntrArrRefExp(topExp,arrayIdx);
               topExp = isSgExpression(pntrArrRefExp);
               break;
           }
           case clang::OffsetOfNode::Field:{
               // OffsetOfNode still uses getField(), not getFieldDecl()
               SgNode* fieldNode = Traverse(ON.getField());
               SgName fieldName(ON.getFieldName()->getName().str());
               SgVarRefExp* varExp = SageBuilder::buildVarRefExp(fieldName);
               if(topExp == nullptr)
               {
                 topExp = isSgExpression(varExp);
               }
               else
               {
                 SgDotExp* dotExp = SageBuilder::buildDotExp(topExp, varExp);
                 topExp = isSgExpression(dotExp);
               }
               break;
           }
           // TODO
           case clang::OffsetOfNode::Identifier:{
               SgName fieldName(ON.getFieldName()->getName().str());
               SgVarRefExp* varExp = SageBuilder::buildVarRefExp(fieldName);
               break;
           }
           // TODO
           case clang::OffsetOfNode::Base:
               break;
        }
    }
    nodePtrList.push_back(topExp);

    SgTypeTraitBuiltinOperator* typeTraitBuiltinOperator = SageBuilder::buildTypeTraitBuiltinOperator("__builtin_offsetof", nodePtrList);

    *node = typeTraitBuiltinOperator;

    return VisitExpr(offset_of_expr, node) && res;
}

bool ClangToSageTranslator::VisitOMPArraySectionExpr(clang::ArraySectionExpr * omp_array_section_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOMPArraySectionExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(omp_array_section_expr, node) && res;
}

bool ClangToSageTranslator::VisitOpaqueValueExpr(clang::OpaqueValueExpr * opaque_value_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOpaqueValueExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(opaque_value_expr, node) && res;
}

bool ClangToSageTranslator::VisitOverloadExpr(clang::OverloadExpr * overload_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitOverloadExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(overload_expr, node) && res;
}

bool ClangToSageTranslator::VisitUnresolvedLookupExpr(clang::UnresolvedLookupExpr * unresolved_lookup_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitUnresolvedLookupExpr" << std::endl;
#endif
    bool res = true;

    // UnresolvedLookupExpr represents a reference to a name that couldn't be resolved during parsing
    // (e.g., template-dependent function names like std::iota)
    // Extract the name and create a variable reference expression as an approximation

    std::string function_name;
    if (unresolved_lookup_expr->hasExplicitTemplateArgs()) {
        // Template function with explicit template arguments
        function_name = unresolved_lookup_expr->getName().getAsString();
    } else {
        // Regular function name
        function_name = unresolved_lookup_expr->getName().getAsString();
    }

    // Check for qualified names (e.g., std::iota)
    if (unresolved_lookup_expr->getQualifier() != NULL) {
        std::string qualifier_str;
        llvm::raw_string_ostream qualifier_stream(qualifier_str);
        unresolved_lookup_expr->getQualifier()->print(qualifier_stream, clang::PrintingPolicy(clang::LangOptions()));
        function_name = qualifier_stream.str() + function_name;
    }

    // Create a variable reference expression with the function name
    // This will unparse as the function name, which is what we want
    SgName sg_name(function_name);
    *node = SageBuilder::buildVarRefExp(sg_name, SageBuilder::topScopeStack());

    // Set source position
    SgExpression* expr = isSgExpression(*node);
    if (expr != NULL) {
        applySourceRange(expr, unresolved_lookup_expr->getSourceRange());
    }

    return VisitOverloadExpr(unresolved_lookup_expr, node) && res;
}

bool ClangToSageTranslator::VisitUnresolvedMemberExpr(clang::UnresolvedMemberExpr * unresolved_member_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitUnresolvedMemberExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitOverloadExpr(unresolved_member_expr, node) && res;
}

bool ClangToSageTranslator::VisitPackExpansionExpr(clang::PackExpansionExpr * pack_expansion_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitPackExpansionExpr" << std::endl;
#endif
    bool res = true;

    // ROOT CAUSE FIX: Pack expansion expressions (e.g., f(args...) where args is a pack)
    // Traverse the pattern expression (the expression before the ...)
    clang::Expr* pattern = pack_expansion_expr->getPattern();
    if (pattern != NULL) {
        SgNode* tmp_node = Traverse(pattern);
        SgExpression* pattern_expr = isSgExpression(tmp_node);
        if (pattern_expr != NULL) {
            *node = pattern_expr;
            return VisitExpr(pack_expansion_expr, node) && res;
        }
    }

    // Fallback if pattern can't be traversed
    *node = SageBuilder::buildNullExpression();

    return VisitExpr(pack_expansion_expr, node) && res;
}

bool ClangToSageTranslator::VisitParenExpr(clang::ParenExpr * paren_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitParenExpr" << std::endl;
#endif

    bool res = true;

    SgNode * tmp_subexpr = Traverse(paren_expr->getSubExpr());
    SgExpression * subexpr = isSgExpression(tmp_subexpr);
    if (tmp_subexpr != NULL && subexpr == NULL) {
        std::cerr << "Runtime error: tmp_subexpr != NULL && subexpr == NULL" << std::endl;
        res = false;
    }

    // bypass ParenExpr, their is nothing equivalent in SageIII
    *node = subexpr;

    return VisitExpr(paren_expr, node) && res;
}

bool ClangToSageTranslator::VisitParenListExpr(clang::ParenListExpr * paran_list_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitParenListExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(paran_list_expr, node) && res;
}

bool ClangToSageTranslator::VisitPredefinedExpr(clang::PredefinedExpr * predefined_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitPredefinedExpr" << std::endl;
#endif

    // FIXME It's get tricky here: PredefinedExpr represent compiler generateed variables
    //    I choose to attach those variables on demand in the function definition scope 

  // Traverse the scope's stack to find the last function definition:

    SgFunctionDefinition * func_def = NULL;
    std::list<SgScopeStatement *>::reverse_iterator it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && func_def == NULL) {
        func_def = isSgFunctionDefinition(*it);
        it++;
    }
    ROSE_ASSERT(func_def != NULL);

  // Determine the name of the variable

    SgName name;

 // (01/29/2020) Pei-Hung: change to getIndentKind.  And this list is incomplete for Clang 9
    // In LLVM 20, enum is PredefinedIdentKind with values Func, Function, etc.
    switch (predefined_expr->getIdentKind()) {
        case clang::PredefinedIdentKind::Func:
        case clang::PredefinedIdentKind::FuncDName:
        case clang::PredefinedIdentKind::FuncSig:
        case clang::PredefinedIdentKind::LFuncSig:
            name = "__func__";
            break;
        case clang::PredefinedIdentKind::Function:
        case clang::PredefinedIdentKind::LFunction:
            name = "__FUNCTION__";
            break;
        case clang::PredefinedIdentKind::PrettyFunction:
        case clang::PredefinedIdentKind::PrettyFunctionNoVirtual:
            name = "__PRETTY_FUNCTION__";
            break;
        default:
            name = "__func__";
            break;
    }

  // Retrieve the associate symbol if it exists

    SgVariableSymbol * symbol = func_def->lookup_variable_symbol(name);

  // Else, build a compiler generated initialized name for this variable in the function defintion scope.

    if (symbol == NULL) {
        SgInitializedName * init_name = SageBuilder::buildInitializedName_nfi(name, SageBuilder::buildPointerType(SageBuilder::buildCharType()), NULL);

        init_name->set_parent(func_def);
        init_name->set_scope(func_def);

        Sg_File_Info * start_fi = Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
        start_fi->setCompilerGenerated();
        init_name->set_startOfConstruct(start_fi);

        Sg_File_Info * end_fi   = Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
        end_fi->setCompilerGenerated();
        init_name->set_endOfConstruct(end_fi);

        symbol = new SgVariableSymbol(init_name);

        func_def->insert_symbol(name, symbol);
    }
    ROSE_ASSERT(symbol != NULL);

  // Finally build the variable reference

    *node = SageBuilder::buildVarRefExp_nfi(symbol);

    return VisitExpr(predefined_expr, node);
}

bool ClangToSageTranslator::VisitPseudoObjectExpr(clang::PseudoObjectExpr * pseudo_object_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitPseudoObjectExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(pseudo_object_expr, node) && res;
}

bool ClangToSageTranslator::VisitShuffleVectorExpr(clang::ShuffleVectorExpr * shuffle_vector_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitShuffleVectorExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(shuffle_vector_expr, node) && res;
}

bool ClangToSageTranslator::VisitSizeOfPackExpr(clang::SizeOfPackExpr * size_of_pack_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitSizeOfPackExpr" << std::endl;
#endif
    bool res = true;

    // ROOT CAUSE FIX: sizeof...(Args) returns the compile-time count of pack elements
    // However, for template-dependent packs, the size isn't known until instantiation

    if (!size_of_pack_expr->isValueDependent()) {
        // Non-dependent: get the pack length and create an integer literal
        unsigned pack_length = size_of_pack_expr->getPackLength();
        *node = SageBuilder::buildUnsignedIntVal(pack_length);
    } else {
        // Value-dependent: create an opaque expression placeholder
        // The actual size will be determined at template instantiation time
        *node = SageBuilder::buildOpaqueVarRefExp("__sizeof_pack_dependent", getGlobalScope());
    }

    return VisitExpr(size_of_pack_expr, node) && res;
}

bool ClangToSageTranslator::VisitSourceLocExpr(clang::SourceLocExpr * source_loc_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitSourceLocExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(source_loc_expr, node) && res;
}

bool ClangToSageTranslator::VisitStmtExpr(clang::StmtExpr * stmt_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitStmtExpr" << std::endl;
#endif

    bool res = true;

    SgNode * tmp_substmt = Traverse(stmt_expr->getSubStmt());
    SgStatement * substmt = isSgStatement(tmp_substmt);
    if (tmp_substmt != NULL && substmt == NULL) {
        std::cerr << "Runtime error: tmp_substmt != NULL && substmt == NULL" << std::endl;
        res = false;
    }

    *node = new SgStatementExpression(substmt);

    return VisitExpr(stmt_expr, node) && res;
}

bool ClangToSageTranslator::VisitStringLiteral(clang::StringLiteral * string_literal, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitStringLiteral" << std::endl;
#endif

    std::string tmp = string_literal->getString().str();
    const char * raw_str = tmp.c_str();

    unsigned i = 0;
    unsigned l = 0;
    while (raw_str[i] != '\0') {
        if (
            raw_str[i] == '\\' ||
            raw_str[i] == '\n' ||
            raw_str[i] == '\r' ||
            raw_str[i] == '"')
        {
            l++;
        }
        l++;
        i++;
    }
    l++;

    char * str = (char *)malloc(l * sizeof(char));
    i = 0;
    unsigned cnt = 0;

    while (raw_str[i] != '\0') {
        switch (raw_str[i]) {
            case '\\':
                str[cnt++] = '\\';
                str[cnt++] = '\\';
                break;
            case '\n':
                str[cnt++] = '\\';
                str[cnt++] = 'n';
                break;
            case '\r':
                str[cnt++] = '\\';
                str[cnt++] = 'r';
                break;
            case '"':
                str[cnt++] = '\\';
                str[cnt++] = '"';
                break;
            default:
                str[cnt++] = raw_str[i];
        }
        i++;
    }
    str[cnt] = '\0';

    ROSE_ASSERT(l==cnt+1);

    *node = SageBuilder::buildStringVal(str);

    return VisitExpr(string_literal, node);
}

bool ClangToSageTranslator::VisitSubstNonTypeTemplateParmExpr(clang::SubstNonTypeTemplateParmExpr * subst_non_type_template_parm_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitSubstNonTypeTemplateParmExpr" << std::endl;
#endif
    bool res = true;

    // SubstNonTypeTemplateParmExpr represents a non-type template parameter that has been
    // substituted with its actual value (e.g., N in array<T,N> being replaced with 1024)
    // Traverse to the replacement expression
    *node = Traverse(subst_non_type_template_parm_expr->getReplacement());

    return VisitExpr(subst_non_type_template_parm_expr, node) && res;
}

bool ClangToSageTranslator::VisitSubstNonTypeTemplateParmPackExpr(clang::SubstNonTypeTemplateParmPackExpr * subst_non_type_template_parm_pack_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitSubstNonTypeTemplateParmPackExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(subst_non_type_template_parm_pack_expr, node) && res;
}

bool ClangToSageTranslator::VisitTypeTraitExpr(clang::TypeTraitExpr * type_trait, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitTypeTraitExpr" << std::endl;
#endif
    bool res = true;

    // ROOT CAUSE FIX: Type traits (std::is_integral, std::is_same, etc.) evaluate at compile-time
    // However, template-dependent type traits cannot be evaluated until instantiation

    if (!type_trait->isValueDependent()) {
        // Non-dependent: get the compile-time result and create a bool literal
        bool trait_value = type_trait->getValue();
        *node = SageBuilder::buildBoolValExp(trait_value);
    } else {
        // Value-dependent (template parameter dependent): create an opaque type expression
        // The actual value will be determined at template instantiation time
        *node = SageBuilder::buildOpaqueVarRefExp("__type_trait_dependent", getGlobalScope());
    }

    return VisitExpr(type_trait, node) && res;
}


// TypoExpr was removed in LLVM 20
/*
bool ClangToSageTranslator::VisitTypoExpr(clang::TypoExpr * typo_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitTypoExpr" << std::endl;
#endif
    bool res = true;

    // TODO

    return VisitExpr(typo_expr, node) && res;
}
*/

bool ClangToSageTranslator::VisitUnaryExprOrTypeTraitExpr(clang::UnaryExprOrTypeTraitExpr * unary_expr_or_type_trait_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitUnaryExprOrTypeTraitExpr" << std::endl;
#endif

    bool res = true;

    SgExpression * expr = NULL;
    SgType * type = NULL;

    if (unary_expr_or_type_trait_expr->isArgumentType()) {
        type = buildTypeFromQualifiedType(unary_expr_or_type_trait_expr->getArgumentType());
    }
    else {
        SgNode * tmp_expr = Traverse(unary_expr_or_type_trait_expr->getArgumentExpr());
        expr = isSgExpression(tmp_expr);

        if (tmp_expr != NULL && expr == NULL) {
            std::cerr << "Runtime error: tmp_expr != NULL && expr == NULL" << std::endl;
            res = false;
        }
    }

    switch (unary_expr_or_type_trait_expr->getKind()) {
        case clang::UETT_SizeOf:
            if (type != NULL) 
            {
               std::map<SgClassType *, bool>::iterator bool_it = p_class_type_decl_first_see_in_type.find(isSgClassType(type));
               SgSizeOfOp* sizeofOp = SageBuilder::buildSizeOfOp_nfi(type);

               //Pei-Hung (08/16/22): try to follow VisitTypedefDecl to check if the classType is first seen 

               clang::QualType argumentQualType = unary_expr_or_type_trait_expr->getArgumentType();
               const clang::Type* argumentType = argumentQualType.getTypePtr();
               bool isembedded = false;
               bool iscompleteDefined = false;

               while((isa<clang::ElaboratedType>(argumentType)) || (isa<clang::PointerType>(argumentType)) || (isa<clang::ArrayType>(argumentType)))
               {
                  if(isa<clang::ElaboratedType>(argumentType))
                  {
                    argumentQualType = ((clang::ElaboratedType *)argumentType)->getNamedType();
                  }
                  else if(isa<clang::PointerType>(argumentType))
                  {
                    argumentQualType = ((clang::PointerType *)argumentType)->getPointeeType();
                  }
                  else if(isa<clang::ArrayType>(argumentType))
                  {
                    argumentQualType = ((clang::ArrayType *)argumentType)->getElementType();
                  }
                  argumentType = argumentQualType.getTypePtr();
               }

               if(isa<clang::RecordType>(argumentType))
               {
                  clang::RecordType* argumentRecordType = (clang::RecordType*)argumentType;
                  clang::RecordDecl* recordDeclaration = argumentRecordType->getDecl();
                  isembedded = recordDeclaration->isEmbeddedInDeclarator();
                  iscompleteDefined = recordDeclaration->isCompleteDefinition();
               }

               if (isSgClassType(type) && iscompleteDefined) {
                   std::map<SgClassType *, bool>::iterator bool_it = p_class_type_decl_first_see_in_type.find(isSgClassType(type));
                   ROSE_ASSERT(bool_it != p_class_type_decl_first_see_in_type.end());
                   if (bool_it->second) {
                       // Pei-Hung (08/16/22) If it is first seen, the definition should be unparsed in sizeofOp
                       sizeofOp->set_sizeOfContainsBaseTypeDefiningDeclaration(true);
                       bool_it->second = false;
                   }
                 
               }

               *node = sizeofOp;
            }
            else if (expr != NULL) *node = SageBuilder::buildSizeOfOp_nfi(expr);
            else res = false;
            break;
        case clang::UETT_AlignOf:
        case clang::UETT_PreferredAlignOf:
            if (type != NULL) {
              *node = SageBuilder::buildSizeOfOp_nfi(type);
              ROSE_ASSERT(FAIL_FIXME == 0); // difference between AlignOf and PreferredAlignOf is not represented in ROSE
            }
            else if (expr != NULL) *node = SageBuilder::buildSizeOfOp_nfi(expr);
            else res = false;
            break;
        case clang::UETT_VecStep:
            ROSE_ASSERT(!"OpenCL - VecStep is not supported!");
        default:
            ROSE_ASSERT(!"Unknown clang::UETT_xx");
    }

    return VisitStmt(unary_expr_or_type_trait_expr, node) && res;
}

bool ClangToSageTranslator::VisitUnaryOperator(clang::UnaryOperator * unary_operator, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitUnaryOperator" << std::endl;
#endif

    bool res = true;

    SgNode * tmp_subexpr = Traverse(unary_operator->getSubExpr());
    SgExpression * subexpr = isSgExpression(tmp_subexpr);
    if (tmp_subexpr != NULL && subexpr == NULL) {
        std::cerr << "Runtime error: tmp_subexpr != NULL && subexpr == NULL" << std::endl;
        res = false;
    }

    switch (unary_operator->getOpcode()) {
        case clang::UO_PostInc:
            *node = SageBuilder::buildPlusPlusOp(subexpr, SgUnaryOp::postfix);
            break;
        case clang::UO_PostDec:
            *node = SageBuilder::buildMinusMinusOp(subexpr, SgUnaryOp::postfix);
            break;
        case clang::UO_PreInc:
            *node = SageBuilder::buildPlusPlusOp(subexpr, SgUnaryOp::prefix);
            break;
        case clang::UO_PreDec:
            *node = SageBuilder::buildMinusMinusOp(subexpr, SgUnaryOp::prefix);
            break;
        case clang::UO_AddrOf:
            *node = SageBuilder::buildAddressOfOp(subexpr);
            break;
        case clang::UO_Deref:
            *node = SageBuilder::buildPointerDerefExp(subexpr);
            break;
        case clang::UO_Plus:
            *node = SageBuilder::buildUnaryAddOp(subexpr);
            break;
        case clang::UO_Minus:
            *node = SageBuilder::buildMinusOp(subexpr);
            break;
        // Def. in Clang: UNARY_OPERATION(Not, "~")
        case clang::UO_Not:
            *node = SageBuilder::buildBitComplementOp(subexpr);
            break;
        // Def. in UNARY_OPERATION(LNot, "!")
        case clang::UO_LNot:
            *node = SageBuilder::buildNotOp(subexpr);
            break;
        case clang::UO_Real:
            *node = SageBuilder::buildImagPartOp(subexpr);
            break;
        case clang::UO_Imag:
            *node = SageBuilder::buildRealPartOp(subexpr);
            break;
        case clang::UO_Extension:
            *node = subexpr;
            break;
        default:
            std::cerr << "Runtime error: Unknown unary operator." << std::endl;
            res = false;
    }

    return VisitExpr(unary_operator, node) && res;
}

bool ClangToSageTranslator::VisitVAArgExpr(clang::VAArgExpr * va_arg_expr, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitVAArgExpr" << std::endl;
#endif

    SgNode * tmp_expr = Traverse(va_arg_expr->getSubExpr());
    SgExpression * expr = isSgExpression(tmp_expr);
    ROSE_ASSERT(expr != NULL);

    SgType* type = buildTypeFromQualifiedType(va_arg_expr->getWrittenTypeInfo()->getType());
    ROSE_ASSERT(type != NULL);

    *node = SageBuilder::buildVarArgOp_nfi(expr, type);

    return VisitExpr(va_arg_expr, node);
}
bool ClangToSageTranslator::VisitLabelStmt(clang::LabelStmt * label_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitLabelStmt" << std::endl;
#endif

    bool res = true;

    SgName name(label_stmt->getName());

    *node = SageBuilder::buildLabelStatement_nfi(name, NULL, SageBuilder::topScopeStack());
    SgLabelStatement * sg_label_stmt = isSgLabelStatement(*node);

    SgFunctionDefinition * label_scope = NULL;
    std::list<SgScopeStatement *>::reverse_iterator it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && label_scope == NULL) {
        label_scope = isSgFunctionDefinition(*it);
        it++;
    }
    if (label_scope == NULL) {
         std::cerr << "Runtime error: Cannot find a surrounding function definition for the label statement: \"" << name << "\"." << std::endl;
         res = false;
    }
    else {
        sg_label_stmt->set_scope(label_scope);
        SgLabelSymbol* label_sym = new SgLabelSymbol(sg_label_stmt);
        label_scope->insert_symbol(label_sym->get_name(), label_sym);
    }

    SgNode * tmp_sub_stmt = Traverse(label_stmt->getSubStmt());
    SgStatement * sg_sub_stmt = isSgStatement(tmp_sub_stmt);
    if (sg_sub_stmt == NULL) {
        SgExpression * sg_sub_expr = isSgExpression(tmp_sub_stmt);
        ROSE_ASSERT(sg_sub_expr != NULL);
        sg_sub_stmt = SageBuilder::buildExprStatement(sg_sub_expr);
    }

    ROSE_ASSERT(sg_sub_stmt != NULL);

    sg_sub_stmt->set_parent(sg_label_stmt);
    sg_label_stmt->set_statement(sg_sub_stmt);

    return VisitStmt(label_stmt, node) && res;
}

bool ClangToSageTranslator::VisitWhileStmt(clang::WhileStmt * while_stmt, SgNode ** node) {
#if DEBUG_VISIT_STMT
    std::cerr << "ClangToSageTranslator::VisitWhileStmt" << std::endl;
#endif

    SgNode * tmp_cond = Traverse(while_stmt->getCond());
    SgExpression * cond = isSgExpression(tmp_cond);
    ROSE_ASSERT(cond != NULL);

    SgStatement * expr_stmt = SageBuilder::buildExprStatement(cond);

    SgWhileStmt * sg_while_stmt = SageBuilder::buildWhileStmt_nfi(expr_stmt, NULL);

    cond->set_parent(expr_stmt);
    expr_stmt->set_parent(sg_while_stmt);

    SageBuilder::pushScopeStack(sg_while_stmt);

    SgNode * tmp_body = Traverse(while_stmt->getBody());
    SgStatement * body = isSgStatement(tmp_body);
    SgExpression * expr = isSgExpression(tmp_body);
    if (expr != NULL) {
        body =  SageBuilder::buildExprStatement(expr);
        applySourceRange(body, while_stmt->getBody()->getSourceRange());
    }
    ROSE_ASSERT(body != NULL);

    body->set_parent(sg_while_stmt);

    SageBuilder::popScopeStack();

    sg_while_stmt->set_body(body);

    *node = sg_while_stmt;

    return VisitStmt(while_stmt, node);
}
