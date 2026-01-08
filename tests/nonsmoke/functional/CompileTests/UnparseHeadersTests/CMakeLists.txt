set(TestUnparseHeaders_SRCS
    TestUnparseHeaders.C
    UnparseHeadersTransformVisitor.C)
add_executable(TestUnparseHeaders ${TestUnparseHeaders_SRCS})
target_link_libraries(TestUnparseHeaders ROSE_DLL ${link_with_libraries} )

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)

set(_rose_switches_base "-w -rose:verbose 0 -rose:unparseHeaderFiles -c")
set(_outliner_switches
  "-rose:outline:use_dlopen -rose:outline:copy_orig_file -rose:outline:temp_variable -rose:outline:exclude_headers")
set(_hdr_root_switches "-rose:unparseHeaderFilesRootFolder unparsedHeaders")

set(_test_specs
  test0 test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11
  test12 test13 test14)

set(_disabled_specs
  test15
  test18)

foreach(spec IN LISTS _test_specs)
  set(_rose_switches
    "${_rose_switches_base} ${_outliner_switches} -rose:applicationRootDirectory ${CMAKE_CURRENT_SOURCE_DIR}/${spec}")

  add_test(
    NAME unparse_headers_prepare_${spec}
    COMMAND /bin/sh -c
      "rm -rf \"${CMAKE_CURRENT_BINARY_DIR}/${spec}\" \"${CMAKE_CURRENT_BINARY_DIR}/_rose_unparsed_headers_\" && \
       mkdir -p \"${CMAKE_CURRENT_BINARY_DIR}/${spec}\""
  )

  add_test(
    NAME unparse_headers_${spec}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      blddir=${CMAKE_CURRENT_BINARY_DIR}
      "ROSE_SWITCHES=${_rose_switches}"
      "HDR_ROOT_SWITCHES=${_hdr_root_switches}"
      ${CMAKE_CURRENT_SOURCE_DIR}/${spec}/config
      ${spec}.passed
  )

  set_tests_properties(unparse_headers_prepare_${spec} PROPERTIES LABELS UNPARSEHEADERTEST)
  set_tests_properties(unparse_headers_${spec}
    PROPERTIES DEPENDS unparse_headers_prepare_${spec} LABELS UNPARSEHEADERTEST)
endforeach()

foreach(spec IN LISTS _disabled_specs)
  set(_rose_switches
    "${_rose_switches_base} ${_outliner_switches} -rose:applicationRootDirectory ${CMAKE_CURRENT_SOURCE_DIR}/${spec}")

  add_test(
    NAME unparse_headers_${spec}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      blddir=${CMAKE_CURRENT_BINARY_DIR}
      "ROSE_SWITCHES=${_rose_switches}"
      "HDR_ROOT_SWITCHES=${_hdr_root_switches}"
      ${CMAKE_CURRENT_SOURCE_DIR}/${spec}/config
      ${spec}.passed
  )

  set_tests_properties(unparse_headers_${spec} PROPERTIES DISABLED TRUE LABELS UNPARSEHEADERTEST)
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_UnparseHeadersTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=unparse_headers_prepare_test0\;unparse_headers_prepare_test1\;unparse_headers_prepare_test10\;unparse_headers_prepare_test11\;unparse_headers_prepare_test12\;unparse_headers_prepare_test13\;unparse_headers_prepare_test14\;unparse_headers_prepare_test2\;unparse_headers_prepare_test3\;unparse_headers_prepare_test4\;unparse_headers_prepare_test5\;unparse_headers_prepare_test6\;unparse_headers_prepare_test7\;unparse_headers_prepare_test8\;unparse_headers_prepare_test9\;unparse_headers_test0\;unparse_headers_test1\;unparse_headers_test10\;unparse_headers_test11\;unparse_headers_test12\;unparse_headers_test13\;unparse_headers_test14\;unparse_headers_test15\;unparse_headers_test18\;unparse_headers_test2\;unparse_headers_test3\;unparse_headers_test4\;unparse_headers_test5\;unparse_headers_test6\;unparse_headers_test7\;unparse_headers_test8\;unparse_headers_test9
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_UnparseHeadersTests_check-local PROPERTIES
  DEPENDS "unparse_headers_prepare_test0;unparse_headers_prepare_test1;unparse_headers_prepare_test10;unparse_headers_prepare_test11;unparse_headers_prepare_test12;unparse_headers_prepare_test13;unparse_headers_prepare_test14;unparse_headers_prepare_test2;unparse_headers_prepare_test3;unparse_headers_prepare_test4;unparse_headers_prepare_test5;unparse_headers_prepare_test6;unparse_headers_prepare_test7;unparse_headers_prepare_test8;unparse_headers_prepare_test9;unparse_headers_test0;unparse_headers_test1;unparse_headers_test10;unparse_headers_test11;unparse_headers_test12;unparse_headers_test13;unparse_headers_test14;unparse_headers_test15;unparse_headers_test18;unparse_headers_test2;unparse_headers_test3;unparse_headers_test4;unparse_headers_test5;unparse_headers_test6;unparse_headers_test7;unparse_headers_test8;unparse_headers_test9"
)
