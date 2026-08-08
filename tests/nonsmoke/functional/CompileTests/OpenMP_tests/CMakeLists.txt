add_executable(parseOmp parseOmp.C)
target_link_libraries(parseOmp ROSE_DLL ${link_with_libraries})

add_executable(checkOmpAnalyzing checkOmpAnalyzing.C)
target_link_libraries(checkOmpAnalyzing ROSE_DLL ${link_with_libraries})

add_executable(rex_test2026_mapper_typedef_unparse
  rex_test2026_mapper_typedef_unparse.cpp)
target_link_libraries(rex_test2026_mapper_typedef_unparse
  ROSE_DLL ${link_with_libraries})

add_executable(rex_openmp_mapper_mangled_identity
  rex_openmp_mapper_mangled_identity.cpp)
target_link_libraries(rex_openmp_mapper_mangled_identity
  ROSE_DLL ${link_with_libraries})

add_executable(rex_omp_masked_filter_identity_translator
  rex_omp_masked_filter_identity_translator.cpp)
target_link_libraries(rex_omp_masked_filter_identity_translator
  ROSE_DLL ${link_with_libraries})

add_executable(rex_openmp_sage_binding_translator
  rex_openmp_sage_binding_translator.cpp)
target_link_libraries(rex_openmp_sage_binding_translator
  ROSE_DLL ${link_with_libraries})

add_executable(rex_openmp_joined_runtime_state_translator
  rex_openmp_joined_runtime_state_translator.cpp)
target_link_libraries(rex_openmp_joined_runtime_state_translator
  ROSE_DLL ${link_with_libraries})

add_executable(rex_openmp_sage_expression_paths_translator
  rex_openmp_sage_expression_paths_translator.cpp)
target_link_libraries(rex_openmp_sage_expression_paths_translator
  ROSE_DLL ${link_with_libraries})

add_executable(rex_openmp_array_section_device_mode_translator
  rex_openmp_array_section_device_mode_translator.cpp)
target_link_libraries(rex_openmp_array_section_device_mode_translator
  ROSE_DLL ${link_with_libraries})

add_executable(rex_openmp_hard_contracts
  rex_openmp_hard_contracts.cpp)
target_link_libraries(rex_openmp_hard_contracts
  ROSE_DLL ${link_with_libraries})

add_executable(rex_openmp_combined_clause_source_order_translator
  rex_openmp_combined_clause_source_order_translator.cpp)
target_link_libraries(rex_openmp_combined_clause_source_order_translator
  ROSE_DLL ${link_with_libraries})

find_package(Threads REQUIRED)
add_executable(rex_openmp_conversion_session
  rex_openmp_conversion_session.cpp)
target_link_libraries(rex_openmp_conversion_session
  ROSE_DLL ${link_with_libraries} Threads::Threads)

add_executable(rex_openmp_typed_clause_items_translator
  rex_openmp_typed_clause_items_translator.cpp)
target_link_libraries(rex_openmp_typed_clause_items_translator
  ROSE_DLL ${link_with_libraries})

add_executable(rex_openmp_absent_contains_typed_contract
  rex_openmp_absent_contains_typed_contract.cpp)
target_link_libraries(rex_openmp_absent_contains_typed_contract
  ROSE_DLL ${link_with_libraries})

add_executable(rex_openmp_clause_list_ownership_translator
  rex_openmp_clause_list_ownership_translator.cpp)
target_link_libraries(rex_openmp_clause_list_ownership_translator
  ROSE_DLL ${link_with_libraries})

add_executable(rex_openmp_declare_simd_exact_target_translator
  rex_openmp_declare_simd_exact_target_translator.cpp)
target_link_libraries(rex_openmp_declare_simd_exact_target_translator
  ROSE_DLL ${link_with_libraries})

add_executable(rex_openmp_declare_variant_exact_target_translator
  rex_openmp_declare_variant_exact_target_translator.cpp)
target_link_libraries(rex_openmp_declare_variant_exact_target_translator
  ROSE_DLL ${link_with_libraries})

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; OpenMP_tests require bash")
endif()

if(ENABLE-C)
  add_subdirectory(cvalidation)
endif()

if(ENABLE-FORTRAN)
  add_subdirectory(fortran)
endif()

if(NOT ENABLE-C)
  return()
endif()

# C test codes
set(C_TESTCODES
	3loops.c affinity.c allocate.c jacobi.c jacobi-collapse.c alignment.c
	array_init.c array_init_2.c array_section_stride.c atoms-2.c atomic.c atomic-3.c barrier.c
	cancel.c cancellation_point.c collapse.c copyin.c copyprivate2.c copyprivate3.c
	rex_omp_collapse_constant_expression.c
	copyprivate.c critical.c critical1.c critical_dead.c critical_orphaned.c
	depobj.c task_dep1.c task_dep3.c task_dep_iterator1.c task_dep_iterator2.c
	rex_openmp_target_update_distinct_items.c
	rex_openmp_taskgraph_target_valid.c
	declare_simd.c distribute_parallel_for.c distribute_parallel_for_simd.c
	distribute_simd.c distribute.c distribute2.c dijkstra_open_mp.c dynamicChunk.c
	empty.c endif.c endif2.c endif3.c nowait.c expressions.c falsesharing.c
	firstprivate.c firstPrivateArray.c firstPrivateArray_2.c firstlastprivate.c
	flush.c flush_exampleA_21_1c.c full_verify.c flush_with_clauses.c
	get_max_threads.c hello.c hello-1.c hello-2.c hello-ordered.c
	include_header_only.c init.c lastprivate0.c lastprivate.c lastprivateOrphaned.c
	limits_threads.c linebreak.c lockarray.c loop.c loop1.c lu_factorization.c
	master.c masterSingle.c mastertaskloop.c mastertaskloopsimd.c metadirective_arch_nvidia.c matrix_vector.c
	matrixmultiply.c md_open_mp.c multiple_return.c nestedpar1.c nestedpar.c omp1.c
	ompfor.c ompfor_c99.c ompfor2.c ompfor3.c ompfor4.c ompfor5.c ompfor6.c
	ompfor8.c ompfor9.c ompfor10.c ompfor-default.c ompfor-decremental.c
	ompfor-static.c ompGetNumThreads.c omp_sections.c ordered3.c ordered2.c
	ordered.c ordered_depend.c orphaned-directives.c parallel.c
	parallel_allocate_predefined_modifier.c parallel_allocate_user_modifier.c
	parallel-if.c parallel-if-numthreads.c parallel_if_parallel.c
	parallel-numthreads.c parallel-reduction.c parallel-reduction2.c
	parallel_pragma.c parallelfor.c parallelfor2.c parallelsections.c
	parallelloop.c parallelmaster.c parallelmastertaskloop.c
	parallelmastertaskloopsimd.c pi.c preprocessingInfo.c prettyfunc.c private.c
	privatej.c private-duplicate.c recursive.c reduction2.c reduction.c
	reduction-classic.c reduction_min_max.c reduction_modifier.c
	reduction_user_identifier.c requires.c requires_atomic_default_mem_order.c
	requires_ext.c requires_reverse_offload.c requires_unified_address.c
	requires_dynamic_allocators.c requires_unified_shared_memory.c rice1.c scan.c
	section.c section1.c sections.c sections1.c set_num_threads.c shared.c
	simd2.c simd3.c simd4.c simd5.c single.c single2.c single3.c
	single_copyprivate.c sizeof.c spmd1.c staticChunk.c subteam2.c subteam.c
	target.c target1.c target_uses_allocators.c targetparallel.c
	rex_test2026_uses_allocators_optional_edges.c
	rex_test2026_uses_allocators_user_defined_isolation.c
	targetparallelforsimd.c targetparallelloop.c targetsimd.c targetteams.c
	targetteamsdistribute.c targetteamsdistributesimd.c targetteamsloop.c
	targetteamsdistributeparallelfor.c targetteamsdistributeparallelforsimd.c
	targetupdate.c targetupdate_nowait.c targetupdate_device.c
	target_literal_scalar.c
	target_update_to.c taskloop_simd_reduction.1.c task_affinity.c task_affinity1.c
	task_array.c task_array2.c task_final.c task_largenumber.c task_mergeable.c
	task_orphaned.c task_outlining.c task_priority.c task_untied.c task_untied2.c
	task_untied3.c task_untied4.c task_underIf.c taskgroup.c taskloop.c
	task_wait.c task_wait2.c taskwait.c taskwait2.c taskwait3.c taskwait4.c taskwait5.c
	taskyield.c
	threadprivate.c threadprivate2.c threadprivate3.c teams.c teamsdistribute.c
	teamsdistributesimd.c teamsdistributeparallelfor.c
	teamsdistributeparallelforsimd.c teamsloop.c twoRegions.c threadProcessor.c
	upperCase.c variables.c requires1.c simd.c ompfor7.c macroCalls.c
	define-directive.c while-task.c task_dep2.c task_dep.load.compute.c)

# C++ test codes
set(CXX_TESTCODES
	classMember.cpp hello1.cpp helloNested.cpp memberFunction.cpp
	memberFunction2.cpp objectPrivate.cpp objectFirstPrivate.cpp
	objectLastprivate.cpp referenceType.cpp
	rex_openmp_mapper_type_qualifiers_valid.cpp
	orphanedAtomic.cpp preprocessingInfo2.cpp task_link.cpp task_link2.cpp
	task_tree.cpp jacobi-ompacc2.cpp data-member-access.cpp define-directive2.cpp
	rex_test2026_array_section_optional_bounds.cpp
	rex_test2026_mapper_template_reachable_scope.cpp)

