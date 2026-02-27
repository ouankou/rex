# this defines ROSE__CXX_TESTS
# include(${CMAKE_CURRENT_SOURCE_DIR}/Cxx_Testcodes_bk.cmake)

set(_rose_cxx_var_file
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/input_codes/cxx/Makefile_variables)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${_rose_cxx_var_file}")
set(ROSE__CXX_TESTS)
if(EXISTS ${_rose_cxx_var_file})
  file(STRINGS ${_rose_cxx_var_file} _rose_cxx_lines)
  set(_collect_cxx FALSE)
  foreach(line IN LISTS _rose_cxx_lines)
    string(REGEX REPLACE "#.*$" "" _line "${line}")
    string(STRIP "${_line}" _line)
    if(_line STREQUAL "")
      continue()
    endif()
    if(NOT _collect_cxx)
      if(_line MATCHES "^[ \t]*ROSE__CXX_TESTS[ \t]*=")
        set(_collect_cxx TRUE)
        string(REGEX REPLACE "^[ \t]*ROSE__CXX_TESTS[ \t]*=[ \t]*" "" _line "${_line}")
      else()
        continue()
      endif()
    endif()
    if(_collect_cxx)
      if(_line MATCHES "^[ \t]*[A-Za-z0-9_]+[ \t]*=" AND NOT _line MATCHES "^[ \t]*ROSE__CXX_TESTS[ \t]*=")
        break()
      endif()
      string(REPLACE "\\" "" _line "${_line}")
      string(STRIP "${_line}" _line)
      if(_line STREQUAL "")
        continue()
      endif()
      string(REGEX REPLACE "[ \t]+" ";" _tokens "${_line}")
      list(APPEND ROSE__CXX_TESTS ${_tokens})
    endif()
  endforeach()
endif()
list(REMOVE_DUPLICATES ROSE__CXX_TESTS)
set(_rose_cxx_tests_existing)
set(_rose_cxx_tests_missing)
foreach(_test IN LISTS ROSE__CXX_TESTS)
  if(EXISTS "${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests/${_test}")
    list(APPEND _rose_cxx_tests_existing "${_test}")
  else()
    list(APPEND _rose_cxx_tests_missing "${_test}")
  endif()
endforeach()
if(_rose_cxx_tests_missing)
  message(WARNING "ROSE__CXX_TESTS entries missing from Cxx_tests: ${_rose_cxx_tests_missing}")
endif()
set(ROSE__CXX_TESTS "${_rose_cxx_tests_existing}")
set(ROSE__CXX_TESTS "${ROSE__CXX_TESTS}" CACHE INTERNAL "" FORCE)

