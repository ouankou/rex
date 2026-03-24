#-------------------------------------------------------------------------------
add_executable(testQuery testQuery.C)
target_link_libraries(testQuery ROSE_DLL ${link_with_libraries})

#-------------------------------------------------------------------------------
add_executable(testQuery2 testQuery2.C)
target_link_libraries(testQuery2 ROSE_DLL ${link_with_libraries})

#-------------------------------------------------------------------------------
add_executable(testQuery3 testQuery3.C)
target_link_libraries(testQuery3 ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

add_test(
  NAME astQuery_testQuery_input1
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=testQuery input1.C [testQuery_input1.C.passed]"
    "USE_SUBDIR=yes"
    "CMD=$<TARGET_FILE:testQuery> -c ${CMAKE_CURRENT_SOURCE_DIR}/input1.C"
    ${_exit_status}
    testQuery_input1.C.passed
)

add_test(
  NAME astQuery_testQuery2_input1
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=testQuery2 input1.C [testQuery2_input1.C.passed]"
    "USE_SUBDIR=yes"
    "CMD=$<TARGET_FILE:testQuery2> -c ${CMAKE_CURRENT_SOURCE_DIR}/input1.C"
    ${_exit_status}
    testQuery2_input1.C.passed
)

add_test(
  NAME astQuery_testQuery3_input1
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=testQuery3 input1.C [testQuery3_input1.C.passed]"
    "USE_SUBDIR=yes"
    "CMD=$<TARGET_FILE:testQuery3> -c ${CMAKE_CURRENT_SOURCE_DIR}/input1.C"
    ${_exit_status}
    testQuery3_input1.C.passed
)

add_test(
  NAME astQuery_test1_conf
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    srcdir=${CMAKE_CURRENT_SOURCE_DIR}
    EXE=$<TARGET_FILE:testQuery>
    ${CMAKE_CURRENT_SOURCE_DIR}/test1.conf
    test1.passed
)

add_test(
  NAME astQuery_test2_input1
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=test2 input1.C [test2.passed]"
    "USE_SUBDIR=yes"
    "CMD=$<TARGET_FILE:testQuery2> -rose:verbose 0 -c ${CMAKE_CURRENT_SOURCE_DIR}/input1.C"
    ${_exit_status}
    test2.passed
)

set(_rose_include_flags)
foreach(entry IN LISTS ROSE_INCLUDES)
  if(entry MATCHES "^-I" OR entry MATCHES "^-isystem")
    list(APPEND _rose_include_flags ${entry})
  else()
    list(APPEND _rose_include_flags -I${entry})
  endif()
endforeach()
string(JOIN " " _rose_include_flags_str ${_rose_include_flags})

add_test(
  NAME astQuery_test3_cxx_grammar
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=test3 Cxx_Grammar.C [test3.passed]"
    "USE_SUBDIR=yes"
    "CMD=$<TARGET_FILE:testQuery3> -rose:verbose 0 -I${CMAKE_BINARY_DIR} ${_rose_include_flags_str} -c ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_Grammar.C"
    ${_exit_status}
    test3.passed
)

install(TARGETS testQuery testQuery2 testQuery3 DESTINATION bin)

