include_directories(${ROSE_INCLUDES})

set(_test_support_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/testSupport)
set(_gtest_include
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/testSupport/gtest/include)

add_executable(nodeHierarchyTest
  symbolTableUnitTests.cpp
  functionTypeTests.cpp
  rex_function_type_argument_qualification_contract.cpp)
target_include_directories(nodeHierarchyTest PRIVATE
  ${_test_support_dir}
  ${_gtest_include})
target_link_libraries(nodeHierarchyTest
  roseUnitTestSupport
  gtest_main
  gtest
  ROSE_DLL
  ${link_with_libraries})

add_test(
  NAME rosetta_nodeHierarchyTest
  COMMAND $<TARGET_FILE:nodeHierarchyTest>
)
set_tests_properties(rosetta_nodeHierarchyTest
  PROPERTIES LABELS "ROSETTA;death")

add_executable(rex_rosetta_hard_error_contract
  rex_rosetta_hard_error_contract.cpp)
target_link_libraries(rex_rosetta_hard_error_contract
  ROSE_DLL
  ${link_with_libraries})

add_executable(rex_rosetta_traversal_consistency
  rex_rosetta_traversal_consistency.cpp)
target_link_libraries(rex_rosetta_traversal_consistency
  ROSE_DLL
  ${link_with_libraries})

function(add_rosetta_hard_error_contract_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0 && \
       log=${test_name}.log && \
       rm -f \"\$log\" && \
       { set +e; \"\$1\" \"\$2\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; } && \
       test \"\$(grep -Fxc \"\$3\" \"\$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_rosetta_hard_error_contract>
      ${mode}
      ${diagnostic})
  set_tests_properties(${test_name} PROPERTIES LABELS "rex;ROSETTA;death")
endfunction()

function(add_rosetta_hard_error_regex_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0 && \
       log=${test_name}.log && \
       rm -f \"\$log\" && \
       { set +e; \"\$1\" \"\$2\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; } && \
       test \"\$(grep -Ec \"\$3\" \"\$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_rosetta_hard_error_contract>
      ${mode}
      ${diagnostic})
  set_tests_properties(${test_name} PROPERTIES LABELS "rex;ROSETTA;death")
endfunction()

add_rosetta_hard_error_contract_test(
  rosetta_rex_function_argument_index_is_rejected
  function-argument-index
  "REX_AST_INVARIANT[function-argument-qualification-use-site]: insertion index 1 exceeds argument count 0")
add_rosetta_hard_error_contract_test(
  rosetta_rex_null_symbol_insert_is_rejected
  null-symbol
  "REX_AST_INVARIANT[symbol-table-insert]: symbol table cannot insert null symbol for name foo")
add_rosetta_hard_error_contract_test(
  rosetta_rex_declaration_group_replace_is_rejected
  declaration-group-replace
  "REX_AST_INVARIANT[declaration-group-mutation]: replace_child cannot preserve an exact-source declaration group")
add_rosetta_hard_error_contract_test(
  rosetta_rex_declaration_group_insert_is_rejected
  declaration-group-insert
  "REX_AST_INVARIANT[declaration-group-mutation]: insert_child cannot preserve an exact-source declaration group")
add_rosetta_hard_error_contract_test(
  rosetta_rex_declaration_group_remove_member_is_rejected
  declaration-group-remove-member
  "REX_AST_INVARIANT[declaration-group-mutation]: removeStatement cannot mutate an individual member of an exact source declaration, mutate the SgDeclarationGroupStatement atomically")
add_rosetta_hard_error_contract_test(
  rosetta_rex_declaration_group_replace_member_is_rejected
  declaration-group-replace-member
  "REX_AST_INVARIANT[declaration-group-mutation]: replaceStatement cannot mutate an individual member of an exact source declaration, mutate the SgDeclarationGroupStatement atomically")
add_rosetta_hard_error_contract_test(
  rosetta_rex_declaration_group_insert_at_member_is_rejected
  declaration-group-insert-at-member
  "REX_AST_INVARIANT[declaration-group-mutation]: insertStatement cannot mutate an individual member of an exact source declaration, mutate the SgDeclarationGroupStatement atomically")
