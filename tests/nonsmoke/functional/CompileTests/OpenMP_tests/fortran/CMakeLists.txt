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
  set_tests_properties(OMPFORTRAN_output_${_name}
    PROPERTIES DEPENDS OMPFORTRAN_${file_to_test} LABELS OMPFORTRAN)

  add_test(
    NAME OMPFORTRAN_output_diff_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND bash -c
      "if diff ${_output_file} ${_openmp_reference_dir}/${_output_file} > ${_diff_file}; then exit 0; else rm -f ${_diff_file}; exit 1; fi"
  )
  set_tests_properties(OMPFORTRAN_output_diff_${_name}
    PROPERTIES DEPENDS OMPFORTRAN_output_${_name} LABELS OMPFORTRAN)
endforeach()
