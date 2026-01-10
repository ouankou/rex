include_directories(${ROSE_INCLUDES})

add_executable(roseomp roseomp.C)
target_link_libraries(roseomp ROSE_DLL ${link_with_libraries})

add_executable(roseompacc roseompacc.C)
target_link_libraries(roseompacc ROSE_DLL ${link_with_libraries})

set(OMP_C_COMPILE_TESTS
  ompfor5.c
  ompfor6.c
  ompfor7.c
  twoRegions.c
)

set(OMP_C_RUN_TESTS
  barrier.c
  ompfor.c
  ompfor_c99.c
  ompfor3.c
  ompfor4.c
  ompfor-default.c
  ompfor-decremental.c
  ompfor-static.c
  parallel.c
  parallel-if.c
  parallel-if-numthreads.c
  parallel-numthreads.c
  parallel-reduction.c
  parallel-reduction2.c
  parallelfor2.c
  single.c
  single3.c
  omp_version.c
)

set(OMP_REX_DIFF_DISABLED_TESTS
  ompfor5.c
  ompfor6.c
  ompfor7.c
  twoRegions.c
  barrier.c
  ompfor.c
  ompfor_c99.c
  ompfor3.c
  ompfor4.c
  ompfor-default.c
  ompfor-decremental.c
  ompfor-static.c
  parallel.c
  parallel-if.c
  parallel-if-numthreads.c
  parallel-numthreads.c
  parallel-reduction.c
  parallel-reduction2.c
  parallelfor2.c
  single.c
  single3.c
  omp_version.c
)

set(OMP_CXX_COMPILE_TESTS
  memberFunction.cpp
  memberFunction2.cpp
  referenceType.cpp
)

set(OMP_CXX_RUN_TESTS
  hello1.cpp
  helloNested.cpp
  objectPrivate.cpp
  objectFirstPrivate.cpp
  objectLastprivate.cpp
  orphanedAtomic.cpp
  preprocessingInfo2.cpp
  task_link.cpp
  task_link2.cpp
  task_tree.cpp
)

set(OMP_ACC_C_TESTS
  collapse_2.c
  axpy_ompacc.c
  axpy_ompacc3.c
  matrixmultiply-ompacc.c
  matrixmultiply-ompacc2.c
  jacobi-ompacc.c
  jacobi-ompacc-v2.c
  jacobi-ompacc-opt1.c
  jacobi-ompacc-opt2.c
  jacobi-ompacc-multiGPU.c
)

set(OMP_ACC_C_DISABLED_TESTS
  axpy_ompacc2.c
  axpy_ompacc4.c
)

set(OMP_ACC_CXX_TESTS
)

set(OMP_C_COMPILE_DISABLED_TESTS
  alignment.c
  collapse.c
  dijkstra_open_mp.c
  dynamicChunk.c
  endif.c
  endif3.c
  full_verify.c
  include_header_only.c
  init.c
  lastprivateOrphaned.c
  linebreak.c
  master.c
  md_open_mp.c
  nowait.c
  ompfor8.c
  parallel_pragma.c
  private-duplicate.c
  single2.c
  sizeof.c
  staticChunk.c
  task_array2.c
  task_orphaned.c
  task_underIf.c
  task_untied4.c
  task_wait.c
  task_wait2.c
)

set(OMP_C_RUN_DISABLED_TESTS
  3loops.c
  array_init.c
  array_init_2.c
  atomic.c
  critical.c
  critical_orphaned.c
  empty.c
  falsesharing.c
  firstPrivateArray.c
  firstPrivateArray_2.c
  firstlastprivate.c
  firstprivate.c
  flush.c
  get_max_threads.c
  hello-1.c
  hello-2.c
  hello-ordered.c
  hello.c
  jacobi-collapse.c
  jacobi.c
  lastprivate.c
  lastprivate0.c
  lockarray.c
  loop1.c
  masterSingle.c
  matrixmultiply.c
  multiple_return.c
  nestedpar.c
  nestedpar1.c
  omp1.c
  ompGetNumThreads.c
  ompfor2.c
  ordered.c
  ordered2.c
  orphaned-directives.c
  parallelfor.c
  parallelsections.c
  pi.c
  prettyfunc.c
  private.c
  privatej.c
  reduction-classic.c
  reduction.c
  reduction2.c
  rice1.c
  section.c
  section1.c
  set_num_threads.c
  spmd1.c
  subteam.c
  subteam2.c
  task_largenumber.c
  task_outlining.c
  task_untied.c
  task_untied2.c
  task_untied3.c
  threadprivate.c
  threadprivate2.c
  threadprivate3.c
)

