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

set(ROSE_FLAGS  -w -std=gnu++03)
foreach(file_to_test ${TESTCODES_1})
  compile_test(${file_to_test} ELSATEST)
endforeach()

set(ROSE_FLAGS -w -rose:C)
foreach(file_to_test ${TESTCODES_2})
  compile_test(${file_to_test} ELSATEST)
endforeach()

