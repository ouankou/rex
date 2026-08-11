cmake_minimum_required(VERSION 3.16)
project(rex_ctest_name_set_fixture_contract NONE)

enable_testing()

add_test(NAME rex_fixture_setup COMMAND ${CMAKE_COMMAND} -E true)
set_tests_properties(rex_fixture_setup
  PROPERTIES FIXTURES_SETUP rex_exact_selection_fixture)

add_test(NAME rex_fixture_consumer COMMAND ${CMAKE_COMMAND} -E true)
set_tests_properties(rex_fixture_consumer
  PROPERTIES FIXTURES_REQUIRED rex_exact_selection_fixture)
