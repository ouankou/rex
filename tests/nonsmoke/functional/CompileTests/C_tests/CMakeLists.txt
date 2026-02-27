# this defines ROSE__C_TESTS
# include(${CMAKE_CURRENT_SOURCE_DIR}/C_Testcodes_bk.cmake)

set(C_TEST_LANG_FLAGS)
set(C_TEST_ROSE_FLAGS -std=c -w -rose:verbose 0)
set(ROSE_FLAGS ${C_TEST_ROSE_FLAGS})
set(C_TEST_TRANSLATOR $<TARGET_FILE:testTranslator>)
string(JOIN " " _c_tests_lang_flags_str ${C_TEST_LANG_FLAGS})
string(JOIN " " _c_tests_rose_flags_str ${C_TEST_ROSE_FLAGS})
set(_c_tests_special_dir ${CMAKE_CURRENT_BINARY_DIR}/special)
file(MAKE_DIRECTORY ${_c_tests_special_dir})

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; C_tests special tests require bash")
endif()

set(TESTCODES_REQUIRED_TO_PASS
  YardenPragmaPackExample.c
  Yarden_client.c
  commentTest.c
  complex_01.c
  constants.c
  gconv_info.c
  math.c
  rex_test2025_embedded_record_decl.c
  rose-1391-a.c
  rose-1391-b.c
  stdio.c
  test2003_12.c
  test2004_20.c
  test2004_21.c
  test2005_172.c
  test2005_186.c
  test2006_100.c
  test2006_110.c
  test2006_114.c
  test2006_132.c
  test2006_133.c
  test2006_135.c
  test2006_136.c
  test2006_14.c
  test2006_48.c
  test2006_65.c
  test2007_177.c
  test2008_01.c
  test2008_02.c
  test2009_01.c
  test2009_02.c
  test2009_03.c
  test2009_04.c
  test2009_07.c
  test2009_08.c
  test2009_16.c
  test2009_19.c
  test2009_21.c
  test2009_22.c
  test2010_01.c
  test2010_02.c
  test2010_03.c
  test2010_04.c
  test2010_06.c
  test2010_07.c
  test2010_08.c
  test2010_09.c
  test2010_10.c
  test2010_11.c
  test2010_12.c
  test2010_13.c
  test2010_14.c
  test2010_15.c
  test2010_17.c
  test2011_03.c
  test2011_04.c
  test2011_05.c
  test2012_01.c
  test2012_02.c
  test2012_03.c
  test2012_04.c
  test2012_05.c
  test2012_06.c
  test2012_07.c
  test2012_08.c
  test2012_09.c
  test2012_10.c
  test2012_100.c
  test2012_101.c
  test2012_102.c
  test2012_103.c
  test2012_104.c
  test2012_105.c
  test2012_106.c
  test2012_107.c
  test2012_108.c
  test2012_109.c
  test2012_11.c
  test2012_110.c
  test2012_111.c
  test2012_112.c
  test2012_113.c
  test2012_114.c
  test2012_115.c
  test2012_116.c
  test2012_117.c
  test2012_118.c
  test2012_119.c
  test2012_12.c
  test2012_120.c
  test2012_121.c
  test2012_122.c
  test2012_123.c
  test2012_124.c
  test2012_125.c
  test2012_126.c
  test2012_127.c
  test2012_129.c
  test2012_13.c
  test2012_130.c
  test2012_131.c
  test2012_133.c
  test2012_134.c
  test2012_135.c
  test2012_136.c
  test2012_137.c
  test2012_139.c
  test2012_14.c
  test2012_140.c
  test2012_141.c
  test2012_142.c
  test2012_143.c
  test2012_144.c
  test2012_145.c
  test2012_146.c
  test2012_147.c
  test2012_148.c
  test2012_149.c
  test2012_15.c
  test2012_150.c
  test2012_151.c
  test2012_152.c
  test2012_153.c
  test2012_154.c
  test2012_156.c
  test2012_157.c
  test2012_158.c
  test2012_159.c
  test2012_16.c
  test2012_162.c
  test2012_163.c
  test2012_164.c
  test2012_165.c
  test2012_166.c
  test2012_167.c
  test2012_168.c
  test2012_169.c
  test2012_17.c
  test2012_170.c
  test2012_171.c
  test2012_172.c
  test2012_173.c
  test2012_173_redhat_rel6.c
  test2012_174.c
  test2012_175.c
  test2012_18.c
  test2012_19.c
  test2012_20.c
  test2012_21.c
  test2012_22.c
  test2012_23.c
  test2012_24.c
  test2012_25.c
  test2012_26.c
  test2012_27.c
  test2012_28.c
  test2012_29.c
  test2012_30.c
  test2012_32.c
  test2012_33.c
  test2012_34.c
  test2012_35.c
  test2012_36.c
  test2012_37.c
  test2012_38.c
  test2012_39.c
  test2012_40.c
  test2012_41.c
  test2012_42.c
  test2012_43.c
  test2012_46.c
  test2012_47.c
  test2012_48.c
  test2012_49.c
  test2012_50.c
  test2012_51.c
  test2012_52.c
  test2012_53.c
  test2012_54.c
  test2012_55.c
  test2012_56.c
  test2012_57.c
  test2012_58.c
  test2012_59.c
  test2012_60.c
  test2012_61.c
  test2012_62.c
  test2012_63.c
  test2012_64.c
  test2012_65.c
  test2012_66.c
  test2012_67.c
  test2012_68.c
  test2012_69.c
  test2012_70.c
  test2012_71.c
  test2012_72.c
  test2012_73.c
  test2012_74.c
  test2012_75.c
  test2012_76.c
  test2012_77.c
  test2012_79.c
  test2012_80.c
  test2012_81.c
  test2012_82.c
  test2012_83.c
  test2012_84.c
  test2012_85.c
  test2012_86.c
  test2012_87.c
  test2012_88.c
  test2012_89.c
  test2012_90.c
  test2012_91.c
  test2012_92.c
  test2012_93.c
  test2012_94.c
  test2012_95.c
  test2012_96.c
  test2012_97.c
  test2012_98.c
  test2012_99.c
  test2013_01.c
  test2013_02.c
  test2013_03.c
  test2013_05.c
  test2013_06.c
  test2013_07.c
  test2013_08.c
  test2013_09.c
  test2013_10.c
  test2013_100.c
  test2013_101.c
  test2013_102.c
  test2013_103.c
  test2013_104.c
  test2013_105.c
  test2013_106.c
  test2013_107.c
  test2013_108.c
  test2013_109.c
  test2013_11.c
  test2013_110.c
  test2013_111.c
  test2013_112.c
  test2013_113.c
  test2013_12.c
  test2013_13.c
  test2013_14.c
  test2013_15.c
  test2013_16.c
  test2013_17.c
  test2013_18.c
  test2013_19.c
  test2013_20.c
  test2013_21.c
  test2013_22.c
  test2013_23.c
  test2013_24.c
  test2013_25.c
  test2013_26.c
  test2013_27.c
  test2013_28.c
  test2013_29.c
  test2013_30.c
  test2013_31.c
  test2013_32.c
  test2013_33.c
  test2013_34.c
  test2013_35.c
  test2013_36.c
  test2013_37.c
  test2013_38.c
  test2013_39.c
  test2013_40.c
  test2013_41.c
  test2013_42.c
  test2013_43.c
  test2013_44.c
  test2013_45.c
  test2013_46.c
  test2013_47.c
  test2013_48.c
  test2013_49.c
  test2013_50.c
  test2013_51.c
  test2013_52.c
  test2013_53.c
  test2013_54.c
  test2013_55.c
  test2013_56.c
  test2013_57.c
  test2013_58.c
  test2013_59.c
  test2013_60.c
  test2013_61.c
  test2013_62.c
  test2013_63.c
  test2013_64.c
  test2013_65.c
  test2013_66.c
  test2013_67.c
  test2013_68.c
  test2013_69.c
  test2013_70.c
  test2013_71.c
  test2013_72.c
  test2013_73.c
  test2013_74.c
  test2013_75.c
  test2013_76.c
  test2013_77.c
  test2013_78.c
  test2013_79.c
  test2013_80.c
  test2013_81.c
  test2013_82.c
  test2013_83.c
  test2013_84.c
  test2013_85.c
  test2013_86.c
  test2013_87.c
  test2013_88.c
  test2013_89.c
  test2013_90.c
  test2013_91.c
  test2013_92.c
  test2013_93.c
  test2013_94.c
  test2013_95.c
  test2013_96.c
  test2013_97.c
  test2013_98.c
  test2013_99.c
  test2014_01.c
  test2014_02.c
  test2014_04.c
  test2014_05.c
  test2014_06.c
  test2014_07.c
  test2014_08.c
  test2014_09.c
  test2014_100.c
  test2014_12.c
  test2014_13.c
  test2014_14.c
  test2014_15.c
  test2014_16.c
  test2014_17.c
  test2014_18.c
  test2014_19.c
  test2014_20.c
  test2014_200.c
  test2014_201.c
  test2014_202.c
  test2014_21.c
  test2014_22.c
  test2014_23.c
  test2014_24.c
  test2014_25.c
  test2014_26.c
  test2014_27.c
  test2014_28.c
  test2014_29.c
  test2014_30.c
  test2014_31.c
  test2014_32.c
  test2014_33.c
  test2014_34.c
  test2014_35.c
  test2014_36.c
  test2014_37.c
  test2014_38.c
  test2014_39.c
  test2014_40.c
  test2014_41.c
  test2014_42.c
  test2014_43.c
  test2014_44.c
  test2014_46.c
  test2014_47.c
  test2014_48.c
  test2014_49.c
  test2014_50.c
  test2014_51.c
  test2014_52.c
  test2014_53.c
  test2014_55.c
  test2014_56.c
  test2014_57.c
  test2014_58.c
  test2014_60.c
  test2014_62.c
  test2014_63.c
  test2014_64.c
  test2014_65.c
  test2014_66.c
  test2014_67.c
  test2014_68.c
  test2014_69.c
  test2014_70.c
  test2014_72.c
  test2014_76.c
  test2014_77.c
  test2014_78.c
  test2014_79.c
  test2014_80.c
  test2014_81.c
  test2014_82.c
  test2014_83.c
  test2014_84.c
  test2014_85.c
  test2014_86.c
  test2014_87.c
  test2014_88.c
  test2014_89.c
  test2014_91.c
  test2014_92.c
  test2014_93.c
  test2014_94.c
  test2014_95.c
  test2015_01.c
  test2015_02.c
  test2015_03.c
  test2015_04.c
  test2015_05.c
  test2015_06.c
  test2015_07.c
  test2015_08.c
  test2015_09.c
  test2015_10.c
  test2015_104.c
  test2015_105.c
  test2015_107.c
  test2015_108.c
  test2015_109.c
  test2015_11.c
  test2015_110.c
  test2015_113.c
  test2015_114.c
  test2015_115.c
  test2015_116.c
  test2015_117.c
  test2015_118.c
  test2015_119.c
  test2015_12.c
  test2015_120.c
  test2015_121.c
  test2015_122.c
  test2015_123.c
  test2015_124.c
  test2015_125.c
  test2015_126.c
  test2015_127.c
  test2015_128.c
  test2015_129.c
  test2015_13.c
  test2015_130.c
  test2015_131.c
  test2015_132.c
  test2015_136.c
  test2015_137.c
  test2015_138.c
  test2015_139.c
  test2015_14.c
  test2015_140.c
  test2015_141.c
  test2015_142.c
  test2015_143.c
  test2015_144.c
  test2015_145.c
  test2015_146.c
  test2015_147.c
  test2015_148.c
  test2015_149.c
  test2015_15.c
  test2015_156.c
  test2015_157.c
  test2015_158.c
  test2015_16.c
  test2015_160.c
  test2015_161.c
  test2015_163.c
  test2015_164.c
  test2015_165.c
  test2015_166.c
  test2015_167.c
  test2015_168.c
  test2015_169.c
  test2015_17.c
  test2015_170.c
  test2015_171.c
  test2015_18.c
  test2015_19.c
  test2015_20.c
  test2015_21.c
  test2015_22.c
  test2015_23.c
  test2015_24.c
  test2015_25.c
  test2015_26.c
  test2015_27.c
  test2015_28.c
  test2015_29.c
  test2015_30.c
  test2015_31.c
  test2015_32.c
  test2015_33.c
  test2015_34.c
  test2015_35.c
  test2015_36.c
  test2015_37.c
  test2015_38.c
  test2015_39.c
  test2015_40.c
  test2015_41.c
  test2015_42.c
  test2015_43.c
  test2015_44.c
  test2015_45.c
  test2015_47.c
  test2015_48.c
  test2015_49.c
  test2015_50.c
  test2015_51.c
  test2015_52.c
  test2015_53.c
  test2015_54.c
  test2015_55.c
  test2015_57.c
  test2015_58.c
  test2015_59.c
  test2015_60.c
  test2015_61.c
  test2015_62.c
  test2015_63.c
  test2015_64.c
  test2015_65.c
  test2015_66.c
  test2015_67.c
  test2015_68.c
  test2015_69.c
  test2015_70.c
  test2015_71.c
  test2015_72.c
  test2015_73.c
  test2015_74.c
  test2015_75.c
  test2015_76.c
  test2015_77.c
  test2015_78.c
  test2015_79.c
  test2015_80.c
  test2015_81.c
  test2015_82.c
  test2015_83.c
  test2015_84.c
  test2015_85.c
  test2015_89.c
  test2015_90.c
  test2015_91.c
  test2015_92.c
  test2016_01.c
  test2016_02.c
  test2016_03.c
  test2016_04.c
  test2016_05.c
  test2016_06.c
  test2016_07.c
  test2016_08.c
  test2016_09.c
  test2016_10.c
  test2016_11.c
  test2016_12.c
  test2016_13.c
  test2016_14.c
  test2016_15.c
  test2017_01.c
  test2017_04.c
  test2017_06.c
  test2017_07.c
  test2017_08.c
  test2017_09.c
  test2017_10.c
  test2017_11.c
  test2017_12.c
  test2017_13.c
  test2017_14.c
  test2017_15.c
  test2017_17.c
  test2017_18.c
  test2017_19.c
  test2017_20.c
  test2017_21.c
  test2017_22.c
  test2017_23.c
  test2017_24.c
  test2017_25.c
  test2018_01.c
  test2018_02.c
  test2018_03.c
  test2018_04.c
  test2018_05.c
  test2018_06.c
  test2018_07.c
  test2018_08.c
  test2018_09.c
  test2018_10.c
  test2018_11.c
  test2018_12.c
  test2018_13.c
  test2018_14.c
  test2018_15.c
  test2018_16.c
  test2018_17.c
  test2018_18.c
  test2018_19.c
  test2018_20.c
  test2018_21.c
  test2018_22.c
  test2018_23.c
  test2018_24.c
  test2018_25.c
  test2018_28.c
  test2018_29.c
  test2018_30.c
  test2018_31.c
  test2018_32.c
  test2018_33.c
  test2018_35.c
  test2018_36.c
  test2018_37.c
  test2018_41.c
  test2018_42.c
  test2018_43.c
  test2018_44.c
  test2018_62.c
  test2018_63.c
  test2018_64.c
  test2018_67.c
  test2019_01.c
  test2019_02.c
  test2019_03.c
  test2019_04.c
  test2019_05.c
  test2019_06.c
  test2019_07.c
  test2019_08.c
  test2019_09.c
  test2019_10.c
  test2019_11.c
  test2019_12.c
  test2019_13.c
  test2019_14.c
  test2019_15.c
  test2020_03.c
  test2020_12.c
  test2020_13.c
  test2020_14.c
  test2020_15.c
  test2020_16.c
  test2020_17.c
  test2020_18.c
  test2020_19.c
  test2020_21.c
  test2020_22.c
  test2020_24.c
  test2020_28.c
  test2020_29.c
  test2020_30.c
  test2020_31.c
  test2020_32.c
  test2020_33.c
  test2020_34.c
  test2020_35.c
  test2020_36.c
  test2020_37.c
  test2020_38.c
  test2020_39.c
  test2020_40.c
  test2020_41.c
  test2020_42.c
  test2020_43.c
  test2020_44.c
  test2020_45.c
  test2020_46.c
  test2020_47.c
  test2020_48.c
  test2020_49.c
  test2021_01.c
  test2021_02.c
  test2021_03.c
  test2021_04.c
  test2021_05.c
  test2021_06.c
  test2021_07.c
  test2021_08.c
  test2021_09.c
  test2021_10.c
  test2021_11.c
  test2021_12.c
  test2021_13.c
  test2021_14.c
  test2021_15.c
  test2021_16.c
  test2021_17.c
  test2021_18.c
  test2021_20.c
  test2021_21.c
  test2021_22.c
  test2021_23.c
  test2021_24.c
  test2021_25.c
  test2021_27.c
  test2021_28.c
  test2022_01.c
  test2022_02.c
  test2022_03.c
  test2022_04.c
  rex_compound_literal_wrapped.c
  testAnsiC.c
  testCvsCpp.c
  test_CplusplusMacro_C.c
  test_multiline_string_01.c
  test2009_05.c
  test2009_17.c
  test2004_126.c
  test2006_134.c
  test2009_06.c
  test2010_05.c
  test2010_16.c
  test2017_02.c
  test2017_03.c
  test2017_05.c
  test2017_16.c
  test2018_26.c
  test2018_27.c
  test2018_34.c
  test2018_38.c
  test2018_39.c
  test2018_40.c
  test2020_20.c
  test2020_23.c
  test2020_25.c
  test2020_26.c
  test2020_27.c
  test2015_56.c
  test2015_86.c
  test2015_87.c
  test2015_88.c
  test2015_93.c
  test2015_94.c
  test2015_95.c
  test2015_96.c
  test2015_97.c
  test2015_98.c
  test2015_99.c
  test2015_100.c
  test2015_101.c
  test2015_102.c
  test2015_103.c
  test2015_106.c
  test2015_111.c
  test2015_150.c
  test2015_151.c
  test2015_152.c
  test2015_153.c
  test2015_154.c
  test2015_155.c
  test2015_159.c
  test2014_03.c
  test2014_11.c
  test2014_45.c
  test2014_54.c
  test2014_59.c
  test2014_61.c
  test2014_71.c
  test2014_73.c
  test2014_74.c
  test2014_75.c
  test2014_90.c
  test2014_96.c
  test2014_97.c
  test2014_98.c
  test2014_99.c
  test2015_46.c
  test2015_112.c
  test2005_168.c
)
set(TESTCODES_REQUIRED_TO_PASS_DISABLED
  test2004_126.c
  test2006_134.c
  test2009_06.c
  test2010_05.c
  test2010_16.c
  test2017_02.c
  test2017_03.c
  test2017_05.c
  test2017_16.c
  test2018_26.c
  test2018_27.c
  test2018_34.c
  test2018_38.c
  test2018_39.c
  test2018_40.c
  test2020_20.c
  test2020_23.c
  test2020_25.c
  test2020_26.c
  test2020_27.c
  test2015_56.c
  test2015_86.c
  test2015_87.c
  test2015_88.c
  test2015_93.c
  test2015_94.c
  test2015_95.c
  test2015_96.c
  test2015_97.c
  test2015_98.c
  test2015_99.c
  test2015_100.c
  test2015_101.c
  test2015_102.c
  test2015_103.c
  test2015_106.c
  test2015_111.c
  test2015_150.c
  test2015_151.c
  test2015_152.c
  test2015_153.c
  test2015_154.c
  test2015_155.c
  test2015_159.c
  test2014_03.c
  test2014_11.c
  test2014_45.c
  test2014_54.c
  test2014_59.c
  test2014_61.c
  test2014_71.c
  test2014_73.c
  test2014_74.c
  test2014_75.c
  test2014_90.c
  test2014_96.c
  test2014_97.c
  test2014_98.c
  test2014_99.c
  test2015_46.c
  test2015_112.c
  test2005_168.c
)
set(TESTCODE_CURRENTLY_FAILING
  Yarden_client.c
  test2004_126.c
  test2005_168.c
  test2006_100.c
  test2006_132.c
  test2006_134.c
  test2006_136.c
  test2006_14.c
  test2009_06.c
  test2009_19.c
  test2010_02.c
  test2010_04.c
  test2010_05.c
  test2010_12.c
  test2010_16.c
  test2011_03.c
  test2011_04.c
  test2012_09.c
  test2012_10.c
  test2012_101.c
  test2012_106.c
  test2012_109.c
  test2012_110.c
  test2012_111.c
  test2012_112.c
  test2012_113.c
  test2012_114.c
  test2012_12.c
  test2012_129.c
  test2012_14.c
  test2012_151.c
  test2012_152.c
  test2012_162.c
  test2012_173.c
  test2012_173_redhat_rel6.c
  test2012_25.c
  test2012_48.c
  test2012_51.c
  test2012_52.c
  test2012_54.c
  test2012_56.c
  test2012_64.c
  test2012_72.c
  test2012_73.c
  test2012_74.c
  test2013_104.c
  test2013_113.c
  test2013_21.c
  test2013_32.c
  test2014_12.c
  test2014_13.c
  test2014_83.c
  test2014_93.c
  test2015_104.c
  test2015_105.c
  test2015_107.c
  test2015_108.c
  test2015_110.c
  test2015_113.c
  test2015_115.c
  test2015_116.c
  test2015_139.c
  test2015_14.c
  test2015_140.c
  test2015_143.c
  test2015_144.c
  test2015_146.c
  test2015_147.c
  test2015_148.c
  test2015_149.c
  test2015_15.c
  test2015_16.c
  test2015_17.c
  test2015_18.c
  test2015_20.c
  test2015_21.c
  test2015_34.c
  test2015_38.c
  test2015_39.c
  test2015_40.c
  test2015_41.c
  test2015_42.c
  test2015_43.c
  test2015_44.c
  test2015_45.c
  test2015_47.c
  test2015_48.c
  test2015_49.c
  test2015_50.c
  test2015_51.c
  test2015_54.c
  test2015_55.c
  test2015_57.c
  test2015_58.c
  test2015_77.c
  test2015_78.c
  test2015_79.c
  test2015_80.c
  test2015_81.c
  test2015_82.c
  test2015_83.c
  test2017_02.c
  test2017_03.c
  test2017_05.c
  test2017_16.c
  test2018_26.c
  test2018_27.c
  test2018_34.c
  test2018_38.c
  test2018_39.c
  test2018_40.c
  test2020_20.c
  test2020_23.c
  test2020_25.c
  test2020_26.c
  test2020_27.c
  constants.c
  test2012_100.c
  test2012_116.c
  test2012_133.c
  test2012_141.c
  test2012_58.c
  test2012_82.c
  test2013_07.c
  test2013_08.c
  test2013_46.c
  test2013_47.c
  test2013_76.c
  test2014_29.c
  test2014_41.c
  test2014_66.c
  test2014_91.c
  test2015_156.c
  test2015_157.c
  test2015_23.c
  test2015_29.c
  test2015_30.c
  test2017_17.c
  test2018_08.c
  test2018_09.c
  test2018_10.c
  test2018_24.c
  test2018_31.c
  test2018_33.c
  test2020_14.c
  test2020_15.c
  test2020_16.c
  test2021_01.c
  test2022_02.c
  test2022_03.c
  test2022_04.c
)

