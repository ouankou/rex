# ROSE upstream sync for REX

Canonical upstream source: https://github.com/llnl/rose.git

Upstream ROSE is read-only evidence for REX. Agents may fetch, inspect logs, show commits, and compare diffs. Agents must never push to upstream ROSE, open upstream PRs/issues/comments, request upstream reviews, or mention upstream maintainers. Any sync PR is a REX PR only.

## Support target (REX)
- Platform: Linux only (non-Linux platforms dropped).
- Languages/features: C/C++, Fortran, OpenMP, OpenACC, OpenCL, CUDA.
- Frontend: Clang/LLVM for C/C++ (LLVM 22); Fortran via Flang.
- Hard dropped: PHP, JavaScript, Python, Ada, Jovial, EDG, OFP/old Fortran parser, Java/ECJ, UPC, X10, Csharp, Matlab, YAML/mini-yaml, binary analysis, Sawyer, CodeThorn, Rosebud source tree, legacy autotools/Tup paths, dropped OpenMP designs, `src/3rdPartyLibraries`, and other legacy frontends/tools.
- Rosebud: do not restore `src/Rosebud`. If an upstream Rosebud commit contains an idea useful to existing REX ROSETTA code, port only that retained-scope logic manually.
- Legacy array test suites are dropped from CompileTests and CompilerOptionsTests inventories.

## Non-negotiable rules
- For sync tasks, use `.codex/skills/rose-upstream-sync/SKILL.md`.
- Scripts may inventory, classify, guard, and update records. They must not be used to bulk-apply code without per-commit review.
- Commit log must only reference upstream `rose/weekly` history; never use local REX history for log entries (always run `git log rose/weekly ...` when enumerating commits).
- Before logging any commit, verify the SHA exists in `rose/weekly` (e.g., `git merge-base --is-ancestor <sha> rose/weekly`); if not, do not log it.
- If a commit is marked `pick`, apply the code change immediately (before moving on).
- If a `pick` is already present in REX, record that in the Notes column and set the REX commit column to the existing REX commit when known.
- Decision order is strict: verify upstream SHA -> decide pick/drop -> apply (if pick) -> validate -> commit -> update commit log immediately.
- One-commit policy is strict: every applied upstream commit or tightly related upstream series maps to one final REX commit. That commit contains both the useful upstream content and any required REX-specific adaptation.
- Do not add later fixup commits for sync-caused build or test regressions. Find the offending REX sync commit, amend it, rebase later sync commits, rerun gates, and refresh CSV `REX commit` mappings to the rewritten final SHAs.
- Do not advance past a code/build/test-affecting `pick` or `pick-partial` with unbuilt code. Build after each such commit, run focused CTest gates after risky commits or small related batches, and run full CTest before PR.
- Keep list below is authoritative; only those paths are eligible for sync.
- Drop list below is authoritative; never reintroduce dropped paths or platforms.
- Autotools/Tup are inventory only: do not port into CMake.
- Preserve REX pruning decisions; do not reintroduce Boost-only subsystems or removed frontends.
- Preserve REX modernizations that removed deprecated C++ template patterns; do not reintroduce those warning-prone constructs.
- Release/version commits update only `ROSE_VERSION` and `config/SCM_DATE`, and only for the latest version marker in a completed sync range. Never restore `configure.ac` or `src/frontend/CxxFrontend/EDG_VERSION`.
- Version numbering must follow upstream ROSE consistently in every REX encoding/validation site. For current three-component ROSE versions, the encoded `SCM_DATE` is `major * 10000000 + minor * 100 + patch` (for example, `2.1.0 -> 20000100`), not a hand-adjusted local value.
- Applied sync commits should be created with `git cherry-pick -n` plus manual `git commit` so REX hooks run normal staged formatting. Do not run broad formatting sweeps or touch tests/reference files for formatting-only churn.
- Keep an untracked scratch ledger under `build/upstream-sync/` while rewriting sync commits. Write the tracked `docs/upstream-sync/rose-YYYY-commits.csv` only after final REX commit SHAs are stable.
- Merge completed sync PRs with rebase so the individual REX sync commit IDs recorded in `docs/upstream-sync/rose-YYYY-commits.csv` remain true on `main`. Do not squash sync PRs unless the CSV mappings are intentionally rewritten to the resulting final commits before the merge is considered complete.
- Treat `REX commit` IDs in the CSV as branch-shape dependent. Any rebase of a sync branch changes those local commit IDs, so audit and refresh the affected CSV rows after the last rebase and before the PR is considered ready. If a commit-ID mismatch is found only after merge, fix the CSV mapping in a documentation follow-up PR instead of leaving known-bad historical records.
- Upstream ROSE tests may be private, absent, or dropped from REX scope. For synced retained behavior that needs local coverage and has no usable upstream public test, add focused REX-owned tests in a follow-up PR using the normal `rex_<area>_<behavior>.<ext>` naming rule. If the test is required to prove the sync commit itself is correct before landing, amend it into the owning sync commit instead.
- Helper scripts must not leave Python cache files in the source tree. Use a temporary `pycache_prefix` for explicit compile checks.
- Flang parse-tree snapshots are upstream-generated and may contain host-specific `#line` markers (e.g., `apple-darwin`); keep them unchanged since they are not platform-support code.