set(EXAMPLE_TESTCODES_REQUIRED_TO_PASS
  Area-01.C
  X.C
  dynamic_cast.C
  cpp_vector.C
  fstream.C
  inputBug317.C
  inputBug327.C
  inputForLoopLocator.C
  lexPhase2003_01.C
  lulesh.C
  luleshTALC.C
  rex_test2025_template_call.cpp
  rex_test2025_member_template_call.cpp
  rex_test2025_variadic_template_params.cpp
  rex_test2025_template_argument_pack.cpp
  rex_test2026_template_instantiation_basic.cpp
  rex_test2026_template_features.cpp
  rex_test2026_template_decl_wrapper.cpp
  rex_test2026_for_init_scope.cpp
  rex_test2026_axpy_system_headers.cpp
  rex_test2026_stl_incremental.cpp
  rex_test2026_stl_map.cpp
  rex_test2026_stl_meta.cpp
  rex_test2026_stl_regex.cpp
  rex_test2026_stl_tuple.cpp
  rex_test2026_stl_type_traits.cpp
  rex_test2026_stl_variant.cpp
  rex_test2026_stl_vector.cpp
  rex_test2026_deduction_guide.cpp
  rex_test2026_sfinae.cpp
  rex_test2026_dependent_qualification.cpp
  rex_test2026_extern_c_brace_roundtrip.cpp
  rex_test2026_std_filesystem.cpp
  rex_test2026_std_shared_ptr_enable_shared.cpp
  rex_test2026_std_bind_function.cpp
  rex_test2026_std_random.cpp
  rex_test2026_std_variant_visit.cpp
  rex_test2026_namespace_qualification.cpp
  rex_test2025_issue88.cpp
  rex_test2025_issue89_alias.cpp
  rex_test2025_issue90_array_dim.cpp
  rex_test2025_issue91_decl_chain.cpp
  rex_test2025_issue92_redundant_sym.cpp
  rex_test2025_issue93_static_assert_order.cpp
  rex_test2025_issue94_template_arg_spacing.cpp
  rex_test2025_issue107_chrono.cpp
  rex_test2025_issue107_redundant.cpp
  rex_test2025_issue107_enable_if.cpp
  rex_test2025_issue113_instantiation_spelling.cpp
  rex_test2025_issue115_unresolved_lookup.cpp
  rex_test2025_issue115_dependent_member.cpp
  rex_test2025_issue115_qualified_unresolved_lookup.cpp
  rex_test2025_dependent_template_specialization_type.cpp
  rex_test2025_nonreal_type_global_qualifier.cpp
  rex_test2025_extern_c_roundtrip.cpp
  rex_test2025_extern_c_brace_roundtrip.cpp
  rex_test2025_issue123_std_double_colon.cpp
  rex_test2025_issue146_std_iota_quad_colon.cpp
  rex_test2025_issue146_shadowed_function_template.cpp
  rex_test2025_issue148_system_header_mutation.cpp
  rex_test2025_issue149_param_list_unique_name.cpp
  rex_test2025_issue150_name_qualification_map.cpp
  rex_test2025_issue160_axpy_roundtrip.cpp
  rex_test2025_issue163_axpy_roundtrip.cpp
  rex_test2025_issue164_constexpr_const.cpp
  rex_test2025_issue160_axpy_system_headers.cpp
  rex_test2025_issue160_axpy_explicit_template_call.cpp
  rex_test2025_issue160_explicit_empty_template_id.cpp
  rex_test2025_issue160_explicit_class_instantiation.cpp
  rex_test2025_issue160_explicit_struct_instantiation.cpp
  rex_test2025_issue160_extern_class_instantiation.cpp
  rex_test2025_issue160_system_header_instantiation.cpp
  rex_test2025_issue160_system_header_class_instantiation.cpp
  rex_test2025_cfe_error_exit_code.cpp
  rex_test2025_issue124_system_header_specialization_args.cpp
  rex_test2025_issue125_fixup_child_list_warning.cpp
  rex_test2025_issue126_default_template_args.cpp
  rex_test2025_defaulted_special_members_anon_union.cpp
  rex_test2025_issue59_template_template_arg_global_qual.cpp
  rex_test2025_issue60_friend_function_symbol_scope.cpp
  rex_test2025_issue61_function_decl_chain.cpp
  rex_test2025_mangle_qualifiers_no_template_syntax.cpp
  rex_test2026_issue203_template_qualified_private_typedef.cpp
  rex_test2025_issue63_pseudo_dtor_typedef_ptr.cpp
  rex_test2025_issue64_shadow_template_global_qual.cpp
  rex_test2025_issue69_member_class_out_of_line.cpp
  rex_test2025_issue73_token_stream_mapping.cpp
  rex_test2025_issue118_empty_decl.cpp
  rex_test2025_namespace_reopenings_preserve_blocks.cpp
  method-defn-in-tpldecl-0.C
  method-defn-in-tpldecl-1.C
  rose-1431-0.C
  rose-1540-0.C
  rose-1541-0.C
  rose-1701-0.C
  rose-1701-1.C
  rose-1766.C
  rose-1789-001.C
  rose-1814.C
  rose-1834.C
  rose-1839.C
  rose-1846.C
  rose-1860.C
  rose-1962.C
  rose-1998.C
  rose-2000-01.C
  rose-2000-02.C
  rose-2007-01.C
  rose-2007-02.C
  rose-2008.C
  rose-2073.C
  test2001_01.C
  test2001_02.C
  test2001_03.C
  test2001_04.C
  test2001_05.C
  test2001_06.C
  test2001_07.C
  test2001_08.C
  test2001_09.C
  test2001_10.C
  test2001_11.C
  test2001_12.C
  test2001_13.C
  test2001_14.C
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
  test2001_26.C
  test2001_27.C
  test2001_28.C
  test2001_29.C
  test2002_01.C
  test2003_01.C
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
  test2003_15.C
  test2003_16.C
  test2003_17.C
  test2003_18.C
  test2003_19.C
  test2003_20.C
  test2003_21.C
  test2003_23.C
  test2003_24.C
  test2003_25.C
  test2003_26.C
  test2003_27.C
  test2003_28.C
  test2003_29.C
  test2003_30.C
  test2003_31.C
  test2003_32.C
  test2003_35.C
  test2003_36.C
  test2003_37.C
  test2003_38.C
  test2004_01.C
  test2004_02.C
  test2004_03.C
  test2004_04.C
  test2004_05.C
  test2004_06.C
  test2004_07.C
  test2004_08.C
  test2004_09.C
  test2004_10.C
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
  test2004_11.C
  test2004_110.C
  test2004_111.C
  test2004_112.C
  test2004_113.C
  test2004_114.C
  test2004_115.C
  test2004_116.C
  test2004_117.C
  test2004_118.C
  test2004_119.C
  test2004_12.C
  test2004_120.C
  test2004_121.C
  test2004_122.C
  test2004_123.C
  test2004_124.C
  test2004_125.C
  test2004_127.C
  test2004_128.C
  test2004_129.C
  test2004_13.C
  test2004_130.C
  test2004_131.C
  test2004_132.C
  test2004_133.C
  test2004_134.C
  test2004_135.C
  test2004_136.C
  test2004_137.C
  test2004_138.C
  test2004_139.C
  test2004_14.C
  test2004_140.C
  test2004_141.C
  test2004_142.C
  test2004_143.C
  test2004_144.C
  test2004_145.C
  test2004_146.C
  test2004_147.C
  test2004_149.C
  test2004_15.C
  test2004_150.C
  test2004_151.C
  test2004_152.C
  test2004_153.C
  test2004_154.C
  test2004_155.C
  test2004_156.C
  test2004_157.C
  test2004_158.C
  test2004_159.C
  test2004_16.C
  test2004_160.C
  test2004_161.C
  test2004_162.C
  test2004_163.C
  test2004_164.C
  test2004_17.C
  test2004_18.C
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
  test2004_36.C
  test2004_37.C
  test2004_38.C
  test2004_39.C
  test2004_40.C
  test2004_42.C
  test2004_43.C
  test2004_44.C
  test2004_45.C
  test2004_47.C
  test2004_48.C
  test2004_49.C
  test2004_50.C
  test2004_51.C
  test2004_52.C
  test2004_53.C
  test2004_54.C
  test2004_55.C
  test2004_56.C
  test2004_58.C
  test2004_59.C
  test2004_60.C
  test2004_61.C
  test2004_62.C
  test2004_63.C
  test2004_64.C
  test2004_65.C
  test2004_67.C
  test2004_68.C
  test2004_69.C
  test2004_70.C
  test2004_71.C
  test2004_72.C
  test2004_73.C
  test2004_74.C
  test2004_75.C
  test2004_76.C
  test2004_77.C
  test2004_78.C
  test2004_79.C
  test2004_80.C
  test2004_81.C
  test2004_82.C
  test2004_84.C
  test2004_85.C
  test2004_86.C
  test2004_87.C
  test2004_88.C
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
  test2005_04.C
  test2005_05.C
  test2005_06.C
  test2005_07.C
  test2005_08.C
  test2005_09.C
  test2005_10.C
  test2005_100.C
  test2005_101.C
  test2005_103.C
  test2005_104.C
  test2005_105.C
  test2005_106.C
  test2005_107.C
  test2005_108.C
  test2005_109.C
  test2005_11.C
  test2005_110.C
  rex_test2026_issue213_va_start.cpp
  test2005_112.C
  test2005_113.C
  test2005_114.C
  test2005_116.C
  test2005_117.C
  test2005_118.C
  test2005_119.C
  test2005_12.C
  test2005_120.C
  test2005_121.C
  test2005_122.C
  test2005_123.C
  test2005_124.C
  test2005_125.C
  test2005_126.C
  test2005_127.C
  test2005_128.C
  test2005_129.C
  test2005_13.C
  test2005_130.C
  test2005_131.C
  test2005_132.C
  test2005_133.C
  test2005_134.C
  test2005_135.C
  test2005_136.C
  test2005_137.C
  test2005_138.C
  test2005_139.C
  test2005_14.C
  test2005_140.C
  test2005_141.C
  test2005_142.C
  test2005_143.C
  test2005_144.C
  test2005_145.C
  test2005_146.C
  test2005_147.C
  test2005_148.C
  test2005_149.C
  test2005_15.C
  test2005_150.C
  test2005_152.C
  test2005_153.C
  test2005_154.C
  test2005_155.C
  test2005_156.C
  test2005_157.C
  test2005_158.C
  test2005_159.C
  test2005_16.C
  test2005_160.C
  test2005_161.C
  test2005_162.C
  test2005_163.C
  test2005_164.C
  test2005_165.C
  test2005_166.C
  test2005_167.C
  test2005_168.C
  test2005_169.C
  test2005_17.C
  test2005_170.C
  test2005_171.C
  test2005_172.C
  test2005_173.C
  test2005_174.C
  test2005_175.C
  test2005_176.C
  test2005_177.C
  test2005_178.C
  test2005_179.C
  test2005_180.C
  test2005_181.C
  test2005_182.C
  test2005_183.C
  test2005_184.C
  test2005_185.C
  test2005_187.C
  test2005_189.C
  test2005_19.C
  test2005_190.C
  test2005_191.C
  test2005_192.C
  test2005_193.C
  test2005_194.C
  test2005_195.C
  test2005_196.C
  test2005_197.C
  test2005_198.C
  test2005_199.C
  test2005_20.C
  test2005_202.C
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
  test2005_27.C
  test2005_28.C
  test2005_29.C
  test2005_30.C
  test2005_31.C
  test2005_32.C
  test2005_33.C
  test2005_34.C
  test2005_35.C
  test2005_36.C
  test2005_37.C
  test2005_38.C
  test2005_39.C
  test2005_40.C
  test2005_41.C
  test2005_42.C
  test2005_43.C
  test2005_44.C
  test2005_45.C
  test2005_46.C
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
  test2005_63.C
  test2005_64.C
  test2005_65.C
  test2005_66.C
  test2005_67.C
  test2005_68.C
  test2005_69.C
  test2005_70.C
  test2005_71.C
  test2005_72.C
  test2005_73.C
  test2005_74.C
  test2005_75a.C
  test2005_75b.C
  test2005_76.C
  test2005_77.C
  test2005_78.C
  test2005_79.C
  test2005_80.C
  test2005_81.C
  test2005_82.C
  test2005_83.C
  test2005_84.C
  test2005_86.C
  test2005_87.C
  test2005_88.C
  test2005_89.C
  test2005_90.C
  test2005_91.C
  test2005_92.C
  test2005_93.C
  test2005_94.C
  test2005_95.C
  test2005_96.C
  test2005_97.C
  test2005_98.C
  test2005_99.C
  test2006_01.C
  test2006_02.C
  test2006_03.C
  test2006_04.C
  test2006_05.C
  test2006_06.C
  test2006_07.C
  test2006_08.C
  test2006_10.C
  test2006_101.C
  test2006_102.C
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
  test2006_115.C
  test2006_116.C
  test2006_117.C
  test2006_118.C
  test2006_119.C
  test2006_12.C
  test2006_120.C
  test2006_122.C
  test2006_123.C
  test2006_124.C
  test2006_125.C
  test2006_126.C
  test2006_129.C
  test2006_13.C
  test2006_139.C
  test2006_14.C
  test2006_140.C
  test2006_141.C
  test2006_142.C
  test2006_143.C
  test2006_144.C
  test2006_145.C
  test2006_146.C
  test2006_147.C
  test2006_148.C
  test2006_149.C
  test2006_150.C
  test2006_151.C
  test2006_152.C
  test2006_153.C
  test2006_154.C
  test2006_155.C
  test2006_156.C
  test2006_157.C
  test2006_158.C
  test2006_159.C
  test2006_16.C
  test2006_160.C
  test2006_161.C
  test2006_162.C
  test2006_163.C
  test2006_164.C
  test2006_165.C
  test2006_166.C
  test2006_17.C
  test2006_177.C
  test2006_178.C
  test2006_179.C
  test2006_18.C
  test2006_181.C
  test2006_183.C
  test2006_185.C
  test2006_189.C
  test2006_19.C
  test2006_20.C
  test2006_22.C
  test2006_23.C
  test2006_25.C
  test2006_26.C
  test2006_28.C
  test2006_34.C
  test2006_37.C
  test2006_38.C
  test2006_39.C
  test2006_40.C
  test2006_41.C
  test2006_42.C
  test2006_44.C
  test2006_46.C
  test2006_47.C
  test2006_50.C
  test2006_52.C
  test2006_53.C
  test2006_54.C
  test2006_55.C
  test2006_56.C
  test2006_57.C
  test2006_58.C
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
  test2006_78.C
  test2006_79.C
  test2006_80.C
  test2006_81.C
  test2006_82.C
  test2006_83.C
  test2006_84.C
  test2006_85.C
  test2006_86.C
  test2006_87.C
  test2006_88.C
  test2006_89.C
  test2006_90.C
  test2006_91.C
  test2006_92.C
  test2006_93.C
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
  test2007_11.C
  test2007_12.C
  test2007_128.C
  test2007_13.C
  test2007_130.C
  test2007_133.C
  test2007_135.C
  test2007_137.C
  test2007_14.C
  test2007_140.C
  test2007_141.C
  test2007_142.C
  test2007_147.C
  test2007_15.C
  test2007_150.C
  test2007_151.C
  test2007_152.C
  test2007_154.C
  test2007_155.C
  test2007_157.C
  test2007_158.C
  test2007_159.C
  test2007_16.C
  test2007_160.C
  test2007_161.C
  test2007_162.C
  test2007_163.C
  test2007_164.C
  test2007_165.C
  test2007_166.C
  test2007_167.C
  test2007_169.C
  test2007_17.C
  test2007_170.C
  test2007_171.C
  test2007_172.C
  test2007_173.C
  test2007_21.C
  test2007_23.C
  test2007_24.C
  test2007_25.C
  test2007_26.C
  test2007_28.C
  test2007_29.C
  test2007_38.C
  test2007_40.C
  test2007_41.C
  test2007_43.C
  test2007_44.C
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
  test2007_94.C
  test2007_98.C
  test2008_01.C
  test2008_03.C
  test2008_04.C
  test2008_05.C
  test2008_06.C
  test2008_08.C
  test2008_09.C
  test2008_10.C
  test2008_11.C
  test2008_12.C
  test2008_13.C
  test2008_14.C
  test2008_16.C
  test2009_01.C
  test2009_02.C
  test2009_03.C
  test2009_04.C
  test2009_05.C
  test2009_06.C
  test2009_08.C
  test2009_09.C
  test2009_10.C
  test2009_11.C
  test2009_12.C
  test2009_14.C
  test2009_15.C
  test2009_18.C
  test2009_19.C
  test2009_20.C
  test2009_21.C
  test2009_27.C
  test2009_28.C
  test2009_30.C
  test2009_31.C
  test2009_37.C
  test2009_39.C
  test2009_40.C
  test2009_41.C
  test2009_42.C
  test2010_03.C
  test2010_04.C
  test2010_05.C
  test2010_06.C
  test2010_18.C
  test2010_19.C
  test2010_24.C
  test2011_01.C
  test2011_02.C
  test2011_05.C
  test2011_07.C
  test2011_100.C
  test2011_101.C
  test2011_110.C
  test2011_111.C
  test2011_115.C
  test2011_117.C
  test2011_118.C
  test2011_119.C
  test2011_121.C
  test2011_133.C
  test2011_135.C
  test2011_136.C
  test2011_137.C
  test2011_138.C
  test2011_139.C
  test2011_140.C
  test2011_141.C
  test2011_142.C
  test2011_143.C
  test2011_146.C
  test2011_147.C
  test2011_149.C
  test2011_150.C
  test2011_151.C
  test2011_152.C
  test2011_153.C
  test2011_154.C
  test2011_155.C
  test2011_156.C
  test2011_157.C
  test2011_158.C
  test2011_159.C
  test2011_160.C
  test2011_161.C
  test2011_162.C
  test2011_163.C
  test2011_164.C
  test2011_165.C
  test2011_166.C
  test2011_167.C
  test2011_168.C
  test2011_169.C
  test2011_170.C
  test2011_171.C
  test2011_172.C
  test2011_173.C
  test2011_174.C
  test2011_175.C
  test2011_176.C
  test2011_177.C
  test2011_179.C
  test2011_180.C
  test2011_181.C
  test2011_182.C
  test2011_183.C
  test2011_184.C
  test2011_185.C
  test2011_186.C
  test2011_187.C
  test2011_188.C
  test2011_189.C
  test2011_19.C
  test2011_190.C
  test2011_191.C
  test2011_192.C
  test2011_193.C
  test2011_194.C
  test2011_195.C
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
  rex_test2025_issue83.cpp
  rex_test2025_issue84_friend_template_in_class.cpp
  rex_test2025_issue83_redeclarations.cpp
  rex_test2025_issue83_collisions.cpp
  rex_test2025_issue84_function_template.cpp
  test2011_32.C
  test2011_33.C
  test2011_34.C
  test2011_35.C
  test2011_36.C
  test2011_37.C
  test2011_38.C
  test2011_39.C
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
  test2011_66.C
  test2011_67.C
  test2011_68.C
  test2011_69.C
  test2011_70.C
  test2011_71.C
  test2011_72.C
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
  test2011_99.C
  test2012_01.C
  test2012_02.C
  test2012_03.C
  test2012_04.C
  test2012_05.C
  test2012_06.C
  test2012_07.C
  test2012_08.C
  test2012_09.C
  test2012_10.C
  test2012_100.C
  test2012_101.C
  test2012_102.C
  test2012_103.C
  test2012_104.C
  test2012_105.C
  test2012_106.C
  test2012_107.C
  test2012_108.C
  test2012_109.C
  test2012_11.C
  test2012_110.C
  test2012_111.C
  test2012_112.C
  test2012_113.C
  test2012_114.C
  test2012_115.C
  test2012_116.C
  test2012_117.C
  test2012_118.C
  test2012_119.C
  test2012_12.C
  test2012_120.C
  test2012_121.C
  test2012_122.C
  test2012_123.C
  test2012_124.C
  test2012_125.C
  test2012_126.C
  test2012_127.C
  test2012_128.C
  test2012_129.C
  test2012_13.C
  test2012_130.C
  test2012_131.C
  test2012_132.C
  test2012_136.C
  test2012_137.C
  test2012_138.C
  test2012_14.C
  test2012_141.C
  test2012_142.C
  test2012_143.C
  test2012_144.C
  test2012_145.C
  test2012_146.C
  test2012_147.C
  test2012_148.C
  test2012_149.C
  test2012_15.C
  test2012_150.C
  test2012_151.C
  test2012_152.C
  test2012_153.C
  test2012_154.C
  test2012_155.C
  test2012_156.C
  test2012_157.C
  test2012_158.C
  test2012_159.C
  test2012_16.C
  test2012_160.C
  test2012_161.C
  test2012_162.C
  test2012_163.C
  test2012_164.C
  test2012_165.C
  test2012_166.C
  test2012_167.C
  test2012_168.C
  test2012_169.C
  test2012_17.C
  test2012_170.C
  test2012_171.C
  test2012_172.C
  test2012_173.C
  test2012_174.C
  test2012_175.C
  test2012_176.C
  test2012_177.C
  test2012_178.C
  test2012_179.C
  test2012_18.C
  test2012_180.C
  test2012_181.C
  test2012_182.C
  test2012_183.C
  test2012_184.C
  test2012_185.C
  test2012_186.C
  test2012_187.C
  test2012_188.C
  test2012_19.C
  test2012_190.C
  test2012_191.C
  test2012_192.C
  test2012_193.C
  test2012_194.C
  test2012_195.C
  test2012_196.C
  test2012_197.C
  test2012_198.C
  test2012_199.C
  test2012_20.C
  test2012_200.C
  test2012_201.C
  test2012_202.C
  test2012_203.C
  test2012_204.C
  test2012_205.C
  test2012_206.C
  test2012_207.C
  test2012_208.C
  test2012_209.C
  test2012_21.C
  test2012_210.C
  test2012_211.C
  test2012_212.C
  test2012_213.C
  test2012_214.C
  test2012_215.C
  test2012_216.C
  test2012_217.C
  test2012_218.C
  test2012_219.C
  test2012_22.C
  test2012_220.C
  test2012_221.C
  test2012_222.C
  test2012_223.C
  test2012_224.C
  test2012_225.C
  test2012_226.C
  test2012_227.C
  test2012_228.C
  test2012_229.C
  test2012_23.C
  test2012_230.C
  test2012_231.C
  test2012_232.C
  test2012_233.C
  test2012_234.C
  test2012_235.C
  test2012_236.C
  test2012_237.C
  test2012_238.C
  test2012_239.C
  test2012_24.C
  test2012_240.C
  test2012_241.C
  test2012_242.C
  test2012_243.C
  test2012_244.C
  test2012_245.C
  test2012_246a.C
  test2012_246b.C
  test2012_247.C
  test2012_248.C
  test2012_249.C
  test2012_25.C
  test2012_250.C
  test2012_251.C
  test2012_253.C
  test2012_254.C
  test2012_255.C
  test2012_256.C
  test2012_257.C
  test2012_258.C
  test2012_259.C
  test2012_26.C
  test2012_260.C
  test2012_262.C
  test2012_263.C
  test2012_264.C
  test2012_265.C
  test2012_266.C
  test2012_267.C
  test2012_268.C
  test2012_269.C
  test2012_27.C
  test2012_270.C
  test2012_271.C
  test2012_28.C
  test2012_29.C
  test2012_30.C
  test2012_31.C
  test2012_32.C
  test2012_33.C
  test2012_34.C
  test2012_36.C
  test2012_37.C
  test2012_38.C
  test2012_39.C
  test2012_40.C
  test2012_41.C
  test2012_42.C
  test2012_43.C
  test2012_44.C
  test2012_45.C
  test2012_46.C
  test2012_47.C
  test2012_48.C
  test2012_49.C
  test2012_50.C
  test2012_51.C
  test2012_52.C
  test2012_53.C
  test2012_54.C
  test2012_55.C
  test2012_56.C
  test2012_57.C
  test2012_58.C
  test2012_59.C
  test2012_60.C
  test2012_61.C
  test2012_62.C
  test2012_63.C
  test2012_64.C
  test2012_65.C
  test2012_66.C
  test2012_67.C
  test2012_69.C
  test2012_70.C
  test2012_71.C
  test2012_72.C
  test2012_73.C
  test2012_74.C
  test2012_75.C
  test2012_76.C
  test2012_77.C
  test2012_78.C
  test2012_80.C
  test2012_81.C
  test2012_82.C
  test2012_83.C
  test2012_84.C
  test2012_85.C
  test2012_87.C
  test2012_88.C
  test2012_89.C
  test2012_90.C
  test2012_91.C
  test2012_92.C
  test2012_93.C
  test2012_94.C
  test2012_95.C
  test2012_96.C
  test2012_97.C
  test2012_98.C
  test2013_01.C
  test2013_02.C
  test2013_03.C
  test2013_04.C
  test2013_05.C
  test2013_06.C
  test2013_07.C
  test2013_08.C
  test2013_09.C
  test2013_10.C
  test2013_100.C
  test2013_101.C
  test2013_102.C
  test2013_103.C
  test2013_104.C
  test2013_105.C
  test2013_106.C
  test2013_107.C
  test2013_108.C
  test2013_109.C
  test2013_11.C
  test2013_110.C
  test2013_111.C
  test2013_112.C
  test2013_113.C
  test2013_114.C
  test2013_115.C
  test2013_116.C
  test2013_117.C
  test2013_118.C
  test2013_119.C
  test2013_12.C
  test2013_120.C
  test2013_121.C
  test2013_122.C
  test2013_123.C
  test2013_124.C
  test2013_125.C
  test2013_126.C
  test2013_127.C
  test2013_128.C
  test2013_129.C
  test2013_130.C
  test2013_131.C
  test2013_132.C
  test2013_133.C
  test2013_134.C
  test2013_135.C
  test2013_136.C
  test2013_137.C
  test2013_138.C
  test2013_139.C
  test2013_140.C
  test2013_141.C
  test2013_142.C
  test2013_143.C
  test2013_144.C
  test2013_145.C
  test2013_146.C
  test2013_147.C
  test2013_148.C
  test2013_149.C
  test2013_150.C
  test2013_151.C
  test2013_152.C
  test2013_153.C
  test2013_154.C
  test2013_155.C
  test2013_156.C
  test2013_157.C
  test2013_158.C
  test2013_159.C
  test2013_16.C
  test2013_160.C
  test2013_161.C
  test2013_162.C
  test2013_163.C
  test2013_164.C
  test2013_165.C
  test2013_166.C
  test2013_167.C
  test2013_168.C
  test2013_169.C
  test2013_17.C
  test2013_170.C
  test2013_171.C
  test2013_172.C
  test2013_173.C
  test2013_174.C
  test2013_175.C
  test2013_176.C
  test2013_177.C
  test2013_178.C
  test2013_179.C
  test2013_18.C
  test2013_180.C
  test2013_181.C
  test2013_182.C
  test2013_183.C
  test2013_184.C
  test2013_186.C
  test2013_187.C
  test2013_188.C
  test2013_189.C
  test2013_190.C
  test2013_191.C
  test2013_192.C
  test2013_193.C
  test2013_194.C
  test2013_195.C
  test2013_196.C
  test2013_198.C
  test2013_199.C
  test2013_20.C
  test2013_200.C
  test2013_201.C
  test2013_202.C
  test2013_203.C
  test2013_204.C
  test2013_205.C
  test2013_206.C
  test2013_207.C
  test2013_208.C
  test2013_209.C
  test2013_21.C
  test2013_210.C
  test2013_211.C
  test2013_212.C
  test2013_213.C
  test2013_214.C
  test2013_215.C
  test2013_216.C
  test2013_217.C
  test2013_218.C
  test2013_219.C
  test2013_22.C
  test2013_220.C
  test2013_221.C
  test2013_222.C
  test2013_223.C
  test2013_225.C
  test2013_226.C
  test2013_227.C
  test2013_228.C
  test2013_229.C
  test2013_23.C
  test2013_230.C
  test2013_231.C
  test2013_232.C
  test2013_238.C
  test2013_239.C
  test2013_243.C
  test2013_244.C
  test2013_247.C
  test2013_25.C
  test2013_250.C
  test2013_251.C
  test2013_252.C
  test2013_253.C
  test2013_255.C
  test2013_256.C
  test2013_258.C
  test2013_259.C
  test2013_26.C
  test2013_261.C
  test2013_27.C
  test2013_271.C
  test2013_272.C
  test2013_273.C
  test2013_274.C
  test2013_275.C
  test2013_276.C
  test2013_277.C
  test2013_278.C
  test2013_279.C
  test2013_28.C
  test2013_280.C
  test2013_281.C
  test2013_282.C
  test2013_283.C
  test2013_284.C
  test2013_285.C
  test2013_286.C
  test2013_287.C
  test2013_288.C
  test2013_289.C
  test2013_29.C
  test2013_290.C
  test2013_291.C
  test2013_292.C
  test2013_293.C
  test2013_294.C
  test2013_295.C
  test2013_296.C
  test2013_300.C
  test2013_301.C
  test2013_302.C
  test2013_303.C
  test2013_304.C
  test2013_305.C
  test2013_306.C
  test2013_307.C
  test2013_308.C
  test2013_31.C
  test2013_310.C
  test2013_311.C
  test2013_312.C
  test2013_313.C
  test2013_314.C
  test2013_315.C
  test2013_316.C
  test2013_317.C
  test2013_318.C
  test2013_319.C
  test2013_32.C
  test2013_320.C
  test2013_33.C
  test2013_34.C
  test2013_36.C
  test2013_37.C
  test2013_38.C
  test2013_39.C
  test2013_40.C
  test2013_44.C
  test2013_46.C
  test2013_47.C
  test2013_48.C
  test2013_50.C
  test2013_51.C
  test2013_52.C
  test2013_53.C
  test2013_54.C
  test2013_55.C
  test2013_56.C
  test2013_57.C
  test2013_58.C
  test2013_59.C
  test2013_60.C
  test2013_61.C
  test2013_64.C
  test2013_65.C
  test2013_66.C
  test2013_67.C
  test2013_68.C
  test2013_69.C
  test2013_70.C
  test2013_71.C
  test2013_72.C
  test2013_73.C
  test2013_74.C
  test2013_75.C
  test2013_76.C
  test2013_77.C
  test2013_78.C
  test2013_79.C
  test2013_80.C
  test2013_81.C
  test2013_82.C
  test2013_83.C
  test2013_84.C
  test2013_85.C
  test2013_86.C
  test2013_87.C
  test2013_88.C
  test2013_89.C
  test2013_90.C
  test2013_91.C
  test2013_92.C
  test2013_93.C
  test2013_94.C
  test2013_95.C
  test2013_96.C
  test2013_97.C
  test2013_98.C
  test2013_99.C
  test2013_milind_01.C
  test2014_01.C
  test2014_02.C
  test2014_03.C
  test2014_04.C
  test2014_05.C
  test2014_06.C
  test2014_07.C
  test2014_08.C
  test2014_09.C
  test2014_10.C
  test2014_100.C
  test2014_101.C
  test2014_102.C
  test2014_107.C
  test2014_109.C
  test2014_11.C
  test2014_112.C
  test2014_114.C
  test2014_115.C
  test2014_116.C
  test2014_117.C
  test2014_118.C
  test2014_119.C
  test2014_12.C
  test2014_120.C
  test2014_121.C
  test2014_122.C
  test2014_123.C
  test2014_124.C
  test2014_125.C
  test2014_126.C
  test2014_127.C
  test2014_128.C
  test2014_129.C
  test2014_13.C
  test2014_130.C
  test2014_131.C
  test2014_132.C
  test2014_133.C
  test2014_134.C
  test2014_135.C
  test2014_136.C
  test2014_137.C
  test2014_139.C
  test2014_140.C
  test2014_141.C
  test2014_142.C
  test2014_143.C
  test2014_144.C
  test2014_145.C
  test2014_146.C
  test2014_147.C
  test2014_148.C
  test2014_149.C
  test2014_150.C
  test2014_151.C
  test2014_152.C
  test2014_153.C
  test2014_154.C
  test2014_155.C
  test2014_156.C
  test2014_157.C
  test2014_159.C
  test2014_16.C
  test2014_160.C
  test2014_161.C
  test2014_162.C
  test2014_163.C
  test2014_165.C
  test2014_166.C
  test2014_167.C
  test2014_168.C
  test2014_169.C
  test2014_17.C
  test2014_170.C
  test2014_171.C
  test2014_172.C
  test2014_174.C
  test2014_176.C
  test2014_177.C
  test2014_178.C
  test2014_179.C
  test2014_18.C
  test2014_182.C
  test2014_183.C
  test2014_184.C
  test2014_185.C
  test2014_186.C
  test2014_189.C
  test2014_19.C
  test2014_190.C
  test2014_191.C
  test2014_192.C
  test2014_193.C
  test2014_194.C
  test2014_195.C
  test2014_196.C
  test2014_197.C
  test2014_198.C
  test2014_199.C
  test2014_20.C
  test2014_200.C
  test2014_201.C
  test2014_202.C
  test2014_203.C
  test2014_204.C
  test2014_206.C
  test2014_207.C
  test2014_208.C
  test2014_209.C
  test2014_21.C
  test2014_210.C
  test2014_211.C
  test2014_212.C
  test2014_215.C
  test2014_216.C
  test2014_217.C
  test2014_218.C
  test2014_219.C
  test2014_220.C
  test2014_221.C
  test2014_222.C
  test2014_223.C
  test2014_224.C
  test2014_225.C
  test2014_226.C
  test2014_230.C
  test2014_231.C
  test2014_232.C
  test2014_233.C
  test2014_234.C
  test2014_235.C
  test2014_24.C
  test2014_240.C
  test2014_241.C
  test2014_242.C
  test2014_243.C
  test2014_26.C
  test2014_27.C
  test2014_28.C
  test2014_29.C
  test2014_30.C
  test2014_31.C
  test2014_32.C
  test2014_33.C
  test2014_34.C
  test2014_35.C
  test2014_36.C
  test2014_37.C
  test2014_38.C
  test2014_39.C
  test2014_40.C
  test2014_41.C
  test2014_43.C
  test2014_45.C
  test2014_47.C
  test2014_48.C
  test2014_49.C
  test2014_50.C
  test2014_53.C
  test2014_54.C
  test2014_55.C
  test2014_56.C
  test2014_57.C
  test2014_58.C
  test2014_60.C
  test2014_61.C
  test2014_62.C
  test2014_67.C
  test2014_68.C
  test2014_70.C
  test2014_74.C
  test2014_75.C
  test2014_76.C
  test2014_79.C
  test2014_80.C
  test2014_82.C
  test2014_83.C
  test2014_84.C
  test2014_85.C
  test2014_86.C
  test2014_87.C
  test2014_88.C
  test2014_89.C
  test2014_90.C
  test2014_91.C
  test2014_92.C
  test2014_93.C
  test2014_94.C
  test2014_95.C
  test2014_96.C
  test2014_97.C
  test2014_98.C
  test2014_99.C
  test2015_01.C
  test2015_02.C
  test2015_03.C
  test2015_04.C
  test2015_05.C
  test2015_07.C
  test2015_08.C
  test2015_09.C
  test2015_106.C
  test2015_107.C
  test2015_108.C
  test2015_110.C
  test2015_111.C
  test2015_112.C
  test2015_113.C
  test2015_114.C
  test2015_117.C
  test2015_118.C
  test2015_119.C
  test2015_122.C
  test2015_123.C
  test2015_124.C
  test2015_125.C
  test2015_126.C
  test2015_128.C
  test2015_130.C
  test2015_131.C
  test2015_132.C
  test2015_133.C
  test2015_134.C
  test2015_135.C
  test2015_136.C
  test2015_137.C
  test2015_138.C
  test2015_139.C
  test2015_140.C
  test2015_141.C
  test2015_142.C
  test2015_143.C
  test2015_144.C
  test2015_145.C
  test2015_146.C
  test2015_150.C
  test2015_151.C
  test2015_152.C
  test2015_153.C
  test2015_154.C
  test2015_20.C
  test2015_21.C
  test2015_22.C
  test2015_24.C
  test2015_26.C
  test2015_27.C
  test2015_28.C
  test2015_29.C
  test2015_30.C
  test2015_32.C
  test2015_33.C
  test2015_34.C
  test2015_35.C
  test2015_36.C
  test2015_40.C
  test2015_43.C
  test2015_44.C
  test2015_45.C
  test2015_50.C
  test2015_52.C
  test2015_57.C
  test2015_58.C
  test2015_59.C
  test2015_60.C
  test2015_61.C
  test2015_70.C
  test2015_76.C
  test2015_77.C
  test2015_78.C
  test2015_79.C
  test2015_80.C
  test2015_81.C
  test2015_85.C
  test2015_86.C
  test2015_89.C
  test2016_01.C
  test2016_03.C
  test2016_04.C
  test2016_05.C
  test2016_06.C
  test2016_07.C
  test2016_08.C
  test2016_09.C
  test2016_10.C
  test2016_11.C
  test2016_12.C
  test2016_13.C
  test2016_14.C
  test2016_19.C
  test2016_20.C
  test2016_21.C
  test2016_25.C
  test2016_26.C
  test2016_27.C
  test2016_28.C
  test2016_29.C
  test2016_30.C
  test2016_31.C
  test2016_32.C
  test2016_33.C
  test2016_36.C
  test2016_37.C
  test2016_38.C
  test2016_40.C
  test2016_41.C
  test2016_42.C
  test2016_43.C
  test2016_44.C
  test2016_45.C
  test2016_46.C
  test2016_47.C
  test2016_49.C
  test2016_50.C
  test2016_51.C
  test2016_53.C
  test2016_57.C
  test2016_59.C
  test2016_60.C
  test2016_61.C
  test2016_62.C
  test2016_63.C
  test2016_64.C
  test2016_65.C
  test2016_66.C
  test2016_67.C
  test2016_68.C
  test2016_69.C
  test2016_70.C
  test2016_71.C
  test2016_80.C
  test2016_81.C
  test2016_82.C
  test2016_83.C
  test2016_85.C
  test2016_86.C
  test2016_87.C
  test2016_88.C
  test2016_91.C
  test2016_93.C
  test2017_01.C
  test2017_02.C
  test2017_03.C
  test2017_04.C
  test2017_09.C
  test2017_10.C
  test2017_100.C
  test2017_101.C
  test2017_102.C
  test2017_103.C
  test2017_104.C
  test2017_105.C
  test2017_106.C
  test2017_107.C
  test2017_11.C
  test2017_13.C
  test2017_14.C
  test2017_15.C
  test2017_16.C
  test2017_17.C
  test2017_18.C
  test2017_19.C
  test2017_21.C
  test2017_22.C
  test2017_23.C
  test2017_30.C
  test2017_31.C
  test2017_32.C
  test2017_35.C
  test2017_36.C
  test2017_37.C
  test2017_38.C
  test2017_39.C
  test2017_40.C
  test2017_41.C
  test2017_42.C
  test2017_43.C
  test2017_45.C
  test2017_46.C
  test2017_47.C
  test2017_48.C
  test2017_49.C
  test2017_52.C
  test2017_53.C
  test2017_54.C
  test2017_55.C
  test2017_56.C
  test2017_57.C
  test2017_58.C
  test2017_60.C
  test2017_61.C
  test2017_62.C
  test2017_64.C
  test2017_65.C
  test2017_66.C
  test2017_68.C
  test2017_69.C
  test2017_70.C
  test2017_71.C
  test2017_72.C
  test2017_73.C
  test2017_74.C
  test2017_75.C
  test2017_76.C
  test2017_77.C
  test2017_78.C
  test2017_79.C
  test2017_80.C
  test2017_81.C
  test2017_82.C
  test2017_83.C
  test2017_84.C
  test2017_87.C
  test2017_88.C
  test2017_89.C
  test2017_90.C
  test2017_91.C
  test2017_92.C
  test2017_93.C
  test2017_94.C
  test2017_95.C
  test2017_96.C
  test2017_97.C
  test2017_98.C
  test2018_01.C
  test2018_02.C
  test2018_03.C
  test2018_04.C
  test2018_05.C
  test2018_06.C
  test2018_07.C
  test2018_08.C
  test2018_09.C
  test2018_10.C
  test2018_11.C
  test2018_12.C
  test2018_13.C
  test2018_14.C
  test2018_15.C
  test2018_16.C
  test2018_17.C
  test2018_18.C
  test2018_19.C
  test2018_20.C
  test2018_21.C
  test2018_24.C
  test2018_27.C
  test2018_29.C
  test2018_30.C
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
  test2019_01.C
  test2019_02.C
  test2019_03.C
  test2019_04.C
  test2019_05.C
  test2019_06.C
  test2019_07.C
  test2019_08.C
  test2019_09.C
  test2019_10.C
  test2019_11.C
  test2019_12.C
  test2019_13.C
  test2019_14.C
  test2019_15.C
  test2019_16.C
  test2019_17.C
  test2019_18.C
  test2019_19.C
  test2019_20.C
  test2019_21.C
  test2019_22.C
  test2019_23.C
  test2019_24.C
  test2020_01.C
  test2020_02.C
  test2020_03.C
  test2020_04.C
  test2020_05.C
  test2020_06.C
  test2020_07.C
  test2020_08.C
  test2020_09.C
  test2020_10.C
  test2020_11.C
  test2020_12.C
  test2020_13.C
  test2020_14.C
  test2020_15.C
  test2020_16.C
  test2020_17.C
  test2020_18.C
  test2020_19.C
  test2020_20.C
  test2020_21.C
  test2020_22.C
  test2020_23.C
  test2020_24.C
  test2020_25.C
  test2020_26.C
  test2020_27.C
  test2020_28.C
  test2020_29.C
  test2020_30.C
  test2020_31.C
  test2020_32.C
  test2020_33.C
  test2020_34.C
  test2020_37.C
  test2020_38.C
  test2020_39.C
  test2020_40.C
  test2020_41.C
  test2020_42.C
  test2020_43.C
  test2020_44.C
  test2020_45.C
  test2020_46.C
  test2020_47.C
  test2020_49.C
  test2020_50.C
  test2020_51.C
  test2020_52.C
  test2020_53.C
  test2020_54.C
  test2020_55.C
  test2020_56.C
  test2020_57.C
  test2020_58.C
  test2020_59.C
  test2020_60.C
  test2020_61.C
  test2020_62.C
  test2020_63.C
  test2020_64.C
  test2020_65.C
  test2020_66.C
  test2020_67.C
  test2020_68.C
  test2020_69.C
  test2020_70.C
  test2020_71.C
  test2020_72.C
  test2020_73.C
  test2020_74.C
  test2020_75.C
  test2020_76.C
  test2020_77.C
  test2020_78.C
  test2020_79.C
  test2020_80.C
  test2020_82.C
  test2020_83.C
  test2021_01.C
  test2021_02.C
  test2021_03.C
  test2021_04.C
  test2021_05.C
  test2021_06.C
  test2021_07.C
  test2021_08.C
  test2021_09.C
  test2021_10.C
  test2021_11.C
  test2021_12.C
  test2021_13.C
  test2021_14.C
  test2021_15.C
  test2021_16.C
  test2021_17.C
  test2022_01.C
  test2022_02.C
  test2022_03.C
  test2022_04.C
  test2022_05.C
  test2022_06.C
  test2022_07.C
  test2022_08.C
  test2022_09.C
  test2022_10.C
  test2022_11.C
  test2022_12.C
  test2022_13.C
  test2022_14.C
  test2022_15.C
  test2022_16.C
  test2022_17.C
  test2022_22.C
  test2022_23.C
  test2022_24.C
  test2022_25.C
  test_CplusplusMacro_Cpp.C
  test_int_lexemes.C
  test_int_lexemes_donot_pass.C
  test_simple_int.C
  test_wchars.C
  unparse-typedef-ctor.C
  rex_test2025_issue99.cpp
  rex_test2025_issue99_recursive.cpp
  rex_test2025_issue99_uaf.cpp
  test2025_issue84_friend_template_in_class.cpp
  test2009_24.C
  test2014_108.C
  test2014_205.C
  test2014_239.C
  test2015_99.C
  test2016_34.C
  test2016_48.C
  test2016_54.C
  test2016_55.C
  test2016_56.C
  test2017_26.C
  test2017_51.C
  test2016_96.C
  test2014_111.C
  test2014_187.C
  test2014_187_work_around.C
  test2014_188.C
  test2014_236.C
  test2014_238.C
  test2015_115.C
  test2015_116.C
  test2015_148.C
  test2016_15.C
  test2016_16.C
  test2016_17.C
  test2016_18.C
  test2016_89.C
  test2017_50.C
  test2004_83.C
  certSignalExample.C
  c++trick.C
  lexPhase2003_02.C
  longFile.C
  math.C
  restrict_template_parameter.C
  test2004_25.C
  test2004_35.C
  test2005_115.C
  test2005_151.C
  test2005_188.C
  test2005_200.C
  test2005_201.C
  test2005_204.C
  test2005_205.C
  test2005_85.C
  test2006_09.C
  test2006_11.C
  test2006_127.C
  test2006_128.C
  test2006_130.C
  test2006_131.C
  test2006_138.C
  test2006_15.C
  test2006_167.C
  test2006_168.C
  test2006_169.C
  test2006_170.C
  test2006_171.C
  test2006_172.C
  test2006_173.C
  test2006_174.C
  test2006_175.C
  test2006_176.C
  test2006_180.C
  test2006_182.C
  test2006_184.C
  test2006_186.C
  test2006_187.C
  test2006_188.C
  test2006_21.C
  test2006_24.C
  test2006_27.C
  test2006_33.C
  test2006_35.C
  test2006_36.C
  test2006_49.C
  test2006_60.C
  test2006_64.C
  test2007_100.C
  test2007_101.C
  test2007_104.C
  test2007_105.C
  test2007_106.C
  test2007_107.C
  test2007_108.C
  test2007_109.C
  test2007_110.C
  test2007_111.C
  test2007_112.C
  test2007_113.C
  test2007_114.C
  test2007_115.C
  test2007_116.C
  test2007_117.C
  test2007_118.C
  test2007_119.C
  test2007_120.C
  test2007_121.C
  test2007_122.C
  test2007_123.C
  test2007_124.C
  test2007_125.C
  test2007_126.C
  test2007_127.C
  test2007_129.C
  test2007_131.C
  test2007_132.C
  test2007_134.C
  test2007_136.C
  test2007_138.C
  test2007_139.C
  test2007_143.C
  test2007_144.C
  test2007_145.C
  test2007_146.C
  test2007_148.C
  test2007_149.C
  test2007_153.C
  test2007_156.C
  test2007_168.C
  test2007_174.C
  test2007_175.C
  test2007_176.C
  test2007_18.C
  test2007_19.C
  test2007_20.C
  test2007_22.C
  test2007_27.C
  test2007_30.C
  test2007_31.C
  test2007_32.C
  test2007_33.C
  test2007_34.C
  test2007_36.C
  test2007_37.C
  test2007_39.C
  test2007_42.C
  test2007_45.C
  test2007_54.C
  test2007_56.C
  test2007_58.C
  test2007_60.C
  test2007_61.C
  test2007_62.C
  test2007_63.C
  test2007_64.C
  test2007_65.C
  test2007_66.C
  test2007_67.C
  test2007_68.C
  test2007_69.C
  test2007_71.C
  test2007_73.C
  test2007_74.C
  test2007_76.C
  test2007_79.C
  test2007_80.C
  test2007_86.C
  test2007_87.C
  test2007_88.C
  test2007_89.C
  test2007_91.C
  test2007_92.C
  test2007_93.C
  test2007_95.C
  test2007_96.C
  test2007_97.C
  test2007_99.C
  test2008_07.C
  test2008_15.C
  test2009_07.C
  test2009_13.C
  test2009_17.C
  test2009_22.C
  test2009_25.C
  test2009_26.C
  test2009_29.C
  test2009_32.C
  test2009_33.C
  test2009_34.C
  test2009_35.C
  test2009_36.C
  test2009_38.C
  test2009_43.C
  test2009_44.C
  test2009_45.C
  test2009_46.C
  test2009_47.C
  test2010_02.C
  test2010_07.C
  test2010_09.C
  test2010_10.C
  test2010_11.C
  test2010_12.C
  test2010_15.C
  test2010_16.C
  test2010_17.C
  test2010_20.C
  test2010_21.C
  test2010_22.C
  test2010_23.C
  test2010_26.C
  test2010_27.C
  test2011_06.C
  test2011_08.C
  test2011_09.C
  test2011_102.C
  test2011_103.C
  test2011_104.C
  test2011_105.C
  test2011_106.C
  test2011_107.C
  test2011_108.C
  test2011_109.C
  test2011_10.C
  test2011_112.C
  test2011_11.C
  test2011_122.C
  test2011_123.C
  test2011_124.C
  test2011_125.C
  test2011_126.C
  test2011_127.C
  test2011_128.C
  test2011_129.C
  test2011_12.C
  test2011_130.C
  test2011_131.C
  test2011_132.C
  test2011_134.C
  test2011_13.C
  test2011_144.C
  test2011_145.C
  test2011_148.C
  test2011_14.C
  test2011_15.C
  test2011_16.C
  test2011_178.C
  test2011_17.C
  test2011_18.C
  test2011_196.C
  test2011_197.C
  test2011_198.C
  test2011_199.C
  test2011_200.C
  test2011_201.C
  test2011_202.C
  test2011_203.C
  test2012_133.C
  test2012_134.C
  test2012_135.C
  test2012_139.C
  test2012_140.C
  test2012_189.C
  test2012_252.C
  test2012_261.C
  test2012_272.C
  test2012_273.C
  test2012_274.C
  test2012_275.C
  test2012_276.C
  test2012_277.C
  test2012_278.C
  test2012_35.C
  test2012_68.C
  test2012_79.C
  test2012_86.C
  test2013_13.C
  test2013_14.C
  test2013_15.C
  test2013_185.C
  test2013_197.C
  test2013_19.C
  test2013_245.C
  test2013_248.C
  test2013_249.C
  test2013_24.C
  test2013_254.C
  test2013_257.C
  test2013_260.C
  test2013_262.C
  test2013_263.C
  test2013_264.C
  test2013_265.C
  test2013_266.C
  test2013_267.C
  test2013_268.C
  test2013_269.C
  test2013_309.C
  test2013_30.C
  test2013_321.C
  test2013_322.C
  test2013_323.C
  test2013_35.C
  test2013_41.C
  test2013_42.C
  test2013_43.C
  test2013_45.C
  test2013_49.C
  test2013_62.C
  test2014_138.C
  test2014_14.C
  test2014_158.C
  test2014_15.C
  test2014_164.C
  test2014_175.C
  test2014_180.C
  test2014_181.C
  test2014_213.C
  test2014_214.C
  test2014_227.C
  test2014_228.C
  test2014_229.C
  test2014_22.C
  test2014_23.C
  test2014_25.C
  test2014_42.C
  test2014_44.C
  test2014_46.C
  test2014_51.C
  test2014_52.C
  test2014_59.C
  test2014_63.C
  test2014_64.C
  test2014_65.C
  test2014_66.C
  test2014_69.C
  test2015_109.C
  test2015_25.C
  test2015_72_icpc.C
  test2015_82.C
  test2015_83.C
  test2015_84.C
  test2016_35.C
  test2016_52.C
  test2017_05.C
  test2017_08.C
  test2017_12.C
  test2017_20.C
  test2017_24.C
  test2017_33.C
  test2017_34.C
  test2017_67.C
  test2017_85.C
  test2017_86.C
  test2017_99.C
  test2018_22.C
  test2018_23.C
  test2018_25.C
  test2018_26.C
  test2018_28.C
  test2018_31.C
  test2019_25.C
  test2020_35.C
  test2020_36.C
  test_gnu.C
  test2017_44.C
  vector_cpp.C
)
set(EXAMPLE_TESTCODES_REQUIRED_TO_PASS_DISABLED
  certSignalExample.C
  c++trick.C
  lexPhase2003_02.C
  longFile.C
  math.C
  restrict_template_parameter.C
  test2004_25.C
  test2004_35.C
  test2005_115.C
  test2005_151.C
  test2005_188.C
  test2005_200.C
  test2005_201.C
  test2005_204.C
  test2005_205.C
  test2005_85.C
  test2006_09.C
  test2006_11.C
  test2006_127.C
  test2006_128.C
  test2006_130.C
  test2006_131.C
  test2006_138.C
  test2006_15.C
  test2006_167.C
  test2006_168.C
  test2006_169.C
  test2006_170.C
  test2006_171.C
  test2006_172.C
  test2006_173.C
  test2006_174.C
  test2006_175.C
  test2006_176.C
  test2006_180.C
  test2006_182.C
  test2006_184.C
  test2006_186.C
  test2006_187.C
  test2006_188.C
  test2006_21.C
  test2006_24.C
  test2006_27.C
  test2006_33.C
  test2006_35.C
  test2006_36.C
  test2006_49.C
  test2006_60.C
  test2006_64.C
  test2007_100.C
  test2007_101.C
  test2007_104.C
  test2007_105.C
  test2007_106.C
  test2007_107.C
  test2007_108.C
  test2007_110.C
  test2007_111.C
  test2007_112.C
  test2007_113.C
  test2007_114.C
  test2007_115.C
  test2007_116.C
  test2007_117.C
  test2007_118.C
  test2007_119.C
  test2007_120.C
  test2007_121.C
  test2007_122.C
  test2007_123.C
  test2007_124.C
  test2007_126.C
  test2007_127.C
  test2007_129.C
  test2007_131.C
  test2007_132.C
  test2007_134.C
  test2007_136.C
  test2007_138.C
  test2007_139.C
  test2007_143.C
  test2007_144.C
  test2007_145.C
  test2007_146.C
  test2007_148.C
  test2007_149.C
  test2007_153.C
  test2007_156.C
  test2007_168.C
  test2007_174.C
  test2007_175.C
  test2007_176.C
  test2007_18.C
  test2007_19.C
  test2007_20.C
  test2007_22.C
  test2007_27.C
  test2007_30.C
  test2007_31.C
  test2007_32.C
  test2007_33.C
  test2007_34.C
  test2007_36.C
  test2007_37.C
  test2007_39.C
  test2007_42.C
  test2007_45.C
  test2007_54.C
  test2007_56.C
  test2007_58.C
  test2007_60.C
  test2007_61.C
  test2007_62.C
  test2007_63.C
  test2007_64.C
  test2007_65.C
  test2007_66.C
  test2007_67.C
  test2007_68.C
  test2007_69.C
  test2007_71.C
  test2007_73.C
  test2007_74.C
  test2007_79.C
  test2007_80.C
  test2007_86.C
  test2007_87.C
  test2007_88.C
  test2007_89.C
  test2007_91.C
  test2007_92.C
  test2007_93.C
  test2007_95.C
  test2007_96.C
  test2007_97.C
  test2007_99.C
  test2008_07.C
  test2009_07.C
  test2009_13.C
  test2009_17.C
  test2009_22.C
  test2009_26.C
  test2009_29.C
  test2009_32.C
  test2009_33.C
  test2009_34.C
  test2009_35.C
  test2009_36.C
  test2009_38.C
  test2009_43.C
  test2009_44.C
  test2009_45.C
  test2009_46.C
  test2009_47.C
  test2010_07.C
  test2010_09.C
  test2010_10.C
  test2010_11.C
  test2010_12.C
  test2010_15.C
  test2010_16.C
  test2010_17.C
  test2010_20.C
  test2010_21.C
  test2010_22.C
  test2010_23.C
  test2010_26.C
  test2011_06.C
  test2011_08.C
  test2011_09.C
  test2011_102.C
  test2011_103.C
  test2011_104.C
  test2011_105.C
  test2011_106.C
  test2011_107.C
  test2011_108.C
  test2011_109.C
  test2011_10.C
  test2011_112.C
  test2011_11.C
  test2011_122.C
  test2011_123.C
  test2011_124.C
  test2011_125.C
  test2011_126.C
  test2011_127.C
  test2011_128.C
  test2011_129.C
  test2011_12.C
  test2011_130.C
  test2011_131.C
  test2011_134.C
  test2011_13.C
  test2011_144.C
  test2011_145.C
  test2011_148.C
  test2011_14.C
  test2011_15.C
  test2011_178.C
  test2011_17.C
  test2011_18.C
  test2011_196.C
  test2011_197.C
  test2011_198.C
  test2011_199.C
  test2011_200.C
  test2011_201.C
  test2011_202.C
  test2011_203.C
  test2012_133.C
  test2012_134.C
  test2012_135.C
  test2012_139.C
  test2012_140.C
  test2012_189.C
  test2012_252.C
  test2012_261.C
  test2012_272.C
  test2012_273.C
  test2012_274.C
  test2012_275.C
  test2012_276.C
  test2012_277.C
  test2012_278.C
  test2012_35.C
  test2012_68.C
  test2012_79.C
  test2012_86.C
  test2013_14.C
  test2013_185.C
  test2013_197.C
  test2013_19.C
  test2013_245.C
  test2013_248.C
  test2013_249.C
  test2013_254.C
  test2013_257.C
  test2013_260.C
  test2013_262.C
  test2013_263.C
  test2013_264.C
  test2013_265.C
  test2013_266.C
  test2013_267.C
  test2013_268.C
  test2013_309.C
  test2013_321.C
  test2013_322.C
  test2013_323.C
  test2013_35.C
  test2013_41.C
  test2013_42.C
  test2013_43.C
  test2013_45.C
  test2013_49.C
  test2013_62.C
  test2014_138.C
  test2014_14.C
  test2014_158.C
  test2014_15.C
  test2014_164.C
  test2014_175.C
  test2014_180.C
  test2014_181.C
  test2014_213.C
  test2014_214.C
  test2014_227.C
  test2014_228.C
  test2014_229.C
  test2014_22.C
  test2014_23.C
  test2014_25.C
  test2014_42.C
  test2014_44.C
  test2014_46.C
  test2014_51.C
  test2014_52.C
  test2014_59.C
  test2014_63.C
  test2014_64.C
  test2014_66.C
  test2014_69.C
  test2015_109.C
  test2015_25.C
  test2015_72_icpc.C
  test2015_82.C
  test2015_83.C
  test2015_84.C
  test2016_35.C
  test2016_52.C
  test2017_05.C
  test2017_08.C
  test2017_12.C
  test2017_20.C
  test2017_33.C
  test2017_34.C
  test2017_67.C
  test2017_85.C
  test2017_86.C
  test2018_22.C
  test2018_23.C
  test2018_25.C
  test2018_26.C
  test2018_28.C
  test2018_31.C
  test2019_25.C
  test2020_35.C
  test2020_36.C
  test_gnu.C
  test2017_44.C
  vector_cpp.C
)
set(TESTCODE_CURRENTLY_FAILING
  c++trick.C
  certSignalExample.C
  lexPhase2003_02.C
  longFile.C
  math.C
  restrict_template_parameter.C
  rose-1701-1.C
  rose-1962.C
  test2004_25.C
  test2004_35.C
  test2004_85.C
  test2005_06.C
  test2005_115.C
  test2005_151.C
  test2005_188.C
  test2005_200.C
  test2005_201.C
  test2005_204.C
  test2005_205.C
  test2005_42.C
  test2005_50.C
  test2005_85.C
  test2006_08.C
  test2006_09.C
  test2006_11.C
  test2006_127.C
  test2006_128.C
  test2006_130.C
  test2006_131.C
  test2006_138.C
  test2006_15.C
  test2006_167.C
  test2006_168.C
  test2006_169.C
  test2006_170.C
  test2006_171.C
  test2006_172.C
  test2006_173.C
  test2006_174.C
  test2006_175.C
  test2006_176.C
  test2006_180.C
  test2006_182.C
  test2006_184.C
  test2006_186.C
  test2006_187.C
  test2006_188.C
  test2006_20.C
  test2006_21.C
  test2006_24.C
  test2006_27.C
  test2006_33.C
  test2006_35.C
  test2006_36.C
  test2006_37.C
  test2006_49.C
  test2006_60.C
  test2006_64.C
  test2007_100.C
  test2007_101.C
  test2007_104.C
  test2007_105.C
  test2007_106.C
  test2007_107.C
  test2007_108.C
  test2007_110.C
  test2007_111.C
  test2007_112.C
  test2007_113.C
  test2007_114.C
  test2007_115.C
  test2007_116.C
  test2007_117.C
  test2007_118.C
  test2007_119.C
  test2007_120.C
  test2007_121.C
  test2007_122.C
  test2007_123.C
  test2007_124.C
  test2007_126.C
  test2007_127.C
  test2007_129.C
  test2007_131.C
  test2007_132.C
  test2007_134.C
  test2007_136.C
  test2007_138.C
  test2007_139.C
  test2007_143.C
  test2007_144.C
  test2007_145.C
  test2007_146.C
  test2007_148.C
  test2007_149.C
  test2007_153.C
  test2007_156.C
  test2007_168.C
  test2007_174.C
  test2007_175.C
  test2007_176.C
  test2007_18.C
  test2007_19.C
  test2007_20.C
  test2007_22.C
  test2007_27.C
  test2007_30.C
  test2007_31.C
  test2007_32.C
  test2007_33.C
  test2007_34.C
  test2007_36.C
  test2007_37.C
  test2007_39.C
  test2007_42.C
  test2007_45.C
  test2007_51.C
  test2007_52.C
  test2007_54.C
  test2007_58.C
  test2007_60.C
  test2007_61.C
  test2007_62.C
  test2007_63.C
  test2007_64.C
  test2007_65.C
  test2007_66.C
  test2007_67.C
  test2007_68.C
  test2007_69.C
  test2007_71.C
  test2007_73.C
  test2007_74.C
  test2007_79.C
  test2007_80.C
  test2007_86.C
  test2007_87.C
  test2007_88.C
  test2007_89.C
  test2007_91.C
  test2007_92.C
  test2007_93.C
  test2007_95.C
  test2007_96.C
  test2007_97.C
  test2007_99.C
  test2008_07.C
  test2009_07.C
  test2009_13.C
  test2009_17.C
  test2009_21.C
  test2009_22.C
  test2009_26.C
  test2009_29.C
  test2009_32.C
  test2009_33.C
  test2009_34.C
  test2009_35.C
  test2009_36.C
  test2009_38.C
  test2009_43.C
  test2009_44.C
  test2009_45.C
  test2009_46.C
  test2009_47.C
  test2010_07.C
  test2010_09.C
  test2010_10.C
  test2010_11.C
  test2010_12.C
  test2010_15.C
  test2010_16.C
  test2010_17.C
  test2010_20.C
  test2010_21.C
  test2010_22.C
  test2010_23.C
  test2010_26.C
  test2011_01.C
  test2011_02.C
  test2011_05.C
  test2011_06.C
  test2011_07.C
  test2011_08.C
  test2011_09.C
  test2011_10.C
  test2011_102.C
  test2011_103.C
  test2011_104.C
  test2011_105.C
  test2011_106.C
  test2011_107.C
  test2011_108.C
  test2011_109.C
  test2011_11.C
  test2011_112.C
  test2011_12.C
  test2011_122.C
  test2011_123.C
  test2011_124.C
  test2011_125.C
  test2011_126.C
  test2011_127.C
  test2011_128.C
  test2011_129.C
  test2011_13.C
  test2011_130.C
  test2011_131.C
  test2011_134.C
  test2011_14.C
  test2011_141.C
  test2011_142.C
  test2011_144.C
  test2011_145.C
  test2011_148.C
  test2011_15.C
  test2011_17.C
  test2011_178.C
  test2011_18.C
  test2011_191.C
  test2011_196.C
  test2011_197.C
  test2011_198.C
  test2011_199.C
  test2011_200.C
  test2011_201.C
  test2011_202.C
  test2011_203.C
  test2012_133.C
  test2012_134.C
  test2012_135.C
  test2012_139.C
  test2012_140.C
  test2012_189.C
  test2012_252.C
  test2012_261.C
  test2012_272.C
  test2012_273.C
  test2012_274.C
  test2012_275.C
  test2012_276.C
  test2012_277.C
  test2012_278.C
  test2012_35.C
  test2012_57.C
  test2012_59.C
  test2012_68.C
  test2012_79.C
  test2012_81.C
  test2012_86.C
  test2012_90.C
  test2013_14.C
  test2013_16.C
  test2013_164.C
  test2013_17.C
  test2013_185.C
  test2013_19.C
  test2013_195.C
  test2013_197.C
  test2013_243.C
  test2013_245.C
  test2013_248.C
  test2013_249.C
  test2013_254.C
  test2013_257.C
  test2013_260.C
  test2013_262.C
  test2013_263.C
  test2013_264.C
  test2013_265.C
  test2013_266.C
  test2013_267.C
  test2013_268.C
  test2013_304.C
  test2013_309.C
  test2013_321.C
  test2013_322.C
  test2013_323.C
  test2013_35.C
  test2013_40.C
  test2013_41.C
  test2013_42.C
  test2013_43.C
  test2013_45.C
  test2013_49.C
  test2013_62.C
  test2013_84.C
  test2013_85.C
  test2013_96.C
  test2014_100.C
  test2014_101.C
  test2014_102.C
  test2014_118.C
  test2014_120.C
  test2014_121.C
  test2014_138.C
  test2014_14.C
  test2014_15.C
  test2014_158.C
  test2014_159.C
  test2014_164.C
  test2014_175.C
  test2014_180.C
  test2014_181.C
  test2014_211.C
  test2014_213.C
  test2014_214.C
  test2014_22.C
  test2014_227.C
  test2014_228.C
  test2014_229.C
  test2014_23.C
  test2014_25.C
  test2014_42.C
  test2014_44.C
  test2014_46.C
  test2014_51.C
  test2014_52.C
  test2014_59.C
  test2014_63.C
  test2014_64.C
  test2014_66.C
  test2014_69.C
  test2014_99.C
  test2015_109.C
  test2015_126.C
  test2015_135.C
  test2015_136.C
  test2015_25.C
  test2015_72_icpc.C
  test2015_82.C
  test2015_83.C
  test2015_84.C
  test2016_20.C
  test2016_35.C
  test2016_52.C
  test2016_82.C
  test2017_05.C
  test2017_08.C
  test2017_12.C
  test2017_20.C
  test2017_31.C
  test2017_32.C
  test2017_33.C
  test2017_34.C
  test2017_44.C
  test2017_46.C
  test2017_47.C
  test2017_67.C
  test2017_85.C
  test2017_86.C
  test2018_01.C
  test2018_22.C
  test2018_23.C
  test2018_25.C
  test2018_26.C
  test2018_28.C
  test2018_31.C
  test2019_25.C
  test2020_35.C
  test2020_36.C
  test_gnu.C
  vector_cpp.C
  test2004_117.C
  test2007_56.C
  test2013_178.C
  test2013_182.C
)

