
#include "AstNodeClass.h"

#include "ROSETTA_macros.h"

#include "rosetta_template_prototype_helpers.h"

#include "grammar.h"

void Grammar::setUpStatements() {
  NEW_TERMINAL_MACRO(ForStatement, "ForStatement", "FOR_STMT");
  NEW_TERMINAL_MACRO(ForInitStatement, "ForInitStatement", "FOR_INIT_STMT");

  NEW_TERMINAL_MACRO(RangeBasedForStatement, "RangeBasedForStatement",
                     "RANGE_BASED_FOR_STMT");

  NEW_TERMINAL_MACRO(CatchStatementSeq, "CatchStatementSeq",
                     "CATCH_STATEMENT_SEQ");
  NEW_TERMINAL_MACRO(FunctionParameterList, "FunctionParameterList",
                     "FUNCTION_PARAMETER_LIST");
  NEW_TERMINAL_MACRO(CtorInitializerList, "CtorInitializerList",
                     "CTOR_INITIALIZER_LIST");
  NEW_TERMINAL_MACRO(BasicBlock, "BasicBlock", "BASIC_BLOCK_STMT");
  NEW_TERMINAL_MACRO(Global, "Global", "GLOBAL_STMT");
  NEW_TERMINAL_MACRO(IfStmt, "IfStmt", "IF_STMT");
  NEW_TERMINAL_MACRO(WhileStmt, "WhileStmt", "WHILE_STMT");
  NEW_TERMINAL_MACRO(DoWhileStmt, "DoWhileStmt", "DO_WHILE_STMT");
  NEW_TERMINAL_MACRO(SwitchStatement, "SwitchStatement", "SWITCH_STMT");
  NEW_TERMINAL_MACRO(CatchOptionStmt, "CatchOptionStmt", "CATCH_STMT");

  NEW_TERMINAL_MACRO(FunctionParameterScope, "FunctionParameterScope",
                     "FUNCTION_PARAMETER_SCOPE");

  NEW_TERMINAL_MACRO(DeclarationScope, "DeclarationScope", "DECLARATION_SCOPE");

  NEW_TERMINAL_MACRO(VariableDefinition, "VariableDefinition", "VAR_DEFN_STMT");
  NEW_TERMINAL_MACRO(EnumDeclaration, "EnumDeclaration", "ENUM_DECL_STMT");

  NEW_TERMINAL_MACRO(AsmStmt, "AsmStmt", "ASM_STMT");
  NEW_TERMINAL_MACRO(FunctionTypeTable, "FunctionTypeTable", "FUNC_TBL_STMT");
  NEW_TERMINAL_MACRO(ExprStatement, "ExprStatement", "EXPR_STMT");
  NEW_TERMINAL_MACRO(LabelStatement, "LabelStatement", "LABEL_STMT");
  NEW_TERMINAL_MACRO(CaseOptionStmt, "CaseOptionStmt", "CASE_STMT");
  NEW_TERMINAL_MACRO(TryStmt, "TryStmt", "TRY_STMT");
  NEW_TERMINAL_MACRO(DefaultOptionStmt, "DefaultOptionStmt", "DEFAULT_STMT");
  NEW_TERMINAL_MACRO(BreakStmt, "BreakStmt", "BREAK_STMT");
  NEW_TERMINAL_MACRO(ContinueStmt, "ContinueStmt", "CONTINUE_STMT");
  // Fortran continue statement has different semantics from C/C++ continue
  // statement [Rasmussen 6/10/2023]
  NEW_TERMINAL_MACRO(FortranContinueStmt, "FortranContinueStmt",
                     "FORTRAN_CONTINUE_STMT");
  NEW_TERMINAL_MACRO(ReturnStmt, "ReturnStmt", "RETURN_STMT");
  NEW_TERMINAL_MACRO(GotoStatement, "GotoStatement", "GOTO_STMT");

  NEW_TERMINAL_MACRO(TemplateTypedefDeclaration, "TemplateTypedefDeclaration",
                     "TEMPLATE_TYPEDEF_DECL_STMT");
  NEW_TERMINAL_MACRO(TemplateInstantiationTypedefDeclaration,
                     "TemplateInstantiationTypedefDeclaration",
                     "TEMPLATE_INST_TYPEDEF_DECL_STMT");
  NEW_NONTERMINAL_MACRO(TypedefDeclaration,
                        TemplateTypedefDeclaration |
                            TemplateInstantiationTypedefDeclaration,
                        "TypedefDeclaration", "TYPEDEF_STMT", true);

  NEW_TERMINAL_MACRO(NullStatement, "NullStatement", "NULL_STMT");

  // Preserve each explicit Clang statement attribute as typed semantic state
  // around its exclusively owned wrapped statement.
  NEW_TERMINAL_MACRO(AttributedStatement, "AttributedStatement",
                     "ATTRIBUTED_STMT");

  NEW_TERMINAL_MACRO(PragmaDeclaration, "PragmaDeclaration", "PRAGMA_DECL");

  NEW_TERMINAL_MACRO(TemplateClassDeclaration, "TemplateClassDeclaration",
                     "TEMPLATE_CLASS_DECL_STMT");
  NEW_TERMINAL_MACRO(TemplateMemberFunctionDeclaration,
                     "TemplateMemberFunctionDeclaration",
                     "TEMPLATE_MEMBER_FUNCTION_DECL_STMT");

  NEW_TERMINAL_MACRO(TemplateFunctionDeclaration, "TemplateFunctionDeclaration",
                     "TEMPLATE_FUNCTION_DECL_STMT");

  NEW_TERMINAL_MACRO(TemplateVariableDeclaration, "TemplateVariableDeclaration",
                     "TEMPLATE_VARIABLE_DECL_STMT");

  NEW_TERMINAL_MACRO(TemplateDeclaration, "TemplateDeclaration",
                     "TEMPLATE_DECL_STMT");

  NEW_NONTERMINAL_MACRO(VariableDeclaration, TemplateVariableDeclaration,
                        "VariableDeclaration", "VAR_DECL_STMT", true);

  NEW_TERMINAL_MACRO(TemplateInstantiationDecl, "TemplateInstantiationDecl",
                     "TEMPLATE_INST_DECL_STMT");
  NEW_TERMINAL_MACRO(TemplateInstantiationDefn, "TemplateInstantiationDefn",
                     "TEMPLATE_INST_DEFN_STMT");
  NEW_TERMINAL_MACRO(TemplateInstantiationFunctionDecl,
                     "TemplateInstantiationFunctionDecl",
                     "TEMPLATE_INST_FUNCTION_DECL_STMT");
  NEW_TERMINAL_MACRO(TemplateInstantiationMemberFunctionDecl,
                     "TemplateInstantiationMemberFunctionDecl",
                     "TEMPLATE_INST_MEMBER_FUNCTION_DECL_STMT");

  NEW_TERMINAL_MACRO(NonrealDecl, "NonrealDecl", "NONREAL_DECL");

  NEW_TERMINAL_MACRO(ProgramHeaderStatement, "ProgramHeaderStatement",
                     "PROGRAM_HEADER_STMT");
  NEW_TERMINAL_MACRO(ProcedureHeaderStatement, "ProcedureHeaderStatement",
                     "PROCEDURE_HEADER_STMT");
  NEW_TERMINAL_MACRO(EntryStatement, "EntryStatement", "ENTRY_STMT");
  NEW_TERMINAL_MACRO(InterfaceStatement, "InterfaceStatement",
                     "INTERFACE_STATEMENT");
  NEW_TERMINAL_MACRO(CommonBlock, "CommonBlock", "COMMON_BLOCK");
  NEW_TERMINAL_MACRO(ModuleStatement, "ModuleStatement", "MODULE_STATEMENT");
  NEW_TERMINAL_MACRO(UseStatement, "UseStatement", "USE_STATEMENT");
  NEW_TERMINAL_MACRO(ProcessControlStatement, "ProcessControlStatement",
                     "PROCESS_CONTROL_STATEMENT");

  NEW_TERMINAL_MACRO(PrintStatement, "PrintStatement", "PRINT_STATEMENT");
  NEW_TERMINAL_MACRO(ReadStatement, "ReadStatement", "READ_STATEMENT");
  NEW_TERMINAL_MACRO(WriteStatement, "WriteStatement", "WRITE_STATEMENT");
  NEW_TERMINAL_MACRO(OpenStatement, "OpenStatement", "OPEN_STATEMENT");
  NEW_TERMINAL_MACRO(CloseStatement, "CloseStatement", "CLOSE_STATEMENT");
  NEW_TERMINAL_MACRO(InquireStatement, "InquireStatement", "INQUIRE_STATEMENT");
  NEW_TERMINAL_MACRO(FlushStatement, "FlushStatement", "FLUSH_STATEMENT");
  NEW_TERMINAL_MACRO(BackspaceStatement, "BackspaceStatement",
                     "BACKSPACE_STATEMENT");
  NEW_TERMINAL_MACRO(RewindStatement, "RewindStatement", "REWIND_STATEMENT");
  NEW_TERMINAL_MACRO(EndfileStatement, "EndfileStatement", "ENDFILE_STATEMENT");
  NEW_TERMINAL_MACRO(WaitStatement, "WaitStatement", "WAIT_STATEMENT");

  NEW_TERMINAL_MACRO(FormatStatement, "FormatStatement", "FORMAT_STATEMENT");

  NEW_TERMINAL_MACRO(FortranDo, "FortranDo", "FORTRAN_DO");

  NEW_TERMINAL_MACRO(ForAllStatement, "ForAllStatement", "FOR_ALL_STMT");

  NEW_NONTERMINAL_MACRO(IOStatement,
                        PrintStatement | ReadStatement | WriteStatement |
                            OpenStatement | CloseStatement | InquireStatement |
                            FlushStatement | BackspaceStatement |
                            RewindStatement | EndfileStatement | WaitStatement,
                        "IOStatement", "IO_STATEMENT", false);

  NEW_TERMINAL_MACRO(SyncAllStatement, "SyncAllStatement",
                     "SYNC_ALL_STATEMENT");
  NEW_TERMINAL_MACRO(SyncImagesStatement, "SyncImagesStatement",
                     "SYNC_IMAGES_STATEMENT");
  NEW_TERMINAL_MACRO(SyncMemoryStatement, "SyncMemoryStatement",
                     "SYNC_MEMORY_STATEMENT");
  NEW_TERMINAL_MACRO(SyncTeamStatement, "SyncTeamStatement",
                     "SYNC_TEAM_STATEMENT");
  NEW_TERMINAL_MACRO(FormTeamStatement, "FormTeamStatement",
                     "FORM_TEAM_STATEMENT");
  NEW_TERMINAL_MACRO(LockStatement, "LockStatement", "LOCK_STATEMENT");
  NEW_TERMINAL_MACRO(UnlockStatement, "UnlockStatement", "UNLOCK_STATEMENT");

  NEW_NONTERMINAL_MACRO(ImageControlStatement,
                        SyncAllStatement | SyncImagesStatement |
                            SyncMemoryStatement | SyncTeamStatement |
                            FormTeamStatement | LockStatement | UnlockStatement,
                        "ImageControlStatement", "IMAGE_CONTROL_STATEMENT",
                        false);

  NEW_TERMINAL_MACRO(OmpParallelStatement, "OmpParallelStatement",
                     "OMP_PARALLEL_STMT");
  NEW_TERMINAL_MACRO(OmpLoopStatement, "OmpLoopStatement", "OMP_LOOP_STMT");
  NEW_TERMINAL_MACRO(OmpScanStatement, "OmpScanStatement", "OMP_SCAN_STMT");
  NEW_TERMINAL_MACRO(OmpTaskloopStatement, "OmpTaskloopStatement",
                     "OMP_TASKLOOP_STMT");
  NEW_TERMINAL_MACRO(OmpMaskedStatement, "OmpMaskedStatement",
                     "OMP_MASKED_STMT");
  NEW_TERMINAL_MACRO(OmpMaskedTaskloopStatement, "OmpMaskedTaskloopStatement",
                     "OMP_MASKED_TASKLOOP_STMT");
  NEW_TERMINAL_MACRO(OmpMaskedTaskloopSimdStatement,
                     "OmpMaskedTaskloopSimdStatement",
                     "OMP_MASKED_TASKLOOP_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpTaskgroupStatement, "OmpTaskgroupStatement",
                     "OMP_TASKGROUP_STMT");
  NEW_TERMINAL_MACRO(OmpDepobjStatement, "OmpDepobjStatement",
                     "OMP_DEPOBJ_STMT");
  NEW_TERMINAL_MACRO(OmpDispatchStatement, "OmpDispatchStatement",
                     "OMP_DISPATCH_STMT");
  NEW_TERMINAL_MACRO(OmpTeamsStatement, "OmpTeamsStatement", "OMP_TEAMS_STMT");
  NEW_TERMINAL_MACRO(OmpCancellationPointStatement,
                     "OmpCancellationPointStatement",
                     "OMP_CANCELLATION_POINT_STMT");
  NEW_TERMINAL_MACRO(OmpDeclareMapperStatement, "OmpDeclareMapperStatement",
                     "OMP_DECLARE_MAPPER_STMT");
  NEW_TERMINAL_MACRO(OmpCancelStatement, "OmpCancelStatement",
                     "OMP_CANCEL_STMT");
  NEW_TERMINAL_MACRO(OmpDistributeStatement, "OmpDistributeStatement",
                     "OMP_DISTRIBUTE_STMT");
  NEW_TERMINAL_MACRO(OmpWorkdistributeStatement, "OmpWorkdistributeStatement",
                     "OMP_WORKDISTRIBUTE_STMT");
  NEW_TERMINAL_MACRO(OmpMetadirectiveStatement, "OmpMetadirectiveStatement",
                     "OMP_METADIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(OmpSingleStatement, "OmpSingleStatement",
                     "OMP_SINGLE_STMT");
  NEW_TERMINAL_MACRO(OmpTaskStatement, "OmpTaskStatement", "OMP_TASK_STMT");
  NEW_TERMINAL_MACRO(OmpTargetEnterDataStatement, "OmpTargetEnterDataStatement",
                     "OMP_TARGET_ENTER_DATA_STMT");
  NEW_TERMINAL_MACRO(OmpTargetExitDataStatement, "OmpTargetExitDataStatement",
                     "OMP_TARGET_EXIT_DATA_STMT");
  NEW_TERMINAL_MACRO(OmpForSimdStatement, "OmpForSimdStatement",
                     "OMP_FOR_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpForStatement, "OmpForStatement", "OMP_FOR_STMT");
  NEW_TERMINAL_MACRO(OmpDoStatement, "OmpDoStatement", "OMP_DO_STMT");
  NEW_TERMINAL_MACRO(OmpSectionsStatement, "OmpSectionsStatement",
                     "OMP_SECTIONS_STMT");
  // OpenMP 4.x : atomic can have clause now.
  NEW_TERMINAL_MACRO(OmpAtomicStatement, "OmpAtomicStatement",
                     "OMP_ATOMIC_STMT");
  // use for testing OpenMP accelerator model
  NEW_TERMINAL_MACRO(OmpTargetStatement, "OmpTargetStatement",
                     "OMP_TARGET_STMT");
  NEW_TERMINAL_MACRO(OmpTargetDataStatement, "OmpTargetDataStatement",
                     "OMP_TARGET_DATA_STMT");
  NEW_TERMINAL_MACRO(OmpTargetDataCompositeStatement,
                     "OmpTargetDataCompositeStatement",
                     "OMP_TARGET_DATA_COMPOSITE_STMT");
  NEW_TERMINAL_MACRO(OmpTargetParallelForStatement,
                     "OmpTargetParallelForStatement",
                     "OMP_TARGET_PARALLEL_FOR_STMT");
  NEW_TERMINAL_MACRO(OmpTargetParallelStatement, "OmpTargetParallelStatement",
                     "OMP_TARGET_PARALLEL_STMT");
  NEW_TERMINAL_MACRO(OmpDistributeSimdStatement, "OmpDistributeSimdStatement",
                     "OMP_DISTRIBUTE_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpDistributeParallelForStatement,
                     "OmpDistributeParallelForStatement",
                     "OMP_DISTRIBUTE_PARALLEL_FOR_STMT");
  NEW_TERMINAL_MACRO(OmpDistributeParallelForSimdStatement,
                     "OmpDistributeParallelForSimdStatement",
                     "OMP_DISTRIBUTE_PARALLEL_FOR_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpTaskloopSimdStatement, "OmpTaskloopSimdStatement",
                     "OMP_TASKLOOP_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpTargetUpdateStatement, "OmpTargetUpdateStatement",
                     "OMP_TARGET_UPDATE_STMT");
  NEW_TERMINAL_MACRO(OmpTargetParallelForSimdStatement,
                     "OmpTargetParallelForSimdStatement",
                     "OMP_TARGET_PARALLEL_FOR_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpTargetParallelLoopStatement,
                     "OmpTargetParallelLoopStatement",
                     "OMP_TARGET_PARALLEL_LOOP_STMT");
  NEW_TERMINAL_MACRO(OmpTargetSimdStatement, "OmpTargetSimdStatement",
                     "OMP_TARGET_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpTargetTeamsStatement, "OmpTargetTeamsStatement",
                     "OMP_TARGET_TEAMS_STMT");
  NEW_TERMINAL_MACRO(OmpTargetTeamsDistributeStatement,
                     "OmpTargetTeamsDistributeStatement",
                     "OMP_TARGET_TEAMS_DISTRIBUTE_STMT");
  NEW_TERMINAL_MACRO(OmpTargetTeamsWorkdistributeStatement,
                     "OmpTargetTeamsWorkdistributeStatement",
                     "OMP_TARGET_TEAMS_WORKDISTRIBUTE_STMT");
  NEW_TERMINAL_MACRO(OmpTargetTeamsDistributeSimdStatement,
                     "OmpTargetTeamsDistributeSimdStatement",
                     "OMP_TARGET_TEAMS_DISTRIBUTE_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpTargetTeamsLoopStatement, "OmpTargetTeamsLoopStatement",
                     "OMP_TARGET_TEAMS_LOOP_STMT");
  NEW_TERMINAL_MACRO(OmpTargetTeamsDistributeParallelForStatement,
                     "OmpTargetTeamsDistributeParallelForStatement",
                     "OMP_TARGET_TEAMS_DISTRIBUTE_PARALLEL_FOR_STMT");
  NEW_TERMINAL_MACRO(OmpTargetTeamsDistributeParallelForSimdStatement,
                     "OmpTargetTeamsDistributeParallelForSimdStatement",
                     "OMP_TARGET_TEAMS_DISTRIBUTE_PARALLEL_FOR_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpMasterTaskloopSimdStatement,
                     "OmpMasterTaskloopSimdStatement",
                     "OMP_MASTER_TASKLOOP_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpParallelMasterTaskloopStatement,
                     "OmpParallelMasterTaskloopStatement",
                     "OMP_PARALLEL_MASTER_TASKLOOP_STMT");
  NEW_TERMINAL_MACRO(OmpParallelMasterTaskloopSimdStatement,
                     "OmpParallelMasterTaskloopSimdStatement",
                     "OMP_PARALLEL_MASTER_TASKLOOP_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpTeamsDistributeStatement, "OmpTeamsDistributeStatement",
                     "OMP_TEAMS_DISTRIBUTE_STMT");
  NEW_TERMINAL_MACRO(OmpTeamsDistributeSimdStatement,
                     "OmpTeamsDistributeSimdStatement",
                     "OMP_TEAMS_DISTRIBUTE_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpTeamsDistributeParallelForStatement,
                     "OmpTeamsDistributeParallelForStatement",
                     "OMP_TEAMS_DISTRIBUTE_PARALLEL_FOR_STMT");
  NEW_TERMINAL_MACRO(OmpTeamsDistributeParallelForSimdStatement,
                     "OmpTeamsDistributeParallelForSimdStatement",
                     "OMP_TEAMS_DISTRIBUTE_PARALLEL_FOR_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpTeamsLoopStatement, "OmpTeamsLoopStatement",
                     "OMP_TEAMS_LOOP_STMT");
  NEW_TERMINAL_MACRO(OmpParallelMasterStatement, "OmpParallelMasterStatement",
                     "OMP_PARALLEL_MASTER_STMT");
  NEW_TERMINAL_MACRO(OmpMasterTaskloopStatement, "OmpMasterTaskloopStatement",
                     "OMP_MASTER_TASKLOOP_STMT");
  NEW_TERMINAL_MACRO(OmpParallelLoopStatement, "OmpParallelLoopStatement",
                     "OMP_PARALLEL_LOOP_STMT");

  // OpenMP 5.1--6.0 structured and loop-transformation directives.  Keep
  // these as distinct terminals so their semantics never depend on source
  // text or an out-of-band spelling attribute.
  NEW_TERMINAL_MACRO(OmpScopeStatement, "OmpScopeStatement", "OMP_SCOPE_STMT");
  NEW_TERMINAL_MACRO(OmpParallelMaskedStatement, "OmpParallelMaskedStatement",
                     "OMP_PARALLEL_MASKED_STMT");
  NEW_TERMINAL_MACRO(OmpAssumeStatement, "OmpAssumeStatement",
                     "OMP_ASSUME_STMT");
  NEW_TERMINAL_MACRO(OmpTaskgraphStatement, "OmpTaskgraphStatement",
                     "OMP_TASKGRAPH_STMT");
  NEW_TERMINAL_MACRO(OmpFuseStatement, "OmpFuseStatement", "OMP_FUSE_STMT");
  NEW_TERMINAL_MACRO(OmpInterchangeStatement, "OmpInterchangeStatement",
                     "OMP_INTERCHANGE_STMT");
  NEW_TERMINAL_MACRO(OmpReverseStatement, "OmpReverseStatement",
                     "OMP_REVERSE_STMT");

  NEW_TERMINAL_MACRO(OmpUnrollStatement, "OmpUnrollStatement",
                     "OMP_UNROLL_STMT");
  NEW_TERMINAL_MACRO(OmpTileStatement, "OmpTileStatement", "OMP_TILE_STMT");

  NEW_TERMINAL_MACRO(OmpSimdStatement, "OmpSimdStatement", "OMP_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpCriticalStatement, "OmpCriticalStatement",
                     "OMP_CRITICAL_STMT");
  NEW_TERMINAL_MACRO(OmpOrderedStatement, "OmpOrderedStatement",
                     "OMP_ORDERED_STMT");

  // A base class for the most commonly formed directives with both clauses and
  // a structured body We treat OmpSectionsStatement separatedly by move the
  // body to a list of SgOmpSectionStatement sensitive to
  NEW_NONTERMINAL_MACRO(
      OmpClauseBodyStatement,
      OmpParallelStatement | OmpTeamsStatement | OmpSingleStatement |
          OmpAtomicStatement | OmpMetadirectiveStatement | OmpLoopStatement |
          OmpMaskedStatement | OmpMaskedTaskloopStatement |
          OmpMaskedTaskloopSimdStatement | OmpOrderedStatement |
          OmpTaskgroupStatement | OmpDispatchStatement | OmpTaskloopStatement |
          OmpParallelMasterStatement | OmpMasterTaskloopStatement |
          OmpTaskStatement | OmpForStatement | OmpDoStatement |
          OmpSectionsStatement | OmpTargetStatement | OmpTargetDataStatement |
          OmpTargetParallelForStatement | OmpParallelLoopStatement |
          OmpTargetParallelStatement | OmpTargetParallelForSimdStatement |
          OmpTargetParallelLoopStatement | OmpTargetSimdStatement |
          OmpTargetTeamsStatement | OmpTargetTeamsDistributeStatement |
          OmpTargetTeamsWorkdistributeStatement |
          OmpTargetTeamsDistributeSimdStatement | OmpTargetTeamsLoopStatement |
          OmpTargetTeamsDistributeParallelForStatement |
          OmpTargetTeamsDistributeParallelForSimdStatement |
          OmpDistributeSimdStatement | OmpDistributeParallelForStatement |
          OmpDistributeParallelForSimdStatement | OmpTaskloopSimdStatement |
          OmpMasterTaskloopSimdStatement | OmpParallelMasterTaskloopStatement |
          OmpParallelMasterTaskloopSimdStatement | OmpTeamsDistributeStatement |
          OmpTeamsDistributeSimdStatement |
          OmpTeamsDistributeParallelForStatement |
          OmpTeamsDistributeParallelForSimdStatement | OmpTeamsLoopStatement |
          OmpSimdStatement | OmpForSimdStatement | OmpCriticalStatement |
          OmpDistributeStatement | OmpWorkdistributeStatement |
          OmpUnrollStatement | OmpTileStatement |
          OmpTargetDataCompositeStatement | OmpScopeStatement |
          OmpParallelMaskedStatement | OmpAssumeStatement |
          OmpTaskgraphStatement | OmpFuseStatement | OmpInterchangeStatement |
          OmpReverseStatement,
      "OmpClauseBodyStatement", "OMP_CLAUSEBODY_STMT", false);

  // + a statement / block
  NEW_TERMINAL_MACRO(OmpMasterStatement, "OmpMasterStatement",
                     "OMP_MASTER_STMT");
  NEW_TERMINAL_MACRO(OmpSectionStatement, "OmpSectionStatement",
                     "OMP_SECTION_STMT"); // need this?
  NEW_TERMINAL_MACRO(OmpWorkshareStatement, "OmpWorkshareStatement",
                     "OMP_WORKSHARE_STMT");
  NEW_TERMINAL_MACRO(OmpFlushStatement, "OmpFlushStatement", "OMP_FLUSH_STMT");
  NEW_TERMINAL_MACRO(OmpAllocateStatement, "OmpAllocateStatement",
                     "OMP_ALLOCATE_STMT");
  NEW_TERMINAL_MACRO(OmpOrderedDependStatement, "OmpOrderedDependStatement",
                     "OMP_ORDERED_DEPEND_STMT");
  NEW_TERMINAL_MACRO(OmpErrorStatement, "OmpErrorStatement", "OMP_ERROR_STMT");
  NEW_TERMINAL_MACRO(OmpInteropStatement, "OmpInteropStatement",
                     "OMP_INTEROP_STMT");
  // + stmt/block + name

  NEW_NONTERMINAL_MACRO(OmpBodyStatement,
                        OmpMasterStatement | OmpSectionStatement |
                            OmpWorkshareStatement | OmpClauseBodyStatement,
                        "OmpBodyStatement", "OMP_BODY_STMT", false);

  NEW_NONTERMINAL_MACRO(
      OmpClauseStatement,
      OmpCancelStatement | OmpCancellationPointStatement |
          OmpTargetUpdateStatement | OmpFlushStatement | OmpAllocateStatement |
          OmpOrderedDependStatement | OmpErrorStatement | OmpInteropStatement |
          OmpScanStatement | OmpDepobjStatement | OmpTargetEnterDataStatement |
          OmpTargetExitDataStatement,
      "OmpClauseStatement", "OMP_CLAUSE_STMT", false);

  // ***********************************************************************
  // ***********************************************************************
  //                       OpenACC Nodes
  // ***********************************************************************
  // ***********************************************************************

  NEW_TERMINAL_MACRO(AccParallelStatement, "AccParallelStatement",
                     "ACC_PARALLEL_STMT");
  NEW_TERMINAL_MACRO(AccParallelLoopStatement, "AccParallelLoopStatement",
                     "ACC_PARALLEL_LOOP_STMT");
  NEW_TERMINAL_MACRO(AccDataStatement, "AccDataStatement", "ACC_DATA_STMT");
  NEW_TERMINAL_MACRO(AccKernelsStatement, "AccKernelsStatement",
                     "ACC_KERNELS_STMT");
  NEW_TERMINAL_MACRO(AccAtomicStatement, "AccAtomicStatement",
                     "ACC_ATOMIC_STMT");
  NEW_TERMINAL_MACRO(AccEnterDataStatement, "AccEnterDataStatement",
                     "ACC_ENTER_DATA_STMT");
  NEW_TERMINAL_MACRO(AccExitDataStatement, "AccExitDataStatement",
                     "ACC_EXIT_DATA_STMT");
  NEW_TERMINAL_MACRO(AccRoutineStatement, "AccRoutineStatement",
                     "ACC_ROUTINE_STMT");
  NEW_TERMINAL_MACRO(AccWaitStatement, "AccWaitStatement", "ACC_WAIT_STMT");
  NEW_TERMINAL_MACRO(AccCacheStatement, "AccCacheStatement", "ACC_CACHE_STMT");

  NEW_NONTERMINAL_MACRO(AccClauseBodyStatement,
                        AccParallelStatement | AccParallelLoopStatement |
                            AccDataStatement | AccKernelsStatement |
                            AccAtomicStatement,
                        "AccClauseBodyStatement", "ACC_CLAUSEBODY_STMT", false);

  NEW_NONTERMINAL_MACRO(AccClauseStatement,
                        AccEnterDataStatement | AccExitDataStatement |
                            AccRoutineStatement | AccWaitStatement |
                            AccCacheStatement,
                        "AccClauseStatement", "ACC_CLAUSE_STMT", false);

  NEW_NONTERMINAL_MACRO(AccBodyStatement, AccClauseBodyStatement,
                        "AccBodyStatement", "ACC_BODY_STMT", false);

  NEW_TERMINAL_MACRO(ImplicitStatement, "ImplicitStatement",
                     "TEMP_Implicit_Statement");
  NEW_TERMINAL_MACRO(StatementFunctionStatement, "StatementFunctionStatement",
                     "TEMP_Statement_Function_Statement");
  NEW_TERMINAL_MACRO(WhereStatement, "WhereStatement", "TEMP_Where_Statement");
  NEW_TERMINAL_MACRO(NullifyStatement, "NullifyStatement",
                     "TEMP_Nullify_Statement");
  NEW_TERMINAL_MACRO(EquivalenceStatement, "EquivalenceStatement",
                     "TEMP_Equivalence_Statement");
  NEW_TERMINAL_MACRO(DerivedTypeStatement, "DerivedTypeStatement",
                     "TEMP_Derived_Type_Statement");

  NEW_TERMINAL_MACRO(AttributeSpecificationStatement,
                     "AttributeSpecificationStatement",
                     "TEMP_Attribute_Specification_Statement");
  NEW_TERMINAL_MACRO(AllocateStatement, "AllocateStatement",
                     "TEMP_Allocate_Statement");
  NEW_TERMINAL_MACRO(DeallocateStatement, "DeallocateStatement",
                     "TEMP_Deallocate_Statement");
  NEW_TERMINAL_MACRO(ContainsStatement, "ContainsStatement",
                     "TEMP_Contains_Statement");

  NEW_TERMINAL_MACRO(ElseWhereStatement, "ElseWhereStatement",
                     "TEMP_Else_Where_Statement");

  // Required for F77 support (but deprecated in F90)
  NEW_TERMINAL_MACRO(ArithmeticIfStatement, "ArithmeticIfStatement",
                     "TEMP_Arithmetic_If_Statement");
  NEW_TERMINAL_MACRO(AssignStatement, "AssignStatement",
                     "TEMP_Assign_Statement");
  NEW_TERMINAL_MACRO(ComputedGotoStatement, "ComputedGotoStatement",
                     "TEMP_Computed_Goto_Statement");
  NEW_TERMINAL_MACRO(AssignedGotoStatement, "AssignedGotoStatement",
                     "TEMP_Assigned_Goto_Statement");

  NEW_TERMINAL_MACRO(NamelistStatement, "NamelistStatement",
                     "TEMP_Namelist_Statement");
  NEW_TERMINAL_MACRO(ImportStatement, "ImportStatement",
                     "TEMP_Import_Statement");

  NEW_TERMINAL_MACRO(AssociateStatement, "AssociateStatement",
                     "TEMP_Associate_Statement");

  // DQ (2/18/2008): Added support for language specific Fortran include line
  // (not a statement in Fortran, but a declaration statement in ROSE similar to
  // the CPP IncludeDirectiveStatement).
  NEW_TERMINAL_MACRO(FortranIncludeLine, "FortranIncludeLine",
                     "TEMP_Fortran_Include_Line");

  NEW_TERMINAL_MACRO(NamespaceDeclarationStatement,
                     "NamespaceDeclarationStatement",
                     "NAMESPACE_DECLARATION_STMT");
  NEW_TERMINAL_MACRO(NamespaceAliasDeclarationStatement,
                     "NamespaceAliasDeclarationStatement",
                     "NAMESPACE_ALIAS_DECLARATION_STMT");
  NEW_TERMINAL_MACRO(NamespaceDefinitionStatement,
                     "NamespaceDefinitionStatement",
                     "NAMESPACE_DEFINITION_STMT");
  NEW_TERMINAL_MACRO(UsingDeclarationStatement, "UsingDeclarationStatement",
                     "USING_DECLARATION_STMT");
  NEW_TERMINAL_MACRO(UsingDirectiveStatement, "UsingDirectiveStatement",
                     "USING_DIRECTIVE_STMT");

  NEW_TERMINAL_MACRO(TemplateInstantiationDirectiveStatement,
                     "TemplateInstantiationDirectiveStatement",
                     "TEMPLATE_INST_DIRECTIVE_STMT");

  NEW_TERMINAL_MACRO(StaticAssertionDeclaration, "StaticAssertionDeclaration",
                     "STATIC_ASSERTION_DECLARATION");

  // C++11 type-only friend declarations, such as `friend T;`, name a type
  // without declaring that type.  They therefore cannot be represented by a
  // class, typedef, variable, or other declaration node.
  NEW_TERMINAL_MACRO(FriendTypeDeclaration, "FriendTypeDeclaration",
                     "FRIEND_TYPE_DECLARATION");

  NEW_TERMINAL_MACRO(TemplateClassDefinition, "TemplateClassDefinition",
                     "TEMPLATE_CLASS_DEF_STMT");
  NEW_TERMINAL_MACRO(TemplateFunctionDefinition, "TemplateFunctionDefinition",
                     "TEMPLATE_FUNCTION_DEF_STMT");

  NEW_NONTERMINAL_MACRO(ClassDeclaration,
                        TemplateClassDeclaration | TemplateInstantiationDecl |
                            DerivedTypeStatement | ModuleStatement,
                        "ClassDeclaration", "CLASS_DECL_STMT", true);
  NEW_NONTERMINAL_MACRO(ClassDefinition,
                        TemplateInstantiationDefn | TemplateClassDefinition,
                        "ClassDefinition", "CLASS_DEFN_STMT", true);

  NEW_NONTERMINAL_MACRO(FunctionDefinition, TemplateFunctionDefinition,
                        "FunctionDefinition", "FUNC_DEFN_STMT", true);

  NEW_NONTERMINAL_MACRO(
      ScopeStatement,
      Global | BasicBlock | IfStmt | ForStatement | FunctionDefinition |
          ClassDefinition | WhileStmt | DoWhileStmt | SwitchStatement |
          CatchOptionStmt | NamespaceDefinitionStatement | AssociateStatement |
          FortranDo | ForAllStatement | FunctionParameterScope |
          DeclarationScope | RangeBasedForStatement,
      "ScopeStatement", "SCOPE_STMT", false);

  NEW_NONTERMINAL_MACRO(MemberFunctionDeclaration,
                        TemplateMemberFunctionDeclaration |
                            TemplateInstantiationMemberFunctionDecl,
                        "MemberFunctionDeclaration", "MFUNC_DECL_STMT", true);

  NEW_NONTERMINAL_MACRO(
      FunctionDeclaration,
      TemplateFunctionDeclaration | MemberFunctionDeclaration |
          TemplateInstantiationFunctionDecl | ProgramHeaderStatement |
          ProcedureHeaderStatement | EntryStatement,
      "FunctionDeclaration", "FUNC_DECL_STMT", true);

  NEW_TERMINAL_MACRO(IncludeDirectiveStatement, "IncludeDirectiveStatement",
                     "INCLUDE_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(DefineDirectiveStatement, "DefineDirectiveStatement",
                     "DEFINE_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(UndefDirectiveStatement, "UndefDirectiveStatement",
                     "UNDEF_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(IfdefDirectiveStatement, "IfdefDirectiveStatement",
                     "IFDEF_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(IfndefDirectiveStatement, "IfndefDirectiveStatement",
                     "IFNDEF_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(IfDirectiveStatement, "IfDirectiveStatement",
                     "IF_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(DeadIfDirectiveStatement, "DeadIfDirectiveStatement",
                     "DEAD_IF_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(ElseDirectiveStatement, "ElseDirectiveStatement",
                     "ELSE_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(ElseifDirectiveStatement, "ElseifDirectiveStatement",
                     "ELSEIF_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(EndifDirectiveStatement, "EndifDirectiveStatement",
                     "ENDIF_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(LineDirectiveStatement, "LineDirectiveStatement",
                     "LINE_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(WarningDirectiveStatement, "WarningDirectiveStatement",
                     "WARNING_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(ErrorDirectiveStatement, "ErrorDirectiveStatement",
                     "ERROR_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(EmptyDirectiveStatement, "EmptyDirectiveStatement",
                     "EMPTY_DIRECTIVE_STMT");

  NEW_TERMINAL_MACRO(IncludeNextDirectiveStatement,
                     "IncludeNextDirectiveStatement",
                     "INCLUDE_NEXT_DIRECTIVE_STMT");
  NEW_TERMINAL_MACRO(IdentDirectiveStatement, "IdentDirectiveStatement",
                     "IDENT_DIRECTIVE_STMT");

  // Note that this IR nodes is critical to Fortran support for CPP processed
  // generated code.
  NEW_TERMINAL_MACRO(LinemarkerDirectiveStatement,
                     "LinemarkerDirectiveStatement",
                     "LINEMARKER_DIRECTIVE_STMT");

  NEW_NONTERMINAL_MACRO(
      C_PreprocessorDirectiveStatement,
      IncludeDirectiveStatement | DefineDirectiveStatement |
          UndefDirectiveStatement | IfdefDirectiveStatement |
          IfndefDirectiveStatement | IfDirectiveStatement |
          DeadIfDirectiveStatement | ElseDirectiveStatement |
          ElseifDirectiveStatement | EndifDirectiveStatement |
          LineDirectiveStatement | WarningDirectiveStatement |
          ErrorDirectiveStatement | EmptyDirectiveStatement |
          IncludeNextDirectiveStatement | IdentDirectiveStatement |
          LinemarkerDirectiveStatement,
      "C_PreprocessorDirectiveStatement", "CPP_DIRECTIVE_STMT", false);

  NEW_TERMINAL_MACRO(ClinkageStartStatement, "ClinkageStartStatement",
                     "C_LINKAGE_START_STMT");

  NEW_TERMINAL_MACRO(ClinkageEndStatement, "ClinkageEndStatement",
                     "C_LINKAGE_END_STMT");

  NEW_NONTERMINAL_MACRO(ClinkageDeclarationStatement,
                        ClinkageStartStatement | ClinkageEndStatement,
                        "ClinkageDeclarationStatement",
                        "C_LINKAGE_DECLARATION_STMT", false);

  // + variable list
  NEW_TERMINAL_MACRO(OmpDeclareSimdStatement, "OmpDeclareSimdStatement",
                     "OMP_DECLARE_SIMD_STMT");
  NEW_TERMINAL_MACRO(OmpDeclareVariantStatement, "OmpDeclareVariantStatement",
                     "OMP_DECLARE_VARIANT_STMT");
  NEW_TERMINAL_MACRO(OmpBeginDeclareVariantStatement,
                     "OmpBeginDeclareVariantStatement",
                     "OMP_BEGIN_DECLARE_VARIANT_STMT");
  NEW_TERMINAL_MACRO(OmpEndDeclareVariantStatement,
                     "OmpEndDeclareVariantStatement",
                     "OMP_END_DECLARE_VARIANT_STMT");
  NEW_TERMINAL_MACRO(OmpDeclareTargetStatement, "OmpDeclareTargetStatement",
                     "OMP_DECLARE_TARGET_STMT");
  NEW_TERMINAL_MACRO(OmpEndDeclareTargetStatement,
                     "OmpEndDeclareTargetStatement",
                     "OMP_END_DECLARE_TARGET_STMT");
  // simplest directives, just one line
  NEW_TERMINAL_MACRO(OmpBarrierStatement, "OmpBarrierStatement",
                     "OMP_BARRIER_STMT");
  NEW_TERMINAL_MACRO(OmpNothingStatement, "OmpNothingStatement",
                     "OMP_NOTHING_STMT");
  NEW_TERMINAL_MACRO(OmpTaskyieldStatement, "OmpTaskyieldStatement",
                     "OMP_TASKYIELD_STMT");
  NEW_TERMINAL_MACRO(OmpRequiresStatement, "OmpRequiresStatement",
                     "OMP_REQUIRES_STMT");
  NEW_TERMINAL_MACRO(OmpTaskwaitStatement, "OmpTaskwaitStatement",
                     "OMP_TASKWAIT_STMT");
  // + variable list
  NEW_TERMINAL_MACRO(OmpThreadprivateStatement, "OmpThreadprivateStatement",
                     "OMP_THREADPRIVATE_STMT");
  NEW_TERMINAL_MACRO(OmpBeginDeclareTargetStatement,
                     "OmpBeginDeclareTargetStatement",
                     "OMP_BEGIN_DECLARE_TARGET_STMT");
  NEW_TERMINAL_MACRO(OmpAssumesStatement, "OmpAssumesStatement",
                     "OMP_ASSUMES_STMT");
  NEW_TERMINAL_MACRO(OmpBeginAssumesStatement, "OmpBeginAssumesStatement",
                     "OMP_BEGIN_ASSUMES_STMT");
  NEW_TERMINAL_MACRO(OmpEndAssumesStatement, "OmpEndAssumesStatement",
                     "OMP_END_ASSUMES_STMT");
  NEW_TERMINAL_MACRO(OmpEndAssumeStatement, "OmpEndAssumeStatement",
                     "OMP_END_ASSUME_STMT");
  NEW_TERMINAL_MACRO(OmpGroupprivateStatement, "OmpGroupprivateStatement",
                     "OMP_GROUPPRIVATE_STMT");

  NEW_TERMINAL_MACRO(DeclarationGroupStatement, "DeclarationGroupStatement",
                     "DECLARATION_GROUP_STMT");
  NEW_TERMINAL_MACRO(AccessLabelStatement, "AccessLabelStatement",
                     "ACCESS_LABEL_STMT");
  NEW_TERMINAL_MACRO(EmptyDeclaration, "EmptyDeclaration",
                     "EMPTY_DECLARATION_STMT");

  NEW_NONTERMINAL_MACRO(
      DeclarationStatement,
      FunctionParameterList | VariableDeclaration | VariableDefinition |
          ClinkageDeclarationStatement | EnumDeclaration | AsmStmt |
          AttributeSpecificationStatement | FormatStatement |
          TemplateDeclaration | TemplateInstantiationDirectiveStatement |
          UseStatement | NamespaceDeclarationStatement | EquivalenceStatement |
          InterfaceStatement | NamespaceAliasDeclarationStatement |
          CommonBlock | TypedefDeclaration | StatementFunctionStatement |
          CtorInitializerList | PragmaDeclaration | UsingDirectiveStatement |
          ClassDeclaration | ImplicitStatement | UsingDeclarationStatement |
          NamelistStatement | ImportStatement | FunctionDeclaration |
          ContainsStatement | C_PreprocessorDirectiveStatement |
          OmpThreadprivateStatement | OmpRequiresStatement |
          FortranIncludeLine | StaticAssertionDeclaration |
          FriendTypeDeclaration | OmpDeclareSimdStatement |
          OmpDeclareVariantStatement | OmpBeginDeclareVariantStatement |
          OmpEndDeclareVariantStatement | NonrealDecl |
          DeclarationGroupStatement | AccessLabelStatement | EmptyDeclaration |
          OmpDeclareMapperStatement | OmpDeclareTargetStatement |
          OmpEndDeclareTargetStatement | OmpBeginDeclareTargetStatement |
          OmpAssumesStatement | OmpBeginAssumesStatement |
          OmpEndAssumesStatement | OmpGroupprivateStatement,
      "DeclarationStatement", "DECL_STMT", false);

  NEW_NONTERMINAL_MACRO(OmpExecStatement,
                        OmpTaskyieldStatement | OmpBarrierStatement |
                            OmpNothingStatement | OmpTaskwaitStatement |
                            OmpEndAssumeStatement | OmpBodyStatement |
                            OmpClauseStatement,
                        "OmpExecStatement", "OmpExecStatementTag", false);

  NEW_NONTERMINAL_MACRO(
      Statement,
      ScopeStatement | FunctionTypeTable | DeclarationStatement |
          ExprStatement | LabelStatement | CaseOptionStmt | TryStmt |
          DefaultOptionStmt | BreakStmt | ContinueStmt | ReturnStmt |
          GotoStatement | NullStatement | AttributedStatement |
          ForInitStatement | CatchStatementSeq | ProcessControlStatement |
          IOStatement | FortranContinueStmt | WhereStatement |
          ElseWhereStatement | NullifyStatement | ArithmeticIfStatement |
          AssignStatement | ComputedGotoStatement | AssignedGotoStatement |
          AllocateStatement | DeallocateStatement | AccBodyStatement |
          AccClauseStatement | OmpExecStatement | ImageControlStatement,
      "Statement", "StatementTag", false);

  Statement.setFunctionPrototype("HEADER", "../Grammar/Statement.code");

  Statement.setPostdeclarationString("HEADER_STATEMENT_POSTDECLARATION",
                                     "../Grammar/Statement.code");

  Statement.setFunctionPrototype("HEADER_POST_CONSTRUCTION_INITIALIZATION",
                                 "../Grammar/Statement.code");
  Statement.setSubTreeFunctionPrototype(
      "HEADER_POST_CONSTRUCTION_INITIALIZATION", "../Grammar/Statement.code");

  Statement.setDataPrototype("SgLabelRefExp*", "numeric_label", "= NULL",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
                             OPTIONAL_TRAVERSAL_MEMBER);

  // DQ (10/1/2009): Adding support for source sequence numbers as part of name
  // qualification support which would include a test for if statements
  // containing references to declarations appear before or after the defining
  // declaration they are associated with. More subtle rules also apply and are
  // critical for correct name qualification in the generated code (unparser).
  Statement.setDataPrototype("int", "source_sequence_value", "= -1",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  Statement.setDataPrototype("bool", "is_short_circuit_generated", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  Statement.setDataPrototype(
      "SgStatement::directive_end_kind_enum", "directive_end_kind",
      "= SgStatement::e_directive_end_not_applicable", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Statement.setDataPrototype(
      "SgStatement::omp_fortran_spelling_enum", "omp_fortran_spelling",
      "= SgStatement::e_omp_fortran_spelling_not_applicable",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  ScopeStatement.setFunctionPrototype("HEADER_SCOPE_STATEMENT",
                                      "../Grammar/Statement.code");
  ScopeStatement.setAutomaticGenerationOfConstructor(false);

  ScopeStatement.setDataPrototype("SgSymbolTable*", "symbol_table", "= NULL",
                                  NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, DEF_DELETE, NO_COPY_DATA);

  ScopeStatement.setDataPrototype(
      "SgTypeTable*", "type_table", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE, NO_COPY_DATA);

  // Semantic-only declarations still require explicit AST ownership. These
  // scopes are traversed from their lexical owner instead of being discoverable
  // only through global memory-pool queries.
  ScopeStatement.setDataPrototype(
      "SgDeclarationScopeList*", "auxiliary_declaration_scopes", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      DEF_DELETE, CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);

  // Semantic-only declarations are owned by their lexical scope but are not
  // part of its source-emission list. Keeping them on a separate traversed
  // edge makes analysis and name qualification complete without emitting
  // implicit declarations as source.
  ScopeStatement.setDataPrototype(
      "SgAuxiliaryDeclarationList*", "auxiliary_declarations", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      DEF_DELETE, CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);

  ScopeStatement.setDataPrototype(
      "SgPragma*", "pragma", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE, NO_COPY_DATA);

  FunctionTypeTable.setFunctionPrototype("HEADER_FUNCTION_TYPE_TABLE",
                                         "../Grammar/Statement.code");
  FunctionTypeTable.setDataPrototype(
      "SgFunctionTypeTable::TypeIndex", "types", "", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);
  FunctionTypeTable.setDataPrototype("SgFunctionTypeTable::EntryIndex",
                                     "entries", "", NO_CONSTRUCTOR_PARAMETER,
                                     NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                     NO_DELETE, NO_COPY_DATA);
  FunctionTypeTable.setDestructorPrologue("clear();");
  FunctionTypeTable.setAutomaticGenerationOfConstructor(false);

  Global.setFunctionPrototype("HEADER_GLOBAL", "../Grammar/Statement.code");
  Global.editSubstitute("HEADER_LIST_DECLARATIONS", "HEADER_LIST_DECLARATIONS",
                        "../Grammar/Statement.code");
  Global.editSubstitute("LIST_DATA_TYPE", "SgDeclarationStatementPtrList");
  Global.editSubstitute("LIST_NAME", "declarations");
  Global.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  Global.editSubstitute("LIST_FUNCTION_NAME", "declaration");
  Global.editSubstitute("LIST_ELEMENT_DATA_TYPE", "SgDeclarationStatement*");
  Global.setDataPrototype("SgDeclarationStatementPtrList", "declarations", "",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS,
                          DEF_TRAVERSAL, NO_DELETE);

  DeclarationStatement.setFunctionPrototype("HEADER_DECLARATION_STATEMENT",
                                            "../Grammar/Statement.code");
  DeclarationStatement.setDataPrototype(
      "SgAlignmentSpecifierPtrList", "alignment_specifiers", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_TREE);

  DeclarationStatement.setPostdeclarationString(
      "HEADER_DECLARATION_STATEMENT_POSTDECLARATION",
      "../Grammar/Statement.code");

  // Detached JSON declarations retain their semantic role and source identity
  // independently of user-defined attributes. Copies retain this provenance.
  DeclarationStatement.setDataPrototype(
      "SgDeclarationStatement::ast_json_external_role_enum",
      "ast_json_external_role", "= SgDeclarationStatement::e_ast_json_local",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  DeclarationStatement.setDataPrototype(
      "std::string", "ast_json_external_source_file", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Exact producer-wide lexical order for a declaration in one translation
  // unit when its frontend publishes such an order.  The field is immutable
  // after one explicit initialization; absence is represented explicitly.
  DeclarationStatement.setDataPrototype(
      "std::optional<unsigned int>", "translation_unit_source_order",
      "= std::nullopt", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);

  DeclarationStatement.setDataPrototype(
      "std::string", "linkage", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  DeclarationStatement.setDataPrototype(
      "SgDeclarationModifier", "declarationModifier", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  DeclarationStatement.setDataPrototype(
      "bool", "nameOnly", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  DeclarationStatement.setDataPrototype(
      "bool", "forward", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  DeclarationStatement.setDataPrototype(
      "bool", "externBrace", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  DeclarationStatement.setDataPrototype(
      "bool", "skipElaborateType", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Preserve a source-written nested-name-specifier structurally on the exact
  // declaration use site.  The presence bit distinguishes an explicitly empty
  // qualifier from an absent producer record.
  DeclarationStatement.setDataPrototype(
      "bool", "source_name_qualification_present", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  DeclarationStatement.setDataPrototype(
      "bool", "source_name_global_qualification", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  DeclarationStatement.setDataPrototype(
      "SgStringList", "source_name_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  DeclarationStatement.setDataPrototype(
      "SgDeclarationStatement*", "definingDeclaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);

  DeclarationStatement.setDataPrototype(
      "SgDeclarationStatement*", "firstNondefiningDeclaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);

  DeclarationStatement.setDataPrototype(
      "std::string", "binding_label", "=\"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // NAME absent and NAME="" have different Fortran binding semantics.
  DeclarationStatement.setDataPrototype(
      "bool", "binding_label_present", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  DeclarationStatement.setDataPrototype(
      "bool", "binding_cdefined", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  DeclarationStatement.setDataPrototype(
      "SgDeclarationScope*", "declarationScope", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);

  // A non-function declarator can introduce one or more source-written tags
  // below pointer, array, or function-type layers (for example,
  // `struct A *(*f)(struct B *)`).  Those declarations have the surrounding
  // lexical scope as their semantic symbol scope, but the exact declarator is
  // their physical source owner.  Model that ownership directly instead of
  // publishing the tags as standalone declarations or leaving them detached.
  // Function declarations retain their more specialized
  // function_declarator_scope because it also owns parameter-name semantics.
  DeclarationStatement.setDataPrototype(
      "SgDeclarationScope*", "source_declarator_scope", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      DEF_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);

  // TV (04/11/2018): Introducing representation for non-real "stuff" (template
  // parameters) on declarations that need a dedicated nonreal scope.
  DeclarationStatement.setDataPrototype(
      "SgDeclarationScope*", "nonreal_decl_scope", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      DEF_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);

  DeclarationStatement.setDataPrototype(
      "bool", "unparse_template_ast", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  BasicBlock.setFunctionPrototype("HEADER_BASIC_BLOCK",
                                  "../Grammar/Statement.code");

  BasicBlock.setAutomaticGenerationOfConstructor(false);
  BasicBlock.setDestructorPrologue(
      "SageInterface::retireBasicBlockNormalization(this);");
  BasicBlock.editSubstitute("HEADER_LIST_DECLARATIONS",
                            "HEADER_LIST_DECLARATIONS",
                            "../Grammar/Statement.code");
  BasicBlock.editSubstitute("LIST_DATA_TYPE", "SgStatementPtrList");
  BasicBlock.editSubstitute("LIST_NAME", "statements");
  BasicBlock.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  BasicBlock.editSubstitute("LIST_FUNCTION_NAME", "statement");
  BasicBlock.editSubstitute("LIST_ELEMENT_DATA_TYPE", "SgStatement*");
  BasicBlock.setDataPrototype(
      "SgStatementPtrList", "statements", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  // Every C++ selection/iteration substatement introduces a block scope even
  // when the source has no compound-statement braces.  Preserve that semantic
  // scope and its unbraced source form explicitly; the backend must not infer
  // it from a declaration parent or silently flatten a malformed block.
  BasicBlock.setDataPrototype("bool", "is_implicit_control_flow_scope",
                              "= false", NO_CONSTRUCTOR_PARAMETER,
                              BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                              COPY_DATA);

  // A GNU statement-expression's compound body is its own lexical scope, but
  // that body must be translated before the SgStatementExpression owner can
  // adopt it.  Publish the exact enclosing semantic scope for only that
  // bounded construction transaction.  This is not structural ownership and
  // must be cleared when the real expression owner consumes the block.
  BasicBlock.setDataPrototype(
      "SgScopeStatement*", "statement_expression_construction_scope", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      NO_COPY_DATA);

  // Once the typed SgStatementExpression owner consumes the construction
  // transaction, retain the exact outer semantic scope independently of the
  // expression's structural parent chain.  A statement expression may be an
  // operand of a shared typeof/function type, whose structural ancestry ends
  // in the type and symbol tables rather than an enclosing statement.
  BasicBlock.setDataPrototype("SgScopeStatement*",
                              "statement_expression_semantic_scope", "= NULL",
                              NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  // A Fortran implied-DO expression likewise owns a construct-local basic
  // block before the expression can be attached to its surrounding
  // expression tree.  Keep that exact outer semantic scope in a distinct,
  // producer-consumed transaction so it cannot be confused with ordinary
  // structural ownership or a GNU statement expression.
  BasicBlock.setDataPrototype("SgScopeStatement*",
                              "implied_do_construction_scope", "= NULL",
                              NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);

  // Once the SgImpliedDo adopts the construct-local block, retain the exact
  // outer semantic scope as a typed edge.  The implied-DO expression can
  // subsequently move into a type-owned bound or initializer tree whose
  // structural ancestry does not reach the declaring statement.
  BasicBlock.setDataPrototype("SgScopeStatement*", "implied_do_semantic_scope",
                              "= NULL", NO_CONSTRUCTOR_PARAMETER,
                              BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                              COPY_DATA);

  // A function body can be required as the exact lexical owner of a local
  // declaration while its function signature is still being translated.  In
  // particular, a deduced return type can contain a lambda whose closure is
  // owned by that body.  Publish the body's exact outer semantic scope during
  // this bounded detached phase.  The function-definition producer must
  // consume the role when it adopts the body.
  BasicBlock.setDataPrototype("SgScopeStatement*",
                              "function_body_construction_scope", "= NULL",
                              NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);

  // FORALL and DO CONCURRENT headers are translated in their construct-local
  // body scope before the SgForAllStatement owner can be created.  Preserve
  // the exact outer scope only for that bounded producer transaction.
  BasicBlock.setDataPrototype("SgScopeStatement*", "forall_construction_scope",
                              "= NULL", NO_CONSTRUCTOR_PARAMETER,
                              NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                              NO_COPY_DATA);

  // PP (06/04/20) - block label support (in analogy to for and while labels)
  BasicBlock.setDataPrototype("std::string", "string_label", "= \"\"",
                              NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);

  // Fortran BLOCK is a lexical scope and therefore maps naturally to a basic
  // block, but unlike a C/C++ compound statement it owns explicit BLOCK and
  // END BLOCK source tokens.  Preserve that source role and its optional
  // construct name as typed AST state; the backend must never infer the role
  // from parent shape or source text.
  BasicBlock.setDataPrototype("bool", "is_fortran_block_construct", "= false",
                              NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  BasicBlock.setDataPrototype("std::string", "fortran_block_construct_name",
                              "= \"\"", NO_CONSTRUCTOR_PARAMETER,
                              BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                              COPY_DATA);
  BasicBlock.setDataPrototype(
      "SgLabelRefExp*", "fortran_block_end_numeric_label", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);

  IfStmt.setFunctionPrototype("HEADER_IF_STATEMENT",
                              "../Grammar/Statement.code");
  IfStmt.setDataPrototype("SgStatement*", "conditional", "= NULL",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                          OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
                          COPY_AFTER_OWNER_CONSTRUCTION);
  IfStmt.setDataPrototype("SgExpression*", "condition_value", "= NULL",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
                          OPTIONAL_TRAVERSAL_MEMBER);
  IfStmt.setDataPrototype("SgStatement*", "true_body", "= NULL",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                          OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
                          COPY_AFTER_OWNER_CONSTRUCTION);
  IfStmt.setDataPrototype("SgStatement*", "false_body", "= NULL",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                          OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
                          COPY_AFTER_OWNER_CONSTRUCTION);

  IfStmt.setDataPrototype("SgIfStmt::if_statement_kind_enum", "if_statement_kind",
                          "= SgIfStmt::e_if_ordinary", NO_CONSTRUCTOR_PARAMETER,
                          BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                          COPY_DATA);
  IfStmt.setDataPrototype("bool", "if_constexpr_value_known", "= false",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  IfStmt.setDataPrototype("bool", "if_constexpr_value", "= false",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);

  // Fortran ELSE and END IF statements can have their own labels.
  IfStmt.setDataPrototype("SgLabelRefExp*", "else_numeric_label", "= NULL",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
                          OPTIONAL_TRAVERSAL_MEMBER);
  IfStmt.setDataPrototype("std::string", "string_label", "= \"\"",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  IfStmt.setDataPrototype("std::string", "else_construct_name", "= \"\"",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  IfStmt.setDataPrototype("SgLabelRefExp*", "end_numeric_label", "= NULL",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
                          OPTIONAL_TRAVERSAL_MEMBER);

  IfStmt.setDataPrototype("bool", "has_end_statement", "= false",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);

  IfStmt.setDataPrototype("bool", "use_then_keyword", "= false",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);

  // DQ (12/7/2010): Fortran specific, has associated else keyword before if (is
  // an else-if statement).
  IfStmt.setDataPrototype("bool", "is_else_if_statement", "= false",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);

  ForStatement.setFunctionPrototype("HEADER_FOR_STATEMENT",
                                    "../Grammar/Statement.code");
  ForStatement.editSubstitute("HEADER_LIST_DECLARATIONS",
                              "HEADER_LIST_DECLARATIONS",
                              "../Grammar/Statement.code");
  ForStatement.editSubstitute("LIST_DATA_TYPE", "SgStatementPtrList");
  ForStatement.editSubstitute("LIST_NAME", "init_stmt");
  ForStatement.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  ForStatement.editSubstitute("LIST_FUNCTION_NAME", "init_stmt");
  ForStatement.editSubstitute("LIST_ELEMENT_DATA_TYPE", "SgStatement*");

  ForStatement.setDataPrototype(
      "SgForInitStatement*", "for_init_stmt", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_PTR, REQUIRED_TRAVERSAL_MEMBER);

  // A transformation may need the for-statement's scope identity while its
  // declaration-bearing initializer is assembled before the statement can be
  // attached.  Carry the exact output owner as an explicit construction
  // transaction and clear it before structural publication.
  ForStatement.setDataPrototype(
      "SgScopeStatement*", "construction_physical_output_owner", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      NO_COPY_DATA);

  // Conditions and increments can refer to declarations in the loop scope.
  ForStatement.setDataPrototype(
      "SgStatement*", "test", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      REQUIRED_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_AFTER_OWNER_CONSTRUCTION);

  ForStatement.setDataPrototype(
      "SgExpression*", "condition_value", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  ForStatement.setDataPrototype(
      "SgExpression*", "increment", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      REQUIRED_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_AFTER_OWNER_CONSTRUCTION);

  ForStatement.setDataPrototype(
      "SgStatement*", "loop_body", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      REQUIRED_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_AFTER_OWNER_CONSTRUCTION);
  ForStatement.setDataPrototype(
      "std::string", "string_label", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ForInitStatement.setFunctionPrototype("HEADER_FOR_INIT_STATEMENT",
                                        "../Grammar/Statement.code");
  ForInitStatement.editSubstitute("HEADER_LIST_DECLARATIONS",
                                  "HEADER_LIST_DECLARATIONS",
                                  "../Grammar/Statement.code");
  ForInitStatement.editSubstitute("LIST_DATA_TYPE", "SgStatementPtrList");
  ForInitStatement.editSubstitute("LIST_NAME", "init_stmt");
  ForInitStatement.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  ForInitStatement.editSubstitute("LIST_FUNCTION_NAME", "init_stmt");
  ForInitStatement.editSubstitute("LIST_ELEMENT_DATA_TYPE", "SgStatement*");
  ForInitStatement.setDataPrototype(
      "SgStatementPtrList", "init_stmt", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  // Range-for declarations refer back to their loop scope; copy the scope
  // first.
  RangeBasedForStatement.setFunctionPrototype(
      "HEADER_RANGE_BASED_FOR_STATEMENT", "../Grammar/Statement.code");

  RangeBasedForStatement.setDataPrototype(
      "SgVariableDeclaration*", "iterator_declaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  RangeBasedForStatement.setDataPrototype(
      "SgVariableDeclaration*", "range_declaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);

  RangeBasedForStatement.setDataPrototype(
      "SgVariableDeclaration*", "begin_declaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  RangeBasedForStatement.setDataPrototype(
      "SgVariableDeclaration*", "end_declaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);

  RangeBasedForStatement.setDataPrototype(
      "SgExpression*", "not_equal_expression", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  RangeBasedForStatement.setDataPrototype(
      "SgExpression*", "increment_expression", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);

  RangeBasedForStatement.setDataPrototype(
      "SgStatement*", "loop_body", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  StaticAssertionDeclaration.setFunctionPrototype(
      "HEADER_STATIC_ASSERTION_DECLARATION", "../Grammar/Statement.code");
  StaticAssertionDeclaration.setDataPrototype(
      "SgExpression*", "condition", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  StaticAssertionDeclaration.setDataPrototype(
      "SgExpression*", "message", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  FriendTypeDeclaration.setFunctionPrototype("HEADER_FRIEND_TYPE_DECLARATION",
                                             "../Grammar/Statement.code");
  FriendTypeDeclaration.setDataPrototype(
      "SgType*", "friend_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionParameterScope.setFunctionPrototype("HEADER_FUNCTION_PARAMETER_SCOPE",
                                              "../Grammar/Statement.code");
  FunctionParameterScope.editSubstitute("HEADER_LIST_DECLARATIONS",
                                        "HEADER_LIST_DECLARATIONS",
                                        "../Grammar/Statement.code");
  FunctionParameterScope.editSubstitute("LIST_DATA_TYPE",
                                        "SgDeclarationStatementPtrList");
  FunctionParameterScope.editSubstitute("LIST_NAME", "declarations");
  FunctionParameterScope.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  FunctionParameterScope.editSubstitute("LIST_FUNCTION_NAME", "declaration");
  FunctionParameterScope.editSubstitute("LIST_ELEMENT_DATA_TYPE",
                                        "SgDeclarationStatement*");
  FunctionParameterScope.setDataPrototype(
      "SgDeclarationStatementPtrList", "declarations", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);
  // A nondefining Fortran procedure is typed only after its specification
  // part has been translated, so its parameter scope is assembled while
  // structurally detached.  Keep the physical output owner and semantic outer
  // scope as distinct typed transaction roles, require them to agree while
  // detached, and clear both when the final declaration consumes the scope.
  // The completed declarative region retains the semantic outer scope as an
  // explicit edge because a requires-expression is not a statement and can
  // itself remain detached while its expression subtree is validated.
  FunctionParameterScope.setDataPrototype(
      "SgScopeStatement*", "construction_physical_output_owner", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  FunctionParameterScope.setDataPrototype(
      "SgScopeStatement*", "construction_semantic_scope", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  FunctionParameterScope.setDataPrototype(
      "SgScopeStatement*", "completed_semantic_scope", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);

  // DQ (7/17/2017): Added to support concept of scope in nondefining
  // declarations (required for more sophisticated level of template useage).
  DeclarationScope.setFunctionPrototype("HEADER_DECLARATION_SCOPE",
                                        "../Grammar/Statement.code");
  DeclarationScope.editSubstitute("HEADER_LIST_DECLARATIONS",
                                  "HEADER_LIST_DECLARATIONS",
                                  "../Grammar/Statement.code");
  DeclarationScope.editSubstitute("LIST_DATA_TYPE",
                                  "SgDeclarationStatementPtrList");
  DeclarationScope.editSubstitute("LIST_NAME", "declarations");
  DeclarationScope.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  DeclarationScope.editSubstitute("LIST_FUNCTION_NAME", "declaration");
  DeclarationScope.editSubstitute("LIST_ELEMENT_DATA_TYPE",
                                  "SgDeclarationStatement*");
  DeclarationScope.setDataPrototype(
      "SgDeclarationStatementPtrList", "declarations", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);
  DeclarationScope.setDataPrototype("SgScopeStatement*", "semantic_outer_scope",
                                    "= nullptr", CONSTRUCTOR_PARAMETER,
                                    NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                    NO_DELETE, COPY_REFERENCE);
  // Distinguish the cached semantic scope across AST copy and serialization.
  DeclarationScope.setDataPrototype(
      "bool", "is_default_nonreal_scope", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setFunctionPrototype(
      "HEADER_FUNCTION_DECLARATION_STATEMENT", "../Grammar/Statement.code");
  FunctionDeclaration.setFunctionPrototype(
      "HEADER_TEMPLATE_SPECIALIZATION_SUPPORT", "../Grammar/Statement.code");

  FunctionDeclaration.editSubstitute("HEADER_LIST_DECLARATIONS",
                                     "HEADER_LIST_DECLARATIONS",
                                     "../Grammar/Statement.code");
  FunctionDeclaration.editSubstitute("LIST_DATA_TYPE",
                                     "SgInitializedNamePtrList");
  FunctionDeclaration.editSubstitute("LIST_NAME", "args");
  FunctionDeclaration.editSubstitute("LIST_FUNCTION_RETURN_TYPE",
                                     "SgInitializedNamePtrList::iterator");
  FunctionDeclaration.editSubstitute("LIST_FUNCTION_NAME", "arg");

  FunctionDeclaration.editSubstitute("LIST_ELEMENT_DATA_TYPE",
                                     "SgInitializedName*");

  FunctionDeclaration.setDataPrototype(
      "SgName", "name", "= \"\"", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "SgFunctionDeclaration::frontend_source_ownership_enum",
      "frontend_source_ownership", "= e_frontend_source_unclassified",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);
  FunctionDeclaration.setDataPrototype(
      "SgFunctionDeclaration::frontend_declaration_origin_enum",
      "frontend_declaration_origin", "= e_frontend_declaration_unclassified",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  // An implicit PROGRAM or unnamed BLOCK DATA has no public source name, but
  // still requires one stable semantic symbol-table identity. Preserve the
  // producer-published key instead of reconstructing it late from a range.
  FunctionDeclaration.setDataPrototype(
      "SgName", "fortran_anonymous_program_unit_symbol_key", "= \"\"",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);
  // Exact lexical C/C++ declarators may spell a simple function name as
  // `(name)` to prevent invocation of a same-named function-like macro.
  // Backends consume this typed source fact instead of scanning directives.
  FunctionDeclaration.setDataPrototype(
      "bool", "source_name_parenthesized_for_macro", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  // A function declared within an OpenMP begin/end declare variant region has
  // one source-language name and one REX-internal semantic identity.  The
  // internal identity keeps the base-language frontend from merging distinct
  // variant definitions; this typed source name is the only spelling emitted
  // by the unparser.
  FunctionDeclaration.setDataPrototype(
      "SgName", "omp_declare_variant_source_name", "= \"\"",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  FunctionDeclaration.setDataPrototype(
      "std::optional<unsigned int>", "omp_declare_variant_region_ordinal",
      "= std::nullopt", NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
  // GNU typeof/decltype-like declaration specifiers can denote a function
  // type while the declarator itself contains no parameter-list tokens, e.g.
  // `extern __typeof(f) alias;`.  Preserve that exact source fact separately
  // from the semantic SgFunctionType and its parameter declarations.
  FunctionDeclaration.setDataPrototype(
      "bool", "source_declarator_uses_wrapped_function_type", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "SgFunctionParameterList*", "parameterList", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);

  // A function's declarator-local signature scope is structurally owned by
  // that exact declaration.  It is distinct from DeclarationStatement's
  // declarationScope, which is the non-owning lexical scope edge used by a
  // declaration located inside such a signature scope, and from the separate
  // nonreal_decl_scope used for template-semantic infrastructure.
  FunctionDeclaration.setDataPrototype(
      "SgDeclarationScope*", "function_declarator_scope", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      DEF_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);

  FunctionDeclaration.setDataPrototype(
      "SgFunctionModifier", "functionModifier", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionDeclaration.setDataPrototype(
      "SgSpecialFunctionModifier", "specialFunctionModifier", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "SgExpression*", "trailingRequiresClause", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  FunctionDeclaration.setDataPrototype(
      "SgExpression*", "explicit_specifier_expression", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  FunctionDeclaration.setDataPrototype(
      "SgExpression*", "cuda_launch_bounds_expression", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);

  // DQ (9/3/2007): In Fortran, the function name has been used in the end
  // statement.
  FunctionDeclaration.setDataPrototype(
      "bool", "named_in_end_statement", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (1/25/2009): Added support for asm names for functions (see
  // test2009_10.C)
  FunctionDeclaration.setDataPrototype(
      "std::string", "asm_name", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionParameterList.setFunctionPrototype("HEADER_FUNCTION_PARAMETER_LIST",
                                             "../Grammar/Statement.code");

  FunctionParameterList.editSubstitute("HEADER_LIST_DECLARATIONS",
                                       "HEADER_LIST_DECLARATIONS",
                                       "../Grammar/Statement.code");
  FunctionParameterList.editSubstitute("LIST_DATA_TYPE",
                                       "SgInitializedNamePtrList");
  FunctionParameterList.editSubstitute("LIST_NAME", "args");
  FunctionParameterList.editSubstitute("LIST_FUNCTION_RETURN_TYPE",
                                       "SgInitializedNamePtrList::iterator");
  FunctionParameterList.editSubstitute("LIST_FUNCTION_NAME", "arg");

  FunctionParameterList.editSubstitute("LIST_ELEMENT_DATA_TYPE",
                                       "SgInitializedName*");
  FunctionParameterList.setDataPrototype(
      "SgInitializedNamePtrList", "args", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "SgFunctionType*", "type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  // Nondefining declarations have no owned function definition.
  FunctionDeclaration.setDataPrototype(
      "SgFunctionDefinition*", "definition", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_AFTER_OWNER_CONSTRUCTION);

  // DQ (2/15/2004): Add support for old-style C function definitions (K&R
  // style)
  FunctionDeclaration.setDataPrototype(
      "bool", "oldStyleDefinition", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "SgScopeStatement*", "scope", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  FunctionDeclaration.setDataPrototype(
      "SgTemplateParameterListPtrList", "sourceSpelledTemplateHeaders", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      DEF_DELETE, CLONE_TREE);
  // A compiler-instantiated function can be an ordinary free/member function
  // whose template dependency comes entirely from an enclosing class template.
  // Such a declaration is not a function-template specialization and cannot
  // use SgTemplateInstantiationFunctionDecl's narrower templateDeclaration
  // edge. Preserve Clang's exact FunctionDecl instantiation-pattern backlink
  // on the common function base instead of recovering it later from
  // coordinates, names, or enclosing classes. The pattern can itself be a
  // semantic-only declaration when its header has no emitted source surface.
  FunctionDeclaration.setDataPrototype(
      "SgFunctionDeclaration*", "templateInstantiationPattern", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  // DQ (5/9/2005): Adding special template support.  These data members help
  // control the instantiation of templates and there representation in the
  // unparsed code (code generated by ROSE). Note that the File_Info object is
  // where IR nodes are marked as compiler generated (also relavant here).
  FunctionDeclaration.setDataPrototype(
      "SgDeclarationStatement::template_specialization_enum", "specialization",
      "= SgDeclarationStatement::e_no_specialization", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Qualification setters must not mark the declaration as modified.
  FunctionDeclaration.setDataPrototype(
      "bool", "requiresNameQualificationOnReturnType", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype("std::string", "gnu_extension_section",
                                       "= \"\"", NO_CONSTRUCTOR_PARAMETER,
                                       BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                       NO_DELETE);
  FunctionDeclaration.setDataPrototype(
      "std::string", "gnu_extension_alias", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionDeclaration.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "int", "name_qualification_length_for_return_type", "= 0",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "bool", "type_elaboration_required_for_return_type", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "bool", "global_qualification_required_for_return_type", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Preserve the exact nested-name-specifier written on this function's
  // return-type use site.  Return types are declaration-local syntax, so the
  // shared semantic SgType cannot carry this source spelling.  The presence
  // bit distinguishes an exact unqualified spelling from absent producer
  // data.
  FunctionDeclaration.setDataPrototype(
      "bool", "source_return_type_qualification_present", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  FunctionDeclaration.setDataPrototype(
      "bool", "source_return_type_global_qualification", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  FunctionDeclaration.setDataPrototype(
      "SgStringList", "source_return_type_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  FunctionDeclaration.setDataPrototype(
      "SgFunctionDeclaration::source_return_type_elaboration_kind_enum",
      "source_return_type_elaboration_kind",
      "= SgFunctionDeclaration::e_source_return_type_elaboration_unspecified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // DQ (2/18/2014): Add support for old-style C function prototypes without
  // function parameters (K&R style)
  FunctionDeclaration.setDataPrototype("bool", "prototypeIsWithoutParameters",
                                       "= false", NO_CONSTRUCTOR_PARAMETER,
                                       BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                       NO_DELETE);

  // DQ (5/27/2015): Note that zero is a ligitimate value to use, so the
  // default should be -1. DQ (2/19/2014): Add support for gnu attribute
  // regnum (required to compile valgrind).
  FunctionDeclaration.setDataPrototype(
      "int", "gnu_regparm_attribute", "= -1", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "SgFunctionType*", "type_syntax", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE, CLONE_TREE);

  FunctionDeclaration.setDataPrototype(
      "bool", "type_syntax_is_available", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "SgFunctionParameterList*", "parameterList_syntax", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      DEF_DELETE, CLONE_TREE);

  // DQ (7/26/2014): Added support for C11 "_Noreturn" keyword (alternative
  // noreturn specification). This could maybe be moved to the
  // SgFunctionModifier.
  FunctionDeclaration.setDataPrototype(
      "bool", "using_C11_Noreturn_keyword", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "SgFunctionDeclaration::constexpr_specifier_enum", "constexpr_specifier",
      "= SgFunctionDeclaration::e_constexpr_unspecified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  // DQ (8/3/2014): Added support for C++11 new function return type syntax.
  FunctionDeclaration.setDataPrototype(
      "bool", "using_new_function_return_type_syntax", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // C++17 deduction guide marker.
  FunctionDeclaration.setDataPrototype(
      "bool", "is_deduction_guide", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // This is the declaration's exact structural owner for declarator-local and
  // Fortran specification-part declarations.  Traversing only parameterList
  // aliases bypasses source-order declarations such as Fortran USE statements
  // and reaches result/dummy types before their bindings become visible.
  FunctionDeclaration.setDataPrototype(
      "SgFunctionParameterScope*", "functionParameterScope", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);

  FunctionDeclaration.setDataPrototype(
      "bool", "marked_as_frontend_normalization", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // DQ (5/10/2018): Add a data member to record when this is an implicit
  // function. Implicit functions can have no explicit parameters and yet
  // have function arguments where they are called (default type is int for
  // such parameters).
  FunctionDeclaration.setDataPrototype(
      "bool", "is_implicit_function", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionDeclaration.setDataPrototype(
      "SgFunctionType*", "declared_exception_signature", "= nullptr",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE,
      CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES);

  FunctionDefinition.setFunctionPrototype(
      "HEADER_FUNCTION_DEFINITION_STATEMENT", "../Grammar/Statement.code");
  FunctionDefinition.editSubstitute("HEADER_LIST_DECLARATIONS",
                                    "HEADER_LIST_DECLARATIONS",
                                    "../Grammar/Statement.code");

  FunctionDefinition.setDataPrototype(
      "SgBasicBlock*", "body", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_AFTER_OWNER_CONSTRUCTION);

  FunctionDefinition.setDataPrototype(
      "SgExprListExp*", "destructor_epilogue", "= nullptr",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS);
  FunctionDefinition.setDataPrototype(
      "bool", "destructor_epilogue_dependent", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Flang builds a defining procedure body before the declaration that will
  // own it exists.  Scope statements normally derive their enclosing scope
  // from structural ownership, so carry the exact outer scope as a typed,
  // temporary construction transaction and clear it when the defining
  // declaration consumes the definition.
  FunctionDefinition.setDataPrototype(
      "SgScopeStatement*", "construction_physical_output_owner", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      NO_COPY_DATA);

  // A detached Flang definition must still provide a stable lexical scope
  // identity while declarations and types inside its specification part are
  // interned.  The exact source procedure name completes that typed
  // construction transaction; it is cleared when the defining declaration
  // adopts the definition.
  FunctionDefinition.setDataPrototype(
      "SgName", "fortran_construction_name", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);

  // DQ (10/13/2007): Modifed to avoid copying within copy mechanism so that it
  // can be built as required.
  FunctionDefinition.setDataPrototype(
      "std::map<SgNode*,int>", "scope_number_list", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      NO_COPY_DATA);

  MemberFunctionDeclaration.setFunctionPrototype(
      "HEADER_MEMBER_FUNCTION_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");
  MemberFunctionDeclaration.editSubstitute("HEADER_LIST_DECLARATIONS",
                                           "HEADER_LIST_DECLARATIONS",
                                           "../Grammar/Statement.code");

  MemberFunctionDeclaration.editSubstitute("LIST_DATA_TYPE",
                                           "SgInitializedNamePtrList");
  MemberFunctionDeclaration.editSubstitute("LIST_NAME", "ctors");
  MemberFunctionDeclaration.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  MemberFunctionDeclaration.editSubstitute("LIST_FUNCTION_NAME",
                                           "ctor_initializer");
  MemberFunctionDeclaration.editSubstitute("LIST_ELEMENT_DATA_TYPE",
                                           "SgInitializedName*");

  MemberFunctionDeclaration.setDataPrototype(
      "SgCtorInitializerList*", "CtorInitializerList", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);

  // Every member declaration requires its exact class or dependent owner.
  MemberFunctionDeclaration.setDataPrototype(
      "SgDeclarationStatement*", "associatedClassDeclaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  CtorInitializerList.setFunctionPrototype("HEADER_CTOR_INITIALIZER_LIST",
                                           "../Grammar/Statement.code");
  CtorInitializerList.editSubstitute("HEADER_LIST_DECLARATIONS",
                                     "HEADER_LIST_DECLARATIONS",
                                     "../Grammar/Statement.code");
  CtorInitializerList.editSubstitute("LIST_DATA_TYPE",
                                     "SgInitializedNamePtrList");
  CtorInitializerList.editSubstitute("LIST_NAME", "ctors");
  CtorInitializerList.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  CtorInitializerList.editSubstitute("LIST_FUNCTION_NAME", "ctor_initializer");
  CtorInitializerList.editSubstitute("LIST_ELEMENT_DATA_TYPE",
                                     "SgInitializedName*");
  CtorInitializerList.setDataPrototype(
      "SgInitializedNamePtrList", "ctors", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  VariableDeclaration.setFunctionPrototype(
      "HEADER_VARIABLE_DECLARATION_STATEMENT", "../Grammar/Statement.code");
  VariableDeclaration.setFunctionPrototype(
      "HEADER_TEMPLATE_SPECIALIZATION_SUPPORT", "../Grammar/Statement.code");

  // A source-spelled elaborated tag introduction in a declarator is an exact,
  // nondefining child of that variable declaration.  It must precede the
  // initialized-name list in traversal order so declaration visibility is
  // established before the declarator's type is processed.
  VariableDeclaration.setDataPrototype(
      "SgDeclarationStatement*", "baseTypeNondefiningDeclaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);

  // Inline class and enum definitions are exact, traversed children of their
  // variable declaration. Keeping this as an explicit edge avoids recovering
  // ownership through shared SgNamedType objects during AST copy and unparse.
  VariableDeclaration.setDataPrototype(
      "SgDeclarationStatement*", "baseTypeDefiningDeclaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER,
      COMPUTED_BASE_TYPE_DECLARATION_ACCESS);

  VariableDeclaration.setDataPrototype(
      "SgExpression*", "requiresClause", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  VariableDeclaration.setDataPrototype(
      "SgInitializedNamePtrList", "variables", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  VariableDeclaration.setDataPrototype(
      "SgTemplateParameterListPtrList", "sourceSpelledTemplateHeaders", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      DEF_DELETE, CLONE_TREE);
  VariableDeclaration.setDataPrototype(
      "SgNamedType*", "sourceSpelledTemplateOwnerType", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  VariableDeclaration.setDataPrototype(
      "SgVariableDeclaration::fortran_declaration_origin_enum",
      "fortran_declaration_origin",
      "= SgVariableDeclaration::e_fortran_source_declaration",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  VariableDeclaration.setDataPrototype(
      "SgDeclarationStatement::template_specialization_enum", "specialization",
      "= SgDeclarationStatement::e_no_specialization", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  VariableDeclaration.setDataPrototype(
      "unsigned int", "explicitTemplateSpecializationHeaderCount", "= 0",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // DQ (8/20/2006): Record if global name qualification is required on the
  // type. See test2003_01.C for an example of where this is required.
  VariableDeclaration.setDataPrototype(
      "bool", "requiresGlobalNameQualificationOnType", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (12/4/2007): Support for GNU attribute mechanism.
  VariableDeclaration.setDataPrototype("std::string", "gnu_extension_section",
                                       "= \"\"", NO_CONSTRUCTOR_PARAMETER,
                                       BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                       NO_DELETE);
  VariableDeclaration.setDataPrototype(
      "std::string", "gnu_extension_alias", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  VariableDeclaration.setDataPrototype(
      "unsigned short", "gnu_extension_initialization_priority", "= 0",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  VariableDeclaration.setDataPrototype(
      "SgVariableDeclaration::gnu_extension_declaration_attributes_enum",
      "gnu_extension_declaration_attribute",
      "= SgVariableDeclaration::e_gnu_attribute_unspecified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // DQ (6/11/2015): Skip building of access functions (because it sets the
  // isModified flag, not wanted for the name qualification step). DQ
  // (5/12/2011): Added support for name qualification.
  VariableDeclaration.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (6/11/2015): Skip building of access functions (because it sets the
  // isModified flag, not wanted for the name qualification step). DQ
  // (5/12/2011): Added information required for new name qualification
  // support.
  VariableDeclaration.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (6/11/2015): Skip building of access functions (because it sets the
  // isModified flag, not wanted for the name qualification step). DQ
  // (5/12/2011): Added information required for new name qualification
  // support.
  VariableDeclaration.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (7/28/2012): We need to support lists of types at the end of a variable
  // declaration. ROSE presently will output separate declarations for each
  // type.  This is OK (but will be fixed) however it interferes with the new
  // support for un-named types in typedef declarations.  so we want to mark
  // declarations that are output assocuated with a previous declaration.
  // Example of where this is relevant is:
  //      struct { } S,T;
  // which will be output as (using __xxx as an internal name):
  //      struct __xxx { } S;
  //      struct __xxx T;
  // But the second (associated) declaration need to use the name of the
  // un-named type "S" See test2012_141.C for an example of this issue.
  VariableDeclaration.setDataPrototype(
      "bool", "isAssociatedWithDeclarationList", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  VariableDeclaration.setDataPrototype(
      "bool", "is_thread_local", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  VariableDeclaration.setDataPrototype(
      "bool", "is_constexpr", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  VariableDeclaration.setDataPrototype(
      "SgVariableDeclaration::inline_kind", "inline_kind",
      "= inline_kind::none", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  VariableDefinition.setFunctionPrototype(
      "HEADER_VARIABLE_DEFINITION_STATEMENT", "../Grammar/Statement.code");

  // Back-reference to the SgInitializedName that structurally owns this
  // definition through SgInitializedName::variable_definition.  It must be
  // cloned through the transaction map, but it is not a traversal edge.
  VariableDefinition.setDataPrototype(
      "SgInitializedName*", "vardefn", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, CLONE_TREE);

  // Preserve the width expression, including target-dependent sizeof operands.
  VariableDefinition.setDataPrototype(
      "SgExpression*", "bitfield", "= NULL", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  VariableDefinition.setDataPrototype(
      "bool", "is_zero_width_bitfield", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  VariableDefinition.setDataPrototype(
      "bool", "has_no_unique_address", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  ClassDeclaration.setFunctionPrototype("HEADER_CLASS_DECLARATION_STATEMENT",
                                        "../Grammar/Statement.code");
  ClassDeclaration.setFunctionPrototype(
      "HEADER_TEMPLATE_SPECIALIZATION_SUPPORT", "../Grammar/Statement.code");

  ClassDeclaration.setDataPrototype(
      "SgName", "name", "= \"\"", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);

  ClassDeclaration.setDataPrototype(
      "SgClassDeclaration::class_types", "class_type",
      "= SgClassDeclaration::e_class", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Closure initializers belong to the construction expression, not the
  // class. Retain its canonical recipe without taking expression ownership.
  ClassDeclaration.setDataPrototype(
      "SgLambdaExp*", "lambda_expression", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  // A class declaration chain owns one canonical SgClassType.  Route copies
  // through SgCopyHelp so the first/nondefining and defining declarations
  // reuse the same copied type instead of cloning one type per declaration.
  ClassDeclaration.setDataPrototype(
      "SgClassType*", "type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, CLONE_TREE);

  ClassDeclaration.setDataPrototype(
      "SgClassDefinition*", "definition", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER, COMPUTED_CLASS_DEFINITION_ACCESS,
      COPY_AFTER_OWNER_CONSTRUCTION);

  // Semantic scope can differ from a nested class's lexical parent.
  ClassDeclaration.setDataPrototype(
      "SgScopeStatement*", "scope", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  ClassDeclaration.setDataPrototype(
      "SgTemplateParameterListPtrList", "sourceSpelledTemplateHeaders", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      DEF_DELETE, CLONE_TREE);

  ClassDeclaration.setDataPrototype(
      "SgDeclarationStatement::template_specialization_enum", "specialization",
      "= SgDeclarationStatement::e_no_specialization", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ClassDeclaration.setDataPrototype(
      "bool", "from_template", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Distinguish unnamed source classes from their internal names.
  ClassDeclaration.setDataPrototype(
      "bool", "isUnNamed", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Qualification setters must not mark the declaration as modified.
  ClassDeclaration.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ClassDeclaration.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ClassDeclaration.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (7/9/2012): Added information required to indicate that declaration is
  // contained in a type reference (as part of another declaration). This is
  // also called autonomous declaration handling.  The following examples make
  // clear the definition of autonomous and non autonomous declarations:
  //      struct A { int i; };         // autonomous declaration
  //      struct B { int i; } b;       // non autonomous declaration
  // Clearly the autonomous case is the more common, but we must handle both.
  ClassDeclaration.setDataPrototype(
      "bool", "isAutonomousDeclaration", "= true", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ClassDefinition.setFunctionPrototype("HEADER_CLASS_DEFINITION_STATEMENT",
                                       "../Grammar/Statement.code");
  ClassDefinition.editSubstitute("HEADER_LIST_DECLARATIONS_1",
                                 "HEADER_LIST_DECLARATIONS",
                                 "../Grammar/Statement.code");
  ClassDefinition.editSubstitute("LIST_DATA_TYPE",
                                 "SgDeclarationStatementPtrList");
  ClassDefinition.editSubstitute("LIST_NAME", "members");
  ClassDefinition.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  ClassDefinition.editSubstitute("LIST_FUNCTION_NAME", "member");
  ClassDefinition.editSubstitute("LIST_ELEMENT_DATA_TYPE",
                                 "SgDeclarationStatement*");
  ClassDefinition.setDataPrototype(
      "SgDeclarationStatementPtrList", "members", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  ClassDefinition.setDataPrototype(
      "SgUnsignedCharList", "source_member_roles", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  // Detached class construction needs an exact enclosing scope until the
  // complete declaration is published under its lexical or auxiliary owner.
  ClassDefinition.setDataPrototype(
      "SgScopeStatement*", "construction_physical_output_owner", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      NO_COPY_DATA);

  ClassDefinition.editSubstitute("HEADER_LIST_DECLARATIONS_2",
                                 "HEADER_LIST_DECLARATIONS",
                                 "../Grammar/Statement.code");
  ClassDefinition.editSubstitute("LIST_DATA_TYPE", "SgBaseClassPtrList");
  ClassDefinition.editSubstitute("LIST_NAME", "inheritances");
  ClassDefinition.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  ClassDefinition.editSubstitute("LIST_FUNCTION_NAME", "inheritance");
  ClassDefinition.editSubstitute("LIST_ELEMENT_DATA_TYPE", "SgBaseClass*");

  ClassDefinition.setDataPrototype(
      "SgBaseClassPtrList", "inheritances", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE, CLONE_TREE);

  ClassDefinition.setDataPrototype(
      "bool", "isSequence", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ClassDefinition.setDataPrototype(
      "bool", "isPrivate", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateDeclaration.setFunctionPrototype(
      "HEADER_TEMPLATE_DECLARATION_STATEMENT", "../Grammar/Statement.code");
  TemplateDeclaration.setDataPrototype(
      "SgName", "name", "= \"\"", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);

  TemplateDeclaration.setDataPrototype(
      "SgTemplateDeclaration::template_type_enum", "template_kind",
      "= SgTemplateDeclaration::e_template_none", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateDeclaration.setDataPrototype(
      "SgTemplateParameterPtrList", "templateParameters",
      "= SgTemplateParameterPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  TemplateDeclaration.setDataPrototype(
      "SgExpression*", "requiresClause", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  TemplateDeclaration.setDataPrototype(
      "SgScopeStatement*", "scope", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  TemplateClassDeclaration.setFunctionPrototype(
      "HEADER_TEMPLATE_CLASS_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");

  TemplateClassDeclaration.setDataPrototype(
      "SgTemplateParameterPtrList", "templateParameters",
      "= SgTemplateParameterPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      std::nullopt, DIRECT_TRAVERSAL_ACCESS, COPY_BEFORE_OTHER_MEMBERS);
  TemplateClassDeclaration.setDataPrototype(
      "SgTemplateArgumentPtrList", "templateSpecializationArguments",
      "= SgTemplateArgumentPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  // A partial specialization is a distinct template declaration family.  Keep
  // its exact primary-template identity as a typed semantic edge; recovering
  // that relationship later from names, mangled names, or specialization
  // arguments loses information and depends on construction order.
  TemplateClassDeclaration.setDataPrototype(
      "SgTemplateClassDeclaration*", "specializedTemplateDeclaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  TemplateClassDeclaration.setDataPrototype(
      "SgTemplateClassDeclaration*", "instantiatedFromMemberTemplate", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);
  TemplateClassDeclaration.setDataPrototype(
      "SgExpression*", "requiresClause", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  TemplateClassDeclaration.setDataPrototype(
      "SgName", "templateName", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateFunctionDeclaration.setFunctionPrototype(
      "HEADER_TEMPLATE_FUNCTION_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");

  TemplateFunctionDeclaration.setDataPrototype(
      "SgTemplateParameterPtrList", "templateParameters",
      "= SgTemplateParameterPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  TemplateFunctionDeclaration.setDataPrototype(
      "SgTemplateArgumentPtrList", "templateSpecializationArguments",
      "= SgTemplateArgumentPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  TemplateFunctionDeclaration.setDataPrototype(
      "SgExpression*", "requiresClause", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  TemplateMemberFunctionDeclaration.setFunctionPrototype(
      "HEADER_TEMPLATE_MEMBER_FUNCTION_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");

  TemplateMemberFunctionDeclaration.setDataPrototype(
      "SgTemplateParameterPtrList", "templateParameters",
      "= SgTemplateParameterPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  TemplateMemberFunctionDeclaration.setDataPrototype(
      "SgTemplateArgumentPtrList", "templateSpecializationArguments",
      "= SgTemplateArgumentPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  TemplateMemberFunctionDeclaration.setDataPrototype(
      "SgExpression*", "requiresClause", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  TemplateVariableDeclaration.setFunctionPrototype(
      "HEADER_TEMPLATE_VARIABLE_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");

  TemplateVariableDeclaration.setDataPrototype(
      "SgTemplateParameterPtrList", "templateParameters",
      "= SgTemplateParameterPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  TemplateVariableDeclaration.setDataPrototype(
      "SgTemplateArgumentPtrList", "templateSpecializationArguments",
      "= SgTemplateArgumentPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  addTemplateInstantiationPrototypes(TemplateVariableDeclaration);
  TemplateVariableDeclaration.setDataPrototype(
      "SgTemplateArgumentPtrList", "semanticTemplateArguments",
      "= SgTemplateArgumentPtrList()", CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  TemplateTypedefDeclaration.setFunctionPrototype(
      "HEADER_TEMPLATE_TYPEDEF_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");

  TemplateTypedefDeclaration.setDataPrototype(
      "SgTemplateParameterPtrList", "templateParameters",
      "= SgTemplateParameterPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  TemplateTypedefDeclaration.setDataPrototype(
      "SgTemplateArgumentPtrList", "templateSpecializationArguments",
      "= SgTemplateArgumentPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  TemplateTypedefDeclaration.setDataPrototype(
      "SgExpression*", "requiresClause", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  TemplateInstantiationTypedefDeclaration.setFunctionPrototype(
      "HEADER_TEMPLATE_INSTANTIATION_TYPEDEF_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");
  TemplateInstantiationTypedefDeclaration.setDataPrototype(
      "SgName", "templateName", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateInstantiationTypedefDeclaration.setDataPrototype(
      "SgName", "templateHeader", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateInstantiationTypedefDeclaration.setDataPrototype(
      "SgTemplateTypedefDeclaration*", "templateDeclaration", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateInstantiationTypedefDeclaration.setDataPrototype(
      "SgTemplateArgumentPtrList", "templateArguments",
      "= SgTemplateArgumentPtrList()", CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      std::nullopt, DIRECT_TRAVERSAL_ACCESS, COPY_AFTER_OWNER_CONSTRUCTION);
  addTemplateInstantiationPrototypes(TemplateInstantiationTypedefDeclaration);

  TemplateClassDefinition.setFunctionPrototype(
      "HEADER_TEMPLATE_CLASS_DEFINITION_STATEMENT",
      "../Grammar/Statement.code");
  TemplateFunctionDefinition.setFunctionPrototype(
      "HEADER_TEMPLATE_FUNCTION_DEFINITION_STATEMENT",
      "../Grammar/Statement.code");

  TemplateInstantiationDirectiveStatement.setFunctionPrototype(
      "HEADER_TEMPLATE_INSTANTIATION_DIRECTIVE_STATEMENT",
      "../Grammar/Statement.code");

  // The directive owns its declaration; traversal and copying follow that edge.
  TemplateInstantiationDirectiveStatement.setDataPrototype(
      "SgDeclarationStatement*", "declaration", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);

  TemplateInstantiationDirectiveStatement.setDataPrototype(
      "bool", "do_not_instantiate", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateInstantiationDecl.setFunctionPrototype(
      "HEADER_TEMPLATE_INSTANTIATION_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");

  TemplateInstantiationDecl.setDataPrototype(
      "SgName", "templateName", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateInstantiationDecl.setDataPrototype(
      "SgName", "templateHeader", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateInstantiationDecl.setDataPrototype(
      "SgTemplateClassDeclaration*", "templateDeclaration", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);

  // Template arguments are declaration-owned structural children.  They must
  // participate in traversal so copying an instantiation cannot alias the
  // source declaration's argument nodes and later reparent the source AST.
  TemplateInstantiationDecl.setDataPrototype(
      "SgTemplateArgumentPtrList", "templateArguments",
      "= SgTemplateArgumentPtrList()", CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  // A class-template specialization's complete semantic argument list is
  // part of its identity and must be supplied at construction. The separately
  // owned templateArguments list records its written source surface, while
  // deducedTemplateArguments records the substitution list for a selected
  // partial specialization.
  TemplateInstantiationDecl.setDataPrototype(
      "SgTemplateArgumentPtrList", "semanticTemplateArguments",
      "= SgTemplateArgumentPtrList()", CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  // An injected current-instantiation name is semantically a complete
  // template-id, but C++ source spells it as the bare injected class name.
  // Preserve that exact source role independently of the complete semantic
  // name and argument payload.
  TemplateInstantiationDecl.setDataPrototype(
      "bool", "sourceSpellsInjectedClassName", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  addTemplateInstantiationPrototypes(TemplateInstantiationDecl, true);

  TemplateInstantiationDefn.setFunctionPrototype(
      "HEADER_TEMPLATE_INSTANTIATION_DEFINITION_STATEMENT",
      "../Grammar/Statement.code");

  TemplateInstantiationFunctionDecl.setFunctionPrototype(
      "HEADER_TEMPLATE_FUNCTION_INSTANTIATION_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");
  TemplateInstantiationFunctionDecl.setDataPrototype(
      "SgName", "templateName", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateInstantiationFunctionDecl.setDataPrototype(
      "SgTemplateFunctionDeclaration*", "templateDeclaration", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);
  TemplateInstantiationFunctionDecl.setDataPrototype(
      "SgTemplateArgumentPtrList", "templateArguments",
      "= SgTemplateArgumentPtrList()", CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  // An explicit dependent function template-id can name an unresolved
  // overload set.  Clang retains every viable FunctionTemplateDecl until
  // substitution makes the parameter types non-dependent.  Such a declaration
  // has no single primary-template identity, so preserve the exact candidate
  // declarations as non-owning semantic edges instead of selecting one by
  // name, source order, or traversal order.
  TemplateInstantiationFunctionDecl.setDataPrototype(
      "SgDeclarationStatementPtrList", "dependentTemplateCandidates", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE, COPY_DATA);
  addTemplateInstantiationPrototypes(TemplateInstantiationFunctionDecl, true);

  TemplateInstantiationFunctionDecl.setDataPrototype(
      "bool", "template_argument_list_is_explicit", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  TemplateInstantiationMemberFunctionDecl.setFunctionPrototype(
      "HEADER_TEMPLATE_MEMBER_FUNCTION_INSTANTIATION_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");

  TemplateInstantiationMemberFunctionDecl.setDataPrototype(
      "SgName", "templateName", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateInstantiationMemberFunctionDecl.setDataPrototype(
      "SgTemplateMemberFunctionDeclaration*", "templateDeclaration", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);
  TemplateInstantiationMemberFunctionDecl.setDataPrototype(
      "SgTemplateArgumentPtrList", "templateArguments",
      "= SgTemplateArgumentPtrList()", CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  addTemplateInstantiationPrototypes(TemplateInstantiationMemberFunctionDecl,
                                     true);

  TemplateInstantiationMemberFunctionDecl.setDataPrototype(
      "bool", "template_argument_list_is_explicit", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  NonrealDecl.setFunctionPrototype("HEADER_NONREAL_DECL",
                                   "../Grammar/Statement.code");

  NonrealDecl.setDataPrototype("SgName", "name", "= \"\"",
                               CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  NonrealDecl.setDataPrototype("SgNonrealType*", "type", "= NULL",
                               NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE, CLONE_TREE);

  NonrealDecl.setDataPrototype("int", "template_parameter_position", "= -1",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  NonrealDecl.setDataPrototype("int", "template_parameter_depth", "= -1",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  NonrealDecl.setDataPrototype("SgDeclarationStatement*", "templateDeclaration",
                               "= NULL", CONSTRUCTOR_PARAMETER,
                               BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                               COPY_REFERENCE);

  NonrealDecl.setDataPrototype(
      "SgTemplateArgumentPtrList", "tpl_args", "= SgTemplateArgumentPtrList()",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);
  NonrealDecl.setDataPrototype(
      "SgTemplateParameterPtrList", "tpl_params",
      "= SgTemplateParameterPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  NonrealDecl.setDataPrototype("SgExpression*", "conceptConstraint", "= NULL",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                               OPTIONAL_TRAVERSAL_MEMBER);

  NonrealDecl.setDataPrototype("bool", "is_template_param", "= false",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  NonrealDecl.setDataPrototype("bool", "has_template_keyword", "= false",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  NonrealDecl.setDataPrototype("bool", "has_global_qualifier", "= false",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  NonrealDecl.setDataPrototype("bool", "suppress_typename", "= false",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  // A source-spelled nonreal type is a per-use syntax surface. Preserve its
  // exact elaborated-type-specifier instead of collapsing `typename`, tag
  // keywords, and no keyword into the legacy suppress_typename boolean.
  NonrealDecl.setDataPrototype(
      "SgNonrealDecl::source_elaboration_kind_enum", "source_elaboration_kind",
      "= SgNonrealDecl::e_source_elaboration_unspecified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  NonrealDecl.setDataPrototype(
      "SgNonrealDecl::nonreal_template_role_enum", "nonreal_template_role",
      "= SgNonrealDecl::e_nonreal_template_none", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NonrealDecl.setDataPrototype("bool", "is_concept", "= false",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  EnumDeclaration.setFunctionPrototype("HEADER_ENUM_DECLARATION_STATEMENT",
                                       "../Grammar/Statement.code");
  EnumDeclaration.editSubstitute("HEADER_LIST_DECLARATIONS",
                                 "HEADER_LIST_DECLARATIONS",
                                 "../Grammar/Statement.code");
  EnumDeclaration.editSubstitute("LIST_DATA_TYPE", "SgInitializedNamePtrList");
  EnumDeclaration.editSubstitute("LIST_NAME", "enumerators");
  EnumDeclaration.editSubstitute("LIST_FUNCTION_RETURN_TYPE",
                                 "SgInitializedNamePtrList::iterator");
  EnumDeclaration.editSubstitute("LIST_FUNCTION_NAME", "enumerator");
  EnumDeclaration.editSubstitute("LIST_ELEMENT_DATA_TYPE",
                                 "SgInitializedName*");

  EnumDeclaration.setDataPrototype(
      "SgName", "name", "= \"\"", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
  EnumDeclaration.setDataPrototype(
      "bool", "embedded", "=false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // An enum declaration chain owns one canonical SgEnumType.  Route copies
  // through SgCopyHelp so the first/nondefining and defining declarations
  // reuse the same copied type instead of cloning one type per declaration.
  EnumDeclaration.setDataPrototype(
      "SgEnumType*", "type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, CLONE_TREE);

  EnumDeclaration.setDataPrototype(
      "SgInitializedNamePtrList", "enumerators", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  EnumDeclaration.setDataPrototype(
      "SgLabelRefExp*", "end_numeric_label", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  EnumDeclaration.setDataPrototype(
      "SgScopeStatement*", "scope", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  EnumDeclaration.setDataPrototype(
      "bool", "isUnNamed", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  EnumDeclaration.setDataPrototype(
      "bool", "isAutonomousDeclaration", "= true", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  EnumDeclaration.setDataPrototype(
      "SgType*", "field_type", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  EnumDeclaration.setDataPrototype(
      "SgType*", "promotion_type", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, CLONE_TREE);

  // field_type is the exact semantic integer type even when the source omitted
  // an enum-base.  Keep source spelling independent so the backend cannot turn
  // an implicit C/C++ underlying type into explicit syntax.
  EnumDeclaration.setDataPrototype("bool", "underlying_type_source_spelled",
                                   "= false", NO_CONSTRUCTOR_PARAMETER,
                                   BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                   NO_DELETE);

  EnumDeclaration.setDataPrototype(
      "bool", "isScopedEnum", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  EnumDeclaration.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  EnumDeclaration.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  EnumDeclaration.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ExprStatement.setFunctionPrototype("HEADER_EXPRESSION_STATEMENT",
                                     "../Grammar/Statement.code");

  ExprStatement.setDataPrototype("SgExpression*", "expression", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                 OPTIONAL_TRAVERSAL_MEMBER);

  LabelStatement.setFunctionPrototype("HEADER_LABEL_STATEMENT",
                                      "../Grammar/Statement.code");
  LabelStatement.setDataPrototype("SgName", "label", "= \"\"",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);
  LabelStatement.setDataPrototype(
      "SgScopeStatement*", "scope", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  // C and C++ labels structurally own exactly one labeled statement. The
  // child is traversed and is not also a sibling in the enclosing block.
  LabelStatement.setDataPrototype("SgStatement*", "statement", "= NULL",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                  OPTIONAL_TRAVERSAL_MEMBER);

  // Not that the SgWhileStmt is used for the Fortran do while statement
  // (because the test is at the top of the loop and so is more closely matched
  // to the C/C++ "while(){}" statement than the C/C++ "do {} while();")
  WhileStmt.setFunctionPrototype("HEADER_WHILE_STATEMENT",
                                 "../Grammar/Statement.code");
  WhileStmt.setDataPrototype("SgStatement*", "condition", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);
  WhileStmt.setDataPrototype("SgExpression*", "condition_value", "= NULL",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
                             OPTIONAL_TRAVERSAL_MEMBER);
  WhileStmt.setDataPrototype("SgStatement*", "body", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);

  WhileStmt.setDataPrototype("SgLabelRefExp*", "end_numeric_label", "= NULL",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
                             OPTIONAL_TRAVERSAL_MEMBER);
  // DQ (11/17/2007): Added support for string labels (for Fortran).
  WhileStmt.setDataPrototype("std::string", "string_label", "= \"\"",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  // DQ (12/26/2007): Fortran specific, has associated enddo statement
  WhileStmt.setDataPrototype("bool", "has_end_statement", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  DoWhileStmt.setFunctionPrototype("HEADER_DO_WHILE_STATEMENT",
                                   "../Grammar/Statement.code");
  // DQ (11/17/2005): Swapped order to force order of traversal to reflect
  // structure.
  DoWhileStmt.setDataPrototype("SgStatement*", "body", "= NULL",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                               OPTIONAL_TRAVERSAL_MEMBER);
  DoWhileStmt.setDataPrototype("SgStatement*", "condition", "= NULL",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                               OPTIONAL_TRAVERSAL_MEMBER);

  SwitchStatement.setFunctionPrototype("HEADER_SWITCH_STATEMENT",
                                       "../Grammar/Statement.code");

  SwitchStatement.setDataPrototype(
      "SgStatement*", "item_selector", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  SwitchStatement.setDataPrototype(
      "SgExpression*", "condition_value", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  // JJW (12/9/2008): The body does not need to be a block
  SwitchStatement.setDataPrototype(
      "SgStatement*", "body", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  SwitchStatement.setDataPrototype(
      "SgLabelRefExp*", "end_numeric_label", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  // DQ (11/16/2007): Added support for string labels (for Fortran).
  SwitchStatement.setDataPrototype(
      "std::string", "string_label", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  CaseOptionStmt.setFunctionPrototype("HEADER_CASE_OPTION_STATEMENT",
                                      "../Grammar/Statement.code");

  CaseOptionStmt.setDataPrototype("SgExpression*", "key", "= NULL",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                  OPTIONAL_TRAVERSAL_MEMBER);

  CaseOptionStmt.setDataPrototype("SgStatement*", "body", "= NULL",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                  OPTIONAL_TRAVERSAL_MEMBER);

  // DQ (1/6/2006): Added support for GNU case label ranges (e.g. "case 2 ...
  // 5:" in switch statements; see test2005_202.C)
  CaseOptionStmt.setDataPrototype(
      "SgExpression*", "key_range_end", " = NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // DQ (1/31/2009): Added support for named case construct.
  CaseOptionStmt.setDataPrototype(
      "std::string", "case_construct_name", " = \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TryStmt.setFunctionPrototype("HEADER_TRY_STATEMENT",
                               "../Grammar/Statement.code");
  TryStmt.editSubstitute("HEADER_LIST_DECLARATIONS", "HEADER_LIST_DECLARATIONS",
                         "../Grammar/Statement.code");
  TryStmt.editSubstitute("LIST_DATA_TYPE", "SgStatementPtrList");
  TryStmt.editSubstitute("LIST_NAME", "catch_statement_seq");
  TryStmt.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  TryStmt.editSubstitute("LIST_FUNCTION_NAME", "catch_statement");
  TryStmt.editSubstitute("LIST_ELEMENT_DATA_TYPE", "SgStatement*");

  TryStmt.setDataPrototype("SgStatement*", "body", "= NULL",
                           CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                           OPTIONAL_TRAVERSAL_MEMBER);
  TryStmt.setDataPrototype("SgCatchStatementSeq*", "catch_statement_seq_root",
                           "= NULL", NO_CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
                           COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  TryStmt.setDataPrototype("bool", "is_function_try_block", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  CatchStatementSeq.setFunctionPrototype("HEADER_CATCH_STATEMENT_SEQ",
                                         "../Grammar/Statement.code");
  CatchStatementSeq.editSubstitute("HEADER_LIST_DECLARATIONS",
                                   "HEADER_LIST_DECLARATIONS",
                                   "../Grammar/Statement.code");
  CatchStatementSeq.editSubstitute("LIST_DATA_TYPE", "SgStatementPtrList");
  CatchStatementSeq.editSubstitute("LIST_NAME", "catch_statement_seq");
  CatchStatementSeq.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  CatchStatementSeq.editSubstitute("LIST_FUNCTION_NAME", "catch_statement");
  CatchStatementSeq.editSubstitute("LIST_ELEMENT_DATA_TYPE", "SgStatement*");
  CatchStatementSeq.setDataPrototype(
      "SgStatementPtrList", "catch_statement_seq", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  CatchOptionStmt.setFunctionPrototype("HEADER_CATCH_OPTION_STATEMENT",
                                       "../Grammar/Statement.code");

  CatchOptionStmt.setDataPrototype(
      "SgVariableDeclaration*", "condition", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CatchOptionStmt.setDataPrototype(
      "SgStatement*", "body", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CatchOptionStmt.setDataPrototype(
      "SgTryStmt*", "trystmt", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  DefaultOptionStmt.setFunctionPrototype("HEADER_DEFAULT_OPTION_STATEMENT",
                                         "../Grammar/Statement.code");

  DefaultOptionStmt.setDataPrototype(
      "SgStatement*", "body", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // DQ (1/31/2009): Added support for named default case construct.
  DefaultOptionStmt.setDataPrototype("std::string", "default_construct_name",
                                     " = \"\"", NO_CONSTRUCTOR_PARAMETER,
                                     BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                     NO_DELETE);

  BreakStmt.setFunctionPrototype("HEADER_BREAK_STATEMENT",
                                 "../Grammar/Statement.code");

  // DQ (11/17/2007): Fortran support requires string label target ("break" in C
  // == "exit" in Fortran)
  BreakStmt.setDataPrototype("std::string", "do_string_label", "= \"\"",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  ContinueStmt.setFunctionPrototype("HEADER_CONTINUE_STATEMENT",
                                    "../Grammar/Statement.code");
  // Fortran requires string label target ("continue" in C == "cycle" in
  // Fortran)
  ContinueStmt.setDataPrototype(
      "std::string", "do_string_label", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ReturnStmt.setFunctionPrototype("HEADER_RETURN_STATEMENT",
                                  "../Grammar/Statement.code");

  ReturnStmt.setDataPrototype("SgExpression*", "expression", "= NULL",
                              CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                              DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                              OPTIONAL_TRAVERSAL_MEMBER);
  ReturnStmt.setDataPrototype("SgInitializedName*", "nrvo_candidate",
                              "= nullptr", CONSTRUCTOR_PARAMETER,
                              NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                              COPY_CONSTRUCTOR_REFERENCE);
  ReturnStmt.setDataPrototype(
      "SgReturnStmt::return_keyword_kind_enum", "return_keyword_kind",
      "= SgReturnStmt::e_return_keyword_return", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  GotoStatement.setFunctionPrototype("HEADER_GOTO_STATEMENT",
                                     "../Grammar/Statement.code");
  GotoStatement.setDataPrototype("SgLabelStatement*", "label", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  // DQ (12/16/2007): Added to support new way to handle labels and goto (as
  // implemented in Fortran support)
  GotoStatement.setDataPrototype(
      "SgLabelRefExp*", "label_expression", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  // GNU computed goto owns its selector, unlike a direct label reference.
  GotoStatement.setDataPrototype(
      "SgExpression*", "selector_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  AsmStmt.setFunctionPrototype("HEADER_ASM_STATEMENT",
                               "../Grammar/Statement.code");

  AsmStmt.setDataPrototype("std::string", "assemblyCode", "= \"\"",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  AsmStmt.setDataPrototype("bool", "useGnuExtendedFormat", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  AsmStmt.setDataPrototype("SgExpressionPtrList", "operands", "",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           DEF_TRAVERSAL, NO_DELETE);

  // Clobber spellings are target-specific source identities.
  AsmStmt.setDataPrototype("SgAsmStmt::AsmRegisterNameList",
                           "clobberRegisterList", "", NO_CONSTRUCTOR_PARAMETER,
                           NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  AsmStmt.setDataPrototype("bool", "isVolatile", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  NullStatement.setFunctionPrototype("HEADER_NULL_STATEMENT",
                                     "../Grammar/Statement.code");

  AttributedStatement.setFunctionPrototype("HEADER_ATTRIBUTED_STATEMENT",
                                           "../Grammar/Statement.code");
  AttributedStatement.setDataPrototype(
      "SgStatementAttributeList*", "attribute_list",
      "= new SgStatementAttributeList()", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      REQUIRED_TRAVERSAL_MEMBER);
  AttributedStatement.setDataPrototype(
      "SgStatement*", "statement", "= NULL", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);

  TypedefDeclaration.setFunctionPrototype(
      "HEADER_TYPEDEF_DECLARATION_STATEMENT", "../Grammar/Statement.code");
  TypedefDeclaration.setDataPrototype(
      "SgName", "name", "= \"\"", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
  TypedefDeclaration.setDataPrototype(
      "SgType*", "base_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, CLONE_TREE);

  TypedefDeclaration.setDataPrototype(
      "SgTypedefDeclaration::typedef_type_enum", "typedef_type",
      "= SgTypedefDeclaration::e_unknown", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TypedefDeclaration.setDataPrototype(
      "SgTypedefType*", "type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, CLONE_TREE);

  TypedefDeclaration.setDataPrototype(
      "SgDeclarationStatement*", "declaration", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER, COMPUTED_BASE_TYPE_DECLARATION_ACCESS);

  // Record a defining tag written within this typedef's base type.
  TypedefDeclaration.setDataPrototype(
      "bool", "typedefBaseTypeContainsDefiningDeclaration", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  TypedefDeclaration.setDataPrototype(
      "SgScopeStatement*", "scope", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  // Qualification accessors must not mark the declaration modified.
  TypedefDeclaration.setDataPrototype(
      "bool", "requiresGlobalNameQualificationOnType", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TypedefDeclaration.setDataPrototype(
      "int", "name_qualification_length_for_base_type", "= 0",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TypedefDeclaration.setDataPrototype(
      "bool", "type_elaboration_required_for_base_type", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TypedefDeclaration.setDataPrototype(
      "bool", "global_qualification_required_for_base_type", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Exact source spelling for the typedef/using base-type qualifier.  This is
  // producer-owned syntax, separate from qualification computed by an
  // unparse session.
  TypedefDeclaration.setDataPrototype(
      "bool", "source_base_type_qualification_present", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  TypedefDeclaration.setDataPrototype(
      "bool", "source_base_type_global_qualification", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  TypedefDeclaration.setDataPrototype(
      "SgStringList", "source_base_type_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // DQ (7/9/2012): Added information required to indicate that declaration is
  // contained in a type reference (as part of another declaration). This is
  // also called autonomous declaration handling.  The following examples make
  // clear the definition of autonomous and non autonomous declarations:
  //      class A { int i; }; typedef A a;    // autonomous declaration
  //      typedef class B { int i; } b;       // non autonomous declaration
  // Clearly the autonomous case is the more common, but we must handle both.
  TypedefDeclaration.setDataPrototype(
      "bool", "isAutonomousDeclaration", "= true", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (7/28/2012): We need to support lists of types at the end of a typedef
  // declaration. ROSE presently will output separate declarations for each
  // type.  This is OK (but will be fixed) however it interferes with the new
  // support for un-named types in typedef declarations.  so we want to mark
  // declarations that are output assocuated with a previous declaration.
  // Example of where this is relevant is:
  //      typedef struct { } S,T;
  // which will be output as:
  //      typedef struct { } S;
  //      typedef S T;
  // But the second (associated) declaration need to use the name of the
  // un-named type "S" See test2012_141.C for an example of this issue.
  TypedefDeclaration.setDataPrototype("bool", "isAssociatedWithDeclarationList",
                                      "= false", NO_CONSTRUCTOR_PARAMETER,
                                      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                      NO_DELETE);

  // DQ (4/10/2019): This is needed to support pointers to member type
  // variables.
  TypedefDeclaration.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (4/10/2019): This is needed to support pointers to member type
  // variables.
  TypedefDeclaration.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (4/10/2019): This is needed to support pointers to member type
  // variables.
  TypedefDeclaration.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  PragmaDeclaration.setFunctionPrototype("HEADER_PRAGMA_STATEMENT",
                                         "../Grammar/Statement.code");

  // DQ (3/17/2007): Since we traverse the SgPragma in a SgPragmaDeclaration,
  // don't delete it. OK to share in AST merge also.
  PragmaDeclaration.setDataPrototype(
      "SgPragma*", "pragma", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // Flang produces exact directive groups from its cooked-source provenance.
  // Keep that contract in canonical AST fields; an AstAttribute string map is
  // not a producer/consumer interface and is lost by AST serialization.
  PragmaDeclaration.setDataPrototype(
      "SgPragmaDeclaration::fortran_directive_family_enum",
      "fortran_directive_family",
      "= SgPragmaDeclaration::e_fortran_directive_none", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  PragmaDeclaration.setDataPrototype(
      "std::string", "fortran_directive_group_id", "= \"\"",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  PragmaDeclaration.setDataPrototype(
      "unsigned long", "fortran_directive_member_index", "= 0",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  PragmaDeclaration.setDataPrototype(
      "unsigned long", "fortran_directive_member_count", "= 0",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  PragmaDeclaration.setDataPrototype(
      "bool", "fortran_directive_primary", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  PragmaDeclaration.setDataPrototype(
      "std::string", "fortran_directive_raw_text", "= \"\"",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  PragmaDeclaration.setDataPrototype(
      "std::string", "fortran_directive_logical_text", "= \"\"",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  PragmaDeclaration.setDataPrototype(
      "std::string", "fortran_directive_semantic_text", "= \"\"",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  // The REX C/C++ preprocessor callback captures both normalized pragma text
  // and the exact source-spelled directive. Preserve the latter, plus whether
  // its top-level token stream came from a macro expansion, as typed frontend
  // provenance. This is not OpenMP-specific and does not enable Clang OpenMP
  // parsing.
  PragmaDeclaration.setDataPrototype(
      "std::string", "cxx_source_text", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  PragmaDeclaration.setDataPrototype(
      "SgPragmaDeclaration::cxx_pragma_payload_kind_enum",
      "cxx_pragma_payload_kind",
      "= SgPragmaDeclaration::e_cxx_pragma_payload_none",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  PragmaDeclaration.setDataPrototype("bool", "cxx_top_level_macro_expansion",
                                     "= false", NO_CONSTRUCTOR_PARAMETER,
                                     BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                     NO_DELETE);

  // DQ (3/22/2019): Adding EmptyDeclaration to support addition of comments and
  // CPP directives that will permit token-based unparsing to work with greater
  // precision. For example, used to add an include directive with greater
  // precision to the global scope and permit the unparsing via the token stream
  // to be used as well.
  EmptyDeclaration.setFunctionPrototype("HEADER_EMPTY_DECLARATION",
                                        "../Grammar/Statement.code");
  EmptyDeclaration.setDataPrototype(
      "SgEmptyDeclaration::empty_declaration_role_enum", "lexical_role", "",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  AccessLabelStatement.setFunctionPrototype("HEADER_ACCESS_LABEL_STATEMENT",
                                            "../Grammar/Statement.code");
  AccessLabelStatement.setDataPrototype(
      "SgAccessLabelStatement::access_label_kind_enum", "label_kind",
      "= SgAccessLabelStatement::e_access_label_private", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // A comma-separated declaration is one grammar surface even when Clang
  // exposes heterogeneous declarators as separate semantic declarations.  The
  // group owns that surface and its ordered members; per-declaration string
  // identities and sibling scans are intentionally not part of the model.
  DeclarationGroupStatement.setFunctionPrototype(
      "HEADER_DECLARATION_GROUP_STATEMENT", "../Grammar/Statement.code");
  DeclarationGroupStatement.setDataPrototype(
      "SgDeclarationStatementPtrList", "declarations", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE);
  DeclarationGroupStatement.setDataPrototype(
      "SgScopeStatement*", "scope", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  DeclarationGroupStatement.setDataPrototype(
      "SgDeclarationGroupStatement::source_terminator_enum",
      "source_terminator",
      "= SgDeclarationGroupStatement::e_source_terminator_file_semicolon",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // DQ (4/22/2004): Support for namespaces: NamespaceDeclarationStatement,
  //      NamespaceAliasDeclarationStatement, UsingDeclarationStatement
  NamespaceDeclarationStatement.setFunctionPrototype(
      "HEADER_NAMESPACE_DECLARATION_STATEMENT", "../Grammar/Statement.code");
  NamespaceDeclarationStatement.setDataPrototype(
      "SgName", "name", "= \"\"", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
  NamespaceDeclarationStatement.setDataPrototype(
      "SgNamespaceDefinitionStatement*", "definition", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_AFTER_OWNER_CONSTRUCTION);
  NamespaceDeclarationStatement.setDataPrototype(
      "bool", "isUnnamedNamespace", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (8/12/2014): Adding support for C++11 inlined namespaces.
  NamespaceDeclarationStatement.setDataPrototype(
      "bool", "isInlinedNamespace", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NamespaceDeclarationStatement.setDataPrototype(
      "SgNamespaceSourceFragment*", "opening_introducer_source_fragment",
      "= NULL", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);
  NamespaceDeclarationStatement.setDataPrototype(
      "SgNamespaceSourceFragment*", "opening_source_fragment", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);
  NamespaceDeclarationStatement.setDataPrototype(
      "SgNamespaceSourceFragment*", "closing_source_fragment", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);

  NamespaceAliasDeclarationStatement.setFunctionPrototype(
      "HEADER_NAMESPACE_ALIAS_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");
  NamespaceAliasDeclarationStatement.setDataPrototype(
      "SgName", "name", "= \"\"", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
  NamespaceAliasDeclarationStatement.setDataPrototype(
      "SgNamespaceDeclarationStatement*", "namespaceDeclaration", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Qualification accessors must not mark the declaration modified.
  NamespaceAliasDeclarationStatement.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NamespaceAliasDeclarationStatement.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NamespaceAliasDeclarationStatement.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // namespaceDeclaration is the semantic target; namespaceAliasDeclaration
  // preserves an alias target written in the source. Both are borrowed links.
  NamespaceAliasDeclarationStatement.setDataPrototype(
      "bool", "is_alias_for_another_namespace_alias", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  NamespaceAliasDeclarationStatement.setDataPrototype(
      "SgNamespaceAliasDeclarationStatement*", "namespaceAliasDeclaration",
      "= NULL", NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  NamespaceDefinitionStatement.setFunctionPrototype(
      "HEADER_NAMESPACE_DEFINITION_STATEMENT", "../Grammar/Statement.code");
  NamespaceDefinitionStatement.editSubstitute("HEADER_LIST_DECLARATIONS",
                                              "HEADER_LIST_DECLARATIONS",
                                              "../Grammar/Statement.code");
  NamespaceDefinitionStatement.editSubstitute("LIST_DATA_TYPE",
                                              "SgDeclarationStatementPtrList");
  NamespaceDefinitionStatement.editSubstitute("LIST_NAME", "declarations");
  NamespaceDefinitionStatement.editSubstitute("LIST_FUNCTION_RETURN_TYPE",
                                              "void");
  NamespaceDefinitionStatement.editSubstitute("LIST_FUNCTION_NAME",
                                              "declaration");
  NamespaceDefinitionStatement.editSubstitute("LIST_ELEMENT_DATA_TYPE",
                                              "SgDeclarationStatement*");
  NamespaceDefinitionStatement.setDataPrototype(
      "SgDeclarationStatementPtrList", "declarations", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);
  NamespaceDefinitionStatement.setDataPrototype(
      "SgNamespaceDeclarationStatement*", "namespaceDeclaration", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      NO_COPY_DATA);

  // DQ (7/23/2011): Pointers to previous and next matching namespace
  // definitions (C++ namespaces are reintrant).  The hand-written setters
  // advance the reopening-chain revision.
  NamespaceDefinitionStatement.setDataPrototype(
      "SgNamespaceDefinitionStatement*", "previousNamespaceDefinition",
      "= NULL", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE, COPY_REFERENCE);
  NamespaceDefinitionStatement.setDataPrototype(
      "SgNamespaceDefinitionStatement*", "nextNamespaceDefinition", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);
  // A destroyed definition leaves every chain it belonged to, and its address
  // may be reused by an unrelated chain.
  NamespaceDefinitionStatement.setDestructorPrologue(
      "advanceReopeningChainRevision();");

  // DQ (5/16/2013): This is where we put the union of all of the symbols for
  // all of the namespace definitions that are logically the same namespace, but
  // separated structurally because C++ namespaces can be re-entrant. Note that
  // only SgAliasSymbols are put here and that the original symbol is placed
  // into the namespace definition associated with its declaration.
  NamespaceDefinitionStatement.setDataPrototype(
      "SgNamespaceDefinitionStatement*", "global_definition", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);
  NamespaceDefinitionStatement.setDataPrototype(
      "bool", "isUnionOfReentrantNamespaceDefinitions", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  UsingDeclarationStatement.setFunctionPrototype(
      "HEADER_USING_DECLARATION_STATEMENT", "../Grammar/Statement.code");
  UsingDeclarationStatement.setDataPrototype(
      "SgDeclarationStatement*", "declaration", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UsingDeclarationStatement.setDataPrototype(
      "SgInitializedName*", "initializedName", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, DELETE_IF_OWNED, CLONE_TREE);

  // Qualification accessors must not mark the declaration modified.
  UsingDeclarationStatement.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  UsingDeclarationStatement.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  UsingDeclarationStatement.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (1/10/2019): Adding support for C++11 inheriting constructor.
  UsingDeclarationStatement.setDataPrototype(
      "bool", "is_inheriting_constructor", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // The terminal token sequence is lexical source provenance. In particular,
  // an inheriting-constructor using declaration must not recover this token by
  // truncating a semantic template-specialization name in the backend.
  UsingDeclarationStatement.setDataPrototype(
      "SgName", "source_terminal_name", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UsingDeclarationStatement.setDataPrototype(
      "bool", "source_has_typename", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UsingDeclarationStatement.setDataPrototype(
      "bool", "source_has_pack_expansion", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  UsingDirectiveStatement.setFunctionPrototype(
      "HEADER_USING_DIRECTIVE_STATEMENT", "../Grammar/Statement.code");
  UsingDirectiveStatement.setDataPrototype(
      "SgNamespaceDeclarationStatement*", "namespaceDeclaration", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Qualification accessors must not mark the declaration modified.
  UsingDirectiveStatement.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  UsingDirectiveStatement.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  UsingDirectiveStatement.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ProgramHeaderStatement.setFunctionPrototype("HEADER_PROGRAM_HEADER_STATEMENT",
                                              "../Grammar/Statement.code");

  ProgramHeaderStatement.setDataPrototype(
      "SgProgramHeaderStatement::program_statement_kind_enum",
      "program_statement_kind",
      "= SgProgramHeaderStatement::e_unknown_program_statement_kind",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // Preserve the exact source spelling of the optional name on END PROGRAM.
  // The inherited function name is the opening PROGRAM name and cannot encode
  // independently cased Fortran source spelling.
  ProgramHeaderStatement.setDataPrototype(
      "SgName", "end_statement_name", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ProgramHeaderStatement.setDataPrototype(
      "SgLabelRefExp*", "end_numeric_label", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  ProcedureHeaderStatement.setFunctionPrototype(
      "HEADER_PROCEDURE_HEADER_STATEMENT", "../Grammar/Statement.code");
  ProcedureHeaderStatement.setDataPrototype(
      "SgProcedureHeaderStatement::subprogram_kind_enum", "subprogram_kind",
      "= SgProcedureHeaderStatement::e_unknown_kind", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Exact external identity supplied by Flang semantic analysis. An empty
  // name denotes no external binding, including BIND(C, NAME="") and internal
  // BIND(C) procedures. It is independent of the source NAME expression.
  ProcedureHeaderStatement.setDataPrototype(
      "SgName", "fortran_binding_name", "= SgName()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // BLOCK DATA permits an optional source-level name. Keep that syntax fact
  // separate from the declaration's internal symbol-table identity.
  ProcedureHeaderStatement.setDataPrototype(
      "SgProcedureHeaderStatement::block_data_name_kind_enum",
      "block_data_name_kind",
      "= SgProcedureHeaderStatement::e_unknown_block_data_name_kind",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // Keep semantic-only procedure identities distinct from each exact
  // source-written procedure surface.  Backends must never infer a type
  // declaration from declaration modifiers or structural accidents.
  ProcedureHeaderStatement.setDataPrototype(
      "SgProcedureHeaderStatement::fortran_procedure_source_form_enum",
      "fortran_procedure_source_form",
      "= SgProcedureHeaderStatement::"
      "e_fortran_procedure_source_form_unknown",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  ProcedureHeaderStatement.setDataPrototype(
      "SgProcedureHeaderStatement::fortran_result_type_spec_enum",
      "fortran_result_type_spec",
      "= SgProcedureHeaderStatement::e_fortran_result_type_spec_unknown",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // Preserve the exact visible symbol named by a derived function-result
  // prefix.  The canonical class type cannot encode a local USE rename.
  ProcedureHeaderStatement.setDataPrototype(
      "SgSymbol*", "fortran_source_derived_type_symbol", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  ProcedureHeaderStatement.setDataPrototype(
      "SgLabelRefExp*", "end_numeric_label", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  // This is a non-owning semantic reference to an initialized name whose
  // exact structural owner is its variable declaration in the function body
  // or parameter scope.  Traversing it here duplicates that node and bypasses
  // the declaration's lexical source-order position.
  ProcedureHeaderStatement.setDataPrototype(
      "SgInitializedName*", "result_name", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  // An ENTRY result is likewise a non-owning reference to a declaration-owned
  // initialized name and must not create a second traversal edge.
  EntryStatement.setFunctionPrototype("HEADER_ENTRY_STATEMENT",
                                      "../Grammar/Statement.code");
  EntryStatement.setDataPrototype(
      "SgInitializedName*", "result_name", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  InterfaceStatement.setFunctionPrototype("HEADER_INTERFACE_STATEMENT",
                                          "../Grammar/Statement.code");
  InterfaceStatement.setDataPrototype(
      "SgName", "name", "= \"\"", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
  InterfaceStatement.setDataPrototype(
      "SgInterfaceStatement::generic_spec_enum", "generic_spec",
      "= SgInterfaceStatement::e_default_interface_type", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InterfaceStatement.setDataPrototype(
      "SgInterfaceBodyPtrList", "interface_body_list", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);

  InterfaceStatement.setDataPrototype(
      "SgLabelRefExp*", "end_numeric_label", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  CommonBlock.setFunctionPrototype("HEADER_COMMON_BLOCK_STATEMENT",
                                   "../Grammar/Statement.code");
  CommonBlock.setDataPrototype("SgCommonBlockObjectPtrList", "block_list", "",
                               // Liao 12/8/2010, This must be traversable to
                               // reach variable references underneath it.
                               NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                               DEF_TRAVERSAL, NO_DELETE);
  CommonBlock.setAutomaticGenerationOfDestructor(false);

  NamelistStatement.setFunctionPrototype("HEADER_NAMELIST_STATEMENT",
                                         "../Grammar/Statement.code");
  NamelistStatement.setDataPrototype("SgNameGroupPtrList", "group_list", "",
                                     CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                     DEF_TRAVERSAL, NO_DELETE, COPY_DATA);

  // C++ module imports and Fortran host-association import statements.
  ImportStatement.setFunctionPrototype("HEADER_IMPORT_STATEMENT",
                                       "../Grammar/Statement.code");
  ImportStatement.setDataPrototype(
      "SgName", "module_name", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ImportStatement.setDataPrototype(
      "bool", "is_cxx_module_import", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ImportStatement.setDataPrototype(
      "bool", "is_cxx_header_unit_import", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ImportStatement.setDataPrototype(
      "SgStringList", "import_list", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ImportStatement.setDataPrototype(
      "SgImportStatement::fortran_import_kind_enum", "fortran_import_kind", "",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  AssociateStatement.setFunctionPrototype("HEADER_ASSOCIATE_STATEMENT",
                                          "../Grammar/Statement.code");
  // Associations own their declarations and selector expressions. Traverse
  // them before the body so copies include every body-symbol basis.
  AssociateStatement.editSubstitute("HEADER_LIST_DECLARATIONS",
                                    "HEADER_LIST_DECLARATIONS",
                                    "../Grammar/Statement.code");
  AssociateStatement.editSubstitute("LIST_DATA_TYPE",
                                    "SgDeclarationStatementPtrList");
  AssociateStatement.editSubstitute("LIST_NAME", "associates");
  AssociateStatement.editSubstitute("LIST_FUNCTION_RETURN_TYPE", "void");
  AssociateStatement.editSubstitute("LIST_FUNCTION_NAME", "associate");
  AssociateStatement.editSubstitute("LIST_ELEMENT_DATA_TYPE",
                                    "SgDeclarationStatement*");
  AssociateStatement.setDataPrototype(
      "SgDeclarationStatementPtrList", "associates", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE);
  AssociateStatement.setDataPrototype(
      "SgBasicBlock*", "body", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  AssociateStatement.setDataPrototype(
      "SgLabelRefExp*", "end_numeric_label", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  ModuleStatement.setFunctionPrototype("HEADER_MODULE_STATEMENT",
                                       "../Grammar/Statement.code");
  ModuleStatement.setDataPrototype(
      "SgLabelRefExp*", "end_numeric_label", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  ModuleStatement.setDataPrototype(
      "SgModuleStatement::fortran_module_kind_enum", "fortran_module_kind",
      "= SgModuleStatement::e_fortran_module", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ModuleStatement.setDataPrototype(
      "SgName", "fortran_submodule_parent", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  UseStatement.setFunctionPrototype("HEADER_USE_STATEMENT",
                                    "../Grammar/Statement.code");
  UseStatement.setDataPrototype("SgName", "name", "= \"\"",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  UseStatement.setDataPrototype("bool", "only_option", "= false",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  UseStatement.setDataPrototype("std::string", "module_nature", "= \"\"",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  UseStatement.setDataPrototype(
      "SgRenamePairPtrList", "rename_list", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  UseStatement.setDataPrototype("SgModuleStatement*", "module", "= NULL",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);

  // DQ (3/22/2007): Note that the FortranDo statement is not a scope and that I
  // have changed the Rice version to use SgExpressions where they had
  // SgStatement (to conform to the grammar). This is the concept of "labeled or
  // non-labeled do" and "block do"
  FortranDo.setFunctionPrototype("HEADER_FORTRAN_DO",
                                 "../Grammar/Statement.code");
  FortranDo.setDataPrototype("SgExpression*", "initialization", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);
  FortranDo.setDataPrototype("SgExpression*", "bound", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);
  FortranDo.setDataPrototype("SgExpression*", "increment", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);
  FortranDo.setDataPrototype("SgBasicBlock*", "body", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);
  FortranDo.setDataPrototype("SgLabelRefExp*", "end_numeric_label", "= NULL",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
                             OPTIONAL_TRAVERSAL_MEMBER);
  FortranDo.setDataPrototype("std::string", "string_label", "= \"\"",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  FortranDo.setDataPrototype("bool", "has_end_statement", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  // Fortran 95 specific construct (different from C/C++ for loop).
  // Rasmussen (10/25/2018): Added forall_statement_kind_enum to allow
  // specifying as a DO CONCURRENT statement
  ForAllStatement.setFunctionPrototype("HEADER_FORALL_STATEMENT",
                                       "../Grammar/Statement.code");
  ForAllStatement.setDataPrototype(
      "SgExprListExp*", "forall_header", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  ForAllStatement.setDataPrototype(
      "SgBasicBlock*", "body", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  ForAllStatement.setDataPrototype(
      "bool", "has_end_statement", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ForAllStatement.setDataPrototype(
      "std::string", "string_label", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ForAllStatement.setDataPrototype(
      "SgLabelRefExp*", "end_numeric_label", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  ForAllStatement.setDataPrototype(
      "SgForAllStatement::forall_statement_kind_enum", "forall_statement_kind",
      "= SgForAllStatement::e_forall_statement", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (8/14/2007): Added new data members to Fortran IR nodes.
  // Rasmussen (7/11/2020): Added F2018 addition, quiet
  ProcessControlStatement.setFunctionPrototype(
      "HEADER_PROCESS_CONTROL_STATEMENT", "../Grammar/Statement.code");
  ProcessControlStatement.setDataPrototype(
      "SgProcessControlStatement::control_enum", "control_kind",
      "= SgProcessControlStatement::e_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ProcessControlStatement.setDataPrototype(
      "SgExpression*", "code", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ProcessControlStatement.setDataPrototype(
      "SgExpression*", "quiet", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // DQ (8/14/2007): Added new data members to Fortran IR nodes.
  IOStatement.setFunctionPrototype("HEADER_IO_STATEMENT",
                                   "../Grammar/Statement.code");
  IOStatement.setDataPrototype("SgExprListExp*", "io_stmt_list", "= NULL",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                               OPTIONAL_TRAVERSAL_MEMBER);
  IOStatement.setDataPrototype("SgExpression*", "unit", "= NULL",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                               OPTIONAL_TRAVERSAL_MEMBER);
  IOStatement.setDataPrototype("SgExpression*", "iostat", "= NULL",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                               OPTIONAL_TRAVERSAL_MEMBER);
  IOStatement.setDataPrototype("SgExpression*", "err", "= NULL",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                               OPTIONAL_TRAVERSAL_MEMBER);
  IOStatement.setDataPrototype("SgExpression*", "iomsg", "= NULL",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                               OPTIONAL_TRAVERSAL_MEMBER);

  // Rasmussen (9/20/2018): Added F2018 image control statements
  ImageControlStatement.setFunctionPrototype("HEADER_IMAGE_CONTROL_STATEMENT",
                                             "../Grammar/Statement.code");
  ImageControlStatement.setDataPrototype(
      "SgImageControlStatement::image_control_statement_enum",
      "image_control_statement", "= SgImageControlStatement::e_unknown",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  ImageControlStatement.setDataPrototype(
      "SgExpression*", "stat", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);
  ImageControlStatement.setDataPrototype(
      "SgExpression*", "err_msg", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);
  ImageControlStatement.setDataPrototype(
      "SgExpression*", "acquired_lock", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);

  // Derived from ImageControlStatement
  SyncAllStatement.setFunctionPrototype("HEADER_SYNC_ALL_STATEMENT",
                                        "../Grammar/Statement.code");
  SyncImagesStatement.setFunctionPrototype("HEADER_SYNC_IMAGES_STATEMENT",
                                           "../Grammar/Statement.code");
  SyncImagesStatement.setDataPrototype(
      "SgExpression*", "image_set", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);
  SyncImagesStatement.setDataPrototype(
      "bool", "all_images", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  SyncMemoryStatement.setFunctionPrototype("HEADER_SYNC_MEMORY_STATEMENT",
                                           "../Grammar/Statement.code");
  SyncTeamStatement.setFunctionPrototype("HEADER_SYNC_TEAM_STATEMENT",
                                         "../Grammar/Statement.code");
  SyncTeamStatement.setDataPrototype(
      "SgExpression*", "team_value", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE,
      REQUIRED_TRAVERSAL_MEMBER);
  FormTeamStatement.setFunctionPrototype("HEADER_FORM_TEAM_STATEMENT",
                                         "../Grammar/Statement.code");
  FormTeamStatement.setDataPrototype(
      "SgExpression*", "team_number", "= nullptr", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE,
      REQUIRED_TRAVERSAL_MEMBER);
  FormTeamStatement.setDataPrototype(
      "SgExpression*", "team_variable", "= nullptr", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE,
      REQUIRED_TRAVERSAL_MEMBER);
  FormTeamStatement.setDataPrototype(
      "SgExpression*", "new_index", "= nullptr", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);
  LockStatement.setFunctionPrototype("HEADER_LOCK_STATEMENT",
                                     "../Grammar/Statement.code");
  LockStatement.setDataPrototype("SgExpression*", "lock_variable", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE,
                                 REQUIRED_TRAVERSAL_MEMBER);
  UnlockStatement.setFunctionPrototype("HEADER_UNLOCK_STATEMENT",
                                       "../Grammar/Statement.code");
  UnlockStatement.setDataPrototype(
      "SgExpression*", "lock_variable", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE,
      REQUIRED_TRAVERSAL_MEMBER);

  // Derived from IOStatement, this adds the status (either "KEEP" or "DELETE")
  CloseStatement.setFunctionPrototype("HEADER_CLOSE_STATEMENT",
                                      "../Grammar/Statement.code");
  CloseStatement.setDataPrototype(
      "SgExpression*", "status", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // The Fortran print only requires a format field (and not the unit, iostat,
  // err fields)
  PrintStatement.setFunctionPrototype("HEADER_PRINT_STATEMENT",
                                      "../Grammar/Statement.code");
  PrintStatement.setDataPrototype(
      "SgExpression*", "format", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  FlushStatement.setFunctionPrototype("HEADER_FLUSH_STATEMENT",
                                      "../Grammar/Statement.code");
  RewindStatement.setFunctionPrototype("HEADER_REWIND_STATEMENT",
                                       "../Grammar/Statement.code");
  BackspaceStatement.setFunctionPrototype("HEADER_BACKSPACE_STATEMENT",
                                          "../Grammar/Statement.code");
  EndfileStatement.setFunctionPrototype("HEADER_ENDFILE_STATEMENT",
                                        "../Grammar/Statement.code");
  WaitStatement.setFunctionPrototype("HEADER_WAIT_STATEMENT",
                                     "../Grammar/Statement.code");
  WaitStatement.setDataPrototype(
      "SgExpression*", "end", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WaitStatement.setDataPrototype(
      "SgExpression*", "eor", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WaitStatement.setDataPrototype(
      "SgExpression*", "id", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // Derived from IOStatement, this adds:
  OpenStatement.setFunctionPrototype("HEADER_OPEN_STATEMENT",
                                     "../Grammar/Statement.code");
  OpenStatement.setDataPrototype(
      "SgExpression*", "file", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  OpenStatement.setDataPrototype(
      "SgExpression*", "status", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  OpenStatement.setDataPrototype(
      "SgExpression*", "access", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  OpenStatement.setDataPrototype(
      "SgExpression*", "form", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  OpenStatement.setDataPrototype(
      "SgExpression*", "recl", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  OpenStatement.setDataPrototype(
      "SgExpression*", "blank", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  OpenStatement.setDataPrototype(
      "SgExpression*", "position", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  OpenStatement.setDataPrototype(
      "SgExpression*", "action", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  OpenStatement.setDataPrototype(
      "SgExpression*", "delim", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  OpenStatement.setDataPrototype(
      "SgExpression*", "pad", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // F2003 features
  OpenStatement.setDataPrototype(
      "SgExpression*", "round", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  OpenStatement.setDataPrototype(
      "SgExpression*", "sign", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  OpenStatement.setDataPrototype(
      "SgExpression*", "asynchronous", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // Derived from IOStatement, this adds:
  // Added missing items to the io-control-spec-list [CR, 2019.05.31]
  WriteStatement.setFunctionPrototype("HEADER_WRITE_STATEMENT",
                                      "../Grammar/Statement.code");
  WriteStatement.setDataPrototype(
      "SgExpression*", "format", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgSymbol*", "namelist", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_CONSTRUCTOR_REFERENCE);
  WriteStatement.setDataPrototype(
      "SgExpression*", "advance", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "asynchronous", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "blank", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "decimal", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "delim", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "end", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "eor", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "id", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "pad", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "pos", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "rec", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "round", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "sign", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WriteStatement.setDataPrototype(
      "SgExpression*", "size", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // Derived from IOStatement, this adds:
  // Added missing items to the io-control-spec-list [CR, 2019.05.31]
  ReadStatement.setFunctionPrototype("HEADER_READ_STATEMENT",
                                     "../Grammar/Statement.code");
  ReadStatement.setDataPrototype(
      "SgExpression*", "format", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgSymbol*", "namelist", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_CONSTRUCTOR_REFERENCE);
  ReadStatement.setDataPrototype(
      "SgExpression*", "advance", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "asynchronous", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "blank", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "decimal", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "delim", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "end", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "eor", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "id", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "pad", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "pos", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "rec", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "round", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "sign", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ReadStatement.setDataPrototype(
      "SgExpression*", "size", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // Derived from IOStatement, this adds (the most complex I/O statement):
  InquireStatement.setFunctionPrototype("HEADER_INQUIRE_STATEMENT",
                                        "../Grammar/Statement.code");
  InquireStatement.setDataPrototype(
      "SgExpression*", "file", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "access", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "form", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "recl", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "blank", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "exist", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "opened", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "number", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "named", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "name", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "sequential", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "direct", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "formatted", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "unformatted", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "nextrec", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "position", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "action", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "read", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "write", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "readwrite", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "delim", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "pad", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // F2003 features
  InquireStatement.setDataPrototype(
      "SgExpression*", "asynchronous", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "decimal", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "stream", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "size", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "pending", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InquireStatement.setDataPrototype(
      "SgExpression*", "id", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (9/3/2010): Added to handle case of "INQUIRE (IOLENGTH=<VarRefExp>)"
  InquireStatement.setDataPrototype(
      "SgVarRefExp*", "iolengthExp", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // More IR nodes required for Fortran support
  ImplicitStatement.setFunctionPrototype("HEADER_IMPLICIT_STATEMENT",
                                         "../Grammar/Statement.code");
  StatementFunctionStatement.setFunctionPrototype(
      "HEADER_STATEMENT_FUNCTION_STATEMENT", "../Grammar/Statement.code");
  WhereStatement.setFunctionPrototype("HEADER_WHERE_STATEMENT",
                                      "../Grammar/Statement.code");
  ElseWhereStatement.setFunctionPrototype("HEADER_ELSE_WHERE_STATEMENT",
                                          "../Grammar/Statement.code");
  EquivalenceStatement.setFunctionPrototype("HEADER_EQUIVALENCE_STATEMENT",
                                            "../Grammar/Statement.code");
  DerivedTypeStatement.setFunctionPrototype("HEADER_DERIVED_TYPE_STATEMENT",
                                            "../Grammar/Statement.code");

  AttributeSpecificationStatement.setFunctionPrototype(
      "HEADER_ATTRIBUTE_SPECIFICATION_STATEMENT", "../Grammar/Statement.code");
  ContainsStatement.setFunctionPrototype("HEADER_CONTAINS_STATEMENT",
                                         "../Grammar/Statement.code");
  // Required for F77 support (but deprecated in F90)
  ArithmeticIfStatement.setFunctionPrototype("HEADER_ARITHMETIC_IF_STATEMENT",
                                             "../Grammar/Statement.code");
  ComputedGotoStatement.setFunctionPrototype("HEADER_COMPUTED_GOTO_STATEMENT",
                                             "../Grammar/Statement.code");
  ImplicitStatement.setDataPrototype(
      "SgImplicitStatement::implicit_spec_enum", "implicit_spec",
      "= SgImplicitStatement::e_unknown_implicit_spec", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ImplicitStatement.setDataPrototype(
      "SgFortranImplicitRangePtrList", "ranges", "= {}", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE);

  StatementFunctionStatement.setDataPrototype(
      "SgFunctionDeclaration*", "function", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  StatementFunctionStatement.setDataPrototype(
      "SgExpression*", "expression", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      REQUIRED_TRAVERSAL_MEMBER);

  WhereStatement.setDataPrototype("SgExpression*", "condition", "= NULL",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                  OPTIONAL_TRAVERSAL_MEMBER);
  WhereStatement.setDataPrototype("SgBasicBlock*", "body", "= NULL",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                  OPTIONAL_TRAVERSAL_MEMBER);
  WhereStatement.setDataPrototype(
      "SgElseWhereStatement*", "elsewhere", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  WhereStatement.setDataPrototype(
      "SgLabelRefExp*", "end_numeric_label", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  // DQ (11/16/2007): Added support for string labels (for Fortran).
  WhereStatement.setDataPrototype(
      "std::string", "string_label", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  WhereStatement.setDataPrototype(
      "bool", "has_end_statement", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ElseWhereStatement.setDataPrototype(
      "SgExpression*", "condition", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ElseWhereStatement.setDataPrototype(
      "SgBasicBlock*", "body", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ElseWhereStatement.setDataPrototype(
      "SgElseWhereStatement*", "elsewhere", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  NullifyStatement.setDataPrototype(
      "SgExprListExp*", "pointer_list", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      REQUIRED_TRAVERSAL_MEMBER);

  EquivalenceStatement.setDataPrototype(
      "SgExprListExp*", "equivalence_set_list", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      REQUIRED_TRAVERSAL_MEMBER);

  DerivedTypeStatement.setDataPrototype(
      "SgLabelRefExp*", "end_numeric_label", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  AttributeSpecificationStatement.setDataPrototype(
      "SgStringList", "name_list", "", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  AttributeSpecificationStatement.setDataPrototype(
      "SgAttributeSpecificationStatement::attribute_spec_enum",
      "attribute_kind",
      "= SgAttributeSpecificationStatement::e_unknown_attribute_spec",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  AttributeSpecificationStatement.setDataPrototype(
      "SgExprListExp*", "parameter_list", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  // DQ (11/20/2007): Added support for data statements
  AttributeSpecificationStatement.setDataPrototype(
      "SgDataStatementGroupPtrList", "data_statement_group_list", "= {}",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE);
  AttributeSpecificationStatement.setDataPrototype(
      "SgExprListExp*", "bind_list", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (12/1/2007): Addes support for dimension statement
  AttributeSpecificationStatement.setDataPrototype(
      "SgDimensionObjectPtrList", "dimension_object_list", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  AllocateStatement.setDataPrototype(
      "SgExprListExp*", "expr_list", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      REQUIRED_TRAVERSAL_MEMBER);
  AllocateStatement.setDataPrototype(
      "SgExpression*", "stat_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  AllocateStatement.setDataPrototype(
      "SgExpression*", "errmsg_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  AllocateStatement.setDataPrototype(
      "SgExpression*", "source_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  AllocateStatement.setDataPrototype(
      "SgExpression*", "mold_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  AllocateStatement.setDataPrototype(
      "SgExpression*", "stream_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  AllocateStatement.setDataPrototype(
      "SgExpression*", "pinned_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  DeallocateStatement.setDataPrototype(
      "SgExprListExp*", "expr_list", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      REQUIRED_TRAVERSAL_MEMBER);
  DeallocateStatement.setDataPrototype(
      "SgExpression*", "stat_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  DeallocateStatement.setDataPrototype(
      "SgExpression*", "errmsg_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // Required for F77 support (but deprecated in F90)
  ArithmeticIfStatement.setDataPrototype(
      "SgExpression*", "conditional", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ArithmeticIfStatement.setDataPrototype(
      "SgLabelRefExp*", "less_label", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  ArithmeticIfStatement.setDataPrototype(
      "SgLabelRefExp*", "equal_label", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  ArithmeticIfStatement.setDataPrototype(
      "SgLabelRefExp*", "greater_label", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  AssignStatement.setDataPrototype(
      "SgLabelRefExp*", "label", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  AssignStatement.setDataPrototype(
      "SgExpression*", "value", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  ComputedGotoStatement.setDataPrototype(
      "SgExprListExp*", "labelList", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ComputedGotoStatement.setDataPrototype(
      "SgExpression*", "label_index", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  AssignedGotoStatement.setDataPrototype(
      "SgExprListExp*", "targets", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  FormatStatement.setFunctionPrototype("HEADER_FORMAT_STATEMENT",
                                       "../Grammar/Statement.code");
  FormatStatement.setDataPrototype("SgFormatItemList*", "format_item_list", "",
                                   CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                   DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
                                   REQUIRED_TRAVERSAL_MEMBER);

  // DQ (2/18/2008): Added support for Fortran specific include mechanism
  // (separate from CPP include mechanism which is language independent).
  FortranIncludeLine.setFunctionPrototype("HEADER_FORTRAN_INCLUDE_LINE",
                                          "../Grammar/Statement.code");
  FortranIncludeLine.setDataPrototype(
      "std::string", "filename", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (11/23/2008): Added support for CPP directives as IR nodes.
  C_PreprocessorDirectiveStatement.setFunctionPrototype(
      "HEADER_PREPROCESSOR_DIRECTIVE_STATEMENT", "../Grammar/Statement.code");

  // DQ (11/23/2008): After we have these into the AST, then we can decide if we
  // want more information to be stored. Examples of more information could
  // include:
  //    1) the substring after the #<directive specification>,
  //    2) the integer values used for compiler generate line numbers, etc.
  // For now the "directiveString" stores the full line represented by the CPP
  // directive.
  C_PreprocessorDirectiveStatement.setDataPrototype(
      "std::string", "directiveString", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // negara1 (08/12/2011): Added header file body as a field of include
  // directive statement.
  IncludeDirectiveStatement.setDataPrototype(
      "SgHeaderFileBody*", "headerFileBody", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // DQ (11/5/2018): Pass the name that was used to the
  // SgIncludeDirectiveStatement.
  IncludeDirectiveStatement.setDataPrototype(
      "std::string", "name_used_in_include_directive", "= \"\"",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // DQ (11/28/2008): Thes are used to mark line numbers in generated CPP
  // output.
  ClinkageDeclarationStatement.setFunctionPrototype(
      "HEADER_CLINKAGE_DECLARATION_STATEMENT", "../Grammar/Statement.code");
  LinemarkerDirectiveStatement.setDataPrototype(
      "int", "linenumber", "= -1", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  LinemarkerDirectiveStatement.setDataPrototype(
      "std::string", "filename", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // The integer values are stored as char (values are '1', '2', '3', '4'), flag
  // values are separated by spaces.
  LinemarkerDirectiveStatement.setDataPrototype(
      "SgUnsignedCharList", "flaglist", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Support for extern "C" and extern "C++"
  ClinkageDeclarationStatement.setDataPrototype(
      "std::string", "languageSpecifier", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // ***********************************************************************
  // ***********************************************************************
  //                       Source Code Definition
  // ***********************************************************************
  // ***********************************************************************

  Statement.setFunctionSource("SOURCE_MAIN_STATEMENT",
                              "../Grammar/Statement.code");

  ScopeStatement.setFunctionSource("SOURCE_SCOPE_STATEMENT",
                                   "../Grammar/Statement.code");
  FunctionTypeTable.setFunctionSource("SOURCE_FUNCTION_TYPE_TABLE_STATEMENT",
                                      "../Grammar/Statement.code");
  Global.setFunctionSource("SOURCE_GLOBAL_STATEMENT",
                           "../Grammar/Statement.code");

  DeclarationStatement.setFunctionSource("SOURCE_DECLARATION_STATEMENT",
                                         "../Grammar/Statement.code");
  BasicBlock.setFunctionSource("SOURCE_BASIC_BLOCK_STATEMENT",
                               "../Grammar/Statement.code");
  IfStmt.setFunctionSource("SOURCE_IF_STATEMENT", "../Grammar/Statement.code");
  ForStatement.setFunctionSource("SOURCE_FOR_STATEMENT",
                                 "../Grammar/Statement.code");
  // ForStatement.editSubstitute            ( "$CLASSNAME", "SgForStatement" );
  ForStatement.editSubstitute("get_body", "get_loop_body");

  // DQ (3/25/2018): Adding Range-based For statement support (previously missed
  // in C++11 support).
  RangeBasedForStatement.setFunctionSource("SOURCE_RANGE_BASED_FOR_STATEMENT",
                                           "../Grammar/Statement.code");

  // DQ (7/25/2014): Adding support for C11 static assertions.
  StaticAssertionDeclaration.setFunctionSource(
      "SOURCE_STATIC_ASSERTION_DECLARATION", "../Grammar/Statement.code");

  FriendTypeDeclaration.setFunctionSource("SOURCE_FRIEND_TYPE_DECLARATION",
                                          "../Grammar/Statement.code");

  // DQ (2/10/2016): Adding support for a scope for function parametes in
  // non-defining declarations.
  FunctionParameterScope.setFunctionSource("SOURCE_FUNCTION_PARAMETER_SCOPE",
                                           "../Grammar/Statement.code");

  // DQ (7/17/2017): Added to support concept of scope in nondefining
  // declarations (required for more sophisticated level of template useage).
  DeclarationScope.setFunctionSource("SOURCE_DECLARATION_SCOPE",
                                     "../Grammar/Statement.code");

  FunctionDeclaration.setFunctionSource("SOURCE_FUNCTION_DECLARATION_STATEMENT",
                                        "../Grammar/Statement.code");
  FunctionDeclaration.setFunctionSource(
      "SOURCE_TEMPLATE_SPECIALIZATION_SUPPORT", "../Grammar/Statement.code");

  FunctionDefinition.setFunctionSource("SOURCE_FUNCTION_DEFINTION_STATEMENT",
                                       "../Grammar/Statement.code");
  FunctionDefinition.editSubstitute("$CLASSNAME", "SgFunctionDefinition");

  MemberFunctionDeclaration.setFunctionSource(
      "SOURCE_MEMBER_FUNCTION_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");
  VariableDeclaration.setFunctionSource("SOURCE_VARIABLE_DECLARATION_STATEMENT",
                                        "../Grammar/Statement.code");
  VariableDeclaration.setFunctionSource(
      "SOURCE_TEMPLATE_SPECIALIZATION_SUPPORT", "../Grammar/Statement.code");

  VariableDefinition.setFunctionSource("SOURCE_VARIABLE_DEFINITION_STATEMENT",
                                       "../Grammar/Statement.code");

  ClassDeclaration.setFunctionSource("SOURCE_CLASS_DECLARATION_STATEMENT",
                                     "../Grammar/Statement.code");
  ClassDeclaration.setFunctionSource("SOURCE_TEMPLATE_SPECIALIZATION_SUPPORT",
                                     "../Grammar/Statement.code");

  NonrealDecl.setFunctionSource("SOURCE_NONREAL_DECL",
                                "../Grammar/Statement.code");

  ClassDefinition.setFunctionSource("SOURCE_CLASS_DEFINITION_STATEMENT",
                                    "../Grammar/Statement.code");

  EnumDeclaration.setFunctionSource("SOURCE_ENUM_DECLARATION_STATEMENT",
                                    "../Grammar/Statement.code");
  ExprStatement.setFunctionSource("SOURCE_EXPRESSION_STATEMENT",
                                  "../Grammar/Statement.code");
  LabelStatement.setFunctionSource("SOURCE_LABEL_STATEMENT",
                                   "../Grammar/Statement.code");

  WhileStmt.setFunctionSource("SOURCE_WHILE_STATEMENT",
                              "../Grammar/Statement.code");
  WhileStmt.editSubstitute("$CLASSNAME", "SgWhileStmt");
  DoWhileStmt.setFunctionSource("SOURCE_DO_WHILE_STATEMENT",
                                "../Grammar/Statement.code");
  DoWhileStmt.editSubstitute("$CLASSNAME", "SgDoWhileStmt");

  SwitchStatement.setFunctionSource("SOURCE_SWITCH_STATEMENT",
                                    "../Grammar/Statement.code");
  SwitchStatement.editSubstitute("$CLASSNAME", "SgSwitchStatement");
  CaseOptionStmt.setFunctionSource("SOURCE_CASE_OPTION_STATEMENT",
                                   "../Grammar/Statement.code");
  CaseOptionStmt.editSubstitute("$CLASSNAME", "SgCaseOptionStmt");

  TryStmt.setFunctionSource("SOURCE_TRY_STATEMENT",
                            "../Grammar/Statement.code");
  CatchOptionStmt.setFunctionSource("SOURCE_CATCH_OPTION_STATEMENT",
                                    "../Grammar/Statement.code");
  CatchOptionStmt.editSubstitute("$CLASSNAME", "SgCatchOptionStmt");
  DefaultOptionStmt.setFunctionSource("SOURCE_DEFAULT_OPTION_STATEMENT",
                                      "../Grammar/Statement.code");
  BreakStmt.setFunctionSource("SOURCE_BREAK_STATEMENT",
                              "../Grammar/Statement.code");
  ContinueStmt.setFunctionSource("SOURCE_CONTINUE_STATEMENT",
                                 "../Grammar/Statement.code");
  FortranContinueStmt.setFunctionSource("SOURCE_FORTRAN_CONTINUE_STATEMENT",
                                        "../Grammar/Statement.code");

  ReturnStmt.setFunctionSource("SOURCE_RETURN_STATEMENT",
                               "../Grammar/Statement.code");
  GotoStatement.setFunctionSource("SOURCE_GOTO_STATEMENT",
                                  "../Grammar/Statement.code");
  AsmStmt.setFunctionSource("SOURCE_ASM_STATEMENT",
                            "../Grammar/Statement.code");

  // DQ (12/13/2005): Added support for empty statement (and empty expression).
  NullStatement.setFunctionSource("SOURCE_NULL_STATEMENT",
                                  "../Grammar/Statement.code");

  AttributedStatement.setFunctionSource("SOURCE_ATTRIBUTED_STATEMENT",
                                        "../Grammar/Statement.code");

  ForInitStatement.setFunctionSource("SOURCE_FOR_INIT_STATEMENT",
                                     "../Grammar/Statement.code");
  CatchStatementSeq.setFunctionSource("SOURCE_CATCH_STATEMENT_SEQ",
                                      "../Grammar/Statement.code");
  FunctionParameterList.setFunctionSource("SOURCE_FUNCTION_PARAMETER_LIST",
                                          "../Grammar/Statement.code");
  CtorInitializerList.setFunctionSource("SOURCE_CTOR_INITIALIZER_LIST",
                                        "../Grammar/Statement.code");

  TypedefDeclaration.setFunctionSource("SOURCE_TYPEDEF_DECLARATION_STATEMENT",
                                       "../Grammar/Statement.code");

  TemplateDeclaration.setFunctionSource("SOURCE_TEMPLATE_DECLARATION_STATEMENT",
                                        "../Grammar/Statement.code");
  TemplateInstantiationDecl.setFunctionSource(
      "SOURCE_TEMPLATE_INSTANTIATION_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");
  TemplateInstantiationDefn.setFunctionSource(
      "SOURCE_TEMPLATE_INSTANTIATION_DEFINITION_STATEMENT",
      "../Grammar/Statement.code");

  TemplateInstantiationFunctionDecl.setFunctionSource(
      "SOURCE_TEMPLATE_INSTANTIATION_FUNCTION_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");
  TemplateInstantiationMemberFunctionDecl.setFunctionSource(
      "SOURCE_TEMPLATE_INSTANTIATION_MEMBER_FUNCTION_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");

  TemplateInstantiationDecl.editSubstitute("$CLASSNAME",
                                           "SgTemplateInstantiationDecl");
  TemplateInstantiationFunctionDecl.editSubstitute(
      "$CLASSNAME", "SgTemplateInstantiationFunctionDecl");
  TemplateInstantiationMemberFunctionDecl.editSubstitute(
      "$CLASSNAME", "SgTemplateInstantiationMemberFunctionDecl");

  // DQ (6/10/2011): Added support for more detail in the template declaration
  // support.
  TemplateClassDeclaration.setFunctionSource(
      "SOURCE_TEMPLATE_CLASS_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");
  TemplateFunctionDeclaration.setFunctionSource(
      "SOURCE_TEMPLATE_FUNCTION_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");
  TemplateMemberFunctionDeclaration.setFunctionSource(
      "SOURCE_TEMPLATE_MEMBER_FUNCTION_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");
  TemplateClassDefinition.setFunctionSource(
      "SOURCE_TEMPLATE_CLASS_DEFINITION_STATEMENT",
      "../Grammar/Statement.code");
  TemplateFunctionDefinition.setFunctionSource(
      "SOURCE_TEMPLATE_FUNCTION_DEFINITION_STATEMENT",
      "../Grammar/Statement.code");

  // DQ (12/6/2011): Adding support for template variables (e.g. static template
  // data members).
  TemplateVariableDeclaration.setFunctionSource(
      "SOURCE_TEMPLATE_VARIABLE_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");

  // DQ (10/14/2014): Adding template typedef for C++11 support.
  TemplateTypedefDeclaration.setFunctionSource(
      "SOURCE_TEMPLATE_TYPEDEF_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");

  // DQ (11/4/2014): Adding IR node for instantiation of template typedef for
  // C++11 support.
  TemplateInstantiationTypedefDeclaration.setFunctionSource(
      "SOURCE_TEMPLATE_INSTANTIATION_TYPEDEF_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");

  // Support for pragmas in the IR
  PragmaDeclaration.setFunctionSource("SOURCE_PRAGMA_STATEMENT",
                                      "../Grammar/Statement.code");

  // DQ (3/22/2019): Adding EmptyDeclaration to support addition of comments and
  // CPP directives that will permit token-based unparsing to work with greater
  // precision. For example, used to add an include directive with greater
  // precision to the global scope and permit the unparsing via the token stream
  // to be used as well.
  EmptyDeclaration.setFunctionSource("SOURCE_EMPTY_DECLARATION",
                                     "../Grammar/Statement.code");
  AccessLabelStatement.setFunctionSource("SOURCE_ACCESS_LABEL_STATEMENT",
                                         "../Grammar/Statement.code");
  DeclarationGroupStatement.setFunctionSource(
      "SOURCE_DECLARATION_GROUP_STATEMENT", "../Grammar/Statement.code");

  // DQ (4/22/2004): Support for namespaces: NamespaceDeclarationStatement,
  //      NamespaceAliasDeclarationStatement, UsingDeclarationStatement
  NamespaceDeclarationStatement.setFunctionSource(
      "SOURCE_NAMESPACE_DECLARATION_STATEMENT", "../Grammar/Statement.code");
  NamespaceAliasDeclarationStatement.setFunctionSource(
      "SOURCE_NAMESPACE_ALIAS_DECLARATION_STATEMENT",
      "../Grammar/Statement.code");
  NamespaceAliasDeclarationStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");

  NamespaceDefinitionStatement.setFunctionSource(
      "SOURCE_NAMESPACE_DEFINITION_STATEMENT", "../Grammar/Statement.code");

  UsingDirectiveStatement.setFunctionSource("SOURCE_USING_DIRECTIVE_STATEMENT",
                                            "../Grammar/Statement.code");
  UsingDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");

  UsingDeclarationStatement.setFunctionSource(
      "SOURCE_USING_DECLARATION_STATEMENT", "../Grammar/Statement.code");

  // DQ (4/16/2005): Added support for explicit template instantiation to IR
  // (required to address template linking issues)
  TemplateInstantiationDirectiveStatement.setFunctionSource(
      "SOURCE_TEMPLATE_INSTANTIATION_DIRECTIVE_STATEMENT",
      "../Grammar/Statement.code");
  TemplateInstantiationDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");

  ProgramHeaderStatement.setFunctionSource("SOURCE_PROGRAM_HEADER_STATEMENT",
                                           "../Grammar/Statement.code");
  ProcedureHeaderStatement.setFunctionSource(
      "SOURCE_PROCEDURE_HEADER_STATEMENT", "../Grammar/Statement.code");
  FortranDo.setFunctionSource("SOURCE_FORTRAN_DO_STATEMENT",
                              "../Grammar/Statement.code");
  InterfaceStatement.setFunctionSource("SOURCE_INTERFACE_STATEMENT",
                                       "../Grammar/Statement.code");
  CommonBlock.setFunctionSource("SOURCE_COMMON_BLOCK_STATEMENT",
                                "../Grammar/Statement.code");
  ModuleStatement.setFunctionSource("SOURCE_MODULE_STATEMENT",
                                    "../Grammar/Statement.code");
  UseStatement.setFunctionSource("SOURCE_USE_STATEMENT",
                                 "../Grammar/Statement.code");
  ProcessControlStatement.setFunctionSource("SOURCE_PROCESS_CONTROL_STATEMENT",
                                            "../Grammar/Statement.code");

  IOStatement.setFunctionSource("SOURCE_IO_STATEMENT",
                                "../Grammar/Statement.code");

  // Rasmussen (9/20/2018): Added F2018 image control statements
  ImageControlStatement.setFunctionSource("SOURCE_IMAGE_CONTROL_STATEMENT",
                                          "../Grammar/Statement.code");

  // Derived from ImageControlStatement
  SyncAllStatement.setFunctionSource("SOURCE_SYNC_ALL_STATEMENT",
                                     "../Grammar/Statement.code");
  SyncImagesStatement.setFunctionSource("SOURCE_SYNC_IMAGES_STATEMENT",
                                        "../Grammar/Statement.code");
  SyncMemoryStatement.setFunctionSource("SOURCE_SYNC_MEMORY_STATEMENT",
                                        "../Grammar/Statement.code");
  SyncTeamStatement.setFunctionSource("SOURCE_SYNC_TEAM_STATEMENT",
                                      "../Grammar/Statement.code");
  FormTeamStatement.setFunctionSource("SOURCE_FORM_TEAM_STATEMENT",
                                      "../Grammar/Statement.code");
  LockStatement.setFunctionSource("SOURCE_LOCK_STATEMENT",
                                  "../Grammar/Statement.code");
  UnlockStatement.setFunctionSource("SOURCE_UNLOCK_STATEMENT",
                                    "../Grammar/Statement.code");

  // DQ (12/27/2007): Added fortran entry statement.
  EntryStatement.setFunctionSource("SOURCE_ENTRY_STATEMENT",
                                   "../Grammar/Statement.code");

  PrintStatement.setFunctionSource("SOURCE_PRINT_STATEMENT",
                                   "../Grammar/Statement.code");
  ReadStatement.setFunctionSource("SOURCE_READ_STATEMENT",
                                  "../Grammar/Statement.code");
  WriteStatement.setFunctionSource("SOURCE_WRITE_STATEMENT",
                                   "../Grammar/Statement.code");
  OpenStatement.setFunctionSource("SOURCE_OPEN_STATEMENT",
                                  "../Grammar/Statement.code");
  CloseStatement.setFunctionSource("SOURCE_CLOSE_STATEMENT",
                                   "../Grammar/Statement.code");
  InquireStatement.setFunctionSource("SOURCE_INQUIRE_STATEMENT",
                                     "../Grammar/Statement.code");
  FlushStatement.setFunctionSource("SOURCE_FLUSH_STATEMENT",
                                   "../Grammar/Statement.code");
  RewindStatement.setFunctionSource("SOURCE_REWIND_STATEMENT",
                                    "../Grammar/Statement.code");
  BackspaceStatement.setFunctionSource("SOURCE_BACKSPACE_STATEMENT",
                                       "../Grammar/Statement.code");
  EndfileStatement.setFunctionSource("SOURCE_ENDFILE_STATEMENT",
                                     "../Grammar/Statement.code");
  WaitStatement.setFunctionSource("SOURCE_WAIT_STATEMENT",
                                  "../Grammar/Statement.code");

  // Note that some IR nodes as so close to the semantics of C/C++
  // IR nodes that we can reuse them directly, examples include:
  //      SgContinueStatement maps to Fortran CYCLE statement
  //      SgBreakStatement maps to Fortran EXIT statement

  // More IR nodes required for Fortran support
  ImplicitStatement.setFunctionSource("SOURCE_IMPLICIT_STATEMENT",
                                      "../Grammar/Statement.code");
  StatementFunctionStatement.setFunctionSource(
      "SOURCE_STATEMENT_FUNCTION_STATEMENT", "../Grammar/Statement.code");
  WhereStatement.setFunctionSource("SOURCE_WHERE_STATEMENT",
                                   "../Grammar/Statement.code");
  ElseWhereStatement.setFunctionSource("SOURCE_ELSE_WHERE_STATEMENT",
                                       "../Grammar/Statement.code");
  NullifyStatement.setFunctionSource("SOURCE_NULLIFY_STATEMENT",
                                     "../Grammar/Statement.code");
  EquivalenceStatement.setFunctionSource("SOURCE_EQUIVALENCE_STATEMENT",
                                         "../Grammar/Statement.code");
  DerivedTypeStatement.setFunctionSource("SOURCE_DERIVED_TYPE_STATEMENT",
                                         "../Grammar/Statement.code");

  AttributeSpecificationStatement.setFunctionSource(
      "SOURCE_ATTRIBUTE_SPECIFICATION_STATEMENT", "../Grammar/Statement.code");
  AllocateStatement.setFunctionSource("SOURCE_ALLOCATE_STATEMENT",
                                      "../Grammar/Statement.code");
  DeallocateStatement.setFunctionSource("SOURCE_DEALLOCATE_STATEMENT",
                                        "../Grammar/Statement.code");
  ContainsStatement.setFunctionSource("SOURCE_CONTAINS_STATEMENT",
                                      "../Grammar/Statement.code");

  // Required for F77 support (but deprecated in F90)
  ArithmeticIfStatement.setFunctionSource("SOURCE_ARITHMETIC_IF_STATEMENT",
                                          "../Grammar/Statement.code");
  AssignStatement.setFunctionSource("SOURCE_ASSIGN_STATEMENT",
                                    "../Grammar/Statement.code");
  ComputedGotoStatement.setFunctionSource("SOURCE_COMPUTED_GOTO_STATEMENT",
                                          "../Grammar/Statement.code");
  AssignedGotoStatement.setFunctionSource("SOURCE_ASSIGNED_GOTO_STATEMENT",
                                          "../Grammar/Statement.code");

  NamelistStatement.setFunctionSource("SOURCE_NAMELIST_STATEMENT",
                                      "../Grammar/Statement.code");
  ImportStatement.setFunctionSource("SOURCE_IMPORT_STATEMENT",
                                    "../Grammar/Statement.code");

  AssociateStatement.setFunctionSource("SOURCE_ASSOCIATE_STATEMENT",
                                       "../Grammar/Statement.code");

  FormatStatement.setFunctionSource("SOURCE_FORMAT_STATEMENT",
                                    "../Grammar/Statement.code");

  ForAllStatement.setFunctionSource("SOURCE_FORALL_STATEMENT",
                                    "../Grammar/Statement.code");

  // DQ (11/23/2008): Added support for CPP directives as IR nodes.
  C_PreprocessorDirectiveStatement.setFunctionSource(
      "SOURCE_PREPROCESSOR_DIRECTIVE_STATEMENT", "../Grammar/Statement.code");
  IncludeDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  DefineDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  UndefDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  IfdefDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  IfndefDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  DeadIfDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  IfDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  ElseDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  ElseifDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  EndifDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  LineDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  WarningDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  ErrorDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  EmptyDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");

  ClinkageDeclarationStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  ClinkageDeclarationStatement.setFunctionSource(
      "SOURCE_CLINKAGE_DECLARATION_STATEMENT", "../Grammar/Statement.code");
  ClinkageStartStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  ClinkageEndStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");

  FortranIncludeLine.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");

  LinemarkerDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  IncludeNextDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");
  IdentDirectiveStatement.setFunctionSource(
      "SOURCE_POST_CONSTRUCTION_INITIALIZATION_STATEMENT",
      "../Grammar/Statement.code");

  FortranIncludeLine.setFunctionSource("SOURCE_FORTRAN_INCLUDE_LINE",
                                       "../Grammar/Statement.code");

  // define members for each AstNodeClass/non-AstNodeClass
  //------------------------------------------------------------
  // every statement needs post_construction_initialization(){} but why ???

  // Base class of OpenMP executable directives
  OmpExecStatement.setFunctionPrototype("HEADER_OMP_EXEC_STATEMENT",
                                        "../Grammar/Statement.code");
  OmpExecStatement.setFunctionSource("SOURCE_OMP_EXEC_STATEMENT",
                                     "../Grammar/Statement.code");

  OmpDoStatement.setFunctionSource("SOURCE_OMP_DO_STATEMENT",
                                   "../Grammar/Statement.code");
  OmpWorkshareStatement.setFunctionSource("SOURCE_OMP_WORKSHARE_STATEMENT",
                                          "../Grammar/Statement.code");
  OmpClauseBodyStatement.setFunctionPrototype("HEADER_OMP_CLAUSEBODY_STATEMENT",
                                              "../Grammar/Statement.code");
  OmpClauseBodyStatement.setFunctionSource("SOURCE_OMP_CLAUSEBODY_STATEMENT",
                                           "../Grammar/Statement.code");

  OmpClauseStatement.setFunctionPrototype("HEADER_OMP_CLAUSE_STATEMENT",
                                          "../Grammar/Statement.code");
  OmpClauseStatement.setFunctionSource("SOURCE_OMP_CLAUSE_STATEMENT",
                                       "../Grammar/Statement.code");

  OmpForStatement.setFunctionSource("SOURCE_OMP_FOR_STATEMENT",
                                    "../Grammar/Statement.code");

  OmpBodyStatement.setFunctionPrototype("HEADER_OMP_BODY_STATEMENT",
                                        "../Grammar/Statement.code");
  OmpBodyStatement.setFunctionSource("SOURCE_OMP_BODY_STATEMENT",
                                     "../Grammar/Statement.code");
  OmpCriticalStatement.setFunctionSource("SOURCE_OMP_CRITICAL_STATEMENT",
                                         "../Grammar/Statement.code");
  OmpDeclareSimdStatement.setFunctionPrototype(
      "HEADER_OMP_DECLARE_SIMD_STATEMENT", "../Grammar/Statement.code");
  OmpDeclareSimdStatement.setFunctionSource("SOURCE_OMP_DECLARE_SIMD_STATEMENT",
                                            "../Grammar/Statement.code");
  OmpDeclareVariantStatement.setFunctionPrototype(
      "HEADER_OMP_DECLARE_VARIANT_STATEMENT", "../Grammar/Statement.code");
  OmpDeclareVariantStatement.setFunctionSource(
      "SOURCE_OMP_DECLARE_VARIANT_STATEMENT", "../Grammar/Statement.code");
  OmpBeginDeclareVariantStatement.setFunctionPrototype(
      "HEADER_OMP_BEGIN_DECLARE_VARIANT_STATEMENT",
      "../Grammar/Statement.code");
  OmpBeginDeclareVariantStatement.setFunctionSource(
      "SOURCE_OMP_BEGIN_DECLARE_VARIANT_STATEMENT",
      "../Grammar/Statement.code");
  OmpEndDeclareVariantStatement.setFunctionPrototype(
      "HEADER_OMP_END_DECLARE_VARIANT_STATEMENT", "../Grammar/Statement.code");
  OmpEndDeclareVariantStatement.setFunctionSource(
      "SOURCE_OMP_END_DECLARE_VARIANT_STATEMENT", "../Grammar/Statement.code");
  OmpDeclareTargetStatement.setFunctionPrototype(
      "HEADER_OMP_DECLARE_TARGET_STATEMENT", "../Grammar/Statement.code");
  OmpDeclareTargetStatement.setFunctionSource(
      "SOURCE_OMP_DECLARE_TARGET_STATEMENT", "../Grammar/Statement.code");
  OmpEndDeclareTargetStatement.setFunctionPrototype(
      "HEADER_OMP_END_DECLARE_TARGET_STATEMENT", "../Grammar/Statement.code");
  OmpEndDeclareTargetStatement.setFunctionSource(
      "SOURCE_OMP_END_DECLARE_TARGET_STATEMENT", "../Grammar/Statement.code");

  OmpRequiresStatement.setFunctionPrototype("HEADER_OMP_REQUIRES_STATEMENT",
                                            "../Grammar/Statement.code");
  OmpRequiresStatement.setFunctionSource("SOURCE_OMP_REQUIRES_STATEMENT",
                                         "../Grammar/Statement.code");
  OmpTaskwaitStatement.setFunctionSource("SOURCE_OMP_TASKWAIT_STATEMENT",
                                         "../Grammar/Statement.code");
  OmpTaskwaitStatement.setFunctionPrototype("HEADER_OMP_TASKWAIT_STATEMENT",
                                            "../Grammar/Statement.code");
  OmpParallelStatement.setFunctionSource("SOURCE_OMP_PARALLEL_STATEMENT",
                                         "../Grammar/Statement.code");
  OmpTeamsStatement.setFunctionSource("SOURCE_OMP_TEAMS_STATEMENT",
                                      "../Grammar/Statement.code");
  OmpCancellationPointStatement.setFunctionSource(
      "SOURCE_OMP_CANCELLATION_POINT_STATEMENT", "../Grammar/Statement.code");
  OmpOrderedDependStatement.setFunctionSource(
      "SOURCE_OMP_ORDERED_DEPEND_STATEMENT", "../Grammar/Statement.code");
  OmpDeclareMapperStatement.setFunctionPrototype(
      "HEADER_OMP_DECLARE_MAPPER_STATEMENT", "../Grammar/Statement.code");
  OmpDeclareMapperStatement.setFunctionSource(
      "SOURCE_OMP_DECLARE_MAPPER_STATEMENT", "../Grammar/Statement.code");
  OmpCancelStatement.setFunctionSource("SOURCE_OMP_CANCEL_STATEMENT",
                                       "../Grammar/Statement.code");
  OmpTaskgroupStatement.setFunctionSource("SOURCE_OMP_TASKGROUP_STATEMENT",
                                          "../Grammar/Statement.code");
  OmpDispatchStatement.setFunctionSource("SOURCE_OMP_DISPATCH_STATEMENT",
                                         "../Grammar/Statement.code");
  OmpDepobjStatement.setFunctionSource("SOURCE_OMP_DEPOBJ_STATEMENT",
                                       "../Grammar/Statement.code");
  OmpDistributeStatement.setFunctionSource("SOURCE_OMP_DISTRIBUTE_STATEMENT",
                                           "../Grammar/Statement.code");
  OmpWorkdistributeStatement.setFunctionSource(
      "SOURCE_OMP_WORKDISTRIBUTE_STATEMENT", "../Grammar/Statement.code");
  OmpLoopStatement.setFunctionSource("SOURCE_OMP_LOOP_STATEMENT",
                                     "../Grammar/Statement.code");
  OmpScanStatement.setFunctionSource("SOURCE_OMP_SCAN_STATEMENT",
                                     "../Grammar/Statement.code");
  OmpTaskloopStatement.setFunctionSource("SOURCE_OMP_TASKLOOP_STATEMENT",
                                         "../Grammar/Statement.code");
  OmpMaskedStatement.setFunctionSource("SOURCE_OMP_MASKED_STATEMENT",
                                       "../Grammar/Statement.code");
  OmpMaskedTaskloopStatement.setFunctionSource(
      "SOURCE_OMP_MASKED_TASKLOOP_STATEMENT", "../Grammar/Statement.code");
  OmpMaskedTaskloopSimdStatement.setFunctionSource(
      "SOURCE_OMP_MASKED_TASKLOOP_SIMD_STATEMENT", "../Grammar/Statement.code");
  OmpMetadirectiveStatement.setFunctionSource(
      "SOURCE_OMP_METADIRECTIVE_STATEMENT", "../Grammar/Statement.code");
  OmpSectionsStatement.setFunctionSource("SOURCE_OMP_SECTIONS_STATEMENT",
                                         "../Grammar/Statement.code");
  OmpSectionStatement.setFunctionSource("SOURCE_OMP_SECTION_STATEMENT",
                                        "../Grammar/Statement.code");
  OmpTaskStatement.setFunctionSource("SOURCE_OMP_TASK_STATEMENT",
                                     "../Grammar/Statement.code");
  OmpTaskStatement.setFunctionPrototype("HEADER_OMP_TASK_STATEMENT",
                                        "../Grammar/Statement.code");
  OmpTaskStatement.setDataPrototype(
      "SgInitializedNamePtrList", "semantic_firstprivate_variables", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE, COPY_REFERENCE);
  OmpTaskStatement.setDataPrototype(
      "SgInitializedNamePtrList", "semantic_private_variables", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE, COPY_REFERENCE);
  OmpTaskStatement.setDataPrototype(
      "bool", "semantic_data_environment_is_available", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpTargetEnterDataStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_ENTER_DATA_STATEMENT", "../Grammar/Statement.code");
  OmpTargetExitDataStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_EXIT_DATA_STATEMENT", "../Grammar/Statement.code");
  OmpSingleStatement.setFunctionSource("SOURCE_OMP_SINGLE_STATEMENT",
                                       "../Grammar/Statement.code");

  OmpThreadprivateStatement.setFunctionPrototype(
      "HEADER_OMP_THREADPRIVATE_STATEMENT", "../Grammar/Statement.code");

  OmpThreadprivateStatement.setFunctionSource(
      "SOURCE_OMP_THREADPRIVATE_STATEMENT", "../Grammar/Statement.code");
  OmpFlushStatement.setFunctionSource("SOURCE_OMP_FLUSH_STATEMENT",
                                      "../Grammar/Statement.code");
  OmpFlushStatement.setFunctionPrototype("HEADER_OMP_FLUSH_STATEMENT",
                                         "../Grammar/Statement.code");
  OmpAllocateStatement.setFunctionSource("SOURCE_OMP_ALLOCATE_STATEMENT",
                                         "../Grammar/Statement.code");
  OmpAllocateStatement.setFunctionPrototype("HEADER_OMP_ALLOCATE_STATEMENT",
                                            "../Grammar/Statement.code");
  OmpForSimdStatement.setFunctionSource("SOURCE_OMP_FOR_SIMD_STATEMENT",
                                        "../Grammar/Statement.code");
  OmpAtomicStatement.setFunctionSource("SOURCE_OMP_ATOMIC_STATEMENT",
                                       "../Grammar/Statement.code");
  OmpBarrierStatement.setFunctionSource("SOURCE_OMP_BARRIER_STATEMENT",
                                        "../Grammar/Statement.code");
  OmpNothingStatement.setFunctionSource("SOURCE_OMP_NOTHING_STATEMENT",
                                        "../Grammar/Statement.code");
  OmpMasterStatement.setFunctionSource("SOURCE_OMP_MASTER_STATEMENT",
                                       "../Grammar/Statement.code");
  OmpTaskyieldStatement.setFunctionSource("SOURCE_OMP_TASKYIELD_STATEMENT",
                                          "../Grammar/Statement.code");
  OmpOrderedStatement.setFunctionSource("SOURCE_OMP_ORDERED_STATEMENT",
                                        "../Grammar/Statement.code");

  OmpTargetStatement.setFunctionSource("SOURCE_OMP_TARGET_STATEMENT",
                                       "../Grammar/Statement.code");
  OmpTargetDataStatement.setFunctionSource("SOURCE_OMP_TARGET_DATA_STATEMENT",
                                           "../Grammar/Statement.code");
  OmpTargetParallelForStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_PARALLEL_FOR_STATEMENT", "../Grammar/Statement.code");
  OmpTargetParallelStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_PARALLEL_STATEMENT", "../Grammar/Statement.code");
  OmpDistributeSimdStatement.setFunctionSource(
      "SOURCE_OMP_DISTRIBUTE_SIMD_STATEMENT", "../Grammar/Statement.code");
  OmpDistributeParallelForStatement.setFunctionSource(
      "SOURCE_OMP_DISTRIBUTE_PARALLEL_FOR_STATEMENT",
      "../Grammar/Statement.code");
  OmpDistributeParallelForSimdStatement.setFunctionSource(
      "SOURCE_OMP_DISTRIBUTE_PARALLEL_FOR_SIMD_STATEMENT",
      "../Grammar/Statement.code");
  OmpTaskloopSimdStatement.setFunctionSource(
      "SOURCE_OMP_TASKLOOP_SIMD_STATEMENT", "../Grammar/Statement.code");
  OmpTargetUpdateStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_UPDATE_STATEMENT", "../Grammar/Statement.code");
  OmpTargetParallelForSimdStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_PARALLEL_FOR_SIMD_STATEMENT",
      "../Grammar/Statement.code");
  OmpTargetParallelLoopStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_PARALLEL_LOOP_STATEMENT", "../Grammar/Statement.code");
  OmpTargetSimdStatement.setFunctionSource("SOURCE_OMP_TARGET_SIMD_STATEMENT",
                                           "../Grammar/Statement.code");
  OmpTargetTeamsStatement.setFunctionSource("SOURCE_OMP_TARGET_TEAMS_STATEMENT",
                                            "../Grammar/Statement.code");
  OmpTargetTeamsDistributeStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_TEAMS_DISTRIBUTE_STATEMENT",
      "../Grammar/Statement.code");
  OmpTargetTeamsWorkdistributeStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_TEAMS_WORKDISTRIBUTE_STATEMENT",
      "../Grammar/Statement.code");
  OmpTargetTeamsDistributeSimdStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_TEAMS_DISTRIBUTE_SIMD_STATEMENT",
      "../Grammar/Statement.code");
  OmpTargetTeamsLoopStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_TEAMS_LOOP_STATEMENT", "../Grammar/Statement.code");
  OmpTargetTeamsDistributeParallelForStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_TEAMS_DISTRIBUTE_PARALLEL_FOR_STATEMENT",
      "../Grammar/Statement.code");
  OmpTargetTeamsDistributeParallelForSimdStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_TEAMS_DISTRIBUTE_PARALLEL_FOR_SIMD_STATEMENT",
      "../Grammar/Statement.code");
  OmpMasterTaskloopSimdStatement.setFunctionSource(
      "SOURCE_OMP_MASTER_TASKLOOP_SIMD_STATEMENT", "../Grammar/Statement.code");
  OmpParallelMasterTaskloopStatement.setFunctionSource(
      "SOURCE_OMP_PARALLEL_MASTER_TASKLOOP_STATEMENT",
      "../Grammar/Statement.code");
  OmpParallelMasterTaskloopSimdStatement.setFunctionSource(
      "SOURCE_OMP_PARALLEL_MASTER_TASKLOOP_SIMD_STATEMENT",
      "../Grammar/Statement.code");
  OmpTeamsDistributeStatement.setFunctionSource(
      "SOURCE_OMP_TEAMS_DISTRIBUTE_STATEMENT", "../Grammar/Statement.code");
  OmpTeamsDistributeSimdStatement.setFunctionSource(
      "SOURCE_OMP_TEAMS_DISTRIBUTE_SIMD_STATEMENT",
      "../Grammar/Statement.code");
  OmpTeamsDistributeParallelForStatement.setFunctionSource(
      "SOURCE_OMP_TEAMS_DISTRIBUTE_PARALLEL_FOR_STATEMENT",
      "../Grammar/Statement.code");
  OmpTeamsDistributeParallelForSimdStatement.setFunctionSource(
      "SOURCE_OMP_TEAMS_DISTRIBUTE_PARALLEL_FOR_SIMD_STATEMENT",
      "../Grammar/Statement.code");
  OmpTeamsLoopStatement.setFunctionSource("SOURCE_OMP_TEAMS_LOOP_STATEMENT",
                                          "../Grammar/Statement.code");
  OmpParallelMasterStatement.setFunctionSource(
      "SOURCE_OMP_PARALLEL_MASTER_STATEMENT", "../Grammar/Statement.code");
  OmpMasterTaskloopStatement.setFunctionSource(
      "SOURCE_OMP_MASTER_TASKLOOP_STATEMENT", "../Grammar/Statement.code");
  OmpParallelLoopStatement.setFunctionSource(
      "SOURCE_OMP_PARALLEL_LOOP_STATEMENT", "../Grammar/Statement.code");
  OmpUnrollStatement.setFunctionSource("SOURCE_OMP_UNROLL_STATEMENT",
                                       "../Grammar/Statement.code");
  OmpTileStatement.setFunctionSource("SOURCE_OMP_TILE_STATEMENT",
                                     "../Grammar/Statement.code");
  OmpTargetDataCompositeStatement.setFunctionSource(
      "SOURCE_OMP_TARGET_DATA_COMPOSITE_STATEMENT",
      "../Grammar/Statement.code");
  OmpScopeStatement.setFunctionSource("SOURCE_OMP_SCOPE_STATEMENT",
                                      "../Grammar/Statement.code");
  OmpParallelMaskedStatement.setFunctionSource(
      "SOURCE_OMP_PARALLEL_MASKED_STATEMENT", "../Grammar/Statement.code");
  OmpAssumeStatement.setFunctionSource("SOURCE_OMP_ASSUME_STATEMENT",
                                       "../Grammar/Statement.code");
  OmpTaskgraphStatement.setFunctionSource("SOURCE_OMP_TASKGRAPH_STATEMENT",
                                          "../Grammar/Statement.code");
  OmpFuseStatement.setFunctionSource("SOURCE_OMP_FUSE_STATEMENT",
                                     "../Grammar/Statement.code");
  OmpInterchangeStatement.setFunctionSource("SOURCE_OMP_INTERCHANGE_STATEMENT",
                                            "../Grammar/Statement.code");
  OmpReverseStatement.setFunctionSource("SOURCE_OMP_REVERSE_STATEMENT",
                                        "../Grammar/Statement.code");
  OmpErrorStatement.setFunctionSource("SOURCE_OMP_ERROR_STATEMENT",
                                      "../Grammar/Statement.code");
  OmpInteropStatement.setFunctionSource("SOURCE_OMP_INTEROP_STATEMENT",
                                        "../Grammar/Statement.code");

  OmpBeginDeclareTargetStatement.setFunctionPrototype(
      "HEADER_OMP_BEGIN_DECLARE_TARGET_STATEMENT", "../Grammar/Statement.code");
  OmpBeginDeclareTargetStatement.setFunctionSource(
      "SOURCE_OMP_BEGIN_DECLARE_TARGET_STATEMENT", "../Grammar/Statement.code");
  OmpAssumesStatement.setFunctionPrototype("HEADER_OMP_ASSUMES_STATEMENT",
                                           "../Grammar/Statement.code");
  OmpAssumesStatement.setFunctionSource("SOURCE_OMP_ASSUMES_STATEMENT",
                                        "../Grammar/Statement.code");
  OmpBeginAssumesStatement.setFunctionPrototype(
      "HEADER_OMP_BEGIN_ASSUMES_STATEMENT", "../Grammar/Statement.code");
  OmpBeginAssumesStatement.setFunctionSource(
      "SOURCE_OMP_BEGIN_ASSUMES_STATEMENT", "../Grammar/Statement.code");
  OmpEndAssumesStatement.setFunctionPrototype(
      "HEADER_OMP_END_ASSUMES_STATEMENT", "../Grammar/Statement.code");
  OmpEndAssumesStatement.setFunctionSource("SOURCE_OMP_END_ASSUMES_STATEMENT",
                                           "../Grammar/Statement.code");
  OmpEndAssumeStatement.setFunctionSource("SOURCE_OMP_END_ASSUME_STATEMENT",
                                          "../Grammar/Statement.code");
  OmpGroupprivateStatement.setFunctionPrototype(
      "HEADER_OMP_GROUPPRIVATE_STATEMENT", "../Grammar/Statement.code");
  OmpGroupprivateStatement.setFunctionSource(
      "SOURCE_OMP_GROUPPRIVATE_STATEMENT", "../Grammar/Statement.code");
  OmpSimdStatement.setFunctionSource("SOURCE_OMP_SIMD_STATEMENT",
                                     "../Grammar/Statement.code");

  // Required source context survives outlining and generated directive copies
  // as typed IR data, independently of user-defined AST attributes.
  OmpExecStatement.setDataPrototype(
      "SgOmpExecStatement::runtime_source_origin_enum", "runtime_source_origin",
      "= SgOmpExecStatement::e_runtime_source_unavailable",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  OmpExecStatement.setDataPrototype(
      "std::string", "runtime_source_function_name", "= \"\"",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  OmpExecStatement.setDataPrototype("int", "runtime_source_physical_file_id",
                                    "= -1", NO_CONSTRUCTOR_PARAMETER,
                                    BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                    NO_DELETE);
  OmpExecStatement.setDataPrototype(
      "int", "runtime_source_line", "= 0", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpExecStatement.setDataPrototype(
      "bool", "runtime_source_is_copied", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Base class of OpenMP executable directives
  OmpExecStatement.setDataPrototype(
      "bool", "source_form_is_attribute", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpExecStatement.setDataPrototype(
      "SgStatement*", "omp_parent", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpExecStatement.setDataPrototype(
      "SgStatementPtrList", "omp_children", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // directives with variable lists
  // omp flush [(var-list)]
  // Model list items as expressions to support array sections, members, and
  // template-dependent references.
  OmpFlushStatement.setDataPrototype(
      "SgExprListExp*", "variable_list", "= new SgExprListExp()",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE, REQUIRED_TRAVERSAL_MEMBER);

  OmpAllocateStatement.setDataPrototype(
      "SgExprListExp*", "variable_list", "= new SgExprListExp()",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE, REQUIRED_TRAVERSAL_MEMBER);

  OmpDeclareSimdStatement.setDataPrototype(
      "SgExpression*", "function_ref", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      REQUIRED_TRAVERSAL_MEMBER);
  OmpDeclareSimdStatement.setDataPrototype(
      "bool", "function_ref_is_explicit", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDeclareSimdStatement.setDataPrototype(
      "std::size_t", "semantic_variant_ordinal", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDeclareSimdStatement.setDataPrototype(
      "SgOmpClausePtrList", "clauses", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  OmpDeclareVariantStatement.setDataPrototype(
      "SgExpression*", "variant_function_ref", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      REQUIRED_TRAVERSAL_MEMBER);
  OmpDeclareVariantStatement.setDataPrototype(
      "SgExpression*", "base_function_ref", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      REQUIRED_TRAVERSAL_MEMBER);
  OmpDeclareVariantStatement.setDataPrototype(
      "bool", "base_function_ref_is_explicit", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDeclareVariantStatement.setDataPrototype(
      "std::size_t", "semantic_variant_ordinal", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDeclareVariantStatement.setDataPrototype(
      "SgOmpClausePtrList", "clauses", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  OmpBeginDeclareVariantStatement.setDataPrototype(
      "SgOmpClausePtrList", "clauses", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  OmpDeclareMapperStatement.setDataPrototype(
      "SgOmpClause::omp_declare_mapper_identifier_enum", "identifier",
      "=SgOmpClause::e_omp_declare_mapper_identifier_unspecified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  OmpDeclareMapperStatement.setDataPrototype(
      "bool", "identifier_is_explicit", "=false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDeclareMapperStatement.setDataPrototype(
      "SgExpression*", "user_defined_identifier", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);
  OmpDeclareMapperStatement.setDataPrototype(
      "SgExpression*", "mapper_type", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpDeclareMapperStatement.setDataPrototype(
      "SgExpression*", "mapper_variable", "= NULL", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpDeclareMapperStatement.setDataPrototype(
      "SgOmpClausePtrList", "clauses", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  OmpDeclareTargetStatement.setDataPrototype(
      "SgOmpClausePtrList", "clauses", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  OmpDeclareTargetStatement.setDataPrototype(
      "SgOmpClause::omp_when_context_kind_enum", "device_type_kind",
      "=SgOmpClause::e_omp_when_context_kind_unknown", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDeclareTargetStatement.setDataPrototype(
      "SgStatementPtrList", "statements", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDeclareTargetStatement.setDataPrototype(
      "bool", "use_underscore_spelling", "=false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpBeginDeclareTargetStatement.setDataPrototype(
      "bool", "use_underscore_spelling", "=false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpEndDeclareTargetStatement.setDataPrototype(
      "bool", "use_underscore_spelling", "=false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpRequiresStatement.setDataPrototype(
      "SgOmpClausePtrList", "clauses", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  OmpTaskwaitStatement.setDataPrototype(
      "SgOmpClausePtrList", "clauses", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  OmpAssumesStatement.setDataPrototype(
      "SgOmpClausePtrList", "clauses", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  OmpBeginAssumesStatement.setDataPrototype(
      "SgOmpClausePtrList", "clauses", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  OmpGroupprivateStatement.setDataPrototype(
      "SgExprListExp*", "variables", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpGroupprivateStatement.setDataPrototype(
      "SgOmpClauseList*", "clause_list", "= new SgOmpClauseList()",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE, REQUIRED_TRAVERSAL_MEMBER);
  OmpGroupprivateStatement.setDataPrototype(
      "SgOmpClause::omp_when_context_kind_enum", "device_type_kind",
      "=SgOmpClause::e_omp_when_context_kind_unknown", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // omp threadprivate [(var-list)]
  OmpThreadprivateStatement.setDataPrototype(
      "SgExpressionPtrList", "variables", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  // Preserve the grammar distinction between `metadirective` and Fortran
  // `begin metadirective` as typed source-form provenance.  The body shape is
  // not sufficient to reconstruct which spelling the frontend parsed.
  OmpMetadirectiveStatement.setDataPrototype(
      "bool", "source_form_is_begin", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Directives with a statement/ structured body
  OmpBodyStatement.setDataPrototype(
      "SgStatement*", "body", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  // This is source-form provenance, not an inference from the nested AST
  // shape.  A separate `parallel` whose body happens to be an OpenMP loop is
  // semantically distinct from a source-spelled combined directive.
  OmpBodyStatement.setDataPrototype(
      "bool", "source_form_is_combined", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpClauseStatement.setDataPrototype(
      "SgOmpClauseList*", "clause_list", "= new SgOmpClauseList()",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE, REQUIRED_TRAVERSAL_MEMBER);
  // Directive with a body + a name:
  // omp critical [name]  \n structured_block
  OmpCriticalStatement.setDataPrototype(
      "SgName", "name", "= \"\"", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);

  // The parenthesized depobj operand is a semantic lvalue expression, not a
  // name spelling.  Preserve its exact symbol/type identity through Sage.
  OmpDepobjStatement.setDataPrototype(
      "SgExpression*", "depobj", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      REQUIRED_TRAVERSAL_MEMBER);

  // Keep the body and the clause-list ownership boundary as two singleton
  // successors.  The wrapper owns the only traversed clause container, so the
  // grammar never needs a mixed singleton/container traversal exception.
  OmpClauseBodyStatement.setDataPrototype(
      "SgOmpClauseList*", "clause_list", "= new SgOmpClauseList()",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE, REQUIRED_TRAVERSAL_MEMBER);

  AccParallelStatement.setFunctionSource("SOURCE_ACC_PARALLEL_STATEMENT",
                                         "../Grammar/Statement.code");
  AccParallelLoopStatement.setFunctionSource(
      "SOURCE_ACC_PARALLEL_LOOP_STATEMENT", "../Grammar/Statement.code");
  AccDataStatement.setFunctionSource("SOURCE_ACC_DATA_STATEMENT",
                                     "../Grammar/Statement.code");
  AccKernelsStatement.setFunctionSource("SOURCE_ACC_KERNELS_STATEMENT",
                                        "../Grammar/Statement.code");
  AccAtomicStatement.setFunctionSource("SOURCE_ACC_ATOMIC_STATEMENT",
                                       "../Grammar/Statement.code");
  AccEnterDataStatement.setFunctionSource("SOURCE_ACC_ENTER_DATA_STATEMENT",
                                          "../Grammar/Statement.code");
  AccExitDataStatement.setFunctionSource("SOURCE_ACC_EXIT_DATA_STATEMENT",
                                         "../Grammar/Statement.code");
  AccRoutineStatement.setFunctionSource("SOURCE_ACC_ROUTINE_STATEMENT",
                                        "../Grammar/Statement.code");
  AccWaitStatement.setFunctionSource("SOURCE_ACC_WAIT_STATEMENT",
                                     "../Grammar/Statement.code");
  AccCacheStatement.setFunctionSource("SOURCE_ACC_CACHE_STATEMENT",
                                      "../Grammar/Statement.code");
  AccBodyStatement.setFunctionPrototype("HEADER_ACC_BODY_STATEMENT",
                                        "../Grammar/Statement.code");
  AccBodyStatement.setFunctionSource("SOURCE_ACC_BODY_STATEMENT",
                                     "../Grammar/Statement.code");
  AccClauseBodyStatement.setFunctionSource("SOURCE_ACC_CLAUSEBODY_STATEMENT",
                                           "../Grammar/Statement.code");
  AccClauseBodyStatement.setFunctionPrototype("HEADER_ACC_CLAUSEBODY_STATEMENT",
                                              "../Grammar/Statement.code");
  AccClauseStatement.setFunctionSource("SOURCE_ACC_CLAUSE_STATEMENT",
                                       "../Grammar/Statement.code");
  AccClauseStatement.setFunctionPrototype("HEADER_ACC_CLAUSE_STATEMENT",
                                          "../Grammar/Statement.code");

  AccBodyStatement.setDataPrototype(
      "SgStatement*", "body", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  AccClauseBodyStatement.setDataPrototype(
      "SgAccClausePtrList", "clauses", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  AccClauseStatement.setDataPrototype(
      "SgAccClausePtrList", "clauses", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  AccRoutineStatement.setDataPrototype(
      "SgName", "routine_name", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  AccWaitStatement.setDataPrototype(
      "SgExprListExp*", "wait_list", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);
  AccWaitStatement.setDataPrototype(
      "SgExpression*", "devnum", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);
  AccWaitStatement.setDataPrototype(
      "bool", "queues", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  AccCacheStatement.setDataPrototype(
      "SgExprListExp*", "variables", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);
  AccCacheStatement.setDataPrototype(
      "int", "modifier", "= 0", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
}
