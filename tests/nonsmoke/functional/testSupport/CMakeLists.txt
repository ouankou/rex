add_subdirectory(gtest)

add_library(roseUnitTestSupport STATIC testSupport.cpp)
target_include_directories(roseUnitTestSupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(roseUnitTestSupport PUBLIC ROSE_DLL gtest ${link_with_libraries})

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_testSupport_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
