add_executable(rex_test2026_ast_json_external_scope_contract
  rex_test2026_ast_json_external_scope_contract.cpp)
target_link_libraries(rex_test2026_ast_json_external_scope_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_test2026_ast_json_detached_file_type_fixup
  rex_test2026_ast_json_detached_file_type_fixup.cpp)
target_link_libraries(rex_test2026_ast_json_detached_file_type_fixup
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_required_fields
  rex_ast_json_required_fields.cpp)
target_include_directories(rex_ast_json_required_fields PRIVATE
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astJson)
target_link_libraries(rex_ast_json_required_fields
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_template_parameter_contract
  rex_ast_json_template_parameter_contract.cpp)
target_include_directories(rex_ast_json_template_parameter_contract PRIVATE
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astJson)
target_link_libraries(rex_ast_json_template_parameter_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_finalization_contract
  rex_ast_json_finalization_contract.cpp)
target_include_directories(rex_ast_json_finalization_contract PRIVATE
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astJson)
target_link_libraries(rex_ast_json_finalization_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_source_spelled_static_member
  rex_ast_json_source_spelled_static_member.cpp)
target_link_libraries(rex_ast_json_source_spelled_static_member
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_function_instantiation_pattern
  rex_ast_json_function_instantiation_pattern.cpp)
target_link_libraries(rex_ast_json_function_instantiation_pattern
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_suppressed_header_inline_tag
  rex_ast_json_suppressed_header_inline_tag.cpp)
target_link_libraries(rex_ast_json_suppressed_header_inline_tag
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_class_type_canonical_owner
  rex_ast_json_class_type_canonical_owner.cpp)
target_link_libraries(rex_ast_json_class_type_canonical_owner
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_enum_owner
  rex_ast_json_enum_owner.cpp)
target_link_libraries(rex_ast_json_enum_owner
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_constructor_preinitialization
  rex_ast_json_constructor_preinitialization.cpp)
target_link_libraries(rex_ast_json_constructor_preinitialization
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_openmp_typed_rejection
  rex_ast_json_openmp_typed_rejection.cpp)
target_include_directories(rex_ast_json_openmp_typed_rejection PRIVATE
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astJson)
target_link_libraries(rex_ast_json_openmp_typed_rejection
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_openmp_wrapper_roundtrip
  rex_ast_json_openmp_wrapper_roundtrip.cpp)
target_link_libraries(rex_ast_json_openmp_wrapper_roundtrip
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_openmp_owned_clause_payloads
  rex_ast_json_openmp_owned_clause_payloads.cpp)
target_link_libraries(rex_ast_json_openmp_owned_clause_payloads
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_openmp_metadirective_roundtrip
  rex_ast_json_openmp_metadirective_roundtrip.cpp)
target_include_directories(rex_ast_json_openmp_metadirective_roundtrip PRIVATE
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astJson)
target_link_libraries(rex_ast_json_openmp_metadirective_roundtrip
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_openmp_syntax_expression_type
  rex_ast_json_openmp_syntax_expression_type.cpp)
target_link_libraries(rex_ast_json_openmp_syntax_expression_type
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_macro_expansion
  rex_ast_json_macro_expansion.cpp)
target_link_libraries(rex_ast_json_macro_expansion
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_source_location_builtin
  rex_ast_json_source_location_builtin.cpp)
target_link_libraries(rex_ast_json_source_location_builtin
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_operator_result_types
  rex_ast_json_operator_result_types.cpp)
target_link_libraries(rex_ast_json_operator_result_types
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_size_this_result_types
  rex_ast_json_size_this_result_types.cpp)
target_link_libraries(rex_ast_json_size_this_result_types
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_statement_attributes
  rex_ast_json_statement_attributes.cpp)
target_link_libraries(rex_ast_json_statement_attributes
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_namespace_source_fragments
  rex_ast_json_namespace_source_fragments.cpp)
target_include_directories(rex_ast_json_namespace_source_fragments PRIVATE
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astJson)
target_link_libraries(rex_ast_json_namespace_source_fragments
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_access_label_lexical_ownership
  rex_ast_json_access_label_lexical_ownership.cpp)
target_include_directories(rex_ast_json_access_label_lexical_ownership PRIVATE
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astJson)
target_link_libraries(rex_ast_json_access_label_lexical_ownership
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_using_terminal_provenance
  rex_ast_json_using_terminal_provenance.cpp)
target_include_directories(rex_ast_json_using_terminal_provenance PRIVATE
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astJson)
target_link_libraries(rex_ast_json_using_terminal_provenance
  ROSE_DLL ${link_with_libraries})

add_executable(rex_ast_json_source_declaration_group
  rex_ast_json_source_declaration_group.cpp)
target_include_directories(rex_ast_json_source_declaration_group PRIVATE
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astJson)
target_link_libraries(rex_ast_json_source_declaration_group
  ROSE_DLL ${link_with_libraries})

if(ENABLE-FORTRAN)
  add_executable(rex_ast_json_fortran_program_end_name
    rex_ast_json_fortran_program_end_name.cpp)
  target_link_libraries(rex_ast_json_fortran_program_end_name
    ROSE_DLL ${link_with_libraries})

  add_executable(rex_ast_json_fortran_absent_program_unit_names
    rex_ast_json_fortran_absent_program_unit_names.cpp)
  target_link_libraries(rex_ast_json_fortran_absent_program_unit_names
    ROSE_DLL ${link_with_libraries})

  add_executable(rex_ast_json_fortran_procedure_source_forms
    rex_ast_json_fortran_procedure_source_forms.cpp)
  target_link_libraries(rex_ast_json_fortran_procedure_source_forms
    ROSE_DLL ${link_with_libraries})

  add_executable(rex_ast_json_fortran_cray_pointer_shape
    rex_ast_json_fortran_cray_pointer_shape.cpp)
  target_link_libraries(rex_ast_json_fortran_cray_pointer_shape
    ROSE_DLL ${link_with_libraries})

  add_executable(rex_ast_json_fortran_separate_pointer
    rex_ast_json_fortran_separate_pointer.cpp)
  target_link_libraries(rex_ast_json_fortran_separate_pointer
    ROSE_DLL ${link_with_libraries})
endif()

set(_ast_json_contract_dir
  ${CMAKE_CURRENT_BINARY_DIR}/external-function-parameter-scope-json)

add_test(
  NAME ASTJSON_external_function_parameter_scope_contract
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_ast_json_contract_dir}
    $<TARGET_FILE:rex_test2026_ast_json_external_scope_contract>
      --rex-ast-json-contract-dir=${_ast_json_contract_dir}
      -rose:skipfinalCompileStep
      -w
      -rose:verbose
      0
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/input_rex_test2026_ast_json_external_scope.c)
set_tests_properties(ASTJSON_external_function_parameter_scope_contract
  PROPERTIES LABELS ASTJSON)

