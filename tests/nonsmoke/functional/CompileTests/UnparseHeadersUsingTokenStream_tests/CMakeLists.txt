set(TestUnparseHeadersUsingTokens_SRCS
    TestUnparseHeadersUsingTokens.C
    UnparseHeadersTransformVisitorUsingTokens.C)

set(whiteSpaceTransformation_SRCS
    whiteSpaceTransformation.C)

add_executable(TestUnparseHeadersUsingTokens ${TestUnparseHeadersUsingTokens_SRCS})
target_link_libraries(TestUnparseHeadersUsingTokens ROSE_DLL ${link_with_libraries} )

add_executable(whiteSpaceTransformation ${whiteSpaceTransformation_SRCS})
target_link_libraries(whiteSpaceTransformation ROSE_DLL ${link_with_libraries} )

add_executable(transformVariable transformVariable.C)
target_link_libraries(transformVariable ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_rose_switches "-w -rose:verbose 0 -rose:unparseHeaderFiles -rose:unparse_tokens")
set(_hdr_root_switches "-rose:unparseHeaderFilesRootFolder unparsedHeaders")

set(_specimens
  test0 test1 test2 test3 test5 test6 test11 test12 test13 test14 test15 test16)

set(_disabled_specimens
  test4 test7 test8 test9 test10
  test17 test18 test19 test21 test22)

foreach(spec IN LISTS _specimens)
  add_test(
    NAME unparse_tokens_prepare_${spec}
    COMMAND /bin/sh -c
      "rm -rf \"${CMAKE_CURRENT_BINARY_DIR}/${spec}\" && mkdir -p \"${CMAKE_CURRENT_BINARY_DIR}/${spec}\""
  )

  add_test(
    NAME unparse_tokens_${spec}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      blddir=${CMAKE_CURRENT_BINARY_DIR}
      "ROSE_SWITCHES=${_rose_switches}"
      "HDR_ROOT_SWITCHES=${_hdr_root_switches}"
      ${CMAKE_CURRENT_SOURCE_DIR}/${spec}/config
      ${spec}.passed
  )

  set_tests_properties(unparse_tokens_prepare_${spec} PROPERTIES LABELS UNPARSEHEADERTOKEN)
  set_tests_properties(unparse_tokens_${spec}
    PROPERTIES DEPENDS unparse_tokens_prepare_${spec} LABELS UNPARSEHEADERTOKEN)
endforeach()

foreach(spec IN LISTS _disabled_specimens)
  add_test(
    NAME unparse_tokens_${spec}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      blddir=${CMAKE_CURRENT_BINARY_DIR}
      "ROSE_SWITCHES=${_rose_switches}"
      "HDR_ROOT_SWITCHES=${_hdr_root_switches}"
      ${CMAKE_CURRENT_SOURCE_DIR}/${spec}/config
      ${spec}.passed
  )
  set_tests_properties(unparse_tokens_${spec} PROPERTIES DISABLED TRUE LABELS UNPARSEHEADERTOKEN)
endforeach()

add_test(
  NAME unparse_tokens_transformVariable
  COMMAND transformVariable -rose:unparse_tokens -c ${CMAKE_CURRENT_SOURCE_DIR}/badInput.c
)

add_test(
  NAME unparse_tokens_transformVariable2
  COMMAND transformVariable -rose:unparse_tokens -c ${CMAKE_CURRENT_SOURCE_DIR}/badInput2.c
)

add_test(
  NAME nonsmoke_functional_CompileTests_UnparseHeadersUsingTokenStream_tests_badInput.c
  COMMAND $<TARGET_FILE:transformVariable> -rose:unparse_tokens
    -c ${CMAKE_CURRENT_SOURCE_DIR}/badInput.c
)
set_tests_properties(nonsmoke_functional_CompileTests_UnparseHeadersUsingTokenStream_tests_badInput.c
  PROPERTIES DEPENDS unparse_tokens_transformVariable)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_UnparseHeadersUsingTokenStream_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astQuery_test2_input1\;astQuery_test3_cxx_grammar\;astSymbolTable_test1\;unparse_tokens_test0\;unparse_tokens_test11\;unparse_tokens_test12\;unparse_tokens_test13\;unparse_tokens_test14\;unparse_tokens_test15\;unparse_tokens_test16\;unparse_tokens_test17\;unparse_tokens_test18\;unparse_tokens_test19\;unparse_tokens_test21\;unparse_tokens_test22\;unparse_tokens_test5\;unparse_tokens_test6\;unparse_tokens_test7\;unparse_tokens_transformVariable\;unparse_tokens_transformVariable2
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_UnparseHeadersUsingTokenStream_tests_check-local PROPERTIES
  DEPENDS "astQuery_test2_input1;astQuery_test3_cxx_grammar;astSymbolTable_test1;unparse_tokens_test0;unparse_tokens_test11;unparse_tokens_test12;unparse_tokens_test13;unparse_tokens_test14;unparse_tokens_test15;unparse_tokens_test16;unparse_tokens_test17;unparse_tokens_test18;unparse_tokens_test19;unparse_tokens_test21;unparse_tokens_test22;unparse_tokens_test5;unparse_tokens_test6;unparse_tokens_test7;unparse_tokens_transformVariable;unparse_tokens_transformVariable2"
)
