# this defines ROSE__C_TESTS
# include(${CMAKE_CURRENT_SOURCE_DIR}/C_Testcodes_bk.cmake)

set(C_TEST_LANG_FLAGS)
set(C_TEST_ROSE_FLAGS -rose:verbose 0)
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

set(C_TESTCODES
  rex_frontend_designated_initializer_chain.c
  rex_frontend_expression_tag_ownership.c
  rex_frontend_if_asm_source_provenance.c
  rex_frontend_inline_tag_ownership.c
  rex_frontend_promoted_anonymous_member_provenance.c
  rex_frontend_source_written_builtin_spelling.c
  rex_frontend_statement_expression_scope.c
  rex_frontend_tag_redeclaration_ownership.c
  rex_unparser_macro_expansion_surface.c
  rex_unparser_enum_include_boundary.c
  rex_unparser_string_literal_encoding.c
  rex_unparser_typeof_variable_qualification.c
  rex_unparser_output_atomicity.c
  rex_unparser_pragma_boundary_roundtrip.c
  rex_unparser_preprocessing_roundtrip.c
  YardenPragmaPackExample.c
  Yarden_client.c
  commentTest.c
  complex_01.c
  constants.c
  gconv_info.c
  math.c
  rex_test2025_embedded_record_decl.c
  rex_test2026_clang_stmt_expr_valgrind_defined_reads.c
  rex_test2026_clang_valgrind_defined_reads.c
  rex_test2026_compound_literal_typeloc_valgrind_defined_reads.c
  rex_test2026_token_stream_explicit_leading_trailing_output.c
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
  test2017_67.c
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
  test2015_88.c
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
list(APPEND C_TESTCODES
  test2004_126.c
  test2006_134.c
  test2009_06.c
  test2010_05.c
  test2010_16.c
  test2017_03.c
  test2017_05.c
  test2017_16.c
  test2018_27.c
  test2018_40.c
  test2020_20.c
  test2020_23.c
  test2020_25.c
  test2015_56.c
  test2015_86.c
  test2015_88.c
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
list(APPEND C_TESTCODES
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
  test2017_03.c
  test2017_05.c
  test2017_16.c
  test2018_27.c
  test2018_40.c
  test2020_20.c
  test2020_23.c
  test2020_25.c
  constants.c
  test2012_100.c
  test2012_116.c
  test2012_133.c
  test2012_141.c
  test2012_58.c
  test2012_82.c
  test2013_08.c
  test2013_76.c
  test2014_29.c
  test2014_91.c
  test2015_156.c
  test2015_157.c
  test2017_17.c
  test2018_24.c
  test2018_31.c
  test2018_33.c
  test2020_14.c
  test2021_01.c
  test2022_02.c
  test2022_03.c
  test2022_04.c
)

# These legacy C tests only compile in a C89-family dialect. Register them from
# the C89 bucket instead of weakening the default C bucket.
set(C_TESTCODES_C89_ONLY
  test2009_04.c
  test2010_01.c
  test2012_27.c
  test2012_116.c
  test2012_44.c
  test2012_45.c
  test2012_173.c
  test2012_173_redhat_rel6.c
  test2012_32.c
  test2012_79.c
  test2012_80.c
  test2012_135.c
  test2013_41.c
  test2013_53.c
  test2013_56.c
  test2013_57.c
  test2013_58.c
  test2013_59.c
  test2013_60.c
  test2013_61.c
  test2013_63.c
  test2013_105.c
  test2013_106.c
  test2013_107.c
  test2013_108.c
  test2013_109.c
  test2013_111.c
  test2014_06.c
  test2014_20.c
  test2014_21.c
  test2014_74.c
  test2015_28.c
  test2017_03.c
  test2017_12.c
  test2017_67.c
  test2018_49.c
  test2018_50.c
  test2018_51.c
  test2018_63.c
  test2020_06.c
)

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _rex_c_tests_processor)
set(REX_C_TESTS_HOST_IS_X86 FALSE)
if(_rex_c_tests_processor MATCHES "^(x86_64|amd64|i[3-6]86)$")
  set(REX_C_TESTS_HOST_IS_X86 TRUE)
endif()

set(C_TESTCODES_X86_ONLY
  test2012_82.c
  test2012_173.c
  test2012_173_redhat_rel6.c
  test2012_174.c
  test2012_175.c
  test2013_72.c
  test2014_36.c
  test2014_39.c
  test2014_40.c
  test2014_41.c
  test2014_42.c
  test2014_43.c
  test2014_44.c
  test2014_83.c
  test2014_84.c
  test2014_85.c
  test2014_86.c
  test2014_87.c
  test2014_91.c
  test2015_88.c
  test2015_89.c
  test2015_141.c
  test2015_142.c
  test2015_154.c
  test2015_155.c
  test2017_03.c
)
if(NOT REX_C_TESTS_HOST_IS_X86)
  rex_register_compile_source_set(REX_C_TESTS_X86_ONLY_SOURCES
    ${C_TESTCODES_X86_ONLY})
  list(REMOVE_ITEM C_TESTCODES ${C_TESTCODES_X86_ONLY})
  list(REMOVE_ITEM C_TESTCODES_C89_ONLY ${C_TESTCODES_X86_ONLY})
endif()

rex_register_compile_source_set(REX_C89_TESTS_SOURCES ${C_TESTCODES_C89_ONLY})
list(REMOVE_ITEM C_TESTCODES ${C_TESTCODES_C89_ONLY})

# These files compiled under the default C mode but were never registered in
# CTest after the hand-written list migration.
list(APPEND C_TESTCODES
  gzip.c
  old_style_function.c
  test2007_70.c
  test2007_95.c
  test2009_18.c
  test2009_20.c
  test2012_128.c
  test2012_132.c
  test2012_155.c
  test2012_160.c
  test2012_161.c
  test2012_45.c
  test2012_78.c
  test2013_04.c
  test2013_114.c
  test2014_10.c
  test2016_16.c
  test2016_17.c
  test2016_18.c
  test2016_19.c
  test2016_20.c
  test2016_22.c
  test2016_23.c
  test2016_24.c
  test2016_25.c
  test2017_01_array.c
  test2018_28_cpp.c
  test2018_45.c
  test2018_46.c
  test2018_47.c
  test2018_48.c
  test2018_54.c
  test2018_55.c
  test2018_60.c
  test2018_61.c
  test2018_66.c
  test2018_68.c
  test2018_69.c
  test2020_01.c
  test2020_02.c
  test2020_04.c
  test2020_05.c
  test2020_07.c
  test2020_08.c
  test2020_09.c
  test2020_10.c
  test2020_11.c
)

# Keep the final generic C bucket strict: if any of the hand-added entries are
# only valid in a C89-family mode, they must remain registered exclusively from
# the C89 test directory.
list(REMOVE_ITEM C_TESTCODES ${C_TESTCODES_C89_ONLY})

# Keep invalid standalone specimens explicit. They are useful coverage as
# negative frontend tests, but they must not silently disappear from CTest.
set(C_TESTCODES_NEGATIVE
  test2011_06.c
  test2011_sse_01.c
  test2012_31.c
  test2012_138.c
  test2013_06.c
  test2013_50.c
  test2013_51.c
  test2014_03.c
  test2014_18.c
  test2014_59.c
  test2014_75.c
  test2014_90.c
  test2015_42.c
  test2015_46.c
  test2015_87.c
  test2015_93.c
  test2016_21.c
  test2017_02.c
  test2018_26.c
  test2018_52.c
  test2018_53.c
  test2018_65.c
  zpagccp.c
)
list(REMOVE_ITEM C_TESTCODES ${C_TESTCODES_NEGATIVE})

list(REMOVE_DUPLICATES C_TESTCODES)

# test2012_145.c requires a command-line string macro and is registered below
# with dedicated flags. test2015_153.c similarly requires an optimization
# flag. Keep both out of the generic compile_test loop.
list(REMOVE_ITEM C_TESTCODES test2012_145.c test2015_153.c)

foreach(file_to_test ${C_TESTCODES})
  compile_test(${file_to_test} "C_tests")
endforeach()

foreach(file_to_test ${C_TESTCODES_NEGATIVE})
  if(file_to_test STREQUAL "test2017_02.c")
    compile_exact_negative_test(
      ${file_to_test} "C_tests"
      "test2017_02.c:90:5: error: cannot redeclare builtin function '__builtin_classify_type'")
  elseif(file_to_test STREQUAL "test2018_26.c")
    compile_exact_negative_test(
      ${file_to_test} "C_tests"
      "test2018_26.c:29:6: error: conflicting types for '__builtin_ia32_maskloadpd256'")
  else()
    compile_negative_test(${file_to_test} "C_tests")
  endif()
endforeach()

set(C_TESTCODES_SPECIAL
  callee.c
  caller.c
  needs_reentrant.c
  rex_frontend_class_include_boundary.c
  rex_frontend_driver_argument_order.c
  rex_frontend_generated_typedef_dependency.c
  rex_frontend_initializer_include_boundary.c
  rex_frontend_preprocessing_anchor_contract.c
  rex_frontend_preprocessing_empty_translation_unit.c
  rex_frontend_preprocessing_include_occurrence.c
  rex_frontend_preprocessing_parameter_boundary.c
  rex_frontend_preprocessing_terminal_directive.c
  test2003_12.c
  test2005_168.c
  test2008_02.c
  test2012_145.c
  test2013_73.c
  test2013_75.c
  test2013_76.c
  test2014_41.c
  test2014_83.c
  test2014_93.c
  test2015_86.c
  test2015_89.c
  test2015_95.c
  test2015_153.c
  test2015_154.c
  test2015_162.c
  test2019_16a.c
  test2019_16b.c
)

# `conftest.c` is exercised from the C89 bucket because the probe uses an
# implicit-int `main()` definition. Register that ownership here as well so the
# C_tests coverage audit does not treat it as silently dead before the C89
# subdirectory is configured.
set_property(GLOBAL APPEND PROPERTY REX_C89_TESTS_SOURCES C_tests/conftest.c)

rex_register_compile_source_set(REX_C_TESTS_SOURCES
  ${C_TESTCODES}
  ${C_TESTCODES_NEGATIVE}
  ${C_TESTCODES_SPECIAL}
)
rex_register_compile_source_set(REX_C_TESTS_NEGATIVE_SOURCES
  ${C_TESTCODES_NEGATIVE}
)

set(ROSE_FLAGS ${C_TEST_ROSE_FLAGS})

add_executable(
  rex_frontend_generated_typedef_dependency_translator
  rex_frontend_generated_typedef_dependency_translator.cpp
)
target_link_libraries(
  rex_frontend_generated_typedef_dependency_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_multi_tu_tag_identity_translator
  rex_frontend_multi_tu_tag_identity_translator.cpp
)
target_link_libraries(
  rex_frontend_multi_tu_tag_identity_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME C_tests_rex_frontend_multi_tu_tag_identity
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND
    $<TARGET_FILE:rex_frontend_multi_tu_tag_identity_translator>
    ${C_TEST_ROSE_FLAGS} -rose:skipfinalCompileStep -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_multi_tu_tag_identity_a.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_multi_tu_tag_identity_b.c
)
set_tests_properties(
  C_tests_rex_frontend_multi_tu_tag_identity
  PROPERTIES
    LABELS "C_tests;rex;CFE"
    RESOURCE_LOCK rex_frontend_multi_tu_tag_identity
)

set(rex_frontend_generated_typedef_dependency_header_dir
    ${CMAKE_CURRENT_SOURCE_DIR}/../../input_codes/system_header_like)
add_test(
  NAME C_tests_rex_frontend_generated_typedef_dependency_structure
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND
    $<TARGET_FILE:rex_frontend_generated_typedef_dependency_translator>
    ${C_TEST_ROSE_FLAGS} -rose:skipfinalCompileStep
    -isystem ${rex_frontend_generated_typedef_dependency_header_dir}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_generated_typedef_dependency.c
)
set_tests_properties(
  C_tests_rex_frontend_generated_typedef_dependency_structure
  PROPERTIES LABELS "rex;CFE;C_tests;UNPARSER")
rex_lock_test_for_input(
  C_tests_rex_frontend_generated_typedef_dependency_structure
  rex_frontend_generated_typedef_dependency.c)

add_executable(
  rex_frontend_inline_tag_ownership_translator
  rex_frontend_inline_tag_ownership_translator.cpp
)
target_link_libraries(
  rex_frontend_inline_tag_ownership_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME C_tests_rex_frontend_inline_tag_ownership_structure
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND
    $<TARGET_FILE:rex_frontend_inline_tag_ownership_translator>
    ${C_TEST_ROSE_FLAGS} -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_inline_tag_ownership.c
)
set_tests_properties(
  C_tests_rex_frontend_inline_tag_ownership_structure
  PROPERTIES LABELS "rex;CFE;C_tests")
rex_lock_test_for_input(
  C_tests_rex_frontend_inline_tag_ownership_structure
  rex_frontend_inline_tag_ownership.c)

add_test(
  NAME C_tests_rex_name_qualification_rejects_parameter_list_tag_owner
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_name_qualification_parameter_owner.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[statement-body-declaration-owner\\]: declaration=0x[[:xdigit:]]+/SgClassDeclaration owner=0x[[:xdigit:]]+/SgFunctionParameterList child=0x[[:xdigit:]]+/SgClassDeclaration occurrences=0 file-info=0x[[:xdigit:]]+ output=1 is not one exact emitted statement body edge$' rex_name_qualification_parameter_owner.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_inline_tag_ownership_translator>
    --corrupt-parameter-tag-owner
    ${C_TEST_ROSE_FLAGS} -rose:skipfinalCompileStep
    -Wno-visibility
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_inline_tag_ownership.c
)
set_tests_properties(
  C_tests_rex_name_qualification_rejects_parameter_list_tag_owner
  PROPERTIES LABELS "rex;CFE;C_tests;UNPARSER;death")
rex_lock_test_for_input(
  C_tests_rex_name_qualification_rejects_parameter_list_tag_owner
  rex_frontend_inline_tag_ownership.c)

add_executable(
  rex_frontend_expression_tag_ownership_translator
  rex_frontend_expression_tag_ownership_translator.cpp
)
target_link_libraries(
  rex_frontend_expression_tag_ownership_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME C_tests_rex_frontend_expression_tag_ownership_structure
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND
    $<TARGET_FILE:rex_frontend_expression_tag_ownership_translator>
    ${C_TEST_ROSE_FLAGS} -std=c11
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_expression_tag_ownership.c
)
set_tests_properties(
  C_tests_rex_frontend_expression_tag_ownership_structure
  PROPERTIES LABELS "rex;CFE;C_tests")
rex_lock_test_for_input(
  C_tests_rex_frontend_expression_tag_ownership_structure
  rex_frontend_expression_tag_ownership.c)

add_test(
  NAME C_tests_rex_frontend_expression_tag_scope_hard_contract
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND
    bash -c
    "ulimit -c 0; set +e; \"$@\" >rex_expression_tag_scope.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[declaration-owner\\]: declaration=0x[[:xdigit:]]+/SgClassDeclaration owner-kind=SgCastExp owner=0x[[:xdigit:]]+/SgCastExp typed=0x[[:xdigit:]]+ type=0x[[:xdigit:]]+ type-declaration=0x[[:xdigit:]]+ semantic-scope=0x[[:xdigit:]]+ lexical-scope=0x[[:xdigit:]]+ statement=0x[[:xdigit:]]+ has malformed expression-owned type-definition provenance$' rex_expression_tag_scope.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_expression_tag_ownership_translator>
    --corrupt-expression-tag-scope
    ${C_TEST_ROSE_FLAGS} -std=c11
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_expression_tag_ownership.c
)
set_tests_properties(
  C_tests_rex_frontend_expression_tag_scope_hard_contract
  PROPERTIES LABELS "rex;CFE;C_tests;UNPARSER;death")
rex_lock_test_for_input(
  C_tests_rex_frontend_expression_tag_scope_hard_contract
  rex_frontend_expression_tag_ownership.c)

add_executable(
  rex_frontend_tag_redeclaration_ownership_translator
  rex_frontend_tag_redeclaration_ownership_translator.cpp
)
target_link_libraries(
  rex_frontend_tag_redeclaration_ownership_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME C_tests_rex_frontend_tag_redeclaration_ownership_structure
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND
    $<TARGET_FILE:rex_frontend_tag_redeclaration_ownership_translator>
    ${C_TEST_ROSE_FLAGS} -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_tag_redeclaration_ownership.c
)
set_tests_properties(
  C_tests_rex_frontend_tag_redeclaration_ownership_structure
  PROPERTIES LABELS "rex;CFE;C_tests")
rex_lock_test_for_input(
  C_tests_rex_frontend_tag_redeclaration_ownership_structure
  rex_frontend_tag_redeclaration_ownership.c)

add_executable(
  rex_frontend_preprocessing_file_boundary_translator
  rex_frontend_preprocessing_file_boundary_translator.cpp
)
target_link_libraries(
  rex_frontend_preprocessing_file_boundary_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME C_tests_rex_frontend_preprocessing_file_boundary_structure
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND
    $<TARGET_FILE:rex_frontend_preprocessing_file_boundary_translator>
    ${C_TEST_ROSE_FLAGS} -rose:collectAllCommentsAndDirectives
    -rose:skipfinalCompileStep -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_preprocessing_roundtrip.c
)
set_tests_properties(
  C_tests_rex_frontend_preprocessing_file_boundary_structure
  PROPERTIES LABELS "rex;CFE;C_tests")
rex_lock_test_for_input(
  C_tests_rex_frontend_preprocessing_file_boundary_structure
  rex_unparser_preprocessing_roundtrip.c)

add_executable(
  rex_frontend_preprocessing_anchor_contract_translator
  rex_frontend_preprocessing_anchor_contract_translator.cpp
)
target_link_libraries(
  rex_frontend_preprocessing_anchor_contract_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_initializer_include_boundary_translator
  rex_frontend_initializer_include_boundary_translator.cpp
)
target_link_libraries(
  rex_frontend_initializer_include_boundary_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_class_include_boundary_translator
  rex_frontend_class_include_boundary_translator.cpp
)
target_link_libraries(
  rex_frontend_class_include_boundary_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_enum_source_ownership_translator
  rex_frontend_enum_source_ownership_translator.cpp
)
target_link_libraries(
  rex_frontend_enum_source_ownership_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME C_tests_rex_frontend_enum_source_ownership_structure
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND
    $<TARGET_FILE:rex_frontend_enum_source_ownership_translator>
    ${C_TEST_ROSE_FLAGS} -std=c11 -rose:collectAllCommentsAndDirectives
    -rose:skipfinalCompileStep -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_enum_include_boundary.c
)
set_tests_properties(
  C_tests_rex_frontend_enum_source_ownership_structure
  PROPERTIES LABELS "rex;CFE;UNPARSER;C_tests")
rex_lock_test_for_input(
  C_tests_rex_frontend_enum_source_ownership_structure
  rex_unparser_enum_include_boundary.c)

add_test(
  NAME C_tests_rex_unparser_enum_include_boundary
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -rf rex_unparser_enum_include_boundary && \
     mkdir rex_unparser_enum_include_boundary && \
     cd rex_unparser_enum_include_boundary && \
     $<TARGET_FILE:rex_frontend_enum_source_ownership_translator> ${_c_tests_rose_flags_str} -std=c11 -rose:collectAllCommentsAndDirectives -rose:skipfinalCompileStep -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_enum_include_boundary.c && \
     enum_line=\$(grep -n 'enum RexExternalEnumeratorBoundary' rose_rex_unparser_enum_include_boundary.c | head -n1 | cut -d: -f1) && \
     define_line=\$(grep -n 'define REX_EXTERNAL_ENUMERATOR' rose_rex_unparser_enum_include_boundary.c | head -n1 | cut -d: -f1) && \
     include_line=\$(grep -n 'rex_unparser_enum_include_entries.def' rose_rex_unparser_enum_include_boundary.c | head -n1 | cut -d: -f1) && \
     last_line=\$(grep -n 'rex_enum_direct_last' rose_rex_unparser_enum_include_boundary.c | head -n1 | cut -d: -f1) && \
     terminal_enum_line=\$(grep -n 'enum RexTerminalExternalEnumeratorBoundary' rose_rex_unparser_enum_include_boundary.c | head -n1 | cut -d: -f1) && \
     terminal_direct_line=\$(grep -n 'rex_enum_terminal_direct' rose_rex_unparser_enum_include_boundary.c | head -n1 | cut -d: -f1) && \
     terminal_define_line=\$(grep -n 'define REX_TERMINAL_EXTERNAL_ENUMERATOR' rose_rex_unparser_enum_include_boundary.c | head -n1 | cut -d: -f1) && \
     terminal_include_line=\$(grep -n 'rex_unparser_enum_terminal_include_entries.def' rose_rex_unparser_enum_include_boundary.c | head -n1 | cut -d: -f1) && \
     test \"\$enum_line\" -le \"\$define_line\" && \
     test \"\$define_line\" -lt \"\$include_line\" && \
     test \"\$include_line\" -le \"\$last_line\" && \
     test \"\$terminal_enum_line\" -le \"\$terminal_direct_line\" && \
     test \"\$terminal_direct_line\" -le \"\$terminal_define_line\" && \
     test \"\$terminal_define_line\" -lt \"\$terminal_include_line\" && \
     ! grep -q 'rex_enum_external_first\|rex_enum_external_second\|rex_enum_terminal_external_first\|rex_enum_terminal_external_second' rose_rex_unparser_enum_include_boundary.c && \
     ! grep -Eq 'enum[[:space:]]+Rex(External|Terminal)EnumeratorBoundary[[:space:]]*:' rose_rex_unparser_enum_include_boundary.c && \
     ${CMAKE_C_COMPILER} -std=c11 -I${CMAKE_CURRENT_SOURCE_DIR} -fsyntax-only rose_rex_unparser_enum_include_boundary.c"
)
set_tests_properties(
  C_tests_rex_unparser_enum_include_boundary
  PROPERTIES LABELS "rex;CFE;UNPARSER;C_tests")
rex_lock_test_for_input(
  C_tests_rex_unparser_enum_include_boundary
  rex_unparser_enum_include_boundary.c)

add_test(
  NAME C_tests_rex_unparser_rejects_unclassified_enum_source
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_unclassified_enum_source.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[enum-source-ownership\\]: enum=0x[[:xdigit:]]+ name=RexExternalEnumeratorBoundary enumerator index=0 has no exact source role$' rex_unclassified_enum_source.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_enum_source_ownership_translator>
    --corrupt-source-role ${C_TEST_ROSE_FLAGS} -std=c11
    -rose:collectAllCommentsAndDirectives -rose:skipfinalCompileStep
    -I${CMAKE_CURRENT_SOURCE_DIR} -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_enum_include_boundary.c
)
set_tests_properties(
  C_tests_rex_unparser_rejects_unclassified_enum_source
  PROPERTIES LABELS "rex;CFE;UNPARSER;C_tests;death")
rex_lock_test_for_input(
  C_tests_rex_unparser_rejects_unclassified_enum_source
  rex_unparser_enum_include_boundary.c)

add_test(
  NAME C_tests_rex_frontend_initializer_include_boundary
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -rf rex_frontend_initializer_include_boundary && \
     mkdir rex_frontend_initializer_include_boundary && \
     cd rex_frontend_initializer_include_boundary && \
     $<TARGET_FILE:rex_frontend_initializer_include_boundary_translator> ${_c_tests_rose_flags_str} -std=c11 -rose:collectAllCommentsAndDirectives -rose:skipfinalCompileStep -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_initializer_include_boundary.c && \
     declaration_line=\$(grep -n 'rex_initializer_include_records' rose_rex_frontend_initializer_include_boundary.c | head -n1 | cut -d: -f1) && \
     include_line=\$(grep -n 'rex_frontend_initializer_include_entries.def' rose_rex_frontend_initializer_include_boundary.c | head -n1 | cut -d: -f1) && \
     scalar_declaration_line=\$(grep -n 'rex_scalar_initializer_include_value' rose_rex_frontend_initializer_include_boundary.c | head -n1 | cut -d: -f1) && \
     scalar_include_line=\$(grep -n 'rex_frontend_scalar_initializer_value.def' rose_rex_frontend_initializer_include_boundary.c | head -n1 | cut -d: -f1) && \
     complete_declaration_line=\$(grep -n 'rex_complete_initializer_include_value' rose_rex_frontend_initializer_include_boundary.c | head -n1 | cut -d: -f1) && \
     complete_include_line=\$(grep -n 'rex_frontend_complete_initializer_value.def' rose_rex_frontend_initializer_include_boundary.c | head -n1 | cut -d: -f1) && \
     test \"\$declaration_line\" -lt \"\$include_line\" && \
     test \"\$scalar_declaration_line\" -lt \"\$scalar_include_line\" && \
     test \"\$complete_declaration_line\" -lt \"\$complete_include_line\" && \
     test \"\$(grep -c '\"included-first\"' rose_rex_frontend_initializer_include_boundary.c)\" -eq 0 && \
     test \"\$(grep -c '\"included-second\"' rose_rex_frontend_initializer_include_boundary.c)\" -eq 0 && \
     test \"\$(grep -c '\"source-tail\"' rose_rex_frontend_initializer_include_boundary.c)\" -eq 1 && \
     ${CMAKE_C_COMPILER} -std=c11 -I${CMAKE_CURRENT_SOURCE_DIR} -fsyntax-only rose_rex_frontend_initializer_include_boundary.c"
)
set_tests_properties(
  C_tests_rex_frontend_initializer_include_boundary
  PROPERTIES LABELS "rex;CFE;UNPARSER;C_tests")
rex_lock_test_for_input(
  C_tests_rex_frontend_initializer_include_boundary
  rex_frontend_initializer_include_boundary.c)

add_test(
  NAME C_tests_rex_frontend_class_include_boundary
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -rf rex_frontend_class_include_boundary && \
     mkdir rex_frontend_class_include_boundary && \
     cd rex_frontend_class_include_boundary && \
     $<TARGET_FILE:rex_frontend_class_include_boundary_translator> ${_c_tests_rose_flags_str} -std=c11 -rose:collectAllCommentsAndDirectives -rose:skipfinalCompileStep -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_class_include_boundary.c && \
     test \"\$(grep -c 'rex_frontend_class_include_members.def' rose_rex_frontend_class_include_boundary.c)\" -eq 1 && \
     test \"\$(grep -c 'included_first' rose_rex_frontend_class_include_boundary.c)\" -eq 1 && \
     test \"\$(grep -c 'included_second' rose_rex_frontend_class_include_boundary.c)\" -eq 0 && \
     test \"\$(grep -c 'owner_head' rose_rex_frontend_class_include_boundary.c)\" -eq 1 && \
     test \"\$(grep -c 'owner_tail' rose_rex_frontend_class_include_boundary.c)\" -eq 2 && \
     ${CMAKE_C_COMPILER} -std=c11 -I${CMAKE_CURRENT_SOURCE_DIR} -fsyntax-only rose_rex_frontend_class_include_boundary.c"
)
set_tests_properties(
  C_tests_rex_frontend_class_include_boundary
  PROPERTIES LABELS "rex;CFE;UNPARSER;C_tests")
rex_lock_test_for_input(
  C_tests_rex_frontend_class_include_boundary
  rex_frontend_class_include_boundary.c)

foreach(_rex_preprocessing_anchor_input IN ITEMS
    rex_frontend_preprocessing_anchor_contract.c
    rex_frontend_preprocessing_empty_translation_unit.c
    rex_frontend_preprocessing_include_occurrence.c
    rex_frontend_preprocessing_parameter_boundary.c
    rex_frontend_preprocessing_terminal_directive.c)
  get_filename_component(
    _rex_preprocessing_anchor_name
    ${_rex_preprocessing_anchor_input}
    NAME_WE
  )
  set(_rex_preprocessing_anchor_test
    C_tests_${_rex_preprocessing_anchor_name}_structure)
  set(_rex_preprocessing_anchor_flags)
  if(_rex_preprocessing_anchor_input STREQUAL
      "rex_frontend_preprocessing_parameter_boundary.c")
    list(APPEND _rex_preprocessing_anchor_flags
      -DREX_PREPROCESSING_TRAILING_PARAMETER_ENABLED=1
      -DREX_PREPROCESSING_LEADING_PARAMETER_ENABLED=1)
  elseif(_rex_preprocessing_anchor_input STREQUAL
      "rex_frontend_preprocessing_anchor_contract.c")
    list(APPEND _rex_preprocessing_anchor_flags
      -DREX_PREPROCESSING_CONDITIONAL_DECLARATOR_ENABLED=1)
  endif()
  add_test(
    NAME ${_rex_preprocessing_anchor_test}
    WORKING_DIRECTORY ${_c_tests_special_dir}
    COMMAND
      $<TARGET_FILE:rex_frontend_preprocessing_anchor_contract_translator>
      ${C_TEST_ROSE_FLAGS} -std=c11
      ${_rex_preprocessing_anchor_flags}
      -rose:collectAllCommentsAndDirectives -rose:skipfinalCompileStep
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${_rex_preprocessing_anchor_input}
  )
  set_tests_properties(
    ${_rex_preprocessing_anchor_test}
    PROPERTIES LABELS "rex;CFE;C_tests")
  rex_lock_test_for_input(
    ${_rex_preprocessing_anchor_test}
    ${_rex_preprocessing_anchor_input})
endforeach()

add_test(
  NAME C_tests_rex_frontend_preprocessing_parameter_boundary_roundtrip
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -rf rex_frontend_preprocessing_parameter_boundary && \
     mkdir rex_frontend_preprocessing_parameter_boundary && \
     cd rex_frontend_preprocessing_parameter_boundary && \
     ${C_TEST_TRANSLATOR} ${_c_tests_rose_flags_str} -std=c11 -DREX_PREPROCESSING_TRAILING_PARAMETER_ENABLED=1 -DREX_PREPROCESSING_LEADING_PARAMETER_ENABLED=1 -rose:collectAllCommentsAndDirectives -rose:skipfinalCompileStep ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_preprocessing_parameter_boundary.c && \
     test \"\$(grep -Fxc '#if REX_PREPROCESSING_PARAMETER_ENABLED' rose_rex_frontend_preprocessing_parameter_boundary.c)\" -eq 1 && \
     test \"\$(grep -Fxc '#if REX_PREPROCESSING_EMPTY_PARAMETER_ENABLED' rose_rex_frontend_preprocessing_parameter_boundary.c)\" -eq 1 && \
     test \"\$(grep -Fxc '#if REX_PREPROCESSING_TRAILING_PARAMETER_ENABLED' rose_rex_frontend_preprocessing_parameter_boundary.c)\" -eq 1 && \
     test \"\$(grep -Fxc '#if REX_PREPROCESSING_LEADING_PARAMETER_ENABLED' rose_rex_frontend_preprocessing_parameter_boundary.c)\" -eq 1 && \
     test \"\$(grep -Fxc '#endif' rose_rex_frontend_preprocessing_parameter_boundary.c)\" -eq 4 && \
     awk '/#if REX_PREPROCESSING_PARAMETER_ENABLED/{opening=NR} /int rex_preprocessing_parameter/{parameter=NR} /^#endif/ && closing == 0 {closing=NR} /^\)/ && declarator_end == 0 {declarator_end=NR} END{exit !(opening < parameter && parameter < closing && closing < declarator_end)}' rose_rex_frontend_preprocessing_parameter_boundary.c && \
     ${CMAKE_C_COMPILER} -std=c11 -DREX_PREPROCESSING_TRAILING_PARAMETER_ENABLED=1 -DREX_PREPROCESSING_LEADING_PARAMETER_ENABLED=1 -fsyntax-only rose_rex_frontend_preprocessing_parameter_boundary.c && \
     ${CMAKE_C_COMPILER} -std=c11 -DREX_PREPROCESSING_TRAILING_PARAMETER_ENABLED=0 -DREX_PREPROCESSING_LEADING_PARAMETER_ENABLED=0 -fsyntax-only rose_rex_frontend_preprocessing_parameter_boundary.c"
)
set_tests_properties(
  C_tests_rex_frontend_preprocessing_parameter_boundary_roundtrip
  PROPERTIES LABELS "rex;CFE;UNPARSER;C_tests")
rex_lock_test_for_input(
  C_tests_rex_frontend_preprocessing_parameter_boundary_roundtrip
  rex_frontend_preprocessing_parameter_boundary.c)

add_test(
  NAME C_tests_rex_frontend_preprocessing_conditional_declarator_roundtrip
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -rf rex_frontend_preprocessing_conditional_declarator && \
     mkdir rex_frontend_preprocessing_conditional_declarator && \
     cd rex_frontend_preprocessing_conditional_declarator && \
     ${C_TEST_TRANSLATOR} ${_c_tests_rose_flags_str} -std=c11 -DREX_PREPROCESSING_CONDITIONAL_DECLARATOR_ENABLED=1 -rose:collectAllCommentsAndDirectives -rose:skipfinalCompileStep ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_preprocessing_anchor_contract.c && \
     test \"\$(grep -Fc '#if REX_PREPROCESSING_CONDITIONAL_DECLARATOR_ENABLED' rose_rex_frontend_preprocessing_anchor_contract.c)\" -eq 1 && \
     test \"\$(grep -Fc '#endif // REX_PREPROCESSING_CONDITIONAL_DECLARATOR_CLOSE' rose_rex_frontend_preprocessing_anchor_contract.c)\" -eq 1 && \
     ${CMAKE_C_COMPILER} -std=c11 -DREX_PREPROCESSING_CONDITIONAL_DECLARATOR_ENABLED=1 -fsyntax-only rose_rex_frontend_preprocessing_anchor_contract.c && \
     ${CMAKE_C_COMPILER} -std=c11 -DREX_PREPROCESSING_CONDITIONAL_DECLARATOR_ENABLED=0 -fsyntax-only rose_rex_frontend_preprocessing_anchor_contract.c && \
     $<TARGET_FILE:rex_frontend_preprocessing_anchor_contract_translator> ${_c_tests_rose_flags_str} -std=c11 -DREX_PREPROCESSING_CONDITIONAL_DECLARATOR_ENABLED=0 -rose:collectAllCommentsAndDirectives -rose:skipfinalCompileStep -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_preprocessing_anchor_contract.c"
)
set_tests_properties(
  C_tests_rex_frontend_preprocessing_conditional_declarator_roundtrip
  PROPERTIES LABELS "rex;CFE;UNPARSER;C_tests")
rex_lock_test_for_input(
  C_tests_rex_frontend_preprocessing_conditional_declarator_roundtrip
  rex_frontend_preprocessing_anchor_contract.c)

add_test(
  NAME C_tests_rex_frontend_preprocessing_terminal_directive_roundtrip
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND
    ${C_TEST_TRANSLATOR} ${C_TEST_ROSE_FLAGS} -std=c11
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_preprocessing_terminal_directive.c
)
set_tests_properties(
  C_tests_rex_frontend_preprocessing_terminal_directive_roundtrip
  PROPERTIES LABELS "rex;CFE;UNPARSER;C_tests")
rex_lock_test_for_input(
  C_tests_rex_frontend_preprocessing_terminal_directive_roundtrip
  rex_frontend_preprocessing_terminal_directive.c)

set(_c_tests_string_macro [[-DSTRING_MACRO="some_text"]])

add_test(
  NAME C_tests_check_pthread
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -pthread -c
          ${CMAKE_CURRENT_SOURCE_DIR}/needs_reentrant.c
)
set_tests_properties(C_tests_check_pthread PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_test2005_168_special_includes
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} ${C_TEST_LANG_FLAGS} ${C_TEST_ROSE_FLAGS}
          -I${CMAKE_CURRENT_SOURCE_DIR}
          -I${CMAKE_SOURCE_DIR}/src/util/commandlineProcessing
          -I${CMAKE_SOURCE_DIR}/src/frontend/SageIII
          -c ${CMAKE_CURRENT_SOURCE_DIR}/test2005_168.c
)
set_tests_properties(C_tests_test2005_168_special_includes PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_test2012_145_string_macro
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} ${C_TEST_LANG_FLAGS} ${C_TEST_ROSE_FLAGS}
          -I${CMAKE_CURRENT_SOURCE_DIR}
          ${_c_tests_string_macro}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/test2012_145.c
)
set_tests_properties(C_tests_test2012_145_string_macro PROPERTIES LABELS C_tests)

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
  NAME rex_unparser_same_directory_output_safety
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0 && \
     rm -rf rex_unparser_same_directory && \
     mkdir rex_unparser_same_directory && \
     cp ${CMAKE_CURRENT_SOURCE_DIR}/test2013_73.c rex_unparser_same_directory/input.c && \
     before=\$(sha256sum rex_unparser_same_directory/input.c) && \
     ${C_TEST_TRANSLATOR} -rose:unparse_in_same_directory_as_input_file ${_c_tests_rose_flags_str} -rose:skipfinalCompileStep rex_unparser_same_directory/input.c && \
     after=\$(sha256sum rex_unparser_same_directory/input.c) && \
     test \"\$before\" = \"\$after\" && \
     test -s rex_unparser_same_directory/rose_input.c && \
     absolute_input=\$(realpath rex_unparser_same_directory/input.c) && \
     { set +e; ${C_TEST_TRANSLATOR} -rose:output \"\$absolute_input\" ${_c_tests_rose_flags_str} -rose:skipfinalCompileStep \"\$absolute_input\" >rex_unparser_same_directory/alias.log 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; } && \
     after_rejected=\$(sha256sum rex_unparser_same_directory/input.c) && \
     test \"\$before\" = \"\$after_rejected\" && \
     test \"\$(grep -Fxc \"Error: refusing to overwrite unparser input without -rose:unparser:clobber_input_file: \\\"\$absolute_input\\\"\" rex_unparser_same_directory/alias.log)\" -eq 1 && \
     cp ${CMAKE_CURRENT_SOURCE_DIR}/test2013_73.c rex_unparser_relative_input.c && \
     ${C_TEST_TRANSLATOR} -rose:unparse_in_same_directory_as_input_file ${_c_tests_rose_flags_str} -rose:skipfinalCompileStep rex_unparser_relative_input.c && \
     test -s rose_rex_unparser_relative_input.c && \
     rm -f rex_unparser_relative_input.c rose_rex_unparser_relative_input.c"
)
set_tests_properties(rex_unparser_same_directory_output_safety PROPERTIES
  LABELS "rex;UNPARSER;C_tests")

