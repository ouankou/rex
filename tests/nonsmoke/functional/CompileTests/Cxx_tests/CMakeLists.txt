# this defines ROSE__CXX_TESTS and ROSE__CXX_TESTS__BOOST
# include(${CMAKE_CURRENT_SOURCE_DIR}/Cxx_Testcodes_bk.cmake)

include(Cxx_Testcodes.cmake)
include(Cxx_Failing_Tests.cmake)

set(CXX_TESTCODES_TO_RUN ${EXAMPLE_TESTCODES_REQUIRED_TO_PASS})
list(REMOVE_ITEM CXX_TESTCODES_TO_RUN ${TESTCODE_CURRENTLY_FAILING})

set(ROSE_FLAGS
  --edg:no_warnings -w -rose:verbose 0
  -rose:skip_unparse_asm_commands
  -rose:skip_unparse_cc_commands
  -I${CMAKE_CURRENT_SOURCE_DIR}/../A++Code
  )

if(REX_ENABLE_FAILING_TESTS)
  foreach(file_to_test ${TESTCODE_CURRENTLY_FAILING})
    compile_test(${file_to_test} "Cxx_tests_failing")
  endforeach()
endif()

foreach(file_to_test ${CXX_TESTCODES_TO_RUN})
  compile_test(${file_to_test} "Cxx_tests")
endforeach()

# Issue 91: Declaration chains must not emit "firstNondefiningDeclaration == NULL"
set_tests_properties(Cxx_tests_rex_test2025_issue91_decl_chain PROPERTIES
  FAIL_REGULAR_EXPRESSION "firstNondefiningDeclaration == NULL"
)

# Verify explicit template arguments are preserved in calls
add_test(
  NAME Cxx_tests_rex_test2025_template_call_explicit_args
  COMMAND sh -c "\"$@\" && grep \"foo<5>\" rose_rex_test2025_template_call.cpp | grep -v \"//\" && grep \"foo<10>\" rose_rex_test2025_template_call.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_template_call.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_template_call.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_template_call_explicit_args PROPERTIES LABELS Cxx_tests)

add_test(
  NAME Cxx_tests_rex_test2025_member_template_call_explicit_args
  COMMAND sh -c "\"$@\" && grep -E \"foo<[[:space:]]*5[[:space:]]*>\" rose_rex_test2025_member_template_call.cpp | grep -v \"//\" && grep -E \"foo<[[:space:]]*10[[:space:]]*>\" rose_rex_test2025_member_template_call.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_member_template_call.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_member_template_call.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_member_template_call_explicit_args PROPERTIES LABELS Cxx_tests)

add_test(
  NAME Cxx_tests_rex_test2025_variadic_template_params_unparse
  COMMAND sh -c "\"$@\" && grep -E \"template <typename[[:space:]]*\\\\.\\\\.\\\\.\" rose_rex_test2025_variadic_template_params.cpp | grep -v \"//\" && grep -E \"template <int[[:space:]]*\\\\.\\\\.\\\\.\" rose_rex_test2025_variadic_template_params.cpp | grep -v \"//\" && grep -E \"class[[:space:]]*\\\\.\\\\.\\\\.[[:space:]]*Templates\" rose_rex_test2025_variadic_template_params.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_variadic_template_params.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_variadic_template_params.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_variadic_template_params_unparse PROPERTIES LABELS Cxx_tests)

# REX: This test intentionally fails (it contains a function-try-block which is currently
# not fully supported and triggers an assertion failure or error). We want to ensure
# it continues to fail (failure propagation) until properly supported.
# We add it manually here (not in Cxx_Testcodes.cmake) to avoid breaking downstream tests.
# REX: This test intentionally fails. We verify it fails (including crashes) by wrapping in shell.
# We use "$@" to pass all arguments safely to the translator command, preserving quoting.
# If "$@" returns 0 (success), we exit 1 (fail). If it returns non-zero (fail), we exit 0 (pass).
add_test(
  NAME Cxx_tests_rex_test2025_issue99_failure_propagation
  COMMAND sh -c "\"$@\" && exit 1 || exit 0" dummy $<TARGET_FILE:${translator}> ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue99_failure_propagation.cpp
)

# REX: New test for Issue 87 (Functions inside namespaces lost)
# We verify that "void bar()" is present in the output associated with namespace behavior.
add_test(
  NAME Cxx_tests_rex_test2025_issue87_namespace
  COMMAND sh -c "\"$@\" && grep \"void bar()\" rose_rex_test2025_issue87_namespace.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue87_namespace.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue87_namespace.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue87_namespace PROPERTIES LABELS Cxx_tests)

add_test(
  NAME Cxx_tests_rex_test2025_issue89_type_alias_template_args
  COMMAND sh -c "\"$@\" && grep -E \"using IntContainer = Container< *int *>\" rose_rex_test2025_issue89_alias.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue89_alias.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue89_alias.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue89_type_alias_template_args PROPERTIES LABELS Cxx_tests)

add_test(
  NAME Cxx_tests_rex_test2025_issue90_array_dim_template_param
  COMMAND sh -c "\"$@\" && grep \"data\\[N\\]\" rose_rex_test2025_issue90_array_dim.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue90_array_dim.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue90_array_dim.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue90_array_dim_template_param PROPERTIES LABELS Cxx_tests)
