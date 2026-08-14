# CMakeLists.txt for astInterfaceTests

# Test executables that will be installed
set(bin_programs
  loopInterchange
  loopUnrolling
  loopTiling
  loopCollapsingDirective
  loopCollapsing
)

# Test executables that will not be installed (for testing only)
set(noinst_programs
  buildFunctionDeclaration
  buildNondefiningFunction
  findMain
  buildVariableDeclaration
  buildAssignmentStmt
  buildFunctionCalls
  buildPragmaDeclaration
  attachComment
  insertHeader
  buildExpression
  buildStructDeclaration
  lookupNamedType
  buildFile
  movePreprocessingInfo
  buildIfStmt
  buildCpreprocessorDefineDeclaration
  instrumentEndOfFunction
  buildLabelStatement
  buildSizeOfOp
  abiStuffTest
  buildNullStatement
  buildForStmt
  canonicalLoop
  loopNormalization
  collectReadWriteVariables
  typeTraits
  livenessAnalysis
  getDependentDecls
  buildStructDeclaration2
  buildTypedefDeclaration
  deepDelete
  insertStatementBeforeFunction
  removeStatementCommentRelocation
  generateUniqueName
  annotateExpressionsWithUniqueNames
  replaceWithPattern
  insertBeforeUsingCommaOp
  insertAfterUsingCommaOp
  deepCopy
  fixVariableReferences
  getArrayElementType
  interfaceFunctionCoverage
  buildTemplateClass
  buildUsingDirectiveStatement
  normalizeArrowExpWithAddressOfLeftOperand
  templateQualification
  rex_test2026_getDependentDecls
  rex_test2026_declaration_scope
  rex_test2026_template_symbol_table
  rex_test2026_null_expression_unparse
  rex_test2026_template_constraints_ir
  rex_test2026_deduction_ordering
  rex_test2026_param_list_unique_name
  rex_test2026_template_decl_wrapper
  rex_test2026_sfinae_prune
  rex_test2026_on_demand_qualification
  rex_test2026_fold_pack_expr
  rex_test2026_stl_vector
  rex_test2026_stl_tuple
  rex_test2026_stl_type_traits
  rex_test2026_type_trait_constant_expr
  rex_test2026_stl_variant
  rex_test2026_stl_regex
  rex_sagebuilder_generated_source
  rex_sageinterface_producer_contract
  rex_sagebuilder_typed_ownership_contract
  rex_function_prototype_chain
  rex_unparser_subtree_context
  rex_loop_core_contracts
  rex_cast_consumer_contract
  rex_symbolic_annotation_contract
  rex_expression_mangling_contract
)

# Fortran-specific executables (only build when ENABLE-FORTRAN=ON)
set(fortran_programs
  buildProcedureHeaderStatement
  buildExternalStatement
  buildCommonBlock
  doLoopNormalization
  buildLabelStatement2
)

# Include directories for test programs
include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/sageInterface
  ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/transformationTracking
  ${CMAKE_SOURCE_DIR}/src/midend/astUtil/symbolicVal
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII
)

# Build all executables
set(all_programs ${bin_programs} ${noinst_programs})
if(ENABLE-FORTRAN)
  list(APPEND all_programs ${fortran_programs})
endif()

foreach(prog ${all_programs})
  add_executable(${prog} ${prog}.C)
  target_link_libraries(${prog} ROSE_DLL ${link_with_libraries})
endforeach()

# Install bin_programs
install(TARGETS ${bin_programs} DESTINATION bin)

# Common test flags
set(TEST_CXXFLAGS -rose:verbose 0 -g3)
set(_ast_interface_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_ast_interface_output_dir})
set(_ast_interface_fortran_output_dir
  ${CMAKE_CURRENT_BINARY_DIR}/test-output/fortran)
file(MAKE_DIRECTORY ${_ast_interface_fortran_output_dir})
set(_ast_interface_fortran_flags
  -I${_ast_interface_fortran_output_dir}
  -outputdir ${_ast_interface_fortran_output_dir})

# Test definitions
# Group 1: Tests that follow the pattern: executable inputfile.C -> rose_inputfile.C
set(group1_tests
  fixVariableReferences
  findMain
  buildVariableDeclaration
  buildAssignmentStmt
  buildIfStmt
  buildFile
  buildExpression
  buildStructDeclaration
  buildStructDeclaration2
  buildTemplateClass
  buildCpreprocessorDefineDeclaration
  instrumentEndOfFunction
  buildLabelStatement
  buildSizeOfOp
  buildNullStatement
  buildForStmt
  canonicalLoop
  loopNormalization
  collectReadWriteVariables
  typeTraits
  interfaceFunctionCoverage
  livenessAnalysis
  generateUniqueName
  annotateExpressionsWithUniqueNames
  insertBeforeUsingCommaOp
  insertAfterUsingCommaOp
  deepCopy
  buildTypedefDeclaration
  getDependentDecls
  replaceWithPattern
  buildUsingDirectiveStatement
  normalizeArrowExpWithAddressOfLeftOperand
  rex_test2026_getDependentDecls
  rex_test2026_declaration_scope
  rex_test2026_template_symbol_table
  rex_test2026_null_expression_unparse
  rex_test2026_deduction_ordering
  rex_test2026_param_list_unique_name
  rex_test2026_template_decl_wrapper
  rex_test2026_on_demand_qualification
  rex_function_prototype_chain
)

foreach(test ${group1_tests})
  if(test STREQUAL "rex_function_prototype_chain")
    set(_test_input rex_function_prototype_chain_input.C)
  else()
    set(_test_input input${test}.C)
  endif()
  add_test(
    NAME astInterface_${test}
    COMMAND ${test} ${TEST_CXXFLAGS} -c
            ${CMAKE_CURRENT_SOURCE_DIR}/${_test_input}
  )
endforeach()

add_test(
  NAME astInterface_rex_interface_function_coverage_memcheck
  COMMAND $<TARGET_FILE:interfaceFunctionCoverage> ${TEST_CXXFLAGS} -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_interface_function_coverage_memcheck.cpp
)
set_tests_properties(
  astInterface_rex_interface_function_coverage_memcheck
  PROPERTIES LABELS "astInterface;rex")

add_test(
  NAME astInterface_rex_on_demand_qualification_missing_use_site_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=astInterface_rex_on_demand_qualification_missing_use_site.log; rm -f \"\$log\"; set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[unparse-to-string-type-use-site\\]: contextual type=0x[[:xdigit:]]+/SgNonrealType requires an exact emitted qualification context and reference identity$' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_test2026_on_demand_qualification>
    --reject-missing-use-site
    ${TEST_CXXFLAGS} -c
    ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_on_demand_qualification.C)
set_tests_properties(
  astInterface_rex_on_demand_qualification_missing_use_site_is_rejected
  PROPERTIES LABELS "astInterface;rex;UNPARSER;UNPARSETOSTRING;death")

add_test(
  NAME astInterface_rex_sagebuilder_typed_ownership_contract
  COMMAND $<TARGET_FILE:rex_sagebuilder_typed_ownership_contract>)
set_tests_properties(
  astInterface_rex_sagebuilder_typed_ownership_contract
  PROPERTIES LABELS "astInterface;rex")

add_test(
  NAME astInterface_rex_generated_prototype_parameter_ownership
  COMMAND $<TARGET_FILE:rex_sagebuilder_typed_ownership_contract>
          --check-generated-prototype-parameter-ownership)
set_tests_properties(
  astInterface_rex_generated_prototype_parameter_ownership
  PROPERTIES LABELS "astInterface;rex;OUTLINER")

add_test(
  NAME astInterface_rex_pending_enclosing_tag_reference
  COMMAND $<TARGET_FILE:rex_sagebuilder_typed_ownership_contract>
          --check-pending-enclosing-tag-reference)
set_tests_properties(
  astInterface_rex_pending_enclosing_tag_reference
  PROPERTIES LABELS "astInterface;rex")

add_test(
  NAME astInterface_rex_reopened_namespace_function_ownership
  COMMAND $<TARGET_FILE:rex_sagebuilder_typed_ownership_contract>
          --check-reopened-namespace-function-ownership)
set_tests_properties(
  astInterface_rex_reopened_namespace_function_ownership
  PROPERTIES LABELS "astInterface;rex")

add_test(
  NAME astInterface_rex_binary_nfi_source_provenance
  COMMAND $<TARGET_FILE:rex_sagebuilder_typed_ownership_contract>
          --check-binary-nfi-source-provenance)
set_tests_properties(
  astInterface_rex_binary_nfi_source_provenance
  PROPERTIES LABELS "astInterface;rex")

add_test(
  NAME astInterface_rex_direct_nfi_source_provenance
  COMMAND $<TARGET_FILE:rex_sagebuilder_typed_ownership_contract>
          --check-direct-nfi-source-provenance)