add_test(
  NAME C_tests_rex_unparser_preprocessing_roundtrip
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -rf rex_unparser_preprocessing_roundtrip && \
     mkdir rex_unparser_preprocessing_roundtrip && \
     cd rex_unparser_preprocessing_roundtrip && \
     ${C_TEST_TRANSLATOR} ${_c_tests_rose_flags_str} -rose:collectAllCommentsAndDirectives -rose:skipfinalCompileStep ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_preprocessing_roundtrip.c && \
     test \"\$(grep -Fxc '#define REX_UNPARSER_SELF REX_UNPARSER_SELF' rose_rex_unparser_preprocessing_roundtrip.c)\" -eq 1 && \
     test \"\$(grep -Fxc '#define REX_UNPARSER_INCREMENT(value) ((value) + 1)' rose_rex_unparser_preprocessing_roundtrip.c)\" -eq 1 && \
     test \"\$(grep -Fxc '#define REX_UNPARSER_CONDITIONAL_IF 0' rose_rex_unparser_preprocessing_roundtrip.c)\" -eq 1 && \
     test \"\$(grep -Fc '#if 0' rose_rex_unparser_preprocessing_roundtrip.c)\" -eq 1 && \
     test \"\$(grep -Fc '#if REX_UNPARSER_CONDITIONAL_IF' rose_rex_unparser_preprocessing_roundtrip.c)\" -eq 1 && \
     test \"\$(grep -Fc '#elif defined(REX_UNPARSER_SELF)' rose_rex_unparser_preprocessing_roundtrip.c)\" -eq 1 && \
     test \"\$(grep -Fc '#else' rose_rex_unparser_preprocessing_roundtrip.c)\" -eq 2 && \
     test \"\$(grep -Fc '#endif' rose_rex_unparser_preprocessing_roundtrip.c)\" -eq 3 && \
     ${CMAKE_C_COMPILER} -std=c11 -I${CMAKE_CURRENT_SOURCE_DIR} -fsyntax-only rose_rex_unparser_preprocessing_roundtrip.c && \
     ! grep -q 'previously processed\|ignored' rose_rex_unparser_preprocessing_roundtrip.c"
)
set_tests_properties(C_tests_rex_unparser_preprocessing_roundtrip PROPERTIES
  LABELS "rex;UNPARSER;C_tests")