set(_ast_json_detached_fixup_dir
  ${CMAKE_CURRENT_BINARY_DIR}/detached-file-type-fixup-json)

add_test(
  NAME ASTJSON_rex_test2026_detached_file_type_fixup
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_ast_json_detached_fixup_dir}
    $<TARGET_FILE:rex_test2026_ast_json_detached_file_type_fixup>
      --rex-ast-json-dir=${_ast_json_detached_fixup_dir}
      -rex:ast-json-checkpoint=pre-omp-construction
      -rex:ast-json-dir=${_ast_json_detached_fixup_dir}
      -rose:skipfinalCompileStep
      -w
      -rose:verbose
      0
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/input_rex_test2026_ast_json_detached_file_type_fixup.cpp)
set_tests_properties(ASTJSON_rex_test2026_detached_file_type_fixup
  PROPERTIES LABELS ASTJSON)

add_test(
  NAME ASTJSON_rex_required_fields
  COMMAND rex_ast_json_required_fields)
set_tests_properties(ASTJSON_rex_required_fields
  PROPERTIES LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_template_parameter_contract
  COMMAND rex_ast_json_template_parameter_contract)
set_tests_properties(ASTJSON_rex_template_parameter_contract
  PROPERTIES LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_finalization_contract_positive
  COMMAND rex_ast_json_finalization_contract
    --finalization-contract=positive
    -rose:skipfinalCompileStep
    -rose:verbose
    0
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_finalization_contract_input.cpp)
set_tests_properties(ASTJSON_rex_finalization_contract_positive
  PROPERTIES LABELS "ASTJSON;rex")

foreach(_mode missing-parameter-list invalid-function-type
              mismatched-return-type missing-declaration-scope
              detached-declaration-scope
              missing-auxiliary-parent wrong-auxiliary-scope
              duplicate-emission-owner missing-this-symbol
              missing-nonreal-symbol inconsistent-nonreal-symbol
              inconsistent-nonreal-variable-identity
              inconsistent-specialization-family
              null-template-argument
              mismatched-source-order
              missing-external-context invalid-external-scope
              inconsistent-external-kind
               cast-unknown-type cast-default-type
               call-unknown-type call-default-type
               inline-unknown-type inline-default-type
               typedef-unknown-source-form
               missing-noexcept-operand noexcept-nonbool-type
               serializer-null-type serializer-unknown-type
               serializer-default-type
               serializer-unknown-typedef-source-form)
  string(REPLACE "-" "_" _mode_name "${_mode}")
  add_test(
    NAME ASTJSON_rex_finalization_${_mode_name}_is_rejected
    COMMAND rex_ast_json_finalization_contract
      --finalization-contract=${_mode}
      -rose:skipfinalCompileStep
      -rose:verbose
      0
      -std=gnu++17
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_finalization_contract_input.cpp)
  set_tests_properties(ASTJSON_rex_finalization_${_mode_name}_is_rejected
    PROPERTIES LABELS "ASTJSON;rex;death")
endforeach()

set(_ast_json_enum_owner_dir
  ${CMAKE_CURRENT_BINARY_DIR}/enum-owner-json)

add_test(
  NAME ASTJSON_rex_enum_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=post-omp-construction
    REX_AST_JSON_DIR=${_ast_json_enum_owner_dir}
    $<TARGET_FILE:rex_ast_json_enum_owner>
      -rose:openmp:ast_only
      -rose:skipfinalCompileStep
      -rose:verbose
      0
      -std=gnu++17
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_enum_owner_input.cpp)
set_tests_properties(ASTJSON_rex_enum_owner
  PROPERTIES LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_constructor_preinitialization_frontend
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:rex_ast_json_constructor_preinitialization>
    -rose:skipfinalCompileStep
    -rose:verbose
    0
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_constructor_preinitialization_input.cpp)
set_tests_properties(ASTJSON_rex_constructor_preinitialization_frontend
  PROPERTIES LABELS "ASTJSON;rex;CFE")

set(_ast_json_constructor_preinitialization_dir
  ${CMAKE_CURRENT_BINARY_DIR}/constructor-preinitialization-json)
add_test(
  NAME ASTJSON_rex_constructor_preinitialization_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_ast_json_constructor_preinitialization_dir}
    $<TARGET_FILE:rex_ast_json_constructor_preinitialization>
      -rose:skipfinalCompileStep
      -rose:verbose
      0
      -std=gnu++17
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_constructor_preinitialization_input.cpp)
set_tests_properties(ASTJSON_rex_constructor_preinitialization_roundtrip
  PROPERTIES LABELS "ASTJSON;rex;CFE")

set(_ast_json_openmp_wrapper_dir
  ${CMAKE_CURRENT_BINARY_DIR}/openmp-wrapper-json)
set(_ast_json_openmp_wrapper_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_openmp_wrapper_roundtrip_input.c)

add_test(
  NAME ASTJSON_rex_openmp_wrapper_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=post-omp-construction
    REX_AST_JSON_DIR=${_ast_json_openmp_wrapper_dir}
    $<TARGET_FILE:rex_ast_json_openmp_wrapper_roundtrip>
      -rose:openmp:ast_only
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_openmp_wrapper_output}
      -rose:verbose
      0
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_openmp_wrapper_roundtrip_input.c)
set_tests_properties(ASTJSON_rex_openmp_wrapper_roundtrip
  PROPERTIES LABELS "ASTJSON;rex;OpenMP")

set(_ast_json_openmp_owned_clause_payloads_dir
  ${CMAKE_CURRENT_BINARY_DIR}/openmp-owned-clause-payloads-json)
set(_ast_json_openmp_owned_clause_payloads_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_openmp_owned_clause_payloads_input.c)

add_test(
  NAME ASTJSON_rex_openmp_owned_clause_payloads
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=post-omp-construction
    REX_AST_JSON_DIR=${_ast_json_openmp_owned_clause_payloads_dir}
    $<TARGET_FILE:rex_ast_json_openmp_owned_clause_payloads>
      -rose:openmp:ast_only
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_openmp_owned_clause_payloads_output}
      -rose:verbose
      0
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_openmp_owned_clause_payloads_input.c)
set_tests_properties(ASTJSON_rex_openmp_owned_clause_payloads
  PROPERTIES LABELS "ASTJSON;rex;OpenMP")

set(_ast_json_openmp_metadirective_dir
  ${CMAKE_CURRENT_BINARY_DIR}/openmp-metadirective-json)
