include_directories(
  ${ROSE_INCLUDES}
  ${CMAKE_SOURCE_DIR}/src/midend/programAnalysis
  ${CMAKE_SOURCE_DIR}/src/midend/programAnalysis/pointerAnal
  ${CMAKE_SOURCE_DIR}/src/midend/programAnalysis/CFG
  ${CMAKE_SOURCE_DIR}/src/midend/programAnalysis/bitvectorDataflow
  ${CMAKE_SOURCE_DIR}/src/util/support
  ${CMAKE_SOURCE_DIR}/src/util/graphs
  ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astSupport
  ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astInterface
  ${CMAKE_SOURCE_DIR}/src/midend/astUtil
)

add_executable(CFGTest CFGTest.C)
target_link_libraries(CFGTest ROSE_DLL ${link_with_libraries})

add_executable(DataFlowTest DataFlowTest.C)
target_link_libraries(DataFlowTest ROSE_DLL ${link_with_libraries})

add_executable(StmtRefTest StmtRefTest.C)
target_link_libraries(StmtRefTest ROSE_DLL ${link_with_libraries})

add_executable(PtrAnalTest PtrAnalTest.C)
target_link_libraries(PtrAnalTest ROSE_DLL ${link_with_libraries})

add_executable(steensgaardTest1 steensgaardTest1.C)
target_link_libraries(steensgaardTest1 ROSE_DLL ${link_with_libraries})

add_executable(steensgaardTest2 steensgaardTest2.C)
target_link_libraries(steensgaardTest2 ROSE_DLL ${link_with_libraries})

add_executable(VirtualFunctionAnalysisTest VirtualFunctionAnalysisTest.C)
target_link_libraries(VirtualFunctionAnalysisTest ROSE_DLL ${link_with_libraries})

add_executable(ReachingDefinitionFacadeTest ReachingDefinitionFacadeTest.C)
target_link_libraries(ReachingDefinitionFacadeTest ROSE_DLL ${link_with_libraries})

set(_annotation_lang_dir ${CMAKE_SOURCE_DIR}/src/midend/programAnalysis/annotationLanguageParser)
if(EXISTS "${_annotation_lang_dir}/broadway.h")
  include_directories(${_annotation_lang_dir})
  add_executable(AnnotationLanguageParserTest AnnotationLanguageParserTest.C)
  target_link_libraries(AnnotationLanguageParserTest ROSE_DLL ${link_with_libraries})
endif()
unset(_annotation_lang_dir)

add_subdirectory(defUseAnalysisTests)
add_subdirectory(generalDataFlowAnalysisTests)
add_subdirectory(staticInterproceduralSlicingTests)
add_subdirectory(testCallGraphAnalysis)
add_subdirectory(typeTraitTests)
add_subdirectory(variableLivenessTests)
add_subdirectory(variableRenamingTests)

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_check_answer ${CMAKE_SOURCE_DIR}/scripts/test_with_answer)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)
set(_cfg_exec "$<TARGET_FILE:CFGTest>")
set(_df_exec "$<TARGET_FILE:DataFlowTest>")
set(_ptr_exec "$<TARGET_FILE:PtrAnalTest>")
set(_sr_exec "$<TARGET_FILE:StmtRefTest>")
set(_vfa_exec "$<TARGET_FILE:VirtualFunctionAnalysisTest>")
set(_ptr_cmd "${_ptr_exec} ${CMAKE_CURRENT_SOURCE_DIR}/testPtr2.C")
set(_cfg_cmd_01 "${_cfg_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/testfile1.c")
set(_cfg_cmd_02 "${_cfg_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/testfile2.c")
set(_cfg_cmd_03 "${_cfg_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/testfile3.c")
set(_df_cmd_01 "${_df_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/testfile1.c")
set(_df_cmd_02 "${_df_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/testfile2.c")
set(_df_cmd_03 "${_df_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/testfile3.c")
set(_df_cmd_04 "${_df_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/testfile4.c")
set(_sr_cmd_01 "${_sr_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/testfile1.c")
set(_sr_cmd_02 "${_sr_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/testfile2.c")
set(_sr_cmd_03 "${_sr_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/testfile3.c")
set(_vf_cmd_01 "${_vfa_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test_vfa1.C")
set(_vf_cmd_02 "${_vfa_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test_vfa2.C")
set(_vf_cmd_03 "${_vfa_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test_vfa3.C")
set(_vf_cmd_04 "${_vfa_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test_vfa4.C")
set(_vf_cmd_05 "${_vfa_exec} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test_vfa5.C")