add_test(
  NAME C_tests_rex_unparser_opencl_attributes
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -rf rex_unparser_opencl_attributes && \
     mkdir rex_unparser_opencl_attributes && \
     cd rex_unparser_opencl_attributes && \
     ${C_TEST_TRANSLATOR} -rose:verbose 0 -rose:skipfinalCompileStep -cl-std=CL2.0 ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_opencl_attributes.cl && \
     test \"\$(grep -Fo '__kernel' rose_rex_unparser_opencl_attributes.cl | wc -l)\" -eq 2 && \
     test \"\$(grep -Fc 'vec_type_hint(float4)' rose_rex_unparser_opencl_attributes.cl)\" -eq 1 && \
     test \"\$(grep -Fc 'work_group_size_hint(8, 4, 2)' rose_rex_unparser_opencl_attributes.cl)\" -eq 1 && \
     test \"\$(grep -Fc 'reqd_work_group_size(16, 2, 1)' rose_rex_unparser_opencl_attributes.cl)\" -eq 1 && \
     ${C_TEST_TRANSLATOR} -rose:verbose 0 -rose:skipfinalCompileStep -cl-std=CL2.0 rose_rex_unparser_opencl_attributes.cl"
)
set_tests_properties(C_tests_rex_unparser_opencl_attributes PROPERTIES
  LABELS "rex;UNPARSER;C_tests")

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
  NAME C_tests_else_case_disambiguation
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "cp ${CMAKE_CURRENT_SOURCE_DIR}/test2008_02.c else_case_disambiguation_test.c && \
     env ROSE_TEST_ELSE_DISAMBIGUATION=x ${C_TEST_TRANSLATOR} ${_c_tests_lang_flags_str} ${_c_tests_rose_flags_str} -c else_case_disambiguation_test.c"
)
set_tests_properties(C_tests_else_case_disambiguation PROPERTIES LABELS C_tests)