## Root files (keep list + sync status)
Status legend: pending / in-progress / synced / inventory / skip
- pending .gitignore
- pending .gitmodules
- pending AGENTS.md
- pending BUILDING_WITH_CLANG.md
- pending CLAUDE.md
- in-progress CMakeLists.txt
- pending COPYRIGHT
- pending FORTRAN_TESTING_GUIDE.md
- pending GEMINI.md
- inventory Makefile.am
- pending OPENMP_SUPPORT.md
- pending README.md
- pending ROSE_COMPILER_FIXES.md
- pending ROSE_VERSION
- inventory configure.ac
- pending build-rex.sh
- pending rose_config.h.in.cmake
- inventory stamp-h.in
- inventory stamp-h1.in
- pending test_omp_normal.c
- pending test_omp_space_after_hash.c
- pending test_omp_spaces.c

## Directory inventory (keep list + sync status)
Default status for all entries is pending. Update individual lines as each directory is fully synced.

```text
pending .claude
pending .claude/commands
pending .gemini
pending .github
pending .github/workflows
pending LicenseInformation
skip lib
inventory acmacros
skip build
skip CMakeFiles
pending cmake
pending cmake/modules
inventory config
pending docs
pending docs/Rose
pending docs/Rose/AstProcessing
pending docs/Rose/ToolDevelopment
pending docs/Rose/Tutorial
pending docs/Rose/powerpoints
pending docs/Rosetta
pending docs/readmes
pending docs/todos
in-progress docs/upstream-sync
pending exampleTranslators
pending exampleTranslators/AstCopyReplTester
pending exampleTranslators/DOTGenerator
pending exampleTranslators/PDFGenerator
pending exampleTranslators/defaultTranslator
pending exampleTranslators/documentedExamples
pending exampleTranslators/documentedExamples/astProcessingExamples
pending exampleTranslators/documentedExamples/astProcessingExamples/printLoopInfo
pending exampleTranslators/documentedExamples/astProcessingExamples/printVars
pending exampleTranslators/documentedExamples/simpleTranslatorExamples
pending exampleTranslators/graphicalUserInterfaceExamples
pending exampleTranslators/graphicalUserInterfaceExamples/attributes
pending exampleTranslators/graphicalUserInterfaceExamples/layout
pending exampleTranslators/graphicalUserInterfaceExamples/query
pending exampleTranslators/graphicalUserInterfaceExamples/slicing
pending scripts
pending scripts/buildExampleRoseWorkspaceDirectory
pending scripts/buildExampleRoseWorkspaceDirectory/config
pending scripts/cgi-bin
pending scripts/configuration
pending scripts/installation
pending scripts/policies
pending src
in-progress src/AstNodes
in-progress src/AstNodes/Expression
skip src/AstNodes/BinaryAnalysis
skip src/AstNodes/Jovial
skip src/3rdPartyLibraries
in-progress src/ROSETTA
in-progress src/ROSETTA/Grammar
in-progress src/ROSETTA/src
pending src/ROSETTA/src/scripts
pending src/Rose
pending src/backend
in-progress src/backend/unparser
in-progress src/backend/unparser/CxxCodeGeneration
pending src/backend/unparser/FortranCodeGeneration
in-progress src/backend/unparser/formatSupport
in-progress src/backend/unparser/languageIndependenceSupport
in-progress src/frontend
in-progress src/frontend/Clang
in-progress src/frontend/Flang
in-progress src/frontend/Flang/builder
in-progress src/frontend/Flang/driver
synced src/frontend/Flang/intrinsics
in-progress src/frontend/SageIII
pending src/frontend/SageIII/GENERATED_CODE_DIRECTORY_Cxx_Grammar
pending src/frontend/SageIII/accparser
pending src/frontend/SageIII/astFixup
pending src/frontend/SageIII/astHiddenTypeAndDeclarationLists
pending src/frontend/SageIII/astPostProcessing
in-progress src/frontend/SageIII/astTokenStream
pending src/frontend/SageIII/docs
pending src/frontend/SageIII/includeDirectivesProcessing
pending src/frontend/SageIII/ompparser
in-progress src/frontend/SageIII/sageInterface
in-progress src/frontend/SageIII/sage_support
pending src/frontend/SageIII/virtualCFG
skip src/integration
skip src/integration/ROSE-LLVM-connection
pending src/midend
pending src/midend/abstractLayer
pending src/midend/astDiagnostics
pending src/midend/astDump
pending src/midend/astProcessing
pending src/midend/astQuery
pending src/midend/astUtil
pending src/midend/astUtil/annotation
pending src/midend/astUtil/astInterface
pending src/midend/astUtil/astSupport
pending src/midend/astUtil/symbolicVal
pending src/midend/programAnalysis
pending src/midend/programAnalysis/CFG
pending src/midend/programAnalysis/CallGraphAnalysis
pending src/midend/programAnalysis/OAWrap
pending src/midend/programAnalysis/OpenAnalysis
pending src/midend/programAnalysis/OpenAnalysis/CFG
pending src/midend/programAnalysis/OpenAnalysis/CallGraph
pending src/midend/programAnalysis/OpenAnalysis/Interface
pending src/midend/programAnalysis/OpenAnalysis/SSA
pending src/midend/programAnalysis/OpenAnalysis/Utils
pending src/midend/programAnalysis/VirtualFunctionAnalysis
pending src/midend/programAnalysis/bitvectorDataflow
pending src/midend/programAnalysis/dataflowAnalysis
pending src/midend/programAnalysis/defUseAnalysis
pending src/midend/programAnalysis/dominanceAnalysis
pending src/midend/programAnalysis/genericDataflow
pending src/midend/programAnalysis/genericDataflow/analysis
pending src/midend/programAnalysis/genericDataflow/arrIndexLabeler
pending src/midend/programAnalysis/genericDataflow/cfgUtils
pending src/midend/programAnalysis/genericDataflow/lattice
pending src/midend/programAnalysis/genericDataflow/rwAccessLabeler
pending src/midend/programAnalysis/genericDataflow/simpleAnalyses
pending src/midend/programAnalysis/genericDataflow/state
pending src/midend/programAnalysis/genericDataflow/variables
pending src/midend/programAnalysis/pointerAnal
pending src/midend/programAnalysis/staticInterproceduralSlicing
pending src/midend/programAnalysis/valuePropagation
pending src/midend/programAnalysis/variableRenaming
pending src/midend/programTransformation
pending src/midend/programTransformation/astInlining
pending src/midend/programTransformation/astOutlining
pending src/midend/programTransformation/astOutlining/lib_test
pending src/midend/programTransformation/constantFolding
pending src/midend/programTransformation/extractFunctionArgumentsNormalization
pending src/midend/programTransformation/finiteDifferencing
pending src/midend/programTransformation/functionCallNormalization
pending src/midend/programTransformation/implicitCodeGeneration
pending src/midend/programTransformation/loopProcessing
pending src/midend/programTransformation/loopProcessing/computation
pending src/midend/programTransformation/loopProcessing/depGraph
pending src/midend/programTransformation/loopProcessing/depInfo
pending src/midend/programTransformation/loopProcessing/driver
pending src/midend/programTransformation/loopProcessing/outsideInterface
pending src/midend/programTransformation/loopProcessing/prepostTransformation
pending src/midend/programTransformation/loopProcessing/slicing
pending src/midend/programTransformation/ompLowering
pending src/midend/programTransformation/partialRedundancyElimination
pending src/midend/programTransformation/singleStatementToBlockNormalization
pending src/midend/programTransformation/transformationTracking
pending src/util
pending src/util/StringUtility
pending src/util/commandlineProcessing
pending src/util/graphs
skip src/util/Sawyer
pending src/util/support
pending tests
pending tests/nonsmoke
pending tests/nonsmoke/ExamplesForTestWriters
pending tests/nonsmoke/acceptance
pending tests/nonsmoke/functional
in-progress tests/nonsmoke/functional/CompileTests
pending tests/nonsmoke/functional/CompileTests/C++Code
pending tests/nonsmoke/functional/CompileTests/C11_tests
pending tests/nonsmoke/functional/CompileTests/C89_std_c89_tests
pending tests/nonsmoke/functional/CompileTests/C99_tests
pending tests/nonsmoke/functional/CompileTests/C_subset_of_Cxx_tests
in-progress tests/nonsmoke/functional/CompileTests/C_tests
pending tests/nonsmoke/functional/CompileTests/CudaTests
pending tests/nonsmoke/functional/CompileTests/Cxx03_tests
pending tests/nonsmoke/functional/CompileTests/Cxx11_tests
pending tests/nonsmoke/functional/CompileTests/Cxx14_tests
pending tests/nonsmoke/functional/CompileTests/Cxx17_tests
pending tests/nonsmoke/functional/CompileTests/Cxx20_tests
in-progress tests/nonsmoke/functional/CompileTests/Cxx_tests
pending tests/nonsmoke/functional/CompileTests/ElsaTestCases
pending tests/nonsmoke/functional/CompileTests/ElsaTestCases/big
pending tests/nonsmoke/functional/CompileTests/ElsaTestCases/ctests
pending tests/nonsmoke/functional/CompileTests/ElsaTestCases/gnu
pending tests/nonsmoke/functional/CompileTests/ElsaTestCases/gnu/bugs
pending tests/nonsmoke/functional/CompileTests/ElsaTestCases/kandr
pending tests/nonsmoke/functional/CompileTests/ElsaTestCases/notCompilable
pending tests/nonsmoke/functional/CompileTests/ElsaTestCases/notCompilable/c
pending tests/nonsmoke/functional/CompileTests/ElsaTestCases/notCompilable/c99
pending tests/nonsmoke/functional/CompileTests/ElsaTestCases/notCompilable/gnu
pending tests/nonsmoke/functional/CompileTests/ElsaTestCases/notCompilable/std
pending tests/nonsmoke/functional/CompileTests/ElsaTestCases/std
pending tests/nonsmoke/functional/CompileTests/ExpressionTemplateExample_tests
pending tests/nonsmoke/functional/CompileTests/Fortran_tests
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/experimental_frontend_tests
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/gfortranTestSuite
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/gfortranTestSuite/gfortran.dg
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/gfortranTestSuite/gfortran.dg/debug
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/gfortranTestSuite/gfortran.dg/g77
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/gfortranTestSuite/gfortran.dg/gomp
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/gfortranTestSuite/gfortran.dg/gomp/appendix-a
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/gfortranTestSuite/gfortran.dg/graphite
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/gfortranTestSuite/gfortran.dg/guality
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/gfortranTestSuite/gfortran.dg/lto
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/gfortranTestSuite/gfortran.dg/vect
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/gfortranTestSuite/gfortran.fortran-torture
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/referenceResults
pending tests/nonsmoke/functional/CompileTests/OpenACC_tests
pending tests/nonsmoke/functional/CompileTests/OpenACC_tests/fortran
pending tests/nonsmoke/functional/CompileTests/OpenACC_tests/fortran/referenceResults
pending tests/nonsmoke/functional/CompileTests/OpenClTests
pending tests/nonsmoke/functional/CompileTests/OpenMP_tests
pending tests/nonsmoke/functional/CompileTests/OpenMP_tests/cvalidation
pending tests/nonsmoke/functional/CompileTests/OpenMP_tests/dataRaceBench
pending tests/nonsmoke/functional/CompileTests/OpenMP_tests/fortran
pending tests/nonsmoke/functional/CompileTests/OpenMP_tests/fortran/referenceResults
pending tests/nonsmoke/functional/CompileTests/OpenMP_tests/referenceResults
pending tests/nonsmoke/functional/CompileTests/OvertureCode
pending tests/nonsmoke/functional/CompileTests/RoseExample_tests
pending tests/nonsmoke/functional/CompileTests/STL_tests
pending tests/nonsmoke/functional/CompileTests/STL_tests/missing-support
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test0
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test1
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test10
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test11
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test12
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test13
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test14
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test15
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test16
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test16/subdir
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test17
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test18
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test2
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test3
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test4
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test5
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test5/subdir
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test6
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test6/subdir
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test7
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test7/subdir
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test8
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test8/subdir
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersTests/test9
in-progress tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test0
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test1
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test10
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test11
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test12
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test13
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test14
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test15
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test16
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test2
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test3
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test4
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test5
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test5/subdir
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test6
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test6/subdir
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test7
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test7/subdir
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test8
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test8/subdir
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests/test9
pending tests/nonsmoke/functional/CompileTests/copyAST_tests
pending tests/nonsmoke/functional/CompileTests/experimental_fortran_tests
pending tests/nonsmoke/functional/CompileTests/frontend_integration
pending tests/nonsmoke/functional/CompileTests/hiddenTypeAndDeclarationListTests
pending tests/nonsmoke/functional/CompileTests/hiddenTypeAndDeclarationListTests/additional_tests_Dan
pending tests/nonsmoke/functional/CompileTests/mergeAST_tests
pending tests/nonsmoke/functional/CompileTests/mixLanguage_tests
pending tests/nonsmoke/functional/CompileTests/nameQualificationAndTypeElaboration_tests
pending tests/nonsmoke/functional/CompileTests/sizeofOperation_tests
pending tests/nonsmoke/functional/CompileTests/sourcePosition_tests
pending tests/nonsmoke/functional/CompileTests/staticCFG_tests
pending tests/nonsmoke/functional/CompileTests/systemc_tests
pending tests/nonsmoke/functional/CompileTests/uninitializedField_tests
pending tests/nonsmoke/functional/CompileTests/unparseToString_tests
pending tests/nonsmoke/functional/CompileTests/unparse_template_from_ast
pending tests/nonsmoke/functional/CompileTests/virtualCFG_tests
pending tests/nonsmoke/functional/CompilerOptionsTests
pending tests/nonsmoke/functional/CompilerOptionsTests/collectAllCommentsAndDirectives_tests
pending tests/nonsmoke/functional/CompilerOptionsTests/preinclude_tests
pending tests/nonsmoke/functional/CompilerOptionsTests/testAnsiOption
pending tests/nonsmoke/functional/CompilerOptionsTests/testCpreprocessorOption
pending tests/nonsmoke/functional/CompilerOptionsTests/testCpreprocessorOption/doChooseMe
pending tests/nonsmoke/functional/CompilerOptionsTests/testCpreprocessorOption/doNotChooseMe
pending tests/nonsmoke/functional/CompilerOptionsTests/testFileNamesAndExtensions
pending tests/nonsmoke/functional/CompilerOptionsTests/testForSpuriousOutput
pending tests/nonsmoke/functional/CompilerOptionsTests/testGenerateSourceFileNames
pending tests/nonsmoke/functional/CompilerOptionsTests/testGnuOptions
pending tests/nonsmoke/functional/CompilerOptionsTests/testHeaderFileOutput
pending tests/nonsmoke/functional/CompilerOptionsTests/testHeaderFileOutput/myinclude
pending tests/nonsmoke/functional/CompilerOptionsTests/testIncludeOptions
pending tests/nonsmoke/functional/CompilerOptionsTests/testIncludeOptions/include
pending tests/nonsmoke/functional/CompilerOptionsTests/testIncludeOptions/system_include
pending tests/nonsmoke/functional/CompilerOptionsTests/testNostdincOption
pending tests/nonsmoke/functional/CompilerOptionsTests/testOutputFileOption
pending tests/nonsmoke/functional/KnownBugs
pending tests/nonsmoke/functional/KnownBugs/AttachPreprocessingInfo
pending tests/nonsmoke/functional/UnitTests
pending tests/nonsmoke/functional/UnitTests/Rose
pending tests/nonsmoke/functional/UnitTests/Rose/SageBuilder
pending tests/nonsmoke/functional/UnitTests/include
pending tests/nonsmoke/functional/UnitTests/include/rose
pending tests/nonsmoke/functional/UnitTests/include/rose/tests
pending tests/nonsmoke/functional/UnitTests/include/rose/tests/unitTests
pending tests/nonsmoke/functional/Utility
pending tests/nonsmoke/functional/input_codes
pending tests/nonsmoke/functional/input_codes/cxx
pending tests/nonsmoke/functional/input_codes/minimal
pending tests/nonsmoke/functional/input_codes/system_header_like
pending tests/nonsmoke/functional/moveDeclarationTool
pending tests/nonsmoke/functional/moveDeclarationTool/referenceResults
pending tests/nonsmoke/functional/moveDeclarationTool/referenceResults/withmerge
pending tests/nonsmoke/functional/roseTests
pending tests/nonsmoke/functional/roseTests/ROSETTA
pending tests/nonsmoke/functional/roseTests/astDiagnostics
pending tests/nonsmoke/functional/roseTests/astInliningTests
pending tests/nonsmoke/functional/roseTests/astInterfaceTests
pending tests/nonsmoke/functional/roseTests/astInterfaceTests/typeEquivalenceTests
pending tests/nonsmoke/functional/roseTests/astInterfaceTests/typeEquivalenceTests/inputFiles
pending tests/nonsmoke/functional/roseTests/astInterfaceTests/unitTests
pending tests/nonsmoke/functional/roseTests/astLValueTests
pending tests/nonsmoke/functional/roseTests/astMergeTests
pending tests/nonsmoke/functional/roseTests/astOutliningTests
pending tests/nonsmoke/functional/roseTests/astOutliningTests/reference
pending tests/nonsmoke/functional/roseTests/astPerformanceTests
pending tests/nonsmoke/functional/roseTests/astProcessingTests
pending tests/nonsmoke/functional/roseTests/astQueryTests
pending tests/nonsmoke/functional/roseTests/astSymbolTableTests
in-progress tests/nonsmoke/functional/roseTests/astTokenStreamTests
pending tests/nonsmoke/functional/roseTests/astTokenStreamTests/tests
pending tests/nonsmoke/functional/roseTests/fileLocation_tests
pending tests/nonsmoke/functional/roseTests/mergeTraversal_tests
pending tests/nonsmoke/functional/roseTests/ompLoweringTests
pending tests/nonsmoke/functional/roseTests/ompLoweringTests/REXReferenceManual
pending tests/nonsmoke/functional/roseTests/ompLoweringTests/REXReferenceTest
pending tests/nonsmoke/functional/roseTests/ompLoweringTests/ROSEXOMPReference
pending tests/nonsmoke/functional/roseTests/ompLoweringTests/fortran
pending tests/nonsmoke/functional/roseTests/programAnalysisTests
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/defUseAnalysisTests
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/defUseAnalysisTests/tests
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/generalDataFlowAnalysisTests
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/staticInterproceduralSlicingTests
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/testCallGraphAnalysis
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/testCallGraphAnalysis/test01-specimens
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/testCallGraphAnalysis/test03-answers
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/testCallGraphAnalysis/test03-specimens
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/testCallGraphAnalysis/test04-answers
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/typeTraitTests
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/typeTraitTests/tests
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/variableLivenessTests
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/variableLivenessTests/tests
pending tests/nonsmoke/functional/roseTests/programAnalysisTests/variableRenamingTests
pending tests/nonsmoke/functional/roseTests/programTransformationTests
pending tests/nonsmoke/functional/roseTests/programTransformationTests/extractFunctionArgumentsTest
pending tests/nonsmoke/functional/roseTests/programTransformationTests/referenceOutput
pending tests/nonsmoke/functional/roseTests/programTransformationTests/singleStatementToBlockNormalization
pending tests/nonsmoke/functional/roseTests/varDeclNorm
pending tests/nonsmoke/functional/RunTests
pending tests/nonsmoke/functional/test_deleteAST
pending tests/nonsmoke/functional/testSupport
pending tests/nonsmoke/functional/translatorTests
pending tests/nonsmoke/functional/utilities
pending tests/nonsmoke/specimens
pending tests/nonsmoke/specimens/c++
pending tests/nonsmoke/specimens/c
pending tests/nonsmoke/specimens/fortran
pending tests/nonsmoke/unit
pending tests/nonsmoke/unit/SageInterface
pending tests/nonsmoke/unit/SageInterface/referenceOutput
pending tests/nonsmoke/unit/SageInterface/tests
pending tests/smoke
pending tests/smoke/ExamplesForTestWriters
pending tests/smoke/acceptance
pending tests/smoke/functional
pending tests/smoke/functional/Fortran
pending tests/smoke/specimens
pending tests/smoke/specimens/c++
pending tests/smoke/specimens/c
pending tests/smoke/specimens/fortran
pending tests/smoke/unit
pending tests/smoke/unit/Utility
pending tools
pending tools/checkFortranInterfaces
pending tools/checkFortranInterfaces/tests
pending tools/classMemberVariablesInLambdas
pending tools/featureVector
pending tools/globalVariablesInLambdas
pending tutorial
pending tutorial/outliner
pending tutorial/spawn-frontend
```