list(REMOVE_ITEM TESTCODES_REQUIRED_TO_PASS test2005_168.c test2012_145.c)
list(REMOVE_ITEM TESTCODES_REQUIRED_TO_PASS_DISABLED test2005_168.c test2012_145.c)
list(REMOVE_ITEM TESTCODE_CURRENTLY_FAILING test2005_168.c test2012_145.c)

set(TESTCODES_REQUIRED_TO_PASS "${TESTCODES_REQUIRED_TO_PASS}" CACHE INTERNAL "")
set(TESTCODES_REQUIRED_TO_PASS_DISABLED "${TESTCODES_REQUIRED_TO_PASS_DISABLED}" CACHE INTERNAL "")
set(TESTCODE_CURRENTLY_FAILING "${TESTCODE_CURRENTLY_FAILING}" CACHE INTERNAL "")
set(C_TESTCODES_TO_RUN ${TESTCODES_REQUIRED_TO_PASS})
list(REMOVE_ITEM C_TESTCODES_TO_RUN ${TESTCODE_CURRENTLY_FAILING})

set(ROSE_DISABLED_TESTCODES ${TESTCODES_REQUIRED_TO_PASS_DISABLED})
set(ROSE_FAILING_TESTCODES ${TESTCODE_CURRENTLY_FAILING})

