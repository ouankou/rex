file(GLOB OPENMP_FORTRAN_TESTCODES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  "*.f"
  "*.f90")
list(SORT OPENMP_FORTRAN_TESTCODES)
list(REMOVE_ITEM OPENMP_FORTRAN_TESTCODES
  rex_flang_openmp_runtime_include_semantics.f90
  rex_fortran_openmp_directive_string_continuation.f90
  rex_fortran_openmp_taskgroup_end.f90)

set(ROSE_FLAGS -rose:openmp:ast_only -rose:verbose 0)
set(TESTCODE_INCLUDES)
set(_omp_fortran_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_omp_fortran_output_dir})

set(_rex_openmp_fortran_semantic_positive_sources
  rex_fortran_openmp_allocate_predefined_semantic_positive.f90
  rex_fortran_openmp_allocate_user_semantic_positive.f90
  rex_fortran_openmp_copyprivate_semantic_positive.f90
  rex_fortran_openmp_recursive_task_semantic_positive.f90)

add_executable(rex_flang_directive_lexical_ownership
  rex_flang_directive_lexical_ownership.cpp)
target_link_libraries(rex_flang_directive_lexical_ownership
  ROSE_DLL ${link_with_libraries})

add_executable(rex_flang_preprocessing_provenance
  rex_flang_preprocessing_provenance.cpp)
target_link_libraries(rex_flang_preprocessing_provenance
  ROSE_DLL ${link_with_libraries})

add_executable(rex_fortran_openmp_exact_semantic_handoff_translator
  rex_fortran_openmp_exact_semantic_handoff_translator.cpp)
target_link_libraries(rex_fortran_openmp_exact_semantic_handoff_translator
  ROSE_DLL ${link_with_libraries})

if(NOT FLANG_INTRINSIC_INCLUDE_DIR OR
   NOT EXISTS "${FLANG_INTRINSIC_INCLUDE_DIR}/omp_lib.h")
  message(FATAL_ERROR
    "REX requires the pinned Flang omp_lib.h physical input")
endif()
add_executable(rex_flang_openmp_runtime_include_semantics
  rex_flang_openmp_runtime_include_semantics.cpp)
target_compile_definitions(rex_flang_openmp_runtime_include_semantics PRIVATE
  REX_FLANG_OMP_LIB_PATH=${FLANG_INTRINSIC_INCLUDE_DIR}/omp_lib.h)
target_link_libraries(rex_flang_openmp_runtime_include_semantics
  ROSE_DLL ${link_with_libraries})

