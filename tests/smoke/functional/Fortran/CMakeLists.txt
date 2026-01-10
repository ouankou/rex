add_executable(testBafio EXCLUDE_FROM_ALL testBafio.C)
target_link_libraries(testBafio ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

add_test(
  NAME smoke_fortran_testBafio
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=Fortran parser test [testBafio.passed]"
    "CMD=$<TARGET_FILE:testBafio>"
    ${_exit_status}
    testBafio.passed
)
set_tests_properties(smoke_fortran_testBafio PROPERTIES DISABLED TRUE LABELS "smoke;fortran")

# Makefile check targets.
add_test(
  NAME smoke_functional_Fortran_check-fortran
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=smoke_fortran_testBafio
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(smoke_functional_Fortran_check-fortran PROPERTIES LABELS disabled DEPENDS smoke_fortran_testBafio )
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(smoke_functional_Fortran_check-fortran PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME smoke_functional_Fortran_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=smoke_fortran_testBafio
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(smoke_functional_Fortran_check-local PROPERTIES DEPENDS smoke_fortran_testBafio )