## Local/generated directories (not tracked, skip)
- skip build/
- skip CMakeFiles/
- skip lib/

## Drop list (upstream-only paths not in REX)
Any upstream directory not present in the keep list is dropped by default. The explicit list below names known upstream-only subtrees.
Note: tests/nonsmoke/functional/RunTests, tests/nonsmoke/functional/testSupport, tests/nonsmoke/functional/roseTests/ROSETTA,
and tests/nonsmoke/functional/roseTests/astInterfaceTests/unitTests are kept (see inventory).

Top-level upstream-only directories:
- install-staging/
- projects/
- python/
- solaris-includes/
- utilities/
- winspecific/

Core source subtrees dropped in REX:
- src/3rdPartyLibraries/
- src/3rdPartyLibraries/MSTL/
- src/3rdPartyLibraries/POET/
- src/3rdPartyLibraries/experimental-cplusplus-parser/
- src/3rdPartyLibraries/experimental-jovial-parser/
- src/3rdPartyLibraries/java-parser/
- src/3rdPartyLibraries/json/
- src/3rdPartyLibraries/qrose/
- src/integration/
- src/frontend/BinaryFormats/
- src/frontend/Disassemblers/
- src/frontend/ECJ_ROSE_Connection/
- src/frontend/Experimental_Ada_ROSE_Connection/
- src/frontend/Experimental_Csharp_ROSE_Connection/
- src/frontend/Experimental_Jovial_ROSE_Connection/
- src/frontend/Experimental_Matlab_ROSE_Connection/
- src/frontend/PHPFrontend/
- src/frontend/PythonFrontend/
- src/frontend/X10_ROSE_Connection/
- src/frontend/CxxFrontend/EDG/
- src/frontend/SageIII/astFileIO/
- src/frontend/SageIII/astFromString/
- src/frontend/SageIII/astVisualization/
- src/midend/BinaryAnalysis/
- src/midend/abstractHandle/
- src/midend/abstractMemoryObject/
- src/midend/astMatching/
- src/midend/astRewriteMechanism/
- src/midend/astSnippet/
- src/midend/programAnalysis/annotationLanguageParser/
- src/backend/asmUnparser/
- src/backend/unparser/AdaCodeGeneration/
- src/backend/unparser/JavaCodeGeneration/
- src/backend/unparser/JovialCodeGeneration/
- src/backend/unparser/MatlabCodeGeneration/
- src/backend/unparser/PHPCodeGeneration/
- src/backend/unparser/PythonCodeGeneration/
- src/backend/unparser/X10CodeGeneration/
- src/util/Sawyer/
- src/util/stringSupport/
- src/Rose/AST/
- src/Rose/BinaryAnalysis/
- src/Rose/CodeGen/
- src/Rose/Color/
- src/Rose/CommandLine/
- src/Rose/Diagnostics/
- src/Rose/FileSystem/
- src/Rose/Traits/
- src/roseSupport/

