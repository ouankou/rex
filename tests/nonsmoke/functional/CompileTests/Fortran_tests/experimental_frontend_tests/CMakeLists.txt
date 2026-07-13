set(_rose_flags -rose:verbose 0 -rose:experimental_fortran_frontend
  -I${CMAKE_CURRENT_SOURCE_DIR})
string(JOIN " " _rose_flags_str ${_rose_flags})
set(_experimental_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_experimental_output_dir})

set(_f90_tests
  tiny.f90
  R1101a.f90
  R1101b.f90
  R1101c.f90
  R1101d.f90
  R1101e.f90
  R1101f.f90
  R1101g.f90
  R1120a.f90
  R404_a.f90
  R521.f90
  R545.f90
  R611a.f90
  R719a.f90
  R832.f90
  R837.f90
  R854.f90
  R856.f90
  contiguous_attr.f90
  test2014_02.f90
)

set(_f2018_tests
  R507-F2015-N2007.f90
  R511-F2015-N2007.f90
  R1123-F2018.f90
  R1164-F2018.f90
  R1166-F2018.f90
  R1168-F2018.f90
  R1169-F2018.f90
  R1179-F2018.f90
  R1181-F2018.f90
)

set(_translator_tests
  ${_f90_tests}
  issue-38.f90
  R404.f90
  R1001.f90
  R1109.f90
  R1111.f90
  R1120b.f90
  R1201.f90
  R1203.f90
  R1204.f90
  R1205.f90
  R1206.f90
  R1207.f90
  R1208.f90
  R1209.f90
  R1210.f90
  R1211.f90
  R1214.f90
  R1218.f90
  R1234.f90
  R1241.f90
  R524.f90
  R526.f90
  R533.f90
  R611.f90
  R620.f90
  R626.f90
  R640.f90
  R701.f90
  R704.f90
  R705.f90
  R706.f90
  R710.f90
  R713.f90
  R719.f90
  R720.f90
  R721.f90
  R743.f90
  R757.f90
  R804.f90
  R807.f90
  R831.f90
  R838.f90
  R850.f90
  R851.f90
  R852.f90
  R853.f90
  R902.f90
  R904.f90
  R905.f90
  R908.f90
  R909.f90
  R911.f90
  R922.f90
  R926.f90
  R927.f90
  R928.f90
  R929.f90
  R930.f90
  rex_unparser_fortran_safety.f90
  rex_unparser_implicit_program.f90
  rex_unparser_unnamed_block_data.f90
  rex_unparser_zero_iteration_implied_do.f90
  rex_unparser_fortran_pointer_comment.f90
  rex_unparser_fortran_procedure_pointer.f90
  rex_fortran_enum_bind_c.f90
  pause_stmt.f77
  R432.f90
  R452.f90
  R816.f90
  R858.f08
  R859.f08
  R862.f08
  R1118.fxx
)

# These .fxx files are standalone grammar-production specimens, not complete
# semantically valid programs.  Keep them in one authoritative negative set
# and require Flang to reject every one before any Sage AST can be built.
set(_semantic_negative_tests
  R1116.fxx
  R1220.fxx
  R1237.fxx
  R426.fxx
  R427.fxx
  R702.fxx
  R722.fxx
  R723.fxx
  R733.fxx
)

set(_frontend_tests
  ${_f2018_tests}
)

function(_experimental_sanitize_name input output_var)
  set(_name "${input}")
  string(REPLACE "/" "_" _name "${_name}")
  string(REPLACE "." "_" _name "${_name}")
  string(REPLACE "-" "_" _name "${_name}")
  set(${output_var} "${_name}" PARENT_SCOPE)
endfunction()

function(_experimental_add_translator_test test_prefix specimen)
  _experimental_sanitize_name(${specimen} _name)
  set(_test_name ${test_prefix}_${_name})
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${_experimental_output_dir}
    COMMAND bash -c
      "rm -f rose_${specimen} && $<TARGET_FILE:testTranslator> ${_rose_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen} && test -s rose_${specimen}"
  )
  set_tests_properties(${_test_name} PROPERTIES RESOURCE_LOCK experimental_frontend_test_output)
  set(${test_prefix}_${specimen}_name ${_test_name} PARENT_SCOPE)