set(_ast_json_openmp_metadirective_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_openmp_metadirective_roundtrip_input.c)

add_test(
  NAME ASTJSON_rex_openmp_metadirective_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=post-omp-construction
    REX_AST_JSON_DIR=${_ast_json_openmp_metadirective_dir}
    $<TARGET_FILE:rex_ast_json_openmp_metadirective_roundtrip>
      -rose:openmp:ast_only
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_openmp_metadirective_output}
      -rose:verbose
      0
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_openmp_metadirective_roundtrip_input.c)
set_tests_properties(ASTJSON_rex_openmp_metadirective_roundtrip
  PROPERTIES LABELS "ASTJSON;rex;OpenMP")

foreach(_kind name source)
  add_test(
    NAME ASTJSON_rex_openmp_${_kind}_expression_has_no_semantic_type
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=rex_ast_json_openmp_${_kind}_expression_type.log; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; if test '${_kind}' = name; then expected='REX_AST_INVARIANT[openmp-name-expression-type]: OpenMP grammar identifier syntax has no semantic value type'; else expected='REX_AST_INVARIANT[openmp-source-expression-type]: exact OpenMP source spelling has no semantic value type'; fi; test \"$(grep -Fxc -- \"$expected\" \"$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_ast_json_openmp_syntax_expression_type>
      ${_kind})
  set_tests_properties(
    ASTJSON_rex_openmp_${_kind}_expression_has_no_semantic_type
    PROPERTIES LABELS "ASTJSON;rex;OpenMP;death")
endforeach()

add_test(
  NAME ASTJSON_rex_openmp_syntax_expression_type_classification
  COMMAND $<TARGET_FILE:rex_ast_json_openmp_syntax_expression_type>
    classification)
set_tests_properties(
  ASTJSON_rex_openmp_syntax_expression_type_classification
  PROPERTIES LABELS "ASTJSON;rex;OpenMP")

set(_openmp_typed_size_t_rejection_modes)
if(CMAKE_SIZEOF_VOID_P LESS 8)
  list(APPEND _openmp_typed_size_t_rejection_modes
    oversized-declare-simd-ordinal
    oversized-declare-variant-ordinal)
endif()