set_tests_properties(
  astInterface_rex_direct_nfi_source_provenance
  PROPERTIES LABELS "astInterface;rex")

add_test(
  NAME astInterface_rex_semantic_function_source_provenance
  COMMAND $<TARGET_FILE:rex_sagebuilder_typed_ownership_contract>
          --check-semantic-function-source-provenance)
set_tests_properties(
  astInterface_rex_semantic_function_source_provenance
  PROPERTIES LABELS "astInterface;rex")

add_test(
  NAME astInterface_rex_frontend_member_ctor_list_publication_boundary
  COMMAND $<TARGET_FILE:rex_sagebuilder_typed_ownership_contract>
          --check-frontend-member-ctor-list-publication-boundary)
set_tests_properties(
  astInterface_rex_frontend_member_ctor_list_publication_boundary
  PROPERTIES LABELS "astInterface;rex;CFE")

add_test(
  NAME astInterface_rex_pending_exact_semantic_function_source_boundary
  COMMAND $<TARGET_FILE:rex_sagebuilder_typed_ownership_contract>
          --check-pending-exact-semantic-function-source-boundary)
set_tests_properties(
  astInterface_rex_pending_exact_semantic_function_source_boundary
  PROPERTIES LABELS "astInterface;rex;FORTRANTEST")

add_test(
  NAME astInterface_rex_sageinterface_producer_contract
  COMMAND $<TARGET_FILE:rex_sageinterface_producer_contract>)
set_tests_properties(
  astInterface_rex_sageinterface_producer_contract
  PROPERTIES LABELS "astInterface;rex")

set(_rex_cast_consumer_input
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_cast_consumer_contract_input.C)
add_test(
  NAME astInterface_rex_cast_consumer_contract
  COMMAND $<TARGET_FILE:rex_cast_consumer_contract>
          ${TEST_CXXFLAGS} -c ${_rex_cast_consumer_input})
set_tests_properties(astInterface_rex_cast_consumer_contract
  PROPERTIES LABELS "astInterface;rex")

add_test(
  NAME astInterface_rex_expression_mangling_contract
  COMMAND $<TARGET_FILE:rex_expression_mangling_contract>)
set_tests_properties(
  astInterface_rex_expression_mangling_contract
  PROPERTIES LABELS "astInterface;rex;CFE")

function(add_rex_cast_consumer_death_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; diagnostic=\"\$1\"; shift; set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec -- \"\$diagnostic\" \"\$log\")\" -eq 1"
      dummy
      "${diagnostic}"
      $<TARGET_FILE:rex_cast_consumer_contract>
      ${mode}
      ${TEST_CXXFLAGS} -c ${_rex_cast_consumer_input})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "astInterface;rex;death")
endfunction()

add_rex_cast_consumer_death_test(
  astInterface_rex_unary_semantic_conversion_rebuild_is_rejected
  --reject-unary-cast-rebuild
  "^REX_AST_INVARIANT\\[unary-cast-rebuild\\]: AstInterface unary operators do not carry an exact cast target type, semantic conversion kind, or value category$")
add_test(
  NAME astInterface_rex_symbolic_integral_cast_is_preserved
  COMMAND $<TARGET_FILE:rex_cast_consumer_contract>
          --preserve-symbolic-integral-cast
          ${TEST_CXXFLAGS} -c ${_rex_cast_consumer_input})
set_tests_properties(astInterface_rex_symbolic_integral_cast_is_preserved
  PROPERTIES LABELS "astInterface;rex")
add_rex_cast_consumer_death_test(
  astInterface_rex_loop_integral_cast_discard_is_rejected
  --reject-loop-cast-discard
  "^REX_AST_INVARIANT\\[cast-value-identity\\]: consumer=AstInterface::SkipCasting cast=0x[[:xdigit:]]+ kind=28 changes the value or identity and cannot be discarded$")
add_rex_cast_consumer_death_test(
  astInterface_rex_dependent_cast_target_failure_is_rejected
  --reject-dependent-cast-target
  "^REX_TEMPLATE_INVARIANT\\[type-parameter-instantiation\\]: type=0x[[:xdigit:]]+ name=T position=0 has no exact argument in a 0-argument specialization$")
add_rex_cast_consumer_death_test(
  astInterface_rex_unsupported_cast_precision_policy_is_rejected
  --reject-unsupported-precision-policy
  "^REX_AST_INVARIANT\\[cast-precision-policy\\]: cast=0x[[:xdigit:]]+ conversion=[0-9]+ source-type=0x[[:xdigit:]]+ target-type=0x[[:xdigit:]]+ has no exact numeric precision policy$")

set(_rex_symbolic_annotation_input
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_symbolic_annotation_contract_input.C)
add_test(
  NAME astInterface_rex_symbolic_annotation_contract
  COMMAND $<TARGET_FILE:rex_symbolic_annotation_contract>
          ${TEST_CXXFLAGS} -c ${_rex_symbolic_annotation_input})
set_tests_properties(astInterface_rex_symbolic_annotation_contract
  PROPERTIES LABELS "astInterface;rex;annotation")

add_test(
  NAME astInterface_rex_symbolic_unsigned_minus_preserves_type
  COMMAND $<TARGET_FILE:rex_symbolic_annotation_contract>
          --check-unsigned-minus
          ${TEST_CXXFLAGS} -c ${_rex_symbolic_annotation_input})
set_tests_properties(astInterface_rex_symbolic_unsigned_minus_preserves_type
  PROPERTIES LABELS "astInterface;rex;annotation")

add_test(
  NAME astInterface_rex_symbolic_floating_grouping_is_exact
  COMMAND $<TARGET_FILE:rex_symbolic_annotation_contract>
          --check-floating-grouping
          ${TEST_CXXFLAGS} -c ${_rex_symbolic_annotation_input})
set_tests_properties(astInterface_rex_symbolic_floating_grouping_is_exact
  PROPERTIES LABELS "astInterface;rex;annotation")

add_test(
  NAME astInterface_rex_symbolic_array_access_preserves_operator
  COMMAND $<TARGET_FILE:rex_symbolic_annotation_contract>
          --check-array-access
          ${TEST_CXXFLAGS} -c ${_rex_symbolic_annotation_input})
set_tests_properties(astInterface_rex_symbolic_array_access_preserves_operator
  PROPERTIES LABELS "astInterface;rex;annotation")

add_test(
  NAME astInterface_rex_symbolic_complex_floating_grouping_is_exact
  COMMAND $<TARGET_FILE:rex_symbolic_annotation_contract>
          --check-complex-floating-grouping
          ${TEST_CXXFLAGS} -c ${_rex_symbolic_annotation_input})
set_tests_properties(
  astInterface_rex_symbolic_complex_floating_grouping_is_exact
  PROPERTIES LABELS "astInterface;rex;annotation")

add_test(
  NAME astInterface_rex_symbolic_operator_arity_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=astInterface_rex_symbolic_operator_arity_is_rejected.log; rm -f \"\$log\"; set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec -- '^REX_SYMBOLIC_INVARIANT\\[operator-arity\\]: operator=BOP_PLUS expected=2 actual=1$' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_symbolic_annotation_contract>
    --reject-operator-arity)
set_tests_properties(astInterface_rex_symbolic_operator_arity_is_rejected
  PROPERTIES LABELS "astInterface;rex;annotation;death")

add_test(
  NAME astInterface_rex_annotation_replacement_arity_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=astInterface_rex_annotation_replacement_arity_is_rejected.log; rm -f \"\$log\"; set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec -- '^REX_ANNOTATION_INVARIANT\\[replacement-arity\\]: parameters=1 arguments=0$' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_symbolic_annotation_contract>
    --reject-replacement-arity)
set_tests_properties(astInterface_rex_annotation_replacement_arity_is_rejected
  PROPERTIES LABELS "astInterface;rex;annotation;death")

add_test(
  NAME astInterface_rex_untyped_symbolic_allocation_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=astInterface_rex_untyped_symbolic_allocation_is_rejected.log; rm -f \"\$log\"; set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec -- '^REX_SYMBOLIC_INVARIANT\\[operator-kind\\]: operator=UOP_ALLOCATE requires an exact typed AST representation$' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_symbolic_annotation_contract>
    --reject-untyped-allocation)
set_tests_properties(astInterface_rex_untyped_symbolic_allocation_is_rejected
  PROPERTIES LABELS "astInterface;rex;annotation;death")

add_test(
  NAME astInterface_rex_empty_preprocessing_move_detached_destination
  COMMAND $<TARGET_FILE:rex_sageinterface_producer_contract>
          --check-empty-move-detached-destination)
set_tests_properties(
  astInterface_rex_empty_preprocessing_move_detached_destination
  PROPERTIES LABELS "astInterface;rex")
