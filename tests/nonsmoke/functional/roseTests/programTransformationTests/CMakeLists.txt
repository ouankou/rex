add_subdirectory(extractFunctionArgumentsTest)
add_subdirectory(singleStatementToBlockNormalization)

#-------------------------------------------------------------------------------
# preTest

add_executable(preTest preTest.C)
target_link_libraries(preTest ROSE_DLL ${link_with_libraries})

set(files_to_test pass1.C pass2.C pass3.C)
foreach(file_to_test ${files_to_test})
  add_test(
    NAME pre_${file_to_test}
    COMMAND preTest ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  add_test(
    NAME pre_${file_to_test}_verify
    COMMAND ${CMAKE_COMMAND} -E compare_files
      ${CMAKE_CURRENT_SOURCE_DIR}/rose_${file_to_test}.save
      ${CMAKE_CURRENT_BINARY_DIR}/rose_${file_to_test}
  )
  set_tests_properties(pre_${file_to_test}_verify
    PROPERTIES DEPENDS pre_${file_to_test})
endforeach()

#-------------------------------------------------------------------------------
# patternRewriteTest

add_executable(patternRewriteTest patternRewriteTest.C)
target_link_libraries(patternRewriteTest ROSE_DLL ${link_with_libraries})

add_test(
  NAME pr_rewrite_test1.C
  COMMAND patternRewriteTest ${CMAKE_CURRENT_SOURCE_DIR}/rewrite_test1.C
)
add_test(
  NAME pr_rewrite_test1.C_verify
  COMMAND ${CMAKE_COMMAND} -E compare_files
    ${CMAKE_CURRENT_SOURCE_DIR}/rose_rewrite_test1.C.save
    ${CMAKE_CURRENT_BINARY_DIR}/rose_rewrite_test1.C
)
set_tests_properties(pr_rewrite_test1.C_verify
  PROPERTIES DEPENDS pr_rewrite_test1.C)

#-------------------------------------------------------------------------------
# finiteDifferencingTest

add_executable(finiteDifferencingTest finiteDifferencingTest.C)
target_link_libraries(finiteDifferencingTest
  ROSE_DLL ${link_with_libraries})

set(files_to_test
  finitediff_test1.C finitediff_test2.C finitediff_test3.C)
foreach(file_to_test ${files_to_test})
  add_test(
    NAME fd_${file_to_test}
    COMMAND finiteDifferencingTest ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  add_test(
    NAME fd_${file_to_test}_verify
    COMMAND ${CMAKE_COMMAND} -E compare_files
      ${CMAKE_CURRENT_SOURCE_DIR}/rose_${file_to_test}.save
      ${CMAKE_CURRENT_BINARY_DIR}/rose_${file_to_test}
  )
  set_tests_properties(fd_${file_to_test}_verify
    PROPERTIES DEPENDS fd_${file_to_test})
endforeach()

#-------------------------------------------------------------------------------
# finiteDifferencingDemo

add_executable(finiteDifferencingDemo finiteDifferencingDemo.C)
target_link_libraries(finiteDifferencingDemo
  ROSE_DLL ${link_with_libraries})

add_test(
  NAME finiteDifferencingDemo_finitediff_test1.C
  COMMAND finiteDifferencingDemo
    -c ${CMAKE_CURRENT_SOURCE_DIR}/finitediff_test1.C
)
set_tests_properties(finiteDifferencingDemo_finitediff_test1.C PROPERTIES DISABLED TRUE)

#-------------------------------------------------------------------------------
# testFunctionNormalization

add_executable(testFunctionNormalization testFunctionNormalization.C)
target_link_libraries(testFunctionNormalization
  ROSE_DLL ${link_with_libraries})

set(function_normalization_tests
  functionNormalizationTest1.C
  functionNormalizationTest2.C)
foreach(file_to_test ${function_normalization_tests})
  add_test(
    NAME functionNormalization_${file_to_test}
    COMMAND testFunctionNormalization -rose:verbose 0
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  set_tests_properties(functionNormalization_${file_to_test} PROPERTIES DISABLED TRUE)
endforeach()

#-------------------------------------------------------------------------------
# constantFoldingTest

add_executable(constantFoldingTest constantFoldingTest.C)
target_link_libraries(constantFoldingTest ROSE_DLL ${link_with_libraries})

add_executable(constantFoldingTest2 constantFoldingTest2.C)
target_link_libraries(constantFoldingTest2 ROSE_DLL ${link_with_libraries})

add_test(
  NAME constantFoldingTest_1
  COMMAND constantFoldingTest
    -c ${CMAKE_CURRENT_SOURCE_DIR}/inputCode_constantFoldingTransformation.C
)

add_test(
  NAME constantFoldingTest_2
  COMMAND constantFoldingTest2
    -c ${CMAKE_CURRENT_SOURCE_DIR}/inputCode_constantFoldingTransformation.C
	  -rose:output rose_inputCode_constantFoldingTransformation_1.C
)

add_test(
  NAME constantFoldingTest_3
  COMMAND constantFoldingTest2
    -c ${CMAKE_CURRENT_SOURCE_DIR}/inputCode_constantFoldingTransformation2.C
)

add_test(
  NAME cf_1
  COMMAND constantFoldingTest
    -c ${CMAKE_CURRENT_SOURCE_DIR}/inputCode_constantFoldingTransformation.C
)

add_test(
  NAME cf_2
  COMMAND constantFoldingTest2
    -c ${CMAKE_CURRENT_SOURCE_DIR}/inputCode_constantFoldingTransformation.C
    -rose:output rose_inputCode_constantFoldingTransformation_1.C
)

add_test(
  NAME cf_3
  COMMAND constantFoldingTest2
    -c ${CMAKE_CURRENT_SOURCE_DIR}/inputCode_constantFoldingTransformation2.C
)

set_tests_properties(constantFoldingTest_1 PROPERTIES DEPENDS cf_1)
set_tests_properties(constantFoldingTest_2 PROPERTIES DEPENDS cf_2)
set_tests_properties(constantFoldingTest_3 PROPERTIES DEPENDS cf_3)

#-------------------------------------------------------------------------------
# implicitCodeGenerationTest

add_executable(implicitCodeGenerationTest implicitCodeGenerationTest.C)
target_link_libraries(implicitCodeGenerationTest
  ROSE_DLL ${link_with_libraries})

set(icg_SWITCHES
  -icg:defaultFunctionGenerator -icg:shortCircuitingTransformation
  -icg:destructorCallAnnotator)

add_test(
  NAME icg_1
  COMMAND implicitCodeGenerationTest ${icg_SWITCHES}
    -c ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests/test2004_116.C
)

add_test(
  NAME icg_2
  COMMAND implicitCodeGenerationTest ${icg_SWITCHES}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/inputCode_implicitCodeGenerationTest.C
)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_programTransformationTests_check-constantFoldingTests
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=constantFoldingTest_1\;constantFoldingTest_2\;constantFoldingTest_3
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programTransformationTests_check-constantFoldingTests PROPERTIES
  LABELS "disabled"
  DEPENDS "constantFoldingTest_1;constantFoldingTest_2;constantFoldingTest_3"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_programTransformationTests_check-constantFoldingTests PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_programTransformationTests_check-finiteDifferencingTest
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=fd_finitediff_test1.C\;fd_finitediff_test2.C\;fd_finitediff_test3.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programTransformationTests_check-finiteDifferencingTest PROPERTIES
  LABELS "disabled"
  DEPENDS "fd_finitediff_test1.C;fd_finitediff_test2.C;fd_finitediff_test3.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_programTransformationTests_check-finiteDifferencingTest PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_programTransformationTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=constantFoldingTest_1\;constantFoldingTest_2\;constantFoldingTest_3\;fd_finitediff_test1.C\;fd_finitediff_test2.C\;fd_finitediff_test3.C\;finiteDifferencingDemo_finitediff_test1.C\;functionNormalization_functionNormalizationTest1.C\;icg_1\;icg_2\;pr_rewrite_test1.C\;pre_pass1.C\;pre_pass2.C\;pre_pass3.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programTransformationTests_check-local PROPERTIES
  DEPENDS "constantFoldingTest_1;constantFoldingTest_2;constantFoldingTest_3;fd_finitediff_test1.C;fd_finitediff_test2.C;fd_finitediff_test3.C;finiteDifferencingDemo_finitediff_test1.C;functionNormalization_functionNormalizationTest1.C;icg_1;icg_2;pr_rewrite_test1.C;pre_pass1.C;pre_pass2.C;pre_pass3.C"
)
add_test(
  NAME nonsmoke_functional_roseTests_programTransformationTests_check-patternRewriteTest
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=pr_rewrite_test1.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programTransformationTests_check-patternRewriteTest PROPERTIES
  LABELS "disabled"
  DEPENDS "pr_rewrite_test1.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_programTransformationTests_check-patternRewriteTest PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_programTransformationTests_check-preTest
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=pre_pass1.C\;pre_pass2.C\;pre_pass3.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programTransformationTests_check-preTest PROPERTIES
  LABELS "disabled"
  DEPENDS "pre_pass1.C;pre_pass2.C;pre_pass3.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_programTransformationTests_check-preTest PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_programTransformationTests_conditinal-check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=finiteDifferencingDemo_finitediff_test1.C\;functionNormalization_functionNormalizationTest1.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programTransformationTests_conditinal-check-local PROPERTIES
  LABELS "disabled"
  DEPENDS "finiteDifferencingDemo_finitediff_test1.C;functionNormalization_functionNormalizationTest1.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_programTransformationTests_conditinal-check-local PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_programTransformationTests_conditional-check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=constantFoldingTest_1\;constantFoldingTest_2\;constantFoldingTest_3\;fd_finitediff_test1.C\;fd_finitediff_test2.C\;fd_finitediff_test3.C\;finiteDifferencingDemo_finitediff_test1.C\;functionNormalization_functionNormalizationTest1.C\;icg_1\;icg_2\;pr_rewrite_test1.C\;pre_pass1.C\;pre_pass2.C\;pre_pass3.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programTransformationTests_conditional-check-local PROPERTIES
  DEPENDS "constantFoldingTest_1;constantFoldingTest_2;constantFoldingTest_3;fd_finitediff_test1.C;fd_finitediff_test2.C;fd_finitediff_test3.C;finiteDifferencingDemo_finitediff_test1.C;functionNormalization_functionNormalizationTest1.C;icg_1;icg_2;pr_rewrite_test1.C;pre_pass1.C;pre_pass2.C;pre_pass3.C"
)