foreach(_mode invalid-enum invalid-at-enum invalid-severity-enum
              invalid-doacross-enum invalid-declare-target-device-type
              invalid-groupprivate-device-type duplicate-singleton
              missing-induction-expression
              missing-init-operand missing-init-modifier-list
              syntax-init-operand
              invalid-init-modifier-enum invalid-adjust-enum
              missing-adjust-arguments syntax-adjust-argument
              missing-append-operations
              missing-append-modifier-list append-missing-interop-type
              append-invalid-depinfo
              missing-clause-list duplicate-clause-list missing-variable-list
              flattened-clauses flattened-variables invalid-context-set-enum
              duplicate-context-set duplicate-context-trait
              duplicate-context-property duplicate-context-kind-property
              duplicate-equivalent-name-property
              duplicate-context-construct duplicate-context-custom
              illegal-context-trait missing-context-expression
              missing-context-properties wrong-context-property-payload
              syntax-expression-with-type semantic-expression-missing-type
              syntax-context-semantic-property syntax-context-score
              noninteger-context-score negative-context-score
              invalid-context-property-enum invalid-context-atomic-enum
              invalid-context-requires-enum
              invalid-context-requires-atomic-enum
              wrong-context-requires-payload
              missing-context-custom-name kind-any-with-another-property
              kind-any-with-another-trait combined-order-absent
              combined-order-duplicate combined-order-out-of-range
              noncombined-order-present missing-combined-order-property
              missing-declare-simd-target negative-declare-simd-ordinal
              missing-declare-variant-base negative-declare-variant-ordinal
              ${_openmp_typed_size_t_rejection_modes})
  string(REPLACE "-" "_" _mode_name "${_mode}")
  if(_mode STREQUAL "invalid-enum")
    set(_expected_rejection "SgOmpInductionItem has an invalid kind")
  elseif(_mode STREQUAL "invalid-at-enum")
    set(_expected_rejection "SgOmpAtClause has an invalid kind")
  elseif(_mode STREQUAL "invalid-severity-enum")
    set(_expected_rejection "SgOmpSeverityClause has an invalid kind")
  elseif(_mode STREQUAL "invalid-doacross-enum")
    set(_expected_rejection "SgOmpDoacrossClause has an invalid kind")
  elseif(_mode STREQUAL "invalid-declare-target-device-type")
    set(_expected_rejection
      "SgOmpDeclareTargetStatement has an invalid device_type kind")
  elseif(_mode STREQUAL "invalid-groupprivate-device-type")
    set(_expected_rejection
      "SgOmpGroupprivateStatement has an invalid device_type kind")
  elseif(_mode STREQUAL "duplicate-singleton")
    set(_expected_rejection "duplicate singleton edge 'operand'")
  elseif(_mode STREQUAL "missing-induction-expression")
    set(_expected_rejection "missing required edge 'expression'")
  elseif(_mode STREQUAL "missing-init-operand")
    set(_expected_rejection "missing required edge 'operand'")
  elseif(_mode STREQUAL "missing-init-modifier-list")
    set(_expected_rejection "missing required edge 'modifier_list'")
  elseif(_mode STREQUAL "syntax-init-operand")
    set(_expected_rejection
      "reconstructed malformed SgOmpInitClause: operand has no semantic value-expression role")
  elseif(_mode STREQUAL "invalid-init-modifier-enum")
    set(_expected_rejection "SgOmpInitModifier has an invalid kind")
  elseif(_mode STREQUAL "invalid-adjust-enum")
    set(_expected_rejection "SgOmpAdjustArgsClause has an invalid modifier")
  elseif(_mode STREQUAL "missing-adjust-arguments")
    set(_expected_rejection
      "reconstructed malformed SgOmpAdjustArgsClause: adjust_args argument list is missing, empty, or misowned")
  elseif(_mode STREQUAL "syntax-adjust-argument")
    set(_expected_rejection
      "reconstructed malformed SgOmpAdjustArgsClause: adjust_args argument has no semantic value-expression role")
  elseif(_mode STREQUAL "missing-append-operations")
    set(_expected_rejection
      "SgOmpAppendArgsClause has no interop operations")
  elseif(_mode STREQUAL "missing-append-modifier-list")
    set(_expected_rejection "missing required edge 'modifier_list'")
  elseif(_mode STREQUAL "append-missing-interop-type")
    set(_expected_rejection
      "reconstructed malformed SgOmpAppendArgsClause: interop type is missing")
  elseif(_mode STREQUAL "append-invalid-depinfo")
    set(_expected_rejection
      "reconstructed malformed SgOmpAppendArgsClause: depinfo modifier has no semantic locator payload")
  elseif(_mode STREQUAL "missing-clause-list")
    set(_expected_rejection "missing required edge 'clause_list'")
  elseif(_mode STREQUAL "duplicate-clause-list")
    set(_expected_rejection "duplicate singleton edge 'clause_list'")
  elseif(_mode STREQUAL "missing-variable-list")
    set(_expected_rejection "missing required edge 'variable_list'")
  elseif(_mode STREQUAL "flattened-clauses")
    set(_expected_rejection "flattened 'clauses' edges")
  elseif(_mode STREQUAL "flattened-variables")
    set(_expected_rejection "flattened 'variables' edges")
  elseif(_mode STREQUAL "invalid-context-set-enum")
    set(_expected_rejection
      "SgOmpContextSelectorSet has an invalid set kind")
  elseif(_mode STREQUAL "duplicate-context-set")
    set(_expected_rejection
      "variant clause has a duplicate selector set")
  elseif(_mode STREQUAL "duplicate-context-trait")
    set(_expected_rejection
      "context selector set has a duplicate selector")
  elseif(_mode STREQUAL "duplicate-context-property" OR
         _mode STREQUAL "duplicate-context-kind-property" OR
         _mode STREQUAL "duplicate-equivalent-name-property")
    set(_expected_rejection
      "context selector has a duplicate property")
  elseif(_mode STREQUAL "duplicate-context-construct")
    set(_expected_rejection
      "context selector set has a duplicate selector")
  elseif(_mode STREQUAL "duplicate-context-custom")
    set(_expected_rejection
      "context selector set has a duplicate selector")
  elseif(_mode STREQUAL "illegal-context-trait")
    set(_expected_rejection
      "trait selector is illegal in its selector set")
  elseif(_mode STREQUAL "missing-context-expression")
    set(_expected_rejection
      "context selector property does not own exactly one typed payload")
  elseif(_mode STREQUAL "missing-context-properties")
    set(_expected_rejection
      "context selector has invalid property cardinality")
  elseif(_mode STREQUAL "wrong-context-property-payload")
    set(_expected_rejection
      "context selector property does not own exactly one typed payload")
  elseif(_mode STREQUAL "syntax-expression-with-type")
    set(_expected_rejection
      "syntax expression has a serialized semantic type")
  elseif(_mode STREQUAL "semantic-expression-missing-type")
    set(_expected_rejection
      "semantic expression is missing its semantic type")
  elseif(_mode STREQUAL "syntax-context-semantic-property")
    set(_expected_rejection
      "semantic context property is not a semantic expression")
  elseif(_mode STREQUAL "syntax-context-score")
    set(_expected_rejection
      "context selector score is not a semantic expression")
  elseif(_mode STREQUAL "noninteger-context-score")
    set(_expected_rejection
      "context selector score is not an integer or enum expression")
  elseif(_mode STREQUAL "negative-context-score")
    set(_expected_rejection
      "context selector score is not a nonnegative constant integer expression")
  elseif(_mode STREQUAL "invalid-context-property-enum")
    set(_expected_rejection
      "SgOmpContextSelectorProperty has an invalid context kind")
  elseif(_mode STREQUAL "invalid-context-atomic-enum")
    set(_expected_rejection
      "SgOmpContextSelectorProperty has an invalid atomic_default_mem_order kind")
  elseif(_mode STREQUAL "invalid-context-requires-enum")
    set(_expected_rejection
      "SgOmpContextSelectorProperty has an invalid requires kind")
  elseif(_mode STREQUAL "invalid-context-requires-atomic-enum")
    set(_expected_rejection
      "SgOmpContextSelectorProperty has an invalid requires atomic_default_mem_order kind")
  elseif(_mode STREQUAL "wrong-context-requires-payload")
    set(_expected_rejection
      "ordinary requires property owns a mismatched payload")
  elseif(_mode STREQUAL "missing-context-custom-name")
    set(_expected_rejection
      "implementation-defined selector has no exact name")
  elseif(_mode STREQUAL "kind-any-with-another-property")
    set(_expected_rejection
      "kind(any) is not the only property in its selector")
  elseif(_mode STREQUAL "kind-any-with-another-trait")
    set(_expected_rejection
      "kind(any) selector is not the only selector in its set")
  elseif(_mode STREQUAL "combined-order-absent" OR
         _mode STREQUAL "combined-order-duplicate" OR
         _mode STREQUAL "combined-order-out-of-range")
    set(_expected_rejection
      "AST JSON combined OpenMP clause source order is absent, duplicate, or out of range")
  elseif(_mode STREQUAL "noncombined-order-present")
    set(_expected_rejection
      "AST JSON non-combined OpenMP directive owns combined clause source order")
  elseif(_mode STREQUAL "missing-combined-order-property")
    set(_expected_rejection
      "AST JSON object is missing key: combined_source_order")
  elseif(_mode STREQUAL "missing-declare-simd-target")
    set(_expected_rejection "is missing required edge 'function_ref'")
  elseif(_mode STREQUAL "negative-declare-simd-ordinal" OR
         _mode STREQUAL "oversized-declare-simd-ordinal")
    set(_expected_rejection
      "AST JSON declare simd semantic variant ordinal is out of range")
  elseif(_mode STREQUAL "missing-declare-variant-base")
    set(_expected_rejection "is missing required edge 'base_function_ref'")
  elseif(_mode STREQUAL "negative-declare-variant-ordinal" OR
         _mode STREQUAL "oversized-declare-variant-ordinal")
    set(_expected_rejection
      "AST JSON declare variant semantic variant ordinal is out of range")
  else()
    message(FATAL_ERROR "unmapped AST JSON rejection mode: ${_mode}")
  endif()
  add_test(
    NAME ASTJSON_rex_openmp_typed_${_mode_name}_is_rejected
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=rex_ast_json_openmp_typed_${_mode_name}.log; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 2; test \"$(grep -Fxc \"REX_AST_JSON_REJECTION[${_mode}]: ${_expected_rejection}\" \"$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_ast_json_openmp_typed_rejection>
      --malformed-json=${_mode}
      -rose:openmp:ast_only
      -rose:skipfinalCompileStep
      -rose:verbose
      0
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_openmp_typed_rejection_input.c)
  set_tests_properties(
    ASTJSON_rex_openmp_typed_${_mode_name}_is_rejected
    PROPERTIES LABELS "ASTJSON;rex;death")
endforeach()

set(_ast_json_macro_expansion_dir
  ${CMAKE_CURRENT_BINARY_DIR}/macro-expansion-json)
set(_ast_json_macro_expansion_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_macro_expansion_input.cpp)

