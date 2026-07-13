
include_directories(${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )

set(_collect_comments_perf_file
  ${CMAKE_BINARY_DIR}/COMMENT_TESTS_ROSE_PERFORMANCE_DATA.csv)
set(_collect_comments_flags
  -w -rose:verbose 0
  -rose:compilationPerformanceFile ${_collect_comments_perf_file}
  -rose:collectAllCommentsAndDirectives)
set(_collect_comments_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_collect_comments_output_dir})

set(_collect_comments_test_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)

set(_collect_comments_extended_instrumented_sources
  longFile.C
  test2010_02.C
  test2013_13.C
  test2013_14.C
  test2013_15.C
  test2017_24.C
  test2017_99.C)

function(rex_add_collect_comments_tests property_name)
  set(_group_flags ${ARGN})
  rex_get_registered_compile_source_set(_collect_sources ${property_name})
  foreach(source_rel IN LISTS _collect_sources)
    set(_source_path ${REX_COMPILE_TESTS_SOURCE_DIR}/${source_rel})
    get_filename_component(_source_name ${_source_path} NAME)
    get_filename_component(_source_dir ${_source_path} DIRECTORY)
    rex_get_cxx_specimen_extra_flags(_extra_frontend_flags ${_source_name})
    string(REPLACE "/" "_" _collect_name ${source_rel})
    string(REPLACE "." "_" _collect_name ${_collect_name})
    add_test(
      NAME compiler_options_collect_comments_${_collect_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${ROSE_TEST_TRANSLATOR}
        ${_collect_comments_flags}
        ${_group_flags}
        ${_extra_frontend_flags}
        ${_functional_include_flags}
        -I${_source_dir}
        -c ${_source_path}
    )
    set_property(TEST compiler_options_collect_comments_${_collect_name}
                 APPEND PROPERTY ENVIRONMENT
                 "ROSE_TEST_OUTPUT_DIR=${_collect_comments_output_dir}")
    if(REX_EXTENDED_TEST_TIMEOUTS AND
       _source_name IN_LIST _collect_comments_extended_instrumented_sources)
      set_tests_properties(compiler_options_collect_comments_${_collect_name}
        PROPERTIES
          TIMEOUT 10800
          PROCESSORS 16)
    endif()
  endforeach()
endfunction()

rex_get_registered_compile_source_set(_collect_default_sources
  REX_CXX_TESTS_DEFAULT_SOURCES)
rex_get_registered_compile_source_set(_collect_negative_sources
  REX_CXX_TESTS_NEGATIVE_SOURCES)
if(_collect_negative_sources)
  list(REMOVE_ITEM _collect_default_sources ${_collect_negative_sources})
endif()
set_property(GLOBAL PROPERTY REX_COLLECT_COMMENTS_CXX_DEFAULT_POSITIVE_SOURCES
  ${_collect_default_sources})

rex_add_collect_comments_tests(REX_COLLECT_COMMENTS_CXX_DEFAULT_POSITIVE_SOURCES
  -std=gnu++14)
rex_add_collect_comments_tests(REX_CXX_TESTS_GNUXX03_SOURCES -std=gnu++03)
rex_add_collect_comments_tests(REX_CXX_TESTS_GNUXX17_SOURCES -std=gnu++17)
rex_add_collect_comments_tests(REX_CXX_TESTS_GNUXX20_SOURCES -std=gnu++20)
