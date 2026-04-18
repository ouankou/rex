include_directories(${ROSE_INCLUDES})

add_executable(member_function member_function.cpp)
target_link_libraries(member_function ROSE_DLL ${link_with_libraries})

add_test(
  NAME systemc_member_function
  COMMAND member_function -c
    -I${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/input_member_function.cpp
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/input_hello_world.C" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
  add_test(
    NAME systemc_input_hello_world
    COMMAND testTranslator -w
      -I${CMAKE_BINARY_DIR}
      -I${CMAKE_CURRENT_SOURCE_DIR}/include
      -c ${CMAKE_CURRENT_SOURCE_DIR}/input_hello_world.C
  )
endif()
