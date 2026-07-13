add_subdirectory(gfortranTestSuite)
add_subdirectory(experimental_frontend_tests)

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; Fortran_tests require bash")
endif()

add_executable(rex_flang_source_position_contract
  rex_flang_source_position_contract.cpp)
target_link_libraries(rex_flang_source_position_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_flang_directive_provenance_contract
  rex_flang_directive_provenance_contract.cpp)
target_link_libraries(rex_flang_directive_provenance_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_flang_program_declaration_chain
  rex_flang_program_declaration_chain.cpp)
target_link_libraries(rex_flang_program_declaration_chain
  ROSE_DLL ${link_with_libraries})

add_executable(rex_flang_procedure_canonical_ownership
  rex_flang_procedure_canonical_ownership.cpp)
target_link_libraries(rex_flang_procedure_canonical_ownership
  ROSE_DLL ${link_with_libraries})

add_executable(rex_flang_parameter_scope_semantic_provenance
  rex_flang_parameter_scope_semantic_provenance.cpp)
target_link_libraries(rex_flang_parameter_scope_semantic_provenance
  ROSE_DLL ${link_with_libraries})

add_executable(rex_flang_auxiliary_declaration_ownership
  rex_flang_auxiliary_declaration_ownership.cpp)
target_link_libraries(rex_flang_auxiliary_declaration_ownership
  ROSE_DLL ${link_with_libraries})

add_executable(rex_flang_parameter_redeclaration_chain
  rex_flang_parameter_redeclaration_chain.cpp)
target_link_libraries(rex_flang_parameter_redeclaration_chain
  ROSE_DLL ${link_with_libraries})

add_executable(rex_flang_include_lexical_owner
  rex_flang_include_lexical_owner.cpp)
target_link_libraries(rex_flang_include_lexical_owner
  ROSE_DLL ${link_with_libraries})

add_executable(rex_flang_semantic_name_contract
  rex_flang_semantic_name_contract.cpp)

add_executable(rex_flang_function_result_contract
  rex_flang_function_result_contract.cpp)

add_executable(rex_flang_source_type_contract
  rex_flang_source_type_contract.cpp)

add_executable(rex_flang_semantic_regression_contract
  rex_flang_semantic_regression_contract.cpp)
target_link_libraries(rex_flang_semantic_regression_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_fortran_dimension_copy_contract
  rex_fortran_dimension_copy_contract.cpp)
target_link_libraries(rex_fortran_dimension_copy_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_flang_wait_spec_contract
  rex_flang_wait_spec_contract.cpp)
target_link_libraries(rex_flang_wait_spec_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_flang_wait_spec_attachment_contract
  rex_flang_wait_spec_attachment_contract.cpp)

# These internal frontend-contract tests construct Flang parser and semantic
# objects directly.  Model that dependency explicitly instead of exposing
# Flang's implementation headers and libraries through ROSE_DLL's public API.
foreach(_rex_flang_contract_target
    rex_flang_semantic_name_contract
    rex_flang_function_result_contract
    rex_flang_source_type_contract
    rex_flang_wait_spec_attachment_contract)
  rose_force_system_include_dirs(${_rex_flang_contract_target}
    ${FLANG_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS})
  if(DEFINED ROSE_FLANG_ENDIAN_DEFINES)
    target_compile_definitions(${_rex_flang_contract_target} PRIVATE
      ${ROSE_FLANG_ENDIAN_DEFINES})
  endif()
  target_link_libraries(${_rex_flang_contract_target} PRIVATE
    ROSE_DLL
    ${link_with_libraries}
    ${FLANG_LIBS})
endforeach()
unset(_rex_flang_contract_target)

add_test(
  NAME FORTRANTEST_rex_fortran_dimension_copy_contract
  COMMAND $<TARGET_FILE:rex_fortran_dimension_copy_contract>
          -rose:skipfinalCompileStep
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_dimension_copy_contract.f90)
set_tests_properties(
  FORTRANTEST_rex_fortran_dimension_copy_contract
  PROPERTIES LABELS "FORTRANTEST;rex;UNPARSER")

add_test(
  NAME FORTRANTEST_rex_flang_wait_spec_attachment_valid
  COMMAND $<TARGET_FILE:rex_flang_wait_spec_attachment_contract> valid)
set_tests_properties(
  FORTRANTEST_rex_flang_wait_spec_attachment_valid
  PROPERTIES LABELS "FORTRANTEST;rex")

function(add_rex_flang_wait_spec_death_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; set +e; \"\$1\" \"\$2\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Fxc -- \"\$3\" \"\$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_flang_wait_spec_attachment_contract>
      ${mode}
      ${diagnostic})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "FORTRANTEST;rex;death")
endfunction()

add_rex_flang_wait_spec_death_test(
  FORTRANTEST_rex_flang_duplicate_wait_spec_is_rejected
  duplicate
  "REX_FLANG_INVARIANT[wait-spec-duplicate]: WAIT spec 'UNIT' appears more than once")
add_rex_flang_wait_spec_death_test(
  FORTRANTEST_rex_flang_missing_wait_spec_expression_is_rejected
  null-expression
  "REX_FLANG_INVARIANT[wait-spec-expression]: WAIT spec 'UNIT' has no exact expression")
add_rex_flang_wait_spec_death_test(
  FORTRANTEST_rex_flang_owned_wait_spec_expression_is_rejected
  owned-expression
  "REX_FLANG_INVARIANT[wait-spec-expression-owner]: WAIT spec 'UNIT' expression already belongs to SgWaitStatement")
add_rex_flang_wait_spec_death_test(
  FORTRANTEST_rex_flang_missing_wait_spec_statement_is_rejected
  null-statement
  "REX_FLANG_INVARIANT[wait-spec-statement]: WAIT spec 'UNIT' has no exact statement owner")
add_rex_flang_wait_spec_death_test(
  FORTRANTEST_rex_flang_invalid_wait_spec_kind_is_rejected
  invalid-kind
  "REX_FLANG_INVARIANT[wait-spec-kind]: WAIT spec has an invalid typed kind")
add_rex_flang_wait_spec_death_test(
  FORTRANTEST_rex_flang_missing_required_wait_unit_is_rejected
  missing-unit
  "REX_FLANG_INVARIANT[wait-spec-required]: WAIT has no UNIT spec")

add_test(
  NAME FORTRANTEST_rex_flang_source_position_valid
  COMMAND $<TARGET_FILE:rex_flang_source_position_contract> valid-source)
add_test(
  NAME FORTRANTEST_rex_flang_pending_source_position_valid
  COMMAND $<TARGET_FILE:rex_flang_source_position_contract>
          valid-pending-source)
add_test(
  NAME FORTRANTEST_rex_flang_expression_source_position_valid
  COMMAND $<TARGET_FILE:rex_flang_source_position_contract>
          valid-source-expression)
add_test(
  NAME FORTRANTEST_rex_flang_pending_expression_source_position_valid
  COMMAND $<TARGET_FILE:rex_flang_source_position_contract>
          valid-pending-source-expression)
add_test(
  NAME FORTRANTEST_rex_flang_attribute_source_position_valid
  COMMAND $<TARGET_FILE:rex_flang_source_position_contract>
          valid-source-attribute)
add_test(
  NAME FORTRANTEST_rex_flang_generated_source_anchor_valid
  COMMAND $<TARGET_FILE:rex_flang_source_position_contract>
          valid-generated-object)
add_test(
  NAME FORTRANTEST_rex_flang_physical_output_owner_valid
  COMMAND $<TARGET_FILE:rex_flang_source_position_contract>
          valid-physical-output-owner)
set_tests_properties(
  FORTRANTEST_rex_flang_source_position_valid
  FORTRANTEST_rex_flang_pending_source_position_valid
  FORTRANTEST_rex_flang_expression_source_position_valid
  FORTRANTEST_rex_flang_pending_expression_source_position_valid
  FORTRANTEST_rex_flang_attribute_source_position_valid
  FORTRANTEST_rex_flang_generated_source_anchor_valid
  FORTRANTEST_rex_flang_physical_output_owner_valid
  PROPERTIES LABELS "FORTRANTEST;rex")

add_test(
  NAME FORTRANTEST_rex_flang_semantic_name_identity_valid
  COMMAND $<TARGET_FILE:rex_flang_semantic_name_contract> valid-object)
add_test(
  NAME FORTRANTEST_rex_flang_semantic_component_identity_valid
  COMMAND $<TARGET_FILE:rex_flang_semantic_name_contract> valid-component)
add_test(
  NAME FORTRANTEST_rex_flang_shadowed_semantic_name_identity_valid
  COMMAND $<TARGET_FILE:rex_flang_semantic_name_contract>
          valid-shadowed-object)
add_test(
  NAME FORTRANTEST_rex_flang_semantic_object_type_identity_valid
  COMMAND $<TARGET_FILE:rex_flang_semantic_name_contract>
          valid-semantic-type)
add_test(
  NAME FORTRANTEST_rex_flang_function_result_created_identity_valid
  COMMAND $<TARGET_FILE:rex_flang_function_result_contract> valid-created)
add_test(
  NAME FORTRANTEST_rex_flang_function_result_existing_identity_valid
  COMMAND $<TARGET_FILE:rex_flang_function_result_contract> valid-existing)
add_test(
  NAME FORTRANTEST_rex_flang_source_type_explicit_selector_valid
  COMMAND $<TARGET_FILE:rex_flang_source_type_contract> valid-explicit)
add_test(
  NAME FORTRANTEST_rex_flang_source_type_omitted_selector_valid
  COMMAND $<TARGET_FILE:rex_flang_source_type_contract> valid-omitted)
add_test(
  NAME FORTRANTEST_rex_flang_semantic_selector_interning_valid
  COMMAND $<TARGET_FILE:rex_flang_source_type_contract>
          valid-semantic-interning)
add_test(
  NAME FORTRANTEST_rex_flang_recursive_source_type_contract_valid
  COMMAND $<TARGET_FILE:rex_flang_source_type_contract>
          valid-recursive-wrapper)
add_test(
  NAME FORTRANTEST_rex_flang_parameter_attribute_source_type_contract_valid
  COMMAND $<TARGET_FILE:rex_flang_source_type_contract>
          valid-parameter-attribute)
add_test(
  NAME FORTRANTEST_rex_flang_function_argument_identity_contract_valid
  COMMAND $<TARGET_FILE:rex_flang_source_type_contract>
          valid-function-argument-identity)
