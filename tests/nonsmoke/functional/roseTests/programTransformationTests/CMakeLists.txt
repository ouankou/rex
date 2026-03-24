add_subdirectory(extractFunctionArgumentsTest)
add_subdirectory(singleStatementToBlockNormalization)

function(rex_configure_generated_output_verify_test setup_test verify_test)
  string(MAKE_C_IDENTIFIER "${setup_test}_fixture" _fixture)
  set_tests_properties(${setup_test} PROPERTIES FIXTURES_SETUP "${_fixture}")
  set_tests_properties(${verify_test} PROPERTIES FIXTURES_REQUIRED "${_fixture}")
endfunction()

#-------------------------------------------------------------------------------
# preTest

add_executable(preTest preTest.C)
target_link_libraries(preTest ROSE_DLL ${link_with_libraries})

set(files_to_test pass1.C pass2.C pass3.C)
set(pre_verify_tests)
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
  rex_configure_generated_output_verify_test(pre_${file_to_test} pre_${file_to_test}_verify)
  list(APPEND pre_verify_tests pre_${file_to_test}_verify)
endforeach()
string(JOIN "\\;" pre_verify_tests_arg ${pre_verify_tests})

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
rex_configure_generated_output_verify_test(pr_rewrite_test1.C pr_rewrite_test1.C_verify)
set(pattern_rewrite_verify_tests pr_rewrite_test1.C_verify)
string(JOIN "\\;" pattern_rewrite_verify_tests_arg ${pattern_rewrite_verify_tests})

#-------------------------------------------------------------------------------
# finiteDifferencingTest

add_executable(finiteDifferencingTest finiteDifferencingTest.C)
target_link_libraries(finiteDifferencingTest
  ROSE_DLL ${link_with_libraries})

set(files_to_test
  finitediff_test1.C finitediff_test2.C finitediff_test3.C)
set(fd_verify_tests)
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
  rex_configure_generated_output_verify_test(fd_${file_to_test} fd_${file_to_test}_verify)
  list(APPEND fd_verify_tests fd_${file_to_test}_verify)
endforeach()
string(JOIN "\\;" fd_verify_tests_arg ${fd_verify_tests})

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

