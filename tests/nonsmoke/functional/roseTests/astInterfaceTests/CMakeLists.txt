# CMakeLists.txt for astInterfaceTests

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
  buildLabelStatement
  buildSizeOfOp
  abiStuffTest
  buildNullStatement
  buildForStmt
  canonicalLoop
  loopNormalization
  collectReadWriteVariables
  typeTraits
  livenessAnalysis
  getDependentDecls
  buildStructDeclaration2
  buildTypedefDeclaration
  deepDelete
  insertStatementBeforeFunction
  removeStatementCommentRelocation
  generateUniqueName
  annotateExpressionsWithUniqueNames
  replaceWithPattern
  insertBeforeUsingCommaOp
  insertAfterUsingCommaOp
  deepCopy
  fixVariableReferences
  getArrayElementType
  interfaceFunctionCoverage
  buildTemplateClass
  buildUsingDirectiveStatement
  normalizeArrowExpWithAddressOfLeftOperand
  templateQualification
  rex_test2026_getDependentDecls
  rex_test2026_declaration_scope
  rex_test2026_template_symbol_table
  rex_test2026_null_expression_unparse
  rex_test2026_template_constraints_ir
  rex_test2026_deduction_ordering
  rex_test2026_param_list_unique_name
  rex_test2026_sfinae_prune
  rex_test2026_on_demand_qualification
  rex_test2026_fold_pack_expr
  rex_test2026_stl_vector
  rex_test2026_stl_tuple
  rex_test2026_stl_type_traits
  rex_test2026_stl_variant
  rex_test2026_stl_regex
)

# Fortran-specific executables (only build when ENABLE-FORTRAN=ON)
set(fortran_programs
  buildProcedureHeaderStatement
  buildExternalStatement
  buildCommonBlock
  doLoopNormalization
  buildLabelStatement2
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
set(all_programs ${bin_programs} ${noinst_programs})
if(ENABLE-FORTRAN)
  list(APPEND all_programs ${fortran_programs})
endif()

foreach(prog ${all_programs})
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
  normalizeArrowExpWithAddressOfLeftOperand
  rex_test2026_getDependentDecls
  rex_test2026_declaration_scope
  rex_test2026_template_symbol_table
  rex_test2026_null_expression_unparse
  rex_test2026_deduction_ordering
  rex_test2026_param_list_unique_name
  rex_test2026_on_demand_qualification
)

foreach(test ${group1_tests})
  add_test(
    NAME astInterface_${test}
    COMMAND ${test} ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/input${test}.C
  )
endforeach()

set_tests_properties(astInterface_interfaceFunctionCoverage PROPERTIES FAIL_REGULAR_EXPRESSION "In addToNameMap\(\): Allow this for replacement now!;Default reached in SgScopeStatement::isNamedScope" )

set(_disable_interface_coverage FALSE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.5")
  set(_disable_interface_coverage TRUE)
endif()
if(_disable_interface_coverage)
  set_tests_properties(astInterface_interfaceFunctionCoverage PROPERTIES DISABLED TRUE)
endif()

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
  NAME astInterface_rex_test2026_template_constraints_ir
  COMMAND rex_test2026_template_constraints_ir ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_template_constraints_ir.C
)

add_test(
  NAME astInterface_rex_test2026_sfinae_prune
  COMMAND rex_test2026_sfinae_prune ${TEST_CXXFLAGS} -rex:clang:continue-on-error -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_sfinae_prune.C
)

add_test(
  NAME astInterface_rex_test2026_fold_pack_expr
  COMMAND rex_test2026_fold_pack_expr ${TEST_CXXFLAGS} -std=c++17 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_fold_pack_expr.C
)

add_test(
  NAME astInterface_rex_test2026_stl_vector
  COMMAND rex_test2026_stl_vector ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_stl_vector.C
)

add_test(
  NAME astInterface_rex_test2026_stl_tuple
  COMMAND rex_test2026_stl_tuple ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_stl_tuple.C
)

add_test(
  NAME astInterface_rex_test2026_stl_type_traits
  COMMAND rex_test2026_stl_type_traits ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_stl_type_traits.C
)

add_test(
  NAME astInterface_rex_test2026_stl_variant
  COMMAND rex_test2026_stl_variant ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_stl_variant.C
)

add_test(
  NAME astInterface_rex_test2026_stl_regex
  COMMAND rex_test2026_stl_regex ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_stl_regex.C
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

# abiStuffTest (doesn't produce C file)
add_test(
  NAME astInterface_abiStuffTest
  COMMAND abiStuffTest -rex:clang:continue-on-error ${CMAKE_CURRENT_SOURCE_DIR}/inputAbiStuffTest.c
)

# deepDelete test (doesn't create output file)
add_test(
  NAME astInterface_deepDelete
  COMMAND deepDelete ${TEST_CXXFLAGS} -rose:detect_dangling_pointers 1 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputdeepDelete.C
)
set(_disable_deep_delete FALSE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9")
  set(_disable_deep_delete TRUE)
endif()
if(_disable_deep_delete)
  set_tests_properties(astInterface_deepDelete PROPERTIES DISABLED TRUE)
endif()

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

# insertStatementBeforeFunction test (needs -I flag)
add_test(
  NAME astInterface_insertStatementBeforeFunction
  COMMAND insertStatementBeforeFunction ${TEST_CXXFLAGS} -rex:clang:continue-on-error -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputinsertStatementBeforeFunction.C
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
    COMMAND removeStatementCommentRelocation ${TEST_CXXFLAGS} -rex:clang:continue-on-error -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/${test}.C
  )
endforeach()

# templateQualification test - exercises template qualification correctness.
# Tests that:
#   1. Non-type template parameters preserve names (no "template<0>")
#   2. No redundant global qualifiers (no "::std::vector")
# Uses test_template_qualification.cpp as input
add_test(
  NAME astInterface_templateQualification
  COMMAND templateQualification ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/test_template_qualification.cpp
)

# Fortran tests (only if Fortran support is enabled)
if(ENABLE-FORTRAN)
  foreach(test ${fortran_programs})
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

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_astInterfaceTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astInterface_abiStuffTest\;astInterface_deepDelete\;astInterface_movePreprocessingInfo
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astInterfaceTests_check-local PROPERTIES DEPENDS "astInterface_abiStuffTest;astInterface_deepDelete;astInterface_movePreprocessingInfo" )
