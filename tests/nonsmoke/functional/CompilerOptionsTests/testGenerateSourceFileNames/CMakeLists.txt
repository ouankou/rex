add_test(
  NAME compiler_options_generate_source_file_names
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; \
    rm -f a.out rose_test1.C rose_test1.o.s test1.o; \
    ${CMAKE_CXX_COMPILER} -c ${CMAKE_CURRENT_SOURCE_DIR}/test1.C -o test1.o; \
    ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -c ${CMAKE_CURRENT_SOURCE_DIR}/test1.C -o test1.o; \
    sh ${CMAKE_CURRENT_SOURCE_DIR}/doesFilesExist.sh ./rose_test1.C; \
    sh ${CMAKE_CURRENT_SOURCE_DIR}/filesNotExist.sh ./rose_test1.o.s; \
    rm -f a.out rose_test1.C rose_test1.o.s test1.o; \
    ${CMAKE_CXX_COMPILER} -c ${CMAKE_CURRENT_SOURCE_DIR}/test1.C -o test1.o; \
    ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 test1.o; \
    sh ${CMAKE_CURRENT_SOURCE_DIR}/doesFilesExist.sh ./a.out; \
    sh ${CMAKE_CURRENT_SOURCE_DIR}/filesNotExist.sh ./rose_test1.o.s; \
    rm -f a.out rose_test1.C rose_test1.o.s test1.o; \
    ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 ${CMAKE_CURRENT_SOURCE_DIR}/test1.C; \
    sh ${CMAKE_CURRENT_SOURCE_DIR}/doesFilesExist.sh ./a.out ./rose_test1.C; \
    rm -f a.out rose_test1.C rose_test1.o.s test1.o"
)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_testGenerateSourceFileNames_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=compiler_options_generate_source_file_names
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_testGenerateSourceFileNames_check-local PROPERTIES DEPENDS compiler_options_generate_source_file_names )

add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_testGenerateSourceFileNames_conditional-check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=compiler_options_generate_source_file_names
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_testGenerateSourceFileNames_conditional-check-local PROPERTIES LABELS disabled DEPENDS compiler_options_generate_source_file_names )
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_CompilerOptionsTests_testGenerateSourceFileNames_conditional-check-local PROPERTIES DISABLED TRUE)
endif()
