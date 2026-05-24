
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
    -include ${CMAKE_CURRENT_SOURCE_DIR}/test_preinclude_1.h
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_preinclude.C
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_preinclude_tests_test_preinclude.C PROPERTIES DEPENDS compiler_options_preinclude)

########### install files ###############