add_test(
  NAME ASTJSON_rex_macro_expansion
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_ast_json_macro_expansion_dir}
    $<TARGET_FILE:rex_ast_json_macro_expansion>
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_macro_expansion_output}
      -rose:verbose
      0
      -std=gnu++17
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_macro_expansion_input.cpp)
set_tests_properties(ASTJSON_rex_macro_expansion
  PROPERTIES
    FIXTURES_SETUP rex_ast_json_macro_expansion_output
    LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_macro_expansion_compile
  COMMAND ${CMAKE_CXX_COMPILER} -std=gnu++17 -fsyntax-only
    ${_ast_json_macro_expansion_output})
set_tests_properties(ASTJSON_rex_macro_expansion_compile
  PROPERTIES
    FIXTURES_REQUIRED rex_ast_json_macro_expansion_output
    LABELS "ASTJSON;rex")

set(_ast_json_source_location_builtin_dir
  ${CMAKE_CURRENT_BINARY_DIR}/source-location-builtin-json)
set(_ast_json_source_location_builtin_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_source_location_builtin_input.cpp)

add_test(
  NAME ASTJSON_rex_source_location_builtin
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_ast_json_source_location_builtin_dir}
    $<TARGET_FILE:rex_ast_json_source_location_builtin>
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_source_location_builtin_output}
      -rose:verbose
      0
      -std=gnu++17
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_source_location_builtin_input.cpp)
set_tests_properties(ASTJSON_rex_source_location_builtin
  PROPERTIES
    FIXTURES_SETUP rex_ast_json_source_location_builtin_output
    LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_source_location_builtin_compile
  COMMAND ${CMAKE_CXX_COMPILER} -std=gnu++17 -fsyntax-only
    ${_ast_json_source_location_builtin_output})
set_tests_properties(ASTJSON_rex_source_location_builtin_compile
  PROPERTIES
    FIXTURES_REQUIRED rex_ast_json_source_location_builtin_output
    LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_operator_result_types_frontend
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:rex_ast_json_operator_result_types>
    -rose:skipfinalCompileStep
    -rose:verbose
    0
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_operator_result_types_input.cpp)
set_tests_properties(ASTJSON_rex_operator_result_types_frontend
  PROPERTIES LABELS "ASTJSON;rex;CFE")

set(_ast_json_operator_result_types_dir
  ${CMAKE_CURRENT_BINARY_DIR}/operator-result-types-json)
add_test(
  NAME ASTJSON_rex_operator_result_types_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_ast_json_operator_result_types_dir}
    $<TARGET_FILE:rex_ast_json_operator_result_types>
      -rose:skipfinalCompileStep
      -rose:verbose
      0
      -std=gnu++17
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_operator_result_types_input.cpp)
set_tests_properties(ASTJSON_rex_operator_result_types_roundtrip
  PROPERTIES LABELS "ASTJSON;rex;CFE")

add_test(
  NAME ASTJSON_rex_size_this_result_types_frontend
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:rex_ast_json_size_this_result_types>
    -rose:skipfinalCompileStep
    -rose:verbose
    0
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_size_this_result_types_input.cpp)
set_tests_properties(ASTJSON_rex_size_this_result_types_frontend
  PROPERTIES LABELS "ASTJSON;rex;CFE")

set(_ast_json_size_this_result_types_dir
  ${CMAKE_CURRENT_BINARY_DIR}/size-this-result-types-json)
add_test(
  NAME ASTJSON_rex_size_this_result_types_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_ast_json_size_this_result_types_dir}
    $<TARGET_FILE:rex_ast_json_size_this_result_types>
      -rose:skipfinalCompileStep
      -rose:verbose
      0
      -std=gnu++17
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_size_this_result_types_input.cpp)
set_tests_properties(ASTJSON_rex_size_this_result_types_roundtrip
  PROPERTIES LABELS "ASTJSON;rex;CFE")

set(_ast_json_statement_attributes_dir
  ${CMAKE_CURRENT_BINARY_DIR}/statement-attributes-json)
set(_ast_json_statement_attributes_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_statement_attributes_input.cpp)

add_test(
  NAME ASTJSON_rex_statement_attributes
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_ast_json_statement_attributes_dir}
    $<TARGET_FILE:rex_ast_json_statement_attributes>
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_statement_attributes_output}
      -rose:verbose
      0
      -std=c++23
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_statement_attributes_input.cpp)
set_tests_properties(ASTJSON_rex_statement_attributes
  PROPERTIES
    FIXTURES_SETUP rex_ast_json_statement_attributes_output
    LABELS "ASTJSON;rex;CFE")

add_test(
  NAME ASTJSON_rex_statement_attributes_compile
  COMMAND ${CMAKE_CXX_COMPILER} -std=c++23 -Wno-attributes -fsyntax-only
    ${_ast_json_statement_attributes_output})
set_tests_properties(ASTJSON_rex_statement_attributes_compile
  PROPERTIES
    FIXTURES_REQUIRED rex_ast_json_statement_attributes_output
    LABELS "ASTJSON;rex")

set(_ast_json_namespace_source_fragments_dir
  ${CMAKE_CURRENT_BINARY_DIR}/namespace-source-fragments-json)
set(_ast_json_namespace_source_fragments_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_namespace_source_fragments_input.cpp)

add_test(
  NAME ASTJSON_rex_namespace_source_fragments
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_ast_json_namespace_source_fragments_dir}
    $<TARGET_FILE:rex_ast_json_namespace_source_fragments>
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_namespace_source_fragments_output}
      -rose:verbose
      0
      -std=c++17
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_namespace_source_fragments_input.cpp)
set_tests_properties(ASTJSON_rex_namespace_source_fragments
  PROPERTIES
    FIXTURES_SETUP rex_ast_json_namespace_source_fragments_output
    LABELS "ASTJSON;rex;CFE")

add_test(
  NAME ASTJSON_rex_namespace_source_fragments_compile
  COMMAND ${CMAKE_CXX_COMPILER} -std=c++17 -fsyntax-only
    ${_ast_json_namespace_source_fragments_output})
set_tests_properties(ASTJSON_rex_namespace_source_fragments_compile
  PROPERTIES
    FIXTURES_REQUIRED rex_ast_json_namespace_source_fragments_output
    LABELS "ASTJSON;rex")

set(_ast_json_source_declaration_group_dir
  ${CMAKE_CURRENT_BINARY_DIR}/source-declaration-group-json)
set(_ast_json_source_declaration_group_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_source_declaration_group_input.cpp)

set(_ast_json_access_label_dir
  ${CMAKE_CURRENT_BINARY_DIR}/access-label-json)