endfunction()

function(_experimental_add_frontend_test test_prefix specimen)
  _experimental_sanitize_name(${specimen} _name)
  add_test(
    NAME ${test_prefix}_${_name}
    WORKING_DIRECTORY ${_experimental_output_dir}
    COMMAND $<TARGET_FILE:testFrontEnd>
      ${_rose_flags}
      ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
  )
  set_tests_properties(${test_prefix}_${_name} PROPERTIES RESOURCE_LOCK experimental_frontend_test_output)
endfunction()

function(_experimental_semantic_negative_diagnostic specimen output_variable)
  if(specimen STREQUAL "R1116.fxx")
    set(_diagnostic "Module 'ancestor' already has a submodule named 'submod2'")
  elseif(specimen STREQUAL "R1220.fxx")
    set(_diagnostic "Operand of .NOT. must be LOGICAL; have REAL(4)")
  elseif(specimen STREQUAL "R1237.fxx")
    set(_diagnostic "'modproc' was not declared a separate module procedure")
  elseif(specimen STREQUAL "R426.fxx")
    set(_diagnostic "No definition found for type parameter 'l'")
  elseif(specimen STREQUAL "R427.fxx")
    set(_diagnostic "Derived type 'truth' not found")
  elseif(specimen STREQUAL "R702.fxx")
    set(_diagnostic "No operator .INVERSE. defined for REAL(4)")
  elseif(specimen STREQUAL "R722.fxx")
    set(_diagnostic "Derived type 'set' not found")
  elseif(specimen STREQUAL "R723.fxx")
    set(_diagnostic "No operator .UNION. defined for INTEGER(4) and INTEGER(4)")
  elseif(specimen STREQUAL "R733.fxx")
    set(_diagnostic "Pointer bounds require 4 elements but target has only 1")
  else()
    message(FATAL_ERROR
      "No exact semantic-negative diagnostic registered for ${specimen}")
  endif()
  set(${output_variable} "${_diagnostic}" PARENT_SCOPE)
endfunction()

function(_experimental_add_semantic_negative_test test_prefix specimen)
  _experimental_sanitize_name(${specimen} _name)
  set(_test_name ${test_prefix}_${_name})
  _experimental_semantic_negative_diagnostic(${specimen}
    _expected_diagnostic)
  set(_expected_count 1)
  if(specimen STREQUAL "R723.fxx")
    set(_expected_count 2)
  endif()
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${_experimental_output_dir}
    COMMAND bash -c
      "ulimit -c 0; expected=\"$1\"; expected_count=\"$2\"; shift 2; log=${specimen}.semantic-negative.log; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test \"$status\" -eq 1; test \"$(grep -F -- \"${specimen}:\" \"$log\" | sed -nE 's/^.*:[0-9]+:[0-9]+: (error:.*)$/\\1/p' | grep -Fxc -- \"error: $expected\")\" -eq \"$expected_count\""
      dummy
      "${_expected_diagnostic}"
      "${_expected_count}"
      $<TARGET_FILE:testTranslator>
      ${_rose_flags}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
  )
  set_tests_properties(${_test_name} PROPERTIES
    LABELS "FORTRANTEST;death"
    RESOURCE_LOCK experimental_frontend_test_output)
endfunction()

foreach(specimen IN LISTS _translator_tests)
  _experimental_add_translator_test(experimental_frontend ${specimen})
endforeach()

foreach(specimen IN LISTS _semantic_negative_tests)
  _experimental_add_semantic_negative_test(experimental_frontend ${specimen})
endforeach()

foreach(specimen IN LISTS _frontend_tests)
  _experimental_add_frontend_test(experimental_frontend ${specimen})
