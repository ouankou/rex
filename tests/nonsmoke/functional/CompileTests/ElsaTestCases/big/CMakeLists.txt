include(ElsaTestCases_big_Testcodes.cmake)
include(ElsaTestCases_big_Testcodes_Disabled.cmake)

set(ROSE_FLAGS -w)
set(ROSE_DISABLED_TESTCODES ${ELSATEST_BIG_DISABLED_TESTCODES})

function(rex_elsa_big_test input_file)
  set(_test_key ${input_file})
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  set(test_id ELSATEST_${_test_key})

  set(_input_path "${input_file}")
  string(REGEX REPLACE "^c/" "ctests/" _input_path "${_input_path}")
  set(_input_path "${CMAKE_CURRENT_SOURCE_DIR}/../${_input_path}")

  add_test(
    NAME ${test_id}
    COMMAND ${translator}
      ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c
      ${_input_path}
  )

  set(_labels ELSATEST)
  set(_disable_test FALSE)
  if(DEFINED ROSE_DISABLED_TESTCODES AND input_file IN_LIST ROSE_DISABLED_TESTCODES)
    list(APPEND _labels disabled)
    if(NOT REX_ENABLE_DISABLED_TESTS)
      set(_disable_test TRUE)
    endif()
  endif()
  list(REMOVE_DUPLICATES _labels)
  set_tests_properties(${test_id} PROPERTIES LABELS "${_labels}")

  if(_disable_test)
    set_tests_properties(${test_id} PROPERTIES DISABLED TRUE)
  endif()
endfunction()