set(_ast_json_access_label_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_access_label_lexical_ownership_input.cpp)

set(_ast_json_using_terminal_dir
  ${CMAKE_CURRENT_BINARY_DIR}/using-terminal-json)
set(_ast_json_using_terminal_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_using_terminal_provenance_input.cpp)

add_test(
  NAME ASTJSON_rex_access_label_lexical_ownership
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_ast_json_access_label_dir}
    $<TARGET_FILE:rex_ast_json_access_label_lexical_ownership>
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_access_label_output}
      -rose:verbose
      0
      -std=c++17
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_access_label_lexical_ownership_input.cpp)
set_tests_properties(ASTJSON_rex_access_label_lexical_ownership
  PROPERTIES
    FIXTURES_SETUP rex_ast_json_access_label_lexical_ownership_output
    LABELS "ASTJSON;rex;CFE;UNPARSER")

add_test(
  NAME ASTJSON_rex_access_label_lexical_ownership_compile
  COMMAND ${CMAKE_CXX_COMPILER} -std=c++17 -fsyntax-only
    ${_ast_json_access_label_output})
set_tests_properties(ASTJSON_rex_access_label_lexical_ownership_compile
  PROPERTIES
    FIXTURES_REQUIRED rex_ast_json_access_label_lexical_ownership_output
    LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_access_label_lexical_ownership_single_surface
  COMMAND sh -c
    "test \"\$(grep -Fc 'public:' \"$1\")\" -eq 2 && test \"\$(grep -Fc 'private:' \"$1\")\" -eq 1 && test \"\$(grep -Fc 'protected:' \"$1\")\" -eq 1"
    dummy ${_ast_json_access_label_output})
set_tests_properties(ASTJSON_rex_access_label_lexical_ownership_single_surface
  PROPERTIES
    FIXTURES_REQUIRED rex_ast_json_access_label_lexical_ownership_output
    LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_using_terminal_provenance
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_ast_json_using_terminal_dir}
    $<TARGET_FILE:rex_ast_json_using_terminal_provenance>
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_using_terminal_output}
      -rose:verbose
      0
      -std=c++20
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_using_terminal_provenance_input.cpp)
set_tests_properties(ASTJSON_rex_using_terminal_provenance
  PROPERTIES
    FIXTURES_SETUP rex_ast_json_using_terminal_provenance_output
    LABELS "ASTJSON;rex;CFE;UNPARSER")

add_test(
  NAME ASTJSON_rex_using_terminal_provenance_compile
  COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only
    ${_ast_json_using_terminal_output})
set_tests_properties(ASTJSON_rex_using_terminal_provenance_compile
  PROPERTIES
    FIXTURES_REQUIRED rex_ast_json_using_terminal_provenance_output
    LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_using_terminal_provenance_single_surface
  COMMAND sh -c
    "set -eu; test \"\$(grep -Fc 'using RexJsonUsingOverloads::rex_call;' \"$1\")\" -eq 1; test \"\$(grep -Fc 'using RexJsonUsingOverloads::operator ();' \"$1\")\" -eq 1; test \"\$(grep -Fc 'using T::T;' \"$1\")\" -eq 1; test \"\$(grep -Fc 'using RexJsonUsingTemplateBase<long>::RexJsonUsingTemplateBase;' \"$1\")\" -eq 1"
    dummy ${_ast_json_using_terminal_output})
set_tests_properties(ASTJSON_rex_using_terminal_provenance_single_surface
  PROPERTIES
    FIXTURES_REQUIRED rex_ast_json_using_terminal_provenance_output
    LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_source_declaration_group
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_ast_json_source_declaration_group_dir}
    $<TARGET_FILE:rex_ast_json_source_declaration_group>
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_source_declaration_group_output}
      -rose:verbose
      0
      -std=gnu++17
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_source_declaration_group_input.cpp)
set_tests_properties(ASTJSON_rex_source_declaration_group
  PROPERTIES
    FIXTURES_SETUP rex_ast_json_source_declaration_group_output
    LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_source_declaration_group_compile
  COMMAND ${CMAKE_CXX_COMPILER} -std=gnu++17 -fsyntax-only
    ${_ast_json_source_declaration_group_output})
set_tests_properties(ASTJSON_rex_source_declaration_group_compile
  PROPERTIES
    FIXTURES_REQUIRED rex_ast_json_source_declaration_group_output
    LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_source_declaration_group_single_surface
  COMMAND sh -c
    "set -eu; tr '\\n' ' ' < \"$1\" | grep -Eq 'rex_json_group_object[[:space:]]*,[[:space:]]*rex_json_group_function[[:space:]]*\\('; tr '\\n' ' ' < \"$1\" | grep -Eq 'rex_json_group_function_a[[:space:]]*\\([^)]*\\)[[:space:]]*,[[:space:]]*rex_json_group_function_b[[:space:]]*\\('; tr '\\n' ' ' < \"$1\" | grep -Eq 'rex_json_group_typedef_scalar[[:space:]]*,[[:space:]]*rex_json_group_typedef_function[[:space:]]*\\('; for name in rex_json_group_a rex_json_group_b rex_json_group_c rex_json_group_object rex_json_group_function rex_json_group_function_a rex_json_group_function_b rex_json_group_typedef_scalar rex_json_group_typedef_function; do count=$(grep -ow \"$name\" \"$1\" | wc -l); test \"$count\" -eq 1; done"
    dummy ${_ast_json_source_declaration_group_output})
set_tests_properties(ASTJSON_rex_source_declaration_group_single_surface
  PROPERTIES
    FIXTURES_REQUIRED rex_ast_json_source_declaration_group_output
    LABELS "ASTJSON;rex")

set(_ast_json_source_static_member_dir
  ${CMAKE_CURRENT_BINARY_DIR}/source-spelled-static-member-json)
set(_ast_json_source_static_member_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_source_spelled_static_member.cpp)

add_test(
  NAME ASTJSON_rex_source_spelled_static_member
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_DIR=${_ast_json_source_static_member_dir}
    $<TARGET_FILE:rex_ast_json_source_spelled_static_member>
      --rex-ast-json-dir=${_ast_json_source_static_member_dir}
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_source_static_member_output}
      -rose:verbose
      0
      -std=gnu++17
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_source_spelled_static_member_input.cpp)
set_tests_properties(ASTJSON_rex_source_spelled_static_member
  PROPERTIES
    FIXTURES_SETUP rex_ast_json_source_spelled_static_member_output
    LABELS "ASTJSON;rex")

add_test(
  NAME ASTJSON_rex_source_spelled_static_member_compile
  COMMAND ${CMAKE_CXX_COMPILER} -std=gnu++17 -fsyntax-only
    ${_ast_json_source_static_member_output})
