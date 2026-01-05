
include_directories(${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )

add_test(
  NAME compiler_options_preinclude
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0
    -include ${CMAKE_CURRENT_SOURCE_DIR}/test_preinclude_1.h
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_preinclude.C
)


########### install files ###############
