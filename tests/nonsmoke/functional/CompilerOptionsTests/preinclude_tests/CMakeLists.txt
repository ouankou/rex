
include_directories(${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )

add_test(
  NAME compiler_options_preinclude
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0
    -include ${CMAKE_CURRENT_SOURCE_DIR}/test_preinclude_1.h
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_preinclude.C
)

add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_preinclude_tests_test_preinclude.C
  COMMAND $<TARGET_FILE:testTranslator> -rose:verbose 0
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_preinclude.C
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_preinclude_tests_test_preinclude.C
  PROPERTIES DEPENDS compiler_options_preinclude)


########### install files ###############

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_preinclude_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=compiler_options_preinclude
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_preinclude_tests_check-local PROPERTIES
  DEPENDS "compiler_options_preinclude"
)