foreach(file_to_test ${TESTCODE_CURRENTLY_FAILING})
  compile_test(${file_to_test} "C_tests_failing")
endforeach()

foreach(file_to_test ${C_TESTCODES_TO_RUN})
  compile_test(${file_to_test} "C_tests")
endforeach()

set(_c_tests_disable_m32 FALSE)
if(CMAKE_C_COMPILER_ID MATCHES "Intel")
  set(_c_tests_disable_m32 TRUE)
endif()

set(_c_tests_string_macro -DSTRING_MACRO=\\\"some_text\\\")

add_test(
  NAME C_tests_check_pthread
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -pthread -c
          ${CMAKE_CURRENT_SOURCE_DIR}/needs_reentrant.c
)
set_tests_properties(C_tests_check_pthread PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_test2005_168
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} ${C_TEST_LANG_FLAGS} ${C_TEST_ROSE_FLAGS}
          -I${CMAKE_CURRENT_SOURCE_DIR}
          -I${CMAKE_SOURCE_DIR}/src/util/commandlineProcessing
          -I${CMAKE_SOURCE_DIR}/src/frontend/SageIII
          -c ${CMAKE_CURRENT_SOURCE_DIR}/test2005_168.c
)
set_tests_properties(C_tests_test2005_168 PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_test2012_145
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} ${C_TEST_LANG_FLAGS} ${C_TEST_ROSE_FLAGS}
          -I${CMAKE_CURRENT_SOURCE_DIR}
          ${_c_tests_string_macro}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/test2012_145.c
)
set_tests_properties(C_tests_test2012_145 PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_build_generated_file_in_same_directory_as_input_file
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "mkdir -p test_directory && \
     cp ${CMAKE_CURRENT_SOURCE_DIR}/test2013_73.c test_directory/test2013_73.c && \
     ${C_TEST_TRANSLATOR} -rose:unparse_in_same_directory_as_input_file ${_c_tests_rose_flags_str} -c test_directory/test2013_73.c -o test_directory/test2013_73.o"
)
set_tests_properties(C_tests_build_generated_file_in_same_directory_as_input_file PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_unparse_includes_option
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -rose:unparse_includes ${C_TEST_ROSE_FLAGS}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/test2013_75.c
          -o test2013_75_unparse_headers.o
)
set_tests_properties(C_tests_unparse_includes_option PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_unparse_includes_option_with_link
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -rose:unparse_includes ${C_TEST_ROSE_FLAGS}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/test2013_76.c
          -o test2013_76_unparse_headers.o
)
set_tests_properties(C_tests_unparse_includes_option_with_link PROPERTIES LABELS C_tests)