set(TESTCODES ${C_TESTCODES} ${CXX_TESTCODES})

# OpenMP+OpenACC combined test codes
set(OMP_ACC_TESTCODES
  axpy_ompacc.c rex_openmp_axpy_multi_device.c axpy_ompacc3.c
  matrixmultiply-ompacc.c matrixmultiply-ompacc2.c
  jacobi-ompacc.c jacobi-ompacc-v2.c jacobi-ompacc-opt1.c jacobi-ompacc-opt2.c
  jacobi-ompacc-multiGPU.c axpy_ompacc4.c
)

# ROSE flags for OpenMP parsing (CFE frontend only).
# Do not force a C standard here; rely on the frontend default dialect.
set(ROSE_C_FLAGS -rose:openmp:ast_only -rose:verbose 0)
set(ROSE_CXX_FLAGS -rose:openmp:ast_only -rose:verbose 0)

set(_omp_test_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_omp_test_output_dir})

# OpenMP 6.0 requires every declare_mapper directive to map its mapper
# variable, limits mapper types by base language, rejects C++ types with
# virtual bases, and requires list items to be directive-wide unique. Keep
# malformed imported specimens intact and require semantic validation to stop
# at the first violation.
function(add_rex_openmp_semantic_negative
         test_name input_file language diagnostic)
  if(language STREQUAL "CXX")
    set(_flags ${ROSE_CXX_FLAGS})
  elseif(language STREQUAL "C")
    set(_flags ${ROSE_C_FLAGS})
  else()
    message(FATAL_ERROR
      "Unsupported language '${language}' for ${test_name}")
  endif()

  add_test(
    NAME ${test_name}
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"$1\"; expected=\"$2\"; shift 2; rm -f \"$log\"; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc -- \"$expected\" \"$log\")\" -eq 1"
      dummy
      ${test_name}.log
      "${diagnostic}"
      $<TARGET_FILE:parseOmp>
      ${_flags}
      -rose:skipfinalCompileStep
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "OMPTEST;rex;death")
endfunction()

add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_imported_mapper_missing_map_is_rejected
  declare_mapper_type_qualifiers.cpp
  CXX
  "  0:0: declare mapper requires at least one map clause")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_mapper_pointer_type_is_rejected
  rex_openmp_mapper_pointer_type_invalid.cpp
  CXX
  "REX_OMP_SEMANTIC[declare-mapper-type]: C++ mapper type must be a struct, union, or class type")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_c_mapper_pointer_type_is_rejected
  rex_openmp_mapper_pointer_type_invalid.c
  C
  "REX_OMP_SEMANTIC[declare-mapper-type]: C mapper type must be a struct or union type")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_mapper_without_mapped_variable_is_rejected
  rex_openmp_mapper_without_mapped_variable_invalid.c
  C
  "REX_OMP_SEMANTIC[declare-mapper-map-clause]: declare mapper requires a map clause that maps its variable")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_mapper_virtual_base_is_rejected
  rex_openmp_mapper_virtual_base_invalid.cpp
  CXX
  "REX_OMP_SEMANTIC[declare-mapper-virtual-base]: C++ mapper type must not be derived from a virtual base class")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_imported_target_update_duplicate_item_is_rejected
  declare_mapper_target_update.c
  C
  "REX_OMP_SEMANTIC[directive-list-item-uniqueness]: target update repeats an OpenMP list item across clauses")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_imported_target_update_from_duplicate_items_are_rejected
  target_update_from.c
  C
  "REX_OMP_SEMANTIC[directive-list-item-uniqueness]: target update repeats an OpenMP list item across clauses")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_orphaned_copyprivate_static_local_is_rejected
  rex_openmp_orphaned_copyprivate_static_local_invalid.c
  C
  "REX_OMP_SEMANTIC[copyprivate-data-environment]: copyprivate list item 'value' is neither threadprivate nor private in an enclosing context")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_nested_parallel_outer_local_copyprivate_is_rejected
  rex_openmp_nested_parallel_outer_local_copyprivate_invalid.c
  C
  "REX_OMP_SEMANTIC[copyprivate-data-environment]: copyprivate list item 'value' is neither threadprivate nor private in an enclosing context")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_nested_parallel_outer_private_clause_copyprivate_is_rejected
  rex_openmp_nested_parallel_outer_private_clause_copyprivate_invalid.c
  C
  "REX_OMP_SEMANTIC[copyprivate-data-environment]: copyprivate list item 'value' is neither threadprivate nor private in an enclosing context")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_copyprivate_shared_overrides_default_private_is_rejected
  rex_openmp_copyprivate_shared_overrides_default_private_invalid.c
  C
  "REX_OMP_SEMANTIC[copyprivate-data-environment]: copyprivate list item 'value' is neither threadprivate nor private in an enclosing context")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_copyprivate_shared_overrides_default_firstprivate_is_rejected
  rex_openmp_copyprivate_shared_overrides_default_firstprivate_invalid.c
  C
  "REX_OMP_SEMANTIC[copyprivate-data-environment]: copyprivate list item 'value' is neither threadprivate nor private in an enclosing context")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_parallel_body_static_local_copyprivate_is_rejected
  rex_openmp_parallel_body_static_local_copyprivate_invalid.c
  C
  "REX_OMP_SEMANTIC[copyprivate-data-environment]: copyprivate list item 'value' is neither threadprivate nor private in an enclosing context")
add_rex_openmp_semantic_negative(
  OMPTEST_rex_openmp_parallel_body_extern_local_copyprivate_is_rejected
  rex_openmp_parallel_body_extern_local_copyprivate_invalid.c
  C
  "REX_OMP_SEMANTIC[copyprivate-data-environment]: copyprivate list item 'value' is neither threadprivate nor private in an enclosing context")
# REX-owned valid counterparts retain positive AST/unparse coverage for every
# construct above.  Reparse each generated source so malformed output cannot
# pass merely because the first translation produced a file.
set(_rex_openmp_semantic_positive_sources
  rex_openmp_allocate_semantic_positive.c
  rex_openmp_atomic_semantic_positive.c
  rex_openmp_critical_semantic_positive.c
  rex_openmp_nested_variant_cache.c
  rex_openmp_orphaned_copyprivate_automatic_local.c
  rex_openmp_orphaned_copyprivate_parameter.cpp
  rex_openmp_orphaned_copyprivate_reference_parameter.cpp
  rex_openmp_simd_semantic_positive.c
  rex_openmp_target_if_semantic_positive.c
  rex_openmp_thread_local_copyprivate.cpp
  rex_openmp_user_allocator_semantic_positive.c)
foreach(_source IN LISTS _rex_openmp_semantic_positive_sources)
  string(MAKE_C_IDENTIFIER "${_source}" _name)
  if(_source MATCHES "[.](cc|cpp|cxx|C)$")
    set(_semantic_flags ${ROSE_CXX_FLAGS})
  else()
    set(_semantic_flags ${ROSE_C_FLAGS})
  endif()
  set(_output ${_omp_test_output_dir}/rose_${_source})
  set(_output_fixture rex_openmp_semantic_positive_${_name}_output)
  add_test(
    NAME OMPTEST_${_name}_translation
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND $<TARGET_FILE:parseOmp>
      ${_semantic_flags}
      -rose:skipfinalCompileStep
      -rose:output ${_output}
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/${_source})
  add_test(
    NAME OMPTEST_${_name}_roundtrip
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND $<TARGET_FILE:parseOmp>
      ${_semantic_flags}
      -rose:skip_unparse
      -rose:skipfinalCompileStep
      -c
      ${_output})
  set_tests_properties(OMPTEST_${_name}_translation
    PROPERTIES
      FIXTURES_SETUP ${_output_fixture}
      LABELS "OMPTEST;rex")
  set_tests_properties(OMPTEST_${_name}_roundtrip
    PROPERTIES
      FIXTURES_REQUIRED ${_output_fixture}
      LABELS "OMPTEST;rex")
endforeach()

set(_rex_openmp_mapper_mangled_identity_input
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_mapper_mangled_identity.c)
add_test(
  NAME OMPTEST_rex_openmp_mapper_mangled_identity
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:rex_openmp_mapper_mangled_identity>
    -rose:openmp:ast_only
    -rose:verbose 0
    -rose:skipfinalCompileStep
    -c ${_rex_openmp_mapper_mangled_identity_input})
set_tests_properties(OMPTEST_rex_openmp_mapper_mangled_identity
  PROPERTIES LABELS "OMPTEST;rex")

function(add_rex_openmp_mapper_identity_death_test test_name mode diagnostic)
  add_test(
    NAME ${test_name}
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"${test_name}.log\"; rm -f \"\$log\"; diagnostic=\"\$1\"; shift; set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Fxc -- \"\$diagnostic\" \"\$log\")\" -eq 1"
      dummy
      ${diagnostic}
      $<TARGET_FILE:rex_openmp_mapper_mangled_identity>
      ${mode}
      -rose:openmp:ast_only
      -rose:verbose 0
      -rose:skipfinalCompileStep
      -c ${_rex_openmp_mapper_mangled_identity_input})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "OMPTEST;rex;death")
