file(GLOB OPENMP_FORTRAN_TESTCODES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  "*.f"
  "*.f90")
list(SORT OPENMP_FORTRAN_TESTCODES)

set(ROSE_FLAGS -rose:openmp:ast_only -w -rose:verbose 0)
set(TESTCODE_INCLUDES)
set(_omp_fortran_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_omp_fortran_output_dir})

foreach(file_to_test ${OPENMP_FORTRAN_TESTCODES})
  add_test(
    NAME OMPFORTRAN_${file_to_test}
    WORKING_DIRECTORY ${_omp_fortran_output_dir}
    COMMAND $<TARGET_FILE:parseOmp> ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
      -rose:output ${_omp_fortran_output_dir}/rose_${file_to_test}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  set_tests_properties(OMPFORTRAN_${file_to_test} PROPERTIES LABELS OMPFORTRAN)
endforeach()
