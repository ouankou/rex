if(NOT ENABLE-FORTRAN)
  message(STATUS "Skipping CAF2_tests (Fortran disabled)")
  return()
endif()

set(files_to_test
  abarrier-1.caf
  acopy-1.caf
  acopy-2.caf
  allgather-1.caf
  allocate-s.caf
  allocate.caf
  alltoall-1.caf
  alltoall-2.caf
  alltoall-3.caf
  assign-1.caf
  assign-2.caf
  assumed-size-coarray-1.caf
  assumed-size-coarray-2.caf
  attrs-async.caf
  attrs-copointer.caf
  attrs-external.caf
  attrs-pointer-coarray.caf
  basic-1.caf
  broadcast-3.caf
  broadcast-4.caf
  broadcast-5.caf
  broadcast-6.caf
  caf-allocate.caf
  caf-derived_type.caf
  caf-sub.caf
  caf-team.caf
  caf-topology.caf
  caf.caf
  cmdline-1.caf
  cmdline-2.caf
  coarray-3.caf
  coarray-4.caf
  coarray-6.caf
  coarray-allocation-1.caf
  coarray-bounds-1.caf
  coarray-bounds-2.caf
  coarray-decl.caf
  coarray-semantics-1.caf
  coarray-string.caf
  copointer-components.caf
  copointer-array.caf
  copointer-coarray.caf
  copointer-declarations.caf
  copointer-derived-types.caf
  copointer-operations.caf
  copointer-scalar-coscalar.caf
  copointer-team1.caf
  copointer-team2.caf
  coshape.caf
  cotarget-declarations.caf
  coscalar-1.caf
  derived-type-1.caf
  derived-type-2.caf
  derived-type-3.caf
  derived-type-4.caf
  derived-type-5.caf
  derived-type-6.caf
  derived.caf
  dimensions-1.caf
  event-1.caf
  event-2.caf
  event-3.caf
  event-4.caf
  event-5.caf
  event.caf
  explicit-shape-1.caf
  func-decl-1.caf
  func-decl-4.caf
  get-1.caf
  get-2.caf
  get-3.caf
  lockset.caf
  loopbound-1.caf
  image-selector.caf
  memory-1.caf
  module-1-mod1.caf
  module-2-module_table.caf
  pointer-1.caf
  pointer-coarray-derived-type.caf
  put-1.caf
  put-3.caf
  remote-read-1.caf
  remote-write-1.caf
  team.caf
  topology.caf
  use-only-rename.caf
  use-rename.caf
  withteam.caf
)

set(failing_tests
  X-copointer-operations-long.caf
  X-cotarget-declarations-long.caf
  X-test-copointer4_1.caf
)

set(ROSE_FLAGS
  -rose:caf2.0
  -rose:skip_syntax_check
  -rose:skipfinalCompileStep
)

set(ROSE_FAILING_TESTCODES ${failing_tests})

