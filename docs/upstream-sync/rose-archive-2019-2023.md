# rose-archive 2019-2023 manual sync (full REX scope)

## Support target (REX)
- Platform: Linux only (Windows/mac dropped).
- Languages/features: C/C++, Fortran, OpenMP, OpenACC, OpenCL, CUDA.
- Frontend: Clang/LLVM only (LLVM 20).
- Hard dropped: PHP, JavaScript, EDG, Java, UPC, Python, binary analysis, and other legacy frontends.

## Non-negotiable rules
- Top rule: do not pause for any reason until the PR is created.
- Manual per-commit review only. No scripts, no batch triage, no bulk patch application.
- Keep list below is authoritative; only those paths are eligible for sync.
- Drop list below is authoritative; never reintroduce dropped paths or platforms.
- Autotools/Tup are inventory only: do not port into CMake.
- Preserve REX pruning decisions; do not reintroduce Boost-only subsystems or removed frontends.
- Preserve REX modernizations that removed deprecated C++ template patterns; do not reintroduce those warning-prone constructs.

## Root files (keep list + sync status)
Status legend: pending / in-progress / synced / inventory / skip
- pending .gitignore
- pending .gitmodules
- pending AGENTS.md
- pending BUILDING_WITH_CLANG.md
- pending CLANG_FRONTEND_FIXES.md
- pending CLANG_FRONTEND_IMPROVEMENTS.md
- pending CLAUDE.md
- pending CMakeLists.txt
- pending COPYRIGHT
- pending FORTRAN_EVALUATION_STATUS.md
- pending FORTRAN_TESTING_GUIDE.md
- pending GEMINI.md
- inventory Makefile.am
- pending OPENMP_SUPPORT.md
- pending README.md
- pending ROSE_COMPILER_FIXES.md
- pending ROSE_VERSION
- pending TEMPLATE_INSTANTIATION_ROADMAP.md
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
inventory acmacros
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
pending docs/testDoxygen
pending docs/todos
pending docs/upstream-sync
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
pending src/3rdPartyLibraries
pending src/3rdPartyLibraries/antlr-jars
pending src/3rdPartyLibraries/fortran-parser
pending src/3rdPartyLibraries/libharu-2.1.0
pending src/3rdPartyLibraries/libharu-2.1.0/demo
pending src/3rdPartyLibraries/libharu-2.1.0/demo/images
pending src/3rdPartyLibraries/libharu-2.1.0/demo/mbtext
pending src/3rdPartyLibraries/libharu-2.1.0/demo/pngsuite
pending src/3rdPartyLibraries/libharu-2.1.0/demo/rawimage
pending src/3rdPartyLibraries/libharu-2.1.0/demo/ttfont
pending src/3rdPartyLibraries/libharu-2.1.0/demo/type1
pending src/3rdPartyLibraries/libharu-2.1.0/doc
pending src/3rdPartyLibraries/libharu-2.1.0/if
pending src/3rdPartyLibraries/libharu-2.1.0/if/c#
pending src/3rdPartyLibraries/libharu-2.1.0/if/c#/demo
pending src/3rdPartyLibraries/libharu-2.1.0/if/c#/demo/pngsuite
pending src/3rdPartyLibraries/libharu-2.1.0/if/c#/demo/rawimage
pending src/3rdPartyLibraries/libharu-2.1.0/if/delphi
pending src/3rdPartyLibraries/libharu-2.1.0/if/freebasic
pending src/3rdPartyLibraries/libharu-2.1.0/if/ruby
pending src/3rdPartyLibraries/libharu-2.1.0/if/ruby/demo
pending src/3rdPartyLibraries/libharu-2.1.0/if/vb6
pending src/3rdPartyLibraries/libharu-2.1.0/include
pending src/3rdPartyLibraries/libharu-2.1.0/script
pending src/3rdPartyLibraries/libharu-2.1.0/src
pending src/3rdPartyLibraries/libharu-2.1.0/win32
pending src/3rdPartyLibraries/libharu-2.1.0/win32/bcc32
pending src/3rdPartyLibraries/libharu-2.1.0/win32/include
pending src/3rdPartyLibraries/libharu-2.1.0/win32/mingw
pending src/3rdPartyLibraries/libharu-2.1.0/win32/msvc
pending src/ROSETTA
pending src/ROSETTA/Grammar
pending src/ROSETTA/src
pending src/ROSETTA/src/scripts
pending src/Rose
pending src/backend
pending src/backend/unparser
pending src/backend/unparser/CxxCodeGeneration
pending src/backend/unparser/FortranCodeGeneration
pending src/backend/unparser/formatSupport
pending src/backend/unparser/languageIndependenceSupport
pending src/frontend
pending src/frontend/CxxFrontend
pending src/frontend/CxxFrontend/Clang
pending src/frontend/OpenFortranParser_SAGE_Connection
pending src/frontend/SageIII
pending src/frontend/SageIII/GENERATED_CODE_DIRECTORY_Cxx_Grammar
pending src/frontend/SageIII/accparser
pending src/frontend/SageIII/astFixup
pending src/frontend/SageIII/astHiddenTypeAndDeclarationLists
pending src/frontend/SageIII/astPostProcessing
pending src/frontend/SageIII/astTokenStream
pending src/frontend/SageIII/docs
pending src/frontend/SageIII/includeDirectivesProcessing
pending src/frontend/SageIII/ompparser
pending src/frontend/SageIII/sageInterface
pending src/frontend/SageIII/sage_support
pending src/frontend/SageIII/virtualCFG
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
pending src/util/support
pending tests
pending tests/nonsmoke
pending tests/nonsmoke/ExamplesForTestWriters
pending tests/nonsmoke/acceptance
pending tests/nonsmoke/functional
pending tests/nonsmoke/functional/CompileTests
pending tests/nonsmoke/functional/CompileTests/A++Code
pending tests/nonsmoke/functional/CompileTests/A++Tests
pending tests/nonsmoke/functional/CompileTests/C++Code
pending tests/nonsmoke/functional/CompileTests/C11_tests
pending tests/nonsmoke/functional/CompileTests/C89_std_c89_tests
pending tests/nonsmoke/functional/CompileTests/C99_tests
pending tests/nonsmoke/functional/CompileTests/CAF2_tests
pending tests/nonsmoke/functional/CompileTests/C_subset_of_Cxx_tests
pending tests/nonsmoke/functional/CompileTests/C_tests
pending tests/nonsmoke/functional/CompileTests/CudaTests
pending tests/nonsmoke/functional/CompileTests/Cxx03_tests
pending tests/nonsmoke/functional/CompileTests/Cxx11_tests
pending tests/nonsmoke/functional/CompileTests/Cxx14_tests
pending tests/nonsmoke/functional/CompileTests/Cxx17_tests
pending tests/nonsmoke/functional/CompileTests/Cxx20_tests
pending tests/nonsmoke/functional/CompileTests/Cxx_tests
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
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/LANL_POP
pending tests/nonsmoke/functional/CompileTests/Fortran_tests/RiceCAF_tests
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
pending tests/nonsmoke/functional/CompileTests/P++Tests
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
pending tests/nonsmoke/functional/CompileTests/UnparseHeadersUsingTokenStream_tests
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
pending tests/nonsmoke/functional/CompileTests/colorAST_tests
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
pending tests/nonsmoke/functional/CompilerOptionsTests/A++Code
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
pending tests/nonsmoke/functional/RunTests
pending tests/nonsmoke/functional/RunTests/A++Tests
pending tests/nonsmoke/functional/RunTests/AstDeleteTests
pending tests/nonsmoke/functional/RunTests/C_tests
pending tests/nonsmoke/functional/RunTests/Cxx_tests
pending tests/nonsmoke/functional/RunTests/FortranTests
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/cxx
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/cxx4
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/examples
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/examples/C
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/examples/CDL
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/examples/CXX
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/examples/CXX4
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/examples/F77
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/examples/F90
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/f90
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/fortran
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/cfcheck
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/doc
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec/shared
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec/shared/mosaic
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec/tools
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec/tools/fregrid
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec/tools/make_coupler_mosaic
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec/tools/make_hgrid
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec/tools/make_solo_mosaic
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec/tools/make_topog
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec/tools/make_vgrid
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec/tools/river_regrid
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec/tools/shared
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/gridspec/tools/transfer_to_mosaic_grid
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/m4
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libcf/src
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libncdap3
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libncdap3/oc
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libncdap4
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libsrc
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/libsrc4
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/m4
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/nc_test
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/nc_test4
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/ncdap_test
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/ncdap_test/expected3
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/ncdap_test/expected4
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/ncdap_test/expectremote3
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/ncdap_test/expectremote4
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/ncdap_test/testdata3
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/ncdump
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/ncdump/cdl4
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/ncdump/expected4
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/ncgen
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/ncgen3
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/nctest
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/nf_test
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/udunits
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/udunits/expat
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/udunits/lib
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/udunits/lib/xmlFailures
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/udunits/lib/xmlSuccesses
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/udunits/m4
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/udunits/prog
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/netcdf-4.1.1/udunits/test
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/doc
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/doc/userguide
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/doc/web
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/input_templates
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/mpi
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/run
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/run/compile
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/serial
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/source
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/tools
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/tools/eos
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/tools/grid
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP/pop-distro/tools/validation
pending tests/nonsmoke/functional/RunTests/FortranTests/LANL_POP_OLD
pending tests/nonsmoke/functional/RunTests/P++Tests
pending tests/nonsmoke/functional/RunTests/multigridTest
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
pending tests/nonsmoke/functional/roseTests/astTokenStreamTests
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
pending tests/nonsmoke/functional/testSupport
pending tests/nonsmoke/functional/testSupport/gtest
pending tests/nonsmoke/functional/testSupport/gtest/include
pending tests/nonsmoke/functional/testSupport/gtest/include/gtest
pending tests/nonsmoke/functional/testSupport/gtest/include/gtest/internal
pending tests/nonsmoke/functional/testSupport/gtest/src
pending tests/nonsmoke/functional/test_deleteAST
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