endfunction()

add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_missing_identifier_is_rejected
  --missing-identifier
  "REX_AST_INVARIANT[declare-mapper-identity]: user mapper has no identifier expression")
add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_wrong_identifier_kind_is_rejected
  --wrong-identifier-kind
  "REX_AST_INVARIANT[declare-mapper-identity]: user mapper identifier is not SgOmpNameExpression")
add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_empty_identifier_is_rejected
  --empty-identifier
  "REX_AST_INVARIANT[declare-mapper-identity]: user mapper identifier spelling is empty")
add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_unspecified_identifier_is_rejected
  --unspecified-identifier
  "REX_AST_INVARIANT[declare-mapper-identity]: mapper identifier kind is unspecified or invalid")
add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_missing_type_is_rejected
  --missing-type
  "REX_AST_INVARIANT[declare-mapper-identity]: mapper has no type expression")
add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_wrong_type_kind_is_rejected
  --wrong-type-kind
  "REX_AST_INVARIANT[declare-mapper-identity]: mapper type is not SgTypeExpression")
add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_missing_semantic_type_is_rejected
  --missing-semantic-type
  "REX_AST_INVARIANT[declare-mapper-identity]: mapper type expression has no exact semantic type")
add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_missing_local_scope_is_rejected
  --missing-local-scope
  "REX_AST_INVARIANT[declare-mapper-identity]: mapper has no exact directive-local declaration scope")
add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_missing_variable_is_rejected
  --missing-variable
  "REX_AST_INVARIANT[declare-mapper-identity]: mapper has no variable expression")
add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_wrong_variable_kind_is_rejected
  --wrong-variable-kind
  "REX_AST_INVARIANT[declare-mapper-identity]: mapper variable is not SgVarRefExp")
add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_wrong_variable_scope_is_rejected
  --wrong-variable-scope
  "REX_AST_INVARIANT[declare-mapper-identity]: mapper variable is not exactly declared in its directive-local scope")
add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_mismatched_variable_type_is_rejected
  --mismatched-variable-type
  "REX_AST_INVARIANT[declare-mapper-identity]: mapper variable semantic type does not match mapper type expression")
add_rex_openmp_mapper_identity_death_test(
  OMPTEST_rex_openmp_mapper_identity_unmangled_scope_is_rejected
  --unmangled-scope
  "REX_AST_INVARIANT[declare-mapper-identity]: mapper lexical scope has no mangled identity")

set(_rex_omp_masked_filter_json_dir
    ${_omp_test_output_dir}/rex_omp_masked_filter_identity_json)
add_test(
  NAME OMPTEST_rex_omp_masked_filter_identity
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=post-omp-construction
    REX_AST_JSON_DIR=${_rex_omp_masked_filter_json_dir}
    $<TARGET_FILE:rex_omp_masked_filter_identity_translator>
      ${ROSE_C_FLAGS}
      -rex:ast-json-checkpoint=post-omp-construction
      -rex:ast-json-dir=${_rex_omp_masked_filter_json_dir}
      -rose:skipfinalCompileStep
      -rose:output ${_omp_test_output_dir}/rose_rex_omp_masked_filter_identity.c
      -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_omp_masked_filter_identity.c)
set_tests_properties(OMPTEST_rex_omp_masked_filter_identity
  PROPERTIES LABELS "OMPTEST;ASTJSON")

add_test(
  NAME OMPTEST_rex_openmp_sage_binding
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:rex_openmp_sage_binding_translator>
    ${ROSE_C_FLAGS}
    -rose:skipfinalCompileStep
    -rose:output
      ${_omp_test_output_dir}/rose_rex_openmp_sage_binding.c
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_sage_binding.c)
set_tests_properties(OMPTEST_rex_openmp_sage_binding
  PROPERTIES LABELS "OMPTEST;rex")

add_test(
  NAME OMPTEST_rex_openmp_joined_runtime_enables_rex_ast
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:rex_openmp_joined_runtime_state_translator>
    -fopenmp=libomp
    -rose:verbose 0
    -rose:skipfinalCompileStep
    -rose:output
      ${_omp_test_output_dir}/rose_rex_openmp_joined_runtime_state.c
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_joined_runtime_state.c)
set_tests_properties(OMPTEST_rex_openmp_joined_runtime_enables_rex_ast
  PROPERTIES LABELS "OMPTEST;rex;AST;CFE")
rex_lock_test_for_input(OMPTEST_rex_openmp_joined_runtime_enables_rex_ast
  rex_openmp_joined_runtime_state.c)

add_test(
  NAME OMPTEST_rex_openmp_empty_semantic_capture
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:parseOmp>
    ${ROSE_C_FLAGS}
    -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_empty_semantic_capture.c)
set_tests_properties(OMPTEST_rex_openmp_empty_semantic_capture
  PROPERTIES LABELS "OMPTEST;rex;AST")
rex_lock_test_for_input(OMPTEST_rex_openmp_empty_semantic_capture
  rex_openmp_empty_semantic_capture.c)

set(_rex_openmp_sage_expression_paths_output
  ${_omp_test_output_dir}/rose_rex_openmp_sage_expression_paths.cpp)
add_test(
  NAME OMPTEST_rex_openmp_sage_expression_paths
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:rex_openmp_sage_expression_paths_translator>
    ${ROSE_CXX_FLAGS}
    -rose:skipfinalCompileStep
    -rose:output
      ${_rex_openmp_sage_expression_paths_output}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_sage_expression_paths.cpp)
set_tests_properties(OMPTEST_rex_openmp_sage_expression_paths
  PROPERTIES
    FIXTURES_SETUP rex_openmp_sage_expression_paths_output
    LABELS "OMPTEST;rex;AST")
rex_lock_test_for_input(OMPTEST_rex_openmp_sage_expression_paths
  rex_openmp_sage_expression_paths.cpp)

add_test(
  NAME OMPTEST_rex_openmp_sage_expression_paths_roundtrip
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:parseOmp>
    -rose:openmp:ast_only
    -rose:verbose 0
    -rose:skipfinalCompileStep
    -c ${_rex_openmp_sage_expression_paths_output})
set_tests_properties(OMPTEST_rex_openmp_sage_expression_paths_roundtrip
  PROPERTIES
    FIXTURES_REQUIRED rex_openmp_sage_expression_paths_output
    LABELS "OMPTEST;rex;AST")

set(_rex_openmp_array_section_device_mode_output
  ${_omp_test_output_dir}/rose_rex_openmp_array_section_device_mode.c)
add_test(
  NAME OMPTEST_rex_openmp_array_section_device_mode
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && grep -Eq 'device *\\(\\*\\)' '${_rex_openmp_array_section_device_mode_output}' && grep -Eq 'matrix\\[1 *: *2\\]\\[2 *: *3\\]' '${_rex_openmp_array_section_device_mode_output}' && grep -Eq 'values\\[ *: *length\\]' '${_rex_openmp_array_section_device_mode_output}'"
    dummy
    $<TARGET_FILE:rex_openmp_array_section_device_mode_translator>
    ${ROSE_C_FLAGS}
    -rose:skipfinalCompileStep
    -rose:output ${_rex_openmp_array_section_device_mode_output}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_array_section_device_mode.c)
set_tests_properties(OMPTEST_rex_openmp_array_section_device_mode
  PROPERTIES LABELS "OMPTEST;rex;AST")
rex_lock_test_for_input(OMPTEST_rex_openmp_array_section_device_mode
  rex_openmp_array_section_device_mode.c)

add_test(
  NAME OMPTEST_rex_openmp_array_section_invalid_is_rejected
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_openmp_array_section_invalid.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc \"REX_OMP_AST_INVARIANT[array-section-base]: expected a pointer or array type, got SgTypeInt\" rex_openmp_array_section_invalid.log)\" -eq 1"
    dummy
    $<TARGET_FILE:parseOmp>
    ${ROSE_C_FLAGS}
    -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_array_section_invalid.c)
set_tests_properties(OMPTEST_rex_openmp_array_section_invalid_is_rejected
  PROPERTIES LABELS "OMPTEST;rex;death")
rex_lock_test_for_input(OMPTEST_rex_openmp_array_section_invalid_is_rejected
  rex_openmp_array_section_invalid.c)

add_test(
  NAME OMPTEST_rex_openmp_sage_name_overload_is_rejected
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_openmp_sage_name_overload_rejected.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc \"REX_OMP_AST_INVARIANT[sage-name-lookup]: identifier 'rex_openmp_sage_overload' has multiple exact Sage value identities\" rex_openmp_sage_name_overload_rejected.log)\" -eq 1"
    dummy
    $<TARGET_FILE:parseOmp>
    ${ROSE_CXX_FLAGS}
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_sage_name_overload_rejected.cpp)
set_tests_properties(
  OMPTEST_rex_openmp_sage_name_overload_is_rejected
  PROPERTIES LABELS "OMPTEST;rex;death")
rex_lock_test_for_input(
  OMPTEST_rex_openmp_sage_name_overload_is_rejected
  rex_openmp_sage_name_overload_rejected.cpp)