add_rosetta_hard_error_contract_test(
  rosetta_rex_class_type_missing_first_owner_is_rejected
  class-type-missing-first-owner
  "REX_AST_INVARIANT[class-type-canonical-owner]: class declaration has no typed first nondefining declaration")
add_rosetta_hard_error_contract_test(
  rosetta_rex_class_type_later_owner_is_rejected
  class-type-later-owner
  "REX_AST_INVARIANT[class-type-canonical-owner]: createType must be called with the canonical first nondefining declaration")
add_rosetta_hard_error_contract_test(
  rosetta_rex_class_type_missing_scope_is_rejected
  class-type-missing-scope
  "REX_AST_INVARIANT[class-type-canonical-owner]: canonical first nondefining declaration has no semantic scope")
add_rosetta_hard_error_regex_test(
  rosetta_rex_declaration_null_start_info_is_rejected
  declaration-start-null
  "^REX_AST_INVARIANT\\[declaration-start-info-owner\\]: node=\\(nil\\) type=<null> expected-parent=0x[[:xdigit:]]+ type=SgAccessLabelStatement actual-parent=\\(nil\\) type=<null> direct-edges=0$")
add_rosetta_hard_error_regex_test(
  rosetta_rex_declaration_null_end_info_is_rejected
  declaration-end-null
  "^REX_AST_INVARIANT\\[declaration-end-info-owner\\]: node=\\(nil\\) type=<null> expected-parent=0x[[:xdigit:]]+ type=SgAccessLabelStatement actual-parent=\\(nil\\) type=<null> direct-edges=0$")
add_rosetta_hard_error_regex_test(
  rosetta_rex_declaration_foreign_start_info_owner_is_rejected
  declaration-start-foreign-owner
  "^REX_AST_INVARIANT\\[declaration-start-info-owner\\]: node=0x[[:xdigit:]]+ type=Sg_File_Info expected-parent=0x[[:xdigit:]]+ type=SgAccessLabelStatement actual-parent=0x[[:xdigit:]]+ type=SgBasicBlock direct-edges=1$")
add_rosetta_hard_error_regex_test(
  rosetta_rex_declaration_foreign_end_info_owner_is_rejected
  declaration-end-foreign-owner
  "^REX_AST_INVARIANT\\[declaration-end-info-owner\\]: node=0x[[:xdigit:]]+ type=Sg_File_Info expected-parent=0x[[:xdigit:]]+ type=SgAccessLabelStatement actual-parent=0x[[:xdigit:]]+ type=SgBasicBlock direct-edges=1$")
add_rosetta_hard_error_contract_test(
  rosetta_rex_named_type_null_declaration_is_rejected
  named-type-null-declaration
  "REX_AST_INVARIANT[named-type-declaration-owner]: node=(nil) type=<null> expected-parent=(nil) type=<null> actual-parent=(nil) type=<null> direct-edges=0")
add_rosetta_hard_error_contract_test(
  rosetta_rex_variable_inline_null_owner_is_rejected
  variable-inline-null-owner
  "REX_AST_INVARIANT[variable-inline-type-owner]: null definition is not a valid ownership publication")
add_rosetta_hard_error_regex_test(
  rosetta_rex_access_label_conflicting_legacy_is_rejected
  access-label-conflicting-legacy
  "^REX_AST_INVARIANT\\[access-label-shape\\]: label=0x[[:xdigit:]]+ kind=2 legacy-access=3 explicit=1 defining=\\(nil\\) first=\\(nil\\)$")
add_rosetta_hard_error_contract_test(
  rosetta_rex_basic_block_extraction_foreign_child_is_rejected
  basic-block-extraction-foreign-child
  "REX_AST_INVARIANT[basic-block-extraction]: source block contains a child without exact structural ownership")
