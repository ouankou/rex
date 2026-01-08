
include_directories(${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; LANL_POP run tests require bash")
endif()

add_subdirectory(netcdf-4.1.1)

add_test(
  NAME lanl_pop_run_pristine
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; (cd pop_pristine_blddir/run && ./pop) > pristine.ans.unfiltered; sed <pristine.ans.unfiltered > pristine.ans -e \"s%Timer number.* seconds%%\" -e \"s%Timer stats.* seconds%%\" -e \"s%max = .* seconds%%\" -e \"s%mean= .* seconds%%\""
)

add_test(
  NAME lanl_pop_run_modified
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; (cd pop_modified_blddir/run && ./pop) > modified.ans.unfiltered; sed <modified.ans.unfiltered > modified.ans -e \"s%Timer number.* seconds%%\" -e \"s%Timer stats.* seconds%%\" -e \"s%max = .* seconds%%\" -e \"s%mean= .* seconds%%\""
)

add_test(
  NAME lanl_pop_compare_answers
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "diff -u pristine.ans modified.ans"
)
set_tests_properties(lanl_pop_compare_answers
  PROPERTIES DEPENDS "lanl_pop_run_pristine;lanl_pop_run_modified")

set_tests_properties(
  lanl_pop_run_pristine
  lanl_pop_run_modified
  lanl_pop_compare_answers
  PROPERTIES DISABLED TRUE)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_RunTests_FortranTests_LANL_POP_check
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=lanl_pop_compare_answers\;lanl_pop_run_modified\;lanl_pop_run_pristine\;netcdf_check_nc_config\;netcdf_test_prog_F\;netcdf_test_prog_c\;netcdf_test_prog_f90
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_RunTests_FortranTests_LANL_POP_check PROPERTIES
  LABELS "disabled"
  DEPENDS "lanl_pop_compare_answers;lanl_pop_run_modified;lanl_pop_run_pristine;netcdf_check_nc_config;netcdf_test_prog_F;netcdf_test_prog_c;netcdf_test_prog_f90"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_RunTests_FortranTests_LANL_POP_check PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_RunTests_FortranTests_LANL_POP_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=lanl_pop_compare_answers\;lanl_pop_run_modified\;lanl_pop_run_pristine\;netcdf_check_nc_config\;netcdf_test_prog_F\;netcdf_test_prog_c\;netcdf_test_prog_f90
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_RunTests_FortranTests_LANL_POP_check-local PROPERTIES
  DEPENDS "lanl_pop_compare_answers;lanl_pop_run_modified;lanl_pop_run_pristine;netcdf_check_nc_config;netcdf_test_prog_F;netcdf_test_prog_c;netcdf_test_prog_f90"
)
add_test(
  NAME nonsmoke_functional_RunTests_FortranTests_LANL_POP_conditional-check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=lanl_pop_compare_answers\;lanl_pop_run_modified\;lanl_pop_run_pristine\;netcdf_check_nc_config\;netcdf_test_prog_F\;netcdf_test_prog_c\;netcdf_test_prog_f90
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_RunTests_FortranTests_LANL_POP_conditional-check-local PROPERTIES
  DEPENDS "lanl_pop_compare_answers;lanl_pop_run_modified;lanl_pop_run_pristine;netcdf_check_nc_config;netcdf_test_prog_F;netcdf_test_prog_c;netcdf_test_prog_f90"
)