add_test(
  NAME OMPTEST_rex_openmp_sage_name_missing_is_rejected
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_openmp_sage_name_missing_rejected.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc \"REX_OMP_AST_INVARIANT[sage-name-lookup]: identifier 'rex_openmp_sage_missing_identity' has no exact Sage value identities\" rex_openmp_sage_name_missing_rejected.log)\" -eq 1"
    dummy
    $<TARGET_FILE:parseOmp>
    ${ROSE_CXX_FLAGS}
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_sage_name_missing_rejected.cpp)
set_tests_properties(
  OMPTEST_rex_openmp_sage_name_missing_is_rejected
  PROPERTIES LABELS "OMPTEST;rex;death")
rex_lock_test_for_input(
  OMPTEST_rex_openmp_sage_name_missing_is_rejected
  rex_openmp_sage_name_missing_rejected.cpp)

add_test(
  NAME OMPTEST_rex_openmp_adjacent_parameter_missing_is_rejected
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_openmp_adjacent_parameter_missing_rejected.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc \"REX_OMP_AST_INVARIANT[sage-name-lookup]: identifier 'rex_missing_parameter' has no exact Sage value identities\" rex_openmp_adjacent_parameter_missing_rejected.log)\" -eq 1"
    dummy
    $<TARGET_FILE:parseOmp>
    ${ROSE_C_FLAGS}
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_adjacent_parameter_missing_rejected.c)
set_tests_properties(
  OMPTEST_rex_openmp_adjacent_parameter_missing_is_rejected
  PROPERTIES LABELS "OMPTEST;rex;death")
rex_lock_test_for_input(
  OMPTEST_rex_openmp_adjacent_parameter_missing_is_rejected
  rex_openmp_adjacent_parameter_missing_rejected.c)

add_test(
  NAME OMPTEST_rex_openmp_conversion_session_sequential_isolation
  COMMAND $<TARGET_FILE:rex_openmp_conversion_session> sequential)
set_tests_properties(
  OMPTEST_rex_openmp_conversion_session_sequential_isolation
  PROPERTIES LABELS "OMPTEST;rex")

foreach(_mode nested concurrent)
  add_test(
    NAME OMPTEST_rex_openmp_conversion_session_${_mode}_is_rejected
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; set +e; \"$@\" >rex_openmp_conversion_session_${_mode}.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc \"REX_OMP_AST_INVARIANT[conversion-session]: nested or concurrent OpenMP conversion session activation\" rex_openmp_conversion_session_${_mode}.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_openmp_conversion_session>
      ${_mode})
  set_tests_properties(
    OMPTEST_rex_openmp_conversion_session_${_mode}_is_rejected
    PROPERTIES LABELS "OMPTEST;rex;death")
endforeach()

add_test(
  NAME OMPTEST_rex_openmp_conversion_sessionless_access_is_rejected
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_openmp_conversion_sessionless_access.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc \"REX_OMP_AST_INVARIANT[conversion-session]: OpenMP parser state accessed without an active conversion session\" rex_openmp_conversion_sessionless_access.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_openmp_conversion_session>
    without-session)
set_tests_properties(
  OMPTEST_rex_openmp_conversion_sessionless_access_is_rejected
  PROPERTIES LABELS "OMPTEST;rex;death")

foreach(_mode_diagnostic IN ITEMS
    "malformed-subexpression-type|^REX_OMP_AST_INVARIANT\\[exact-subexpression-construction\\]: record has no valid operation kind and exact Sage result type$"
    "combined-order-republication|^REX_AST_INVARIANT\\[omp-clause-source-order-publication\\]: clause=0x[[:xdigit:]]+ already owns combined source order 0$"
    "fortran-range|^REX_OMP_AST_INVARIANT\\[fortran-exact-semantic-range\\]: identifier span does not exactly name its directive source payload$"
    "fortran-overlap|^REX_OMP_AST_INVARIANT\\[fortran-exact-semantic-range\\]: identifier records contain an empty, duplicate, overlapping, or out-of-order source span$"
    "fortran-duplicate-span|^REX_OMP_AST_INVARIANT\\[fortran-exact-semantic-range\\]: identifier records contain an empty, duplicate, overlapping, or out-of-order source span$")
  string(REPLACE "|" ";" _mode_and_diagnostic "${_mode_diagnostic}")
  list(GET _mode_and_diagnostic 0 _mode)
  list(GET _mode_and_diagnostic 1 _diagnostic)
  string(REPLACE "-" "_" _mode_name "${_mode}")
  add_test(
    NAME OMPTEST_rex_openmp_exact_${_mode_name}_is_rejected
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; set +e; \"$@\" >rex_openmp_exact_${_mode_name}.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '${_diagnostic}' rex_openmp_exact_${_mode_name}.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_openmp_hard_contracts>
      ${_mode})
  set_tests_properties(
    OMPTEST_rex_openmp_exact_${_mode_name}_is_rejected
    PROPERTIES LABELS "OMPTEST;rex;death")
endforeach()

add_test(
  NAME OMPTEST_rex_openmp_declare_simd_exact_target
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:rex_openmp_declare_simd_exact_target_translator>
    ${ROSE_C_FLAGS}
    -rex:ast-json-checkpoint=post-omp-construction
    -rex:ast-json-dir=${_omp_test_output_dir}/declare-simd-exact-target-json
    -rose:skip_unparse
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_declare_simd_exact_target.c)
set_tests_properties(OMPTEST_rex_openmp_declare_simd_exact_target
  PROPERTIES LABELS "OMPTEST;rex;AST;ASTJSON")
rex_lock_test_for_input(OMPTEST_rex_openmp_declare_simd_exact_target
  rex_openmp_declare_simd_exact_target.c)

add_test(
  NAME OMPTEST_rex_openmp_declare_variant_exact_target
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:rex_openmp_declare_variant_exact_target_translator>
    ${ROSE_C_FLAGS}
    -rex:ast-json-checkpoint=post-omp-construction
    -rex:ast-json-dir=${_omp_test_output_dir}/declare-variant-exact-target-json
    -rose:skip_unparse
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_declare_variant_exact_target.c)
set_tests_properties(OMPTEST_rex_openmp_declare_variant_exact_target
  PROPERTIES LABELS "OMPTEST;rex;AST;ASTJSON")
rex_lock_test_for_input(OMPTEST_rex_openmp_declare_variant_exact_target
  rex_openmp_declare_variant_exact_target.c)

add_test(
  NAME OMPTEST_rex_openmp_combined_clause_source_order
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:rex_openmp_combined_clause_source_order_translator>
    ${ROSE_C_FLAGS}
    -rex:ast-json-checkpoint=post-omp-construction
    -rex:ast-json-dir=${_omp_test_output_dir}/combined-clause-order-json
    -rose:skipfinalCompileStep
    -rose:output
      ${_omp_test_output_dir}/rose_rex_openmp_combined_clause_source_order.c
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_combined_clause_source_order.c)
set_tests_properties(OMPTEST_rex_openmp_combined_clause_source_order
  PROPERTIES LABELS "OMPTEST;rex;AST;ASTJSON")
rex_lock_test_for_input(OMPTEST_rex_openmp_combined_clause_source_order
  rex_openmp_combined_clause_source_order.c)

add_test(
  NAME OMPTEST_rex_openmp_fortran_exact_semantic_mismatch_is_rejected
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_openmp_fortran_exact_semantic_mismatch.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc \"REX_OMP_AST_INVARIANT[fortran-exact-semantic-consumption]: producer identifier 'producer_identity' does not match parser identifier 'consumer_identity'\" rex_openmp_fortran_exact_semantic_mismatch.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_openmp_hard_contracts>
    fortran-mismatch)
set_tests_properties(
  OMPTEST_rex_openmp_fortran_exact_semantic_mismatch_is_rejected
  PROPERTIES LABELS "OMPTEST;rex;death")

add_test(
  NAME OMPTEST_rex_openmp_fortran_exact_semantic_wrong_kind_is_rejected
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_openmp_fortran_exact_semantic_wrong_kind.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc \"REX_OMP_AST_INVARIANT[fortran-exact-binding-construction]: identifier binding has an invalid kind, source span, spelling, or node/symbol identity\" rex_openmp_fortran_exact_semantic_wrong_kind.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_openmp_hard_contracts>
    fortran-wrong-kind)
set_tests_properties(
  OMPTEST_rex_openmp_fortran_exact_semantic_wrong_kind_is_rejected
  PROPERTIES LABELS "OMPTEST;rex;death")

add_test(
  NAME OMPTEST_rex_openmp_fortran_exact_semantic_missing_record_is_rejected
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_openmp_fortran_exact_semantic_missing_record.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc \"REX_OMP_AST_INVARIANT[fortran-exact-semantic-consumption]: no producer binding for identifier 'consumer_identity'\" rex_openmp_fortran_exact_semantic_missing_record.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_openmp_hard_contracts>
    fortran-missing-record)
set_tests_properties(
  OMPTEST_rex_openmp_fortran_exact_semantic_missing_record_is_rejected
  PROPERTIES LABELS "OMPTEST;rex;death")

add_test(
  NAME OMPTEST_rex_openmp_fortran_exact_semantic_stale_record_is_rejected
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_openmp_fortran_exact_semantic_stale_record.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc \"REX_OMP_AST_INVARIANT[fortran-exact-semantic-consumption]: expression parser did not consume every exact Fortran identifier binding\" rex_openmp_fortran_exact_semantic_stale_record.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_openmp_hard_contracts>
    fortran-stale)
