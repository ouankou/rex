set(files_to_test
  test2003_12.c test2004_20.c test2005_172.c test2006_132.c test2006_48.c
  test2007_177.c test2008_01.c test2008_02.c test2009_08.c test2009_07.c
  test2009_02.c test2009_22.c test2009_01.c test2009_21.c test2009_04.c
  test2009_16.c test2010_14.c test2010_10.c test2010_01.c test2011_05.c
  test2010_15.c test2010_17.c test2010_11.c test2012_07.c test2012_153.c
  test2012_30.c test2012_51.c test2012_68.c test2012_84.c test2012_05.c
  test2012_148.c test2012_175.c test2012_49.c test2012_67.c test2012_83.c
  test2012_99.c test2012_11.c test2012_158.c test2012_35.c test2012_61.c
  test2012_70.c test2012_86.c test2012_127.c test2012_165.c test2012_47.c
  test2012_62.c test2012_71.c test2012_87.c test2012_140.c test2012_170.c
  test2012_48.c test2012_65.c test2012_81.c test2012_93.c test2012_08.c
  test2012_154.c test2012_34.c test2012_53.c test2012_69.c test2012_85.c
  test2013_05.c test2013_06.c test2013_22.c test2013_24.c test2013_25.c
  test2013_26.c test2013_29.c test2013_30.c test2013_37.c test2013_38.c
  test2013_39.c test2013_52.c test2013_55.c test2013_62.c test2013_64.c
  test2013_67.c test2013_68.c test2013_69.c test2013_71.c test2013_72.c
  test2013_78.c commentTest.c testAnsiC.c gconv_info.c
  test_CplusplusMacro_C.c math.c testCvsCpp.c stdio.c
  YardenPragmaPackExample.c)

# C_tests specimens that only work in a C89-family mode belong in this bucket,
# not in the generic default-C sweep.
list(APPEND files_to_test
  ../C_tests/test2009_04.c
  ../C_tests/test2010_01.c
  ../C_tests/test2012_27.c
  ../C_tests/test2012_32.c
  ../C_tests/test2012_116.c
  ../C_tests/test2012_44.c
  ../C_tests/test2012_45.c
  ../C_tests/test2012_173.c
  ../C_tests/test2012_173_redhat_rel6.c
  ../C_tests/test2012_79.c
  ../C_tests/test2012_80.c
  ../C_tests/test2012_135.c
  ../C_tests/test2013_41.c
  ../C_tests/test2013_53.c
  ../C_tests/test2013_56.c
  ../C_tests/test2013_57.c
  ../C_tests/test2013_58.c
  ../C_tests/test2013_59.c
  ../C_tests/test2013_60.c
  ../C_tests/test2013_61.c
  ../C_tests/test2013_63.c
  ../C_tests/test2013_105.c
  ../C_tests/test2013_106.c
  ../C_tests/test2013_107.c
  ../C_tests/test2013_108.c
  ../C_tests/test2013_109.c
  ../C_tests/test2013_111.c
  ../C_tests/test2014_06.c
  ../C_tests/test2014_20.c
  ../C_tests/test2014_21.c
  ../C_tests/test2014_74.c
  ../C_tests/test2015_28.c
  ../C_tests/test2017_03.c
  ../C_tests/test2017_12.c
  ../C_tests/test2017_67.c
  ../C_tests/test2018_49.c
  ../C_tests/test2018_50.c
  ../C_tests/test2018_51.c
  ../C_tests/test2018_63.c
  ../C_tests/test2020_06.c
)
list(REMOVE_DUPLICATES files_to_test)

set(ROSE_FLAGS -std=c89)
if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  list(APPEND ROSE_FLAGS  )
endif()
list(APPEND ROSE_FLAGS -w)

set(ROSE_GNU89_TESTCODES
  ../C_tests/test2012_44.c
  ../C_tests/test2012_45.c
  ../C_tests/test2012_173.c
  ../C_tests/test2012_173_redhat_rel6.c
  ../C_tests/test2015_28.c
  ../C_tests/test2017_03.c
)

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _rex_c89_tests_processor)
set(REX_C89_TESTS_HOST_IS_X86 FALSE)
if(_rex_c89_tests_processor MATCHES "^(x86_64|amd64|i[3-6]86)$")
  set(REX_C89_TESTS_HOST_IS_X86 TRUE)
endif()

set(C89_TESTCODES_X86_ONLY
  test2012_175.c
  test2013_72.c
  ../C_tests/test2012_173.c
  ../C_tests/test2012_173_redhat_rel6.c
  ../C_tests/test2017_03.c
)
if(NOT REX_C89_TESTS_HOST_IS_X86)
  rex_register_compile_source_set(REX_C89_X86_ONLY_SOURCES
    ${C89_TESTCODES_X86_ONLY})
  list(REMOVE_ITEM files_to_test ${C89_TESTCODES_X86_ONLY})
  list(REMOVE_ITEM ROSE_GNU89_TESTCODES ${C89_TESTCODES_X86_ONLY})
endif()
rex_register_compile_source_set(REX_C89_GNU89_SOURCES ${ROSE_GNU89_TESTCODES})

# test2013_06.c is invalid even for stock C compilers because its header uses
# `bool` before <stdbool.h> is included.
list(REMOVE_ITEM files_to_test test2013_06.c)

rex_register_compile_source_set(REX_C89_TESTS_SOURCES ${files_to_test} test2013_06.c)

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} C89TEST TRUE)
endforeach()

compile_negative_test(test2013_06.c C89TEST TRUE)

add_test(
  NAME C89TEST_conftest_link
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND testTranslator -std=c89 -w -I${CMAKE_CURRENT_SOURCE_DIR}/../C_tests
          ${CMAKE_CURRENT_SOURCE_DIR}/../C_tests/conftest.c
          -o conftest_configure_test
)
set_tests_properties(C89TEST_conftest_link PROPERTIES LABELS "C89TEST")

rex_register_compile_source_set(REX_C89_TESTS_SOURCES ../C_tests/conftest.c)

set(_c89_test_names "")
foreach(file_to_test ${files_to_test})
  set(_test_key "${file_to_test}")
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  list(APPEND _c89_test_names "C89TEST_${_test_key}")
endforeach()
list(REMOVE_DUPLICATES _c89_test_names)

rex_verify_compile_test_source_coverage(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTIES REX_C89_TESTS_SOURCES REX_C89_X86_ONLY_SOURCES
)
