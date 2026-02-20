include_directories(${ROSE_INCLUDES})

add_executable(roseomp roseomp.C)
target_link_libraries(roseomp ROSE_DLL ${link_with_libraries})

add_executable(roseompacc roseompacc.C)
target_link_libraries(roseompacc ROSE_DLL ${link_with_libraries})

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

set(_omp_legacy_labels
  OMPLOWERING
  OMPLOWERING_LEGACY)

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

function(add_omp_translate_only_test test_name translator input_file work_subdir)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} ${_omp_translate_script}
            ${translator}
            ${_omp_test_dir}/${input_file}
            ${CMAKE_CURRENT_BINARY_DIR}/${work_subdir}
            ${input_file})
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 180)
  _label_omp_compile_only(${test_name})
endfunction()

function(add_omp_semantic_test test_name translator compiler input_file mode language
         work_subdir)
  add_test(
    NAME ${test_name}
    COMMAND ${BASH_EXECUTABLE} ${_omp_semantic_script}
            ${translator}
            ${compiler}
            ${_omp_test_dir}/${input_file}
            ${CMAKE_CURRENT_BINARY_DIR}/${work_subdir}
            ${mode}
            ${ROSE_LLVM_OPENMP_INCLUDE_DIR}
            ${ROSE_LLVM_OPENMP_RUNTIME_DIR}
            ${_omp_lowering_inc}
            ${input_file}
            ${language})
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 300)
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
  atomic.c
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
  task_link.cpp
  task_link2.cpp
  task_tree.cpp)

set(OMP_ACC_C_CASES)
set(OMP_ACC_CXX_CASES)

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
  private.c
  reduction.c
  single.c)

set(OMP_CXX_SEMANTIC_CASES
  hello1.cpp
  objectFirstPrivate.cpp
  objectLastprivate.cpp
  objectPrivate.cpp
  orphanedAtomic.cpp
  preprocessingInfo2.cpp)

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
  preprocessingInfo2.cpp)

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
        COMMAND ${BASH_EXECUTABLE} ${_omp_invariant_script}
                $<TARGET_FILE:roseomp>
                ${_omp_test_dir}/${testcode}
                ${CMAKE_CURRENT_BINARY_DIR}/legacy_invariants/${_omp_name}
                ${testcode})
      set_tests_properties(
        omp_lowering_invariant_${_omp_name}
        PROPERTIES
          DEPENDS ${_invariant_dep}
          TIMEOUT 180)
      _label_omp_invariant(omp_lowering_invariant_${_omp_name})
    endif()
  endforeach()

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
endif()

set(OMP_LOWERING_FORTRAN_LABELS ${_omp_legacy_labels})
set(OMP_LOWERING_TRANSLATE_SCRIPT ${_omp_translate_script})
set(OMP_LOWERING_FORTRAN_SEMANTIC_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_fortran_semantic_compare.sh)
set(OMP_LOWERING_INVARIANT_VERIFY_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_lowering_invariants.sh)
set(OMP_LOWERING_TEST_DIR ${_omp_test_dir})
set(OMP_LOWERING_INC_DIR ${_omp_lowering_inc})

add_subdirectory(fortran)
