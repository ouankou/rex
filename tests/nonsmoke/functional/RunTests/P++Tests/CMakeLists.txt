set(_pplus_tests
  test1.C
)

set(_pplus_include_flags
  -I${CMAKE_CURRENT_SOURCE_DIR}
  -I${CMAKE_CURRENT_SOURCE_DIR}/../..
  -I${CMAKE_SOURCE_DIR}/include
  -I${CMAKE_SOURCE_DIR}/ExamplePreprocessors/A++Preprocessor/ArrayTransformationGrammar
)

foreach(test_file IN LISTS _pplus_tests)
  string(REPLACE "." "_" _name "${test_file}")
  add_test(
    NAME RunTests_Pxx_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:testTranslator> ${_pplus_include_flags}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
  )
  set_tests_properties(RunTests_Pxx_${_name} PROPERTIES DISABLED TRUE LABELS RunTests)
endforeach()