add_test(
  NAME CTEST_test2013_76
  COMMAND testTranslator -rose:C89 --edg:no_warnings --edg:restrict -w
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2013_76.c
)
set_tests_properties(CTEST_test2013_76 PROPERTIES LABELS "CTEST;disabled")
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(CTEST_test2013_76 PROPERTIES DISABLED TRUE)
endif()

add_test(
  NAME C_tests_else_case_disambiguation
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "cp ${CMAKE_CURRENT_SOURCE_DIR}/test2008_02.c else_case_disambiguation_test.c && \
     env ROSE_TEST_ELSE_DISAMBIGUATION=x ${C_TEST_TRANSLATOR} ${_c_tests_lang_flags_str} ${_c_tests_rose_flags_str} -c else_case_disambiguation_test.c"
)
set_tests_properties(C_tests_else_case_disambiguation PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_common_configure_test_with_link
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -I${CMAKE_CURRENT_SOURCE_DIR}
          ${C_TEST_ROSE_FLAGS}
          ${CMAKE_CURRENT_SOURCE_DIR}/conftest.c
          -o conftest_configure_test
)
set_tests_properties(C_tests_common_configure_test_with_link PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_link_using_asm_symbol_injection
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -o test2014_83
          ${CMAKE_CURRENT_SOURCE_DIR}/test2014_83.c
)
set_tests_properties(C_tests_link_using_asm_symbol_injection PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_link_using_alias_attribute
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -o test2014_93
          ${CMAKE_CURRENT_SOURCE_DIR}/test2014_93.c
)
set_tests_properties(C_tests_link_using_alias_attribute PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_using_explicit_output_file_option
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -rose:verbose 0 -MMD -MF .subdirs-install.d
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_86.c -o test2015_86
)
set_tests_properties(C_tests_using_explicit_output_file_option PROPERTIES LABELS C_tests RESOURCE_LOCK c_tests_output_file_option)