add_test(
  NAME OMPFORTRAN_rex_flang_directive_lexical_ownership
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_directive_lexical_ownership>
    ${ROSE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -rose:skip_unparse
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openmp_directive_lexical_ownership.F90)
set_tests_properties(OMPFORTRAN_rex_flang_directive_lexical_ownership
  PROPERTIES LABELS "OMPFORTRAN;rex;AST")

set(_removed_openmp_version_option -fopenmp-version=60)
set(_removed_openmp_version_option_test
  OMPFORTRAN_rex_removed_openmp_version_option)
set(_removed_openmp_version_option_log
  ${_removed_openmp_version_option_test}.log)
set(_removed_openmp_version_option_diagnostic
  "REX_FLANG_INVARIANT[removed-openmp-version-option]: option '${_removed_openmp_version_option}' was part of the removed frontend-owned OpenMP design and is not supported")
add_test(
  NAME ${_removed_openmp_version_option_test}
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=\"$1\"; expected=\"$2\"; shift 2; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc -- \"$expected\" \"$log\")\" -eq 1"
    dummy
    ${_removed_openmp_version_option_log}
    "${_removed_openmp_version_option_diagnostic}"
    $<TARGET_FILE:testTranslator>
    ${ROSE_FLAGS}
    -rose:skipfinalCompileStep
    ${_removed_openmp_version_option}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openmp_directive_lexical_ownership.F90)
set_tests_properties(${_removed_openmp_version_option_test}
  PROPERTIES LABELS "OMPFORTRAN;rex;death")
rex_lock_test_for_input(${_removed_openmp_version_option_test}
  rex_fortran_openmp_directive_lexical_ownership.F90)
unset(_removed_openmp_version_option)
unset(_removed_openmp_version_option_diagnostic)
unset(_removed_openmp_version_option_log)
unset(_removed_openmp_version_option_test)

set(_unsupported_openmp_runtime_option -fopenmp=libiomp5)
set(_unsupported_openmp_runtime_test
  OMPFORTRAN_rex_unsupported_openmp_runtime_is_rejected)
set(_unsupported_openmp_runtime_log
  ${_unsupported_openmp_runtime_test}.log)
set(_unsupported_openmp_runtime_diagnostic
  "REX_FRONTEND_INVARIANT[rex-openmp-option]: option '${_unsupported_openmp_runtime_option}' selects a runtime other than REX's required LLVM libomp")
add_test(
  NAME ${_unsupported_openmp_runtime_test}
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=\"$1\"; expected=\"$2\"; shift 2; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc -- \"$expected\" \"$log\")\" -eq 1"
    dummy
    ${_unsupported_openmp_runtime_log}
    "${_unsupported_openmp_runtime_diagnostic}"
    $<TARGET_FILE:testTranslator>
    ${ROSE_FLAGS}
    -rose:skipfinalCompileStep
    ${_unsupported_openmp_runtime_option}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_directive_parser_ownership.f90)
set_tests_properties(${_unsupported_openmp_runtime_test}
  PROPERTIES LABELS "OMPFORTRAN;rex;death")
rex_lock_test_for_input(${_unsupported_openmp_runtime_test}
  rex_fortran_directive_parser_ownership.f90)
unset(_unsupported_openmp_runtime_diagnostic)
unset(_unsupported_openmp_runtime_log)
unset(_unsupported_openmp_runtime_option)
unset(_unsupported_openmp_runtime_test)

add_test(
  NAME OMPFORTRAN_rex_supported_openmp_runtime_uses_rex_ast
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND $<TARGET_FILE:parseOmp>
    ${ROSE_FLAGS}
    -fopenmp=libomp
    -rose:skipfinalCompileStep
    -rose:output
      ${_omp_fortran_output_dir}/rose_rex_fortran_supported_openmp_runtime.f90
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_directive_parser_ownership.f90)
set_tests_properties(
  OMPFORTRAN_rex_supported_openmp_runtime_uses_rex_ast
  PROPERTIES LABELS "OMPFORTRAN;rex;AST")
rex_lock_test_for_input(
  OMPFORTRAN_rex_supported_openmp_runtime_uses_rex_ast
  rex_fortran_directive_parser_ownership.f90)

add_test(
  NAME OMPFORTRAN_rex_flang_preprocessing_provenance
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_preprocessing_provenance>
    ${ROSE_FLAGS}
    -rose:skip_unparse
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_source_token_provenance.F90)
set_tests_properties(OMPFORTRAN_rex_flang_preprocessing_provenance
  PROPERTIES LABELS "OMPFORTRAN;rex;AST")

add_test(
  NAME OMPFORTRAN_rex_fortran_openmp_exact_semantic_handoff
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_fortran_openmp_exact_semantic_handoff_translator>
    ${ROSE_FLAGS}
    -rex:ast-json-checkpoint=post-omp-construction
    -rex:ast-json-dir=${_omp_fortran_output_dir}/exact-semantic-handoff-json
    -rose:skip_unparse
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openmp_exact_semantic_handoff.f90)
set_tests_properties(
  OMPFORTRAN_rex_fortran_openmp_exact_semantic_handoff
  PROPERTIES LABELS "OMPFORTRAN;rex;AST;ASTJSON")

add_test(
  NAME OMPFORTRAN_rex_flang_openmp_runtime_include_semantics
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND $<TARGET_FILE:rex_flang_openmp_runtime_include_semantics>
    ${ROSE_FLAGS}
    -I${FLANG_INTRINSIC_INCLUDE_DIR}
    -rose:skip_unparse
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_openmp_runtime_include_semantics.f90)
set_tests_properties(
  OMPFORTRAN_rex_flang_openmp_runtime_include_semantics
  PROPERTIES LABELS "OMPFORTRAN;rex;AST")

set(_custom_line_width_check
    ${CMAKE_CURRENT_SOURCE_DIR}/../../check_rex_fortran_custom_line_width.sh)
add_test(
  NAME OMPFORTRAN_rex_fortran_openmp_directive_context_custom_width
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND ${_custom_line_width_check}
    $<TARGET_FILE:rex_unparser_fortran_custom_line_width>
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openmp_directive_context_coverage.f90
    ${_omp_fortran_output_dir}/custom_width_rex_fortran_openmp_directive_context_coverage.f90
    48
    "!$omp&"
    ${ROSE_FLAGS}
    -rose:skipfinalCompileStep)
set_tests_properties(
  OMPFORTRAN_rex_fortran_openmp_directive_context_custom_width
  PROPERTIES LABELS "OMPFORTRAN;rex;UNPARSER")

set(_directive_spelling_check
    ${CMAKE_CURRENT_SOURCE_DIR}/../../check_rex_fortran_directive_spelling.sh)
add_test(
  NAME OMPFORTRAN_rex_fortran_openmp_exact_expression_spelling_contract
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} ${_directive_spelling_check}
    $<TARGET_FILE:parseOmp>
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openmp_exact_expression_spelling.f90
    ${_omp_fortran_output_dir}/contract_rex_fortran_openmp_exact_expression_spelling.f90
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openmp_exact_expression_spelling.expected
    ${ROSE_FLAGS}
    -rose:skipfinalCompileStep)