set_tests_properties(ASTJSON_rex_source_spelled_static_member_compile
  PROPERTIES
    FIXTURES_REQUIRED rex_ast_json_source_spelled_static_member_output
    LABELS "ASTJSON;rex")

set(_ast_json_function_instantiation_pattern_dir
  ${CMAKE_CURRENT_BINARY_DIR}/function-instantiation-pattern-json)

add_test(
  NAME ASTJSON_rex_function_instantiation_pattern
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_DIR=${_ast_json_function_instantiation_pattern_dir}
    $<TARGET_FILE:rex_ast_json_function_instantiation_pattern>
      -rose:skipfinalCompileStep
      -rose:verbose
      0
      -std=gnu++17
      -isystem
      ${CMAKE_CURRENT_SOURCE_DIR}
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_function_instantiation_pattern_input.cpp)
set_tests_properties(ASTJSON_rex_function_instantiation_pattern
  PROPERTIES LABELS "ASTJSON;rex;CFE")

set(_ast_json_suppressed_header_inline_tag_dir
  ${CMAKE_CURRENT_BINARY_DIR}/suppressed-header-inline-tag-json)

add_test(
  NAME ASTJSON_rex_suppressed_header_inline_tag
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_DIR=${_ast_json_suppressed_header_inline_tag_dir}
    $<TARGET_FILE:rex_ast_json_suppressed_header_inline_tag>
      -rose:skipfinalCompileStep
      -rose:verbose
      0
      -std=gnu++17
      -isystem
      ${CMAKE_CURRENT_SOURCE_DIR}
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_suppressed_header_inline_tag_input.cpp)
set_tests_properties(ASTJSON_rex_suppressed_header_inline_tag
  PROPERTIES LABELS "ASTJSON;rex;CFE")

set(_ast_json_class_type_canonical_owner_dir
  ${CMAKE_CURRENT_BINARY_DIR}/class-type-canonical-owner-json)
add_test(
  NAME ASTJSON_rex_class_type_canonical_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_DIR=${_ast_json_class_type_canonical_owner_dir}
    $<TARGET_FILE:rex_ast_json_class_type_canonical_owner>
      --rex-ast-json-dir=${_ast_json_class_type_canonical_owner_dir}
      -rose:skipfinalCompileStep
      -rose:verbose
      0
      -std=gnu++17
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_class_type_canonical_owner_input.cpp)
set_tests_properties(ASTJSON_rex_class_type_canonical_owner
  PROPERTIES LABELS "ASTJSON;rex")

