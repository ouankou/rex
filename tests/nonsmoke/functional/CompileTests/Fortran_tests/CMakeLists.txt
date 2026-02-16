add_subdirectory(gfortranTestSuite)
add_subdirectory(experimental_frontend_tests)

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; Fortran_tests require bash")
endif()

include(Fortran_F90_Testcodes.cmake)
include(Fortran_F77_Testcodes.cmake)
include(Fortran_F03_Testcodes.cmake)
include(Fortran_Comments_Testcodes.cmake)

set(_fortran_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_fortran_output_dir})

set(ROSE_FLAGS -rose:verbose 0 -rose:detect_dangling_pointers 2
               -I${CMAKE_CURRENT_SOURCE_DIR}
               -I${CMAKE_CURRENT_BINARY_DIR}
               -I${_fortran_output_dir}
               -outputdir ${_fortran_output_dir})
string(JOIN " " ROSE_FLAGS_STR ${ROSE_FLAGS})

function(fortran_test input_file)
  set(_fortran_flags ${FORTRAN_FLAGS} ${ARGN})
  add_test(
    NAME FORTRANTEST_${input_file}_translation
    WORKING_DIRECTORY ${_fortran_output_dir}
    COMMAND testTranslator ${ROSE_FLAGS} ${_fortran_flags}
            -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
  add_test(
    NAME FORTRANTEST_${input_file}_graph_generation
    WORKING_DIRECTORY ${_fortran_output_dir}
    COMMAND testGraphGeneration ${ROSE_FLAGS} ${_fortran_flags}
            -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
  add_test(
    NAME FORTRANTEST_${input_file}_token_generation
    WORKING_DIRECTORY ${_fortran_output_dir}
    COMMAND testTokenGeneration ${ROSE_FLAGS} ${_fortran_flags}
            -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
  set_tests_properties(FORTRANTEST_${input_file}_translation PROPERTIES LABELS FORTRANTEST)
  set_tests_properties(FORTRANTEST_${input_file}_graph_generation PROPERTIES DEPENDS FORTRANTEST_${input_file}_translation LABELS FORTRANTEST)
  set_tests_properties(FORTRANTEST_${input_file}_token_generation PROPERTIES DEPENDS FORTRANTEST_${input_file}_graph_generation LABELS FORTRANTEST)
endfunction()

function(fortran_frontend_negative_test input_file)
  set(_fortran_flags ${FORTRAN_FLAGS} ${ARGN})
  string(JOIN " " _fortran_flags_str ${_fortran_flags})
  add_test(
    NAME FORTRANTEST_${input_file}_frontend_negative
    WORKING_DIRECTORY ${_fortran_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "! $<TARGET_FILE:testTranslator> ${ROSE_FLAGS_STR} ${_fortran_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file}"
  )
  set_tests_properties(FORTRANTEST_${input_file}_frontend_negative PROPERTIES LABELS FORTRANTEST)
endfunction()

set(FORTRAN_FLAGS -rose:fortran_std=f77)
foreach(F77_test ${F77_TESTCODES})
  fortran_test(${F77_test})
endforeach()

set(FORTRAN_FLAGS -rose:fortran_std=f90)
set(_fortran_cray_pointer_tests
  test2007_81.f90
  test2007_82.f90
  test2007_83.f90
  test2007_84.f90
  test2011_imperial_crayPointers.f90
)
set(_fortran_frontend_negative_tests
  rex_test2026_coarray_image_selector.f90
  rex_test2026_cuda_kernel_launch_call.f90
  rex_test2026_allocate_specs.f90
  test2007_88.f90
  test2007_152.f90
  test2007_177.f90
  test2010_19.f90
  test2010_26.f90
  test2010_102.f90
  test2010_175.f90
  test2011_38.f90
  test2011_59.f90
)
foreach(F90_test ${F90_TESTCODES})
  if(F90_test IN_LIST _fortran_frontend_negative_tests)
    fortran_frontend_negative_test(${F90_test})
  elseif(F90_test IN_LIST _fortran_cray_pointer_tests)
    fortran_test(${F90_test} -fcray-pointer)
  else()
    fortran_test(${F90_test})
  endif()
endforeach()

add_test(
  NAME fortran_rex_coarray_image_selector_reports_backend_limitation
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "! $<TARGET_FILE:testTranslator> ${ROSE_FLAGS_STR} -rose:fortran_std=f90 -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_coarray_image_selector.f90"
)

foreach(F90_comment_test ${F90_COMMENTS_TESTCODES})
  fortran_test(${F90_comment_test})
endforeach()

set(FORTRAN_FLAGS -rose:skip_syntax_check -rose:skipfinalCompileStep
                  -rose:fortran_std=f2003)
set(_fortran_f03_frontend_negative_tests
  test2007_171.f03
  test2010_27.f03
  test2010_74.f03
)
foreach(F03_test ${F03_TESTCODES})
  if(F03_test IN_LIST _fortran_f03_frontend_negative_tests)
    fortran_frontend_negative_test(${F03_test})
  else()
    fortran_test(${F03_test})
  endif()
endforeach()

# Extra tests from Automake conditional-check-local and supporting rules.
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

set(_fortran_mpi_type_tests
  ISO_C_BINDING.f03
  mpi_f08_types.f03
  mpi_f08_interfaces_test.f03
)
foreach(testcode IN LISTS _fortran_mpi_type_tests)
  string(REPLACE "." "_" _name ${testcode})
  add_test(
    NAME fortran_testMPItypes_${_name}
    WORKING_DIRECTORY ${_fortran_output_dir}
    COMMAND testTranslator ${ROSE_FLAGS}
      -rose:skip_syntax_check -rose:skipfinalCompileStep
      ${CMAKE_CURRENT_SOURCE_DIR}/${testcode}
  )
endforeach()

add_test(
  NAME fortran_testMultipleFortranFiles
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "$<TARGET_FILE:testTranslator> ${ROSE_FLAGS_STR} -rose:fortran_std=f90 -c ${CMAKE_CURRENT_SOURCE_DIR}/test2010_78.f90 ${CMAKE_CURRENT_SOURCE_DIR}/test2010_79.f90 || cat ${CMAKE_CURRENT_SOURCE_DIR}/test2010_79.f90 rose_test2010_79.f90"
)

set(_fortran_reference_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests/referenceResults)
add_test(
  NAME fortran_testPreprocessedOutput
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "$<TARGET_FILE:testTranslator> ${ROSE_FLAGS_STR} -rose:fortran_std=f90 -c ${CMAKE_CURRENT_SOURCE_DIR}/test2007_suffixTest_07.F90 && diff <(tail -n 6 rose_test2007_suffixTest_07.F90) <(tail -n 6 ${_fortran_reference_dir}/rose_test2007_suffixTest_07.F90.output)"
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

add_test(
  NAME fortran_module_module_A_file
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND testTranslator ${ROSE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/module_A_file.f90
)
add_test(
  NAME fortran_module_module_B_file
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND testTranslator ${ROSE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/module_B_file.f90
)
set_tests_properties(fortran_module_module_B_file PROPERTIES DEPENDS fortran_module_module_A_file)

add_test(
  NAME fortran_module_test2019_module_1_file
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND testTranslator ${ROSE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2019_module_1_file.f90
)
add_test(
  NAME fortran_module_test2019_module_2_file
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND testTranslator ${ROSE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2019_module_2_file.f90
)
set_tests_properties(fortran_module_test2019_module_2_file PROPERTIES DEPENDS fortran_module_test2019_module_1_file)

add_test(
  NAME fortran_module_test2019_private_attribute2
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND testTranslator ${ROSE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2019_private_attribute2.f90
)
add_test(
  NAME fortran_module_test2019_private_attribute
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND testTranslator ${ROSE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2019_private_attribute.f90
)
set_tests_properties(fortran_module_test2019_private_attribute PROPERTIES DEPENDS fortran_module_test2019_private_attribute2)

add_test(
  NAME fortran_module_test2020_use_iso_c_binding_3
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND testTranslator ${ROSE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2020_use_iso_c_binding_3.f90
)
add_test(
  NAME fortran_module_test2020_use_iso_c_binding_4
  WORKING_DIRECTORY ${_fortran_output_dir}
  COMMAND testTranslator ${ROSE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2020_use_iso_c_binding_4.f90
)
set_tests_properties(fortran_module_test2020_use_iso_c_binding_4 PROPERTIES DEPENDS fortran_module_test2020_use_iso_c_binding_3)