set(EXAMPLE_TESTCODES_REQUIRED_TO_PASS "${EXAMPLE_TESTCODES_REQUIRED_TO_PASS}" CACHE INTERNAL "")
set(EXAMPLE_TESTCODES_REQUIRED_TO_PASS_DISABLED "${EXAMPLE_TESTCODES_REQUIRED_TO_PASS_DISABLED}" CACHE INTERNAL "")
set(TESTCODE_CURRENTLY_FAILING "${TESTCODE_CURRENTLY_FAILING}" CACHE INTERNAL "")
set(CXX_TESTCODES_TO_RUN ${EXAMPLE_TESTCODES_REQUIRED_TO_PASS})
list(REMOVE_ITEM CXX_TESTCODES_TO_RUN ${TESTCODE_CURRENTLY_FAILING})
list(REMOVE_ITEM CXX_TESTCODES_TO_RUN ${REX_FRONTEND_ERROR_TESTS})
# Custom test uses -isystem to force system-header classification.
list(REMOVE_ITEM CXX_TESTCODES_TO_RUN
  rex_test2025_issue148_system_header_mutation.cpp
  )
set(CXX_TESTCODES_ALWAYS_RUN
  test2013_09.C)
list(APPEND CXX_TESTCODES_TO_RUN ${CXX_TESTCODES_ALWAYS_RUN})
list(REMOVE_DUPLICATES CXX_TESTCODES_TO_RUN)

set(CXX_TESTCODES_GNUXX03
  # Legacy access declarations (e.g., A::foo;) require pre-C++11 mode.
  test2020_31.C
)
list(REMOVE_ITEM CXX_TESTCODES_TO_RUN ${CXX_TESTCODES_GNUXX03})

set(CXX_TESTCODES_GNUXX17
  # These use C++17 library/features (variant/filesystem/*_v traits/CTAD).
  rex_test2026_template_features.cpp
  rex_test2026_stl_map.cpp
  rex_test2026_stl_meta.cpp
  rex_test2026_stl_type_traits.cpp
  rex_test2026_stl_variant.cpp
  rex_test2026_deduction_guide.cpp
  rex_test2026_std_filesystem.cpp
  rex_test2026_std_variant_visit.cpp
)
list(REMOVE_ITEM CXX_TESTCODES_TO_RUN ${CXX_TESTCODES_GNUXX17})

set(CXX_TESTCODES_DELAYED_TEMPLATE_PARSING
  test2016_85.C
)
list(REMOVE_ITEM CXX_TESTCODES_TO_RUN ${CXX_TESTCODES_DELAYED_TEMPLATE_PARSING})


set(ROSE_DISABLED_TESTCODES ${EXAMPLE_TESTCODES_REQUIRED_TO_PASS_DISABLED})
set(ROSE_FAILING_TESTCODES ${TESTCODE_CURRENTLY_FAILING})

set(ROSE_FLAGS
  -w -rose:verbose 0
  -rose:skip_unparse_asm_commands
  -rose:skip_unparse_cc_commands
  )

set(_rose_flags_base ${ROSE_FLAGS})
set(_rose_flags_default ${_rose_flags_base} -std=gnu++14)
set(ROSE_FLAGS ${_rose_flags_default})

set(ROSE_TEST_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/rose_output)
file(MAKE_DIRECTORY ${ROSE_TEST_OUTPUT_DIR})
set(ROSE_OUTPUT_TO_BUILD_TESTCODES
  test2007_109.C
  test2007_125.C
)

