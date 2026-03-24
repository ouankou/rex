add_test(
  NAME compiler_options_include_order_sys_include_cc
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_C_COMPILER} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/doChooseMe
    -I${CMAKE_CURRENT_SOURCE_DIR}/doNotChooseMe
    -c ${CMAKE_CURRENT_SOURCE_DIR}/testSysIncludeOptionOrder.c
)

add_test(
  NAME compiler_options_include_order_sys_include_translator
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0
    -isystem ${CMAKE_CURRENT_SOURCE_DIR}/doChooseMe
    -I${CMAKE_CURRENT_SOURCE_DIR}/doNotChooseMe
    -c ${CMAKE_CURRENT_SOURCE_DIR}/testSysIncludeOptionOrder.c
)

add_test(
  NAME compiler_options_include_order_include_order
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "${CMAKE_C_COMPILER} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/doNotChooseMe -I${CMAKE_CURRENT_SOURCE_DIR}/doChooseMe -c ${CMAKE_CURRENT_SOURCE_DIR}/testIncludeOptionOrder.c > testIncludeOptionOrder.out && ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -isystem ${CMAKE_CURRENT_SOURCE_DIR}/doNotChooseMe -I${CMAKE_CURRENT_SOURCE_DIR}/doChooseMe -c ${CMAKE_CURRENT_SOURCE_DIR}/testIncludeOptionOrder.c > testIncludeOptionOrder.out"
)