if(REX_C_TESTS_HOST_IS_X86)
  add_test(
    NAME C_tests_link_using_asm_symbol_injection
    WORKING_DIRECTORY ${_c_tests_special_dir}
    COMMAND ${C_TEST_TRANSLATOR} -o test2014_83
            ${CMAKE_CURRENT_SOURCE_DIR}/test2014_83.c
  )
  set_tests_properties(C_tests_link_using_asm_symbol_injection PROPERTIES LABELS C_tests)
endif()

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

if(REX_C_TESTS_HOST_IS_X86)
  add_test(
    NAME C_tests_rtd_use
    WORKING_DIRECTORY ${_c_tests_special_dir}
    COMMAND ${C_TEST_TRANSLATOR} -m32 -mrtd -std=gnu89 -c
            ${CMAKE_CURRENT_SOURCE_DIR}/test2015_89.c
  )
  set_tests_properties(C_tests_rtd_use PROPERTIES LABELS C_tests)
endif()

add_test(
  NAME C_tests_optimization_specifier_O
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} ${C_TEST_ROSE_FLAGS} -O -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_153.c
)
set_tests_properties(C_tests_optimization_specifier_O PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_optimization_specifier_Os
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} ${C_TEST_ROSE_FLAGS} -Os -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_153.c
)
set_tests_properties(C_tests_optimization_specifier_Os PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_optimization_specifier_O1
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} ${C_TEST_ROSE_FLAGS} -O1 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_153.c
)
set_tests_properties(C_tests_optimization_specifier_O1 PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_optimization_specifier_O2
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} ${C_TEST_ROSE_FLAGS} -O2 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_153.c
)
set_tests_properties(C_tests_optimization_specifier_O2 PROPERTIES LABELS C_tests)

