include(ElsaTestCases_big_Testcodes.cmake)
include(ElsaTestCases_big_Testcodes_Additional.cmake)

set(ROSE_FLAGS -w)
set(ELSATEST_BIG_ALL_TESTCODES
  ${ELSATEST_BIG_TESTCODES}
  ${ELSATEST_BIG_ADDITIONAL_TESTCODES})
set(ELSATEST_BIG_GNU89_C_TESTCODES
  c/dC0026.c
  c/dC0027.c
  c/dC0029.c
  c/k0003a.c
  c/k0006.c
  c/k0006a.c
  c/t0006.c
  c/t0011.c
  c/t0012.c
  c/t0014.c
  c/t0015.c
  c/t0016.c
  c/t0021.c
)
set(ELSATEST_BIG_EXPECT_FAIL
  c/dC0024.c
  c/dC0025.c
  c/t0010.c
  c/t0026.c
  t0071.cc
  t0253.cc
  t0337.cc
  t0373.cc
  t0460.cc
  t0461.cc
  t0464.cc
  t0469.cc
  t0470.cc
  t0482.cc
)

function(rex_elsa_big_test input_file)
  set(_test_key ${input_file})
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  set(test_id ELSATEST_${_test_key})
  set(_rose_flags ${ROSE_FLAGS})

  set(_input_path "${input_file}")
  string(REGEX REPLACE "^c/" "ctests/" _input_path "${_input_path}")
  set(_input_path "${CMAKE_CURRENT_SOURCE_DIR}/../${_input_path}")
  if(NOT EXISTS "${_input_path}")
    message(STATUS "Skipping Elsa big source missing from tree: ${input_file}")
    return()
  endif()

  if(input_file MATCHES "^c/")
    list(FILTER _rose_flags EXCLUDE REGEX "^-std=.*")
    list(APPEND _rose_flags -rose:C)
    if(input_file IN_LIST ELSATEST_BIG_GNU89_C_TESTCODES)
      list(APPEND _rose_flags -std=gnu89)
    endif()
  else()
    list(FILTER _rose_flags EXCLUDE REGEX "^-std=.*")
    list(APPEND _rose_flags -std=gnu++03)
  endif()

  add_test(
    NAME ${test_id}
    COMMAND ${translator}
      ${_rose_flags} ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c
      ${_input_path}
  )
  set_tests_properties(${test_id} PROPERTIES LABELS ELSATEST)
  if(input_file IN_LIST ELSATEST_BIG_EXPECT_FAIL)
    set_tests_properties(${test_id} PROPERTIES WILL_FAIL TRUE)
  endif()
endfunction()

foreach(file_to_test ${ELSATEST_BIG_ALL_TESTCODES})
  rex_elsa_big_test(${file_to_test})
endforeach()