set_tests_properties(
  OMPFORTRAN_rex_fortran_openmp_exact_expression_spelling_contract
  PROPERTIES LABELS "OMPFORTRAN;rex;UNPARSER")

add_test(
  NAME OMPFORTRAN_rex_fortran_openmp_macro_string_surface_contract
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} ${_directive_spelling_check}
    $<TARGET_FILE:parseOmp>
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openmp_macro_string_surface.F90
    ${_omp_fortran_output_dir}/contract_rex_fortran_openmp_macro_string_surface.F90
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openmp_macro_string_surface.expected
    ${ROSE_FLAGS}
    -rose:skipfinalCompileStep)
set_tests_properties(
  OMPFORTRAN_rex_fortran_openmp_macro_string_surface_contract
  PROPERTIES LABELS "OMPFORTRAN;rex;UNPARSER")

set(_source_token_check
    ${CMAKE_CURRENT_SOURCE_DIR}/../../check_rex_fortran_source_tokens.sh)
add_test(
  NAME OMPFORTRAN_rex_fortran_source_token_provenance_contract
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} ${_source_token_check}
    $<TARGET_FILE:parseOmp>
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_source_token_provenance.F90
    ${_omp_fortran_output_dir}/contract_rex_fortran_source_token_provenance.F90
    ${ROSE_FLAGS}
    -rose:skipfinalCompileStep)
set_tests_properties(
  OMPFORTRAN_rex_fortran_source_token_provenance_contract
  PROPERTIES LABELS "OMPFORTRAN;rex;UNPARSER")

