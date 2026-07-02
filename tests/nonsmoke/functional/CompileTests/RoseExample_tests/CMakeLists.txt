set(files_to_test
  testRoseHeaders_00.C testRoseHeaders_01.C testRoseHeaders_02.C
  testRoseHeaders_03.C testRoseHeaders_04.C testRoseHeaders_09.C
  testRoseHeaders_14.C
  testRosePublicConfigPaths.C
  test2003_33.C test2003_34.C test2006_43.C)

set(_rose_example_extended_instrumented_timeout_sources
  testRoseHeaders_01.C
  testRoseHeaders_02.C
  testRoseHeaders_03.C
  testRoseHeaders_04.C
  test2003_33.C
  test2003_34.C
  test2006_43.C
)

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
  set(_test_name rose_example_${file_to_test})
  string(REPLACE "." "_" _test_name "${_test_name}")
  if(REX_EXTENDED_TEST_TIMEOUTS AND
     file_to_test IN_LIST _rose_example_extended_instrumented_timeout_sources)
    set_tests_properties(${_test_name} PROPERTIES
      TIMEOUT 10800
      PROCESSORS 16)
  endif()
endforeach()
set(_rose_example_tests)
foreach(file_to_test ${files_to_test})
  string(REPLACE "/" "_" _test_key "${file_to_test}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  list(APPEND _rose_example_tests "rose_example_${_test_key}")
endforeach()

set(_rose_example_grammar_flags -w -std=c++${CMAKE_CXX_STANDARD} -rose:verbose 0)
set(_rose_example_generated_grammar_instrumented_timeout 21600)
add_test(
  NAME rose_example_src_frontend_SageIII_Cxx_Grammar_C
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTranslator>
    ${_rose_example_grammar_flags}
    -I${CMAKE_BINARY_DIR}
    ${ROSE_INCLUDE_FLAGS}
    -c ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_Grammar.C
)
set_tests_properties(rose_example_src_frontend_SageIII_Cxx_Grammar_C PROPERTIES  LABELS rose_example)
if(REX_EXTENDED_TEST_TIMEOUTS)
  set_tests_properties(rose_example_src_frontend_SageIII_Cxx_Grammar_C PROPERTIES
    TIMEOUT ${_rose_example_generated_grammar_instrumented_timeout}
    PROCESSORS 16)
endif()
