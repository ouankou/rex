add_executable(parseOmp parseOmp.C)
target_link_libraries(parseOmp ROSE_DLL ${link_with_libraries})

if(enable-c)
  add_subdirectory(cvalidation)
endif()

if(enable-fortran)
  add_subdirectory(fortran)
endif()

if(NOT enable-c)
  return()
endif()

# C test codes - synced with Autotools REX_C_TESTCODES_REQUIRED_TO_PASS
set(C_TESTCODES
	3loops.c affinity.c allocate.c jacobi.c jacobi-collapse.c alignment.c
	array_init.c array_init_2.c atoms-2.c atomic.c atomic-3.c barrier.c
	cancel.c cancellation_point.c collapse.c copyin.c copyprivate2.c copyprivate3.c
	copyprivate.c critical.c critical1.c critical_dead.c critical_orphaned.c
	depobj.c task_dep1.c task_dep3.c task_dep_iterator1.c task_dep_iterator2.c
	declare_simd.c distribute_parallel_for.c distribute_parallel_for_simd.c
	distribute_simd.c distribute.c distribute2.c dijkstra_open_mp.c dynamicChunk.c
	empty.c endif.c endif2.c endif3.c nowait.c expressions.c falsesharing.c
	firstprivate.c firstPrivateArray.c firstPrivateArray_2.c firstlastprivate.c
	flush.c flush_exampleA_21_1c.c full_verify.c flush_with_clauses.c
	get_max_threads.c hello.c hello-1.c hello-2.c hello-ordered.c
	include_header_only.c init.c lastprivate0.c lastprivate.c lastprivateOrphaned.c
	limits_threads.c linebreak.c lockarray.c loop.c loop1.c lu_factorization.c
	master.c masterSingle.c mastertaskloop.c mastertaskloopsimd.c matrix_vector.c
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
	targetparallelforsimd.c targetparallelloop.c targetsimd.c targetteams.c
	targetteamsdistribute.c targetteamsdistributesimd.c targetteamsloop.c
	targetteamsdistributeparallelfor.c targetteamsdistributeparallelforsimd.c
	targetupdate.c targetupdate_nowait.c targetupdate_device.c target_update_from.c
	target_update_to.c taskloop_simd_reduction.1.c task_affinity.c task_affinity1.c
	task_array.c task_array2.c task_final.c task_largenumber.c task_mergeable.c
	task_orphaned.c task_outlining.c task_priority.c task_untied.c task_untied2.c
	task_untied3.c task_untied4.c task_underIf.c taskgroup.c taskloop.c
	taskwait.c taskwait2.c taskwait3.c taskwait4.c taskwait5.c taskyield.c
	threadprivate.c threadprivate2.c threadprivate3.c teams.c teamsdistribute.c
	teamsdistributesimd.c teamsdistributeparallelfor.c
	teamsdistributeparallelforsimd.c teamsloop.c twoRegions.c threadProcessor.c
	upperCase.c variables.c)

# C++ test codes - synced with Autotools REX_CXX_TESTCODES_REQUIRED_TO_PASS
# Plus additional tests to track and fix with CFE
set(CXX_TESTCODES
	classMember.cpp hello1.cpp helloNested.cpp memberFunction.cpp
	memberFunction2.cpp objectPrivate.cpp objectFirstPrivate.cpp
	objectLastprivate.cpp referenceType.cpp
	orphanedAtomic.cpp preprocessingInfo2.cpp task_link.cpp task_link2.cpp
	task_tree.cpp)

set(TESTCODES ${C_TESTCODES} ${CXX_TESTCODES})

# OpenMP+OpenACC combined test codes - synced with Autotools REX_C_OMP_ACC_TESTCODES_REQUIRED_TO_PASS
set(OMP_ACC_TESTCODES
  axpy_ompacc.c axpy_ompacc2.c axpy_ompacc3.c
  matrixmultiply-ompacc.c matrixmultiply-ompacc2.c
  jacobi-ompacc.c jacobi-ompacc-v2.c jacobi-ompacc-opt1.c jacobi-ompacc-opt2.c
  jacobi-ompacc-multiGPU.c)

# ROSE flags for OpenMP parsing (CFE frontend only)
set(ROSE_FLAGS -rose:openmp:ast_only -w -rose:verbose 0)

# Include path to find omp.h header
set(TESTCODE_INCLUDES -I${CMAKE_SOURCE_DIR}/src/frontend/SageIII)

foreach(file_to_test ${TESTCODES})
  add_test(
    NAME OMPTEST_${file_to_test}
    COMMAND parseOmp ${ROSE_FLAGS} ${TESTCODE_INCLUDES} -c
            ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(OMPTEST_${file_to_test} PROPERTIES LABELS OMPTEST)
endforeach()

foreach(file_to_test ${OMP_ACC_TESTCODES})
  add_test(
    NAME OMPACCTEST_${file_to_test}
    COMMAND parseOmp -rose:openmp:ast_only -rose:skipfinalCompileStep
                     -w -rose:verbose 0
                     -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(OMPACCTEST_${file_to_test} PROPERTIES LABELS OMPACCTEST)
endforeach()


# Multi-file test: verifies correct handling of multiple source files with OpenMP pragmas
add_test(
  NAME OMPTEST_bonds-2
  COMMAND parseOmp ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/atoms-2.c
          ${CMAKE_CURRENT_SOURCE_DIR}/bonds-2.c)
set_tests_properties(OMPTEST_bonds-2 PROPERTIES LABELS OMPTEST)

# Test for macro ID handling with comment/directive collection
add_test(
  NAME OMPTEST_macroIds
  COMMAND parseOmp ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
          -rose:collectAllCommentsAndDirectives
          ${CMAKE_CURRENT_SOURCE_DIR}/macroIds.c)
set_tests_properties(OMPTEST_macroIds PROPERTIES LABELS OMPTEST)
