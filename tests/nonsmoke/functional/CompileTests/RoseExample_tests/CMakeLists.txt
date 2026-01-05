set(files_to_test
  testRoseHeaders_00.C testRoseHeaders_01.C testRoseHeaders_02.C
  testRoseHeaders_03.C testRoseHeaders_04.C testRoseHeaders_05.C
  testRoseHeaders_06.C testRoseHeaders_07.C testRoseHeaders_08.C
  test2003_33.C test2003_34.C test2004_46.C test2006_43.C)

set(ROSE_FLAGS  -w )

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} rose_example)
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
set_tests_properties(rose_example_src_frontend_SageIII_Cxx_Grammar_C
  PROPERTIES DISABLED TRUE LABELS rose_example)