add_test(
  NAME C_tests_using_implicit_output_file_option
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -rose:verbose 0 -MMD -MF .subdirs-install.d
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_86.c
)
set_tests_properties(C_tests_using_implicit_output_file_option PROPERTIES LABELS C_tests RESOURCE_LOCK c_tests_output_file_option)

add_test(
  NAME C_tests_rtd_use
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -m32 -mrtd -rose:C89 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_89.c
)
set_tests_properties(C_tests_rtd_use PROPERTIES DISABLED ${_c_tests_disable_m32} LABELS C_tests)

add_test(
  NAME C_tests_optimization_specifier_O
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -O -c ${CMAKE_CURRENT_SOURCE_DIR}/test2015_153.c
)
set_tests_properties(C_tests_optimization_specifier_O PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_optimization_specifier_Os
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -Os -c ${CMAKE_CURRENT_SOURCE_DIR}/test2015_153.c
)
set_tests_properties(C_tests_optimization_specifier_Os PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_optimization_specifier_O1
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -O1 -c ${CMAKE_CURRENT_SOURCE_DIR}/test2015_153.c
)
set_tests_properties(C_tests_optimization_specifier_O1 PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_optimization_specifier_O2
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -O2 -c ${CMAKE_CURRENT_SOURCE_DIR}/test2015_153.c
)
set_tests_properties(C_tests_optimization_specifier_O2 PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_mregparm_m32_use
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -mregparm=3 -m32 -rose:C89 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_154.c
)
set_tests_properties(C_tests_mregparm_m32_use PROPERTIES DISABLED ${_c_tests_disable_m32} LABELS C_tests)