foreach(file_to_test ${TESTCODE_CURRENTLY_FAILING})
  compile_test(${file_to_test} "Cxx_tests_failing")
  rex_compile_test_name(_test_name "Cxx_tests_failing" ${file_to_test})
  rex_lock_test_for_input(${_test_name} ${file_to_test})
  set_tests_properties(${_test_name} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()

set(ROSE_FLAGS ${_rose_flags_base} -std=gnu++03)
foreach(file_to_test ${CXX_TESTCODES_GNUXX03})
  compile_test(${file_to_test} "Cxx_tests")
  rex_compile_test_name(_test_name "Cxx_tests" ${file_to_test})
  rex_lock_test_for_input(${_test_name} ${file_to_test})
  set_tests_properties(${_test_name} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()
set(ROSE_FLAGS ${_rose_flags_default})

set(ROSE_FLAGS ${_rose_flags_base} -std=gnu++17)
foreach(file_to_test ${CXX_TESTCODES_GNUXX17})
  compile_test(${file_to_test} "Cxx_tests")
  rex_compile_test_name(_test_name "Cxx_tests" ${file_to_test})
  rex_lock_test_for_input(${_test_name} ${file_to_test})
  set_tests_properties(${_test_name} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()
set(ROSE_FLAGS ${_rose_flags_default})

set(ROSE_FLAGS ${_rose_flags_default} -rex:clang:delayed-template-parsing)
foreach(file_to_test ${CXX_TESTCODES_DELAYED_TEMPLATE_PARSING})
  compile_test(${file_to_test} "Cxx_tests")
  rex_compile_test_name(_test_name "Cxx_tests" ${file_to_test})
  rex_lock_test_for_input(${_test_name} ${file_to_test})
  set_tests_properties(${_test_name} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()
set(ROSE_FLAGS ${_rose_flags_default})


foreach(file_to_test ${CXX_TESTCODES_TO_RUN})
  compile_test(${file_to_test} "Cxx_tests")
  rex_compile_test_name(_test_name "Cxx_tests" ${file_to_test})
  rex_lock_test_for_input(${_test_name} ${file_to_test})
  set_tests_properties(${_test_name} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()

# Automake TEST_Objects_UnfoldedConstants targets (non-default "make check").
set(_unfolded_constants_inputs
  ${EXAMPLE_TESTCODES_REQUIRED_TO_PASS}
  ${TESTCODE_CURRENTLY_FAILING})
set(_unfolded_constants_tests ${_unfolded_constants_inputs})
list(FILTER _unfolded_constants_tests INCLUDE REGEX "\\.C$")
list(REMOVE_DUPLICATES _unfolded_constants_tests)

foreach(file_to_test ${_unfolded_constants_tests})
  set(_unfolded_target "${file_to_test}.unfoldedConstants-o")
  set(_unfolded_flags ${_rose_flags_base})
  if(file_to_test IN_LIST CXX_TESTCODES_GNUXX03)
    list(APPEND _unfolded_flags -std=gnu++03)
  elseif(file_to_test IN_LIST CXX_TESTCODES_GNUXX17)
    list(APPEND _unfolded_flags -std=gnu++17)
  else()
    list(APPEND _unfolded_flags -std=gnu++14)
  endif()
  if(file_to_test IN_LIST CXX_TESTCODES_DELAYED_TEMPLATE_PARSING)
    list(APPEND _unfolded_flags -rex:clang:delayed-template-parsing)
  endif()
  add_test(
    NAME ${_unfolded_target}
    COMMAND $<TARGET_FILE:testTranslatorUnfoldedConstants>
      ${_unfolded_flags} ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  set_tests_properties(${_unfolded_target} PROPERTIES LABELS Cxx_tests)
  rex_lock_test_for_input(${_unfolded_target} ${file_to_test})
endforeach()

# Automake special-rule tests (non-list targets and custom command lines).
add_test(
  NAME Cxx_tests_method_defn_in_tpldecl_0_passed
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c "\"$@\" && test -e method-defn-in-tpldecl-0"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/method-defn-in-tpldecl-0.C
    -o method-defn-in-tpldecl-0
)
set_tests_properties(Cxx_tests_method_defn_in_tpldecl_0_passed PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(Cxx_tests_method_defn_in_tpldecl_0_passed method-defn-in-tpldecl-0.C)

add_test(
  NAME Cxx_tests_method_defn_in_tpldecl_1_passed
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c "\"$@\" && test -e method-defn-in-tpldecl-1"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/method-defn-in-tpldecl-1.C
    -o method-defn-in-tpldecl-1
)
set_tests_properties(Cxx_tests_method_defn_in_tpldecl_1_passed PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(Cxx_tests_method_defn_in_tpldecl_1_passed method-defn-in-tpldecl-1.C)

add_test(
  NAME Cxx_tests_test2005_56_special
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    "-DTEST_STRING_MACRO=\"Thu Apr 14 08:18:33 PDT 2005\""
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2005_56.C
    -o test2005_56.o
)
set_tests_properties(Cxx_tests_test2005_56_special PROPERTIES LABELS Cxx_tests DISABLED TRUE)
rex_lock_test_for_input(Cxx_tests_test2005_56_special test2005_56.C)

add_test(
  NAME Cxx_tests_test2010_02_special
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_BINARY_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2010_02.C
    -o test2010_02.o
)
set_tests_properties(Cxx_tests_test2010_02_special PROPERTIES LABELS Cxx_tests DISABLED TRUE)
rex_lock_test_for_input(Cxx_tests_test2010_02_special test2010_02.C)

add_test(
  NAME Cxx_tests_test2010_07_special
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_BINARY_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2010_07.C
    -o test2010_07.o
)
set_tests_properties(Cxx_tests_test2010_07_special PROPERTIES LABELS Cxx_tests DISABLED TRUE)
rex_lock_test_for_input(Cxx_tests_test2010_07_special test2010_07.C)

add_test(
  NAME Cxx_tests_cpp_test2010_07
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_CXX_COMPILER}
    -E -I${CMAKE_BINARY_DIR} ${ROSE_INCLUDE_FLAGS}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2010_07.C
)
set_tests_properties(Cxx_tests_cpp_test2010_07 PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_test_unparse_includes_option
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -rose:unparse_includes -rose:verbose 0
    -I${CMAKE_BINARY_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2013_257.C
    -o test2013_257_unparse_headers.o
)
set_tests_properties(Cxx_tests_test_unparse_includes_option PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(Cxx_tests_test_unparse_includes_option test2013_257.C)

add_test(
  NAME unparse_includes_option
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    --edg:no_warnings -w -rose:verbose 0 -rose:skip_unparse_asm_commands
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -rose:unparse_includes -rose:verbose 0
    -I${CMAKE_BINARY_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2013_257.C
    -o test2013_257_unparse_headers.o
)
set_tests_properties(unparse_includes_option PROPERTIES LABELS Cxx_tests DEPENDS Cxx_tests_test_unparse_includes_option)
rex_lock_test_for_input(unparse_includes_option test2013_257.C)

add_test(
  NAME Cxx_tests_test_unparse_includes_option_with_link
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -rose:unparse_includes -rose:verbose 0
    -I${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/test2013_260.C
    -o test2013_260_unparse_headers
)
set_tests_properties(Cxx_tests_test_unparse_includes_option_with_link PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(Cxx_tests_test_unparse_includes_option_with_link test2013_260.C)

add_test(
  NAME Cxx_tests_common_configure_test_with_link_part_1
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "cp ${CMAKE_CURRENT_SOURCE_DIR}/conftest.C conftest.C && \"$@\""
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    conftest.C
    -o conftest_configure_test_part_1
)
set_tests_properties(Cxx_tests_common_configure_test_with_link_part_1 PROPERTIES LABELS Cxx_tests RESOURCE_LOCK rose_output_conftest)

add_test(
  NAME common_configure_test_with_link_part_1
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    --edg:no_warnings -w -rose:verbose 0 -rose:skip_unparse_asm_commands
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/conftest.C
    -o conftest_configure_test_part_1
)
set_tests_properties(common_configure_test_with_link_part_1 PROPERTIES LABELS Cxx_tests RESOURCE_LOCK rose_output_conftest DEPENDS Cxx_tests_common_configure_test_with_link_part_1)

add_test(
  NAME Cxx_tests_common_configure_test_with_link_part_2
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "cp ${CMAKE_CURRENT_SOURCE_DIR}/conftest_2.C conftest_2.C && \"$@\""
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    conftest_2.C
    -o conftest_configure_test_part_2
)
set_tests_properties(Cxx_tests_common_configure_test_with_link_part_2 PROPERTIES LABELS Cxx_tests RESOURCE_LOCK rose_output_conftest)

add_test(
  NAME Cxx_tests_common_configure_test_with_link_part_3
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "cp ${CMAKE_CURRENT_SOURCE_DIR}/conftest_3.C conftest_3.C && \"$@\""
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    conftest_3.C
    -o conftest_configure_test_part_3
)
set_tests_properties(Cxx_tests_common_configure_test_with_link_part_3 PROPERTIES LABELS Cxx_tests RESOURCE_LOCK rose_output_conftest DISABLED TRUE)

add_test(
  NAME Cxx_tests_suppress_variable_declaration_normalization
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:suppress_variable_declaration_normalization
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_multible_variables_in_variable_declaration.C
)
set_tests_properties(Cxx_tests_suppress_variable_declaration_normalization PROPERTIES LABELS Cxx_tests DISABLED TRUE)
rex_lock_test_for_input(
  Cxx_tests_suppress_variable_declaration_normalization
  test_multible_variables_in_variable_declaration.C)

add_test(
  NAME Cxx_tests_testOption
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 0 -nostdinc
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2015_08.C
)
set_tests_properties(Cxx_tests_testOption PROPERTIES LABELS Cxx_tests DISABLED TRUE)
rex_lock_test_for_input(Cxx_tests_testOption test2015_08.C)

add_test(
  NAME Cxx_tests_testTemplateExample_1
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTemplates>
    ${ROSE_FLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/test2015_32.C
)
set_tests_properties(Cxx_tests_testTemplateExample_1 PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(Cxx_tests_testTemplateExample_1 test2015_32.C)

add_test(
  NAME Cxx_tests_testTemplateExample_2
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTemplates>
    ${ROSE_FLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/test2015_33.C
)
set_tests_properties(Cxx_tests_testTemplateExample_2 PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(Cxx_tests_testTemplateExample_2 test2015_33.C)

add_test(
  NAME Cxx_tests_testTemplateExample_3
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTemplates>
    ${ROSE_FLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/test2015_34.C
)
set_tests_properties(Cxx_tests_testTemplateExample_3 PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(Cxx_tests_testTemplateExample_3 test2015_34.C)

add_test(
  NAME Cxx_tests_testTemplateExample_4
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTemplates>
    ${ROSE_FLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/test2015_35.C
)
set_tests_properties(Cxx_tests_testTemplateExample_4 PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(Cxx_tests_testTemplateExample_4 test2015_35.C)

add_test(
  NAME Cxx_tests_testTemplateExample_5
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTemplates>
    ${ROSE_FLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/test2015_36.C
)
set_tests_properties(Cxx_tests_testTemplateExample_5 PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(Cxx_tests_testTemplateExample_5 test2015_36.C)

add_test(
  NAME Cxx_tests_testUsingBadCommandline
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "echo \"class A { int x\\; float y\\;}\\; int main() { int x=0\\; return x\\; }\" > roseTestProgram.C && \"$@\""
    dummy
    $<TARGET_FILE:${translator}>
    -DthisIsOK -D thisIsAnError -rose:verbose 0
    roseTestProgram.C
)
set_tests_properties(Cxx_tests_testUsingBadCommandline PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_testBuildUsingNoPath
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "echo \"class A { int x\\; float y\\;}\\; int main() { int x=0\\; return x\\; }\" > roseTestProgram.C && \"$@\""
    dummy
    $<TARGET_FILE:${translator}>
    -rose:verbose 0
    roseTestProgram.C
)
set_tests_properties(Cxx_tests_testBuildUsingNoPath PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_testBuildUsingRelativePath
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "echo \"class A { int x\\; float y\\;}\\; int main() { int x=0\\; return x\\; }\" > roseTestProgram.C && \"$@\""
    dummy
    $<TARGET_FILE:${translator}>
    -rose:verbose 0
    ../Cxx_tests/roseTestProgram.C
)
set_tests_properties(Cxx_tests_testBuildUsingRelativePath PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_testBuildUsingAbsolutePath
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "echo \"class A { int x\\; float y\\;}\\; int main() { int x=0\\; return x\\; }\" > roseTestProgram.C && \"$@\" `pwd`/roseTestProgram.C"
    dummy
    $<TARGET_FILE:${translator}>
    -rose:verbose 0
)
set_tests_properties(Cxx_tests_testBuildUsingAbsolutePath PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_test_ansi_and_nonansi_modes
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "\"$@\" -rose:verbose 0 -ansi -c ${CMAKE_CURRENT_SOURCE_DIR}/test_ansi.C && \"$@\" -rose:verbose 0 -c ${CMAKE_CURRENT_SOURCE_DIR}/test_nonansi.C"
    dummy
    $<TARGET_FILE:${translator}>
)
set_tests_properties(Cxx_tests_test_ansi_and_nonansi_modes PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_multipleDeclarationsBug
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/markus_1a.C
    ${CMAKE_CURRENT_SOURCE_DIR}/markus_1b.C
)
set_tests_properties(Cxx_tests_multipleDeclarationsBug PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_test_MPI_LAM
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 0
    -I/home/dquinlan/ROSE/ROSE_TESTS/MPI_header_tests/MPI_header_tests_64bit/LAM_MPI/include
    -c /home/dquinlan/ROSE/ROSE_TESTS/MPI_header_tests/MPI_header_tests_64bit/tests/input.C
)
set_tests_properties(Cxx_tests_test_MPI_LAM PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_test_multiple_files
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 1 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test2012_246a.C
    ${CMAKE_CURRENT_SOURCE_DIR}/test2012_246b.C
)
set_tests_properties(Cxx_tests_test_multiple_files PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME CXXTEST_test2012_246a
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    --edg:no_warnings -w -rose:verbose 0 -rose:skip_unparse_asm_commands
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2012_246a.C
)
set_tests_properties(CXXTEST_test2012_246a PROPERTIES LABELS Cxx_tests)

add_test(
  NAME Cxx_tests_test_multiple_files_2
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test2018_01.C
    ${CMAKE_CURRENT_SOURCE_DIR}/test2018_02.C
)
set_tests_properties(Cxx_tests_test_multiple_files_2 PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_test_multiple_files_3
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test2019_30a.C
    ${CMAKE_CURRENT_SOURCE_DIR}/test2019_30b.C
)
set_tests_properties(Cxx_tests_test_multiple_files_3 PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_test_multiple_files_4
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test2019_42a.C
    ${CMAKE_CURRENT_SOURCE_DIR}/test2019_42b.C
)
set_tests_properties(Cxx_tests_test_multiple_files_4 PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_test_bug_980
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp_vector.C
)
set_tests_properties(Cxx_tests_test_bug_980 PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_testParser
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testParser>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2001_01.C
)
set_tests_properties(Cxx_tests_testParser PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_performance_timing
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testGraphGeneration>
    -w -rose:verbose 3 -rose:skip_unparse_asm_commands
    -rose:skipAstConsistancyTests -rose:skip_commentsAndDirectives
    -rose:skip_unparse -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/testPerformance.C
)
set_tests_properties(Cxx_tests_performance_timing PROPERTIES LABELS Cxx_tests DISABLED TRUE)

add_test(
  NAME Cxx_tests_test_pdf_generation
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c "\"$@\" -w -rose:verbose 0 -rose:skip_unparse_asm_commands -rose:skipAstConsistancyTests -rose:skip_commentsAndDirectives -rose:skip_unparse -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/test2004_77.C && ls -l *.pdf && rm -f *.pdf"
    dummy
    $<TARGET_FILE:testPDFGeneration>
)
set_tests_properties(Cxx_tests_test_pdf_generation PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(Cxx_tests_test_pdf_generation test2004_77.C)

add_test(
  NAME Cxx_tests_hpctoolkit
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND hpcrun -e WALLCLOCK --
    $<TARGET_FILE:${translator}>
    -w -rose:verbose 0 -rose:skip_unparse_asm_commands
    -rose:skipAstConsistancyTests -rose:skip_commentsAndDirectives
    -rose:skip_unparse -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/testPerformance.C
)
set_tests_properties(Cxx_tests_hpctoolkit PROPERTIES LABELS Cxx_tests DISABLED TRUE)

# Issue 99: Friend function template lookup requires error-tolerant mode.
add_test(
  NAME Cxx_tests_rex_test2025_issue99
  COMMAND
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rex:clang:continue-on-error ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue99.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue99 PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(Cxx_tests_rex_test2025_issue99 rex_test2025_issue99.cpp)

# Issue 91: Declaration chains must not emit "firstNondefiningDeclaration == NULL"
rex_compile_test_name(_issue91_test "Cxx_tests" rex_test2025_issue91_decl_chain.cpp)
set_tests_properties(${_issue91_test} PROPERTIES
  FAIL_REGULAR_EXPRESSION "firstNondefiningDeclaration == NULL"
)

# Issue 92: Must not emit "Redundant symbol removed...from symbol table"
rex_compile_test_name(_issue92_test "Cxx_tests" rex_test2025_issue92_redundant_sym.cpp)
set_tests_properties(${_issue92_test} PROPERTIES
  FAIL_REGULAR_EXPRESSION "Redundant symbol removed"
)

# Issue 145: Defaulted/deleted special members must not trigger placeholder warnings.
rex_compile_test_name(_issue145_test "Cxx_tests" rex_test2025_defaulted_special_members_anon_union.cpp)
set_tests_properties(${_issue145_test} PROPERTIES FAIL_REGULAR_EXPRESSION "Cannot resolve Field member;using placeholder" )

# Issue 150: Name-qualification map should not emit overwrite or default-scope warnings.
rex_compile_test_name(_issue150_test "Cxx_tests" rex_test2025_issue150_name_qualification_map.cpp)
set_tests_properties(${_issue150_test} PROPERTIES FAIL_REGULAR_EXPRESSION "In addToNameMap\(\): Allow this for replacement now!;Default reached in SgScopeStatement::isNamedScope" )

# Issue 160: Preserve file-scope static and implicit member access in unparse output.
string(CONCAT rex_test2025_issue160_axpy_roundtrip_unparse_command
  "\"$@\" && "
  "grep -E \"^[[:space:]]*static[[:space:]]+const[[:space:]]+unsigned[[:space:]]+long[[:space:]]+kElements\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "grep -E \"^[[:space:]]*static[[:space:]]+void[[:space:]]+axpy[[:space:]]*[(]\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "grep -E \"^[[:space:]]*static[[:space:]]+double[[:space:]]+checksum[[:space:]]*[(]\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "! grep -E \"(^|[^[:alnum:]_])::Array\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "! grep -E \"(^|[^[:alnum:]_])this([^[:alnum:]_]|$)\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "! grep -E \"checksum[[:space:]]*[(][[:space:]]*[(][[:space:]]*y[[:space:]]*[.][[:space:]]*get_data\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "! grep -E \",[[:space:]]*[(][[:space:]]*x[[:space:]]*[.][[:space:]]*get_data\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "! grep -E \",[[:space:]]*[(][[:space:]]*y[[:space:]]*[.][[:space:]]*get_data\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "! grep -E \",[[:space:]]*[(][[:space:]]*y[[:space:]]*[.][[:space:]]*size\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue160_axpy_roundtrip.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue160_axpy_roundtrip_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue160_axpy_roundtrip_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_axpy_roundtrip.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue160_axpy_roundtrip_unparse PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue160_axpy_roundtrip_unparse
  rex_test2025_issue160_axpy_roundtrip.cpp)

# Issue 163: Round-trip axpy.cpp must match exactly.
string(CONCAT rex_test2025_issue163_axpy_roundtrip_unparse_command
  "\"$@\" && "
  "cmp -s ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue163_axpy_roundtrip.cpp "
  "rose_rex_test2025_issue163_axpy_roundtrip.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue163_axpy_roundtrip.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue163_axpy_roundtrip_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue163_axpy_roundtrip_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue163_axpy_roundtrip.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue163_axpy_roundtrip_unparse PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue163_axpy_roundtrip_unparse
  rex_test2025_issue163_axpy_roundtrip.cpp)

# Issue 164: Preserve explicit const on constexpr declarations.
string(CONCAT rex_test2025_issue164_constexpr_const_unparse_command
  "\"$@\" && "
  "cmp -s ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue164_constexpr_const.cpp "
  "rose_rex_test2025_issue164_constexpr_const.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c "
  "rose_rex_test2025_issue164_constexpr_const.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue164_constexpr_const_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue164_constexpr_const_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue164_constexpr_const.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue164_constexpr_const_unparse PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue164_constexpr_const_unparse
  rex_test2025_issue164_constexpr_const.cpp)

# Issue 160: Preserve explicit qualifiers, casts, and list-init for std headers.
string(CONCAT rex_test2025_issue160_axpy_system_headers_unparse_command
  "\"$@\" && "
  "cmp -s ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_axpy_system_headers.cpp "
  "rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"constexpr[[:space:]]+std::size_t[[:space:]]+kElements\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"1u[[:space:]]*<<[[:space:]]*10\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"std::array<[^>]*>[[:space:]]+x[[:space:]]*\\{\\}\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"std::array<[^>]*>[[:space:]]+y[[:space:]]*\\{\\}\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "! grep -E \"std::iota[[:space:]]*<\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"std::iota[[:space:]]*\\([[:space:]]*x\\.begin\\(\\)\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"std::iota[[:space:]]*\\([[:space:]]*y\\.begin\\(\\)\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"axpy[[:space:]]*\\([[:space:]]*a[[:space:]]*,[[:space:]]*x\\.data\\(\\)[[:space:]]*,[[:space:]]*y\\.data\\(\\)[[:space:]]*,[[:space:]]*y\\.size\\(\\)[[:space:]]*\\)\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"static_cast[[:space:]]*<[^>]*double[^>]*>\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"std::fabs[[:space:]]*\\(\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"const[[:space:]]+double[[:space:]]+a[[:space:]]*=[[:space:]]*2\\.5\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"1e-9\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue160_axpy_system_headers.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue160_axpy_system_headers_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue160_axpy_system_headers_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_axpy_system_headers.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue160_axpy_system_headers_unparse PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue160_axpy_system_headers_unparse
  rex_test2025_issue160_axpy_system_headers.cpp)

# Issue 160: Preserve explicit instantiation keyword for struct templates.
string(CONCAT rex_test2025_issue160_explicit_struct_instantiation_unparse_command
  "\"$@\" && "
  "grep -E \"^template[[:space:]]+struct[[:space:]]+Crate<\" "
  "rose_rex_test2025_issue160_explicit_struct_instantiation.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c "
  "rose_rex_test2025_issue160_explicit_struct_instantiation.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue160_explicit_struct_instantiation_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue160_explicit_struct_instantiation_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_explicit_struct_instantiation.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue160_explicit_struct_instantiation_unparse PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue160_explicit_struct_instantiation_unparse
  rex_test2025_issue160_explicit_struct_instantiation.cpp)

# Issue 160: Explicit class template instantiation must unparse as a directive.
string(CONCAT rex_test2025_issue160_explicit_class_instantiation_unparse_command
  "\"$@\" && "
  "grep -E \"^[[:space:]]*template[[:space:]]+class[[:space:]]+Box[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]*;\" rose_rex_test2025_issue160_explicit_class_instantiation.cpp && "
  "! grep -E \"template[[:space:]]+struct[[:space:]]+Box[[:space:]]*<[[:space:]]*int[[:space:]]*>\" rose_rex_test2025_issue160_explicit_class_instantiation.cpp && "
  "! grep -E \"template[[:space:]]+class[[:space:]]+Box[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]*\\{\" rose_rex_test2025_issue160_explicit_class_instantiation.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue160_explicit_class_instantiation.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue160_explicit_class_instantiation_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue160_explicit_class_instantiation_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_explicit_class_instantiation.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue160_explicit_class_instantiation_unparse PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue160_explicit_class_instantiation_unparse
  rex_test2025_issue160_explicit_class_instantiation.cpp)

# Issue 160: Extern class template instantiation must unparse with "extern".
string(CONCAT rex_test2025_issue160_extern_class_instantiation_unparse_command
  "\"$@\" && "
  "grep -E \"^[[:space:]]*extern[[:space:]]+template[[:space:]]+struct[[:space:]]+Holder[[:space:]]*<[[:space:]]*double[[:space:]]*>[[:space:]]*;\" rose_rex_test2025_issue160_extern_class_instantiation.cpp && "
  "! grep -E \"extern[[:space:]]+template[[:space:]]+class[[:space:]]+Holder[[:space:]]*<[[:space:]]*double[[:space:]]*>\" rose_rex_test2025_issue160_extern_class_instantiation.cpp && "
  "! grep -E \"template[[:space:]]+struct[[:space:]]+Holder[[:space:]]*<[[:space:]]*double[[:space:]]*>[[:space:]]*\\{\" rose_rex_test2025_issue160_extern_class_instantiation.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue160_extern_class_instantiation.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue160_extern_class_instantiation_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue160_extern_class_instantiation_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_extern_class_instantiation.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue160_extern_class_instantiation_unparse PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue160_extern_class_instantiation_unparse
  rex_test2025_issue160_extern_class_instantiation.cpp)

# Issue 160/#268: Keep explicit system-header instantiation in the template's namespace.
set(_issue160_system_header_class_instantiation_base
    "rex_test2025_issue160_system_header_class_instantiation")
set(_issue160_system_header_class_instantiation_namespace
    "rex_test2025_issue160")
string(CONCAT _issue160_system_header_class_instantiation_unparse_command
  "\"$@\" && "
  "grep -E \"^[[:space:]]*template[[:space:]]+(class|struct)[[:space:]]+${_issue160_system_header_class_instantiation_namespace}::Box[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]*;\" rose_${_issue160_system_header_class_instantiation_base}.cpp && "
  "\"${CMAKE_CXX_COMPILER}\" -std=c++20 -I\"${CMAKE_CURRENT_SOURCE_DIR}\" -c rose_${_issue160_system_header_class_instantiation_base}.cpp"
)
add_test(
  NAME Cxx_tests_${_issue160_system_header_class_instantiation_base}_unparse
  COMMAND
    sh -c
    "${_issue160_system_header_class_instantiation_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/${_issue160_system_header_class_instantiation_base}.cpp
)
set_tests_properties(
  Cxx_tests_${_issue160_system_header_class_instantiation_base}_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_${_issue160_system_header_class_instantiation_base}_unparse
  ${_issue160_system_header_class_instantiation_base}.cpp)

# Issue 62: Mangled qualifiers must never contain template syntax.
rex_compile_test_name(_issue62_test "Cxx_tests" rex_test2025_mangle_qualifiers_no_template_syntax.cpp)
set_tests_properties(${_issue62_test} PROPERTIES
  FAIL_REGULAR_EXPRESSION "In mangleQualifiersToString\\(\\):"
)

# Issue 203: Template-qualified default template args must not assert in
# mangleQualifiersToString.
rex_compile_test_name(_issue203_test "Cxx_tests" rex_test2026_issue203_template_qualified_private_typedef.cpp)
set_tests_properties(${_issue203_test} PROPERTIES
  FAIL_REGULAR_EXPRESSION "In mangleQualifiersToString\\(\\):"
)

# Issue 113: Template argument spelling must not depend on using directives in
# the instantiation scope; keep namespace qualifiers in the generated name.
add_test(
  NAME Cxx_tests_rex_test2025_issue113_instantiation_spelling_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E \"Container[[:space:]]*<[[:space:]]*(class|struct|union)?[[:space:]]*(::)?ns::Inner\" rose_rex_test2025_issue113_instantiation_spelling.cpp && ! grep -E \"Container[[:space:]]*<[[:space:]]*(class|struct|union)?[[:space:]]*Inner\" rose_rex_test2025_issue113_instantiation_spelling.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue113_instantiation_spelling.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue113_instantiation_spelling.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue113_instantiation_spelling_unparse PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue113_instantiation_spelling_unparse
  rex_test2025_issue113_instantiation_spelling.cpp)

# Issue 115: Dependent/unresolved name refs must use SgNonrealRefExp.
add_executable(
  rex_test2025_issue115_nonreal_refexp_translator
  rex_test2025_issue115_nonreal_refexp_translator.cpp
)
target_link_libraries(
  rex_test2025_issue115_nonreal_refexp_translator
  ROSE_DLL
  ${link_with_libraries}
)

foreach(
  file_to_test
  rex_test2025_issue115_unresolved_lookup.cpp
  rex_test2025_issue115_dependent_member.cpp
  rex_test2025_issue115_qualified_unresolved_lookup.cpp
)
  get_filename_component(test_name ${file_to_test} NAME_WE)
  add_test(
    NAME Cxx_tests_${test_name}_nonreal_refexp
    COMMAND
      $<TARGET_FILE:rex_test2025_issue115_nonreal_refexp_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  set_tests_properties(Cxx_tests_${test_name}_nonreal_refexp PROPERTIES LABELS Cxx_tests )
  rex_lock_test_for_input(
    Cxx_tests_${test_name}_nonreal_refexp
    ${file_to_test})
endforeach()

# Issue 115 Phase C: Instantiated bodies must use resolved refs.
add_executable(
  rex_test2025_issue115_phaseC_resolution_translator
  rex_test2025_issue115_phaseC_resolution_translator.cpp
)
target_link_libraries(
  rex_test2025_issue115_phaseC_resolution_translator
  ROSE_DLL
  ${link_with_libraries}
)

foreach(
  file_to_test
  rex_test2025_issue115_unresolved_lookup.cpp
  rex_test2025_issue115_dependent_member.cpp
  rex_test2025_issue115_qualified_unresolved_lookup.cpp
)
  get_filename_component(test_name ${file_to_test} NAME_WE)
  add_test(
    NAME Cxx_tests_${test_name}_phaseC_resolution
    COMMAND
      $<TARGET_FILE:rex_test2025_issue115_phaseC_resolution_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  set_tests_properties(Cxx_tests_${test_name}_phaseC_resolution PROPERTIES LABELS Cxx_tests )
  rex_lock_test_for_input(
    Cxx_tests_${test_name}_phaseC_resolution
    ${file_to_test})
endforeach()

# Issue 147: Dependent template types must use SgNonrealType.
add_executable(
  rex_test2025_dependent_template_specialization_type_translator
  rex_test2025_dependent_template_specialization_type_translator.cpp
)
target_link_libraries(
  rex_test2025_dependent_template_specialization_type_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME Cxx_tests_rex_test2025_dependent_template_specialization_type_nonreal_type
  COMMAND
    $<TARGET_FILE:rex_test2025_dependent_template_specialization_type_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_dependent_template_specialization_type.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_dependent_template_specialization_type_nonreal_type PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_dependent_template_specialization_type_nonreal_type
  rex_test2025_dependent_template_specialization_type.cpp)

# Issue 148: Mutating system-header declarations must preserve AST invariants.
add_executable(
  rex_test2025_issue148_system_header_mutation_translator
  rex_test2025_issue148_system_header_mutation_translator.cpp
)
target_link_libraries(
  rex_test2025_issue148_system_header_mutation_translator
  ROSE_DLL
  ${link_with_libraries}
)

set(rex_test2025_issue148_system_header_dir
    ${CMAKE_CURRENT_SOURCE_DIR}/../../input_codes/system_header_like)
add_test(
  NAME Cxx_tests_rex_test2025_issue148_system_header_mutation
  COMMAND
    $<TARGET_FILE:rex_test2025_issue148_system_header_mutation_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -isystem ${rex_test2025_issue148_system_header_dir}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue148_system_header_mutation.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue148_system_header_mutation PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue148_system_header_mutation
  rex_test2025_issue148_system_header_mutation.cpp)

# Issue 189: CFE token-stream mapping validation.
add_executable(
  rex_test2025_cfe_token_stream_mapping_translator
  rex_test2025_cfe_token_stream_mapping_translator.cpp
)
target_link_libraries(
  rex_test2025_cfe_token_stream_mapping_translator
  ROSE_DLL
  ${link_with_libraries}
)

set(rex_test2025_cfe_token_stream_mapping_tests
  rex_test2025_cfe_token_stream_mapping_basic.cpp
  rex_test2025_cfe_token_stream_mapping_shared_intervals.cpp
  rex_test2025_cfe_token_stream_mapping_else_whitespace.cpp
  rex_test2025_cfe_token_stream_mapping_macro_transform.cpp
  rex_test2025_cfe_token_stream_mapping_preproc_order.cpp
)

foreach(file_to_test ${rex_test2025_cfe_token_stream_mapping_tests})
  get_filename_component(test_name ${file_to_test} NAME_WE)
  add_test(
    NAME Cxx_tests_${test_name}_token_mapping
    COMMAND
      $<TARGET_FILE:rex_test2025_cfe_token_stream_mapping_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
      ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  set_tests_properties(Cxx_tests_${test_name}_token_mapping PROPERTIES LABELS Cxx_tests )
  rex_lock_test_for_input(
    Cxx_tests_${test_name}_token_mapping
    ${file_to_test})
endforeach()

add_test(
  NAME Cxx_tests_rex_test2025_cfe_token_stream_mapping_macros_includes_token_mapping
  COMMAND
    $<TARGET_FILE:rex_test2025_cfe_token_stream_mapping_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    -rose:unparseHeaderFiles ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_cfe_token_stream_mapping_macros_includes.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_cfe_token_stream_mapping_macros_includes_token_mapping PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_cfe_token_stream_mapping_macros_includes_token_mapping
  rex_test2025_cfe_token_stream_mapping_macros_includes.cpp)

# Issue 189 follow-up: enforce token-unparse contract before backend unparsing.
add_executable(
  rex_test2026_token_unparse_contract_translator
  rex_test2026_token_unparse_contract_translator.cpp
)
target_link_libraries(
  rex_test2026_token_unparse_contract_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME Cxx_tests_rex_test2026_token_unparse_contract_rejects_incomplete_map
  COMMAND
    sh -c
    "\"$@\" 2> rex_test2026_token_unparse_contract.err; status=$?; test $status -ne 0 && grep -F 'token-unparse contract violation' rex_test2026_token_unparse_contract.err"
    dummy
    $<TARGET_FILE:rex_test2026_token_unparse_contract_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_token_unparse_contract.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2026_token_unparse_contract_rejects_incomplete_map
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2026_token_unparse_contract_rejects_incomplete_map
  rex_test2026_token_unparse_contract.cpp)

if(UNIX AND NOT APPLE)
  # CMake-level root-cause guard: Clang frontend builds must link shared libLLVM.
  add_test(
    NAME Cxx_tests_rex_test2026_shared_llvm_linkage
    COMMAND
      sh -c "ldd \"$1\" | grep -E 'libLLVM([^[:space:]]*)\\.so' >/dev/null"
      dummy
      $<TARGET_FILE:rex_test2025_cfe_token_stream_mapping_translator>
  )
  set_tests_properties(Cxx_tests_rex_test2026_shared_llvm_linkage PROPERTIES LABELS Cxx_tests)
endif()

# Issue 151: LinkageSpecDecl (extern "C") must not drop declarations and must
# preserve linkage in the generated output.
add_test(
  NAME Cxx_tests_rex_test2025_extern_c_roundtrip_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -F 'extern \"C\"' rose_rex_test2025_extern_c_roundtrip.cpp && grep -E '(^|[^[:alnum:]_])int[[:space:]]+foo[[:space:]]*[(]' rose_rex_test2025_extern_c_roundtrip.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_extern_c_roundtrip.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_extern_c_roundtrip.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_extern_c_roundtrip_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_extern_c_roundtrip_unparse
  rex_test2025_extern_c_roundtrip.cpp)

# Issue 151: Preserve extern "C" brace form without flattening.
add_test(
  NAME Cxx_tests_rex_test2025_extern_c_brace_roundtrip_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -F 'extern \"C\" {' rose_rex_test2025_extern_c_brace_roundtrip.cpp && grep -E '(^|[^[:alnum:]_])int[[:space:]]+foo[[:space:]]*[(]' rose_rex_test2025_extern_c_brace_roundtrip.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_extern_c_brace_roundtrip.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_extern_c_brace_roundtrip.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_extern_c_brace_roundtrip_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_extern_c_brace_roundtrip_unparse
  rex_test2025_extern_c_brace_roundtrip.cpp)

# Issue 123: Function refs must not emit duplicated qualification (e.g. std::::get).
add_test(
  NAME Cxx_tests_rex_test2025_issue123_std_double_colon_unparse
  COMMAND
    sh -c
    "\"$@\" && ! grep -E \"::::\" rose_rex_test2025_issue123_std_double_colon.cpp && grep -E \"std::get[[:space:]]*<0>\" rose_rex_test2025_issue123_std_double_colon.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue123_std_double_colon.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue123_std_double_colon.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue123_std_double_colon_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue123_std_double_colon_unparse
  rex_test2025_issue123_std_double_colon.cpp)

# Issue 146: Namespace-scope template instantiations must not emit std::::iota.
add_test(
  NAME Cxx_tests_rex_test2025_issue146_std_iota_quad_colon_unparse
  COMMAND
    sh -c
    "\"$@\" && ! grep -F '::::' rose_rex_test2025_issue146_std_iota_quad_colon.cpp && grep -E '(^|[^[:alnum:]_])std::iota[[:space:]]*(<|\\()' rose_rex_test2025_issue146_std_iota_quad_colon.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue146_std_iota_quad_colon.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue146_std_iota_quad_colon.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue146_std_iota_quad_colon_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue146_std_iota_quad_colon_unparse
  rex_test2025_issue146_std_iota_quad_colon.cpp)

# Issue 146: Global function template instantiations must keep :: under shadowing.
add_test(
  NAME Cxx_tests_rex_test2025_issue146_shadowed_function_template_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E '(^|[^[:alnum:]_])::make_tag' rose_rex_test2025_issue146_shadowed_function_template.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue146_shadowed_function_template.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue146_shadowed_function_template.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue146_shadowed_function_template_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue146_shadowed_function_template_unparse
  rex_test2025_issue146_shadowed_function_template.cpp)

# Issue 149: Parameter lists must not be reported as unsupported declarations.
add_test(
  NAME Cxx_tests_rex_test2025_issue149_param_list_unique_name_interface_function_coverage
  COMMAND
    $<TARGET_FILE:interfaceFunctionCoverage>
    -rose:verbose 0 -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue149_param_list_unique_name.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue149_param_list_unique_name_interface_function_coverage
  PROPERTIES
    LABELS Cxx_tests
    FAIL_REGULAR_EXPRESSION "Unsupported declaration = .*SgFunctionParameterList"
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue149_param_list_unique_name_interface_function_coverage
  rex_test2025_issue149_param_list_unique_name.cpp)

# Issue 144: Clang diagnostic errors must be fatal by default.
add_test(
  NAME Cxx_tests_rex_test2025_cfe_error_exit_code
  COMMAND
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_cfe_error_exit_code.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_cfe_error_exit_code
  PROPERTIES LABELS Cxx_tests WILL_FAIL TRUE
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_cfe_error_exit_code
  rex_test2025_cfe_error_exit_code.cpp)

# Issue 144: Allow backend when explicitly opted in.
add_test(
  NAME Cxx_tests_rex_test2025_cfe_error_exit_code_continue_on_error_unparse
  COMMAND
    sh -c
    "\"$@\" && test -f rose_rex_test2025_cfe_error_exit_code.cpp && { ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_cfe_error_exit_code.cpp 2> compile.err || true; } && grep -F \"non-void function 'main' should return a value\" compile.err"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_cfe_error_exit_code.cpp
    -rex:clang:continue-on-error
)
set_tests_properties(
  Cxx_tests_rex_test2025_cfe_error_exit_code_continue_on_error_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_cfe_error_exit_code_continue_on_error_unparse
  rex_test2025_cfe_error_exit_code.cpp)

# Issue 264: -fno-rtti should remain backend-only unless explicitly opted in.
add_test(
  NAME Cxx_tests_rex_test2026_issue264_rtti_backend_only
  COMMAND
    sh -c
    "\"$@\" && test -f rose_rex_test2026_issue264_rtti_dynamic_cast.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -fno-rtti
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_issue264_rtti_dynamic_cast.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2026_issue264_rtti_backend_only
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2026_issue264_rtti_backend_only
  rex_test2026_issue264_rtti_dynamic_cast.cpp)

# Issue 264: Allow explicitly opting in to frontend RTTI flag enforcement.
add_test(
  NAME Cxx_tests_rex_test2026_issue264_rtti_respect_frontend_flags
  COMMAND
    sh -c
    "\"$@\" > issue264_rtti_respect_frontend_flags.log 2>&1 && exit 1; grep -E \"dynamic_cast.*-frtti\" issue264_rtti_respect_frontend_flags.log"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -fno-rtti
    -rex:clang:respect-rtti-flags
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_issue264_rtti_dynamic_cast.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2026_issue264_rtti_respect_frontend_flags
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2026_issue264_rtti_respect_frontend_flags
  rex_test2026_issue264_rtti_dynamic_cast.cpp)

# Issue 317: Keep -fno-rtti/-fno-exceptions backend-only in frontend parsing.
add_test(
  NAME Cxx_tests_rex_test2026_issue317_rtti_exceptions_backend_only
  COMMAND
    sh -c
    "OUTFILE=rose_rex_test2026_issue317_rtti_exceptions.cpp; \"$@\" && test -f \"$OUTFILE\" && \"${CMAKE_CXX_COMPILER}\" -std=c++20 -c \"$OUTFILE\""
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -fno-rtti
    -fno-exceptions
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_issue317_rtti_exceptions.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2026_issue317_rtti_exceptions_backend_only
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2026_issue317_rtti_exceptions_backend_only
  rex_test2026_issue317_rtti_exceptions.cpp)

# Issue 124: System header template specializations must preserve template args.
add_test(
  NAME Cxx_tests_rex_test2025_issue124_system_header_specialization_args_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E \"Use<.*(::)?std::vector[[:space:]]*<\" rose_rex_test2025_issue124_system_header_specialization_args.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue124_system_header_specialization_args.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue124_system_header_specialization_args.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue124_system_header_specialization_args_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue124_system_header_specialization_args_unparse
  rex_test2025_issue124_system_header_specialization_args.cpp)

# Issue 125: Declarations must not be missing from their owning scope child list.
rex_compile_test_name(_issue125_test "Cxx_tests" rex_test2025_issue125_fixup_child_list_warning.cpp)
set_tests_properties(${_issue125_test} PROPERTIES
  FAIL_REGULAR_EXPRESSION "not in child list of scope"
)

# Issue 126: Default template args must be printed from IR; A<> must not unparse
# as "template A a;".
add_test(
  NAME Cxx_tests_rex_test2025_issue126_default_template_args_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E \"template[[:space:]]*<[[:space:]]*(class|typename)[[:space:]]+T[[:space:]]*=[[:space:]]*void\" rose_rex_test2025_issue126_default_template_args.cpp && ! grep -E \"template[[:space:]]+A[[:space:]]+a;\" rose_rex_test2025_issue126_default_template_args.cpp && ! grep -E \"T[[:space:]]*=[[:space:]]*void[[:space:]]*=[[:space:]]*void\" rose_rex_test2025_issue126_default_template_args.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue126_default_template_args.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue126_default_template_args.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue126_default_template_args_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue126_default_template_args_unparse
  rex_test2025_issue126_default_template_args.cpp)

# Issue 59: Template-template arguments must preserve required global
# qualification (no string stripping in the unparser).
add_test(
  NAME Cxx_tests_rex_test2025_issue59_template_template_arg_global_qual_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E \"UsePack[[:space:]]*<[[:space:]]*::Lib::Tuple\" rose_rex_test2025_issue59_template_template_arg_global_qual.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue59_template_template_arg_global_qual.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue59_template_template_arg_global_qual.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue59_template_template_arg_global_qual_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue59_template_template_arg_global_qual_unparse
  rex_test2025_issue59_template_template_arg_global_qual.cpp)

# Issue 60: Friend free functions declared/defined inside a class must keep
# their lexical placement (inside the class) without emitting extra namespace
# declarations/definitions.
add_test(
  NAME Cxx_tests_rex_test2025_issue60_friend_function_symbol_scope_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E \"friend[[:space:]]+int[[:space:]]+f[[:space:]]*\\(\" rose_rex_test2025_issue60_friend_function_symbol_scope.cpp && grep -E \"friend[[:space:]]+int[[:space:]]+g[[:space:]]*\\(\" rose_rex_test2025_issue60_friend_function_symbol_scope.cpp && ! grep -E \"^[[:space:]]*int[[:space:]]+f[[:space:]]*\\(\" rose_rex_test2025_issue60_friend_function_symbol_scope.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue60_friend_function_symbol_scope.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue60_friend_function_symbol_scope.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue60_friend_function_symbol_scope_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue60_friend_function_symbol_scope_unparse
  rex_test2025_issue60_friend_function_symbol_scope.cpp)

# Issue 64: Unparser must preserve required global qualification for template
# instantiations when a global template is shadowed by a nested-scope template.
add_test(
  NAME Cxx_tests_rex_test2025_issue64_shadow_template_global_qual_unparse
  COMMAND
    sh -c
    "\"$@\" && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue64_shadow_template_global_qual.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue64_shadow_template_global_qual.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue64_shadow_template_global_qual_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue64_shadow_template_global_qual_unparse
  rex_test2025_issue64_shadow_template_global_qual.cpp)

# Issue 118: EmptyDecl must translate to SgEmptyDeclaration (preserve stray ';').
rex_compile_test_name(_issue118_test "Cxx_tests" rex_test2025_issue118_empty_decl.cpp)
set_tests_properties(${_issue118_test} PROPERTIES
  FAIL_REGULAR_EXPRESSION "No Sage node associated with the declaration"
)

# Namespace reopenings: keep declarations in their lexical namespace block.
add_test(
  NAME Cxx_tests_rex_test2025_namespace_reopenings_preserve_blocks_unparse
  COMMAND
    sh -c
    "\"$@\" && awk '/namespace[[:space:]]+A/ { ++ns; if (ns==1) ns1=NR; if (ns==2) ns2=NR } { if (!f && /void[[:space:]]+f[[:space:]]*\\(/) f=NR; if (!g && /void[[:space:]]+g[[:space:]]*\\(/) g=NR } END { exit !(ns2 && f && g && ns1 < f && f < ns2 && ns2 < g) }' rose_rex_test2025_namespace_reopenings_preserve_blocks.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_namespace_reopenings_preserve_blocks.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_namespace_reopenings_preserve_blocks.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_namespace_reopenings_preserve_blocks_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_namespace_reopenings_preserve_blocks_unparse
  rex_test2025_namespace_reopenings_preserve_blocks.cpp)

# Issue 93: Class-scope static_assert must preserve source order vs fields.
add_test(
  NAME Cxx_tests_rex_test2025_issue93_static_assert_order_unparse
  COMMAND sh -c "\"$@\" && awk '/struct Foo/ {inside=1} inside { if (!sa && $0 ~ /static_assert/) sa=NR; if (!x && $0 ~ /int x;/) x=NR } /};/ && inside {inside=0} END { exit !(sa && x && sa < x) }' rose_rex_test2025_issue93_static_assert_order.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue93_static_assert_order.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue93_static_assert_order.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue93_static_assert_order_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue93_static_assert_order_unparse
  rex_test2025_issue93_static_assert_order.cpp)

# Issue 94: Template argument lists must not introduce a space before commas.
add_test(
  NAME Cxx_tests_rex_test2025_issue94_template_arg_spacing_unparse
  COMMAND sh -c "\"$@\" && grep -F \"Container<int, 10>\" rose_rex_test2025_issue94_template_arg_spacing.cpp && ! grep -E \"Container<int[[:space:]]+,\" rose_rex_test2025_issue94_template_arg_spacing.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue94_template_arg_spacing.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue94_template_arg_spacing.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue94_template_arg_spacing_unparse PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue94_template_arg_spacing_unparse
  rex_test2025_issue94_template_arg_spacing.cpp)

# Issue 145: Preserve explicit = default and avoid implicit-body placeholders.
add_test(
  NAME Cxx_tests_rex_test2025_defaulted_special_members_anon_union_unparse
  COMMAND sh -c "\"$@\" && grep -F \"= default\" rose_rex_test2025_defaulted_special_members_anon_union.cpp && ! grep -F \".  operator=  .\" rose_rex_test2025_defaulted_special_members_anon_union.cpp && ! grep -F \"(this) ->  .\" rose_rex_test2025_defaulted_special_members_anon_union.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_defaulted_special_members_anon_union.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -std=c++20
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_defaulted_special_members_anon_union.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_defaulted_special_members_anon_union_unparse
  PROPERTIES LABELS Cxx_tests
  FAIL_REGULAR_EXPRESSION "Cannot resolve Field member;using placeholder"
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_defaulted_special_members_anon_union_unparse
  rex_test2025_defaulted_special_members_anon_union.cpp)

# Verify explicit template arguments are preserved in calls
add_test(
  NAME Cxx_tests_rex_test2025_template_call_explicit_args
  COMMAND sh -c "\"$@\" && grep \"foo<5>\" rose_rex_test2025_template_call.cpp | grep -v \"//\" && grep \"foo<10>\" rose_rex_test2025_template_call.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_template_call.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_template_call.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_template_call_explicit_args PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_template_call_explicit_args
  rex_test2025_template_call.cpp)

add_test(
  NAME Cxx_tests_rex_test2025_member_template_call_explicit_args
  COMMAND sh -c "\"$@\" && grep -E \"foo<[[:space:]]*5[[:space:]]*>\" rose_rex_test2025_member_template_call.cpp | grep -v \"//\" && grep -E \"foo<[[:space:]]*10[[:space:]]*>\" rose_rex_test2025_member_template_call.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_member_template_call.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_member_template_call.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_member_template_call_explicit_args PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_member_template_call_explicit_args
  rex_test2025_member_template_call.cpp)

add_test(
  NAME Cxx_tests_rex_test2025_variadic_template_params_unparse
  COMMAND sh -c "\"$@\" && grep -E \"template <typename[[:space:]]*\\\\.\\\\.\\\\.\" rose_rex_test2025_variadic_template_params.cpp | grep -v \"//\" && grep -E \"template <int[[:space:]]*\\\\.\\\\.\\\\.\" rose_rex_test2025_variadic_template_params.cpp | grep -v \"//\" && grep -E \"class[[:space:]]*\\\\.\\\\.\\\\.[[:space:]]*Templates\" rose_rex_test2025_variadic_template_params.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_variadic_template_params.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_variadic_template_params.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_variadic_template_params_unparse PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_variadic_template_params_unparse
  rex_test2025_variadic_template_params.cpp)

add_test(
  NAME Cxx_tests_rex_test2025_template_argument_pack_unparse
  COMMAND sh -c "\"$@\" && grep -E \"\\\\bT[[:space:]]+t[[:space:]]*(=[[:space:]]*)?\\\\{[[:space:]]*\\\\}\" rose_rex_test2025_template_argument_pack.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_template_argument_pack.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_template_argument_pack.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_template_argument_pack_unparse PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_template_argument_pack_unparse
  rex_test2025_template_argument_pack.cpp)

# REX: Function-try-blocks should translate and unparse cleanly (including constructor init lists).
# We add it manually here (not in Cxx_Testcodes.cmake) to avoid breaking downstream tests.
add_test(
  NAME Cxx_tests_rex_test2025_issue99_failure_propagation
  COMMAND sh -c "\"$@\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue99_failure_propagation.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue99_failure_propagation.cpp
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue99_failure_propagation
  rex_test2025_issue99_failure_propagation.cpp)

# REX: New test for Issue 87 (Functions inside namespaces lost)
# We verify that "void bar()" is present in the output associated with namespace behavior.
add_test(
  NAME Cxx_tests_rex_test2025_issue87_namespace
  COMMAND sh -c "\"$@\" && grep \"void bar()\" rose_rex_test2025_issue87_namespace.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue87_namespace.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue87_namespace.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue87_namespace PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue87_namespace
  rex_test2025_issue87_namespace.cpp)

add_test(
  NAME Cxx_tests_rex_test2025_issue89_type_alias_template_args
  COMMAND sh -c "\"$@\" && grep -E \"using IntContainer = Container< *int *>\" rose_rex_test2025_issue89_alias.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue89_alias.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue89_alias.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue89_type_alias_template_args PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue89_type_alias_template_args
  rex_test2025_issue89_alias.cpp)

add_test(
  NAME Cxx_tests_rex_test2025_issue90_array_dim_template_param
  COMMAND sh -c "\"$@\" && grep \"data\\[N\\]\" rose_rex_test2025_issue90_array_dim.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue90_array_dim.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue90_array_dim.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue90_array_dim_template_param PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue90_array_dim_template_param
  rex_test2025_issue90_array_dim.cpp)

# rex_test2026: concepts/constraints should unparse and compile under C++20.
add_test(
  NAME Cxx_tests_rex_test2026_concepts_unparse
  COMMAND
    sh -c
    "\"$@\" && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2026_concepts.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -std=c++20
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_concepts.cpp
)
set_tests_properties(Cxx_tests_rex_test2026_concepts_unparse PROPERTIES
  LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2026_concepts_unparse
  rex_test2026_concepts.cpp)

# rex_test2026: dependent template qualification should round-trip.
add_test(
  NAME Cxx_tests_rex_test2026_dependent_qualification_unparse
  COMMAND
    sh -c
    "\"$@\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2026_dependent_qualification.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_dependent_qualification.cpp
)
set_tests_properties(Cxx_tests_rex_test2026_dependent_qualification_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2026_dependent_qualification_unparse
  rex_test2026_dependent_qualification.cpp)

# Makefile check targets.
set(_cxx_check_local_tests)
foreach(file_to_test ${CXX_TESTCODES_TO_RUN})
  rex_compile_test_name(_test_name "Cxx_tests" ${file_to_test})
  list(APPEND _cxx_check_local_tests ${_test_name})
endforeach()
list(APPEND _cxx_check_local_tests
  Cxx_tests_test_pdf_generation
  Cxx_tests_test_unparse_includes_option
  Cxx_tests_test_unparse_includes_option_with_link
  Cxx_tests_common_configure_test_with_link_part_1
  Cxx_tests_common_configure_test_with_link_part_2
  Cxx_tests_testTemplateExample_1
  Cxx_tests_testTemplateExample_2
  Cxx_tests_testTemplateExample_3
  Cxx_tests_testTemplateExample_4
  Cxx_tests_testTemplateExample_5
)

set(_cxx_perf_tests)
foreach(file_to_test ${CXX_TESTCODES_TO_RUN})
  rex_compile_test_name(_test_name "Cxx_tests" ${file_to_test})
  list(APPEND _cxx_perf_tests ${_test_name})
endforeach()
foreach(file_to_test ${TESTCODE_CURRENTLY_FAILING})
  rex_compile_test_name(_test_name "Cxx_tests_failing" ${file_to_test})
  list(APPEND _cxx_perf_tests ${_test_name})
endforeach()
list(REMOVE_DUPLICATES _cxx_perf_tests)

add_test(
  NAME nonsmoke_functional_CompileTests_Cxx_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=Cxx_tests_Area-01_C\;Cxx_tests_X_C\;Cxx_tests_cpp_vector_C\;Cxx_tests_dynamic_cast_C\;Cxx_tests_failing_c++trick_C\;Cxx_tests_failing_certSignalExample_C\;Cxx_tests_failing_lexPhase2003_02_C\;Cxx_tests_failing_longFile_C\;Cxx_tests_failing_math_C\;Cxx_tests_failing_restrict_template_parameter_C\;Cxx_tests_failing_rose-1701-1_C\;Cxx_tests_failing_rose-1962_C\;Cxx_tests_failing_test2004_117_C\;Cxx_tests_failing_test2004_121_C\;Cxx_tests_failing_test2004_25_C\;Cxx_tests_failing_test2004_35_C\;Cxx_tests_test2004_77_C\;Cxx_tests_failing_test2004_85_C\;Cxx_tests_failing_test2005_06_C\;Cxx_tests_failing_test2005_115_C\;Cxx_tests_failing_test2005_151_C\;Cxx_tests_failing_test2005_188_C\;Cxx_tests_failing_test2005_200_C\;Cxx_tests_failing_test2005_201_C\;Cxx_tests_failing_test2005_204_C\;Cxx_tests_failing_test2005_205_C\;Cxx_tests_failing_test2005_42_C\;Cxx_tests_failing_test2005_50_C\;Cxx_tests_failing_test2005_69_C\;Cxx_tests_failing_test2005_85_C\;Cxx_tests_failing_test2006_08_C\;Cxx_tests_failing_test2006_09_C\;Cxx_tests_failing_test2006_11_C\;Cxx_tests_failing_test2006_127_C\;Cxx_tests_failing_test2006_128_C\;Cxx_tests_failing_test2006_130_C\;Cxx_tests_failing_test2006_131_C\;Cxx_tests_failing_test2006_138_C\;Cxx_tests_failing_test2006_15_C\;Cxx_tests_failing_test2006_167_C\;Cxx_tests_failing_test2006_168_C\;Cxx_tests_failing_test2006_169_C\;Cxx_tests_failing_test2006_170_C\;Cxx_tests_failing_test2006_171_C\;Cxx_tests_failing_test2006_172_C\;Cxx_tests_failing_test2006_173_C\;Cxx_tests_failing_test2006_174_C\;Cxx_tests_failing_test2006_175_C\;Cxx_tests_failing_test2006_176_C\;Cxx_tests_failing_test2006_180_C\;Cxx_tests_failing_test2006_182_C\;Cxx_tests_failing_test2006_184_C\;Cxx_tests_failing_test2006_186_C\;Cxx_tests_failing_test2006_187_C\;Cxx_tests_failing_test2006_188_C\;Cxx_tests_failing_test2006_20_C\;Cxx_tests_failing_test2006_21_C\;Cxx_tests_failing_test2006_24_C\;Cxx_tests_failing_test2006_27_C\;Cxx_tests_failing_test2006_33_C\;Cxx_tests_failing_test2006_35_C\;Cxx_tests_failing_test2006_36_C\;Cxx_tests_failing_test2006_37_C\;Cxx_tests_failing_test2006_49_C\;Cxx_tests_failing_test2006_60_C\;Cxx_tests_failing_test2006_64_C\;Cxx_tests_failing_test2007_100_C\;Cxx_tests_failing_test2007_101_C\;Cxx_tests_failing_test2007_104_C\;Cxx_tests_failing_test2007_105_C\;Cxx_tests_failing_test2007_106_C\;Cxx_tests_failing_test2007_107_C\;Cxx_tests_failing_test2007_108_C\;Cxx_tests_test2007_109_C\;Cxx_tests_failing_test2007_110_C\;Cxx_tests_failing_test2007_111_C\;Cxx_tests_failing_test2007_112_C\;Cxx_tests_failing_test2007_113_C\;Cxx_tests_failing_test2007_114_C\;Cxx_tests_failing_test2007_115_C\;Cxx_tests_failing_test2007_116_C\;Cxx_tests_failing_test2007_117_C\;Cxx_tests_failing_test2007_118_C\;Cxx_tests_failing_test2007_119_C\;Cxx_tests_failing_test2007_120_C\;Cxx_tests_failing_test2007_121_C\;Cxx_tests_failing_test2007_122_C\;Cxx_tests_failing_test2007_123_C\;Cxx_tests_failing_test2007_124_C\;Cxx_tests_test2007_125_C\;Cxx_tests_failing_test2007_126_C\;Cxx_tests_failing_test2007_127_C\;Cxx_tests_failing_test2007_129_C\;Cxx_tests_failing_test2007_131_C\;Cxx_tests_failing_test2007_132_C\;Cxx_tests_failing_test2007_134_C\;Cxx_tests_failing_test2007_136_C\;Cxx_tests_failing_test2007_138_C\;Cxx_tests_failing_test2007_139_C\;Cxx_tests_test2007_141_C\;Cxx_tests_failing_test2007_143_C\;Cxx_tests_failing_test2007_144_C\;Cxx_tests_failing_test2007_145_C\;Cxx_tests_failing_test2007_146_C\;Cxx_tests_failing_test2007_148_C\;Cxx_tests_failing_test2007_149_C\;Cxx_tests_failing_test2007_153_C\;Cxx_tests_failing_test2007_156_C\;Cxx_tests_failing_test2007_168_C\;Cxx_tests_failing_test2007_174_C\;Cxx_tests_failing_test2007_175_C\;Cxx_tests_failing_test2007_176_C\;Cxx_tests_failing_test2007_18_C\;Cxx_tests_failing_test2007_19_C\;Cxx_tests_failing_test2007_20_C\;Cxx_tests_failing_test2007_22_C\;Cxx_tests_failing_test2007_27_C\;Cxx_tests_failing_test2007_30_C\;Cxx_tests_failing_test2007_31_C\;Cxx_tests_failing_test2007_32_C\;Cxx_tests_failing_test2007_33_C\;Cxx_tests_failing_test2007_34_C\;Cxx_tests_failing_test2007_35_C\;Cxx_tests_failing_test2007_36_C\;Cxx_tests_failing_test2007_37_C\;Cxx_tests_failing_test2007_39_C\;Cxx_tests_failing_test2007_42_C\;Cxx_tests_failing_test2007_45_C\;Cxx_tests_failing_test2007_51_C\;Cxx_tests_failing_test2007_52_C\;Cxx_tests_failing_test2007_54_C\;Cxx_tests_failing_test2007_56_C\;Cxx_tests_failing_test2007_58_C\;Cxx_tests_failing_test2007_60_C\;Cxx_tests_failing_test2007_61_C\;Cxx_tests_failing_test2007_62_C\;Cxx_tests_failing_test2007_63_C\;Cxx_tests_failing_test2007_64_C\;Cxx_tests_failing_test2007_65_C\;Cxx_tests_failing_test2007_66_C\;Cxx_tests_failing_test2007_67_C\;Cxx_tests_failing_test2007_68_C\;Cxx_tests_failing_test2007_69_C\;Cxx_tests_failing_test2007_71_C\;Cxx_tests_failing_test2007_73_C\;Cxx_tests_failing_test2007_74_C\;Cxx_tests_failing_test2007_76_C\;Cxx_tests_failing_test2007_79_C\;Cxx_tests_failing_test2007_80_C\;Cxx_tests_failing_test2007_86_C\;Cxx_tests_failing_test2007_87_C\;Cxx_tests_failing_test2007_88_C\;Cxx_tests_failing_test2007_89_C\;Cxx_tests_failing_test2007_91_C\;Cxx_tests_failing_test2007_92_C\;Cxx_tests_failing_test2007_93_C\;Cxx_tests_failing_test2007_95_C\;Cxx_tests_failing_test2007_96_C\;Cxx_tests_failing_test2007_97_C\;Cxx_tests_failing_test2007_99_C\;Cxx_tests_failing_test2008_07_C\;Cxx_tests_failing_test2008_15_C\;Cxx_tests_failing_test2009_07_C\;Cxx_tests_failing_test2009_13_C\;Cxx_tests_failing_test2009_17_C\;Cxx_tests_failing_test2009_21_C\;Cxx_tests_failing_test2009_22_C\;Cxx_tests_failing_test2009_23_C\;Cxx_tests_test2009_25_C\;Cxx_tests_failing_test2009_26_C\;Cxx_tests_failing_test2009_29_C\;Cxx_tests_failing_test2009_32_C\;Cxx_tests_failing_test2009_33_C\;Cxx_tests_failing_test2009_34_C\;Cxx_tests_failing_test2009_35_C\;Cxx_tests_failing_test2009_36_C\;Cxx_tests_failing_test2009_38_C\;Cxx_tests_failing_test2009_43_C\;Cxx_tests_failing_test2009_44_C\;Cxx_tests_failing_test2009_45_C\;Cxx_tests_failing_test2009_46_C\;Cxx_tests_failing_test2009_47_C\;Cxx_tests_failing_test2010_01_C\;Cxx_tests_failing_test2010_02_C\;Cxx_tests_failing_test2010_07_C\;Cxx_tests_failing_test2010_08_C\;Cxx_tests_failing_test2010_09_C\;Cxx_tests_failing_test2010_10_C\;Cxx_tests_failing_test2010_11_C\;Cxx_tests_failing_test2010_12_C\;Cxx_tests_failing_test2010_15_C\;Cxx_tests_failing_test2010_16_C\;Cxx_tests_failing_test2010_17_C\;Cxx_tests_failing_test2010_20_C\;Cxx_tests_failing_test2010_21_C\;Cxx_tests_failing_test2010_22_C\;Cxx_tests_failing_test2010_23_C\;Cxx_tests_failing_test2010_26_C\;Cxx_tests_test2010_27_C\;Cxx_tests_failing_test2011_01_C\;Cxx_tests_failing_test2011_02_C\;Cxx_tests_failing_test2011_05_C\;Cxx_tests_failing_test2011_06_C\;Cxx_tests_failing_test2011_07_C\;Cxx_tests_failing_test2011_08_C\;Cxx_tests_failing_test2011_09_C\;Cxx_tests_failing_test2011_102_C\;Cxx_tests_failing_test2011_103_C\;Cxx_tests_failing_test2011_104_C\;Cxx_tests_failing_test2011_105_C\;Cxx_tests_failing_test2011_106_C\;Cxx_tests_failing_test2011_107_C\;Cxx_tests_failing_test2011_108_C\;Cxx_tests_failing_test2011_109_C\;Cxx_tests_failing_test2011_10_C\;Cxx_tests_failing_test2011_112_C\;Cxx_tests_failing_test2011_11_C\;Cxx_tests_failing_test2011_122_C\;Cxx_tests_failing_test2011_123_C\;Cxx_tests_failing_test2011_124_C\;Cxx_tests_failing_test2011_125_C\;Cxx_tests_failing_test2011_126_C\;Cxx_tests_failing_test2011_127_C\;Cxx_tests_failing_test2011_128_C\;Cxx_tests_failing_test2011_129_C\;Cxx_tests_failing_test2011_12_C\;Cxx_tests_failing_test2011_130_C\;Cxx_tests_failing_test2011_131_C\;Cxx_tests_failing_test2011_132_C\;Cxx_tests_failing_test2011_134_C\;Cxx_tests_failing_test2011_13_C\;Cxx_tests_failing_test2011_141_C\;Cxx_tests_failing_test2011_142_C\;Cxx_tests_failing_test2011_144_C\;Cxx_tests_failing_test2011_145_C\;Cxx_tests_failing_test2011_148_C\;Cxx_tests_failing_test2011_14_C\;Cxx_tests_failing_test2011_15_C\;Cxx_tests_test2011_16_C\;Cxx_tests_failing_test2011_178_C\;Cxx_tests_failing_test2011_17_C\;Cxx_tests_failing_test2011_18_C\;Cxx_tests_failing_test2011_191_C\;Cxx_tests_failing_test2011_196_C\;Cxx_tests_failing_test2011_197_C\;Cxx_tests_failing_test2011_198_C\;Cxx_tests_failing_test2011_199_C\;Cxx_tests_failing_test2011_200_C\;Cxx_tests_failing_test2011_201_C\;Cxx_tests_failing_test2011_202_C\;Cxx_tests_failing_test2011_203_C\;Cxx_tests_test2012_09_C\;Cxx_tests_failing_test2012_133_C\;Cxx_tests_failing_test2012_134_C\;Cxx_tests_failing_test2012_135_C\;Cxx_tests_failing_test2012_139_C\;Cxx_tests_failing_test2012_140_C\;Cxx_tests_test2012_188_C\;Cxx_tests_failing_test2012_189_C\;Cxx_tests_failing_test2012_252_C\;Cxx_tests_failing_test2012_261_C\;Cxx_tests_failing_test2012_272_C\;Cxx_tests_failing_test2012_273_C\;Cxx_tests_failing_test2012_274_C\;Cxx_tests_failing_test2012_275_C\;Cxx_tests_failing_test2012_276_C\;Cxx_tests_failing_test2012_277_C\;Cxx_tests_failing_test2012_278_C\;Cxx_tests_failing_test2012_35_C\;Cxx_tests_failing_test2012_57_C\;Cxx_tests_failing_test2012_59_C\;Cxx_tests_failing_test2012_68_C\;Cxx_tests_failing_test2012_79_C\;Cxx_tests_failing_test2012_81_C\;Cxx_tests_failing_test2012_86_C\;Cxx_tests_failing_test2012_90_C\;Cxx_tests_failing_test2013_09_C\;Cxx_tests_failing_test2013_13_C\;Cxx_tests_failing_test2013_14_C\;Cxx_tests_failing_test2013_15_C\;Cxx_tests_failing_test2013_164_C\;Cxx_tests_failing_test2013_16_C\;Cxx_tests_failing_test2013_178_C\;Cxx_tests_failing_test2013_17_C\;Cxx_tests_failing_test2013_182_C\;Cxx_tests_failing_test2013_185_C\;Cxx_tests_failing_test2013_195_C\;Cxx_tests_failing_test2013_197_C\;Cxx_tests_failing_test2013_19_C\;Cxx_tests_test2013_212_C\;Cxx_tests_failing_test2013_224_C\;Cxx_tests_failing_test2013_233_C\;Cxx_tests_failing_test2013_234_C\;Cxx_tests_failing_test2013_235_C\;Cxx_tests_failing_test2013_236_C\;Cxx_tests_failing_test2013_240_C\;Cxx_tests_failing_test2013_241_C\;Cxx_tests_failing_test2013_242_C\;Cxx_tests_failing_test2013_243_C\;Cxx_tests_failing_test2013_245_C\;Cxx_tests_failing_test2013_247_C\;Cxx_tests_failing_test2013_248_C\;Cxx_tests_failing_test2013_249_C\;Cxx_tests_failing_test2013_24_C\;Cxx_tests_failing_test2013_254_C\;Cxx_tests_failing_test2013_257_C\;Cxx_tests_failing_test2013_260_C\;Cxx_tests_failing_test2013_262_C\;Cxx_tests_failing_test2013_263_C\;Cxx_tests_failing_test2013_264_C\;Cxx_tests_failing_test2013_265_C\;Cxx_tests_failing_test2013_266_C\;Cxx_tests_failing_test2013_267_C\;Cxx_tests_failing_test2013_268_C\;Cxx_tests_failing_test2013_269_C\;Cxx_tests_failing_test2013_270_C\;Cxx_tests_failing_test2013_304_C\;Cxx_tests_failing_test2013_309_C\;Cxx_tests_failing_test2013_30_C\;Cxx_tests_failing_test2013_321_C\;Cxx_tests_failing_test2013_322_C\;Cxx_tests_failing_test2013_323_C\;Cxx_tests_failing_test2013_35_C\;Cxx_tests_failing_test2013_40_C\;Cxx_tests_failing_test2013_41_C\;Cxx_tests_failing_test2013_42_C\;Cxx_tests_failing_test2013_43_C\;Cxx_tests_failing_test2013_45_C\;Cxx_tests_failing_test2013_49_C\;Cxx_tests_failing_test2013_62_C\;Cxx_tests_failing_test2013_84_C\;Cxx_tests_failing_test2013_85_C\;Cxx_tests_failing_test2013_96_C\;Cxx_tests_failing_test2014_100_C\;Cxx_tests_failing_test2014_101_C\;Cxx_tests_failing_test2014_102_C\;Cxx_tests_failing_test2014_103_C\;Cxx_tests_failing_test2014_104_C\;Cxx_tests_failing_test2014_105_C\;Cxx_tests_failing_test2014_106_C\;Cxx_tests_failing_test2014_110_C\;Cxx_tests_failing_test2014_113_C\;Cxx_tests_failing_test2014_118_C\;Cxx_tests_failing_test2014_119_C\;Cxx_tests_failing_test2014_120_C\;Cxx_tests_failing_test2014_121_C\;Cxx_tests_failing_test2014_138_C\;Cxx_tests_failing_test2014_14_C\;Cxx_tests_failing_test2014_158_C\;Cxx_tests_failing_test2014_159_C\;Cxx_tests_failing_test2014_15_C\;Cxx_tests_failing_test2014_164_C\;Cxx_tests_failing_test2014_173_C\;Cxx_tests_failing_test2014_175_C\;Cxx_tests_failing_test2014_180_C\;Cxx_tests_failing_test2014_181_C\;Cxx_tests_failing_test2014_211_C\;Cxx_tests_failing_test2014_213_C\;Cxx_tests_failing_test2014_214_C\;Cxx_tests_failing_test2014_227_C\;Cxx_tests_failing_test2014_228_C\;Cxx_tests_failing_test2014_229_C\;Cxx_tests_failing_test2014_22_C\;Cxx_tests_failing_test2014_237_C\;Cxx_tests_failing_test2014_23_C\;Cxx_tests_failing_test2014_25_C\;Cxx_tests_failing_test2014_42_C\;Cxx_tests_failing_test2014_44_C\;Cxx_tests_failing_test2014_46_C\;Cxx_tests_failing_test2014_51_C\;Cxx_tests_failing_test2014_52_C\;Cxx_tests_failing_test2014_59_C\;Cxx_tests_failing_test2014_63_C\;Cxx_tests_failing_test2014_64_C\;Cxx_tests_failing_test2014_65_C\;Cxx_tests_failing_test2014_66_C\;Cxx_tests_failing_test2014_69_C\;Cxx_tests_failing_test2014_99_C\;Cxx_tests_failing_test2015_100_C\;Cxx_tests_failing_test2015_105_C\;Cxx_tests_failing_test2015_109_C\;Cxx_tests_failing_test2015_126_C\;Cxx_tests_failing_test2015_127_C\;Cxx_tests_failing_test2015_129_C\;Cxx_tests_failing_test2015_135_C\;Cxx_tests_failing_test2015_136_C\;Cxx_tests_failing_test2015_25_C\;Cxx_tests_failing_test2015_62_C\;Cxx_tests_failing_test2015_63_C\;Cxx_tests_failing_test2015_64_C\;Cxx_tests_failing_test2015_70_C\;Cxx_tests_failing_test2015_72_icpc_C\;Cxx_tests_failing_test2015_81_C\;Cxx_tests_failing_test2015_82_C\;Cxx_tests_failing_test2015_83_C\;Cxx_tests_failing_test2015_84_C\;Cxx_tests_failing_test2015_87_C\;Cxx_tests_failing_test2015_90_C\;Cxx_tests_failing_test2015_91_C\;Cxx_tests_failing_test2015_92_C\;Cxx_tests_failing_test2015_93_C\;Cxx_tests_failing_test2015_94_C\;Cxx_tests_failing_test2015_95_C\;Cxx_tests_failing_test2015_96_C\;Cxx_tests_failing_test2015_98_C\;Cxx_tests_failing_test2016_20_C\;Cxx_tests_failing_test2016_24_C\;Cxx_tests_failing_test2016_35_C\;Cxx_tests_failing_test2016_39_C\;Cxx_tests_failing_test2016_52_C\;Cxx_tests_failing_test2016_58_C\;Cxx_tests_failing_test2016_82_C\;Cxx_tests_failing_test2016_90_C\;Cxx_tests_failing_test2016_92_C\;Cxx_tests_failing_test2016_94_C\;Cxx_tests_failing_test2016_95_C\;Cxx_tests_failing_test2017_05_C\;Cxx_tests_failing_test2017_06_C\;Cxx_tests_failing_test2017_07_C\;Cxx_tests_failing_test2017_08_C\;Cxx_tests_failing_test2017_12_C\;Cxx_tests_failing_test2017_20_C\;Cxx_tests_failing_test2017_24_C\;Cxx_tests_failing_test2017_27_C\;Cxx_tests_failing_test2017_28_C\;Cxx_tests_failing_test2017_31_C\;Cxx_tests_failing_test2017_32_C\;Cxx_tests_failing_test2017_33_C\;Cxx_tests_failing_test2017_34_C\;Cxx_tests_failing_test2017_44_C\;Cxx_tests_failing_test2017_46_C\;Cxx_tests_failing_test2017_47_C\;Cxx_tests_failing_test2017_63_C\;Cxx_tests_failing_test2017_67_C\;Cxx_tests_failing_test2017_69_C\;Cxx_tests_failing_test2017_85_C\;Cxx_tests_failing_test2017_86_C\;Cxx_tests_failing_test2017_99_C\;Cxx_tests_failing_test2018_01_C\;Cxx_tests_failing_test2018_22_C\;Cxx_tests_failing_test2018_23_C\;Cxx_tests_failing_test2018_25_C\;Cxx_tests_failing_test2018_26_C\;Cxx_tests_failing_test2018_28_C\;Cxx_tests_failing_test2018_31_C\;Cxx_tests_failing_test2019_25_C\;Cxx_tests_failing_test2020_35_C\;Cxx_tests_failing_test2020_36_C\;Cxx_tests_failing_test_gnu_C\;Cxx_tests_failing_vector_cpp_C\;Cxx_tests_fstream_C\;Cxx_tests_inputBug317_C\;Cxx_tests_inputBug327_C\;Cxx_tests_inputForLoopLocator_C\;Cxx_tests_lexPhase2003_01_C\;Cxx_tests_luleshTALC_C\;Cxx_tests_lulesh_C\;Cxx_tests_method-defn-in-tpldecl-0_C\;Cxx_tests_method-defn-in-tpldecl-1_C\;Cxx_tests_rex_test2025_defaulted_special_members_anon_union_cpp\;Cxx_tests_rex_test2025_dependent_template_specialization_type_cpp\;Cxx_tests_rex_test2025_extern_c_brace_roundtrip_cpp\;Cxx_tests_rex_test2025_extern_c_roundtrip_cpp\;Cxx_tests_rex_test2025_issue107_chrono_cpp\;Cxx_tests_rex_test2025_issue107_enable_if_cpp\;Cxx_tests_rex_test2025_issue107_redundant_cpp\;Cxx_tests_rex_test2025_issue113_instantiation_spelling_cpp\;Cxx_tests_rex_test2025_issue115_dependent_member_cpp\;Cxx_tests_rex_test2025_issue115_qualified_unresolved_lookup_cpp\;Cxx_tests_rex_test2025_issue115_unresolved_lookup_cpp\;Cxx_tests_rex_test2025_issue118_empty_decl_cpp\;Cxx_tests_rex_test2025_issue123_std_double_colon_cpp\;Cxx_tests_rex_test2025_issue124_system_header_specialization_args_cpp\;Cxx_tests_rex_test2025_issue125_fixup_child_list_warning_cpp\;Cxx_tests_rex_test2025_issue126_default_template_args_cpp\;Cxx_tests_rex_test2025_issue146_shadowed_function_template_cpp\;Cxx_tests_rex_test2025_issue146_std_iota_quad_colon_cpp\;Cxx_tests_rex_test2025_issue149_param_list_unique_name_cpp\;Cxx_tests_rex_test2025_issue150_name_qualification_map_cpp\;Cxx_tests_rex_test2025_issue160_axpy_explicit_template_call_cpp\;Cxx_tests_rex_test2025_issue160_axpy_roundtrip_cpp\;Cxx_tests_rex_test2025_issue160_axpy_system_headers_cpp\;Cxx_tests_rex_test2025_issue160_explicit_class_instantiation_cpp\;Cxx_tests_rex_test2025_issue160_explicit_empty_template_id_cpp\;Cxx_tests_rex_test2025_issue160_explicit_struct_instantiation_cpp\;Cxx_tests_rex_test2025_issue160_extern_class_instantiation_cpp\;Cxx_tests_rex_test2025_issue160_system_header_class_instantiation_cpp\;Cxx_tests_rex_test2025_issue160_system_header_instantiation_cpp\;Cxx_tests_rex_test2025_issue163_axpy_roundtrip_cpp\;Cxx_tests_rex_test2025_issue164_constexpr_const_cpp\;Cxx_tests_rex_test2025_issue59_template_template_arg_global_qual_cpp\;Cxx_tests_rex_test2025_issue60_friend_function_symbol_scope_cpp\;Cxx_tests_rex_test2025_issue61_function_decl_chain_cpp\;Cxx_tests_rex_test2025_issue63_pseudo_dtor_typedef_ptr_cpp\;Cxx_tests_rex_test2025_issue64_shadow_template_global_qual_cpp\;Cxx_tests_rex_test2025_issue69_member_class_out_of_line_cpp\;Cxx_tests_rex_test2025_issue73_token_stream_mapping_cpp\;Cxx_tests_rex_test2025_issue83_collisions_cpp\;Cxx_tests_rex_test2025_issue83_cpp\;Cxx_tests_rex_test2025_issue83_redeclarations_cpp\;Cxx_tests_rex_test2025_issue84_friend_template_in_class_cpp\;Cxx_tests_rex_test2025_issue84_function_template_cpp\;Cxx_tests_rex_test2025_issue88_cpp\;Cxx_tests_rex_test2025_issue89_alias_cpp\;Cxx_tests_rex_test2025_issue90_array_dim_cpp\;Cxx_tests_rex_test2025_issue91_decl_chain_cpp\;Cxx_tests_rex_test2025_issue92_redundant_sym_cpp\;Cxx_tests_rex_test2025_issue93_static_assert_order_cpp\;Cxx_tests_rex_test2025_issue94_template_arg_spacing_cpp\;Cxx_tests_rex_test2025_issue99_recursive_cpp\;Cxx_tests_rex_test2025_issue99_uaf_cpp\;Cxx_tests_rex_test2025_mangle_qualifiers_no_template_syntax_cpp\;Cxx_tests_rex_test2025_member_template_call_cpp\;Cxx_tests_rex_test2025_namespace_reopenings_preserve_blocks_cpp\;Cxx_tests_rex_test2025_nonreal_type_global_qualifier_cpp\;Cxx_tests_rex_test2025_template_argument_pack_cpp\;Cxx_tests_rex_test2025_template_call_cpp\;Cxx_tests_rex_test2025_variadic_template_params_cpp\;Cxx_tests_rose-1431-0_C\;Cxx_tests_rose-1540-0_C\;Cxx_tests_rose-1541-0_C\;Cxx_tests_rose-1701-0_C\;Cxx_tests_rose-1766_C\;Cxx_tests_rose-1789-001_C\;Cxx_tests_rose-1814_C\;Cxx_tests_rose-1834_C\;Cxx_tests_rose-1839_C\;Cxx_tests_rose-1846_C\;Cxx_tests_rose-1860_C\;Cxx_tests_rose-1998_C\;Cxx_tests_rose-2000-01_C\;Cxx_tests_rose-2000-02_C\;Cxx_tests_rose-2007-01_C\;Cxx_tests_rose-2007-02_C\;Cxx_tests_rose-2008_C\;Cxx_tests_rose-2073_C\;Cxx_tests_test2001_01_C\;Cxx_tests_test2001_02_C\;Cxx_tests_test2001_03_C\;Cxx_tests_test2001_04_C\;Cxx_tests_test2001_05_C\;Cxx_tests_test2001_06_C\;Cxx_tests_test2001_07_C\;Cxx_tests_test2001_08_C\;Cxx_tests_test2001_09_C\;Cxx_tests_test2001_10_C\;Cxx_tests_test2001_11_C\;Cxx_tests_test2001_12_C\;Cxx_tests_test2001_13_C\;Cxx_tests_test2001_14_C\;Cxx_tests_test2001_15_C\;Cxx_tests_test2001_16_C\;Cxx_tests_test2001_17_C\;Cxx_tests_test2001_18_C\;Cxx_tests_test2001_19_C\;Cxx_tests_test2001_20_C\;Cxx_tests_test2001_21_C\;Cxx_tests_test2001_22_C\;Cxx_tests_test2001_23_C\;Cxx_tests_test2001_24_C\;Cxx_tests_test2001_25_C\;Cxx_tests_test2001_26_C\;Cxx_tests_test2001_27_C\;Cxx_tests_test2001_28_C\;Cxx_tests_test2001_29_C\;Cxx_tests_test2002_01_C\;Cxx_tests_test2003_01_C\;Cxx_tests_test2003_02_C\;Cxx_tests_test2003_03_C\;Cxx_tests_test2003_04_C\;Cxx_tests_test2003_05_C\;Cxx_tests_test2003_06_C\;Cxx_tests_test2003_08_C\;Cxx_tests_test2003_09_C\;Cxx_tests_test2003_10_C\;Cxx_tests_test2003_11_C\;Cxx_tests_test2003_12_C\;Cxx_tests_test2003_13_C\;Cxx_tests_test2003_14_C\;Cxx_tests_test2003_15_C\;Cxx_tests_test2003_16_C\;Cxx_tests_test2003_17_C\;Cxx_tests_test2003_18_C\;Cxx_tests_test2003_19_C\;Cxx_tests_test2003_20_C\;Cxx_tests_test2003_21_C\;Cxx_tests_test2003_23_C\;Cxx_tests_test2003_24_C\;Cxx_tests_test2003_25_C\;Cxx_tests_test2003_26_C\;Cxx_tests_test2003_27_C\;Cxx_tests_test2003_28_C\;Cxx_tests_test2003_29_C\;Cxx_tests_test2003_30_C\;Cxx_tests_test2003_31_C\;Cxx_tests_test2003_32_C\;Cxx_tests_test2003_35_C\;Cxx_tests_test2003_36_C\;Cxx_tests_test2003_37_C\;Cxx_tests_test2003_38_C\;Cxx_tests_test2004_01_C\;Cxx_tests_test2004_02_C\;Cxx_tests_test2004_03_C\;Cxx_tests_test2004_04_C\;Cxx_tests_test2004_05_C\;Cxx_tests_test2004_06_C\;Cxx_tests_test2004_07_C\;Cxx_tests_test2004_08_C\;Cxx_tests_test2004_09_C\;Cxx_tests_test2004_100_C\;Cxx_tests_test2004_101_C\;Cxx_tests_test2004_102_C\;Cxx_tests_test2004_103_C\;Cxx_tests_test2004_104_C\;Cxx_tests_test2004_105_C\;Cxx_tests_test2004_106_C\;Cxx_tests_test2004_107_C\;Cxx_tests_test2004_108_C\;Cxx_tests_test2004_109_C\;Cxx_tests_test2004_10_C\;Cxx_tests_test2004_110_C\;Cxx_tests_test2004_111_C\;Cxx_tests_test2004_112_C\;Cxx_tests_test2004_113_C\;Cxx_tests_test2004_114_C\;Cxx_tests_test2004_115_C\;Cxx_tests_test2004_116_C\;Cxx_tests_test2004_118_C\;Cxx_tests_test2004_119_C\;Cxx_tests_test2004_11_C\;Cxx_tests_test2004_120_C\;Cxx_tests_test2004_122_C\;Cxx_tests_test2004_123_C\;Cxx_tests_test2004_124_C\;Cxx_tests_test2004_125_C\;Cxx_tests_test2004_127_C\;Cxx_tests_test2004_128_C\;Cxx_tests_test2004_129_C\;Cxx_tests_test2004_12_C\;Cxx_tests_test2004_130_C\;Cxx_tests_test2004_131_C\;Cxx_tests_test2004_132_C\;Cxx_tests_test2004_133_C\;Cxx_tests_test2004_134_C\;Cxx_tests_test2004_135_C\;Cxx_tests_test2004_136_C\;Cxx_tests_test2004_137_C\;Cxx_tests_test2004_138_C\;Cxx_tests_test2004_139_C\;Cxx_tests_test2004_13_C\;Cxx_tests_test2004_140_C\;Cxx_tests_test2004_141_C\;Cxx_tests_test2004_142_C\;Cxx_tests_test2004_143_C\;Cxx_tests_test2004_144_C\;Cxx_tests_test2004_145_C\;Cxx_tests_test2004_146_C\;Cxx_tests_test2004_147_C\;Cxx_tests_test2004_148_C\;Cxx_tests_test2004_149_C\;Cxx_tests_test2004_14_C\;Cxx_tests_test2004_150_C\;Cxx_tests_test2004_151_C\;Cxx_tests_test2004_152_C\;Cxx_tests_test2004_153_C\;Cxx_tests_test2004_154_C\;Cxx_tests_test2004_155_C\;Cxx_tests_test2004_156_C\;Cxx_tests_test2004_157_C\;Cxx_tests_test2004_158_C\;Cxx_tests_test2004_159_C\;Cxx_tests_test2004_15_C\;Cxx_tests_test2004_160_C\;Cxx_tests_test2004_161_C\;Cxx_tests_test2004_162_C\;Cxx_tests_test2004_163_C\;Cxx_tests_test2004_164_C\;Cxx_tests_test2004_16_C\;Cxx_tests_test2004_17_C\;Cxx_tests_test2004_18_C\;Cxx_tests_test2004_19_C\;Cxx_tests_test2004_23_C\;Cxx_tests_test2004_24_C\;Cxx_tests_test2004_26_C\;Cxx_tests_test2004_27_C\;Cxx_tests_test2004_28_C\;Cxx_tests_test2004_29_C\;Cxx_tests_test2004_30_C\;Cxx_tests_test2004_31_C\;Cxx_tests_test2004_32_C\;Cxx_tests_test2004_33_C\;Cxx_tests_test2004_34_C\;Cxx_tests_test2004_36_C\;Cxx_tests_test2004_37_C\;Cxx_tests_test2004_38_C\;Cxx_tests_test2004_39_C\;Cxx_tests_test2004_40_C\;Cxx_tests_test2004_42_C\;Cxx_tests_test2004_43_C\;Cxx_tests_test2004_44_C\;Cxx_tests_test2004_45_C\;Cxx_tests_test2004_47_C\;Cxx_tests_test2004_48_C\;Cxx_tests_test2004_49_C\;Cxx_tests_test2004_50_C\;Cxx_tests_test2004_51_C\;Cxx_tests_test2004_52_C\;Cxx_tests_test2004_53_C\;Cxx_tests_test2004_54_C\;Cxx_tests_test2004_55_C\;Cxx_tests_test2004_56_C\;Cxx_tests_test2004_58_C\;Cxx_tests_test2004_59_C\;Cxx_tests_test2004_60_C\;Cxx_tests_test2004_61_C\;Cxx_tests_test2004_62_C\;Cxx_tests_test2004_63_C\;Cxx_tests_test2004_64_C\;Cxx_tests_test2004_65_C\;Cxx_tests_test2004_67_C\;Cxx_tests_test2004_68_C\;Cxx_tests_test2004_69_C\;Cxx_tests_test2004_70_C\;Cxx_tests_test2004_71_C\;Cxx_tests_test2004_72_C\;Cxx_tests_test2004_73_C\;Cxx_tests_test2004_74_C\;Cxx_tests_test2004_75_C\;Cxx_tests_test2004_76_C\;Cxx_tests_test2004_78_C\;Cxx_tests_test2004_79_C\;Cxx_tests_test2004_80_C\;Cxx_tests_test2004_81_C\;Cxx_tests_test2004_83_C\;Cxx_tests_test2004_84_C\;Cxx_tests_test2004_86_C\;Cxx_tests_test2004_87_C\;Cxx_tests_test2004_88_C\;Cxx_tests_test2004_89_C\;Cxx_tests_test2004_90_C\;Cxx_tests_test2004_91_C\;Cxx_tests_test2004_93_C\;Cxx_tests_test2004_94_C\;Cxx_tests_test2004_95_C\;Cxx_tests_test2004_96_C\;Cxx_tests_test2004_97_C\;Cxx_tests_test2004_98_C\;Cxx_tests_test2004_99_C\;Cxx_tests_test2005_01_C\;Cxx_tests_test2005_02_C\;Cxx_tests_test2005_03_C\;Cxx_tests_test2005_04_C\;Cxx_tests_test2005_05_C\;Cxx_tests_test2005_07_C\;Cxx_tests_test2005_08_C\;Cxx_tests_test2005_09_C\;Cxx_tests_test2005_100_C\;Cxx_tests_test2005_101_C\;Cxx_tests_test2005_102_C\;Cxx_tests_test2005_103_C\;Cxx_tests_test2005_104_C\;Cxx_tests_test2005_105_C\;Cxx_tests_test2005_106_C\;Cxx_tests_test2005_107_C\;Cxx_tests_test2005_108_C\;Cxx_tests_test2005_109_C\;Cxx_tests_test2005_10_C\;Cxx_tests_test2005_110_C\;Cxx_tests_test2005_111_C\;Cxx_tests_test2005_112_C\;Cxx_tests_test2005_113_C\;Cxx_tests_test2005_114_C\;Cxx_tests_test2005_116_C\;Cxx_tests_test2005_117_C\;Cxx_tests_test2005_118_C\;Cxx_tests_test2005_119_C\;Cxx_tests_test2005_11_C\;Cxx_tests_test2005_120_C\;Cxx_tests_test2005_121_C\;Cxx_tests_test2005_122_C\;Cxx_tests_test2005_123_C\;Cxx_tests_test2005_124_C\;Cxx_tests_test2005_125_C\;Cxx_tests_test2005_126_C\;Cxx_tests_test2005_127_C\;Cxx_tests_test2005_128_C\;Cxx_tests_test2005_129_C\;Cxx_tests_test2005_12_C\;Cxx_tests_test2005_130_C\;Cxx_tests_test2005_131_C\;Cxx_tests_test2005_132_C\;Cxx_tests_test2005_133_C\;Cxx_tests_test2005_134_C\;Cxx_tests_test2005_135_C\;Cxx_tests_test2005_136_C\;Cxx_tests_test2005_137_C\;Cxx_tests_test2005_138_C\;Cxx_tests_test2005_139_C\;Cxx_tests_test2005_13_C\;Cxx_tests_test2005_140_C\;Cxx_tests_test2005_141_C\;Cxx_tests_test2005_142_C\;Cxx_tests_test2005_143_C\;Cxx_tests_test2005_144_C\;Cxx_tests_test2005_145_C\;Cxx_tests_test2005_146_C\;Cxx_tests_test2005_147_C\;Cxx_tests_test2005_148_C\;Cxx_tests_test2005_149_C\;Cxx_tests_test2005_14_C\;Cxx_tests_test2005_150_C\;Cxx_tests_test2005_152_C\;Cxx_tests_test2005_153_C\;Cxx_tests_test2005_154_C\;Cxx_tests_test2005_155_C\;Cxx_tests_test2005_156_C\;Cxx_tests_test2005_157_C\;Cxx_tests_test2005_158_C\;Cxx_tests_test2005_159_C\;Cxx_tests_test2005_15_C\;Cxx_tests_test2005_160_C\;Cxx_tests_test2005_161_C\;Cxx_tests_test2005_162_C\;Cxx_tests_test2005_163_C\;Cxx_tests_test2005_164_C\;Cxx_tests_test2005_165_C\;Cxx_tests_test2005_166_C\;Cxx_tests_test2005_167_C\;Cxx_tests_test2005_168_C\;Cxx_tests_test2005_169_C\;Cxx_tests_test2005_16_C\;Cxx_tests_test2005_170_C\;Cxx_tests_test2005_171_C\;Cxx_tests_test2005_172_C\;Cxx_tests_test2005_173_C\;Cxx_tests_test2005_174_C\;Cxx_tests_test2005_175_C\;Cxx_tests_test2005_176_C\;Cxx_tests_test2005_177_C\;Cxx_tests_test2005_178_C\;Cxx_tests_test2005_179_C\;Cxx_tests_test2005_17_C\;Cxx_tests_test2005_180_C\;Cxx_tests_test2005_181_C\;Cxx_tests_test2005_182_C\;Cxx_tests_test2005_183_C\;Cxx_tests_test2005_184_C\;Cxx_tests_test2005_185_C\;Cxx_tests_test2005_187_C\;Cxx_tests_test2005_189_C\;Cxx_tests_test2005_190_C\;Cxx_tests_test2005_191_C\;Cxx_tests_test2005_192_C\;Cxx_tests_test2005_193_C\;Cxx_tests_test2005_194_C\;Cxx_tests_test2005_195_C\;Cxx_tests_test2005_196_C\;Cxx_tests_test2005_197_C\;Cxx_tests_test2005_198_C\;Cxx_tests_test2005_199_C\;Cxx_tests_test2005_19_C\;Cxx_tests_test2005_202_C\;Cxx_tests_test2005_203_C\;Cxx_tests_test2005_206_C\;Cxx_tests_test2005_207_C\;Cxx_tests_test2005_208_C\;Cxx_tests_test2005_209_C\;Cxx_tests_test2005_20_C\;Cxx_tests_test2005_21_C\;Cxx_tests_test2005_22_C\;Cxx_tests_test2005_23_C\;Cxx_tests_test2005_24_C\;Cxx_tests_test2005_25_C\;Cxx_tests_test2005_26_C\;Cxx_tests_test2005_27_C\;Cxx_tests_test2005_28_C\;Cxx_tests_test2005_29_C\;Cxx_tests_test2005_30_C\;Cxx_tests_test2005_31_C\;Cxx_tests_test2005_32_C\;Cxx_tests_test2005_33_C\;Cxx_tests_test2005_34_C\;Cxx_tests_test2005_35_C\;Cxx_tests_test2005_36_C\;Cxx_tests_test2005_37_C\;Cxx_tests_test2005_38_C\;Cxx_tests_test2005_39_C\;Cxx_tests_test2005_40_C\;Cxx_tests_test2005_41_C\;Cxx_tests_test2005_43_C\;Cxx_tests_test2005_44_C\;Cxx_tests_test2005_45_C\;Cxx_tests_test2005_46_C\;Cxx_tests_test2005_47_C\;Cxx_tests_test2005_48_C\;Cxx_tests_test2005_49_C\;Cxx_tests_test2005_51_C\;Cxx_tests_test2005_52_C\;Cxx_tests_test2005_53_C\;Cxx_tests_test2005_55_C\;Cxx_tests_test2005_57_C\;Cxx_tests_test2005_58_C\;Cxx_tests_test2005_59_C\;Cxx_tests_test2005_60_C\;Cxx_tests_test2005_61_C\;Cxx_tests_test2005_62_C\;Cxx_tests_test2005_63_C\;Cxx_tests_test2005_64_C\;Cxx_tests_test2005_65_C\;Cxx_tests_test2005_66_C\;Cxx_tests_test2005_67_C\;Cxx_tests_test2005_68_C\;Cxx_tests_test2005_70_C\;Cxx_tests_test2005_71_C\;Cxx_tests_test2005_72_C\;Cxx_tests_test2005_73_C\;Cxx_tests_test2005_74_C\;Cxx_tests_test2005_75a_C\;Cxx_tests_test2005_75b_C\;Cxx_tests_test2005_76_C\;Cxx_tests_test2005_77_C\;Cxx_tests_test2005_78_C\;Cxx_tests_test2005_79_C\;Cxx_tests_test2005_80_C\;Cxx_tests_test2005_81_C\;Cxx_tests_test2005_82_C\;Cxx_tests_test2005_83_C\;Cxx_tests_test2005_84_C\;Cxx_tests_test2005_86_C\;Cxx_tests_test2005_87_C\;Cxx_tests_test2005_88_C\;Cxx_tests_test2005_89_C\;Cxx_tests_test2005_90_C\;Cxx_tests_test2005_91_C\;Cxx_tests_test2005_92_C\;Cxx_tests_test2005_93_C\;Cxx_tests_test2005_94_C\;Cxx_tests_test2005_95_C\;Cxx_tests_test2005_96_C\;Cxx_tests_test2005_97_C\;Cxx_tests_test2005_98_C\;Cxx_tests_test2005_99_C\;Cxx_tests_test2006_01_C\;Cxx_tests_test2006_02_C\;Cxx_tests_test2006_03_C\;Cxx_tests_test2006_04_C\;Cxx_tests_test2006_05_C\;Cxx_tests_test2006_06_C\;Cxx_tests_test2006_07_C\;Cxx_tests_test2006_101_C\;Cxx_tests_test2006_102_C\;Cxx_tests_test2006_103_C\;Cxx_tests_test2006_104_C\;Cxx_tests_test2006_105_C\;Cxx_tests_test2006_106_C\;Cxx_tests_test2006_107_C\;Cxx_tests_test2006_108_C\;Cxx_tests_test2006_109_C\;Cxx_tests_test2006_10_C\;Cxx_tests_test2006_111_C\;Cxx_tests_test2006_113_C\;Cxx_tests_test2006_114_C\;Cxx_tests_test2006_115_C\;Cxx_tests_test2006_116_C\;Cxx_tests_test2006_117_C\;Cxx_tests_test2006_118_C\;Cxx_tests_test2006_119_C\;Cxx_tests_test2006_120_C\;Cxx_tests_test2006_121_C\;Cxx_tests_test2006_122_C\;Cxx_tests_test2006_123_C\;Cxx_tests_test2006_124_C\;Cxx_tests_test2006_125_C\;Cxx_tests_test2006_126_C\;Cxx_tests_test2006_129_C\;Cxx_tests_test2006_12_C\;Cxx_tests_test2006_139_C\;Cxx_tests_test2006_13_C\;Cxx_tests_test2006_140_C\;Cxx_tests_test2006_141_C\;Cxx_tests_test2006_142_C\;Cxx_tests_test2006_143_C\;Cxx_tests_test2006_144_C\;Cxx_tests_test2006_145_C\;Cxx_tests_test2006_146_C\;Cxx_tests_test2006_147_C\;Cxx_tests_test2006_148_C\;Cxx_tests_test2006_149_C\;Cxx_tests_test2006_14_C\;Cxx_tests_test2006_150_C\;Cxx_tests_test2006_151_C\;Cxx_tests_test2006_152_C\;Cxx_tests_test2006_153_C\;Cxx_tests_test2006_154_C\;Cxx_tests_test2006_155_C\;Cxx_tests_test2006_156_C\;Cxx_tests_test2006_157_C\;Cxx_tests_test2006_158_C\;Cxx_tests_test2006_159_C\;Cxx_tests_test2006_160_C\;Cxx_tests_test2006_161_C\;Cxx_tests_test2006_162_C\;Cxx_tests_test2006_163_C\;Cxx_tests_test2006_164_C\;Cxx_tests_test2006_165_C\;Cxx_tests_test2006_166_C\;Cxx_tests_test2006_16_C\;Cxx_tests_test2006_177_C\;Cxx_tests_test2006_178_C\;Cxx_tests_test2006_179_C\;Cxx_tests_test2006_17_C\;Cxx_tests_test2006_181_C\;Cxx_tests_test2006_183_C\;Cxx_tests_test2006_185_C\;Cxx_tests_test2006_189_C\;Cxx_tests_test2006_18_C\;Cxx_tests_test2006_19_C\;Cxx_tests_test2006_22_C\;Cxx_tests_test2006_23_C\;Cxx_tests_test2006_25_C\;Cxx_tests_test2006_26_C\;Cxx_tests_test2006_28_C\;Cxx_tests_test2006_34_C\;Cxx_tests_test2006_38_C\;Cxx_tests_test2006_39_C\;Cxx_tests_test2006_40_C\;Cxx_tests_test2006_41_C\;Cxx_tests_test2006_42_C\;Cxx_tests_test2006_44_C\;Cxx_tests_test2006_46_C\;Cxx_tests_test2006_47_C\;Cxx_tests_test2006_50_C\;Cxx_tests_test2006_52_C\;Cxx_tests_test2006_53_C\;Cxx_tests_test2006_54_C\;Cxx_tests_test2006_55_C\;Cxx_tests_test2006_56_C\;Cxx_tests_test2006_57_C\;Cxx_tests_test2006_58_C\;Cxx_tests_test2006_59_C\;Cxx_tests_test2006_61_C\;Cxx_tests_test2006_62_C\;Cxx_tests_test2006_63_C\;Cxx_tests_test2006_66_C\;Cxx_tests_test2006_67_C\;Cxx_tests_test2006_68_C\;Cxx_tests_test2006_69_C\;Cxx_tests_test2006_70_C\;Cxx_tests_test2006_71_C\;Cxx_tests_test2006_72_C\;Cxx_tests_test2006_73_C\;Cxx_tests_test2006_74_C\;Cxx_tests_test2006_75_C\;Cxx_tests_test2006_76_C\;Cxx_tests_test2006_77_C\;Cxx_tests_test2006_78_C\;Cxx_tests_test2006_79_C\;Cxx_tests_test2006_80_C\;Cxx_tests_test2006_81_C\;Cxx_tests_test2006_82_C\;Cxx_tests_test2006_83_C\;Cxx_tests_test2006_84_C\;Cxx_tests_test2006_85_C\;Cxx_tests_test2006_86_C\;Cxx_tests_test2006_87_C\;Cxx_tests_test2006_88_C\;Cxx_tests_test2006_89_C\;Cxx_tests_test2006_90_C\;Cxx_tests_test2006_91_C\;Cxx_tests_test2006_92_C\;Cxx_tests_test2006_93_C\;Cxx_tests_test2006_94_C\;Cxx_tests_test2006_95_C\;Cxx_tests_test2006_96_C\;Cxx_tests_test2006_97_C\;Cxx_tests_test2006_98_C\;Cxx_tests_test2006_99_C\;Cxx_tests_test2007_01_C\;Cxx_tests_test2007_02_C\;Cxx_tests_test2007_03_C\;Cxx_tests_test2007_04_C\;Cxx_tests_test2007_05_C\;Cxx_tests_test2007_06_C\;Cxx_tests_test2007_07_C\;Cxx_tests_test2007_08_C\;Cxx_tests_test2007_09_C\;Cxx_tests_test2007_102_C\;Cxx_tests_test2007_103_C\;Cxx_tests_test2007_10_C\;Cxx_tests_test2007_11_C\;Cxx_tests_test2007_128_C\;Cxx_tests_test2007_12_C\;Cxx_tests_test2007_130_C\;Cxx_tests_test2007_133_C\;Cxx_tests_test2007_135_C\;Cxx_tests_test2007_137_C\;Cxx_tests_test2007_13_C\;Cxx_tests_test2007_140_C\;Cxx_tests_test2007_142_C\;Cxx_tests_test2007_147_C\;Cxx_tests_test2007_14_C\;Cxx_tests_test2007_150_C\;Cxx_tests_test2007_151_C\;Cxx_tests_test2007_152_C\;Cxx_tests_test2007_154_C\;Cxx_tests_test2007_155_C\;Cxx_tests_test2007_157_C\;Cxx_tests_test2007_158_C\;Cxx_tests_test2007_159_C\;Cxx_tests_test2007_15_C\;Cxx_tests_test2007_160_C\;Cxx_tests_test2007_161_C\;Cxx_tests_test2007_162_C\;Cxx_tests_test2007_163_C\;Cxx_tests_test2007_164_C\;Cxx_tests_test2007_165_C\;Cxx_tests_test2007_166_C\;Cxx_tests_test2007_167_C\;Cxx_tests_test2007_169_C\;Cxx_tests_test2007_16_C\;Cxx_tests_test2007_170_C\;Cxx_tests_test2007_171_C\;Cxx_tests_test2007_172_C\;Cxx_tests_test2007_173_C\;Cxx_tests_test2007_17_C\;Cxx_tests_test2007_21_C\;Cxx_tests_test2007_23_C\;Cxx_tests_test2007_24_C\;Cxx_tests_test2007_25_C\;Cxx_tests_test2007_26_C\;Cxx_tests_test2007_28_C\;Cxx_tests_test2007_29_C\;Cxx_tests_test2007_38_C\;Cxx_tests_test2007_40_C\;Cxx_tests_test2007_41_C\;Cxx_tests_test2007_43_C\;Cxx_tests_test2007_44_C\;Cxx_tests_test2007_46_C\;Cxx_tests_test2007_47_C\;Cxx_tests_test2007_48_C\;Cxx_tests_test2007_49_C\;Cxx_tests_test2007_50_C\;Cxx_tests_test2007_53_C\;Cxx_tests_test2007_55_C\;Cxx_tests_test2007_57_C\;Cxx_tests_test2007_59_C\;Cxx_tests_test2007_70_C\;Cxx_tests_test2007_72_C\;Cxx_tests_test2007_75_C\;Cxx_tests_test2007_77_C\;Cxx_tests_test2007_78_C\;Cxx_tests_test2007_81_C\;Cxx_tests_test2007_82_C\;Cxx_tests_test2007_83_C\;Cxx_tests_test2007_84_C\;Cxx_tests_test2007_85_C\;Cxx_tests_test2007_90_C\;Cxx_tests_test2007_94_C\;Cxx_tests_test2007_98_C\;Cxx_tests_test2008_01_C\;Cxx_tests_test2008_03_C\;Cxx_tests_test2008_04_C\;Cxx_tests_test2008_05_C\;Cxx_tests_test2008_06_C\;Cxx_tests_test2008_08_C\;Cxx_tests_test2008_09_C\;Cxx_tests_test2008_10_C\;Cxx_tests_test2008_11_C\;Cxx_tests_test2008_12_C\;Cxx_tests_test2008_13_C\;Cxx_tests_test2008_14_C\;Cxx_tests_test2008_16_C\;Cxx_tests_test2009_01_C\;Cxx_tests_test2009_02_C\;Cxx_tests_test2009_03_C\;Cxx_tests_test2009_04_C\;Cxx_tests_test2009_05_C\;Cxx_tests_test2009_06_C\;Cxx_tests_test2009_08_C\;Cxx_tests_test2009_09_C\;Cxx_tests_test2009_10_C\;Cxx_tests_test2009_11_C\;Cxx_tests_test2009_12_C\;Cxx_tests_test2009_14_C\;Cxx_tests_test2009_15_C\;Cxx_tests_test2009_18_C\;Cxx_tests_test2009_19_C\;Cxx_tests_test2009_20_C\;Cxx_tests_test2009_24_C\;Cxx_tests_test2009_27_C\;Cxx_tests_test2009_28_C\;Cxx_tests_test2009_30_C\;Cxx_tests_test2009_31_C\;Cxx_tests_test2009_37_C\;Cxx_tests_test2009_39_C\;Cxx_tests_test2009_40_C\;Cxx_tests_test2009_41_C\;Cxx_tests_test2009_42_C\;Cxx_tests_test2010_03_C\;Cxx_tests_test2010_04_C\;Cxx_tests_test2010_05_C\;Cxx_tests_test2010_06_C\;Cxx_tests_test2010_18_C\;Cxx_tests_test2010_19_C\;Cxx_tests_test2010_24_C\;Cxx_tests_test2011_100_C\;Cxx_tests_test2011_101_C\;Cxx_tests_test2011_110_C\;Cxx_tests_test2011_111_C\;Cxx_tests_test2011_115_C\;Cxx_tests_test2011_117_C\;Cxx_tests_test2011_118_C\;Cxx_tests_test2011_119_C\;Cxx_tests_test2011_120_C\;Cxx_tests_test2011_121_C\;Cxx_tests_test2011_133_C\;Cxx_tests_test2011_135_C\;Cxx_tests_test2011_136_C\;Cxx_tests_test2011_137_C\;Cxx_tests_test2011_138_C\;Cxx_tests_test2011_139_C\;Cxx_tests_test2011_140_C\;Cxx_tests_test2011_143_C\;Cxx_tests_test2011_146_C\;Cxx_tests_test2011_147_C\;Cxx_tests_test2011_149_C\;Cxx_tests_test2011_150_C\;Cxx_tests_test2011_151_C\;Cxx_tests_test2011_152_C\;Cxx_tests_test2011_153_C\;Cxx_tests_test2011_154_C\;Cxx_tests_test2011_155_C\;Cxx_tests_test2011_156_C\;Cxx_tests_test2011_157_C\;Cxx_tests_test2011_158_C\;Cxx_tests_test2011_159_C\;Cxx_tests_test2011_160_C\;Cxx_tests_test2011_161_C\;Cxx_tests_test2011_162_C\;Cxx_tests_test2011_163_C\;Cxx_tests_test2011_164_C\;Cxx_tests_test2011_165_C\;Cxx_tests_test2011_166_C\;Cxx_tests_test2011_167_C\;Cxx_tests_test2011_168_C\;Cxx_tests_test2011_169_C\;Cxx_tests_test2011_170_C\;Cxx_tests_test2011_171_C\;Cxx_tests_test2011_172_C\;Cxx_tests_test2011_173_C\;Cxx_tests_test2011_174_C\;Cxx_tests_test2011_175_C\;Cxx_tests_test2011_176_C\;Cxx_tests_test2011_177_C\;Cxx_tests_test2011_179_C\;Cxx_tests_test2011_180_C\;Cxx_tests_test2011_181_C\;Cxx_tests_test2011_182_C\;Cxx_tests_test2011_183_C\;Cxx_tests_test2011_184_C\;Cxx_tests_test2011_185_C\;Cxx_tests_test2011_186_C\;Cxx_tests_test2011_187_C\;Cxx_tests_test2011_188_C\;Cxx_tests_test2011_189_C\;Cxx_tests_test2011_190_C\;Cxx_tests_test2011_192_C\;Cxx_tests_test2011_193_C\;Cxx_tests_test2011_194_C\;Cxx_tests_test2011_195_C\;Cxx_tests_test2011_19_C\;Cxx_tests_test2011_20_C\;Cxx_tests_test2011_21_C\;Cxx_tests_test2011_22_C\;Cxx_tests_test2011_23_C\;Cxx_tests_test2011_24_C\;Cxx_tests_test2011_25_C\;Cxx_tests_test2011_26_C\;Cxx_tests_test2011_27_C\;Cxx_tests_test2011_28_C\;Cxx_tests_test2011_29_C\;Cxx_tests_test2011_30_C\;Cxx_tests_test2011_31_C\;Cxx_tests_test2011_32_C\;Cxx_tests_test2011_33_C\;Cxx_tests_test2011_34_C\;Cxx_tests_test2011_35_C\;Cxx_tests_test2011_36_C\;Cxx_tests_test2011_37_C\;Cxx_tests_test2011_38_C\;Cxx_tests_test2011_39_C\;Cxx_tests_test2011_40_C\;Cxx_tests_test2011_41_C\;Cxx_tests_test2011_42_C\;Cxx_tests_test2011_43_C\;Cxx_tests_test2011_44_C\;Cxx_tests_test2011_45_C\;Cxx_tests_test2011_46_C\;Cxx_tests_test2011_47_C\;Cxx_tests_test2011_48_C\;Cxx_tests_test2011_49_C\;Cxx_tests_test2011_50_C\;Cxx_tests_test2011_51_C\;Cxx_tests_test2011_52_C\;Cxx_tests_test2011_53_C\;Cxx_tests_test2011_54_C\;Cxx_tests_test2011_55_C\;Cxx_tests_test2011_56_C\;Cxx_tests_test2011_57_C\;Cxx_tests_test2011_58_C\;Cxx_tests_test2011_59_C\;Cxx_tests_test2011_60_C\;Cxx_tests_test2011_61_C\;Cxx_tests_test2011_62_C\;Cxx_tests_test2011_63_C\;Cxx_tests_test2011_64_C\;Cxx_tests_test2011_65_C\;Cxx_tests_test2011_66_C\;Cxx_tests_test2011_67_C\;Cxx_tests_test2011_68_C\;Cxx_tests_test2011_69_C\;Cxx_tests_test2011_70_C\;Cxx_tests_test2011_71_C\;Cxx_tests_test2011_72_C\;Cxx_tests_test2011_73_C\;Cxx_tests_test2011_74_C\;Cxx_tests_test2011_75_C\;Cxx_tests_test2011_76_C\;Cxx_tests_test2011_77_C\;Cxx_tests_test2011_78_C\;Cxx_tests_test2011_79_C\;Cxx_tests_test2011_80_C\;Cxx_tests_test2011_81_C\;Cxx_tests_test2011_82_C\;Cxx_tests_test2011_83_C\;Cxx_tests_test2011_84_C\;Cxx_tests_test2011_85_C\;Cxx_tests_test2011_86_C\;Cxx_tests_test2011_87_C\;Cxx_tests_test2011_88_C\;Cxx_tests_test2011_89_C\;Cxx_tests_test2011_90_C\;Cxx_tests_test2011_91_C\;Cxx_tests_test2011_92_C\;Cxx_tests_test2011_93_C\;Cxx_tests_test2011_94_C\;Cxx_tests_test2011_95_C\;Cxx_tests_test2011_96_C\;Cxx_tests_test2011_97_C\;Cxx_tests_test2011_98_C\;Cxx_tests_test2011_99_C\;Cxx_tests_test2012_01_C\;Cxx_tests_test2012_02_C\;Cxx_tests_test2012_03_C\;Cxx_tests_test2012_04_C\;Cxx_tests_test2012_05_C\;Cxx_tests_test2012_06_C\;Cxx_tests_test2012_07_C\;Cxx_tests_test2012_08_C\;Cxx_tests_test2012_100_C\;Cxx_tests_test2012_101_C\;Cxx_tests_test2012_102_C\;Cxx_tests_test2012_103_C\;Cxx_tests_test2012_104_C\;Cxx_tests_test2012_105_C\;Cxx_tests_test2012_106_C\;Cxx_tests_test2012_107_C\;Cxx_tests_test2012_108_C\;Cxx_tests_test2012_109_C\;Cxx_tests_test2012_10_C\;Cxx_tests_test2012_110_C\;Cxx_tests_test2012_111_C\;Cxx_tests_test2012_112_C\;Cxx_tests_test2012_113_C\;Cxx_tests_test2012_114_C\;Cxx_tests_test2012_115_C\;Cxx_tests_test2012_116_C\;Cxx_tests_test2012_117_C\;Cxx_tests_test2012_118_C\;Cxx_tests_test2012_119_C\;Cxx_tests_test2012_11_C\;Cxx_tests_test2012_120_C\;Cxx_tests_test2012_121_C\;Cxx_tests_test2012_122_C\;Cxx_tests_test2012_123_C\;Cxx_tests_test2012_124_C\;Cxx_tests_test2012_125_C\;Cxx_tests_test2012_126_C\;Cxx_tests_test2012_127_C\;Cxx_tests_test2012_128_C\;Cxx_tests_test2012_129_C\;Cxx_tests_test2012_12_C\;Cxx_tests_test2012_130_C\;Cxx_tests_test2012_131_C\;Cxx_tests_test2012_132_C\;Cxx_tests_test2012_136_C\;Cxx_tests_test2012_137_C\;Cxx_tests_test2012_138_C\;Cxx_tests_test2012_13_C\;Cxx_tests_test2012_141_C\;Cxx_tests_test2012_142_C\;Cxx_tests_test2012_143_C\;Cxx_tests_test2012_144_C\;Cxx_tests_test2012_145_C\;Cxx_tests_test2012_146_C\;Cxx_tests_test2012_147_C\;Cxx_tests_test2012_148_C\;Cxx_tests_test2012_149_C\;Cxx_tests_test2012_14_C\;Cxx_tests_test2012_150_C\;Cxx_tests_test2012_151_C\;Cxx_tests_test2012_152_C\;Cxx_tests_test2012_153_C\;Cxx_tests_test2012_154_C\;Cxx_tests_test2012_155_C\;Cxx_tests_test2012_156_C\;Cxx_tests_test2012_157_C\;Cxx_tests_test2012_158_C\;Cxx_tests_test2012_159_C\;Cxx_tests_test2012_15_C\;Cxx_tests_test2012_160_C\;Cxx_tests_test2012_161_C\;Cxx_tests_test2012_162_C\;Cxx_tests_test2012_163_C\;Cxx_tests_test2012_164_C\;Cxx_tests_test2012_165_C\;Cxx_tests_test2012_166_C\;Cxx_tests_test2012_167_C\;Cxx_tests_test2012_168_C\;Cxx_tests_test2012_169_C\;Cxx_tests_test2012_16_C\;Cxx_tests_test2012_170_C\;Cxx_tests_test2012_171_C\;Cxx_tests_test2012_172_C\;Cxx_tests_test2012_173_C\;Cxx_tests_test2012_174_C\;Cxx_tests_test2012_175_C\;Cxx_tests_test2012_176_C\;Cxx_tests_test2012_177_C\;Cxx_tests_test2012_178_C\;Cxx_tests_test2012_179_C\;Cxx_tests_test2012_17_C\;Cxx_tests_test2012_180_C\;Cxx_tests_test2012_181_C\;Cxx_tests_test2012_182_C\;Cxx_tests_test2012_183_C\;Cxx_tests_test2012_184_C\;Cxx_tests_test2012_185_C\;Cxx_tests_test2012_186_C\;Cxx_tests_test2012_187_C\;Cxx_tests_test2012_18_C\;Cxx_tests_test2012_190_C\;Cxx_tests_test2012_191_C\;Cxx_tests_test2012_192_C\;Cxx_tests_test2012_193_C\;Cxx_tests_test2012_194_C\;Cxx_tests_test2012_195_C\;Cxx_tests_test2012_196_C\;Cxx_tests_test2012_197_C\;Cxx_tests_test2012_198_C\;Cxx_tests_test2012_199_C\;Cxx_tests_test2012_19_C\;Cxx_tests_test2012_200_C\;Cxx_tests_test2012_201_C\;Cxx_tests_test2012_202_C\;Cxx_tests_test2012_203_C\;Cxx_tests_test2012_204_C\;Cxx_tests_test2012_205_C\;Cxx_tests_test2012_206_C\;Cxx_tests_test2012_207_C\;Cxx_tests_test2012_208_C\;Cxx_tests_test2012_209_C\;Cxx_tests_test2012_20_C\;Cxx_tests_test2012_210_C\;Cxx_tests_test2012_211_C\;Cxx_tests_test2012_212_C\;Cxx_tests_test2012_213_C\;Cxx_tests_test2012_214_C\;Cxx_tests_test2012_215_C\;Cxx_tests_test2012_216_C\;Cxx_tests_test2012_217_C\;Cxx_tests_test2012_218_C\;Cxx_tests_test2012_219_C\;Cxx_tests_test2012_21_C\;Cxx_tests_test2012_220_C\;Cxx_tests_test2012_221_C\;Cxx_tests_test2012_222_C\;Cxx_tests_test2012_223_C\;Cxx_tests_test2012_224_C\;Cxx_tests_test2012_225_C\;Cxx_tests_test2012_226_C\;Cxx_tests_test2012_227_C\;Cxx_tests_test2012_228_C\;Cxx_tests_test2012_229_C\;Cxx_tests_test2012_22_C\;Cxx_tests_test2012_230_C\;Cxx_tests_test2012_231_C\;Cxx_tests_test2012_232_C\;Cxx_tests_test2012_233_C\;Cxx_tests_test2012_234_C\;Cxx_tests_test2012_235_C\;Cxx_tests_test2012_236_C\;Cxx_tests_test2012_237_C\;Cxx_tests_test2012_238_C\;Cxx_tests_test2012_239_C\;Cxx_tests_test2012_23_C\;Cxx_tests_test2012_240_C\;Cxx_tests_test2012_241_C\;Cxx_tests_test2012_242_C\;Cxx_tests_test2012_243_C\;Cxx_tests_test2012_244_C\;Cxx_tests_test2012_245_C\;Cxx_tests_test2012_246a_C\;Cxx_tests_test2012_246b_C\;Cxx_tests_test2012_247_C\;Cxx_tests_test2012_248_C\;Cxx_tests_test2012_249_C\;Cxx_tests_test2012_24_C\;Cxx_tests_test2012_250_C\;Cxx_tests_test2012_251_C\;Cxx_tests_test2012_253_C\;Cxx_tests_test2012_254_C\;Cxx_tests_test2012_255_C\;Cxx_tests_test2012_256_C\;Cxx_tests_test2012_257_C\;Cxx_tests_test2012_258_C\;Cxx_tests_test2012_259_C\;Cxx_tests_test2012_25_C\;Cxx_tests_test2012_260_C\;Cxx_tests_test2012_262_C\;Cxx_tests_test2012_263_C\;Cxx_tests_test2012_264_C\;Cxx_tests_test2012_265_C\;Cxx_tests_test2012_266_C\;Cxx_tests_test2012_267_C\;Cxx_tests_test2012_268_C\;Cxx_tests_test2012_269_C\;Cxx_tests_test2012_26_C\;Cxx_tests_test2012_270_C\;Cxx_tests_test2012_271_C\;Cxx_tests_test2012_27_C\;Cxx_tests_test2012_28_C\;Cxx_tests_test2012_29_C\;Cxx_tests_test2012_30_C\;Cxx_tests_test2012_31_C\;Cxx_tests_test2012_32_C\;Cxx_tests_test2012_33_C\;Cxx_tests_test2012_34_C\;Cxx_tests_test2012_36_C\;Cxx_tests_test2012_37_C\;Cxx_tests_test2012_38_C\;Cxx_tests_test2012_39_C\;Cxx_tests_test2012_40_C\;Cxx_tests_test2012_41_C\;Cxx_tests_test2012_42_C\;Cxx_tests_test2012_43_C\;Cxx_tests_test2012_44_C\;Cxx_tests_test2012_45_C\;Cxx_tests_test2012_46_C\;Cxx_tests_test2012_47_C\;Cxx_tests_test2012_48_C\;Cxx_tests_test2012_49_C\;Cxx_tests_test2012_50_C\;Cxx_tests_test2012_51_C\;Cxx_tests_test2012_52_C\;Cxx_tests_test2012_53_C\;Cxx_tests_test2012_54_C\;Cxx_tests_test2012_55_C\;Cxx_tests_test2012_56_C\;Cxx_tests_test2012_58_C\;Cxx_tests_test2012_60_C\;Cxx_tests_test2012_61_C\;Cxx_tests_test2012_62_C\;Cxx_tests_test2012_63_C\;Cxx_tests_test2012_64_C\;Cxx_tests_test2012_65_C\;Cxx_tests_test2012_66_C\;Cxx_tests_test2012_67_C\;Cxx_tests_test2012_69_C\;Cxx_tests_test2012_70_C\;Cxx_tests_test2012_71_C\;Cxx_tests_test2012_72_C\;Cxx_tests_test2012_73_C\;Cxx_tests_test2012_74_C\;Cxx_tests_test2012_75_C\;Cxx_tests_test2012_76_C\;Cxx_tests_test2012_77_C\;Cxx_tests_test2012_78_C\;Cxx_tests_test2012_80_C\;Cxx_tests_test2012_82_C\;Cxx_tests_test2012_83_C\;Cxx_tests_test2012_84_C\;Cxx_tests_test2012_85_C\;Cxx_tests_test2012_87_C\;Cxx_tests_test2012_88_C\;Cxx_tests_test2012_89_C\;Cxx_tests_test2012_91_C\;Cxx_tests_test2012_92_C\;Cxx_tests_test2012_93_C\;Cxx_tests_test2012_94_C\;Cxx_tests_test2012_95_C\;Cxx_tests_test2012_96_C\;Cxx_tests_test2012_97_C\;Cxx_tests_test2012_98_C\;Cxx_tests_test2012_99_C\;Cxx_tests_test2013_01_C\;Cxx_tests_test2013_02_C\;Cxx_tests_test2013_03_C\;Cxx_tests_test2013_04_C\;Cxx_tests_test2013_05_C\;Cxx_tests_test2013_06_C\;Cxx_tests_test2013_07_C\;Cxx_tests_test2013_08_C\;Cxx_tests_test2013_100_C\;Cxx_tests_test2013_101_C\;Cxx_tests_test2013_102_C\;Cxx_tests_test2013_103_C\;Cxx_tests_test2013_104_C\;Cxx_tests_test2013_105_C\;Cxx_tests_test2013_106_C\;Cxx_tests_test2013_107_C\;Cxx_tests_test2013_108_C\;Cxx_tests_test2013_109_C\;Cxx_tests_test2013_10_C\;Cxx_tests_test2013_110_C\;Cxx_tests_test2013_111_C\;Cxx_tests_test2013_112_C\;Cxx_tests_test2013_113_C\;Cxx_tests_test2013_114_C\;Cxx_tests_test2013_115_C\;Cxx_tests_test2013_116_C\;Cxx_tests_test2013_117_C\;Cxx_tests_test2013_118_C\;Cxx_tests_test2013_119_C\;Cxx_tests_test2013_11_C\;Cxx_tests_test2013_120_C\;Cxx_tests_test2013_121_C\;Cxx_tests_test2013_122_C\;Cxx_tests_test2013_123_C\;Cxx_tests_test2013_124_C\;Cxx_tests_test2013_125_C\;Cxx_tests_test2013_126_C\;Cxx_tests_test2013_127_C\;Cxx_tests_test2013_128_C\;Cxx_tests_test2013_129_C\;Cxx_tests_test2013_12_C\;Cxx_tests_test2013_130_C\;Cxx_tests_test2013_131_C\;Cxx_tests_test2013_132_C\;Cxx_tests_test2013_133_C\;Cxx_tests_test2013_134_C\;Cxx_tests_test2013_135_C\;Cxx_tests_test2013_136_C\;Cxx_tests_test2013_137_C\;Cxx_tests_test2013_138_C\;Cxx_tests_test2013_139_C\;Cxx_tests_test2013_140_C\;Cxx_tests_test2013_141_C\;Cxx_tests_test2013_142_C\;Cxx_tests_test2013_143_C\;Cxx_tests_test2013_144_C\;Cxx_tests_test2013_145_C\;Cxx_tests_test2013_146_C\;Cxx_tests_test2013_147_C\;Cxx_tests_test2013_148_C\;Cxx_tests_test2013_149_C\;Cxx_tests_test2013_150_C\;Cxx_tests_test2013_151_C\;Cxx_tests_test2013_152_C\;Cxx_tests_test2013_153_C\;Cxx_tests_test2013_154_C\;Cxx_tests_test2013_155_C\;Cxx_tests_test2013_156_C\;Cxx_tests_test2013_157_C\;Cxx_tests_test2013_158_C\;Cxx_tests_test2013_159_C\;Cxx_tests_test2013_160_C\;Cxx_tests_test2013_161_C\;Cxx_tests_test2013_162_C\;Cxx_tests_test2013_163_C\;Cxx_tests_test2013_165_C\;Cxx_tests_test2013_166_C\;Cxx_tests_test2013_167_C\;Cxx_tests_test2013_168_C\;Cxx_tests_test2013_169_C\;Cxx_tests_test2013_170_C\;Cxx_tests_test2013_171_C\;Cxx_tests_test2013_172_C\;Cxx_tests_test2013_173_C\;Cxx_tests_test2013_174_C\;Cxx_tests_test2013_175_C\;Cxx_tests_test2013_176_C\;Cxx_tests_test2013_177_C\;Cxx_tests_test2013_179_C\;Cxx_tests_test2013_180_C\;Cxx_tests_test2013_181_C\;Cxx_tests_test2013_183_C\;Cxx_tests_test2013_184_C\;Cxx_tests_test2013_186_C\;Cxx_tests_test2013_187_C\;Cxx_tests_test2013_188_C\;Cxx_tests_test2013_189_C\;Cxx_tests_test2013_18_C\;Cxx_tests_test2013_190_C\;Cxx_tests_test2013_191_C\;Cxx_tests_test2013_192_C\;Cxx_tests_test2013_193_C\;Cxx_tests_test2013_194_C\;Cxx_tests_test2013_196_C\;Cxx_tests_test2013_198_C\;Cxx_tests_test2013_199_C\;Cxx_tests_test2013_200_C\;Cxx_tests_test2013_201_C\;Cxx_tests_test2013_202_C\;Cxx_tests_test2013_203_C\;Cxx_tests_test2013_204_C\;Cxx_tests_test2013_205_C\;Cxx_tests_test2013_206_C\;Cxx_tests_test2013_207_C\;Cxx_tests_test2013_208_C\;Cxx_tests_test2013_209_C\;Cxx_tests_test2013_20_C\;Cxx_tests_test2013_210_C\;Cxx_tests_test2013_211_C\;Cxx_tests_test2013_213_C\;Cxx_tests_test2013_214_C\;Cxx_tests_test2013_215_C\;Cxx_tests_test2013_216_C\;Cxx_tests_test2013_217_C\;Cxx_tests_test2013_218_C\;Cxx_tests_test2013_219_C\;Cxx_tests_test2013_21_C\;Cxx_tests_test2013_220_C\;Cxx_tests_test2013_221_C\;Cxx_tests_test2013_222_C\;Cxx_tests_test2013_223_C\;Cxx_tests_test2013_225_C\;Cxx_tests_test2013_226_C\;Cxx_tests_test2013_227_C\;Cxx_tests_test2013_228_C\;Cxx_tests_test2013_229_C\;Cxx_tests_test2013_22_C\;Cxx_tests_test2013_230_C\;Cxx_tests_test2013_231_C\;Cxx_tests_test2013_232_C\;Cxx_tests_test2013_237_C\;Cxx_tests_test2013_238_C\;Cxx_tests_test2013_239_C\;Cxx_tests_test2013_23_C\;Cxx_tests_test2013_244_C\;Cxx_tests_test2013_250_C\;Cxx_tests_test2013_251_C\;Cxx_tests_test2013_252_C\;Cxx_tests_test2013_253_C\;Cxx_tests_test2013_255_C\;Cxx_tests_test2013_256_C\;Cxx_tests_test2013_258_C\;Cxx_tests_test2013_259_C\;Cxx_tests_test2013_25_C\;Cxx_tests_test2013_261_C\;Cxx_tests_test2013_26_C\;Cxx_tests_test2013_271_C\;Cxx_tests_test2013_272_C\;Cxx_tests_test2013_273_C\;Cxx_tests_test2013_274_C\;Cxx_tests_test2013_275_C\;Cxx_tests_test2013_276_C\;Cxx_tests_test2013_277_C\;Cxx_tests_test2013_278_C\;Cxx_tests_test2013_279_C\;Cxx_tests_test2013_27_C\;Cxx_tests_test2013_280_C\;Cxx_tests_test2013_281_C\;Cxx_tests_test2013_282_C\;Cxx_tests_test2013_283_C\;Cxx_tests_test2013_284_C\;Cxx_tests_test2013_285_C\;Cxx_tests_test2013_286_C\;Cxx_tests_test2013_287_C\;Cxx_tests_test2013_288_C\;Cxx_tests_test2013_289_C\;Cxx_tests_test2013_28_C\;Cxx_tests_test2013_290_C\;Cxx_tests_test2013_291_C\;Cxx_tests_test2013_292_C\;Cxx_tests_test2013_293_C\;Cxx_tests_test2013_294_C\;Cxx_tests_test2013_295_C\;Cxx_tests_test2013_296_C\;Cxx_tests_test2013_29_C\;Cxx_tests_test2013_300_C\;Cxx_tests_test2013_301_C\;Cxx_tests_test2013_302_C\;Cxx_tests_test2013_303_C\;Cxx_tests_test2013_305_C\;Cxx_tests_test2013_306_C\;Cxx_tests_test2013_307_C\;Cxx_tests_test2013_308_C\;Cxx_tests_test2013_310_C\;Cxx_tests_test2013_311_C\;Cxx_tests_test2013_312_C\;Cxx_tests_test2013_313_C\;Cxx_tests_test2013_314_C\;Cxx_tests_test2013_315_C\;Cxx_tests_test2013_316_C\;Cxx_tests_test2013_317_C\;Cxx_tests_test2013_318_C\;Cxx_tests_test2013_319_C\;Cxx_tests_test2013_31_C\;Cxx_tests_test2013_320_C\;Cxx_tests_test2013_32_C\;Cxx_tests_test2013_33_C\;Cxx_tests_test2013_34_C\;Cxx_tests_test2013_36_C\;Cxx_tests_test2013_37_C\;Cxx_tests_test2013_38_C\;Cxx_tests_test2013_39_C\;Cxx_tests_test2013_44_C\;Cxx_tests_test2013_46_C\;Cxx_tests_test2013_47_C\;Cxx_tests_test2013_48_C\;Cxx_tests_test2013_50_C\;Cxx_tests_test2013_51_C\;Cxx_tests_test2013_52_C\;Cxx_tests_test2013_53_C\;Cxx_tests_test2013_54_C\;Cxx_tests_test2013_55_C\;Cxx_tests_test2013_56_C\;Cxx_tests_test2013_57_C\;Cxx_tests_test2013_58_C\;Cxx_tests_test2013_59_C\;Cxx_tests_test2013_60_C\;Cxx_tests_test2013_61_C\;Cxx_tests_test2013_63_C\;Cxx_tests_test2013_64_C\;Cxx_tests_test2013_65_C\;Cxx_tests_test2013_66_C\;Cxx_tests_test2013_67_C\;Cxx_tests_test2013_68_C\;Cxx_tests_test2013_69_C\;Cxx_tests_test2013_70_C\;Cxx_tests_test2013_71_C\;Cxx_tests_test2013_72_C\;Cxx_tests_test2013_73_C\;Cxx_tests_test2013_74_C\;Cxx_tests_test2013_75_C\;Cxx_tests_test2013_76_C\;Cxx_tests_test2013_77_C\;Cxx_tests_test2013_78_C\;Cxx_tests_test2013_79_C\;Cxx_tests_test2013_80_C\;Cxx_tests_test2013_81_C\;Cxx_tests_test2013_82_C\;Cxx_tests_test2013_83_C\;Cxx_tests_test2013_86_C\;Cxx_tests_test2013_87_C\;Cxx_tests_test2013_88_C\;Cxx_tests_test2013_89_C\;Cxx_tests_test2013_90_C\;Cxx_tests_test2013_91_C\;Cxx_tests_test2013_92_C\;Cxx_tests_test2013_93_C\;Cxx_tests_test2013_94_C\;Cxx_tests_test2013_95_C\;Cxx_tests_test2013_97_C\;Cxx_tests_test2013_98_C\;Cxx_tests_test2013_99_C\;Cxx_tests_test2013_milind_01_C\;Cxx_tests_test2014_01_C\;Cxx_tests_test2014_02_C\;Cxx_tests_test2014_03_C\;Cxx_tests_test2014_04_C\;Cxx_tests_test2014_05_C\;Cxx_tests_test2014_06_C\;Cxx_tests_test2014_07_C\;Cxx_tests_test2014_08_C\;Cxx_tests_test2014_09_C\;Cxx_tests_test2014_107_C\;Cxx_tests_test2014_108_C\;Cxx_tests_test2014_109_C\;Cxx_tests_test2014_10_C\;Cxx_tests_test2014_111_C\;Cxx_tests_test2014_112_C\;Cxx_tests_test2014_114_C\;Cxx_tests_test2014_115_C\;Cxx_tests_test2014_116_C\;Cxx_tests_test2014_117_C\;Cxx_tests_test2014_11_C\;Cxx_tests_test2014_122_C\;Cxx_tests_test2014_123_C\;Cxx_tests_test2014_124_C\;Cxx_tests_test2014_125_C\;Cxx_tests_test2014_126_C\;Cxx_tests_test2014_127_C\;Cxx_tests_test2014_128_C\;Cxx_tests_test2014_129_C\;Cxx_tests_test2014_12_C\;Cxx_tests_test2014_130_C\;Cxx_tests_test2014_131_C\;Cxx_tests_test2014_132_C\;Cxx_tests_test2014_133_C\;Cxx_tests_test2014_134_C\;Cxx_tests_test2014_135_C\;Cxx_tests_test2014_136_C\;Cxx_tests_test2014_137_C\;Cxx_tests_test2014_139_C\;Cxx_tests_test2014_13_C\;Cxx_tests_test2014_140_C\;Cxx_tests_test2014_141_C\;Cxx_tests_test2014_142_C\;Cxx_tests_test2014_143_C\;Cxx_tests_test2014_144_C\;Cxx_tests_test2014_145_C\;Cxx_tests_test2014_146_C\;Cxx_tests_test2014_147_C\;Cxx_tests_test2014_148_C\;Cxx_tests_test2014_149_C\;Cxx_tests_test2014_150_C\;Cxx_tests_test2014_151_C\;Cxx_tests_test2014_152_C\;Cxx_tests_test2014_153_C\;Cxx_tests_test2014_154_C\;Cxx_tests_test2014_155_C\;Cxx_tests_test2014_156_C\;Cxx_tests_test2014_157_C\;Cxx_tests_test2014_160_C\;Cxx_tests_test2014_161_C\;Cxx_tests_test2014_162_C\;Cxx_tests_test2014_163_C\;Cxx_tests_test2014_165_C\;Cxx_tests_test2014_166_C\;Cxx_tests_test2014_167_C\;Cxx_tests_test2014_168_C\;Cxx_tests_test2014_169_C\;Cxx_tests_test2014_16_C\;Cxx_tests_test2014_170_C\;Cxx_tests_test2014_171_C\;Cxx_tests_test2014_172_C\;Cxx_tests_test2014_174_C\;Cxx_tests_test2014_176_C\;Cxx_tests_test2014_177_C\;Cxx_tests_test2014_178_C\;Cxx_tests_test2014_179_C\;Cxx_tests_test2014_17_C\;Cxx_tests_test2014_182_C\;Cxx_tests_test2014_183_C\;Cxx_tests_test2014_184_C\;Cxx_tests_test2014_185_C\;Cxx_tests_test2014_186_C\;Cxx_tests_test2014_187_C\;Cxx_tests_test2014_187_work_around_C\;Cxx_tests_test2014_188_C\;Cxx_tests_test2014_189_C\;Cxx_tests_test2014_18_C\;Cxx_tests_test2014_190_C\;Cxx_tests_test2014_191_C\;Cxx_tests_test2014_192_C\;Cxx_tests_test2014_193_C\;Cxx_tests_test2014_194_C\;Cxx_tests_test2014_195_C\;Cxx_tests_test2014_196_C\;Cxx_tests_test2014_197_C\;Cxx_tests_test2014_198_C\;Cxx_tests_test2014_199_C\;Cxx_tests_test2014_19_C\;Cxx_tests_test2014_200_C\;Cxx_tests_test2014_201_C\;Cxx_tests_test2014_202_C\;Cxx_tests_test2014_203_C\;Cxx_tests_test2014_204_C\;Cxx_tests_test2014_205_C\;Cxx_tests_test2014_206_C\;Cxx_tests_test2014_207_C\;Cxx_tests_test2014_208_C\;Cxx_tests_test2014_209_C\;Cxx_tests_test2014_20_C\;Cxx_tests_test2014_210_C\;Cxx_tests_test2014_212_C\;Cxx_tests_test2014_215_C\;Cxx_tests_test2014_216_C\;Cxx_tests_test2014_217_C\;Cxx_tests_test2014_218_C\;Cxx_tests_test2014_219_C\;Cxx_tests_test2014_21_C\;Cxx_tests_test2014_220_C\;Cxx_tests_test2014_221_C\;Cxx_tests_test2014_222_C\;Cxx_tests_test2014_223_C\;Cxx_tests_test2014_224_C\;Cxx_tests_test2014_225_C\;Cxx_tests_test2014_226_C\;Cxx_tests_test2014_230_C\;Cxx_tests_test2014_231_C\;Cxx_tests_test2014_232_C\;Cxx_tests_test2014_233_C\;Cxx_tests_test2014_234_C\;Cxx_tests_test2014_235_C\;Cxx_tests_test2014_236_C\;Cxx_tests_test2014_238_C\;Cxx_tests_test2014_239_C\;Cxx_tests_test2014_240_C\;Cxx_tests_test2014_241_C\;Cxx_tests_test2014_242_C\;Cxx_tests_test2014_243_C\;Cxx_tests_test2014_24_C\;Cxx_tests_test2014_26_C\;Cxx_tests_test2014_27_C\;Cxx_tests_test2014_28_C\;Cxx_tests_test2014_29_C\;Cxx_tests_test2014_30_C\;Cxx_tests_test2014_31_C\;Cxx_tests_test2014_32_C\;Cxx_tests_test2014_33_C\;Cxx_tests_test2014_34_C\;Cxx_tests_test2014_35_C\;Cxx_tests_test2014_36_C\;Cxx_tests_test2014_37_C\;Cxx_tests_test2014_38_C\;Cxx_tests_test2014_39_C\;Cxx_tests_test2014_40_C\;Cxx_tests_test2014_41_C\;Cxx_tests_test2014_43_C\;Cxx_tests_test2014_45_C\;Cxx_tests_test2014_47_C\;Cxx_tests_test2014_48_C\;Cxx_tests_test2014_49_C\;Cxx_tests_test2014_50_C\;Cxx_tests_test2014_53_C\;Cxx_tests_test2014_54_C\;Cxx_tests_test2014_55_C\;Cxx_tests_test2014_56_C\;Cxx_tests_test2014_57_C\;Cxx_tests_test2014_58_C\;Cxx_tests_test2014_60_C\;Cxx_tests_test2014_61_C\;Cxx_tests_test2014_62_C\;Cxx_tests_test2014_67_C\;Cxx_tests_test2014_68_C\;Cxx_tests_test2014_70_C\;Cxx_tests_test2014_71_C\;Cxx_tests_test2014_72_C\;Cxx_tests_test2014_73_C\;Cxx_tests_test2014_74_C\;Cxx_tests_test2014_75_C\;Cxx_tests_test2014_76_C\;Cxx_tests_test2014_77_C\;Cxx_tests_test2014_78_C\;Cxx_tests_test2014_79_C\;Cxx_tests_test2014_80_C\;Cxx_tests_test2014_81_C\;Cxx_tests_test2014_82_C\;Cxx_tests_test2014_83_C\;Cxx_tests_test2014_84_C\;Cxx_tests_test2014_85_C\;Cxx_tests_test2014_86_C\;Cxx_tests_test2014_87_C\;Cxx_tests_test2014_88_C\;Cxx_tests_test2014_89_C\;Cxx_tests_test2014_90_C\;Cxx_tests_test2014_91_C\;Cxx_tests_test2014_92_C\;Cxx_tests_test2014_93_C\;Cxx_tests_test2014_94_C\;Cxx_tests_test2014_95_C\;Cxx_tests_test2014_96_C\;Cxx_tests_test2014_97_C\;Cxx_tests_test2014_98_C\;Cxx_tests_test2015_01_C\;Cxx_tests_test2015_02_C\;Cxx_tests_test2015_03_C\;Cxx_tests_test2015_04_C\;Cxx_tests_test2015_05_C\;Cxx_tests_test2015_07_C\;Cxx_tests_test2015_08_C\;Cxx_tests_test2015_09_C\;Cxx_tests_test2015_101_C\;Cxx_tests_test2015_102_C\;Cxx_tests_test2015_103_C\;Cxx_tests_test2015_104_C\;Cxx_tests_test2015_106_C\;Cxx_tests_test2015_107_C\;Cxx_tests_test2015_108_C\;Cxx_tests_test2015_110_C\;Cxx_tests_test2015_111_C\;Cxx_tests_test2015_112_C\;Cxx_tests_test2015_113_C\;Cxx_tests_test2015_114_C\;Cxx_tests_test2015_115_C\;Cxx_tests_test2015_116_C\;Cxx_tests_test2015_117_C\;Cxx_tests_test2015_118_C\;Cxx_tests_test2015_119_C\;Cxx_tests_test2015_120_C\;Cxx_tests_test2015_121_C\;Cxx_tests_test2015_122_C\;Cxx_tests_test2015_123_C\;Cxx_tests_test2015_124_C\;Cxx_tests_test2015_125_C\;Cxx_tests_test2015_128_C\;Cxx_tests_test2015_130_C\;Cxx_tests_test2015_131_C\;Cxx_tests_test2015_132_C\;Cxx_tests_test2015_133_C\;Cxx_tests_test2015_134_C\;Cxx_tests_test2015_137_C\;Cxx_tests_test2015_138_C\;Cxx_tests_test2015_139_C\;Cxx_tests_test2015_140_C\;Cxx_tests_test2015_141_C\;Cxx_tests_test2015_142_C\;Cxx_tests_test2015_143_C\;Cxx_tests_test2015_144_C\;Cxx_tests_test2015_145_C\;Cxx_tests_test2015_146_C\;Cxx_tests_test2015_147_C\;Cxx_tests_test2015_148_C\;Cxx_tests_test2015_149_C\;Cxx_tests_test2015_150_C\;Cxx_tests_test2015_151_C\;Cxx_tests_test2015_152_C\;Cxx_tests_test2015_153_C\;Cxx_tests_test2015_154_C\;Cxx_tests_test2015_20_C\;Cxx_tests_test2015_21_C\;Cxx_tests_test2015_22_C\;Cxx_tests_test2015_24_C\;Cxx_tests_test2015_26_C\;Cxx_tests_test2015_27_C\;Cxx_tests_test2015_28_C\;Cxx_tests_test2015_29_C\;Cxx_tests_test2015_30_C\;Cxx_tests_test2015_32_C\;Cxx_tests_test2015_33_C\;Cxx_tests_test2015_34_C\;Cxx_tests_test2015_35_C\;Cxx_tests_test2015_36_C\;Cxx_tests_test2015_40_C\;Cxx_tests_test2015_43_C\;Cxx_tests_test2015_44_C\;Cxx_tests_test2015_45_C\;Cxx_tests_test2015_50_C\;Cxx_tests_test2015_52_C\;Cxx_tests_test2015_55_C\;Cxx_tests_test2015_57_C\;Cxx_tests_test2015_58_C\;Cxx_tests_test2015_59_C\;Cxx_tests_test2015_60_C\;Cxx_tests_test2015_61_C\;Cxx_tests_test2015_76_C\;Cxx_tests_test2015_77_C\;Cxx_tests_test2015_78_C\;Cxx_tests_test2015_79_C\;Cxx_tests_test2015_80_C\;Cxx_tests_test2015_85_C\;Cxx_tests_test2015_86_C\;Cxx_tests_test2015_88_C\;Cxx_tests_test2015_89_C\;Cxx_tests_test2015_97_C\;Cxx_tests_test2015_99_C\;Cxx_tests_test2016_01_C\;Cxx_tests_test2016_02_C\;Cxx_tests_test2016_03_C\;Cxx_tests_test2016_04_C\;Cxx_tests_test2016_05_C\;Cxx_tests_test2016_06_C\;Cxx_tests_test2016_07_C\;Cxx_tests_test2016_08_C\;Cxx_tests_test2016_09_C\;Cxx_tests_test2016_10_C\;Cxx_tests_test2016_11_C\;Cxx_tests_test2016_12_C\;Cxx_tests_test2016_13_C\;Cxx_tests_test2016_14_C\;Cxx_tests_test2016_15_C\;Cxx_tests_test2016_16_C\;Cxx_tests_test2016_17_C\;Cxx_tests_test2016_18_C\;Cxx_tests_test2016_19_C\;Cxx_tests_test2016_21_C\;Cxx_tests_test2016_25_C\;Cxx_tests_test2016_26_C\;Cxx_tests_test2016_27_C\;Cxx_tests_test2016_28_C\;Cxx_tests_test2016_29_C\;Cxx_tests_test2016_30_C\;Cxx_tests_test2016_31_C\;Cxx_tests_test2016_32_C\;Cxx_tests_test2016_33_C\;Cxx_tests_test2016_34_C\;Cxx_tests_test2016_36_C\;Cxx_tests_test2016_37_C\;Cxx_tests_test2016_38_C\;Cxx_tests_test2016_40_C\;Cxx_tests_test2016_41_C\;Cxx_tests_test2016_42_C\;Cxx_tests_test2016_43_C\;Cxx_tests_test2016_44_C\;Cxx_tests_test2016_45_C\;Cxx_tests_test2016_46_C\;Cxx_tests_test2016_47_C\;Cxx_tests_test2016_48_C\;Cxx_tests_test2016_49_C\;Cxx_tests_test2016_50_C\;Cxx_tests_test2016_51_C\;Cxx_tests_test2016_53_C\;Cxx_tests_test2016_54_C\;Cxx_tests_test2016_55_C\;Cxx_tests_test2016_56_C\;Cxx_tests_test2016_57_C\;Cxx_tests_test2016_59_C\;Cxx_tests_test2016_60_C\;Cxx_tests_test2016_61_C\;Cxx_tests_test2016_62_C\;Cxx_tests_test2016_63_C\;Cxx_tests_test2016_64_C\;Cxx_tests_test2016_65_C\;Cxx_tests_test2016_66_C\;Cxx_tests_test2016_67_C\;Cxx_tests_test2016_68_C\;Cxx_tests_test2016_69_C\;Cxx_tests_test2016_70_C\;Cxx_tests_test2016_71_C\;Cxx_tests_test2016_80_C\;Cxx_tests_test2016_81_C\;Cxx_tests_test2016_83_C\;Cxx_tests_test2016_85_C\;Cxx_tests_test2016_86_C\;Cxx_tests_test2016_87_C\;Cxx_tests_test2016_88_C\;Cxx_tests_test2016_89_C\;Cxx_tests_test2016_91_C\;Cxx_tests_test2016_93_C\;Cxx_tests_test2016_96_C\;Cxx_tests_test2017_01_C\;Cxx_tests_test2017_02_C\;Cxx_tests_test2017_03_C\;Cxx_tests_test2017_04_C\;Cxx_tests_test2017_09_C\;Cxx_tests_test2017_100_C\;Cxx_tests_test2017_101_C\;Cxx_tests_test2017_102_C\;Cxx_tests_test2017_103_C\;Cxx_tests_test2017_104_C\;Cxx_tests_test2017_105_C\;Cxx_tests_test2017_106_C\;Cxx_tests_test2017_107_C\;Cxx_tests_test2017_10_C\;Cxx_tests_test2017_11_C\;Cxx_tests_test2017_13_C\;Cxx_tests_test2017_14_C\;Cxx_tests_test2017_15_C\;Cxx_tests_test2017_16_C\;Cxx_tests_test2017_17_C\;Cxx_tests_test2017_18_C\;Cxx_tests_test2017_19_C\;Cxx_tests_test2017_21_C\;Cxx_tests_test2017_22_C\;Cxx_tests_test2017_23_C\;Cxx_tests_test2017_26_C\;Cxx_tests_test2017_30_C\;Cxx_tests_test2017_35_C\;Cxx_tests_test2017_36_C\;Cxx_tests_test2017_37_C\;Cxx_tests_test2017_38_C\;Cxx_tests_test2017_39_C\;Cxx_tests_test2017_40_C\;Cxx_tests_test2017_41_C\;Cxx_tests_test2017_42_C\;Cxx_tests_test2017_43_C\;Cxx_tests_test2017_45_C\;Cxx_tests_test2017_48_C\;Cxx_tests_test2017_49_C\;Cxx_tests_test2017_50_C\;Cxx_tests_test2017_51_C\;Cxx_tests_test2017_52_C\;Cxx_tests_test2017_53_C\;Cxx_tests_test2017_54_C\;Cxx_tests_test2017_55_C\;Cxx_tests_test2017_56_C\;Cxx_tests_test2017_57_C\;Cxx_tests_test2017_58_C\;Cxx_tests_test2017_60_C\;Cxx_tests_test2017_61_C\;Cxx_tests_test2017_62_C\;Cxx_tests_test2017_64_C\;Cxx_tests_test2017_65_C\;Cxx_tests_test2017_66_C\;Cxx_tests_test2017_68_C\;Cxx_tests_test2017_70_C\;Cxx_tests_test2017_71_C\;Cxx_tests_test2017_72_C\;Cxx_tests_test2017_73_C\;Cxx_tests_test2017_74_C\;Cxx_tests_test2017_75_C\;Cxx_tests_test2017_76_C\;Cxx_tests_test2017_77_C\;Cxx_tests_test2017_78_C\;Cxx_tests_test2017_79_C\;Cxx_tests_test2017_80_C\;Cxx_tests_test2017_81_C\;Cxx_tests_test2017_82_C\;Cxx_tests_test2017_83_C\;Cxx_tests_test2017_84_C\;Cxx_tests_test2017_87_C\;Cxx_tests_test2017_88_C\;Cxx_tests_test2017_89_C\;Cxx_tests_test2017_90_C\;Cxx_tests_test2017_91_C\;Cxx_tests_test2017_92_C\;Cxx_tests_test2017_93_C\;Cxx_tests_test2017_94_C\;Cxx_tests_test2017_95_C\;Cxx_tests_test2017_96_C\;Cxx_tests_test2017_97_C\;Cxx_tests_test2017_98_C\;Cxx_tests_test2018_02_C\;Cxx_tests_test2018_03_C\;Cxx_tests_test2018_04_C\;Cxx_tests_test2018_05_C\;Cxx_tests_test2018_06_C\;Cxx_tests_test2018_07_C\;Cxx_tests_test2018_08_C\;Cxx_tests_test2018_09_C\;Cxx_tests_test2018_10_C\;Cxx_tests_test2018_11_C\;Cxx_tests_test2018_12_C\;Cxx_tests_test2018_13_C\;Cxx_tests_test2018_14_C\;Cxx_tests_test2018_15_C\;Cxx_tests_test2018_16_C\;Cxx_tests_test2018_17_C\;Cxx_tests_test2018_18_C\;Cxx_tests_test2018_19_C\;Cxx_tests_test2018_20_C\;Cxx_tests_test2018_21_C\;Cxx_tests_test2018_24_C\;Cxx_tests_test2018_27_C\;Cxx_tests_test2018_29_C\;Cxx_tests_test2018_30_C\;Cxx_tests_test2018_32_C\;Cxx_tests_test2018_33_C\;Cxx_tests_test2018_34_C\;Cxx_tests_test2018_35_C\;Cxx_tests_test2018_36_C\;Cxx_tests_test2018_37_C\;Cxx_tests_test2018_38_C\;Cxx_tests_test2018_39_C\;Cxx_tests_test2018_40_C\;Cxx_tests_test2018_41_C\;Cxx_tests_test2018_42_C\;Cxx_tests_test2018_43_C\;Cxx_tests_test2018_44_C\;Cxx_tests_test2018_45_C\;Cxx_tests_test2018_46_C\;Cxx_tests_test2018_47_C\;Cxx_tests_test2018_48_C\;Cxx_tests_test2018_49_C\;Cxx_tests_test2018_50_C\;Cxx_tests_test2018_51_C\;Cxx_tests_test2018_52_C\;Cxx_tests_test2018_53_C\;Cxx_tests_test2018_54_C\;Cxx_tests_test2018_55_C\;Cxx_tests_test2018_56_C\;Cxx_tests_test2018_57_C\;Cxx_tests_test2018_58_C\;Cxx_tests_test2018_59_C\;Cxx_tests_test2018_60_C\;Cxx_tests_test2018_61_C\;Cxx_tests_test2018_62_C\;Cxx_tests_test2018_63_C\;Cxx_tests_test2018_64_C\;Cxx_tests_test2019_01_C\;Cxx_tests_test2019_02_C\;Cxx_tests_test2019_03_C\;Cxx_tests_test2019_04_C\;Cxx_tests_test2019_05_C\;Cxx_tests_test2019_06_C\;Cxx_tests_test2019_07_C\;Cxx_tests_test2019_08_C\;Cxx_tests_test2019_09_C\;Cxx_tests_test2019_10_C\;Cxx_tests_test2019_11_C\;Cxx_tests_test2019_12_C\;Cxx_tests_test2019_13_C\;Cxx_tests_test2019_14_C\;Cxx_tests_test2019_15_C\;Cxx_tests_test2019_16_C\;Cxx_tests_test2019_17_C\;Cxx_tests_test2019_18_C\;Cxx_tests_test2019_19_C\;Cxx_tests_test2019_20_C\;Cxx_tests_test2019_21_C\;Cxx_tests_test2019_22_C\;Cxx_tests_test2019_23_C\;Cxx_tests_test2019_24_C\;Cxx_tests_test2020_01_C\;Cxx_tests_test2020_02_C\;Cxx_tests_test2020_03_C\;Cxx_tests_test2020_04_C\;Cxx_tests_test2020_05_C\;Cxx_tests_test2020_06_C\;Cxx_tests_test2020_07_C\;Cxx_tests_test2020_08_C\;Cxx_tests_test2020_09_C\;Cxx_tests_test2020_10_C\;Cxx_tests_test2020_11_C\;Cxx_tests_test2020_12_C\;Cxx_tests_test2020_13_C\;Cxx_tests_test2020_14_C\;Cxx_tests_test2020_15_C\;Cxx_tests_test2020_16_C\;Cxx_tests_test2020_17_C\;Cxx_tests_test2020_18_C\;Cxx_tests_test2020_19_C\;Cxx_tests_test2020_20_C\;Cxx_tests_test2020_21_C\;Cxx_tests_test2020_22_C\;Cxx_tests_test2020_23_C\;Cxx_tests_test2020_24_C\;Cxx_tests_test2020_25_C\;Cxx_tests_test2020_26_C\;Cxx_tests_test2020_27_C\;Cxx_tests_test2020_28_C\;Cxx_tests_test2020_29_C\;Cxx_tests_test2020_30_C\;Cxx_tests_test2020_31_C\;Cxx_tests_test2020_32_C\;Cxx_tests_test2020_33_C\;Cxx_tests_test2020_34_C\;Cxx_tests_test2020_37_C\;Cxx_tests_test2020_38_C\;Cxx_tests_test2020_39_C\;Cxx_tests_test2020_40_C\;Cxx_tests_test2020_41_C\;Cxx_tests_test2020_42_C\;Cxx_tests_test2020_43_C\;Cxx_tests_test2020_44_C\;Cxx_tests_test2020_45_C\;Cxx_tests_test2020_46_C\;Cxx_tests_test2020_47_C\;Cxx_tests_test2020_49_C\;Cxx_tests_test2020_50_C\;Cxx_tests_test2020_51_C\;Cxx_tests_test2020_52_C\;Cxx_tests_test2020_53_C\;Cxx_tests_test2020_54_C\;Cxx_tests_test2020_55_C\;Cxx_tests_test2020_56_C\;Cxx_tests_test2020_57_C\;Cxx_tests_test2020_58_C\;Cxx_tests_test2020_59_C\;Cxx_tests_test2020_60_C\;Cxx_tests_test2020_61_C\;Cxx_tests_test2020_62_C\;Cxx_tests_test2020_63_C\;Cxx_tests_test2020_64_C\;Cxx_tests_test2020_65_C\;Cxx_tests_test2020_66_C\;Cxx_tests_test2020_67_C\;Cxx_tests_test2020_68_C\;Cxx_tests_test2020_69_C\;Cxx_tests_test2020_70_C\;Cxx_tests_test2020_71_C\;Cxx_tests_test2020_72_C\;Cxx_tests_test2020_73_C\;Cxx_tests_test2020_74_C\;Cxx_tests_test2020_75_C\;Cxx_tests_test2020_76_C\;Cxx_tests_test2020_77_C\;Cxx_tests_test2020_78_C\;Cxx_tests_test2020_79_C\;Cxx_tests_test2020_80_C\;Cxx_tests_test2020_82_C\;Cxx_tests_test2020_83_C\;Cxx_tests_test2021_01_C\;Cxx_tests_test2021_02_C\;Cxx_tests_test2021_03_C\;Cxx_tests_test2021_04_C\;Cxx_tests_test2021_05_C\;Cxx_tests_test2021_06_C\;Cxx_tests_test2021_07_C\;Cxx_tests_test2021_08_C\;Cxx_tests_test2021_09_C\;Cxx_tests_test2021_10_C\;Cxx_tests_test2021_11_C\;Cxx_tests_test2021_12_C\;Cxx_tests_test2021_13_C\;Cxx_tests_test2021_14_C\;Cxx_tests_test2021_15_C\;Cxx_tests_test2021_16_C\;Cxx_tests_test2021_17_C\;Cxx_tests_test2022_01_C\;Cxx_tests_test2022_02_C\;Cxx_tests_test2022_03_C\;Cxx_tests_test2022_04_C\;Cxx_tests_test2022_05_C\;Cxx_tests_test2022_06_C\;Cxx_tests_test2022_07_C\;Cxx_tests_test2022_08_C\;Cxx_tests_test2022_09_C\;Cxx_tests_test2022_10_C\;Cxx_tests_test2022_11_C\;Cxx_tests_test2022_12_C\;Cxx_tests_test2022_13_C\;Cxx_tests_test2022_14_C\;Cxx_tests_test2022_15_C\;Cxx_tests_test2022_16_C\;Cxx_tests_test2022_17_C\;Cxx_tests_test2022_22_C\;Cxx_tests_test2022_23_C\;Cxx_tests_test2022_24_C\;Cxx_tests_test2022_25_C\;Cxx_tests_test2025_issue84_friend_template_in_class_cpp\;Cxx_tests_test_CplusplusMacro_Cpp_C\;Cxx_tests_test_int_lexemes_C\;Cxx_tests_test_int_lexemes_donot_pass_C\;Cxx_tests_test_simple_int_C\;Cxx_tests_test_wchars_C\;Cxx_tests_unparse-typedef-ctor_C\;compiler_options_collect_comments_test2004_82_C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_Cxx_tests_check-local PROPERTIES
  DEPENDS "${_cxx_check_local_tests}"
)

add_test(
  NAME nonsmoke_functional_CompileTests_Cxx_tests_check-performance-timing-with-threshold
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_cxx_perf_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_Cxx_tests_check-performance-timing-with-threshold PROPERTIES
  LABELS disabled
  DEPENDS "${_cxx_perf_tests}"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_CompileTests_Cxx_tests_check-performance-timing-with-threshold PROPERTIES DISABLED TRUE)
endif()
