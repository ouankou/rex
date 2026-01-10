message(STATUS "A++Tests require A++/P++ configuration; tests are registered but disabled by default.")

set(_axxppr_flags "-DRESTRICT=\"\"")
set(_axxppr_includes
  -I${CMAKE_CURRENT_SOURCE_DIR}
  -I${CMAKE_CURRENT_SOURCE_DIR}/../..
  -I${CMAKE_SOURCE_DIR}/include)

add_test(
  NAME run_a_pp_test1
  COMMAND $<TARGET_FILE:testTranslator>
    ${_axxppr_flags}
    ${_axxppr_includes}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test1.C
)
set_tests_properties(run_a_pp_test1 PROPERTIES DISABLED TRUE LABELS "run;a++")

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_RunTests_A++Tests_conditional-check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=run_a_pp_test1
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_RunTests_A++Tests_conditional-check-local PROPERTIES DEPENDS run_a_pp_test1 )
