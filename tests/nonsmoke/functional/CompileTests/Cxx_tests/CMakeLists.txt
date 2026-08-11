set(CXX_TESTCODES
  rex_unparser_frontier_child_identity.cpp
  rex_frontend_static_assert_typed_message.cpp
  rex_frontend_scalar_value_init.cpp
  rex_frontend_typedef_tag_owner.cpp
  rex_frontend_base_specifier_identity.cpp
  rex_frontend_access_label_lexical_ownership.cpp
  rex_frontend_declaration_scope_contract.cpp
  rex_frontend_implicit_control_flow_scope.cpp
  rex_frontend_new_initializer_source_form.cpp
  rex_frontend_gnu_binary_conditional.cpp
  rex_frontend_field_tag_introduction.cpp
  rex_frontend_linkage_container_contract.cpp
  rex_frontend_variable_declarator_identity.cpp
  rex_frontend_explicit_instantiation_identity.cpp
  rex_frontend_function_explicit_instantiation_identity.cpp
  rex_frontend_initializer_destination_type.cpp
  rex_frontend_constructor_target_source_type.cpp
  rex_frontend_header_member_initializer_ownership.cpp
  rex_frontend_hidden_variable_semantic_scope.cpp
  rex_frontend_header_typedef_template_argument.cpp
  rex_frontend_dependent_member_typedef_identity.cpp
  rex_frontend_dependent_nested_friend_header.cpp
  rex_frontend_nested_friend_signature_parameter_identity.cpp
  rex_frontend_dependent_type_qualification.cpp
  rex_frontend_operator_semantic_path.cpp
  rex_frontend_operator_surface_contract.cpp
  rex_frontend_udl_structural_ownership.cpp
  rex_frontend_using_terminal_provenance.cpp
  rex_frontend_lazy_system_header_fields.cpp
  rex_frontend_local_class_template_constructor_field_identity.cpp
  rex_frontend_standard_template_specialization_transaction.cpp
  rex_frontend_static_member_publication.cpp
  rex_frontend_template_redeclaration_parameter_spelling.cpp
  rex_frontend_namespace_source_fragments.cpp
  rex_frontend_preprocessor_scope_gap.cpp
  rex_unparser_exact_source_surface_ownership.cpp
  rex_unparser_implicit_copy_and_arrow.cpp
  rex_unparser_macro_expansion_surface.cpp
  rex_unparser_string_literal_surface.cpp
  rex_unparser_partial_specialization_identity.cpp
  rex_unparser_source_qualification_identity.cpp
  rex_unparser_source_qualification_provenance.cpp
  rex_unparser_nonreal_use_site_qualification.cpp
  rex_unparser_dependent_typename_return.cpp
  rex_name_qualification_root_contract.cpp
  rex_name_qualification_member_context_contract.cpp
  rex_name_qualification_frontend_input_publication.cpp
  rex_name_qualification_on_demand_header_function.cpp
  rex_unparser_extended_float_identity.cpp
  rex_unparser_contextual_qualification.cpp
  rex_unparser_function_pointer_argument_qualification.cpp
  rex_unparser_lambda_return_type_qualification.cpp
  rex_unparser_member_function_pointer_argument_qualification.cpp
  rex_unparser_typed_type_context.cpp
  rex_unparser_nested_alias_visibility.cpp
  rex_unparser_vararg_type_qualification.cpp
  rex_unparser_enum_initializer_parent.cpp
  rex_unparser_enum_initializer_reference.cpp
  rex_unparser_if_body_parenting.cpp
  rex_unparser_lambda_capture_parent.cpp
  rex_unparser_structured_binding_parent.cpp
  rex_frontend_structured_binding_template_body.cpp
  rex_unparser_template_argument_ownership.cpp
  rex_unparser_inherited_typedef_parameter.C
  rex_unparser_anonymous_enum_declarator_group.cpp
  rex_unparser_anonymous_prefix_identity.cpp
  rex_unparser_anonymous_template_parameters.cpp
  rex_unparser_instantiated_operator_identity.cpp
  rex_unparser_visibility_contract.cpp
  rex_unparser_builtin_prefix_identity.cpp
  rex_unparser_typed_owner_contracts.cpp
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
  rex_frontend_partial_specialization_reused_parameter_owner.cpp
  rex_frontend_out_of_line_template_surface_lifetime.cpp
  rex_frontend_grouped_local_class_template_owner.cpp
  rex_test2026_stl_regex.cpp
  rex_test2026_stl_tuple.cpp
  rex_test2026_stl_type_traits.cpp
  rex_test2026_stl_variant.cpp
  rex_test2026_stl_vector.cpp
  rex_test2026_deduction_guide.cpp
  rex_test2026_sfinae.cpp
  rex_test2026_dependent_qualification.cpp
  rex_test2026_friend_template_saved_spelling.cpp
  rex_test2026_dependent_using_dedup.cpp
  rex_test2026_inline_constructor_preserved.cpp
  rex_test2026_namespace_template_source_order.cpp
  rex_test2026_guarded_friend_template_definition.cpp
  rex_test2026_reopened_namespace_template_spelling.cpp
  rex_frontend_dependent_friend_candidate_set.cpp
  rex_frontend_injected_member_pointer_owner.cpp
  rex_test2026_base_alias_skips_ctor_parameter.cpp
  rex_test2026_anonymous_member_no_qualification.cpp
  rex_test2026_clang_method_context_valgrind_defined_reads.cpp
  rex_test2026_clang_cxx_stmt_type_valgrind_defined_reads.cpp
  rex_test2026_template_instantiation_reachable_scope.cpp
  rex_test2026_declaration_scope_structural_successor.cpp
  rex_test2026_ast_json_clang_option_filter.cpp
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
  rex_test2026_redecl_default_template_arg.cpp
  rex_test2026_template_definition_prior_default.cpp
  rex_test2026_split_template_defaults.cpp
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
  test2005_111.C
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
  test2009_24.C
  test2015_99.C
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
  test2014_23.C
  test2014_25.C
  test2014_42.C
  test2014_44.C
  test2014_46.C
  test2014_51.C
  test2014_52.C
  test2014_59.C
  test2014_64.C
  test2014_65.C
  test2014_66.C
  test2014_69.C
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
)
list(APPEND CXX_TESTCODES
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
  test2006_15.C
  test2006_167.C
  test2006_168.C
  test2006_169.C
  test2006_170.C
  test2006_172.C
  test2006_173.C
  test2006_174.C
  test2006_175.C
  test2006_176.C
  test2006_180.C
  test2006_182.C
  test2006_184.C
  test2006_186.C
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
  test2007_116.C
  test2007_117.C
  test2007_118.C
  test2007_119.C
  test2007_120.C
  test2007_121.C
  test2007_122.C
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
  test2007_31.C
  test2007_32.C
  test2007_33.C
  test2007_34.C
  test2007_36.C
  test2007_37.C
  test2007_39.C
  test2007_45.C
  test2007_54.C
  test2007_56.C
  test2007_60.C
  test2007_62.C
  test2007_63.C
  test2007_64.C
  test2007_65.C
  test2007_66.C
  test2007_67.C
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
  test2007_93.C
  test2007_96.C
  test2007_97.C
  test2007_99.C
  test2008_07.C
  test2009_13.C
  test2009_22.C
  test2009_29.C
  test2009_32.C
  test2009_33.C
  test2009_34.C
  test2009_35.C
  test2009_43.C
  test2009_44.C
  test2009_46.C
  test2009_47.C
  test2010_09.C
  test2010_10.C
  test2010_11.C
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
  test2011_196.C
  test2011_198.C
  test2011_199.C
  test2011_200.C
  test2011_201.C
  test2011_202.C
  test2011_203.C
  test2012_134.C
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
  test2012_79.C
  test2012_86.C
  test2013_14.C
  test2013_185.C
  test2013_197.C
  test2013_19.C
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
  test2014_42.C
  test2014_44.C
  test2014_46.C
  test2014_52.C
  test2014_59.C
  test2014_64.C
  test2014_69.C
  test2015_25.C
  test2015_72_icpc.C
  test2015_83.C
  test2015_84.C
  test2016_52.C
  test2017_05.C
  test2017_08.C
  test2017_12.C
  test2017_20.C
  test2017_33.C
  test2017_34.C
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
  test2017_44.C
)
list(APPEND CXX_TESTCODES
  c++trick.C
  certSignalExample.C
  lexPhase2003_02.C
  longFile.C
  math.C
  restrict_template_parameter.C
  rose-1701-1.C
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
  test2005_85.C
  test2006_08.C
  test2006_09.C
  test2006_11.C
  test2006_127.C
  test2006_128.C
  test2006_130.C
  test2006_15.C
  test2006_167.C
  test2006_168.C
  test2006_169.C
  test2006_170.C
  test2006_172.C
  test2006_173.C
  test2006_174.C
  test2006_175.C
  test2006_176.C
  test2006_180.C
  test2006_182.C
  test2006_184.C
  test2006_186.C
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
  test2007_116.C
  test2007_117.C
  test2007_118.C
  test2007_119.C
  test2007_120.C
  test2007_121.C
  test2007_122.C
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
  test2007_31.C
  test2007_32.C
  test2007_33.C
  test2007_34.C
  test2007_36.C
  test2007_37.C
  test2007_39.C
  test2007_45.C
  test2007_51.C
  test2007_52.C
  test2007_54.C
  test2007_60.C
  test2007_62.C
  test2007_63.C
  test2007_64.C
  test2007_65.C
  test2007_66.C
  test2007_67.C
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
  test2007_93.C
  test2007_96.C
  test2007_97.C
  test2007_99.C
  test2008_07.C
  test2009_13.C
  test2009_21.C
  test2009_22.C
  test2009_29.C
  test2009_32.C
  test2009_33.C
  test2009_34.C
  test2009_35.C
  test2009_43.C
  test2009_44.C
  test2009_46.C
  test2009_47.C
  test2010_09.C
  test2010_10.C
  test2010_11.C
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
  test2011_191.C
  test2011_196.C
  test2011_198.C
  test2011_199.C
  test2011_200.C
  test2011_201.C
  test2011_202.C
  test2011_203.C
  test2012_134.C
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
  test2014_227.C
  test2014_228.C
  test2014_229.C
  test2014_42.C
  test2014_44.C
  test2014_46.C
  test2014_52.C
  test2014_59.C
  test2014_64.C
  test2014_69.C
  test2014_99.C
  test2015_126.C
  test2015_135.C
  test2015_136.C
  test2015_25.C
  test2015_72_icpc.C
  test2015_83.C
  test2015_84.C
  test2016_20.C
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
  test2004_117.C
  test2007_56.C
  test2013_178.C
  test2013_182.C
)

list(APPEND CXX_TESTCODES
  test2006_131.C
  test2014_22.C
)

list(APPEND CXX_TESTCODES
  test2013_09.C)
list(REMOVE_DUPLICATES CXX_TESTCODES)

list(APPEND CXX_TESTCODES_GNUXX17
  rex_frontend_structured_binding_template_body.cpp)

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _rex_cxx_tests_processor)
set(REX_CXX_TESTS_HOST_IS_X86 FALSE)
if(_rex_cxx_tests_processor MATCHES "^(x86_64|amd64|i[3-6]86)$")
  set(REX_CXX_TESTS_HOST_IS_X86 TRUE)
endif()

set(CXX_TESTCODES_X86_ONLY
  test2007_97.C
  test2007_102.C
  test2007_106.C
  test2008_15.C
  test2009_02.C
  test2017_69.C
)
if(NOT REX_CXX_TESTS_HOST_IS_X86)
  list(REMOVE_ITEM CXX_TESTCODES ${CXX_TESTCODES_X86_ONLY})
endif()

list(REMOVE_ITEM CXX_TESTCODES ${CXX_TESTCODES_GNUXX03})

list(REMOVE_ITEM CXX_TESTCODES ${CXX_TESTCODES_GNUXX17})
list(REMOVE_ITEM CXX_TESTCODES ${CXX_TESTCODES_GNUXX20})

function(assert_cxx_specimens_exist list_name)
  foreach(specimen IN LISTS ${list_name})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${specimen}")
      message(FATAL_ERROR
        "Cxx_tests specimen '${specimen}' listed in ${list_name} does not exist")
    endif()
  endforeach()
endfunction()

assert_cxx_specimens_exist(CXX_TESTCODES_GNUXX03)
assert_cxx_specimens_exist(CXX_TESTCODES_GNUXX17)
assert_cxx_specimens_exist(CXX_TESTCODES_GNUXX20)
assert_cxx_specimens_exist(CXX_TESTCODES)

# Keep intentionally ill-formed frontend specimens visible in CTest as
# negative coverage instead of registering them as positive compile tests.
# `test2017_08.C` is a frozen legacy libstdc++ snapshot that no longer compiles
# as a standalone source with modern compilers because it accesses private
# `std::complex` storage directly instead of using the public `__rep()` helper.
set(CXX_TESTCODES_NEGATIVE
  rex_frontend_macro_missing_separator.cpp
  rex_test2025_cfe_error_exit_code.cpp
  rex_test2025_issue99.cpp
  test2006_131.C
  test2014_22.C
  test2016_35.C
  test2005_69.C
  test2015_84.C
  test2014_63.C
  # Intentionally non-compiling template metaprogramming specimen.
  test2015_109.C
  # The template definition names a nonexistent member.  Delayed template
  # parsing used to hide this malformed source until instantiation.
  test2016_85.C
  test2017_08.C
)

function(cxx_exact_negative_diagnostic input_file output_variable)
  if(input_file STREQUAL "rex_frontend_macro_missing_separator.cpp")
    set(_diagnostic
      "rex_frontend_macro_missing_separator.cpp:1:2: error: invalid preprocessing directive")
  elseif(input_file STREQUAL "rex_test2025_cfe_error_exit_code.cpp")
    set(_diagnostic
      "rex_test2025_cfe_error_exit_code.cpp:3:5: error: non-void function 'main' should return a value")
  elseif(input_file STREQUAL "rex_test2025_issue99.cpp")
    set(_diagnostic
      "rex_test2025_issue99.cpp:11:3: error: use of undeclared identifier 'foo'")
  elseif(input_file STREQUAL "test2006_131.C")
    set(_diagnostic
      "test2006_131.C:6:2: error: \"AST has a SgBasicBlock which has the SgIfStmt as a parent, but is not a child of the SgIfStmt IR node.\"")
  elseif(input_file STREQUAL "test2014_22.C")
    set(_diagnostic
      "test2014_22.C:16:21: error: no function template matches function template specialization 'operator<<'")
  elseif(input_file STREQUAL "test2016_35.C")
    set(_diagnostic
      "test2016_35.C:60:5: error: cannot redeclare builtin function '__builtin_fpclassify'")
  elseif(input_file STREQUAL "test2005_69.C")
    set(_diagnostic
      "test2005_69.C:59:25: error: template specialization requires 'template<>'")
  elseif(input_file STREQUAL "test2015_84.C")
    set(_diagnostic
      "test2015_84.C:11:16: error: exception specification of overriding function is more lax than base version")
  elseif(input_file STREQUAL "test2014_63.C")
    set(_diagnostic
      "test2014_63.C:19:16: error: use of class template 'Interval' requires template arguments")
  elseif(input_file STREQUAL "test2015_109.C")
    set(_diagnostic
      "test2015_109.C:33:8: error: template specialization requires 'template<>'")
  elseif(input_file STREQUAL "test2016_85.C")
    set(_diagnostic
      "test2016_85.C:5:70: error: no member named '_M_off' in 'fpos<_StateT>'")
  elseif(input_file STREQUAL "test2017_08.C")
    set(_diagnostic
      "test2017_08.C:25951:24: error: '_M_value' is a private member of 'std::complex<double>'")
  else()
    message(FATAL_ERROR
      "No exact frontend-negative diagnostic is registered for ${input_file}")
  endif()
  set(${output_variable} "${_diagnostic}" PARENT_SCOPE)
endfunction()

assert_cxx_specimens_exist(CXX_TESTCODES_NEGATIVE)
list(REMOVE_ITEM CXX_TESTCODES ${CXX_TESTCODES_NEGATIVE})
list(REMOVE_ITEM CXX_TESTCODES_GNUXX03 ${CXX_TESTCODES_NEGATIVE})
list(REMOVE_ITEM CXX_TESTCODES_GNUXX17 ${CXX_TESTCODES_NEGATIVE})
list(REMOVE_ITEM CXX_TESTCODES_GNUXX20 ${CXX_TESTCODES_NEGATIVE})

rex_register_compile_source_set(REX_CXX_TESTS_GNUXX03_SOURCES
  ${CXX_TESTCODES_GNUXX03})
rex_register_compile_source_set(REX_CXX_TESTS_GNUXX17_SOURCES
  ${CXX_TESTCODES_GNUXX17})
rex_register_compile_source_set(REX_CXX_TESTS_GNUXX20_SOURCES
  ${CXX_TESTCODES_GNUXX20})
rex_register_compile_source_set(REX_CXX_TESTS_DEFAULT_SOURCES
  ${CXX_TESTCODES}
  ${CXX_TESTCODES_NEGATIVE})
rex_register_compile_source_set(REX_CXX_TESTS_NEGATIVE_SOURCES
  ${CXX_TESTCODES_NEGATIVE})