if(ENABLE-FORTRAN)
  add_test(
    NAME astInterface_rex_generated_fortran_physical_owner
    COMMAND $<TARGET_FILE:rex_sageinterface_producer_contract>
            --fortran-generated-owner)
  set_tests_properties(
    astInterface_rex_generated_fortran_physical_owner
    PROPERTIES LABELS "astInterface;rex;fortran")
endif()

function(add_rex_sageinterface_producer_death_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; diagnostic=\"\$1\"; shift; set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec -- \"\$diagnostic\" \"\$log\")\" -eq 1"
      dummy
      "${diagnostic}"
      $<TARGET_FILE:rex_sageinterface_producer_contract>
      ${mode})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "astInterface;rex;death")
endfunction()

add_rex_sageinterface_producer_death_test(
  astInterface_rex_detached_preprocessing_owner_is_rejected
  --reject-detached-preprocessing-owner
  "^REX_AST_INVARIANT\\[generated-preprocessing-owner\\]: owner=0x[[:xdigit:]]+ type=SgNullStatement has no exact attached physical output identity \\(source-file=\\(nil\\) info=0x[[:xdigit:]]+ shared=0 physical-file-id=-2 parent=\\(nil\\)/null\\)$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_detached_move_preprocessing_owner_is_rejected
  --reject-detached-move-owner-with-record
  "^REX_AST_INVARIANT\\[move-preprocessing-owner\\]: owner=0x[[:xdigit:]]+ type=SgNullStatement has no exact attached physical output identity \\(source-file=\\(nil\\) info=0x[[:xdigit:]]+ shared=0 physical-file-id=-2 parent=\\(nil\\)/null\\)$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_unknown_preprocessing_kind_is_rejected
  --reject-unknown-preprocessing-kind
  "^REX_AST_INVARIANT\\[generated-preprocessing-kind\\]: record=0x[[:xdigit:]]+ directive=0 position=2 placement=1 is not publishable$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_invalid_preprocessing_position_is_rejected
  --reject-invalid-preprocessing-position
  "^REX_AST_INVARIANT\\[generated-preprocessing-kind\\]: record=0x[[:xdigit:]]+ directive=1 position=0 placement=1 is not publishable$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_contradictory_preprocessing_physical_owner_is_rejected
  --reject-contradictory-preprocessing-physical-owner
  "^REX_AST_INVARIANT\\[generated-preprocessing-physical-provenance\\]: record=0x[[:xdigit:]]+ prior-physical-owner=1 contradicts exact attached-owner=0$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_impossible_stored_physical_id_is_rejected
  --reject-impossible-stored-physical-id
  "^REX_AST_INVARIANT\\[file-info-physical-owner\\]: file-info=0x[[:xdigit:]]+ has impossible stored physical-file-id=-1 transformation=0 compiler-generated=0$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_unowned_physical_id_query_is_rejected
  --reject-unowned-physical-id-query
  "^REX_AST_INVARIANT\\[file-info-physical-owner\\]: file-info=0x[[:xdigit:]]+ requested-file-id=1 is neither exact-owner-id=0 nor a shared physical owner$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_unowned_logical_line_query_is_rejected
  --reject-unowned-logical-line-query
  "^REX_AST_INVARIANT\\[file-info-logical-line-owner\\]: file-info=0x[[:xdigit:]]+ requested-file-id=1 is neither exact-owner-id=0 nor a shared physical owner$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_unowned_physical_line_query_is_rejected
  --reject-unowned-physical-line-query
  "^REX_AST_INVARIANT\\[file-info-physical-line-owner\\]: file-info=0x[[:xdigit:]]+ requested-file-id=1 is neither exact-owner-id=0 nor a shared physical owner$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_mixed_generated_source_origin_is_rejected
  --reject-mixed-generated-source-origin
  "^REX_AST_INVARIANT\\[generated-subtree-semantic-origin\\]: node=0x[[:xdigit:]]+ type=SgCastExp mixes generated and source position origins$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_contradictory_generated_physical_owner_is_rejected
  --reject-contradictory-generated-physical-owners
  "^REX_AST_INVARIANT\\[generated-subtree-physical-provenance\\]: node=0x[[:xdigit:]]+ type=SgCastExp prior-physical-owner=1 contradicts exact attached-owner=0$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_mixed_generated_physical_owners_are_rejected
  --reject-mixed-generated-physical-owners
  "^REX_AST_INVARIANT\\[generated-subtree-physical-provenance\\]: node=0x[[:xdigit:]]+ type=SgCastExp has contradictory prior physical owners [0-9]+ and [0-9]+$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_detached_outliner_message_owner_is_rejected
  --reject-detached-message-owner
  "^REX_OUTLINER_INVARIANT\\[message-owner\\]: statement=0x[[:xdigit:]]+ has no exact attached physical output identity$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_malformed_outliner_message_spelling_is_rejected
  --reject-malformed-message-spelling
  "^REX_OUTLINER_INVARIANT\\[message-spelling\\]: generated message must be one complete C-style comment$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_null_append_scope_is_rejected
  --reject-null-append-scope
  "^REX_AST_INVARIANT\\[append-statement-scope\\]: statement=0x[[:xdigit:]]+ scope=\\(nil\\) must be non-null and explicit$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_null_prepend_scope_is_rejected
  --reject-null-prepend-scope
  "^REX_AST_INVARIANT\\[prepend-statement-scope\\]: statement=0x[[:xdigit:]]+ scope=\\(nil\\) must be non-null and explicit$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_null_append_list_scope_is_rejected
  --reject-null-append-list-scope
  "^REX_AST_INVARIANT\\[append-statement-list-scope\\]: explicit scope is null for statement_count=0$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_null_prepend_list_scope_is_rejected
  --reject-null-prepend-list-scope
  "^REX_AST_INVARIANT\\[prepend-statement-list-scope\\]: explicit scope is null for statement_count=0$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_null_temporary_variable_anchor_is_rejected
  --reject-null-temporary-variable-anchor
  "^REX_AST_INVARIANT\\[temporary-variable-owner\\]: expression=0x[[:xdigit:]]+ insertion-anchor=\\(nil\\) scope=\\(nil\\) must identify one expression and one exact attached direct-child insertion site$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_null_nonreal_scope_is_rejected
  --reject-null-nonreal-scope
  "^REX_AST_INVARIANT\\[nonreal-declaration-scope\\]: name=rex_missing_nonreal_scope requires an explicit non-null lexical declaration scope$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_null_nonreal_type_scope_is_rejected
  --reject-null-nonreal-type-scope
  "^REX_AST_INVARIANT\\[nonreal-declaration-scope\\]: name=rex_missing_nonreal_type_scope requires an explicit non-null lexical scope$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_generic_template_publication_is_rejected
  --reject-template-declaration-publication
  "^REX_AST_INVARIANT\\[template-declaration-publication\\]: declaration=0x[[:xdigit:]]+ scope=0x[[:xdigit:]]+ generic SgTemplateDeclaration has no typed symbol-publication contract; use a concrete template declaration builder$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_missing_canonical_function_symbol_is_rejected
  --reject-missing-canonical-function-symbol
  "^REX_AST_INVARIANT\\[function-canonical-symbol\\]: function=0x[[:xdigit:]]+ name=rex_missing_function_symbol first_nondefining=0x[[:xdigit:]]+ semantic_scope=0x[[:xdigit:]]+ has no canonical function symbol$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_malformed_implicit_conversion_role_is_rejected
  --reject-malformed-implicit-conversion-role
  "^REX_AST_INVARIANT\\[generated-subtree-semantic-role\\]: implicit cast=0x[[:xdigit:]]+ position\\[2\\]=0x[[:xdigit:]]+ has shared=0 generated=1 output=1 implicit=0 transformation=0$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_misowned_frontend_for_placeholder_is_rejected
  --reject-misowned-frontend-for-placeholder
  "^REX_AST_INVARIANT\\[frontend-for-syntactic-absence-role\\]: node=0x[[:xdigit:]]+/SgNullStatement carries semantic-only frontend provenance without one exact absent for-header edge$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_marked_malformed_implicit_conversion_role_is_rejected
  --reject-malformed-marked-implicit-conversion-role
  "^REX_AST_INVARIANT\\[generated-subtree-semantic-role\\]: implicit cast=0x[[:xdigit:]]+ position\\[0\\]=0x[[:xdigit:]]+ has shared=0 generated=1 output=1 implicit=0 transformation=0$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_noncomment_style_is_rejected
  --reject-comment-style
  "^REX_AST_INVARIANT\\[generated-comment-style\\]: unsupported explicit comment directive type=18$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_already_spelled_comment_is_rejected
  --reject-spelled-comment
  "^REX_AST_INVARIANT\\[generated-comment-style\\]: C\\+\\+ comment payload is already spelled or multiline$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_malformed_generated_define_is_rejected
  --reject-malformed-define
  "^REX_FRONTEND_INVARIANT\\[macro-definition-spelling\\]: reason=missing-name-separator spelling=#define$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_detached_define_owner_is_rejected
  --reject-detached-define-owner
  "^REX_AST_INVARIANT\\[generated-preprocessing-owner\\]: owner=0x[[:xdigit:]]+ type=SgNullStatement has no exact attached physical output identity \\(source-file=\\(nil\\) info=0x[[:xdigit:]]+ shared=0 physical-file-id=-2 parent=\\(nil\\)/null\\)$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_detached_header_owner_is_rejected
  --reject-detached-header-owner
  "^REX_AST_INVARIANT\\[insert-header-owner\\]: owner=0x[[:xdigit:]]+ type=SgGlobal has no exact attached physical output identity \\(source-file=\\(nil\\) info=\\(nil\\) shared=0 physical-file-id=-1 parent=\\(nil\\)/null\\)$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_detached_statement_scope_is_rejected
  --reject-detached-statement-scope
  "^REX_AST_INVARIANT\\[statement-scope-parent\\]: statement=0x[[:xdigit:]]+ type=SgEmptyDeclaration has null parent$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_noncontiguous_header_anchor_is_rejected
  --reject-noncontiguous-header-anchor
  "^REX_AST_INVARIANT\\[insert-header-anchor-order\\]: physical owner=0 has noncontiguous preprocessing anchors$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_duplicate_statement_append_is_rejected
  --reject-duplicate-append
  "^REX_AST_INVARIANT\\[append-statement-duplicate-owner\\]: statement=0x[[:xdigit:]]+ type=SgVariableDeclaration is already owned by scope=0x[[:xdigit:]]+ type=SgGlobal$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_nonautonomous_statement_append_is_rejected
  --reject-nonautonomous-append
  "^REX_AST_INVARIANT\\[append-statement-lexical-owner\\]: non-autonomous class declaration=0x[[:xdigit:]]+ type=SgClassDeclaration must use typed auxiliary ownership$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_statement_self_replacement_is_rejected
  --reject-replace-self
  "^REX_AST_INVARIANT\\[replace-statement-self\\]: statement=0x[[:xdigit:]]+ type=SgVariableDeclaration cannot replace itself$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_detached_statement_append_is_rejected
  --reject-detached-append-owner
  "^REX_AST_INVARIANT\\[generated-subtree-owner\\]: owner=0x[[:xdigit:]]+ type=SgBasicBlock resolved-owner=0x[[:xdigit:]]+/SgBasicBlock has no exact attached physical output identity \\(source-file=\\(nil\\) info=0x[[:xdigit:]]+ shared=0 physical-file-id=-2 parent=\\(nil\\)/null\\)$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_insert_preprocessing_merge_is_rejected
  --reject-insert-preprocessing-merge
  "^REX_AST_INVARIANT\\[insert-preprocessing-merge\\]: target=0x[[:xdigit:]]+ and inserted statement=0x[[:xdigit:]]+ both own preprocessing$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_insert_preprocessing_basic_block_is_rejected
  --reject-insert-preprocessing-basic-block
  "^REX_AST_INVARIANT\\[insert-preprocessing-basic-block\\]: target=0x[[:xdigit:]]+ owns preprocessing that cannot be relocated onto basic-block replacement=0x[[:xdigit:]]+$")
