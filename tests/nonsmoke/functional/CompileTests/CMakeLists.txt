# setup ROSE_INCLUDE_FLAGS variable (used by some compile tests)
foreach(include ${ROSE_INCLUDES})
  list(APPEND ROSE_INCLUDE_FLAGS "-I${include}")
endforeach()

# default values for our compile tests
set(ROSE_FLAGS -w -rose:verbose 0)
set(translator testTranslator)

option(REX_ENABLE_FAILING_TESTS "Enable tests known to fail" OFF)

# define compile_test CMake function
function(compile_test input_file label)
  set(_test_key ${input_file})
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  set(test_id ${label}_${_test_key})

  # optional argument to omit include directories
  if(ARGV2)
    add_test(
      NAME ${test_id}
      COMMAND ${translator}
        ${ROSE_FLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file}
    )
  else()
    add_test(
      NAME ${test_id}
      COMMAND ${translator}
        ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c
        ${CMAKE_CURRENT_SOURCE_DIR}/${input_file}
    )
  endif()
  set(_labels ${label})
  set(_disable_test FALSE)
  if(DEFINED ROSE_DISABLED_TESTCODES AND input_file IN_LIST ROSE_DISABLED_TESTCODES)
    list(APPEND _labels disabled)
    if(NOT REX_ENABLE_DISABLED_TESTS)
      set(_disable_test TRUE)
    endif()
  endif()
  if(DEFINED ROSE_FAILING_TESTCODES AND input_file IN_LIST ROSE_FAILING_TESTCODES)
    list(APPEND _labels known_fail)
    if(NOT REX_ENABLE_FAILING_TESTS)
      list(APPEND _labels disabled)
      set(_disable_test TRUE)
    endif()
  endif()
  list(REMOVE_DUPLICATES _labels)
  set_tests_properties(${test_id} PROPERTIES LABELS "${_labels}")

  if(_disable_test)
    set_tests_properties(${test_id} PROPERTIES DISABLED TRUE)
  endif()
endfunction()

# determine what subdirectories to enter based on our configuration settings.

add_subdirectory(CudaTests)

# REX: Fortran tests are independent of which C/C++ compiler is used to build REX itself
# Move Fortran test registration outside the Clang-specific if/else block
if(ENABLE-FORTRAN)
  add_subdirectory(Fortran_tests)
  add_subdirectory(experimental_fortran_tests)
endif()

# REX: Enable all tests regardless of compiler (Clang frontend is now standard)
if(ENABLE-C)
  add_subdirectory(C_tests)
  add_subdirectory(C89_std_c89_tests)
  add_subdirectory(C11_tests)
  add_subdirectory(C_subset_of_Cxx_tests)
  add_subdirectory(OpenClTests)
  add_subdirectory(Cxx_tests)
  add_subdirectory(Cxx03_tests)
  add_subdirectory(Cxx11_tests)
  add_subdirectory(Cxx14_tests)
  add_subdirectory(Cxx17_tests)
  add_subdirectory(Cxx20_tests)
  add_subdirectory(C99_tests)
  add_subdirectory(OpenMP_tests)
  add_subdirectory(OpenACC_tests)

  add_subdirectory(C++Code)
  add_subdirectory(copyAST_tests)
  # ROSE-1738
  add_subdirectory(RoseExample_tests)
  add_subdirectory(unparseToString_tests)
  add_subdirectory(sourcePosition_tests)
  # requires Valgrind
  add_subdirectory(uninitializedField_tests)
  add_subdirectory(OvertureCode)
  add_subdirectory(ExpressionTemplateExample_tests)
  add_subdirectory(hiddenTypeAndDeclarationListTests)
  add_subdirectory(sizeofOperation_tests)

  add_subdirectory(nameQualificationAndTypeElaboration_tests)
  add_subdirectory(unparse_template_from_ast)
  add_subdirectory(STL_tests)

  # Legacy frontend interface tests are obsolete
  # if(enable-legacy-frontend-interface)
  #   add_subdirectory(LegacyFrontendInterface_C_tests)
  # endif()

  add_subdirectory(UnparseHeadersTests)
  add_subdirectory(UnparseHeadersUsingTokenStream_tests)
  add_subdirectory(frontend_integration)

  if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Xtensa")
    add_subdirectory(mergeAST_tests)
    add_subdirectory(ElsaTestCases)
    add_subdirectory(virtualCFG_tests)
    add_subdirectory(mixLanguage_tests)
    add_subdirectory(systemc_tests)
    add_subdirectory(staticCFG_tests)
  endif()
