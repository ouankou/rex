set(TESTCODES
  test07.C test09.C  test10.C  test11.C
  test12.C test13.C  test14.C)

# This is the long test of ROSE (takes a while!)
list(APPEND TESTCODES
  test16.C test19.C  test20.C  test21.C
  test42.C test43.C  test44.C  test45.C
  test47.C test48.C  test50.C  test53.C
  test56.C test58.C
  test60.C
)

set(ROSE_FLAGS  -w -rose:verbose 0)

set(TESTCODE_INCLUDES -I${CMAKE_CURRENT_SOURCE_DIR})

foreach(file_to_test ${TESTCODES})
  add_test(
    NAME HIDDENTYPETEST_${file_to_test}
    COMMAND testTranslator ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(HIDDENTYPETEST_${file_to_test} PROPERTIES LABELS HIDDENTYPETEST)
endforeach()
set(_hidden_type_tests)
foreach(file_to_test ${TESTCODES})
  list(APPEND _hidden_type_tests "HIDDENTYPETEST_${file_to_test}")
endforeach()

set(_additional_hidden_type_tests
  test15.C
  test22.C
  test57.C
  test59.C
)

foreach(file_to_test ${_additional_hidden_type_tests})
  add_test(
    NAME HIDDENTYPETEST_${file_to_test}
    COMMAND testTranslator ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(HIDDENTYPETEST_${file_to_test} PROPERTIES  LABELS HIDDENTYPETEST)
endforeach()

add_test(
  NAME HIDDENTYPETEST_test2005_56.C
  COMMAND testTranslator ${ROSE_FLAGS}
    -DTEST_STRING_MACRO="Thu Apr 14 08:18:33 PDT 2005"
    -c ${CMAKE_CURRENT_SOURCE_DIR}/../Cxx_tests/test2005_56.C
    -o ${CMAKE_CURRENT_BINARY_DIR}/test2005_56.o)
  set_tests_properties(HIDDENTYPETEST_test2005_56.C PROPERTIES LABELS HIDDENTYPETEST)

