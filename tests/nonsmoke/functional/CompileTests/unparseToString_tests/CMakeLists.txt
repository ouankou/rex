add_executable(unparseToString unparseToString.C )
target_link_libraries(unparseToString ROSE_DLL ${link_with_libraries} )

add_executable(unparseProject unparseProject.C)
target_link_libraries(unparseProject ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_format_help_ownership
  rex_unparser_format_help_ownership.C)
target_link_libraries(rex_unparser_format_help_ownership
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_format_help_position_contract
  rex_unparser_format_help_position_contract.C)
target_link_libraries(rex_unparser_format_help_position_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_info_session_state
  rex_unparser_info_session_state.C)
target_link_libraries(rex_unparser_info_session_state
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_invocation_state
  rex_unparser_invocation_state.C)
target_link_libraries(rex_unparser_invocation_state
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_string_literal_whitespace
  rex_unparser_string_literal_whitespace.C)
target_link_libraries(rex_unparser_string_literal_whitespace
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_header_snapshot
  rex_unparser_header_snapshot.C)
target_link_libraries(rex_unparser_header_snapshot
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_project_isolation
  rex_unparser_project_isolation.C)
target_link_libraries(rex_unparser_project_isolation
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_dirty_header_contract
  rex_unparser_dirty_header_contract.C)
target_link_libraries(rex_unparser_dirty_header_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_malformed_nonreal_template
  rex_unparser_malformed_nonreal_template.C)
target_link_libraries(rex_unparser_malformed_nonreal_template
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_alias_provenance
  rex_ast_alias_provenance.C)
target_link_libraries(rex_ast_alias_provenance
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_unresolved_variable_reference
  rex_unparser_unresolved_variable_reference.C)
target_link_libraries(rex_unparser_unresolved_variable_reference
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_unresolved_named_type
  rex_unparser_unresolved_named_type.C)
target_link_libraries(rex_unparser_unresolved_named_type
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_null_token_map
  rex_unparser_null_token_map.C)
target_link_libraries(rex_unparser_null_token_map
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_missing_type_context
  rex_unparser_missing_type_context.C)
target_link_libraries(rex_unparser_missing_type_context
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_compiler_generated_order
  rex_unparser_compiler_generated_order.C)
target_link_libraries(rex_unparser_compiler_generated_order
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_user_defined_binary_type_contract
  rex_ast_user_defined_binary_type_contract.C)
target_link_libraries(rex_ast_user_defined_binary_type_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_source_surface_ownership
  rex_ast_source_surface_ownership.C)
target_link_libraries(rex_ast_source_surface_ownership
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_using_terminal_name_contract
  rex_ast_using_terminal_name_contract.C)
target_link_libraries(rex_ast_using_terminal_name_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_fortran_custom_line_width
  rex_unparser_fortran_custom_line_width.C)
target_link_libraries(rex_unparser_fortran_custom_line_width
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_fortran_string_boundary
  rex_unparser_fortran_string_boundary.C)
target_link_libraries(rex_unparser_fortran_string_boundary
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_fortran_fixed_comment_wrap
  rex_unparser_fortran_fixed_comment_wrap.C)
target_link_libraries(rex_unparser_fortran_fixed_comment_wrap
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_fortran_source_form_contracts
  rex_unparser_fortran_source_form_contracts.C)
target_link_libraries(rex_unparser_fortran_source_form_contracts
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_backend_hard_contracts
  rex_unparser_backend_hard_contracts.C)
target_link_libraries(rex_unparser_backend_hard_contracts
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_type_definition_context_contract
  rex_unparser_type_definition_context_contract.C)
target_link_libraries(rex_unparser_type_definition_context_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_frontend_preprocessing_hard_contract
  rex_frontend_preprocessing_hard_contract.C)
target_link_libraries(rex_frontend_preprocessing_hard_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_switch_label_token_boundary_contract
  rex_unparser_switch_label_token_boundary_contract.C)
target_link_libraries(rex_unparser_switch_label_token_boundary_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_cxx_expression_role_contract
  rex_unparser_cxx_expression_role_contract.C)
target_link_libraries(rex_unparser_cxx_expression_role_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_source_provenance_contract
  rex_unparser_source_provenance_contract.C)
target_link_libraries(rex_unparser_source_provenance_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_owned_list_contract
  rex_unparser_owned_list_contract.C)
target_link_libraries(rex_unparser_owned_list_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_dispatch_ownership_contract
  rex_unparser_dispatch_ownership_contract.C)
target_link_libraries(rex_unparser_dispatch_ownership_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_invocation_context_contract
  rex_unparser_invocation_context_contract.C)
target_link_libraries(rex_unparser_invocation_context_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_included_files_contract
  rex_unparser_included_files_contract.C)
target_link_libraries(rex_unparser_included_files_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_unparser_template_and_tag_ownership_contract
  rex_unparser_template_and_tag_ownership_contract.C)
target_link_libraries(rex_unparser_template_and_tag_ownership_contract
  ROSE_DLL ${link_with_libraries})

if(ENABLE-FORTRAN)
  add_executable(rex_unparser_fortran_format_literal_spaces
    rex_unparser_fortran_format_literal_spaces.C)
  target_link_libraries(rex_unparser_fortran_format_literal_spaces
    ROSE_DLL ${link_with_libraries})
endif()

set(ROSE_FLAGS)

set(TESTCODE_DIR ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)

set(TESTCODE_INCLUDES -I${TESTCODE_DIR})

set(CXX_TESTCODES_TO_UNPARSE ${CXX_TESTCODES})
list(APPEND CXX_TESTCODES_TO_UNPARSE test2003_24.C test2004_82.C)
list(REMOVE_DUPLICATES CXX_TESTCODES_TO_UNPARSE)

foreach(file_to_test ${CXX_TESTCODES_TO_UNPARSE})
  rex_get_cxx_specimen_extra_flags(extra_frontend_flags ${file_to_test})

  add_test(
    NAME ut_${file_to_test}
    COMMAND unparseToString ${ROSE_FLAGS} ${TESTCODE_INCLUDES} ${extra_frontend_flags}
            -c ${TESTCODE_DIR}/${file_to_test})
  add_test(
    NAME ua_${file_to_test}
    COMMAND unparseToString --all ${ROSE_FLAGS} ${TESTCODE_INCLUDES} ${extra_frontend_flags}
            -c ${TESTCODE_DIR}/${file_to_test})

  rex_lock_test_for_input(ut_${file_to_test} ${file_to_test})
  rex_lock_test_for_input(ua_${file_to_test} ${file_to_test})

  set_tests_properties(ua_${file_to_test} PROPERTIES DEPENDS ut_${file_to_test} LABELS UNPARSETOSTRING)
endforeach()

add_test(
  NAME rex_issue481_typedef_const_no_double_const
  COMMAND sh -c "out=$(\"$@\") || exit $?; echo \"$out\" | grep -q \"const constint\" && exit 1; echo \"$out\" | grep -q \"\\bconstint\\b\""
    dummy
    $<TARGET_FILE:unparseToString> ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
      -c ${TESTCODE_DIR}/test2006_130.C
)
set_tests_properties(rex_issue481_typedef_const_no_double_const PROPERTIES LABELS rex)

add_test(
  NAME unparseProject_test2014_26
  COMMAND unparseProject ${CMAKE_CURRENT_SOURCE_DIR}/test2014_26.C
)

add_test(
  NAME unparseProject_test2017_01
  COMMAND unparseProject ${TESTCODE_DIR}/test2017_01.C
)