if(ENABLE-FORTRAN)
  set(_ast_json_fortran_program_end_name_dir
    ${CMAKE_CURRENT_BINARY_DIR}/fortran-program-end-name-json)
  set(_ast_json_fortran_program_end_name_output
    ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_fortran_program_end_name.f90)

  add_test(
    NAME ASTJSON_rex_fortran_program_end_name
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:rex_ast_json_fortran_program_end_name>
      --rex-ast-json-dir=${_ast_json_fortran_program_end_name_dir}
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_fortran_program_end_name_output}
      -rose:verbose
      0
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_fortran_program_end_name_input.f90)
  set_tests_properties(ASTJSON_rex_fortran_program_end_name
    PROPERTIES
      ENVIRONMENT REX_AST_JSON_CHECKPOINT=pre-omp-construction
      LABELS "ASTJSON;rex;Fortran")

  set(_ast_json_fortran_absent_names_dir
    ${CMAKE_CURRENT_BINARY_DIR}/fortran-absent-program-unit-names-json)
  set(_ast_json_fortran_absent_names_output
    ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_fortran_absent_program_unit_names.f90)
  add_test(
    NAME ASTJSON_rex_fortran_absent_program_unit_names
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:rex_ast_json_fortran_absent_program_unit_names>
      --rex-ast-json-dir=${_ast_json_fortran_absent_names_dir}
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_fortran_absent_names_output}
      -rose:verbose
      0
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_fortran_absent_program_unit_names_input.f90)
  set_tests_properties(ASTJSON_rex_fortran_absent_program_unit_names
    PROPERTIES
      ENVIRONMENT REX_AST_JSON_CHECKPOINT=pre-omp-construction
      LABELS "ASTJSON;rex;Fortran")

  set(_ast_json_fortran_procedure_source_forms_dir
    ${CMAKE_CURRENT_BINARY_DIR}/fortran-procedure-source-forms-json)
  set(_ast_json_fortran_procedure_source_forms_output
    ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_fortran_procedure_source_forms.f90)
  add_test(
    NAME ASTJSON_rex_fortran_procedure_source_forms
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:rex_ast_json_fortran_procedure_source_forms>
      --rex-ast-json-dir=${_ast_json_fortran_procedure_source_forms_dir}
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_fortran_procedure_source_forms_output}
      -rose:verbose
      0
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_fortran_procedure_source_forms_input.f90)
  set_tests_properties(ASTJSON_rex_fortran_procedure_source_forms
    PROPERTIES
      ENVIRONMENT REX_AST_JSON_CHECKPOINT=pre-omp-construction
      FIXTURES_SETUP rex_ast_json_fortran_procedure_source_forms_output
      LABELS "ASTJSON;rex;Fortran")

  set(_ast_json_fortran_cray_pointer_shape_dir
    ${CMAKE_CURRENT_BINARY_DIR}/fortran-cray-pointer-shape-json)
  set(_ast_json_fortran_cray_pointer_shape_output
    ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_fortran_cray_pointer_shape.f90)
  add_test(
    NAME ASTJSON_rex_fortran_cray_pointer_shape
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:rex_ast_json_fortran_cray_pointer_shape>
      --rex-ast-json-dir=${_ast_json_fortran_cray_pointer_shape_dir}
      -rose:cray_pointer_support
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_fortran_cray_pointer_shape_output}
      -rose:verbose
      0
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_fortran_cray_pointer_shape_input.f90)
  set_tests_properties(ASTJSON_rex_fortran_cray_pointer_shape
    PROPERTIES
      ENVIRONMENT REX_AST_JSON_CHECKPOINT=pre-omp-construction
      LABELS "ASTJSON;rex;Fortran")

  set(_ast_json_fortran_separate_pointer_dir
    ${CMAKE_CURRENT_BINARY_DIR}/fortran-separate-pointer-json)
  set(_ast_json_fortran_separate_pointer_output
    ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_fortran_separate_pointer.f90)
  add_test(
    NAME ASTJSON_rex_fortran_separate_pointer
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:rex_ast_json_fortran_separate_pointer>
      --rex-ast-json-dir=${_ast_json_fortran_separate_pointer_dir}
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_fortran_separate_pointer_output}
      -rose:verbose
      0
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_fortran_separate_pointer_input.f90)
  set_tests_properties(ASTJSON_rex_fortran_separate_pointer
    PROPERTIES
      ENVIRONMENT REX_AST_JSON_CHECKPOINT=pre-omp-construction
      FIXTURES_SETUP rex_ast_json_fortran_separate_pointer_output
      LABELS "ASTJSON;rex;Fortran")

  add_test(
    NAME ASTJSON_rex_fortran_separate_pointer_compile
    COMMAND ${CMAKE_Fortran_COMPILER} -fsyntax-only
      ${_ast_json_fortran_separate_pointer_output})
  set_tests_properties(ASTJSON_rex_fortran_separate_pointer_compile
    PROPERTIES
      FIXTURES_REQUIRED rex_ast_json_fortran_separate_pointer_output
      LABELS "ASTJSON;rex;Fortran")

  add_test(
    NAME ASTJSON_rex_fortran_procedure_source_forms_compile
    COMMAND ${CMAKE_Fortran_COMPILER} -fsyntax-only
      ${_ast_json_fortran_procedure_source_forms_output})
  set_tests_properties(ASTJSON_rex_fortran_procedure_source_forms_compile
    PROPERTIES
      FIXTURES_REQUIRED rex_ast_json_fortran_procedure_source_forms_output
      LABELS "ASTJSON;rex;Fortran")

  set(_ast_json_fortran_unsigned_selector_dir
    ${CMAKE_CURRENT_BINARY_DIR}/fortran-unsigned-selector-json)
  set(_ast_json_fortran_unsigned_selector_output
    ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_fortran_unsigned_selector.f90)
  add_test(
    NAME ASTJSON_rex_fortran_unsigned_selector
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:rex_ast_json_fortran_procedure_source_forms>
      --rex-ast-json-dir=${_ast_json_fortran_unsigned_selector_dir}
      --rex-contract-mode=unsigned-roundtrip
      -rose:skipfinalCompileStep
      -rose:output ${_ast_json_fortran_unsigned_selector_output}
      -rose:verbose
      0
      -funsigned
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_fortran_unsigned_selector_input.f90)
  set_tests_properties(ASTJSON_rex_fortran_unsigned_selector
    PROPERTIES
      ENVIRONMENT REX_AST_JSON_CHECKPOINT=pre-omp-construction
      LABELS "ASTJSON;rex;Fortran")

  function(add_rex_ast_json_fortran_source_type_death_test
           test_name mode diagnostic)
    set(_json_dir
      ${CMAKE_CURRENT_BINARY_DIR}/fortran-source-type-${mode}-json)
    set(_output
      ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_ast_json_fortran_source_type_${mode}.f90)
    add_test(
      NAME ${test_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${BASH_EXECUTABLE} -c
        "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; set +e; \"\$1\" \"\$2\" \"\$3\" -rose:skipfinalCompileStep -rose:output \"\$4\" -rose:verbose 0 -c \"\$5\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Fxc -- \"\$6\" \"\$log\")\" -eq 1"
        dummy
        $<TARGET_FILE:rex_ast_json_fortran_procedure_source_forms>
        --rex-ast-json-dir=${_json_dir}
        --rex-contract-mode=${mode}
        ${_output}
        ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_fortran_procedure_source_forms_input.f90
        ${diagnostic})
    set_tests_properties(${test_name}
      PROPERTIES
        ENVIRONMENT REX_AST_JSON_CHECKPOINT=pre-omp-construction
        LABELS "ASTJSON;rex;Fortran;death")
  endfunction()

  add_rex_ast_json_fortran_source_type_death_test(
    ASTJSON_rex_fortran_unparser_procedure_kind_mismatch_is_rejected
    unparser-procedure-kind-mismatch
    "REX_UNPARSE_INVARIANT[fortran-procedure-type-source]: typed procedure declaration 'k_twice' has malformed lexical ownership, source provenance, or canonical function identity")
  add_rex_ast_json_fortran_source_type_death_test(
    ASTJSON_rex_fortran_json_procedure_kind_mismatch_is_rejected
    json-procedure-kind-mismatch
    "REX_AST_JSON_REJECTION[json-procedure-kind-mismatch]: AST JSON Fortran procedure has a contradictory explicit source/semantic result type contract")
  add_rex_ast_json_fortran_source_type_death_test(
    ASTJSON_rex_fortran_json_selector_availability_mismatch_is_rejected
    json-selector-availability-mismatch
    "REX_AST_JSON_REJECTION[json-selector-availability-mismatch]: AST JSON source intrinsic KIND selector has no folded value")
  add_rex_ast_json_fortran_source_type_death_test(
    ASTJSON_rex_fortran_unparser_assumed_type_spec_mismatch_is_rejected
    unparser-assumed-type-spec-mismatch
    "REX_UNPARSE_INVARIANT[fortran-assumed-source-type]: assumed type has no exact TYPE(*) declaration use")
  add_rex_ast_json_fortran_source_type_death_test(
    ASTJSON_rex_fortran_unparser_header_prefix_type_spec_mismatch_is_rejected
    unparser-header-prefix-type-spec-mismatch
    "REX_UNPARSE_INVARIANT[fortran-procedure-result-type-spec]: intrinsic procedure result has a non-intrinsic exact source type-spec identity")
  add_rex_ast_json_fortran_source_type_death_test(
    ASTJSON_rex_fortran_unparser_missing_derived_type_spec_is_rejected
    unparser-derived-type-spec-missing
    "REX_UNPARSE_INVARIANT[fortran-procedure-result-type-spec]: typed procedure 'make_selector_box' has no exact TYPE/CLASS source identity")
  add_rex_ast_json_fortran_source_type_death_test(
    ASTJSON_rex_fortran_json_semantic_star_kind_is_rejected
    json-semantic-star-kind
    "REX_AST_JSON_REJECTION[json-semantic-star-kind]: AST JSON type has source-only star KIND metadata without an eligible Fortran source type")
  add_rex_ast_json_fortran_source_type_death_test(
    ASTJSON_rex_fortran_json_parameter_source_type_is_rejected
    json-parameter-source-type
    "REX_AST_JSON_REJECTION[json-parameter-source-type]: AST JSON Fortran procedure parameter owns declaration-statement source syntax")
  add_rex_ast_json_fortran_source_type_death_test(
    ASTJSON_rex_fortran_json_dynamic_character_result_source_is_rejected
    json-dynamic-result-source
    "REX_AST_JSON_REJECTION[json-dynamic-result-source]: AST JSON dynamic CHARACTER result type has contradictory source or selector state")
endif()