set_tests_properties(
  OMPTEST_rex_openmp_fortran_exact_semantic_stale_record_is_rejected
  PROPERTIES LABELS "OMPTEST;rex;death")

add_test(
  NAME OMPTEST_rex_openmp_declare_simd_missing_target_is_rejected
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_openmp_declare_simd_missing_target.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '^REX_AST_INVARIANT\\[declare-simd-construction\\]: statement=0x[[:xdigit:]]+ requires one unowned exact function reference$' rex_openmp_declare_simd_missing_target.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_openmp_hard_contracts>
    declare-simd-missing-target)
set_tests_properties(
  OMPTEST_rex_openmp_declare_simd_missing_target_is_rejected
  PROPERTIES LABELS "OMPTEST;rex;death")

add_test(
  NAME OMPTEST_rex_openmp_declare_variant_missing_target_is_rejected
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_openmp_declare_variant_missing_target.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '^REX_AST_INVARIANT\\[declare-variant-construction\\]: statement=0x[[:xdigit:]]+ requires distinct unowned exact base and variant function references$' rex_openmp_declare_variant_missing_target.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_openmp_hard_contracts>
    declare-variant-missing-target)
set_tests_properties(
  OMPTEST_rex_openmp_declare_variant_missing_target_is_rejected
  PROPERTIES LABELS "OMPTEST;rex;death")

add_test(
  NAME OMPTEST_rex_openmp_nonentity_identity_is_rejected
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; set +e; \"$@\" >rex_openmp_nonentity_identity.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '^REX_AST_INVARIANT\\[openmp-nonentity-identity\\]: statement=0x[[:xdigit:]]+ type=SgOmpRequiresStatement directive=requires does not declare an independently named semantic entity$' rex_openmp_nonentity_identity.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_openmp_hard_contracts>
    openmp-nonentity-identity)
set_tests_properties(
  OMPTEST_rex_openmp_nonentity_identity_is_rejected
  PROPERTIES LABELS "OMPTEST;rex;death")

rex_add_clang_frontend_negative_test(
  OMPTEST_rex_openmp_include_diagnostic_between_declarator_and_body
  OMPTEST
  ${_omp_test_output_dir}
  rex_openmp_include_diagnostic_between_declarator_and_body.log
  "error: REX_OPENMP_INCLUDE_BETWEEN_DECLARATOR_AND_BODY_MUST_NOT_BE_SUPPRESSED"
  $<TARGET_FILE:${translator}>
  ${ROSE_C_FLAGS}
  -rose:skipfinalCompileStep
  -I${CMAKE_CURRENT_SOURCE_DIR}
  -c
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_include_diagnostic_between_declarator_and_body.c)

add_test(
  NAME OMPTEST_rex_openmp_clause_list_ownership
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:rex_openmp_clause_list_ownership_translator>
    ${ROSE_C_FLAGS}
    -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_clause_list_ownership.c)
set_tests_properties(OMPTEST_rex_openmp_clause_list_ownership
  PROPERTIES LABELS "OMPTEST;rex;AST")

foreach(_mode null-clause duplicate-clause foreign-clause
              direct-clause-owner wrong-list-owner wrong-variable-owner
              null-variable replace-nonempty-list replace-owned-list
              replace-null-list replace-nonempty-variable-list
              replace-owned-variable-list replace-malformed-variable-list
              remove-null-clause remove-foreign-clause)
  string(REPLACE "-" "_" _mode_name "${_mode}")
  if(_mode STREQUAL "null-clause")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-clause-list\\]: list=0x[[:xdigit:]]+ cannot append a null clause$")
  elseif(_mode STREQUAL "duplicate-clause")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-clause-list\\]: list=0x[[:xdigit:]]+ cannot append the same clause twice$")
  elseif(_mode STREQUAL "foreign-clause")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-clause-list\\]: list=0x[[:xdigit:]]+ cannot take a clause owned by 0x[[:xdigit:]]+$")
  elseif(_mode STREQUAL "direct-clause-owner")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-clause-list\\]: list=0x[[:xdigit:]]+ contains a null clause or a clause with the wrong structural owner$")
  elseif(_mode STREQUAL "wrong-list-owner")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-clause-list\\]: statement=0x[[:xdigit:]]+ type=Sg[^[:space:]]+ has an invalid structural clause-list owner$")
  elseif(_mode STREQUAL "wrong-variable-owner")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-flush-variable-list\\]: statement=0x[[:xdigit:]]+ contains a null variable or one with the wrong owner$")
  elseif(_mode STREQUAL "null-variable")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-flush-variable-list\\]: statement=0x[[:xdigit:]]+ cannot append through an invalid wrapper or append a null or already-owned variable$")
  elseif(_mode STREQUAL "replace-nonempty-list")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-clause-list-replacement\\]: statement=0x[[:xdigit:]]+ type=Sg[^[:space:]]+ requires an exclusively owned empty current wrapper and a distinct unowned replacement$")
  elseif(_mode STREQUAL "replace-owned-list" OR
         _mode STREQUAL "replace-null-list")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-clause-list-replacement\\]: groupprivate=0x[[:xdigit:]]+ requires an exclusively owned empty current wrapper and a distinct unowned replacement$")
  elseif(_mode STREQUAL "replace-nonempty-variable-list" OR
         _mode STREQUAL "replace-owned-variable-list")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-flush-variable-list-replacement\\]: statement=0x[[:xdigit:]]+ requires an exclusively owned empty current wrapper and a distinct unowned replacement$")
  elseif(_mode STREQUAL "replace-malformed-variable-list")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-flush-variable-list-replacement\\]: replacement=0x[[:xdigit:]]+ contains a null variable or one with the wrong owner$")
  elseif(_mode STREQUAL "remove-null-clause" OR
         _mode STREQUAL "remove-foreign-clause")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-clause-list\\]: list=0x[[:xdigit:]]+ cannot remove a clause it does not exclusively own$")
  else()
    message(FATAL_ERROR "unmapped OpenMP ownership corruption mode: ${_mode}")
  endif()
  add_test(
    NAME OMPTEST_rex_openmp_clause_list_ownership_${_mode_name}_is_rejected
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; set +e; \"$@\" >rex_openmp_clause_list_ownership_${_mode_name}.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '${_expected_diagnostic}' rex_openmp_clause_list_ownership_${_mode_name}.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_openmp_clause_list_ownership_translator>
      --${_mode}
      ${ROSE_C_FLAGS}
      -rose:skipfinalCompileStep
      -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_clause_list_ownership.c)
  set_tests_properties(
    OMPTEST_rex_openmp_clause_list_ownership_${_mode_name}_is_rejected
    PROPERTIES LABELS "OMPTEST;rex;AST;death")
endforeach()

add_test(
  NAME OMPTEST_rex_openmp_typed_clause_items
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && grep -Eq 'reduction\\(original\\(private\\), *\\+ *: *reduction_sum\\)' ${_omp_test_output_dir}/rose_rex_openmp_typed_clause_items.c && grep -Eq 'induction\\(\\* *: *induction_value, *step\\(step_value\\), *induction_value\\)' ${_omp_test_output_dir}/rose_rex_openmp_typed_clause_items.c && grep -Eq 'apply\\(grid *: *unroll partial\\(2\\) +apply\\(reverse\\)\\)' ${_omp_test_output_dir}/rose_rex_openmp_typed_clause_items.c && grep -Eq 'init\\(interop, *targetsync *: *object\\)' ${_omp_test_output_dir}/rose_rex_openmp_typed_clause_items.c && grep -Eq 'firstprivate\\(saved *: *allocated_value\\)' ${_omp_test_output_dir}/rose_rex_openmp_typed_clause_items.c && grep -Eq 'adjust_args\\(need_device_addr *: *first, *second\\)' ${_omp_test_output_dir}/rose_rex_openmp_typed_clause_items.c && grep -Eq 'append_args\\(interop\\(target, *targetsync\\), *interop\\(prefer_type\\(rex_vendor_type\\), *target\\)\\)' ${_omp_test_output_dir}/rose_rex_openmp_typed_clause_items.c && grep -Eq 'allocate\\(allocator\\(allocator\\), *align\\(64\\) *: *allocated_value\\)' ${_omp_test_output_dir}/rose_rex_openmp_typed_clause_items.c && grep -Eq 'nowait\\(is_deferred\\)' ${_omp_test_output_dir}/rose_rex_openmp_typed_clause_items.c && grep -Eq 'map\\(tofrom *: *mapped_values\\[0 *: *4\\], *mapped_values\\[4 *: *4\\]\\)' ${_omp_test_output_dir}/rose_rex_openmp_typed_clause_items.c"
    dummy
    $<TARGET_FILE:rex_openmp_typed_clause_items_translator>
    ${ROSE_C_FLAGS}
    -rex:ast-json-checkpoint=post-omp-construction
    -rex:ast-json-dir=${_omp_test_output_dir}/typed-clause-items-json
    -rose:skipfinalCompileStep
    -rose:output ${_omp_test_output_dir}/rose_rex_openmp_typed_clause_items.c
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_typed_clause_items.c)
set_tests_properties(OMPTEST_rex_openmp_typed_clause_items
  PROPERTIES LABELS "OMPTEST;rex;AST")

set(_rex_openmp_absent_contains_output
    ${_omp_test_output_dir}/rose_rex_openmp_absent_contains_typed_contract.c)
