# CMakeLists.txt for astInterfaceTests
# Based on Makefile.am - provides CMake equivalents for autotools tests

# Test executables that will be installed
set(bin_programs
  loopInterchange
  loopUnrolling
  loopTiling
  loopCollapsingDirective
  loopCollapsing
)

# Test executables that will not be installed (for testing only)
set(noinst_programs
  buildFunctionDeclaration
  buildNondefiningFunction
  findMain
  buildVariableDeclaration
  buildAssignmentStmt
  buildFunctionCalls
  buildPragmaDeclaration
  attachComment
  insertHeader
  buildExpression
  buildStructDeclaration
  lookupNamedType
  buildFile
  movePreprocessingInfo
  buildIfStmt
  buildCpreprocessorDefineDeclaration
  instrumentEndOfFunction
  isUpcSharedType
  isUpcPhaseLessSharedType
  buildLabelStatement
  buildSizeOfOp
  abiStuffTest
  abiStuffTestUPC
  buildNullStatement
  buildForStmt
  canonicalLoop
  loopNormalization
  collectReadWriteVariables
  typeTraits
  livenessAnalysis
  buildProcedureHeaderStatement
  replaceMacroCalls
  getDependentDecls
  buildStructDeclaration2
  buildTypedefDeclaration
  deepDelete
  insertStatementBeforeFunction
  removeStatementCommentRelocation
  generateUniqueName
  annotateExpressionsWithUniqueNames
  buildExternalStatement
  buildCommonBlock
  doLoopNormalization
  buildLabelStatement2
  replaceWithPattern
  insertBeforeUsingCommaOp
  insertAfterUsingCommaOp
  deepCopy
  fixVariableReferences
  buildJavaPackage
  getArrayElementType
  interfaceFunctionCoverage
  buildTemplateClass
  buildUsingDirectiveStatement
)

# Include directories for test programs
include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/sageInterface
  ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/transformationTracking
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII
)

# Build all executables
foreach(prog ${bin_programs} ${noinst_programs})
  add_executable(${prog} ${prog}.C)
  target_link_libraries(${prog} ROSE_DLL ${link_with_libraries})
endforeach()

# Install bin_programs
install(TARGETS ${bin_programs} DESTINATION bin)

# Common test flags
set(TEST_CXXFLAGS -rose:verbose 0 -g3)

# Test definitions
# Group 1: Tests that follow the pattern: executable inputfile.C -> rose_inputfile.C
set(group1_tests
  fixVariableReferences
  findMain
  buildVariableDeclaration
  buildAssignmentStmt
  buildIfStmt
  buildFile
  buildExpression
  buildStructDeclaration
  buildStructDeclaration2
  buildTemplateClass
  buildCpreprocessorDefineDeclaration
  instrumentEndOfFunction
  buildLabelStatement
  buildSizeOfOp
  buildNullStatement
  buildForStmt
  canonicalLoop
  loopNormalization
  collectReadWriteVariables
  typeTraits
  interfaceFunctionCoverage
  livenessAnalysis
  generateUniqueName
  annotateExpressionsWithUniqueNames
  insertBeforeUsingCommaOp
  insertAfterUsingCommaOp
  deepCopy
  buildTypedefDeclaration
  getDependentDecls
  replaceWithPattern
  buildUsingDirectiveStatement
)

foreach(test ${group1_tests})
  add_test(
    NAME astInterface_${test}
    COMMAND ${test} ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/input${test}.C
  )
endforeach()

# buildFunctionDeclaration test (uses inputBlank1.C)
add_test(
  NAME astInterface_buildFunctionDeclaration
  COMMAND buildFunctionDeclaration ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputBlank1.C
)

# buildNondefiningFunction test (uses inputBlank2.C)
add_test(
  NAME astInterface_buildNondefiningFunction
  COMMAND buildNondefiningFunction ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputBlank2.C
)