add_rosetta_hard_error_contract_test(
  rosetta_rex_basic_block_extraction_null_child_is_rejected
  basic-block-extraction-null-child
  "REX_AST_INVARIANT[basic-block-extraction]: source block contains a child without exact structural ownership")
add_rosetta_hard_error_contract_test(
  rosetta_rex_basic_block_extraction_duplicate_child_is_rejected
  basic-block-extraction-duplicate-child
  "REX_AST_INVARIANT[basic-block-extraction]: source block contains a duplicate child")
add_rosetta_hard_error_contract_test(
  rosetta_rex_basic_block_extraction_destination_shared_child_is_rejected
  basic-block-extraction-destination-shared-child
  "REX_AST_INVARIANT[basic-block-extraction]: source and destination containers already share a child")
add_rosetta_hard_error_regex_test(
  rosetta_rex_binary_foreign_replacement_is_rejected
  binary-foreign-replacement
  "^REX_AST_INVARIANT\\[binary-replacement-edge\\]: expression=SgAddOp does not own the requested old operand$")
add_rosetta_hard_error_regex_test(
  rosetta_rex_unary_foreign_replacement_is_rejected
  unary-foreign-replacement
  "^REX_AST_INVARIANT\\[unary-replacement-edge\\]: expression=SgMinusOp does not own the requested old operand$")
add_rosetta_hard_error_regex_test(
  rosetta_rex_vararg_foreign_replacement_is_rejected
  vararg-foreign-replacement
  "^REX_AST_INVARIANT\\[vararg-replacement-edge\\]: SgVarArgOp does not own the requested old operand$")