add_test(
  NAME C_tests_compile_and_link_with_NDEBUG
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -o test2015_162 -DNDEBUG
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_162.c
)
set_tests_properties(C_tests_compile_and_link_with_NDEBUG PROPERTIES LABELS C_tests RESOURCE_LOCK c_tests_test2015_162)

add_test(
  NAME C_tests_compile_and_link_without_NDEBUG
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -o test2015_162
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_162.c
)
set_tests_properties(C_tests_compile_and_link_without_NDEBUG PROPERTIES LABELS C_tests RESOURCE_LOCK c_tests_test2015_162)

add_test(
  NAME C_tests_multiple_file_test_01
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "${C_TEST_TRANSLATOR} ${_c_tests_lang_flags_str} ${_c_tests_rose_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/test2019_16a.c ${CMAKE_CURRENT_SOURCE_DIR}/test2019_16b.c && \
     ${C_TEST_TRANSLATOR} ${_c_tests_lang_flags_str} ${_c_tests_rose_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/test2019_16b.c ${CMAKE_CURRENT_SOURCE_DIR}/test2019_16a.c"
)
set_tests_properties(C_tests_multiple_file_test_01 PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_caller_out
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "${C_TEST_TRANSLATOR} ${_c_tests_lang_flags_str} ${_c_tests_rose_flags_str} ${CMAKE_CURRENT_SOURCE_DIR}/callee.c ${CMAKE_CURRENT_SOURCE_DIR}/caller.c -o caller.out && \
     ./caller.out"
)
set_tests_properties(C_tests_caller_out PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_token_mapping
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -rose:C89 -rose:unparse_tokens -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2003_12.c
)
set_tests_properties(C_tests_token_mapping PROPERTIES DISABLED TRUE LABELS C_tests)