add_rex_sageinterface_producer_death_test(
  astInterface_rex_paste_inside_is_rejected
  --reject-paste-inside
  "^REX_AST_INVARIANT\\[paste-preprocessing-owner\\]: destination=0x[[:xdigit:]]+ position=4 is not an exact supported attachment$")

function(add_rex_sagebuilder_typed_ownership_death_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; diagnostic=\"\$1\"; shift; set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec -- \"\$diagnostic\" \"\$log\")\" -eq 1"
      dummy
      "${diagnostic}"
      $<TARGET_FILE:rex_sagebuilder_typed_ownership_contract>
      ${mode})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "astInterface;rex;death")
endfunction()

add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_preclassified_nfi_source_is_rejected
  --reject-preclassified-nfi-source
  "^REX_AST_INVARIANT\\[nfi-source-state\\]: node=0x[[:xdigit:]]+/SgNullExpression no-file-info construction requires fresh null provenance start=0x[[:xdigit:]]+ end=0x[[:xdigit:]]+ operator=0x[[:xdigit:]]+ pragma-start=\\(nil\\) pragma-end=\\(nil\\)$")

add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_frontend_semantic_physical_hybrid_is_rejected
  --reject-frontend-semantic-physical-hybrid
  "^REX_AST_INVARIANT\\[physical-source-position\\]: node=0x[[:xdigit:]]+/SgNullStatement file-info=0x[[:xdigit:]]+ claims both frontend-semantic and physical source ownership$")

add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_standalone_parameter_list_copy_is_rejected
  --reject-standalone-parameter-list-copy
  "^REX_COPY_INVARIANT\\[function-parameter-list-root\\]: list=0x[[:xdigit:]]+ owner=\\(nil\\)/<null> cannot be copied without a destination function or requires-expression transaction$")