add_test(
  NAME rosetta_rex_invalid_traversal_successor_index_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0 && \
     log=rosetta_rex_invalid_traversal_successor_index_is_rejected.log && \
     rm -f \"\$log\" && \
     { set +e; \"\$1\" invalid-successor-index >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; } && \
     test \"\$(grep -Fxc 'REX_AST_INVARIANT[traversal-successor-index]: invalid index 0 for SgOmpContextSelector' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_rosetta_traversal_consistency>)
set_tests_properties(
  rosetta_rex_invalid_traversal_successor_index_is_rejected
  PROPERTIES LABELS "rex;ROSETTA;death")

add_test(
  NAME rosetta_rex_null_traversal_child_index_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0 && \
     log=rosetta_rex_null_traversal_child_index_is_rejected.log && \
     rm -f \"\$log\" && \
     { set +e; \"\$1\" null-child-index >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; } && \
     test \"\$(grep -Fxc 'REX_AST_INVARIANT[traversal-child-index]: null child query for SgOmpContextSelector' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_rosetta_traversal_consistency>)
set_tests_properties(
  rosetta_rex_null_traversal_child_index_is_rejected
  PROPERTIES LABELS "rex;ROSETTA;death")

add_test(
  NAME rosetta_rex_foreign_traversal_child_index_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0 && \
     log=rosetta_rex_foreign_traversal_child_index_is_rejected.log && \
     rm -f \"\$log\" && \
     { set +e; \"\$1\" foreign-child-index >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; } && \
     test \"\$(grep -Fxc 'REX_AST_INVARIANT[traversal-child-index]: foreign child query for SgOmpContextSelector' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_rosetta_traversal_consistency>)
set_tests_properties(
  rosetta_rex_foreign_traversal_child_index_is_rejected
  PROPERTIES LABELS "rex;ROSETTA;death")

function(add_rosetta_traversal_death_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0 && \
       log=${test_name}.log && \
       rm -f \"\$log\" && \
       { set +e; \"\$1\" \"\$2\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; } && \
       test \"\$(grep -Fxc \"\$3\" \"\$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_rosetta_traversal_consistency>
      ${mode}
      ${diagnostic})
  set_tests_properties(${test_name} PROPERTIES LABELS "rex;ROSETTA;death")
endfunction()

add_rosetta_traversal_death_test(
  rosetta_rex_required_null_traversal_successor_is_rejected
  required-null-successor
  "REX_AST_INVARIANT[traversal-required-successor]: required successor SgAddOp.p_lhs_operand_i is null")
add_rosetta_traversal_death_test(
  rosetta_rex_null_container_traversal_successor_is_rejected
  null-container-successor
  "REX_AST_INVARIANT[traversal-null-successor]: null successor in SgExprListExp.p_expressions")
add_rosetta_traversal_death_test(
  rosetta_rex_duplicate_traversal_successor_is_rejected
  duplicate-successor
  "REX_AST_INVARIANT[traversal-duplicate-successor]: duplicate successor identity in SgExprListExp.p_expressions")

add_test(
  NAME rosetta_rex_optional_null_traversal_successor_is_omitted
  COMMAND $<TARGET_FILE:rex_rosetta_traversal_consistency> optional-null)
set_tests_properties(
  rosetta_rex_optional_null_traversal_successor_is_omitted
  PROPERTIES LABELS "rex;ROSETTA;AST")

add_test(
  NAME rosetta_rex_alignof_conditional_operand_traversal
  COMMAND $<TARGET_FILE:rex_rosetta_traversal_consistency>
    alignof-conditional-operand)
set_tests_properties(
  rosetta_rex_alignof_conditional_operand_traversal
  PROPERTIES LABELS "rex;ROSETTA;AST")

add_rosetta_traversal_death_test(
  rosetta_rex_alignof_missing_typed_operand_is_rejected
  alignof-missing-operand
  "REX_AST_INVARIANT[alignof-operand]: SgAlignOfOp must have exactly one expression or type operand")
add_rosetta_traversal_death_test(
  rosetta_rex_alignof_conflicting_typed_operands_are_rejected
  alignof-conflicting-operands
  "REX_AST_INVARIANT[alignof-operand]: SgAlignOfOp must have exactly one expression or type operand")

add_rosetta_hard_error_contract_test(
  rosetta_rex_unary_null_operand_is_rejected
  unary-null-operand
  "REX_AST_INVARIANT[unary-operand-producer]: builder requires one structural operand")
add_rosetta_hard_error_contract_test(
  rosetta_rex_binary_null_operand_is_rejected
  binary-null-operand
  "REX_AST_INVARIANT[binary-operand-producer]: builder requires both structural operands")
add_rosetta_hard_error_contract_test(
  rosetta_rex_throw_expression_null_operand_is_rejected
  throw-expression-null-operand
  "REX_AST_INVARIANT[throw-operand-producer]: throw builder requires exactly the operand prescribed by its explicit throw kind")
add_rosetta_hard_error_contract_test(
  rosetta_rex_rethrow_nonnull_operand_is_rejected
  rethrow-nonnull-operand
  "REX_AST_INVARIANT[throw-operand-producer]: throw builder requires exactly the operand prescribed by its explicit throw kind")
add_rosetta_hard_error_contract_test(
  rosetta_rex_unary_owned_operand_is_rejected
  unary-owned-operand
  "REX_AST_INVARIANT[unary-operand-ownership]: builder requires a detached structural operand")
add_rosetta_hard_error_contract_test(
  rosetta_rex_binary_owned_operand_is_rejected
  binary-owned-operand
  "REX_AST_INVARIANT[binary-operand-ownership]: builder requires two distinct detached structural operands")
add_rosetta_hard_error_contract_test(
  rosetta_rex_binary_duplicate_operand_is_rejected
  binary-duplicate-operand
  "REX_AST_INVARIANT[binary-operand-ownership]: builder requires two distinct detached structural operands")
add_rosetta_hard_error_contract_test(
  rosetta_rex_throw_owned_operand_is_rejected
  throw-owned-operand
  "REX_AST_INVARIANT[throw-operand-ownership]: throw builder requires a detached expression operand")

add_test(
  NAME rosetta_rex_declaration_group_iteration_and_atomic_mutation
  COMMAND $<TARGET_FILE:rex_rosetta_hard_error_contract>
    declaration-group-iteration)
set_tests_properties(
  rosetta_rex_declaration_group_iteration_and_atomic_mutation
  PROPERTIES LABELS "rex;ROSETTA;SageInterface")

add_test(
  NAME rosetta_rex_mixed_traversal_successor_order
  COMMAND $<TARGET_FILE:rex_rosetta_hard_error_contract>
    mixed-traversal-successor-order)
set_tests_properties(
  rosetta_rex_mixed_traversal_successor_order
  PROPERTIES LABELS "rex;ROSETTA;AST")

add_test(
  NAME rosetta_rex_multiple_container_traversal_successor_order
  COMMAND $<TARGET_FILE:rex_rosetta_hard_error_contract>
    multiple-container-traversal-successor-order)
set_tests_properties(
  rosetta_rex_multiple_container_traversal_successor_order
  PROPERTIES LABELS "rex;ROSETTA;AST")

add_test(
  NAME rosetta_rex_all_live_nodes_have_consistent_traversal_apis
  COMMAND $<TARGET_FILE:rex_rosetta_traversal_consistency> all-live-nodes)
set_tests_properties(
  rosetta_rex_all_live_nodes_have_consistent_traversal_apis
  PROPERTIES LABELS "rex;ROSETTA;AST")

add_test(
  NAME rosetta_rex_template_argument_declaration_is_a_semantic_copy_edge
  COMMAND $<TARGET_FILE:rex_rosetta_traversal_consistency>
    template-argument-semantic-declaration)
set_tests_properties(
  rosetta_rex_template_argument_declaration_is_a_semantic_copy_edge
  PROPERTIES LABELS "rex;ROSETTA;AST;copy")

add_test(
  NAME rosetta_rex_dynamic_traversal_has_no_flat_index_enum
  COMMAND ${BASH_EXECUTABLE} -c
    "! grep -Fq 'E_SgTemplateVariableDeclaration' \"\$1\""
    dummy
    ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarTreeTraversalAccessEnums.h)
set_tests_properties(
  rosetta_rex_dynamic_traversal_has_no_flat_index_enum
  PROPERTIES LABELS "rex;ROSETTA;generator")

add_test(
  NAME rosetta_rex_traversal_generator_has_only_typed_value_containers
  COMMAND ${BASH_EXECUTABLE} -c
    "! grep -Eq 'p_[A-Za-z0-9_]+->(size|begin|end)\(' \"\$1\" && \
     ! grep -Fq '&(*iter)' \"\$1\" && \
     ! grep -Fq 'find_if(' \"\$1\""
    dummy
    ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarTreeTraversalSuccessorContainer.C)
set_tests_properties(
  rosetta_rex_traversal_generator_has_only_typed_value_containers
  PROPERTIES LABELS "rex;ROSETTA;generator")

add_test(
  NAME rosetta_rex_basic_block_extraction_ownership_and_order
  COMMAND $<TARGET_FILE:rex_rosetta_hard_error_contract>
    basic-block-extraction-order)
set_tests_properties(
  rosetta_rex_basic_block_extraction_ownership_and_order
  PROPERTIES LABELS "rex;ROSETTA;AST")

add_test(
  NAME rosetta_rex_class_type_canonical_owner
  COMMAND $<TARGET_FILE:rex_rosetta_hard_error_contract>
    class-type-canonical-owner)
set_tests_properties(
  rosetta_rex_class_type_canonical_owner
  PROPERTIES LABELS "rex;ROSETTA;AST")

add_test(
  NAME rosetta_rex_required_declaration_ownership_contract
  COMMAND $<TARGET_FILE:rex_rosetta_hard_error_contract>
    required-declaration-ownership)
set_tests_properties(
  rosetta_rex_required_declaration_ownership_contract
  PROPERTIES LABELS "rex;ROSETTA;AST")

add_test(
  NAME rosetta_rex_access_label_canonical_construction
  COMMAND $<TARGET_FILE:rex_rosetta_hard_error_contract>
    access-label-construction)
set_tests_properties(
  rosetta_rex_access_label_canonical_construction
  PROPERTIES LABELS "rex;ROSETTA;AST")