foreach(file_to_test ${ELSATEST_BIG_TESTCODES} ${ELSATEST_BIG_DISABLED_TESTCODES})
  rex_elsa_big_test(${file_to_test})
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_ElsaTestCases_big_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=ELSATEST_c_d0124b_c\;ELSATEST_c_dC0010_c\;ELSATEST_c_dC0011_c\;ELSATEST_c_dC0013_c\;ELSATEST_c_dC0017_c\;ELSATEST_c_dC0018_c\;ELSATEST_c_dC0019_c\;ELSATEST_c_dC0020_c\;ELSATEST_c_dC0021_c\;ELSATEST_c_dC0022_c\;ELSATEST_c_dC0023_c\;ELSATEST_c_dC0024_c\;ELSATEST_c_dC0025_c\;ELSATEST_c_dC0026_c\;ELSATEST_c_dC0027_c\;ELSATEST_c_dC0028_c\;ELSATEST_c_dC0029_c\;ELSATEST_c_dC0030_c\;ELSATEST_c_dC0031_c\;ELSATEST_c_dC0032_c\;ELSATEST_c_k0001_c\;ELSATEST_c_k0002_c\;ELSATEST_c_k0003_c\;ELSATEST_c_k0003a_c\;ELSATEST_c_k0004_c\;ELSATEST_c_k0005_c\;ELSATEST_c_k0006_c\;ELSATEST_c_k0006a_c\;ELSATEST_c_k0007_c\;ELSATEST_c_k0008_c\;ELSATEST_c_k0009_c\;ELSATEST_c_k0010_c\;ELSATEST_c_k0011_c\;ELSATEST_c_t0001_c\;ELSATEST_c_t0002_c\;ELSATEST_c_t0003_c\;ELSATEST_c_t0005_c\;ELSATEST_c_t0006_c\;ELSATEST_c_t0007_c\;ELSATEST_c_t0008_c\;ELSATEST_c_t0009_c\;ELSATEST_c_t0010_c\;ELSATEST_c_t0011_c\;ELSATEST_c_t0012_c\;ELSATEST_c_t0014_c\;ELSATEST_c_t0015_c\;ELSATEST_c_t0016_c\;ELSATEST_c_t0017_c\;ELSATEST_c_t0018_c\;ELSATEST_c_t0019_c\;ELSATEST_c_t0020_c\;ELSATEST_c_t0021_c\;ELSATEST_c_t0023_c\;ELSATEST_c_t0024_c\;ELSATEST_c_t0025_c\;ELSATEST_c_t0026_c\;ELSATEST_gnu_asm01_c\;ELSATEST_gnu_attr01_c\;ELSATEST_gnu_attr02_c\;ELSATEST_gnu_bugs_gb0001_c\;ELSATEST_gnu_bugs_gb0002_c\;ELSATEST_gnu_bugs_gb0003_cc\;ELSATEST_gnu_bugs_gb0004_c\;ELSATEST_gnu_bugs_gb0006_cc\;ELSATEST_gnu_bugs_gb0012_cc\;ELSATEST_gnu_c0001_c\;ELSATEST_gnu_c0002_c\;ELSATEST_gnu_d0078_cc\;ELSATEST_gnu_d0081_cc\;ELSATEST_gnu_d0083_c\;ELSATEST_gnu_d0085_cc\;ELSATEST_gnu_d0086_cc\;ELSATEST_gnu_d0089_cc\;ELSATEST_gnu_d0092_cc\;ELSATEST_gnu_d0093_cc\;ELSATEST_gnu_d0094_cc\;ELSATEST_gnu_d0095_c\;ELSATEST_gnu_dC0001_c\;ELSATEST_gnu_dC0003_c\;ELSATEST_gnu_dC0004_c\;ELSATEST_gnu_dC0005_c\;ELSATEST_gnu_dC0006_c\;ELSATEST_gnu_dC0008_c\;ELSATEST_gnu_dC0009_c\;ELSATEST_gnu_dC0015_c\;ELSATEST_gnu_dC0016_c\;ELSATEST_gnu_dC0017_c\;ELSATEST_gnu_g0001_cc\;ELSATEST_gnu_g0002_cc\;ELSATEST_gnu_g0003_cc\;ELSATEST_gnu_g0004_cc\;ELSATEST_gnu_g0007_cc\;ELSATEST_gnu_g0008_cc\;ELSATEST_gnu_g0010_cc\;ELSATEST_gnu_g0011_cc\;ELSATEST_gnu_g0012_cc\;ELSATEST_gnu_g0013_cc\;ELSATEST_gnu_g0014_cc\;ELSATEST_gnu_g0017_cc\;ELSATEST_gnu_g0018_cc\;ELSATEST_gnu_g0022_cc\;ELSATEST_gnu_g0028_cc\;ELSATEST_gnu_g0029_cc\;ELSATEST_gnu_g0030_cc\;ELSATEST_gnu_g0032_cc\;ELSATEST_gnu_g0033_cc\;ELSATEST_gnu_k0001_cc\;ELSATEST_gnu_k0003_cc\;ELSATEST_gnu_k0004_cc\;ELSATEST_gnu_k0005_cc\;ELSATEST_gnu_t0124_cc\;ELSATEST_gnu_t0126_c\;ELSATEST_gnu_t0127_cc\;ELSATEST_gnu_t0129_cc\;ELSATEST_gnu_t0131_cc\;ELSATEST_gnu_t0132_cc\;ELSATEST_kandr_t0001_c\;ELSATEST_kandr_t0002_c\;ELSATEST_kandr_t0003_c\;ELSATEST_kandr_t0004_c\;ELSATEST_std_12_3_2c_cc\;ELSATEST_std_12_6_2_2a_cc\;ELSATEST_std_12_6_2_2b_cc\;ELSATEST_std_13_1a_cc\;ELSATEST_std_13_1b_cc\;ELSATEST_std_13_1c_cc\;ELSATEST_std_13_1d_cc\;ELSATEST_std_13_1e_cc\;ELSATEST_std_13_1f_cc\;ELSATEST_std_13_1g_cc\;ELSATEST_std_13_2a_cc\;ELSATEST_std_13_3_1_1_2_cc\;ELSATEST_std_3_4_3a_cc\;ELSATEST_std_7_1_3b_cc\;ELSATEST_std_7_3_1_2b_cc\;ELSATEST_std_7_3_2_cc\;ELSATEST_std_7_3_3a_cc\;ELSATEST_std_7_3_3b_cc\;ELSATEST_std_7_3_3c_cc\;ELSATEST_std_7_3_3d_cc\;ELSATEST_std_7_3_3f_cc\;ELSATEST_std_7_3_3k_cc\;ELSATEST_std_7_3_4b_cc\;ELSATEST_std_7_3_4d_cc\;ELSATEST_std_7_3_4e_cc\;ELSATEST_std_8_2_1_cc\;ELSATEST_std_8_2_7a_cc\;ELSATEST_std_8_2_7b_cc\;ELSATEST_std_8_3_3_2_cc
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_ElsaTestCases_big_check-local PROPERTIES
  LABELS "disabled"
  DEPENDS "ELSATEST_c_d0124b_c;ELSATEST_c_dC0010_c;ELSATEST_c_dC0011_c;ELSATEST_c_dC0013_c;ELSATEST_c_dC0017_c;ELSATEST_c_dC0018_c;ELSATEST_c_dC0019_c;ELSATEST_c_dC0020_c;ELSATEST_c_dC0021_c;ELSATEST_c_dC0022_c;ELSATEST_c_dC0023_c;ELSATEST_c_dC0024_c;ELSATEST_c_dC0025_c;ELSATEST_c_dC0026_c;ELSATEST_c_dC0027_c;ELSATEST_c_dC0028_c;ELSATEST_c_dC0029_c;ELSATEST_c_dC0030_c;ELSATEST_c_dC0031_c;ELSATEST_c_dC0032_c;ELSATEST_c_k0001_c;ELSATEST_c_k0002_c;ELSATEST_c_k0003_c;ELSATEST_c_k0003a_c;ELSATEST_c_k0004_c;ELSATEST_c_k0005_c;ELSATEST_c_k0006_c;ELSATEST_c_k0006a_c;ELSATEST_c_k0007_c;ELSATEST_c_k0008_c;ELSATEST_c_k0009_c;ELSATEST_c_k0010_c;ELSATEST_c_k0011_c;ELSATEST_c_t0001_c;ELSATEST_c_t0002_c;ELSATEST_c_t0003_c;ELSATEST_c_t0005_c;ELSATEST_c_t0006_c;ELSATEST_c_t0007_c;ELSATEST_c_t0008_c;ELSATEST_c_t0009_c;ELSATEST_c_t0010_c;ELSATEST_c_t0011_c;ELSATEST_c_t0012_c;ELSATEST_c_t0014_c;ELSATEST_c_t0015_c;ELSATEST_c_t0016_c;ELSATEST_c_t0017_c;ELSATEST_c_t0018_c;ELSATEST_c_t0019_c;ELSATEST_c_t0020_c;ELSATEST_c_t0021_c;ELSATEST_c_t0023_c;ELSATEST_c_t0024_c;ELSATEST_c_t0025_c;ELSATEST_c_t0026_c;ELSATEST_gnu_asm01_c;ELSATEST_gnu_attr01_c;ELSATEST_gnu_attr02_c;ELSATEST_gnu_bugs_gb0001_c;ELSATEST_gnu_bugs_gb0002_c;ELSATEST_gnu_bugs_gb0003_cc;ELSATEST_gnu_bugs_gb0004_c;ELSATEST_gnu_bugs_gb0006_cc;ELSATEST_gnu_bugs_gb0012_cc;ELSATEST_gnu_c0001_c;ELSATEST_gnu_c0002_c;ELSATEST_gnu_d0078_cc;ELSATEST_gnu_d0081_cc;ELSATEST_gnu_d0083_c;ELSATEST_gnu_d0085_cc;ELSATEST_gnu_d0086_cc;ELSATEST_gnu_d0089_cc;ELSATEST_gnu_d0092_cc;ELSATEST_gnu_d0093_cc;ELSATEST_gnu_d0094_cc;ELSATEST_gnu_d0095_c;ELSATEST_gnu_dC0001_c;ELSATEST_gnu_dC0003_c;ELSATEST_gnu_dC0004_c;ELSATEST_gnu_dC0005_c;ELSATEST_gnu_dC0006_c;ELSATEST_gnu_dC0008_c;ELSATEST_gnu_dC0009_c;ELSATEST_gnu_dC0015_c;ELSATEST_gnu_dC0016_c;ELSATEST_gnu_dC0017_c;ELSATEST_gnu_g0001_cc;ELSATEST_gnu_g0002_cc;ELSATEST_gnu_g0003_cc;ELSATEST_gnu_g0004_cc;ELSATEST_gnu_g0007_cc;ELSATEST_gnu_g0008_cc;ELSATEST_gnu_g0010_cc;ELSATEST_gnu_g0011_cc;ELSATEST_gnu_g0012_cc;ELSATEST_gnu_g0013_cc;ELSATEST_gnu_g0014_cc;ELSATEST_gnu_g0017_cc;ELSATEST_gnu_g0018_cc;ELSATEST_gnu_g0022_cc;ELSATEST_gnu_g0028_cc;ELSATEST_gnu_g0029_cc;ELSATEST_gnu_g0030_cc;ELSATEST_gnu_g0032_cc;ELSATEST_gnu_g0033_cc;ELSATEST_gnu_k0001_cc;ELSATEST_gnu_k0003_cc;ELSATEST_gnu_k0004_cc;ELSATEST_gnu_k0005_cc;ELSATEST_gnu_t0124_cc;ELSATEST_gnu_t0126_c;ELSATEST_gnu_t0127_cc;ELSATEST_gnu_t0129_cc;ELSATEST_gnu_t0131_cc;ELSATEST_gnu_t0132_cc;ELSATEST_kandr_t0001_c;ELSATEST_kandr_t0002_c;ELSATEST_kandr_t0003_c;ELSATEST_kandr_t0004_c;ELSATEST_std_12_3_2c_cc;ELSATEST_std_12_6_2_2a_cc;ELSATEST_std_12_6_2_2b_cc;ELSATEST_std_13_1a_cc;ELSATEST_std_13_1b_cc;ELSATEST_std_13_1c_cc;ELSATEST_std_13_1d_cc;ELSATEST_std_13_1e_cc;ELSATEST_std_13_1f_cc;ELSATEST_std_13_1g_cc;ELSATEST_std_13_2a_cc;ELSATEST_std_13_3_1_1_2_cc;ELSATEST_std_3_4_3a_cc;ELSATEST_std_7_1_3b_cc;ELSATEST_std_7_3_1_2b_cc;ELSATEST_std_7_3_2_cc;ELSATEST_std_7_3_3a_cc;ELSATEST_std_7_3_3b_cc;ELSATEST_std_7_3_3c_cc;ELSATEST_std_7_3_3d_cc;ELSATEST_std_7_3_3f_cc;ELSATEST_std_7_3_3k_cc;ELSATEST_std_7_3_4b_cc;ELSATEST_std_7_3_4d_cc;ELSATEST_std_7_3_4e_cc;ELSATEST_std_8_2_1_cc;ELSATEST_std_8_2_7a_cc;ELSATEST_std_8_2_7b_cc;ELSATEST_std_8_3_3_2_cc"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_CompileTests_ElsaTestCases_big_check-local PROPERTIES DISABLED TRUE)
endif()