set(_rex_openmp_absent_spelling
    "#pragmaompassumesabsent(parallel,for,do,simd,target,teams,distribute,task,taskloop,sections,section,single,master,masked,critical,barrier,taskwait,taskgroup,atomic,flush,ordered,scan,scope,loop,workshare,cancel,metadirective)")
set(_rex_openmp_contains_spelling
    "#pragmaompassumescontains(metadirective,cancel,workshare,loop,scope,scan,ordered,flush,atomic,taskgroup,taskwait,barrier,critical,masked,master,single,section,sections,taskloop,task,distribute,teams,target,simd,do,for,parallel)")
add_test(
  NAME OMPTEST_rex_openmp_absent_contains_typed_contract
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && tr -d '[:space:]' < '${_rex_openmp_absent_contains_output}' | grep -Fq '${_rex_openmp_absent_spelling}' && tr -d '[:space:]' < '${_rex_openmp_absent_contains_output}' | grep -Fq '${_rex_openmp_contains_spelling}' && $<TARGET_FILE:parseOmp> -rose:openmp:ast_only -rose:verbose 0 -rose:skipfinalCompileStep -c '${_rex_openmp_absent_contains_output}'"
    dummy
    $<TARGET_FILE:rex_openmp_absent_contains_typed_contract>
    ${ROSE_C_FLAGS}
    -rex:ast-json-checkpoint=post-omp-construction
    -rex:ast-json-dir=${_omp_test_output_dir}/absent-contains-json
    -rose:skipfinalCompileStep
    -rose:output ${_rex_openmp_absent_contains_output}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_absent_contains_typed_contract.c)
set_tests_properties(OMPTEST_rex_openmp_absent_contains_typed_contract
  PROPERTIES LABELS "OMPTEST;ASTJSON;rex;AST")

foreach(_mode empty unknown duplicate)
  if(_mode STREQUAL "empty")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-directive-kind-list\\]: clause=0x[[:xdigit:]]+ type=SgOmpDirectiveKindClause requires a nonempty typed directive-kind list$")
  elseif(_mode STREQUAL "unknown")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-directive-kind-list\\]: clause=0x[[:xdigit:]]+ type=SgOmpDirectiveKindClause has invalid directive kind=999 at index=0$")
  elseif(_mode STREQUAL "duplicate")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[openmp-directive-kind-list\\]: clause=0x[[:xdigit:]]+ type=SgOmpDirectiveKindClause has duplicate directive kind=1 at index=1$")
  else()
    message(FATAL_ERROR "unmapped absent/contains typed-list corruption mode")
  endif()
  add_test(
    NAME OMPTEST_rex_openmp_absent_contains_${_mode}_is_rejected
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; set +e; \"$@\" >rex_openmp_absent_contains_${_mode}.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '${_expected_diagnostic}' rex_openmp_absent_contains_${_mode}.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_openmp_absent_contains_typed_contract>
      --${_mode})
  set_tests_properties(
    OMPTEST_rex_openmp_absent_contains_${_mode}_is_rejected
    PROPERTIES LABELS "OMPTEST;rex;AST;death")
endforeach()

foreach(_mode item-type bad-first-separator bad-later-separator
              wrong-sizes-type empty-sizes null-sizes-element
              missing-init-modifier-list replacement-null replacement-owned
              replacement-wrong-apply-payload replacement-wrong-init-payload
              replacement-list-wrong-type)
  string(REPLACE "-" "_" _mode_name "${_mode}")
  if(_mode STREQUAL "item-type")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[openmp-apply-transformation-type]: apply transformation is directive syntax, not a value expression, and has no semantic value type")
  elseif(_mode STREQUAL "bad-first-separator")
    set(_expected_diagnostic
        "REX_UNPARSE_INVARIANT[apply]: first transform does not use the none separator")
  elseif(_mode STREQUAL "bad-later-separator")
    set(_expected_diagnostic
        "REX_UNPARSE_INVARIANT[apply]: invalid transform separator")
  elseif(_mode STREQUAL "wrong-sizes-type")
    set(_expected_diagnostic
        "REX_UNPARSE_INVARIANT[openmp-expression-list]: SgOmpSizesClause requires an exact SgExprListExp operand")
  elseif(_mode STREQUAL "empty-sizes")
    set(_expected_diagnostic
        "REX_UNPARSE_INVARIANT[openmp-expression-list]: SgOmpSizesClause requires a nonempty list")
  elseif(_mode STREQUAL "null-sizes-element")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[traversal-null-successor]: null successor in SgExprListExp.p_expressions")
  elseif(_mode STREQUAL "missing-init-modifier-list")
    set(_expected_diagnostic
        "REX_UNPARSE_INVARIANT[init]: missing modifier-list wrapper or invalid wrapper ownership")
  elseif(_mode STREQUAL "replacement-null")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[openmp-induction-replacement]: replacement requires two non-null expressions")
  elseif(_mode STREQUAL "replacement-owned")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[openmp-induction-replacement]: replacement expression already has an owner")
  elseif(_mode STREQUAL "replacement-wrong-apply-payload")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[openmp-apply-replacement]: transformation has an invalid kind, payload, or child ownership")
  elseif(_mode STREQUAL "replacement-wrong-init-payload")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[openmp-init-modifier-replacement]: modifier has an invalid kind, payload, or child ownership")
  elseif(_mode STREQUAL "replacement-list-wrong-type")
    set(_expected_diagnostic
        "REX_AST_INVARIANT[openmp-init-modifier-list-replacement]: replacement is not a valid SgOmpInitModifier payload")
  else()
    message(FATAL_ERROR "unmapped OpenMP corruption mode: ${_mode}")
  endif()
  add_test(
    NAME OMPTEST_rex_openmp_typed_clause_items_${_mode_name}_is_rejected
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; set +e; \"$@\" >rex_openmp_typed_clause_items_${_mode_name}.log 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc '${_expected_diagnostic}' rex_openmp_typed_clause_items_${_mode_name}.log)\" -eq 1"
      dummy
      $<TARGET_FILE:rex_openmp_typed_clause_items_translator>
      --${_mode}
      ${ROSE_C_FLAGS}
      -rose:skipfinalCompileStep
      -rose:output
        ${_omp_test_output_dir}/death_${_mode_name}_rex_openmp_typed_clause_items.c
      -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_typed_clause_items.c)
  set_tests_properties(
    OMPTEST_rex_openmp_typed_clause_items_${_mode_name}_is_rejected
    PROPERTIES LABELS "OMPTEST;rex;AST;death")
endforeach()

set(_rex_openmp_macro_clause_spelling_output
    ${_omp_test_output_dir}/rose_rex_openmp_macro_clause_spelling.c)
add_test(
  NAME OMPTEST_rex_openmp_macro_clause_spelling
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && grep -Fq '#pragma omp parallel num_threads(NUM_THREADS)' ${_rex_openmp_macro_clause_spelling_output} && grep -Fq '#pragma omp taskloop num_tasks(NUM_TASKS)' ${_rex_openmp_macro_clause_spelling_output} && $<TARGET_FILE:parseOmp> -rose:openmp:ast_only -rose:verbose 0 -rose:skipfinalCompileStep -c ${_rex_openmp_macro_clause_spelling_output}"
    --
    $<TARGET_FILE:parseOmp>
    ${ROSE_C_FLAGS}
    -rose:skipfinalCompileStep
    -rose:output ${_rex_openmp_macro_clause_spelling_output}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_macro_clause_spelling.c)
set_tests_properties(OMPTEST_rex_openmp_macro_clause_spelling
  PROPERTIES LABELS "OMPTEST;rex")

set(_rex_openmp_declare_mapper_local_symbol_output
    ${_omp_test_output_dir}/rose_rex_openmp_declare_mapper_local_symbol.c)
add_test(
  NAME OMPTEST_rex_openmp_declare_mapper_local_symbol
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && grep -Fq '#pragma omp declare mapper(rex_whole : RexMapperValue v) map(tofrom : v,v.data[0:v.len])' ${_rex_openmp_declare_mapper_local_symbol_output} && $<TARGET_FILE:parseOmp> -rose:openmp:ast_only -rose:verbose 0 -rose:skipfinalCompileStep -c ${_rex_openmp_declare_mapper_local_symbol_output}"
    --
    $<TARGET_FILE:parseOmp>
    ${ROSE_C_FLAGS}
    -rose:skipfinalCompileStep
    -rose:output ${_rex_openmp_declare_mapper_local_symbol_output}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_declare_mapper_local_symbol.c)
set_tests_properties(OMPTEST_rex_openmp_declare_mapper_local_symbol
  PROPERTIES LABELS "OMPTEST;rex")

add_test(
  NAME OMPTEST_rex_openmp_included_pragma_physical_owner
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:parseOmp>
    ${ROSE_C_FLAGS}
    -rose:skipfinalCompileStep
    -rose:output
      ${_omp_test_output_dir}/rose_rex_openmp_included_pragma_physical_owner.c
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_included_pragma_physical_owner.c)
set_tests_properties(OMPTEST_rex_openmp_included_pragma_physical_owner
  PROPERTIES LABELS "OMPTEST;rex")

set(ROSE_OMP_ANALYZING_FLAGS -rose:openmp:analyzing
  -rose:skipfinalCompileStep -rose:verbose 0)

