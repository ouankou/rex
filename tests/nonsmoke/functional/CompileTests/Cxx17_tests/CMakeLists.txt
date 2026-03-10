set(CXX17_TESTCODES
  test2018_01.C
  test2018_04.C
  test2018_19.C
  test2018_02.C
  test2018_05.C
  test2018_06.C
  test2018_07.C
  test2018_08.C
  test2018_09.C
  test2018_10.C
  test2018_11.C
  test2018_13.C
  test2018_14.C
  test2018_15.C
  test2018_16.C
  test2018_17.C
  test2018_18.C
  test2018_20.C
  test2018_21.C
  test2018_22.C
  test2018_23.C
  test2018_24.C
  test2018_25.C
  test2018_26.C
  test2018_27.C
  test2018_28.C
  test2018_29.C
  test2018_30.C
  test2018_31.C
  test2018_32.C
  test2018_33.C
  test2018_34.C
  test2018_35.C
  test2018_36.C
  test2018_37.C
  test2018_38.C
  test2018_39.C
  test2018_40.C
  test2018_41.C
  test2018_42.C
  test2018_43.C
  test2018_44.C
  test2018_45.C
  test2018_46.C
  test2018_47.C
  test2018_48.C
  test2018_49.C
  test2018_50.C
  test2018_51.C
  test2018_52.C
  test2018_53.C
  test2018_54.C
  test2018_55.C
  test2018_56.C
  test2018_57.C
  test2018_58.C
  test2018_59.C
  test2018_60.C
  test2018_61.C
  test2018_62.C
  test2018_63.C
  test2018_64.C
  test2018_65.C
  test2018_66.C
  test2018_67.C
  test2018_68.C
  test2018_69.C
  test2018_70.C
  test2018_71.C
  test2018_72.C
  test2018_73.C
  test2018_03.C
  test2018_12.C
)
set(CXX17_DISABLED_TESTCODES
  test2018_02.C
  test2018_05.C
  test2018_06.C
  test2018_07.C
  test2018_08.C
  test2018_09.C
  test2018_10.C
  test2018_11.C
  test2018_13.C
  test2018_14.C
  test2018_20.C
  test2018_28.C
  test2018_30.C
  test2018_31.C
  test2018_32.C
  test2018_33.C
  test2018_34.C
  test2018_35.C
  test2018_36.C
  test2018_37.C
  test2018_38.C
  test2018_43.C
  test2018_46.C
  test2018_48.C
  test2018_49.C
  test2018_50.C
  test2018_51.C
  test2018_52.C
  test2018_53.C
  test2018_54.C
  test2018_55.C
  test2018_56.C
  test2018_57.C
  test2018_58.C
  test2018_59.C
  test2018_60.C
  test2018_61.C
  test2018_62.C
  test2018_63.C
  test2018_64.C
  test2018_65.C
  test2018_66.C
  test2018_67.C
  test2018_68.C
  test2018_69.C
  test2018_70.C
  test2018_71.C
  test2018_72.C
  test2018_73.C
)

set(ROSE_FLAGS -w -rose:verbose 0 -std=c++17)

set(ROSE_DISABLED_TESTCODES ${CXX17_DISABLED_TESTCODES})