set(ROSE_FLAGS
  -rose:verbose 0
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

set(CXX_TESTCODES_EXTENDED_INSTRUMENTED_TIMEOUT
  test2010_02.C
  test2017_24.C
  test2017_99.C
  longFile.C
  test2013_13.C
  test2013_14.C
  test2013_15.C
)

set(CXX_UNFOLDED_CONSTANTS_EXTENDED_INSTRUMENTED_TIMEOUT
  longFile.C
  test2013_13.C
  test2013_14.C
  test2013_15.C
  test2017_24.C
)

function(rex_set_cxx_extended_instrumented_timeout test_name specimen)
  if(REX_EXTENDED_TEST_TIMEOUTS AND
     specimen IN_LIST CXX_TESTCODES_EXTENDED_INSTRUMENTED_TIMEOUT)
    set_tests_properties(${test_name} PROPERTIES
      TIMEOUT 10800
      PROCESSORS 16)
  endif()
endfunction()

set(ROSE_FLAGS ${_rose_flags_base} -std=gnu++03)
foreach(file_to_test ${CXX_TESTCODES_GNUXX03})
  compile_test(${file_to_test} "Cxx_tests")
  rex_compile_test_name(_test_name "Cxx_tests" ${file_to_test})
  rex_lock_test_for_input(${_test_name} ${file_to_test})
  set_tests_properties(${_test_name} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  rex_set_cxx_extended_instrumented_timeout(${_test_name} ${file_to_test})
endforeach()
set(ROSE_FLAGS ${_rose_flags_default})

set(ROSE_FLAGS ${_rose_flags_base} -std=gnu++17)
foreach(file_to_test ${CXX_TESTCODES_GNUXX17})
  compile_test(${file_to_test} "Cxx_tests")
  rex_compile_test_name(_test_name "Cxx_tests" ${file_to_test})
  rex_lock_test_for_input(${_test_name} ${file_to_test})
  set_tests_properties(${_test_name} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  rex_set_cxx_extended_instrumented_timeout(${_test_name} ${file_to_test})
endforeach()
set(ROSE_FLAGS ${_rose_flags_default})

set(ROSE_FLAGS ${_rose_flags_base} -std=gnu++20)
foreach(file_to_test ${CXX_TESTCODES_GNUXX20})
  compile_test(${file_to_test} "Cxx_tests")
  rex_compile_test_name(_test_name "Cxx_tests" ${file_to_test})
  rex_lock_test_for_input(${_test_name} ${file_to_test})
  set_tests_properties(${_test_name} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  rex_set_cxx_extended_instrumented_timeout(${_test_name} ${file_to_test})
endforeach()
set(ROSE_FLAGS ${_rose_flags_default})

foreach(file_to_test ${CXX_TESTCODES})
  compile_test(${file_to_test} "Cxx_tests")
  rex_compile_test_name(_test_name "Cxx_tests" ${file_to_test})
  rex_lock_test_for_input(${_test_name} ${file_to_test})
  set_tests_properties(${_test_name} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  rex_set_cxx_extended_instrumented_timeout(${_test_name} ${file_to_test})
endforeach()

foreach(file_to_test ${CXX_TESTCODES_NEGATIVE})
  cxx_exact_negative_diagnostic(${file_to_test} _exact_diagnostic)
  compile_exact_negative_test(${file_to_test} "Cxx_tests"
    "${_exact_diagnostic}")
  rex_compile_test_name(_test_name "Cxx_tests" ${file_to_test})
  rex_lock_test_for_input(${_test_name} ${file_to_test})
  set_tests_properties(${_test_name} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()

function(add_cxx_unfolded_constants_test file_to_test expected_diagnostic)
  set(_unfolded_target "${file_to_test}.unfoldedConstants-o")
  set(_unfolded_flags ${_rose_flags_base})
  rex_get_cxx_specimen_extra_flags(_extra_frontend_flags ${file_to_test})
  if(file_to_test IN_LIST CXX_TESTCODES_GNUXX03)
    list(APPEND _unfolded_flags -std=gnu++03)
  elseif(file_to_test IN_LIST CXX_TESTCODES_GNUXX17)
    list(APPEND _unfolded_flags -std=gnu++17)
  elseif(file_to_test IN_LIST CXX_TESTCODES_GNUXX20)
    list(APPEND _unfolded_flags -std=gnu++20)
  else()
    list(APPEND _unfolded_flags -std=gnu++14)
  endif()
  if(NOT "${expected_diagnostic}" STREQUAL "")
    string(REPLACE "/" "_" _unfolded_log_key "${file_to_test}")
    string(REPLACE "." "_" _unfolded_log_key "${_unfolded_log_key}")
    rex_add_clang_frontend_negative_test(
      ${_unfolded_target}
      Cxx_tests
      ${CMAKE_CURRENT_BINARY_DIR}
      ${_unfolded_log_key}.unfolded.frontend-negative.log
      "${expected_diagnostic}"
      $<TARGET_FILE:testTranslatorUnfoldedConstants>
      ${_unfolded_flags} ${_extra_frontend_flags} ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  else()
    add_test(
      NAME ${_unfolded_target}
      COMMAND $<TARGET_FILE:testTranslatorUnfoldedConstants>
        ${_unfolded_flags} ${_extra_frontend_flags} ${ROSE_INCLUDE_FLAGS}
        -I${CMAKE_CURRENT_SOURCE_DIR}
        -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
    set_tests_properties(${_unfolded_target} PROPERTIES
      LABELS Cxx_tests
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  endif()
  set_property(TEST ${_unfolded_target} APPEND PROPERTY ENVIRONMENT
               "ROSE_TEST_OUTPUT_DIR=${ROSE_TEST_OUTPUT_DIR}")
  rex_lock_test_for_input(${_unfolded_target} ${file_to_test})
  if(REX_EXTENDED_TEST_TIMEOUTS AND
     file_to_test IN_LIST CXX_UNFOLDED_CONSTANTS_EXTENDED_INSTRUMENTED_TIMEOUT)
    set_tests_properties(${_unfolded_target} PROPERTIES
      TIMEOUT 10800
      PROCESSORS 16)
  endif()
endfunction()

# Additional unfolded-constants coverage for the default CTest run.
set(_unfolded_constants_tests ${CXX_TESTCODES})
list(FILTER _unfolded_constants_tests INCLUDE REGEX "\\.C$")
list(REMOVE_DUPLICATES _unfolded_constants_tests)

foreach(file_to_test ${_unfolded_constants_tests})
  add_cxx_unfolded_constants_test(${file_to_test} "")
endforeach()

set(_unfolded_negative_tests ${CXX_TESTCODES_NEGATIVE})
list(FILTER _unfolded_negative_tests INCLUDE REGEX "\\.C$")
list(REMOVE_DUPLICATES _unfolded_negative_tests)

foreach(file_to_test ${_unfolded_negative_tests})
  cxx_exact_negative_diagnostic(${file_to_test} _exact_diagnostic)
  add_cxx_unfolded_constants_test(${file_to_test} "${_exact_diagnostic}")
endforeach()

# Additional custom command-line coverage.
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
set_tests_properties(Cxx_tests_test2005_56_special PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(Cxx_tests_test2005_56_special test2005_56.C)

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
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
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
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
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
set_tests_properties(Cxx_tests_common_configure_test_with_link_part_3 PROPERTIES LABELS Cxx_tests RESOURCE_LOCK rose_output_conftest )

add_test(
  NAME Cxx_tests_suppress_variable_declaration_normalization
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_multible_variables_in_variable_declaration.C
)
set_tests_properties(Cxx_tests_suppress_variable_declaration_normalization PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_suppress_variable_declaration_normalization
  test_multible_variables_in_variable_declaration.C)

add_test(
  NAME Cxx_tests_rex_unparser_single_pass_formatting
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_single_pass_formatting.cpp
)
set_tests_properties(Cxx_tests_rex_unparser_single_pass_formatting
  PROPERTIES LABELS "rex;Cxx_tests;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_single_pass_formatting
  rex_unparser_single_pass_formatting.cpp)

add_test(
  NAME Cxx_tests_testOption
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 0 -nostdinc
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2015_08.C
)
set_tests_properties(Cxx_tests_testOption PROPERTIES LABELS Cxx_tests )
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
    "printf '%s\n' 'class A { int x; float y;}; int main() { int x=0; return x; }' > roseTestProgram.C && \"$@\""
    dummy
    $<TARGET_FILE:${translator}>
    -DthisIsOK -D thisIsAnError -rose:verbose 0
    roseTestProgram.C
)
set_tests_properties(Cxx_tests_testUsingBadCommandline PROPERTIES LABELS Cxx_tests )

add_test(
  NAME Cxx_tests_testBuildUsingNoPath
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "printf '%s\n' 'class A { int x; float y;}; int main() { int x=0; return x; }' > roseTestProgram.C && \"$@\""
    dummy
    $<TARGET_FILE:${translator}>
    -rose:verbose 0
    roseTestProgram.C
)
set_tests_properties(Cxx_tests_testBuildUsingNoPath PROPERTIES LABELS Cxx_tests )

add_test(
  NAME Cxx_tests_testBuildUsingRelativePath
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "printf '%s\n' 'class A { int x; float y;}; int main() { int x=0; return x; }' > roseTestProgram.C && \"$@\""
    dummy
    $<TARGET_FILE:${translator}>
    -rose:verbose 0
    ../Cxx_tests/roseTestProgram.C
)
set_tests_properties(Cxx_tests_testBuildUsingRelativePath PROPERTIES LABELS Cxx_tests )

add_test(
  NAME Cxx_tests_testBuildUsingAbsolutePath
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "printf '%s\n' 'class A { int x; float y;}; int main() { int x=0; return x; }' > roseTestProgram.C && \"$@\" `pwd`/roseTestProgram.C"
    dummy
    $<TARGET_FILE:${translator}>
    -rose:verbose 0
)
set_tests_properties(Cxx_tests_testBuildUsingAbsolutePath PROPERTIES LABELS Cxx_tests )

add_test(
  NAME Cxx_tests_test_ansi_and_nonansi_modes
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "\"$@\" -rose:verbose 0 -ansi -c ${CMAKE_CURRENT_SOURCE_DIR}/test_ansi.C && \"$@\" -rose:verbose 0 -c ${CMAKE_CURRENT_SOURCE_DIR}/test_nonansi.C"
    dummy
    $<TARGET_FILE:${translator}>
)
set_tests_properties(Cxx_tests_test_ansi_and_nonansi_modes PROPERTIES LABELS Cxx_tests )

add_test(
  NAME Cxx_tests_multipleDeclarationsBug
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/markus_1a.C
    ${CMAKE_CURRENT_SOURCE_DIR}/markus_1b.C
)
set_tests_properties(Cxx_tests_multipleDeclarationsBug PROPERTIES LABELS Cxx_tests )

set(_mpi_lam_include_dir
  /home/dquinlan/ROSE/ROSE_TESTS/MPI_header_tests/MPI_header_tests_64bit/LAM_MPI/include)
set(_mpi_lam_input
  /home/dquinlan/ROSE/ROSE_TESTS/MPI_header_tests/MPI_header_tests_64bit/tests/input.C)
if(EXISTS "${_mpi_lam_include_dir}" AND EXISTS "${_mpi_lam_input}")
  add_test(
    NAME Cxx_tests_test_MPI_LAM
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:${translator}>
      -rose:verbose 0
      -I${_mpi_lam_include_dir}
      -c ${_mpi_lam_input}
  )
  set_tests_properties(Cxx_tests_test_MPI_LAM PROPERTIES LABELS Cxx_tests )
endif()

add_test(
  NAME Cxx_tests_test_multiple_files
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 1 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test2012_246a.C
    ${CMAKE_CURRENT_SOURCE_DIR}/test2012_246b.C
)
set_tests_properties(Cxx_tests_test_multiple_files PROPERTIES LABELS Cxx_tests )

add_test(
  NAME CXXTEST_test2012_246a
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
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
set_tests_properties(Cxx_tests_test_multiple_files_2 PROPERTIES LABELS Cxx_tests )

add_test(
  NAME Cxx_tests_test_multiple_files_3
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test2019_30a.C
    ${CMAKE_CURRENT_SOURCE_DIR}/test2019_30b.C
)
set_tests_properties(Cxx_tests_test_multiple_files_3 PROPERTIES LABELS Cxx_tests )

add_test(
  NAME Cxx_tests_test_multiple_files_4
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test2019_42a.C
    ${CMAKE_CURRENT_SOURCE_DIR}/test2019_42b.C
)
set_tests_properties(Cxx_tests_test_multiple_files_4 PROPERTIES LABELS Cxx_tests )

# Issue 476: switch init-statements with case-local declarations must survive
# compile, link, and execution in the normal Cxx test suite.
add_test(
  NAME Cxx_tests_test2017_85_runs
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c "\"$@\" && ./test2017_85_run"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/test2017_85.C
    -o test2017_85_run
)
set_tests_properties(Cxx_tests_test2017_85_runs PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(Cxx_tests_test2017_85_runs test2017_85.C)

add_test(
  NAME Cxx_tests_test_bug_980
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:${translator}>
    -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp_vector.C
)
set_tests_properties(Cxx_tests_test_bug_980 PROPERTIES LABELS Cxx_tests )

add_test(
  NAME Cxx_tests_testParser
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testParser>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test2001_01.C
)
set_tests_properties(Cxx_tests_testParser PROPERTIES LABELS Cxx_tests )

add_test(
  NAME Cxx_tests_performance_timing
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testGraphGeneration>
    -w -rose:verbose 3
    -rose:skipAstConsistancyTests -rose:skip_commentsAndDirectives
    -rose:skip_unparse -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/testPerformance.C
)
set_tests_properties(Cxx_tests_performance_timing PROPERTIES LABELS Cxx_tests )

add_test(
  NAME Cxx_tests_test_pdf_generation
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c "\"$@\" -w -rose:verbose 0 -rose:skipAstConsistancyTests -rose:skip_commentsAndDirectives -rose:skip_unparse -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/test2004_77.C && ls -l *.pdf && rm -f *.pdf"
    dummy
    $<TARGET_FILE:testPDFGeneration>
)
set_tests_properties(Cxx_tests_test_pdf_generation PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(Cxx_tests_test_pdf_generation test2004_77.C)

# Issue 482: if-body blocks must remain linked through the SgIfStmt child list.
add_executable(
  rex_test2026_issue482_if_body_parenting_translator
  rex_test2026_issue482_if_body_parenting_translator.cpp
)
target_link_libraries(
  rex_test2026_issue482_if_body_parenting_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_declaration_scope_contract_translator
  rex_frontend_declaration_scope_contract_translator.cpp
)
target_link_libraries(
  rex_frontend_declaration_scope_contract_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_implicit_control_flow_scope_translator
  rex_frontend_implicit_control_flow_scope_translator.cpp
)
target_link_libraries(
  rex_frontend_implicit_control_flow_scope_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_preprocessor_scope_gap_translator
  rex_frontend_preprocessor_scope_gap_translator.cpp
)
target_link_libraries(
  rex_frontend_preprocessor_scope_gap_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_friend_template_canonical_chain_translator
  rex_frontend_friend_template_canonical_chain_translator.cpp
)
target_link_libraries(
  rex_frontend_friend_template_canonical_chain_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_new_initializer_source_form_translator
  rex_frontend_new_initializer_source_form_translator.cpp
)
target_link_libraries(
  rex_frontend_new_initializer_source_form_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_gnu_binary_conditional_translator
  rex_frontend_gnu_binary_conditional_translator.cpp
)
target_link_libraries(
  rex_frontend_gnu_binary_conditional_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_linkage_container_contract_translator
  rex_frontend_linkage_container_contract_translator.cpp
)
target_link_libraries(
  rex_frontend_linkage_container_contract_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_field_tag_introduction_translator
  rex_frontend_field_tag_introduction_translator.cpp
)
target_link_libraries(
  rex_frontend_field_tag_introduction_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_unparser_source_qualification_provenance_translator
  rex_unparser_source_qualification_provenance_translator.cpp
)
target_link_libraries(
  rex_unparser_source_qualification_provenance_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_cxx_dependent_qualified_template_type_translator
  rex_cxx_dependent_qualified_template_type_translator.cpp
)
target_link_libraries(
  rex_cxx_dependent_qualified_template_type_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_dependent_template_argument_source_owner_translator
  rex_frontend_dependent_template_argument_source_owner_translator.cpp
)
target_link_libraries(
  rex_frontend_dependent_template_argument_source_owner_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_function_template_default_parameter_owner_translator
  rex_frontend_function_template_default_parameter_owner_translator.cpp
)
target_link_libraries(
  rex_frontend_function_template_default_parameter_owner_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_semantic_alias_template_parameter_owner_translator
  rex_frontend_semantic_alias_template_parameter_owner_translator.cpp
)
target_link_libraries(
  rex_frontend_semantic_alias_template_parameter_owner_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_anonymous_aggregate_source_owner_translator
  rex_frontend_anonymous_aggregate_source_owner_translator.cpp
)
target_link_libraries(
  rex_frontend_anonymous_aggregate_source_owner_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_qualified_template_local_argument_owner_translator
  rex_frontend_qualified_template_local_argument_owner_translator.cpp
)
target_link_libraries(
  rex_frontend_qualified_template_local_argument_owner_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_explicit_member_specialization_header_owner_translator
  rex_frontend_explicit_member_specialization_header_owner_translator.cpp
)
target_link_libraries(
  rex_frontend_explicit_member_specialization_header_owner_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_dependent_typename_use_owner_translator
  rex_frontend_dependent_typename_use_owner_translator.cpp
)
target_link_libraries(
  rex_frontend_dependent_typename_use_owner_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_semantic_auxiliary_token_frontier_translator
  rex_frontend_semantic_auxiliary_token_frontier_translator.cpp
)
target_link_libraries(
  rex_frontend_semantic_auxiliary_token_frontier_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_name_qualification_root_contract_translator
  rex_name_qualification_root_contract_translator.cpp
)
target_link_libraries(
  rex_name_qualification_root_contract_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_unparser_visibility_contract_translator
  rex_unparser_visibility_contract_translator.cpp
)
target_link_libraries(
  rex_unparser_visibility_contract_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_unparser_typed_owner_contracts_translator
  rex_unparser_typed_owner_contracts_translator.cpp
)
target_link_libraries(
  rex_unparser_typed_owner_contracts_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_access_label_lexical_ownership_translator
  rex_frontend_access_label_lexical_ownership_translator.cpp
)
target_link_libraries(
  rex_frontend_access_label_lexical_ownership_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_function_publication_contract_translator
  rex_frontend_function_publication_contract_translator.cpp
)
target_link_libraries(
  rex_frontend_function_publication_contract_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_function_production_contract_translator
  rex_frontend_function_production_contract_translator.cpp
)
target_link_libraries(
  rex_frontend_function_production_contract_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_include_ownership_missing_rejected
  rex_frontend_include_ownership_missing_rejected.cpp
)
target_include_directories(
  rex_frontend_include_ownership_missing_rejected
  PRIVATE
    ${ROSE_INCLUDES}
    ${CMAKE_SOURCE_DIR}/src/frontend/Clang
)
rose_force_system_include_dirs(
  rex_frontend_include_ownership_missing_rejected
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)
target_link_libraries(
  rex_frontend_include_ownership_missing_rejected
  ROSE_DLL
  ${link_with_libraries}
)
add_test(
  NAME Cxx_tests_rex_frontend_include_ownership_missing_rejected
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_frontend_include_ownership_missing_rejected.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc 'REX_FRONTEND_INVARIANT[include-ownership]: physical path=/rex/missing/preprocessor/ownership.hpp has no exact preprocessor ownership producer' rex_frontend_include_ownership_missing_rejected.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_include_ownership_missing_rejected>
)
set_tests_properties(
  Cxx_tests_rex_frontend_include_ownership_missing_rejected
  PROPERTIES LABELS "rex;CFE;Cxx_tests"
)

add_executable(
  rex_frontend_variable_declarator_identity_translator
  rex_frontend_variable_declarator_identity_translator.cpp
)
target_link_libraries(
  rex_frontend_variable_declarator_identity_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_mixed_declarator_group_translator
  rex_frontend_mixed_declarator_group_translator.cpp
)
target_link_libraries(
  rex_frontend_mixed_declarator_group_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_nonreal_auxiliary_owner_translator
  rex_frontend_nonreal_auxiliary_owner_translator.cpp
)
target_link_libraries(
  rex_frontend_nonreal_auxiliary_owner_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_operator_semantic_path_translator
  rex_frontend_operator_semantic_path_translator.cpp
)
target_link_libraries(
  rex_frontend_operator_semantic_path_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_operator_surface_contract_translator
  rex_frontend_operator_surface_contract_translator.cpp
)
target_link_libraries(
  rex_frontend_operator_surface_contract_translator
  ROSE_DLL
  ${link_with_libraries}
)
target_include_directories(
  rex_frontend_operator_surface_contract_translator PRIVATE
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astJson
)

add_executable(
  rex_frontend_initializer_destination_type_translator
  rex_frontend_initializer_destination_type_translator.cpp
)
target_link_libraries(
  rex_frontend_initializer_destination_type_translator
  ROSE_DLL
  ${link_with_libraries}
)
target_include_directories(
  rex_frontend_initializer_destination_type_translator PRIVATE
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astJson
)

add_executable(
  rex_frontend_source_location_builtin_translator
  rex_frontend_source_location_builtin_translator.cpp
)
target_link_libraries(
  rex_frontend_source_location_builtin_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_semantic_expression_wrappers_translator
  rex_frontend_semantic_expression_wrappers_translator.cpp
)
target_link_libraries(
  rex_frontend_semantic_expression_wrappers_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_statement_attributes_translator
  rex_frontend_statement_attributes_translator.cpp
)
target_link_libraries(
  rex_frontend_statement_attributes_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_class_source_range_translator
  rex_frontend_class_source_range_translator.cpp
)
target_link_libraries(
  rex_frontend_class_source_range_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_namespace_source_fragments_translator
  rex_frontend_namespace_source_fragments_translator.cpp
)
target_link_libraries(
  rex_frontend_namespace_source_fragments_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_support_namespace_semantic_root_translator
  rex_frontend_support_namespace_semantic_root_translator.cpp
)
target_link_libraries(
  rex_frontend_support_namespace_semantic_root_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_injected_class_nttp_provenance_translator
  rex_frontend_injected_class_nttp_provenance_translator.cpp
)
target_link_libraries(
  rex_frontend_injected_class_nttp_provenance_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_function_body_auxiliary_provenance_translator
  rex_frontend_function_body_auxiliary_provenance_translator.cpp
)
target_link_libraries(
  rex_frontend_function_body_auxiliary_provenance_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_semantic_auxiliary_access_translator
  rex_frontend_semantic_auxiliary_access_translator.cpp
)
target_link_libraries(
  rex_frontend_semantic_auxiliary_access_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_explicit_specialization_instantiation_chain_translator
  rex_frontend_explicit_specialization_instantiation_chain_translator.cpp
)
target_link_libraries(
  rex_frontend_explicit_specialization_instantiation_chain_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_requires_local_parameter_owner_translator
  rex_frontend_requires_local_parameter_owner_translator.cpp
)
target_link_libraries(
  rex_frontend_requires_local_parameter_owner_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_application_header_semantic_pragma_owner_translator
  rex_frontend_application_header_semantic_pragma_owner_translator.cpp
)
target_link_libraries(
  rex_frontend_application_header_semantic_pragma_owner_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_application_header_member_publication_translator
  rex_frontend_application_header_member_publication_translator.cpp
)
target_link_libraries(
  rex_frontend_application_header_member_publication_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_dependent_member_typedef_identity_translator
  rex_frontend_dependent_member_typedef_identity_translator.cpp
)
target_link_libraries(
  rex_frontend_dependent_member_typedef_identity_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_namespace_cross_tu_chain_translator
  rex_frontend_namespace_cross_tu_chain_translator.cpp
)
target_link_libraries(
  rex_frontend_namespace_cross_tu_chain_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_gnu_null_literal_translator
  rex_frontend_gnu_null_literal_translator.cpp
)
target_link_libraries(
  rex_frontend_gnu_null_literal_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_choose_expression_operand_contract
  rex_frontend_choose_expression_operand_contract.cpp
)
target_include_directories(
  rex_frontend_choose_expression_operand_contract
  PRIVATE ${ROSE_TOP_SRC_DIR}/src/frontend/Clang
)
rose_force_system_include_dirs(
  rex_frontend_choose_expression_operand_contract
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)
target_link_libraries(
  rex_frontend_choose_expression_operand_contract
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME Cxx_tests_rex_frontend_choose_expression_operand_contract_positive
  COMMAND $<TARGET_FILE:rex_frontend_choose_expression_operand_contract>
)
set_tests_properties(
  Cxx_tests_rex_frontend_choose_expression_operand_contract_positive
  PROPERTIES LABELS "Cxx_tests;rex;CFE"
)

foreach(_rex_choose_expression_operand condition true false)
  add_test(
    NAME Cxx_tests_rex_frontend_choose_expression_rejects_null_${_rex_choose_expression_operand}_operand
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_frontend_choose_expression_${_rex_choose_expression_operand}.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_FRONTEND_INVARIANT\\[choose-expression-operand\\]: ${_rex_choose_expression_operand} operand is not an exact Clang expression$' rex_frontend_choose_expression_${_rex_choose_expression_operand}.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_frontend_choose_expression_operand_contract>
      ${_rex_choose_expression_operand}
  )
  set_tests_properties(
    Cxx_tests_rex_frontend_choose_expression_rejects_null_${_rex_choose_expression_operand}_operand
    PROPERTIES LABELS "Cxx_tests;rex;CFE;death"
  )
endforeach()
unset(_rex_choose_expression_operand)

add_executable(
  rex_frontend_predefined_identifier_type_contract
  rex_frontend_predefined_identifier_type_contract.cpp
)
target_include_directories(
  rex_frontend_predefined_identifier_type_contract
  PRIVATE ${ROSE_TOP_SRC_DIR}/src/frontend/Clang
)
target_link_libraries(
  rex_frontend_predefined_identifier_type_contract
  ROSE_DLL
  ${link_with_libraries}
)
add_test(
  NAME Cxx_tests_rex_frontend_predefined_identifier_type_contract
  COMMAND $<TARGET_FILE:rex_frontend_predefined_identifier_type_contract>
)
set_tests_properties(
  Cxx_tests_rex_frontend_predefined_identifier_type_contract
  PROPERTIES LABELS "Cxx_tests;rex;CFE"
)
add_test(
  NAME Cxx_tests_rex_frontend_predefined_identifier_rejects_equivalent_distinct_type
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$1\" equivalent-distinct >rex_frontend_predefined_identifier_equivalent_distinct.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc 'REX_FRONTEND_INVARIANT[predefined-identifier-type]: predefined identifier does not retain its exact canonical Clang type identity' rex_frontend_predefined_identifier_equivalent_distinct.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_predefined_identifier_type_contract>
)
set_tests_properties(
  Cxx_tests_rex_frontend_predefined_identifier_rejects_equivalent_distinct_type
  PROPERTIES LABELS "Cxx_tests;rex;CFE;death"
)

add_executable(
  rex_frontend_function_source_location_contract
  rex_frontend_function_source_location_contract.cpp
)
target_include_directories(
  rex_frontend_function_source_location_contract
  PRIVATE ${ROSE_TOP_SRC_DIR}/src/frontend/Clang
)
rose_force_system_include_dirs(
  rex_frontend_function_source_location_contract
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)
target_link_libraries(
  rex_frontend_function_source_location_contract
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME Cxx_tests_rex_frontend_function_source_location_contract_positive
  COMMAND $<TARGET_FILE:rex_frontend_function_source_location_contract>
)
set_tests_properties(
  Cxx_tests_rex_frontend_function_source_location_contract_positive
  PROPERTIES LABELS "Cxx_tests;rex;CFE"
)

add_executable(
  rex_frontend_source_group_ownership_contract
  rex_frontend_source_group_ownership_contract.cpp
)
target_include_directories(
  rex_frontend_source_group_ownership_contract
  PRIVATE ${ROSE_TOP_SRC_DIR}/src/frontend/Clang
)
rose_force_system_include_dirs(
  rex_frontend_source_group_ownership_contract
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)
target_link_libraries(
  rex_frontend_source_group_ownership_contract
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME Cxx_tests_rex_frontend_detached_source_group_is_not_completed
  COMMAND $<TARGET_FILE:rex_frontend_source_group_ownership_contract>
)
set_tests_properties(
  Cxx_tests_rex_frontend_detached_source_group_is_not_completed
  PROPERTIES LABELS "Cxx_tests;rex;CFE"
)

add_test(
  NAME Cxx_tests_rex_frontend_explicit_function_without_source_location_is_rejected
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$1\" explicit-invalid >rex_frontend_function_source_location.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc \"REX_FRONTEND_INVARIANT[function-source-location]: explicit non-builtin function 'rex_explicit_invalid' has no valid non-builtin source location\" rex_frontend_function_source_location.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_function_source_location_contract>
)
set_tests_properties(
  Cxx_tests_rex_frontend_explicit_function_without_source_location_is_rejected
  PROPERTIES LABELS "Cxx_tests;rex;CFE;death"
)

add_executable(
  rex_frontend_ordered_declaration_provenance_contract
  rex_frontend_ordered_declaration_provenance_contract.cpp
)
target_include_directories(
  rex_frontend_ordered_declaration_provenance_contract
  PRIVATE ${ROSE_TOP_SRC_DIR}/src/frontend/Clang
)
rose_force_system_include_dirs(
  rex_frontend_ordered_declaration_provenance_contract
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)
target_link_libraries(
  rex_frontend_ordered_declaration_provenance_contract
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME Cxx_tests_rex_frontend_ordered_declaration_provenance_contract_positive
  COMMAND $<TARGET_FILE:rex_frontend_ordered_declaration_provenance_contract>
)
set_tests_properties(
  Cxx_tests_rex_frontend_ordered_declaration_provenance_contract_positive
  PROPERTIES LABELS "Cxx_tests;rex;CFE"
)

foreach(
  _rex_ordered_declaration_invalid_state
  unpositioned-ordinary
  order-without-start
  start-without-order
  mismatched-occurrence
  generated-without-order
  generated-wrong-kind
  generated-transformation
  generated-lexical-dependency
)
  if(_rex_ordered_declaration_invalid_state STREQUAL "unpositioned-ordinary")
    set(_rex_ordered_declaration_type SgVariableDeclaration)
    set(_rex_ordered_declaration_name "")
    set(_rex_ordered_declaration_values "real-start=0 real-end=0 source-order=0 source-occurrence=0")
  elseif(_rex_ordered_declaration_invalid_state STREQUAL "order-without-start")
    set(_rex_ordered_declaration_type SgVariableDeclaration)
    set(_rex_ordered_declaration_name "")
    set(_rex_ordered_declaration_values "real-start=0 real-end=0 source-order=31 source-occurrence=0")
  elseif(_rex_ordered_declaration_invalid_state STREQUAL "start-without-order")
    set(_rex_ordered_declaration_type SgVariableDeclaration)
    set(_rex_ordered_declaration_name "")
    set(_rex_ordered_declaration_values "real-start=1 real-end=1 source-order=0 source-occurrence=31")
  elseif(_rex_ordered_declaration_invalid_state STREQUAL "mismatched-occurrence")
    set(_rex_ordered_declaration_type SgVariableDeclaration)
    set(_rex_ordered_declaration_name "")
    set(_rex_ordered_declaration_values "real-start=1 real-end=1 source-order=31 source-occurrence=32")
  elseif(_rex_ordered_declaration_invalid_state STREQUAL "generated-wrong-kind")
    set(_rex_ordered_declaration_type SgVariableDeclaration)
    set(_rex_ordered_declaration_name "")
    set(_rex_ordered_declaration_values "real-start=0 real-end=0 source-order=31 source-occurrence=0")
  elseif(_rex_ordered_declaration_invalid_state STREQUAL "generated-without-order")
    set(_rex_ordered_declaration_type SgTypedefDeclaration)
    set(_rex_ordered_declaration_name rex_generated_dependency)
    set(_rex_ordered_declaration_values "real-start=0 real-end=0 source-order=0 source-occurrence=0")
  else()
    set(_rex_ordered_declaration_type SgTypedefDeclaration)
    set(_rex_ordered_declaration_name rex_generated_dependency)
    set(_rex_ordered_declaration_values "real-start=0 real-end=0 source-order=31 source-occurrence=0")
  endif()
  add_test(
    NAME Cxx_tests_rex_frontend_ordered_declaration_rejects_${_rex_ordered_declaration_invalid_state}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; set +e; \"$1\" \"$2\" >rex_frontend_ordered_declaration_${_rex_ordered_declaration_invalid_state}.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_FRONTEND_INVARIANT\\[ordered-declaration-attachment\\]: context=${_rex_ordered_declaration_invalid_state} declaration=0x[[:xdigit:]]+ type=${_rex_ordered_declaration_type} name=${_rex_ordered_declaration_name} requires exactly one source lexical start/order occurrence or one canonical-generated namespace shell \\(${_rex_ordered_declaration_values}\\)$' rex_frontend_ordered_declaration_${_rex_ordered_declaration_invalid_state}.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_frontend_ordered_declaration_provenance_contract>
      ${_rex_ordered_declaration_invalid_state}
  )
  set_tests_properties(
    Cxx_tests_rex_frontend_ordered_declaration_rejects_${_rex_ordered_declaration_invalid_state}
    PROPERTIES LABELS "Cxx_tests;rex;CFE;death"
  )
endforeach()
unset(_rex_ordered_declaration_invalid_state)
unset(_rex_ordered_declaration_type)
unset(_rex_ordered_declaration_name)
unset(_rex_ordered_declaration_values)

add_executable(
  rex_frontend_literal_spelling_translator
  rex_frontend_literal_spelling_translator.cpp
)
target_link_libraries(
  rex_frontend_literal_spelling_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_udl_structural_ownership_translator
  rex_frontend_udl_structural_ownership_translator.cpp
)
target_link_libraries(
  rex_frontend_udl_structural_ownership_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_using_terminal_provenance_translator
  rex_frontend_using_terminal_provenance_translator.cpp
)
target_link_libraries(
  rex_frontend_using_terminal_provenance_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_function_name_source_form_translator
  rex_frontend_function_name_source_form_translator.cpp
)
target_link_libraries(
  rex_frontend_function_name_source_form_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_typeof_function_declarator_translator
  rex_frontend_typeof_function_declarator_translator.cpp
)
target_link_libraries(
  rex_frontend_typeof_function_declarator_translator
  ROSE_DLL
  ${link_with_libraries}
)
target_include_directories(
  rex_frontend_typeof_function_declarator_translator PRIVATE
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astJson
)

add_executable(
  rex_frontend_embedded_base_owner_translator
  rex_frontend_embedded_base_owner_translator.cpp
)
target_link_libraries(
  rex_frontend_embedded_base_owner_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_system_header_default_template_translator
  rex_frontend_system_header_default_template_translator.cpp
)
target_link_libraries(
  rex_frontend_system_header_default_template_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_system_header_primary_definition_translator
  rex_frontend_system_header_primary_definition_translator.cpp
)
target_link_libraries(
  rex_frontend_system_header_primary_definition_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_alias_elaboration_translator
  rex_frontend_alias_elaboration_translator.cpp
)
target_link_libraries(
  rex_frontend_alias_elaboration_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_predefined_identifier_translator
  rex_frontend_predefined_identifier_translator.cpp
)
target_link_libraries(
  rex_frontend_predefined_identifier_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME Cxx_tests_rex_frontend_gnu_null_literal_roundtrip_and_provenance
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_gnu_null_literal_translator>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_gnu_null_literal.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_gnu_null_literal_roundtrip_and_provenance
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_gnu_null_literal_roundtrip_and_provenance
  rex_frontend_gnu_null_literal.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_literal_spelling_roundtrip_and_provenance
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_literal_spelling_translator>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -std=c++20
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_literal_spelling.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_literal_spelling_roundtrip_and_provenance
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_literal_spelling_roundtrip_and_provenance
  rex_frontend_literal_spelling.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_udl_structural_ownership
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_udl_structural_ownership_translator>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -std=c++20
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_udl_structural_ownership.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_udl_structural_ownership
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_udl_structural_ownership
  rex_frontend_udl_structural_ownership.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_using_terminal_provenance
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_using_terminal_provenance_translator>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -std=c++20
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_using_terminal_provenance.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_using_terminal_provenance
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_using_terminal_provenance
  rex_frontend_using_terminal_provenance.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_function_name_source_form
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_function_name_source_form_translator>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -std=c++20
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_macro_expansion_surface.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_function_name_source_form
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_function_name_source_form
  rex_unparser_macro_expansion_surface.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_typeof_function_declarator_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_typeof_function_declarator_translator>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_typeof_function_declarator.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_typeof_function_declarator_structure
  PROPERTIES LABELS "Cxx_tests;rex;CFE;AST_JSON;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_typeof_function_declarator_structure
  rex_frontend_typeof_function_declarator.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_embedded_base_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_embedded_base_owner_translator>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -std=c++20
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_embedded_base_owner.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_embedded_base_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_embedded_base_owner
  rex_frontend_embedded_base_owner.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_system_header_default_template_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_system_header_default_template_translator>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -std=c++20
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_system_header_instantiation.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_system_header_default_template_roundtrip
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_system_header_default_template_roundtrip
  rex_test2025_issue160_system_header_instantiation.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_system_header_primary_definition
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_system_header_primary_definition_translator>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_system_header_primary_definition.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_system_header_primary_definition
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_system_header_primary_definition
  rex_frontend_system_header_primary_definition.cpp)

set(_rex_alias_elaboration_output
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_alias_elaboration.output.cpp")
add_test(
  NAME Cxx_tests_rex_frontend_alias_elaboration
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "output=\"$1\"; shift; \"$@\" && grep -E 'using[[:space:]]+RexExplicitStructAlias[[:space:]]*=[[:space:]]*struct[[:space:]]+RexAliasStruct' \"$output\" && grep -E 'using[[:space:]]+RexExplicitClassAlias[[:space:]]*=[[:space:]]*class[[:space:]]+RexAliasClass' \"$output\" && grep -E 'using[[:space:]]+RexExplicitEnumAlias[[:space:]]*=[[:space:]]*enum[[:space:]]+RexAliasEnum' \"$output\" && ! grep -E 'using[[:space:]]+RexPlain(Struct|Class|Enum)Alias[[:space:]]*=[[:space:]]*(struct|class|enum)' \"$output\" && grep -E 'RexPlainArgumentBox[[:space:]]*<[[:space:]]*RexAliasTypedefTag[[:space:]]*>' \"$output\" && ! grep -E 'RexPlainArgumentBox[[:space:]]*<[[:space:]]*struct' \"$output\" && grep -E 'RexExplicitArgumentBox[[:space:]]*<[[:space:]]*struct[[:space:]]+RexAliasTypedefTag[[:space:]]*>' \"$output\" && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only \"$output\""
    dummy
    "${_rex_alias_elaboration_output}"
    $<TARGET_FILE:rex_frontend_alias_elaboration_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output "${_rex_alias_elaboration_output}"
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_alias_elaboration.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_alias_elaboration
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_alias_elaboration
  rex_frontend_alias_elaboration.cpp)

set(_rex_predefined_identifier_output
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_predefined_identifier.output.cpp")
add_test(
  NAME Cxx_tests_rex_frontend_predefined_identifier
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "output=\"$1\"; shift; \"$@\" && test \"$(grep -o '__func__' \"$output\" | wc -l)\" -eq 7 && test \"$(grep -o '__FUNCTION__' \"$output\" | wc -l)\" -eq 1 && test \"$(grep -o '__PRETTY_FUNCTION__' \"$output\" | wc -l)\" -eq 1 && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only \"$output\""
    dummy
    "${_rex_predefined_identifier_output}"
    $<TARGET_FILE:rex_frontend_predefined_identifier_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output "${_rex_predefined_identifier_output}"
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_predefined_identifier.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_predefined_identifier
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_predefined_identifier
  rex_frontend_predefined_identifier.cpp)

set(_rex_injected_class_source_syntax_output
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_injected_class_source_syntax.output.cpp")
add_test(
  NAME Cxx_tests_rex_frontend_injected_class_source_syntax
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "output=\"$1\"; shift; \"$@\" && ! grep -E 'Rex(CurrentInstantiation|NestedAlias)[[:space:]]*<[[:space:]]*>[[:space:]]*&' \"$output\" && ! grep -E 'RexCurrentInstantiation[^;{]*::[[:space:]]*RexCurrentInstantiation[^;{]*::[[:space:]]*const_iterator' \"$output\" && ! grep -E 'RexNestedAlias[^;{]*::[[:space:]]*RexNestedAlias[^;{]*::[[:space:]]*const_iterator' \"$output\" && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only \"$output\""
    dummy
    "${_rex_injected_class_source_syntax_output}"
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output "${_rex_injected_class_source_syntax_output}"
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_injected_class_source_syntax.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_injected_class_source_syntax
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_injected_class_source_syntax
  rex_frontend_injected_class_source_syntax.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_class_source_range_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_class_source_range_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    -rose:unparseHeaderFiles
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_class_source_range.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_class_source_range_structure
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_class_source_range_structure
  rex_frontend_class_source_range.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_on_demand_header_forward_source_identity
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_class_source_range_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_class_source_range.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_on_demand_header_forward_source_identity
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_on_demand_header_forward_source_identity
  rex_frontend_class_source_range.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_cross_file_source_range_is_rejected
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=\"$1\"; shift; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_FRONTEND_INVARIANT\\[source-range\\]: node=0x[[:xdigit:]]+ type=Sg[A-Za-z0-9_]+ has cross-file endpoints that cannot be represented by one exact Sage source range$' \"$log\")\" -eq 1"
    dummy
    "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_cross_file_source_range.log"
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_cross_file_source_range.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_cross_file_source_range_is_rejected
  PROPERTIES LABELS "Cxx_tests;rex;CFE;DEATH")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_cross_file_source_range_is_rejected
  rex_frontend_cross_file_source_range.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_namespace_source_fragments_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_namespace_source_fragments_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_namespace_source_fragments.cpp
)

add_test(
  NAME Cxx_tests_rex_frontend_on_demand_namespace_order
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -std=c++14
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_on_demand_namespace_order.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_on_demand_namespace_order
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_on_demand_namespace_order
  rex_frontend_on_demand_namespace_order.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_expanded_token_zero_order_is_rejected
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; REX_TEST_ZERO_EXPANDED_TOKEN_ORDER=1 \"$@\" >rex_frontend_expanded_token_zero_order.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_FRONTEND_INVARIANT[expanded-token-order-state]: exact expanded-token order must be positive' rex_frontend_expanded_token_zero_order.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_namespace_source_fragments_translator>
)
set_tests_properties(
  Cxx_tests_rex_frontend_expanded_token_zero_order_is_rejected
  PROPERTIES LABELS "Cxx_tests;rex;CFE;death")
set_tests_properties(
  Cxx_tests_rex_frontend_namespace_source_fragments_structure
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_namespace_source_fragments_structure
  rex_frontend_namespace_source_fragments.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_support_namespace_semantic_root
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_support_namespace_semantic_root_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -target aarch64-unknown-linux-gnu
    -rose:output
    ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_support_namespace_semantic_root.cpp
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_support_namespace_semantic_root.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_support_namespace_semantic_root
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_support_namespace_semantic_root
  rex_frontend_support_namespace_semantic_root.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_dependent_member_typedef_identity
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_dependent_member_typedef_identity_translator>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -std=c++14
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_dependent_member_typedef_identity.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_dependent_member_typedef_identity
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_dependent_member_typedef_identity
  rex_frontend_dependent_member_typedef_identity.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_namespace_cross_tu_chain
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_namespace_cross_tu_chain_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_namespace_cross_tu_chain_a.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_namespace_cross_tu_chain_b.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_namespace_cross_tu_chain
  PROPERTIES LABELS "Cxx_tests;rex;CFE"
)

set(_rex_source_location_builtin_output
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_source_location_builtin.output.cpp")
add_test(
  NAME Cxx_tests_rex_frontend_source_location_builtin_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "output=\"$1\"; shift; \"$@\" && grep -Fq '__builtin_FILE()' \"$output\" && grep -Fq '__builtin_FILE_NAME()' \"$output\" && grep -Fq '__builtin_FUNCTION()' \"$output\" && grep -Fq '__builtin_LINE()' \"$output\" && grep -Fq '__builtin_COLUMN()' \"$output\" && '${CMAKE_CXX_COMPILER}' -std=gnu++14 -fsyntax-only \"$output\""
    dummy
    "${_rex_source_location_builtin_output}"
    $<TARGET_FILE:rex_frontend_source_location_builtin_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output "${_rex_source_location_builtin_output}"
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_source_location_builtin.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_source_location_builtin_structure
  PROPERTIES LABELS "Cxx_tests;rex;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_source_location_builtin_structure
  rex_frontend_source_location_builtin.cpp)

set(_rex_semantic_expression_wrappers_output
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_semantic_expression_wrappers.output.cpp")
add_test(
  NAME Cxx_tests_rex_frontend_semantic_expression_wrappers
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "output=\"$1\"; shift; \"$@\" && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only \"$output\""
    dummy
    "${_rex_semantic_expression_wrappers_output}"
    $<TARGET_FILE:rex_frontend_semantic_expression_wrappers_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output "${_rex_semantic_expression_wrappers_output}"
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_semantic_expression_wrappers.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_semantic_expression_wrappers
  PROPERTIES LABELS "Cxx_tests;rex;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_semantic_expression_wrappers
  rex_frontend_semantic_expression_wrappers.cpp)

set(_rex_statement_attributes_output
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_statement_attributes.output.cpp")
add_test(
  NAME Cxx_tests_rex_frontend_statement_attributes_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "output=\"$1\"; shift; \"$@\" && grep -Fq '[[assume(' \"$output\" && grep -Fq '[[clang::nomerge]]' \"$output\" && grep -Fq '[[clang::always_inline]]' \"$output\" && grep -Fq '[[likely]]' \"$output\" && grep -Fq '[[unlikely]]' \"$output\" && grep -Fq '[[fallthrough]]' \"$output\" && grep -Fq '__attribute__((fallthrough))' \"$output\" && grep -Fq '[[clang::musttail]]' \"$output\" && awk 'index($0, \"#pragma clang loop vectorize_width\") { ++vectorize_count; vectorize=NR } index($0, \"#pragma clang loop interleave\") { ++interleave_count; interleave=NR } index($0, \"#pragma unroll 4\") { ++unroll_count } index($0, \"for (int index\") && !first_loop { first_loop=NR } END { exit !(vectorize_count == 1 && interleave_count == 1 && unroll_count == 1 && first_loop && vectorize < interleave && interleave < first_loop) }' \"$output\" && '${CMAKE_CXX_COMPILER}' -std=c++23 -Wno-attributes -fsyntax-only \"$output\""
    dummy
    "${_rex_statement_attributes_output}"
    $<TARGET_FILE:rex_frontend_statement_attributes_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output "${_rex_statement_attributes_output}"
    ${ROSE_INCLUDE_FLAGS}
    -std=c++23
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_statement_attributes.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_statement_attributes_roundtrip
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_statement_attributes_roundtrip
  rex_frontend_statement_attributes.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_statement_attributes_reject_unsupported
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; output=\"$1\"; shift; set +e; \"$@\" >\"$output\" 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_FRONTEND_INVARIANT\\[statement-attribute-kind\\]: unsupported explicit Clang statement attribute kind=[0-9]+$' \"$output\")\" -eq 1"
    dummy
    "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_statement_attributes_unsupported.log"
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=c++23
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_statement_attributes_unsupported.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_statement_attributes_reject_unsupported
  PROPERTIES LABELS "Cxx_tests;rex;CFE;DEATH")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_statement_attributes_reject_unsupported
  rex_frontend_statement_attributes_unsupported.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_ms_property_hard_error
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=\"$1\"; expected=\"$2\"; shift 2; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- \"$expected\" \"$log\")\" -eq 1"
    dummy
    rex_frontend_ms_property_hard_error.log
    "REX_FRONTEND_INVARIANT[unsupported-ms-property]: source-written Microsoft property 'rex_ms_property_owner::value' has no lossless typed Sage representation"
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -fms-extensions
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_ms_property_hard_error.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_ms_property_hard_error
  PROPERTIES LABELS "Cxx_tests;rex;CFE;DEATH")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_ms_property_hard_error
  rex_frontend_ms_property_hard_error.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_access_label_lexical_ownership
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && test \"\$(grep -Fc 'public:' rose_rex_frontend_access_label_lexical_ownership.cpp)\" -eq 2 && test \"\$(grep -Fc 'protected:' rose_rex_frontend_access_label_lexical_ownership.cpp)\" -eq 1 && test \"\$(grep -Fc 'private:' rose_rex_frontend_access_label_lexical_ownership.cpp)\" -eq 2 && '${CMAKE_CXX_COMPILER}' -std=c++17 -fsyntax-only rose_rex_frontend_access_label_lexical_ownership.cpp"
    dummy
    $<TARGET_FILE:rex_frontend_access_label_lexical_ownership_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_access_label_lexical_ownership.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_access_label_lexical_ownership
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_access_label_lexical_ownership
  rex_frontend_access_label_lexical_ownership.cpp)

add_test(
  NAME Cxx_tests_rex_unparser_malformed_access_label_is_rejected
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=rex_unparser_malformed_access_label.log; set +e; REX_TEST_MALFORMED_CLASS_ACCESS=1 \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc -- 'REX_UNPARSE_INVARIANT[class-member-access]: class=RexAccessLabelClass member=SgVariableDeclaration name=_variable_declaration_explicit_private effective=public does not match lexical=private' \"$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_access_label_lexical_ownership_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_access_label_lexical_ownership.cpp
)
set_tests_properties(
  Cxx_tests_rex_unparser_malformed_access_label_is_rejected
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER;DEATH")
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_malformed_access_label_is_rejected
  rex_frontend_access_label_lexical_ownership.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_operator_semantic_path_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_operator_semantic_path_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_operator_semantic_path.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_operator_semantic_path_structure
  PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_operator_semantic_path_structure
  rex_frontend_operator_semantic_path.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_operator_surface_contract
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_operator_surface_contract_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -std=c++23
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_operator_surface_contract.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_operator_surface_contract
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_operator_surface_contract
  rex_frontend_operator_surface_contract.cpp)

foreach(_rex_operator_surface_mode
        missing-role semantic-source wrong-surface wrong-callee
        udl-missing-lexical udl-bad-suffix-role)
  if(_rex_operator_surface_mode STREQUAL "missing-role")
    set(_rex_operator_surface_diagnostic
      "^REX_UNPARSE_INVARIANT\\[operator-source-operands\\]: call=0x[[:xdigit:]]+ owns 1 arguments but 0 exact operand roles$")
  elseif(_rex_operator_surface_mode STREQUAL "semantic-source")
    set(_rex_operator_surface_diagnostic
      "^REX_UNPARSE_INVARIANT\\[operator-source-operands\\]: binary operator call=0x[[:xdigit:]]+ requires source=2 semantic=0, found source=1 semantic=1$")
  elseif(_rex_operator_surface_mode STREQUAL "wrong-surface")
    set(_rex_operator_surface_diagnostic
      "^REX_UNPARSE_INVARIANT\\[operator-source-callee\\]: call=0x[[:xdigit:]]+ surface=15 requires declaration=operator/, found=operator-$")
  elseif(_rex_operator_surface_mode STREQUAL "wrong-callee")
    set(_rex_operator_surface_diagnostic
      "^REX_UNPARSE_INVARIANT\\[operator-source-callee\\]: nonmember surface call=0x[[:xdigit:]]+ owns a member callee$")
  elseif(_rex_operator_surface_mode STREQUAL "udl-missing-lexical")
    set(_rex_operator_surface_diagnostic
      "^REX_UNPARSE_INVARIANT\\[user-defined-literal-call\\]: literal surface has malformed lexical/semantic operands$")
  elseif(_rex_operator_surface_mode STREQUAL "udl-bad-suffix-role")
    set(_rex_operator_surface_diagnostic
      "^REX_UNPARSE_INVARIANT\\[user-defined-literal-call\\]: lexical operand has an invalid suffix role$")
  else()
    message(FATAL_ERROR
      "unmapped malformed operator-surface mode: ${_rex_operator_surface_mode}")
  endif()
  add_test(
    NAME Cxx_tests_rex_unparser_rejects_malformed_operator_surface_${_rex_operator_surface_mode}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=rex_operator_surface_${_rex_operator_surface_mode}.log; set +e; REX_TEST_MALFORMED_OPERATOR_SURFACE=${_rex_operator_surface_mode} \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '${_rex_operator_surface_diagnostic}' \"$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_frontend_operator_surface_contract_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep
      ${ROSE_INCLUDE_FLAGS}
      -std=c++23
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_operator_surface_contract.cpp
  )
  set_tests_properties(
    Cxx_tests_rex_unparser_rejects_malformed_operator_surface_${_rex_operator_surface_mode}
    PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER;death")
  rex_lock_test_for_input(
    Cxx_tests_rex_unparser_rejects_malformed_operator_surface_${_rex_operator_surface_mode}
    rex_frontend_operator_surface_contract.cpp)
endforeach()
unset(_rex_operator_surface_mode)
unset(_rex_operator_surface_diagnostic)

add_test(
  NAME Cxx_tests_rex_unparser_source_qualification_provenance_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_unparser_source_qualification_provenance_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_source_qualification_provenance.cpp
)
set_tests_properties(
  Cxx_tests_rex_unparser_source_qualification_provenance_structure
  PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_source_qualification_provenance_structure
  rex_unparser_source_qualification_provenance.cpp)

add_test(
  NAME Cxx_tests_rex_cxx_dependent_qualified_template_type_contract
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_cxx_dependent_qualified_template_type_translator>
    ${ROSE_FLAGS}
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_cxx_dependent_qualified_template_type.cpp
)
set_tests_properties(
  Cxx_tests_rex_cxx_dependent_qualified_template_type_contract
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_cxx_dependent_qualified_template_type_contract
  rex_cxx_dependent_qualified_template_type.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_dependent_template_argument_source_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_dependent_template_argument_source_owner_translator>
    ${ROSE_FLAGS}
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_dependent_template_argument_source_owner.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_dependent_template_argument_source_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_dependent_template_argument_source_owner
  rex_frontend_dependent_template_argument_source_owner.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_function_template_default_parameter_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_function_template_default_parameter_owner_translator>
    ${ROSE_FLAGS}
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_function_template_default_parameter_owner.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_function_template_default_parameter_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_function_template_default_parameter_owner
  rex_frontend_function_template_default_parameter_owner.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_semantic_alias_template_parameter_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_semantic_alias_template_parameter_owner_translator>
    ${ROSE_FLAGS}
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -isystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../input_codes/system_header_like
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_semantic_alias_template_parameter_owner.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_semantic_alias_template_parameter_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_semantic_alias_template_parameter_owner
  rex_frontend_semantic_alias_template_parameter_owner.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_anonymous_aggregate_source_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_anonymous_aggregate_source_owner_translator>
    ${ROSE_FLAGS}
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_anonymous_aggregate_source_owner.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_anonymous_aggregate_source_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_anonymous_aggregate_source_owner
  rex_frontend_anonymous_aggregate_source_owner.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_qualified_template_local_argument_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_qualified_template_local_argument_owner_translator>
    ${ROSE_FLAGS}
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_qualified_template_local_argument_owner.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_qualified_template_local_argument_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_qualified_template_local_argument_owner
  rex_frontend_qualified_template_local_argument_owner.cpp)

set(_rex_explicit_member_specialization_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_explicit_member_specialization_header_owner.cpp)
add_test(
  NAME Cxx_tests_rex_frontend_explicit_member_specialization_header_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && test \"$(grep -Fo 'template<>' '${_rex_explicit_member_specialization_output}' | wc -l)\" -eq 1 && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only '${_rex_explicit_member_specialization_output}'"
    dummy
    $<TARGET_FILE:rex_frontend_explicit_member_specialization_header_owner_translator>
    ${ROSE_FLAGS}
    ${ROSE_INCLUDE_FLAGS}
    -rose:skipfinalCompileStep
    -rose:output ${_rex_explicit_member_specialization_output}
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_explicit_member_specialization_header_owner.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_explicit_member_specialization_header_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_explicit_member_specialization_header_owner
  rex_frontend_explicit_member_specialization_header_owner.cpp)

set(_rex_dependent_typename_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_dependent_typename_use_owner.cpp)
add_test(
  NAME Cxx_tests_rex_frontend_dependent_typename_use_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && test \"$(grep -Fo 'typename RexDependentTypenameOwner<T>::type' '${_rex_dependent_typename_output}' | wc -l)\" -eq 8 && test \"$(grep -Fo 'struct ::RexQualifiedElaboratedTag' '${_rex_dependent_typename_output}' | wc -l)\" -eq 1 && test \"$(grep -Fo 'class RexElaboratedTemplateTag<int>' '${_rex_dependent_typename_output}' | wc -l)\" -eq 1 && test \"$(grep -Fo 'sizeof(RexTypeOperandElaboration)' '${_rex_dependent_typename_output}' | wc -l)\" -eq 1 && test \"$(grep -Fo 'sizeof(struct RexTypeOperandElaboration)' '${_rex_dependent_typename_output}' | wc -l)\" -eq 1 && test \"$(grep -Fo '__alignof__(RexTypeOperandElaboration)' '${_rex_dependent_typename_output}' | wc -l)\" -eq 1 && test \"$(grep -Fo '__alignof__(struct RexTypeOperandElaboration)' '${_rex_dependent_typename_output}' | wc -l)\" -eq 1 && test \"$(grep -Fo 'struct RexTypeOperandElaboration' '${_rex_dependent_typename_output}' | wc -l)\" -eq 4 && test \"$(grep -Fo 'sizeof(Nested)' '${_rex_dependent_typename_output}' | wc -l)\" -eq 1 && test \"$(grep -Fo 'sizeof(RexDependentSizeofOwner<T>::Nested)' '${_rex_dependent_typename_output}' | wc -l)\" -eq 1 && test \"$(grep -Eo 'sizeof\\(struct ([^)]*::)?Nested\\)' '${_rex_dependent_typename_output}' | wc -l)\" -eq 0 && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only '${_rex_dependent_typename_output}'"
    dummy
    $<TARGET_FILE:rex_frontend_dependent_typename_use_owner_translator>
    ${ROSE_FLAGS}
    ${ROSE_INCLUDE_FLAGS}
    -rose:skipfinalCompileStep
    -rose:output ${_rex_dependent_typename_output}
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_dependent_typename_use_owner.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_dependent_typename_use_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_dependent_typename_use_owner
  rex_frontend_dependent_typename_use_owner.cpp)

set(_rex_semantic_auxiliary_frontier_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_semantic_auxiliary_token_frontier.cpp)
add_test(
  NAME Cxx_tests_rex_frontend_semantic_auxiliary_token_frontier
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only '${_rex_semantic_auxiliary_frontier_output}'"
    dummy
    $<TARGET_FILE:rex_frontend_semantic_auxiliary_token_frontier_translator>
    ${ROSE_FLAGS}
    ${ROSE_INCLUDE_FLAGS}
    -rose:unparse_tokens
    -rose:skipfinalCompileStep
    -rose:output ${_rex_semantic_auxiliary_frontier_output}
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_semantic_auxiliary_token_frontier.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_semantic_auxiliary_token_frontier
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_semantic_auxiliary_token_frontier
  rex_frontend_semantic_auxiliary_token_frontier.cpp)

add_test(
  NAME Cxx_tests_rex_name_qualification_root_contract
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && grep -E 'Derived[[:space:]]*\\([[:space:]]*\\)[[:space:]]*:[[:space:]]*Base[[:space:]]*::[[:space:]]*Nested[[:space:]]*\\(' rose_rex_name_qualification_root_contract.cpp && grep -E 'rex_nq_enum_outer[[:space:]]*::[[:space:]]*a[[:space:]]*::[[:space:]]*Type[[:space:]]+rex_nq_enum_value' rose_rex_name_qualification_root_contract.cpp && grep -F 'RexNqTemplate<rex_nq_template_argument::Payload>' rose_rex_name_qualification_root_contract.cpp && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only rose_rex_name_qualification_root_contract.cpp"
    dummy
    $<TARGET_FILE:rex_name_qualification_root_contract_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_name_qualification_root_contract.cpp
)
set_tests_properties(
  Cxx_tests_rex_name_qualification_root_contract
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_name_qualification_root_contract
  rex_name_qualification_root_contract.cpp)

add_test(
  NAME Cxx_tests_rex_name_qualification_rejects_missing_member_context
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_name_qualification_missing_member_context.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[member-function-reference-context\\]: reference=0x[[:xdigit:]]+ declaration=0x[[:xdigit:]]+ has no enclosing or explicitly inherited emission statement$' rex_name_qualification_missing_member_context.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_name_qualification_root_contract_translator>
    --reject-missing-member-context
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_name_qualification_member_context_contract.cpp
)
set_tests_properties(
  Cxx_tests_rex_name_qualification_rejects_missing_member_context
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER;death")
rex_lock_test_for_input(
  Cxx_tests_rex_name_qualification_rejects_missing_member_context
  rex_name_qualification_member_context_contract.cpp)

add_test(
  NAME Cxx_tests_rex_unparser_visibility_contract
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && test \"\$(grep -Fc '__attribute__((visibility(\"hidden\")))' rose_rex_unparser_visibility_contract.cpp)\" -eq 4 && test \"\$(grep -Fc '__attribute__((visibility(\"protected\")))' rose_rex_unparser_visibility_contract.cpp)\" -eq 1 && test \"\$(grep -Fc '__attribute__((visibility(\"default\")))' rose_rex_unparser_visibility_contract.cpp)\" -eq 2 && test \"\$(grep -Fc '__attribute__((visibility(\"internal\")))' rose_rex_unparser_visibility_contract.cpp)\" -eq 2 && test \"\$(grep -Fc '__attribute__((type_visibility(\"hidden\")))' rose_rex_unparser_visibility_contract.cpp)\" -eq 1 && test \"\$(grep -Fc '__attribute__((type_visibility(\"internal\")))' rose_rex_unparser_visibility_contract.cpp)\" -eq 1 && test \"\$(grep -Fc '__attribute__((type_visibility(\"protected\")))' rose_rex_unparser_visibility_contract.cpp)\" -eq 1 && grep -Fq 'rex_visibility_hidden' rose_rex_unparser_visibility_contract.cpp && grep -Fq 'rex_visibility_protected' rose_rex_unparser_visibility_contract.cpp && grep -Fq 'rex_visibility_default' rose_rex_unparser_visibility_contract.cpp && grep -Fq 'rex_visibility_internal' rose_rex_unparser_visibility_contract.cpp && grep -Fq 'rex_visibility_hidden_variable' rose_rex_unparser_visibility_contract.cpp && grep -Fq 'rex_visibility_macro_hidden' rose_rex_unparser_visibility_contract.cpp && grep -Fq 'rex_visibility_macro_default' rose_rex_unparser_visibility_contract.cpp && grep -Fq 'rex_visibility_macro_internal' rose_rex_unparser_visibility_contract.cpp && grep -Fq 'RexVisibilityHiddenType' rose_rex_unparser_visibility_contract.cpp && grep -Fq 'RexTypeVisibilityHidden' rose_rex_unparser_visibility_contract.cpp && grep -Fq 'RexTypeVisibilityInternal' rose_rex_unparser_visibility_contract.cpp && grep -Fq 'RexTypeVisibilityMacroProtected' rose_rex_unparser_visibility_contract.cpp && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only rose_rex_unparser_visibility_contract.cpp"
    dummy
    $<TARGET_FILE:rex_unparser_visibility_contract_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rex:ast-json-checkpoint=pre-omp-construction
    -rex:ast-json-dir=rex_unparser_visibility_contract_json
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_visibility_contract.cpp
)
set_tests_properties(
  Cxx_tests_rex_unparser_visibility_contract
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_visibility_contract
  rex_unparser_visibility_contract.cpp)

add_test(
  NAME Cxx_tests_rex_unparser_visibility_macro_conflict_is_rejected
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_unparser_visibility_macro_conflict.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_FRONTEND_INVARIANT[gnu-visibility-source]: visibility attribute has conflicting exact expanded or spelled visibility token evidence' rex_unparser_visibility_macro_conflict.log)\" -eq 1"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_visibility_macro_conflict.cpp
)
set_tests_properties(
  Cxx_tests_rex_unparser_visibility_macro_conflict_is_rejected
  PROPERTIES LABELS "Cxx_tests;rex;CFE;DEATH")
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_visibility_macro_conflict_is_rejected
  rex_unparser_visibility_macro_conflict.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_declaration_scope_contract_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_declaration_scope_contract_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_declaration_scope_contract.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_declaration_scope_contract_structure
  PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_declaration_scope_contract_structure
  rex_frontend_declaration_scope_contract.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_friend_template_canonical_chain_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_friend_template_canonical_chain_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_friend_template_canonical_chain.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_friend_template_canonical_chain_structure
  PROPERTIES LABELS "Cxx_tests;rex;CFE;frontend;contracts")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_friend_template_canonical_chain_structure
  rex_frontend_friend_template_canonical_chain.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_implicit_control_flow_scope_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_implicit_control_flow_scope_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_implicit_control_flow_scope.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_implicit_control_flow_scope_structure
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_implicit_control_flow_scope_structure
  rex_frontend_implicit_control_flow_scope.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_preprocessor_scope_gap_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_preprocessor_scope_gap_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=c++14
    -DREX_FRONTEND_CONDITIONAL_ARGUMENT_ENABLED=1
    -DREX_FRONTEND_CONDITIONAL_CTOR_ENABLED=1
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_preprocessor_scope_gap.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_preprocessor_scope_gap_structure
  PROPERTIES LABELS "Cxx_tests;rex;CFE;frontend;contracts")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_preprocessor_scope_gap_structure
  rex_frontend_preprocessor_scope_gap.cpp)

set(_rex_preprocessor_conditional_separator_output
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_preprocessor_conditional_separator.output.cpp")
add_test(
  NAME Cxx_tests_rex_frontend_preprocessor_conditional_separator_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "output=\"$1\"; shift; rm -f \"$output\"; \"$@\" && test -s \"$output\" && test \"$(grep -Fc '#if REX_FRONTEND_CONDITIONAL_ARGUMENT_ENABLED' \"$output\")\" -eq 1 && test \"$(grep -Fc '#if REX_FRONTEND_CONDITIONAL_CTOR_ENABLED' \"$output\")\" -eq 1 && '${CMAKE_CXX_COMPILER}' -std=gnu++14 -DREX_FRONTEND_CONDITIONAL_ARGUMENT_ENABLED=1 -DREX_FRONTEND_CONDITIONAL_CTOR_ENABLED=1 -fsyntax-only \"$output\" && '${CMAKE_CXX_COMPILER}' -std=gnu++14 -DREX_FRONTEND_CONDITIONAL_ARGUMENT_ENABLED=0 -DREX_FRONTEND_CONDITIONAL_CTOR_ENABLED=0 -fsyntax-only \"$output\""
    dummy
    "${_rex_preprocessor_conditional_separator_output}"
    $<TARGET_FILE:testTranslator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output "${_rex_preprocessor_conditional_separator_output}"
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++14
    -DREX_FRONTEND_CONDITIONAL_ARGUMENT_ENABLED=1
    -DREX_FRONTEND_CONDITIONAL_CTOR_ENABLED=1
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_preprocessor_scope_gap.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_preprocessor_conditional_separator_roundtrip
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER;contracts")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_preprocessor_conditional_separator_roundtrip
  rex_frontend_preprocessor_scope_gap.cpp)

add_test(
  NAME Cxx_tests_rex_malformed_implicit_control_flow_scope_is_rejected
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=rex_malformed_implicit_control_flow_scope.log; rm -f \"\$log\"; set +e; REX_TEST_MALFORMED_IMPLICIT_CONTROL_FLOW_SCOPE=1 \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[implicit-control-flow-scope\\]: block=0x[[:xdigit:]]+ has malformed typed ownership or synthesized provenance$' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_implicit_control_flow_scope_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_implicit_control_flow_scope.cpp
)
set_tests_properties(
  Cxx_tests_rex_malformed_implicit_control_flow_scope_is_rejected
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER;DEATH")
rex_lock_test_for_input(
  Cxx_tests_rex_malformed_implicit_control_flow_scope_is_rejected
  rex_frontend_implicit_control_flow_scope.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_new_initializer_source_form_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_new_initializer_source_form_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -std=gnu++14
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_new_initializer_source_form.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_new_initializer_source_form_structure
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_new_initializer_source_form_structure
  rex_frontend_new_initializer_source_form.cpp)

add_test(
  NAME Cxx_tests_rex_malformed_new_constructor_source_form_is_rejected
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=rex_malformed_new_constructor_source_form.log; rm -f \"\$log\"; set +e; REX_TEST_MALFORMED_NEW_CONSTRUCTOR_SOURCE_FORM=1 \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[new-constructor-source-form\\]:' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_new_initializer_source_form_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -std=gnu++14
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_new_initializer_source_form.cpp
)
set_tests_properties(
  Cxx_tests_rex_malformed_new_constructor_source_form_is_rejected
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER;DEATH")
rex_lock_test_for_input(
  Cxx_tests_rex_malformed_new_constructor_source_form_is_rejected
  rex_frontend_new_initializer_source_form.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_gnu_binary_conditional_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "output=rose_rex_frontend_gnu_binary_conditional.cpp; \"\$@\" && test \"\$(grep -Ec 'rex_gnu_binary_source[[:space:]]*[(][[:space:]]*[)][[:space:]]*[?][[:space:]]*:' \"\$output\")\" -eq 1 && '${CMAKE_CXX_COMPILER}' -std=gnu++14 -fsyntax-only \"\$output\""
    dummy
    $<TARGET_FILE:rex_frontend_gnu_binary_conditional_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -std=gnu++14
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_gnu_binary_conditional.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_gnu_binary_conditional_structure
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_gnu_binary_conditional_structure
  rex_frontend_gnu_binary_conditional.cpp)

add_test(
  NAME Cxx_tests_rex_malformed_gnu_binary_conditional_is_rejected
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=rex_malformed_gnu_binary_conditional.log; rm -f \"\$log\"; set +e; REX_TEST_MALFORMED_GNU_BINARY_CONDITIONAL=1 \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[conditional-expression\\]: GNU binary conditional=0x[[:xdigit:]]+ requires one common operand, no duplicated true operand, one false operand, and one exact result type$' \"\$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_gnu_binary_conditional_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -std=gnu++14
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_gnu_binary_conditional.cpp
)
set_tests_properties(
  Cxx_tests_rex_malformed_gnu_binary_conditional_is_rejected
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER;DEATH")
rex_lock_test_for_input(
  Cxx_tests_rex_malformed_gnu_binary_conditional_is_rejected
  rex_frontend_gnu_binary_conditional.cpp)

set(_rex_field_tag_introduction_output
  "${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_field_tag_introduction.cpp")
add_test(
  NAME Cxx_tests_rex_frontend_field_tag_introduction_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "output=\"$1\"; shift; \"$@\" && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only \"$output\""
    dummy
    "${_rex_field_tag_introduction_output}"
    $<TARGET_FILE:rex_frontend_field_tag_introduction_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output "${_rex_field_tag_introduction_output}"
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_field_tag_introduction.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_field_tag_introduction_structure
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_field_tag_introduction_structure
  rex_frontend_field_tag_introduction.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_linkage_container_contract_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_linkage_container_contract_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    -rose:unparseHeaderFiles
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_linkage_container_contract.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_linkage_container_contract_structure
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_linkage_container_contract_structure
  rex_frontend_linkage_container_contract.cpp)

set(_rex_variable_declarator_ast_output
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_variable_declarator_identity.ast.cpp")
set(_rex_variable_declarator_token_output
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_variable_declarator_identity.token.cpp")

add_test(
  NAME Cxx_tests_rex_frontend_variable_declarator_identity_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_variable_declarator_identity_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output ${_rex_variable_declarator_ast_output}
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_variable_declarator_identity.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_variable_declarator_identity_structure
  PROPERTIES
    FIXTURES_SETUP rex_variable_declarator_identity_ast_output
    LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_variable_declarator_identity_structure
  rex_frontend_variable_declarator_identity.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_variable_declarator_identity_token_mapping
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_variable_declarator_identity_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    -rose:output ${_rex_variable_declarator_token_output}
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_variable_declarator_identity.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_variable_declarator_identity_token_mapping
  PROPERTIES
    FIXTURES_SETUP rex_variable_declarator_identity_token_output
    LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_variable_declarator_identity_token_mapping
  rex_frontend_variable_declarator_identity.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_variable_declarator_identity_ast_surface
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/check_rex_declaration_group_surface.sh
    ast ${_rex_variable_declarator_ast_output} ${CMAKE_CXX_COMPILER}
)
set_tests_properties(
  Cxx_tests_rex_frontend_variable_declarator_identity_ast_surface
  PROPERTIES
    FIXTURES_REQUIRED rex_variable_declarator_identity_ast_output
    LABELS "Cxx_tests;rex;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_variable_declarator_identity_ast_surface
  rex_frontend_variable_declarator_identity.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_variable_declarator_identity_token_surface
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/check_rex_declaration_group_surface.sh
    token ${_rex_variable_declarator_token_output} ${CMAKE_CXX_COMPILER}
)
set_tests_properties(
  Cxx_tests_rex_frontend_variable_declarator_identity_token_surface
  PROPERTIES
    FIXTURES_REQUIRED rex_variable_declarator_identity_token_output
    LABELS "Cxx_tests;rex;UNPARSER;TOKEN")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_variable_declarator_identity_token_surface
  rex_frontend_variable_declarator_identity.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_condition_declaration_token_mapping_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_variable_declarator_identity_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_condition_declaration_token_mapping.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_condition_declaration_token_mapping_structure
  PROPERTIES LABELS "Cxx_tests;rex")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_condition_declaration_token_mapping_structure
  rex_frontend_condition_declaration_token_mapping.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_condition_declaration_token_mapping_tokens
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_variable_declarator_identity_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_condition_declaration_token_mapping.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_condition_declaration_token_mapping_tokens
  PROPERTIES LABELS "Cxx_tests;rex")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_condition_declaration_token_mapping_tokens
  rex_frontend_condition_declaration_token_mapping.cpp)

foreach(corruption
    erase-direct
    drop-group-member
    direct-group-member
    borrow-group-member
    truncate-group-semicolon
    macro-as-file-terminator
    file-as-macro-terminator
    borrow-independent)
  string(REPLACE "-" "_" corruption_name ${corruption})
  if(corruption STREQUAL "erase-direct")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[declaration-mapping\\]: declaration=0x[[:xdigit:]]+/Sg[^[:space:]]+ has no direct token mapping in file=.*rex_frontend_variable_declarator_identity\\.cpp$")
  elseif(corruption STREQUAL "drop-group-member")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[declaration-mapping\\]: declaration=0x[[:xdigit:]]+/Sg[^[:space:]]+ occurs 0 times in its direct token mapping node vector$")
  elseif(corruption STREQUAL "direct-group-member")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[declaration-group\\]: member=0x[[:xdigit:]]+/Sg[^[:space:]]+ group=0x[[:xdigit:]]+ has a forbidden direct token mapping$")
  elseif(corruption STREQUAL "borrow-group-member")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[declaration-group\\]: mapping=0x[[:xdigit:]]+ owner=0x[[:xdigit:]]+/Sg[^[:space:]]+ associates member=0x[[:xdigit:]]+/Sg[^[:space:]]+ structurally owned by group=0x[[:xdigit:]]+$")
  elseif(corruption STREQUAL "truncate-group-semicolon")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[declaration-group\\]: group=0x[[:xdigit:]]+ source=[0-9]+:[0-9]+-[0-9]+:[0-9]+ does not exactly match mapped tokens=[0-9]+:[0-9]+-[0-9]+:[0-9]+$")
  elseif(corruption STREQUAL "macro-as-file-terminator")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[declaration-group\\]: group=0x[[:xdigit:]]+ token mapping interval=\\[[0-9]+,[0-9]+\\] does not end at its required trailing semicolon$")
  elseif(corruption STREQUAL "file-as-macro-terminator")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[declaration-group\\]: macro-terminated group=0x[[:xdigit:]]+ incorrectly owns a file semicolon token=[0-9]+$")
  else()
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[declaration-mapping\\]: declaration=0x[[:xdigit:]]+/Sg[^[:space:]]+ token mapping borrows distinct declaration=0x[[:xdigit:]]+/Sg[^[:space:]]+$")
  endif()
  add_test(
    NAME Cxx_tests_rex_frontend_variable_declarator_identity_rejects_${corruption_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND
      sh -c
      "ulimit -c 0; set +e; REX_TEST_TOKEN_MAPPING_CORRUPTION='${corruption}' \"$@\" >rex_frontend_variable_declarator_identity_${corruption_name}.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec -- '${corruption_diagnostic}' rex_frontend_variable_declarator_identity_${corruption_name}.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_frontend_variable_declarator_identity_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
      ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_variable_declarator_identity.cpp
  )
  set_tests_properties(
    Cxx_tests_rex_frontend_variable_declarator_identity_rejects_${corruption_name}
    PROPERTIES LABELS "Cxx_tests;rex;death")
  rex_lock_test_for_input(
    Cxx_tests_rex_frontend_variable_declarator_identity_rejects_${corruption_name}
    rex_frontend_variable_declarator_identity.cpp)
endforeach()

add_test(
  NAME Cxx_tests_rex_frontend_nonreal_auxiliary_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_nonreal_auxiliary_owner_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++20
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_nonreal_auxiliary_owner.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_nonreal_auxiliary_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_nonreal_auxiliary_owner
  rex_frontend_nonreal_auxiliary_owner.cpp)

set(_rex_mixed_declarator_source
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_mixed_declarator_group.cpp)
set(_rex_mixed_declarator_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_mixed_declarator_group.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_mixed_declarator_group_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_mixed_declarator_group_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_mixed_declarator_source}
)
set_tests_properties(
  Cxx_tests_rex_frontend_mixed_declarator_group_structure
  PROPERTIES
    FIXTURES_SETUP rex_mixed_declarator_group_output
    LABELS "Cxx_tests;rex")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_mixed_declarator_group_structure
  rex_frontend_mixed_declarator_group.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_mixed_declarator_group_compile
  COMMAND ${CMAKE_CXX_COMPILER} -std=gnu++17 -fsyntax-only
    -I${CMAKE_CURRENT_SOURCE_DIR}
    ${_rex_mixed_declarator_output}
)
set_tests_properties(
  Cxx_tests_rex_frontend_mixed_declarator_group_compile
  PROPERTIES
    FIXTURES_REQUIRED rex_mixed_declarator_group_output
    LABELS "Cxx_tests;rex")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_mixed_declarator_group_compile
  rex_frontend_mixed_declarator_group.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_mixed_declarator_group_single_surface
  COMMAND sh -c
    "set -eu; tr '\\n' ' ' < \"$1\" | grep -Eq 'rex_mixed_global_object[[:space:]]*,[[:space:]]*rex_mixed_global_function[[:space:]]*\\('; tr '\\n' ' ' < \"$1\" | grep -Eq 'rex_mixed_function_a[[:space:]]*\\([^)]*\\)[[:space:]]*,[[:space:]]*rex_mixed_function_b[[:space:]]*\\('; tr '\\n' ' ' < \"$1\" | grep -Eq 'rex_mixed_for_object[[:space:]]*,[[:space:]]*rex_mixed_for_function[[:space:]]*\\('; tr '\\n' ' ' < \"$1\" | grep -Eq 'rex_mixed_typedef_scalar[[:space:]]*,[[:space:]]*rex_mixed_typedef_function[[:space:]]*\\('; tr '\\n' ' ' < \"$1\" | grep -Eq 'rex_mixed_pointer_field[[:space:]]*,[[:space:]]*\\*[[:space:]]*rex_mixed_pointer_method[[:space:]]*\\('; for name in rex_mixed_global_object rex_mixed_global_function rex_mixed_global_function_first rex_mixed_global_object_second rex_mixed_function_a rex_mixed_function_b rex_mixed_typedef_scalar rex_mixed_typedef_function rex_mixed_field rex_mixed_method rex_mixed_pointer_field rex_mixed_pointer_method rex_mixed_local rex_mixed_block_function rex_mixed_local_extern_object rex_mixed_local_extern_function rex_mixed_for_object rex_mixed_for_function; do count=$(grep -ow \"$name\" \"$1\" | wc -l); test \"$count\" -eq 1; done"
    dummy ${_rex_mixed_declarator_output}
)
set_tests_properties(
  Cxx_tests_rex_frontend_mixed_declarator_group_single_surface
  PROPERTIES
    FIXTURES_REQUIRED rex_mixed_declarator_group_output
    LABELS "Cxx_tests;rex")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_mixed_declarator_group_single_surface
  rex_frontend_mixed_declarator_group.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_mixed_declarator_group_token_mapping
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_mixed_declarator_group_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_mixed_declarator_source}
)
set_tests_properties(
  Cxx_tests_rex_frontend_mixed_declarator_group_token_mapping
  PROPERTIES LABELS "Cxx_tests;rex")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_mixed_declarator_group_token_mapping
  rex_frontend_mixed_declarator_group.cpp)

foreach(corruption base-type storage-modifier)
  string(REPLACE "-" "_" corruption_name ${corruption})
  add_test(
    NAME Cxx_tests_rex_frontend_mixed_declarator_group_rejects_${corruption_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND
      sh -c
      "ulimit -c 0; set +e; REX_TEST_SOURCE_GROUP_CORRUPTION='${corruption}' \"$@\" >rex_frontend_mixed_declarator_group_${corruption_name}.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[source-declaration-group\\]: group=[^[:space:]]+ member index=[0-9]+ has a mismatched common declaration specifier/base-type/modifier contract$' rex_frontend_mixed_declarator_group_${corruption_name}.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_frontend_mixed_declarator_group_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep
      ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${_rex_mixed_declarator_source}
  )
  set_tests_properties(
    Cxx_tests_rex_frontend_mixed_declarator_group_rejects_${corruption_name}
    PROPERTIES LABELS "Cxx_tests;rex;death")
  rex_lock_test_for_input(
    Cxx_tests_rex_frontend_mixed_declarator_group_rejects_${corruption_name}
    rex_frontend_mixed_declarator_group.cpp)
endforeach()

add_test(
  NAME Cxx_tests_rex_frontend_mixed_declarator_group_rejects_member_scope
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    sh -c
    "ulimit -c 0; set +e; REX_TEST_SOURCE_GROUP_CORRUPTION='member-scope' \"$@\" >rex_frontend_mixed_declarator_group_member_scope.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[declaration-group-shape\\]: group=0x[[:xdigit:]]+ members=2 parent=0x[[:xdigit:]]+ scope=0x[[:xdigit:]]+ structural-scope=0x[[:xdigit:]]+ terminator=0 defining=\\(nil\\) first=\\(nil\\)$' rex_frontend_mixed_declarator_group_member_scope.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_mixed_declarator_group_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_mixed_declarator_source}
)
set_tests_properties(
  Cxx_tests_rex_frontend_mixed_declarator_group_rejects_member_scope
  PROPERTIES LABELS "Cxx_tests;rex;death")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_mixed_declarator_group_rejects_member_scope
  rex_frontend_mixed_declarator_group.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_mixed_declarator_group_rejects_implicit_scope_update
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    sh -c
    "ulimit -c 0; set +e; REX_TEST_SOURCE_GROUP_CORRUPTION='implicit-scope-update' \"$@\" >rex_frontend_mixed_declarator_group_implicit_scope_update.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[statement-scope-update\\]: statement=0x[[:xdigit:]]+ type=SgVariableDeclaration cannot change its derived structural scope from 0x[[:xdigit:]]+ to 0x[[:xdigit:]]+$' rex_frontend_mixed_declarator_group_implicit_scope_update.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_frontend_mixed_declarator_group_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_mixed_declarator_source}
)
set_tests_properties(
  Cxx_tests_rex_frontend_mixed_declarator_group_rejects_implicit_scope_update
  PROPERTIES LABELS "Cxx_tests;rex;death")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_mixed_declarator_group_rejects_implicit_scope_update
  rex_frontend_mixed_declarator_group.cpp)

add_test(
  NAME Cxx_tests_rex_unparser_if_body_parenting_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_test2026_issue482_if_body_parenting_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_if_body_parenting.cpp
)
set_tests_properties(
  Cxx_tests_rex_unparser_if_body_parenting_structure
  PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_if_body_parenting_structure
  rex_unparser_if_body_parenting.cpp)

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

set(_cxx_unparse_output_dir ${CMAKE_CURRENT_BINARY_DIR}/rose_unparse_output)
file(MAKE_DIRECTORY ${_cxx_unparse_output_dir})

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
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue163_axpy_roundtrip.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue163_axpy_roundtrip_unparse PROPERTIES LABELS Cxx_tests )
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue163_axpy_roundtrip_unparse
  rex_test2025_issue163_axpy_roundtrip.cpp)

# Issue 164 has two distinct contracts: AST unparsing must preserve the
# explicit/implicit const semantics, while validated token replay must preserve
# the complete source byte-for-byte.
string(CONCAT rex_test2025_issue164_constexpr_const_unparse_command
  "\"$@\" && "
  "grep -E '^constexpr[[:space:]]+int[[:space:]]+implicit_scalar[[:space:]]*=' rose_rex_test2025_issue164_constexpr_const.cpp && "
  "grep -E '^constexpr[[:space:]]+const[[:space:]]+int[[:space:]]+explicit_scalar[[:space:]]*=' rose_rex_test2025_issue164_constexpr_const.cpp && "
  "grep -E '^constexpr[[:space:]]+int[[:space:]]*\\*[[:space:]]*implicit_ptr[[:space:]]*=' rose_rex_test2025_issue164_constexpr_const.cpp && "
  "grep -E '^constexpr[[:space:]]+int[[:space:]]*\\*[[:space:]]*const[[:space:]]+explicit_ptr[[:space:]]*=' rose_rex_test2025_issue164_constexpr_const.cpp && "
  "grep -E '^constexpr[[:space:]]+const[[:space:]]+int[[:space:]]*\\*[[:space:]]*implicit_pointee[[:space:]]*=' rose_rex_test2025_issue164_constexpr_const.cpp && "
  "grep -E '^constexpr[[:space:]]+const[[:space:]]+int[[:space:]]*\\*[[:space:]]*const[[:space:]]+explicit_pointee_ptr[[:space:]]*=' rose_rex_test2025_issue164_constexpr_const.cpp && "
  "grep -E '^constexpr[[:space:]]+ConstInt[[:space:]]+implicit_typedef[[:space:]]*=' rose_rex_test2025_issue164_constexpr_const.cpp && "
  "grep -E '^constexpr[[:space:]]+const[[:space:]]+ConstInt[[:space:]]+explicit_typedef[[:space:]]*=' rose_rex_test2025_issue164_constexpr_const.cpp && "
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

string(CONCAT rex_test2025_issue164_constexpr_const_token_roundtrip_command
  "\"$@\" && "
  "cmp -s ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue164_constexpr_const.cpp "
  "rose_rex_test2025_issue164_constexpr_const.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c "
  "rose_rex_test2025_issue164_constexpr_const.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue164_constexpr_const_token_roundtrip
  COMMAND
    sh -c
    "${rex_test2025_issue164_constexpr_const_token_roundtrip_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue164_constexpr_const.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue164_constexpr_const_token_roundtrip
  PROPERTIES LABELS Cxx_tests)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_issue164_constexpr_const_token_roundtrip
  rex_test2025_issue164_constexpr_const.cpp)

# Issue 160: Preserve explicit qualifiers, casts, and list-init for std headers.
string(CONCAT rex_test2025_issue160_axpy_system_headers_unparse_command
  "\"$@\" && "
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

# Helper for Cxx unparse tests that run translator, then shell checks on rose_ output.
function(rex_add_cxx_unparse_check_test base_name shell_check_command)
  string(CONCAT _unparse_command "\"$@\" && " "${shell_check_command}")
  add_test(
    NAME Cxx_tests_${base_name}_unparse
    COMMAND
      sh -c
      "${_unparse_command}"
      dummy
      $<TARGET_FILE:${translator}>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${base_name}.cpp
  )
  set_tests_properties(Cxx_tests_${base_name}_unparse PROPERTIES
    LABELS Cxx_tests
    WORKING_DIRECTORY ${_cxx_unparse_output_dir}
  )
  rex_lock_test_for_input(Cxx_tests_${base_name}_unparse ${base_name}.cpp)
endfunction()

rex_add_cxx_unparse_check_test(
  "rex_unparser_implicit_copy_and_arrow"
  "! grep -E 'return[[:space:]]+const[[:space:]]+RexImplicitCopy' rose_rex_unparser_implicit_copy_and_arrow.cpp && ! grep -E -- '->[[:space:]]*<' rose_rex_unparser_implicit_copy_and_arrow.cpp && grep -E 'chain[[:space:]]*->[[:space:]]*value' rose_rex_unparser_implicit_copy_and_arrow.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_unparser_implicit_copy_and_arrow.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_unparser_partial_specialization_identity"
  "grep -E 'RexPartial[[:space:]]*<[[:space:]]*bool[[:space:]]*,[[:space:]]*Allocator[[:space:]]*>' rose_rex_unparser_partial_specialization_identity.cpp && ! grep -E 'type-parameter-|type_parameter_' rose_rex_unparser_partial_specialization_identity.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_unparser_partial_specialization_identity.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_unparser_anonymous_template_parameters"
  "! grep -E '__non_type_param_|__template_template_param_' rose_rex_unparser_anonymous_template_parameters.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_unparser_anonymous_template_parameters.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_unparser_anonymous_prefix_identity"
  "grep -E 'union[[:space:]]+__anonymous_0xUserUnion' rose_rex_unparser_anonymous_prefix_identity.cpp && grep -E 'enum[[:space:]]+__anonymous_0xUserEnum' rose_rex_unparser_anonymous_prefix_identity.cpp && grep -F '__anonymous_enum_value' rose_rex_unparser_anonymous_prefix_identity.cpp && grep -F '__anonymous_0xvariable.__anonymous_0xUserUnion::value' rose_rex_unparser_anonymous_prefix_identity.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_unparser_anonymous_prefix_identity.cpp"
)

add_test(
  NAME Cxx_tests_rex_unparser_anonymous_prefix_identity_checkpoint
  COMMAND
    sh -c
    "\"$@\" && grep -F '__anonymous_0xvariable.__anonymous_0xUserUnion::value' rose_rex_unparser_anonymous_prefix_identity.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_unparser_anonymous_prefix_identity.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rex:ast-json-checkpoint=pre-omp-construction
    -rex:ast-json-dir=rex_unparser_anonymous_prefix_identity_json
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_anonymous_prefix_identity.cpp)
set_tests_properties(
  Cxx_tests_rex_unparser_anonymous_prefix_identity_checkpoint
  PROPERTIES
    LABELS "Cxx_tests;ASTJSON;rex"
    WORKING_DIRECTORY ${_cxx_unparse_output_dir})
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_anonymous_prefix_identity_checkpoint
  rex_unparser_anonymous_prefix_identity.cpp)

rex_add_cxx_unparse_check_test(
  "rex_unparser_nested_alias_visibility"
  "! grep -F 'rex_nested_alias_target::Payload' rose_rex_unparser_nested_alias_visibility.cpp && test \"$(grep -o 'rex_nested_alias_name::Payload' rose_rex_unparser_nested_alias_visibility.cpp | wc -l)\" -eq 2 && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_unparser_nested_alias_visibility.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_unparser_source_qualification_identity"
  "grep -F 'rex_source_qualification_alias::Payload' rose_rex_unparser_source_qualification_identity.cpp && grep -F '::rex_source_qualification_target::Payload' rose_rex_unparser_source_qualification_identity.cpp && grep -E 'using[[:space:]]+rex_source_qualification_alias::global_value' rose_rex_unparser_source_qualification_identity.cpp && grep -E 'using[[:space:]]+::rex_source_qualification_target::global_value' rose_rex_unparser_source_qualification_identity.cpp && grep -E 'enum[[:space:]]+rex_source_qualification_alias::Mode' rose_rex_unparser_source_qualification_identity.cpp && grep -E 'void[[:space:]]+rex_source_qualification_alias::function' rose_rex_unparser_source_qualification_identity.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_unparser_source_qualification_identity.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_unparser_source_qualification_provenance"
  "grep -F 'REX_SOURCE_QUALIFIER_ALIAS::Payload' rose_rex_unparser_source_qualification_provenance.cpp && grep -E 'using[[:space:]]+RexMacroDelimiterPayload[[:space:]]*=[[:space:]]*REX_SOURCE_DELIMITER_QUALIFIER[[:space:]]+Payload' rose_rex_unparser_source_qualification_provenance.cpp && grep -F '::rex_source_qualification_provenance_target::Payload' rose_rex_unparser_source_qualification_provenance.cpp && grep -E 'using[[:space:]]+REX_SOURCE_QUALIFIER_ALIAS::global_value' rose_rex_unparser_source_qualification_provenance.cpp && grep -E 'using[[:space:]]+::rex_source_global_using_value' rose_rex_unparser_source_qualification_provenance.cpp && grep -E 'enum[[:space:]]+REX_SOURCE_QUALIFIER_ALIAS::Mode' rose_rex_unparser_source_qualification_provenance.cpp && grep -E 'void[[:space:]]+REX_SOURCE_QUALIFIER_ALIAS::function' rose_rex_unparser_source_qualification_provenance.cpp && grep -E 'REX_SOURCE_QUALIFIER_ALIAS::Payload[[:space:]]*\\([[:space:]]*RexSourceQualificationOwner[[:space:]]*::[[:space:]]*\\*[[:space:]]*\\)' rose_rex_unparser_source_qualification_provenance.cpp && grep -F 'RexSourceQualificationOuter<T>::Inner<U>::convert' rose_rex_unparser_source_qualification_provenance.cpp && grep -E 'RexSourceMacroBaseName[[:space:]]*:[[:space:]]*public[[:space:]]+REX_SOURCE_BASE_NAME[[:space:]]*<T>' rose_rex_unparser_source_qualification_provenance.cpp && grep -E 'RexSourceMacroBaseType[[:space:]]*:[[:space:]]*public[[:space:]]+REX_SOURCE_BASE_TYPE\\(T\\)' rose_rex_unparser_source_qualification_provenance.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_unparser_source_qualification_provenance.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_unparser_nonreal_use_site_qualification"
  "grep -E 'std[[:space:]]*::[[:space:]]*size_t[[:space:]]+rex_qualified_size' rose_rex_unparser_nonreal_use_site_qualification.cpp && grep -E 'return[[:space:]]+std[[:space:]]*::[[:space:]]*size_t[[:space:]]*\\([[:space:]]*-1[[:space:]]*\\)' rose_rex_unparser_nonreal_use_site_qualification.cpp && grep -E 'return[[:space:]]+size_t[[:space:]]*\\([[:space:]]*value[[:space:]]*\\)' rose_rex_unparser_nonreal_use_site_qualification.cpp && ! grep -E 'return[[:space:]]+std[[:space:]]*::[[:space:]]*size_t[[:space:]]*\\([[:space:]]*value[[:space:]]*\\)' rose_rex_unparser_nonreal_use_site_qualification.cpp && grep -E 'typename[[:space:]]+RexUseSiteOuter<T>[[:space:]]*::[[:space:]]*Inner[[:space:]]*\\*rex_make_qualified_inner' rose_rex_unparser_nonreal_use_site_qualification.cpp && ! grep -E 'RexUseSiteOuter<T>[[:space:]]*::[[:space:]]*RexUseSiteOuter<T>[[:space:]]*::' rose_rex_unparser_nonreal_use_site_qualification.cpp && grep -E 'RexUseSiteSelect<RexUseSiteWrap<RexUseSiteWrap<T>>>::Type[[:space:]]+rex_nested_qualified_return' rose_rex_unparser_nonreal_use_site_qualification.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_unparser_nonreal_use_site_qualification.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_unparser_typed_type_context"
  "grep -F 'decltype(nullptr)' rose_rex_unparser_typed_type_context.cpp && grep -E 'Owner[[:space:]]*::[[:space:]]*[*]' rose_rex_unparser_typed_type_context.cpp && grep -F '__vector_size__(sizeof(' rose_rex_unparser_typed_type_context.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_unparser_typed_type_context.cpp"
)

add_test(
  NAME Cxx_tests_rex_unparser_typed_owner_contracts
  WORKING_DIRECTORY ${_cxx_unparse_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && test \"\$(grep -Eo 'template[[:space:]]*<[[:space:]]*>' rose_rex_unparser_typed_owner_contracts.cpp | wc -l)\" -eq 5 && grep -E 'base_ns[[:space:]]*::[[:space:]]*Base' rose_rex_unparser_typed_owner_contracts.cpp && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only rose_rex_unparser_typed_owner_contracts.cpp"
    dummy
    $<TARGET_FILE:rex_unparser_typed_owner_contracts_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_typed_owner_contracts.cpp
)
set_tests_properties(
  Cxx_tests_rex_unparser_typed_owner_contracts
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_typed_owner_contracts
  rex_unparser_typed_owner_contracts.cpp)

foreach(malformed_case template_header enclosing_template_header ctor_owner)
  string(TOUPPER ${malformed_case} malformed_case_upper)
  if(malformed_case STREQUAL "template_header")
    set(malformed_diagnostic
      "^REX_UNPARSE_INVARIANT\\[member-specialization-source-form\\]: non-template member specialization=0x[[:xdigit:]]+ name=ordinary requires one or more exact source-owned empty headers, found headers=0 empty=0$")
  elseif(malformed_case STREQUAL "enclosing_template_header")
    set(malformed_diagnostic
      "^REX_UNPARSE_INVARIANT\\[member-specialization-source-form\\]: member-function specialization=0x[[:xdigit:]]+ name=templated<long> requires one or more exact source-owned empty enclosing headers, found headers=0 empty=0$")
  else()
    set(malformed_diagnostic
      "^REX_UNPARSE_INVARIANT\\[constructor-qualification-context\\]: initialized-name=0x[[:xdigit:]]+ list=0x[[:xdigit:]]+ constructor=0x[[:xdigit:]]+ class-scope=0x[[:xdigit:]]+ current-scope=0x[[:xdigit:]]+ source-scope=0x[[:xdigit:]]+ source-statement=0x[[:xdigit:]]+ current-statement=0x[[:xdigit:]]+ emitted=1 occurrences=1$")
  endif()
  add_test(
    NAME Cxx_tests_rex_unparser_typed_owner_rejects_${malformed_case}
    WORKING_DIRECTORY ${_cxx_unparse_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=rex_unparser_typed_owner_${malformed_case}.log; set +e; REX_TEST_MALFORMED_${malformed_case_upper}=1 \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '${malformed_diagnostic}' \"$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_unparser_typed_owner_contracts_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep
      ${ROSE_INCLUDE_FLAGS}
      -std=gnu++17
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_typed_owner_contracts.cpp
  )
  set_tests_properties(
    Cxx_tests_rex_unparser_typed_owner_rejects_${malformed_case}
    PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER;DEATH")
  rex_lock_test_for_input(
    Cxx_tests_rex_unparser_typed_owner_rejects_${malformed_case}
    rex_unparser_typed_owner_contracts.cpp)
endforeach()
unset(malformed_diagnostic)

rex_add_cxx_unparse_check_test(
  "rex_frontend_explicit_instantiation_identity"
  "grep -E 'extern[[:space:]]+template[[:space:]]+(class|struct)[[:space:]]+Holder[[:space:]]*<[[:space:]]*int[[:space:]]*>' rose_rex_frontend_explicit_instantiation_identity.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_frontend_explicit_instantiation_identity.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_frontend_function_explicit_instantiation_identity"
  "grep -E 'extern[[:space:]]+template[[:space:]]+int[[:space:]]+([[:alnum:]_]+::)*twice[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]*\\([[:space:]]*int([[:space:]]+[[:alnum:]_]+)?[[:space:]]*\\)' rose_rex_frontend_function_explicit_instantiation_identity.cpp && grep -E 'template[[:space:]]+long[[:space:]]+([[:alnum:]_]+::)*twice[[:space:]]*<[[:space:]]*long[[:space:]]*>[[:space:]]*\\([[:space:]]*long([[:space:]]+[[:alnum:]_]+)?[[:space:]]*\\)' rose_rex_frontend_function_explicit_instantiation_identity.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_frontend_function_explicit_instantiation_identity.cpp"
)

add_test(
  NAME Cxx_tests_rex_frontend_function_production_contract_structure
  COMMAND
    $<TARGET_FILE:rex_frontend_function_production_contract_translator>
    -rose:skipfinalCompileStep
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_function_production_contract.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_function_production_contract_structure
  PROPERTIES LABELS "Cxx_tests;CFE;rex;frontend;contracts"
)

set(_rex_initializer_destination_output
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_initializer_destination_type.output.cpp")
add_test(
  NAME Cxx_tests_rex_frontend_initializer_destination_type_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "output=\"$1\"; shift; \"$@\" && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only \"$output\""
    dummy
    "${_rex_initializer_destination_output}"
    $<TARGET_FILE:rex_frontend_initializer_destination_type_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output "${_rex_initializer_destination_output}"
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_initializer_destination_type.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_initializer_destination_type_roundtrip
  PROPERTIES LABELS "Cxx_tests;rex;CFE;AST_JSON;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_initializer_destination_type_roundtrip
  rex_frontend_initializer_destination_type.cpp)

rex_add_cxx_unparse_check_test(
  "rex_frontend_base_specifier_identity"
  "grep -E 'DerivedFromAlias[[:space:]]*:[^{}]*Alias[[:space:]]*\\{' rose_rex_frontend_base_specifier_identity.cpp && grep -E 'DerivedFromAliasTemplate[[:space:]]*:[^{}]*BoxAlias[[:space:]]*<[[:space:]]*int[[:space:]]*>' rose_rex_frontend_base_specifier_identity.cpp && grep -E 'DependentBase[[:space:]]*:[^{}]*T[[:space:]]*\\{' rose_rex_frontend_base_specifier_identity.cpp && grep -E 'PackedBases[[:space:]]*:[^{}]*Bases[[:space:]]*[.][.][.]' rose_rex_frontend_base_specifier_identity.cpp && ! grep -q '__unknown_base' rose_rex_frontend_base_specifier_identity.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_frontend_base_specifier_identity.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_frontend_declaration_scope_contract"
  "test \"$(grep -Ec 'friend[[:space:]]+void[[:space:]]+rex_friend_template' rose_rex_frontend_declaration_scope_contract.cpp)\" -eq 1 && grep -E 'friend[[:space:]]+void[[:space:]]+::rex_global_target' rose_rex_frontend_declaration_scope_contract.cpp && ! grep -E 'template[[:space:]]*<>[[:space:]]*void[[:space:]]+rex_member' rose_rex_frontend_declaration_scope_contract.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_frontend_declaration_scope_contract.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_frontend_typeof_function_declarator"
  "grep -E 'extern[[:space:]]+__typeof.*rex_typeof_target.*rex_typeof_expression_alias[[:space:]]*;' rose_rex_frontend_typeof_function_declarator.cpp && grep -E 'extern[[:space:]]+__typeof.*rex_typeof_nothrow_target.*rex_typeof_nothrow_alias[[:space:]]*;' rose_rex_frontend_typeof_function_declarator.cpp && grep -E 'extern[[:space:]]+__typeof.*int.*int.*rex_typeof_type_alias[[:space:]]*;' rose_rex_frontend_typeof_function_declarator.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_frontend_typeof_function_declarator.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_frontend_implicit_control_flow_scope"
  "grep -zEq 'if[[:space:]]*[(][[:space:]]*condition[[:space:]]*[)][[:space:]]*class[[:space:]]+RexIfLocal' rose_rex_frontend_implicit_control_flow_scope.cpp && grep -zEq 'while[[:space:]]*[(][[:space:]]*condition[[:space:]]*[)][[:space:]]*class[[:space:]]+RexWhileLocal' rose_rex_frontend_implicit_control_flow_scope.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++14 -fsyntax-only rose_rex_frontend_implicit_control_flow_scope.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_frontend_new_initializer_source_form"
  "grep -E 'rex_default[[:space:]]*=[[:space:]]*new[[:space:]]+RexNewObject[[:space:]]*;' rose_rex_frontend_new_initializer_source_form.cpp && grep -E 'rex_value[[:space:]]*=[[:space:]]*new[[:space:]]+RexNewObject[[:space:]]*[(][[:space:]]*[)];' rose_rex_frontend_new_initializer_source_form.cpp && grep -E 'rex_braced[[:space:]]*=[[:space:]]*new[[:space:]]+RexNewObject[[:space:]]*[{][[:space:]]*[}];' rose_rex_frontend_new_initializer_source_form.cpp && grep -E 'rex_argument[[:space:]]*=[[:space:]]*new[[:space:]]+RexNewObject[[:space:]]*[(][[:space:]]*1[[:space:]]*[)];' rose_rex_frontend_new_initializer_source_form.cpp && grep -E 'rex_scalar_value[[:space:]]*=[[:space:]]*new[[:space:]]+int[[:space:]]*[(][[:space:]]*[)];' rose_rex_frontend_new_initializer_source_form.cpp && ! grep -F '(())' rose_rex_frontend_new_initializer_source_form.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++14 -fsyntax-only rose_rex_frontend_new_initializer_source_form.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_frontend_lazy_system_header_fields"
  "! grep -E 'type-parameter-|type_parameter_' rose_rex_frontend_lazy_system_header_fields.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_frontend_lazy_system_header_fields.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_frontend_static_member_publication"
  "grep -E 'RexStaticMemberOwner::MemberType[[:space:]]+RexStaticMemberOwner::member' rose_rex_frontend_static_member_publication.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_frontend_static_member_publication.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_frontend_template_redeclaration_parameter_spelling"
  "grep -E 'template[[:space:]]*<[^>]*DefinitionLeft[^>]*DefinitionRight' rose_rex_frontend_template_redeclaration_parameter_spelling.cpp && grep -E 'void[[:space:]]+rex_exchange[[:space:]]*[(][[:space:]]*DefinitionLeft' rose_rex_frontend_template_redeclaration_parameter_spelling.cpp && grep -E 'template[[:space:]]*<[^>]*RedeclarationLeft[^>]*RedeclarationRight' rose_rex_frontend_template_redeclaration_parameter_spelling.cpp && grep -E 'void[[:space:]]+rex_exchange[[:space:]]*[(][[:space:]]*RedeclarationLeft' rose_rex_frontend_template_redeclaration_parameter_spelling.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_frontend_template_redeclaration_parameter_spelling.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_unparser_extended_float_identity"
  "grep -F '1.5f16' rose_rex_unparser_extended_float_identity.cpp && grep -F '1.25Q' rose_rex_unparser_extended_float_identity.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++17 -c rose_rex_unparser_extended_float_identity.cpp"
)

# Issue 160/#268: Keep the instantiated entity in the template's semantic
# namespace while emitting each explicit-instantiation directive in its exact
# source-written lexical scope.
set(_issue160_system_header_function_instantiation_base
    "rex_test2025_issue160_system_header_instantiation")
set(_issue160_system_header_function_instantiation_generated_file
    "rose_${_issue160_system_header_function_instantiation_base}.cpp")
string(CONCAT _issue160_system_header_function_instantiation_check_command
  "grep -E '^[[:space:]]*template[[:space:]]+int[[:space:]]+rex_test2025_issue160::scale_and_shift[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]*\\(' \"${_issue160_system_header_function_instantiation_generated_file}\" && "
  "grep -E '^[[:space:]]*template[[:space:]]+(class|struct)[[:space:]]+rex_test2025_issue160::DefaultedWrapper[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]*;' \"${_issue160_system_header_function_instantiation_generated_file}\" && "
  "grep -E '^[[:space:]]*template[[:space:]]+int[[:space:]]+namespace_adjust[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]*\\(' \"${_issue160_system_header_function_instantiation_generated_file}\" && "
  "\"${CMAKE_CXX_COMPILER}\" -std=c++20 -I\"${CMAKE_CURRENT_SOURCE_DIR}\" -c \"${_issue160_system_header_function_instantiation_generated_file}\""
)
rex_add_cxx_unparse_check_test(
  "${_issue160_system_header_function_instantiation_base}"
  "${_issue160_system_header_function_instantiation_check_command}"
)

set(_issue160_system_header_class_instantiation_base
    "rex_test2025_issue160_system_header_class_instantiation")
set(_issue160_system_header_class_instantiation_namespace
    "rex_test2025_issue160")
set(_issue160_grep_pattern
    "^[[:space:]]*template[[:space:]]+(class|struct)[[:space:]]+${_issue160_system_header_class_instantiation_namespace}::Box[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]*;")
set(_issue160_generated_file
    "rose_${_issue160_system_header_class_instantiation_base}.cpp")
string(CONCAT _issue160_system_header_class_instantiation_check_command
  "grep -E \"${_issue160_grep_pattern}\" \"${_issue160_generated_file}\" && "
  "\"${CMAKE_CXX_COMPILER}\" -std=c++20 -I\"${CMAKE_CURRENT_SOURCE_DIR}\" -c \"${_issue160_generated_file}\""
)
rex_add_cxx_unparse_check_test(
  "${_issue160_system_header_class_instantiation_base}"
  "${_issue160_system_header_class_instantiation_check_command}"
)

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

rex_add_cxx_unparse_check_test(
  "rex_test2026_anonymous_member_no_qualification"
  "! grep -F '__anonymous_0x' rose_rex_test2026_anonymous_member_no_qualification.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++14 -c rose_rex_test2026_anonymous_member_no_qualification.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_test2026_friend_template_saved_spelling"
  "grep -E 'friend[[:space:]]+U[[:space:]]+rex_test2026_friend_template_saved_spelling_friend' rose_rex_test2026_friend_template_saved_spelling.cpp && grep -F 'after_friend' rose_rex_test2026_friend_template_saved_spelling.cpp && ${CMAKE_CXX_COMPILER} -std=c++98 -c rose_rex_test2026_friend_template_saved_spelling.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_test2026_dependent_using_dedup"
  "awk '/using .*touch/ { ++n } END { exit !(n == 1) }' rose_rex_test2026_dependent_using_dedup.cpp && ${CMAKE_CXX_COMPILER} -std=c++98 -c rose_rex_test2026_dependent_using_dedup.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_test2026_inline_constructor_preserved"
  "grep -F 'RexTest2026InlineConstructorBase' rose_rex_test2026_inline_constructor_preserved.cpp && ${CMAKE_CXX_COMPILER} -std=c++98 -c rose_rex_test2026_inline_constructor_preserved.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_test2026_namespace_template_source_order"
  "awk '/template <typename T> inline void swap/ { if (!def) def=NR } /rex_test2026_namespace_template_source_order::swap/ { if (!call) call=NR } END { exit !(def && call && def < call) }' rose_rex_test2026_namespace_template_source_order.cpp && ${CMAKE_CXX_COMPILER} -std=c++98 -c rose_rex_test2026_namespace_template_source_order.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_test2026_guarded_friend_template_definition"
  "awk '/#if REX_TEST2026_AS_FRIEND/ { ++n } END { exit !(n == 2) }' rose_rex_test2026_guarded_friend_template_definition.cpp && ${CMAKE_CXX_COMPILER} -std=gnu++14 -c rose_rex_test2026_guarded_friend_template_definition.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_test2026_reopened_namespace_template_spelling"
  "${CMAKE_CXX_COMPILER} -std=gnu++14 -c rose_rex_test2026_reopened_namespace_template_spelling.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_test2026_base_alias_skips_ctor_parameter"
  "${CMAKE_CXX_COMPILER} -std=gnu++14 -c rose_rex_test2026_base_alias_skips_ctor_parameter.cpp"
)

rex_add_cxx_unparse_check_test(
  "rex_test2026_split_template_defaults"
  "${CMAKE_CXX_COMPILER} -std=gnu++14 -c rose_rex_test2026_split_template_defaults.cpp"
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

add_executable(
  rex_frontend_phase_c_declarator_owned_local_class_translator
  rex_frontend_phase_c_declarator_owned_local_class_translator.cpp
)
target_link_libraries(
  rex_frontend_phase_c_declarator_owned_local_class_translator
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

set(_rex_phase_c_local_class_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_phase_c_declarator_owned_local_class.cpp)
add_test(
  NAME Cxx_tests_rex_frontend_phase_c_declarator_owned_local_class
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && '${CMAKE_CXX_COMPILER}' -std=gnu++17 -fsyntax-only '${_rex_phase_c_local_class_output}'"
    dummy
    $<TARGET_FILE:rex_frontend_phase_c_declarator_owned_local_class_translator>
    ${ROSE_FLAGS}
    -rose:skipfinalCompileStep
    -rose:output ${_rex_phase_c_local_class_output}
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_phase_c_declarator_owned_local_class.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_phase_c_declarator_owned_local_class
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER"
)
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_phase_c_declarator_owned_local_class
  rex_frontend_phase_c_declarator_owned_local_class.cpp)

# A deduced member-template call has two exact identities: the primary
# template written in source and the concrete callable selected by deduction.
# Cover explicit member syntax, operator syntax, detached expression copy, and
# serialization roundtrip without allowing either identity to collapse.
add_executable(
  rex_frontend_template_member_semantic_identity_translator
  rex_frontend_template_member_semantic_identity_translator.cpp
)
target_link_libraries(
  rex_frontend_template_member_semantic_identity_translator
  ROSE_DLL
  ${link_with_libraries}
)

set(_rex_template_member_semantic_input
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_template_member_semantic_identity.cpp)
set(_rex_template_member_semantic_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_template_member_semantic_identity.cpp)
add_test(
  NAME Cxx_tests_rex_frontend_template_member_semantic_identity
  COMMAND
    sh -c
    "\"$@\" && ${CMAKE_CXX_COMPILER} -std=gnu++17 -fsyntax-only '${_rex_template_member_semantic_output}'"
    dummy
    $<TARGET_FILE:rex_frontend_template_member_semantic_identity_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output ${_rex_template_member_semantic_output}
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -c ${_rex_template_member_semantic_input}
)
set_tests_properties(
  Cxx_tests_rex_frontend_template_member_semantic_identity
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER"
)
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_template_member_semantic_identity
  rex_frontend_template_member_semantic_identity.cpp)

add_executable(
  rex_frontend_reference_nttp_template_argument_translator
  rex_frontend_reference_nttp_template_argument_translator.cpp
)
target_link_libraries(
  rex_frontend_reference_nttp_template_argument_translator
  ROSE_DLL
  ${link_with_libraries}
)

set(_rex_reference_nttp_input
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_reference_nttp_template_argument.cpp)
set(_rex_reference_nttp_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_reference_nttp_template_argument.cpp)
add_test(
  NAME Cxx_tests_rex_frontend_reference_nttp_template_argument
  COMMAND
    sh -c
    "\"$@\" && ${CMAKE_CXX_COMPILER} -std=gnu++17 -fsyntax-only '${_rex_reference_nttp_output}'"
    dummy
    $<TARGET_FILE:rex_frontend_reference_nttp_template_argument_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output ${_rex_reference_nttp_output}
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -c ${_rex_reference_nttp_input}
)
set_tests_properties(
  Cxx_tests_rex_frontend_reference_nttp_template_argument
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER"
)
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_reference_nttp_template_argument
  rex_frontend_reference_nttp_template_argument.cpp)

set(_rex_template_member_semantic_json_dir
  ${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_template_member_semantic_identity_json)
set(_rex_template_member_semantic_json_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_template_member_semantic_identity_json.cpp)
add_test(
  NAME Cxx_tests_rex_frontend_template_member_semantic_identity_ast_json
  COMMAND
    sh -c
    "\"$@\" && ${CMAKE_CXX_COMPILER} -std=gnu++17 -fsyntax-only '${_rex_template_member_semantic_json_output}'"
    dummy
    $<TARGET_FILE:rex_frontend_template_member_semantic_identity_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rex:ast-json-checkpoint=pre-omp-construction
    -rex:ast-json-dir=${_rex_template_member_semantic_json_dir}
    -rose:output ${_rex_template_member_semantic_json_output}
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -c ${_rex_template_member_semantic_input}
)
set_tests_properties(
  Cxx_tests_rex_frontend_template_member_semantic_identity_ast_json
  PROPERTIES LABELS "Cxx_tests;ASTJSON;rex;CFE;UNPARSER"
)
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_template_member_semantic_identity_ast_json
  rex_frontend_template_member_semantic_identity.cpp)

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
set(rex_frontend_provenance_system_header_dir
    ${CMAKE_CURRENT_SOURCE_DIR}/../input_codes/system_header_like)

add_test(
  NAME Cxx_tests_rex_frontend_injected_class_nttp_provenance
  COMMAND
    $<TARGET_FILE:rex_frontend_injected_class_nttp_provenance_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -isystem ${rex_frontend_provenance_system_header_dir}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_injected_class_nttp_provenance.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_injected_class_nttp_provenance
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_injected_class_nttp_provenance
  rex_frontend_injected_class_nttp_provenance.cpp)

set(_rex_system_header_local_constructor_pp
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_system_header_local_constructor_family.pp.cpp")
add_test(
  NAME Cxx_tests_rex_frontend_system_header_local_constructor_preprocess
  COMMAND
    ${CMAKE_CXX_COMPILER}
    -std=c++11
    -E
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_system_header_local_constructor_family.cpp
    -o ${_rex_system_header_local_constructor_pp}
)
set_tests_properties(
  Cxx_tests_rex_frontend_system_header_local_constructor_preprocess
  PROPERTIES
    FIXTURES_SETUP rex_frontend_system_header_local_constructor_input
    LABELS "Cxx_tests;rex;CFE")

add_test(
  NAME Cxx_tests_rex_frontend_system_header_local_constructor_family
  COMMAND
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=c++11
    -c ${_rex_system_header_local_constructor_pp}
)
set_tests_properties(
  Cxx_tests_rex_frontend_system_header_local_constructor_family
  PROPERTIES
    FIXTURES_REQUIRED rex_frontend_system_header_local_constructor_input
    LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_system_header_local_constructor_family
  rex_frontend_system_header_local_constructor_family.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_function_body_auxiliary_provenance
  COMMAND
    $<TARGET_FILE:rex_frontend_function_body_auxiliary_provenance_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -isystem ${rex_frontend_provenance_system_header_dir}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_function_body_auxiliary_provenance.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_function_body_auxiliary_provenance
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_function_body_auxiliary_provenance
  rex_frontend_function_body_auxiliary_provenance.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_semantic_auxiliary_access
  COMMAND
    $<TARGET_FILE:rex_frontend_semantic_auxiliary_access_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -isystem ${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_semantic_auxiliary_access.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_semantic_auxiliary_access
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_semantic_auxiliary_access
  rex_frontend_semantic_auxiliary_access.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_explicit_specialization_instantiation_chain
  COMMAND
    $<TARGET_FILE:rex_frontend_explicit_specialization_instantiation_chain_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_explicit_specialization_instantiation_chain.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_explicit_specialization_instantiation_chain
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_explicit_specialization_instantiation_chain
  rex_frontend_explicit_specialization_instantiation_chain.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_requires_local_parameter_owner
  COMMAND
    $<TARGET_FILE:rex_frontend_requires_local_parameter_owner_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++20
    -isystem ${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_requires_local_parameter_owner.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_requires_local_parameter_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_requires_local_parameter_owner
  rex_frontend_requires_local_parameter_owner.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_requires_dependent_alias_owner
  COMMAND
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++20
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_requires_dependent_alias_owner.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_requires_dependent_alias_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_requires_dependent_alias_owner
  rex_frontend_requires_dependent_alias_owner.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_application_header_semantic_pragma_owner
  COMMAND
    $<TARGET_FILE:rex_frontend_application_header_semantic_pragma_owner_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_application_header_semantic_pragma_owner.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_application_header_semantic_pragma_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_application_header_semantic_pragma_owner
  rex_frontend_application_header_semantic_pragma_owner.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_application_header_member_publication
  COMMAND
    $<TARGET_FILE:rex_frontend_application_header_member_publication_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    -rose:unparseHeaderFiles
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_application_header_member_publication.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_application_header_member_publication
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_application_header_member_publication
  rex_frontend_application_header_member_publication.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_application_header_member_publication_token
  COMMAND
    $<TARGET_FILE:rex_frontend_application_header_member_publication_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -std=gnu++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_application_header_member_publication.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_application_header_member_publication_token
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_application_header_member_publication_token
  rex_frontend_application_header_member_publication.cpp)

add_test(
  NAME Cxx_tests_rex_test2025_issue148_system_header_mutation
  COMMAND
    $<TARGET_FILE:rex_test2025_issue148_system_header_mutation_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparseHeaderFiles
    ${ROSE_INCLUDE_FLAGS}
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
  rex_token_mapping_macro_ended_declaration.cpp
  rex_token_mapping_macro_declaration_fragments.cpp
  rex_token_mapping_function_prototype_boundaries.cpp
  rex_frontend_macro_splice_contract.cpp
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

add_test(
  NAME Cxx_tests_rex_test2025_cfe_token_stream_mapping_rejects_duplicate_macro_publication
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; REX_TEST_TOKEN_MAPPING_CORRUPTION=duplicate-macro-publication \"$@\" >rex_test2025_cfe_token_stream_mapping_duplicate_macro_publication.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_TOKEN_INVARIANT\\[macro-expansion-publication\\]: file=.*rex_test2025_cfe_token_stream_mapping_macro_transform\\.cpp already contains [1-9][0-9]* published macro-expansion associations$' rex_test2025_cfe_token_stream_mapping_duplicate_macro_publication.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_test2025_cfe_token_stream_mapping_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_cfe_token_stream_mapping_macro_transform.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_cfe_token_stream_mapping_rejects_duplicate_macro_publication
  PROPERTIES LABELS "Cxx_tests;rex;death")
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_cfe_token_stream_mapping_rejects_duplicate_macro_publication
  rex_test2025_cfe_token_stream_mapping_macro_transform.cpp)

add_test(
  NAME Cxx_tests_rex_test2025_cfe_token_stream_mapping_rejects_invalid_macro_physical_file
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; REX_TEST_TOKEN_MAPPING_CORRUPTION=invalid-macro-physical-file \"$@\" >rex_test2025_cfe_token_stream_mapping_invalid_macro_physical_file.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[file-info-physical-owner\\]: file-info=0x[[:xdigit:]]+ has impossible stored physical-file-id=-1 transformation=0 compiler-generated=0$' rex_test2025_cfe_token_stream_mapping_invalid_macro_physical_file.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_test2025_cfe_token_stream_mapping_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_cfe_token_stream_mapping_macro_transform.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_cfe_token_stream_mapping_rejects_invalid_macro_physical_file
  PROPERTIES LABELS "Cxx_tests;rex;death")
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_cfe_token_stream_mapping_rejects_invalid_macro_physical_file
  rex_test2025_cfe_token_stream_mapping_macro_transform.cpp)

add_test(
  NAME Cxx_tests_rex_test2025_cfe_token_stream_mapping_rejects_invalid_macro_source_physical_file
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; REX_TEST_TOKEN_MAPPING_CORRUPTION=invalid-macro-source-physical-file \"$@\" >rex_test2025_cfe_token_stream_mapping_invalid_macro_source_physical_file.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[file-info-physical-owner\\]: file-info=0x[[:xdigit:]]+ has impossible stored physical-file-id=-1 transformation=0 compiler-generated=0$' rex_test2025_cfe_token_stream_mapping_invalid_macro_source_physical_file.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_test2025_cfe_token_stream_mapping_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_cfe_token_stream_mapping_macro_transform.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_cfe_token_stream_mapping_rejects_invalid_macro_source_physical_file
  PROPERTIES LABELS "Cxx_tests;rex;death")
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_cfe_token_stream_mapping_rejects_invalid_macro_source_physical_file
  rex_test2025_cfe_token_stream_mapping_macro_transform.cpp)

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

set(_rex_token_contract_output
    ${CMAKE_CURRENT_BINARY_DIR}/rex_test2026_token_unparse_contract_output.cpp)
add_test(
  NAME Cxx_tests_rex_test2026_token_unparse_contract_rejects_incomplete_map
  COMMAND
    sh -c
    "ulimit -c 0; printf '%s\\n' REX_TOKEN_CONTRACT_SENTINEL > '${_rex_token_contract_output}'; set +e; \"$@\" 2> rex_test2026_token_unparse_contract.err; status=$?; set -e; test $status -eq 134 && test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[token-map\\]: token-unparse contract violation: file=.*/rex_test2026_token_unparse_contract\\.cpp node=SgGlobal address=0x[[:xdigit:]]+ missing global-scope mapping$' rex_test2026_token_unparse_contract.err)\" -eq 1 && grep -Fx REX_TOKEN_CONTRACT_SENTINEL '${_rex_token_contract_output}' && ! find '${CMAKE_CURRENT_BINARY_DIR}' -maxdepth 1 -name '.rex_test2026_token_unparse_contract_output.cpp.rex-unparse-*' -print -quit | grep -q ."
    dummy
    $<TARGET_FILE:rex_test2026_token_unparse_contract_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -rose:output ${_rex_token_contract_output}
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

add_executable(
  rex_unparser_exact_token_replay_translator
  rex_unparser_exact_token_replay_translator.cpp
)
target_link_libraries(
  rex_unparser_exact_token_replay_translator
  ROSE_DLL
  ${link_with_libraries}
)

set(_rex_exact_token_replay_source
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_exact_token_replay.cpp)
add_test(
  NAME Cxx_tests_rex_unparser_exact_token_replay_is_byte_exact
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_unparser_exact_token_replay_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_exact_token_replay_source}
)
set_tests_properties(
  Cxx_tests_rex_unparser_exact_token_replay_is_byte_exact
  PROPERTIES LABELS "Cxx_tests;rex")
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_exact_token_replay_is_byte_exact
  rex_unparser_exact_token_replay.cpp)

set(_rex_shared_macro_mapping_source
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_token_mapping_shared_macro.cpp)
add_test(
  NAME Cxx_tests_rex_token_mapping_shared_macro_interval
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_unparser_exact_token_replay_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_shared_macro_mapping_source}
)
set_tests_properties(
  Cxx_tests_rex_token_mapping_shared_macro_interval
  PROPERTIES LABELS "Cxx_tests;rex")
rex_lock_test_for_input(
  Cxx_tests_rex_token_mapping_shared_macro_interval
  rex_token_mapping_shared_macro.cpp)

add_test(
  NAME Cxx_tests_rex_unparser_token_mapping_draft_optional_whitespace
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    ${CMAKE_COMMAND} -E env
    REX_TEST_TOKEN_REPLAY_CORRUPTION=draft-optional-whitespace
    $<TARGET_FILE:rex_unparser_exact_token_replay_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_exact_token_replay_source}
)
set_tests_properties(
  Cxx_tests_rex_unparser_token_mapping_draft_optional_whitespace
  PROPERTIES LABELS "Cxx_tests;rex")
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_token_mapping_draft_optional_whitespace
  rex_unparser_exact_token_replay.cpp)

set(_rex_exact_token_replay_empty_source
    ${CMAKE_CURRENT_SOURCE_DIR}/../Cxx11_tests/test2020_emptyFile.C)
add_test(
  NAME Cxx_tests_rex_unparser_token_mapping_empty_file
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_unparser_exact_token_replay_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_exact_token_replay_empty_source}
)
set_tests_properties(
  Cxx_tests_rex_unparser_token_mapping_empty_file
  PROPERTIES LABELS "Cxx_tests;rex")
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_token_mapping_empty_file
  ${_rex_exact_token_replay_empty_source})

add_test(
  NAME Cxx_tests_rex_unparser_frontier_excludes_exact_auxiliary_declarations
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    ${CMAKE_COMMAND} -E env
    REX_TEST_TOKEN_REPLAY_CORRUPTION=frontier-auxiliary-positive
    $<TARGET_FILE:rex_unparser_exact_token_replay_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_exact_token_replay_source}
)
set_tests_properties(
  Cxx_tests_rex_unparser_frontier_excludes_exact_auxiliary_declarations
  PROPERTIES LABELS "Cxx_tests;rex")
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_frontier_excludes_exact_auxiliary_declarations
  rex_unparser_exact_token_replay.cpp)

foreach(auxiliary_corruption
    frontier-auxiliary-detached-container
    frontier-auxiliary-detached-declaration)
  string(REPLACE "-" "_" auxiliary_corruption_name
    ${auxiliary_corruption})
  if(auxiliary_corruption STREQUAL "frontier-auxiliary-detached-container")
    set(auxiliary_corruption_diagnostic
      "^REX_UNPARSE_INVARIANT\\[frontier-auxiliary-owner\\]: node=0x[[:xdigit:]]+/SgAuxiliaryDeclarationList .*reason=container has no exact lexical-scope owner$")
  else()
    set(auxiliary_corruption_diagnostic
      "^REX_UNPARSE_INVARIANT\\[frontier-auxiliary-owner\\]: node=0x[[:xdigit:]]+/SgTypedefDeclaration .*reason=traversed declaration is not owned exactly once by the container$")
  endif()
  add_test(
    NAME Cxx_tests_rex_unparser_${auxiliary_corruption_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND
      ${BASH_EXECUTABLE} -c
      "ulimit -c 0; mode=\"$1\"; shift; log=rex_unparser_${auxiliary_corruption_name}.log; rm -f \"$log\"; set +e; REX_TEST_TOKEN_REPLAY_CORRUPTION=\"$mode\" \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec -- '${auxiliary_corruption_diagnostic}' \"$log\")\" -eq 1; test \"\$(grep -Ec '^REX_(TOKEN|UNPARSE)_INVARIANT' \"$log\")\" -eq 1"
      dummy
      ${auxiliary_corruption}
      $<TARGET_FILE:rex_unparser_exact_token_replay_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
      ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${_rex_exact_token_replay_source}
  )
  set_tests_properties(
    Cxx_tests_rex_unparser_${auxiliary_corruption_name}
    PROPERTIES LABELS "Cxx_tests;rex;death")
  rex_lock_test_for_input(
    Cxx_tests_rex_unparser_${auxiliary_corruption_name}
    rex_unparser_exact_token_replay.cpp)
endforeach()

add_test(
  NAME Cxx_tests_rex_unparser_frontier_rejects_missing_typedef_definition
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=rex_unparser_frontier_missing_typedef_definition.log; rm -f \"$log\"; set +e; REX_TEST_TOKEN_REPLAY_CORRUPTION=frontier-missing-typedef-definition \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_AST_INVARIANT\\[typedef-inline-tag-definition-owner\\]: typedef=0x[[:xdigit:]]+ declaration=\\(nil\\) is not one exact owned non-autonomous class/enum definition$' \"$log\")\" -eq 1; test \"\$(grep -Ec '^REX_(AST|TOKEN|UNPARSE)_INVARIANT' \"$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_exact_token_replay_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_exact_token_replay_source}
)
set_tests_properties(
  Cxx_tests_rex_unparser_frontier_rejects_missing_typedef_definition
  PROPERTIES LABELS "Cxx_tests;rex;death")
rex_lock_test_for_input(
  Cxx_tests_rex_unparser_frontier_rejects_missing_typedef_definition
  rex_unparser_exact_token_replay.cpp)

foreach(corruption
    inverted
    out-of-bounds
    nonadjacent
    non-whitespace-request
    draft-half-present
    draft-missing-core
    draft-double-publication
    draft-mutation-after-publication
    missing-child-interval
    empty-nonroot
    producer-inverted
    producer-missing-core
    producer-nonadjacent)
  string(REPLACE "-" "_" corruption_name ${corruption})
  if(corruption STREQUAL "nonadjacent")
    set(corruption_diagnostic
      "^REX_UNPARSE_INVARIANT\\[token-replay\\]: file=.*rex_unparser_exact_token_replay\\.cpp node=0x[[:xdigit:]]+/SgGlobal has inconsistent published intervals leading=\\[[0-9]+,[0-9]+\\) core=\\[[0-9]+,[0-9]+\\) trailing=\\[[0-9]+,[0-9]+\\) else=\\[[0-9]+,[0-9]+\\)$")
  elseif(corruption STREQUAL "non-whitespace-request")
    set(corruption_diagnostic
      "^REX_UNPARSE_INVARIANT\\[token-replay\\]: file=.*rex_unparser_exact_token_replay\\.cpp interval=\\[0,[1-9][0-9]*\\) requested whitespace-only replay but token=0 has classification=[0-9]+$")
  elseif(corruption STREQUAL "producer-inverted")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[published-interval\\]: node=0x[[:xdigit:]]+/SgGlobal token-subsequence interval \\[1,0\\) is outside \\[0,[1-9][0-9]*\\)$")
  elseif(corruption STREQUAL "producer-missing-core")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[published-interval\\]: node=0x[[:xdigit:]]+/SgGlobal has inconsistent intervals leading=\\[0,0\\) core=\\[0,0\\) trailing=\\[0,0\\) else=\\[0,0\\)$")
  elseif(corruption STREQUAL "producer-nonadjacent")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[published-interval\\]: node=0x[[:xdigit:]]+/SgGlobal has inconsistent intervals leading=\\[0,1\\) core=\\[2,3\\) trailing=\\[3,3\\) else=\\[3,3\\)$")
  elseif(corruption STREQUAL "draft-half-present")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[draft-interval\\]: node=0x[[:xdigit:]]+/SgGlobal optional leading-whitespace has half-present inclusive endpoints \\[-1,0\\]$")
  elseif(corruption STREQUAL "draft-missing-core")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[draft-interval\\]: node=0x[[:xdigit:]]+/SgGlobal required token-subsequence inclusive interval \\[-1,-1\\] is invalid$")
  elseif(corruption STREQUAL "draft-double-publication")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[half-open-publication\\]: node=0x[[:xdigit:]]+/SgGlobal token intervals were published more than once$")
  elseif(corruption STREQUAL "draft-mutation-after-publication")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[draft-mutation\\]: cannot replace a core interval after publication$")
  elseif(corruption STREQUAL "missing-child-interval")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[direct-owner-interval\\]: node=0x[[:xdigit:]]+/SgPragmaDeclaration file=.*rex_unparser_exact_token_replay\\.cpp parent=0x[[:xdigit:]]+/SgGlobal has no exact inherited or cross-file boundary interval$")
  elseif(corruption STREQUAL "empty-nonroot")
    set(corruption_diagnostic
      "^REX_TOKEN_INVARIANT\\[empty-root\\]: node=0x[[:xdigit:]]+/SgPragmaDeclaration cannot own the empty token stream$")
  else()
    set(corruption_diagnostic
      "^REX_UNPARSE_INVARIANT\\[token-replay\\]: file=.*rex_unparser_exact_token_replay\\.cpp node=0x[[:xdigit:]]+/SgGlobal token-subsequence=\\[-?[0-9]+,-?[0-9]+\\) is outside \\[0,[0-9]+\\)$")
  endif()
  add_test(
    NAME Cxx_tests_rex_unparser_exact_token_replay_rejects_${corruption_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND
      ${BASH_EXECUTABLE} -c
      "ulimit -c 0; mode=\"$1\"; shift; log=rex_unparser_exact_token_replay_${corruption_name}.log; rm -f \"$log\"; set +e; REX_TEST_TOKEN_REPLAY_CORRUPTION=\"$mode\" \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec -- '${corruption_diagnostic}' \"$log\")\" -eq 1; test \"\$(grep -Ec '^REX_(TOKEN|UNPARSE)_INVARIANT' \"$log\")\" -eq 1"
      dummy
      ${corruption}
      $<TARGET_FILE:rex_unparser_exact_token_replay_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
      ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${_rex_exact_token_replay_source}
  )
  set_tests_properties(
    Cxx_tests_rex_unparser_exact_token_replay_rejects_${corruption_name}
    PROPERTIES LABELS "Cxx_tests;rex;death")
  rex_lock_test_for_input(
    Cxx_tests_rex_unparser_exact_token_replay_rejects_${corruption_name}
    rex_unparser_exact_token_replay.cpp)
endforeach()

if(UNIX AND NOT APPLE)
  # CMake-level root-cause guard: Clang frontend builds must link shared libLLVM.
  add_test(
    NAME Cxx_tests_rex_test2026_shared_llvm_linkage
    COMMAND
      sh -c "readelf -d $<TARGET_FILE:ROSE_DLL> | grep -E 'NEEDED.*libLLVM([^[:space:]]*)\\.so' >/dev/null"
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

# Extra Issue 144 diagnostic coverage beyond the default compile test.
rex_add_clang_frontend_negative_test(
  Cxx_tests_rex_test2025_cfe_error_exit_code_diagnostics
  Cxx_tests
  ${CMAKE_CURRENT_BINARY_DIR}
  rex_test2025_cfe_error_exit_code.log
  "rex_test2025_cfe_error_exit_code.cpp:3:5: error: non-void function 'main' should return a value"
  $<TARGET_FILE:${translator}>
  ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
  -I${CMAKE_CURRENT_SOURCE_DIR}
  -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_cfe_error_exit_code.cpp)
rex_lock_test_for_input(
  Cxx_tests_rex_test2025_cfe_error_exit_code_diagnostics
  rex_test2025_cfe_error_exit_code.cpp)

# rex_test2026: a hard Clang diagnostic must stop before ROSE translates or
# postprocesses an invalid recursive-template instantiation graph.
rex_add_clang_frontend_negative_test(
  Cxx_tests_rex_test2026_recursive_template_diagnostic_stop
  Cxx_tests
  ${CMAKE_CURRENT_BINARY_DIR}
  rex_test2026_recursive_template_diagnostic_stop.log
  "rex_test2026_recursive_template_diagnostic_stop.cpp:2:3: fatal error: recursive template instantiation exceeded maximum depth of 32"
  $<TARGET_FILE:${translator}>
  ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
  -ftemplate-depth=32
  -I${CMAKE_CURRENT_SOURCE_DIR}
  -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_recursive_template_diagnostic_stop.cpp)
set_tests_properties(
  Cxx_tests_rex_test2026_recursive_template_diagnostic_stop
  PROPERTIES
    ENVIRONMENT "ROSE_PHASE_TRACE=1"
    FIXTURES_SETUP rex_recursive_template_diagnostic_log
    TIMEOUT 60)
rex_lock_test_for_input(
  Cxx_tests_rex_test2026_recursive_template_diagnostic_stop
  rex_test2026_recursive_template_diagnostic_stop.cpp)

add_test(
  NAME Cxx_tests_rex_test2026_recursive_template_diagnostic_phase_barrier
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "! grep -Fq 'clang_main.translation_unit.begin' rex_test2026_recursive_template_diagnostic_stop.log && ! grep -Fq 'clang_main.finishSageAST.begin' rex_test2026_recursive_template_diagnostic_stop.log")
set_tests_properties(
  Cxx_tests_rex_test2026_recursive_template_diagnostic_phase_barrier
  PROPERTIES
    FIXTURES_REQUIRED rex_recursive_template_diagnostic_log
    LABELS "Cxx_tests;death")

# Frontend and backend language semantics are identical: disabling RTTI must
# reject a source that requires dynamic_cast.
rex_add_clang_frontend_negative_test(
  Cxx_tests_rex_test2026_issue264_rtti_disabled
  Cxx_tests
  ${CMAKE_CURRENT_BINARY_DIR}
  rex_test2026_issue264_rtti_disabled.frontend-negative.log
  "rex_test2026_issue264_rtti_dynamic_cast.cpp:10:22: error: use of dynamic_cast requires -frtti"
  $<TARGET_FILE:${translator}>
  ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
  -I${CMAKE_CURRENT_SOURCE_DIR}
  -fno-rtti
  -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_issue264_rtti_dynamic_cast.cpp)
rex_lock_test_for_input(
  Cxx_tests_rex_test2026_issue264_rtti_disabled
  rex_test2026_issue264_rtti_dynamic_cast.cpp)

# Disabling exceptions must likewise be enforced by the frontend before any
# Sage AST is published.
rex_add_clang_frontend_negative_test(
  Cxx_tests_rex_test2026_issue317_exceptions_disabled
  Cxx_tests
  ${CMAKE_CURRENT_BINARY_DIR}
  rex_test2026_issue317_exceptions_disabled.frontend-negative.log
  "rex_test2026_issue317_rtti_exceptions.cpp:22:5: error: cannot use 'throw' with exceptions disabled"
  $<TARGET_FILE:${translator}>
  ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
  -I${CMAKE_CURRENT_SOURCE_DIR}
  -fno-exceptions
  -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_issue317_rtti_exceptions.cpp)
rex_lock_test_for_input(
  Cxx_tests_rex_test2026_issue317_exceptions_disabled
  rex_test2026_issue317_rtti_exceptions.cpp)

# Removed frontend-compatibility switches are hard API errors.  A generic
# nonzero expectation would let an unrelated parser failure satisfy this
# contract, so require the exact invariant and SIGABRT status.
foreach(_removed_clang_option IN ITEMS
    -rex:clang:disable-access-control
    -rex:clang:delayed-template-parsing
    -rex:clang:respect-rtti-flags)
  string(REPLACE "-rex:clang:" "" _removed_clang_option_key
    "${_removed_clang_option}")
  string(REPLACE "-" "_" _removed_clang_option_key
    "${_removed_clang_option_key}")
  set(_removed_clang_option_test
    Cxx_tests_rex_removed_clang_option_${_removed_clang_option_key})
  set(_removed_clang_option_log
    ${_removed_clang_option_test}.log)
  set(_removed_clang_option_diagnostic
    "REX_FRONTEND_INVARIANT[removed-clang-option]: option '${_removed_clang_option}' was an inexact compatibility API and is not supported")
  add_test(
    NAME ${_removed_clang_option_test}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"$1\"; expected=\"$2\"; shift 2; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc -- \"$expected\" \"$log\")\" -eq 1"
      dummy
      ${_removed_clang_option_log}
      "${_removed_clang_option_diagnostic}"
      $<TARGET_FILE:${translator}>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      ${_removed_clang_option}
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_issue264_rtti_dynamic_cast.cpp)
  set_tests_properties(${_removed_clang_option_test}
    PROPERTIES LABELS "Cxx_tests;death")
  rex_lock_test_for_input(${_removed_clang_option_test}
    rex_test2026_issue264_rtti_dynamic_cast.cpp)
endforeach()

set(_removed_openmp_version_option -fopenmp-version=60)
set(_removed_openmp_version_option_test
  Cxx_tests_rex_removed_openmp_version_option)
set(_removed_openmp_version_option_log
  ${_removed_openmp_version_option_test}.log)
set(_removed_openmp_version_option_diagnostic
  "REX_FRONTEND_INVARIANT[removed-openmp-version-option]: option '${_removed_openmp_version_option}' was part of the removed frontend-owned OpenMP design and is not supported")
add_test(
  NAME ${_removed_openmp_version_option_test}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=\"$1\"; expected=\"$2\"; shift 2; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc -- \"$expected\" \"$log\")\" -eq 1"
    dummy
    ${_removed_openmp_version_option_log}
    "${_removed_openmp_version_option_diagnostic}"
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    ${_removed_openmp_version_option}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_issue264_rtti_dynamic_cast.cpp)
set_tests_properties(${_removed_openmp_version_option_test}
  PROPERTIES LABELS "Cxx_tests;death")
rex_lock_test_for_input(${_removed_openmp_version_option_test}
  rex_test2026_issue264_rtti_dynamic_cast.cpp)
unset(_removed_openmp_version_option)
unset(_removed_openmp_version_option_diagnostic)
unset(_removed_openmp_version_option_log)
unset(_removed_openmp_version_option_test)

set(_removed_rose_language_options
  -rose:c
  -rose:C
  -rose:C_only
  -rose:C89
  -rose:C89_only
  -rose:C99
  -rose:C99_only
  -rose:C11
  -rose:C11_only
  -rose:C17
  -rose:C17_only
  -rose:C23
  -rose:C23_only
  -rose:C2y
  -rose:C2y_only
  -rose:cxx
  -rose:Cxx
  -rose:Cxx_only
  -rose:Cxx0x
  -rose:Cxx0x_only
  -rose:Cxx11
  -rose:Cxx11_only
  -rose:Cxx14
  -rose:Cxx14_only
  -rose:Cxx17
  -rose:Cxx17_only
  -rose:Cxx20
  -rose:Cxx20_only
  -rose:Cxx23
  -rose:Cxx23_only
  -rose:Cxx26
  -rose:Cxx26_only
  --rose:C
  -std=c
  -std=gnu
  -std=c++
  -std=gnu++)
add_test(
  NAME Cxx_tests_rex_removed_rose_language_options
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; translator=\"$1\"; source=\"$2\"; shift 2; for option in \"$@\"; do key=\${option//[^A-Za-z0-9]/_}; log=rex_removed_rose_language_option_$key.log; expected=\"REX_FRONTEND_INVARIANT[removed-language-option]: option '$option' was a deprecated language-selection API; use the source extension, -x, and -std= explicitly\"; set +e; \"$translator\" -rose:skipfinalCompileStep \"$option\" -c \"$source\" >\"$log\" 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc -- \"$expected\" \"$log\")\" -eq 1; done"
    dummy
    $<TARGET_FILE:${translator}>
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_issue264_rtti_dynamic_cast.cpp
    ${_removed_rose_language_options})
set_tests_properties(Cxx_tests_rex_removed_rose_language_options
  PROPERTIES LABELS "Cxx_tests;death")
rex_lock_test_for_input(Cxx_tests_rex_removed_rose_language_options
  rex_test2026_issue264_rtti_dynamic_cast.cpp)

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

# Defaults first introduced on a redeclaration/definition must not be stripped
# as inherited defaults.
add_test(
  NAME Cxx_tests_rex_test2026_redecl_default_template_arg_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E \"template[[:space:]]*<[[:space:]]*(class|typename)[[:space:]]+T[[:space:]]*=[[:space:]]*int\" rose_rex_test2026_redecl_default_template_arg.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2026_redecl_default_template_arg.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_redecl_default_template_arg.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2026_redecl_default_template_arg_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2026_redecl_default_template_arg_unparse
  rex_test2026_redecl_default_template_arg.cpp)

# A definition following a declaration with a default template argument must not
# re-emit the inherited default.
add_test(
  NAME Cxx_tests_rex_test2026_template_definition_prior_default_unparse
  COMMAND
    sh -c
    "\"$@\" && awk '/Iter[[:space:]]*=[[:space:]]*T[[:space:]]*\\*/ { ++n } END { exit !(n == 1) }' rose_rex_test2026_template_definition_prior_default.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2026_template_definition_prior_default.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_template_definition_prior_default.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2026_template_definition_prior_default_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2026_template_definition_prior_default_unparse
  rex_test2026_template_definition_prior_default.cpp)

# Replacing redeclared definition parameters must also refresh template-param
# lookups before translating requires clauses and dependent members.
add_test(
  NAME Cxx_tests_rex_test2026_redecl_default_template_arg_requires_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E \"template[[:space:]]*<[[:space:]]*(class|typename)[[:space:]]+T[[:space:]]*=[[:space:]]*int\" rose_rex_test2026_redecl_default_template_arg_requires.cpp && grep -E \"requires[[:space:]]*\\(\" rose_rex_test2026_redecl_default_template_arg_requires.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2026_redecl_default_template_arg_requires.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -std=c++20
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_redecl_default_template_arg_requires.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2026_redecl_default_template_arg_requires_unparse
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_test2026_redecl_default_template_arg_requires_unparse
  rex_test2026_redecl_default_template_arg_requires.cpp)

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
# Keep this explicit because it exercises additional post-translation checks.
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

add_test(
  NAME Cxx_tests_rex_frontend_static_assert_typed_message_exact_surface
  COMMAND
    sh -c
    "\"$@\" && grep -F 'static_assert(sizeof(int) >= 2);' rose_rex_frontend_static_assert_typed_message.cpp && grep -F 'static_assert(true, u8\"utf8 message\");' rose_rex_frontend_static_assert_typed_message.cpp && grep -F 'static_assert(true, \"left right\");' rose_rex_frontend_static_assert_typed_message.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_frontend_static_assert_typed_message.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_static_assert_typed_message.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_static_assert_typed_message_exact_surface
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_static_assert_typed_message_exact_surface
  rex_frontend_static_assert_typed_message.cpp)

add_test(
  NAME Cxx_tests_rex_frontend_scalar_value_init_exact_surface
  COMMAND
    sh -c
    "\"$@\" && grep -F 'return int();' rose_rex_frontend_scalar_value_init.cpp && grep -F 'return RexPointer();' rose_rex_frontend_scalar_value_init.cpp && grep -F 'return T();' rose_rex_frontend_scalar_value_init.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_frontend_scalar_value_init.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_scalar_value_init.cpp
)
set_tests_properties(Cxx_tests_rex_frontend_scalar_value_init_exact_surface
  PROPERTIES LABELS Cxx_tests
)
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_scalar_value_init_exact_surface
  rex_frontend_scalar_value_init.cpp)

# A semantic function-signature traversal may reach a default-argument
# expression before the source declaration that spells it.  The semantic
# translation must never populate the lexical expression cache used by that
# later source surface.
add_test(
  NAME Cxx_tests_rex_frontend_semantic_parameter_default_cache_role
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_semantic_parameter_default_cache_role.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_semantic_parameter_default_cache_role
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_semantic_parameter_default_cache_role
  rex_frontend_semantic_parameter_default_cache_role.cpp)

set_tests_properties(
  Cxx_tests_rex_test2025_issue160_axpy_roundtrip_unparse
  Cxx_tests_rex_test2025_issue163_axpy_roundtrip_unparse
  Cxx_tests_rex_test2025_issue164_constexpr_const_unparse
  Cxx_tests_rex_test2025_issue164_constexpr_const_token_roundtrip
  Cxx_tests_rex_test2025_issue160_axpy_system_headers_unparse
  Cxx_tests_rex_test2025_issue160_explicit_struct_instantiation_unparse
  Cxx_tests_rex_test2025_issue160_explicit_class_instantiation_unparse
  Cxx_tests_rex_test2025_issue160_extern_class_instantiation_unparse
  Cxx_tests_${_issue160_system_header_class_instantiation_base}_unparse
  Cxx_tests_rex_test2025_issue113_instantiation_spelling_unparse
  Cxx_tests_rex_test2026_token_unparse_contract_rejects_incomplete_map
  Cxx_tests_rex_test2025_extern_c_roundtrip_unparse
  Cxx_tests_rex_test2025_extern_c_brace_roundtrip_unparse
  Cxx_tests_rex_test2025_issue123_std_double_colon_unparse
  Cxx_tests_rex_test2025_issue146_std_iota_quad_colon_unparse
  Cxx_tests_rex_test2025_issue146_shadowed_function_template_unparse
  Cxx_tests_rex_test2026_issue264_rtti_disabled
  Cxx_tests_rex_test2026_issue317_exceptions_disabled
  Cxx_tests_rex_removed_clang_option_disable_access_control
  Cxx_tests_rex_removed_clang_option_delayed_template_parsing
  Cxx_tests_rex_removed_clang_option_respect_rtti_flags
  Cxx_tests_rex_removed_rose_language_options
  Cxx_tests_rex_test2025_issue124_system_header_specialization_args_unparse
  Cxx_tests_rex_test2025_issue126_default_template_args_unparse
  Cxx_tests_rex_test2026_redecl_default_template_arg_unparse
  Cxx_tests_rex_test2026_template_definition_prior_default_unparse
  Cxx_tests_rex_test2026_redecl_default_template_arg_requires_unparse
  Cxx_tests_rex_test2025_issue59_template_template_arg_global_qual_unparse
  Cxx_tests_rex_test2025_issue60_friend_function_symbol_scope_unparse
  Cxx_tests_rex_test2025_issue64_shadow_template_global_qual_unparse
  Cxx_tests_rex_test2025_namespace_reopenings_preserve_blocks_unparse
  Cxx_tests_rex_test2025_issue93_static_assert_order_unparse
  Cxx_tests_rex_test2025_issue94_template_arg_spacing_unparse
  Cxx_tests_rex_test2025_defaulted_special_members_anon_union_unparse
  Cxx_tests_rex_test2025_template_call_explicit_args
  Cxx_tests_rex_test2025_member_template_call_explicit_args
  Cxx_tests_rex_test2025_variadic_template_params_unparse
  Cxx_tests_rex_test2025_template_argument_pack_unparse
  Cxx_tests_rex_test2025_issue99_failure_propagation
  Cxx_tests_rex_test2025_issue87_namespace
  Cxx_tests_rex_test2025_issue89_type_alias_template_args
  Cxx_tests_rex_test2025_issue90_array_dim_template_param
  Cxx_tests_rex_test2026_concepts_unparse
  Cxx_tests_rex_test2026_dependent_qualification_unparse
  Cxx_tests_rex_frontend_scalar_value_init_exact_surface
  PROPERTIES WORKING_DIRECTORY ${_cxx_unparse_output_dir}
)

# Clang semantic declaration wrappers have no independent source spelling.
# The CapturedStmt remains the exact source owner and explicitly validates its
# no-surface CapturedDecl before translating the body.
add_test(
  NAME Cxx_tests_rex_frontend_captured_decl_no_surface
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_captured_decl_no_surface.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_captured_decl_no_surface
  PROPERTIES LABELS "Cxx_tests;rex;CFE")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_captured_decl_no_surface
  rex_frontend_captured_decl_no_surface.cpp)

# Clang's ExternCContextDecl is lookup-only.  Ordinary source linkage
# declarations must continue to use their LinkageSpecDecl surface and roundtrip
# without publishing the semantic lookup context.
set(_rex_extern_c_semantic_context_output
  "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_extern_c_semantic_context.output.cpp")
add_test(
  NAME Cxx_tests_rex_frontend_extern_c_semantic_context
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "output=\"$1\"; shift; \"$@\" && '${CMAKE_CXX_COMPILER}' -std=c++17 -fsyntax-only \"$output\""
    dummy
    "${_rex_extern_c_semantic_context_output}"
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output "${_rex_extern_c_semantic_context_output}"
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_extern_c_semantic_context.cpp
)
set_tests_properties(
  Cxx_tests_rex_frontend_extern_c_semantic_context
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_extern_c_semantic_context
  rex_frontend_extern_c_semantic_context.cpp)

# A dependent friend template-id retains the complete exact overload set until
# substitution resolves its dependent parameter types.  No candidate may be
# selected merely by declaration order.
add_executable(
  rex_frontend_dependent_friend_candidate_set_translator
  rex_frontend_dependent_friend_candidate_set_translator.cpp)
target_link_libraries(
  rex_frontend_dependent_friend_candidate_set_translator
  ROSE_DLL
  ${link_with_libraries})
add_test(
  NAME Cxx_tests_rex_frontend_dependent_friend_candidate_set
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_dependent_friend_candidate_set_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_dependent_friend_candidate_set.cpp)
set_tests_properties(
  Cxx_tests_rex_frontend_dependent_friend_candidate_set
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_dependent_friend_candidate_set
  rex_frontend_dependent_friend_candidate_set.cpp)

set(_rex_dependent_friend_candidate_json_dir
  ${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_dependent_friend_candidate_set_json)
set(_rex_dependent_friend_candidate_json_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_dependent_friend_candidate_set_json.cpp)
add_test(
  NAME Cxx_tests_rex_frontend_dependent_friend_candidate_set_ast_json
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    ${BASH_EXECUTABLE} -c
    "\"$@\" && '${CMAKE_CXX_COMPILER}' -std=c++17 -fsyntax-only '${_rex_dependent_friend_candidate_json_output}'"
    dummy
    $<TARGET_FILE:rex_frontend_dependent_friend_candidate_set_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rex:ast-json-checkpoint=pre-omp-construction
    -rex:ast-json-dir=${_rex_dependent_friend_candidate_json_dir}
    -rose:output ${_rex_dependent_friend_candidate_json_output}
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_dependent_friend_candidate_set.cpp)
set_tests_properties(
  Cxx_tests_rex_frontend_dependent_friend_candidate_set_ast_json
  PROPERTIES LABELS "Cxx_tests;ASTJSON;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_dependent_friend_candidate_set_ast_json
  rex_frontend_dependent_friend_candidate_set.cpp)

# An unqualified injected class name in a pointer-to-member declarator must
# retain its bare current-instantiation spelling independently of the semantic
# class specialization and across AST JSON restoration.
add_executable(
  rex_frontend_injected_member_pointer_owner_translator
  rex_frontend_injected_member_pointer_owner_translator.cpp)
target_link_libraries(
  rex_frontend_injected_member_pointer_owner_translator
  ROSE_DLL
  ${link_with_libraries})

set(_rex_injected_member_pointer_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_injected_member_pointer_owner.cpp)
add_test(
  NAME Cxx_tests_rex_frontend_injected_member_pointer_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && grep -E 'rex_injected_member_pointer_owner[[:space:]]*::[[:space:]]*\\*[[:space:]]*pointer' '${_rex_injected_member_pointer_output}' && ! grep -E '[[:alnum:]_]::rex_injected_member_pointer_owner[[:space:]]*::[[:space:]]*\\*' '${_rex_injected_member_pointer_output}' && '${CMAKE_CXX_COMPILER}' -std=c++17 -fsyntax-only '${_rex_injected_member_pointer_output}'"
    dummy
    $<TARGET_FILE:rex_frontend_injected_member_pointer_owner_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rose:output ${_rex_injected_member_pointer_output}
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_injected_member_pointer_owner.cpp)
set_tests_properties(
  Cxx_tests_rex_frontend_injected_member_pointer_owner
  PROPERTIES LABELS "Cxx_tests;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_injected_member_pointer_owner
  rex_frontend_injected_member_pointer_owner.cpp)

set(_rex_injected_member_pointer_json_dir
  ${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_injected_member_pointer_owner_json)
set(_rex_injected_member_pointer_json_output
  ${CMAKE_CURRENT_BINARY_DIR}/rose_rex_frontend_injected_member_pointer_owner_json.cpp)
add_test(
  NAME Cxx_tests_rex_frontend_injected_member_pointer_owner_ast_json
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && grep -E 'rex_injected_member_pointer_owner[[:space:]]*::[[:space:]]*\\*[[:space:]]*pointer' '${_rex_injected_member_pointer_json_output}' && ! grep -E '[[:alnum:]_]::rex_injected_member_pointer_owner[[:space:]]*::[[:space:]]*\\*' '${_rex_injected_member_pointer_json_output}' && '${CMAKE_CXX_COMPILER}' -std=c++17 -fsyntax-only '${_rex_injected_member_pointer_json_output}'"
    dummy
    $<TARGET_FILE:rex_frontend_injected_member_pointer_owner_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rex:ast-json-checkpoint=pre-omp-construction
    -rex:ast-json-dir=${_rex_injected_member_pointer_json_dir}
    -rose:output ${_rex_injected_member_pointer_json_output}
    ${ROSE_INCLUDE_FLAGS}
    -std=c++17
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_injected_member_pointer_owner.cpp)
set_tests_properties(
  Cxx_tests_rex_frontend_injected_member_pointer_owner_ast_json
  PROPERTIES LABELS "Cxx_tests;ASTJSON;rex;CFE;UNPARSER")
rex_lock_test_for_input(
  Cxx_tests_rex_frontend_injected_member_pointer_owner_ast_json
  rex_frontend_injected_member_pointer_owner.cpp)

function(rex_add_unsupported_clang_surface_death_test
         test_name input_file expected_diagnostic)
  add_test(
    NAME ${test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"$1\"; expected=\"$2\"; shift 2; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc -- \"$expected\" \"$log\")\" -eq 1"
      dummy
      "${test_name}.log"
      "${expected_diagnostic}"
      $<TARGET_FILE:${translator}>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
      ${ROSE_INCLUDE_FLAGS}
      ${ARGN}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file}
  )
  set_tests_properties(
    ${test_name}
    PROPERTIES LABELS "Cxx_tests;rex;CFE;DEATH")
  rex_lock_test_for_input(${test_name} ${input_file})
endfunction()

rex_add_unsupported_clang_surface_death_test(
  Cxx_tests_rex_frontend_block_expression_rejected
  rex_frontend_block_expression_rejected.cpp
  "REX_FRONTEND_INVARIANT[unsupported-source-expression]: Clang BlockExpr has no lossless typed Sage representation"
  -std=c++17 -fblocks)

rex_add_unsupported_clang_surface_death_test(
  Cxx_tests_rex_frontend_pragma_comment_rejected
  rex_frontend_pragma_comment_rejected.cpp
  "REX_FRONTEND_INVARIANT[unsupported-source-declaration]: Clang PragmaComment has no lossless typed Sage representation"
  -std=c++17 -fms-extensions)

rex_add_unsupported_clang_surface_death_test(
  Cxx_tests_rex_frontend_pragma_detect_mismatch_rejected
  rex_frontend_pragma_detect_mismatch_rejected.cpp
  "REX_FRONTEND_INVARIANT[unsupported-source-declaration]: Clang PragmaDetectMismatch has no lossless typed Sage representation"
  -std=c++17 -fms-extensions)
