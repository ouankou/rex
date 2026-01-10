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
    blddir=${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/test1.conf
    test1.passed
)
set_tests_properties(astQuery_test1_conf PROPERTIES DISABLED TRUE)

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
set_tests_properties(astQuery_test2_input1 PROPERTIES DISABLED TRUE)

set(_rose_include_flags)
foreach(entry IN LISTS ROSE_INCLUDES)
  if(entry MATCHES "^-I" OR entry MATCHES "^-isystem")
    list(APPEND _rose_include_flags ${entry})
  else()
    list(APPEND _rose_include_flags -I${entry})
  endif()
endforeach()

add_test(
  NAME astQuery_test3_cxx_grammar
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=test3 Cxx_Grammar.C [test3.passed]"
    "USE_SUBDIR=yes"
    "CMD=$<TARGET_FILE:testQuery3> -rose:verbose 0 -I${CMAKE_BINARY_DIR} ${_rose_include_flags} -c ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_Grammar.C"
    ${_exit_status}
    test3.passed
)
set_tests_properties(astQuery_test3_cxx_grammar PROPERTIES DISABLED TRUE)

install(TARGETS testQuery testQuery2 testQuery3 DESTINATION bin)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_astQueryTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astQuery_test2_input1\;astQuery_test3_cxx_grammar\;astQuery_testQuery2_input1\;astQuery_testQuery3_input1\;astQuery_testQuery_input1
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astQueryTests_check-local PROPERTIES DEPENDS "astQuery_test2_input1;astQuery_test3_cxx_grammar;astQuery_testQuery2_input1;astQuery_testQuery3_input1;astQuery_testQuery_input1" )