foreach(file_to_test ${CXX17_TESTCODES})
  compile_test(${file_to_test} "Cxx17_tests")
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_Cxx17_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=Cxx17_tests_test2018_01_C\;Cxx17_tests_test2018_02_C\;Cxx17_tests_test2018_04_C\;Cxx17_tests_test2018_05_C\;Cxx17_tests_test2018_06_C\;Cxx17_tests_test2018_07_C\;Cxx17_tests_test2018_08_C\;Cxx17_tests_test2018_09_C\;Cxx17_tests_test2018_10_C\;Cxx17_tests_test2018_11_C\;Cxx17_tests_test2018_13_C\;Cxx17_tests_test2018_14_C\;Cxx17_tests_test2018_15_C\;Cxx17_tests_test2018_16_C\;Cxx17_tests_test2018_17_C\;Cxx17_tests_test2018_18_C\;Cxx17_tests_test2018_19_C\;Cxx17_tests_test2018_20_C\;Cxx17_tests_test2018_21_C\;Cxx17_tests_test2018_22_C\;Cxx17_tests_test2018_23_C\;Cxx17_tests_test2018_24_C\;Cxx17_tests_test2018_25_C\;Cxx17_tests_test2018_26_C\;Cxx17_tests_test2018_27_C\;Cxx17_tests_test2018_28_C\;Cxx17_tests_test2018_29_C\;Cxx17_tests_test2018_30_C\;Cxx17_tests_test2018_31_C\;Cxx17_tests_test2018_32_C\;Cxx17_tests_test2018_33_C\;Cxx17_tests_test2018_34_C\;Cxx17_tests_test2018_35_C\;Cxx17_tests_test2018_36_C\;Cxx17_tests_test2018_37_C\;Cxx17_tests_test2018_38_C\;Cxx17_tests_test2018_39_C\;Cxx17_tests_test2018_40_C\;Cxx17_tests_test2018_41_C\;Cxx17_tests_test2018_42_C\;Cxx17_tests_test2018_43_C\;Cxx17_tests_test2018_44_C\;Cxx17_tests_test2018_45_C\;Cxx17_tests_test2018_46_C\;Cxx17_tests_test2018_47_C\;Cxx17_tests_test2018_48_C\;Cxx17_tests_test2018_49_C\;Cxx17_tests_test2018_50_C\;Cxx17_tests_test2018_51_C\;Cxx17_tests_test2018_52_C\;Cxx17_tests_test2018_53_C\;Cxx17_tests_test2018_54_C\;Cxx17_tests_test2018_55_C\;Cxx17_tests_test2018_56_C\;Cxx17_tests_test2018_57_C\;Cxx17_tests_test2018_58_C\;Cxx17_tests_test2018_59_C\;Cxx17_tests_test2018_60_C\;Cxx17_tests_test2018_61_C\;Cxx17_tests_test2018_62_C\;Cxx17_tests_test2018_63_C\;Cxx17_tests_test2018_64_C\;Cxx17_tests_test2018_65_C\;Cxx17_tests_test2018_66_C\;Cxx17_tests_test2018_67_C\;Cxx17_tests_test2018_68_C\;Cxx17_tests_test2018_69_C\;Cxx17_tests_test2018_70_C\;Cxx17_tests_test2018_71_C\;Cxx17_tests_test2018_72_C\;Cxx17_tests_test2018_73_C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_Cxx17_tests_check-local PROPERTIES DEPENDS "Cxx17_tests_test2018_01_C;Cxx17_tests_test2018_02_C;Cxx17_tests_test2018_04_C;Cxx17_tests_test2018_05_C;Cxx17_tests_test2018_06_C;Cxx17_tests_test2018_07_C;Cxx17_tests_test2018_08_C;Cxx17_tests_test2018_09_C;Cxx17_tests_test2018_10_C;Cxx17_tests_test2018_11_C;Cxx17_tests_test2018_13_C;Cxx17_tests_test2018_14_C;Cxx17_tests_test2018_15_C;Cxx17_tests_test2018_16_C;Cxx17_tests_test2018_17_C;Cxx17_tests_test2018_18_C;Cxx17_tests_test2018_19_C;Cxx17_tests_test2018_20_C;Cxx17_tests_test2018_21_C;Cxx17_tests_test2018_22_C;Cxx17_tests_test2018_23_C;Cxx17_tests_test2018_24_C;Cxx17_tests_test2018_25_C;Cxx17_tests_test2018_26_C;Cxx17_tests_test2018_27_C;Cxx17_tests_test2018_28_C;Cxx17_tests_test2018_29_C;Cxx17_tests_test2018_30_C;Cxx17_tests_test2018_31_C;Cxx17_tests_test2018_32_C;Cxx17_tests_test2018_33_C;Cxx17_tests_test2018_34_C;Cxx17_tests_test2018_35_C;Cxx17_tests_test2018_36_C;Cxx17_tests_test2018_37_C;Cxx17_tests_test2018_38_C;Cxx17_tests_test2018_39_C;Cxx17_tests_test2018_40_C;Cxx17_tests_test2018_41_C;Cxx17_tests_test2018_42_C;Cxx17_tests_test2018_43_C;Cxx17_tests_test2018_44_C;Cxx17_tests_test2018_45_C;Cxx17_tests_test2018_46_C;Cxx17_tests_test2018_47_C;Cxx17_tests_test2018_48_C;Cxx17_tests_test2018_49_C;Cxx17_tests_test2018_50_C;Cxx17_tests_test2018_51_C;Cxx17_tests_test2018_52_C;Cxx17_tests_test2018_53_C;Cxx17_tests_test2018_54_C;Cxx17_tests_test2018_55_C;Cxx17_tests_test2018_56_C;Cxx17_tests_test2018_57_C;Cxx17_tests_test2018_58_C;Cxx17_tests_test2018_59_C;Cxx17_tests_test2018_60_C;Cxx17_tests_test2018_61_C;Cxx17_tests_test2018_62_C;Cxx17_tests_test2018_63_C;Cxx17_tests_test2018_64_C;Cxx17_tests_test2018_65_C;Cxx17_tests_test2018_66_C;Cxx17_tests_test2018_67_C;Cxx17_tests_test2018_68_C;Cxx17_tests_test2018_69_C;Cxx17_tests_test2018_70_C;Cxx17_tests_test2018_71_C;Cxx17_tests_test2018_72_C;Cxx17_tests_test2018_73_C" )