add_test(
  NAME FORTRANTEST_rex_flang_dynamic_length_result_identity_contract_valid
  COMMAND $<TARGET_FILE:rex_flang_source_type_contract>
          valid-dynamic-length-result-identity)
add_test(
  NAME FORTRANTEST_rex_flang_signed_selector_boundary_valid
  COMMAND $<TARGET_FILE:rex_flang_source_type_contract>
          valid-signed-selector-boundary)
add_test(
  NAME FORTRANTEST_rex_flang_distinct_function_argument_identity_is_rejected
  COMMAND $<TARGET_FILE:rex_flang_source_type_contract>
          distinct-function-argument)
add_test(
  NAME FORTRANTEST_rex_flang_literal_and_sign_contract_valid
  COMMAND $<TARGET_FILE:rex_flang_source_type_contract> valid-literals)
add_test(
  NAME FORTRANTEST_rex_flang_dynamic_character_transaction_valid
  COMMAND $<TARGET_FILE:rex_flang_semantic_name_contract>
          valid-dynamic-character-transaction)
add_test(
  NAME FORTRANTEST_rex_flang_dynamic_character_first_producer_valid
  COMMAND $<TARGET_FILE:rex_flang_semantic_name_contract>
          valid-dynamic-character-first-producer)
set_tests_properties(
  FORTRANTEST_rex_flang_semantic_name_identity_valid
  FORTRANTEST_rex_flang_semantic_component_identity_valid
  FORTRANTEST_rex_flang_shadowed_semantic_name_identity_valid
  FORTRANTEST_rex_flang_semantic_object_type_identity_valid
  FORTRANTEST_rex_flang_function_result_created_identity_valid
  FORTRANTEST_rex_flang_function_result_existing_identity_valid
  FORTRANTEST_rex_flang_source_type_explicit_selector_valid
  FORTRANTEST_rex_flang_source_type_omitted_selector_valid
  FORTRANTEST_rex_flang_semantic_selector_interning_valid
  FORTRANTEST_rex_flang_recursive_source_type_contract_valid
  FORTRANTEST_rex_flang_parameter_attribute_source_type_contract_valid
  FORTRANTEST_rex_flang_function_argument_identity_contract_valid
  FORTRANTEST_rex_flang_dynamic_length_result_identity_contract_valid
  FORTRANTEST_rex_flang_signed_selector_boundary_valid
  FORTRANTEST_rex_flang_distinct_function_argument_identity_is_rejected
  FORTRANTEST_rex_flang_literal_and_sign_contract_valid
  FORTRANTEST_rex_flang_dynamic_character_transaction_valid
  FORTRANTEST_rex_flang_dynamic_character_first_producer_valid
  PROPERTIES LABELS "FORTRANTEST;rex")

function(add_rex_flang_source_type_death_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; set +e; \"\$1\" \"\$2\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Fxc -- \"\$3\" \"\$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_flang_source_type_contract>
      ${mode}
      ${diagnostic})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "FORTRANTEST;rex;death")
endfunction()

function(add_rex_flang_source_type_death_regex_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; set +e; \"\$1\" \"\$2\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec -- \"\$3\" \"\$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_flang_source_type_contract>
      ${mode}
      ${diagnostic})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "FORTRANTEST;rex;death")
endfunction()

add_rex_flang_source_type_death_regex_test(
  FORTRANTEST_rex_flang_mismatched_source_kind_is_rejected
  mismatched-kind
  "^REX_FLANG_INVARIANT\\[explicit-selector-contract\\]: source KIND/LEN selectors of explicitly declared entity 'selector_probe' do not resolve to its exact Flang semantic type \\(source=SgTypeInt@0x[[:xdigit:]]+, semantic=SgTypeInt@0x[[:xdigit:]]+\\)$")
add_rex_flang_source_type_death_regex_test(
  FORTRANTEST_rex_flang_missing_source_type_identity_is_rejected
  missing-source-identity
  "^REX_FLANG_INVARIANT\\[explicit-selector-contract\\]: source KIND/LEN selectors of explicitly declared entity 'selector_probe' do not resolve to its exact Flang semantic type \\(source=SgTypeInt@0x[[:xdigit:]]+, semantic=SgTypeInt@0x[[:xdigit:]]+\\)$")
add_rex_flang_source_type_death_regex_test(
  FORTRANTEST_rex_flang_mismatched_source_wrapper_is_rejected
  mismatched-wrapper
  "^REX_FLANG_INVARIANT\\[explicit-selector-contract\\]: source KIND/LEN selectors of explicitly declared entity 'selector_probe' do not resolve to its exact Flang semantic type \\(source=SgModifierType@0x[[:xdigit:]]+, semantic=SgModifierType@0x[[:xdigit:]]+\\)$")
add_rex_flang_source_type_death_test(
  FORTRANTEST_rex_flang_invalid_integer_literal_is_rejected
  invalid-integer-literal
  "REX_FLANG_INVARIANT[integer-literal-spelling]: signed integer token '12x' is invalid")
add_rex_flang_source_type_death_test(
  FORTRANTEST_rex_flang_integer_literal_range_is_rejected
  integer-literal-range
  "REX_FLANG_INVARIANT[integer-literal-range]: signed integer '9223372036854775808' exceeds the Sage 64-bit value range")
add_rex_flang_source_type_death_test(
  FORTRANTEST_rex_flang_unsigned_literal_sign_is_rejected
  unsigned-literal-sign
  "REX_FLANG_INVARIANT[unsigned-literal-spelling]: unsigned integer token '-1' is invalid")
add_rex_flang_source_type_death_test(
  FORTRANTEST_rex_flang_kind_param_range_is_rejected
  kind-param-range
  "REX_FLANG_INVARIANT[kind-param-range]: unsigned KIND parameter exceeds the Sage signed 64-bit value range")
add_rex_flang_source_type_death_test(
  FORTRANTEST_rex_flang_kind_star_range_is_rejected
  kind-star-range
  "REX_FLANG_INVARIANT[source-kind-star-selector]: source selector value exceeds signed 64-bit metadata range")
add_rex_flang_source_type_death_test(
  FORTRANTEST_rex_flang_character_length_range_is_rejected
  character-length-range
  "REX_FLANG_INVARIANT[source-character-length-selector]: source selector value exceeds signed 64-bit metadata range")
add_rex_flang_source_type_death_regex_test(
  FORTRANTEST_rex_flang_distinct_dynamic_length_result_type_is_rejected
  distinct-dynamic-length-result-type
  "^REX_FLANG_INVARIANT\\[explicit-selector-contract\\]: source KIND/LEN selectors of explicitly declared entity 'selector_probe' do not resolve to its exact Flang semantic type \\(source=SgTypeString@0x[[:xdigit:]]+, semantic=SgTypeString@0x[[:xdigit:]]+\\)$")
add_rex_flang_source_type_death_test(
  FORTRANTEST_rex_flang_component_length_missing_containment_is_rejected
  component-length-missing-containment
  "REX_FLANG_INVARIANT[component-character-length-source]: component name has no exact containing statement range")
add_rex_flang_source_type_death_test(
  FORTRANTEST_rex_flang_component_length_partial_position_is_rejected
  component-length-partial-position
  "REX_FLANG_INVARIANT[component-character-length-source]: source length has partial provenance before exact publication")
add_rex_flang_source_type_death_test(
  FORTRANTEST_rex_flang_component_length_foreign_position_owner_is_rejected
  component-length-foreign-position-owner
  "REX_FLANG_INVARIANT[component-character-length-source]: source length positions are not owned by the component LEN expression")
add_rex_flang_source_type_death_test(
  FORTRANTEST_rex_flang_component_length_preclassification_is_rejected
  component-length-preclassified
  "REX_FLANG_INVARIANT[component-character-length-source]: source-less typed component length was preclassified before exact entity-token publication")
add_rex_flang_source_type_death_test(
  FORTRANTEST_rex_flang_missing_signed_real_sign_is_rejected
  missing-signed-real-sign
  "REX_FLANG_INVARIANT[complex-literal-source]: signed real value part has no exact parser sign token")

function(add_rex_flang_semantic_name_death_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; set +e; \"\$1\" \"\$2\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Fxc -- \"\$3\" \"\$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_flang_semantic_name_contract>
      ${mode}
      ${diagnostic})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "FORTRANTEST;rex;death")
endfunction()

add_rex_flang_semantic_name_death_test(
  FORTRANTEST_rex_flang_missing_expression_semantic_symbol_is_rejected
  missing-name-symbol
  "REX_FLANG_INVARIANT[expression-semantic-symbol]: expression name 'semantic_object' has no Flang semantic symbol")
add_rex_flang_semantic_name_death_test(
  FORTRANTEST_rex_flang_missing_expression_semantic_publication_is_rejected
  missing-object-publication
  "REX_FLANG_INVARIANT[expression-semantic-publication]: Flang expression name 'semantic_object' has no exact published Sage symbol")
add_rex_flang_semantic_name_death_test(
  FORTRANTEST_rex_flang_wrong_expression_semantic_kind_is_rejected
  wrong-object-kind
  "REX_FLANG_INVARIANT[expression-semantic-kind]: Flang object 'semantic_object' maps to SgClassSymbol instead of an exact Sage object symbol")
add_rex_flang_semantic_name_death_test(
  FORTRANTEST_rex_flang_missing_component_semantic_publication_is_rejected
  missing-component-publication
  "REX_FLANG_INVARIANT[component-reference-publication]: Flang component 'semantic_object' has no exact published Sage symbol")
add_rex_flang_semantic_name_death_test(
  FORTRANTEST_rex_flang_wrong_component_semantic_owner_is_rejected
  wrong-component-owner
  "REX_FLANG_INVARIANT[component-reference-publication]: Flang component 'semantic_object' does not map to an exact Sage class-member variable symbol")
add_rex_flang_semantic_name_death_test(
  FORTRANTEST_rex_flang_wrong_semantic_object_type_is_rejected
  wrong-semantic-object-type
  "REX_FLANG_INVARIANT[semantic-object-type-mismatch]: Flang object 'semantic_object' does not map to a Sage variable with its exact semantic type")
