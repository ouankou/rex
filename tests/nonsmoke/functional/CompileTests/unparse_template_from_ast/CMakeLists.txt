set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

set(_unparse_template_specimens
  kripke-tpl-000.cxx
  kripke-tpl-001.cxx
  kripke-tpl-002.cxx
  kripke-tpl-003.cxx
  kripke-tpl-004.cxx
  kripke-tpl-005.cxx
  kripke-tpl-006.cxx
  kripke-tpl-007.cxx
  docgen-000.cxx
  docgen-001.cxx
)

set(_unparse_template_flags
  -rose:unparse_template_ast
  -DSKIP_ROSE_BUILTIN_DECLARATIONS
)
list(JOIN _unparse_template_flags " " _unparse_template_flags)

foreach(specimen IN LISTS _unparse_template_specimens)
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
endforeach()