add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_missing_template_identity_is_rejected
  --reject-missing-template-identity
  "^REX_AST_INVARIANT\\[template-instantiation-builder-identity\\]: name=rex_box template=\\(nil\\) arguments=0x[[:xdigit:]]+ instantiation-name=0x[[:xdigit:]]+ do not form one exact typed construction request$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_auxiliary_output_role_is_rejected
  --reject-auxiliary-output-role
  "^REX_AST_INVARIANT\\[auxiliary-container-provenance\\]: container=0x[[:xdigit:]]+ start=0x[[:xdigit:]]+ start-parent=0x[[:xdigit:]]+ start-file=-4 start-compiler=1 start-transform=0 start-output=1 start-unavailable=0 end=0x[[:xdigit:]]+ end-parent=0x[[:xdigit:]]+ end-file=-4 end-compiler=1 end-transform=0 end-output=1 end-unavailable=0 file-info=0x[[:xdigit:]]+ preprocessing=0 is not exact compiler-generated, non-output semantic infrastructure$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_transformation_semantic_function_parameters_are_rejected
  --reject-transformation-semantic-function-parameters
  "^REX_AST_INVARIANT\\[semantic-source-provenance\\]: node=0x[[:xdigit:]]+/SgFunctionParameterList was already classified before semantic construction start=0x[[:xdigit:]]+ end=0x[[:xdigit:]]+ operator=\\(nil\\)$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_semantic_typedef_lexical_publication_is_rejected
  --reject-semantic-typedef-lexical-publication
  "^REX_AST_INVARIANT\\[semantic-declaration-lexical-publication\\]: declaration=0x[[:xdigit:]]+ type=SgTypedefDeclaration retains semantic-only source provenance$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_unknown_typedef_source_form_is_rejected
  --reject-unknown-typedef-source-form
  "^REX_AST_INVARIANT\\[typedef-builder-source-form\\]: name=rex_unknown_typedef_source_form declaration-form=0 is not an exact typedef/using choice$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_unknown_template_typedef_source_form_is_rejected
  --reject-unknown-template-typedef-source-form
  "^REX_AST_INVARIANT\\[typedef-builder-source-form\\]: name=rex_unknown_template_typedef_source_form declaration-form=0 is not an exact typedef/using choice$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_unknown_template_typedef_instantiation_source_form_is_rejected
  --reject-unknown-template-typedef-instantiation-source-form
  "^REX_AST_INVARIANT\\[typedef-builder-source-form\\]: name=rex_alias declaration-form=0 is not an exact typedef/using choice$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_semantic_function_lexical_publication_is_rejected
  --reject-semantic-function-lexical-publication
  "^REX_AST_INVARIANT\\[semantic-declaration-lexical-publication\\]: declaration=0x[[:xdigit:]]+ type=SgFunctionDeclaration retains semantic-only source provenance$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_auxiliary_basic_block_append_is_rejected
  --reject-auxiliary-basic-block-append
  "^REX_AST_INVARIANT\\[basic-block-append-auxiliary-owner\\]: statement=0x[[:xdigit:]]+ type=SgTypedefDeclaration remains owned by auxiliary container=0x[[:xdigit:]]+ owner=0x[[:xdigit:]]+; detach it through the typed ownership transaction before lexical adoption$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_missing_namespace_fragments_are_rejected
  --reject-missing-source-fragments
  "^REX_AST_INVARIANT\\[namespace-builder-source-fragments\\]: source namespace 'rex_namespace' requires one complete typed opening and closing source-spelled fragment pair and an exact source order$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_missing_namespace_translation_unit_order_is_rejected
  --reject-missing-source-order
  "^REX_AST_INVARIANT\\[namespace-builder-source-fragments\\]: source namespace 'rex_namespace' requires one complete typed opening and closing source-spelled fragment pair and an exact source order$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_reinitialized_declaration_source_order_is_rejected
  --reject-reinitialized-declaration-source-order
  "^REX_AST_INVARIANT\\[translation-unit-source-order\\]: declaration=0x[[:xdigit:]]+ type=SgVariableDeclaration cannot initialize source_order=20 from current_order=10$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_missing_template_builder_scope_is_rejected
  --reject-missing-template-scope
  "^REX_AST_INVARIANT\\[class-builder-scope\\]: class 'rex_box' requires one explicit nonnull semantic scope$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_template_unrelated_namespace_scope_is_rejected
  --reject-template-unrelated-namespace-scope
  "^REX_AST_INVARIANT\\[template-class-exact-namespace-surface\\]: context=buildNondefiningTemplateClassDeclaration_nfi semantic-scope=0x[[:xdigit:]]+ lexical-scope=0x[[:xdigit:]]+ do not share one exact namespace/global ownership domain$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_template_definition_unrelated_scope_is_rejected
  --reject-template-definition-unrelated-scope
  "^REX_AST_INVARIANT\\[template-class-canonical-owner\\]: provided first nondefining declaration is not an attached, unresolved canonical declaration in the requested scope$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_template_definition_missing_symbol_is_rejected
  --reject-template-definition-missing-symbol
  "^REX_AST_INVARIANT\\[template-class-canonical-symbol\\]: name=rex_missing_symbol_template provided first declaration=0x[[:xdigit:]]+ does not own the one exact canonical symbol in scope=0x[[:xdigit:]]+$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_missing_namespace_builder_scope_is_rejected
  --reject-missing-namespace-scope
  "^REX_AST_INVARIANT\\[namespace-builder-scope\\]: namespace 'rex_namespace' requires one explicit nonnull semantic scope$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_generated_namespace_fragments_as_source_are_rejected
  --reject-generated-source-fragments
  "^REX_AST_INVARIANT\\[namespace-builder-source-fragments\\]: source namespace 'rex_namespace' requires one complete typed opening and closing source-spelled fragment pair and an exact source order$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_ambiguous_namespace_translation_unit_order_is_rejected
  --reject-ambiguous-namespace-order
  "^REX_AST_INVARIANT\\[namespace-scope-order\\]: declaration=0x[[:xdigit:]]+ and namespace=0x[[:xdigit:]]+ name=rex_duplicate_order have one ambiguous translation-unit order=10$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_missing_mixed_declaration_source_order_is_rejected
  --reject-missing-mixed-declaration-order
  "^REX_AST_INVARIANT\\[namespace-scope-order\\]: source declaration=0x[[:xdigit:]]+ type=SgVariableDeclaration has no exact producer-wide translation-unit order$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_null_function_parameter_list_is_rejected
  --reject-null-function-parameter-list
  "^REX_AST_INVARIANT\\[function-parameter-type-list\\]: parameter list is null; zero parameters require an explicit empty list$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_null_nfi_function_parameter_type_list_is_rejected
  --reject-null-nfi-function-parameter-type-list
  "^REX_AST_INVARIANT\\[function-parameter-type-list\\]: NFI parameter list construction requires one explicit non-null type list$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_pending_exact_source_without_provenance_is_rejected
  --reject-pending-exact-source-without-provenance
  "^REX_AST_INVARIANT\\[pending-exact-source-publication\\]: role=declaration node=0x[[:xdigit:]]+ has no complete exact physical source provenance$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_pending_exact_semantic_function_outside_frontend_is_rejected
  --reject-pending-exact-semantic-function-outside-frontend
  "^REX_AST_INVARIANT\\[function-builder-pending-exact-source\\]: semantic auxiliary pending exact source requires frontend construction mode$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_duplicate_pending_exact_source_publication_is_rejected
  --reject-duplicate-pending-exact-source-publication
  "^REX_AST_INVARIANT\\[pending-exact-source-publication\\]: declaration=0x[[:xdigit:]]+ scope=0x[[:xdigit:]]+ must be one detached declaration with one exact semantic scope before lexical publication$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_procedure_parameter_arity_mismatch_is_rejected
  --reject-procedure-parameter-arity
  "^REX_FORTRAN_BUILDER_INVARIANT\\[procedure-parameter-congruence\\]: procedure='rex_parameter_arity' index=0 definition and canonical parameter arities differ$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_procedure_parameter_name_mismatch_is_rejected
  --reject-procedure-parameter-name
  "^REX_FORTRAN_BUILDER_INVARIANT\\[procedure-parameter-congruence\\]: procedure='rex_parameter_name' index=0 definition and canonical parameter names differ$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_exact_procedure_parameter_type_mismatch_is_rejected
  --reject-exact-procedure-parameter-type
  "^REX_FORTRAN_BUILDER_INVARIANT\\[procedure-parameter-congruence\\]: procedure='rex_parameter_type' index=0 definition and canonical parameter types differ$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_null_function_source_group_is_rejected
  --reject-null-function-source-group
  "^REX_AST_INVARIANT\\[function-builder-group-owner\\]: source group ownership requires a non-null declaration group$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_null_typedef_source_group_is_rejected
  --reject-null-typedef-source-group
  "^REX_AST_INVARIANT\\[typedef-builder-group-owner\\]: source group ownership requires a non-null declaration group$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_null_source_lexical_owner_is_rejected
  --reject-null-source-lexical-owner
  "^REX_AST_INVARIANT\\[declaration-builder-lexical-owner\\]: explicit lexical ownership requires a non-null scope$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_derived_forward_lexical_owner_is_rejected
  --reject-derived-forward-lexical
  "^REX_AST_INVARIANT\\[derived-type-forward-ownership\\]: derived type=rex_invalid_lexical_forward scope=0x[[:xdigit:]]+ requires a valid semantic identity and exact auxiliary ownership$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_duplicate_derived_forward_is_rejected
  --reject-duplicate-derived-forward
  "^REX_AST_INVARIANT\\[derived-type-forward-conflict\\]: derived type=rex_duplicate_derived_forward scope=0x[[:xdigit:]]+ already has an exact local symbol$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_duplicate_derived_definition_is_rejected
  --reject-duplicate-derived-definition
  "^REX_AST_INVARIANT\\[derived-type-definition-conflict\\]: derived type=rex_duplicate_derived_definition scope=0x[[:xdigit:]]+ canonical=0x[[:xdigit:]]+ already has defining=0x[[:xdigit:]]+$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_ordinary_variable_embedded_tag_is_rejected
  --reject-ordinary-variable-for-embedded-tag
  "^REX_AST_INVARIANT\\[embedded-tag-variable-transaction\\]: variable=0x[[:xdigit:]]+ type=0x[[:xdigit:]]+ names pending definition=0x[[:xdigit:]]+ without the strict embedded-tag builder$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_ordinary_typedef_embedded_tag_is_rejected
  --reject-ordinary-typedef-for-embedded-tag
  "^REX_AST_INVARIANT\\[embedded-tag-typedef-transaction\\]: typedef=0x[[:xdigit:]]+ name=rex_pending_typedef base-type=0x[[:xdigit:]]+ pending-definition=0x[[:xdigit:]]+/SgClassDeclaration name=rex_pending_typedef_tag parent=\\(nil\\)/<null> scope=0x[[:xdigit:]]+ without the strict embedded-tag builder$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_mismatched_embedded_tag_type_is_rejected
  --reject-mismatched-embedded-tag-type
  "^REX_AST_INVARIANT\\[embedded-tag-type-identity\\]: context=buildVariableDeclarationWithEmbeddedTag base-type=0x[[:xdigit:]]+/SgTypeInt names definition=\\(nil\\), not supplied=0x[[:xdigit:]]+/SgClassDeclaration$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_null_variable_builder_scope_is_rejected
  --reject-null-variable-scope
  "^REX_AST_INVARIANT\\[variable-builder-scope\\]: declaration '\"rex_missing_scope\"' has no exact semantic scope$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_implicit_variable_redeclaration_is_rejected
  --reject-implicit-variable-redeclaration
  "^REX_AST_INVARIANT\\[variable-redeclaration-identity\\]: declaration '\"rex_implicit_redeclaration\"' collides with a same-scope symbol without explicit redeclaration identity$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_mismatched_variable_redeclaration_is_rejected
  --reject-mismatched-variable-redeclaration
  "^REX_AST_INVARIANT\\[variable-builder-redeclaration-identity\\]: declaration '\"rex_wrong_identity\"' does not match an exact prior declaration in its semantic scope$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_borrowed_variable_initializer_is_rejected
  --reject-borrowed-variable-initializer
  "^REX_AST_INVARIANT\\[variable-builder-initializer-owner\\]: declaration '\"rex_initializer_borrower\"' cannot borrow an initializer owned by SgInitializedName$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_parent_only_auxiliary_declaration_is_rejected
  --reject-parent-only-auxiliary-declaration
  "^\\[REX-AST-MALFORMED-AUXILIARY-DECLARATION\\] declaration has a parent without an exact auxiliary child-list edge$")
