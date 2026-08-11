include_directories(${ROSE_INCLUDES})

add_executable(roseomp roseomp.C)
target_link_libraries(roseomp ROSE_DLL ${link_with_libraries})

add_executable(roseompacc roseompacc.C)
target_link_libraries(roseompacc ROSE_DLL ${link_with_libraries})

add_executable(rex_omp_simd_addr3_translator
               rex_omp_simd_addr3_translator.C)
target_include_directories(
  rex_omp_simd_addr3_translator
  PRIVATE ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/ompLowering)
target_link_libraries(rex_omp_simd_addr3_translator
                      ROSE_DLL ${link_with_libraries})

add_executable(rex_omp_associated_loop_contract
               rex_omp_associated_loop_contract.cpp)
target_include_directories(
  rex_omp_associated_loop_contract
  PRIVATE ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/ompLowering)
target_link_libraries(rex_omp_associated_loop_contract
                      ROSE_DLL ${link_with_libraries})

add_executable(rex_omp_copy_semantic_edges
               rex_omp_copy_semantic_edges.cpp)
target_link_libraries(rex_omp_copy_semantic_edges
                      ROSE_DLL ${link_with_libraries})

add_executable(rex_omp_clause_copy_identity
               rex_omp_clause_copy_identity.cpp)
target_include_directories(
  rex_omp_clause_copy_identity
  PRIVATE ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/ompLowering)
target_link_libraries(rex_omp_clause_copy_identity
                      ROSE_DLL ${link_with_libraries})

add_executable(rex_omp_runtime_int64_contract
               rex_omp_runtime_int64_contract.cpp)
target_include_directories(
  rex_omp_runtime_int64_contract
  PRIVATE ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/ompLowering)
target_link_libraries(rex_omp_runtime_int64_contract
                      ROSE_DLL ${link_with_libraries})

add_executable(rex_omp_intel_simd_identity_contract
               rex_omp_intel_simd_identity_contract.cpp)
target_include_directories(
  rex_omp_intel_simd_identity_contract
  PRIVATE ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/ompLowering)
target_link_libraries(rex_omp_intel_simd_identity_contract
                      ROSE_DLL ${link_with_libraries})

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; ompLoweringTests require bash")
endif()

set(_omp_test_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/OpenMP_tests)
set(_omp_lowering_inc
  ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/ompLowering)
set(_omp_translate_script
  ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_translate_only.sh)
set(_omp_semantic_script
  ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_semantic_compare.sh)
set(_omp_invariant_script
  ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_invariant_check.sh)
set(_omp_mapper_checkpoint_script
  ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_mapper_checkpoint_compare.sh)
set(_omp_deterministic_script
  ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_deterministic_lowering_compare.sh)
set(_omp_lowering_test_output_dir
  ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_omp_lowering_test_output_dir})

set(_omp_associated_loop_contract_input
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_omp_associated_loop_contract_input.cpp)
set(_omp_runtime_int64_contract_input
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_omp_runtime_int64_contract_input.c)
set(_omp_intel_simd_identity_contract_input
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_omp_intel_simd_identity_contract_input.c)
set(_omp_clause_copy_identity_input
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_omp_clause_copy_identity_input.c)

foreach(_abi IN ITEMS lp64 ilp32)
  add_test(
    NAME omp_lowering_rex_runtime_int64_${_abi}_contract
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND $<TARGET_FILE:rex_omp_runtime_int64_contract>
            --${_abi}
            ${_omp_runtime_int64_contract_input})
  set_tests_properties(
    omp_lowering_rex_runtime_int64_${_abi}_contract
    PROPERTIES LABELS "rex;OMPLOWERING;OMPLOWERING_CONTRACT")
endforeach()

foreach(_identity IN ITEMS vec ptr part)
  add_test(
    NAME omp_lowering_rex_intel_simd_${_identity}_identity_contract
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND $<TARGET_FILE:rex_omp_intel_simd_identity_contract>
            --${_identity}
            ${_omp_intel_simd_identity_contract_input})
  set_tests_properties(
    omp_lowering_rex_intel_simd_${_identity}_identity_contract
    PROPERTIES LABELS "rex;OMPLOWERING;OMPLOWERING_SIMD;OMPLOWERING_CONTRACT")
endforeach()

