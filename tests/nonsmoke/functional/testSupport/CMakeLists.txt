add_subdirectory(gtest)

add_library(roseUnitTestSupport STATIC testSupport.cpp)
target_include_directories(roseUnitTestSupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(roseUnitTestSupport PUBLIC ROSE_DLL gtest ${link_with_libraries})
