set(ROSE_FLAGS -w -std=gnu++03)
file(GLOB ELSATEST_NOTCOMPILABLE_GNU_ALL_TESTCODES
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  "*.cc"
  "*.c")
list(SORT ELSATEST_NOTCOMPILABLE_GNU_ALL_TESTCODES)

set(_elsa_notcompilable_gnu_roots
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../gnu
  ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

foreach(file_to_test ${ELSATEST_NOTCOMPILABLE_GNU_ALL_TESTCODES})
  elsa_not_compilable_test(${file_to_test} ELSATEST
    ${_elsa_notcompilable_gnu_roots})
endforeach()