Top-level upstream-only directories:
- install-staging/
- projects/
- python/
- solaris-includes/
- utilities/
- winspecific/

Core source subtrees dropped in REX:
- src/3rdPartyLibraries/MSTL/
- src/3rdPartyLibraries/POET/
- src/3rdPartyLibraries/experimental-cplusplus-parser/
- src/3rdPartyLibraries/experimental-jovial-parser/
- src/3rdPartyLibraries/flang-parser/
- src/3rdPartyLibraries/java-parser/
- src/3rdPartyLibraries/json/
- src/3rdPartyLibraries/qrose/
- src/frontend/BinaryFormats/
- src/frontend/Disassemblers/
- src/frontend/ECJ_ROSE_Connection/
- src/frontend/Experimental_Ada_ROSE_Connection/
- src/frontend/Experimental_Csharp_ROSE_Connection/
- src/frontend/Experimental_Flang_ROSE_Connection/
- src/frontend/Experimental_General_Language_Support/
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
- src/Rose/StringUtility/
- src/Rose/Traits/

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
- PHP, JavaScript, Python, Java, UPC, Ada, Jovial, Matlab, X10, C#.
- Windows/mac-specific code or tests.

## Manual sync workflow (no batch processing)
1) Start from `origin/main` and review upstream commits one by one (2019-01-01 → 2023-10-26).
2) For each commit:
   - Inspect the diff with `git show <commit>`.
   - Decide pick/drop/pending based on the keep/drop lists above.
   - If mixed hunks in a file, apply only the relevant hunks manually.
