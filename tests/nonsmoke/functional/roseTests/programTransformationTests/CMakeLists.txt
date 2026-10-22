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

set(constant_domain_source
  "${CMAKE_CURRENT_BINARY_DIR}/rose_rex_pattern_rewrite_constant_domains.cpp")
set(constant_domain_binary
  "${CMAKE_CURRENT_BINARY_DIR}/rex_pattern_rewrite_constant_domains")
add_test(NAME pr_rex_pattern_rewrite_constant_domains
  COMMAND patternRewriteTest -std=c++17
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_pattern_rewrite_constant_domains.cpp"
    -rose:output "${constant_domain_source}")
add_test(NAME pr_rex_pattern_rewrite_constant_domains_compile
  COMMAND "${CMAKE_CXX_COMPILER}" -std=c++17 -fsanitize=undefined
    -fno-sanitize-recover=undefined "${constant_domain_source}"
    -o "${constant_domain_binary}")
add_test(NAME pr_rex_pattern_rewrite_constant_domains_runtime
  COMMAND "${constant_domain_binary}")
rex_configure_generated_output_verify_test(
  pr_rex_pattern_rewrite_constant_domains
  pr_rex_pattern_rewrite_constant_domains_compile)
rex_configure_generated_output_verify_test(
  pr_rex_pattern_rewrite_constant_domains_compile
  pr_rex_pattern_rewrite_constant_domains_runtime)

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
  add_test(
    NAME fd_${file_to_test}_compile
    COMMAND "${CMAKE_CXX_COMPILER}" -std=c++17 -fsyntax-only
      "${CMAKE_CURRENT_BINARY_DIR}/rose_${file_to_test}"
  )
  rex_configure_generated_output_verify_test(fd_${file_to_test} fd_${file_to_test}_compile)
  list(APPEND fd_verify_tests fd_${file_to_test}_verify)
endforeach()
string(JOIN "\\;" fd_verify_tests_arg ${fd_verify_tests})

set(fd_initialization_source
  "${CMAKE_CURRENT_BINARY_DIR}/rose_rex_finite_differencing_initialization.cpp")
set(fd_initialization_binary
  "${CMAKE_CURRENT_BINARY_DIR}/rex_finite_differencing_initialization")
add_test(NAME fd_rex_finite_differencing_initialization
  COMMAND finiteDifferencingTest -std=c++17
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_finite_differencing_initialization.cpp"
    -rose:output "${fd_initialization_source}")
add_test(NAME fd_rex_finite_differencing_initialization_compile
  COMMAND "${CMAKE_CXX_COMPILER}" -std=c++17 -O1 -Werror=uninitialized -fsanitize=undefined
    -fno-sanitize-recover=undefined "${fd_initialization_source}"
    -o "${fd_initialization_binary}")
add_test(NAME fd_rex_finite_differencing_initialization_runtime
  COMMAND "${fd_initialization_binary}")
rex_configure_generated_output_verify_test(
  fd_rex_finite_differencing_initialization
  fd_rex_finite_differencing_initialization_compile)
rex_configure_generated_output_verify_test(
  fd_rex_finite_differencing_initialization_compile
  fd_rex_finite_differencing_initialization_runtime)

set(fd_c_initialization_source
  "${CMAKE_CURRENT_BINARY_DIR}/rose_rex_finite_differencing_c_initialization.c")
add_test(NAME fd_rex_finite_differencing_c_initialization
  COMMAND finiteDifferencingTest -x c -std=c11
    "${CMAKE_CURRENT_SOURCE_DIR}/finitediff_test1.C"
    -rose:output "${fd_c_initialization_source}")
add_test(NAME fd_rex_finite_differencing_c_initialization_compile
  COMMAND "${CMAKE_C_COMPILER}" -std=c11 -fsyntax-only
    "${fd_c_initialization_source}")
rex_configure_generated_output_verify_test(
  fd_rex_finite_differencing_c_initialization
  fd_rex_finite_differencing_c_initialization_compile)

#-------------------------------------------------------------------------------
# finiteDifferencingDemo

add_executable(finiteDifferencingDemo finiteDifferencingDemo.C)
target_link_libraries(finiteDifferencingDemo
  ROSE_DLL ${link_with_libraries})

add_test(
  NAME finiteDifferencingDemo_finitediff_test1.C
  COMMAND finiteDifferencingDemo
    -c ${CMAKE_CURRENT_SOURCE_DIR}/finitediff_test1.C
    -rose:output rose_finiteDifferencingDemo_finitediff_test1.C
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
  -icg:shortCircuitingTransformation
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

add_test(
  NAME rex_icg_destructor_lifetime_compile
  COMMAND implicitCodeGenerationTest ${icg_SWITCHES}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_icg_destructor_lifetime.cpp
    -o ${CMAKE_CURRENT_BINARY_DIR}/rex_icg_destructor_lifetime
)
set_tests_properties(rex_icg_destructor_lifetime_compile PROPERTIES
  FIXTURES_SETUP rex_icg_destructor_lifetime
)
add_test(
  NAME rex_icg_destructor_lifetime
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/rex_icg_destructor_lifetime
)
set_tests_properties(rex_icg_destructor_lifetime PROPERTIES
  FIXTURES_REQUIRED rex_icg_destructor_lifetime
)

add_test(
  NAME rex_icg_logical_values_compile
  COMMAND implicitCodeGenerationTest -icg:shortCircuitingTransformation
    -std=c++17 ${CMAKE_CURRENT_SOURCE_DIR}/rex_icg_logical_values.cpp
    -o ${CMAKE_CURRENT_BINARY_DIR}/rex_icg_logical_values
)
set_tests_properties(rex_icg_logical_values_compile PROPERTIES
  FIXTURES_SETUP rex_icg_logical_values
)
add_test(
  NAME rex_icg_logical_values
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/rex_icg_logical_values
)
set_tests_properties(rex_icg_logical_values PROPERTIES
  FIXTURES_REQUIRED rex_icg_logical_values
)

add_test(
  NAME rex_icg_elided_construction_compile
  COMMAND implicitCodeGenerationTest -std=c++17
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_icg_elided_construction.cpp
    -o ${CMAKE_CURRENT_BINARY_DIR}/rex_icg_elided_construction
)
set_tests_properties(rex_icg_elided_construction_compile PROPERTIES
  FIXTURES_SETUP rex_icg_elided_construction
)
add_test(
  NAME rex_icg_elided_construction
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/rex_icg_elided_construction
)
set_tests_properties(rex_icg_elided_construction PROPERTIES
  FIXTURES_REQUIRED rex_icg_elided_construction
)

add_test(
  NAME rex_icg_c_aggregate_compile
  COMMAND implicitCodeGenerationTest -std=c11
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_icg_c_aggregate.c
    -o ${CMAKE_CURRENT_BINARY_DIR}/rex_icg_c_aggregate
)
set_tests_properties(rex_icg_c_aggregate_compile PROPERTIES
  FIXTURES_SETUP rex_icg_c_aggregate
)
add_test(
  NAME rex_icg_c_aggregate
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/rex_icg_c_aggregate
)
set_tests_properties(rex_icg_c_aggregate PROPERTIES
  FIXTURES_REQUIRED rex_icg_c_aggregate
)
