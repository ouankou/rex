find_package(Threads REQUIRED)

add_library(gtest STATIC src/gtest-all.cc)
target_include_directories(gtest
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gtest PUBLIC Threads::Threads)

add_library(gtest_main STATIC src/gtest_main.cc)
target_link_libraries(gtest_main PUBLIC gtest)
