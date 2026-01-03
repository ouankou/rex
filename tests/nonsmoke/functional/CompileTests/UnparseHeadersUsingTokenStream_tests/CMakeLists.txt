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
  test4 test7 test8 test9 test10)

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
