add_executable(testMerge testMerge.C)
target_link_libraries(testMerge ROSE_DLL ${link_with_libraries})
install(TARGETS testMerge DESTINATION bin)

add_test(
  NAME testMerge_test1
  COMMAND testMerge -rose:verbose 0 -rose:ast:merge
          -c ${CMAKE_CURRENT_SOURCE_DIR}/mangleTest.C
          ${CMAKE_CURRENT_SOURCE_DIR}/mangleTwo.C
)

add_test(
  NAME testMerge_test2
  COMMAND testMerge -rose:verbose 0
          -c ${CMAKE_CURRENT_SOURCE_DIR}/mangleTest.C
          ${CMAKE_CURRENT_SOURCE_DIR}/mangleTwo.C
          ${CMAKE_CURRENT_SOURCE_DIR}/mangleThree.C
)
set_tests_properties(testMerge_test2 PROPERTIES DISABLED TRUE)

add_test(
  NAME testMerge_test3
  COMMAND testMerge -rose:verbose 0 -rose:ast:merge
          -c ${CMAKE_CURRENT_SOURCE_DIR}/mangleTest.C
          ${CMAKE_CURRENT_SOURCE_DIR}/mangleTwo.C
          ${CMAKE_CURRENT_SOURCE_DIR}/mangleThree.C
)
set_tests_properties(testMerge_test3 PROPERTIES DISABLED TRUE)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_astMergeTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=testMerge_test1\;testMerge_test2\;testMerge_test3
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astMergeTests_check-local PROPERTIES
  DEPENDS "testMerge_test1;testMerge_test2;testMerge_test3"
)
add_test(
  NAME nonsmoke_functional_roseTests_astMergeTests_check_testMerge
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=testMerge_test1\;testMerge_test2\;testMerge_test3
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astMergeTests_check_testMerge PROPERTIES
  LABELS "disabled"
  DEPENDS "testMerge_test1;testMerge_test2;testMerge_test3"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astMergeTests_check_testMerge PROPERTIES DISABLED TRUE)
endif()