# VirtualFunctionAnalysisTest specimens
set(_vfa_specimen_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)
set(_vfa_switches "-w -rose:verbose 0")
set(_vfa_includes
  "-I${_vfa_specimen_dir} -I${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/A++Code")
set(_vfa_system_header_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/input_codes/system_header_like)

foreach(specimen IN LISTS EXAMPLE_TESTCODES_REQUIRED_TO_PASS)
  string(REPLACE "." "_" _vfa_name ${specimen})
  set(_vfa_target vfa_${_vfa_name}.passed)
  set(_vfa_extra_flags "")
  if (specimen STREQUAL "rex_test2025_issue148_system_header_mutation.cpp")
    set(_vfa_extra_flags "-isystem ${_vfa_system_header_dir}")
  endif()
  if (specimen IN_LIST REX_FRONTEND_ERROR_TESTS)
    set(_vfa_extra_flags "${_vfa_extra_flags} -rex:clang:continue-on-error")
  endif()
  string(STRIP "${_vfa_extra_flags}" _vfa_extra_flags)
  set(_vfa_cmd
    "${_vfa_exec} ${_vfa_switches} ${_vfa_includes} ${_vfa_extra_flags} -c ${_vfa_specimen_dir}/${specimen}")
  add_test(
    NAME program_analysis_vfa_${_vfa_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD=${_vfa_cmd}
      ${_exit_status}
      ${_vfa_target}
  )
endforeach()

# Pointer analysis tests
add_test(
  NAME program_analysis_ptr_01
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_ptr_cmd}
    ANS=${CMAKE_CURRENT_SOURCE_DIR}/PtrAnalTest.out2
    ${_check_answer}
    ptr_01.passed
)

# Control flow graph tests
add_test(
  NAME program_analysis_cfg_01
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_cfg_cmd_01}
    ANS=${CMAKE_CURRENT_SOURCE_DIR}/testfile1.c.cfg
    ${_check_answer}
    cfg_01.passed
)
add_test(
  NAME program_analysis_cfg_02
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_cfg_cmd_02}
    ANS=${CMAKE_CURRENT_SOURCE_DIR}/testfile2.c.cfg
    ${_check_answer}
    cfg_02.passed
)
add_test(
  NAME program_analysis_cfg_03
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_cfg_cmd_03}
    ANS=${CMAKE_CURRENT_SOURCE_DIR}/testfile3.c.cfg
    ${_check_answer}
    cfg_03.passed
)

# Data flow tests
add_test(
  NAME program_analysis_df_01
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_df_cmd_01}
    ANS=${CMAKE_CURRENT_SOURCE_DIR}/testfile1.c.du
    ${_check_answer}
    df_01.passed
)
add_test(
  NAME program_analysis_df_02
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_df_cmd_02}
    ANS=${CMAKE_CURRENT_SOURCE_DIR}/testfile2.c.du
    ${_check_answer}
    df_02.passed
)
add_test(
  NAME program_analysis_df_03
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_df_cmd_03}
    ANS=${CMAKE_CURRENT_SOURCE_DIR}/testfile3.c.du
    ${_check_answer}
    df_03.passed
)
add_test(
  NAME program_analysis_df_04
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_df_cmd_04}
    ANS=${CMAKE_CURRENT_SOURCE_DIR}/testfile4.c.du
    ${_check_answer}
    df_04.passed
)

