set(ROSE_FLAGS -rose:openacc:ast_only -rose:verbose 0)
set(_openacc_includes -I${CMAKE_SOURCE_DIR}/src/frontend/SageIII)

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; OpenACC Fortran tests require bash")
endif()

set(_custom_line_width_check
    ${CMAKE_CURRENT_SOURCE_DIR}/../../check_rex_fortran_custom_line_width.sh)
add_test(
  NAME OpenACC_fortran_rex_directive_context_custom_width
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_custom_line_width_check}
    $<TARGET_FILE:rex_unparser_fortran_custom_line_width>
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_openacc_directive_context_coverage.f90
    ${CMAKE_CURRENT_BINARY_DIR}/custom_width_rex_fortran_openacc_directive_context_coverage.f90
    48
    "!$acc&"
    ${ROSE_FLAGS}
    ${_openacc_includes}
    -rose:skipfinalCompileStep)
set_tests_properties(OpenACC_fortran_rex_directive_context_custom_width
  PROPERTIES LABELS "OpenACC_fortran;rex;UNPARSER")

set(_openacc_f90_tests
  rex_fortran_openacc_semantic_positive.f90)

rex_add_flang_frontend_negative_test(
  OpenACC_fortran_trans.F90_frontend_negative
  OpenACC_fortran
  ${CMAKE_CURRENT_BINARY_DIR}
  trans.F90.frontend-negative.log
  trans
  "Must have LOGICAL or INTEGER type"
  1
  $<TARGET_FILE:testTranslator>
  ${ROSE_FLAGS}
  ${_openacc_includes}
  -rose:skip_unparse
  -rose:skipfinalCompileStep
  -c ${CMAKE_CURRENT_SOURCE_DIR}/trans.F90)

foreach(specimen IN LISTS _openacc_f90_tests)
  string(REPLACE "." "_" _name "${specimen}")
  string(REPLACE "-" "_" _name "${_name}")
  set(_rose_file rose_${specimen})
  set(_output_file ${_rose_file}.output)
  set(_diff_file ${_output_file}.diff)
  set(_reference_dir ${CMAKE_CURRENT_SOURCE_DIR}/referenceResults)

  add_test(
    NAME OpenACC_fortran_${specimen}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "rm -f '${_rose_file}' '${_output_file}' '${_diff_file}'; exec \"$@\""
      dummy $<TARGET_FILE:testTranslator> ${ROSE_FLAGS} ${_openacc_includes}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
  )
  set_tests_properties(OpenACC_fortran_${specimen} PROPERTIES LABELS OpenACC_fortran)

  add_test(
    NAME OpenACC_fortran_output_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "test -s '${_rose_file}' && grep '!$acc' '${_rose_file}' > '${_output_file}'"
  )
  set_tests_properties(OpenACC_fortran_output_${_name} PROPERTIES DEPENDS OpenACC_fortran_${specimen} LABELS OpenACC_fortran)

  add_test(
    NAME OpenACC_fortran_output_diff_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "diff '${_output_file}' '${_reference_dir}/${_output_file}' > '${_diff_file}'"
  )
  set_tests_properties(OpenACC_fortran_output_diff_${_name} PROPERTIES DEPENDS OpenACC_fortran_output_${_name} LABELS OpenACC_fortran)
endforeach()

set(_openacc_common_block_source
  rex_fortran_openacc_common_block_reference.f90)
set(_openacc_common_block_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_${_openacc_common_block_source})
set(_openacc_common_block_json
  ${CMAKE_CURRENT_BINARY_DIR}/common-block-reference-json)
add_test(
  NAME OpenACC_fortran_rex_common_block_reference_ast_json
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTranslator>
    ${ROSE_FLAGS}
    ${_openacc_includes}
    -rex:ast-json-checkpoint=post-omp-construction
    -rex:ast-json-dir=${_openacc_common_block_json}
    -rose:output ${_openacc_common_block_output}
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/${_openacc_common_block_source})
set_tests_properties(OpenACC_fortran_rex_common_block_reference_ast_json
  PROPERTIES
    FIXTURES_SETUP rex_openacc_common_block_reference_output
    LABELS "OpenACC_fortran;ASTJSON;rex;UNPARSER")
add_test(
  NAME OpenACC_fortran_rex_common_block_reference_exact_surface
  COMMAND ${BASH_EXECUTABLE} -c
    "test \"$(grep -Ec 'present[[:space:]]*\\([[:space:]]*/fLaGs/[[:space:]]*\\)' '${_openacc_common_block_output}')\" -eq 1")
set_tests_properties(OpenACC_fortran_rex_common_block_reference_exact_surface
  PROPERTIES
    FIXTURES_REQUIRED rex_openacc_common_block_reference_output
    LABELS "OpenACC_fortran;ASTJSON;rex;UNPARSER")

set(_directive_continuation_check
    ${CMAKE_CURRENT_SOURCE_DIR}/../../check_rex_fortran_directive_continuation.sh)
set(_source rex_fortran_openacc_token_literal_continuation.f90)
add_test(
  NAME OpenACC_fortran_rex_token_literal_directive_continuation_contract
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} ${_directive_continuation_check}
    $<TARGET_FILE:testTranslator>
    ${CMAKE_CURRENT_SOURCE_DIR}/${_source}
    ${CMAKE_CURRENT_BINARY_DIR}/rose_${_source}
    132
    "!$acc&"
    ${ROSE_FLAGS}
    ${_openacc_includes}
    -rose:skipfinalCompileStep)
set_tests_properties(
  OpenACC_fortran_rex_token_literal_directive_continuation_contract
  PROPERTIES LABELS "OpenACC_fortran;rex;UNPARSER")

foreach(_form IN ITEMS free fixed)
  if(_form STREQUAL "free")
    set(_source rex_fortran_openacc_free_directive_continuation.f90)
    set(_maximum_columns 132)
  else()
    set(_source rex_fortran_openacc_fixed_directive_continuation.f)
    set(_maximum_columns 72)
  endif()
  add_test(
    NAME OpenACC_fortran_rex_${_form}_directive_continuation_contract
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} ${_directive_continuation_check}
      $<TARGET_FILE:testTranslator>
      ${CMAKE_CURRENT_SOURCE_DIR}/${_source}
      ${CMAKE_CURRENT_BINARY_DIR}/rose_${_source}
      ${_maximum_columns}
      "!$acc&"
      ${ROSE_FLAGS}
      ${_openacc_includes}
      -rose:skipfinalCompileStep)
  set_tests_properties(
    OpenACC_fortran_rex_${_form}_directive_continuation_contract
    PROPERTIES LABELS "OpenACC_fortran;rex;UNPARSER")
endforeach()