add_rex_flang_semantic_name_death_test(
  FORTRANTEST_rex_flang_duplicate_dynamic_character_transaction_is_rejected
  duplicate-dynamic-character-transaction
  "REX_FLANG_INVARIANT[dynamic-character-provisional]: semantic object 'semantic_object' acquired more than one provisional dynamic LEN transaction")
add_rex_flang_semantic_name_death_test(
  FORTRANTEST_rex_flang_contradictory_dynamic_character_first_producer_is_rejected
  contradictory-dynamic-character-first-producer
  "REX_FLANG_INVARIANT[dynamic-character-provisional]: semantic object 'semantic_object' reached its defining dynamic CHARACTER declaration in a contradictory first-producer state")
add_rex_flang_semantic_name_death_test(
  FORTRANTEST_rex_flang_stale_dynamic_character_transaction_is_rejected
  stale-dynamic-character-transaction
  "REX_FLANG_INVARIANT[dynamic-character-provisional]: semantic object 'semantic_object' reached a non-dynamic defining declaration with a pending dynamic CHARACTER transaction")

function(add_rex_flang_function_result_death_test
         test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; set +e; \"\$1\" \"\$2\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Fxc -- \"\$3\" \"\$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_flang_function_result_contract>
      ${mode}
      ${diagnostic})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "FORTRANTEST;rex;death")
endfunction()

add_rex_flang_function_result_death_test(
  FORTRANTEST_rex_flang_wrong_function_result_semantic_kind_is_rejected
  wrong-result-kind
  "REX_FLANG_INVARIANT[result-semantic-kind]: result of 'semantic_function' is not an exact Flang object entity")
add_rex_flang_function_result_death_test(
  FORTRANTEST_rex_flang_missing_function_result_semantic_type_is_rejected
  missing-result-type
  "REX_FLANG_INVARIANT[result-semantic-type]: function result 'semantic_result' has no Flang semantic type")
add_rex_flang_function_result_death_test(
  FORTRANTEST_rex_flang_wrong_function_result_parser_name_is_rejected
  wrong-result-name
  "REX_FLANG_INVARIANT[result-semantic-name]: parser result 'spelling_decoy' does not identify exact Flang result 'semantic_result'")
add_rex_flang_function_result_death_test(
  FORTRANTEST_rex_flang_wrong_function_result_publication_kind_is_rejected
  wrong-publication-kind
  "REX_FLANG_INVARIANT[result-semantic-publication]: exact Flang result 'semantic_result' maps to SgClassSymbol instead of a Sage variable symbol")
add_rex_flang_function_result_death_test(
  FORTRANTEST_rex_flang_wrong_function_result_type_is_rejected
  wrong-result-type
  "REX_FLANG_INVARIANT[result-semantic-publication]: exact Flang result 'semantic_result' has malformed Sage declaration, type, or scope ownership")
add_rex_flang_function_result_death_test(
  FORTRANTEST_rex_flang_wrong_function_result_scope_is_rejected
  wrong-result-scope
  "REX_FLANG_INVARIANT[result-semantic-scope]: exact Flang result 'semantic_result' was published in a different Sage scope")

function(add_rex_flang_source_position_death_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; set +e; \"\$1\" \"\$2\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Fxc -- \"\$3\" \"\$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_flang_source_position_contract>
      ${mode}
      ${diagnostic})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "FORTRANTEST;rex;death")
endfunction()

add_rex_flang_source_position_death_test(
  FORTRANTEST_rex_flang_missing_source_position_is_rejected
  missing-start
  "REX_FLANG_INVARIANT[source-position-missing]: SgNullStatement requires both exact source-range endpoints")
add_rex_flang_source_position_death_test(
  FORTRANTEST_rex_flang_partial_source_position_is_rejected
  partial-start
  "REX_FLANG_INVARIANT[source-position-partial]: SgNullStatement has a partially initialized source-range endpoint")
add_rex_flang_source_position_death_test(
  FORTRANTEST_rex_flang_cross_file_source_range_is_rejected
  different-files
  "REX_FLANG_INVARIANT[source-position-file]: SgNullStatement source range does not belong to one exact physical file")
add_rex_flang_source_position_death_test(
  FORTRANTEST_rex_flang_reversed_source_range_is_rejected
  reversed-range
  "REX_FLANG_INVARIANT[source-position-order]: SgNullStatement source range is not a non-empty ordered half-open interval")
add_rex_flang_source_position_death_test(
  FORTRANTEST_rex_flang_preclassified_source_node_is_rejected
  preclassified-statement
  "REX_FLANG_INVARIANT[source-position-state]: SgNullStatement must be fresh or carry one exact typed pending frontend source transaction before publication (input=source-spelled start=classified end=classified operator=not-applicable)")
add_rex_flang_source_position_death_test(
  FORTRANTEST_rex_flang_preclassified_expression_operator_is_rejected
  preclassified-expression-operator
  "REX_FLANG_INVARIANT[source-position-state]: SgIntVal must be fresh or carry one exact typed pending frontend source transaction before publication (input=source-spelled start=null end=null operator=classified)")
add_rex_flang_source_position_death_test(
  FORTRANTEST_rex_flang_generated_source_kind_is_rejected
  wrong-generated-kind
  "REX_FLANG_INVARIANT[generated-source-kind]: SgNullStatement is incompatible with its generated semantic source anchor kind")
add_rex_flang_source_position_death_test(
  FORTRANTEST_rex_flang_missing_physical_output_owner_is_rejected
  missing-output-owner
  "REX_FLANG_INVARIANT[physical-output-owner]: active Fortran source file has missing or ambiguous physical output ownership")
add_rex_flang_source_position_death_test(
  FORTRANTEST_rex_flang_ambiguous_physical_output_owner_is_rejected
  ambiguous-output-owner
  "REX_FLANG_INVARIANT[physical-output-owner]: active Fortran source file has missing or ambiguous physical output ownership")

function(add_rex_flang_directive_provenance_death_test
         test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; set +e; \"\$1\" \"\$2\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Fxc -- \"\$3\" \"\$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_flang_directive_provenance_contract>
      ${mode}
      ${diagnostic})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "FORTRANTEST;rex;death")
endfunction()

add_rex_flang_directive_provenance_death_test(
  FORTRANTEST_rex_flang_missing_directive_provenance_is_rejected
  missing
  "REX_FLANG_INVARIANT[source-stream-provenance]: directive cooked character at zero-based index 7 has no physical mapping in expected source file '/tmp/rex_flang_expected.f90'")
add_rex_flang_directive_provenance_death_test(
  FORTRANTEST_rex_flang_cross_file_directive_provenance_is_rejected
  cross-file
  "REX_FLANG_INVARIANT[source-stream-provenance]: directive cooked character at zero-based index 11 maps to physical source file '/tmp/rex_flang_actual.f90' instead of expected source file '/tmp/rex_flang_expected.f90'")