endforeach()

set_tests_properties(
  experimental_frontend_rex_unparser_fortran_safety_f90
  PROPERTIES FIXTURES_SETUP rex_unparser_fortran_declaration_output)
set_tests_properties(
  experimental_frontend_rex_unparser_implicit_program_f90
  PROPERTIES FIXTURES_SETUP rex_unparser_fortran_implicit_program_output)
set_tests_properties(
  experimental_frontend_rex_unparser_unnamed_block_data_f90
  PROPERTIES FIXTURES_SETUP rex_unparser_fortran_unnamed_block_data_output)
set_tests_properties(
  experimental_frontend_rex_unparser_fortran_procedure_pointer_f90
  PROPERTIES FIXTURES_SETUP rex_unparser_fortran_procedure_pointer_output)
set_tests_properties(
  experimental_frontend_rex_unparser_zero_iteration_implied_do_f90
  PROPERTIES FIXTURES_SETUP rex_unparser_fortran_zero_iteration_implied_do_output)
set_tests_properties(
  experimental_frontend_rex_unparser_fortran_pointer_comment_f90
  PROPERTIES FIXTURES_SETUP rex_unparser_fortran_pointer_comment_output)
set_tests_properties(
  experimental_frontend_rex_fortran_enum_bind_c_f90
  PROPERTIES FIXTURES_SETUP rex_unparser_fortran_enum_bind_c_output)