set(_ast_json_checkpoint_script
    ${CMAKE_CURRENT_SOURCE_DIR}/ast_json_checkpoint/run_checkpoint_compare.sh)

function(add_ast_json_checkpoint_test name checkpoint input_file label)
  set(_workdir ${_omp_test_output_dir}/ast-json/${name})
  add_test(
    NAME ASTJSON_${name}
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND ${BASH_EXECUTABLE}
            ${_ast_json_checkpoint_script}
            $<TARGET_FILE:parseOmp>
            ${checkpoint}
            ${CMAKE_CURRENT_SOURCE_DIR}/${input_file}
            ${_workdir}
            ${input_file}
            ${ARGN})
  set_tests_properties(ASTJSON_${name} PROPERTIES LABELS "ASTJSON;${label}")
endfunction()

add_ast_json_checkpoint_test(
  pre_omp_construction_parallel
  pre-omp-construction
  parallel.c
  OMPTEST
  ${ROSE_C_FLAGS})

add_ast_json_checkpoint_test(
  post_omp_construction_targetparallel
  post-omp-construction
  targetparallel.c
  OMPTEST
  ${ROSE_C_FLAGS})

add_ast_json_checkpoint_test(
  post_omp_construction_axpy_ompacc
  post-omp-construction
  axpy_ompacc.c
  OMPACCTEST
  ${ROSE_C_FLAGS}
  -rose:skipfinalCompileStep)

add_ast_json_checkpoint_test(
  all_omp_construction_task_dep2
  all
  task_dep2.c
  OMPTEST
  ${ROSE_C_FLAGS})

add_ast_json_checkpoint_test(
  all_omp_construction_task_dep3
  all
  task_dep3.c
  OMPTEST
  ${ROSE_C_FLAGS})

add_ast_json_checkpoint_test(
  all_omp_construction_memberFunction_cpp
  all
  memberFunction.cpp
  OMPTEST
  ${ROSE_CXX_FLAGS})

add_ast_json_checkpoint_test(
  all_omp_construction_rex_omp_template_default_semantic_owner_cpp
  all
  rex_omp_template_default_semantic_owner.cpp
  OMPTEST
  ${ROSE_CXX_FLAGS})

add_ast_json_checkpoint_test(
  all_omp_construction_rex_ast_json_noexcept_expression_cpp
  all
  rex_ast_json_noexcept_expression.cpp
  OMPTEST
  ${ROSE_CXX_FLAGS})

add_ast_json_checkpoint_test(
  all_omp_construction_rex_ast_json_type_owned_new_expression_cpp
  all
  rex_ast_json_type_owned_new_expression.cpp
  OMPTEST
  ${ROSE_CXX_FLAGS})

add_ast_json_checkpoint_test(
  all_omp_construction_rex_omp_declare_variant_semantic_identity_c
  all
  rex_omp_declare_variant_semantic_identity.c
  OMPTEST
  ${ROSE_C_FLAGS}
  -rose:skipfinalCompileStep)

add_ast_json_checkpoint_test(
  all_omp_construction_task_link2_cpp
  all
  task_link2.cpp
  OMPTEST
  ${ROSE_CXX_FLAGS})

add_ast_json_checkpoint_test(
  all_omp_construction_rex_test2026_array_section_optional_bounds_cpp
  all
  rex_test2026_array_section_optional_bounds.cpp
  OMPTEST
  ${ROSE_CXX_FLAGS})

add_ast_json_checkpoint_test(
  all_omp_construction_rex_test2026_uses_allocators_optional_edges_c
  all
  rex_test2026_uses_allocators_optional_edges.c
  OMPTEST
  ${ROSE_C_FLAGS})

add_ast_json_checkpoint_test(
  all_omp_construction_rex_test2026_uses_allocators_user_defined_isolation_c
  all
  rex_test2026_uses_allocators_user_defined_isolation.c
  OMPTEST
  ${ROSE_C_FLAGS})

add_ast_json_checkpoint_test(
  all_omp_construction_rex_test2026_mapper_template_reachable_scope_cpp
  all
  rex_test2026_mapper_template_reachable_scope.cpp
  OMPTEST
  ${ROSE_CXX_FLAGS})

add_ast_json_checkpoint_test(
  all_omp_construction_rex_openmp_context_score_constant_expressions_c
  all
  rex_openmp_context_score_constant_expressions.c
  OMPTEST
  ${ROSE_C_FLAGS}
  -rose:skipfinalCompileStep)

