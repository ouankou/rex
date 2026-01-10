include(OpenMP_fortran_Testcodes.cmake)
include(OpenMP_fortran_Testcodes_Disabled.cmake)

set(ROSE_FLAGS -rose:openmp:ast_only -w -rose:verbose 0)
set(TESTCODE_INCLUDES -I${CMAKE_SOURCE_DIR}/src/frontend/SageIII)
set(ROSE_DISABLED_TESTCODES ${OPENMP_FORTRAN_DISABLED_TESTCODES})

foreach(file_to_test ${OPENMP_FORTRAN_TESTCODES} ${OPENMP_FORTRAN_DISABLED_TESTCODES})
  add_test(
    NAME OMPFORTRAN_${file_to_test}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:parseOmp> ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  set_tests_properties(OMPFORTRAN_${file_to_test} PROPERTIES LABELS OMPFORTRAN)
  if(file_to_test IN_LIST ROSE_DISABLED_TESTCODES)
    set_tests_properties(OMPFORTRAN_${file_to_test} PROPERTIES DISABLED TRUE)
  endif()
endforeach()

set(_openmp_fortran_diff_tests ${OPENMP_FORTRAN_TESTCODES})
list(REMOVE_ITEM _openmp_fortran_diff_tests testNewOFP.f)
set(_openmp_reference_dir ${CMAKE_CURRENT_SOURCE_DIR}/referenceResults)

foreach(file_to_test IN LISTS _openmp_fortran_diff_tests)
  string(REPLACE "." "_" _name "${file_to_test}")
  string(REPLACE "-" "_" _name "${_name}")
  set(_rose_file rose_${file_to_test})
  set(_output_file ${_rose_file}.output)
  set(_diff_file ${_output_file}.diff)

  add_test(
    NAME OMPFORTRAN_output_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND bash -c "grep '!$omp' ${_rose_file} > ${_output_file} || exit 0"
  )
  set_tests_properties(OMPFORTRAN_output_${_name} PROPERTIES DEPENDS OMPFORTRAN_${file_to_test} LABELS OMPFORTRAN)

  add_test(
    NAME OMPFORTRAN_output_diff_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND bash -c
      "if diff ${_output_file} ${_openmp_reference_dir}/${_output_file} > ${_diff_file}; then exit 0; else rm -f ${_diff_file}; exit 1; fi"
  )
  set_tests_properties(OMPFORTRAN_output_diff_${_name} PROPERTIES DEPENDS OMPFORTRAN_output_${_name} LABELS OMPFORTRAN)
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_OpenMP_tests_fortran_conditional-check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=OMPFORTRAN_allocate_predefined_modifiers.f\;OMPFORTRAN_allocate_user_defined_modifiers.f\;OMPFORTRAN_array-one.f\;OMPFORTRAN_array_initialize.f\;OMPFORTRAN_collapse.f90\;OMPFORTRAN_continuation.f90\;OMPFORTRAN_critical.f90\;OMPFORTRAN_default.f\;OMPFORTRAN_exampleA251f.f\;OMPFORTRAN_flush.f\;OMPFORTRAN_helloworld.f\;OMPFORTRAN_helloworld0.f\;OMPFORTRAN_helloworld2.f\;OMPFORTRAN_jacobi.f\;OMPFORTRAN_master.f\;OMPFORTRAN_ompdo-decremental.f\;OMPFORTRAN_ompdo-default.f\;OMPFORTRAN_ompdo-multiple.f\;OMPFORTRAN_ompdo-orphaned.f\;OMPFORTRAN_ompdo-static.f\;OMPFORTRAN_parallel-1.f\;OMPFORTRAN_parallel-2.f\;OMPFORTRAN_parallel.f\;OMPFORTRAN_paralleldo-1.f90\;OMPFORTRAN_paralleldo.f90\;OMPFORTRAN_private.f\;OMPFORTRAN_reduction.f\;OMPFORTRAN_schedule.f\;OMPFORTRAN_section1.f\;OMPFORTRAN_shared-array.f\;OMPFORTRAN_shared.f\;OMPFORTRAN_single-copyprivate.f90\;OMPFORTRAN_single.f\;OMPFORTRAN_task_largenumber.f\;OMPFORTRAN_task_largenumber2.f\;OMPFORTRAN_task_scalar.f\;OMPFORTRAN_task_simplest.f\;OMPFORTRAN_testNewOFP.f
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_OpenMP_tests_fortran_conditional-check-local PROPERTIES DEPENDS "OMPFORTRAN_allocate_predefined_modifiers.f;OMPFORTRAN_allocate_user_defined_modifiers.f;OMPFORTRAN_array-one.f;OMPFORTRAN_array_initialize.f;OMPFORTRAN_collapse.f90;OMPFORTRAN_continuation.f90;OMPFORTRAN_critical.f90;OMPFORTRAN_default.f;OMPFORTRAN_exampleA251f.f;OMPFORTRAN_flush.f;OMPFORTRAN_helloworld.f;OMPFORTRAN_helloworld0.f;OMPFORTRAN_helloworld2.f;OMPFORTRAN_jacobi.f;OMPFORTRAN_master.f;OMPFORTRAN_ompdo-decremental.f;OMPFORTRAN_ompdo-default.f;OMPFORTRAN_ompdo-multiple.f;OMPFORTRAN_ompdo-orphaned.f;OMPFORTRAN_ompdo-static.f;OMPFORTRAN_parallel-1.f;OMPFORTRAN_parallel-2.f;OMPFORTRAN_parallel.f;OMPFORTRAN_paralleldo-1.f90;OMPFORTRAN_paralleldo.f90;OMPFORTRAN_private.f;OMPFORTRAN_reduction.f;OMPFORTRAN_schedule.f;OMPFORTRAN_section1.f;OMPFORTRAN_shared-array.f;OMPFORTRAN_shared.f;OMPFORTRAN_single-copyprivate.f90;OMPFORTRAN_single.f;OMPFORTRAN_task_largenumber.f;OMPFORTRAN_task_largenumber2.f;OMPFORTRAN_task_scalar.f;OMPFORTRAN_task_simplest.f;OMPFORTRAN_testNewOFP.f" )
