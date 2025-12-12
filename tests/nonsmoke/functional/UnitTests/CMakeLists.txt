include_directories(${ROSE_INCLUDES} ${Boost_INCLUDE_DIRS})

add_library(unitTestsCommon STATIC common.C)
target_include_directories(unitTestsCommon PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${ROSE_INCLUDES}
  ${Boost_INCLUDE_DIRS})
target_link_libraries(unitTestsCommon ROSE_DLL ${link_with_libraries})

add_subdirectory(Rose)