foreach(file_to_test ${OPENMP_FORTRAN_TESTCODES})
  add_test(
    NAME OMPFORTRAN_${file_to_test}
    WORKING_DIRECTORY ${_omp_fortran_output_dir}
    COMMAND $<TARGET_FILE:parseOmp> ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
      -rose:output ${_omp_fortran_output_dir}/rose_${file_to_test}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  if(file_to_test IN_LIST _rex_openmp_fortran_semantic_positive_sources)
    set(_labels "OMPFORTRAN;rex")
  else()
    set(_labels OMPFORTRAN)
  endif()
  set_tests_properties(OMPFORTRAN_${file_to_test}
    PROPERTIES LABELS "${_labels}")
endforeach()

set(_openmp_function_reference_role_output
  ${_omp_fortran_output_dir}/rose_rex_fortran_openmp_exact_semantic_handoff.f90)
add_test(
  NAME OMPFORTRAN_rex_fortran_openmp_function_reference_roles_unparse
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "output=\"$1\"; test \"$(grep -Ec '^[!][$]omp[[:space:]]+declare[[:space:]]+simd\\(rex_openmp_exact_simd_target\\)' \"$output\")\" -eq 2; test \"$(grep -Ec '^[!][$]omp[[:space:]]+declare[[:space:]]+simd[[:space:]]+uniform\\(AdJ_ArG\\)' \"$output\")\" -eq 1; test \"$(grep -Ec 'declare[[:space:]]+simd\\(rex_openmp_exact_simd_adjacency\\)' \"$output\")\" -eq 0; test \"$(grep -Ec '^[!][$]omp[[:space:]]+declare[[:space:]]+variant\\(rex_openmp_exact_variant_base:rex_openmp_exact_variant_explicit_(one|two)\\)' \"$output\")\" -eq 2; test \"$(grep -Ec '^[!][$]omp[[:space:]]+declare[[:space:]]+variant\\(rex_openmp_exact_variant_adjacency_impl\\)' \"$output\")\" -eq 1"
    dummy
    ${_openmp_function_reference_role_output})
set_tests_properties(
  OMPFORTRAN_rex_fortran_openmp_function_reference_roles_unparse
  PROPERTIES
    DEPENDS OMPFORTRAN_rex_fortran_openmp_exact_semantic_handoff.f90
    LABELS "OMPFORTRAN;rex;UNPARSER")

# Keep this REX regression explicit: it verifies that the END TASKGROUP
# directive is consumed as the exact peer of its opening TASKGROUP rather than
# surviving only because the directory glob happened to discover the fixture.
add_test(
  NAME OMPFORTRAN_rex_fortran_openmp_taskgroup_end.f90
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND $<TARGET_FILE:parseOmp>
    ${ROSE_FLAGS}
    ${TESTCODE_INCLUDES}
    -rose:output
      ${_omp_fortran_output_dir}/rose_rex_fortran_openmp_taskgroup_end.f90
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openmp_taskgroup_end.f90)
set_tests_properties(OMPFORTRAN_rex_fortran_openmp_taskgroup_end.f90
  PROPERTIES LABELS "OMPFORTRAN;rex")

set(_openmp_common_block_source
  rex_fortran_openmp_common_block_reference.f90)
set(_openmp_common_block_output
  ${_omp_fortran_output_dir}/roundtrip_${_openmp_common_block_source})
set(_openmp_common_block_json
  ${_omp_fortran_output_dir}/common-block-reference-json)
add_test(
  NAME OMPFORTRAN_rex_common_block_reference_ast_json
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND $<TARGET_FILE:parseOmp>
    ${ROSE_FLAGS}
    ${TESTCODE_INCLUDES}
    -rex:ast-json-checkpoint=post-omp-construction
    -rex:ast-json-dir=${_openmp_common_block_json}
    -rose:output ${_openmp_common_block_output}
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/${_openmp_common_block_source})
set_tests_properties(OMPFORTRAN_rex_common_block_reference_ast_json
  PROPERTIES
    FIXTURES_SETUP rex_openmp_fortran_common_block_reference_output
    LABELS "OMPFORTRAN;ASTJSON;rex;UNPARSER")
add_test(
  NAME OMPFORTRAN_rex_common_block_reference_exact_surface
  COMMAND ${BASH_EXECUTABLE} -c
    "grep -Eq 'threadprivate[[:space:]]*\\([[:space:]]*/fLaGs/[[:space:]]*\\)' '${_openmp_common_block_output}' && grep -Eq 'copyin[[:space:]]*\\([[:space:]]*/FlAgS/[[:space:]]*\\)' '${_openmp_common_block_output}'")
set_tests_properties(OMPFORTRAN_rex_common_block_reference_exact_surface
  PROPERTIES
    FIXTURES_REQUIRED rex_openmp_fortran_common_block_reference_output
    LABELS "OMPFORTRAN;ASTJSON;rex;UNPARSER")

set(_directive_continuation_check
    ${CMAKE_CURRENT_SOURCE_DIR}/../../check_rex_fortran_directive_continuation.sh)
add_test(
  NAME OMPFORTRAN_rex_fortran_openmp_directive_string_continuation_contract
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} ${_directive_continuation_check}
    $<TARGET_FILE:parseOmp>
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openmp_directive_string_continuation.f90
    ${_omp_fortran_output_dir}/contract_rex_fortran_openmp_directive_string_continuation.f90
    132
    "!$omp& &"
    ${ROSE_FLAGS}
    -rose:skipfinalCompileStep)