add_test(
  NAME rex_unparser_format_help_is_non_owning
  COMMAND rex_unparser_format_help_ownership
)
set_tests_properties(rex_unparser_format_help_is_non_owning
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_unparser_format_help_default_position_uses_default_formatter
  COMMAND rex_unparser_format_help_position_contract
)
set_tests_properties(
  rex_unparser_format_help_default_position_uses_default_formatter
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_unparser_format_help_explicit_position_is_applied
  COMMAND rex_unparser_format_help_position_contract --explicit-forward
)
set_tests_properties(rex_unparser_format_help_explicit_position_is_applied
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_unparser_format_help_explicit_position_from_empty_output_is_applied
  COMMAND rex_unparser_format_help_position_contract --explicit-from-empty
)
set_tests_properties(
  rex_unparser_format_help_explicit_position_from_empty_output_is_applied
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_unparser_format_help_invalid_line_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" --invalid-line >rex_unparser_format_help_invalid_line.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_UNPARSE_INVARIANT[formatter-position]: absolute position requires a positive one-based line and nonnegative zero-based column, requested line=0 column=0' rex_unparser_format_help_invalid_line.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_format_help_position_contract>
)
set_tests_properties(rex_unparser_format_help_invalid_line_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

add_test(
  NAME rex_unparser_format_help_invalid_column_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" --invalid-column >rex_unparser_format_help_invalid_column.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_UNPARSE_INVARIANT[formatter-position]: absolute position requires a positive one-based line and nonnegative zero-based column, requested line=1 column=-1' rex_unparser_format_help_invalid_column.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_format_help_position_contract>
)
set_tests_properties(rex_unparser_format_help_invalid_column_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

add_test(
  NAME rex_unparser_format_help_retrograde_position_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" --retrograde >rex_unparser_format_help_retrograde_position.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_UNPARSE_INVARIANT[formatter-position]: custom formatting requested a position before the current output position, requested line=1 column=0 current line=1 column=6' rex_unparser_format_help_retrograde_position.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_format_help_position_contract>
)
set_tests_properties(rex_unparser_format_help_retrograde_position_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

add_test(
  NAME rex_ast_user_defined_binary_null_result_type_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" >rex_ast_user_defined_binary_type_contract.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_AST_INVARIANT[binary-result-type]: expression=SgUserDefinedBinaryOp has no exact semantic result type' rex_ast_user_defined_binary_type_contract.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_ast_user_defined_binary_type_contract>
)
set_tests_properties(
  rex_ast_user_defined_binary_null_result_type_is_rejected
  PROPERTIES LABELS "rex;AST;UNPARSER;death")

add_test(
  NAME rex_ast_hidden_direct_source_surface_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$@\" >rex_ast_source_surface_ownership.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[source-surface-ownership\\]: declaration=0x[[:xdigit:]]+ type=Sg[^[:space:]]+ file=.* line=[0-9]+ col=[0-9]+ direct scope owner=0x[[:xdigit:]]+ is hidden from code generation$' rex_ast_source_surface_ownership.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_ast_source_surface_ownership>
    -rose:skipfinalCompileStep
    -c
    ${TESTCODE_DIR}/rex_unparser_exact_source_surface_ownership.cpp)
set_tests_properties(
  rex_ast_hidden_direct_source_surface_is_rejected
  PROPERTIES LABELS "rex;AST;UNPARSER;death")

add_test(
  NAME rex_ast_using_terminal_names_are_preserved
  COMMAND
    $<TARGET_FILE:rex_ast_using_terminal_name_contract>
    -rose:skipfinalCompileStep
    -c
    ${TESTCODE_DIR}/rex_using_terminal_name_contract.cpp)
set_tests_properties(
  rex_ast_using_terminal_names_are_preserved
  PROPERTIES LABELS "rex;AST;UNPARSER")

add_test(
  NAME rex_ast_source_using_missing_terminal_name_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; REX_TEST_CLEAR_USING_TERMINAL=source \"$@\" >rex_ast_source_using_terminal_name.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[using-terminal-name\\]: declaration=0x[[:xdigit:]]+ has no exact terminal token$' rex_ast_source_using_terminal_name.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_ast_using_terminal_name_contract>
    -rose:skipfinalCompileStep
    -c
    ${TESTCODE_DIR}/rex_using_terminal_name_contract.cpp)
set_tests_properties(
  rex_ast_source_using_missing_terminal_name_is_rejected
  PROPERTIES LABELS "rex;AST;UNPARSER;death")

add_test(
  NAME rex_ast_inheriting_using_missing_terminal_name_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; REX_TEST_CLEAR_USING_TERMINAL=inheriting \"$@\" >rex_ast_inheriting_using_terminal_name.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[using-terminal-name\\]: declaration=0x[[:xdigit:]]+ has no exact terminal token$' rex_ast_inheriting_using_terminal_name.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_ast_using_terminal_name_contract>
    -rose:skipfinalCompileStep
    -c
    ${TESTCODE_DIR}/rex_using_terminal_name_contract.cpp)
set_tests_properties(
  rex_ast_inheriting_using_missing_terminal_name_is_rejected
  PROPERTIES LABELS "rex;AST;UNPARSER;death")

add_test(
  NAME rex_ast_compiler_generated_using_missing_terminal_name_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; REX_TEST_CLEAR_USING_TERMINAL=compiler-generated \"$@\" >rex_ast_compiler_generated_using_terminal_name.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[using-terminal-name\\]: declaration=0x[[:xdigit:]]+ has no exact terminal token$' rex_ast_compiler_generated_using_terminal_name.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_ast_using_terminal_name_contract>
    -rose:skipfinalCompileStep
    -c
    ${TESTCODE_DIR}/rex_using_terminal_name_contract.cpp)
set_tests_properties(
  rex_ast_compiler_generated_using_missing_terminal_name_is_rejected
  PROPERTIES LABELS "rex;AST;UNPARSER;death")

add_test(
  NAME rex_unparser_fortran_string_boundary
  COMMAND $<TARGET_FILE:rex_unparser_fortran_string_boundary>)
set_tests_properties(rex_unparser_fortran_string_boundary
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_unparser_fortran_fixed_comment_wrap
  COMMAND $<TARGET_FILE:rex_unparser_fortran_fixed_comment_wrap>)
set_tests_properties(rex_unparser_fortran_fixed_comment_wrap
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_unparser_fortran_comment_preserves_linewrap_state
  COMMAND $<TARGET_FILE:rex_unparser_fortran_fixed_comment_wrap>
    linewrap-state)
set_tests_properties(rex_unparser_fortran_comment_preserves_linewrap_state
  PROPERTIES LABELS "rex;Fortran;UNPARSER")

set(_fortran_fixed_comment_modes
  invalid-marker
  non-column-one)
set(_fortran_fixed_comment_diagnostics
  "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: preprocessing record is not a Fortran comment"
  "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: fixed-format comment does not begin in column one")
foreach(_fortran_fixed_comment_index RANGE 0 1)
  list(GET _fortran_fixed_comment_modes ${_fortran_fixed_comment_index} _mode)
  list(GET _fortran_fixed_comment_diagnostics ${_fortran_fixed_comment_index}
    _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_unparser_fortran_fixed_comment_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_unparser_fortran_fixed_comment_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$4\" rex_unparser_fortran_fixed_comment_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_fortran_fixed_comment_wrap>
      ${_mode}
      ${_suffix}
      ${_diagnostic})
  set_tests_properties(
    rex_unparser_fortran_fixed_comment_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;Fortran;UNPARSER;death")
endforeach()
unset(_fortran_fixed_comment_modes)
unset(_fortran_fixed_comment_diagnostics)
unset(_fortran_fixed_comment_index)

add_test(
  NAME rex_unparser_fortran_source_form_contracts
  COMMAND $<TARGET_FILE:rex_unparser_fortran_source_form_contracts>)
set_tests_properties(rex_unparser_fortran_source_form_contracts
  PROPERTIES LABELS "rex;AST;UNPARSER")

set(_fortran_source_form_contract_modes
  directive-context-missing-file
  directive-context-nonfortran-file
  module-kind-invalid
  module-parent-on-module
  submodule-missing-parent
  submodule-parent-invalid
  declaration-origin-invalid
  declaration-origin-pending
  declaration-origin-semantic-source
  object-kind-selector-mismatch
  auxiliary-lexical-owner-conflict
  pragma-family-invalid
  openmp-clause-context-missing-file
  openmp-clause-context-missing-scope
  openmp-clause-context-nonfortran-language
  openmp-end-kind-invalid
  openmp-explicit-end-unsupported
  openmp-spelling-missing
  openmp-spelling-invalid
  openacc-end-kind-invalid
  openacc-end-provenance-missing
  openacc-implicit-end-unsupported
  openacc-atomic-capture-end-missing)
set(_fortran_source_form_contract_diagnostics
  "REX_UNPARSE_INVARIANT[fortran-directive-context]: directive context requires an exact Fortran source file"
  "REX_UNPARSE_INVARIANT[fortran-directive-context]: directive context requires an exact Fortran source file"
  "REX_UNPARSE_INVARIANT[fortran-module-kind]: invalid value=999"
  "REX_UNPARSE_INVARIANT[fortran-module-kind]: MODULE has unexpected submodule parent='ancestor'"
  "REX_UNPARSE_INVARIANT[fortran-submodule-parent]: invalid parent=''"
  "REX_UNPARSE_INVARIANT[fortran-submodule-parent]: invalid parent='ancestor:parent:extra'"
  "REX_UNPARSE_INVARIANT[fortran-declaration-origin]: invalid value=999"
  "REX_UNPARSE_INVARIANT[fortran-declaration-origin]: pending source declaration reached the unparser"
  "REX_UNPARSE_INVARIANT[fortran-declaration-origin]: semantic-only declaration reached a source-emission list"
  "REX_UNPARSE_INVARIANT[fortran-source-type]: source declaration has a missing or contradictory semantic/source type pair"
  "[REX-AST-DUPLICATE-AUXILIARY-DECLARATION] declaration is already structurally owned by its scope"
  "REX_UNPARSE_INVARIANT[fortran-pragma-family]: pragma has invalid family=999"
  "REX_UNPARSE_INVARIANT[fortran-openmp-clause-context]: directive has no exact active Fortran source file"
  "REX_UNPARSE_INVARIANT[fortran-openmp-clause-context]: directive has no exact source scope"
  "REX_UNPARSE_INVARIANT[fortran-openmp-clause-context]: directive has a non-Fortran language context"
  "REX_UNPARSE_INVARIANT[fortran-directive-end-kind]: statement=SgOmpParallelStatement has invalid value=999"
  "Error: unsupported explicit END form for SgOmpFlushStatement"
  "REX_UNPARSE_INVARIANT[openmp-source-spelling]: statement=SgOmpTargetParallelForStatement has no exact Fortran DO-family spelling"
  "REX_UNPARSE_INVARIANT[openmp-source-spelling]: statement=SgOmpTargetParallelForStatement has invalid spelling=999"
  "REX_UNPARSE_INVARIANT[fortran-directive-end-kind]: statement=SgAccParallelStatement has invalid value=999"
  "REX_UNPARSE_INVARIANT[fortran-directive-end-kind]: statement=SgAccParallelStatement has no exact source END provenance"
  "REX_UNPARSE_INVARIANT[fortran-directive-end-kind]: statement=SgAccParallelStatement cannot omit its source END directive"
  "REX_UNPARSE_INVARIANT[fortran-directive-end-kind]: statement=SgAccAtomicStatement has no exact source END provenance")
list(LENGTH _fortran_source_form_contract_modes
  _fortran_source_form_contract_mode_count)
list(LENGTH _fortran_source_form_contract_diagnostics
  _fortran_source_form_contract_diagnostic_count)
if(NOT _fortran_source_form_contract_mode_count EQUAL
   _fortran_source_form_contract_diagnostic_count)
  message(FATAL_ERROR
    "Fortran source-form hard-contract test tables have different lengths")
endif()
math(EXPR _fortran_source_form_contract_last
  "${_fortran_source_form_contract_mode_count} - 1")
foreach(_fortran_source_form_contract_index RANGE
    ${_fortran_source_form_contract_last})
  list(GET _fortran_source_form_contract_modes
    ${_fortran_source_form_contract_index} _mode)
  list(GET _fortran_source_form_contract_diagnostics
    ${_fortran_source_form_contract_index} _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_unparser_fortran_source_form_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_unparser_fortran_source_form_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$4\" rex_unparser_fortran_source_form_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_fortran_source_form_contracts>
      ${_mode}
      ${_suffix}
      ${_diagnostic})
  set_tests_properties(
    rex_unparser_fortran_source_form_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;AST;UNPARSER;death")
endforeach()
unset(_fortran_source_form_contract_modes)
unset(_fortran_source_form_contract_diagnostics)
unset(_fortran_source_form_contract_mode_count)
unset(_fortran_source_form_contract_diagnostic_count)
unset(_fortran_source_form_contract_last)
unset(_fortran_source_form_contract_index)
unset(_mode)
unset(_diagnostic)
unset(_suffix)

add_test(
  NAME rex_frontend_preprocessing_spelling_contract
  COMMAND $<TARGET_FILE:rex_frontend_preprocessing_hard_contract>)
set_tests_properties(rex_frontend_preprocessing_spelling_contract
  PROPERTIES LABELS "rex;CFE")

set(_frontend_preprocessing_modes
  missing-define-keyword
  missing-macro-name
  wrong-directive-kind
  missing-separator-after-splice
  unknown-hash-directive
  empty-hash-directive
  invalid-hash-identifier
  null-list-entry
  invalid-fortran-fixed-byte)
set(_frontend_preprocessing_diagnostics
  "REX_FRONTEND_INVARIANT[macro-definition-spelling]: reason=missing-define-keyword spelling=#undef REX_BROKEN"
  "REX_FRONTEND_INVARIANT[macro-definition-spelling]: reason=missing-macro-name spelling=#define /* nothing */"
  "REX_FRONTEND_INVARIANT[macro-definition-kind]: directive=CplusplusStyleComment cannot provide a #define macro name"
  [=[REX_FRONTEND_INVARIANT[macro-definition-spelling]: reason=missing-name-separator spelling=#define\\nREX_BROKEN 1]=]
  "REX_FRONTEND_INVARIANT[preprocessing-directive-kind]: unsupported hash directive=rex_unknown"
  "REX_FRONTEND_INVARIANT[preprocessing-directive-kind]: hash directive has no identifier"
  "REX_FRONTEND_INVARIANT[preprocessing-directive-kind]: hash directive starts with invalid identifier character"
  "REX_AST_INVARIANT[preprocessing-list-entry]: display label=hard-contract contains null entry index=0"
  "REX_FRONTEND_INVARIANT[fortran-fixed-source-byte]: first-byte=255 is not printable ASCII, space, or tab")
list(LENGTH _frontend_preprocessing_modes
  _frontend_preprocessing_mode_count)
list(LENGTH _frontend_preprocessing_diagnostics
  _frontend_preprocessing_diagnostic_count)
if(NOT _frontend_preprocessing_mode_count EQUAL
   _frontend_preprocessing_diagnostic_count)
  message(FATAL_ERROR
    "Frontend preprocessing hard-contract tables have different lengths")
endif()
math(EXPR _frontend_preprocessing_last
  "${_frontend_preprocessing_mode_count} - 1")
foreach(_frontend_preprocessing_index
        RANGE ${_frontend_preprocessing_last})
  list(GET _frontend_preprocessing_modes
    ${_frontend_preprocessing_index} _mode)
  list(GET _frontend_preprocessing_diagnostics
    ${_frontend_preprocessing_index} _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_frontend_preprocessing_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_frontend_preprocessing_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$4\" rex_frontend_preprocessing_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_frontend_preprocessing_hard_contract>
      ${_mode}
      ${_suffix}
      ${_diagnostic})
  set_tests_properties(
    rex_frontend_preprocessing_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;CFE;death")
endforeach()
unset(_frontend_preprocessing_modes)
unset(_frontend_preprocessing_diagnostics)
unset(_frontend_preprocessing_mode_count)
unset(_frontend_preprocessing_diagnostic_count)
unset(_frontend_preprocessing_last)
unset(_frontend_preprocessing_index)
unset(_mode)
unset(_diagnostic)
unset(_suffix)

set(_frontend_preprocessing_fixture_dir
  "${CMAKE_CURRENT_SOURCE_DIR}/../Cxx_tests")
add_test(
  NAME rex_frontend_preprocessing_spliced_directive_scanner_contract
  COMMAND $<TARGET_FILE:rex_frontend_preprocessing_hard_contract>
    scan-valid-source
    ${_frontend_preprocessing_fixture_dir}/rex_frontend_macro_splice_contract.cpp)
set_tests_properties(
  rex_frontend_preprocessing_spliced_directive_scanner_contract
  PROPERTIES LABELS "rex;CFE")

add_test(
  NAME rex_frontend_preprocessing_missing_splice_separator_scanner_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" scan-invalid-source \"$2\" >rex_frontend_preprocessing_invalid_splice_source.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$3\" rex_frontend_preprocessing_invalid_splice_source.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_preprocessing_hard_contract>
    ${_frontend_preprocessing_fixture_dir}/rex_frontend_macro_missing_separator.cpp
    [=[REX_FRONTEND_INVARIANT[macro-definition-spelling]: reason=missing-name-separator spelling=#define\\nREX_INVALID_SPLICE 1\n]=])
set_tests_properties(
  rex_frontend_preprocessing_missing_splice_separator_scanner_is_rejected
  PROPERTIES LABELS "rex;CFE;death")
unset(_frontend_preprocessing_fixture_dir)

set(_frontend_fortran_preprocessing_fixture_dir
  "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(
  NAME rex_frontend_fortran_preprocessing_collector_contract
  COMMAND $<TARGET_FILE:rex_frontend_preprocessing_hard_contract>
    scan-fortran-source
    ${_frontend_fortran_preprocessing_fixture_dir}/rex_fortran_preprocessing_collector_contract.F90)
set_tests_properties(
  rex_frontend_fortran_preprocessing_collector_contract
  PROPERTIES LABELS "rex;CFE;Fortran")

set(_frontend_fortran_preprocessing_invalid_fixtures
  rex_fortran_preprocessing_unterminated_continuation.F90
  rex_fortran_preprocessing_missing_linemarker_filename.F90
  rex_fortran_preprocessing_linemarker_overflow.F90)
set(_frontend_fortran_preprocessing_invalid_diagnostics
  "REX_FRONTEND_INVARIANT[preprocessing-continuation]: file=${_frontend_fortran_preprocessing_fixture_dir}/rex_fortran_preprocessing_unterminated_continuation.F90 line=1 ends in a continuation at end of file"
  "REX_FRONTEND_INVARIANT[preprocessing-linemarker]: file=${_frontend_fortran_preprocessing_fixture_dir}/rex_fortran_preprocessing_missing_linemarker_filename.F90 line=1 has no quoted filename"
  "REX_FRONTEND_INVARIANT[preprocessing-linemarker]: file=${_frontend_fortran_preprocessing_fixture_dir}/rex_fortran_preprocessing_linemarker_overflow.F90 line=1 line number is out of range")
list(LENGTH _frontend_fortran_preprocessing_invalid_fixtures
  _frontend_fortran_preprocessing_invalid_fixture_count)
list(LENGTH _frontend_fortran_preprocessing_invalid_diagnostics
  _frontend_fortran_preprocessing_invalid_diagnostic_count)
if(NOT _frontend_fortran_preprocessing_invalid_fixture_count EQUAL
   _frontend_fortran_preprocessing_invalid_diagnostic_count)
  message(FATAL_ERROR
    "Fortran preprocessing hard-contract tables have different lengths")
endif()
math(EXPR _frontend_fortran_preprocessing_invalid_last
  "${_frontend_fortran_preprocessing_invalid_fixture_count} - 1")
foreach(_frontend_fortran_preprocessing_invalid_index
        RANGE ${_frontend_fortran_preprocessing_invalid_last})
  list(GET _frontend_fortran_preprocessing_invalid_fixtures
    ${_frontend_fortran_preprocessing_invalid_index} _fixture)
  list(GET _frontend_fortran_preprocessing_invalid_diagnostics
    ${_frontend_fortran_preprocessing_invalid_index} _diagnostic)
  string(REGEX REPLACE "\\.[^.]+$" "" _suffix "${_fixture}")
  add_test(
    NAME rex_frontend_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" scan-invalid-fortran-source \"$2\" >rex_frontend_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$4\" rex_frontend_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_frontend_preprocessing_hard_contract>
      ${_frontend_fortran_preprocessing_fixture_dir}/${_fixture}
      ${_suffix}
      ${_diagnostic})
  set_tests_properties(
    rex_frontend_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;CFE;Fortran;death")
endforeach()
unset(_frontend_fortran_preprocessing_fixture_dir)
unset(_frontend_fortran_preprocessing_invalid_fixtures)
unset(_frontend_fortran_preprocessing_invalid_diagnostics)
unset(_frontend_fortran_preprocessing_invalid_fixture_count)
unset(_frontend_fortran_preprocessing_invalid_diagnostic_count)
unset(_frontend_fortran_preprocessing_invalid_last)
unset(_frontend_fortran_preprocessing_invalid_index)
unset(_fixture)
unset(_diagnostic)
unset(_suffix)

set(_frontend_preprocessing_missing_source
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_preprocessing_missing_source.c")
add_test(
  NAME rex_frontend_preprocessing_missing_source_file_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" missing-source-file \"$2\" >rex_frontend_preprocessing_missing_source_file.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_FRONTEND_INVARIANT\\[preprocessing-source-open\\]: file=.*rex_frontend_preprocessing_missing_source\\.c errno=[1-9][0-9]* message=.+$' rex_frontend_preprocessing_missing_source_file.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_preprocessing_hard_contract>
    ${_frontend_preprocessing_missing_source})
set_tests_properties(
  rex_frontend_preprocessing_missing_source_file_is_rejected
  PROPERTIES LABELS "rex;CFE;death")
unset(_frontend_preprocessing_missing_source)

add_test(
  NAME rex_unparser_backend_hard_contracts
  COMMAND $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(rex_unparser_backend_hard_contracts
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING")

add_test(
  NAME rex_ast_append_statement_nondeclaration_to_declaration_scope_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" append-statement-nondeclaration-to-declaration-scope >rex_ast_append_statement_nondeclaration_to_declaration_scope.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[append-statement-kind\\]: statement=0x[[:xdigit:]]+ type=SgNullStatement cannot be published in declaration-only scope=0x[[:xdigit:]]+ type=SgGlobal$' rex_ast_append_statement_nondeclaration_to_declaration_scope.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(
  rex_ast_append_statement_nondeclaration_to_declaration_scope_is_rejected
  PROPERTIES LABELS "rex;AST;death")

add_test(
  NAME rex_ast_append_variable_declaration_to_conflicting_scope_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" append-variable-declaration-to-conflicting-scope >rex_ast_append_variable_declaration_to_conflicting_scope.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[variable-declaration-scope\\]: initialized name .*rex_scope_conflict.* already identifies semantic scope=0x[[:xdigit:]]+/SgGlobal but publication requested scope=0x[[:xdigit:]]+/SgNamespaceDefinitionStatement, declaration parent=SgNamespaceDefinitionStatement$' rex_ast_append_variable_declaration_to_conflicting_scope.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(
  rex_ast_append_variable_declaration_to_conflicting_scope_is_rejected
  PROPERTIES LABELS "rex;AST;death")

foreach(_definition_context_kind IN ITEMS class enum)
  add_test(
    NAME rex_unparser_unrelated_${_definition_context_kind}_definition_context_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; rm -f rex_unparser_$2_definition_context.out rex_unparser_$2_definition_context.err; set +e; \"$1\" unrelated-$2-context >rex_unparser_$2_definition_context.out 2>rex_unparser_$2_definition_context.err; status=$?; set -e; test $status -eq 134; printf 'REX_TYPE_DEFINITION_OUTPUT_SENTINEL\\n' | cmp -s - rex_unparser_$2_definition_context.out; test \"\$(grep -Fxc -- \"REX_UNPARSE_INVARIANT[type-definition-context]: $2 definition context has no exact canonical declaration/type identity and structural ownership\" rex_unparser_$2_definition_context.err)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_type_definition_context_contract>
      ${_definition_context_kind})
  set_tests_properties(
    rex_unparser_unrelated_${_definition_context_kind}_definition_context_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;death")
endforeach()
unset(_definition_context_kind)

add_test(
  NAME rex_unparser_switch_label_token_boundary_contract
  COMMAND $<TARGET_FILE:rex_unparser_switch_label_token_boundary_contract>)
set_tests_properties(rex_unparser_switch_label_token_boundary_contract
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING")

set(_switch_label_token_boundary_modes
  case-missing-body-map
  default-missing-body-map
  case-invalid-boundary
  token-missing-end-range
  aliased-token-owner)
set(_switch_label_token_boundary_diagnostics
  "REX_UNPARSE_INVARIANT[switch-label-token-boundary]: case statement and its exactly owned body require direct token mappings for partial replay (label=mapped body=missing)"
  "REX_UNPARSE_INVARIANT[switch-label-token-boundary]: default statement and its exactly owned body require direct token mappings for partial replay (label=mapped body=missing)"
  "REX_UNPARSE_INVARIANT[switch-label-token-boundary]: case statement has invalid exact boundary label-core=@OPEN@0,5) body-core=@OPEN@0,1)"
  "REX_UNPARSE_INVARIANT[token-source-range]: token has no complete exact physical source range"
  "REX_UNPARSE_INVARIANT[token-surface-owner]: file=switch-label-token-boundary.cpp statement-type=SgCaseOptionStmt is an alias of token-owner-type=SgDefaultOptionStmt")
list(LENGTH _switch_label_token_boundary_modes
  _switch_label_token_boundary_mode_count)
list(LENGTH _switch_label_token_boundary_diagnostics
  _switch_label_token_boundary_diagnostic_count)
if(NOT _switch_label_token_boundary_mode_count EQUAL
   _switch_label_token_boundary_diagnostic_count)
  message(FATAL_ERROR
    "Switch-label token-boundary contract tables have different lengths: "
    "modes=${_switch_label_token_boundary_mode_count} "
    "diagnostics=${_switch_label_token_boundary_diagnostic_count}")
endif()
math(EXPR _switch_label_token_boundary_last
  "${_switch_label_token_boundary_mode_count} - 1")
foreach(_switch_label_token_boundary_index
    RANGE ${_switch_label_token_boundary_last})
  list(GET _switch_label_token_boundary_modes
    ${_switch_label_token_boundary_index} _mode)
  list(GET _switch_label_token_boundary_diagnostics
    ${_switch_label_token_boundary_index} _diagnostic)
  string(REPLACE "@OPEN@" "[" _diagnostic "${_diagnostic}")
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_unparser_switch_label_token_boundary_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_unparser_switch_label_token_boundary_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$4\" rex_unparser_switch_label_token_boundary_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_switch_label_token_boundary_contract>
      "${_mode}"
      "${_suffix}"
      "${_diagnostic}")
  set_tests_properties(
    rex_unparser_switch_label_token_boundary_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;death")
endforeach()
unset(_switch_label_token_boundary_modes)
unset(_switch_label_token_boundary_diagnostics)
unset(_switch_label_token_boundary_mode_count)
unset(_switch_label_token_boundary_diagnostic_count)
unset(_switch_label_token_boundary_last)
unset(_switch_label_token_boundary_index)
unset(_mode)
unset(_diagnostic)
unset(_suffix)

add_test(
  NAME rex_unparser_cxx_expression_role_contract
  COMMAND $<TARGET_FILE:rex_unparser_cxx_expression_role_contract>)
set_tests_properties(rex_unparser_cxx_expression_role_contract
  PROPERTIES LABELS "rex;AST;UNPARSER;UNPARSETOSTRING")

set(_cxx_expression_role_modes
  offsetof-roles
  offsetof-owner
  implicit-conversion-structure
  implicit-conversion-provenance
  implicit-cast-provenance
  implicit-cast-kind
  implicit-cast-builder-kind
  cast-value-category
  cast-base-path
  cast-surface-semantic-mismatch
  builtin-bit-cast-semantic-mismatch
  functional-cast-semantic-mismatch
  invalid-call-source-syntax)
set(_cxx_expression_role_diagnostics
  "REX_UNPARSE_INVARIANT[typed-builtin-expression]: kind=1 name=__builtin_offsetof does not have the exact offsetof name, type occurrence, and designator roles"
  "REX_UNPARSE_INVARIANT[typed-builtin-expression]: kind=1 name=__builtin_offsetof has a missing or non-exclusively-owned member designator"
  "REX_UNPARSE_INVARIANT[implicit-conversion-call]: semantic conversion callee is not an exact dot or arrow member access"
  "REX_UNPARSE_INVARIANT[implicit-conversion-call]: exact argument-list wrapper node lacks synthesized semantic provenance"
  "REX_UNPARSE_INVARIANT[cast-expression-role]: implicit cast lacks exact synthesized implicit-conversion provenance"
  "REX_AST_INVARIANT[checked-cast]"
  "REX_SAGEBUILDER_INVARIANT[cast-structure]"
  "REX_AST_INVARIANT[checked-cast]"
  "REX_AST_INVARIANT[cast-base-path]"
  "REX_AST_INVARIANT[checked-cast]: dynamic_cast surface"
  "REX_AST_INVARIANT[checked-cast]: __builtin_bit_cast surface"
  "REX_AST_INVARIANT[checked-cast]: functional cast surface"
  "REX_UNPARSE_INVARIANT[function-call-source-syntax]: invalid value=999")
list(LENGTH _cxx_expression_role_modes _cxx_expression_role_mode_count)
list(LENGTH _cxx_expression_role_diagnostics
  _cxx_expression_role_diagnostic_count)
if(NOT _cxx_expression_role_mode_count EQUAL
   _cxx_expression_role_diagnostic_count)
  message(FATAL_ERROR
    "C++ expression-role hard-contract test tables have different lengths")
endif()
math(EXPR _cxx_expression_role_last
  "${_cxx_expression_role_mode_count} - 1")
foreach(_cxx_expression_role_index RANGE ${_cxx_expression_role_last})
  list(GET _cxx_expression_role_modes ${_cxx_expression_role_index} _mode)
  list(GET _cxx_expression_role_diagnostics ${_cxx_expression_role_index}
    _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_unparser_cxx_expression_role_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_unparser_cxx_expression_role_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc -- \"$4\" rex_unparser_cxx_expression_role_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_cxx_expression_role_contract>
      ${_mode}
      ${_suffix}
      ${_diagnostic})
  set_tests_properties(
    rex_unparser_cxx_expression_role_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;AST;UNPARSER;UNPARSETOSTRING;death")
endforeach()
unset(_cxx_expression_role_modes)
unset(_cxx_expression_role_diagnostics)
unset(_cxx_expression_role_mode_count)
unset(_cxx_expression_role_diagnostic_count)
unset(_cxx_expression_role_last)
unset(_cxx_expression_role_index)
unset(_mode)
unset(_diagnostic)
unset(_suffix)

add_test(
  NAME rex_unparser_source_provenance_contract
  COMMAND $<TARGET_FILE:rex_unparser_source_provenance_contract>)
set_tests_properties(rex_unparser_source_provenance_contract
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING")

set(_source_provenance_modes
  original-missing-owner
  original-chain
  original-semantic-lowering
  original-cast-role
  original-source-range)
set(_source_provenance_diagnostics
  "REX_NAME_QUALIFICATION_INVARIANT[original-expression-ownership]: expression=SgIntVal original=SgOmpSourceExpression is not one exactly owned source-provenance edge"
  "REX_UNPARSE_INVARIANT[original-expression-provenance]: consumer=unparse-expression owner=SgIntVal source=SgIntVal cannot chain source-expression provenance"
  "REX_UNPARSE_INVARIANT[original-expression-provenance]: consumer=unparse-expression owner=SgAddOp source=SgFunctionCallExp is a semantic lowering, not source-expression provenance"
  "REX_UNPARSE_INVARIANT[original-expression-provenance]: consumer=unparse-expression owner=SgCastExp source=SgIntVal requires a cast expression in the cast-source role"
  "REX_UNPARSE_INVARIANT[original-expression-provenance]: consumer=unparse-expression owner=SgIntVal source=SgIntVal does not cover its owner's exact source range")
list(LENGTH _source_provenance_modes _source_provenance_mode_count)
list(LENGTH _source_provenance_diagnostics
  _source_provenance_diagnostic_count)
if(NOT _source_provenance_mode_count EQUAL
   _source_provenance_diagnostic_count)
  message(FATAL_ERROR
    "Source-provenance hard-contract test tables have different lengths")
endif()
math(EXPR _source_provenance_last "${_source_provenance_mode_count} - 1")
foreach(_source_provenance_index RANGE ${_source_provenance_last})
  list(GET _source_provenance_modes ${_source_provenance_index} _mode)
  list(GET _source_provenance_diagnostics ${_source_provenance_index}
    _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_unparser_source_provenance_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_unparser_source_provenance_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$4\" rex_unparser_source_provenance_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_source_provenance_contract>
      ${_mode}
      ${_suffix}
      ${_diagnostic})
  set_tests_properties(
    rex_unparser_source_provenance_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;death")
endforeach()
unset(_source_provenance_modes)
unset(_source_provenance_diagnostics)
unset(_source_provenance_mode_count)
unset(_source_provenance_diagnostic_count)
unset(_source_provenance_last)
unset(_source_provenance_index)
unset(_mode)
unset(_diagnostic)
unset(_suffix)

add_test(
  NAME rex_unparser_owned_list_contract
  COMMAND $<TARGET_FILE:rex_unparser_owned_list_contract>)
set_tests_properties(rex_unparser_owned_list_contract
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING")

set(_owned_list_contract_modes
  basic-block-null-statement
  basic-block-foreign-statement
  basic-block-duplicate-statement
  global-null-declaration
  global-foreign-declaration
  global-duplicate-declaration
  empty-declaration-invalid-role
  empty-declaration-source-non-output
  empty-declaration-anchor-no-record
  empty-declaration-anchor-non-output
  empty-declaration-anchor-record-not-generated
  macro-null-list
  macro-null-entry
  attached-preprocessing-null-entry
  attached-preprocessing-shared-record
  attached-preprocessing-position-mutation
  attached-preprocessing-misowned-record
  attached-preprocessing-duplicate-receipt
  move-comments-duplicate-selection
  cxx-attached-preprocessing-null-entry
  lexical-non-output-statement
  non-output-preprocessing
  orphan-class-definition)
set(_owned_list_contract_diagnostics
  "REX_UNPARSE_INVARIANT[owned-statement-list]: owner=SgBasicBlock has null entry at index=0"
  "REX_UNPARSE_INVARIANT[owned-statement-list]: owner=SgBasicBlock entry index=0 type=SgNullStatement has parent=SgGlobal and occurrence-count=1"
  "REX_UNPARSE_INVARIANT[owned-statement-list]: owner=SgBasicBlock entry index=0 type=SgNullStatement has parent=SgBasicBlock and occurrence-count=2"
  "REX_UNPARSE_INVARIANT[owned-declaration-list]: owner=SgGlobal has null entry at index=0"
  "REX_UNPARSE_INVARIANT[owned-declaration-list]: owner=SgGlobal entry index=0 type=SgEmptyDeclaration has parent=SgBasicBlock and occurrence-count=1"
  "REX_UNPARSE_INVARIANT[owned-declaration-list]: owner=SgGlobal entry index=0 type=SgEmptyDeclaration has parent=SgGlobal and occurrence-count=2"
  "REX_AST_INVARIANT[empty-declaration-role]: invalid lexical role=999"
  "REX_UNPARSE_INVARIANT[statement-output-owner]: lexical statement=SgEmptyDeclaration has neither AST emission nor its own exact token surface"
  "REX_UNPARSE_INVARIANT[preprocessing-anchor]: anchor has no exactly owned preprocessing record"
  "REX_UNPARSE_INVARIANT[empty-declaration-role]: zero-width role=1 uses the legacy non-output suppression bit"
  "REX_UNPARSE_INVARIANT[preprocessing-anchor]: record-index=0 is not published as generated preprocessing"
  "REX_UNPARSE_INVARIANT[cxx-macro-directive-list]: file=rex_unparser_owned_list_contract.cpp has null owned directive list"
  "REX_UNPARSE_INVARIANT[cxx-macro-directive-list]: file=rex_unparser_owned_list_contract.cpp has null directive at index=0"
  "REX_UNPARSE_INVARIANT[attached-preprocessing-info-list]: null entry at index=0"
  "REX_AST_INVARIANT[preprocessing-attachment-owner]: record is not uniquely detached"
  "REX_AST_INVARIANT[preprocessing-attached-position]: attached record position cannot change outside its owner transaction"
  "REX_AST_INVARIANT[preprocessing-attachment-list]: owner list contains a record without exact owner identity at index=0"
  "REX_UNPARSE_INVARIANT[preprocessing-receipt-duplicate]: one record was claimed twice in this unparser invocation"
  "REX_AST_INVARIANT[move-comments-selection]: index=0 record-count=1 is not one exact selection"
  "REX_UNPARSE_INVARIANT[attached-preprocessing-info-list]: null entry at index=0"
  "REX_UNPARSE_INVARIANT[statement-output-owner]: lexical statement=SgNullStatement has neither AST emission nor its own exact token surface"
  "REX_UNPARSE_INVARIANT[preprocessing-output-owner]: attached preprocessing entry uses the legacy non-output suppression bit"
  "REX_UNPARSE_INVARIANT[class-definition-owner]: class definition has no exact owning declaration")
list(LENGTH _owned_list_contract_modes _owned_list_contract_count)
list(LENGTH _owned_list_contract_diagnostics
  _owned_list_contract_diagnostic_count)
if(NOT _owned_list_contract_count EQUAL
    _owned_list_contract_diagnostic_count)
  message(FATAL_ERROR "Owned-list hard-contract test tables have different lengths")
endif()
math(EXPR _owned_list_contract_last "${_owned_list_contract_count} - 1")
foreach(_owned_list_contract_index RANGE ${_owned_list_contract_last})
  list(GET _owned_list_contract_modes ${_owned_list_contract_index} _mode)
  list(GET _owned_list_contract_diagnostics ${_owned_list_contract_index}
    _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_unparser_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_unparser_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$4\" rex_unparser_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_owned_list_contract>
      ${_mode}
      ${_suffix}
      ${_diagnostic})
  set_tests_properties(rex_unparser_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;death")
endforeach()
unset(_owned_list_contract_modes)
unset(_owned_list_contract_diagnostics)
unset(_owned_list_contract_count)
unset(_owned_list_contract_diagnostic_count)
unset(_owned_list_contract_last)
unset(_owned_list_contract_index)

set(_hard_contract_modes
  formatter-negative-newline-count
  formatter-negative-indent
  formatter-zero-linewrap
  formatter-negative-linewrap
  token-sequence-position-invalid
  obsolete-include-directive-statement
  name-qualification-null-initialized-name
  name-qualification-unpublished-declaration
  name-qualification-canonical-namespace-missing-function
  preprocessing-missing-file-info
  preprocessing-invalid-source-line
  preprocessing-retrograde-source-anchor
  fortran-text-detached
  fortran-string-detached
  fortran-text-cxx
  fortran-string-cxx
  fortran-call-target-missing-type
  fortran-statement-missing-physical-owner
  fortran-statement-ambiguous-physical-owner
  fortran-implied-do-missing-object-list
  fortran-implied-do-empty-object-list
  fortran-implied-do-foreign-object-list-owner
  template-parameter-missing-keyword
  template-argument-explicit-after-deduced
  template-argument-source-type-nonexplicit
  declaration-missing-semantic-scope
  delegate-invalid-core-emission
  concept-missing-constraint
  fortran-print-missing-format
  fortran-dimension-missing-source-owner
  requires-missing-requirements
  template-identity-only
  required-name-qualification-missing-record
  name-qualification-unsupported-symbol-kind
  copy-fixup-unsupported-symbol-kind
  copy-fixup-catch-scope
  variable-partial-token
  function-body-partial-token-missing-source
  function-body-partial-token-disabled-source
  namespace-missing-fragments
  namespace-source-fragments-string-without-owner
  source-location-kind
  source-location-missing-type
  source-int-missing-spelling
  source-char-missing-spelling
  source-float-missing-spelling
  source-long-double-missing-spelling
  float80-missing-spelling
  float128-missing-spelling
  source-unnamed-enum-missing-spelling
  void-value
  anonymous-class-type-name
  context-free-named-type
  context-free-declarator-type
  detached-bool-type
  constrained-auto-missing-source-spelling
  unconstrained-auto-with-source-spelling
  cxx-unknown-type-second
  cxx-default-type-skipped
  cxx-imaginary-type-second
  typedef-unknown-source-form
  typedef-null-modifier-base
  typedef-cyclic-modifier-base
  constructor-missing-arguments
  constructor-missing-type
  compound-literal-missing-owner
  compound-literal-visible-owner
  assignment-initializer-missing-operand
  braced-initializer-missing-list
  braced-initializer-null-element
  aggregate-initializer-missing-list
  aggregate-initializer-null-element
  aggregate-initializer-unclassified-source
  designated-initializer-nondesignator
  fortran-aggregate-missing-list
  static-assert-message-owner
  cuda-launch-bounds-owner
  declare-mapper-missing-user-identifier
  declare-mapper-unspecified-identifier-kind
  openmp-device-kind
  openmp-vendor
  openmp-requires-kind
  openmp-requires-mismatched-payload
  openmp-adjust-invalid-modifier
  openmp-adjust-missing-arguments
  openmp-append-missing-operations
  openmp-append-missing-modifier-list
  openmp-append-missing-interop-type
  openmp-append-invalid-depinfo
  openmp-append-invalid-prefer-type-payload
  openmp-init-missing-interop-type
  openmp-init-invalid-depinfo
  openmp-init-syntax-operand
  openmp-append-invalid-list-replacement
  openmp-init-duplicate-modifier-replacement
  openmp-variables-missing-list
  openmp-variables-null-item
  openmp-variables-comma-item
  openmp-source-override-missing-list
  openmp-source-list-missing-discriminator
  openmp-map-unwrapped-item
  openmp-map-invalid-policy-payload
  switch-missing-selector
  switch-foreign-selector
  switch-missing-body
  switch-foreign-body
  switch-aliased-roles
  case-missing-key
  case-foreign-key
  case-missing-body
  case-foreign-body
  case-foreign-range-end
  case-aliased-range-end
  default-missing-body
  default-foreign-body
  coroutine-missing-keyword
  coroutine-invalid-keyword
  coroutine-missing-operand
  coroutine-invalid-return-kind
  gnu-visibility-unknown
  gnu-visibility-error
  gnu-type-visibility-unknown
  gnu-type-visibility-error
  cxx-udl-missing-operator-syntax
  cxx-udl-missing-suffix
  cxx-udl-missing-operand
  cxx-source-pragma-missing-payload
  cxx-pragma-missing-kind
  cxx-generated-pragma-source-payload
  current-source-file-missing
  current-source-file-mismatch
  constructor-initializer-borrowed-from-definition
  member-function-restrict-type-only
  member-function-restrict-declaration-only
  openacc-default
  openacc-reduction)
set(_hard_contract_diagnostics
  "REX_UNPARSE_INVARIANT[formatter-newline]: invalid request num=-1 indent=<none> current-indent=0 chars-on-line=0"
  "REX_UNPARSE_INVARIANT[formatter-newline]: invalid request num=1 indent=-1 current-indent=0 chars-on-line=0"
  "REX_UNPARSE_INVARIANT[formatter-linewrap]: width must be positive, got 0"
  "REX_UNPARSE_INVARIANT[formatter-linewrap]: width must be positive, got -1"
  "REX_UNPARSE_INVARIANT[token-sequence-position]: invalid value=999"
  "REX_UNPARSE_INVARIANT[include-directive-statement]: obsolete SgIncludeDirectiveStatement reached language-independent dispatch: #include <obsolete.h>"
  "REX_UNPARSE_INVARIANT[contextual-name-qualification]: cannot qualify a null initialized name"
  "REX_UNPARSE_INVARIANT[name-qualification-declaration]: declaration-type=SgFunctionDeclaration has no exact first-nondefining identity"
  "REX_NAME_QUALIFICATION_INVARIANT[template-instantiation-function]: name=rex_missing_canonical_function has no exact function symbol in its canonical namespace scope"
  "REX_UNPARSE_INVARIANT[preprocessing-output-owner]: attached preprocessing entry has no typed source or transformation provenance"
  "REX_UNPARSE_INVARIANT[preprocessing-source-anchor]: source preprocessing entry or owner has an invalid physical range"
  "REX_UNPARSE_INVARIANT[preprocessing-source-anchor]: source comment follows its typed attachment boundary"
  "REX_UNPARSE_INVARIANT[fortran-text-context]: emission requires an active Fortran source file"
  "REX_UNPARSE_INVARIANT[fortran-string-context]: emission requires an active Fortran source file"
  "REX_UNPARSE_INVARIANT[fortran-text-context]: emission requires an active Fortran source file"
  "REX_UNPARSE_INVARIANT[fortran-string-context]: emission requires an active Fortran source file"
  "REX_UNPARSE_INVARIANT[fortran-call-target-type]: standalone call target type=SgExpression has no exact semantic procedure type"
  "REX_UNPARSE_INVARIANT[statement-physical-owner]: lexical statement type=SgNullStatement has missing or ambiguous physical output ownership"
  "REX_UNPARSE_INVARIANT[statement-physical-owner]: lexical statement type=SgNullStatement has missing or ambiguous physical output ownership"
  "REX_UNPARSE_INVARIANT[fortran-implied-do-object-list]: implied-do requires one owned nonempty object list"
  "REX_UNPARSE_INVARIANT[fortran-implied-do-object-list]: implied-do requires one owned nonempty object list"
  "REX_UNPARSE_INVARIANT[fortran-implied-do-object-list]: implied-do requires one owned nonempty object list"
  "REX_UNPARSE_INVARIANT[template-parameter-keyword]: template parameter has no exact source keyword"
  "REX_UNPARSE_INVARIANT[template-argument-prefix]: explicit argument follows a non-explicit semantic argument"
  "REX_UNPARSE_INVARIANT[template-argument-source-type]: exact source type belongs only to an explicit type argument"
  "REX_UNPARSE_INVARIANT[declaration-semantic-scope]: declaration has no exact semantic scope"
  "REX_UNPARSE_INVARIANT[delegate-core-emission]: delegate returned an invalid statement-core result"
  "REX_UNPARSE_INVARIANT[concept-constraint]: concept name=rex_concept has no exact constraint expression"
  "REX_UNPARSE_INVARIANT[fortran-print-format]: PRINT statement has no exact format expression"
  "REX_UNPARSE_INVARIANT[fortran-dimension-source]: DIMENSION item does not have an exact variable-to-statement ownership edge"
  "REX_UNPARSE_INVARIANT[requires-structure]: requires-expression has no nonempty owned requirement list"
  "REX_UNPARSE_INVARIANT[template-identity-only]: base SgTemplateDeclaration name=rex_identity kind=1 is a semantic template identity and has no standalone source declaration\; source templates must use a typed SgTemplate*Declaration node"
  "REX_UNPARSE_INVARIANT[required-name-qualification]: required contextual qualification is missing"
  "REX_NAME_QUALIFICATION_INVARIANT[symbol-kind]: unsupported explicit symbol kind"
  "REX_AST_INVARIANT[copy-fixup-symbol-kind]: unsupported explicit symbol kind"
  "REX_AST_INVARIANT[copy-fixup-catch-structure]:"
  "REX_UNPARSE_INVARIANT[variable-partial-token]: AST variable declaration emission cannot inherit partial token replay state"
  "REX_UNPARSE_INVARIANT[function-body-partial-token]: function body inherited partial token replay without an active token-enabled source file"
  "REX_UNPARSE_INVARIANT[function-body-partial-token]: function body inherited partial token replay without an active token-enabled source file"
  "REX_UNPARSE_INVARIANT[namespace-source-fragment]: namespace declaration has no complete typed opening/closing fragment pair"
  "REX_UNPARSE_INVARIANT[namespace-source-owner]: source-spelled namespace fragments require an active physical output source owner"
  "REX_UNPARSE_INVARIANT[source-location-builtin-kind]: invalid kind=999"
  "REX_UNPARSE_INVARIANT[source-location-builtin-type]: builtin expression has no semantic result type"
  "REX_UNPARSE_INVARIANT[literal-spelling]: SgIntVal form=1 has no exact source spelling and is not explicitly canonical-generated"
  "REX_UNPARSE_INVARIANT[literal-spelling]: SgCharVal form=1 has no exact source spelling and is not explicitly canonical-generated"
  "REX_UNPARSE_INVARIANT[literal-spelling]: SgFloatVal form=1 has no exact source spelling and is not explicitly canonical-generated"
  "REX_UNPARSE_INVARIANT[literal-spelling]: SgLongDoubleVal form=1 has no exact source spelling and is not explicitly canonical-generated"
  "REX_UNPARSE_INVARIANT[literal-spelling]: SgFloat80Val form=1 has no exact source spelling and is not explicitly canonical-generated"
  "REX_UNPARSE_INVARIANT[literal-spelling]: SgFloat128Val form=1 has no exact source spelling and is not explicitly canonical-generated"
  "REX_UNPARSE_INVARIANT[literal-spelling]: SgEnumVal form=1 has no exact source spelling and is not explicitly canonical-generated"
  "REX_UNPARSE_INVARIANT[void-value]: SgVoidVal has no source syntax"
  "REX_UNPARSE_INVARIANT[anonymous-class-type]: source-anonymous class type has no standalone source spelling"
  "REX_UNPARSE_INVARIANT[unparse-to-string-type-context]: detached type=SgTypedefType requires an exact source use-site"
  "REX_UNPARSE_INVARIANT[unparse-to-string-type-context]: detached type=SgPointerType requires an exact source use-site"
  "REX_UNPARSE_INVARIANT[builtin-type-language]: context=SgTypeBool requires an exact C or C++ output language"
  "REX_UNPARSE_INVARIANT[constrained-auto-source]: semantic constraint state has no exact source spelling"
  "REX_UNPARSE_INVARIANT[constrained-auto-source]: semantic constraint state has no exact source spelling"
  "REX_UNPARSE_INVARIANT[unknown-type]: cannot emit SgTypeUnknown"
  "REX_UNPARSE_INVARIANT[default-type]: SgTypeDefault has no C/C++ source spelling"
  "REX_UNPARSE_INVARIANT[imaginary-type]: legacy imaginary type specifiers are unsupported"
  "REX_UNPARSE_INVARIANT[typedef-source-form]: name=rex_bad_alias form=0 has no exact typedef/using source form"
  "REX_UNPARSE_INVARIANT[declarator-type]: declaration has a null type in its modifier chain"
  "REX_UNPARSE_INVARIANT[declarator-type]: cyclic modifier type chain"
  "REX_AST_INVARIANT[constructor-initializer-arguments]: constructor initializer has no exact argument list"
  "REX_AST_INVARIANT[exact-expression-type]: node=SgConstructorInitializer has no exact semantic result type"
  "REX_UNPARSE_INVARIANT[compound-literal-owner]: aggregate initializer has no exact initialized-name owner"
  "REX_UNPARSE_INVARIANT[compound-literal-owner]: compound literal does not have one exact auxiliary compiler-generated declaration owner"
  "REX_UNPARSE_INVARIANT[assignment-initializer-operand]: initializer has no exact operand"
  "REX_UNPARSE_INVARIANT[braced-initializer-list]: initializer has no exact expression list"
  "REX_UNPARSE_INVARIANT[braced-initializer-element]: initializer element 0 is null"
  "REX_UNPARSE_INVARIANT[aggregate-initializer-list]: initializer has no exact expression list"
  "REX_UNPARSE_INVARIANT[aggregate-initializer-element]: initializer element 0 is null"
  "REX_UNPARSE_INVARIANT[aggregate-initializer-source-form]: C/C++ aggregate initializer has invalid source form=0"
  "REX_UNPARSE_INVARIANT[designator-element]: designator 0 is not an exactly owned SgDesignator"
  "REX_UNPARSE_INVARIANT[fortran-array-constructor-list]: aggregate initializer has no exact expression list"
  "REX_AST_INVARIANT[static-assert-message]: declaration has neither exact unevaluated-string syntax nor an exactly owned semantic message"
  "REX_UNPARSE_INVARIANT[cuda-launch-bounds]: expression has no exact function owner and semantic type"
  "REX_UNPARSE_INVARIANT[declare-mapper-identifier]: user-defined mapper has no exact explicit identifier expression"
  "REX_UNPARSE_INVARIANT[declare-mapper-identifier]: mapper identifier kind is unspecified"
  "REX_UNPARSE_INVARIANT[openmp-context-selector]: invalid context kind=999"
  "REX_UNPARSE_INVARIANT[openmp-context-selector]: invalid vendor=999"
  "REX_UNPARSE_INVARIANT[openmp-context-selector]: invalid requires payload kind"
  "REX_UNPARSE_INVARIANT[openmp-context-selector]: ordinary requires property owns mismatched payload"
  "REX_UNPARSE_INVARIANT[adjust-args]: malformed typed clause: adjust_args modifier is invalid"
  "REX_UNPARSE_INVARIANT[adjust-args]: malformed typed clause: adjust_args argument list is missing, empty, or misowned"
  "REX_UNPARSE_INVARIANT[append-args]: malformed typed clause: append_args operation list is empty"
  "REX_UNPARSE_INVARIANT[append-args]: malformed typed clause: modifier list is null"
  "REX_UNPARSE_INVARIANT[append-args]: malformed typed clause: interop type is missing"
  "REX_UNPARSE_INVARIANT[append-args]: malformed typed clause: depinfo modifier is invalid in an interop context"
  "REX_UNPARSE_INVARIANT[append-args]: malformed typed clause: prefer_type has no exact nonempty source payload"
  "REX_UNPARSE_INVARIANT[init]: malformed typed clause: interop type is missing"
  "REX_UNPARSE_INVARIANT[init]: malformed typed clause: depinfo modifier is invalid in an interop context"
  "REX_UNPARSE_INVARIANT[init]: malformed typed clause: operand has no semantic value-expression role"
  "REX_AST_INVARIANT[openmp-append-args-operation-replacement]: modifier-list replacement is malformed or owned: interop type is missing"
  "REX_AST_INVARIANT[openmp-init-modifier-list-replacement]: replacement would violate modifier-list semantics: modifier names and interop types are not unique"
  "REX_UNPARSE_INVARIANT[openmp-clause-items]: SgOmpPrivateClause requires one nonempty exactly owned expression list"
  "REX_UNPARSE_INVARIANT[openmp-clause-items]: SgOmpPrivateClause has a null or foreign item at index 0"
  "REX_UNPARSE_INVARIANT[openmp-clause-items]: SgOmpPrivateClause has an unnormalized comma item at index 0"
  "REX_UNPARSE_INVARIANT[omp-source-variable-override]: source override discriminator and owned list disagree"
  "REX_UNPARSE_INVARIANT[omp-source-variable-override]: source override discriminator and owned list disagree"
  "REX_UNPARSE_INVARIANT[omp-map-item-owner]: map clause requires exactly owned typed map items"
  "REX_UNPARSE_INVARIANT[omp-dist-data-payload]: policy kind, argument, or ownership is invalid"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: switch statement has no exactly owned selector"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: switch statement has no exactly owned selector"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: switch statement has no exactly owned body"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: switch statement has no exactly owned body"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: switch statement aliases selector and body"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: case statement has no exactly owned key"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: case statement has no exactly owned key"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: case statement has no exactly owned body"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: case statement has no exactly owned body"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: case statement has no exactly owned range end"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: case statement aliases key and range end"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: default statement has no exactly owned body"
  "REX_UNPARSE_INVARIANT[cxx-statement-structure]: default statement has no exactly owned body"
  "REX_UNPARSE_INVARIANT[coroutine-keyword]: await expression has invalid typed source kind=0"
  "REX_UNPARSE_INVARIANT[coroutine-keyword]: await expression has invalid typed source kind=999"
  "REX_UNPARSE_INVARIANT[coroutine-operand]: await expression has no exact owned operand"
  "REX_UNPARSE_INVARIANT[coroutine-return-kind]: return statement has invalid typed source kind=999"
  "REX_UNPARSE_INVARIANT[gnu-visibility]: declaration has unknown visibility"
  "REX_UNPARSE_INVARIANT[gnu-visibility]: declaration has error visibility"
  "REX_UNPARSE_INVARIANT[gnu-visibility]: declaration has unknown type visibility"
  "REX_UNPARSE_INVARIANT[gnu-visibility]: declaration has error type visibility"
  "REX_UNPARSE_INVARIANT[operator-source-surface]: typed operator surface is incomplete"
  "REX_UNPARSE_INVARIANT[user-defined-literal-call]: literal surface has no exact source suffix"
  "REX_UNPARSE_INVARIANT[user-defined-literal-call]: literal surface has malformed lexical/semantic operands"
  "REX_UNPARSE_INVARIANT[pragma-source-text]: source-spelled C/C++ pragma has no exact typed source payload"
  "REX_UNPARSE_INVARIANT[pragma-payload-kind]: pragma has invalid typed C/C++ payload kind=0"
  "REX_UNPARSE_INVARIANT[pragma-source-text]: generated C/C++ pragma also owns a source-spelled payload"
  "REX_UNPARSE_INVARIANT[current-source-file]: unparse-file=rex_current_source_file.cpp inherited=<null> must identify the same source file"
  "REX_UNPARSE_INVARIANT[current-source-file]: unparse-file=rex_current_source_file.cpp inherited=rex_other_source_file.cpp must identify the same source file"
  "REX_UNPARSE_INVARIANT[constructor-initializer-owner]: emitted declaration has no exact initializer list but its distinct defining declaration has 1 entries"
  "REX_UNPARSE_INVARIANT[member-function-restrict-role]: type-restrict=true declaration-restrict=false disagree"
  "REX_UNPARSE_INVARIANT[member-function-restrict-role]: type-restrict=false declaration-restrict=true disagree"
  "REX_UNPARSE_INVARIANT[openacc-default-kind]: invalid value=999"
  "REX_UNPARSE_INVARIANT[openacc-reduction-operator]: invalid value=999")
list(LENGTH _hard_contract_modes _hard_contract_count)
list(LENGTH _hard_contract_diagnostics _hard_contract_diagnostic_count)
if(NOT _hard_contract_count EQUAL _hard_contract_diagnostic_count)
  message(FATAL_ERROR "Backend hard-contract test tables have different lengths")
endif()
math(EXPR _hard_contract_last "${_hard_contract_count} - 1")
foreach(_hard_contract_index RANGE ${_hard_contract_last})
  list(GET _hard_contract_modes ${_hard_contract_index} _mode)
  list(GET _hard_contract_diagnostics ${_hard_contract_index} _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_unparser_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_unparser_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$4\" rex_unparser_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_backend_hard_contracts>
      "${_mode}"
      "${_suffix}"
      "${_diagnostic}")
  set_tests_properties(rex_unparser_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;death")
endforeach()

add_test(
  NAME rex_ast_openmp_append_self_replacement
  COMMAND $<TARGET_FILE:rex_unparser_backend_hard_contracts>
    openmp-append-self-replacement)
set_tests_properties(rex_ast_openmp_append_self_replacement
  PROPERTIES LABELS "rex;AST;OpenMP")

add_test(
  NAME rex_unparser_line_directive_missing_source_location_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" line-directive-missing-source-location >rex_unparser_line_directive_missing_source_location.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[line-directive-location\\]: statement=0x[[:xdigit:]]+/SgNullStatement has no exact positive source line and nonempty filename$' rex_unparser_line_directive_missing_source_location.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(
  rex_unparser_line_directive_missing_source_location_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

set(_line_directive_filename_modes
  quote
  backslash
  line-feed
  carriage-return
  control-byte)
set(_line_directive_filename_bytes
  22
  5c
  0a
  0d
  01)
list(LENGTH _line_directive_filename_modes _line_directive_filename_count)
math(EXPR _line_directive_filename_last
  "${_line_directive_filename_count} - 1")
foreach(_line_directive_filename_index RANGE
    ${_line_directive_filename_last})
  list(GET _line_directive_filename_modes
    ${_line_directive_filename_index} _line_directive_filename_mode)
  list(GET _line_directive_filename_bytes
    ${_line_directive_filename_index} _line_directive_filename_byte)
  string(REPLACE "-" "_" _line_directive_filename_suffix
    "${_line_directive_filename_mode}")
  add_test(
    NAME rex_unparser_line_directive_filename_${_line_directive_filename_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" line-directive-filename-$2 >rex_unparser_line_directive_filename_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[line-directive-filename\\]: statement=0x[[:xdigit:]]+/SgNullStatement filename has unrepresentable byte=0x${_line_directive_filename_byte} at offset=5$' rex_unparser_line_directive_filename_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_backend_hard_contracts>
      "${_line_directive_filename_mode}"
      "${_line_directive_filename_suffix}"
      "${_line_directive_filename_byte}")
  set_tests_properties(
    rex_unparser_line_directive_filename_${_line_directive_filename_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;death")
endforeach()
unset(_line_directive_filename_modes)
unset(_line_directive_filename_bytes)
unset(_line_directive_filename_count)
unset(_line_directive_filename_last)
unset(_line_directive_filename_index)
unset(_line_directive_filename_mode)
unset(_line_directive_filename_byte)
unset(_line_directive_filename_suffix)

add_test(
  NAME rex_unparser_cxx_udl_missing_declaration_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" cxx-udl-missing-declaration >rex_unparser_cxx_udl_missing_declaration.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[operator-source-callee\\]: call=0x[[:xdigit:]]+ has no exact operator declaration$' rex_unparser_cxx_udl_missing_declaration.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(rex_unparser_cxx_udl_missing_declaration_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

add_test(
  NAME rex_unparser_name_qualification_using_null_scope_child_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" name-qualification-using-null-scope-child >rex_unparser_name_qualification_using_null_scope_child.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_NAME_QUALIFICATION_INVARIANT\\[using-directive-order\\]: scope=0x[[:xdigit:]]+/SgGlobal has malformed child\\[[[:digit:]]+\\]=\\(nil\\) with parent=\\(nil\\)$' rex_unparser_name_qualification_using_null_scope_child.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(
  rex_unparser_name_qualification_using_null_scope_child_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

foreach(_using_target_mode IN ITEMS
    null-target
    target-missing-definition)
  string(REPLACE "-" "_" _using_target_suffix "${_using_target_mode}")
  add_test(
    NAME rex_unparser_name_qualification_using_${_using_target_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" name-qualification-using-$2 >rex_unparser_name_qualification_using_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_NAME_QUALIFICATION_INVARIANT\\[using-directive-order\\]: directive=0x[[:xdigit:]]+ at scope=0x[[:xdigit:]]+ index=[[:digit:]]+ has namespace=(\\(nil\\)|0x[[:xdigit:]]+) definition=\\(nil\\)$' rex_unparser_name_qualification_using_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_backend_hard_contracts>
      "${_using_target_mode}"
      "${_using_target_suffix}")
  set_tests_properties(
    rex_unparser_name_qualification_using_${_using_target_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;death")
endforeach()
unset(_using_target_mode)
unset(_using_target_suffix)

add_test(
  NAME rex_unparser_name_qualification_using_missing_comparison_order_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" name-qualification-using-missing-comparison-order >rex_unparser_name_qualification_using_missing_comparison_order.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_NAME_QUALIFICATION_INVARIANT\\[using-directive-order\\]: scope=0x[[:xdigit:]]+ position=0x[[:xdigit:]]+ directive=0x[[:xdigit:]]+ has neither a shared lexical anchor nor complete typed translation-unit source order$' rex_unparser_name_qualification_using_missing_comparison_order.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(
  rex_unparser_name_qualification_using_missing_comparison_order_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

foreach(_using_order_state IN ITEMS
    typed-mirror-mismatch
    mirror-without-typed-order
    typed-order-without-mirror)
  if(_using_order_state STREQUAL "typed-mirror-mismatch")
    set(_using_order_typed_value 11)
    set(_using_order_mirror_value 12)
  elseif(_using_order_state STREQUAL "mirror-without-typed-order")
    set(_using_order_typed_value "<absent>")
    set(_using_order_mirror_value 12)
  else()
    set(_using_order_typed_value 11)
    set(_using_order_mirror_value 0)
  endif()
  string(REPLACE "-" "_" _using_order_state_suffix
    "${_using_order_state}")
  add_test(
    NAME rex_unparser_name_qualification_using_${_using_order_state_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" name-qualification-using-$2 >rex_unparser_name_qualification_using_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_NAME_QUALIFICATION_INVARIANT\\[using-directive-order\\]: directive=0x[[:xdigit:]]+ at scope=0x[[:xdigit:]]+ index=[[:digit:]]+ has typed translation-unit source order=${_using_order_typed_value} but source-position mirror=${_using_order_mirror_value}$' rex_unparser_name_qualification_using_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_backend_hard_contracts>
      "${_using_order_state}"
      "${_using_order_state_suffix}")
  set_tests_properties(
    rex_unparser_name_qualification_using_${_using_order_state_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;death")
endforeach()
unset(_using_order_state)
unset(_using_order_typed_value)
unset(_using_order_mirror_value)
unset(_using_order_state_suffix)

add_test(
  NAME rex_unparser_name_qualification_position_typed_mirror_mismatch_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" name-qualification-position-typed-mirror-mismatch >rex_unparser_name_qualification_position_typed_mirror_mismatch.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_NAME_QUALIFICATION_INVARIANT\\[using-directive-order\\]: position=0x[[:xdigit:]]+/SgUsingDirectiveStatement has typed translation-unit source order=11 but source-position mirror=12$' rex_unparser_name_qualification_position_typed_mirror_mismatch.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(
  rex_unparser_name_qualification_position_typed_mirror_mismatch_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

add_test(
  NAME rex_unparser_name_qualification_using_unsupported_scope_kind_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" name-qualification-using-unsupported-scope-kind >rex_unparser_name_qualification_using_unsupported_scope_kind.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_NAME_QUALIFICATION_INVARIANT\\[using-directive-order\\]: scope=0x[[:xdigit:]]+/SgScopeStatement has unsupported lexical child-list kind=[[:digit:]]+$' rex_unparser_name_qualification_using_unsupported_scope_kind.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(
  rex_unparser_name_qualification_using_unsupported_scope_kind_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

add_test(
  NAME rex_unparser_name_qualification_using_scope_cycle_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" name-qualification-using-scope-cycle >rex_unparser_name_qualification_using_scope_cycle.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_NAME_QUALIFICATION_INVARIANT\\[using-directive-order\\]: scope chain from current scope=0x[[:xdigit:]]+ contains a cycle at scope=0x[[:xdigit:]]+/SgBasicBlock$' rex_unparser_name_qualification_using_scope_cycle.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(
  rex_unparser_name_qualification_using_scope_cycle_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

add_test(
  NAME rex_unparser_class_declaration_inside_preprocessing_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" class-declaration-inside-preprocessing >rex_unparser_class_declaration_inside_preprocessing.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[preprocessing-owner\\]: class declaration=0x[[:xdigit:]]+ owns inside syntax but is not an exact forward declaration boundary$' rex_unparser_class_declaration_inside_preprocessing.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(
  rex_unparser_class_declaration_inside_preprocessing_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

add_test(
  NAME rex_unparser_class_type_declaration_inside_preprocessing_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" class-type-declaration-inside-preprocessing >rex_unparser_class_type_declaration_inside_preprocessing.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[preprocessing-owner\\]: class declaration=0x[[:xdigit:]]+ owns inside syntax; its definition must own the class body boundary$' rex_unparser_class_type_declaration_inside_preprocessing.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(
  rex_unparser_class_type_declaration_inside_preprocessing_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

add_test(
  NAME rex_unparser_namespace_declaration_inside_preprocessing_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" namespace-declaration-inside-preprocessing >rex_unparser_namespace_declaration_inside_preprocessing.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[preprocessing-owner\\]: namespace declaration=0x[[:xdigit:]]+ owns inside syntax; its definition must own the namespace body boundary$' rex_unparser_namespace_declaration_inside_preprocessing.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(
  rex_unparser_namespace_declaration_inside_preprocessing_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

add_test(
  NAME rex_unparser_cxx_call_default_argument_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" cxx-call-default-argument >rex_unparser_cxx_call_default_argument.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[function-call-argument\\]: call=0x[[:xdigit:]]+ contains an implicit default argument instead of only source-explicit arguments$' rex_unparser_cxx_call_default_argument.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(rex_unparser_cxx_call_default_argument_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

add_test(
  NAME rex_ast_function_call_literal_callee_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" function-call-literal-callee >rex_ast_function_call_literal_callee.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_AST_INVARIANT[function-call-callee]: non-callable literal SgIntVal cannot be a function-call callee' rex_ast_function_call_literal_callee.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(rex_ast_function_call_literal_callee_is_rejected
  PROPERTIES LABELS "rex;AST;death")

set(_ast_type_contract_modes
  lambda-missing-closure-type
  this-missing-result-type
  this-builder-missing-result-type
  this-symbol-type-mismatch
  sizeof-missing-result-type
  alignof-missing-result-type
  sizeof-builder-missing-result-type
  alignof-builder-missing-result-type
  sizeof-explicit-invalid-result-type
  alignof-explicit-invalid-result-type
  sizeof-foreign-target-result-type
  alignof-foreign-target-result-type
  sizeof-replace-foreign-old
  alignof-replace-foreign-old
  sizeof-replace-owned-new
  alignof-replace-owned-new
  target-size-detached-context
  function-call-missing-result-type
  function-call-callable-result-type
  function-call-builder-callable-result-type
  implied-do-type-query
  conditional-missing-result-type
  typed-builtin-missing-result-type
  binary-missing-result-type
  unary-missing-result-type
  binary-builder-missing-result-type
  unary-builder-missing-result-type
  throw-nonvoid-result-type
  await-missing-result-type
  fold-default-result-type
  statement-expression-unknown-result-type
  caf-coexpression-missing-result-type
  typeid-missing-result-type
  null-expression-type-query
  null-expression-invalid-role
  expr-list-type-query
  cuda-exec-config-type-query
  range-type-query
  range-fourth-element
  subscript-type-query
  designator-type-query
  asm-operand-type-query
  simple-requirement-type-query
  type-requirement-type-query
  compound-requirement-type-query
  nested-requirement-type-query
  type-expression-type-query
  type-expression-missing-represented-type
  source-location-builtin-missing-result-type
  pseudo-destructor-missing-result-type
  vararg-missing-result-type
  function-parameter-reference-missing-type
  function-parameter-reference-placeholder-type
  asterisk-shape-type-query
  colon-shape-type-query
  assumed-rank-type-query
  coarray-image-selector-type-query)
set(_ast_type_contract_diagnostics
  "REX_AST_INVARIANT[lambda-type]: lambda expression has no exact closure class type"
  "REX_AST_INVARIANT[this-result-type]: SgThisExp has no exact semantic pointer result type"
  "REX_SAGE_BUILDER_INVARIANT[this-expression]: exact class/nonreal symbol and its matching cv-qualified pointer result type are required"
  "REX_AST_INVARIANT[this-symbol-type]: SgThisExp result pointee does not match its exact class/nonreal symbol type"
  "REX_AST_INVARIANT[sizeof-result-type]: SgSizeOfOp has no exact unsigned target size_t result type"
  "REX_AST_INVARIANT[alignof-result-type]: SgAlignOfOp has no exact unsigned target size_t result type"
  "REX_SAGE_BUILDER_INVARIANT[sizeof-expression]: exactly one detached operand and its exact unsigned target size_t result type are required"
  "REX_SAGE_BUILDER_INVARIANT[alignof-expression]: exactly one detached operand and its exact unsigned target size_t result type are required"
  "REX_AST_INVARIANT[sizeof-result-type]: SgSizeOfOp has no exact unsigned target size_t result type"
  "REX_AST_INVARIANT[alignof-result-type]: SgAlignOfOp has no exact unsigned target size_t result type"
  "REX_AST_INVARIANT[sizeof-result-type]: SgSizeOfOp has no exact unsigned target size_t result type"
  "REX_AST_INVARIANT[alignof-result-type]: SgAlignOfOp has no exact unsigned target size_t result type"
  "REX_AST_INVARIANT[sizeof-replace-old-operand]: old expression is not the owned sizeof operand"
  "REX_AST_INVARIANT[alignof-replace-old-operand]: old expression is not the owned alignof operand"
  "REX_AST_INVARIANT[sizeof-replace-new-operand-owner]: new sizeof operand already has a structural owner"
  "REX_AST_INVARIANT[alignof-replace-new-operand-owner]: new alignof operand already has a structural owner"
  "REX_AST_INVARIANT[target-size-type]: context=SgIntVal requires one owning source file with its exact unsigned target size_t type"
  "REX_AST_INVARIANT[call-result-type]: SgFunctionCallExp has no exact semantic result type"
  "REX_AST_INVARIANT[call-result-type]: SgFunctionCallExp stores a callable type instead of its exact semantic result type"
  "REX_AST_INVARIANT[call-result-type-producer]: producer=buildFunctionCallExp_nfi(expression) published invalid result type=SgFunctionType"
  "REX_AST_INVARIANT[implied-do-type]: implied-do is a control/list construct and has no standalone expression type"
  "REX_AST_INVARIANT[conditional-result-type]: conditional expression has no exact semantic result type"
  "REX_AST_INVARIANT[typed-builtin-result-type]: builtin=__builtin_offsetof has no exact semantic result type"
  "REX_AST_INVARIANT[binary-result-type]: expression=SgAddOp has no exact semantic result type"
  "REX_AST_INVARIANT[unary-result-type]: expression=SgMinusOp has no exact semantic result type"
  "REX_AST_INVARIANT[binary-result-type-producer]: builder requires an exact semantic result type"
  "REX_AST_INVARIANT[unary-result-type-producer]: builder requires an exact semantic result type"
  "REX_AST_INVARIANT[throw-result-type]: throw expression result type must be void"
  "REX_AST_INVARIANT[await-result-type]: SgAwaitExpression has no exact semantic result type"
  "REX_AST_INVARIANT[fold-result-type]: SgFoldExpression has no exact semantic result type"
  "REX_AST_INVARIANT[statement-expression-result-type]: SgStatementExpression has no exact semantic result type"
  "REX_AST_INVARIANT[caf-coexpression-result-type]: coarray reference has no exact semantic result type"
  "REX_AST_INVARIANT[typeid-result-type]: typeid expression must store its exact const reference result type"
  "REX_AST_INVARIANT[syntax-expression-type]: node=SgNullExpression has no standalone semantic value type"
  "REX_AST_INVARIANT[null-expression-role]: null-expression builders require the exact syntactic-absence role"
  "REX_AST_INVARIANT[syntax-expression-type]: node=SgExprListExp has no standalone semantic value type"
  "REX_AST_INVARIANT[syntax-expression-type]: node=SgCudaKernelExecConfig has no standalone semantic value type"
  "REX_AST_INVARIANT[syntax-expression-type]: node=SgRangeExp has no standalone semantic value type"
  "REX_AST_INVARIANT[range-arity]: SgRangeExp cannot own more than start, stride, and end expressions"
  "REX_AST_INVARIANT[syntax-expression-type]: node=SgSubscriptExpression has no standalone semantic value type"
  "REX_AST_INVARIANT[syntax-expression-type]: node=SgDesignator has no standalone semantic value type"
  "REX_AST_INVARIANT[syntax-expression-type]: node=SgAsmOp has no standalone semantic value type"
  "REX_AST_INVARIANT[syntax-expression-type]: node=SgSimpleRequirement has no standalone semantic value type"
  "REX_AST_INVARIANT[syntax-expression-type]: node=SgTypeRequirement has no standalone semantic value type"
  "REX_AST_INVARIANT[syntax-expression-type]: node=SgCompoundRequirement has no standalone semantic value type"
  "REX_AST_INVARIANT[syntax-expression-type]: node=SgNestedRequirement has no standalone semantic value type"
  "REX_AST_INVARIANT[syntax-expression-type]: node=SgTypeExpression has no standalone semantic value type"
  "REX_AST_INVARIANT[type-expression-represented-type]: type expression builder requires an exact represented type"
  "REX_AST_INVARIANT[exact-expression-type]: node=SgSourceLocationBuiltinExp has no exact semantic result type"
  "REX_AST_INVARIANT[pseudo-destructor-type-producer]: pseudo-destructor requires an exact object type and void member-function result type"
  "REX_AST_INVARIANT[vararg-result-type]: SgVarArgOp has no exact semantic result type"
  "REX_AST_INVARIANT[function-parameter-reference-type]: SgFunctionParameterRefExp has no exact semantic result type"
  "REX_AST_INVARIANT[function-parameter-reference-type]: SgFunctionParameterRefExp has no exact semantic result type"
  "REX_AST_INVARIANT[fortran-shape-syntax-type]: asterisk shape has no standalone semantic value type"
  "REX_AST_INVARIANT[fortran-shape-syntax-type]: colon shape has no standalone semantic value type"
  "REX_AST_INVARIANT[fortran-shape-syntax-type]: assumed-rank shape has no standalone semantic value type"
  "REX_AST_INVARIANT[fortran-image-selector-type]: coarray image selector has no standalone semantic value type")
list(LENGTH _ast_type_contract_modes _ast_type_contract_mode_count)
list(LENGTH _ast_type_contract_diagnostics
  _ast_type_contract_diagnostic_count)
if(NOT _ast_type_contract_mode_count EQUAL
   _ast_type_contract_diagnostic_count)
  message(FATAL_ERROR "AST type-contract test tables have different lengths")
endif()
math(EXPR _ast_type_contract_last "${_ast_type_contract_mode_count} - 1")
foreach(_ast_type_contract_index RANGE ${_ast_type_contract_last})
  list(GET _ast_type_contract_modes ${_ast_type_contract_index} _mode)
  list(GET _ast_type_contract_diagnostics ${_ast_type_contract_index}
    _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_ast_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_ast_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$4\" rex_ast_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_backend_hard_contracts>
      "${_mode}"
      "${_suffix}"
      "${_diagnostic}")
  set_tests_properties(rex_ast_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;AST;death")
endforeach()

add_test(
  NAME rex_ast_pack_pattern_foreign_owner_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" pack-pattern-foreign-owner >rex_ast_pack_pattern_foreign_owner.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[pack-expansion-pattern-owner\\]: pack-expansion pattern=0x[[:xdigit:]]+/SgIntVal already has foreign owner=0x[[:xdigit:]]+/SgExprStatement$' rex_ast_pack_pattern_foreign_owner.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_backend_hard_contracts>)
set_tests_properties(rex_ast_pack_pattern_foreign_owner_is_rejected
  PROPERTIES LABELS "rex;AST;death")
unset(_ast_type_contract_modes)
unset(_ast_type_contract_diagnostics)
unset(_ast_type_contract_mode_count)
unset(_ast_type_contract_diagnostic_count)
unset(_ast_type_contract_last)
unset(_ast_type_contract_index)

unset(_hard_contract_modes)
unset(_hard_contract_diagnostics)
unset(_hard_contract_count)
unset(_hard_contract_diagnostic_count)
unset(_hard_contract_last)
unset(_hard_contract_index)

if(ENABLE-FORTRAN)
  add_test(
    NAME rex_unparser_fortran_format_literal_spaces
    COMMAND $<TARGET_FILE:rex_unparser_fortran_format_literal_spaces>
      -rose:skipfinalCompileStep
      -c
      ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests/rex_fortran_format_literal_spaces.f90)
  set_tests_properties(rex_unparser_fortran_format_literal_spaces
    PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;Fortran")
endif()

add_test(
  NAME rex_unparser_fortran_malformed_string_boundary_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" --malformed >rex_unparser_fortran_malformed_string_boundary.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_UNPARSE_INVARIANT[fortran-string]: delimiter inside a literal is not doubled' rex_unparser_fortran_malformed_string_boundary.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_fortran_string_boundary>)
set_tests_properties(
  rex_unparser_fortran_malformed_string_boundary_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;death")

add_test(
  NAME rex_unparser_info_session_state_is_per_root
  COMMAND rex_unparser_info_session_state
)
set_tests_properties(rex_unparser_info_session_state_is_per_root
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_unparser_invocation_state_is_isolated
  COMMAND rex_unparser_invocation_state
)
set_tests_properties(rex_unparser_invocation_state_is_isolated
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_unparser_string_literal_whitespace_is_preserved
  COMMAND rex_unparser_string_literal_whitespace
)
set_tests_properties(rex_unparser_string_literal_whitespace_is_preserved
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING")

add_test(
  NAME rex_unparser_compact_output_raw_text_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" --raw-text >rex_unparser_compact_output_raw_text.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_UNPARSE_INVARIANT[compact-output]: raw output bypassed the emission-time formatter' rex_unparser_compact_output_raw_text.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_string_literal_whitespace>)
set_tests_properties(rex_unparser_compact_output_raw_text_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;death")

add_test(
  NAME rex_unparser_compact_output_double_finalize_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" --double-finalize >rex_unparser_compact_output_double_finalize.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_UNPARSE_INVARIANT[compact-output]: formatter was finalized more than once' rex_unparser_compact_output_double_finalize.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_string_literal_whitespace>)
set_tests_properties(
  rex_unparser_compact_output_double_finalize_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;death")

add_test(
  NAME rex_unparser_compact_output_post_finalize_write_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" --post-finalize >rex_unparser_compact_output_post_finalize.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_UNPARSE_INVARIANT[compact-output]: emission requires an active, unfinalized compact formatter' rex_unparser_compact_output_post_finalize.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_string_literal_whitespace>)
set_tests_properties(
  rex_unparser_compact_output_post_finalize_write_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;death")

add_test(
  NAME rex_unparser_compact_output_missing_finalize_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" --missing-finalize >rex_unparser_compact_output_missing_finalize.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_UNPARSE_INVARIANT[compact-output]: formatter was destroyed before finalization' rex_unparser_compact_output_missing_finalize.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_string_literal_whitespace>)
set_tests_properties(
  rex_unparser_compact_output_missing_finalize_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;death")

add_test(
  NAME rex_unparser_compact_output_inactive_finalize_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" --inactive-finalize >rex_unparser_compact_output_inactive_finalize.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_UNPARSE_INVARIANT[compact-output]: finalization requires an active compact formatter' rex_unparser_compact_output_inactive_finalize.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_string_literal_whitespace>)
set_tests_properties(
  rex_unparser_compact_output_inactive_finalize_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;death")

set(_compact_directive_death_modes
  directive-newline
  directive-carriage-return
  directive-line-splice
  nested-directive
  unterminated-directive
  failed-compact-output
  failed-compact-finalize)
set(_compact_directive_death_diagnostics
  "REX_UNPARSE_INVARIANT[compact-directive]: dense diagnostic directive must be one nonempty logical line with a C/C++ or Fortran directive sentinel\; a C/C++ directive must not end in a line-splicing backslash"
  "REX_UNPARSE_INVARIANT[compact-directive]: dense diagnostic directive must be one nonempty logical line with a C/C++ or Fortran directive sentinel\; a C/C++ directive must not end in a line-splicing backslash"
  "REX_UNPARSE_INVARIANT[compact-directive]: dense diagnostic directive must be one nonempty logical line with a C/C++ or Fortran directive sentinel\; a C/C++ directive must not end in a line-splicing backslash"
  "REX_UNPARSE_INVARIANT[compact-directive]: nested or stale directive assembly"
  "REX_UNPARSE_INVARIANT[compact-directive]: compact output was finalized with an unterminated directive"
  "REX_UNPARSE_INVARIANT[compact-output]: failed writing exact compact payload"
  "REX_UNPARSE_INVARIANT[compact-output]: failed finalizing the exact compact output stream")
list(LENGTH _compact_directive_death_modes
  _compact_directive_death_mode_count)
list(LENGTH _compact_directive_death_diagnostics
  _compact_directive_death_diagnostic_count)
if(NOT _compact_directive_death_mode_count EQUAL
   _compact_directive_death_diagnostic_count)
  message(FATAL_ERROR
    "Compact-directive death-test tables have different lengths")
endif()
math(EXPR _compact_directive_death_last
  "${_compact_directive_death_mode_count} - 1")
foreach(_compact_directive_death_index
        RANGE ${_compact_directive_death_last})
  list(GET _compact_directive_death_modes
    ${_compact_directive_death_index} _mode)
  list(GET _compact_directive_death_diagnostics
    ${_compact_directive_death_index} _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_unparser_compact_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" --$2 >rex_unparser_compact_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$4\" rex_unparser_compact_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_string_literal_whitespace>
      ${_mode}
      ${_suffix}
      "${_diagnostic}")
  set_tests_properties(rex_unparser_compact_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;death")
endforeach()
unset(_compact_directive_death_modes)
unset(_compact_directive_death_diagnostics)
unset(_compact_directive_death_mode_count)
unset(_compact_directive_death_diagnostic_count)
unset(_compact_directive_death_last)
unset(_compact_directive_death_index)
unset(_mode)
unset(_diagnostic)
unset(_suffix)

foreach(_mode IN ITEMS
    invalid-string-encoding
    invalid-cxx-string-delimiter
    invalid-raw-string-state
    invalid-raw-string-delimiter
    invalid-raw-string-control-delimiter
    invalid-raw-string-payload)
  string(REPLACE "-" "_" _test_suffix "${_mode}")
  if(_mode STREQUAL "invalid-string-encoding")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[cxx-string-encoding]: invalid encoding=999")
  elseif(_mode STREQUAL "invalid-cxx-string-delimiter")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[cxx-string-delimiter]: C/C++ string literal carries a Fortran delimiter=34")
  elseif(_mode STREQUAL "invalid-raw-string-state")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[cxx-raw-string-state]: non-raw literal carries raw delimiter or payload state")
  elseif(_mode STREQUAL "invalid-raw-string-delimiter")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[cxx-raw-string-delimiter]: delimiter contains forbidden byte=32")
  elseif(_mode STREQUAL "invalid-raw-string-control-delimiter")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[cxx-raw-string-delimiter]: delimiter contains forbidden byte=1")
  elseif(_mode STREQUAL "invalid-raw-string-payload")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[cxx-raw-string-payload]: payload contains its own closing delimiter")
  else()
    message(FATAL_ERROR "unmapped C/C++ literal corruption mode: ${_mode}")
  endif()
  add_test(
    NAME rex_unparser_cxx_${_test_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" --$2 >rex_unparser_cxx_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc '${_expected_diagnostic}' rex_unparser_cxx_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_string_literal_whitespace>
      ${_mode}
      ${_test_suffix})
  set_tests_properties(rex_unparser_cxx_${_test_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;death")
endforeach()

add_test(
  NAME rex_unparser_compact_output_pragma_has_typed_logical_boundary
  COMMAND rex_unparser_string_literal_whitespace --pragma
)
set_tests_properties(
  rex_unparser_compact_output_pragma_has_typed_logical_boundary
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING")

add_test(
  NAME rex_unparser_header_output_uses_frontend_snapshot
  COMMAND rex_unparser_header_snapshot
)
set_tests_properties(rex_unparser_header_output_uses_frontend_snapshot
  PROPERTIES LABELS "rex;UNPARSER;UNPARSEHEADERTEST")

add_test(
  NAME rex_unparser_projects_and_repeated_output_are_isolated
  COMMAND rex_unparser_project_isolation
)
set_tests_properties(rex_unparser_projects_and_repeated_output_are_isolated
  PROPERTIES LABELS "rex;UNPARSER")

set(_decl_attachment_death_modes
  null-list
  null-owner
  null-declaration
  null-membership-entry
  null-source-line-entry
  null-insertion
  null-erasure
  null-existing-replacement
  null-new-replacement
  null-source-line-list
  null-source-line-location
  null-invalidation-list
  duplicate-child-membership
  parent-list-disagreement
  simultaneous-cross-owner-membership
  replacement-already-present
  replacement-cross-owned
  detach-missing-child)
set(_decl_attachment_death_diagnostics
  "REX_FRONTEND_INVARIANT[declaration-attachment-session]: operation=index requires non-null declaration-list"
  "REX_FRONTEND_INVARIANT[declaration-attachment-session]: operation=index requires non-null owner"
  "REX_FRONTEND_INVARIANT[declaration-attachment-session]: operation=contains requires non-null declaration"
  "REX_FRONTEND_INVARIANT[declaration-attachment-session]: operation=rebuild declaration-list contains null entry at index=0"
  "REX_FRONTEND_INVARIANT[declaration-attachment-session]: operation=lookup-source-line declaration-list contains null entry at index=0"
  "REX_FRONTEND_INVARIANT[declaration-attachment-session]: operation=record-insertion requires non-null declaration"
  "REX_FRONTEND_INVARIANT[declaration-attachment-session]: operation=record-erasure requires non-null declaration"
  "REX_FRONTEND_INVARIANT[declaration-attachment-session]: operation=record-replacement requires non-null existing-declaration"
  "REX_FRONTEND_INVARIANT[declaration-attachment-session]: operation=record-replacement requires non-null replacement-declaration"
  "REX_FRONTEND_INVARIANT[declaration-attachment-session]: operation=lookup-source-line requires non-null declaration-list"
  "REX_FRONTEND_INVARIANT[declaration-attachment-session]: operation=lookup-source-line requires non-null location"
  "REX_FRONTEND_INVARIANT[declaration-attachment-session]: operation=invalidate requires non-null declaration-list"
  "REX_FRONTEND_INVARIANT[declaration-child-list-cardinality]: operation=contract-duplicate declaration occurs 2 times\; expected at most one"
  "^REX_FRONTEND_INVARIANT\\[declaration-child-list-owner\\]: operation=contract-parent-list declaration=0x[[:xdigit:]]+ type=SgEmptyDeclaration occurrences=1 parent=0x[[:xdigit:]]+/SgGlobal owner=0x[[:xdigit:]]+/SgGlobal membership and structural parent disagree$"
  "^REX_FRONTEND_INVARIANT\\[declaration-child-list-owner\\]: operation=contract-cross-owner declaration=0x[[:xdigit:]]+ type=SgEmptyDeclaration occurrences=1 parent=0x[[:xdigit:]]+/SgGlobal owner=0x[[:xdigit:]]+/SgGlobal membership and structural parent disagree$"
  "REX_FRONTEND_INVARIANT[declaration-child-list-replacement]: operation=contract-replacement-present replacement is already attached"
  "REX_FRONTEND_INVARIANT[declaration-child-list-replacement]: operation=contract-replacement-cross-owned replacement already has a structural owner"
  "REX_FRONTEND_INVARIANT[declaration-child-list-detach]: operation=contract-detach-missing declaration is not attached exactly once")
list(LENGTH _decl_attachment_death_modes _decl_attachment_mode_count)
list(LENGTH _decl_attachment_death_diagnostics
  _decl_attachment_diagnostic_count)
if(NOT _decl_attachment_mode_count EQUAL
   _decl_attachment_diagnostic_count)
  message(FATAL_ERROR
    "Declaration-attachment hard-contract test tables have different lengths")
endif()
math(EXPR _decl_attachment_last "${_decl_attachment_mode_count} - 1")
foreach(_decl_attachment_index RANGE ${_decl_attachment_last})
  list(GET _decl_attachment_death_modes ${_decl_attachment_index} _mode)
  list(GET _decl_attachment_death_diagnostics ${_decl_attachment_index}
    _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  set(_grep_option -Fxc)
  if(_mode STREQUAL "parent-list-disagreement" OR
     _mode STREQUAL "simultaneous-cross-owner-membership")
    set(_grep_option -Ec)
  endif()
  add_test(
    NAME rex_frontend_decl_attachment_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"--$2\" >rex_frontend_decl_attachment_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep \"$5\" -- \"$4\" rex_frontend_decl_attachment_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_project_isolation>
      ${_mode}
      ${_suffix}
      "${_diagnostic}"
      ${_grep_option})
  set_tests_properties(
    rex_frontend_decl_attachment_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;CFE;death")
endforeach()
unset(_decl_attachment_death_modes)
unset(_decl_attachment_death_diagnostics)
unset(_decl_attachment_mode_count)
unset(_decl_attachment_diagnostic_count)
unset(_decl_attachment_last)
unset(_decl_attachment_index)
unset(_grep_option)
unset(_mode)
unset(_diagnostic)
unset(_suffix)

add_test(
  NAME rex_unparser_dirty_header_requires_frontend_ast
  COMMAND rex_unparser_dirty_header_contract
)
set_tests_properties(rex_unparser_dirty_header_requires_frontend_ast
  PROPERTIES LABELS "rex;UNPARSER;UNPARSEHEADERTEST")

add_test(
  NAME rex_unparser_malformed_nonreal_template_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" >rex_unparser_malformed_nonreal_template.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_AST_INVARIANT[nonreal-template-name]: base=rex_malformed semantic=rex_malformed is not a complete template-id' rex_unparser_malformed_nonreal_template.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_malformed_nonreal_template>
)
set_tests_properties(rex_unparser_malformed_nonreal_template_is_rejected
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_ast_alias_without_provenance_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" >rex_ast_alias_provenance.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[alias-provenance\\]: alias=0x[[:xdigit:]]+ name=rex_alias_without_cause target=0x[[:xdigit:]]+ has no construction-time causal node$' rex_ast_alias_provenance.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_ast_alias_provenance>
)
set_tests_properties(rex_ast_alias_without_provenance_is_rejected
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_unparser_unresolved_variable_reference_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" >rex_unparser_unresolved_variable_reference.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[variable-reference\\]: name=rex_missing_variable scope=0x[[:xdigit:]]+ has no variable symbol$' rex_unparser_unresolved_variable_reference.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_unresolved_variable_reference>
)
set_tests_properties(rex_unparser_unresolved_variable_reference_is_rejected
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_unparser_unresolved_named_type_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" >rex_unparser_unresolved_named_type.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc \"[REX-AST-UNRESOLVED-NAMED-TYPE] no exact declaration for 'rex_missing_type' in the current lexical context\" rex_unparser_unresolved_named_type.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_unresolved_named_type>
)
set_tests_properties(rex_unparser_unresolved_named_type_is_rejected
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_unparser_null_token_map_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" >rex_unparser_null_token_map.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_AST_INVARIANT[token-subsequence-map]: null map is not permitted' rex_unparser_null_token_map.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_null_token_map>
)
set_tests_properties(rex_unparser_null_token_map_is_rejected
  PROPERTIES LABELS "rex;UNPARSER")

add_test(
  NAME rex_unparser_missing_type_context_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$@\" >rex_unparser_missing_type_context.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_NAME_QUALIFICATION_INVARIANT\\[standalone-type-use\\]: type=0x[[:xdigit:]]+/SgTypedefType lacks an exact emission statement or reference identity$' rex_unparser_missing_type_context.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_missing_type_context>
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_missing_type_context_input.cpp
)
set_tests_properties(rex_unparser_missing_type_context_is_rejected
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING")

add_test(
  NAME rex_unparser_compiler_generated_uses_ast_order
  COMMAND rex_unparser_compiler_generated_order
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output rex_unparser_compiler_generated_order_output.cpp
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_compiler_generated_order_input.cpp
)
set_tests_properties(rex_unparser_compiler_generated_uses_ast_order
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING")
rex_lock_test_for_input(
  rex_unparser_compiler_generated_uses_ast_order
  rex_unparser_compiler_generated_order_input.cpp)

add_test(
  NAME rex_unparser_dispatch_ownership_contract
  COMMAND $<TARGET_FILE:rex_unparser_dispatch_ownership_contract>)
set_tests_properties(rex_unparser_dispatch_ownership_contract
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING")

add_test(
  NAME rex_unparser_invocation_context_contract
  COMMAND $<TARGET_FILE:rex_unparser_invocation_context_contract>
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_invocation_context.cpp)
set_tests_properties(rex_unparser_invocation_context_contract
  PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING")
rex_lock_test_for_input(rex_unparser_invocation_context_contract
  rex_unparser_invocation_context.cpp)

set(_invocation_context_modes
  detached-context-sensitive
  conflicting-source-candidates
  detached-null-absence
  detached-template-argument)
set(_invocation_context_diagnostics
  "REX_UNPARSE_INVARIANT[unparse-to-string-context]: input=SgVarRefExp is context-sensitive and has no exact source-file context"
  "REX_UNPARSE_INVARIANT[unparse-to-string-context]: candidate=current-source-file file=rex_unparser_conflicting_context.cpp conflicts with candidate=node file=${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_invocation_context.cpp"
  "REX_UNPARSE_INVARIANT[unparse-to-string-null-expression]: typed null role=1 has no standalone expression spelling"
  "REX_UNPARSE_INVARIANT[unparse-to-string-list-context]: detached template argument is not context-free")
list(LENGTH _invocation_context_modes _invocation_context_mode_count)
list(LENGTH _invocation_context_diagnostics
  _invocation_context_diagnostic_count)
if(NOT _invocation_context_mode_count EQUAL
   _invocation_context_diagnostic_count)
  message(FATAL_ERROR
    "Invocation-context hard-contract test tables have different lengths")
endif()
math(EXPR _invocation_context_last "${_invocation_context_mode_count} - 1")
foreach(_invocation_context_index RANGE ${_invocation_context_last})
  list(GET _invocation_context_modes ${_invocation_context_index} _mode)
  list(GET _invocation_context_diagnostics ${_invocation_context_index}
    _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_unparser_invocation_context_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"--$2\" \"$3\" >rex_unparser_invocation_context_$4.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$5\" rex_unparser_invocation_context_$4.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_invocation_context_contract>
      ${_mode}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_invocation_context.cpp
      ${_suffix}
      ${_diagnostic})
  set_tests_properties(
    rex_unparser_invocation_context_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;death")
  rex_lock_test_for_input(
    rex_unparser_invocation_context_${_suffix}_is_rejected
    rex_unparser_invocation_context.cpp)
endforeach()
unset(_invocation_context_modes)
unset(_invocation_context_diagnostics)
unset(_invocation_context_mode_count)
unset(_invocation_context_diagnostic_count)
unset(_invocation_context_last)
unset(_invocation_context_index)

add_test(
  NAME rex_unparser_included_files_exact_graph_contract
  COMMAND $<TARGET_FILE:rex_unparser_included_files_contract>)
set_tests_properties(rex_unparser_included_files_exact_graph_contract
  PROPERTIES LABELS "rex;UNPARSER;headers")

add_test(
  NAME rex_unparser_included_files_duplicate_occurrence_contract
  COMMAND $<TARGET_FILE:rex_unparser_included_files_contract>
    duplicate-occurrence-positive)
set_tests_properties(rex_unparser_included_files_duplicate_occurrence_contract
  PROPERTIES LABELS "rex;UNPARSER;headers")

set(_included_files_contract_modes
  null-token-map
  query-missing-file-info
  missing-associated-include
  foreign-associated-include
  missing-parent-include
  missing-parent-edge
  missing-source-spelling
  absolute-source-spelling
  unresolved-parent-output
  copied-missing-associated-include
  copied-unresolved-parent-output)
set(_included_files_contract_diagnostics
  "^REX_UNPARSE_INVARIANT\\[header-token-map\\]: token map contains a null source-file key$"
  "^REX_UNPARSE_INVARIANT\\[header-query\\]: inventory=physical-output .* type=SgEmptyDeclaration has no file information$"
  "^REX_UNPARSE_INVARIANT\\[header-include-graph\\]: operation=unparse-output header=.* source=.* has no associated include node$"
  "^REX_UNPARSE_INVARIANT\\[header-include-graph\\]: operation=unparse-output header=.* associated-include=.* owns filename=.* source=.*$"
  "^REX_UNPARSE_INVARIANT\\[header-include-graph\\]: operation=unparse-output header=.* include=.* has no parent include node$"
  "^REX_UNPARSE_INVARIANT\\[header-include-graph\\]: operation=unparse-output header=.* parent=.* contains associated include=.* exactly 0 times instead of once$"
  "^REX_UNPARSE_INVARIANT\\[header-include-graph\\]: operation=unparse-output header=.* include=.* has no exact source spelling$"
  "^REX_UNPARSE_INVARIANT\\[header-include-graph\\]: operation=unparse-output header=.* include=.* has absolute source spelling=.*rex_unparser_exact_leaf.hpp$"
  "^REX_UNPARSE_INVARIANT\\[header-output-graph\\]: header=.*rex_unparser_exact_(leaf|parent)\\.hpp parent=.*rex_unparser_exact_(leaf|parent)\\.hpp has no resolved output\\; every included header requires a complete path to one CLI root$"
  "^REX_UNPARSE_INVARIANT\\[header-include-graph\\]: operation=copy-output header=.*rex_unparser_exact_copy.hpp source=.* has no associated include node$"
  "^REX_UNPARSE_INVARIANT\\[header-output-graph\\]: copied header=.*rex_unparser_exact_copy.hpp parent=.*copy_orphan.hpp has no resolved unparse or copy output$")
list(LENGTH _included_files_contract_modes _included_files_contract_mode_count)
list(LENGTH _included_files_contract_diagnostics
  _included_files_contract_diagnostic_count)
if(NOT _included_files_contract_mode_count EQUAL
   _included_files_contract_diagnostic_count)
  message(FATAL_ERROR
    "Included-files hard-contract test tables have different lengths")
endif()
math(EXPR _included_files_contract_last
  "${_included_files_contract_mode_count} - 1")
foreach(_included_files_contract_index RANGE ${_included_files_contract_last})
  list(GET _included_files_contract_modes ${_included_files_contract_index}
    _mode)
  list(GET _included_files_contract_diagnostics
    ${_included_files_contract_index} _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_unparser_included_files_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_unparser_included_files_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec -- \"$4\" rex_unparser_included_files_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_included_files_contract>
      ${_mode}
      ${_suffix}
      ${_diagnostic})
  set_tests_properties(rex_unparser_included_files_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;headers;death")
endforeach()
unset(_included_files_contract_modes)
unset(_included_files_contract_diagnostics)
unset(_included_files_contract_mode_count)
unset(_included_files_contract_diagnostic_count)
unset(_included_files_contract_last)
unset(_included_files_contract_index)
unset(_mode)
unset(_diagnostic)
unset(_suffix)

set(_dispatch_ownership_modes
  symbol
  function-symbol
  initialized-name
  openacc-located-support
  missing-local-symbol
  wrong-local-symbol
  compiler-generated-output-owner
  compiler-generated-hidden-lexical-owner
  direct-skipped-file
  standalone-auxiliary-statement)
set(_dispatch_ownership_diagnostics
  "REX_UNPARSE_INVARIANT[unparse-to-string-dispatch]: node=SgVariableSymbol category=symbol has no typed standalone emitter"
  "REX_UNPARSE_INVARIANT[unparse-to-string-dispatch]: node=SgFunctionSymbol category=symbol has no typed standalone emitter"
  "REX_UNPARSE_INVARIANT[unparse-to-string-dispatch]: node=SgInitializedName category=initialized-name has no typed standalone emitter"
  "REX_UNPARSE_INVARIANT[unparse-to-string-dispatch]: node=SgAccDefaultClause category=unsupported-located-support has no typed standalone emitter"
  "REX_NAME_QUALIFICATION_INVARIANT[initialized-name-symbol]: name=rex_missing_local semantic-scope=SgBasicBlock reason=missing-semantic-symbol has no exact variable identity"
  "REX_NAME_QUALIFICATION_INVARIANT[initialized-name-symbol]: name=rex_wrong_local semantic-scope=SgBasicBlock reason=foreign-semantic-symbol has no exact variable identity"
  "REX_UNPARSE_INVARIANT[physical-file]"
  "REX_UNPARSE_INVARIANT[header-plan]: lexical node=SgNullStatement uses the legacy non-output suppression bit"
  "REX_UNPARSE_INVARIANT[skipped-file-dispatch]: file=rex_unparser_direct_skipped_file.cpp was dispatched directly despite its explicit skip_unparse role"
  "REX_UNPARSE_INVARIANT[standalone-auxiliary-statement]: semantic auxiliary statement has no standalone source spelling")
list(LENGTH _dispatch_ownership_modes _dispatch_ownership_mode_count)
list(LENGTH _dispatch_ownership_diagnostics
  _dispatch_ownership_diagnostic_count)
if(NOT _dispatch_ownership_mode_count EQUAL
   _dispatch_ownership_diagnostic_count)
  message(FATAL_ERROR
    "Dispatcher/ownership hard-contract test tables have different lengths")
endif()
math(EXPR _dispatch_ownership_last "${_dispatch_ownership_mode_count} - 1")
foreach(_dispatch_ownership_index RANGE ${_dispatch_ownership_last})
  list(GET _dispatch_ownership_modes ${_dispatch_ownership_index} _mode)
  list(GET _dispatch_ownership_diagnostics ${_dispatch_ownership_index}
    _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  if(_mode STREQUAL "compiler-generated-output-owner")
    add_test(
      NAME rex_unparser_${_suffix}_is_rejected
      COMMAND
        sh -c
        "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_unparser_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[physical-file\\]: node=SgNullStatement address=0x[[:xdigit:]]+ name=SgNullStatement file-id=-[0-9]+ physical-file-id=-[0-9]+ compiler-generated=1 transformation=0 modified=0 has no explicit physical source ownership$' rex_unparser_$3.log)\" -eq 1"
        dummy
        $<TARGET_FILE:rex_unparser_dispatch_ownership_contract>
        ${_mode}
        ${_suffix})
  elseif(_mode STREQUAL "direct-skipped-file")
    add_test(
      NAME rex_unparser_${_suffix}_is_rejected
      COMMAND
        sh -c
        "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_unparser_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[skipped-file-dispatch\\]: file=(.*/)?rex_unparser_direct_skipped_file\\.cpp was dispatched directly despite its explicit skip_unparse role$' rex_unparser_$3.log)\" -eq 1"
        dummy
        $<TARGET_FILE:rex_unparser_dispatch_ownership_contract>
        ${_mode}
        ${_suffix})
  else()
    add_test(
      NAME rex_unparser_${_suffix}_is_rejected
      COMMAND
        sh -c
        "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_unparser_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$4\" rex_unparser_$3.log)\" -eq 1"
        dummy
        $<TARGET_FILE:rex_unparser_dispatch_ownership_contract>
        ${_mode}
        ${_suffix}
        ${_diagnostic})
  endif()
  set_tests_properties(rex_unparser_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;death")
endforeach()
unset(_dispatch_ownership_modes)
unset(_dispatch_ownership_diagnostics)
unset(_dispatch_ownership_mode_count)
unset(_dispatch_ownership_diagnostic_count)
unset(_dispatch_ownership_last)

add_test(
  NAME rex_unparser_template_and_tag_ownership_contract
  COMMAND $<TARGET_FILE:rex_unparser_template_and_tag_ownership_contract>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -I${TESTCODE_DIR} -c
    ${TESTCODE_DIR}/rex_frontend_explicit_instantiation_identity.cpp
    ${TESTCODE_DIR}/rex_frontend_function_explicit_instantiation_identity.cpp
    ${TESTCODE_DIR}/test2007_38.C
    ${TESTCODE_DIR}/test2018_63.C)
set_tests_properties(rex_unparser_template_and_tag_ownership_contract
  PROPERTIES
    LABELS "rex;UNPARSER;UNPARSETOSTRING"
    RESOURCE_LOCK
      "rose_output_rex_frontend_explicit_instantiation_identity;rose_output_rex_frontend_function_explicit_instantiation_identity;rose_output_test2007_38;rose_output_test2018_63")

set(_template_and_tag_ownership_modes
  empty-template-name
  empty-template-id
  malformed-template-id
  mismatched-template-payload
  for-init-foreign-owner
  for-init-empty-list)
set(_template_and_tag_ownership_diagnostics
  "REX_UNPARSE_INVARIANT[template-instantiation-identity]: template name is empty"
  "REX_UNPARSE_INVARIANT[template-instantiation-identity]: stored declaration name is not a complete template-id rooted at the template name"
  "REX_UNPARSE_INVARIANT[template-instantiation-identity]: stored declaration name is not a complete template-id rooted at the template name"
  "REX_UNPARSE_INVARIANT[template-instantiation-identity]: typed argument list disagrees with the stored template-id payload"
  "REX_UNPARSE_INVARIANT[for-init-owner]: for statement has no exactly owned SgForInitStatement"
  "REX_UNPARSE_INVARIANT[for-init-owner]: for initializer has no typed statement\; syntactic absence requires SgNullStatement")
list(LENGTH _template_and_tag_ownership_modes
  _template_and_tag_ownership_mode_count)
list(LENGTH _template_and_tag_ownership_diagnostics
  _template_and_tag_ownership_diagnostic_count)
if(NOT _template_and_tag_ownership_mode_count EQUAL
   _template_and_tag_ownership_diagnostic_count)
  message(FATAL_ERROR
    "Template/tag ownership hard-contract test tables have different lengths")
endif()
math(EXPR _template_and_tag_ownership_last
  "${_template_and_tag_ownership_mode_count} - 1")
foreach(_template_and_tag_ownership_index
        RANGE ${_template_and_tag_ownership_last})
  list(GET _template_and_tag_ownership_modes
    ${_template_and_tag_ownership_index} _mode)
  list(GET _template_and_tag_ownership_diagnostics
    ${_template_and_tag_ownership_index} _diagnostic)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME rex_unparser_template_and_tag_${_suffix}_is_rejected
    COMMAND
      sh -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_unparser_template_and_tag_$3.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$4\" rex_unparser_template_and_tag_$3.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_template_and_tag_ownership_contract>
      ${_mode}
      ${_suffix}
      "${_diagnostic}")
  set_tests_properties(rex_unparser_template_and_tag_${_suffix}_is_rejected
    PROPERTIES LABELS "rex;UNPARSER;UNPARSETOSTRING;death")
endforeach()
unset(_template_and_tag_ownership_modes)
unset(_template_and_tag_ownership_diagnostics)
unset(_template_and_tag_ownership_mode_count)
unset(_template_and_tag_ownership_diagnostic_count)
unset(_template_and_tag_ownership_last)
unset(_template_and_tag_ownership_index)
unset(_mode)
unset(_diagnostic)
unset(_suffix)