add_test(
  NAME omp_lowering_rex_intel_simd_missing_output_identity_is_rejected
  WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=rex_intel_simd_missing_output.log; set +e; \"$@\" --missing-output \"${_omp_intel_simd_identity_contract_input}\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '^REX_OMP_LOWERING_INVARIANT\\[intel-simd-store-ownership\\]: generated original symbol=0x[[:xdigit:]]+ name=__generated_unbound has no exact output identity$' \"$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_omp_intel_simd_identity_contract>)
set_tests_properties(
  omp_lowering_rex_intel_simd_missing_output_identity_is_rejected
  PROPERTIES LABELS "rex;death;OMPLOWERING;OMPLOWERING_SIMD;OMPLOWERING_CONTRACT")

add_test(
  NAME omp_lowering_rex_clause_copy_identity_shadowed_repeated
  WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
  COMMAND $<TARGET_FILE:rex_omp_clause_copy_identity>
          --accept-shadowed-repeated
          ${_omp_clause_copy_identity_input})
set_tests_properties(
  omp_lowering_rex_clause_copy_identity_shadowed_repeated
  PROPERTIES LABELS "rex;OpenMP;OMPLOWERING;OMPLOWERING_CONTRACT")

add_test(
  NAME omp_lowering_rex_clause_copy_identity_missing_map_is_rejected
  WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=rex_omp_clause_copy_identity_missing_map.log; rm -f \"$log\"; set +e; \"$@\" --reject-missing-directive-map \"${_omp_clause_copy_identity_input}\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '^REX_OMP_LOWERING_INVARIANT\\[outlined-clause-copy-identity\\]: role=directive original=0x[[:xdigit:]]+ type=SgOmpParallelStatement mapped=\\(nil\\) type=<null> has no exact nonidentity typed copy$' \"$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_omp_clause_copy_identity>)
set_tests_properties(
  omp_lowering_rex_clause_copy_identity_missing_map_is_rejected
  PROPERTIES LABELS "rex;death;OpenMP;OMPLOWERING;OMPLOWERING_CONTRACT")

add_test(
  NAME omp_lowering_rex_openmp_copy_semantic_edges
  WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
  COMMAND $<TARGET_FILE:rex_omp_copy_semantic_edges>
          --openmp
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_omp_copy_semantic_edges_input.c)
set_tests_properties(
  omp_lowering_rex_openmp_copy_semantic_edges
  PROPERTIES LABELS "rex;OMPLOWERING;OMPLOWERING_CONTRACT")

add_test(
  NAME omp_lowering_rex_openacc_copy_semantic_edges
  WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
  COMMAND $<TARGET_FILE:rex_omp_copy_semantic_edges>
          --openacc
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_acc_copy_semantic_edges_input.c)
set_tests_properties(
  omp_lowering_rex_openacc_copy_semantic_edges
  PROPERTIES LABELS "rex;OpenACC;OMPLOWERING_CONTRACT")

add_test(
  NAME omp_lowering_rex_openmp_declaration_copy_semantic_edges
  WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
  COMMAND $<TARGET_FILE:rex_omp_copy_semantic_edges>
          --openmp-declarations
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_omp_declaration_copy_semantic_edges_input.c)
set_tests_properties(
  omp_lowering_rex_openmp_declaration_copy_semantic_edges
  PROPERTIES LABELS "rex;OpenMP;OMPLOWERING_CONTRACT")

foreach(_positive_mode IN ITEMS
    accept-direct
    accept-basic-block
    accept-transform-wrapper
    accept-cxx-constexpr-factor
    accept-cxx-const-factor)
  add_test(
    NAME omp_lowering_rex_associated_loop_${_positive_mode}
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND $<TARGET_FILE:rex_omp_associated_loop_contract>
            --${_positive_mode}
            ${_omp_associated_loop_contract_input})
  set_tests_properties(
    omp_lowering_rex_associated_loop_${_positive_mode}
    PROPERTIES LABELS "rex;OMPLOWERING;OMPLOWERING_CONTRACT")
endforeach()

add_test(
  NAME omp_lowering_rex_c_const_integral_constant_is_rejected
  WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
  COMMAND ${BASH_EXECUTABLE} -c
    "ulimit -c 0; log=rex_c_const_integral_constant.log; set +e; \"$@\" --reject-c-const-factor \"${CMAKE_CURRENT_SOURCE_DIR}/rex_omp_c_const_integral_constant_input.c\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '^REX_OMP_LOWERING_INVARIANT\\[unroll-factor\\]: expression=0x[[:xdigit:]]+ kind=SgVarRefExp is not one exact positive target-ABI integral constant in \\[1,18446744073709551615\\]$' \"$log\")\" -eq 1"
    dummy
    $<TARGET_FILE:rex_omp_associated_loop_contract>)
set_tests_properties(
  omp_lowering_rex_c_const_integral_constant_is_rejected
  PROPERTIES LABELS "rex;death;OMPLOWERING;OMPLOWERING_CONTRACT")

set(_omp_associated_loop_death_cases
  "ambiguous|reject-ambiguous-block"
  "non_loop|reject-non-loop"
  "wrong_root|reject-wrong-root"
  "wrong_parent|reject-parent"
  "parallel_wrapper|reject-parallel-wrapper"
  "task_wrapper|reject-task-wrapper"
  "critical_wrapper|reject-critical-wrapper"
  "attributed_wrapper|reject-attributed-wrapper"
  "cycle|reject-cycle"
  "continue|reject-continue"
  "stale_constant_symbol|reject-stale-constant-symbol")
foreach(_case IN LISTS _omp_associated_loop_death_cases)
  string(REPLACE "|" ";" _fields "${_case}")
  list(GET _fields 0 _name)
  list(GET _fields 1 _mode)
  if(_name STREQUAL "ambiguous")
    set(_expected_diagnostic
      "^REX_OMP_LOWERING_INVARIANT\\[associated-loop-contract\\]: associated-loop block=0x[[:xdigit:]]+ contains 2 statements instead of one exact child$")
  elseif(_name STREQUAL "non_loop")
    set(_expected_diagnostic
      "^REX_OMP_LOWERING_INVARIANT\\[associated-loop-contract\\]: associated-loop path reaches unsupported node=0x[[:xdigit:]]+ kind=SgNullStatement$")
  elseif(_name STREQUAL "wrong_root")
    set(_expected_diagnostic
      "^REX_OMP_LOWERING_INVARIANT\\[associated-loop-contract\\]: expected root=0x[[:xdigit:]]+ does not match directive=0x[[:xdigit:]]+ exact body=0x[[:xdigit:]]+$")
  elseif(_name STREQUAL "wrong_parent")
    set(_expected_diagnostic
      "^REX_OMP_LOWERING_INVARIANT\\[associated-loop-contract\\]: child=0x[[:xdigit:]]+ kind=SgForStatement parent=0x[[:xdigit:]]+ does not match exact owner=0x[[:xdigit:]]+$")
  elseif(_name STREQUAL "parallel_wrapper")
    set(_expected_diagnostic
      "^REX_OMP_LOWERING_INVARIANT\\[associated-loop-contract\\]: OpenMP wrapper=0x[[:xdigit:]]+ kind=SgOmpParallelStatement is not admitted by associated-loop path contract=1$")
  elseif(_name STREQUAL "task_wrapper")
    set(_expected_diagnostic
      "^REX_OMP_LOWERING_INVARIANT\\[associated-loop-contract\\]: OpenMP wrapper=0x[[:xdigit:]]+ kind=SgOmpTaskStatement is not admitted by associated-loop path contract=1$")
  elseif(_name STREQUAL "critical_wrapper")
    set(_expected_diagnostic
      "^REX_OMP_LOWERING_INVARIANT\\[associated-loop-contract\\]: OpenMP wrapper=0x[[:xdigit:]]+ kind=SgOmpCriticalStatement is not admitted by associated-loop path contract=1$")
  elseif(_name STREQUAL "attributed_wrapper")
    set(_expected_diagnostic
      "^REX_OMP_LOWERING_INVARIANT\\[associated-loop-contract\\]: attributed statement=0x[[:xdigit:]]+ is an explicit semantic/source wrapper and is not supported on an associated-loop path$")
  elseif(_name STREQUAL "cycle")
    set(_expected_diagnostic
      "^REX_OMP_LOWERING_INVARIANT\\[associated-loop-contract\\]: associated-loop owner chain is cyclic at node=0x[[:xdigit:]]+ kind=SgBasicBlock$")
  elseif(_name STREQUAL "continue")
    set(_expected_diagnostic
      "^REX_AST_INVARIANT\\[loop-unrolling-continue\\]: loop=0x[[:xdigit:]]+ contains a continue targeting the loop\\; grouped unrolling cannot preserve that control edge$")
  elseif(_name STREQUAL "stale_constant_symbol")
    set(_expected_diagnostic
      "^REX_OMP_LOWERING_INVARIANT\\[unroll-factor\\]: expression=0x[[:xdigit:]]+ kind=SgVarRefExp is not one exact positive target-ABI integral constant in \\[1,18446744073709551615\\]$")
  else()
    message(FATAL_ERROR "unmapped associated-loop death case: ${_name}")
  endif()
  add_test(
    NAME omp_lowering_rex_associated_loop_${_name}_is_rejected
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=rex_associated_loop_${_name}.log; set +e; \"$@\" --${_mode} \"${_omp_associated_loop_contract_input}\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '${_expected_diagnostic}' \"$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:rex_omp_associated_loop_contract>)
  set_tests_properties(
    omp_lowering_rex_associated_loop_${_name}_is_rejected
    PROPERTIES LABELS "rex;death;OMPLOWERING;OMPLOWERING_CONTRACT")
endforeach()

set(_omp_legacy_labels
  OMPLOWERING
  OMPLOWERING_LEGACY)

set(_omp_sanitizer_test_environment)
set(_omp_fortran_sanitizer_test_environment)
if(ROSE_USE_SANITIZER AND ROSE_SANITIZERS)
  string(REPLACE ";" "," _omp_sanitizer_test_list "${ROSE_SANITIZERS}")
  list(APPEND _omp_sanitizer_test_environment
       "ROSE_TEST_SANITIZER_FLAGS=-fsanitize=${_omp_sanitizer_test_list}")

  if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    execute_process(
      COMMAND "${CMAKE_C_COMPILER}" -print-resource-dir
      OUTPUT_VARIABLE _omp_clang_resource_dir
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    execute_process(
      COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
      OUTPUT_VARIABLE _omp_clang_target_triple
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)

    set(_omp_clang_runtime_dir "")
    if(_omp_clang_resource_dir AND _omp_clang_target_triple AND
       IS_DIRECTORY "${_omp_clang_resource_dir}/lib/${_omp_clang_target_triple}")
      set(_omp_clang_runtime_dir "${_omp_clang_resource_dir}/lib/${_omp_clang_target_triple}")
    endif()

    set(_omp_fortran_sanitizer_link_flags)
    if(_omp_clang_runtime_dir)
      if("address" IN_LIST ROSE_SANITIZERS OR "leak" IN_LIST ROSE_SANITIZERS)
        set(_omp_asan_static "${_omp_clang_runtime_dir}/libclang_rt.asan_static.a")
        set(_omp_asan "${_omp_clang_runtime_dir}/libclang_rt.asan.a")
        set(_omp_asan_syms "${_omp_clang_runtime_dir}/libclang_rt.asan.a.syms")
        if(EXISTS "${_omp_asan_static}" AND EXISTS "${_omp_asan}" AND
           EXISTS "${_omp_asan_syms}")
          list(APPEND _omp_fortran_sanitizer_link_flags
               -Wl,--whole-archive
               "${_omp_asan_static}"
               "${_omp_asan}"
               -Wl,--no-whole-archive
               "-Wl,--dynamic-list=${_omp_asan_syms}"
               -pthread
               -lrt
               -lm
               -ldl
               -lresolv)
        endif()
      elseif("undefined" IN_LIST ROSE_SANITIZERS)
        set(_omp_ubsan "${_omp_clang_runtime_dir}/libclang_rt.ubsan_standalone.a")
        set(_omp_ubsan_syms "${_omp_clang_runtime_dir}/libclang_rt.ubsan_standalone.a.syms")
        if(EXISTS "${_omp_ubsan}" AND EXISTS "${_omp_ubsan_syms}")
          list(APPEND _omp_fortran_sanitizer_link_flags
               -Wl,--whole-archive
               "${_omp_ubsan}"
               -Wl,--no-whole-archive
               "-Wl,--dynamic-list=${_omp_ubsan_syms}"
               -pthread
               -lrt
               -lm
               -ldl
               -lresolv)
        endif()
      endif()
    endif()

    if(_omp_fortran_sanitizer_link_flags)
      list(JOIN _omp_fortran_sanitizer_link_flags " "
           _omp_fortran_sanitizer_link_flags_string)
      list(APPEND _omp_fortran_sanitizer_test_environment
           "ROSE_TEST_SANITIZER_LINK_FLAGS=${_omp_fortran_sanitizer_link_flags_string}")
    endif()
  endif()
endif()

function(_set_omp_test_sanitizer_environment test_name)
  if(_omp_sanitizer_test_environment)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT
                 "${_omp_sanitizer_test_environment}")
  endif()
endfunction()

function(_omp_sanitize_name out_var specimen)
  set(_name ${specimen})
  string(REPLACE "." "_" _name ${_name})
  string(REPLACE "-" "_" _name ${_name})
  set(${out_var} ${_name} PARENT_SCOPE)
endfunction()

function(_label_omp_legacy test_name)
  set_property(TEST ${test_name} APPEND PROPERTY LABELS "${_omp_legacy_labels};OMPLOWERING_LEGACY_TRANSLATE")
endfunction()

function(_label_omp_compile_only test_name)
  set_property(TEST ${test_name} APPEND PROPERTY LABELS
               "${_omp_legacy_labels};OMPLOWERING_LEGACY_TRANSLATE;OMPLOWERING_COMPILE_ONLY")
endfunction()

function(_label_omp_semantic test_name)
  set_property(TEST ${test_name} APPEND PROPERTY LABELS "${_omp_legacy_labels};OMPLOWERING_SEMANTIC")
endfunction()

function(_label_omp_invariant test_name)
  set_property(TEST ${test_name} APPEND PROPERTY LABELS "${_omp_legacy_labels};OMPLOWERING_INVARIANT")
endfunction()

set(_omp_lowering_ctest_timeout_scale 1)
if(ROSE_USE_VALGRIND)
  if(DEFINED ROSE_CTEST_EXPLICIT_TIMEOUT_SCALE)
    set(_omp_lowering_ctest_timeout_scale ${ROSE_CTEST_EXPLICIT_TIMEOUT_SCALE})
  else()
    set(_omp_lowering_ctest_timeout_scale 4)
  endif()
endif()

function(_omp_scaled_ctest_timeout out_var timeout_seconds)
  if(NOT timeout_seconds MATCHES "^[0-9]+$")
    message(FATAL_ERROR "OpenMP lowering test timeout must be an integer: ${timeout_seconds}")
  endif()
  math(EXPR _timeout "${timeout_seconds} * ${_omp_lowering_ctest_timeout_scale}")
  set(${out_var} ${_timeout} PARENT_SCOPE)
endfunction()

function(_omp_set_test_timeout test_name timeout_seconds)
  _omp_scaled_ctest_timeout(_timeout ${timeout_seconds})
  set_tests_properties(${test_name} PROPERTIES TIMEOUT ${_timeout})
endfunction()

function(add_omp_translate_only_test test_name translator input_file work_subdir)
  add_test(
    NAME ${test_name}
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} ${_omp_translate_script}
            ${translator}
            ${_omp_test_dir}/${input_file}
            ${_omp_lowering_test_output_dir}/${work_subdir}
            ${input_file})
  _omp_set_test_timeout(${test_name} 540)
  _label_omp_compile_only(${test_name})
endfunction()

function(add_omp_semantic_test test_name translator compiler input_file mode language
         work_subdir)
  add_test(
    NAME ${test_name}
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} ${_omp_semantic_script}
            ${translator}
            ${compiler}
            ${_omp_test_dir}/${input_file}
            ${_omp_lowering_test_output_dir}/${work_subdir}
            ${mode}
            ${ROSE_LLVM_OPENMP_INCLUDE_DIR}
            ${ROSE_LLVM_OPENMP_RUNTIME_DIR}
            ${_omp_lowering_inc}
            ${input_file}
            ${language}
            ${ROSE_OPENMP_VERSION})
  _omp_set_test_timeout(${test_name} 900)
  _set_omp_test_sanitizer_environment(${test_name})
  _label_omp_semantic(${test_name})
endfunction()

set(OMP_C_COMPILE_BASE_CASES
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
  ompfor5.c
  ompfor6.c
  ompfor7.c
  ompfor8.c
  parallel_pragma.c
  private-duplicate.c
  rex_omp_lowering_loop_transform_ownership.c
  rex_openmp_ordered_map_sections.c
  single2.c
  sizeof.c
  staticChunk.c
  task_array2.c
  task_orphaned.c
  task_underIf.c
  task_untied4.c
  task_wait.c
  task_wait2.c
  twoRegions.c)

set(OMP_C_RUNTIME_BASE_CASES
  3loops.c
  array_init.c
  array_init_2.c
  barrier.c
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
  ompfor.c
  ompfor2.c
  ompfor3.c
  ompfor4.c
  ompfor-default.c
  ompfor-decremental.c
  ompfor-static.c
  ompfor_c99.c
  omp_version.c
  ordered.c
  ordered2.c
  orphaned-directives.c
  parallel.c
  parallel-if.c
  parallel-if-numthreads.c
  parallel-numthreads.c
  parallel-reduction.c
  parallel-reduction2.c
  parallelfor.c
  parallelfor2.c
  parallelsections.c
  pi.c
  prettyfunc.c
  private.c
  privatej.c
  reduction-classic.c
  reduction.c
  reduction2.c
  rex_omp_unroll_exact_boundary.c
  rice1.c
  section.c
  section1.c
  set_num_threads.c
  single.c
  single3.c
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
  threadprivate3.c)

set(OMP_CXX_COMPILE_BASE_CASES
  memberFunction.cpp
  memberFunction2.cpp
  referenceType.cpp)

set(OMP_CXX_RUNTIME_BASE_CASES
  hello1.cpp
  helloNested.cpp
  objectPrivate.cpp
  objectFirstPrivate.cpp
  objectLastprivate.cpp
  orphanedAtomic.cpp
  preprocessingInfo2.cpp
  rex_omp_lowering_typed_transform_constants.cpp
  task_link.cpp
  task_link2.cpp
  task_tree.cpp)

set(OMP_ACC_C_CASES)
set(OMP_ACC_CXX_CASES)

# Keep semantic-compare cases deterministic. Exclude tests such as private.c
# that intentionally read uninitialized private variables (undefined behavior).
set(OMP_C_SEMANTIC_CASES
  barrier.c
  flush.c
  hello.c
  matrixmultiply.c
  ompfor.c
  ompfor_c99.c
  ompfor3.c
  ompfor4.c
  ompfor-default.c
  ompfor-decremental.c
  ompfor-static.c
  omp_version.c
  parallel.c
  parallel-if.c
  parallel-if-numthreads.c
  parallel-numthreads.c
  parallel-reduction.c
  parallel-reduction2.c
  parallelfor2.c
  rex_omp_lowering_loop_transform_ownership.c
  rex_omp_unroll_exact_boundary.c
  reduction.c
  single.c)

set(OMP_CXX_SEMANTIC_CASES
  hello1.cpp
  objectFirstPrivate.cpp
  objectLastprivate.cpp
  objectPrivate.cpp
  orphanedAtomic.cpp
  preprocessingInfo2.cpp
  rex_omp_lowering_typed_transform_constants.cpp)

set(OMP_C_INVARIANT_CASES
  barrier.c
  flush.c
  hello.c
  matrixmultiply.c
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
  private.c
  rex_omp_lowering_loop_transform_ownership.c
  rex_omp_unroll_exact_boundary.c
  reduction.c
  single.c
  single3.c
  ompfor5.c
  ompfor6.c
  ompfor7.c
  twoRegions.c)

set(OMP_CXX_INVARIANT_CASES
  hello1.cpp
  objectFirstPrivate.cpp
  objectLastprivate.cpp
  objectPrivate.cpp
  orphanedAtomic.cpp
  preprocessingInfo2.cpp
  rex_omp_lowering_typed_transform_constants.cpp)

set(OMP_C_ALL_CASES ${OMP_C_COMPILE_BASE_CASES} ${OMP_C_RUNTIME_BASE_CASES})
set(OMP_CXX_ALL_CASES ${OMP_CXX_COMPILE_BASE_CASES} ${OMP_CXX_RUNTIME_BASE_CASES})

list(REMOVE_DUPLICATES OMP_C_ALL_CASES)
list(REMOVE_DUPLICATES OMP_CXX_ALL_CASES)
list(REMOVE_DUPLICATES OMP_C_SEMANTIC_CASES)
list(REMOVE_DUPLICATES OMP_CXX_SEMANTIC_CASES)
list(REMOVE_DUPLICATES OMP_C_INVARIANT_CASES)
list(REMOVE_DUPLICATES OMP_CXX_INVARIANT_CASES)

foreach(testcode IN LISTS OMP_C_SEMANTIC_CASES OMP_C_INVARIANT_CASES)
  if(NOT testcode IN_LIST OMP_C_ALL_CASES)
    message(FATAL_ERROR "Case '${testcode}' is not in OMP_C_ALL_CASES")
  endif()
endforeach()

foreach(testcode IN LISTS OMP_CXX_SEMANTIC_CASES OMP_CXX_INVARIANT_CASES)
  if(NOT testcode IN_LIST OMP_CXX_ALL_CASES)
    message(FATAL_ERROR "Case '${testcode}' is not in OMP_CXX_ALL_CASES")
  endif()
endforeach()

set(OMP_LEGACY_CASE_MATRIX)
function(_append_omp_legacy_case specimen language profile mode invariant)
  list(APPEND OMP_LEGACY_CASE_MATRIX
       "${specimen}|${language}|${profile}|${mode}|${invariant}")
  set(OMP_LEGACY_CASE_MATRIX "${OMP_LEGACY_CASE_MATRIX}" PARENT_SCOPE)
endfunction()

foreach(testcode IN LISTS OMP_C_ALL_CASES)
  set(_profile compile_only)
  if(testcode IN_LIST OMP_C_SEMANTIC_CASES)
    set(_profile semantic)
  endif()
  set(_invariant off)
  if(testcode IN_LIST OMP_C_INVARIANT_CASES)
    set(_invariant on)
  endif()
  _append_omp_legacy_case(${testcode} c ${_profile} sort ${_invariant})
endforeach()

foreach(testcode IN LISTS OMP_CXX_ALL_CASES)
  set(_profile compile_only)
  if(testcode IN_LIST OMP_CXX_SEMANTIC_CASES)
    set(_profile semantic)
  endif()
  set(_invariant off)
  if(testcode IN_LIST OMP_CXX_INVARIANT_CASES)
    set(_invariant on)
  endif()
  _append_omp_legacy_case(${testcode} cxx ${_profile} sort ${_invariant})
endforeach()

set(_omp_c_semantic_enabled ON)
if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
  message(WARNING "Legacy C lowering semantic tests require Clang; using translate-only mode")
  set(_omp_c_semantic_enabled OFF)
endif()
if(NOT ROSE_LLVM_OPENMP_INCLUDE_DIR OR NOT EXISTS "${ROSE_LLVM_OPENMP_INCLUDE_DIR}/omp.h")
  message(WARNING "LLVM OpenMP include directory unavailable; using translate-only mode for legacy C semantic tests")
  set(_omp_c_semantic_enabled OFF)
endif()
if(NOT ROSE_LLVM_OPENMP_RUNTIME_DIR)
  message(WARNING "LLVM OpenMP runtime directory unavailable; using translate-only mode for legacy C semantic tests")
  set(_omp_c_semantic_enabled OFF)
endif()

set(_omp_cxx_semantic_enabled ON)
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(WARNING "Legacy C++ lowering semantic tests require Clang; using translate-only mode")
  set(_omp_cxx_semantic_enabled OFF)
endif()
if(NOT _omp_c_semantic_enabled)
  set(_omp_cxx_semantic_enabled OFF)
endif()

if(ENABLE-C)
  if(_omp_c_semantic_enabled)
    add_omp_semantic_test(
      omp_lowering_c_semantic_rex_omp_simd_ordered_runtime_c
      $<TARGET_FILE:rex_omp_simd_addr3_translator>
      ${CMAKE_C_COMPILER}
      rex_omp_simd_ordered_runtime.c
      exact
      c
      simd_addr3/rex_omp_simd_ordered_runtime_c)
  endif()

  set(_omp_simd_length_cases
    "absent|rex_omp_simd_length_absent.c|_mm512_loadu_ps"
    "simdlen|rex_omp_simd_length_simdlen.c|_mm256_loadu_ps"
    "safelen|rex_omp_simd_length_safelen.c|_mm_loadu_ps"
    "both|rex_omp_simd_length_both.c|_mm_loadu_ps")
  foreach(_case IN LISTS _omp_simd_length_cases)
    string(REPLACE "|" ";" _fields "${_case}")
    list(GET _fields 0 _name)
    list(GET _fields 1 _source)
    list(GET _fields 2 _intrinsic)
    set(_output
      ${_omp_lowering_test_output_dir}/rose_rex_omp_simd_length_${_name}.c)
    add_test(
      NAME omp_lowering_rex_simd_length_${_name}
      WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
      COMMAND ${BASH_EXECUTABLE} -c
        "set -e; output=\"$1\"; expected=\"$2\"; shift 2; rm -f \"$output\"; \"$@\" -target x86_64-unknown-linux-gnu -rose:simd:intel-avx -rose:openmp:lowering -rose:skipfinalCompileStep -rose:verbose 0 -rose:output \"$output\" -c \"${_omp_test_dir}/${_source}\"; test -s \"$output\"; grep -Fq \"$expected\" \"$output\""
        dummy
        ${_output}
        ${_intrinsic}
        $<TARGET_FILE:rex_omp_simd_addr3_translator>)
    set_tests_properties(
      omp_lowering_rex_simd_length_${_name}
      PROPERTIES LABELS "rex;OMPLOWERING;OMPLOWERING_SIMD")
  endforeach()

  set(_omp_simd_two_region_output
      ${_omp_lowering_test_output_dir}/rose_rex_omp_simd_two_region_reduction.c)
  add_test(
    NAME omp_lowering_rex_simd_two_region_reduction_ownership
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "set -e; output=\"$1\"; shift; rm -f \"$output\"; MALLOC_PERTURB_=165 \"$@\" -target x86_64-unknown-linux-gnu -rose:simd:intel-avx -rose:openmp:lowering -rose:skipfinalCompileStep -rose:verbose 0 -rose:output \"$output\" -c \"${_omp_test_dir}/rex_omp_simd_two_region_reduction.c\"; test -s \"$output\"; test \"$(grep -Fc '_mm512_setzero_ps' \"$output\")\" -eq 2"
      dummy
      ${_omp_simd_two_region_output}
      $<TARGET_FILE:rex_omp_simd_addr3_translator>)
  set_tests_properties(
    omp_lowering_rex_simd_two_region_reduction_ownership
    PROPERTIES LABELS "rex;OMPLOWERING;OMPLOWERING_SIMD")

  set(_omp_simd_death_cases
    "strided|rex_omp_simd_strided_invalid.c|^REX_OMP_LOWERING_INVARIANT\\[omp-simd-address\\]: node=0x[[:xdigit:]]+ kind=SgMultiplyOp uses a non-affine index operation$"
    "runtime_index|rex_omp_simd_runtime_index_invalid.c|^REX_OMP_LOWERING_INVARIANT\\[omp-simd-address\\]: expression=0x[[:xdigit:]]+ kind=SgCastExp is not one exact target-ABI integral constant expression$"
    "mixed_type|rex_omp_simd_mixed_type_invalid.c|^REX_OMP_LOWERING_INVARIANT\\[omp-simd-type\\]: node=0x[[:xdigit:]]+ kind=SgCastExp is not an exact no-op scalar cast$"
    "mixed_lane_width|rex_omp_simd_mixed_lane_width_invalid.c|^REX_OMP_LOWERING_INVARIANT\\[intel-simd-width\\]: region mixes 32-bit and 64-bit lane widths before emission$"
    "value_cast|rex_omp_simd_value_cast_invalid.c|^REX_OMP_LOWERING_INVARIANT\\[omp-simd-type\\]: node=0x[[:xdigit:]]+ kind=SgCastExp is not an exact no-op scalar cast$"
    "induction_scalar|rex_omp_simd_induction_scalar_invalid.c|^REX_OMP_LOWERING_INVARIANT\\[omp-simd-expression\\]: node=0x[[:xdigit:]]+ kind=SgVarRefExp uses the induction variable as a scalar broadcast$"
    "length_order|rex_omp_simd_length_order_invalid.c|^REX_OMP_LOWERING_INVARIANT\\[omp-simd-length\\]: node=0x[[:xdigit:]]+ kind=SgOmpSimdStatement has simdlen greater than safelen$"
    "duplicate_length_clause|rex_omp_simd_length_duplicate_invalid.c|^  0:0: duplicate unique clause [[:punct:]]simdlen[[:punct:]]$"
    "length_unsupported|rex_omp_simd_length_unsupported_invalid.c|^REX_OMP_LOWERING_INVARIANT\\[omp-simd-length\\]: node=0x[[:xdigit:]]+ kind=SgOmpSimdStatement requests a width unsupported by the exact Intel SIMD backend$"
    "safelen_too_small|rex_omp_simd_safelen_too_small_invalid.c|^REX_OMP_LOWERING_INVARIANT\\[omp-simd-length\\]: node=0x[[:xdigit:]]+ kind=SgOmpSimdStatement has no Intel SIMD width that satisfies safelen$")
  foreach(_case IN LISTS _omp_simd_death_cases)
    string(REPLACE "|" ";" _fields "${_case}")
    list(GET _fields 0 _name)
    list(GET _fields 1 _source)
    list(GET _fields 2 _expected_diagnostic)
    add_test(
      NAME omp_lowering_rex_simd_${_name}_is_rejected
      WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
      COMMAND ${BASH_EXECUTABLE} -c
        "ulimit -c 0; log=rex_simd_${_name}.log; set +e; \"$@\" -target x86_64-unknown-linux-gnu -rose:simd:intel-avx -rose:openmp:lowering -rose:skipfinalCompileStep -rose:verbose 0 -c \"${_omp_test_dir}/${_source}\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '${_expected_diagnostic}' \"$log\")\" -eq 1"
        dummy
        $<TARGET_FILE:rex_omp_simd_addr3_translator>)
    set_tests_properties(
      omp_lowering_rex_simd_${_name}_is_rejected
      PROPERTIES LABELS "rex;death;OMPLOWERING;OMPLOWERING_SIMD")
  endforeach()

  execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir
    RESULT_VARIABLE _clang_resource_dir_status
    OUTPUT_VARIABLE _clang_resource_dir
    ERROR_VARIABLE _clang_resource_dir_error
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _clang_resource_dir_status EQUAL 0 OR _clang_resource_dir STREQUAL "")
    message(FATAL_ERROR
      "Clang resource directory query failed: ${_clang_resource_dir_error}")
  endif()
  set(_clang_immintrin_header
    "${_clang_resource_dir}/include/immintrin.h")
  if(NOT EXISTS "${_clang_immintrin_header}")
    message(FATAL_ERROR
      "Clang resource directory has no immintrin.h: ${_clang_resource_dir}")
  endif()
  file(RELATIVE_PATH _clang_immintrin_relative
    "${_omp_lowering_test_output_dir}" "${_clang_immintrin_header}")

  add_test(
    NAME omp_lowering_rex_simd_intel_target_mismatch_is_rejected
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; set -euo pipefail; translator=\"$1\"; source=\"${_omp_test_dir}/rex_omp_simd_length_absent.c\"; expected=\"REX_FRONTEND_INVARIANT[intel-simd-target]: forced immintrin.h requires an exact x86 or x86_64 frontend target, but Clang selected 'aarch64-unknown-linux-gnu'\"; run_case() { name=\"$1\"; shift; log=\"rex_simd_intel_target_mismatch_$name.log\"; rm -f \"$log\"; set +e; \"$translator\" -target aarch64-unknown-linux-gnu \"$@\" -rose:openmp:lowering -rose:skipfinalCompileStep -rose:verbose 0 -c \"$source\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Fxc \"$expected\" \"$log\")\" -eq 1; }; run_case injected -rose:simd:intel-avx; run_case basename -include immintrin.h; run_case relative -include \"${_clang_immintrin_relative}\"; run_case absolute -include \"${_clang_immintrin_header}\"; run_case joined \"-include${_clang_immintrin_header}\""
      dummy
      $<TARGET_FILE:rex_omp_simd_addr3_translator>)
  set_tests_properties(
    omp_lowering_rex_simd_intel_target_mismatch_is_rejected
    PROPERTIES LABELS "rex;death;CFE;OMPLOWERING;OMPLOWERING_SIMD")

  set(_omp_integral_constant_death_cases
    "plain_char|rex_omp_unroll_plain_char_invalid.c|^REX_OMP_LOWERING_INVARIANT\\[integral-constant-abi\\]: type=0x[[:xdigit:]]+ kind=SgTypeChar has no persisted target signedness contract$"
    "wchar|rex_omp_unroll_wchar_invalid.cpp|^REX_OMP_LOWERING_INVARIANT\\[integral-constant-abi\\]: type=0x[[:xdigit:]]+ kind=SgTypeWchar has no persisted target signedness contract$"
    "negative_shift|rex_omp_unroll_negative_shift_invalid.c|^REX_OMP_LOWERING_INVARIANT\\[integral-constant-shift\\]: negative signed right shift has no persisted target policy$")
  foreach(_case IN LISTS _omp_integral_constant_death_cases)
    string(REPLACE "|" ";" _fields "${_case}")
    list(GET _fields 0 _name)
    list(GET _fields 1 _source)
    list(GET _fields 2 _expected_diagnostic)
    add_test(
      NAME omp_lowering_rex_integral_constant_${_name}_is_rejected
      WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
      COMMAND ${BASH_EXECUTABLE} -c
        "ulimit -c 0; log=rex_integral_constant_${_name}.log; set +e; \"$@\" -rose:openmp:lowering -rose:skipfinalCompileStep -rose:verbose 0 -c \"${_omp_test_dir}/${_source}\" >\"$log\" 2>&1; status=$?; set -e; test $status -eq 134; test \"$(grep -Ec '${_expected_diagnostic}' \"$log\")\" -eq 1"
        dummy
        $<TARGET_FILE:roseomp>)
    set_tests_properties(
      omp_lowering_rex_integral_constant_${_name}_is_rejected
      PROPERTIES LABELS "rex;death;OMPLOWERING;OMPLOWERING_CONSTANT")
  endforeach()

  foreach(_case_entry IN LISTS OMP_LEGACY_CASE_MATRIX)
    string(REPLACE "|" ";" _case_fields "${_case_entry}")
    list(GET _case_fields 0 testcode)
    list(GET _case_fields 1 _language)
    list(GET _case_fields 2 _profile)
    list(GET _case_fields 3 _mode)
    list(GET _case_fields 4 _invariant)

    _omp_sanitize_name(_omp_name ${testcode})
    set(_semantic_enabled ON)
    set(_compiler "")
    if(_language STREQUAL "c")
      set(_semantic_enabled ${_omp_c_semantic_enabled})
      set(_compiler ${CMAKE_C_COMPILER})
    elseif(_language STREQUAL "cxx")
      set(_semantic_enabled ${_omp_cxx_semantic_enabled})
      set(_compiler ${CMAKE_CXX_COMPILER})
    else()
      message(FATAL_ERROR "Unexpected language '${_language}' in OMP_LEGACY_CASE_MATRIX")
    endif()

    set(_semantic_selected OFF)
    if(_profile STREQUAL "semantic" AND _semantic_enabled)
      add_omp_semantic_test(
        omp_lowering_${_language}_semantic_${_omp_name}
        $<TARGET_FILE:roseomp>
        ${_compiler}
        ${testcode}
        ${_mode}
        ${_language}
        legacy_semantic/${_language}/${_omp_name})
      set(_semantic_selected ON)
    else()
      add_omp_translate_only_test(
        omp_lowering_${_language}_compile_only_${_omp_name}
        $<TARGET_FILE:roseomp>
        ${testcode}
        legacy_compile_only/${_language}/${_omp_name})
    endif()

    if(_invariant STREQUAL "on")
      set(_invariant_dep
          omp_lowering_${_language}_compile_only_${_omp_name})
      if(_semantic_selected)
        set(_invariant_dep
            omp_lowering_${_language}_semantic_${_omp_name})
      endif()

      add_test(
        NAME omp_lowering_invariant_${_omp_name}
        WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
        COMMAND ${BASH_EXECUTABLE} ${_omp_invariant_script}
                $<TARGET_FILE:roseomp>
                ${_omp_test_dir}/${testcode}
                ${_omp_lowering_test_output_dir}/legacy_invariants/${_omp_name}
                ${testcode})
      set_tests_properties(
        omp_lowering_invariant_${_omp_name}
        PROPERTIES
          DEPENDS ${_invariant_dep})
      _omp_set_test_timeout(omp_lowering_invariant_${_omp_name} 540)
      _label_omp_invariant(omp_lowering_invariant_${_omp_name})
    endif()
  endforeach()

  set(_loop_transform_ownership_name
      rex_omp_lowering_loop_transform_ownership_c)
  if(_omp_c_semantic_enabled)
    set(_loop_transform_ownership_translate_test
        omp_lowering_c_semantic_${_loop_transform_ownership_name})
    set(_loop_transform_ownership_generated_source
        ${_omp_lowering_test_output_dir}/legacy_semantic/c/${_loop_transform_ownership_name}/rose_rex_omp_lowering_loop_transform_ownership.c)
  else()
    set(_loop_transform_ownership_translate_test
        omp_lowering_c_compile_only_${_loop_transform_ownership_name})
    set(_loop_transform_ownership_generated_source
        ${_omp_lowering_test_output_dir}/legacy_compile_only/c/${_loop_transform_ownership_name}/rose_rex_omp_lowering_loop_transform_ownership.c)
  endif()
  add_test(
    NAME omp_lowering_c_generated_compile_${_loop_transform_ownership_name}
    COMMAND ${CMAKE_C_COMPILER}
            -std=c11
            -fsyntax-only
            -I${_omp_lowering_inc}
            ${_loop_transform_ownership_generated_source})
  set_tests_properties(
    omp_lowering_c_generated_compile_${_loop_transform_ownership_name}
    PROPERTIES
      DEPENDS ${_loop_transform_ownership_translate_test})
  _omp_set_test_timeout(
    omp_lowering_c_generated_compile_${_loop_transform_ownership_name} 120)
  set_property(
    TEST omp_lowering_c_generated_compile_${_loop_transform_ownership_name}
    APPEND PROPERTY LABELS
           "${_omp_legacy_labels};OMPLOWERING_GENERATED_COMPILE")

  foreach(testcode IN LISTS OMP_ACC_C_CASES)
    _omp_sanitize_name(_omp_name ${testcode})
    add_omp_translate_only_test(
      omp_acc_lowering_c_compile_only_${_omp_name}
      $<TARGET_FILE:roseompacc>
      ${testcode}
      legacy_compile_only/acc_c/${_omp_name})
  endforeach()

  foreach(testcode IN LISTS OMP_ACC_CXX_CASES)
    _omp_sanitize_name(_omp_name ${testcode})
    add_omp_translate_only_test(
      omp_acc_lowering_cxx_compile_only_${_omp_name}
      $<TARGET_FILE:roseompacc>
      ${testcode}
      legacy_compile_only/acc_cxx/${_omp_name})
  endforeach()

  add_test(
    NAME omp_lowering_mapper_semantic_rex_openmp_target_update_distinct_items
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_mapper_lowering_check.sh
            $<TARGET_FILE:roseomp>
            ${_omp_test_dir}/rex_openmp_target_update_distinct_items.c
            ${_omp_lowering_test_output_dir}/mapper_semantic/rex_openmp_target_update_distinct_items
            rex_openmp_target_update_distinct_items.c)
  _omp_set_test_timeout(
    omp_lowering_mapper_semantic_rex_openmp_target_update_distinct_items 540)
  _label_omp_semantic(
    omp_lowering_mapper_semantic_rex_openmp_target_update_distinct_items)

  add_test(
    NAME ASTJSON_omp_lowering_mapper_semantic_rex_openmp_target_update_distinct_items
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND ${BASH_EXECUTABLE}
            ${_omp_mapper_checkpoint_script}
            $<TARGET_FILE:roseomp>
            ${_omp_test_dir}/rex_openmp_target_update_distinct_items.c
            ${_omp_lowering_test_output_dir}/ast-json/mapper_semantic/rex_openmp_target_update_distinct_items
            rex_openmp_target_update_distinct_items.c)
  set_tests_properties(
    ASTJSON_omp_lowering_mapper_semantic_rex_openmp_target_update_distinct_items
    PROPERTIES
      LABELS "ASTJSON;OMPLOWERING;OMPLOWERING_SEMANTIC")
  _omp_set_test_timeout(
    ASTJSON_omp_lowering_mapper_semantic_rex_openmp_target_update_distinct_items
    540)

  add_test(
    NAME omp_lowering_rex_test2026_deterministic_capture_order_cpp
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND ${BASH_EXECUTABLE}
            ${_omp_deterministic_script}
            $<TARGET_FILE:roseomp>
            ${_omp_test_dir}/rex_test2026_omp_lowering_deterministic_capture_order.cpp
            ${_omp_lowering_test_output_dir}/deterministic/capture_order
            rex_test2026_omp_lowering_deterministic_capture_order.cpp
            none)
  set_tests_properties(
    omp_lowering_rex_test2026_deterministic_capture_order_cpp
    PROPERTIES
      LABELS "OMPLOWERING;OMPLOWERING_DETERMINISTIC")
  _omp_set_test_timeout(omp_lowering_rex_test2026_deterministic_capture_order_cpp 540)

  add_test(
    NAME omp_lowering_rex_test2026_deterministic_root_order_cpp
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND ${BASH_EXECUTABLE}
            ${_omp_deterministic_script}
            $<TARGET_FILE:roseomp>
            ${_omp_test_dir}/rex_test2026_omp_lowering_deterministic_root_order.cpp
            ${_omp_lowering_test_output_dir}/deterministic/root_order
            rex_test2026_omp_lowering_deterministic_root_order.cpp
            none)
  set_tests_properties(
    omp_lowering_rex_test2026_deterministic_root_order_cpp
    PROPERTIES
      LABELS "OMPLOWERING;OMPLOWERING_DETERMINISTIC")
  _omp_set_test_timeout(omp_lowering_rex_test2026_deterministic_root_order_cpp 540)

  add_test(
    NAME ASTJSON_omp_lowering_rex_test2026_deterministic_root_order_cpp
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND ${BASH_EXECUTABLE}
            ${_omp_deterministic_script}
            $<TARGET_FILE:roseomp>
            ${_omp_test_dir}/rex_test2026_omp_lowering_deterministic_root_order.cpp
            ${_omp_lowering_test_output_dir}/deterministic/root_order_ast_json
            rex_test2026_omp_lowering_deterministic_root_order.cpp
            all)
  set_tests_properties(
    ASTJSON_omp_lowering_rex_test2026_deterministic_root_order_cpp
    PROPERTIES
      LABELS "ASTJSON;OMPLOWERING;OMPLOWERING_DETERMINISTIC")
  _omp_set_test_timeout(ASTJSON_omp_lowering_rex_test2026_deterministic_root_order_cpp 540)

  add_test(
    NAME omp_acc_lowering_c_typed_launch_bounds
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_rex_typed_launch_bounds_check.sh
            $<TARGET_FILE:roseompacc>
            ${_omp_test_dir}/rex_omp_lowering_typed_launch_bounds.c
            ${_omp_lowering_test_output_dir}/acc_c/typed_launch_bounds
            rex_omp_lowering_typed_launch_bounds.c)
  _omp_set_test_timeout(omp_acc_lowering_c_typed_launch_bounds 540)
  _label_omp_compile_only(omp_acc_lowering_c_typed_launch_bounds)

  add_test(
    NAME omp_acc_lowering_c_compile_only_target_literal_scalar
    WORKING_DIRECTORY ${_omp_lowering_test_output_dir}
    COMMAND ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_literal_target_param_check.sh
            $<TARGET_FILE:roseompacc>
            ${_omp_test_dir}/target_literal_scalar.c
            ${_omp_lowering_test_output_dir}/acc_c/target_literal_scalar
            target_literal_scalar.c)
  _omp_set_test_timeout(omp_acc_lowering_c_compile_only_target_literal_scalar 540)
  _label_omp_compile_only(omp_acc_lowering_c_compile_only_target_literal_scalar)
endif()

set(OMP_LOWERING_FORTRAN_LABELS ${_omp_legacy_labels})
set(OMP_LOWERING_TRANSLATE_SCRIPT ${_omp_translate_script})
set(OMP_LOWERING_FORTRAN_SEMANTIC_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_fortran_semantic_compare.sh)
set(OMP_LOWERING_FORTRAN_CHECKPOINT_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_fortran_checkpoint_compare.sh)
set(OMP_LOWERING_INVARIANT_VERIFY_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_lowering_invariants.sh)
set(OMP_LOWERING_TEST_DIR ${_omp_test_dir})
set(OMP_LOWERING_INC_DIR ${_omp_lowering_inc})

add_subdirectory(fortran)
