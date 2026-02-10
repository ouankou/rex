set(files_to_test
  empty.cl
  transpose_kernel.cl
  nbody.cl
  nbody_cpu.cl
  nbody_gpu.cl
)

set(ROSE_FLAGS -rose:skipfinalCompileStep)
set(ROSE_DISABLED_TESTCODES)
if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(ROSE_DISABLED_TESTCODES
    transpose_kernel.cl
    nbody.cl
    nbody_cpu.cl
    nbody_gpu.cl
  )
endif()

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} OPENCLTEST TRUE)
endforeach()

add_test(
  NAME OPENCLTEST_empty
  COMMAND testTranslator ${ROSE_FLAGS}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/empty.cl
)
set_tests_properties(OPENCLTEST_empty PROPERTIES LABELS "OPENCLTEST")

# Makefile check targets.
set(_opencl_group_tests)
foreach(file_to_test ${files_to_test})
  string(REPLACE "/" "_" _test_key "${file_to_test}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  list(APPEND _opencl_group_tests "OPENCLTEST_${_test_key}")
endforeach()
list(APPEND _opencl_group_tests OPENCLTEST_empty)
add_test(
  NAME nonsmoke_functional_CompileTests_OpenClTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_opencl_group_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_OpenClTests_check-local PROPERTIES DEPENDS "${_opencl_group_tests}" )
if(NOT ROSE_BUILD_OPENCL_LANGUAGE_SUPPORT)
  set_tests_properties(nonsmoke_functional_CompileTests_OpenClTests_check-local PROPERTIES DISABLED TRUE LABELS disabled )
endif()