Explicit file drops (never reintroduce):
- src/Rose/BitFlags.h
- src/Rose/GraphUtility.h
- src/Rose/Diagnostics.h
- tests/nonsmoke/functional/Utility/bitFlags.C
- tests/nonsmoke/functional/Utility/progressReports.C
- tests/nonsmoke/functional/Utility/graphIO.C
- tests/nonsmoke/functional/Utility/graphPerformance.C
- tests/nonsmoke/functional/Utility/hash.C

Docs/scripts/tools/tutorial/tests dropped from upstream:
- docs/IDE-Hints/
- scripts/nmiBuildAndTestFarm/
- scripts/spack/
- scripts/tup/
- tutorial/TAU_INCLUDE_DIR/
- tutorial/binaryAnalysis/
- tutorial/blockingTutorial/
- tutorial/intelPin/
- tutorial/roseHPCT/
- tools/BinaryAnalysis/
- tools/CodeThorn/
- tools/PortabilityTesting/
- tools/tests/
- tests/CompileTests/
- tests/roseTests/

Dropped languages/platforms (any upstream paths related to these are dropped by default):
- PHP, JavaScript, Python, Java, UPC, Ada, Jovial, Matlab, X10, C#, YAML/mini-yaml.
- Non-Linux platform-specific code or tests.