set_tests_properties(
  OMPFORTRAN_rex_fortran_openmp_directive_string_continuation_contract
  PROPERTIES LABELS "OMPFORTRAN;rex;UNPARSER")

add_test(
  NAME OMPFORTRAN_rex_fortran_openmp_module_directive_continuation_contract
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} ${_directive_continuation_check}
    $<TARGET_FILE:parseOmp>
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openmp_module_directive_continuation.f90
    ${_omp_fortran_output_dir}/contract_rex_fortran_openmp_module_directive_continuation.f90
    132
    "!$omp&"
    ${ROSE_FLAGS}
    -rose:skipfinalCompileStep)
set_tests_properties(
  OMPFORTRAN_rex_fortran_openmp_module_directive_continuation_contract
  PROPERTIES LABELS "OMPFORTRAN;rex;UNPARSER")

set(_ompx_comment_check
    ${CMAKE_CURRENT_SOURCE_DIR}/../../check_rex_fortran_ompx_comment.sh)
add_test(
  NAME OMPFORTRAN_rex_fortran_openmp_unknown_ompx_comment_contract
  WORKING_DIRECTORY ${_omp_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} ${_ompx_comment_check}
    $<TARGET_FILE:parseOmp>
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openmp_unknown_ompx_comment.f90
    ${_omp_fortran_output_dir}/contract_rex_fortran_openmp_unknown_ompx_comment.f90
    ${ROSE_FLAGS}
    -rose:skipfinalCompileStep)
set_tests_properties(
  OMPFORTRAN_rex_fortran_openmp_unknown_ompx_comment_contract
  PROPERTIES LABELS "OMPFORTRAN;rex;UNPARSER")

foreach(_form IN ITEMS free fixed)
  if(_form STREQUAL "free")
    set(_source rex_fortran_openmp_free_directive_continuation.f90)
    set(_maximum_columns 132)
  else()
    set(_source rex_fortran_openmp_fixed_directive_continuation.f)
    set(_maximum_columns 72)
  endif()
  add_test(
    NAME OMPFORTRAN_rex_fortran_openmp_${_form}_directive_continuation_contract
    WORKING_DIRECTORY ${_omp_fortran_output_dir}
    COMMAND ${BASH_EXECUTABLE} ${_directive_continuation_check}
      $<TARGET_FILE:parseOmp>
      ${CMAKE_CURRENT_SOURCE_DIR}/${_source}
      ${_omp_fortran_output_dir}/contract_${_source}
      ${_maximum_columns}
      "!$omp&"
      ${ROSE_FLAGS}
      -rose:skipfinalCompileStep)
  set_tests_properties(
    OMPFORTRAN_rex_fortran_openmp_${_form}_directive_continuation_contract
    PROPERTIES LABELS "OMPFORTRAN;rex;UNPARSER")
endforeach()
