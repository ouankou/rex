
include_directories(${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; LANL_POP run tests require bash")
endif()

add_subdirectory(netcdf-4.1.1)

set(_lanl_pop_pristine "${CMAKE_CURRENT_BINARY_DIR}/pop_pristine_blddir/run/pop")
set(_lanl_pop_modified "${CMAKE_CURRENT_BINARY_DIR}/pop_modified_blddir/run/pop")
if(EXISTS "${_lanl_pop_pristine}" AND EXISTS "${_lanl_pop_modified}")
  set(_lanl_pop_ready TRUE)
else()
  set(_lanl_pop_ready FALSE)
endif()

if(_lanl_pop_ready)
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
  set_tests_properties(lanl_pop_compare_answers PROPERTIES DEPENDS "lanl_pop_run_pristine;lanl_pop_run_modified")
else()
  message(STATUS "LANL_POP run tests disabled: missing pop executables in ${CMAKE_CURRENT_BINARY_DIR}")
endif()