# Statement ref tests
add_test(
  NAME program_analysis_sr_01
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_sr_cmd_01}
    ANS=${CMAKE_CURRENT_SOURCE_DIR}/testfile1.c.ref
    ${_check_answer}
    sr_01.passed
)
add_test(
  NAME program_analysis_sr_02
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_sr_cmd_02}
    ANS=${CMAKE_CURRENT_SOURCE_DIR}/testfile2.c.ref
    ${_check_answer}
    sr_02.passed
)
add_test(
  NAME program_analysis_sr_03
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_sr_cmd_03}
    ANS=${CMAKE_CURRENT_SOURCE_DIR}/testfile3.c.ref
    ${_check_answer}
    sr_03.passed
)

# Virtual function analysis tests (exit status only)
add_test(
  NAME program_analysis_vf_01
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_vf_cmd_01}
    ${_exit_status}
    vf_01.passed
)
add_test(
  NAME program_analysis_vf_02
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_vf_cmd_02}
    ${_exit_status}
    vf_02.passed
)
add_test(
  NAME program_analysis_vf_03
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_vf_cmd_03}
    ${_exit_status}
    vf_03.passed
)
add_test(
  NAME program_analysis_vf_04
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_vf_cmd_04}
    ${_exit_status}
    vf_04.passed
)
add_test(
  NAME program_analysis_vf_05
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD=${_vf_cmd_05}
    ${_exit_status}
    vf_05.passed
)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_programAnalysisTests_check-extra
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=program_analysis_cfg_01\;program_analysis_cfg_02\;program_analysis_cfg_03\;program_analysis_df_01\;program_analysis_df_02\;program_analysis_df_03\;program_analysis_df_04\;program_analysis_ptr_01\;program_analysis_sr_01\;program_analysis_sr_02\;program_analysis_sr_03\;program_analysis_vf_01\;program_analysis_vf_02\;program_analysis_vf_03\;program_analysis_vf_04\;program_analysis_vf_05
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programAnalysisTests_check-extra PROPERTIES
  DEPENDS "program_analysis_cfg_01;program_analysis_cfg_02;program_analysis_cfg_03;program_analysis_df_01;program_analysis_df_02;program_analysis_df_03;program_analysis_df_04;program_analysis_ptr_01;program_analysis_sr_01;program_analysis_sr_02;program_analysis_sr_03;program_analysis_vf_01;program_analysis_vf_02;program_analysis_vf_03;program_analysis_vf_04;program_analysis_vf_05"
)
add_test(
  NAME nonsmoke_functional_roseTests_programAnalysisTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=program_analysis_cfg_01\;program_analysis_cfg_02\;program_analysis_cfg_03\;program_analysis_df_01\;program_analysis_df_02\;program_analysis_df_03\;program_analysis_df_04\;program_analysis_ptr_01\;program_analysis_sr_01\;program_analysis_sr_02\;program_analysis_sr_03\;program_analysis_vf_01\;program_analysis_vf_02\;program_analysis_vf_03\;program_analysis_vf_04\;program_analysis_vf_05\;program_analysis_vfa_test2001_02_C\;program_analysis_vfa_test2001_03_C\;program_analysis_vfa_test2004_116_C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programAnalysisTests_check-local PROPERTIES
  DEPENDS "program_analysis_cfg_01;program_analysis_cfg_02;program_analysis_cfg_03;program_analysis_df_01;program_analysis_df_02;program_analysis_df_03;program_analysis_df_04;program_analysis_ptr_01;program_analysis_sr_01;program_analysis_sr_02;program_analysis_sr_03;program_analysis_vf_01;program_analysis_vf_02;program_analysis_vf_03;program_analysis_vf_04;program_analysis_vf_05;program_analysis_vfa_test2001_02_C;program_analysis_vfa_test2001_03_C;program_analysis_vfa_test2004_116_C"
)
add_test(
  NAME nonsmoke_functional_roseTests_programAnalysisTests_check-vfa
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=program_analysis_vfa_test2001_02_C\;program_analysis_vfa_test2001_03_C\;program_analysis_vfa_test2004_116_C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programAnalysisTests_check-vfa PROPERTIES
  DEPENDS "program_analysis_vfa_test2001_02_C;program_analysis_vfa_test2001_03_C;program_analysis_vfa_test2004_116_C"
)