# buildFunctionCalls test (needs -I flag)
add_test(
  NAME astInterface_buildFunctionCalls
  COMMAND buildFunctionCalls ${TEST_CXXFLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputbuildFunctionCalls.C
)

# buildPragmaDeclaration test (C file, not C++)
add_test(
  NAME astInterface_buildPragmaDeclaration
  COMMAND buildPragmaDeclaration ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputbuildPragmaDeclaration.c
)

# Special named input files
add_test(
  NAME astInterface_attachComment
  COMMAND attachComment ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputAttachComment.C
)

add_test(
  NAME astInterface_insertHeader
  COMMAND insertHeader ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputInsertHeader.C
)

add_test(
  NAME astInterface_lookupNamedType
  COMMAND lookupNamedType ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputLookupNamedType.C
)

add_test(
  NAME astInterface_movePreprocessingInfo
  COMMAND movePreprocessingInfo ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputMovePreprocessingInfo.C
)

# UPC tests - DISABLED: UPC language not fully supported with Clang frontend
# The Clang frontend does not recognize .upc files and UPC language extensions
# TODO: Re-enable when UPC support is added to Clang frontend
# Note: These require -rose:skipfinalCompileStep -rose:upc_threads 1
# add_test(
#   NAME astInterface_isUpcSharedType
#   COMMAND isUpcSharedType ${TEST_CXXFLAGS} -rose:skipfinalCompileStep -rose:upc_threads 1 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputisUpcSharedType.upc
# )
#
# add_test(
#   NAME astInterface_isUpcPhaseLessSharedType
#   COMMAND isUpcPhaseLessSharedType ${TEST_CXXFLAGS} -rose:skipfinalCompileStep -rose:upc_threads 1 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputisUpcPhaseLessSharedType.upc
# )

# abiStuffTest (doesn't produce C file)
add_test(
  NAME astInterface_abiStuffTest
  COMMAND abiStuffTest ${CMAKE_CURRENT_SOURCE_DIR}/inputAbiStuffTest.c
)

# abiStuffTestUPC - DISABLED: UPC language not fully supported with Clang frontend
# add_test(
#   NAME astInterface_abiStuffTestUPC
#   COMMAND abiStuffTestUPC ${TEST_CXXFLAGS} -rose:upc_threads 1 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputAbiStuffTestUPC.upc
# )

# deepDelete test (doesn't create output file)
add_test(
  NAME astInterface_deepDelete
  COMMAND deepDelete ${TEST_CXXFLAGS} -rose:detect_dangling_pointers 1 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputdeepDelete.C
)

# loopUnrolling tests (multiple tests with different parameters)
add_test(
  NAME astInterface_loopUnrolling
  COMMAND loopUnrolling ${TEST_CXXFLAGS} -rose:loopunroll:line 5 -rose:loopunroll:factor 3 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopUnrolling.C
)

add_test(
  NAME astInterface_loopUnrolling1
  COMMAND loopUnrolling ${TEST_CXXFLAGS} -rose:loopunroll:line 6 -rose:loopunroll:factor 5 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopUnrolling.C -rose:output rose_inputloopUnrolling1.C
)

add_test(
  NAME astInterface_loopUnrolling2
  COMMAND loopUnrolling ${TEST_CXXFLAGS} -rose:loopunroll:line 6 -rose:loopunroll:factor 3 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopUnrolling.C -rose:output rose_inputloopUnrolling2.C
)

# loopCollapsing tests (multiple tests with different parameters)
add_test(
  NAME astInterface_loopCollapsing_1
  COMMAND loopCollapsing ${TEST_CXXFLAGS} -rose:loopcollapse:line 5 -rose:loopcollapse:factor 2 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopCollapsing_1.C
)

add_test(
  NAME astInterface_loopCollapsing_2
  COMMAND loopCollapsing ${TEST_CXXFLAGS} -rose:loopcollapse:line 9 -rose:loopcollapse:factor 2 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopCollapsing_2.C
)

add_test(
  NAME astInterface_loopCollapsing_3
  COMMAND loopCollapsing ${TEST_CXXFLAGS} -rose:loopcollapse:line 9 -rose:loopcollapse:factor 3 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopCollapsing_3.C
)

add_test(
  NAME astInterface_loopCollapsing_4
  COMMAND loopCollapsing ${TEST_CXXFLAGS} -rose:loopcollapse:line 9 -rose:loopcollapse:factor 4 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopCollapsing_4.C
)

add_test(
  NAME astInterface_loopCollapsing_5
  COMMAND loopCollapsing ${TEST_CXXFLAGS} -rose:loopcollapse:line 9 -rose:loopcollapse:factor 5 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopCollapsing_5.C
)

# loopInterchange test
add_test(
  NAME astInterface_loopInterchange
  COMMAND loopInterchange ${TEST_CXXFLAGS} -rose:loopInterchange:line 5 -rose:loopInterchange:depth 4 -rose:loopInterchange:order 1 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopInterchange.C
)

# loopTiling test
add_test(
  NAME astInterface_loopTiling
  COMMAND loopTiling -rose:loopTiling:line 7 -rose:loopTiling:depth 3 -rose:loopTiling:tilesize 5 ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopTiling.C
)

# replaceMacroCalls test (disabled in Makefile.am due to boost:wave issues)
# add_test(
#   NAME astInterface_replaceMacroCalls
#   COMMAND replaceMacroCalls ${TEST_CXXFLAGS} -rose:wave -c ${CMAKE_CURRENT_SOURCE_DIR}/inputreplaceMacroCalls.C
# )

# insertStatementBeforeFunction test (needs -I flag)
add_test(
  NAME astInterface_insertStatementBeforeFunction
  COMMAND insertStatementBeforeFunction ${TEST_CXXFLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputinsertStatementBeforeFunction.C
)

# removeStatementCommentRelocation tests (multiple variants)
set(comment_relocation_tests
  inputRemoveStatementCommentRelocation
  inputRemoveStatementCommentRelocation_withTopBoundingStatement
  inputRemoveStatementCommentRelocation_withoutTopBoundingStatement
  inputRemoveStatementCommentRelocation_withBottomBoundingStatement
  inputRemoveStatementCommentRelocation_withoutBottomBoundingStatement
  inputRemoveStatementCommentRelocation_withTopandBottomBoundingStatement
  inputRemoveStatementCommentRelocation_withoutTopandBottomBoundingStatement
)

foreach(test ${comment_relocation_tests})
  add_test(
    NAME astInterface_${test}
    COMMAND removeStatementCommentRelocation ${TEST_CXXFLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/${test}.C
  )
endforeach()

# buildJavaPackage test (doesn't produce output source files)
add_test(
  NAME astInterface_buildJavaPackage
  COMMAND buildJavaPackage ${TEST_CXXFLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputbuildJavaPackage.C
)

# Fortran tests (only if Fortran support is enabled)
if(enable-fortran)
  set(fortran_tests
    buildProcedureHeaderStatement
    buildExternalStatement
    buildCommonBlock
    doLoopNormalization
    buildLabelStatement2
  )

  foreach(test ${fortran_tests})
    add_test(
      NAME astInterface_${test}
      COMMAND ${test} ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/input${test}.f
    )
  endforeach()
endif()

# Add subdirectories if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/typeEquivalenceTests/CMakeLists.txt)
  add_subdirectory(typeEquivalenceTests)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/CMakeLists.txt)
  add_subdirectory(unitTests)
endif()
