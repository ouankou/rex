set(ROSE_FLAGS -rose:openacc -w -rose:verbose 0)
set(_openacc_includes -I${CMAKE_SOURCE_DIR}/src/frontend/SageIII)

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; OpenACC Fortran tests require bash")
endif()

set(_openacc_f90_tests
  trans.F90
)

foreach(specimen IN LISTS _openacc_f90_tests)
  string(REPLACE "." "_" _name "${specimen}")
  string(REPLACE "-" "_" _name "${_name}")
  add_test(
    NAME OpenACC_fortran_${specimen}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND testTranslator ${ROSE_FLAGS} ${_openacc_includes}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
  )
  set_tests_properties(OpenACC_fortran_${specimen} PROPERTIES LABELS OpenACC_fortran)

  set(_rose_file rose_${specimen})
  set(_output_file ${_rose_file}.output)
  set(_diff_file ${_output_file}.diff)
  set(_reference_dir ${CMAKE_CURRENT_SOURCE_DIR}/referenceResults)

  add_test(
    NAME OpenACC_fortran_output_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "grep '!$acc' ${_rose_file} > ${_output_file} || exit 0"
  )
  set_tests_properties(OpenACC_fortran_output_${_name}
    PROPERTIES DEPENDS OpenACC_fortran_${specimen} LABELS OpenACC_fortran)

  add_test(
    NAME OpenACC_fortran_output_diff_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "if diff ${_output_file} ${_reference_dir}/${_output_file} > ${_diff_file}; then exit 0; else rm -f ${_diff_file}; exit 1; fi"
  )
  set_tests_properties(OpenACC_fortran_output_diff_${_name}
    PROPERTIES DEPENDS OpenACC_fortran_output_${_name} LABELS OpenACC_fortran)
endforeach()
