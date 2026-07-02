add_executable(rex_test2026_ast_json_external_scope_contract
  rex_test2026_ast_json_external_scope_contract.cpp)
target_link_libraries(rex_test2026_ast_json_external_scope_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_test2026_ast_json_detached_file_type_fixup
  rex_test2026_ast_json_detached_file_type_fixup.cpp)
target_link_libraries(rex_test2026_ast_json_detached_file_type_fixup
  ROSE_DLL ${link_with_libraries})

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
