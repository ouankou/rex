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

set(_program_analysis_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_program_analysis_output_dir})

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
  "-I${_vfa_specimen_dir}")

foreach(specimen IN LISTS CXX_TESTCODES)
  string(REPLACE "." "_" _vfa_name ${specimen})
  set(_vfa_target vfa_${_vfa_name}.passed)
  rex_get_cxx_specimen_extra_flags(_vfa_extra_flags ${specimen})
  string(JOIN " " _vfa_extra_flags_str ${_vfa_extra_flags})
  set(_vfa_cmd
    "${_vfa_exec} ${_vfa_switches} ${_vfa_includes}")
  if(_vfa_extra_flags_str)
    string(APPEND _vfa_cmd " ${_vfa_extra_flags_str}")
  endif()
  string(APPEND _vfa_cmd " -c ${_vfa_specimen_dir}/${specimen}")
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

get_property(_program_analysis_registered_tests DIRECTORY PROPERTY TESTS)
if(_program_analysis_registered_tests)
  set_property(TEST ${_program_analysis_registered_tests} APPEND PROPERTY
               ENVIRONMENT
               "ROSE_TEST_OUTPUT_DIR=${_program_analysis_output_dir}")
endif()
