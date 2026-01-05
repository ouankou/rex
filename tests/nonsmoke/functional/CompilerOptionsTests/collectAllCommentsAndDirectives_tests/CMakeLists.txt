
include_directories(${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )

include(${CMAKE_CURRENT_SOURCE_DIR}/collectAllCommentsAndDirectives_tests.cmake)

set(_collect_comments_perf_file
  ${CMAKE_BINARY_DIR}/COMMENT_TESTS_ROSE_PERFORMANCE_DATA.csv)
set(_collect_comments_flags
  -w -rose:verbose 0
  -rose:compilationPerformanceFile ${_collect_comments_perf_file}
  -rose:collectAllCommentsAndDirectives)

set(_collect_comments_include_flags
  -I${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/A++Code)

set(_collect_comments_test_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)

foreach(testcode IN LISTS COLLECT_COMMENTS_TEST_SOURCES)
  string(REPLACE "." "_" _collect_name ${testcode})
  add_test(
    NAME compiler_options_collect_comments_${_collect_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${ROSE_TEST_TRANSLATOR}
      ${_collect_comments_flags}
      ${_collect_comments_include_flags}
      -I${_collect_comments_test_dir}
      -c ${_collect_comments_test_dir}/${testcode}
  )
endforeach()

foreach(testcode IN LISTS COLLECT_COMMENTS_DISABLED_TEST_SOURCES)
  string(REPLACE "." "_" _collect_name ${testcode})
  add_test(
    NAME compiler_options_collect_comments_${_collect_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${ROSE_TEST_TRANSLATOR}
      ${_collect_comments_flags}
      ${_collect_comments_include_flags}
      -I${_collect_comments_test_dir}
      -c ${_collect_comments_test_dir}/${testcode}
  )
  set_tests_properties(compiler_options_collect_comments_${_collect_name}
    PROPERTIES DISABLED TRUE)
endforeach()