add_test(
  NAME C_tests_rex_frontend_driver_argument_order
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} ${C_TEST_ROSE_FLAGS}
          -DREX_DRIVER_ARGUMENT_ORDER=1
          -UREX_DRIVER_ARGUMENT_ORDER
          -DREX_DRIVER_ARGUMENT_ORDER=2
          -DREX_DRIVER_ARGUMENT_UNDEFINED
          -UREX_DRIVER_ARGUMENT_UNDEFINED
          -c
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_driver_argument_order.c
)
set_tests_properties(C_tests_rex_frontend_driver_argument_order
  PROPERTIES LABELS "C_tests;rex")

set(_rex_frontend_linker_object
  ${_c_tests_special_dir}/rex_frontend_linker_input.o)
set(_rex_frontend_linker_archive
  ${_c_tests_special_dir}/rex_frontend_linker_input.a)
file(WRITE ${_rex_frontend_linker_object} "")
file(WRITE ${_rex_frontend_linker_archive} "")
add_test(
  NAME C_tests_rex_frontend_distinguishes_linker_inputs
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} ${C_TEST_ROSE_FLAGS}
          -rose:skipfinalCompileStep
          -DREX_DRIVER_ARGUMENT_ORDER=1
          -UREX_DRIVER_ARGUMENT_ORDER
          -DREX_DRIVER_ARGUMENT_ORDER=2
          -DREX_DRIVER_ARGUMENT_UNDEFINED
          -UREX_DRIVER_ARGUMENT_UNDEFINED
          -c
          ${_rex_frontend_linker_object}
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_driver_argument_order.c
          ${_rex_frontend_linker_archive}
)
set_tests_properties(C_tests_rex_frontend_distinguishes_linker_inputs
  PROPERTIES LABELS "C_tests;rex")