add_test(
  NAME C_tests_gnu_S_option
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -S -o test2014_41.s
          ${CMAKE_CURRENT_SOURCE_DIR}/test2014_41.c
)
set_tests_properties(C_tests_gnu_S_option PROPERTIES DISABLED TRUE LABELS C_tests)

add_test(
  NAME C_tests_m32_use
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -m32 -rose:C89 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_95.c
)
set_tests_properties(C_tests_m32_use PROPERTIES DISABLED TRUE LABELS C_tests)

add_test(
  NAME C_tests_file_suffix
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_133.S
)
set_tests_properties(C_tests_file_suffix PROPERTIES DISABLED TRUE LABELS C_tests)

# Makefile check targets.
set(_c_tests_check_local_files ${C_TESTCODES_TO_RUN})
list(REMOVE_ITEM _c_tests_check_local_files test2009_05.c test2009_17.c)

set(_c_tests_check_local_tests "")
foreach(file_to_test ${_c_tests_check_local_files})
  set(_test_key "${file_to_test}")
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  list(APPEND _c_tests_check_local_tests "C_tests_${_test_key}")
endforeach()

set(_c_tests_check_tests "")
foreach(file_to_test IN ITEMS test2009_05.c test2009_17.c)
  set(_test_key "${file_to_test}")
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  list(APPEND _c_tests_check_tests "C_tests_${_test_key}")
endforeach()
list(APPEND _c_tests_check_tests C_tests_caller_out)

