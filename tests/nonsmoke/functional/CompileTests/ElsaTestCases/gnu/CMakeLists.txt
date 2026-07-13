set(TESTCODES_1
  d0085.cc d0094.cc g0003.cc g0008.cc g0013.cc g0018.cc g0028.cc g0033.cc
  k0005.cc t0127.cc t0132.cc d0078.cc d0086.cc g0004.cc g0014.cc g0029.cc
  k0001.cc t0124.cc d0081.cc d0089.cc d0092.cc d0093.cc g0001.cc g0002.cc
  g0007.cc g0010.cc g0011.cc g0012.cc g0017.cc g0022.cc g0030.cc g0032.cc
  t0129.cc t0131.cc k0003.cc k0004.cc bugs/gb0003.cc bugs/gb0006.cc
  bugs/gb0012.cc)
set(TESTCODES_2
  asm01.c attr01.c attr02.c c0001.c c0002.c d0083.c dC0001.c dC0003.c
  dC0004.c dC0005.c dC0006.c dC0008.c dC0009.c dC0015.c dC0016.c dC0017.c
  d0095.c t0126.c bugs/gb0001.c bugs/gb0002.c bugs/gb0004.c)

set(ELSATEST_GNU_X86_TARGET_TESTCODES
  t0126.c)

set(ELSATEST_GNU_GNU89_C_TESTCODES
  c0001.c)

set(ELSATEST_GNU_NEGATIVE_TESTCODES
  # Clang 22 rejects these historical GNU extension/error specimens in their
  # configured language modes. Keep them registered as negative coverage.
  bugs/gb0002.c
  bugs/gb0003.cc
  bugs/gb0004.c
  bugs/gb0006.cc
  bugs/gb0012.cc
  d0078.cc
  d0089.cc
  dC0005.c
  dC0006.c
  dC0008.c
  dC0015.c
  g0007.cc
  g0013.cc
  g0018.cc
  k0004.cc
  t0126.c
)

function(rex_elsa_gnu_compile_test input_file label)
  set(_rose_flags ${ROSE_FLAGS})
  if(NOT input_file STREQUAL "k0001.cc")
    set(REX_COMPILE_TEST_NAME_PREFIX ELSATEST_GNU)
  endif()
  if(input_file IN_LIST ELSATEST_GNU_X86_TARGET_TESTCODES)
    list(APPEND _rose_flags -target x86_64-unknown-linux-gnu)
  endif()
  if(input_file IN_LIST ELSATEST_GNU_GNU89_C_TESTCODES)
    list(FILTER _rose_flags EXCLUDE REGEX "^-std=.*")
    list(APPEND _rose_flags -std=gnu89)
  endif()
  set(ROSE_FLAGS ${_rose_flags})
  if(input_file STREQUAL "t0126.c")
    compile_exact_negative_test(${input_file} ${label}
      "error: function declared with regparm(0) attribute was previously declared without the regparm attribute")
  elseif(input_file IN_LIST ELSATEST_GNU_NEGATIVE_TESTCODES)
    compile_negative_test(${input_file} ${label})
  else()
    list(APPEND _rose_flags ${REX_ELSA_SEMANTIC_PROBE_FLAGS})
    set(ROSE_FLAGS ${_rose_flags})
    compile_test(${input_file} ${label})
  endif()
endfunction()

set(ROSE_FLAGS  -w -std=gnu++03)
foreach(file_to_test ${TESTCODES_1})
  rex_elsa_gnu_compile_test(${file_to_test} ELSATEST)
endforeach()

set(ROSE_FLAGS -w)
foreach(file_to_test ${TESTCODES_2})
  rex_elsa_gnu_compile_test(${file_to_test} ELSATEST)
endforeach()
