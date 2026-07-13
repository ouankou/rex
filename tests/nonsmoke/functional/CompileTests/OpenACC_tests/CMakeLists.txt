add_executable(rex_openacc_typed_integration
  rex_openacc_typed_integration.cpp)
target_link_libraries(rex_openacc_typed_integration
  ROSE_DLL ${link_with_libraries})

add_test(
  NAME OpenACC_rex_typed_input_forms
  COMMAND $<TARGET_FILE:rex_openacc_typed_integration> --forms)
set_tests_properties(OpenACC_rex_typed_input_forms
  PROPERTIES LABELS "OpenACC;rex;typed")

set(_rex_openacc_flags
  -rose:openacc:ast_only
  -rose:verbose 0
  -rose:skipfinalCompileStep)
add_test(
  NAME OpenACC_rex_typed_supported_conversion_session_isolation
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:rex_openacc_typed_integration>
    ${_rex_openacc_flags}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_openacc_session_first.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_openacc_session_second.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_openacc_wait_forms.c)
set_tests_properties(OpenACC_rex_typed_supported_conversion_session_isolation
  PROPERTIES LABELS "OpenACC;rex;typed")

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; OpenACC hard-error tests require bash")
endif()
set(_rex_openacc_hard_error
  ${CMAKE_CURRENT_SOURCE_DIR}/run_rex_openacc_hard_error.sh)

add_test(
  NAME OpenACC_rex_exact_parse_failure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} ${_rex_openacc_hard_error}
    rex_openacc_parse_failure.log
    "REX_ACC_AST_INVARIANT[parse]: diagnostic-code=7 severity=1 at 1:22: unknown OpenACC clause 'unknown_clause'"
    --
    $<TARGET_FILE:testTranslator>
    ${_rex_openacc_flags}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_openacc_parse_failure.c)
set_tests_properties(OpenACC_rex_exact_parse_failure
  PROPERTIES LABELS "OpenACC;rex;typed;death")

add_test(
  NAME OpenACC_rex_unsupported_semantic_payload
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} ${_rex_openacc_hard_error}
    rex_openacc_unsupported_data_modifier.log
    "REX_ACC_AST_INVARIANT[unsupported-data-modifier]: SgAccCopyinClause cannot represent typed copyin modifiers"
    --
    $<TARGET_FILE:testTranslator>
    ${_rex_openacc_flags}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_openacc_unsupported_data_modifier.c)
set_tests_properties(OpenACC_rex_unsupported_semantic_payload
  PROPERTIES LABELS "OpenACC;rex;typed;death")

foreach(_rex_openacc_wait_mode IN ITEMS
    queues-without-list
    empty-wait-list)
  if(_rex_openacc_wait_mode STREQUAL "queues-without-list")
    set(_rex_openacc_wait_option --reject-queues-without-list)
    set(_rex_openacc_wait_diagnostic
      "REX_UNPARSE_INVARIANT[openacc-wait]: queues keyword requires a non-empty wait list")
  else()
    set(_rex_openacc_wait_option --reject-empty-wait-list)
    set(_rex_openacc_wait_diagnostic
      "REX_UNPARSE_INVARIANT[openacc-wait]: wait list must be non-empty and owned by its SgAccWaitStatement")
  endif()
  add_test(
    NAME OpenACC_rex_reject_${_rex_openacc_wait_mode}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} ${_rex_openacc_hard_error}
      rex_openacc_${_rex_openacc_wait_mode}.log
      "${_rex_openacc_wait_diagnostic}"
      --
      $<TARGET_FILE:rex_openacc_typed_integration>
      ${_rex_openacc_wait_option}
      ${_rex_openacc_flags}
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_openacc_session_first.c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_openacc_session_second.c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_openacc_wait_forms.c)
  set_tests_properties(OpenACC_rex_reject_${_rex_openacc_wait_mode}
    PROPERTIES LABELS "OpenACC;rex;typed;death")
endforeach()
unset(_rex_openacc_wait_diagnostic)
unset(_rex_openacc_wait_mode)
unset(_rex_openacc_wait_option)

if(ENABLE-FORTRAN)
  add_subdirectory(fortran)
endif()
