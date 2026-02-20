set(_saf_src_root ${CMAKE_SOURCE_DIR}/src/midend/programAnalysis/genericDataflow)

include_directories(
  ${ROSE_INCLUDES}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${_saf_src_root}
  ${_saf_src_root}/analysis
  ${_saf_src_root}/arrIndexLabeler
  ${_saf_src_root}/cfgUtils
  ${_saf_src_root}/lattice
  ${_saf_src_root}/rwAccessLabeler
  ${_saf_src_root}/simpleAnalyses
  ${_saf_src_root}/state
  ${_saf_src_root}/variables
)

add_executable(taintAnalysisTest taintAnalysisTest.C)
target_link_libraries(taintAnalysisTest ROSE_DLL ${link_with_libraries})

add_executable(constantPropagationTest constantPropagation.C constantPropagationTest.C)
target_link_libraries(constantPropagationTest ROSE_DLL ${link_with_libraries})

add_executable(taintedFlowAnalysisTest taintedFlowAnalysis.C taintedFlowAnalysisTest.C)
target_link_libraries(taintedFlowAnalysisTest ROSE_DLL ${link_with_libraries})

add_executable(liveDeadVarAnalysisTest liveDeadVarAnalysisTest.C)
target_link_libraries(liveDeadVarAnalysisTest ROSE_DLL ${link_with_libraries})

