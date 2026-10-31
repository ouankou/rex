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

# Clang implements the 'model' attribute only for x86-64 and LoongArch; other
# targets ignore it and accept d0099.c. Pin the specimen to x86-64 so its exact
# rejection is the same on every host.
set(ELSATEST_NOTCOMPILABLE_X86_TARGET_TESTCODES
  d0099.c)

set(REX_COMPILE_TEST_NAME_PREFIX ELSATEST_NOTCOMPILABLE_GNU)
foreach(file_to_test ${ELSATEST_NOTCOMPILABLE_GNU_ALL_TESTCODES})
  elsa_not_compilable_test(${file_to_test} ELSATEST
    ${_elsa_notcompilable_gnu_roots})
endforeach()
