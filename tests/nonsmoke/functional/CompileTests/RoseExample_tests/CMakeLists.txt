set(files_to_test
  testRoseHeaders_00.C testRoseHeaders_01.C testRoseHeaders_02.C
  testRoseHeaders_03.C testRoseHeaders_04.C testRoseHeaders_09.C
  testRoseHeaders_14.C
  testRosePublicConfigPaths.C
  test2003_33.C test2003_34.C test2006_43.C)

set(ROSE_FLAGS  -w )

foreach(file_to_test ${files_to_test})
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}")
    message(
      FATAL_ERROR
        "RoseExample_tests references missing source '${file_to_test}' in ${CMAKE_CURRENT_SOURCE_DIR}"
    )
  endif()
endforeach()

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} rose_example)
endforeach()
set(_rose_example_tests)
foreach(file_to_test ${files_to_test})
  string(REPLACE "/" "_" _test_key "${file_to_test}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  list(APPEND _rose_example_tests "rose_example_${_test_key}")
endforeach()

set(_rose_example_grammar_flags -w -std=c++14 -rose:verbose 0)
add_test(
  NAME rose_example_src_frontend_SageIII_Cxx_Grammar_C
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTranslator>
    ${_rose_example_grammar_flags}
    -I${CMAKE_BINARY_DIR}
    ${ROSE_INCLUDE_FLAGS}
    -c ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_Grammar.C
)
set_tests_properties(rose_example_src_frontend_SageIII_Cxx_Grammar_C PROPERTIES DISABLED TRUE LABELS rose_example)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_RoseExample_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_rose_example_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_RoseExample_tests_check-local PROPERTIES DEPENDS "${_rose_example_tests}" )
