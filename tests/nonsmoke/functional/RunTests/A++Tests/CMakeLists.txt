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