## Automated per-commit sync workflow
1) Start from `origin/main` with a clean working tree. Ensure `rose` points at `https://github.com/llnl/rose.git`, disable its push URL, then fetch `rose/weekly`.
2) Use the repo-local skill and helper scripts:
   ```bash
   python3 .codex/skills/rose-upstream-sync/scripts/list_pending.py
   python3 .codex/skills/rose-upstream-sync/scripts/classify_commit.py <upstream-sha>
   ```
3) Walk pending upstream commits in chronological order from the latest recorded upstream SHA in the newest non-empty sync CSV.
4) For each upstream commit:
   - inspect with `git show <commit>`;
   - decide `drop`, `pick`, `pick-partial`, `drop-superseded`, or `already-present`;
   - apply only retained REX scope;
   - run `guard_dropped_paths.py --staged` before committing;
   - validate the touched subsystem;
   - commit with `Upstream-ROSE`, `Sync-Decision`, and `Sync-Log` trailers;
   - update the current sync CSV immediately, including the REX commit SHA.
5) For version-only upstream commits:
   - record intermediate release bumps as `drop-superseded`;
   - after the useful sync range is otherwise complete, apply the latest upstream version marker to `ROSE_VERSION` and `config/SCM_DATE`;
   - run `verify_version.py`;
   - never restore `configure.ac` or `src/frontend/CxxFrontend/EDG_VERSION`.
6) When the sync range is complete, run the required validations:
   ```bash
   python3 .codex/skills/rose-upstream-sync/scripts/verify_coverage.py --csv docs/upstream-sync/rose-2026-commits.csv
   cmake --build build -j32
   ctest --test-dir build --output-on-failure -j32
   ```
7) The sync is not complete until `verify_coverage.py` proves every upstream commit in the range has a CSV row, every picked upstream SHA maps to a REX commit, and full CTest passes without regressions.
