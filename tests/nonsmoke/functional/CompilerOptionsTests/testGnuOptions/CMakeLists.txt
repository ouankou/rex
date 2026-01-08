add_test(
  NAME compiler_options_gnu_options
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; \
     ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -include ${CMAKE_CURRENT_SOURCE_DIR}/testOptions.h -c ${CMAKE_CURRENT_SOURCE_DIR}/test-includeOption.C; \
     ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -isystem ${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/test-isystemOption.C; \
     cp ${CMAKE_CURRENT_SOURCE_DIR}/test-isystemOption.C local_test-isystemOption.C; \
     ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -isystem ${CMAKE_CURRENT_SOURCE_DIR} -c local_test-isystemOption.C"
)

add_test(
  NAME compiler_options_gnu_minus_x
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -c -x c /dev/null -rose:verbose 2
)

add_test(
  NAME compiler_options_gnu_skip_rose
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; \
     ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -isystem ${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/test-isystemOption.C -rose:skip_rose; \
     ${ROSE_TEST_TRANSLATOR} -rose:C -w -c ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/C_tests/test2006_135.c -rose:skip_rose"
)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_testGnuOptions_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=compiler_options_gnu_minus_x\;compiler_options_gnu_options
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_testGnuOptions_check-local PROPERTIES
  DEPENDS "compiler_options_gnu_minus_x;compiler_options_gnu_options"
)
