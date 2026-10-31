#include "AstNodeClass.h"

#include "ROSETTA_macros.h"

#include "featureTests.h"

#include "grammar.h"

void Grammar::setUpNodes() {
  AstNodeClass &Expression = *lookupTerminal(terminalList, "Expression");
  AstNodeClass &Statement = *lookupTerminal(terminalList, "Statement");

  // DQ (11/21/2007): This is part of support for the common block statement
  NEW_TERMINAL_MACRO(CommonBlockObject, "CommonBlockObject",
                     "TEMP_CommonBlockObject");

  NEW_TERMINAL_MACRO(InitializedName, "InitializedName", "InitializedNameTag");

  // DQ (9/3/2014): Adding support for C++11 lambda expresions.
  NEW_TERMINAL_MACRO(LambdaCapture, "LambdaCapture", "LambdaCaptureTag");
  NEW_TERMINAL_MACRO(LambdaCaptureList, "LambdaCaptureList",
                     "LambdaCaptureListTag");
  NEW_TERMINAL_MACRO(DeclarationScopeList, "DeclarationScopeList",
                     "DeclarationScopeListTag");
  NEW_TERMINAL_MACRO(AuxiliaryDeclarationList, "AuxiliaryDeclarationList",
                     "AuxiliaryDeclarationListTag");
  NEW_TERMINAL_MACRO(OmpClauseList, "OmpClauseList", "OmpClauseListTag");
  NEW_TERMINAL_MACRO(StatementAttribute, "StatementAttribute",
                     "T_STATEMENT_ATTRIBUTE");
  NEW_TERMINAL_MACRO(StatementAttributeList, "StatementAttributeList",
                     "T_STATEMENT_ATTRIBUTE_LIST");
  NEW_TERMINAL_MACRO(AlignmentSpecifier, "AlignmentSpecifier",
                     "T_ALIGNMENT_SPECIFIER");
  NEW_TERMINAL_MACRO(FortranImplicitRange, "FortranImplicitRange",
                     "T_FORTRAN_IMPLICIT_RANGE");
  NEW_TERMINAL_MACRO(NamespaceSourceFragment, "NamespaceSourceFragment",
                     "T_NAMESPACE_SOURCE_FRAGMENT");

  // they have source position info and should be traversed
  // add all terminals first, then bottom-up traverse class hierarchy to define
  // non-terminals
  /*
      Class hierarchy
        SgOmpClause  {define all enum types here}
          // simplest clause
        * SgOmpFullClause
        * SgOmpOrderedClause
        * SgOmpNowaitClause
        * SgOmpUntiedClause
        * SgOmpBeginClause // experimental nodes for MPI begin..end segments
        * SgOmpEndClause
          // with some value
        * SgOmpDefaultClause
        * SgOmpProcBindClause
          // with kind, chunksize
        * SgOmpScheduleClause
           //with expression
        * SgOmpExpressionClause
        ** SgOmpCollapseClause
        ** SgOmpIfClause
        ** SgOmpNumThreadsClause
        ** SgOmpPartialClause
        * SgOmpDirectiveKindClause
        ** SgOmpAbsentClause
        ** SgOmpContainsClause
          // with variable list
        * SgOmpVariablesClause
        ** SgOmpCopyprivateClause
        ** SgOmpPrivateClause
        ** SgOmpFirstprivateClause
        ** SgOmpSharedClause
        ** SgOmpCopyInClause
        ** SgOmpLastprivateClause
           // reduction, op : list
        *** SgOmpReductionClause
     */
  NEW_TERMINAL_MACRO(OmpOrderedClause, "OmpOrderedClause",
                     "OmpOrderedClauseTag");
  NEW_TERMINAL_MACRO(OmpNowaitClause, "OmpNowaitClause", "OmpNowaitClauseTag");
  NEW_TERMINAL_MACRO(OmpNogroupClause, "OmpNogroupClause",
                     "OmpNogroupClauseTag");
  NEW_TERMINAL_MACRO(OmpReadClause, "OmpReadClause", "OmpReadClauseTag");
  NEW_TERMINAL_MACRO(OmpThreadsClause, "OmpThreadsClause",
                     "OmpThreadsClauseTag");
  NEW_TERMINAL_MACRO(OmpSimdClause, "OmpSimdClause", "OmpSimdClauseTag");
  NEW_TERMINAL_MACRO(OmpCompareClause, "OmpCompareClause",
                     "OmpCompareClauseTag");
  NEW_TERMINAL_MACRO(OmpWeakClause, "OmpWeakClause", "OmpWeakClauseTag");
  NEW_TERMINAL_MACRO(OmpFailClause, "OmpFailClause", "OmpFailClauseTag");
  NEW_TERMINAL_MACRO(OmpReverseOffloadClause, "OmpReverseOffloadClause",
                     "OmpReverseOffloadClauseTag");
  NEW_TERMINAL_MACRO(OmpExtImplementationDefinedRequirementClause,
                     "OmpExtImplementationDefinedRequirementClause",
                     "OmpExtImplementationDefinedRequirementClauseTag");
  NEW_TERMINAL_MACRO(OmpUnifiedAddressClause, "OmpUnifiedAddressClause",
                     "OmpUnifiedAddressClauseTag");
  NEW_TERMINAL_MACRO(OmpUnifiedSharedMemoryClause,
                     "OmpUnifiedSharedMemoryClause",
                     "OmpUnifiedSharedMemoryClauseTag");
  NEW_TERMINAL_MACRO(OmpDynamicAllocatorsClause, "OmpDynamicAllocatorsClause",
                     "OmpDynamicAllocatorsClauseTag");
  NEW_TERMINAL_MACRO(OmpAtomicDefaultMemOrderClause,
                     "OmpAtomicDefaultMemOrderClause",
                     "OmpAtomicDefaultMemOrderClauseTag");
  NEW_TERMINAL_MACRO(OmpWriteClause, "OmpWriteClause", "OmpWriteClauseTag");
  NEW_TERMINAL_MACRO(OmpUpdateClause, "OmpUpdateClause", "OmpUpdateClauseTag");
  NEW_TERMINAL_MACRO(OmpDepobjUpdateClause, "OmpDepobjUpdateClause",
                     "OmpDepobjUpdateClauseTag");
  NEW_TERMINAL_MACRO(OmpDestroyClause, "OmpDestroyClause",
                     "OmpDestroyClauseTag");
  NEW_TERMINAL_MACRO(OmpSelfMapsClause, "OmpSelfMapsClause",
                     "OmpSelfMapsClauseTag");
  NEW_TERMINAL_MACRO(OmpIndirectClause, "OmpIndirectClause",
                     "OmpIndirectClauseTag");
  NEW_TERMINAL_MACRO(OmpNoOpenmpClause, "OmpNoOpenmpClause",
                     "OmpNoOpenmpClauseTag");
  NEW_TERMINAL_MACRO(OmpNoOpenmpRoutinesClause, "OmpNoOpenmpRoutinesClause",
                     "OmpNoOpenmpRoutinesClauseTag");
  NEW_TERMINAL_MACRO(OmpNoParallelismClause, "OmpNoParallelismClause",
                     "OmpNoParallelismClauseTag");
  NEW_TERMINAL_MACRO(OmpAtClause, "OmpAtClause", "OmpAtClauseTag");
  NEW_TERMINAL_MACRO(OmpSeverityClause, "OmpSeverityClause",
                     "OmpSeverityClauseTag");
  NEW_TERMINAL_MACRO(OmpDoacrossClause, "OmpDoacrossClause",
                     "OmpDoacrossClauseTag");
  NEW_TERMINAL_MACRO(OmpOtherwiseClause, "OmpOtherwiseClause",
                     "OmpOtherwiseClauseTag");
  NEW_TERMINAL_MACRO(OmpInductionClause, "OmpInductionClause",
                     "OmpInductionClauseTag");
  NEW_TERMINAL_MACRO(OmpApplyClause, "OmpApplyClause", "OmpApplyClauseTag");
  NEW_TERMINAL_MACRO(OmpInitClause, "OmpInitClause", "OmpInitClauseTag");
  NEW_TERMINAL_MACRO(OmpCaptureClause, "OmpCaptureClause",
                     "OmpCaptureClauseTag");
  NEW_TERMINAL_MACRO(OmpSeqCstClause, "OmpSeqCstClause", "OmpSeqCstClauseTag");
  NEW_TERMINAL_MACRO(OmpAcqRelClause, "OmpAcqRelClause", "OmpAcqRelClauseTag");
  NEW_TERMINAL_MACRO(OmpReleaseClause, "OmpReleaseClause",
                     "OmpReleaseClauseTag");
  NEW_TERMINAL_MACRO(OmpAcquireClause, "OmpAcquireClause",
                     "OmpAcquireClauseTag");
  NEW_TERMINAL_MACRO(OmpRelaxedClause, "OmpRelaxedClause",
                     "OmpRelaxedClauseTag");
  NEW_TERMINAL_MACRO(OmpParallelClause, "OmpParallelClause",
                     "OmpParallelClauseTag");
  NEW_TERMINAL_MACRO(OmpSectionsClause, "OmpSectionsClause",
                     "OmpSectionsClauseTag");
  NEW_TERMINAL_MACRO(OmpForClause, "OmpForClause", "OmpForClauseTag");
  NEW_TERMINAL_MACRO(OmpTaskgroupClause, "OmpTaskgroupClause",
                     "OmpTaskgroupClauseTag");
  NEW_TERMINAL_MACRO(OmpBeginClause, "OmpBeginClause", "OmpBeginClauseTag");
  NEW_TERMINAL_MACRO(OmpEndClause, "OmpEndClause", "OmpEndClauseTag");
  NEW_TERMINAL_MACRO(OmpUntiedClause, "OmpUntiedClause", "OmpUntiedClauseTag");
  NEW_TERMINAL_MACRO(OmpMergeableClause, "OmpMergeableClause",
                     "OmpMergeableClauseTag");
  NEW_TERMINAL_MACRO(OmpDefaultClause, "OmpDefaultClause",
                     "OmpDefaultClauseTag");
  NEW_TERMINAL_MACRO(OmpAtomicClause, "OmpAtomicClause", "OmpAtomicClauseTag");
  NEW_TERMINAL_MACRO(OmpProcBindClause, "OmpProcBindClause",
                     "OmpProcBindClauseTag");
  NEW_TERMINAL_MACRO(OmpOrderClause, "OmpOrderClause", "OmpOrderClauseTag");
  NEW_TERMINAL_MACRO(OmpBindClause, "OmpBindClause", "OmpBindClauseTag");
  NEW_TERMINAL_MACRO(OmpInbranchClause, "OmpInbranchClause",
                     "OmpInbranchClauseTag");
  NEW_TERMINAL_MACRO(OmpNotinbranchClause, "OmpNotinbranchClause",
                     "OmpNotinbranchClauseTag");

  NEW_TERMINAL_MACRO(OmpCollapseClause, "OmpCollapseClause",
                     "OmpCollapseClauseTag");
  NEW_TERMINAL_MACRO(OmpIfClause, "OmpIfClause", "OmpIfClauseTag");
  NEW_TERMINAL_MACRO(OmpFinalClause, "OmpFinalClause", "OmpFinalClauseTag");
  NEW_TERMINAL_MACRO(OmpPriorityClause, "OmpPriorityClause",
                     "OmpPriorityClauseTag");
  NEW_TERMINAL_MACRO(OmpNumThreadsClause, "OmpNumThreadsClause",
                     "OmpNumThreadsClauseTag");
  NEW_TERMINAL_MACRO(OmpNumTeamsClause, "OmpNumTeamsClause",
                     "OmpNumTeamsClauseTag");
  NEW_TERMINAL_MACRO(OmpGrainsizeClause, "OmpGrainsizeClause",
                     "OmpGrainsizeClauseTag");
  NEW_TERMINAL_MACRO(OmpDetachClause, "OmpDetachClause", "OmpDetachClauseTag");
  NEW_TERMINAL_MACRO(OmpNumTasksClause, "OmpNumTasksClause",
                     "OmpNumTasksClauseTag");
  NEW_TERMINAL_MACRO(OmpHintClause, "OmpHintClause", "OmpHintClauseTag");
  NEW_TERMINAL_MACRO(OmpThreadLimitClause, "OmpThreadLimitClause",
                     "OmpThreadLimitClauseTag");
  NEW_TERMINAL_MACRO(OmpNontemporalClause, "OmpNontemporalClause",
                     "OmpNontemporalClauseTag");
  NEW_TERMINAL_MACRO(OmpInclusiveClause, "OmpInclusiveClause",
                     "OmpInclusiveClauseTag");
  NEW_TERMINAL_MACRO(OmpExclusiveClause, "OmpExclusiveClause",
                     "OmpExclusiveClauseTag");
  NEW_TERMINAL_MACRO(OmpIsDevicePtrClause, "OmpIsDevicePtrClause",
                     "OmpIsDevicePtrClauseTag");
  NEW_TERMINAL_MACRO(OmpUseDevicePtrClause, "OmpUseDevicePtrClause",
                     "OmpUseDevicePtrClauseTag");
  NEW_TERMINAL_MACRO(OmpUseDeviceAddrClause, "OmpUseDeviceAddrClause",
                     "OmpUseDeviceAddrClauseTag");
  NEW_TERMINAL_MACRO(OmpHasDeviceAddrClause, "OmpHasDeviceAddrClause",
                     "OmpHasDeviceAddrClauseTag");
  NEW_TERMINAL_MACRO(OmpDeviceClause, "OmpDeviceClause", "OmpIfDeviceTag");
  NEW_TERMINAL_MACRO(OmpNocontextClause, "OmpNocontextClause",
                     "OmpNocontextTag");
  NEW_TERMINAL_MACRO(OmpNovariantsClause, "OmpNovariantsClause",
                     "OmpNovariantsTag");
  NEW_TERMINAL_MACRO(OmpSafelenClause, "OmpSafelenClause", "OmpSafelenTag");
  NEW_TERMINAL_MACRO(OmpSimdlenClause, "OmpSimdlenClause", "OmpSimdlenTag");
  NEW_TERMINAL_MACRO(OmpPartialClause, "OmpPartialClause", "OmpPartialTag");
  NEW_TERMINAL_MACRO(OmpFilterClause, "OmpFilterClause", "OmpFilterTag");
  NEW_TERMINAL_MACRO(OmpFullClause, "OmpFullClause", "OmpFullTag");
  NEW_TERMINAL_MACRO(OmpSizesClause, "OmpSizesClause", "OmpSizesTag");
  NEW_TERMINAL_MACRO(OmpAlignClause, "OmpAlignClause", "OmpAlignClauseTag");
  NEW_TERMINAL_MACRO(OmpMessageClause, "OmpMessageClause",
                     "OmpMessageClauseTag");
  NEW_TERMINAL_MACRO(OmpGraphIdClause, "OmpGraphIdClause",
                     "OmpGraphIdClauseTag");
  NEW_TERMINAL_MACRO(OmpGraphResetClause, "OmpGraphResetClause",
                     "OmpGraphResetClauseTag");
  NEW_TERMINAL_MACRO(OmpTransparentClause, "OmpTransparentClause",
                     "OmpTransparentClauseTag");
  NEW_TERMINAL_MACRO(OmpThreadsetClause, "OmpThreadsetClause",
                     "OmpThreadsetClauseTag");
  NEW_TERMINAL_MACRO(OmpSafesyncClause, "OmpSafesyncClause",
                     "OmpSafesyncClauseTag");
  NEW_TERMINAL_MACRO(OmpLooprangeClause, "OmpLooprangeClause",
                     "OmpLooprangeClauseTag");
  NEW_TERMINAL_MACRO(OmpNoOpenmpConstructsClause, "OmpNoOpenmpConstructsClause",
                     "OmpNoOpenmpConstructsClauseTag");
  NEW_TERMINAL_MACRO(OmpHoldsClause, "OmpHoldsClause", "OmpHoldsClauseTag");
  NEW_TERMINAL_MACRO(OmpUseClause, "OmpUseClause", "OmpUseClauseTag");
  NEW_TERMINAL_MACRO(OmpAbsentClause, "OmpAbsentClause", "OmpAbsentClauseTag");
  NEW_TERMINAL_MACRO(OmpContainsClause, "OmpContainsClause",
                     "OmpContainsClauseTag");

  NEW_NONTERMINAL_MACRO(
      OmpDirectiveKindClause, OmpAbsentClause | OmpContainsClause,
      "OmpDirectiveKindClause", "OmpDirectiveKindClauseTag", false);

  NEW_NONTERMINAL_MACRO(
      OmpExpressionClause,
      OmpNowaitClause | OmpOrderedClause | OmpCollapseClause | OmpIfClause |
          OmpNumThreadsClause | OmpNumTeamsClause | OmpThreadLimitClause |
          OmpDeviceClause | OmpHintClause | OmpGrainsizeClause |
          OmpNumTasksClause | OmpDetachClause | OmpSafelenClause |
          OmpSimdlenClause | OmpFinalClause | OmpPriorityClause |
          OmpNocontextClause | OmpNovariantsClause | OmpPartialClause |
          OmpFilterClause | OmpSizesClause | OmpAlignClause | OmpMessageClause |
          OmpGraphIdClause | OmpGraphResetClause | OmpTransparentClause |
          OmpThreadsetClause | OmpSafesyncClause | OmpLooprangeClause |
          OmpNoOpenmpConstructsClause | OmpHoldsClause | OmpUseClause,
      "OmpExpressionClause", "OmpExpressionClauseTag", false);

  NEW_TERMINAL_MACRO(OmpCopyprivateClause, "OmpCopyprivateClause",
                     "OmpCopyprivateClauseTag");
  NEW_TERMINAL_MACRO(OmpPrivateClause, "OmpPrivateClause",
                     "OmpPrivateClauseTag");
  NEW_TERMINAL_MACRO(OmpFirstprivateClause, "OmpFirstprivateClause",
                     "OmpFirstprivateClauseTag");
  NEW_TERMINAL_MACRO(OmpSharedClause, "OmpSharedClause", "OmpSharedClauseTag");
  NEW_TERMINAL_MACRO(OmpCopyinClause, "OmpCopyinClause", "OmpCopyinClauseTag");
  NEW_TERMINAL_MACRO(OmpLastprivateClause, "OmpLastprivateClause",
                     "OmpLastprivateClauseTag");
  NEW_TERMINAL_MACRO(OmpReductionClause, "OmpReductionClause",
                     "OmpReductionClauseTag");
  NEW_TERMINAL_MACRO(OmpInReductionClause, "OmpInReductionClause",
                     "OmpInReductionClauseTag");
  NEW_TERMINAL_MACRO(OmpTaskReductionClause, "OmpTaskReductionClause",
                     "OmpTaskReductionClauseTag");
  NEW_TERMINAL_MACRO(OmpAllocateClause, "OmpAllocateClause",
                     "OmpAllocateClauseTag");
  NEW_TERMINAL_MACRO(OmpDependClause, "OmpDependClause", "OmpDependClauseTag");
  NEW_TERMINAL_MACRO(OmpToClause, "OmpToClause", "OmpToClauseTag");
  NEW_TERMINAL_MACRO(OmpUsesAllocatorsClause, "OmpUsesAllocatorsClause",
                     "OmpUsesAllocatorsClauseTag");
  NEW_TERMINAL_MACRO(OmpFromClause, "OmpFromClause", "OmpFromClauseTag");
  NEW_TERMINAL_MACRO(OmpAffinityClause, "OmpAffinityClause",
                     "OmpAffinityClauseTag");
  NEW_TERMINAL_MACRO(OmpMapClause, "OmpMapClause", "OmpMapClauseTag");
  NEW_TERMINAL_MACRO(OmpLinearClause, "OmpLinearClause", "OmpLinearClauseTag");
  NEW_TERMINAL_MACRO(OmpUniformClause, "OmpUniformClause",
                     "OmpUniformClauseTag");
  NEW_TERMINAL_MACRO(OmpAlignedClause, "OmpAlignedClause",
                     "OmpAlignedClauseTag");
  NEW_TERMINAL_MACRO(OmpLinkClause, "OmpLinkClause", "OmpLinkClauseTag");
  NEW_TERMINAL_MACRO(OmpEnterClause, "OmpEnterClause", "OmpEnterClauseTag");
  NEW_TERMINAL_MACRO(OmpLocalClause, "OmpLocalClause", "OmpLocalClauseTag");

  NEW_NONTERMINAL_MACRO(
      OmpVariablesClause,
      OmpCopyprivateClause | OmpPrivateClause | OmpFirstprivateClause |
          OmpNontemporalClause | OmpInclusiveClause | OmpExclusiveClause |
          OmpIsDevicePtrClause | OmpUseDevicePtrClause |
          OmpUseDeviceAddrClause | OmpHasDeviceAddrClause | OmpSharedClause |
          OmpCopyinClause | OmpLastprivateClause | OmpReductionClause |
          OmpInReductionClause | OmpTaskReductionClause | OmpMapClause |
          OmpAllocateClause | OmpUniformClause | OmpAlignedClause |
          OmpLinearClause | OmpDependClause | OmpAffinityClause | OmpToClause |
          OmpFromClause | OmpLinkClause | OmpEnterClause | OmpLocalClause,
      "OmpVariablesClause", "OmpVariablesClauseTag", false);

  NEW_TERMINAL_MACRO(OmpScheduleClause, "OmpScheduleClause",
                     "OmpScheduleClauseTag");
  NEW_TERMINAL_MACRO(OmpContextSelectorProperty, "OmpContextSelectorProperty",
                     "OmpContextSelectorPropertyTag");
  NEW_TERMINAL_MACRO(OmpContextSelector, "OmpContextSelector",
                     "OmpContextSelectorTag");
  NEW_TERMINAL_MACRO(OmpContextSelectorSet, "OmpContextSelectorSet",
                     "OmpContextSelectorSetTag");
  NEW_TERMINAL_MACRO(OmpWhenClause, "OmpWhenClause", "OmpWhenClauseTag");
  NEW_TERMINAL_MACRO(OmpMatchClause, "OmpMatchClause", "OmpMatchClauseTag");
  NEW_TERMINAL_MACRO(OmpAdjustArgsClause, "OmpAdjustArgsClause",
                     "OmpAdjustArgsClauseTag");
  NEW_TERMINAL_MACRO(OmpAppendArgsClause, "OmpAppendArgsClause",
                     "OmpAppendArgsClauseTag");
  NEW_TERMINAL_MACRO(OmpDistScheduleClause, "OmpDistScheduleClause",
                     "OmpDistScheduleClauseTag");
  NEW_TERMINAL_MACRO(OmpDefaultmapClause, "OmpDefaultmapClause",
                     "OmpDefaultmapClauseTag");
  NEW_TERMINAL_MACRO(OmpAllocatorClause, "OmpAllocatorClause",
                     "OmpAllocatorClauseTag");
  NEW_TERMINAL_MACRO(OmpUsesAllocatorsDefination, "OmpUsesAllocatorsDefination",
                     "OmpUsesAllocatorsDefinationTag");
  NEW_NONTERMINAL_MACRO(
      OmpClause,
      OmpReadClause | OmpThreadsClause | OmpSimdClause | OmpCompareClause |
          OmpWeakClause | OmpFailClause | OmpWriteClause | OmpUpdateClause |
          OmpDepobjUpdateClause | OmpDestroyClause | OmpCaptureClause |
          OmpBeginClause | OmpEndClause | OmpUntiedClause | OmpSeqCstClause |
          OmpAcqRelClause | OmpReleaseClause | OmpAcquireClause |
          OmpRelaxedClause | OmpReverseOffloadClause | OmpUnifiedAddressClause |
          OmpUnifiedSharedMemoryClause | OmpDynamicAllocatorsClause |
          OmpParallelClause | OmpSectionsClause | OmpForClause |
          OmpTaskgroupClause | OmpNogroupClause | OmpDefaultClause |
          OmpAllocatorClause | OmpAtomicClause | OmpProcBindClause |
          OmpBindClause | OmpOrderClause | OmpDistScheduleClause |
          OmpExpressionClause | OmpInbranchClause | OmpNotinbranchClause |
          OmpDefaultmapClause | OmpAtomicDefaultMemOrderClause |
          OmpExtImplementationDefinedRequirementClause |
          OmpUsesAllocatorsDefination | OmpVariablesClause | OmpScheduleClause |
          OmpMergeableClause | OmpWhenClause | OmpMatchClause |
          OmpAdjustArgsClause | OmpAppendArgsClause | OmpUsesAllocatorsClause |
          OmpFullClause | OmpSelfMapsClause | OmpIndirectClause |
          OmpNoOpenmpClause | OmpNoOpenmpRoutinesClause |
          OmpNoParallelismClause | OmpAtClause | OmpSeverityClause |
          OmpDoacrossClause | OmpOtherwiseClause | OmpInductionClause |
          OmpApplyClause | OmpInitClause | OmpDirectiveKindClause,
      "OmpClause", "OmpClauseTag", false);

  // ***********************************************************************
  // ***********************************************************************
  //                       OpenACC Clauses
  // ***********************************************************************
  // ***********************************************************************

  NEW_TERMINAL_MACRO(AccCollapseClause, "AccCollapseClause",
                     "AccCollapseClauseTag");
  NEW_TERMINAL_MACRO(AccNumGangsClause, "AccNumGangsClause",
                     "AccNumGangsClauseTag");
  NEW_TERMINAL_MACRO(AccNumWorkersClause, "AccNumWorkersClause",
                     "AccNumWorkersClauseTag");
  NEW_TERMINAL_MACRO(AccVectorLengthClause, "AccVectorLengthClause",
                     "AccVectorLengthClauseTag");
  NEW_TERMINAL_MACRO(AccAsyncClause, "AccAsyncClause", "AccAsyncClauseTag");
  NEW_TERMINAL_MACRO(AccIfClause, "AccIfClause", "AccIfClauseTag");
  NEW_TERMINAL_MACRO(AccVectorClause, "AccVectorClause", "AccVectorClauseTag");

  NEW_NONTERMINAL_MACRO(AccExpressionClause,
                        AccCollapseClause | AccNumGangsClause |
                            AccNumWorkersClause | AccVectorLengthClause |
                            AccAsyncClause | AccIfClause | AccVectorClause,
                        "AccExpressionClause", "AccExpressionClauseTag", false);

  NEW_TERMINAL_MACRO(AccCopyClause, "AccCopyClause", "AccCopyClauseTag");
  NEW_TERMINAL_MACRO(AccCopyinClause, "AccCopyinClause", "AccCopyinClauseTag");
  NEW_TERMINAL_MACRO(AccCopyoutClause, "AccCopyoutClause",
                     "AccCopyoutClauseTag");
  NEW_TERMINAL_MACRO(AccCreateClause, "AccCreateClause", "AccCreateClauseTag");
  NEW_TERMINAL_MACRO(AccPresentClause, "AccPresentClause",
                     "AccPresentClauseTag");
  NEW_TERMINAL_MACRO(AccPrivateClause, "AccPrivateClause",
                     "AccPrivateClauseTag");
  NEW_TERMINAL_MACRO(AccDeviceptrClause, "AccDeviceptrClause",
                     "AccDeviceptrClauseTag");
  NEW_TERMINAL_MACRO(AccDeleteClause, "AccDeleteClause", "AccDeleteClauseTag");
  NEW_TERMINAL_MACRO(AccReductionClause, "AccReductionClause",
                     "AccReductionClauseTag");

  NEW_NONTERMINAL_MACRO(AccVariablesClause,
                        AccCopyClause | AccCopyinClause | AccCopyoutClause |
                            AccCreateClause | AccPresentClause |
                            AccPrivateClause | AccDeviceptrClause |
                            AccDeleteClause | AccReductionClause,
                        "AccVariablesClause", "AccVariablesClauseTag", false);

  NEW_TERMINAL_MACRO(AccDefaultClause, "AccDefaultClause",
                     "AccDefaultClauseTag");
  NEW_TERMINAL_MACRO(AccGangClause, "AccGangClause", "AccGangClauseTag");
  NEW_TERMINAL_MACRO(AccSeqClause, "AccSeqClause", "AccSeqClauseTag");
  NEW_TERMINAL_MACRO(AccUpdateClause, "AccUpdateClause", "AccUpdateClauseTag");
  NEW_TERMINAL_MACRO(AccReadClause, "AccReadClause", "AccReadClauseTag");
  NEW_TERMINAL_MACRO(AccWriteClause, "AccWriteClause", "AccWriteClauseTag");
  NEW_TERMINAL_MACRO(AccCaptureClause, "AccCaptureClause",
                     "AccCaptureClauseTag");

  NEW_NONTERMINAL_MACRO(AccClause,
                        AccExpressionClause | AccVariablesClause |
                            AccDefaultClause | AccGangClause | AccSeqClause |
                            AccUpdateClause | AccReadClause | AccWriteClause |
                            AccCaptureClause,
                        "AccClause", "AccClauseTag", false);

  // DQ (10/3/2008): Support for the Fortran "USE" statement and its rename list
  // option.
  NEW_TERMINAL_MACRO(RenamePair, "RenamePair", "TEMP_Rename_Pair");

  // DQ (10/6/2008): Support for the Fortran "USE" statement and its rename list
  // option.
  NEW_TERMINAL_MACRO(InterfaceBody, "InterfaceBody", "TEMP_Interface_Body");

  // negara1 (08/10/2011): Support for included files (i.e. headers) bodies.
  NEW_TERMINAL_MACRO(HeaderFileBody, "HeaderFileBody", "TEMP_Header_File_Body");

  NEW_TERMINAL_MACRO(CxxLiteralToken, "CxxLiteralToken", "CXX_LITERAL_TOKEN");
  NEW_TERMINAL_MACRO(CxxLiteralTokenList, "CxxLiteralTokenList",
                     "CXX_LITERAL_TOKEN_LIST");

  NEW_NONTERMINAL_MACRO(
      LocatedNodeSupport,
      CxxLiteralToken | CxxLiteralTokenList | CommonBlockObject |
          InitializedName | InterfaceBody | HeaderFileBody | RenamePair |
          OmpClause | AccClause | LambdaCapture | LambdaCaptureList |
          DeclarationScopeList | AuxiliaryDeclarationList | OmpClauseList |
          StatementAttribute | StatementAttributeList | AlignmentSpecifier |
          FortranImplicitRange | NamespaceSourceFragment |
          OmpContextSelectorProperty | OmpContextSelector |
          OmpContextSelectorSet,
      "LocatedNodeSupport", "LocatedNodeSupportTag", false);

  NEW_TERMINAL_MACRO(Token, "Token", "TOKEN");

  NEW_NONTERMINAL_MACRO(LocatedNode,
                        Token | LocatedNodeSupport | Statement | Expression,
                        "LocatedNode", "LocatedNodeTag", false);

  AstNodeClass &Type = *lookupTerminal(terminalList, "Type");
  AstNodeClass &Symbol = *lookupTerminal(terminalList, "Symbol");
  AstNodeClass &Support = *lookupTerminal(terminalList, "Support");

  NEW_TERMINAL_MACRO(CxxArrayInitBinding, "CxxArrayInitBinding",
                     "CXX_ARRAY_INIT_BINDING");
  CxxArrayInitBinding.setFunctionPrototype("HEADER_CXX_ARRAY_INIT_BINDING",
                                           "../Grammar/Node.code");
  CxxArrayInitBinding.setDataPrototype(
      "SgExpression*", "common_source", "= nullptr", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      REQUIRED_TRAVERSAL_MEMBER);
  CxxArrayInitBinding.setDataPrototype(
      "SgCastExp::value_category_enum", "common_value_category",
      "= SgCastExp::e_value_category_unclassified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_NONTERMINAL_MACRO(
      Node, Support | Type | LocatedNode | Symbol | CxxArrayInitBinding, "Node",
      "NodeTag", false);

  // ***********************************************************************
  // ***********************************************************************
  //                       Header Code Declaration
  // ***********************************************************************
  // ***********************************************************************

  // Header declarations for Node
  Node.setPredeclarationString("HEADER_NODE_PREDECLARATION",
                               "../Grammar/Node.code");

  Node.setFunctionPrototype("HEADER", "../Grammar/Common.code");
  Node.setSubTreeFunctionPrototype("HEADER", "../Grammar/Common.code");

  Node.setFunctionPrototype("HEADER", "../Grammar/Node.code");

  // This function exists everywhere (at each node of the grammar)!
  Node.setSubTreeFunctionPrototype("HEADER_IS_CLASSNAME",
                                   "../Grammar/Node.code");

  // MK: I moved the following data member declaration from ../Grammar/Node.code
  // to this position, we rely on the access functions to be defined in the
  // .code files, maybe this should be changed;
  Node.setDataPrototype("SgNode*", "parent", "= NULL", NO_CONSTRUCTOR_PARAMETER,
                        NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                        NO_COPY_DATA);

  // A semantic copy preserves the source edit state. Copy-construction
  // rebinding uses its exact identity transaction without marking source edits.
  Node.setDataPrototype("bool", "isModified", "= false",
                        NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  Node.setDataPrototype("bool", "containsTransformation", "= false",
                        NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  Node.setDataPrototype("uint64_t", "lastMutationSequence", "= 0",
                        NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  // The source-state construction epoch current when this node was born. A
  // copy is born at its own construction, so the epoch is never copied.
  Node.setDataPrototype("uint64_t", "constructionEpoch", "= 0",
                        NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);

  // Checkpoint numbering belongs to this node's lifetime. Copies acquire their
  // own numbering; recycling a memory-pool address must not inherit an old ID.
  Node.setDataPrototype("uint64_t", "ast_json_node_id", "= 0",
                        NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);

  Node.setDataPrototype("$CLASSNAME*", "freepointer",
                        "= ASTMemoryPool::validNodeMarker()",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);

  // Static table storage is defined separately from generated constructors.
  Node.setDataPrototype("static SgFunctionTypeTable*",
                        "globalFunctionTypeTable", "", NO_CONSTRUCTOR_PARAMETER,
                        NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                        NO_COPY_DATA);

  // DQ (7/22/2010): Added support for type table supporting construction of
  // unique types.
  Node.setDataPrototype("static SgTypeTable*", "globalTypeTable", "",
                        NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);

  Node.setDataPrototype("static std::unordered_map<SgNode*,std::string>",
                        "globalMangledNameMap", "", NO_CONSTRUCTOR_PARAMETER,
                        NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                        NO_COPY_DATA);
  // DQ (6/26/2007): Added support from Jeremiah for shortened mangle names
  Node.setDataPrototype("static std::map<std::string, uint64_t>",
                        "shortMangledNameCache", "", NO_CONSTRUCTOR_PARAMETER,
                        NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                        NO_COPY_DATA);

  LocatedNode.setFunctionPrototype("HEADER", "../Grammar/LocatedNode.code");
  LocatedNode.setDataPrototype("Sg_File_Info*", "startOfConstruct", "= NULL",
                               CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, DEF_DELETE, CLONE_PTR);
  LocatedNode.setDataPrototype("Sg_File_Info*", "endOfConstruct", "= NULL",
                               NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, DEF_DELETE, CLONE_PTR);

  // Preserve whether the frontend declaration range ends in a macro expansion
  // as typed source provenance.  Token ownership must not depend on a
  // string-keyed AstAttribute side channel.
  LocatedNode.setDataPrototype("bool", "source_range_ends_in_macro_expansion",
                               "= false", NO_CONSTRUCTOR_PARAMETER,
                               NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                               COPY_DATA);
  // A declaration or statement that covers only a semantic fragment of a macro
  // replacement has physical invocation coordinates, but it does not own an
  // independently written token surface.  Preserve that producer fact instead
  // of asking token mapping to infer ownership from overlapping coordinates.
  LocatedNode.setDataPrototype(
      "bool", "source_range_is_macro_expansion_fragment", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  // Each copy owns its records. Destruction must release their reciprocal
  // owner edges before deleting the records and their container.
  LocatedNode.setDataPrototype("AttachedPreprocessingInfoType*",
                               "attachedPreprocessingInfoPtr", "= NULL",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, DEF_DELETE, CLONE_PTR);
  LocatedNode.setMemberDestructor("attachedPreprocessingInfoPtr",
                                  "clearAttachedPreprocessingInfo();");

  LocatedNode.setDataPrototype("AstAttributeMechanism*", "attributeMechanism",
                               "= NULL", NO_CONSTRUCTOR_PARAMETER,
                               BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                               DEF_DELETE, CLONE_PTR);

  LocatedNode.setFunctionPrototype("HEADER_ATTRIBUTE_SUPPORT",
                                   "../Grammar/Support.code");
  LocatedNode.setFunctionSource("SOURCE_ATTRIBUTE_SUPPORT",
                                "../Grammar/Support.code");

  LocatedNode.setDataPrototype(
      "bool", "containsTransformationToSurroundingWhitespace", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE, NO_COPY_DATA);

  // A token's spelling and classification are its semantic payload and must
  // be preserved when its owning source file is copied.
  Token.setDataPrototype("std::string", "lexeme_string", "= \"\"",
                         CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                         NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  Token.setDataPrototype("unsigned int", "classification_code", "= 0",
                         CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                         NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  CxxLiteralToken.setFunctionPrototype("HEADER_CXX_LITERAL_TOKEN",
                                       "../Grammar/LocatedNode.code");
  CxxLiteralToken.setDataPrototype(
      "SgCxxLiteralToken::token_kind_enum", "token_kind",
      "= SgCxxLiteralToken::e_numeric_token", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CxxLiteralToken.setDataPrototype(
      "std::string", "spelling", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CxxLiteralTokenList.setDataPrototype(
      "SgCxxLiteralTokenPtrList", "tokens", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  AuxiliaryDeclarationList.setFunctionPrototype(
      "HEADER_AUXILIARY_DECLARATION_LIST", "../Grammar/LocatedNode.code");
  OmpClauseList.setFunctionPrototype("HEADER_OMP_CLAUSE_LIST",
                                     "../Grammar/LocatedNode.code");
  StatementAttribute.setFunctionPrototype("HEADER_STATEMENT_ATTRIBUTE",
                                          "../Grammar/Support.code");
  StatementAttributeList.setFunctionPrototype("HEADER_STATEMENT_ATTRIBUTE_LIST",
                                              "../Grammar/Support.code");
  AlignmentSpecifier.setFunctionPrototype("HEADER_ALIGNMENT_SPECIFIER",
                                          "../Grammar/Support.code");
  FortranImplicitRange.setFunctionPrototype("HEADER_FORTRAN_IMPLICIT_RANGE",
                                            "../Grammar/LocatedNode.code");
  FortranImplicitRange.setFunctionSource("SOURCE_FORTRAN_IMPLICIT_RANGE",
                                         "../Grammar/LocatedNode.code");
  FortranImplicitRange.setDataPrototype(
      "char", "first_letter", "= 0", CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
  FortranImplicitRange.setDataPrototype(
      "std::optional<char>", "last_letter", "= std::nullopt",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FortranImplicitRange.setDataPrototype(
      "SgType*", "type", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  FortranImplicitRange.setDataPrototype(
      "SgFortranImplicitRange::type_spec_enum", "type_spec",
      "= SgFortranImplicitRange::e_intrinsic", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FortranImplicitRange.setDataPrototype(
      "SgSymbol*", "source_derived_type_symbol", "= nullptr",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);
  NamespaceSourceFragment.setFunctionPrototype(
      "HEADER_NAMESPACE_SOURCE_FRAGMENT", "../Grammar/LocatedNode.code");
  NamespaceSourceFragment.setDataPrototype(
      "SgNamespaceSourceFragment::namespace_source_fragment_kind_enum", "kind",
      "= e_namespace_source_fragment_unknown", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NamespaceSourceFragment.setDataPrototype(
      "SgNamespaceSourceFragment::namespace_source_fragment_form_enum",
      "source_form", "= e_namespace_source_fragment_form_unclassified",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NamespaceSourceFragment.setDataPrototype(
      "bool", "contains_namespace_name", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  RenamePair.setFunctionPrototype("HEADER_RENAME_PAIR",
                                  "../Grammar/LocatedNode.code");
  RenamePair.setDataPrototype("SgName", "local_name", "= \"\"",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  RenamePair.setDataPrototype("SgName", "use_name", "= \"\"",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);

  // Record whether the function declaration or the function name was used in
  // the interface body (F90 permits either one).
  InterfaceBody.setDataPrototype("SgName", "function_name", "= \"\"",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);
  // An interface body is the exclusive structural owner of its source
  // procedure declaration.  Sharing this child with a CONTAINS declaration is
  // malformed AST state and is rejected by the frontend producer.
  InterfaceBody.setDataPrototype(
      "SgFunctionDeclaration*", "functionDeclaration", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE,
      COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  InterfaceBody.setDataPrototype("bool", "use_function_name", "= false",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);

  HeaderFileBody.setDataPrototype(
      "SgSourceFile*", "include_file", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  CommonBlockObject.setDataPrototype(
      "std::string", "block_name", "=\"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CommonBlockObject.setDataPrototype(
      "SgExprListExp*", "variable_reference_list", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  CommonBlockObject.setDataPrototype(
      "SgCommonBlockObject*", "canonical_common_block", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  InitializedName.setFunctionPrototype("HEADER_INITIALIZED_NAME",
                                       "../Grammar/Support.code");

  InitializedName.setDataPrototype("SgName", "name", "= NULL",
                                   CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                   NO_TRAVERSAL, NO_DELETE);
  InitializedName.setDataPrototype(
      "SgAlignmentSpecifierPtrList", "alignment_specifiers", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_TREE);

  InitializedName.setDataPrototype("SgType*", "typeptr", "= NULL",
                                   CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                   NO_TRAVERSAL, DELETE_IF_OWNED, CLONE_TREE);
  InitializedName.setDataPrototype(
      "std::optional<long long>", "storage_size_bytes", "= std::nullopt",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);
  InitializedName.setDataPrototype(
      "std::optional<unsigned int>", "cxx_field_index", "= std::nullopt",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  // Fortran declarations have two independent type contracts.  p_typeptr is
  // the exact semantic type published by Flang; this cross-edge records the
  // source-spelled type surface used by the unparser (including omitted
  // default kind/length selectors and source-owned shape syntax).
  InitializedName.setDataPrototype(
      "SgType*", "fortran_source_type", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  InitializedName.setDataPrototype(
      "SgInitializedName::fortran_c_argument_passing_enum",
      "fortran_c_argument_passing",
      "= SgInitializedName::e_fortran_c_argument_none",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  // A C/C++ declarator likewise has independent semantic and source type
  // contracts.  Clang canonicalizes an explicitly written template-id to one
  // shared class specialization, while each TypeLoc can spell equivalent
  // arguments differently.  Keep the per-declarator source graph here; never
  // write that spelling onto the shared semantic SgTemplateInstantiationDecl.
  InitializedName.setDataPrototype(
      "SgType*", "cxx_source_type", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, DELETE_IF_OWNED, CLONE_TREE);

  // A source-written derived type-spec names a visible symbol, which may be a
  // USE-associated alias whose spelling intentionally differs from the
  // canonical SgClassType name.  Keep that binding separate from both the
  // semantic type and the source-owned selector/shape type surface.
  InitializedName.setDataPrototype(
      "SgSymbol*", "fortran_source_derived_type_symbol", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  // A Cray pointer declaration is a semantic pair, POINTER(pointer, pointee).
  // Do not overload prev_decl_item: that field is declaration-chain state and
  // is rewritten when the pointer name already has an ordinary declaration.
  InitializedName.setDataPrototype("SgInitializedName*", "cray_pointer_pointee",
                                   "= NULL", NO_CONSTRUCTOR_PARAMETER,
                                   BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                   NO_DELETE, COPY_DATA);

  // A shaped Cray pointee owns its written ArraySpec in the POINTER
  // statement, independently of the pointee's complete semantic array type.
  // Keep that syntax as an owned child of the pointer name; the pointee edge
  // above remains the semantic cross-reference.
  InitializedName.setDataPrototype(
      "SgExprListExp*", "fortran_cray_pointer_pointee_shape", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);

  // Clang predefined identifiers such as __func__ have function-local
  // variable semantics but no source declaration.  Their initialized name is
  // owned by a semantic auxiliary variable declaration so symbol bases remain
  // structurally reachable during copies and AST moves.
  InitializedName.setDataPrototype(
      "bool", "is_predefined_identifier", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Generated variable spellings must not serve as a hidden communication
  // channel between transformations.  Preserve the exact typed role instead.
  InitializedName.setDataPrototype(
      "SgInitializedName::generated_variable_role_enum",
      "generated_variable_role",
      "= SgInitializedName::e_generated_variable_none",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // An enum definition's semantic enumerator list can include constants whose
  // spelling is owned by an included physical file.  Preserve the exact
  // construction-time source role per constant so preprocessing placement and
  // code generation never infer ownership from a path or output flag.
  InitializedName.setDataPrototype(
      "SgInitializedName::enum_constant_source_ownership_enum",
      "enum_constant_source_ownership",
      "= SgInitializedName::e_enum_constant_source_unclassified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  InitializedName.setDataPrototype(
      "std::optional<long long>", "enum_constant_value", "= std::nullopt",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  // REX (7/10/2026): Preserve per-declarator Fortran spelling. SgClassType and
  // SgFunctionType are shared structural types and therefore cannot encode
  // whether this particular declaration used TYPE/CLASS or a named procedure
  // interface.
  InitializedName.setDataPrototype(
      "SgInitializedName::fortran_type_spec_enum", "fortran_type_spec",
      "= SgInitializedName::e_fortran_type_spec_default",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  InitializedName.setDataPrototype(
      "SgExpression*", "fortran_procedure_interface", "= nullptr",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE,
      CLONE_TREE);
  InitializedName.setDataPrototype(
      "std::optional<SgInitializedName::FortranProcedureComponentBinding>",
      "fortran_procedure_component_binding", "= std::nullopt",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  // A Fortran entity's semantic type includes its complete array shape even
  // when a separate DIMENSION, ALLOCATABLE, COMMON, or Cray POINTER statement
  // owns the written shape.
  // Preserve that exact statement as a typed cross-edge so no consumer has to
  // rediscover source ownership by scanning neighboring declarations.
  InitializedName.setDataPrototype(
      "SgStatement*", "fortran_separate_shape_declaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  // A separate Fortran POINTER statement owns the written POINTER attribute,
  // independently of the entity's semantic SgPointerType wrapper and its
  // declaration-type-spec source surface.  Preserve that exact statement as a
  // typed cross-edge; source/semantic type validation must never infer the
  // missing attribute from a type mismatch or neighboring statement text.
  InitializedName.setDataPrototype(
      "SgStatement*", "fortran_separate_pointer_declaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  InitializedName.setDataPrototype(
      "SgInitializer*", "initptr", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_AFTER_OWNER_CONSTRUCTION);

  InitializedName.setDataPrototype("SgInitializedName*", "prev_decl_item",
                                   "= NULL", NO_CONSTRUCTOR_PARAMETER,
                                   BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                   NO_DELETE, COPY_REFERENCE);
  InitializedName.setDataPrototype(
      "SgDeclarationStatement*", "declptr", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  // SgVariableDefinition is structurally owned by the exact initialized name
  // that it describes.  The historical declptr field cannot express that
  // ownership: it is also used as a non-owning cross-reference to variable,
  // function, and enum declarations.  Keeping the definition only in declptr
  // hid it from traversal and forced copy fixup to manufacture the missing
  // node after the structural copy had already completed.  Publish the owned
  // edge explicitly; declptr remains the independently validated semantic
  // declaration edge.
  InitializedName.setDataPrototype(
      "SgVariableDefinition*", "variable_definition", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);

  // post_construction_initialization allocates a fresh modifier for each copy.
  InitializedName.setDataPrototype("SgStorageModifier*", "storageModifier",
                                   "= NULL", NO_CONSTRUCTOR_PARAMETER,
                                   NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                   DEF_DELETE, NO_COPY_DATA);
  // DQ (11/15/2004): class declarations for nested classes can appear outside
  // the scope of the class to which they belong, thus the parent information is
  // not sufficent to define the relationship of nested classes (and typedefs
  // within the classes, as well, which is the current bug in Kull).  So we need
  // an additional data member to explicitly represent the scope of a class
  // (consistant with the design of the member function declaration).
  InitializedName.setDataPrototype(
      "SgScopeStatement*", "scope", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  InitializedName.setDataPrototype(
      "SgInitializedName::preinitialization_enum", "preinitialization",
      "= e_unknown_preinitialization", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InitializedName.setDataPrototype(
      "SgInitializedName*", "ctor_initializer_field_target", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);
  InitializedName.setDataPrototype(
      "std::optional<unsigned int>", "ctor_initializer_source_order",
      "= std::nullopt", NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  InitializedName.setDataPrototype(
      "bool", "isCoArray", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);

  InitializedName.setFunctionPrototype("HEADER_ATTRIBUTE_SUPPORT",
                                       "../Grammar/Support.code");
  InitializedName.setFunctionSource("SOURCE_ATTRIBUTE_SUPPORT",
                                    "../Grammar/Support.code");
  // Preserve the exact target spelling for GNU variable asm labels. Register
  // aliases and widths are target-specific source identity and cannot be
  // represented by an architecture-dependent enum.
  InitializedName.setDataPrototype(
      "std::string", "register_name_string", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "bool", "requiresGlobalNameQualificationOnType", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  InitializedName.setDataPrototype(
      "bool", "shapeDeferred", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "bool", "initializationDeferred", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "SgBitVector", "gnu_attribute_modifierVector", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  InitializedName.setDataPrototype("std::string", "gnu_attribute_section_name",
                                   "=\"\"", NO_CONSTRUCTOR_PARAMETER,
                                   BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                   NO_DELETE);

  InitializedName.setDataPrototype(
      "int", "gnu_attribute_alignment", "= -1", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "bool", "protected_declaration", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "int", "name_qualification_length_for_type", "= 0",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "bool", "type_elaboration_required_for_type", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "bool", "global_qualification_required_for_type", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Preserve the exact source-written qualifier for this declarator's type.
  // The producer-owned record is independent of per-unparse-session computed
  // qualification.
  InitializedName.setDataPrototype("bool", "source_type_qualification_present",
                                   "= false", NO_CONSTRUCTOR_PARAMETER,
                                   BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                   NO_DELETE);
  InitializedName.setDataPrototype("bool", "source_type_global_qualification",
                                   "= false", NO_CONSTRUCTOR_PARAMETER,
                                   BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                   NO_DELETE);
  InitializedName.setDataPrototype(
      "SgStringList", "source_type_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  // Preserve the exact source-written qualifier on the declarator name.  This
  // is a distinct channel from the type qualifier above: `N::T N::value`
  // owns two independently written nested-name-specifiers.
  InitializedName.setDataPrototype("bool", "source_name_qualification_present",
                                   "= false", NO_CONSTRUCTOR_PARAMETER,
                                   BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                   NO_DELETE);
  InitializedName.setDataPrototype("bool", "source_name_global_qualification",
                                   "= false", NO_CONSTRUCTOR_PARAMETER,
                                   BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                   NO_DELETE);
  InitializedName.setDataPrototype(
      "SgStringList", "source_name_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  InitializedName.setDataPrototype(
      "SgType *", "auto_decltype", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "SgDeclarationScope*", "structured_binding_scope", "= nullptr",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);
  InitializedName.setDataPrototype(
      "SgExprListExp*", "structured_binding_pattern", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);

  InitializedName.setDataPrototype(
      "SgInitializedName::structured_binding_kind_enum",
      "structured_binding_kind",
      "= SgInitializedName::e_not_structured_binding", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InitializedName.setDataPrototype(
      "SgInitializedName*", "structured_binding_owner", "= nullptr",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);
  InitializedName.setDataPrototype(
      "SgExpression*", "structured_binding_value", "= nullptr",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);
  InitializedName.setDataPrototype(
      "SgInitializedName*", "structured_binding_holding_variable", "= nullptr",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);

  InitializedName.setDataPrototype(
      "bool", "is_braced_initialized", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "bool", "using_assignment_copy_constructor_syntax", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  InitializedName.setDataPrototype(
      "bool", "needs_definitions", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  InitializedName.setDataPrototype(
      "bool", "is_parameter_pack", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InitializedName.setDataPrototype(
      "bool", "is_pack_element", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InitializedName.setDataPrototype("bool", "deduction_guide_default_available",
                                   "= false", NO_CONSTRUCTOR_PARAMETER,
                                   BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                   NO_DELETE, COPY_DATA);

  LambdaCapture.setFunctionSource(
      "SOURCE_LAMBDA_CAPTURE_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Node.code");
  LambdaCapture.setFunctionPrototype(
      "HEADER_LAMBDA_CAPTURE_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Node.code");
  LambdaCapture.setDataPrototype("SgExpression*", "capture_variable", "",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                 REQUIRED_TRAVERSAL_MEMBER);
  LambdaCapture.setDataPrototype(
      "SgExpression*", "capture_initializer", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      REQUIRED_TRAVERSAL_MEMBER);
  LambdaCapture.setDataPrototype("SgExpression*", "closure_variable", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                 OPTIONAL_TRAVERSAL_MEMBER);
  LambdaCapture.setDataPrototype("bool", "capture_by_reference", "= false",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);
  LambdaCapture.setDataPrototype("bool", "implicit", "= false",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);
  LambdaCapture.setDataPrototype("bool", "pack_expansion", "= false",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);
  LambdaCapture.setDataPrototype("bool", "is_init_capture", "= false",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);

  LambdaCaptureList.setDataPrototype(
      "SgLambdaCapturePtrList", "capture_list", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  DeclarationScopeList.setDataPrototype(
      "SgDeclarationScopePtrList", "scopes", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  AuxiliaryDeclarationList.setDataPrototype(
      "SgDeclarationStatementPtrList", "declarations", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);
  OmpClauseList.setDataPrototype("SgOmpClausePtrList", "clauses", "",
                                 NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                 DEF_TRAVERSAL, NO_DELETE, CLONE_TREE);
  StatementAttribute.setDataPrototype(
      "SgStatementAttribute::statement_attribute_kind_enum", "kind",
      "= SgStatementAttribute::e_last_statement_attribute_kind",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  StatementAttribute.setDataPrototype(
      "SgStatementAttribute::statement_attribute_spelling_enum", "spelling",
      "= SgStatementAttribute::e_last_statement_attribute_spelling",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  StatementAttribute.setDataPrototype(
      "SgExpression*", "expression_argument", "= NULL", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);
  StatementAttribute.setDataPrototype(
      "unsigned long", "integral_argument", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  StatementAttribute.setDataPrototype(
      "SgStatementAttribute::loop_hint_option_enum", "loop_hint_option",
      "= SgStatementAttribute::e_loop_hint_option_none", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  StatementAttribute.setDataPrototype(
      "SgStatementAttribute::loop_hint_state_enum", "loop_hint_state",
      "= SgStatementAttribute::e_loop_hint_state_none", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  StatementAttributeList.setDataPrototype(
      "SgStatementAttributePtrList", "attributes", "", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE);
  AlignmentSpecifier.setDataPrototype(
      "SgAlignmentSpecifier::spelling_enum", "spelling",
      "= SgAlignmentSpecifier::e_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  AlignmentSpecifier.setDataPrototype(
      "SgExpression*", "operand", "= NULL", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);
  AlignmentSpecifier.setDataPrototype(
      "bool", "is_pack_expansion", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  AlignmentSpecifier.setFunctionSource("SOURCE_ALIGNMENT_SPECIFIER",
                                       "../Grammar/Support.code");

  // ***********************************************************************
  // ***********************************************************************
  //                       Source Code Definition
  // ***********************************************************************
  // ***********************************************************************

  // Source code for Node
  Node.setFunctionSource("SOURCE", "../Grammar/Common.code");
  Node.setSubTreeFunctionSource("SOURCE", "../Grammar/Common.code");

  Node.setFunctionSource("SOURCE", "../Grammar/Node.code");
  Node.editSubstitute("CONSTRUCTOR_BODY", " ");

  // Source code for LocatedNode
  LocatedNode.setFunctionSource("SOURCE", "../Grammar/LocatedNode.code");
  LocatedNode.editSubstitute("CONSTRUCTOR_BODY", " ");
  AuxiliaryDeclarationList.setFunctionSource(
      "SOURCE_AUXILIARY_DECLARATION_LIST", "../Grammar/LocatedNode.code");
  OmpClauseList.setFunctionSource("SOURCE_OMP_CLAUSE_LIST",
                                  "../Grammar/LocatedNode.code");
  StatementAttribute.setFunctionSource("SOURCE_STATEMENT_ATTRIBUTE",
                                       "../Grammar/Support.code");
  StatementAttributeList.setFunctionSource("SOURCE_STATEMENT_ATTRIBUTE_LIST",
                                           "../Grammar/Support.code");
  NamespaceSourceFragment.setFunctionSource("SOURCE_NAMESPACE_SOURCE_FRAGMENT",
                                            "../Grammar/LocatedNode.code");

  // DQ (10/6/2008): Moved from SgSupport.
  RenamePair.setFunctionSource("SOURCE_RENAME_PAIR",
                               "../Grammar/LocatedNode.code");

  InitializedName.setFunctionSource("SOURCE_INITIALIZED_NAME",
                                    "../Grammar/Support.code");
  ROSE_ASSERT(InitializedName.associatedGrammar != NULL);

  // ***********************************************************************
  // ***********************************************************************
  //                       OpenMP Clauses
  // ***********************************************************************
  // ***********************************************************************

  // supporting clause nodes
  // declared enum types within SgOmpClause
  OmpClause.setFunctionPrototype("HEADER_OMP_CLAUSE",
                                 "../Grammar/Support.code");
  OmpClause.setFunctionSource("SOURCE_OMP_CLAUSE", "../Grammar/Support.code");
  OmpClause.setDataPrototype("SgOmpClause::omp_directive_name_modifier_enum",
                             "directive_name_modifier",
                             "=e_omp_directive_name_modifier_unspecified",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  // Combined directives distribute clauses across nested semantic statement
  // owners. Preserve their exact source order as one optional typed state,
  // initialized once at producer publication and immutable thereafter.
  OmpClause.setDataPrototype(
      "std::optional<std::size_t>", "combined_source_order", "= std::nullopt",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // clauses with expressions
  OmpExpressionClause.setDataPrototype(
      "SgExpression*", "expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  // The absent and contains clauses carry directive grammar terminals, not
  // value expressions.  Their typed list is required at construction and has
  // no mutable access API, so malformed or late string-backed payloads cannot
  // enter the Sage AST.
  OmpDirectiveKindClause.setFunctionPrototype(
      "HEADER_OMP_DIRECTIVE_KIND_CLAUSE", "../Grammar/Support.code");
  OmpDirectiveKindClause.setFunctionSource("SOURCE_OMP_DIRECTIVE_KIND_CLAUSE",
                                           "../Grammar/Support.code");
  OmpDirectiveKindClause.setDataPrototype(
      "SgOmpClause::omp_directive_kind_list", "directive_kinds", "",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  OmpDestroyClause.setDataPrototype(
      "SgExpression*", "expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpDestroyClause.setFunctionPrototype("HEADER_OMP_DESTROY_CLAUSE",
                                        "../Grammar/Support.code");
  OmpDestroyClause.setFunctionSource("SOURCE_OMP_DESTROY_CLAUSE",
                                     "../Grammar/Support.code");
  OmpAtClause.setDataPrototype("SgOmpClause::omp_at_kind_enum", "kind",
                               "=e_omp_at_unknown", CONSTRUCTOR_PARAMETER,
                               BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpSeverityClause.setDataPrototype(
      "SgOmpClause::omp_severity_kind_enum", "kind", "=e_omp_severity_unknown",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDoacrossClause.setDataPrototype(
      "SgOmpClause::omp_doacross_kind_enum", "kind", "=e_omp_doacross_unknown",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDoacrossClause.setDataPrototype(
      "SgExprListExp*", "expressions", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpOtherwiseClause.setDataPrototype(
      "SgStatement*", "variant_directive", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpInductionClause.setDataPrototype(
      "SgOmpInductionItemPtrList", "items", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  OmpApplyClause.setDataPrototype("std::string", "label", "= \"\"",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);
  OmpApplyClause.setDataPrototype(
      "SgOmpApplyTransformationPtrList", "transformations", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);
  OmpInitClause.setDataPrototype(
      "SgOmpInitModifierList*", "modifier_list", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);
  OmpInitClause.setDataPrototype("SgExpression*", "operand", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
                                 OPTIONAL_TRAVERSAL_MEMBER);

  // schedule([modifier [, modifier]:]kind[, chunk_size])

  OmpScheduleClause.setDataPrototype(
      "SgOmpClause::omp_schedule_modifier_enum", "modifier",
      "=e_omp_schedule_modifier_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpScheduleClause.setDataPrototype(
      "SgOmpClause::omp_schedule_modifier_enum", "modifier1",
      "=e_omp_schedule_modifier_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpScheduleClause.setDataPrototype(
      "SgOmpClause::omp_schedule_kind_enum", "kind",
      "=e_omp_schedule_kind_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpScheduleClause.setDataPrototype(
      "SgExpression*", "chunk_size", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  OmpContextSelectorProperty.setDataPrototype(
      "SgExpression*", "expression", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpContextSelectorProperty.setDataPrototype(
      "SgOmpClause::omp_when_context_kind_enum", "context_kind",
      "= SgOmpClause::e_omp_when_context_kind_unknown",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  OmpContextSelectorProperty.setDataPrototype(
      "SgOmpClause::omp_when_context_vendor_enum", "context_vendor",
      "= SgOmpClause::e_omp_when_context_vendor_unspecified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  OmpContextSelectorProperty.setDataPrototype(
      "SgOmpClause::omp_atomic_default_mem_order_kind_enum",
      "atomic_default_mem_order",
      "= SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  OmpContextSelectorProperty.setDataPrototype(
      "SgOmpClause::omp_requires_property_kind_enum", "requires_kind",
      "= SgOmpClause::e_omp_requires_property_unspecified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  OmpContextSelectorProperty.setDataPrototype(
      "SgExpression*", "requires_expression", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);
  OmpContextSelectorProperty.setDataPrototype(
      "SgOmpClause::omp_atomic_default_mem_order_kind_enum",
      "requires_atomic_default_mem_order",
      "= SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  OmpContextSelectorProperty.setDataPrototype(
      "SgName", "requires_extension", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  OmpContextSelector.setDataPrototype(
      "SgOmpClause::omp_context_trait_selector_kind_enum", "selector_kind",
      "= SgOmpClause::e_omp_context_trait_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpContextSelector.setDataPrototype(
      "SgExpression*", "score", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpContextSelector.setDataPrototype("SgName", "implementation_defined_name",
                                      "= \"\"", NO_CONSTRUCTOR_PARAMETER,
                                      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                      NO_DELETE);
  OmpContextSelector.setDataPrototype(
      "SgStatement*", "construct_directive", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpContextSelector.setDataPrototype(
      "SgOmpContextSelectorPropertyPtrList", "properties", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);
  OmpContextSelectorSet.setDataPrototype(
      "SgOmpClause::omp_context_selector_set_kind_enum", "set_kind",
      "= SgOmpClause::e_omp_context_selector_set_unknown",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpContextSelectorSet.setDataPrototype(
      "SgOmpContextSelectorPtrList", "selectors", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  // A when/match clause owns an ordered list of typed, set-scoped context
  // selectors.  This is the semantic OpenMP representation; no late unparser
  // reconstruction from unrelated singleton fields is permitted.
  OmpWhenClause.setDataPrototype("SgStatement*", "variant_directive", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
                                 OPTIONAL_TRAVERSAL_MEMBER);
  OmpWhenClause.setDataPrototype(
      "SgOmpContextSelectorSetPtrList", "context_selector_sets", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);
  OmpMatchClause.setDataPrototype(
      "SgOmpContextSelectorSetPtrList", "context_selector_sets", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);

  OmpAdjustArgsClause.setDataPrototype(
      "SgExprListExp*", "arguments", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpAdjustArgsClause.setDataPrototype(
      "SgOmpClause::omp_adjust_args_modifier_enum", "modifier",
      "=SgOmpClause::e_omp_adjust_args_modifier_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  OmpAppendArgsClause.setDataPrototype(
      "SgOmpAppendArgsOperationPtrList", "interop_operations", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);

  // clauses with variable lists
  // Using a SgNode for variable list, avoiding mixed container + simple member
  OmpVariablesClause.setDataPrototype(
      "SgExprListExp*", "variables", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  // Source-level list spelling is retained separately when the preprocessor
  // expands one or more macros into the semantic variable list.
  OmpVariablesClause.setDataPrototype(
      "SgExprListExp*", "source_variables", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpVariablesClause.setDataPrototype("bool", "has_source_variables_override",
                                      "= false", NO_CONSTRUCTOR_PARAMETER,
                                      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                      NO_DELETE);
  OmpFirstprivateClause.setDataPrototype(
      "bool", "saved", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // linear (varlist[:step]) varlist may be modifier(list)
  OmpLinearClause.setDataPrototype(
      "SgExpression*", "step", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  OmpLinearClause.setDataPrototype(
      "SgOmpClause::omp_linear_modifier_enum", "modifier",
      "=e_omp_linear_modifier_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // aligned (varlist[:alignment])
  OmpAlignedClause.setDataPrototype(
      "SgExpression*", "alignment", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  // default (private | firstprivate | shared | none)
  OmpDefaultClause.setDataPrototype(
      "SgOmpClause::omp_default_option_enum", "data_sharing",
      "=e_omp_default_unknown", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
  OmpDefaultClause.setDataPrototype(
      "SgStatement*", "variant_directive", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  // allocate (allocator)
  OmpAllocatorClause.setDataPrototype(
      "SgOmpClause::omp_allocator_modifier_enum", "modifier",
      "=e_omp_allocator_modifier_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // allocate (user-defined modifier)
  OmpAllocatorClause.setDataPrototype(
      "SgExpression*", "user_defined_modifier", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  // atomic clause is one of : read, write, update, or capture
  OmpAtomicClause.setDataPrototype(
      "SgOmpClause::omp_atomic_clause_enum", "atomicity",
      "=e_omp_atomic_clause_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // fail(memory-order)
  OmpFailClause.setDataPrototype(
      "SgOmpClause::omp_fail_memory_order_kind_enum", "memory_order",
      "=e_omp_fail_memory_order_kind_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // proc_bind(master | close | spread)
  OmpProcBindClause.setDataPrototype(
      "SgOmpClause::omp_proc_bind_policy_enum", "policy",
      "=e_omp_proc_bind_policy_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // order(concurrent)
  OmpOrderClause.setDataPrototype("SgOmpClause::omp_order_kind_enum", "kind",
                                  "=e_omp_order_kind_unspecified",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);
  OmpOrderClause.setDataPrototype(
      "SgOmpClause::omp_order_modifier_enum", "modifier",
      "=e_omp_order_modifier_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // grainsize([strict:]expr)
  OmpGrainsizeClause.setDataPrototype(
      "SgOmpClause::omp_grainsize_modifier_enum", "modifier",
      "=e_omp_grainsize_modifier_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // num_tasks([strict:]expr)
  OmpNumTasksClause.setDataPrototype(
      "SgOmpClause::omp_num_tasks_modifier_enum", "modifier",
      "=e_omp_num_tasks_modifier_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // dist_schedule(kind[, chunk_size])
  OmpDistScheduleClause.setDataPrototype(
      "SgOmpClause::omp_dist_schedule_kind_enum", "kind",
      "=e_omp_dist_schedule_kind_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDistScheduleClause.setDataPrototype(
      "SgExpression*", "chunk_size", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  // defaultmap(implicit-behavior[:variable-category])
  OmpDefaultmapClause.setDataPrototype(
      "SgOmpClause::omp_defaultmap_behavior_enum", "behavior",
      "=e_omp_defaultmap_behavior_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDefaultmapClause.setDataPrototype(
      "SgOmpClause::omp_defaultmap_category_enum", "category",
      "=e_omp_defaultmap_category_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // ext_implementation_defined_requirement
  OmpExtImplementationDefinedRequirementClause.setDataPrototype(
      "SgExpression*", "implementation_defined_requirement", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);

  // bind(binding)
  OmpBindClause.setDataPrototype("SgOmpClause::omp_bind_binding_enum",
                                 "binding", "=e_omp_bind_binding_unspecified",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);

  // atomic_default_mem_order(seq_cst | acq_rel | relaxed)
  OmpAtomicDefaultMemOrderClause.setDataPrototype(
      "SgOmpClause::omp_atomic_default_mem_order_kind_enum", "kind",
      "=e_omp_atomic_default_mem_order_kind_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // reduction(modifier, identifier : variables)
  OmpReductionClause.setDataPrototype(
      "SgOmpClause::omp_reduction_modifier_enum", "modifier",
      "=e_omp_reduction_modifier_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // reduction(op:variables)
  OmpReductionClause.setDataPrototype(
      "SgOmpClause::omp_reduction_identifier_enum", "identifier",
      "=e_omp_reduction_unknown", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);

  // reduction(modifier, user-defined identifier : variables)
  OmpReductionClause.setDataPrototype(
      "SgOmpNameExpression*", "user_defined_identifier", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);

  // in_reduction(op:variables)
  OmpInReductionClause.setDataPrototype(
      "SgOmpClause::omp_in_reduction_identifier_enum", "identifier",
      "=e_omp_in_reduction_identifier_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // in_reduction(user-defined identifier : variables)
  OmpInReductionClause.setDataPrototype(
      "SgOmpNameExpression*", "user_defined_identifier", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);

  // task_reduction(op:variables)
  OmpTaskReductionClause.setDataPrototype(
      "SgOmpClause::omp_task_reduction_identifier_enum", "identifier",
      "=e_omp_task_reduction_identifier_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // task_reduction(user-defined identifier : variables)
  OmpTaskReductionClause.setDataPrototype(
      "SgOmpNameExpression*", "user_defined_identifier", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);

  // if (modifier : expression)
  OmpIfClause.setDataPrototype("SgOmpClause::omp_if_modifier_enum", "modifier",
                               "=e_omp_if_modifier_unknown",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  // lastprivate (modifier : variable_list)
  OmpLastprivateClause.setDataPrototype(
      "SgOmpClause::omp_lastprivate_modifier_enum", "modifier",
      "=e_omp_lastprivate_modifier_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // device([ device-modifier :] integer-expression)
  OmpDeviceClause.setDataPrototype(
      "SgOmpClause::omp_device_modifier_enum", "modifier",
      "=e_omp_device_modifier_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // allocate (modifier : variables)
  OmpAllocateClause.setDataPrototype(
      "SgOmpClause::omp_allocate_modifier_enum", "modifier",
      "=e_omp_allocate_modifier_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // allocate (user-defined modifier : variables)
  OmpAllocateClause.setDataPrototype(
      "SgExpression*", "user_defined_modifier", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpAllocateClause.setDataPrototype(
      "SgExpression*", "alignment", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpAllocateClause.setDataPrototype("bool", "uses_allocator_modifier_syntax",
                                     "=false", NO_CONSTRUCTOR_PARAMETER,
                                     BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                     NO_DELETE);

  // uses_allocators(allocator[(allocator-traits-array)][,allocator[(allocator-traits-array)]
  // ...])
  OmpUsesAllocatorsClause.setDataPrototype(
      "SgOmpUsesAllocatorsDefinationPtrList", "uses_allocators_defination", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);

  OmpUsesAllocatorsDefination.setDataPrototype(
      "SgOmpClause::omp_uses_allocators_allocator_enum", "allocator",
      "=e_omp_uses_allocators_allocator_unknown", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpUsesAllocatorsDefination.setDataPrototype(
      "SgExpression*", "user_defined_allocator", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);
  OmpUsesAllocatorsDefination.setDataPrototype(
      "SgExpression*", "allocator_traits_array", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);

  // depend(modifier, type:variables)
  OmpDependClause.setDataPrototype(
      "SgOmpClause::omp_depend_modifier_enum", "depend_modifier",
      "=e_omp_depend_modifier_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDependClause.setDataPrototype(
      "SgOmpClause::omp_dependence_type_enum", "dependence_type",
      "=e_omp_depend_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDependClause.setDataPrototype(
      "SgExprListExp*", "sink_vectors", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpDependClause.setDataPrototype(
      "SgOmpIteratorDefinitionPtrList", "iterator_definitions", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);

  // to([mapper(mapper-identifier):]locator-list)
  OmpToClause.setDataPrototype("SgOmpClause::omp_to_kind_enum", "kind",
                               "=e_omp_to_kind_unknown", CONSTRUCTOR_PARAMETER,
                               BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpToClause.setDataPrototype("bool", "declare_target_extended_list",
                               "= false", NO_CONSTRUCTOR_PARAMETER,
                               BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpToClause.setDataPrototype("SgOmpNameExpression*", "mapper_identifier",
                               "= NULL", NO_CONSTRUCTOR_PARAMETER,
                               BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
                               CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);
  OmpToClause.setDataPrototype(
      "SgOmpIteratorDefinitionPtrList", "iterator_definitions", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);

  // from([mapper(mapper-identifier):]locator-list)
  OmpFromClause.setDataPrototype(
      "SgOmpClause::omp_from_kind_enum", "kind", "=e_omp_from_kind_unknown",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpFromClause.setDataPrototype(
      "SgOmpNameExpression*", "mapper_identifier", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);
  OmpFromClause.setDataPrototype(
      "SgOmpIteratorDefinitionPtrList", "iterator_definitions", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);

  // affinity([aff-modifier :] locator-list)
  OmpAffinityClause.setDataPrototype(
      "SgOmpClause::omp_affinity_modifier_enum", "affinity_modifier",
      "=e_omp_affinity_modifier_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpAffinityClause.setDataPrototype(
      "SgOmpIteratorDefinitionPtrList", "iterator_definitions", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);

  // map (inout|alloc|in|out:variable_list) , a variable could be array type
  // with additional dimension info, such as a[0:n][0:m]
  OmpMapClause.setDataPrototype(
      "SgOmpClause::omp_map_operator_enum", "operation", "=e_omp_map_unknown",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpMapClause.setDataPrototype(
      "SgOmpClause::omp_map_modifier_enum", "modifier1",
      "=e_omp_map_modifier_unspecified", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpMapClause.setDataPrototype(
      "SgOmpClause::omp_map_modifier_enum", "modifier2",
      "=e_omp_map_modifier_unspecified", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpMapClause.setDataPrototype(
      "SgOmpClause::omp_map_modifier_enum", "modifier3",
      "=e_omp_map_modifier_unspecified", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpMapClause.setDataPrototype(
      "SgOmpNameExpression*", "mapper_identifier", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);
  OmpMapClause.setDataPrototype(
      "SgOmpIteratorDefinitionPtrList", "iterator_definitions", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);

  // update(in|out|inout|mutexinoutset|depobj)
  OmpDepobjUpdateClause.setDataPrototype(
      "SgOmpClause::omp_depobj_modifier_enum", "modifier",
      "=e_omp_depobj_modifier_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // ***********************************************************************
  // ***********************************************************************
  //                       OpenACC Clauses
  // ***********************************************************************
  // ***********************************************************************

  AccExpressionClause.setDataPrototype(
      "SgExpression*", "expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  AccVariablesClause.setDataPrototype(
      "SgExprListExp*", "variables", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  AccDefaultClause.setDataPrototype(
      "int", "default_kind", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  AccReductionClause.setDataPrototype(
      "int", "reduction_operator", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

} // end
