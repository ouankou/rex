include_directories(${ROSE_INCLUDES})

set(_cxx_tests
  testSpuriousOutput_1.C
  testSpuriousOutput_2.C
)

set(_f90_tests
  testSpuriousOutput_3.f90
  testSpuriousOutput_4.f90
)

set(_cxx_test_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)

function(_add_spurious_output_test name command)
  add_test(
    NAME ${name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "rm -f outputFile; ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 ${command} > outputFile 2>&1; outputLines=$(wc -l < outputFile); if [ \"$outputLines\" -eq 0 ]; then exit 0; else cat outputFile; exit 1; fi"
  )
endfunction()

foreach(testcode IN LISTS _cxx_tests)
  get_filename_component(_name ${testcode} NAME_WE)
  _add_spurious_output_test(
    compiler_options_spurious_${_name}
    "-c ${CMAKE_CURRENT_SOURCE_DIR}/${testcode}"
  )
endforeach()

_add_spurious_output_test(
  compiler_options_spurious_multiple_files_1
  "-c ${_cxx_test_dir}/test2012_246a.C ${_cxx_test_dir}/test2012_246b.C"
)

_add_spurious_output_test(
  compiler_options_spurious_multiple_files_2
  "-c ${_cxx_test_dir}/test2018_01.C ${_cxx_test_dir}/test2018_02.C"
)

_add_spurious_output_test(
  compiler_options_spurious_hello_world
  "-c ${CMAKE_CURRENT_SOURCE_DIR}/helloworld.C"
)

if(enable-fortran)
  foreach(testcode IN LISTS _f90_tests)
    get_filename_component(_name ${testcode} NAME_WE)
    _add_spurious_output_test(
      compiler_options_spurious_${_name}
      "-c ${CMAKE_CURRENT_SOURCE_DIR}/${testcode}"
    )
  endforeach()
endif()