set(OMP_ACC_CXX_DISABLED_TESTS
  jacobi-ompacc2.cpp
)

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

set(_omp_test_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/OpenMP_tests)
set(_omp_test_includes
  -I${CMAKE_SOURCE_DIR}/src/frontend/SageIII
  -I${CMAKE_SOURCE_DIR}/src/midend/programTransformation/ompLowering
  -I${CMAKE_BINARY_DIR})

set(_omp_flags ${_omp_test_includes} -rose:openmp:lowering -g)
set(_omp_acc_flags ${_omp_test_includes} -rose:openmp:lowering -g)

add_test(
  NAME single2.o
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:roseomp> ${_omp_flags} --edg:no_warnings -c ${_omp_test_dir}/single2.c
)
set_tests_properties(single2.o PROPERTIES DISABLED TRUE)

add_test(
  NAME sizeof.o
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:roseomp> ${_omp_flags} --edg:no_warnings -c ${_omp_test_dir}/sizeof.c
)
set_tests_properties(sizeof.o PROPERTIES DISABLED TRUE)

foreach(testcode IN LISTS OMP_C_COMPILE_TESTS)
  string(REPLACE "." "_" _omp_name ${testcode})
  set(_omp_target roseomp_compile_${testcode}.passed)
  add_test(
    NAME omp_lowering_c_compile_${_omp_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="roseomp ${testcode} [${_omp_target}]"
      CMD="./roseomp ${_omp_flags} -c ${_omp_test_dir}/${testcode}"
      ${_exit_status}
      ${_omp_target}
  )
endforeach()

foreach(testcode IN LISTS OMP_C_COMPILE_DISABLED_TESTS)
  string(REPLACE "." "_" _omp_name ${testcode})
  set(_omp_target roseomp_compile_${testcode}.passed)
  add_test(
    NAME omp_lowering_c_compile_${_omp_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="roseomp ${testcode} [${_omp_target}]"
      CMD="./roseomp ${_omp_flags} -c ${_omp_test_dir}/${testcode}"
      ${_exit_status}
      ${_omp_target}
  )
  set_tests_properties(omp_lowering_c_compile_${_omp_name} PROPERTIES DISABLED TRUE)
endforeach()

foreach(testcode IN LISTS OMP_C_RUN_TESTS)
  string(REPLACE "." "_" _omp_name ${testcode})
  set(_omp_target roseomp_run_${testcode}.passed)
  add_test(
    NAME omp_lowering_c_run_${_omp_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="roseomp ${testcode} [${_omp_target}]"
      CMD="./roseomp ${_omp_flags} -c ${_omp_test_dir}/${testcode}"
      ${_exit_status}
      ${_omp_target}
  )
endforeach()

foreach(testcode IN LISTS OMP_C_RUN_DISABLED_TESTS)
  string(REPLACE "." "_" _omp_name ${testcode})
  set(_omp_target roseomp_run_${testcode}.passed)
  add_test(
    NAME omp_lowering_c_run_${_omp_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="roseomp ${testcode} [${_omp_target}]"
      CMD="./roseomp ${_omp_flags} -c ${_omp_test_dir}/${testcode}"
      ${_exit_status}
      ${_omp_target}
  )
  set_tests_properties(omp_lowering_c_run_${_omp_name} PROPERTIES DISABLED TRUE)
endforeach()

foreach(testcode IN LISTS OMP_CXX_COMPILE_TESTS)
  string(REPLACE "." "_" _omp_name ${testcode})
  set(_omp_target roseomp_cxx_compile_${testcode}.passed)
  add_test(
    NAME omp_lowering_cxx_compile_${_omp_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="roseomp ${testcode} [${_omp_target}]"
      CMD="./roseomp ${_omp_flags} -c ${_omp_test_dir}/${testcode}"
      ${_exit_status}
      ${_omp_target}
  )
endforeach()

foreach(testcode IN LISTS OMP_CXX_RUN_TESTS)
  string(REPLACE "." "_" _omp_name ${testcode})
  set(_omp_target roseomp_cxx_run_${testcode}.passed)
  add_test(
    NAME omp_lowering_cxx_run_${_omp_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="roseomp ${testcode} [${_omp_target}]"
      CMD="./roseomp ${_omp_flags} -c ${_omp_test_dir}/${testcode}"
      ${_exit_status}
      ${_omp_target}
  )
endforeach()

foreach(testcode IN LISTS OMP_ACC_C_TESTS)
  string(REPLACE "." "_" _omp_name ${testcode})
  set(_omp_target roseompacc_${testcode}.passed)
  add_test(
    NAME omp_acc_lowering_c_${_omp_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="roseompacc ${testcode} [${_omp_target}]"
      CMD="./roseompacc ${_omp_acc_flags} -rose:skipfinalCompileStep -c ${_omp_test_dir}/${testcode}"
      ${_exit_status}
      ${_omp_target}
  )
endforeach()

foreach(testcode IN LISTS OMP_ACC_C_DISABLED_TESTS)
  string(REPLACE "." "_" _omp_name ${testcode})
  set(_omp_target roseompacc_${testcode}.passed)
  add_test(
    NAME omp_acc_lowering_c_${_omp_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="roseompacc ${testcode} [${_omp_target}]"
      CMD="./roseompacc ${_omp_acc_flags} -rose:skipfinalCompileStep -c ${_omp_test_dir}/${testcode}"
      ${_exit_status}
      ${_omp_target}
  )
  set_tests_properties(omp_acc_lowering_c_${_omp_name} PROPERTIES DISABLED TRUE)
endforeach()

foreach(testcode IN LISTS OMP_ACC_CXX_TESTS)
  string(REPLACE "." "_" _omp_name ${testcode})
  set(_omp_target roseompacc_${testcode}.passed)
  add_test(
    NAME omp_acc_lowering_cxx_${_omp_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="roseompacc ${testcode} [${_omp_target}]"
      CMD="./roseompacc ${_omp_acc_flags} -rose:skipfinalCompileStep -c ${_omp_test_dir}/${testcode}"
      ${_exit_status}
      ${_omp_target}
  )
endforeach()

foreach(testcode IN LISTS OMP_ACC_CXX_DISABLED_TESTS)
  string(REPLACE "." "_" _omp_name ${testcode})
  set(_omp_target roseompacc_${testcode}.passed)
  add_test(
    NAME omp_acc_lowering_cxx_${_omp_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="roseompacc ${testcode} [${_omp_target}]"
      CMD="./roseompacc ${_omp_acc_flags} -rose:skipfinalCompileStep -c ${_omp_test_dir}/${testcode}"
      ${_exit_status}
      ${_omp_target}
  )
  set_tests_properties(omp_acc_lowering_cxx_${_omp_name} PROPERTIES DISABLED TRUE)
endforeach()

set(_omp_rex_reference_dir ${CMAKE_CURRENT_SOURCE_DIR}/REXReferenceTest)
foreach(testcode IN LISTS OMP_C_COMPILE_TESTS)
  string(REPLACE "." "_" _omp_name ${testcode})
  add_test(
    NAME omp_lowering_rex_diff_${_omp_name}
    COMMAND ${CMAKE_COMMAND} -E compare_files
      ${CMAKE_CURRENT_BINARY_DIR}/rose_${testcode}
      ${_omp_rex_reference_dir}/rex_${testcode}
  )
  set_tests_properties(omp_lowering_rex_diff_${_omp_name} PROPERTIES DEPENDS omp_lowering_c_compile_${_omp_name})
  if(testcode IN_LIST OMP_REX_DIFF_DISABLED_TESTS)
    set_tests_properties(omp_lowering_rex_diff_${_omp_name} PROPERTIES DISABLED TRUE)
  endif()
endforeach()

foreach(testcode IN LISTS OMP_C_RUN_TESTS)
  string(REPLACE "." "_" _omp_name ${testcode})
  add_test(
    NAME omp_lowering_rex_diff_${_omp_name}
    COMMAND ${CMAKE_COMMAND} -E compare_files
      ${CMAKE_CURRENT_BINARY_DIR}/rose_${testcode}
      ${_omp_rex_reference_dir}/rex_${testcode}
  )
  set_tests_properties(omp_lowering_rex_diff_${_omp_name} PROPERTIES DEPENDS omp_lowering_c_run_${_omp_name})
  if(testcode IN_LIST OMP_REX_DIFF_DISABLED_TESTS)
    set_tests_properties(omp_lowering_rex_diff_${_omp_name} PROPERTIES DISABLED TRUE)
  endif()
endforeach()

add_subdirectory(fortran)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_ompLoweringTests_conditional-check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=OMPTEST_memberFunction.cpp\;OMPTEST_memberFunction2.cpp\;OMPTEST_ompfor5.c\;OMPTEST_ompfor6.c\;OMPTEST_ompfor7.c\;OMPTEST_referenceType.cpp\;OMPTEST_twoRegions.c
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_ompLoweringTests_conditional-check-local PROPERTIES DEPENDS "OMPTEST_memberFunction.cpp;OMPTEST_memberFunction2.cpp;OMPTEST_ompfor5.c;OMPTEST_ompfor6.c;OMPTEST_ompfor7.c;OMPTEST_referenceType.cpp;OMPTEST_twoRegions.c" )
add_test(
  NAME nonsmoke_functional_roseTests_ompLoweringTests_ompacc_diff_check
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=OMPTEST_alignment.c\;OMPTEST_collapse.c\;OMPTEST_dijkstra_open_mp.c\;OMPTEST_dynamicChunk.c\;OMPTEST_endif.c\;OMPTEST_endif3.c\;OMPTEST_full_verify.c\;OMPTEST_include_header_only.c\;OMPTEST_init.c\;OMPTEST_lastprivateOrphaned.c\;OMPTEST_linebreak.c\;OMPTEST_master.c\;OMPTEST_md_open_mp.c\;OMPTEST_memberFunction.cpp\;OMPTEST_memberFunction2.cpp\;OMPTEST_nowait.c\;OMPTEST_ompfor5.c\;OMPTEST_ompfor6.c\;OMPTEST_ompfor7.c\;OMPTEST_parallel_pragma.c\;OMPTEST_private-duplicate.c\;OMPTEST_referenceType.cpp\;OMPTEST_single2.c\;OMPTEST_sizeof.c\;OMPTEST_staticChunk.c\;OMPTEST_task_array2.c\;OMPTEST_task_orphaned.c\;OMPTEST_task_underIf.c\;OMPTEST_task_untied4.c\;OMPTEST_taskwait.c\;OMPTEST_taskwait2.c\;OMPTEST_twoRegions.c
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_ompLoweringTests_ompacc_diff_check PROPERTIES LABELS disabled DEPENDS "OMPTEST_alignment.c;OMPTEST_collapse.c;OMPTEST_dijkstra_open_mp.c;OMPTEST_dynamicChunk.c;OMPTEST_endif.c;OMPTEST_endif3.c;OMPTEST_full_verify.c;OMPTEST_include_header_only.c;OMPTEST_init.c;OMPTEST_lastprivateOrphaned.c;OMPTEST_linebreak.c;OMPTEST_master.c;OMPTEST_md_open_mp.c;OMPTEST_memberFunction.cpp;OMPTEST_memberFunction2.cpp;OMPTEST_nowait.c;OMPTEST_ompfor5.c;OMPTEST_ompfor6.c;OMPTEST_ompfor7.c;OMPTEST_parallel_pragma.c;OMPTEST_private-duplicate.c;OMPTEST_referenceType.cpp;OMPTEST_single2.c;OMPTEST_sizeof.c;OMPTEST_staticChunk.c;OMPTEST_task_array2.c;OMPTEST_task_orphaned.c;OMPTEST_task_underIf.c;OMPTEST_task_untied4.c;OMPTEST_taskwait.c;OMPTEST_taskwait2.c;OMPTEST_twoRegions.c" )
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_ompLoweringTests_ompacc_diff_check PROPERTIES DISABLED TRUE)
endif()