foreach(file_to_test ${TESTCODES})
  if(file_to_test MATCHES "\\.c$")
    set(_rose_flags ${ROSE_C_FLAGS})
  else()
    set(_rose_flags ${ROSE_CXX_FLAGS})
  endif()
  add_test(
    NAME OMPTEST_${file_to_test}
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND parseOmp ${_rose_flags}
            -rose:output ${_omp_test_output_dir}/rose_${file_to_test}
            -c
            ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(OMPTEST_${file_to_test} PROPERTIES LABELS OMPTEST)
endforeach()

foreach(file_to_test ${OMP_ACC_TESTCODES})
  add_test(
    NAME OMPACCTEST_${file_to_test}
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND parseOmp ${ROSE_C_FLAGS}
                     -rose:output ${_omp_test_output_dir}/rose_${file_to_test}
                     -rose:skipfinalCompileStep
                     -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(OMPACCTEST_${file_to_test} PROPERTIES LABELS OMPACCTEST)
endforeach()

# Multi-file test: verifies correct handling of multiple source files with OpenMP pragmas
add_test(
  NAME OMPTEST_bonds-2
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND parseOmp ${ROSE_C_FLAGS}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/atoms-2.c
          ${CMAKE_CURRENT_SOURCE_DIR}/bonds-2.c)
set_tests_properties(OMPTEST_bonds-2 PROPERTIES LABELS OMPTEST)

# Test for macro ID handling with comment/directive collection
add_test(
  NAME OMPTEST_macroIds
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND parseOmp ${ROSE_C_FLAGS}
          -rose:output ${_omp_test_output_dir}/rose_macroIds.c
          -rose:collectAllCommentsAndDirectives
          ${CMAKE_CURRENT_SOURCE_DIR}/macroIds.c)
set_tests_properties(OMPTEST_macroIds PROPERTIES LABELS OMPTEST)

set(_rex_test2026_mapper_typedef_output
    ${_omp_test_output_dir}/rex_test2026_mapper_typedef_unparse.c)
add_test(
  NAME OMPTEST_rex_test2026_mapper_typedef_unparse_cpp
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && grep -F '#pragma omp declare mapper(default : struct Vec v)' ${_rex_test2026_mapper_typedef_output} && ! grep -F 'class ::Vec' ${_rex_test2026_mapper_typedef_output} && $<TARGET_FILE:parseOmp> -rose:openmp:ast_only -rose:verbose 0 -rose:skipfinalCompileStep -c ${_rex_test2026_mapper_typedef_output}"
    --
    $<TARGET_FILE:rex_test2026_mapper_typedef_unparse>
    ${ROSE_C_FLAGS}
    -rose:output ${_rex_test2026_mapper_typedef_output}
    -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_openmp_target_update_distinct_items.c)
set_tests_properties(OMPTEST_rex_test2026_mapper_typedef_unparse_cpp
  PROPERTIES LABELS OMPTEST)

set(_rex_test2026_mapper_template_output
    ${_omp_test_output_dir}/rex_test2026_mapper_template_reachable_scope.cpp)
add_test(
  NAME OMPTEST_rex_test2026_mapper_template_reachable_scope_cpp
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "\"$@\" && grep -F '#pragma omp declare mapper(default : RexTest2026Vec<float> v)' ${_rex_test2026_mapper_template_output} && $<TARGET_FILE:parseOmp> -rose:openmp:ast_only -rose:verbose 0 -rose:skipfinalCompileStep -c ${_rex_test2026_mapper_template_output}"
    --
    $<TARGET_FILE:parseOmp>
    ${ROSE_CXX_FLAGS}
    -rose:output ${_rex_test2026_mapper_template_output}
    -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_mapper_template_reachable_scope.cpp)
set_tests_properties(OMPTEST_rex_test2026_mapper_template_reachable_scope_cpp
  PROPERTIES LABELS OMPTEST)

set(_rex_test2026_ast_json_option_dir
    ${_omp_test_output_dir}/rex_test2026_ast_json_option_filter)
add_test(
  NAME OMPTEST_rex_test2026_ast_json_clang_option_filter_cpp
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=pre-omp-construction
    REX_AST_JSON_DIR=${_rex_test2026_ast_json_option_dir}
    $<TARGET_FILE:parseOmp>
      ${ROSE_CXX_FLAGS}
      -rex:ast-json-checkpoint=pre-omp-construction
      -rex:ast-json-dir=${_rex_test2026_ast_json_option_dir}
      -rose:output ${_omp_test_output_dir}/rose_rex_test2026_ast_json_clang_option_filter.cpp
      -c ${CMAKE_CURRENT_SOURCE_DIR}/../Cxx_tests/rex_test2026_ast_json_clang_option_filter.cpp)
set_tests_properties(OMPTEST_rex_test2026_ast_json_clang_option_filter_cpp
  PROPERTIES LABELS "OMPTEST;ASTJSON")

set(_rex_test2026_omp_expr_lookup_dir
    ${_omp_test_output_dir}/rex_test2026_omp_expr_lookup_current_file)
add_test(
  NAME OMPTEST_rex_test2026_omp_expr_lookup_current_file_cpp
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND ${CMAKE_COMMAND} -E env
    REX_AST_JSON_CHECKPOINT=all
    REX_AST_JSON_DIR=${_rex_test2026_omp_expr_lookup_dir}
    $<TARGET_FILE:parseOmp>
      ${ROSE_CXX_FLAGS}
      -rex:ast-json-checkpoint=all
      -rex:ast-json-dir=${_rex_test2026_omp_expr_lookup_dir}
      -rose:output ${_omp_test_output_dir}/rose_rex_test2026_omp_expr_lookup_current_file.cpp
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_omp_expr_lookup_current_file_a.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_omp_expr_lookup_current_file_b.cpp)
set_tests_properties(OMPTEST_rex_test2026_omp_expr_lookup_current_file_cpp
  PROPERTIES LABELS "OMPTEST;ASTJSON")

set(_omp_reference_dir ${CMAKE_CURRENT_SOURCE_DIR}/referenceResults)

function(add_omp_output_diff_tests input_file dependency label)
  string(REPLACE "." "_" _name "${input_file}")
  string(REPLACE "-" "_" _name "${_name}")
  set(_rose_file rose_${input_file})
  set(_output_file ${_rose_file}.output)
  set(_diff_file ${_output_file}.diff)
  if(ARGC GREATER 3)
    set(_reference_file ${ARGV3})
  else()
    set(_reference_file ${_output_file})
  endif()
  set(_reference_path ${_omp_reference_dir}/${_reference_file})
  if(NOT EXISTS "${_reference_path}")
    message(FATAL_ERROR
      "Missing exact OpenMP output reference for ${input_file}: "
      "${_reference_path}")
  endif()
  file(SIZE "${_reference_path}" _reference_size)

  if(_reference_size EQUAL 0)
    set(_extract_command
      "test -s '${_rose_file}'; set +e; grep -E \"^[[:space:]]*(#pragma[[:space:]]+omp|//[[:space:]]*#pragma[[:space:]]+omp|/\\*[[:space:]]*#pragma[[:space:]]+omp.*)\" '${_rose_file}' > '${_output_file}'; status=$?; set -e; test $status -eq 1; test ! -s '${_output_file}'")
  else()
    set(_extract_command
      "test -s '${_rose_file}' && grep -E \"^[[:space:]]*(#pragma[[:space:]]+omp|//[[:space:]]*#pragma[[:space:]]+omp|/\\*[[:space:]]*#pragma[[:space:]]+omp.*)\" '${_rose_file}' > '${_output_file}' && test -s '${_output_file}'")
  endif()

  add_test(
    NAME OMPTEST_output_${_name}
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c "${_extract_command}"
  )
  set_tests_properties(OMPTEST_output_${_name} PROPERTIES DEPENDS "${dependency}" LABELS "${label}")

  add_test(
    NAME OMPTEST_output_diff_${_name}
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "if diff ${_output_file} ${_omp_reference_dir}/${_reference_file} > ${_diff_file}; then rm -f ${_diff_file}; exit 0; else cat ${_diff_file}; rm -f ${_diff_file}; exit 1; fi"
  )
  set_tests_properties(OMPTEST_output_diff_${_name} PROPERTIES DEPENDS OMPTEST_output_${_name} LABELS "${label}")
endfunction()

foreach(file_to_test IN LISTS C_TESTCODES CXX_TESTCODES)
  add_omp_output_diff_tests(${file_to_test} OMPTEST_${file_to_test} OMPTEST)
endforeach()

# These two round-trip tests consume outputs from a three-stage chain.  Gate
# every stage on successful completion of the preceding producer so stale
# generated files cannot satisfy a later test after an earlier failure.
set_tests_properties(OMPTEST_rex_openmp_target_update_distinct_items.c
  PROPERTIES FIXTURES_SETUP rex_openmp_target_update_translation)
set_tests_properties(OMPTEST_output_rex_openmp_target_update_distinct_items_c
  PROPERTIES
    FIXTURES_REQUIRED rex_openmp_target_update_translation
    FIXTURES_SETUP rex_openmp_target_update_directives)
set_tests_properties(
  OMPTEST_output_diff_rex_openmp_target_update_distinct_items_c
  PROPERTIES
    FIXTURES_REQUIRED rex_openmp_target_update_directives
    FIXTURES_SETUP rex_openmp_target_update_reference_match)

set_tests_properties(OMPTEST_rex_openmp_mapper_type_qualifiers_valid.cpp
  PROPERTIES FIXTURES_SETUP rex_openmp_mapper_qualifiers_translation)
set_tests_properties(OMPTEST_output_rex_openmp_mapper_type_qualifiers_valid_cpp
  PROPERTIES
    FIXTURES_REQUIRED rex_openmp_mapper_qualifiers_translation
    FIXTURES_SETUP rex_openmp_mapper_qualifiers_directives)
set_tests_properties(
  OMPTEST_output_diff_rex_openmp_mapper_type_qualifiers_valid_cpp
  PROPERTIES
    FIXTURES_REQUIRED rex_openmp_mapper_qualifiers_directives
    FIXTURES_SETUP rex_openmp_mapper_qualifiers_reference_match)

# Exact reference matching alone does not prove that an updated reference is
# valid OpenMP source.  Reparse the two REX-owned replacements after their
# generated directive streams have matched the checked-in references.
add_test(
  NAME OMPTEST_roundtrip_rex_openmp_target_update_distinct_items_c
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:parseOmp>
    ${ROSE_C_FLAGS}
    -rose:skip_unparse
    -rose:skipfinalCompileStep
    -c
    ${_omp_test_output_dir}/rose_rex_openmp_target_update_distinct_items.c)
set_tests_properties(
  OMPTEST_roundtrip_rex_openmp_target_update_distinct_items_c
  PROPERTIES
    FIXTURES_REQUIRED rex_openmp_target_update_reference_match
    LABELS "OMPTEST;rex")

add_test(
  NAME OMPTEST_roundtrip_rex_openmp_mapper_type_qualifiers_valid_cpp
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND $<TARGET_FILE:parseOmp>
    ${ROSE_CXX_FLAGS}
    -rose:skip_unparse
    -rose:skipfinalCompileStep
    -c
    ${_omp_test_output_dir}/rose_rex_openmp_mapper_type_qualifiers_valid.cpp)
set_tests_properties(
  OMPTEST_roundtrip_rex_openmp_mapper_type_qualifiers_valid_cpp
  PROPERTIES
    FIXTURES_REQUIRED rex_openmp_mapper_qualifiers_reference_match
    LABELS "OMPTEST;rex")

foreach(file_to_test IN LISTS OMP_ACC_TESTCODES)
  if(file_to_test STREQUAL "rex_openmp_axpy_multi_device.c")
    add_omp_output_diff_tests(${file_to_test} OMPACCTEST_${file_to_test}
      OMPACCTEST rose_axpy_ompacc2.c.output)
  else()
    add_omp_output_diff_tests(${file_to_test} OMPACCTEST_${file_to_test}
      OMPACCTEST)
  endif()
endforeach()

add_omp_output_diff_tests(bonds-2.c OMPTEST_bonds-2 OMPTEST)

add_omp_output_diff_tests(macroIds.c OMPTEST_macroIds OMPTEST)

# Focused OpenMP analyzer regressions.
add_test(
  NAME OMPANALYZE_schedule_default_for
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND checkOmpAnalyzing
          --rex-check=schedule-default-for
          ${ROSE_OMP_ANALYZING_FLAGS}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/ompfor-default.c)
set_tests_properties(OMPANALYZE_schedule_default_for PROPERTIES LABELS OMPANALYZE)

add_test(
  NAME OMPANALYZE_schedule_dynamic_for
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND checkOmpAnalyzing
          --rex-check=schedule-dynamic-for
          ${ROSE_OMP_ANALYZING_FLAGS}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/ompfor4.c)
set_tests_properties(OMPANALYZE_schedule_dynamic_for PROPERTIES LABELS OMPANALYZE)

if(ENABLE-FORTRAN)
  add_test(
    NAME OMPANALYZE_schedule_default_do
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND checkOmpAnalyzing
            --rex-check=schedule-default-do
            ${ROSE_OMP_ANALYZING_FLAGS}
            -I${CMAKE_SOURCE_DIR}/src/frontend/SageIII
            -c ${CMAKE_CURRENT_SOURCE_DIR}/fortran/ompdo-default.f)
  set_tests_properties(OMPANALYZE_schedule_default_do PROPERTIES LABELS OMPANALYZE)
endif()

add_test(
  NAME OMPANALYZE_target_implicit_map
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND checkOmpAnalyzing
          --rex-check=target-implicit-map
          ${ROSE_OMP_ANALYZING_FLAGS}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/analyzing_target_implicit_map.c)
set_tests_properties(OMPANALYZE_target_implicit_map PROPERTIES LABELS OMPANALYZE)

# Rodinia-derived OpenMP lowering regressions (separate from AST output refs).
add_subdirectory(lowering_rodinia)

# CPU lowering semantic equivalence (original vs lowered executable behavior).
add_subdirectory(lowering_cpu)
