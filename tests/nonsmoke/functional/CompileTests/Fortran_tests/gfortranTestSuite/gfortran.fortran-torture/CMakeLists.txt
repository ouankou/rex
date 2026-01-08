set(F90_TESTCODES
  actual.f90 allocate.f90 bergervoet2.f90 complex_1.f90 contained_1.f90
  contained_2.f90 contained_3.f90 contained_4.f90 contained_5.f90 convert.f90
  defined_type_2.f90 dummyfn.f90 empty.f90 empty_interface_1.f90 emptyif-1.f90
  emptyif.f90 fnresvar.f90 gen_interf.f90 implicit_2.f90 inline_1.f90
  inquiry_1.f90 io_end.f90 mloc.f90 module_common.f90 module_expr.f90
  module_proc.f90 module_result.f90 name_clash.f90 named_args.f90
  named_args_2.f90 nested.f90 parameter_1.f90 parameter_3.f90 pr26806.f90
  pr32417.f90 pr41654.f90 pr45634.f90 shape_reshape.f90 strparm_1.f90
  transfer-1.f90 write.f90 20080806-1.f90 ambig.f90 defined_type_1.f90
  defined_type_3.f90 implicit.f90 implicit_1.f90 logical-1.f90 pr30147.f90
  pr36078.f90 arrayio.f90 pr33276.f90 pr40413.f90 pr42781.f90 pr45598.f90
  vrp_1.f90)

set(F77_FIXED_FORMAT_TESTCODES pr32583.f noncontinuation_1.f)

set(ROSE_FLAGS -rose:verbose 0 -rose:skip_syntax_check
               -rose:skipfinalCompileStep -rose:detect_dangling_pointers 1)