file(GLOB F77_TESTCODES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.f")
file(GLOB F90_TESTCODES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.f90" "*.F90")
file(GLOB F03_TESTCODES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.f03")
file(GLOB F90_COMMENTS_TESTCODES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  "test2020_comment_*.f90")
list(APPEND F77_TESTCODES
  rex_fortran_fixed_format_string_boundary.f
  rex_flang_labeled_do_header_identity.f)
list(REMOVE_DUPLICATES F77_TESTCODES)
list(SORT F77_TESTCODES)
list(SORT F90_TESTCODES)
list(SORT F03_TESTCODES)
list(SORT F90_COMMENTS_TESTCODES)
list(REMOVE_ITEM F90_TESTCODES
  ${F90_COMMENTS_TESTCODES}
  inputUsingDefinesOnCommandline.F90
  invalid_fortran_error.f90)

# Keep the semantic-regression specimens explicit even though the legacy
# compile matrix still discovers the larger imported corpus with a glob.
set(_rex_flang_intrinsic_module_shadow_input
  rex_flang_intrinsic_module_shadow_identity/rex_flang_intrinsic_module_shadow_identity.f90)
set(_rex_flang_semantic_regression_inputs
  rex_flang_component_character_length_contract.f90
  rex_flang_dynamic_character_pointer_signature.f90
  rex_flang_dynamic_character_result_predeclaration.f90
  rex_flang_function_source_visible_binding.f90
  rex_flang_intrinsic_module_type_identity.f90
  rex_flang_intrinsic_procedure_reexport_identity.f90
  rex_flang_use_overloaded_procedure_identity.f90
  rex_flang_use_procedure_array_source.f90
  rex_test2026_coarray_image_selector.f90
  rex_fortran_custom_implicit_semantic_type.f90
  rex_fortran_defined_intrinsic_operator_array_result.f90
  rex_fortran_function_result_semantic_identity.f90
  rex_fortran_intrinsic_use_duplicate_same_entity.f90
  rex_fortran_semantic_name_and_component_identity.f90
  rex_fortran_statement_function_semantic_identity.f90
  rex_fortran_zero_iteration_implied_do.f90)
list(APPEND F90_TESTCODES ${_rex_flang_semantic_regression_inputs})
list(REMOVE_DUPLICATES F90_TESTCODES)
list(SORT F90_TESTCODES)

set(_fortran_f90_sources
  ${F90_TESTCODES}
  ${F90_COMMENTS_TESTCODES}
  ${_rex_flang_intrinsic_module_shadow_input})
rex_register_compile_source_set(REX_FORTRAN_F77_SOURCES ${F77_TESTCODES})
rex_register_compile_source_set(REX_FORTRAN_F90_SOURCES ${_fortran_f90_sources})
rex_register_compile_source_set(REX_FORTRAN_F03_SOURCES ${F03_TESTCODES})

set(_fortran_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_fortran_output_dir})
set(_fortran_test2010_module_sources
    test2010_78.f90
    test2010_80.f90)
set(_fortran_private_attribute_module_sources
    test2019_private_attribute2.f90
    test2019_private_attribute.f90)
set(_fortran_protected_module_sources
    protected_mod.f03
    protected.f03)
set(_fortran_module_a_b_sources
    module_A_file.f90
    module_B_file.f90)
set(_fortran_test2019_module_sources
    test2019_module_1_file.f90
    test2019_module_2_file.f90)
set(_fortran_mpi_module_sources
    mpi.f90
    mpi_test_01.f90)
set(_fortran_mpi_f08_module_sources
    mpi_f08_types.f03
    mpi_f08_interfaces_test.f03)
set(_fortran_iso_c_binding_module_sources
    test2020_use_iso_c_binding_3.f90
    test2020_use_iso_c_binding_4.f90)
set(_fortran_intrinsic_fixture_module_sources
    ISO_FORTRAN_ENV.f03)
set(_fortran_f2018_frontend_only_sources
    rex_test2026_coarray_image_selector.f90)
rex_register_compile_source_set(REX_FORTRAN_F2018_FRONTEND_ONLY_SOURCES
  ${_fortran_f2018_frontend_only_sources})
set(_fortran_test2010_module_output_dir
    ${CMAKE_CURRENT_BINARY_DIR}/test2010-module-output)
set(_fortran_private_attribute_module_output_dir
    ${CMAKE_CURRENT_BINARY_DIR}/private-attribute-module-output)
set(_fortran_protected_module_output_dir
    ${CMAKE_CURRENT_BINARY_DIR}/protected-module-output)
set(_fortran_module_a_b_output_dir
    ${CMAKE_CURRENT_BINARY_DIR}/module-a-b-output)
set(_fortran_test2019_module_output_dir
    ${CMAKE_CURRENT_BINARY_DIR}/test2019-module-output)
set(_fortran_mpi_module_output_dir
    ${CMAKE_CURRENT_BINARY_DIR}/mpi-module-output)
set(_fortran_mpi_f08_module_output_dir
    ${CMAKE_CURRENT_BINARY_DIR}/mpi-f08-module-output)
set(_fortran_iso_c_binding_module_output_dir
    ${CMAKE_CURRENT_BINARY_DIR}/iso-c-binding-module-output)
set(_fortran_intrinsic_fixture_module_output_dir
    ${CMAKE_CURRENT_BINARY_DIR}/intrinsic-fixture-module-output)
set(_fortran_f2018_frontend_only_output_dir
    ${CMAKE_CURRENT_BINARY_DIR}/f2018-frontend-only-output)
set(_rex_flang_relocated_intrinsic_module_output_dir
    ${CMAKE_CURRENT_BINARY_DIR}/relocated-intrinsic-module-output)
set(_flang_frontend_module_output_isolation_dir
    ${CMAKE_CURRENT_BINARY_DIR}/frontend-module-output-isolation)
file(MAKE_DIRECTORY
     ${_fortran_test2010_module_output_dir}
     ${_fortran_private_attribute_module_output_dir}
     ${_fortran_protected_module_output_dir}
     ${_fortran_module_a_b_output_dir}
     ${_fortran_test2019_module_output_dir}
     ${_fortran_mpi_module_output_dir}
     ${_fortran_mpi_f08_module_output_dir}
     ${_fortran_iso_c_binding_module_output_dir}
     ${_fortran_intrinsic_fixture_module_output_dir}
     ${_fortran_f2018_frontend_only_output_dir}
     ${_rex_flang_relocated_intrinsic_module_output_dir})
configure_file(
  ${FLANG_INTRINSIC_INCLUDE_DIR}/__fortran_builtins.mod
  ${_rex_flang_relocated_intrinsic_module_output_dir}/__fortran_builtins.mod
  COPYONLY)

function(rex_fortran_output_dir_for_source output_variable input_file)
  if(input_file IN_LIST _fortran_test2010_module_sources)
    set(_output_dir ${_fortran_test2010_module_output_dir})
  elseif(input_file IN_LIST _fortran_private_attribute_module_sources)
    set(_output_dir ${_fortran_private_attribute_module_output_dir})
  elseif(input_file IN_LIST _fortran_protected_module_sources)
    set(_output_dir ${_fortran_protected_module_output_dir})
  elseif(input_file IN_LIST _fortran_module_a_b_sources)
    set(_output_dir ${_fortran_module_a_b_output_dir})
  elseif(input_file IN_LIST _fortran_test2019_module_sources)
    set(_output_dir ${_fortran_test2019_module_output_dir})
  elseif(input_file IN_LIST _fortran_mpi_module_sources)
    set(_output_dir ${_fortran_mpi_module_output_dir})
  elseif(input_file IN_LIST _fortran_mpi_f08_module_sources)
    set(_output_dir ${_fortran_mpi_f08_module_output_dir})
  elseif(input_file IN_LIST _fortran_iso_c_binding_module_sources)
    set(_output_dir ${_fortran_iso_c_binding_module_output_dir})
  elseif(input_file IN_LIST _fortran_intrinsic_fixture_module_sources)
    set(_output_dir ${_fortran_intrinsic_fixture_module_output_dir})
  elseif(input_file IN_LIST _fortran_f2018_frontend_only_sources)
    set(_output_dir ${_fortran_f2018_frontend_only_output_dir})
  else()
    set(_source_key ${input_file})
    string(REPLACE "/" "_" _source_key "${_source_key}")
    string(REPLACE "." "_" _source_key "${_source_key}")
    string(REPLACE "-" "_" _source_key "${_source_key}")
    set(_output_dir
        ${CMAKE_CURRENT_BINARY_DIR}/source-output/${_source_key})
  endif()
  set(${output_variable} ${_output_dir} PARENT_SCOPE)
endfunction()

set(_fortran_common_flags
    -rose:verbose 0
    -rose:detect_dangling_pointers 2
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -I${CMAKE_CURRENT_BINARY_DIR})
set(ROSE_FLAGS
    ${_fortran_common_flags}
    -I${_fortran_output_dir}
    -outputdir ${_fortran_output_dir})
string(JOIN " " ROSE_FLAGS_STR ${ROSE_FLAGS})

add_test(
  NAME FORTRANTEST_rex_flang_frontend_module_output_isolation
  COMMAND
    ${CMAKE_COMMAND}
    -DTEST_TRANSLATOR=$<TARGET_FILE:testTranslator>
    -DSPECIMEN=${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_frontend_module_output_isolation.f90
    -DOUTPUT_DIR=${_flang_frontend_module_output_isolation_dir}
    -P
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_frontend_module_output_isolation.cmake)
set_tests_properties(
  FORTRANTEST_rex_flang_frontend_module_output_isolation
  PROPERTIES LABELS "FORTRANTEST;rex;frontend")

add_test(
  NAME FORTRANTEST_rex_removed_relax_syntax_check_option
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_removed_relax_syntax_check_option.log 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc -- 'REX_FRONTEND_INVARIANT[removed-relax-syntax-check]: -rose:relax_syntax_check was removed because Flang semantic validation is mandatory' rex_removed_relax_syntax_check_option.log)\" -eq 1"
    dummy
    $<TARGET_FILE:testTranslator>
    ${ROSE_FLAGS}
    -rose:relax_syntax_check
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_cray_pointer_declaration.f90)
add_test(
  NAME FORTRANTEST_rex_removed_skip_syntax_check_option
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_removed_skip_syntax_check_option.log 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc -- 'REX_FRONTEND_INVARIANT[removed-skip-syntax-check]: -rose:skip_syntax_check was removed because Flang semantic validation is mandatory' rex_removed_skip_syntax_check_option.log)\" -eq 1"
    dummy
    $<TARGET_FILE:testTranslator>
    ${ROSE_FLAGS}
    -rose:skip_syntax_check
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_cray_pointer_declaration.f90)
add_test(
  NAME FORTRANTEST_rex_removed_fcray_pointer_option
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_removed_fcray_pointer_option.log 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc -- 'REX_FRONTEND_INVARIANT[removed-cray-pointer-option]: -fcray-pointer is not a supported Flang option; use -rose:cray_pointer_support explicitly' rex_removed_fcray_pointer_option.log)\" -eq 1"
    dummy
    $<TARGET_FILE:testTranslator>
    ${ROSE_FLAGS}
    -rose:skipfinalCompileStep
    -fcray-pointer
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_cray_pointer_declaration.f90)
add_test(
  NAME FORTRANTEST_rex_unsupported_fno_cray_pointer_option
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_unsupported_fno_cray_pointer_option.log 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc -- 'REX_FRONTEND_INVARIANT[unsupported-cray-pointer-option]: -fno-cray-pointer is not supported by the pinned Flang frontend' rex_unsupported_fno_cray_pointer_option.log)\" -eq 1"
    dummy
    $<TARGET_FILE:testTranslator>
    ${ROSE_FLAGS}
    -rose:skipfinalCompileStep
    -fno-cray-pointer
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_cray_pointer_declaration.f90)
set_tests_properties(
  FORTRANTEST_rex_removed_relax_syntax_check_option
  FORTRANTEST_rex_removed_skip_syntax_check_option
  FORTRANTEST_rex_removed_fcray_pointer_option
  FORTRANTEST_rex_unsupported_fno_cray_pointer_option
  PROPERTIES LABELS "FORTRANTEST;rex;death")

add_test(
  NAME FORTRANTEST_rex_flang_explicit_program_declaration_chain
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_program_declaration_chain>
          ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:fortran_std=f90 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_explicit_program_declaration_chain.f90)
add_test(
  NAME FORTRANTEST_rex_flang_implicit_program_declaration_chain
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_program_declaration_chain>
          ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:fortran_std=f2003 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_implicit_program_declaration_chain.f90)
add_test(
  NAME FORTRANTEST_rex_flang_implicit_program_construct_span
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_program_declaration_chain>
          ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:fortran_std=f2003 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_implicit_program_construct_span.f90)
add_test(
  NAME FORTRANTEST_rex_flang_implicit_program_execution_span
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_program_declaration_chain>
          ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:fortran_std=f2003 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_implicit_program_execution_span.f90)
add_test(
  NAME FORTRANTEST_rex_flang_implicit_program_empty_span
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_program_declaration_chain>
          ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:fortran_std=f2003 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_implicit_program_empty_span.f90)
add_test(
  NAME FORTRANTEST_rex_flang_procedure_canonical_ownership
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_procedure_canonical_ownership>
          ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:fortran_std=f90 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_procedure_canonical_ownership.f90)
add_test(
  NAME FORTRANTEST_rex_flang_parameter_scope_semantic_provenance
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_parameter_scope_semantic_provenance>
          ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:fortran_std=f90 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_procedure_canonical_ownership.f90)
add_test(
  NAME FORTRANTEST_rex_flang_auxiliary_declaration_ownership
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_auxiliary_declaration_ownership>
          ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:fortran_std=f2003 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_intrinsic_use_derived_type_publication.f90)
add_test(
  NAME FORTRANTEST_rex_flang_parameter_redeclaration_chain
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_parameter_redeclaration_chain>
          ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:fortran_std=f2008 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_parameter_redeclaration_chain.f90)
