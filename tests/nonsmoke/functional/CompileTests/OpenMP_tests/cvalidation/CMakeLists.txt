##########################################################################
# These are OpenMP C testcodes targeting aspects of OpenMP specific to C #
##########################################################################

set(TESTCODES for_firstprivate.c for_lastprivate.c)

if(with-ROSE_LONG_MAKE_CHECK_RULE)
  list(APPEND TESTCODES
    for_ordered.c for_private.c for_reduction.c for_schedule_dynamic.c
    for_schedule_guided.c for_schedule_static.c has_openmp.c omp_atomic.c
    omp_barrier.c omp_copyin.c omp_critical.c omp_flush.c omp_get_num_threads.c
    omp_in_parallel.c omp_lock.c omp_master_thread.c omp_nested.c omp_nest_lock.c
    omp_nest_testlock.c omp_num_threads.c omp_testlock.c omp_threadprivate.c
    omp_ticks_time.c omp_time.c parallel_for_firstprivate.c
    parallel_for_lastprivate.c parallel_for_ordered.c parallel_for_private.c
    parallel_for_reduction.c parallel_section_firstprivate.c
    parallel_section_lastprivate.c parallel_section_private.c
    parallel_section_reduction.c section_firstprivate.c section_lastprivate.c
    section_private.c section_reduction.c single.c single_copyprivate.c
    single_nowait.c single_private.c main.c omp_pragma_example1.c
    omp_pragma_example2.c)
endif()

set(ROSE_FLAGS -std=c99 -rose:C_only  -w -rose:verbose 0
               -rose:openmp -rose:openmp:ast_only)

foreach(file_to_test ${TESTCODES})
  add_test(
    NAME OMPTEST_${file_to_test}
    COMMAND parseOmp ${ROSE_FLAGS} -I${CMAKE_SOURCE_DIR}/src/frontend/SageIII
                     -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(OMPTEST_${file_to_test} PROPERTIES LABELS OMPTEST)
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_OpenMP_tests_cvalidation_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=OMPTEST_for_firstprivate.c\;OMPTEST_for_lastprivate.c\;OMPTEST_for_ordered.c\;OMPTEST_for_private.c\;OMPTEST_for_reduction.c\;OMPTEST_for_schedule_dynamic.c\;OMPTEST_for_schedule_guided.c\;OMPTEST_for_schedule_static.c\;OMPTEST_has_openmp.c\;OMPTEST_main.c\;OMPTEST_omp_atomic.c\;OMPTEST_omp_barrier.c\;OMPTEST_omp_copyin.c\;OMPTEST_omp_critical.c\;OMPTEST_omp_flush.c\;OMPTEST_omp_get_num_threads.c\;OMPTEST_omp_in_parallel.c\;OMPTEST_omp_lock.c\;OMPTEST_omp_master_thread.c\;OMPTEST_omp_nest_lock.c\;OMPTEST_omp_nest_testlock.c\;OMPTEST_omp_nested.c\;OMPTEST_omp_num_threads.c\;OMPTEST_omp_pragma_example1.c\;OMPTEST_omp_pragma_example2.c\;OMPTEST_omp_testlock.c\;OMPTEST_omp_threadprivate.c\;OMPTEST_omp_ticks_time.c\;OMPTEST_omp_time.c\;OMPTEST_parallel_for_firstprivate.c\;OMPTEST_parallel_for_lastprivate.c\;OMPTEST_parallel_for_ordered.c\;OMPTEST_parallel_for_private.c\;OMPTEST_parallel_for_reduction.c\;OMPTEST_parallel_section_firstprivate.c\;OMPTEST_parallel_section_lastprivate.c\;OMPTEST_parallel_section_private.c\;OMPTEST_parallel_section_reduction.c\;OMPTEST_section_firstprivate.c\;OMPTEST_section_lastprivate.c\;OMPTEST_section_private.c\;OMPTEST_section_reduction.c\;OMPTEST_single.c\;OMPTEST_single_copyprivate.c\;OMPTEST_single_nowait.c\;OMPTEST_single_private.c
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_OpenMP_tests_cvalidation_check-local PROPERTIES DEPENDS "OMPTEST_for_firstprivate.c;OMPTEST_for_lastprivate.c;OMPTEST_for_ordered.c;OMPTEST_for_private.c;OMPTEST_for_reduction.c;OMPTEST_for_schedule_dynamic.c;OMPTEST_for_schedule_guided.c;OMPTEST_for_schedule_static.c;OMPTEST_has_openmp.c;OMPTEST_main.c;OMPTEST_omp_atomic.c;OMPTEST_omp_barrier.c;OMPTEST_omp_copyin.c;OMPTEST_omp_critical.c;OMPTEST_omp_flush.c;OMPTEST_omp_get_num_threads.c;OMPTEST_omp_in_parallel.c;OMPTEST_omp_lock.c;OMPTEST_omp_master_thread.c;OMPTEST_omp_nest_lock.c;OMPTEST_omp_nest_testlock.c;OMPTEST_omp_nested.c;OMPTEST_omp_num_threads.c;OMPTEST_omp_pragma_example1.c;OMPTEST_omp_pragma_example2.c;OMPTEST_omp_testlock.c;OMPTEST_omp_threadprivate.c;OMPTEST_omp_ticks_time.c;OMPTEST_omp_time.c;OMPTEST_parallel_for_firstprivate.c;OMPTEST_parallel_for_lastprivate.c;OMPTEST_parallel_for_ordered.c;OMPTEST_parallel_for_private.c;OMPTEST_parallel_for_reduction.c;OMPTEST_parallel_section_firstprivate.c;OMPTEST_parallel_section_lastprivate.c;OMPTEST_parallel_section_private.c;OMPTEST_parallel_section_reduction.c;OMPTEST_section_firstprivate.c;OMPTEST_section_lastprivate.c;OMPTEST_section_private.c;OMPTEST_section_reduction.c;OMPTEST_single.c;OMPTEST_single_copyprivate.c;OMPTEST_single_nowait.c;OMPTEST_single_private.c" )