3) Update the commit log below immediately after each decision.
4) Update directory status lines as each directory is fully synced.
5) Validate with:
   ```bash
   cmake --build build -j32
   ctest --test-dir build --output-on-failure -R astInterface
   ctest --test-dir build --output-on-failure -R rex
   ```

## Commit decision log (append-only)
No upstream commits reviewed yet for this restart. Append entries below as each commit is handled.

| Upstream commit | Date | Summary | Paths touched | Decision | REX commit | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| 6ae04d2d84d58e1e6d48c84ae704c61664edaed3 | 2019-01-01 | Add C++17 regression tests | src/backend/unparser; config/support-rose.m4; tests/CompileTests; tests/nonsmoke/functional/CompileTests/Makefile.am | drop |  | Tests live under dropped `tests/CompileTests`; only unparser change is comment/debug print; autotools inventory only. |
| 22297d7765c18d5945dc5f27294508519478c9c3 | 2019-01-01 | C++17 support follow-up | src/backend/unparser | drop |  | Removes a blank line only; no functional change. |
| e117da6ebbfeb6e52be9ba66768e32f04e70e1f2 | 2019-01-01 | Nargin test fix | projects/MatlabTranslation | drop |  | Matlab translation is dropped in REX. |
| cb2f1596fb49d37e44595a0043b072aaa074a286 | 2019-01-01 | Merge origin/master | widespread (Tup/config/projects/binary/Python/etc.) | drop |  | Merge commit spanning dropped and inventory-only subsystems; not safe to cherry-pick. |
| 5f91c83a79112032c84d4111a9dd26aec8856028 | 2019-01-01 | Bump ROSE version | ROSE_VERSION; configure.ac | drop |  | REX maintains its own versioning; configure.ac is inventory only. |
| 2b620a23fbb871c565cd25b51af0f3f3bd54accb | 2019-01-02 | Add C++17 tests (batch 2) | tests/CompileTests/Cxx17_tests | drop |  | `tests/CompileTests` is dropped in REX. |
| f3d10bba63eb0ac7b65d9ebec0b6f2c4c66c58bf | 2019-01-02 | Merge origin/master | widespread (Tup/config/projects/binary/Python/etc.) | drop |  | Merge commit spanning dropped and inventory-only subsystems; not safe to cherry-pick. |
| b33975f95ea4b62b67fc8d9ae46299d779c1411c | 2019-01-02 | Bump ROSE version | ROSE_VERSION; configure.ac | drop |  | REX maintains its own versioning; configure.ac is inventory only. |
| f7f710e4eb5f9e170fb80f7f04a399538244055f | 2019-01-02 | Matlab type inference tweaks | projects/MatlabTranslation | drop |  | Matlab translation is dropped in REX. |
| dab0af90093b799da12bff01b3091d3b6360be44 | 2019-01-02 | M2Cxx script robustness | projects/MatlabTranslation | drop |  | Matlab translation is dropped in REX. |
| f766fb51e83ba90c90aec1e2dd72c34e9415e58a | 2019-01-03 | Matlab test categorization | projects/MatlabTranslation | drop |  | Matlab translation is dropped in REX. |
| f1936ce54727864a6acd8f2d773c3a76210a5c12 | 2019-01-03 | Merge origin/master | widespread (EDG/binary/Tup/tests/etc.) | drop |  | Merge commit spanning dropped and inventory-only subsystems; not safe to cherry-pick. |
| 0214bad6b6a56f9775d4f8f6c7e2b36dc1eb01eb | 2019-01-03 | Bump ROSE version | ROSE_VERSION; configure.ac | drop |  | REX maintains its own versioning; configure.ac is inventory only. |
| 810e30048ee8c83bf1d1d458952966df15292ca2 | 2019-01-03 | Update C++11 tests | tests/CompileTests/Cxx11_tests | drop |  | `tests/CompileTests` is dropped in REX. |
| ea03e474b097557a154d024a97d4f9abd2af247d | 2019-01-04 | Matlab translator test classification | projects/MatlabTranslation | drop |  | Matlab translation is dropped in REX. |
| fc87d71376ea6dd6dcf266478145120b0b33597c | 2019-01-04 | Merge origin/master | widespread (EDG/binary/Tup/tests/etc.) | drop |  | Merge commit spanning dropped and inventory-only subsystems; not safe to cherry-pick. |
| 9eda7be23021c228272881527eb20dbe1a00c7d1 | 2019-01-04 | Bump ROSE version | ROSE_VERSION; configure.ac | drop |  | REX maintains its own versioning; configure.ac is inventory only. |
| 87075e624b8747712a7d3f5b25570740ca7a69f2 | 2019-01-04 | C++11 test list update | tests/CompileTests/Cxx11_tests | drop |  | `tests/CompileTests` is dropped in REX. |
| 23360b59665e0412658e6902d07692dc77ae45d2 | 2019-01-04 | Merge origin/master | widespread (EDG/binary/Tup/tests/etc.) | drop |  | Merge commit spanning dropped and inventory-only subsystems; not safe to cherry-pick. |
| f557f24a66782662e67ac0b8d5da2d4cc7799c7b | 2019-01-04 | Bump ROSE version | ROSE_VERSION; configure.ac | drop |  | REX maintains its own versioning; configure.ac is inventory only. |
| 7b9e2acfaf0468c9bb3dc86130cb3bd888f788fe | 2019-01-05 | Unparse __float80/__float128 | src/backend/unparser/CxxCodeGeneration/unparseCxx_types.C | drop |  | Already present in REX. |
| 439ee4c79549385cc82111d656a6121bacf1b67c | 2019-01-05 | Control __float128 support | config/rose_edg_required_macros_and_functions.h.in; src/frontend/SageIII/sage_support/cmdline.cpp | drop |  | EDG-only macro wiring; REX is Clang-only. |
| 638978f235b10a3e4fbde1e9de50f1410521e5f9 | 2019-01-06 | _FloatXXX macro support | config/rose_edg_required_macros_and_functions.h.in | drop |  | EDG config header; inventory only. |
| 8d1b444888d1d564a2816aa004c3b4a46f77e7f2 | 2019-01-07 | Merge branch master | widespread (EDG/binary/Tup/projects/tests/etc.) | drop |  | Merge commit; contains dropped subsystems and inventory-only changes. |
| ef61eb044566b54d4d1c320f158bf717027c0f49 | 2019-01-07 | Fortran Bind(C) fixes | src/frontend/OpenFortranParser_SAGE_Connection/FortranParserActionROSE.C | drop |  | Already present in REX. |
| 0d628306d95df6a142356d6e1db1f46ad931b43a | 2019-01-07 | Merge priv-rose | widespread (projects/binary/python/Tup/tests/etc.) | drop |  | Merge commit; contains many dropped/inventory subsystems. |
| 40829904f22493caaae93f4a13c4a2ca4ed8376f | 2019-01-07 | Fortran Bind(C) fixes | src/frontend/OpenFortranParser_SAGE_Connection/FortranParserActionROSE.C | drop |  | Already present in REX (duplicate of earlier fix). |
| 8ce85a02df81a03840a1b0f46dcdd86e4ba594bf | 2019-01-08 | EDG builtin_addressof | src/frontend/CxxFrontend/EDG | drop |  | EDG frontend is dropped in REX. |
| e536e5965c98bb5e5b49a64b6564ae3d2df978b8 | 2019-01-08 | Apple clang config fix | config/* | drop |  | macOS support dropped; config is inventory only. |
| b380bc5b1ea2e7058361e0037ac2bb9c2965ef34 | 2019-01-08 | float80/128 + boost path tweak | src/backend/unparser/*; src/frontend/SageIII/sage_support/cmdline.cpp; EDG | drop |  | float80/128 handling already in REX; boost path logic not used and would reintroduce boost behavior; EDG change dropped. |
| 52ee5a178e074ee4c3cacb83284b23eaff874de1 | 2019-01-08 | typeforge unsupported types fix | projects/typeforge | drop |  | typeforge is dropped in REX. |
| 0408fd937ddafff4d549e3fe8acc6dbe35a5a632 | 2019-01-08 | CodeThorn manual author | projects/CodeThorn | drop |  | CodeThorn is dropped in REX. |
| 0b66bdc9204e968706449af63d398165b4ea9f7e | 2019-01-09 | Bump ROSE version | ROSE_VERSION; configure.ac | drop |  | REX maintains its own versioning; configure.ac is inventory only. |
| 6cbfb4851ff8fd1d154207ca9baf5bea33b6a0b0 | 2019-01-09 | SgNodeHelper function-call in decls | src/midend/abstractLayer/SgNodeHelper.* | drop |  | Already present in REX. |
| 98910cb562aff87a6bad401e87c79d18c6e3a047 | 2019-01-09 | Labeler function-call init handling | src/midend/abstractLayer/Labeler.C | drop |  | File not present in REX (pruned); CodeThorn test dropped. |
| 0986162d4b2d56f17e936808d2dc19a304ca7d7a | 2019-01-09 | CodeThorn transfer for init call | projects/CodeThorn | drop |  | CodeThorn is dropped in REX. |
| a47cb8a260b18e6670730c42191a997232801262 | 2019-01-10 | CPP directive + Java cmdline fix | src/frontend/SageIII/*; tests/CompileTests | drop |  | REX already carries the template/CPP attachment changes; Java cmdline handling removed (Java dropped). |
| dd21e23465423494f0b5227f9de8f1927d34d801 | 2019-01-10 | Add Cxx11 tests | tests/CompileTests/Cxx11_tests | drop |  | `tests/CompileTests` is dropped in REX. |
| 3f02775cc873290f8242ea7aa27d92f19c9668bb | 2019-01-10 | Java cmdline guard tweak | src/frontend/SageIII/sage_support/cmdline.cpp | drop |  | Java blocks are not present in REX; Java is dropped. |
| 063b168c3d3401d0053b793939f07062e33f0381 | 2019-01-10 | CodeThorn normalization tests | projects/CodeThorn | drop |  | CodeThorn is dropped in REX. |