set(_rex_frontend_option_operand_dir
  ${_c_tests_special_dir}/rex_frontend_option_operand.c)
file(MAKE_DIRECTORY ${_rex_frontend_option_operand_dir})
add_test(
  NAME C_tests_rex_frontend_exact_option_operand_ownership
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} ${C_TEST_ROSE_FLAGS}
          -rose:skipfinalCompileStep
          -working-directory ${_rex_frontend_option_operand_dir}
          -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2003_12.c
)
set_tests_properties(C_tests_rex_frontend_exact_option_operand_ownership
  PROPERTIES LABELS "C_tests;rex")

set(_rex_missing_llvm_root
  "/rex-test-missing-llvm-${LLVM_VERSION_MAJOR}")
set(_rex_missing_llvm_driver_diagnostic
  "REX_FRONTEND_INVARIANT[clang-driver-path]: LLVM_DIR selects '${_rex_missing_llvm_root}', but that root has no executable for linked LLVM ${LLVM_VERSION_MAJOR}")
add_test(
  NAME C_tests_rex_frontend_exact_llvm_driver
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=rex_frontend_exact_llvm_driver.log; expected=\"$1\"; shift; set +e; LLVM_DIR='${_rex_missing_llvm_root}' \"$@\" >\"$log\" 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc -- \"$expected\" \"$log\")\" -eq 1"
    dummy
    "${_rex_missing_llvm_driver_diagnostic}"
    ${C_TEST_TRANSLATOR} ${C_TEST_ROSE_FLAGS}
    -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_driver_argument_order.c
)
set_tests_properties(C_tests_rex_frontend_exact_llvm_driver
  PROPERTIES LABELS "C_tests;rex;death")