add_test(
  NAME rex_unparser_fortran_declaration_fidelity
  WORKING_DIRECTORY ${_experimental_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "grep -Eiq 'CLASS[[:space:]]*\\([[:space:]]*rex_type' rose_rex_unparser_fortran_safety.f90 && \
     grep -Eiq 'ptr[[:space:]]*=>[[:space:]]*target' rose_rex_unparser_fortran_safety.f90 && \
     grep -Eiq 'BLOCK[[:space:]]+DATA[[:space:]]+rex_data' rose_rex_unparser_fortran_safety.f90"
)
set_tests_properties(rex_unparser_fortran_declaration_fidelity PROPERTIES
  FIXTURES_REQUIRED rex_unparser_fortran_declaration_output
  RESOURCE_LOCK experimental_frontend_test_output
  LABELS "rex;UNPARSER;FORTRAN")

add_test(
  NAME rex_unparser_fortran_implicit_program_metadata
  WORKING_DIRECTORY ${_experimental_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "test -s rose_rex_unparser_implicit_program.f90 && \
     grep -Eiq '^[[:space:]]*end[[:space:]]*$' rose_rex_unparser_implicit_program.f90 && \
     ! grep -Eiq '^[[:space:]]*program([[:space:]]|$)' rose_rex_unparser_implicit_program.f90 && \
     ! grep -Fq '__rex_internal_implicit_program_' rose_rex_unparser_implicit_program.f90"
)
set_tests_properties(rex_unparser_fortran_implicit_program_metadata PROPERTIES
  FIXTURES_REQUIRED rex_unparser_fortran_implicit_program_output
  RESOURCE_LOCK experimental_frontend_test_output
  LABELS "rex;UNPARSER;FORTRAN")

add_test(
  NAME rex_unparser_fortran_unnamed_block_data_metadata
  WORKING_DIRECTORY ${_experimental_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "test -s rose_rex_unparser_unnamed_block_data.f90 && \
     grep -Eiq '^[[:space:]]*block[[:space:]]+data[[:space:]]*$' rose_rex_unparser_unnamed_block_data.f90 && \
     grep -Eiq '^[[:space:]]*end[[:space:]]+block[[:space:]]+data[[:space:]]*$' rose_rex_unparser_unnamed_block_data.f90 && \
     ! grep -Fq '__rex_internal_unnamed_block_data_' rose_rex_unparser_unnamed_block_data.f90"
)
set_tests_properties(rex_unparser_fortran_unnamed_block_data_metadata PROPERTIES
  FIXTURES_REQUIRED rex_unparser_fortran_unnamed_block_data_output
  RESOURCE_LOCK experimental_frontend_test_output
  LABELS "rex;UNPARSER;FORTRAN")

add_test(
  NAME rex_unparser_fortran_procedure_fidelity
  WORKING_DIRECTORY ${_experimental_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "grep -Eiq 'PROCEDURE[[:space:]]*\\([[:space:]]*rex_callback[[:space:]]*\\)[[:space:]]*,[[:space:]]*POINTER[[:space:]]*::[[:space:]]*procedure_pointer' rose_rex_unparser_fortran_procedure_pointer.f90 && \
     grep -Eiq 'procedure_pointer[[:space:]]*=>[[:space:]]*rex_target' rose_rex_unparser_fortran_procedure_pointer.f90 && \
     ! grep -Eiq 'POINTER[[:space:]]*,[[:space:]]*POINTER' rose_rex_unparser_fortran_procedure_pointer.f90"
)
set_tests_properties(rex_unparser_fortran_procedure_fidelity PROPERTIES
  FIXTURES_REQUIRED rex_unparser_fortran_procedure_pointer_output
  RESOURCE_LOCK experimental_frontend_test_output
  LABELS "rex;UNPARSER;FORTRAN")

add_test(
  NAME rex_unparser_fortran_zero_iteration_implied_do_fidelity
  WORKING_DIRECTORY ${_experimental_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "grep -Eiq 'i[[:space:]]*=[[:space:]]*4[[:space:]]*,[[:space:]]*0[[:space:]]*,[[:space:]]*11' rose_rex_unparser_zero_iteration_implied_do.f90"
)
set_tests_properties(rex_unparser_fortran_zero_iteration_implied_do_fidelity
  PROPERTIES
    FIXTURES_REQUIRED rex_unparser_fortran_zero_iteration_implied_do_output
    RESOURCE_LOCK experimental_frontend_test_output
    LABELS "rex;UNPARSER;FORTRAN")

add_test(
  NAME rex_unparser_fortran_comment_and_pointer_fidelity
  WORKING_DIRECTORY ${_experimental_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "test \"$(grep -Fc '!! REX pointer initialization' rose_rex_unparser_fortran_pointer_comment.f90)\" -eq 1 && \
     grep -Eiq 'pointer[[:space:]]*=>[[:space:]]*target' rose_rex_unparser_fortran_pointer_comment.f90"
)
set_tests_properties(rex_unparser_fortran_comment_and_pointer_fidelity
  PROPERTIES
    FIXTURES_REQUIRED rex_unparser_fortran_pointer_comment_output
    RESOURCE_LOCK experimental_frontend_test_output
    LABELS "rex;UNPARSER;FORTRAN")

add_test(
  NAME rex_unparser_fortran_enum_bind_c_fidelity
  WORKING_DIRECTORY ${_experimental_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "test \"$(grep -Eic '^[[:space:]]*ENUM,[[:space:]]*BIND[(]C[)]' rose_rex_fortran_enum_bind_c.f90)\" -eq 2 && \
     test \"$(grep -Eic '^[[:space:]]*END[[:space:]]+ENUM' rose_rex_fortran_enum_bind_c.f90)\" -eq 2 && \
     grep -Eiq 'ENUMERATOR[[:space:]]*::[[:space:]]*rex_zero[[:space:]]*=[[:space:]]*0' rose_rex_fortran_enum_bind_c.f90 && \
     grep -Eiq 'ENUMERATOR[[:space:]]*::[[:space:]]*rex_local_next[[:space:]]*=[[:space:]]*6' rose_rex_fortran_enum_bind_c.f90"
)
set_tests_properties(rex_unparser_fortran_enum_bind_c_fidelity
  PROPERTIES
    FIXTURES_REQUIRED rex_unparser_fortran_enum_bind_c_output
    RESOURCE_LOCK experimental_frontend_test_output
    LABELS "rex;UNPARSER;FORTRAN")
