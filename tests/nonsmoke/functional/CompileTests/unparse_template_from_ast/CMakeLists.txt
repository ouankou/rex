set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

set(_unparse_template_passing
  kripke-tpl-000.cxx
  kripke-tpl-001.cxx
  kripke-tpl-002.cxx
  kripke-tpl-003.cxx
  kripke-tpl-004.cxx
  kripke-tpl-005.cxx
  kripke-tpl-006.cxx
)

set(_unparse_template_failing
  kripke-tpl-007.cxx
  doxygen-000.cxx
  doxygen-001.cxx
)

set(_unparse_template_tests
  ${_unparse_template_passing}
  ${_unparse_template_failing}
)

set(_unparse_template_flags
  -std=c++ -rose:unparse_template_ast -DSKIP_ROSE_BUILTIN_DECLARATIONS)

foreach(specimen IN LISTS _unparse_template_tests)
  set(_target ${specimen}.passed)
  add_test(
    NAME unparse_template_${specimen}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:testTranslator> ${_unparse_template_flags} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
      TITLE="TEST: testTranslator ${specimen}"
      ${_exit_status}
      ${_target}
  )
  if(specimen IN_LIST _unparse_template_failing)
    set_tests_properties(unparse_template_${specimen} PROPERTIES DISABLED TRUE)
  endif()
endforeach()