if(REX_C_TESTS_HOST_IS_X86)
  add_test(
    NAME C_tests_mregparm_m32_use
    WORKING_DIRECTORY ${_c_tests_special_dir}
    COMMAND ${C_TEST_TRANSLATOR} -mregparm=3 -m32 -std=gnu89 -c
            ${CMAKE_CURRENT_SOURCE_DIR}/test2015_154.c
  )
  set_tests_properties(C_tests_mregparm_m32_use PROPERTIES LABELS C_tests)
endif()

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
  COMMAND ${C_TEST_TRANSLATOR} -std=gnu89 -rose:unparse_tokens -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2003_12.c
)
set_tests_properties(C_tests_token_mapping PROPERTIES  LABELS C_tests)

add_test(
  NAME rex_unparser_output_paths_are_failure_atomic
  COMMAND
    ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/run_rex_unparser_output_safety.sh
    ${C_TEST_TRANSLATOR}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_output_atomicity.c
    ${_c_tests_special_dir}/rex_unparser_output_safety
)
set_tests_properties(
  rex_unparser_output_paths_are_failure_atomic
  PROPERTIES
    LABELS "rex;UNPARSER;C_tests"
    RESOURCE_LOCK rex_unparser_output_safety
)

add_test(
  NAME rex_unparser_hard_linked_destination_is_rejected
  COMMAND
    ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_hard_linked_destination.sh
    ${C_TEST_TRANSLATOR}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_output_atomicity.c
    ${_c_tests_special_dir}/rex_unparser_hard_linked_destination
)
set_tests_properties(
  rex_unparser_hard_linked_destination_is_rejected
  PROPERTIES
    LABELS "rex;UNPARSER;C_tests"
    RESOURCE_LOCK rex_unparser_hard_linked_destination
)