add_rex_sagebuilder_typed_ownership_death_test(
  astInterface_rex_declaration_scope_transfer_is_rejected
  --reject-declaration-scope-transfer
  "^REX_AST_INVARIANT\\[function-declarator-scope-owner\\]: signature scope must be detached before adoption$")

add_test(
  NAME astInterface_rex_required_null_expression_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0 && \
     log=astInterface_rex_required_null_expression_is_rejected.log && \
     rm -f \"\$log\" && \
     { set +e; REX_TEST_REJECT_REQUIRED_NULL_EXPRESSION=1 \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; } && \
     test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[null-expression-required\\]: SgNullExpression marked as syntactically absent is owned by required context 0x[[:xdigit:]]+/SgAssignInitializer$' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_test2026_null_expression_unparse>
    ${TEST_CXXFLAGS}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_null_expression_unparse.C)
set_tests_properties(
  astInterface_rex_required_null_expression_is_rejected
  PROPERTIES LABELS "astInterface;rex;death")

add_test(
  NAME astInterface_rex_empty_named_struct_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0 && \
     log=astInterface_rex_empty_named_struct_is_rejected.log && \
     rm -f \"\$log\" && \
     { set +e; REX_TEST_REJECT_EMPTY_NAMED_STRUCT=1 \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; } && \
     test \"\$(grep -Fxc 'REX_AST_INVARIANT[struct-declaration-name]: named structure builder requires a source name; use buildAnonymousStructDeclaration with an explicit internal identity' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:buildStructDeclaration2>
    ${TEST_CXXFLAGS}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/inputbuildStructDeclaration2.C)
set_tests_properties(
  astInterface_rex_empty_named_struct_is_rejected
  PROPERTIES LABELS "astInterface;rex;death")

add_test(
  NAME astInterface_rex_null_label_child_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0 && \
     log=astInterface_rex_null_label_child_is_rejected.log && \
     rm -f \"\$log\" && \
     { set +e; REX_TEST_REJECT_NULL_LABEL_CHILD=1 \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; } && \
     test \"\$(grep -Ec \"^REX_AST_INVARIANT\\[label-statement-child\\]: label ['']\\\"invalid\\\"[''] requires an explicit labeled statement$\" \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:buildLabelStatement>
    ${TEST_CXXFLAGS}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/inputbuildLabelStatement.C)
set_tests_properties(
  astInterface_rex_null_label_child_is_rejected
  PROPERTIES LABELS "astInterface;rex;death")

add_test(
  NAME astInterface_rex_out_of_class_member_prototype_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0 && \
     log=astInterface_rex_out_of_class_member_prototype_is_rejected.log && \
     rm -f \"\$log\" && \
     { set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; } && \
     test \"\$(grep -Ec '^REX_AST_INVARIANT\\[function-prototype-owner\\]: member=0x[[:xdigit:]]+ name=rex_out_of_line_member_definition is structurally owned outside its semantic class scope; a qualified out-of-class member prototype is ill-formed C\\+\\+$' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_function_prototype_chain>
    --reject-out-of-class-member-prototype
    ${TEST_CXXFLAGS}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_function_prototype_chain_input.C)
set_tests_properties(
  astInterface_rex_out_of_class_member_prototype_is_rejected
  PROPERTIES LABELS "astInterface;rex;death")

add_test(
  NAME astInterface_rex_nonempty_symbol_table_rebuild_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0 && \
     log=astInterface_rex_nonempty_symbol_table_rebuild_is_rejected.log && \
     rm -f \"\$log\" && \
     { set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; } && \
     test \"\$(grep -Ec '^.*FAIL : ASSERTION:require:[[:space:]]+\\[[^]]*/sageInterface\\.C:[0-9]+,[[:space:]]+rebuildSymbolTable\\]:[[:space:]]+.*symbolTable->size\\(\\) == 0$' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_function_prototype_chain>
    --reject-nonempty-symbol-rebuild
    ${TEST_CXXFLAGS}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_function_prototype_chain_input.C)
set_tests_properties(
  astInterface_rex_nonempty_symbol_table_rebuild_is_rejected
  PROPERTIES LABELS "astInterface;rex;death")

add_test(
  NAME astInterface_rex_copied_semantic_function_identity
  COMMAND $<TARGET_FILE:rex_function_prototype_chain>
    --require-copied-semantic-function-identity
    ${TEST_CXXFLAGS}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_function_prototype_chain_input.C)
set_tests_properties(
  astInterface_rex_copied_semantic_function_identity
  PROPERTIES LABELS "astInterface;rex")

add_test(
  NAME astInterface_rex_complete_function_chain_contract
  COMMAND $<TARGET_FILE:rex_function_prototype_chain>
    --require-complete-function-chain
    ${TEST_CXXFLAGS}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_function_prototype_chain_input.C)
set_tests_properties(
  astInterface_rex_complete_function_chain_contract
  PROPERTIES LABELS "astInterface;rex")

function(add_rex_function_chain_death_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; diagnostic=\"\$1\"; shift; set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec -- \"\$diagnostic\" \"\$log\")\" -eq 1"
      dummy
      "${diagnostic}"
      $<TARGET_FILE:rex_function_prototype_chain>
      ${mode}
      ${TEST_CXXFLAGS}
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_function_prototype_chain_input.C)
  set_tests_properties(${test_name}
    PROPERTIES LABELS "astInterface;rex;death")
endfunction()

add_rex_function_chain_death_test(
  astInterface_rex_function_chain_missing_scope_is_rejected
  --reject-function-chain-missing-scope
  "^REX_AST_INVARIANT\\[function-declaration-chain\\]: function 'rex_free_definition' first-nondefining declaration has no semantic scope$")
add_rex_function_chain_death_test(
  astInterface_rex_function_chain_missing_owner_is_rejected
  --reject-function-chain-missing-owner
  "^REX_AST_INVARIANT\\[function-declaration-chain\\]: function 'rex_free_definition' first-nondefining declaration has no exact typed structural owner$")
add_rex_function_chain_death_test(
  astInterface_rex_function_chain_wrong_scope_is_rejected
  --reject-function-chain-wrong-scope
  "^REX_AST_INVARIANT\\[function-declaration-chain\\]: function 'rex_free_definition' first-nondefining declaration semantic scope=0x[[:xdigit:]]+ does not match exact or canonical namespace scope=0x[[:xdigit:]]+$")
add_rex_function_chain_death_test(
  astInterface_rex_function_chain_wrong_owner_is_rejected
  --reject-function-chain-wrong-owner
  "^REX_AST_INVARIANT\\[function-declaration-chain\\]: function 'rex_free_definition' first-nondefining declaration has no exact typed structural owner$")
add_rex_function_chain_death_test(
  astInterface_rex_function_chain_broken_reciprocal_is_rejected
  --reject-function-chain-broken-reciprocal
  "^REX_AST_INVARIANT\\[function-declaration-chain\\]: function 'rex_free_definition' first-nondefining declaration has non-reciprocal canonical links$")

set_tests_properties(astInterface_interfaceFunctionCoverage PROPERTIES FAIL_REGULAR_EXPRESSION "In addToNameMap\(\): Allow this for replacement now!;Default reached in SgScopeStatement::isNamedScope" )