endif()

add_subdirectory(CAF2_tests)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_check
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=CAF2_tests_X-copointer-operations-long_caf\;CAF2_tests_X-cotarget-declarations-long_caf\;CAF2_tests_X-test-copointer4_1_caf\;CAF2_tests_abarrier-1_caf\;CAF2_tests_acopy-1_caf\;CAF2_tests_acopy-2_caf\;CAF2_tests_allgather-1_caf\;CAF2_tests_allocate-s_caf\;CAF2_tests_allocate_caf\;CAF2_tests_alltoall-1_caf\;CAF2_tests_alltoall-2_caf\;CAF2_tests_alltoall-3_caf\;CAF2_tests_assign-1_caf\;CAF2_tests_assign-2_caf\;CAF2_tests_assumed-size-coarray-1_caf\;CAF2_tests_assumed-size-coarray-2_caf\;CAF2_tests_attrs-async_caf\;CAF2_tests_attrs-copointer_caf\;CAF2_tests_attrs-external_caf\;CAF2_tests_attrs-pointer-coarray_caf\;CAF2_tests_basic-1_caf\;CAF2_tests_broadcast-3_caf\;CAF2_tests_broadcast-4_caf\;CAF2_tests_broadcast-5_caf\;CAF2_tests_broadcast-6_caf\;CAF2_tests_caf-allocate_caf\;CAF2_tests_caf-derived_type_caf\;CAF2_tests_caf-sub_caf\;CAF2_tests_caf-team_caf\;CAF2_tests_caf-topology_caf\;CAF2_tests_caf_caf\;CAF2_tests_cmdline-1_caf\;CAF2_tests_cmdline-2_caf\;CAF2_tests_coarray-3_caf\;CAF2_tests_coarray-4_caf\;CAF2_tests_coarray-6_caf\;CAF2_tests_coarray-allocation-1_caf\;CAF2_tests_coarray-bounds-1_caf\;CAF2_tests_coarray-bounds-2_caf\;CAF2_tests_coarray-decl_caf\;CAF2_tests_coarray-semantics-1_caf\;CAF2_tests_coarray-string_caf\;CAF2_tests_copointer-array_caf\;CAF2_tests_copointer-coarray_caf\;CAF2_tests_copointer-components_caf\;CAF2_tests_copointer-declarations_caf\;CAF2_tests_copointer-derived-types_caf\;CAF2_tests_copointer-operations_caf\;CAF2_tests_copointer-scalar-coscalar_caf\;CAF2_tests_copointer-team1_caf\;CAF2_tests_copointer-team2_caf\;CAF2_tests_coscalar-1_caf\;CAF2_tests_coshape_caf\;CAF2_tests_cotarget-declarations_caf\;CAF2_tests_derived-type-1_caf\;CAF2_tests_derived-type-2_caf\;CAF2_tests_derived-type-3_caf\;CAF2_tests_derived-type-4_caf\;CAF2_tests_derived-type-5_caf\;CAF2_tests_derived-type-6_caf\;CAF2_tests_derived_caf\;CAF2_tests_dimensions-1_caf\;CAF2_tests_event-1_caf\;CAF2_tests_event-2_caf\;CAF2_tests_event-3_caf\;CAF2_tests_event-4_caf\;CAF2_tests_event-5_caf\;CAF2_tests_event_caf\;CAF2_tests_explicit-shape-1_caf\;CAF2_tests_func-decl-1_caf\;CAF2_tests_func-decl-4_caf\;CAF2_tests_get-1_caf\;CAF2_tests_get-2_caf\;CAF2_tests_get-3_caf\;CAF2_tests_image-selector_caf\;CAF2_tests_lockset_caf\;CAF2_tests_loopbound-1_caf\;CAF2_tests_memory-1_caf\;CAF2_tests_module-1-mod1_caf\;CAF2_tests_module-2-module_table_caf\;CAF2_tests_pointer-1_caf\;CAF2_tests_pointer-coarray-derived-type_caf\;CAF2_tests_put-1_caf\;CAF2_tests_put-3_caf\;CAF2_tests_remote-read-1_caf\;CAF2_tests_remote-write-1_caf\;CAF2_tests_team_caf\;CAF2_tests_topology_caf\;CAF2_tests_use-only-rename_caf\;CAF2_tests_use-rename_caf\;CAF2_tests_withteam_caf
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_check PROPERTIES LABELS disabled DEPENDS "CAF2_tests_X-copointer-operations-long_caf;CAF2_tests_X-cotarget-declarations-long_caf;CAF2_tests_X-test-copointer4_1_caf;CAF2_tests_abarrier-1_caf;CAF2_tests_acopy-1_caf;CAF2_tests_acopy-2_caf;CAF2_tests_allgather-1_caf;CAF2_tests_allocate-s_caf;CAF2_tests_allocate_caf;CAF2_tests_alltoall-1_caf;CAF2_tests_alltoall-2_caf;CAF2_tests_alltoall-3_caf;CAF2_tests_assign-1_caf;CAF2_tests_assign-2_caf;CAF2_tests_assumed-size-coarray-1_caf;CAF2_tests_assumed-size-coarray-2_caf;CAF2_tests_attrs-async_caf;CAF2_tests_attrs-copointer_caf;CAF2_tests_attrs-external_caf;CAF2_tests_attrs-pointer-coarray_caf;CAF2_tests_basic-1_caf;CAF2_tests_broadcast-3_caf;CAF2_tests_broadcast-4_caf;CAF2_tests_broadcast-5_caf;CAF2_tests_broadcast-6_caf;CAF2_tests_caf-allocate_caf;CAF2_tests_caf-derived_type_caf;CAF2_tests_caf-sub_caf;CAF2_tests_caf-team_caf;CAF2_tests_caf-topology_caf;CAF2_tests_caf_caf;CAF2_tests_cmdline-1_caf;CAF2_tests_cmdline-2_caf;CAF2_tests_coarray-3_caf;CAF2_tests_coarray-4_caf;CAF2_tests_coarray-6_caf;CAF2_tests_coarray-allocation-1_caf;CAF2_tests_coarray-bounds-1_caf;CAF2_tests_coarray-bounds-2_caf;CAF2_tests_coarray-decl_caf;CAF2_tests_coarray-semantics-1_caf;CAF2_tests_coarray-string_caf;CAF2_tests_copointer-array_caf;CAF2_tests_copointer-coarray_caf;CAF2_tests_copointer-components_caf;CAF2_tests_copointer-declarations_caf;CAF2_tests_copointer-derived-types_caf;CAF2_tests_copointer-operations_caf;CAF2_tests_copointer-scalar-coscalar_caf;CAF2_tests_copointer-team1_caf;CAF2_tests_copointer-team2_caf;CAF2_tests_coscalar-1_caf;CAF2_tests_coshape_caf;CAF2_tests_cotarget-declarations_caf;CAF2_tests_derived-type-1_caf;CAF2_tests_derived-type-2_caf;CAF2_tests_derived-type-3_caf;CAF2_tests_derived-type-4_caf;CAF2_tests_derived-type-5_caf;CAF2_tests_derived-type-6_caf;CAF2_tests_derived_caf;CAF2_tests_dimensions-1_caf;CAF2_tests_event-1_caf;CAF2_tests_event-2_caf;CAF2_tests_event-3_caf;CAF2_tests_event-4_caf;CAF2_tests_event-5_caf;CAF2_tests_event_caf;CAF2_tests_explicit-shape-1_caf;CAF2_tests_func-decl-1_caf;CAF2_tests_func-decl-4_caf;CAF2_tests_get-1_caf;CAF2_tests_get-2_caf;CAF2_tests_get-3_caf;CAF2_tests_image-selector_caf;CAF2_tests_lockset_caf;CAF2_tests_loopbound-1_caf;CAF2_tests_memory-1_caf;CAF2_tests_module-1-mod1_caf;CAF2_tests_module-2-module_table_caf;CAF2_tests_pointer-1_caf;CAF2_tests_pointer-coarray-derived-type_caf;CAF2_tests_put-1_caf;CAF2_tests_put-3_caf;CAF2_tests_remote-read-1_caf;CAF2_tests_remote-write-1_caf;CAF2_tests_team_caf;CAF2_tests_topology_caf;CAF2_tests_use-only-rename_caf;CAF2_tests_use-rename_caf;CAF2_tests_withteam_caf" )
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_CompileTests_check PROPERTIES DISABLED TRUE)
endif()
