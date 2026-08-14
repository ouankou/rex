cmake_minimum_required(VERSION 3.16)
project(rex_ctest_name_set_fixture_contract NONE)

# CTest requires a memory-check command even when -T memcheck is combined with
# -N to inspect only the filtered test registry.  This fixture never executes
# the command: its MemCheck invocation is deliberately a dry-run that verifies
# exact name selection after CTEST_CUSTOM_MEMCHECK_IGNORE is applied.  Give the
# fixture an explicit, always-present command so its registry contract does not
# depend on whether the host happens to have Valgrind installed.
set(MEMORYCHECK_COMMAND "${CMAKE_COMMAND}")
set(MEMORYCHECK_TYPE Valgrind)
include(CTest)

configure_file(CTestCustom.cmake.in CTestCustom.cmake COPYONLY)

add_test(NAME rex_fixture_setup COMMAND ${CMAKE_COMMAND} -E true)
set_tests_properties(rex_fixture_setup
  PROPERTIES FIXTURES_SETUP rex_exact_selection_fixture)

add_test(NAME rex_fixture_consumer COMMAND ${CMAKE_COMMAND} -E true)
set_tests_properties(rex_fixture_consumer
  PROPERTIES
    DEPENDS rex_dependency_consumer
    FIXTURES_REQUIRED rex_exact_selection_fixture)

add_test(NAME rex_dependency_producer
  COMMAND ${CMAKE_COMMAND} -E touch
    ${CMAKE_CURRENT_BINARY_DIR}/dependency.ready)
add_test(NAME rex_dependency_consumer
  COMMAND ${CMAKE_COMMAND} -E compare_files
    ${CMAKE_CURRENT_BINARY_DIR}/dependency.ready
    ${CMAKE_CURRENT_BINARY_DIR}/dependency.ready)
set_tests_properties(rex_dependency_consumer
  PROPERTIES DEPENDS rex_dependency_producer)

add_test(NAME rex_regex_only COMMAND ${CMAKE_COMMAND} -E true)

add_test(NAME rex_memcheck_ignored COMMAND ${CMAKE_COMMAND} -E true)
add_test(NAME rex_memcheck_selected COMMAND ${CMAKE_COMMAND} -E true)