set(_disable_interface_coverage FALSE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.5")
  set(_disable_interface_coverage TRUE)
endif()
if(_disable_interface_coverage)
endif()

# buildFunctionDeclaration test (uses inputBlank1.C)
add_test(
  NAME astInterface_buildFunctionDeclaration
  COMMAND buildFunctionDeclaration ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputBlank1.C
)

# buildNondefiningFunction test (uses inputBlank2.C)
add_test(
  NAME astInterface_buildNondefiningFunction
  COMMAND buildNondefiningFunction ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputBlank2.C
)

# buildFunctionCalls test (needs -I flag)
add_test(
  NAME astInterface_buildFunctionCalls
  COMMAND buildFunctionCalls ${TEST_CXXFLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputbuildFunctionCalls.C
)

# buildPragmaDeclaration test (C file, not C++)
add_test(
  NAME astInterface_buildPragmaDeclaration
  COMMAND buildPragmaDeclaration ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputbuildPragmaDeclaration.c
)

# Special named input files
add_test(
  NAME astInterface_attachComment
  COMMAND attachComment ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputAttachComment.C
)

add_test(
  NAME astInterface_rex_test2026_template_constraints_ir
  COMMAND rex_test2026_template_constraints_ir ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_template_constraints_ir.C
)

add_test(
  NAME astInterface_rex_test2026_sfinae_prune
  COMMAND rex_test2026_sfinae_prune ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_sfinae_prune.C
)

add_test(
  NAME astInterface_rex_test2026_fold_pack_expr
  COMMAND rex_test2026_fold_pack_expr ${TEST_CXXFLAGS} -std=c++17 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_fold_pack_expr.C
)

add_test(
  NAME astInterface_rex_test2026_stl_vector
  COMMAND rex_test2026_stl_vector ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_stl_vector.C
)

add_test(
  NAME astInterface_rex_test2026_stl_tuple
  COMMAND rex_test2026_stl_tuple ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_stl_tuple.C
)

add_test(
  NAME astInterface_rex_test2026_stl_type_traits
  COMMAND rex_test2026_stl_type_traits ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_stl_type_traits.C
)

add_test(
  NAME astInterface_rex_test2026_type_trait_constant_expr
  COMMAND rex_test2026_type_trait_constant_expr ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_type_trait_constant_expr.C
)

add_test(
  NAME astInterface_rex_test2026_stl_variant
  COMMAND rex_test2026_stl_variant ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_stl_variant.C
)

add_test(
  NAME astInterface_rex_test2026_stl_regex
  COMMAND rex_test2026_stl_regex ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputrex_test2026_stl_regex.C
)

add_test(
  NAME astInterface_rex_template_instantiation_memcheck
  COMMAND rex_test2026_stl_regex ${TEST_CXXFLAGS} -std=c++20 -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_interface_template_instantiation_memcheck.cpp
)

add_test(
  NAME astInterface_insertHeader
  COMMAND insertHeader ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputInsertHeader.C
)

add_test(
  NAME astInterface_lookupNamedType
  COMMAND lookupNamedType ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputLookupNamedType.C
)

add_test(
  NAME astInterface_movePreprocessingInfo
  COMMAND movePreprocessingInfo ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputMovePreprocessingInfo.C
)

# abiStuffTest (doesn't produce C file)
add_test(
  NAME astInterface_abiStuffTest
  COMMAND abiStuffTest ${CMAKE_CURRENT_SOURCE_DIR}/inputAbiStuffTest.c
)

# deepDelete test (doesn't create output file)
add_test(
  NAME astInterface_deepDelete
  COMMAND deepDelete ${TEST_CXXFLAGS} -rose:detect_dangling_pointers 1 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputdeepDelete.C
)
set(_disable_deep_delete FALSE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9")
  set(_disable_deep_delete TRUE)
endif()
if(_disable_deep_delete)
endif()

# loopUnrolling tests (multiple tests with different parameters)
add_test(
  NAME astInterface_loopUnrolling
  COMMAND loopUnrolling ${TEST_CXXFLAGS} -rose:loopunroll:line 5 -rose:loopunroll:factor 3 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopUnrolling.C
)

add_test(
  NAME astInterface_loopUnrolling1
  COMMAND loopUnrolling ${TEST_CXXFLAGS} -rose:loopunroll:line 6 -rose:loopunroll:factor 5 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopUnrolling.C -rose:output rose_inputloopUnrolling1.C
)

add_test(
  NAME astInterface_loopUnrolling2
  COMMAND loopUnrolling ${TEST_CXXFLAGS} -rose:loopunroll:line 6 -rose:loopunroll:factor 3 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopUnrolling.C -rose:output rose_inputloopUnrolling2.C
)

# loopCollapsing tests (multiple tests with different parameters)
add_test(
  NAME astInterface_loopCollapsing_1
  COMMAND loopCollapsing ${TEST_CXXFLAGS} -rose:loopcollapse:line 5 -rose:loopcollapse:factor 2 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopCollapsing_1.C
)

add_test(
  NAME astInterface_loopCollapsing_2
  COMMAND loopCollapsing ${TEST_CXXFLAGS} -rose:loopcollapse:line 9 -rose:loopcollapse:factor 2 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopCollapsing_2.C
)

add_test(
  NAME astInterface_loopCollapsing_3
  COMMAND loopCollapsing ${TEST_CXXFLAGS} -rose:loopcollapse:line 9 -rose:loopcollapse:factor 3 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopCollapsing_3.C
)

add_test(
  NAME astInterface_loopCollapsing_4
  COMMAND loopCollapsing ${TEST_CXXFLAGS} -rose:loopcollapse:line 9 -rose:loopcollapse:factor 4 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopCollapsing_4.C
)

add_test(
  NAME astInterface_loopCollapsing_5
  COMMAND loopCollapsing ${TEST_CXXFLAGS} -rose:loopcollapse:line 9 -rose:loopcollapse:factor 5 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopCollapsing_5.C
)

# loopInterchange test
add_test(
  NAME astInterface_loopInterchange
  COMMAND loopInterchange ${TEST_CXXFLAGS} -rose:loopInterchange:line 5 -rose:loopInterchange:depth 4 -rose:loopInterchange:order 1 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopInterchange.C
)

# loopTiling test
add_test(
  NAME astInterface_loopTiling
  COMMAND loopTiling -rose:loopTiling:line 7 -rose:loopTiling:depth 3 -rose:loopTiling:tilesize 5 ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopTiling.C
)

add_test(
  NAME astInterface_rex_loop_tiling_identity
  COMMAND loopTiling -rose:loopTiling:line 7 -rose:loopTiling:depth 3 -rose:loopTiling:tilesize 1 ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputloopTiling.C
)
set_tests_properties(astInterface_rex_loop_tiling_identity
  PROPERTIES LABELS "rex")

add_test(
  NAME astInterface_rex_loop_tiling_zero_size_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=rex_loop_tiling_zero_size.log; rm -f \"$log\"; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '^REX_AST_INVARIANT\\[loop-tiling-size\\]: operation=loop-tiling level=1 tile-size=0 is not one positive exact counter value$' \"$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:loopTiling>
    -rose:loopTiling:line 7
    -rose:loopTiling:depth 3
    -rose:loopTiling:tilesize 0
    ${TEST_CXXFLAGS}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/inputloopTiling.C
)
set_tests_properties(astInterface_rex_loop_tiling_zero_size_is_rejected
  PROPERTIES LABELS "rex;death")

add_test(
  NAME astInterface_rex_loop_normalization_declaration_group_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=rex_loop_normalization_declaration_group.log; rm -f \"$log\"; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '^REX_AST_INVARIANT\\[loop-plan-canonical\\]: operation=for-loop-normalization loop=0x[[:xdigit:]]+ is not an exact canonical C/C\\+\\+ loop$' \"$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:loopNormalization>
    ${TEST_CXXFLAGS}
    -std=c++20
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_loop_normalization_declaration_group.cpp
)
set_tests_properties(
  astInterface_rex_loop_normalization_declaration_group_is_rejected
  PROPERTIES LABELS "rex;death")

set(_rex_loop_core_input
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_loop_core_contracts_input.cpp)

foreach(_mode IN ITEMS runtime checked_plan_api unroll_identity assignment)
  add_test(
    NAME astInterface_rex_loop_core_${_mode}
    COMMAND ${BASH_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/run_rex_loop_core_runtime.sh
      ${_mode}
      $<TARGET_FILE:rex_loop_core_contracts>
      ${CMAKE_CXX_COMPILER}
      ${_rex_loop_core_input}
      ${_ast_interface_output_dir}/rex_loop_core_${_mode}.cpp
      ${_ast_interface_output_dir}/rex_loop_core_${_mode}.exe
      ${TEST_CXXFLAGS}
    )
  set_tests_properties(astInterface_rex_loop_core_${_mode}
    PROPERTIES LABELS "astInterface;rex;runtime")
endforeach()

function(add_rex_loop_core_death_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=${test_name}.log; mode=$1; diagnostic=$2; shift 2; rm -f \"$log\"; set +e; env REX_LOOP_CORE_MODE=$mode \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec -- \"$diagnostic\" \"$log\")\" -eq 1"
      dummy
      ${mode}
      "${diagnostic}"
      $<TARGET_FILE:rex_loop_core_contracts>
      ${TEST_CXXFLAGS}
      -std=c++20
      -c
      ${_rex_loop_core_input})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "astInterface;rex;death")
endfunction()

add_rex_loop_core_death_test(
  astInterface_rex_loop_unrolling_zero_is_rejected
  unroll_zero
  "^REX_AST_INVARIANT\\[loop-unrolling-factor\\]: operation=loop-unrolling factor=0 requires one positive exactly generatable factor$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_unrolling_overflow_is_rejected
  unroll_overflow
  "^REX_AST_INVARIANT\\[loop-unrolling-factor\\]: operation=loop-unrolling factor=18446744073709551615 requires one positive exactly generatable factor$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_unrolling_continue_is_rejected
  unroll_continue
  "^REX_AST_INVARIANT\\[loop-unrolling-continue\\]: loop=0x[[:xdigit:]]+ contains a continue targeting the loop; grouped unrolling cannot preserve that control edge$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_unrolling_label_is_rejected
  unroll_label
  "^REX_AST_INVARIANT\\[loop-unrolling-label\\]: loop=0x[[:xdigit:]]+ contains a label or goto whose exact function-scope identity cannot be duplicated$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_tiling_zero_is_rejected
  tiling_zero
  "^REX_AST_INVARIANT\\[loop-tiling-size\\]: operation=loop-tiling level=1 tile-size=0 is not one positive exact counter value$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_tiling_imperfect_nest_is_rejected
  tiling_imperfect
  "^REX_AST_INVARIANT\\[loop-tiling-nest\\]: loop=0x[[:xdigit:]]+ at level=1 does not have one exact nested-loop statement$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_tiling_break_is_rejected
  tiling_break
  "^REX_AST_INVARIANT\\[loop-plan-canonical\\]: operation=loop-tiling loop=0x[[:xdigit:]]+ is not an exact canonical C/C\\+\\+ loop$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_zero_stride_is_rejected
  zero_stride
  "^REX_AST_INVARIANT\\[loop-plan-stride\\]: operation=loop-unrolling loop=0x[[:xdigit:]]+ requires an exact positive nonzero constant stride magnitude$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_negative_stride_is_rejected
  negative_stride
  "^REX_AST_INVARIANT\\[loop-plan-stride\\]: operation=loop-unrolling loop=0x[[:xdigit:]]+ requires an exact positive nonzero constant stride magnitude$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_dynamic_stride_is_rejected
  dynamic_stride
  "^REX_AST_INVARIANT\\[loop-plan-stride\\]: operation=loop-unrolling loop=0x[[:xdigit:]]+ requires an exact positive nonzero constant stride magnitude$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_induction_write_is_rejected
  induction_write
  "^REX_AST_INVARIANT\\[loop-plan-invariance\\]: operation=loop-unrolling variable=index is written, address-exposed, captured, or passed from the loop body$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_induction_address_is_rejected
  induction_address
  "^REX_AST_INVARIANT\\[loop-plan-invariance\\]: operation=loop-unrolling variable=index is written, address-exposed, captured, or passed from the loop body$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_induction_call_is_rejected
  induction_call
  "^REX_AST_INVARIANT\\[loop-plan-invariance\\]: operation=loop-unrolling variable=index is written, address-exposed, captured, or passed from the loop body$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_bound_write_is_rejected
  bound_write
  "^REX_AST_INVARIANT\\[loop-plan-invariance\\]: operation=loop-unrolling variable=limit is written, address-exposed, captured, or passed from the loop body$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_direction_mismatch_is_rejected
  direction_mismatch
  "^REX_AST_INVARIANT\\[loop-plan-canonical\\]: operation=for-loop-normalization loop=0x[[:xdigit:]]+ is not an exact canonical C/C\\+\\+ loop$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_assignment_increment_is_rejected_before_mutation
  assignment_malformed
  "^REX_AST_INVARIANT\\[loop-plan-canonical\\]: operation=for-loop-normalization loop=0x[[:xdigit:]]+ is not an exact canonical C/C\\+\\+ loop$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_duplicate_owner_is_rejected
  owner_duplicate
  "^REX_AST_INVARIANT\\[traversal-duplicate-successor\\]: duplicate successor identity in SgBasicBlock.p_statements$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_stale_trip_count_is_rejected
  stale_trip_count
  "^REX_AST_INVARIANT\\[loop-plan-stale\\]: constant trip-count plan for loop=0x[[:xdigit:]]+ changed before evaluation$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_null_plan_request_is_rejected
  canonical_null
  "^REX_AST_INVARIANT\\[loop-plan-owner\\]: operation=null-loop-death-test loop=\\(nil\\) requires a non-null input$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_empty_tiling_request_is_rejected
  tiling_empty
  "^REX_AST_INVARIANT\\[loop-tiling-request\\]: operation=empty-tiling-death-test outer=0x[[:xdigit:]]+ requires one nonempty perfect-nest plan$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_stale_unroll_plan_is_rejected
  stale_unroll
  "^REX_AST_INVARIANT\\[loop-plan-stale\\]: operation=loop-unrolling-commit loop=0x[[:xdigit:]]+ changed between read-only planning and commit$")
add_rex_loop_core_death_test(
  astInterface_rex_loop_stale_tiling_plan_is_rejected
  stale_tiling
  "^REX_AST_INVARIANT\\[loop-plan-stale\\]: loop tiling level=1 changed before commit$")

# insertStatementBeforeFunction test (needs -I flag)
add_test(
  NAME astInterface_insertStatementBeforeFunction
  COMMAND insertStatementBeforeFunction ${TEST_CXXFLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/inputinsertStatementBeforeFunction.C
)

# removeStatementCommentRelocation tests (multiple variants)
set(comment_relocation_tests
  inputRemoveStatementCommentRelocation
  inputRemoveStatementCommentRelocation_withTopBoundingStatement
  inputRemoveStatementCommentRelocation_withoutTopBoundingStatement
  inputRemoveStatementCommentRelocation_withBottomBoundingStatement
  inputRemoveStatementCommentRelocation_withoutBottomBoundingStatement
  inputRemoveStatementCommentRelocation_withTopandBottomBoundingStatement
  inputRemoveStatementCommentRelocation_withoutTopandBottomBoundingStatement
)

foreach(test ${comment_relocation_tests})
  add_test(
    NAME astInterface_${test}
    COMMAND removeStatementCommentRelocation ${TEST_CXXFLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/${test}.C
  )
endforeach()

# templateQualification test - exercises template qualification correctness.
# Tests that:
#   1. Non-type template parameters preserve names (no "template<0>")
#   2. No redundant global qualifiers (no "::std::vector")
# Uses test_template_qualification.cpp as input
add_test(
  NAME astInterface_templateQualification
  COMMAND templateQualification ${TEST_CXXFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/test_template_qualification.cpp
)

add_test(
  NAME rex_sagebuilder_generated_source
  WORKING_DIRECTORY ${_ast_interface_output_dir}
  COMMAND rex_sagebuilder_generated_source
          rex_sagebuilder_generated_source_output.cpp
)

add_test(
  NAME rex_unparser_subtree_context
  COMMAND rex_unparser_subtree_context ${TEST_CXXFLAGS} -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_subtree_context_input.cpp
)

add_test(
  NAME astInterface_rex_name_qualification_c_system_function
  WORKING_DIRECTORY ${_ast_interface_output_dir}
  COMMAND $<TARGET_FILE:identityTranslator> -rose:skipfinalCompileStep
          ${TEST_CXXFLAGS} -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_name_qualification_system_function.c
)
set_tests_properties(astInterface_rex_name_qualification_c_system_function
  PROPERTIES LABELS "astInterface;rex;unparser")

# Fortran tests (only if Fortran support is enabled)
if(ENABLE-FORTRAN)
  add_test(
    NAME astInterface_rex_name_qualification_fortran_semantic_function
    WORKING_DIRECTORY ${_ast_interface_fortran_output_dir}
    COMMAND $<TARGET_FILE:identityTranslator> -rose:skipfinalCompileStep
            ${TEST_CXXFLAGS} ${_ast_interface_fortran_flags} -c
            ${CMAKE_CURRENT_SOURCE_DIR}/rex_name_qualification_semantic_function.f90
  )
  set_tests_properties(
    astInterface_rex_name_qualification_fortran_semantic_function
    PROPERTIES LABELS "astInterface;rex;unparser")

  foreach(test ${fortran_programs})
    add_test(
      NAME astInterface_${test}
      WORKING_DIRECTORY ${_ast_interface_fortran_output_dir}
      COMMAND ${test} ${TEST_CXXFLAGS} ${_ast_interface_fortran_flags}
              -c ${CMAKE_CURRENT_SOURCE_DIR}/input${test}.f
    )
  endforeach()
endif()

# Add subdirectories if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/typeEquivalenceTests/CMakeLists.txt)
  add_subdirectory(typeEquivalenceTests)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/CMakeLists.txt)
  add_subdirectory(unitTests)
endif()

get_property(_ast_interface_registered_tests DIRECTORY PROPERTY TESTS)
foreach(_test IN LISTS _ast_interface_registered_tests)
  if(_test MATCHES "^astInterface_")
    set_property(TEST ${_test} APPEND PROPERTY ENVIRONMENT
                 "ROSE_TEST_OUTPUT_DIR=${_ast_interface_output_dir}")
  endif()
endforeach()
