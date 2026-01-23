
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
set_tests_properties(lanl_pop_compare_answers PROPERTIES DEPENDS "lanl_pop_run_pristine;lanl_pop_run_modified)")