list(APPEND _c_tests_check_local_tests
  C_tests_check_pthread
  C_tests_link_using_asm_symbol_injection
  C_tests_link_using_alias_attribute
  C_tests_build_generated_file_in_same_directory_as_input_file
  C_tests_unparse_includes_option
  C_tests_unparse_includes_option_with_link
  C_tests_else_case_disambiguation
  C_tests_common_configure_test_with_link
  C_tests_using_explicit_output_file_option
  C_tests_using_implicit_output_file_option
  C_tests_rtd_use
  C_tests_optimization_specifier_O
  C_tests_optimization_specifier_Os
  C_tests_optimization_specifier_O1
  C_tests_optimization_specifier_O2
  C_tests_mregparm_m32_use
  C_tests_compile_and_link_with_NDEBUG
  C_tests_compile_and_link_without_NDEBUG
  C_tests_multiple_file_test_01
)
list(REMOVE_DUPLICATES _c_tests_check_local_tests)
list(REMOVE_DUPLICATES _c_tests_check_tests)

add_test(
  NAME nonsmoke_functional_CompileTests_C_tests_check-TESTS
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_c_tests_check_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_C_tests_check-TESTS PROPERTIES LABELS disabled DEPENDS "${_c_tests_check_tests}" )
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_CompileTests_C_tests_check-TESTS PROPERTIES DISABLED TRUE)
endif()

add_test(
  NAME nonsmoke_functional_CompileTests_C_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_c_tests_check_local_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_C_tests_check-local PROPERTIES DEPENDS "${_c_tests_check_local_tests}" )

add_test(
  NAME nonsmoke_functional_CompileTests_C_tests_check-pthread.passed
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=C_tests_check_pthread
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_C_tests_check-pthread.passed PROPERTIES DEPENDS C_tests_check_pthread )