add_test(
  NAME FORTRANTEST_rex_flang_include_lexical_owner
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_include_lexical_owner>
          ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:fortran_std=f90 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_include_lexical_owner.f90)
add_test(
  NAME FORTRANTEST_rex_flang_wait_spec_contract
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_wait_spec_contract>
          ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:fortran_std=f2003 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_wait_spec_contract.f90)
rex_fortran_output_dir_for_source(
  _wait_spec_output_dir rex_fortran_wait_spec_contract.f90)
add_test(
  NAME FORTRANTEST_rex_flang_wait_spec_unparse_contract
  WORKING_DIRECTORY ${_wait_spec_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "file=rose_rex_fortran_wait_spec_contract.f90; test -f \"\$file\"; test \"\$(grep -Eic '^[[:space:]]*WAIT[[:space:]]*[(]' \"\$file\")\" -eq 1; grep -Eiq 'UNIT[[:space:]]*=[[:space:]]*unit_number' \"\$file\"; grep -Eiq 'END[[:space:]]*=[[:space:]]*100' \"\$file\"; grep -Eiq 'EOR[[:space:]]*=[[:space:]]*200' \"\$file\"; grep -Eiq 'ERR[[:space:]]*=[[:space:]]*300' \"\$file\"; grep -Eiq 'ID[[:space:]]*=[[:space:]]*request_id' \"\$file\"; grep -Eiq 'IOMSG[[:space:]]*=[[:space:]]*message' \"\$file\"; grep -Eiq 'IOSTAT[[:space:]]*=[[:space:]]*status_code' \"\$file\"")
set_tests_properties(
  FORTRANTEST_rex_flang_wait_spec_unparse_contract
  PROPERTIES
    FIXTURES_REQUIRED rex_fortran_wait_spec_output
    LABELS "FORTRANTEST;rex;UNPARSER")
add_test(
  NAME FORTRANTEST_rex_variable_redeclaration_cycle_is_rejected
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=rex_variable_redeclaration_cycle.log; rm -f \"\$log\"; set +e; \"\$1\" --cycle >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[variable-redeclaration-chain\\]: declaration=0x[[:xdigit:]]+ name=rex_cycle contains a cycle$' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_flang_parameter_redeclaration_chain>)
set_tests_properties(
  FORTRANTEST_rex_flang_explicit_program_declaration_chain
  FORTRANTEST_rex_flang_implicit_program_declaration_chain
  FORTRANTEST_rex_flang_implicit_program_construct_span
  FORTRANTEST_rex_flang_implicit_program_execution_span
  FORTRANTEST_rex_flang_implicit_program_empty_span
  FORTRANTEST_rex_flang_procedure_canonical_ownership
  FORTRANTEST_rex_flang_parameter_scope_semantic_provenance
  FORTRANTEST_rex_flang_auxiliary_declaration_ownership
  FORTRANTEST_rex_flang_parameter_redeclaration_chain
  FORTRANTEST_rex_flang_include_lexical_owner
  FORTRANTEST_rex_flang_wait_spec_contract
  PROPERTIES LABELS "FORTRANTEST;rex")
set_tests_properties(
  FORTRANTEST_rex_variable_redeclaration_cycle_is_rejected
  PROPERTIES LABELS "FORTRANTEST;rex;death")

function(fortran_test input_file)
  set(_fortran_flags ${_fortran_common_flags} ${FORTRAN_FLAGS} ${ARGN})
  rex_fortran_output_dir_for_source(
    _fortran_test_output_dir ${input_file})
  file(MAKE_DIRECTORY ${_fortran_test_output_dir})
  list(APPEND _fortran_flags
       -I${_fortran_test_output_dir}
       -outputdir ${_fortran_test_output_dir})
  add_test(
    NAME FORTRANTEST_${input_file}_translation
    WORKING_DIRECTORY ${_fortran_test_output_dir}
    COMMAND testTranslator ${_fortran_flags}
            -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
  add_test(
    NAME FORTRANTEST_${input_file}_graph_generation
    WORKING_DIRECTORY ${_fortran_test_output_dir}
    COMMAND testGraphGeneration ${_fortran_flags}
            -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
  add_test(
    NAME FORTRANTEST_${input_file}_token_generation
    WORKING_DIRECTORY ${_fortran_test_output_dir}
    COMMAND testTokenGeneration ${_fortran_flags}
            -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
  set_tests_properties(FORTRANTEST_${input_file}_translation PROPERTIES LABELS FORTRANTEST)
  set_tests_properties(FORTRANTEST_${input_file}_graph_generation PROPERTIES DEPENDS FORTRANTEST_${input_file}_translation LABELS FORTRANTEST)
  set_tests_properties(FORTRANTEST_${input_file}_token_generation PROPERTIES DEPENDS FORTRANTEST_${input_file}_graph_generation LABELS FORTRANTEST)
endfunction()

function(fortran_frontend_negative_diagnostic
         input_file output_variable output_count_variable)
  set(_diagnostic_count 1)
  if(input_file STREQUAL "test1956_01.f")
    set(_diagnostic "expected ')'")
  elseif(input_file STREQUAL "test2007_146.f")
    set(_diagnostic "Unmatched '('")
  elseif(input_file STREQUAL "triangle-fixed.f")
    set(_diagnostic "expected '('")
  elseif(input_file STREQUAL "rex_test2026_cuda_kernel_launch_call.f90")
    set(_diagnostic "expected 'END'")
  elseif(input_file STREQUAL "rex_test2026_allocate_specs.f90")
    set(_diagnostic "expected '='")
  elseif(input_file STREQUAL "test2007_88.f90")
    set(_diagnostic "expected end of statement")
  elseif(input_file STREQUAL "test2007_152.f90")
    set(_diagnostic "end of file")
  elseif(input_file STREQUAL "test2007_177.f90")
    set(_diagnostic "expected end of statement")
  elseif(input_file STREQUAL "test2008_61.f90")
    set(_diagnostic "'print_char' is already declared in this scoping unit")
  elseif(input_file STREQUAL "test2007_84.f90")
    set(_diagnostic "No explicit type declared for 'subpte'")
  elseif(input_file STREQUAL "test2010_43.f90")
    set(_diagnostic "subscript 0 is less than lower bound 1 for dimension 1 of array")
  elseif(input_file STREQUAL "test2010_19.f90")
    set(_diagnostic "expected end of statement")
  elseif(input_file STREQUAL "test2010_26.f90")
    set(_diagnostic "end of file")
  elseif(input_file STREQUAL "test2010_96.f90")
    set(_diagnostic "'p' must be either an accessible module procedure or an external procedure with an explicit interface")
  elseif(input_file STREQUAL "test2010_99.f90")
    set(_diagnostic "Binding name 'plus' not found in this derived type")
  elseif(input_file STREQUAL "test2010_101.f90")
    set(_diagnostic "Binding name 'q' not found in this derived type")
  elseif(input_file STREQUAL "test2010_102.f90")
    set(_diagnostic "expected end of statement")
  elseif(input_file STREQUAL "test2010_110.f90")
    set(_diagnostic "The dimensions of 'i' have already been declared")
  elseif(input_file STREQUAL "test2010_175.f90")
    set(_diagnostic "expected end of statement")
    set(_diagnostic_count 2)
  elseif(input_file STREQUAL "test2011_38.f90")
    set(_diagnostic "end of file")
  elseif(input_file STREQUAL "test2011_59.f90")
    set(_diagnostic "end of file")
  elseif(input_file STREQUAL "test2011_67.f90")
    set(_diagnostic "DO construct name required but missing")
  elseif(input_file STREQUAL "test2007_42.f03" OR
         input_file STREQUAL "test2011_Rice_bindc.f03" OR
         input_file STREQUAL "test2011_Rice_bindc-name-2.f03" OR
         input_file STREQUAL "test2011_Rice_bindc-name.f03")
    set(_diagnostic "A variable with BIND(C) attribute may only appear in the specification part of a module")
  elseif(input_file STREQUAL "test2007_112.f03")
    set(_diagnostic "A variable with BIND(C) attribute may only appear in the specification part of a module")
    set(_diagnostic_count 2)
  elseif(input_file STREQUAL "test2007_114.f03" OR
         input_file STREQUAL "test2010_171.f03")
    set(_diagnostic "'sharedcommon' appears as a COMMON block in a BIND statement but not in a COMMON statement")
  elseif(input_file STREQUAL "test2007_115.f03")
    set(_diagnostic "Must be a constant value")
  elseif(input_file STREQUAL "test2007_116.f03")
    set(_diagnostic "Must be a constant value")
  elseif(input_file STREQUAL "test2007_180.f03")
    set(_diagnostic "References to the procedure 'test' require an explicit interface")
  elseif(input_file STREQUAL "ISO_C_BINDING.f03")
    set(_diagnostic "Derived type 'c_ptr' not found")
    set(_diagnostic_count 4)
  elseif(input_file STREQUAL "test2007_134.f03")
    set(_diagnostic "Keyword 'shift=' may not appear in a reference to a procedure with an implicit interface")
  elseif(input_file STREQUAL "test2008_55.f03")
    set(_diagnostic "'real_not' is not a module procedure")
  elseif(input_file STREQUAL "test2010_76.f03")
    set(_diagnostic "Derived type 'cfi_dim_t' not found")
  elseif(input_file STREQUAL "test2010_104.f03")
    set(_diagnostic "'p' must be either an accessible module procedure or an external procedure with an explicit interface")
  elseif(input_file STREQUAL "test2007_171.f03")
    set(_diagnostic "expected end of statement")
  elseif(input_file STREQUAL "test2010_27.f03")
    set(_diagnostic "expected end of statement")
  elseif(input_file STREQUAL "test2010_74.f03")
    set(_diagnostic "end of file")
  else()
    message(FATAL_ERROR
      "No exact frontend-negative diagnostic registered for ${input_file}")
  endif()
  set(${output_variable} "${_diagnostic}" PARENT_SCOPE)
  set(${output_count_variable} "${_diagnostic_count}" PARENT_SCOPE)
endfunction()

function(fortran_frontend_negative_test input_file)
  set(_fortran_flags ${FORTRAN_FLAGS} ${ARGN})
  fortran_frontend_negative_diagnostic(
    ${input_file} _expected_diagnostic _expected_diagnostic_count)
  get_filename_component(_source_stem ${input_file} NAME_WE)
  rex_add_flang_frontend_negative_test(
    FORTRANTEST_${input_file}_frontend_negative
    FORTRANTEST
    ${_fortran_output_dir}
    ${input_file}.frontend-negative.log
    ${_source_stem}
    "${_expected_diagnostic}"
    ${_expected_diagnostic_count}
    $<TARGET_FILE:testTranslator>
    ${ROSE_FLAGS}
    ${_fortran_flags}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
endfunction()

set(FORTRAN_FLAGS -rose:fortran_std=f77)
set(_fortran_f77_frontend_negative_tests
  test1956_01.f
  test2007_146.f
  triangle-fixed.f
)
rex_register_compile_source_set(REX_FORTRAN_F77_NEGATIVE_SOURCES
  ${_fortran_f77_frontend_negative_tests})
foreach(F77_test ${F77_TESTCODES})
  if(F77_test IN_LIST _fortran_f77_frontend_negative_tests)
    fortran_frontend_negative_test(${F77_test})
  else()
    fortran_test(${F77_test})
  endif()
endforeach()

set(FORTRAN_FLAGS -rose:fortran_std=f90)
set(_fortran_cray_pointer_tests
  test2007_81.f90
  test2007_82.f90
  test2007_83.f90
  rex_fortran_cray_pointer_declaration.f90
  test2011_imperial_crayPointers.f90
)
set(_fortran_frontend_negative_tests
  # Standalone parser/legacy regression specimens below intentionally omit
  # declarations or violate semantic constraints.  They must hard-fail the
  # Flang semantic frontend; never route them through a syntax-skip mode.
  rex_test2026_cuda_kernel_launch_call.f90
  rex_test2026_allocate_specs.f90
  test2007_84.f90
  test2007_88.f90
  test2007_152.f90
  test2007_177.f90
  test2008_61.f90
  test2010_43.f90
  test2010_19.f90
  test2010_26.f90
  test2010_96.f90
  test2010_99.f90
  test2010_101.f90
  test2010_102.f90
  test2010_110.f90
  test2010_175.f90
  test2011_38.f90
  test2011_59.f90
  test2011_67.f90
)
rex_register_compile_source_set(REX_FORTRAN_F90_NEGATIVE_SOURCES
  ${_fortran_frontend_negative_tests})
foreach(F90_test ${F90_TESTCODES})
  if(F90_test IN_LIST _fortran_frontend_negative_tests)
    fortran_frontend_negative_test(${F90_test})
  elseif(F90_test IN_LIST _fortran_f2018_frontend_only_sources)
    set(_saved_fortran_flags ${FORTRAN_FLAGS})
    set(FORTRAN_FLAGS
        -rose:skipfinalCompileStep
        -rose:fortran_std=f2018)
    fortran_test(${F90_test})
    set(FORTRAN_FLAGS ${_saved_fortran_flags})
    unset(_saved_fortran_flags)
  elseif(F90_test IN_LIST _fortran_cray_pointer_tests)
    fortran_test(${F90_test} -rose:cray_pointer_support)
  else()
    fortran_test(${F90_test})
  endif()
endforeach()

set(_rex_flang_intrinsic_module_shadow_output_dir
  ${CMAKE_CURRENT_BINARY_DIR}/rex-flang-intrinsic-module-shadow)
file(MAKE_DIRECTORY ${_rex_flang_intrinsic_module_shadow_output_dir})
set(_rex_flang_intrinsic_module_shadow_flags
  -rose:verbose 0
  -rose:detect_dangling_pointers 2
  -I${CMAKE_CURRENT_SOURCE_DIR}
  -I${CMAKE_CURRENT_BINARY_DIR}
  -I${_rex_flang_intrinsic_module_shadow_output_dir}
  -outputdir ${_rex_flang_intrinsic_module_shadow_output_dir}
  -rose:fortran_std=f90)
set(_rex_flang_intrinsic_module_shadow_source
  ${CMAKE_CURRENT_SOURCE_DIR}/${_rex_flang_intrinsic_module_shadow_input})
add_test(
  NAME FORTRANTEST_rex_flang_intrinsic_module_shadow_identity.f90_translation
  WORKING_DIRECTORY ${_rex_flang_intrinsic_module_shadow_output_dir}
  COMMAND testTranslator
    ${_rex_flang_intrinsic_module_shadow_flags}
    -c ${_rex_flang_intrinsic_module_shadow_source})
add_test(
  NAME FORTRANTEST_rex_flang_intrinsic_module_shadow_identity.f90_graph_generation
  WORKING_DIRECTORY ${_rex_flang_intrinsic_module_shadow_output_dir}
  COMMAND testGraphGeneration
    ${_rex_flang_intrinsic_module_shadow_flags}
    -c ${_rex_flang_intrinsic_module_shadow_source})
add_test(
  NAME FORTRANTEST_rex_flang_intrinsic_module_shadow_identity.f90_token_generation
  WORKING_DIRECTORY ${_rex_flang_intrinsic_module_shadow_output_dir}
  COMMAND testTokenGeneration
    ${_rex_flang_intrinsic_module_shadow_flags}
    -c ${_rex_flang_intrinsic_module_shadow_source})
set_tests_properties(
  FORTRANTEST_rex_flang_intrinsic_module_shadow_identity.f90_translation
  PROPERTIES LABELS FORTRANTEST)
set_tests_properties(
  FORTRANTEST_rex_flang_intrinsic_module_shadow_identity.f90_graph_generation
  PROPERTIES
    DEPENDS
      FORTRANTEST_rex_flang_intrinsic_module_shadow_identity.f90_translation
    LABELS FORTRANTEST)
set_tests_properties(
  FORTRANTEST_rex_flang_intrinsic_module_shadow_identity.f90_token_generation
  PROPERTIES
    DEPENDS
      FORTRANTEST_rex_flang_intrinsic_module_shadow_identity.f90_graph_generation
    LABELS FORTRANTEST)

rex_fortran_output_dir_for_source(
  _character_literal_output_dir
  rex_flang_character_literal_decoded_value.f90)
add_test(
  NAME FORTRANTEST_rex_flang_character_literal_decoded_value_unparse
  WORKING_DIRECTORY ${_character_literal_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "file=rose_rex_flang_character_literal_decoded_value.f90; test -f \"\$file\"; test \"\$(grep -Fc \"apostrophe = 'isn''t'\" \"\$file\")\" -eq 1; test \"\$(grep -Fc \"quotation = 'say \\\"yes\\\"'\" \"\$file\")\" -eq 1")
set_tests_properties(
  FORTRANTEST_rex_flang_character_literal_decoded_value_unparse
  PROPERTIES
    DEPENDS
      FORTRANTEST_rex_flang_character_literal_decoded_value.f90_translation
    LABELS "FORTRANTEST;rex;UNPARSER")

add_test(
  NAME FORTRANTEST_rex_flang_coarray_image_selector_unparse_contract
  WORKING_DIRECTORY ${_fortran_f2018_frontend_only_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "file=rose_rex_test2026_coarray_image_selector.f90; test -f \"\$file\"; test \"\$(grep -Fxc 'USE iso_fortran_env, ONLY : team_type' \"\$file\")\" -eq 1; test \"\$(grep -Eic '^[[:space:]]*a\\[1, STAT = stat\\] = 1$' \"\$file\")\" -eq 1; test \"\$(grep -Eic '^[[:space:]]*a\\[1, STAT = stat, TEAM = team\\] = 2$' \"\$file\")\" -eq 1; test \"\$(grep -Eic '^[[:space:]]*a\\[1, STAT = stat, TEAM_NUMBER = team_number\\] = 3$' \"\$file\")\" -eq 1")
set_tests_properties(
  FORTRANTEST_rex_flang_coarray_image_selector_unparse_contract
  PROPERTIES
    DEPENDS FORTRANTEST_rex_test2026_coarray_image_selector.f90_translation
    LABELS "FORTRANTEST;rex;UNPARSER")

# The dedicated frontend contract verifies semantic identities that a
# text-only round trip cannot observe.
foreach(_input IN LISTS _rex_flang_semantic_regression_inputs)
  get_filename_component(_stem ${_input} NAME_WE)
  rex_fortran_output_dir_for_source(_semantic_output_dir ${_input})
  set(_semantic_standard f2008)
  set(_semantic_flags
      ${_fortran_common_flags}
      -I${_semantic_output_dir}
      -outputdir ${_semantic_output_dir})
  if(_input IN_LIST _fortran_f2018_frontend_only_sources)
    set(_semantic_standard f2018)
  endif()
  add_test(
    NAME FORTRANTEST_${_stem}_semantic_contract
    WORKING_DIRECTORY ${_semantic_output_dir}
    COMMAND $<TARGET_FILE:rex_flang_semantic_regression_contract>
            ${_semantic_flags} -rose:skipfinalCompileStep
            -rose:fortran_std=${_semantic_standard} -c
            ${CMAKE_CURRENT_SOURCE_DIR}/${_input})
  set_tests_properties(
    FORTRANTEST_${_stem}_semantic_contract
    PROPERTIES
      DEPENDS FORTRANTEST_${_input}_translation
      LABELS "FORTRANTEST;rex")
endforeach()
unset(_semantic_output_dir)
unset(_semantic_standard)
unset(_semantic_flags)
add_test(
  NAME FORTRANTEST_rex_flang_intrinsic_module_shadow_identity_semantic_contract
  WORKING_DIRECTORY ${_rex_flang_intrinsic_module_shadow_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_semantic_regression_contract>
    ${_rex_flang_intrinsic_module_shadow_flags}
    -rose:skipfinalCompileStep
    -rose:fortran_std=f2008
    -c ${_rex_flang_intrinsic_module_shadow_source})
set_tests_properties(
  FORTRANTEST_rex_flang_intrinsic_module_shadow_identity_semantic_contract
  PROPERTIES
    DEPENDS
      FORTRANTEST_rex_flang_intrinsic_module_shadow_identity.f90_token_generation
    LABELS "FORTRANTEST;rex")
add_test(
  NAME
    FORTRANTEST_rex_flang_relocated_intrinsic_module_identity_semantic_contract
  WORKING_DIRECTORY ${_rex_flang_relocated_intrinsic_module_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_semantic_regression_contract>
    -rose:verbose 0
    -rose:detect_dangling_pointers 2
    -I${_rex_flang_relocated_intrinsic_module_output_dir}
    -outputdir ${_rex_flang_relocated_intrinsic_module_output_dir}
    -rose:skipfinalCompileStep
    -rose:fortran_std=f90
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_intrinsic_module_type_identity.f90)
set_tests_properties(
  FORTRANTEST_rex_flang_relocated_intrinsic_module_identity_semantic_contract
  PROPERTIES LABELS "FORTRANTEST;rex")
rex_fortran_output_dir_for_source(
  _labeled_do_output_dir rex_flang_labeled_do_header_identity.f)
add_test(
  NAME FORTRANTEST_rex_flang_labeled_do_header_identity_semantic_contract
  WORKING_DIRECTORY ${_labeled_do_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_semantic_regression_contract>
          ${_fortran_common_flags}
          -I${_labeled_do_output_dir}
          -outputdir ${_labeled_do_output_dir}
          -rose:skipfinalCompileStep
          -rose:fortran_std=f77 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_labeled_do_header_identity.f)
set_tests_properties(
  FORTRANTEST_rex_flang_labeled_do_header_identity_semantic_contract
  PROPERTIES
    DEPENDS FORTRANTEST_rex_flang_labeled_do_header_identity.f_translation
    LABELS "FORTRANTEST;rex")
unset(_input)
unset(_stem)
unset(_rex_flang_semantic_regression_inputs)
unset(_rex_flang_intrinsic_module_shadow_flags)
unset(_rex_flang_intrinsic_module_shadow_input)
unset(_rex_flang_intrinsic_module_shadow_output_dir)
unset(_rex_flang_intrinsic_module_shadow_source)

set_tests_properties(
  FORTRANTEST_rex_fortran_wait_spec_contract.f90_translation
  PROPERTIES FIXTURES_SETUP rex_fortran_wait_spec_output)

foreach(F90_comment_test ${F90_COMMENTS_TESTCODES})
  fortran_test(${F90_comment_test})
endforeach()

set(FORTRAN_FLAGS -rose:skipfinalCompileStep -rose:fortran_std=f2003)
set(_fortran_f03_frontend_negative_tests
  # These source fragments exercise parser productions but are not complete,
  # semantically valid Fortran programs.  Flang rejection is the expected
  # result and guards against accidentally accepting malformed AST input.
  test2007_42.f03
  test2007_112.f03
  test2007_114.f03
  test2007_115.f03
  test2007_116.f03
  test2007_134.f03
  test2007_180.f03
  test2008_55.f03
  test2010_76.f03
  test2010_104.f03
  test2010_171.f03
  test2007_171.f03
  test2010_27.f03
  test2010_74.f03
  test2011_Rice_bindc-name-2.f03
  test2011_Rice_bindc-name.f03
  test2011_Rice_bindc.f03
  ISO_C_BINDING.f03
)
rex_register_compile_source_set(REX_FORTRAN_F03_NEGATIVE_SOURCES
  ${_fortran_f03_frontend_negative_tests})
foreach(F03_test ${F03_TESTCODES})
  if(F03_test IN_LIST _fortran_f03_frontend_negative_tests)
    fortran_frontend_negative_test(${F03_test})
  else()
    fortran_test(${F03_test})
  endif()
endforeach()

function(rex_fortran_module_fixture producer consumer fixture_name)
  set(_producer_tests
      FORTRANTEST_${producer}_translation
      FORTRANTEST_${producer}_graph_generation
      FORTRANTEST_${producer}_token_generation)
  set(_consumer_tests
      FORTRANTEST_${consumer}_translation
      FORTRANTEST_${consumer}_graph_generation
      FORTRANTEST_${consumer}_token_generation)
  set_tests_properties(
    ${_producer_tests}
    PROPERTIES
      FIXTURES_SETUP ${fixture_name}
      RESOURCE_LOCK ${fixture_name})
  set_tests_properties(
    ${_consumer_tests}
    PROPERTIES
      FIXTURES_REQUIRED ${fixture_name}
      RESOURCE_LOCK ${fixture_name})

  foreach(_source IN ITEMS ${producer} ${consumer})
    get_filename_component(
      _source_abs ${_source} ABSOLUTE
      BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    file(RELATIVE_PATH _source_from_compile_tests
         ${REX_COMPILE_TESTS_SOURCE_DIR} ${_source_abs})
    list(APPEND _chain_sources ${_source_from_compile_tests})
  endforeach()
  list(GET _chain_sources 0 _producer_from_compile_tests)
  list(GET _chain_sources 1 _consumer_from_compile_tests)
  set_property(
    GLOBAL APPEND PROPERTY REX_FORTRAN_MODULE_CHAINS
    "${_producer_from_compile_tests}|${_consumer_from_compile_tests}|${fixture_name}")
endfunction()

# These sources are producer/consumer programs, not independent compilations.
# Isolated output directories prevent unrelated module files from satisfying a
# USE statement, and fixtures make the exact producers part of subset runs.
rex_fortran_module_fixture(
  test2010_78.f90 test2010_80.f90 rex_fortran_test2010_m178_module)
rex_fortran_module_fixture(
  test2019_private_attribute2.f90 test2019_private_attribute.f90
  rex_fortran_private_attribute2_module)
rex_fortran_module_fixture(
  protected_mod.f03 protected.f03 rex_fortran_protected_module)
rex_fortran_module_fixture(
  module_A_file.f90 module_B_file.f90 rex_fortran_module_a_b)
rex_fortran_module_fixture(
  test2019_module_1_file.f90 test2019_module_2_file.f90
  rex_fortran_test2019_module)
rex_fortran_module_fixture(
  mpi.f90 mpi_test_01.f90 rex_fortran_mpi_module)
rex_fortran_module_fixture(
  mpi_f08_types.f03 mpi_f08_interfaces_test.f03
  rex_fortran_mpi_f08_module)
rex_fortran_module_fixture(
  test2020_use_iso_c_binding_3.f90
  test2020_use_iso_c_binding_4.f90
  rex_fortran_test2020_iso_c_binding_module)

# Extra tests carried over from the old Automake-only rules.
add_test(
  NAME fortran_enforce_module_name_policy
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/policies/UniqueFortranModules.pl
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(
  NAME fortran_testCPP_Defines
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND testTranslator ${ROSE_FLAGS} -DPS=8
    ${CMAKE_CURRENT_SOURCE_DIR}/inputUsingDefinesOnCommandline.F90
)

set(_fortran_mpi_type_output_dir
  ${CMAKE_CURRENT_BINARY_DIR}/mpi-f08-specialized-output)
file(MAKE_DIRECTORY ${_fortran_mpi_type_output_dir})
set(_fortran_mpi_type_flags
  ${_fortran_common_flags}
  -I${_fortran_mpi_type_output_dir}
  -outputdir ${_fortran_mpi_type_output_dir})
set(_fortran_mpi_type_tests
  mpi_f08_types.f03
  mpi_f08_interfaces_test.f03
)
foreach(testcode IN LISTS _fortran_mpi_type_tests)
  string(REPLACE "." "_" _name ${testcode})
  add_test(
    NAME fortran_testMPItypes_${_name}
    WORKING_DIRECTORY ${_fortran_mpi_type_output_dir}
    COMMAND testTranslator ${_fortran_mpi_type_flags}
      -rose:skipfinalCompileStep
      ${CMAKE_CURRENT_SOURCE_DIR}/${testcode}
  )
endforeach()

# This separate driver-level chain validates multi-file handling without
# sharing module artifacts with the generic translation/graph/token tests.
set_tests_properties(fortran_testMPItypes_mpi_f08_interfaces_test_f03
  PROPERTIES DEPENDS fortran_testMPItypes_mpi_f08_types_f03)

set(_fortran_intrinsic_module_isolation_dir
  ${CMAKE_CURRENT_BINARY_DIR}/intrinsic-module-isolation)
file(MAKE_DIRECTORY ${_fortran_intrinsic_module_isolation_dir})
set(_fortran_intrinsic_module_flags
  -rose:verbose 0
  -rose:detect_dangling_pointers 2
  -rose:skipfinalCompileStep
  -rose:fortran_std=f2003
  -I${CMAKE_CURRENT_SOURCE_DIR}
  -I${CMAKE_CURRENT_BINARY_DIR}
  -I${_fortran_intrinsic_module_isolation_dir}
  -outputdir ${_fortran_intrinsic_module_isolation_dir})
string(JOIN " " _fortran_intrinsic_module_flags_str
  ${_fortran_intrinsic_module_flags})
add_test(
  NAME FORTRANTEST_rex_iso_c_binding_fixture_artifact_is_isolated
  WORKING_DIRECTORY ${_fortran_intrinsic_module_isolation_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f iso_c_binding.mod iso_c_binding.rmod; trap 'rm -f iso_c_binding.mod iso_c_binding.rmod' EXIT; $<TARGET_FILE:testTranslator> ${_fortran_intrinsic_module_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_iso_c_binding_fixture/rex_fortran_iso_c_binding_shadow.f90 && test -s iso_c_binding.mod")
add_test(
  NAME FORTRANTEST_rex_iso_c_binding_intrinsic_consumer_after_fixture
  WORKING_DIRECTORY ${_fortran_intrinsic_module_isolation_dir}
  COMMAND $<TARGET_FILE:testTranslator>
          ${_fortran_intrinsic_module_flags} -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_iso_c_binding_intrinsic_consumer.f90)
set_tests_properties(
  FORTRANTEST_rex_iso_c_binding_fixture_artifact_is_isolated
  PROPERTIES
    FIXTURES_SETUP rex_iso_c_binding_fixture_isolation
    LABELS "FORTRANTEST;rex")
set_tests_properties(
  FORTRANTEST_rex_iso_c_binding_intrinsic_consumer_after_fixture
  PROPERTIES
    FIXTURES_REQUIRED rex_iso_c_binding_fixture_isolation
    LABELS "FORTRANTEST;rex")

set(_fortran_multiple_files_output_dir
  ${CMAKE_CURRENT_BINARY_DIR}/multiple-files-output)
file(MAKE_DIRECTORY ${_fortran_multiple_files_output_dir})
set(_fortran_multiple_files_flags
  ${_fortran_common_flags}
  -I${_fortran_multiple_files_output_dir}
  -outputdir ${_fortran_multiple_files_output_dir})
add_test(
  NAME fortran_testMultipleFortranFiles
  WORKING_DIRECTORY ${_fortran_multiple_files_output_dir}
  COMMAND $<TARGET_FILE:testTranslator>
    ${_fortran_multiple_files_flags}
    -rose:fortran_std=f90
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test2010_78.f90
    ${CMAKE_CURRENT_SOURCE_DIR}/test2010_79.f90
)

set(_fortran_reference_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests/referenceResults)
add_test(
  NAME fortran_testPreprocessedOutput
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "$<TARGET_FILE:testTranslator> ${ROSE_FLAGS_STR} -rose:fortran_std=f90 -c ${CMAKE_CURRENT_SOURCE_DIR}/test2007_suffixTest_07.F90 && diff <(tail -n 6 rose_test2007_suffixTest_07.F90) <(tail -n 6 ${_fortran_reference_dir}/rose_test2007_suffixTest_07.F90.output)"
)

set(_fortran_module_owner_output_dir
  ${_fortran_output_dir}/module-physical-output-owner)
file(MAKE_DIRECTORY ${_fortran_module_owner_output_dir})
add_test(
  NAME FORTRANTEST_rex_fortran_module_physical_output_owner
  WORKING_DIRECTORY ${_fortran_module_owner_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f rex_fortran_module_physical_output_ownership.rmod && $<TARGET_FILE:testTranslator> -rose:verbose 0 -rose:skipfinalCompileStep -I${CMAKE_CURRENT_SOURCE_DIR} -I${_fortran_module_owner_output_dir} -outputdir ${_fortran_module_owner_output_dir} -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_module_physical_output_ownership.f90 && test -s rex_fortran_module_physical_output_ownership.rmod && grep -Eqi '^[[:space:]]*MODULE[[:space:]]+rex_fortran_module_physical_output_ownership' rex_fortran_module_physical_output_ownership.rmod && grep -Eqi '^[[:space:]]*INTEGER.*exact_value' rex_fortran_module_physical_output_ownership.rmod && grep -Eqi '^[[:space:]]*END[[:space:]]+MODULE[[:space:]]+rex_fortran_module_physical_output_ownership' rex_fortran_module_physical_output_ownership.rmod")
set_tests_properties(
  FORTRANTEST_rex_fortran_module_physical_output_owner
  PROPERTIES LABELS "FORTRANTEST;rex;UNPARSER")

add_test(
  NAME fortran_rex_program_name_exact_spelling
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "$<TARGET_FILE:testTranslator> ${ROSE_FLAGS_STR} -rose:fortran_std=f90 -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_program_name_exact_spelling.F90 && awk 'tolower($1) == \"program\" && $2 == \"MiXeD_Program_Name\" { ++count } END { exit count != 1 }' rose_rex_fortran_program_name_exact_spelling.F90 && awk 'tolower($1) == \"end\" && tolower($2) == \"program\" && $3 == \"mIxEd_PrOgRaM_nAmE\" { ++count } END { exit count != 1 }' rose_rex_fortran_program_name_exact_spelling.F90"
)

add_test(
  NAME fortran_rex_test2026_equivalence_unparse
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "$<TARGET_FILE:testTranslator> ${ROSE_FLAGS_STR} -rose:fortran_std=f90 -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_equivalence.f90 && grep -qi 'equivalence' rose_rex_test2026_equivalence.f90"
)

add_test(
  NAME fortran_rex_test2026_include_unparse
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "$<TARGET_FILE:testTranslator> ${ROSE_FLAGS_STR} -rose:fortran_std=f90 -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_include.f90 && awk 'tolower($0) ~ /include/ && $0 ~ /rex_test2026_include\\.inc/ {found=1} END {exit !found}' rose_rex_test2026_include.f90"
)

add_test(
  NAME fortran_rex_issue192_concat_continuation
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "$<TARGET_FILE:testTranslator> ${ROSE_FLAGS_STR} -rose:fortran_std=f90 -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_issue192_concat_continuation.F90 && awk 'BEGIN{IGNORECASE=1} /log_filename[[:space:]]*=/ && index($0, \"//\") && !index($0, \"/ /\") { lf=1 } /mix_steps[[:space:]]*=/ && index($0, \"//\") && !index($0, \"/ /\") { ms=1 } END{exit !(lf && ms)}' rose_rex_issue192_concat_continuation.F90"
)

set(_fortran_issue487_output_dir ${_fortran_output_dir}/rex_issue487_compile_only)
file(MAKE_DIRECTORY ${_fortran_issue487_output_dir})
set(ROSE_FLAGS_ISSUE487 -rose:verbose 0 -rose:detect_dangling_pointers 2
                       -I${CMAKE_CURRENT_SOURCE_DIR}
                       -I${CMAKE_CURRENT_BINARY_DIR}
                       -I${_fortran_issue487_output_dir}
                       -outputdir ${_fortran_issue487_output_dir})
string(JOIN " " ROSE_FLAGS_ISSUE487_STR ${ROSE_FLAGS_ISSUE487})

add_test(
  NAME fortran_rex_issue487_compile_only
  WORKING_DIRECTORY ${_fortran_issue487_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f a.out && $<TARGET_FILE:testTranslator> ${ROSE_FLAGS_ISSUE487_STR} -rose:fortran_std=f90 -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_issue487_compile_only.f90 && test ! -e a.out"
)

set(_fortran_invalid_output_dir ${_fortran_output_dir}/invalid_fortran)
file(MAKE_DIRECTORY ${_fortran_invalid_output_dir})
set(ROSE_FLAGS_INVALID -rose:verbose 0 -rose:detect_dangling_pointers 2
                       -I${CMAKE_CURRENT_SOURCE_DIR}
                       -I${CMAKE_CURRENT_BINARY_DIR}
                       -I${_fortran_invalid_output_dir}
                       -outputdir ${_fortran_invalid_output_dir})
string(JOIN " " ROSE_FLAGS_INVALID_STR ${ROSE_FLAGS_INVALID})

add_test(
  NAME fortran_rex_invalid_source_reports_error
  WORKING_DIRECTORY ${_fortran_invalid_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f invalid_fortran_error.o rose_invalid_fortran_error.f90 && ! $<TARGET_FILE:testTranslator> ${ROSE_FLAGS_INVALID_STR} -rose:fortran_std=f90 -c ${CMAKE_CURRENT_SOURCE_DIR}/invalid_fortran_error.f90"
)

add_test(
  NAME fortran_rex_invalid_source_no_backend_object
  WORKING_DIRECTORY ${_fortran_invalid_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f invalid_fortran_error.o rose_invalid_fortran_error.f90 && ! $<TARGET_FILE:testTranslator> ${ROSE_FLAGS_INVALID_STR} -rose:fortran_std=f90 -c ${CMAKE_CURRENT_SOURCE_DIR}/invalid_fortran_error.f90 && test ! -e invalid_fortran_error.o"
)

set(_fortran_comment_diff_tests
  test2020_comment_1.f90
  test2020_comment_2.f90
  test2020_comment_4.f90
)
foreach(testcode IN LISTS _fortran_comment_diff_tests)
  string(REPLACE "." "_" _name ${testcode})
  add_test(
    NAME fortran_comment_diff_${_name}
    WORKING_DIRECTORY ${_fortran_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "$<TARGET_FILE:testTranslator> -c ${CMAKE_CURRENT_SOURCE_DIR}/${testcode} && diff <(grep -n \"!\" rose_${testcode}) <(grep -n \"!\" ${CMAKE_CURRENT_SOURCE_DIR}/${testcode}) > ${testcode}.diff"
  )
endforeach()

function(rex_add_fortran_module_driver_pair pair_name producer consumer)
  set(_pair_output_dir
      ${CMAKE_CURRENT_BINARY_DIR}/${pair_name}-specialized-output)
  file(MAKE_DIRECTORY ${_pair_output_dir})
  set(_pair_flags
      ${_fortran_common_flags}
      -I${_pair_output_dir}
      -outputdir ${_pair_output_dir})
  get_filename_component(_producer_stem ${producer} NAME_WE)
  get_filename_component(_consumer_stem ${consumer} NAME_WE)
  set(_producer_test fortran_module_${_producer_stem})
  set(_consumer_test fortran_module_${_consumer_stem})
  set(_fixture rex_fortran_module_driver_${pair_name})

  add_test(
    NAME ${_producer_test}
    WORKING_DIRECTORY ${_pair_output_dir}
    COMMAND testTranslator ${_pair_flags}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${producer})
  add_test(
    NAME ${_consumer_test}
    WORKING_DIRECTORY ${_pair_output_dir}
    COMMAND testTranslator ${_pair_flags}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${consumer})
  set_tests_properties(
    ${_producer_test}
    PROPERTIES
      FIXTURES_SETUP ${_fixture}
      RESOURCE_LOCK ${_fixture})
  set_tests_properties(
    ${_consumer_test}
    PROPERTIES
      FIXTURES_REQUIRED ${_fixture}
      RESOURCE_LOCK ${_fixture})
endfunction()

# These driver-level tests intentionally repeat four generic module chains.
# Give each pair a separate physical module owner so neither the generic
# graph/token tests nor an unrelated driver can satisfy its USE statements.
rex_add_fortran_module_driver_pair(
  module-a-b module_A_file.f90 module_B_file.f90)
rex_add_fortran_module_driver_pair(
  test2019-module
  test2019_module_1_file.f90 test2019_module_2_file.f90)
rex_add_fortran_module_driver_pair(
  private-attribute
  test2019_private_attribute2.f90 test2019_private_attribute.f90)
rex_add_fortran_module_driver_pair(
  iso-c-binding
  test2020_use_iso_c_binding_3.f90
  test2020_use_iso_c_binding_4.f90)

get_property(_fortran_registered_tests DIRECTORY PROPERTY TESTS)
foreach(_test IN LISTS _fortran_registered_tests)
  if(_test MATCHES "^FORTRANTEST_" OR _test MATCHES "^fortran_")
    get_property(
      _test_output_dir TEST ${_test} PROPERTY WORKING_DIRECTORY)
    if(NOT _test_output_dir OR _test_output_dir MATCHES "-NOTFOUND$")
      set(_test_output_dir ${_fortran_output_dir})
    endif()
    set_property(TEST ${_test} APPEND PROPERTY ENVIRONMENT
                 "ROSE_TEST_OUTPUT_DIR=${_test_output_dir}")
  endif()
endforeach()