add_test(
  NAME rex_unparser_atomic_output_preserves_permissions
  COMMAND
    ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/run_rex_unparser_output_mode.sh
    ${C_TEST_TRANSLATOR}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_output_atomicity.c
    ${_c_tests_special_dir}/rex_unparser_output_mode
)
set_tests_properties(
  rex_unparser_atomic_output_preserves_permissions
  PROPERTIES
    LABELS "rex;UNPARSER;C_tests"
    RESOURCE_LOCK rex_unparser_output_mode
)

add_test(
  NAME rex_unparser_atomic_output_retries_stale_collision
  COMMAND
    ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/run_rex_unparser_output_collision.sh
    ${C_TEST_TRANSLATOR}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_output_atomicity.c
    ${_c_tests_special_dir}/rex_unparser_output_collision
)
set_tests_properties(
  rex_unparser_atomic_output_retries_stale_collision
  PROPERTIES
    LABELS "rex;UNPARSER;C_tests"
    RESOURCE_LOCK rex_unparser_output_collision
)

add_test(
  NAME rex_unparser_atomic_output_keeps_exclusive_descriptor
  COMMAND
    ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/run_rex_unparser_staging_descriptor.sh
    ${C_TEST_TRANSLATOR}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_output_atomicity.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_staging_close_substitution.c
    ${CMAKE_C_COMPILER}
    ${_c_tests_special_dir}/rex_unparser_staging_descriptor
)
set_tests_properties(
  rex_unparser_atomic_output_keeps_exclusive_descriptor
  PROPERTIES
    LABELS "rex;UNPARSER;C_tests"
    RESOURCE_LOCK rex_unparser_staging_descriptor
)

if(REX_C_TESTS_HOST_IS_X86)
  add_test(
    NAME C_tests_gnu_S_option
    WORKING_DIRECTORY ${_c_tests_special_dir}
    COMMAND ${C_TEST_TRANSLATOR} -S -o test2014_41.s
            ${CMAKE_CURRENT_SOURCE_DIR}/test2014_41.c
  )
  set_tests_properties(C_tests_gnu_S_option PROPERTIES  LABELS C_tests)
endif()

add_test(
  NAME C_tests_m32_use
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -m32 -std=gnu89 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_95.c
)
set_tests_properties(C_tests_m32_use PROPERTIES  LABELS C_tests)

add_test(
  NAME C_tests_file_suffix
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${C_TEST_TRANSLATOR} -c
          ${CMAKE_CURRENT_SOURCE_DIR}/test2015_133.S
)
set_tests_properties(C_tests_file_suffix PROPERTIES  LABELS C_tests)

add_test(
  NAME C_tests_rex_frontend_designated_initializer_ordered_surface
  WORKING_DIRECTORY ${_c_tests_special_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "${C_TEST_TRANSLATOR} ${_c_tests_lang_flags_str} ${_c_tests_rose_flags_str} -rose:skipfinalCompileStep -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_designated_initializer_chain.c && grep -F '.leaves[1].values[2] = 7' rose_rex_frontend_designated_initializer_chain.c && grep -F '[2 ... 5] = 9' rose_rex_frontend_designated_initializer_chain.c && ${CMAKE_C_COMPILER} -std=gnu11 -c rose_rex_frontend_designated_initializer_chain.c"
)
set_tests_properties(
  C_tests_rex_frontend_designated_initializer_ordered_surface
  PROPERTIES LABELS "C_tests;rex"
)

rex_verify_compile_test_source_coverage(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTIES
    REX_C_TESTS_SOURCES
    REX_C89_TESTS_SOURCES
    REX_C_TESTS_X86_ONLY_SOURCES
  IGNORE
    rex_frontend_generated_typedef_dependency_translator.cpp
    rex_frontend_inline_tag_ownership_translator.cpp
    rex_frontend_multi_tu_tag_identity_a.c
    rex_frontend_multi_tu_tag_identity_b.c
    rex_frontend_multi_tu_tag_identity_translator.cpp
    rex_frontend_tag_redeclaration_ownership_translator.cpp
    rex_unparser_staging_close_substitution.c
    run_rex_unparser_output_collision.sh
    run_rex_unparser_output_safety.sh
    run_rex_unparser_staging_descriptor.sh
    test2014_20_inc.c
    test2014_27_inc.c
    test2014_28_inc.c
)