add_executable(pointerAliasAnalysisTest pointerAliasAnalysis.C pointerAliasAnalysisTest.C)
target_link_libraries(pointerAliasAnalysisTest ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

set(_cxx_test_dir ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)
set(_c99_test_dir ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/C99_tests)
set(_f77_test_dir ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests)
set(_f03_test_dir ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests)

set(TAINT_ANALYSIS_SPECIMENS
  taint_input0.C
  taint_input1.C
  taint_input2.C
  taint_input3.C
  taint_input4.C
  taint_input5.C
  taint_input6.C
)

set(CXX_CONSTPROP_LOCAL_SPECIMENS
  cp_test1.C
)

set(CXX_LIVENESS_SPECIMENS
  test1.C
  test4.C
  test5.C
  test6.C
)

set(CXX_POINTER_ALIAS_SPECIMENS
  test_ptr1.C
  test_ptr2.C
  test_ptr3.C
  test_ptr4.C
  test_ptr5.C
)

set(CXX_CONSTPROP_SPECIMENS
  test2001_29.C
  test2011_133.C
)

set(FAILING_CXX_CONSTPROP_SPECIMENS
  X.C
  lexPhase2003_01.C
  math.C
  test2001_01.C
  test2001_02.C
  test2001_05.C
  test2001_06.C
  test2001_07.C
  test2001_08.C
  test2001_09.C
  test2001_10.C
  test2001_11.C
  test2001_12.C
  test2001_13.C
  test2001_15.C
  test2001_16.C
  test2001_17.C
  test2001_18.C
  test2001_19.C
  test2001_20.C
  test2001_21.C
  test2001_22.C
  test2001_23.C
  test2001_24.C
  test2001_25.C
  test2001_27.C
  test2002_01.C
  test2003_02.C
  test2003_03.C
  test2003_04.C
  test2003_05.C
  test2003_06.C
  test2003_08.C
  test2003_09.C
  test2003_10.C
  test2003_11.C
  test2003_12.C
  test2003_13.C
  test2003_14.C
  test2003_16.C
  test2003_17.C
  test2003_20.C
  test2003_21.C
  test2003_23.C
  test2003_25.C
  test2003_26.C
  test2003_27.C
  test2003_28.C
  test2003_29.C
  test2003_30.C
  test2003_31.C
  test2003_37.C
  test2003_38.C
  test2004_01.C
  test2004_03.C
  test2004_04.C
  test2004_05.C
  test2004_06.C
  test2004_07.C
  test2004_08.C
  test2004_09.C
  test2004_100.C
  test2004_101.C
  test2004_102.C
  test2004_103.C
  test2004_104.C
  test2004_105.C
  test2004_106.C
  test2004_107.C
  test2004_108.C
  test2004_109.C
  test2004_110.C
  test2004_111.C
  test2004_112.C
  test2004_113.C
  test2004_114.C
  test2004_115.C
  test2004_117.C
  test2004_118.C
  test2004_119.C
  test2004_12.C
  test2004_121.C
  test2004_122.C
  test2004_123.C
  test2004_124.C
  test2004_125.C
  test2004_128.C
  test2004_13.C
  test2004_131.C
  test2004_132.C
  test2004_133.C
  test2004_134.C
  test2004_135.C
  test2004_136.C
  test2004_137.C
  test2004_138.C
  test2004_140.C
  test2004_144.C
  test2004_146.C
  test2004_147.C
  test2004_150.C
  test2004_151.C
  test2004_152.C
  test2004_153.C
  test2004_154.C
  test2004_155.C
  test2004_156.C
  test2004_158.C
  test2004_16.C
  test2004_160.C
  test2004_163.C
  test2004_17.C
  test2004_19.C
  test2004_23.C
  test2004_24.C
  test2004_26.C
  test2004_27.C
  test2004_28.C
  test2004_29.C
  test2004_30.C
  test2004_31.C
  test2004_32.C
  test2004_33.C
  test2004_34.C
  test2004_37.C
  test2004_38.C
  test2004_39.C
  test2004_40.C
  test2004_42.C
  test2004_44.C
  test2004_49.C
  test2004_50.C
  test2004_51.C
  test2004_53.C
  test2004_54.C
  test2004_56.C
  test2004_58.C
  test2004_59.C
  test2004_61.C
  test2004_62.C
  test2004_64.C
  test2004_65.C
  test2004_67.C
  test2004_68.C
  test2004_69.C
  test2004_70.C
  test2004_71.C
  test2004_73.C
  test2004_74.C
  test2004_76.C
  test2004_78.C
  test2004_79.C
  test2004_80.C
  test2004_81.C
  test2004_83.C
  test2004_84.C
  test2004_86.C
  test2004_87.C
  test2004_90.C
  test2004_91.C
  test2004_93.C
  test2004_94.C
  test2004_95.C
  test2004_96.C
  test2004_97.C
  test2004_98.C
  test2004_99.C
  test2005_01.C
  test2005_02.C
  test2005_03.C
  test2005_05.C
  test2005_105.C
  test2005_107.C
  test2005_109.C
  test2005_11.C
  test2005_110.C
  test2005_111.C
  test2005_113.C
  test2005_116.C
  test2005_117.C
  test2005_119.C
  test2005_120.C
  test2005_121.C
  test2005_123.C
  test2005_124.C
  test2005_125.C
  test2005_126.C
  test2005_127.C
  test2005_13.C
  test2005_134.C
  test2005_135.C
  test2005_14.C
  test2005_140.C
  test2005_144.C
  test2005_145.C
  test2005_148.C
  test2005_149.C
  test2005_15.C
  test2005_150.C
  test2005_153.C
  test2005_154.C
  test2005_157.C
  test2005_159.C
  test2005_16.C
  test2005_160.C
  test2005_162.C
  test2005_163.C
  test2005_165.C
  test2005_169.C
  test2005_170.C
  test2005_171.C
  test2005_172.C
  test2005_173.C
  test2005_174.C
  test2005_175.C
  test2005_176.C
  test2005_179.C
  test2005_180.C
  test2005_182.C
  test2005_183.C
  test2005_184.C
  test2005_185.C
  test2005_189.C
  test2005_190.C
  test2005_191.C
  test2005_192.C
  test2005_195.C
  test2005_196.C
  test2005_197.C
  test2005_198.C
  test2005_20.C
  test2005_203.C
  test2005_206.C
  test2005_207.C
  test2005_208.C
  test2005_209.C
  test2005_21.C
  test2005_22.C
  test2005_23.C
  test2005_24.C
  test2005_25.C
  test2005_26.C
  test2005_30.C
  test2005_31.C
  test2005_32.C
  test2005_33.C
  test2005_36.C
  test2005_40.C
  test2005_41.C
  test2005_45.C
  test2005_47.C
  test2005_48.C
  test2005_49.C
  test2005_50.C
  test2005_51.C
  test2005_52.C
  test2005_53.C
  test2005_55.C
  test2005_57.C
  test2005_58.C
  test2005_59.C
  test2005_60.C
  test2005_61.C
  test2005_62.C
  test2005_64.C
  test2005_65.C
  test2005_66.C
  test2005_67.C
  test2005_68.C
  test2005_70.C
  test2005_72.C
  test2005_73.C
  test2005_74.C
  test2005_78.C
  test2005_79.C
  test2005_80.C
  test2005_81.C
  test2005_83.C
  test2005_84.C
  test2005_86.C
  test2005_87.C
  test2005_88.C
  test2005_89.C
  test2005_90.C
  test2005_91.C
  test2005_92.C
  test2005_94.C
  test2005_95.C
  test2005_96.C
  test2005_97.C
  test2005_99.C
  test2006_01.C
  test2006_02.C
  test2006_03.C
  test2006_06.C
  test2006_07.C
  test2006_08.C
  test2006_10.C
  test2006_101.C
  test2006_103.C
  test2006_104.C
  test2006_105.C
  test2006_106.C
  test2006_107.C
  test2006_108.C
  test2006_109.C
  test2006_111.C
  test2006_113.C
  test2006_114.C
  test2006_116.C
  test2006_117.C
  test2006_118.C
  test2006_12.C
  test2006_120.C
  test2006_122.C
  test2006_123.C
  test2006_126.C
  test2006_129.C
  test2006_13.C
  test2006_14.C
  test2006_140.C
  test2006_142.C
  test2006_143.C
  test2006_144.C
  test2006_145.C
  test2006_146.C
  test2006_149.C
  test2006_150.C
  test2006_151.C
  test2006_152.C
  test2006_153.C
  test2006_155.C
  test2006_156.C
  test2006_157.C
  test2006_158.C
  test2006_16.C
  test2006_161.C
  test2006_162.C
  test2006_163.C
  test2006_164.C
  test2006_165.C
  test2006_17.C
  test2006_177.C
  test2006_178.C
  test2006_179.C
  test2006_18.C
  test2006_181.C
  test2006_183.C
  test2006_19.C
  test2006_22.C
  test2006_23.C
  test2006_25.C
  test2006_26.C
  test2006_34.C
  test2006_37.C
  test2006_38.C
  test2006_41.C
  test2006_42.C
  test2006_44.C
  test2006_46.C
  test2006_52.C
  test2006_53.C
  test2006_55.C
  test2006_56.C
  test2006_57.C
  test2006_59.C
  test2006_61.C
  test2006_62.C
  test2006_63.C
  test2006_66.C
  test2006_67.C
  test2006_68.C
  test2006_69.C
  test2006_70.C
  test2006_71.C
  test2006_72.C
  test2006_73.C
  test2006_74.C
  test2006_75.C
  test2006_76.C
  test2006_77.C
  test2006_80.C
  test2006_81.C
  test2006_82.C
  test2006_83.C
  test2006_86.C
  test2006_87.C
  test2006_88.C
  test2006_91.C
  test2006_94.C
  test2006_95.C
  test2006_96.C
  test2006_97.C
  test2006_98.C
  test2006_99.C
  test2007_01.C
  test2007_02.C
  test2007_03.C
  test2007_04.C
  test2007_05.C
  test2007_06.C
  test2007_07.C
  test2007_08.C
  test2007_09.C
  test2007_10.C
  test2007_102.C
  test2007_103.C
  test2007_12.C
  test2007_128.C
  test2007_13.C
  test2007_130.C
  test2007_133.C
  test2007_137.C
  test2007_14.C
  test2007_140.C
  test2007_142.C
  test2007_147.C
  test2007_15.C
  test2007_150.C
  test2007_151.C
  test2007_152.C
  test2007_154.C
  test2007_157.C
  test2007_16.C
  test2007_160.C
  test2007_161.C
  test2007_162.C
  test2007_163.C
  test2007_165.C
  test2007_167.C
  test2007_169.C
  test2007_17.C
  test2007_172.C
  test2007_173.C
  test2007_21.C
  test2007_23.C
  test2007_24.C
  test2007_25.C
  test2007_28.C
  test2007_29.C
  test2007_38.C
  test2007_41.C
  test2007_43.C
  test2007_46.C
  test2007_47.C
  test2007_48.C
  test2007_49.C
  test2007_50.C
  test2007_51.C
  test2007_52.C
  test2007_53.C
  test2007_55.C
  test2007_57.C
  test2007_59.C
  test2007_62.C
  test2007_70.C
  test2007_72.C
  test2007_75.C
  test2007_77.C
  test2007_78.C
  test2007_81.C
  test2007_82.C
  test2007_83.C
  test2007_84.C
  test2007_85.C
  test2007_90.C
  test2007_98.C
  test2008_01.C
  test2008_03.C
  test2008_04.C
  test2008_05.C
  test2008_06.C
  test2008_09.C
  test2008_10.C
  test2008_11.C
  test2008_12.C
  test2008_13.C
  test2008_14.C
  test2009_01.C
  test2009_02.C
  test2009_03.C
  test2009_04.C
  test2009_05.C
  test2009_06.C
  test2009_08.C
  test2009_09.C
  test2009_10.C
  test2009_12.C
  test2009_14.C
  test2009_15.C
  test2009_18.C
  test2009_19.C
  test2009_20.C
  test2009_21.C
  test2009_24.C
  test2009_27.C
  test2009_28.C
  test2009_30.C
  test2009_39.C
  test2010_18.C
  test2010_25.C
  test2011_01.C
  test2011_02.C
  test2011_05.C
  test2011_07.C
  test2011_100.C
  test2011_111.C
  test2011_115.C
  test2011_117.C
  test2011_118.C
  test2011_119.C
  test2011_19.C
  test2011_20.C
  test2011_21.C
  test2011_22.C
  test2011_23.C
  test2011_24.C
  test2011_25.C
  test2011_26.C
  test2011_27.C
  test2011_28.C
  test2011_29.C
  test2011_30.C
  test2011_31.C
  test2011_32.C
  test2011_33.C
  test2011_34.C
  test2011_35.C
  test2011_36.C
  test2011_37.C
  test2011_38.C
  test2011_40.C
  test2011_41.C
  test2011_42.C
  test2011_43.C
  test2011_44.C
  test2011_45.C
  test2011_46.C
  test2011_47.C
  test2011_48.C
  test2011_49.C
  test2011_50.C
  test2011_51.C
  test2011_52.C
  test2011_53.C
  test2011_54.C
  test2011_55.C
  test2011_56.C
  test2011_57.C
  test2011_58.C
  test2011_59.C
  test2011_60.C
  test2011_61.C
  test2011_62.C
  test2011_63.C
  test2011_64.C
  test2011_65.C
  test2011_67.C
  test2011_68.C
  test2011_69.C
  test2011_70.C
  test2011_71.C
  test2011_73.C
  test2011_74.C
  test2011_75.C
  test2011_76.C
  test2011_77.C
  test2011_78.C
  test2011_79.C
  test2011_80.C
  test2011_81.C
  test2011_82.C
  test2011_83.C
  test2011_84.C
  test2011_85.C
  test2011_86.C
  test2011_87.C
  test2011_88.C
  test2011_89.C
  test2011_90.C
  test2011_91.C
  test2011_92.C
  test2011_93.C
  test2011_94.C
  test2011_95.C
  test2011_96.C
  test2011_97.C
  test2011_98.C
  test_CplusplusMacro_Cpp.C
  test_int_lexemes.C
  test_int_lexemes_donot_pass.C
  test_simple_int.C
  test_wchars.C
)

set(CXX_CONSTPROP_SPECIMENS_FAILING
  inputBug317.C
  inputBug327.C
  inputForLoopLocator.C
  test2001_04.C
  test2001_14.C
  test2001_28.C
  test2003_01.C
  test2003_15.C
  test2003_18.C
  test2003_32.C
  test2003_35.C
  test2003_36.C
  test2004_02.C
  test2004_10.C
  test2004_11.C
  test2004_120.C
  test2004_127.C
  test2004_129.C
  test2004_130.C
  test2004_139.C
  test2004_14.C
  test2004_141.C
  test2004_142.C
  test2004_143.C
  test2004_145.C
  test2004_149.C
  test2004_15.C
  test2004_157.C
  test2004_159.C
  test2004_161.C
  test2004_162.C
  test2004_164.C
  test2004_18.C
  test2004_35.C
  test2004_36.C
  test2004_43.C
  test2004_45.C
  test2004_47.C
  test2004_48.C
  test2004_52.C
  test2004_55.C
  test2004_60.C
  test2004_63.C
  test2004_75.C
  test2004_77.C
  test2004_85.C
  test2004_88.C
  test2005_06.C
  test2005_07.C
  test2005_08.C
  test2005_09.C
  test2005_10.C
  test2005_100.C
  test2005_101.C
  test2005_103.C
  test2005_104.C
  test2005_106.C
  test2005_108.C
  test2005_112.C
  test2005_118.C
  test2005_12.C
  test2005_122.C
  test2005_128.C
  test2005_129.C
  test2005_130.C
  test2005_131.C
  test2005_132.C
  test2005_133.C
  test2005_136.C
  test2005_137.C
  test2005_138.C
  test2005_139.C
  test2005_141.C
  test2005_142.C
  test2005_143.C
  test2005_146.C
  test2005_152.C
  test2005_155.C
  test2005_156.C
  test2005_158.C
  test2005_161.C
  test2005_164.C
  test2005_166.C
  test2005_167.C
  test2005_168.C
  test2005_17.C
  test2005_177.C
  test2005_178.C
  test2005_181.C
  test2005_187.C
  test2005_19.C
  test2005_193.C
  test2005_194.C
  test2005_199.C
  test2005_202.C
  test2005_27.C
  test2005_28.C
  test2005_29.C
  test2005_34.C
  test2005_35.C
  test2005_37.C
  test2005_38.C
  test2005_39.C
  test2005_42.C
  test2005_43.C
  test2005_44.C
  test2005_46.C
  test2005_63.C
  test2005_71.C
  test2005_75a.C
  test2005_75b.C
  test2005_76.C
  test2005_77.C
  test2005_82.C
  test2005_93.C
  test2005_98.C
  test2006_04.C
  test2006_05.C
  test2006_102.C
  test2006_112.C
  test2006_115.C
  test2006_119.C
  test2006_124.C
  test2006_125.C
  test2006_141.C
  test2006_154.C
  test2006_159.C
  test2006_160.C
  test2006_166.C
  test2006_185.C
  test2006_189.C
  test2006_20.C
  test2006_28.C
  test2006_39.C
  test2006_40.C
  test2006_47.C
  test2006_50.C
  test2006_54.C
  test2006_58.C
  test2006_78.C
  test2006_79.C
  test2006_84.C
  test2006_85.C
  test2006_89.C
  test2006_90.C
  test2006_92.C
  test2007_11.C
  test2007_135.C
  test2007_141.C
  test2007_155.C
  test2007_158.C
  test2007_159.C
  test2007_164.C
  test2007_166.C
  test2007_170.C
  test2007_171.C
  test2007_26.C
  test2007_40.C
  test2007_44.C
  test2007_94.C
  test2008_08.C
  test2008_16.C
  test2009_11.C
  test2009_31.C
  test2009_37.C
  test2009_40.C
  test2009_41.C
  test2009_42.C
  test2010_03.C
  test2010_04.C
  test2010_05.C
  test2010_06.C
  test2010_19.C
  test2010_24.C
  test2011_06.C
  test2011_101.C
  test2011_110.C
  test2011_121.C
  test2011_39.C
  test2011_72.C
  test2011_99.C
)

set(CXX_CONSTPROP_SPECIMENS_HANGING
  test2005_114.C
)

set(FAILING_C_CONSTPROP_SPECIMENS
  X.C
  test2001_02.C
  test2001_05.C
  test2001_07.C
  test2001_08.C
  test2001_09.C
  test2001_17.C
  test2001_23.C
  test2001_24.C
  test2001_25.C
  test2002_01.C
  test2003_04.C
  test2003_05.C
  test2003_06.C
  test2003_10.C
  test2003_11.C
  test2003_13.C
  test2003_14.C
  test2003_16.C
  test2003_20.C
  test2003_21.C
  test2003_23.C
  test2003_24.C
  test2003_25.C
  test2003_26.C
  test2003_29.C
  test2003_30.C
  test2003_31.C
  test2004_03.C
  test2004_04.C
  test2004_05.C
  test2004_102.C
  test2004_105.C
  test2004_107.C
  test2004_108.C
  test2004_114.C
  test2004_118.C
  test2004_13.C
  test2004_131.C
  test2004_152.C
  test2004_153.C
  test2004_154.C
  test2004_163.C
  test2004_19.C
  test2004_23.C
  test2004_28.C
  test2004_29.C
  test2004_49.C
  test2004_53.C
  test2004_61.C
  test2004_74.C
  test2004_94.C
  test2004_96.C
  test2005_111.C
  test2005_116.C
  test2005_125.C
  test2005_157.C
  test2005_159.C
  test2005_16.C
  test2005_165.C
  test2005_169.C
  test2005_170.C
  test2005_176.C
  test2005_180.C
  test2005_182.C
  test2005_184.C
  test2005_191.C
  test2005_198.C
  test2005_20.C
  test2005_209.C
  test2005_21.C
  test2005_22.C
  test2005_23.C
  test2005_26.C
  test2005_48.C
  test2005_62.C
  test2005_84.C
  test2005_92.C
  test2005_95.C
  test2006_07.C
  test2006_10.C
  test2006_103.C
  test2006_104.C
  test2006_105.C
  test2006_106.C
  test2006_107.C
  test2006_108.C
  test2006_109.C
  test2006_111.C
  test2006_113.C
  test2006_116.C
  test2006_12.C
  test2006_137.C
  test2006_14.C
  test2006_140.C
  test2006_142.C
  test2006_145.C
  test2006_146.C
  test2006_149.C
  test2006_150.C
  test2006_151.C
  test2006_152.C
  test2006_153.C
  test2006_154.C
  test2006_16.C
  test2006_181.C
  test2006_23.C
  test2006_34.C
  test2006_52.C
  test2006_53.C
  test2006_55.C
  test2006_56.C
  test2006_57.C
  test2006_62.C
  test2006_69.C
  test2006_75.C
  test2006_76.C
  test2006_82.C
  test2006_83.C
  test2006_88.C
  test2006_91.C
  test2006_94.C
  test2006_97.C
  test2006_99.C
  test_CplusplusMacro_C.C
  test_int_lexemes.C
  test_int_lexemes_donot_pass.C
  test_simple_int.C
  test_wchars.C
)

set(C_CONSTPROP_SPECIMENS_FAILING
  lastTest.C
  test2001_14.C
  test2003_15.C
  test2003_18.C
  test2003_32.C
  test2003_35.C
  test2003_36.C
  test2004_14.C
  test2004_60.C
  test2005_100.C
  test2005_164.C
  test2005_187.C
  test2005_202.C
  test2006_40.C
  test2006_50.C
)

set(FAILING_C99_CONSTPROP_SPECIMENS
  bool.c
  complex_01.c
  complex_03.c
  constants.c
  test2005_186.c
  test2006_127.c
  test2006_143.c
)

set(C99_CONSTPROP_SPECIMENS_FAILING
  test2008_01.c
)

set(F77_CONSTPROP_SPECIMENS
  test2007_222.f
)

set(F77_CONSTPROP_SPECIMENS_FAILING
  test2007_129.f
  test2007_132.f
  test2007_148.f
  test2007_150.f
  test2007_151.f
  test2007_153.f
  test2007_154.f
  test2007_155.f
  test2007_156.f
  test2007_162.f
  test2007_163.f
  test2007_164.f
  test2007_175.f
  test2007_198.f
  test2007_200.f
  test2007_201.f
  test2007_202.f
  test2007_204.f
  test2007_205.f
  test2007_206.f
  test2007_207.f
  test2007_208.f
  test2007_211.f
  test2007_212.f
  test2007_213.f
  test2007_214.f
  test2007_215.f
  test2007_216.f
  test2007_217.f
  test2007_218.f
  test2007_220.f
  test2007_221.f
  test2007_223.f
  test2007_224.f
  test2007_226.f
  test2007_228.f
  test2007_229.f
  test2007_230.f
  test2007_231.f
  test2007_232.f
  test2007_234.f
  test2007_242.f
  test2007_243.f
  test2007_245.f
  test2007_246.f
  test2007_247.f
  test2007_251.f
  test2007_253.f
  test2007_259.f
  test2007_260.f
  test2007_261.f
)

set(F77_CONSTPROP_SPECIMENS_FAILING_CFG
  test2007_124.f
  test2007_125.f
  test2007_128.f
  test2007_145.f
  test2007_157.f
  test2007_183.f
  test2007_184.f
  test2007_185.f
  test2007_186.f
  test2007_187.f
  test2007_188.f
  test2007_189.f
  test2007_191.f
  test2007_192.f
  test2007_193.f
  test2007_194.f
  test2007_195.f
  test2007_196.f
  test2007_197.f
  test2007_209.f
  test2007_227.f
  test2007_256.f
  test2007_258.f
)

set(F03_CONSTPROP_SPECIMENS
  test2007_37.f03
)

set(F03_CONSTPROP_SPECIMENS_FAILING
  test2007_112.f03
  test2007_113.f03
  test2007_114.f03
  test2007_120.f03
  test2007_123.f03
  test2007_134.f03
  test2007_174.f03
  test2007_31.f03
  test2007_41.f03
  test2007_42.f03
  test2007_44.f03
  test2007_47.f03
  test2007_98.f03
)

set(F03_CONSTPROP_SPECIMENS_FAILING_CFG
  test2007_115.f03
  test2007_116.f03
  test2007_130.f03
  test2007_133.f03
  test2007_176.f03
  test2007_30.f03
  test2007_35.f03
  test2007_43.f03
  test2007_45.f03
)


#-------------------------------------------------------------------------------
# Taint analysis tests (disabled in Automake)
set(_taint_conf ${CMAKE_CURRENT_SOURCE_DIR}/taintAnalysisTest.conf)
foreach(specimen IN LISTS TAINT_ANALYSIS_SPECIMENS)
  string(REPLACE "." "_" _name ${specimen})
  add_test(
    NAME gdf_taint_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      INPUT=${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
      ANSWER=${CMAKE_CURRENT_SOURCE_DIR}/${specimen}.ans
      TAINT_ANALYSIS_TEST=$<TARGET_FILE:taintAnalysisTest>
      ${_taint_conf}
      cxxta_${specimen}.passed
  )
  set_tests_properties(gdf_taint_${_name} PROPERTIES DISABLED TRUE)
endforeach()

#-------------------------------------------------------------------------------
# C++ constant propagation (local) tests (disabled in Automake)
foreach(specimen IN LISTS CXX_CONSTPROP_LOCAL_SPECIMENS)
  string(REPLACE "." "_" _name ${specimen})
  add_test(
    NAME gdf_constprop_local_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="\"$<TARGET_FILE:constantPropagationTest>\" -c \"${CMAKE_CURRENT_SOURCE_DIR}/${specimen}\""
      ${_exit_status}
      cxxcpls_${specimen}.passed
  )
  set_tests_properties(gdf_constprop_local_${_name} PROPERTIES DISABLED TRUE)
endforeach()

#-------------------------------------------------------------------------------
# C++ constant propagation tests from CXX test directory
foreach(specimen IN LISTS CXX_CONSTPROP_SPECIMENS)
  string(REPLACE "." "_" _name ${specimen})
  add_test(
    NAME gdf_cxx_constprop_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="\"$<TARGET_FILE:constantPropagationTest>\" -I \"${_cxx_test_dir}\" -c \"${_cxx_test_dir}/${specimen}\""
      ${_exit_status}
      cxxcp_${specimen}.passed
  )
endforeach()

#-------------------------------------------------------------------------------
# C++ constant propagation tests from CXX test directory (disabled in Automake)
set(_cxx_constprop_disabled
  ${FAILING_CXX_CONSTPROP_SPECIMENS}
  ${CXX_CONSTPROP_SPECIMENS_FAILING}
  ${CXX_CONSTPROP_SPECIMENS_HANGING}
)
foreach(specimen IN LISTS _cxx_constprop_disabled)
  string(REPLACE "." "_" _name ${specimen})
  add_test(
    NAME gdf_cxx_constprop_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="\"$<TARGET_FILE:constantPropagationTest>\" -I \"${_cxx_test_dir}\" -c \"${_cxx_test_dir}/${specimen}\""
      ${_exit_status}
      cxxcp_${specimen}.passed
  )
  set_tests_properties(gdf_cxx_constprop_${_name} PROPERTIES DISABLED TRUE)
endforeach()

#-------------------------------------------------------------------------------
# C constant propagation tests from CXX test directory (disabled in Automake)
set(_c_constprop_disabled
  ${FAILING_C_CONSTPROP_SPECIMENS}
  ${C_CONSTPROP_SPECIMENS_FAILING}
)
foreach(specimen IN LISTS _c_constprop_disabled)
  string(REPLACE "." "_" _name ${specimen})
  string(REPLACE ".C" ".c" _c_specimen ${specimen})
  add_test(
    NAME gdf_c_constprop_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="cp \"${_cxx_test_dir}/${specimen}\" \"${_c_specimen}\" && \"$<TARGET_FILE:constantPropagationTest>\" -I \"${_cxx_test_dir}\" -c \"${_c_specimen}\""
      ${_exit_status}
      ccp_${specimen}.passed
  )
  set_tests_properties(gdf_c_constprop_${_name} PROPERTIES DISABLED TRUE)
endforeach()

#-------------------------------------------------------------------------------
# C99 constant propagation tests (disabled in Automake)
set(_c99_constprop_disabled
  ${FAILING_C99_CONSTPROP_SPECIMENS}
  ${C99_CONSTPROP_SPECIMENS_FAILING}
)
foreach(specimen IN LISTS _c99_constprop_disabled)
  string(REPLACE "." "_" _name ${specimen})
  add_test(
    NAME gdf_c99_constprop_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="\"$<TARGET_FILE:constantPropagationTest>\" -I \"${_c99_test_dir}\" -c \"${_c99_test_dir}/${specimen}\""
      ${_exit_status}
      c99cp_${specimen}.passed
  )
  set_tests_properties(gdf_c99_constprop_${_name} PROPERTIES DISABLED TRUE)
endforeach()

#-------------------------------------------------------------------------------
# Fortran constant propagation tests
if(enable-fortran)
  foreach(specimen IN LISTS F77_CONSTPROP_SPECIMENS)
    string(REPLACE "." "_" _name ${specimen})
    if(NOT EXISTS "${_f77_test_dir}/${specimen}")
      message(STATUS "Skipping gdf_f77_constprop_${_name} (missing ${_f77_test_dir}/${specimen})")
      continue()
    endif()
    add_test(
      NAME gdf_f77_constprop_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        CMD="\"$<TARGET_FILE:constantPropagationTest>\" -rose:fortran_std=f77 -c \"${_f77_test_dir}/${specimen}\""
        ${_exit_status}
        f77cp_${specimen}.passed
    )
  endforeach()

  foreach(specimen IN LISTS F77_CONSTPROP_SPECIMENS_FAILING F77_CONSTPROP_SPECIMENS_FAILING_CFG)
    string(REPLACE "." "_" _name ${specimen})
    if(NOT EXISTS "${_f77_test_dir}/${specimen}")
      message(STATUS "Skipping gdf_f77_constprop_${_name} (missing ${_f77_test_dir}/${specimen})")
      continue()
    endif()
    add_test(
      NAME gdf_f77_constprop_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        CMD="\"$<TARGET_FILE:constantPropagationTest>\" -rose:fortran_std=f77 -c \"${_f77_test_dir}/${specimen}\""
        ${_exit_status}
        f77cp_${specimen}.passed
    )
  endforeach()

  foreach(specimen IN LISTS F03_CONSTPROP_SPECIMENS)
    string(REPLACE "." "_" _name ${specimen})
    if(NOT EXISTS "${_f03_test_dir}/${specimen}")
      message(STATUS "Skipping gdf_f03_constprop_${_name} (missing ${_f03_test_dir}/${specimen})")
      continue()
    endif()
    add_test(
      NAME gdf_f03_constprop_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        CMD="\"$<TARGET_FILE:constantPropagationTest>\" -rose:skip_syntax_check -rose:skipfinalCompileStep -rose:fortran_std=f2003 -c \"${_f03_test_dir}/${specimen}\""
        ${_exit_status}
        f03cp_${specimen}.passed
    )
  endforeach()

  foreach(specimen IN LISTS F03_CONSTPROP_SPECIMENS_FAILING F03_CONSTPROP_SPECIMENS_FAILING_CFG)
    string(REPLACE "." "_" _name ${specimen})
    if(NOT EXISTS "${_f03_test_dir}/${specimen}")
      message(STATUS "Skipping gdf_f03_constprop_${_name} (missing ${_f03_test_dir}/${specimen})")
      continue()
    endif()
    add_test(
      NAME gdf_f03_constprop_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        CMD="\"$<TARGET_FILE:constantPropagationTest>\" -rose:skip_syntax_check -rose:skipfinalCompileStep -rose:fortran_std=f2003 -c \"${_f03_test_dir}/${specimen}\""
        ${_exit_status}
        f03cp_${specimen}.passed
    )
  endforeach()
endif()

#-------------------------------------------------------------------------------
# C++ liveness analysis tests (disabled in Automake)
foreach(specimen IN LISTS CXX_LIVENESS_SPECIMENS)
  string(REPLACE "." "_" _name ${specimen})
  add_test(
    NAME gdf_liveness_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="\"$<TARGET_FILE:liveDeadVarAnalysisTest>\" -c \"${CMAKE_CURRENT_SOURCE_DIR}/${specimen}\""
      ${_exit_status}
      cxxla_${specimen}.passed
  )
  set_tests_properties(gdf_liveness_${_name} PROPERTIES DISABLED TRUE)
endforeach()

#-------------------------------------------------------------------------------
# C++ pointer alias analysis tests (disabled in Automake)
foreach(specimen IN LISTS CXX_POINTER_ALIAS_SPECIMENS)
  string(REPLACE "." "_" _name ${specimen})
  add_test(
    NAME gdf_pointer_alias_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="\"$<TARGET_FILE:pointerAliasAnalysisTest>\" -c \"${CMAKE_CURRENT_SOURCE_DIR}/${specimen}\""
      ${_exit_status}
      cxxpa_${specimen}.passed
  )
  set_tests_properties(gdf_pointer_alias_${_name} PROPERTIES DISABLED TRUE)
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_programAnalysisTests_generalDataFlowAnalysisTests_check-cxx-constprop-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=gdf_f03_constprop_test2007_37_f03\;gdf_f77_constprop_test2007_222_f
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programAnalysisTests_generalDataFlowAnalysisTests_check-cxx-constprop-local PROPERTIES DEPENDS "gdf_f03_constprop_test2007_37_f03;gdf_f77_constprop_test2007_222_f" )
add_test(
  NAME nonsmoke_functional_roseTests_programAnalysisTests_generalDataFlowAnalysisTests_check-f03-constprop
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=gdf_f03_constprop_test2007_37_f03
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programAnalysisTests_generalDataFlowAnalysisTests_check-f03-constprop PROPERTIES DEPENDS gdf_f03_constprop_test2007_37_f03 )
add_test(
  NAME nonsmoke_functional_roseTests_programAnalysisTests_generalDataFlowAnalysisTests_check-f77-constprop
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=gdf_f77_constprop_test2007_222_f
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programAnalysisTests_generalDataFlowAnalysisTests_check-f77-constprop PROPERTIES DEPENDS gdf_f77_constprop_test2007_222_f )
