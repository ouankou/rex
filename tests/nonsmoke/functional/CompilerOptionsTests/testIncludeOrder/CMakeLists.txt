add_test(
  NAME compiler_options_include_order_sys_include_cc
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_C_COMPILER} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/doChooseMe
    -I${CMAKE_CURRENT_SOURCE_DIR}/doNotChooseMe
    -c ${CMAKE_CURRENT_SOURCE_DIR}/testSysIncludeOptionOrder.c
)
set_tests_properties(compiler_options_include_order_sys_include_cc PROPERTIES WILL_FAIL TRUE)

add_test(
  NAME compiler_options_include_order_sys_include_translator
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0
    -isystem ${CMAKE_CURRENT_SOURCE_DIR}/doChooseMe
    -I${CMAKE_CURRENT_SOURCE_DIR}/doNotChooseMe
    -c ${CMAKE_CURRENT_SOURCE_DIR}/testSysIncludeOptionOrder.c
)
set_tests_properties(compiler_options_include_order_sys_include_translator PROPERTIES WILL_FAIL TRUE)

add_test(
  NAME compiler_options_include_order_include_order
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "${CMAKE_C_COMPILER} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/doNotChooseMe -I${CMAKE_CURRENT_SOURCE_DIR}/doChooseMe -c ${CMAKE_CURRENT_SOURCE_DIR}/testIncludeOptionOrder.c > testIncludeOptionOrder.out && ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -isystem ${CMAKE_CURRENT_SOURCE_DIR}/doNotChooseMe -I${CMAKE_CURRENT_SOURCE_DIR}/doChooseMe -c ${CMAKE_CURRENT_SOURCE_DIR}/testIncludeOptionOrder.c > testIncludeOptionOrder.out"
)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_testIncludeOrder_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=compiler_options_include_order_include_order\;compiler_options_include_order_sys_include_cc\;compiler_options_include_order_sys_include_translator
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_testIncludeOrder_check-local PROPERTIES DEPENDS "compiler_options_include_order_include_order;compiler_options_include_order_sys_include_cc;compiler_options_include_order_sys_include_translator")