foreach(file_to_test IN LISTS files_to_test failing_tests)
  compile_test(${file_to_test} CAF2_tests)
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_CAF2_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=CAF2_tests_abarrier-1_caf\;CAF2_tests_acopy-1_caf\;CAF2_tests_acopy-2_caf\;CAF2_tests_allgather-1_caf\;CAF2_tests_allocate-s_caf\;CAF2_tests_allocate_caf\;CAF2_tests_alltoall-1_caf\;CAF2_tests_alltoall-2_caf\;CAF2_tests_alltoall-3_caf\;CAF2_tests_assign-1_caf\;CAF2_tests_assign-2_caf\;CAF2_tests_assumed-size-coarray-1_caf\;CAF2_tests_assumed-size-coarray-2_caf\;CAF2_tests_attrs-async_caf\;CAF2_tests_attrs-copointer_caf\;CAF2_tests_attrs-external_caf\;CAF2_tests_attrs-pointer-coarray_caf\;CAF2_tests_basic-1_caf\;CAF2_tests_broadcast-3_caf\;CAF2_tests_broadcast-4_caf\;CAF2_tests_broadcast-5_caf\;CAF2_tests_broadcast-6_caf\;CAF2_tests_caf-allocate_caf\;CAF2_tests_caf-derived_type_caf\;CAF2_tests_caf-sub_caf\;CAF2_tests_caf-team_caf\;CAF2_tests_caf-topology_caf\;CAF2_tests_caf_caf\;CAF2_tests_cmdline-1_caf\;CAF2_tests_cmdline-2_caf\;CAF2_tests_coarray-3_caf\;CAF2_tests_coarray-4_caf\;CAF2_tests_coarray-6_caf\;CAF2_tests_coarray-allocation-1_caf\;CAF2_tests_coarray-bounds-1_caf\;CAF2_tests_coarray-bounds-2_caf\;CAF2_tests_coarray-decl_caf\;CAF2_tests_coarray-semantics-1_caf\;CAF2_tests_coarray-string_caf\;CAF2_tests_copointer-array_caf\;CAF2_tests_copointer-coarray_caf\;CAF2_tests_copointer-components_caf\;CAF2_tests_copointer-declarations_caf\;CAF2_tests_copointer-derived-types_caf\;CAF2_tests_copointer-operations_caf\;CAF2_tests_copointer-scalar-coscalar_caf\;CAF2_tests_copointer-team1_caf\;CAF2_tests_copointer-team2_caf\;CAF2_tests_coscalar-1_caf\;CAF2_tests_coshape_caf\;CAF2_tests_cotarget-declarations_caf\;CAF2_tests_derived-type-1_caf\;CAF2_tests_derived-type-2_caf\;CAF2_tests_derived-type-3_caf\;CAF2_tests_derived-type-4_caf\;CAF2_tests_derived-type-5_caf\;CAF2_tests_derived-type-6_caf\;CAF2_tests_derived_caf\;CAF2_tests_dimensions-1_caf\;CAF2_tests_event-1_caf\;CAF2_tests_event-2_caf\;CAF2_tests_event-3_caf\;CAF2_tests_event-4_caf\;CAF2_tests_event-5_caf\;CAF2_tests_event_caf\;CAF2_tests_explicit-shape-1_caf\;CAF2_tests_func-decl-1_caf\;CAF2_tests_func-decl-4_caf\;CAF2_tests_get-1_caf\;CAF2_tests_get-2_caf\;CAF2_tests_get-3_caf\;CAF2_tests_image-selector_caf\;CAF2_tests_lockset_caf\;CAF2_tests_loopbound-1_caf\;CAF2_tests_memory-1_caf\;CAF2_tests_module-1-mod1_caf\;CAF2_tests_module-2-module_table_caf\;CAF2_tests_pointer-1_caf\;CAF2_tests_pointer-coarray-derived-type_caf\;CAF2_tests_put-1_caf\;CAF2_tests_put-3_caf\;CAF2_tests_remote-read-1_caf\;CAF2_tests_remote-write-1_caf\;CAF2_tests_team_caf\;CAF2_tests_topology_caf\;CAF2_tests_use-only-rename_caf\;CAF2_tests_use-rename_caf\;CAF2_tests_withteam_caf
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_CAF2_tests_check-local PROPERTIES
  DEPENDS "CAF2_tests_abarrier-1_caf;CAF2_tests_acopy-1_caf;CAF2_tests_acopy-2_caf;CAF2_tests_allgather-1_caf;CAF2_tests_allocate-s_caf;CAF2_tests_allocate_caf;CAF2_tests_alltoall-1_caf;CAF2_tests_alltoall-2_caf;CAF2_tests_alltoall-3_caf;CAF2_tests_assign-1_caf;CAF2_tests_assign-2_caf;CAF2_tests_assumed-size-coarray-1_caf;CAF2_tests_assumed-size-coarray-2_caf;CAF2_tests_attrs-async_caf;CAF2_tests_attrs-copointer_caf;CAF2_tests_attrs-external_caf;CAF2_tests_attrs-pointer-coarray_caf;CAF2_tests_basic-1_caf;CAF2_tests_broadcast-3_caf;CAF2_tests_broadcast-4_caf;CAF2_tests_broadcast-5_caf;CAF2_tests_broadcast-6_caf;CAF2_tests_caf-allocate_caf;CAF2_tests_caf-derived_type_caf;CAF2_tests_caf-sub_caf;CAF2_tests_caf-team_caf;CAF2_tests_caf-topology_caf;CAF2_tests_caf_caf;CAF2_tests_cmdline-1_caf;CAF2_tests_cmdline-2_caf;CAF2_tests_coarray-3_caf;CAF2_tests_coarray-4_caf;CAF2_tests_coarray-6_caf;CAF2_tests_coarray-allocation-1_caf;CAF2_tests_coarray-bounds-1_caf;CAF2_tests_coarray-bounds-2_caf;CAF2_tests_coarray-decl_caf;CAF2_tests_coarray-semantics-1_caf;CAF2_tests_coarray-string_caf;CAF2_tests_copointer-array_caf;CAF2_tests_copointer-coarray_caf;CAF2_tests_copointer-components_caf;CAF2_tests_copointer-declarations_caf;CAF2_tests_copointer-derived-types_caf;CAF2_tests_copointer-operations_caf;CAF2_tests_copointer-scalar-coscalar_caf;CAF2_tests_copointer-team1_caf;CAF2_tests_copointer-team2_caf;CAF2_tests_coscalar-1_caf;CAF2_tests_coshape_caf;CAF2_tests_cotarget-declarations_caf;CAF2_tests_derived-type-1_caf;CAF2_tests_derived-type-2_caf;CAF2_tests_derived-type-3_caf;CAF2_tests_derived-type-4_caf;CAF2_tests_derived-type-5_caf;CAF2_tests_derived-type-6_caf;CAF2_tests_derived_caf;CAF2_tests_dimensions-1_caf;CAF2_tests_event-1_caf;CAF2_tests_event-2_caf;CAF2_tests_event-3_caf;CAF2_tests_event-4_caf;CAF2_tests_event-5_caf;CAF2_tests_event_caf;CAF2_tests_explicit-shape-1_caf;CAF2_tests_func-decl-1_caf;CAF2_tests_func-decl-4_caf;CAF2_tests_get-1_caf;CAF2_tests_get-2_caf;CAF2_tests_get-3_caf;CAF2_tests_image-selector_caf;CAF2_tests_lockset_caf;CAF2_tests_loopbound-1_caf;CAF2_tests_memory-1_caf;CAF2_tests_module-1-mod1_caf;CAF2_tests_module-2-module_table_caf;CAF2_tests_pointer-1_caf;CAF2_tests_pointer-coarray-derived-type_caf;CAF2_tests_put-1_caf;CAF2_tests_put-3_caf;CAF2_tests_remote-read-1_caf;CAF2_tests_remote-write-1_caf;CAF2_tests_team_caf;CAF2_tests_topology_caf;CAF2_tests_use-only-rename_caf;CAF2_tests_use-rename_caf;CAF2_tests_withteam_caf"
)