foreach(file_to_test ${F90_TESTCODES})
  add_test(
    NAME GFORTRANSUITE_${file_to_test}
    COMMAND testTranslator ${ROSE_FLAGS} -rose:f90 -c
            ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(GFORTRANSUITE_${file_to_test} PROPERTIES LABELS GFORTRANSUITE)
endforeach()

foreach(file_to_test ${F77_FIXED_FORMAT_TESTCODES})
  add_test(
    NAME GFORTRANSUITE_${file_to_test}
    COMMAND testTranslator ${ROSE_FLAGS} -rose:f77 -c
            ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(GFORTRANSUITE_${file_to_test} PROPERTIES LABELS GFORTRANSUITE)
endforeach()

set(_gfortran_torture_disabled_f90
  data_1.f90
  do_1.f90
  enum_1.f90
  forall-1.f90
  parameter_2.f90
  pr40421.f90
)

foreach(file_to_test ${_gfortran_torture_disabled_f90})
  add_test(
    NAME GFORTRANSUITE_${file_to_test}
    COMMAND testTranslator ${ROSE_FLAGS} -rose:f90 -c
            ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(GFORTRANSUITE_${file_to_test}
    PROPERTIES DISABLED TRUE LABELS GFORTRANSUITE)
endforeach()

set(_gfortran_torture_disabled_f77
  pr32663.f
  pr39937.f
  pr40421.f
)

foreach(file_to_test ${_gfortran_torture_disabled_f77})
  add_test(
    NAME GFORTRANSUITE_${file_to_test}
    COMMAND testTranslator ${ROSE_FLAGS} -rose:f77 -c
            ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(GFORTRANSUITE_${file_to_test}
    PROPERTIES DISABLED TRUE LABELS GFORTRANSUITE)
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_Fortran_tests_gfortranTestSuite_gfortran.fortran-torture_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=GFORTRANSUITE_20080806-1.f90\;GFORTRANSUITE_actual.f90\;GFORTRANSUITE_allocate.f90\;GFORTRANSUITE_ambig.f90\;GFORTRANSUITE_arrayio.f90\;GFORTRANSUITE_bergervoet2.f90\;GFORTRANSUITE_complex_1.f90\;GFORTRANSUITE_contained_1.f90\;GFORTRANSUITE_contained_2.f90\;GFORTRANSUITE_contained_3.f90\;GFORTRANSUITE_contained_4.f90\;GFORTRANSUITE_contained_5.f90\;GFORTRANSUITE_convert.f90\;GFORTRANSUITE_data_1.f90\;GFORTRANSUITE_defined_type_1.f90\;GFORTRANSUITE_defined_type_2.f90\;GFORTRANSUITE_defined_type_3.f90\;GFORTRANSUITE_do_1.f90\;GFORTRANSUITE_dummyfn.f90\;GFORTRANSUITE_empty.f90\;GFORTRANSUITE_empty_interface_1.f90\;GFORTRANSUITE_emptyif-1.f90\;GFORTRANSUITE_emptyif.f90\;GFORTRANSUITE_enum_1.f90\;GFORTRANSUITE_fnresvar.f90\;GFORTRANSUITE_forall-1.f90\;GFORTRANSUITE_gen_interf.f90\;GFORTRANSUITE_implicit.f90\;GFORTRANSUITE_implicit_1.f90\;GFORTRANSUITE_implicit_2.f90\;GFORTRANSUITE_inline_1.f90\;GFORTRANSUITE_inquiry_1.f90\;GFORTRANSUITE_io_end.f90\;GFORTRANSUITE_logical-1.f90\;GFORTRANSUITE_mloc.f90\;GFORTRANSUITE_module_common.f90\;GFORTRANSUITE_module_expr.f90\;GFORTRANSUITE_module_proc.f90\;GFORTRANSUITE_module_result.f90\;GFORTRANSUITE_name_clash.f90\;GFORTRANSUITE_named_args.f90\;GFORTRANSUITE_named_args_2.f90\;GFORTRANSUITE_nested.f90\;GFORTRANSUITE_noncontinuation_1.f\;GFORTRANSUITE_parameter_1.f90\;GFORTRANSUITE_parameter_2.f90\;GFORTRANSUITE_parameter_3.f90\;GFORTRANSUITE_pr26806.f90\;GFORTRANSUITE_pr30147.f90\;GFORTRANSUITE_pr32417.f90\;GFORTRANSUITE_pr32583.f\;GFORTRANSUITE_pr32663.f\;GFORTRANSUITE_pr33276.f90\;GFORTRANSUITE_pr36078.f90\;GFORTRANSUITE_pr39937.f\;GFORTRANSUITE_pr40413.f90\;GFORTRANSUITE_pr40421.f\;GFORTRANSUITE_pr40421.f90\;GFORTRANSUITE_pr41654.f90\;GFORTRANSUITE_pr42781.f90\;GFORTRANSUITE_pr45598.f90\;GFORTRANSUITE_pr45634.f90\;GFORTRANSUITE_shape_reshape.f90\;GFORTRANSUITE_strparm_1.f90\;GFORTRANSUITE_transfer-1.f90\;GFORTRANSUITE_vrp_1.f90\;GFORTRANSUITE_write.f90
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_Fortran_tests_gfortranTestSuite_gfortran.fortran-torture_check-local PROPERTIES
  DEPENDS "GFORTRANSUITE_20080806-1.f90;GFORTRANSUITE_actual.f90;GFORTRANSUITE_allocate.f90;GFORTRANSUITE_ambig.f90;GFORTRANSUITE_arrayio.f90;GFORTRANSUITE_bergervoet2.f90;GFORTRANSUITE_complex_1.f90;GFORTRANSUITE_contained_1.f90;GFORTRANSUITE_contained_2.f90;GFORTRANSUITE_contained_3.f90;GFORTRANSUITE_contained_4.f90;GFORTRANSUITE_contained_5.f90;GFORTRANSUITE_convert.f90;GFORTRANSUITE_data_1.f90;GFORTRANSUITE_defined_type_1.f90;GFORTRANSUITE_defined_type_2.f90;GFORTRANSUITE_defined_type_3.f90;GFORTRANSUITE_do_1.f90;GFORTRANSUITE_dummyfn.f90;GFORTRANSUITE_empty.f90;GFORTRANSUITE_empty_interface_1.f90;GFORTRANSUITE_emptyif-1.f90;GFORTRANSUITE_emptyif.f90;GFORTRANSUITE_enum_1.f90;GFORTRANSUITE_fnresvar.f90;GFORTRANSUITE_forall-1.f90;GFORTRANSUITE_gen_interf.f90;GFORTRANSUITE_implicit.f90;GFORTRANSUITE_implicit_1.f90;GFORTRANSUITE_implicit_2.f90;GFORTRANSUITE_inline_1.f90;GFORTRANSUITE_inquiry_1.f90;GFORTRANSUITE_io_end.f90;GFORTRANSUITE_logical-1.f90;GFORTRANSUITE_mloc.f90;GFORTRANSUITE_module_common.f90;GFORTRANSUITE_module_expr.f90;GFORTRANSUITE_module_proc.f90;GFORTRANSUITE_module_result.f90;GFORTRANSUITE_name_clash.f90;GFORTRANSUITE_named_args.f90;GFORTRANSUITE_named_args_2.f90;GFORTRANSUITE_nested.f90;GFORTRANSUITE_noncontinuation_1.f;GFORTRANSUITE_parameter_1.f90;GFORTRANSUITE_parameter_2.f90;GFORTRANSUITE_parameter_3.f90;GFORTRANSUITE_pr26806.f90;GFORTRANSUITE_pr30147.f90;GFORTRANSUITE_pr32417.f90;GFORTRANSUITE_pr32583.f;GFORTRANSUITE_pr32663.f;GFORTRANSUITE_pr33276.f90;GFORTRANSUITE_pr36078.f90;GFORTRANSUITE_pr39937.f;GFORTRANSUITE_pr40413.f90;GFORTRANSUITE_pr40421.f;GFORTRANSUITE_pr40421.f90;GFORTRANSUITE_pr41654.f90;GFORTRANSUITE_pr42781.f90;GFORTRANSUITE_pr45598.f90;GFORTRANSUITE_pr45634.f90;GFORTRANSUITE_shape_reshape.f90;GFORTRANSUITE_strparm_1.f90;GFORTRANSUITE_transfer-1.f90;GFORTRANSUITE_vrp_1.f90;GFORTRANSUITE_write.f90"
)
add_test(
  NAME nonsmoke_functional_CompileTests_Fortran_tests_gfortranTestSuite_gfortran.fortran-torture_conditional-check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=GFORTRANSUITE_20080806-1.f90\;GFORTRANSUITE_actual.f90\;GFORTRANSUITE_allocate.f90\;GFORTRANSUITE_ambig.f90\;GFORTRANSUITE_arrayio.f90\;GFORTRANSUITE_bergervoet2.f90\;GFORTRANSUITE_complex_1.f90\;GFORTRANSUITE_contained_1.f90\;GFORTRANSUITE_contained_2.f90\;GFORTRANSUITE_contained_3.f90\;GFORTRANSUITE_contained_4.f90\;GFORTRANSUITE_contained_5.f90\;GFORTRANSUITE_convert.f90\;GFORTRANSUITE_data_1.f90\;GFORTRANSUITE_defined_type_1.f90\;GFORTRANSUITE_defined_type_2.f90\;GFORTRANSUITE_defined_type_3.f90\;GFORTRANSUITE_do_1.f90\;GFORTRANSUITE_dummyfn.f90\;GFORTRANSUITE_empty.f90\;GFORTRANSUITE_empty_interface_1.f90\;GFORTRANSUITE_emptyif-1.f90\;GFORTRANSUITE_emptyif.f90\;GFORTRANSUITE_enum_1.f90\;GFORTRANSUITE_fnresvar.f90\;GFORTRANSUITE_forall-1.f90\;GFORTRANSUITE_gen_interf.f90\;GFORTRANSUITE_implicit.f90\;GFORTRANSUITE_implicit_1.f90\;GFORTRANSUITE_implicit_2.f90\;GFORTRANSUITE_inline_1.f90\;GFORTRANSUITE_inquiry_1.f90\;GFORTRANSUITE_io_end.f90\;GFORTRANSUITE_logical-1.f90\;GFORTRANSUITE_mloc.f90\;GFORTRANSUITE_module_common.f90\;GFORTRANSUITE_module_expr.f90\;GFORTRANSUITE_module_proc.f90\;GFORTRANSUITE_module_result.f90\;GFORTRANSUITE_name_clash.f90\;GFORTRANSUITE_named_args.f90\;GFORTRANSUITE_named_args_2.f90\;GFORTRANSUITE_nested.f90\;GFORTRANSUITE_noncontinuation_1.f\;GFORTRANSUITE_parameter_1.f90\;GFORTRANSUITE_parameter_2.f90\;GFORTRANSUITE_parameter_3.f90\;GFORTRANSUITE_pr26806.f90\;GFORTRANSUITE_pr30147.f90\;GFORTRANSUITE_pr32417.f90\;GFORTRANSUITE_pr32583.f\;GFORTRANSUITE_pr32663.f\;GFORTRANSUITE_pr33276.f90\;GFORTRANSUITE_pr36078.f90\;GFORTRANSUITE_pr39937.f\;GFORTRANSUITE_pr40413.f90\;GFORTRANSUITE_pr40421.f\;GFORTRANSUITE_pr40421.f90\;GFORTRANSUITE_pr41654.f90\;GFORTRANSUITE_pr42781.f90\;GFORTRANSUITE_pr45598.f90\;GFORTRANSUITE_pr45634.f90\;GFORTRANSUITE_shape_reshape.f90\;GFORTRANSUITE_strparm_1.f90\;GFORTRANSUITE_transfer-1.f90\;GFORTRANSUITE_vrp_1.f90\;GFORTRANSUITE_write.f90
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_Fortran_tests_gfortranTestSuite_gfortran.fortran-torture_conditional-check-local PROPERTIES
  DEPENDS "GFORTRANSUITE_20080806-1.f90;GFORTRANSUITE_actual.f90;GFORTRANSUITE_allocate.f90;GFORTRANSUITE_ambig.f90;GFORTRANSUITE_arrayio.f90;GFORTRANSUITE_bergervoet2.f90;GFORTRANSUITE_complex_1.f90;GFORTRANSUITE_contained_1.f90;GFORTRANSUITE_contained_2.f90;GFORTRANSUITE_contained_3.f90;GFORTRANSUITE_contained_4.f90;GFORTRANSUITE_contained_5.f90;GFORTRANSUITE_convert.f90;GFORTRANSUITE_data_1.f90;GFORTRANSUITE_defined_type_1.f90;GFORTRANSUITE_defined_type_2.f90;GFORTRANSUITE_defined_type_3.f90;GFORTRANSUITE_do_1.f90;GFORTRANSUITE_dummyfn.f90;GFORTRANSUITE_empty.f90;GFORTRANSUITE_empty_interface_1.f90;GFORTRANSUITE_emptyif-1.f90;GFORTRANSUITE_emptyif.f90;GFORTRANSUITE_enum_1.f90;GFORTRANSUITE_fnresvar.f90;GFORTRANSUITE_forall-1.f90;GFORTRANSUITE_gen_interf.f90;GFORTRANSUITE_implicit.f90;GFORTRANSUITE_implicit_1.f90;GFORTRANSUITE_implicit_2.f90;GFORTRANSUITE_inline_1.f90;GFORTRANSUITE_inquiry_1.f90;GFORTRANSUITE_io_end.f90;GFORTRANSUITE_logical-1.f90;GFORTRANSUITE_mloc.f90;GFORTRANSUITE_module_common.f90;GFORTRANSUITE_module_expr.f90;GFORTRANSUITE_module_proc.f90;GFORTRANSUITE_module_result.f90;GFORTRANSUITE_name_clash.f90;GFORTRANSUITE_named_args.f90;GFORTRANSUITE_named_args_2.f90;GFORTRANSUITE_nested.f90;GFORTRANSUITE_noncontinuation_1.f;GFORTRANSUITE_parameter_1.f90;GFORTRANSUITE_parameter_2.f90;GFORTRANSUITE_parameter_3.f90;GFORTRANSUITE_pr26806.f90;GFORTRANSUITE_pr30147.f90;GFORTRANSUITE_pr32417.f90;GFORTRANSUITE_pr32583.f;GFORTRANSUITE_pr32663.f;GFORTRANSUITE_pr33276.f90;GFORTRANSUITE_pr36078.f90;GFORTRANSUITE_pr39937.f;GFORTRANSUITE_pr40413.f90;GFORTRANSUITE_pr40421.f;GFORTRANSUITE_pr40421.f90;GFORTRANSUITE_pr41654.f90;GFORTRANSUITE_pr42781.f90;GFORTRANSUITE_pr45598.f90;GFORTRANSUITE_pr45634.f90;GFORTRANSUITE_shape_reshape.f90;GFORTRANSUITE_strparm_1.f90;GFORTRANSUITE_transfer-1.f90;GFORTRANSUITE_vrp_1.f90;GFORTRANSUITE_write.f90"
)
